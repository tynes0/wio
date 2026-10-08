#include "native_bridge_internal.h"
#include "native_object_internal.h"
#include "wio/bytecode/codec.h"
#include "wio/codegen/wir_native_object_emitter.h"
#include "wio/vm/machine.h"
#include <wir_object_abi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
    namespace bc = wio::bytecode;
    namespace bridge = wio::vm::detail;
    namespace abi = wio::wir_backend::abi;
    using wio::vm::Value;

    void check(bool passed, const char* message)
    {
        if (!passed)
            throw std::runtime_error(message);
    }

    void qualifySelection()
    {
        using namespace wio::wir;
        using Emitter = wio::codegen::WirNativeObjectEmitter;
        lowered::Module module;
        const auto integer = module.types.i32Type();
        const auto object = module.types.internNominal(Type{
            .kind = TypeKind::Named,
            .name = "Counter",
            .nominalKind = NominalKind::Object,
            .fields = {{.name = "value", .type = integer}},
            .methods = {{.name = "Read",
                         .returnType = integer,
                         .function = FunctionId{0},
                         .slot = 2,
                         .visibility = FieldVisibility::Public}},
        });
        const auto receiver =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {object}, .isMutable = true});
        module.types.getMutable(object).dispatchEntries = {
            {.contractType = object, .slot = 2, .implementation = FunctionId{0}}};
        module.functions = {
            {.id = FunctionId{0},
             .name = "Read",
             .parameters = {{.type = receiver}},
             .returnType = integer,
             .ownerType = object,
             .isMethod = true},
            {.id = FunctionId{1},
             .name = "NativeRead",
             .returnType = integer,
             .nativeBinding = NativeBinding{.parameters = {{.type = object}}}},
        };
        module.contract.reflection = {
            {.stableTypeId = 9001,
             .type = object,
             .nominalKind = NominalKind::Object,
             .methods = {
                 {.stableId = 1001, .name = "Read", .function = FunctionId{0}, .returnType = integer, .slot = 2}}}};
        check(Emitter::eligibleTypes(module) == std::set{object}, "eligible method-only object was rejected");
        const auto emitted = Emitter::emit(
            module, Emitter::eligibleTypes(module), [](TypeId) { return "std::int32_t"; },
            [](const auto&, const auto&) { return "_WF_Read"; });
        check(emitted.find("_WF_Read() override") != std::string::npos &&
                  emitted.find("_dispatch_" + std::to_string(object.value()) + "_2() override") != std::string::npos &&
                  emitted.find("1001ULL, nullptr, 0") != std::string::npos &&
                  emitted.find("9001ULL, WIO_NATIVE_OBJECT_HOST_PROXY") != std::string::npos,
              "proxy emission lost legacy/dispatch overrides or canonical identities");
        const auto rejects = [&](auto mutate, const char* message)
        {
            auto trial = module;
            mutate(trial);
            check(Emitter::eligibleTypes(trial).empty(), message);
        };
        rejects([&](auto& trial) { trial.types.getMutable(object).fields[0].visibility = FieldVisibility::Public; },
                "public field object was proxied without field synchronization");
        rejects([&](auto& trial) { trial.types.getMutable(object).arguments = {integer}; },
                "generic object was silently proxied");
        rejects([&](auto& trial) { trial.types.getMutable(object).fields[0].type = object; },
                "structured field was proxied without nested lifecycle integration");
        rejects([&](auto& trial) { trial.functions[0].isAsync = true; }, "async method was silently proxied");
        rejects([&](auto& trial) { trial.functions[0].isAbstract = true; }, "abstract method was silently proxied");
        rejects([&](auto& trial) { trial.functions[0].isExternal = true; }, "external method was silently proxied");
        rejects([&](auto& trial) { trial.functions[0].parameters.push_back({.type = receiver}); },
                "borrowed method parameter was silently proxied");
        rejects([&](auto& trial) { trial.functions[0].returnType = object; },
                "object return method was partially proxied");
        rejects([&](auto& trial) { trial.contract.reflection[0].methods[0].stableId = 0; },
                "method without stable identity was proxied");
        rejects([&](auto& trial) { trial.types.getMutable(object).baseTypes = {object}; },
                "unqualified inheritance was silently proxied");
        rejects([&](auto& trial) { trial.functions[1].nativeBinding.reset(); },
                "object without a native boundary emitted an unused proxy");
    }

    struct Library
    {
#if defined(_WIN32)
        HMODULE handle;
        explicit Library(const std::filesystem::path& path) : handle(LoadLibraryW(path.c_str()))
        {
        }
        ~Library()
        {
            if (handle)
                FreeLibrary(handle);
        }
        template <class T> T symbol(const char* name) const
        {
            return reinterpret_cast<T>(GetProcAddress(handle, name));
        }
#else
        void* handle;
        explicit Library(const std::filesystem::path& path) : handle(dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL))
        {
        }
        ~Library()
        {
            if (handle)
                dlclose(handle);
        }
        template <class T> T symbol(const char* name) const
        {
            return reinterpret_cast<T>(dlsym(handle, name));
        }
#endif
        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;
    };

    struct Wire
    {
        WioNativeAbiValue value{};
        ~Wire()
        {
            WioNativeAbiReleaseValue(&value);
        }
    };
    struct Weak
    {
        WioNativeAbiHandle value{};
        ~Weak()
        {
            WioNativeAbiRelease(&value);
        }
    };

    struct Context
    {
        // The module must outlive Machine, which borrows it immutably.
        bc::Module module;
        std::shared_ptr<wio::vm::Machine> machine;
        std::shared_ptr<Library> library;
        const bc::Reflection* objectType = nullptr;
        int released = 0;
        int dispatches = 0;
    };

    struct Host
    {
        std::atomic_uint32_t references{1};
        std::shared_ptr<Context> context;
        Value object;
        ~Host()
        {
            ++context->released;
            // This qualification host explicitly owns lifecycle cleanup.
            // Automatic Machine-managed lifecycle remains a separate step.
            const auto destructor = context->module.types[object.aggregateType()].destructor;
            if (destructor != bc::InvalidIndex)
            {
                const std::array arguments{object};
                const auto result = context->machine->invoke(destructor, arguments);
                if (!result.succeeded())
                    std::cerr << "host cleanup failed: " << result.error().message << '\n';
            }
        }
    };

    void retain(void* state) noexcept
    {
        ++static_cast<Host*>(state)->references;
    }
    void release(void* state) noexcept
    {
        auto* host = static_cast<Host*>(state);
        if (--host->references == 0)
            delete host;
    }
    std::uint64_t typeId(const void* state) noexcept
    {
        return static_cast<const Host*>(state)->context->objectType->stableTypeId;
    }
    const WioNativeAbiHandleOps operations{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};

    WioNativeAbiStatus dispatch(const WioNativeObjectHost* token, std::uint64_t methodId,
                                const WioNativeAbiValue* arguments, std::uint64_t count, WioNativeAbiValue* output,
                                WioNativeAbiFailure* failure) noexcept
    {
        return abi::ContainerAdapter<Value, 0>::boundary(
            failure,
            [&]
            {
                abi::require(token && token->owner.state && output, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                auto& host = *static_cast<Host*>(token->owner.state);
                const auto& context = *host.context;
                abi::require(token->objectId == 801 && token->generation == 19, WIO_NATIVE_ABI_STALE_HANDLE);
                const auto& methods = context.objectType->methods;
                const auto method = std::ranges::find(methods, methodId, &bc::ReflectedMethod::stableId);
                abi::require(method != methods.end() && count == method->parameterTypes.size() && (!count || arguments),
                             WIO_NATIVE_ABI_INVALID_ARGUMENT);
                std::vector<Value> parameters{host.object};
                for (std::size_t index = 0; index < count; ++index)
                {
                    std::uint8_t marshalling = 0;
                    abi::require(
                        bridge::callbackValueMarshalling(context.module, method->parameterTypes[index], marshalling));
                    auto value = bridge::decodeNativeValue(context.module, arguments[index],
                                                           method->parameterTypes[index], marshalling);
                    abi::require(value.succeeded());
                    parameters.push_back(std::move(value.value));
                }
                ++host.context->dispatches;
                const auto result = context.machine->invoke(method->function, parameters);
                if (!result.succeeded())
                    throw std::runtime_error(result.error().code + ": " + result.error().message);
                std::uint8_t marshalling = 0;
                abi::require(bridge::callbackValueMarshalling(context.module, method->returnType, marshalling));
                std::string error;
                abi::require(bridge::encodeOwnedNativeValue(context.module, result.value(), method->returnType,
                                                            marshalling, *output, error));
            });
    }

    WioNativeAbiStatus call(const WioNativeAbiRegistry& registry, const char* symbol,
                            std::span<const WioNativeAbiValue> arguments, WioNativeAbiValue& result)
    {
        const auto functions = std::span{registry.functions, registry.functionCount};
        const auto function = std::ranges::find_if(functions, [&](const auto& item)
                                                   { return std::string_view(item.nativeSymbol) == symbol; });
        check(function != functions.end(), "generated native function descriptor missing");
        WioNativeAbiFailure failure{};
        const auto status = function->thunk(arguments.data(), arguments.size(), &result, &failure);
        if (status != WIO_NATIVE_ABI_OK)
            std::cerr << bridge::nativeFailureMessage(failure) << '\n';
        return status;
    }

    std::int64_t scalar(const WioNativeAbiRegistry& registry, const char* symbol,
                        std::span<const WioNativeAbiValue> arguments = {})
    {
        Wire result;
        check(call(registry, symbol, arguments, result.value) == WIO_NATIVE_ABI_OK &&
                  result.value.kind == WIO_NATIVE_ABI_I64,
              "generated scalar thunk failed");
        return result.value.payload.signedInteger;
    }

    int qualify(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "generated object bytecode decode failed");
        auto context = std::make_shared<Context>();
        context->module = std::move(decoded.module);
        context->machine = std::make_shared<wio::vm::Machine>(context->module);
        context->library = std::make_shared<Library>(std::filesystem::u8path(producer));
        check(bool(context->library->handle), "generated object DLL open failed");
        const auto getObjects = context->library->symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = context->library->symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getRegistry, "generated object exports missing");
        const auto* objects = getObjects();
        const auto* registry = getRegistry();
        check(objects && registry && objects->version == 1 && objects->typeCount == 1,
              "generated object descriptor closure is wrong");
        for (const auto& reflected : context->module.contract.reflection)
            if (context->module.string(reflected.logicalName) == "Counter")
                context->objectType = &reflected;
        check(context->objectType && objects->types[0].typeId == context->objectType->stableTypeId,
              "generated object uses a non-canonical type identity");
        std::string error;
        check(context->machine->loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        std::uint32_t make = bc::InvalidIndex;
        for (const auto& function : context->module.functions)
            if (context->module.string(function.name) == "Make")
                make = function.id;
        auto object = context->machine->invoke(make);
        check(object.succeeded() && object.value().kind() == Value::Kind::Object, "VM object constructor failed");
        auto* host = new Host{.context = context, .object = object.value()};
        object = {};
        WioNativeAbiHandle claim{host, &operations, 19};
        struct ClaimGuard
        {
            WioNativeAbiHandle& claim;
            ~ClaimGuard()
            {
                WioNativeAbiRelease(&claim);
            }
        } claimGuard{claim};
        const WioNativeObjectHost token{.typeId = context->objectType->stableTypeId,
                                        .objectId = 801,
                                        .generation = 19,
                                        .owner = claim,
                                        .invoke = &dispatch};
        const auto& adapter = objects->types[0];
        Wire proxy;
        WioNativeAbiFailure failure{};
        check(adapter.create(&token, &proxy.value, &failure) == WIO_NATIVE_ABI_OK, "generated proxy factory failed");
        check(host->references == 2 && scalar(*registry, "vm_object_proxy::Constructions") == 1,
              "generated proxy reran the hosted constructor or retained twice");
        Weak weak;
        check(adapter.weak(&proxy.value, &weak.value, &failure) == WIO_NATIVE_ABI_OK, "generated weak cache failed");
        const std::array readArguments{proxy.value};
        check(scalar(*registry, "vm_object_proxy::Read", readArguments) == 7, "native method did not read VM fields");
        WioNativeAbiValue delta{};
        delta.kind = WIO_NATIVE_ABI_I64;
        delta.payload.signedInteger = 11;
        const std::array addArguments{proxy.value, delta};
        check(scalar(*registry, "vm_object_proxy::Add", addArguments) == 18 &&
                  host->object.field(0)->asSignedInteger() == 18,
              "generated native method mutated a copied object");
        const std::string prefix{"hello\0world", 11};
        auto label = abi::Codec<std::string, 0>::write(prefix);
        const std::array labelArguments{proxy.value, label};
        Wire result;
        check(call(*registry, "vm_object_proxy::Label", labelArguments, result.value) == WIO_NATIVE_ABI_OK &&
                  abi::Codec<std::string, 0>::read(result.value) == prefix + ":counter",
              "generated string method lost ownership or embedded NUL");
        const auto unicode = wio::runtime::Text::FromUtf8("\xc4\xb0stanbul \xf0\x9f\x9a\x80");
        const auto text = abi::Codec<wio::runtime::Text, 0>::write(unicode);
        const std::array textArguments{proxy.value, text};
        Wire textResult;
        check(call(*registry, "vm_object_proxy::EchoText", textArguments, textResult.value) == WIO_NATIVE_ABI_OK &&
                  abi::Codec<wio::runtime::Text, 0>::read(textResult.value) == unicode,
              "generated Unicode method lost its owned result or non-ASCII scalars");
        delta.payload.signedInteger = 23;
        const std::array resetArguments{proxy.value, delta};
        Wire resetResult;
        check(call(*registry, "vm_object_proxy::Reset", resetArguments, resetResult.value) == WIO_NATIVE_ABI_OK &&
                  resetResult.value.kind == WIO_NATIVE_ABI_VOID && host->object.field(0)->asSignedInteger() == 23,
              "generated void method did not update its VM receiver");
        Wire stored;
        check(call(*registry, "vm_object_proxy::Store", readArguments, stored.value) == WIO_NATIVE_ABI_OK,
              "native Ref retention failed");
        WioNativeObjectHost recovered{};
        check(adapter.host(&proxy.value, &recovered, &failure) == WIO_NATIVE_ABI_OK && recovered.owner.state == host,
              "generated identity recovery changed the VM owner");
        WioNativeAbiRelease(&claim);
        WioNativeAbiReleaseValue(&proxy.value);
        check(context->released == 0 && scalar(*registry, "vm_object_proxy::ReadStored") == 23,
              "native-held Ref lost its live VM host");
        Wire clear;
        check(call(*registry, "vm_object_proxy::Clear", {}, clear.value) == WIO_NATIVE_ABI_OK && context->released == 1,
              "last native Ref did not release its host");
        check(scalar(*registry, "vm_object_proxy::Destructions") == 1, "VM-owned lifecycle cleanup was not once");
        Wire locked;
        check(adapter.lock(&weak.value, &locked.value, &failure) == WIO_NATIVE_ABI_STALE_HANDLE,
              "weak cache resurrected the released VM host");
        WioNativeAbiRelease(&weak.value);
        check(scalar(*registry, "vm_object_proxy::Destructions") == 1 && context->dispatches == 6,
              "delayed native destructor reran Wio lifecycle or native wrappers bypassed VM dispatch");
        return 0;
    }

    void qualifyIdentityCache(const bc::Module& module, const WioNativeObjectDescriptor& adapter)
    {
        const auto reflected =
            std::ranges::find(module.contract.reflection, adapter.typeId, &bc::Reflection::stableTypeId);
        check(reflected != module.contract.reflection.end(), "identity-cache object type missing");
        auto retiredCode = std::make_shared<int>(0);
        const auto invoke = [](const Value&, std::uint64_t, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                               WioNativeAbiFailure*) { return WIO_NATIVE_ABI_INVALID_ARGUMENT; };
        const auto codeLease = [retiredCode]() -> std::shared_ptr<void>
        {
            const auto release = [retiredCode](int* lease)
            {
                ++*retiredCode;
                delete lease;
            };
            return std::shared_ptr<int>(new int(0), release);
        };
        auto manager = std::make_shared<bridge::NativeObjectBridge>(invoke, codeLease);
        auto other = std::make_shared<bridge::NativeObjectBridge>(invoke, []() -> std::shared_ptr<void> { return {}; });
        Value object = Value::object(reflected->type, {Value::signedInteger(3)});
        Value second = Value::object(reflected->type, {Value::signedInteger(3)});
        check(object.objectIdentity() != second.objectIdentity(), "different objects reused a VM identity");
        Wire first;
        Wire alias;
        std::string error;
        check(manager->encode(object, reflected->type, adapter, first.value, error) &&
                  manager->encode(object, reflected->type, adapter, alias.value, error),
              error.c_str());
        WioNativeObjectHost firstHost{}, aliasHost{};
        check(adapter.host(&first.value, &firstHost, nullptr) == WIO_NATIVE_ABI_OK &&
                  adapter.host(&alias.value, &aliasHost, nullptr) == WIO_NATIVE_ABI_OK &&
                  firstHost.owner.state == aliasHost.owner.state,
              "weak cache did not reuse its canonical live proxy");
        bool matched = false;
        auto recovered = manager->recover(alias.value, reflected->type, adapter, matched);
        check(matched && recovered.succeeded() && recovered.value.objectIdentity() == object.objectIdentity(),
              "producer object return did not recover original VM identity");
        recovered = other->recover(alias.value, reflected->type, adapter, matched);
        check(matched && !recovered.succeeded(), "identity recovery accepted a different machine host");
        auto invalid = firstHost;
        ++invalid.generation;
        Wire result;
        check(invalid.invoke(&invalid, 1, nullptr, 0, &result.value, nullptr) == WIO_NATIVE_ABI_STALE_HANDLE,
              "dispatch accepted a stale host generation");
        WioNativeAbiReleaseValue(&alias.value);
        WioNativeAbiReleaseValue(&first.value);
        auto retired = manager->takeRetired();
        check(retired.size() == 1 && retired.front().objectIdentity() == object.objectIdentity(),
              "final native release lost or duplicated its cleanup claim");
        retired.clear();
        check(manager->encode(object, reflected->type, adapter, first.value, error), error.c_str());
        check(adapter.host(&first.value, &aliasHost, nullptr) == WIO_NATIVE_ABI_OK &&
                  aliasHost.generation != firstHost.generation,
              "expired weak cache resurrected an old host generation");
        auto closed = manager->shutdown();
        check(closed.size() == 1, "shutdown lost its live object cleanup claim");
        check(aliasHost.invoke(&aliasHost, 1, nullptr, 0, &result.value, nullptr) == WIO_NATIVE_ABI_STALE_HANDLE,
              "closed manager accepted object dispatch");
        const int beforeFinalRelease = *retiredCode;
        WioNativeAbiReleaseValue(&first.value);
        check(*retiredCode == beforeFinalRelease, "producer code lease retired inside final release");
        wio::vm::Machine::collectNativeObjectRetirements();
        check(*retiredCode == beforeFinalRelease + 1, "safe host collection did not retire producer code lease");
    }

    int qualifyAutomatic(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "automatic object bytecode decode failed");
        auto machine = std::make_unique<wio::vm::Machine>(decoded.module);
        Library library{std::filesystem::u8path(producer)};
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getRegistry, "automatic object registry missing");
        const auto& registry = *getRegistry();
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        check(getObjects && getObjects()->typeCount == 1, "automatic object sidecar missing");
        qualifyIdentityCache(decoded.module, getObjects()->types[0]);
        std::string error;
        check(machine->loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto functionId = [&](const char* name)
        {
            const auto function = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                       { return decoded.module.string(function.name) == name; });
            check(function != decoded.module.functions.end(), "automatic object entry missing");
            return function->id;
        };
        const auto result = machine->invoke(functionId("Entry"));
        if (!result.succeeded())
            throw std::runtime_error(result.error().code + ": " + result.error().message);
        if (result.value().asSignedInteger() != 0)
        {
            std::cerr << "Entry returned " << result.value().asSignedInteger()
                      << ", constructions=" << scalar(registry, "vm_object_proxy::Constructions")
                      << ", destructions=" << scalar(registry, "vm_object_proxy::Destructions") << '\n';
            throw std::runtime_error("automatic object lifecycle/identity qualification failed");
        }
        const auto aliased = machine->invoke(functionId("AliasedEntry"));
        if (!aliased.succeeded())
            std::cerr << aliased.error().code << ": " << aliased.error().message << '\n';
        check(!aliased.succeeded() && aliased.error().code == "WVM1225",
              "native object/field overlap was not rejected before producer invocation");
        check(scalar(registry, "vm_object_proxy::Destructions") == 4,
              "object/field overlap failure did not unwind its VM object once");
        check(machine->invoke(functionId("RetainForHost")).succeeded(), "native-held VM object setup failed");
        check(scalar(registry, "vm_object_proxy::ReadStored") == 41 &&
                  scalar(registry, "vm_object_proxy::Destructions") == 4,
              "native-held Ref did not keep its original VM object alive");
        WioNativeAbiStatus threadStatus = WIO_NATIVE_ABI_OK;
        std::thread foreign(
            [&]
            {
                Wire wire;
                threadStatus = call(registry, "vm_object_proxy::ReadStored", {}, wire.value);
            });
        foreign.join();
        check(threadStatus == WIO_NATIVE_ABI_WRONG_THREAD, "hosted object accepted foreign-thread dispatch");
        std::thread foreignRelease(
            [&]
            {
                Wire wire;
                threadStatus = call(registry, "vm_object_proxy::Clear", {}, wire.value);
            });
        foreignRelease.join();
        check(threadStatus == WIO_NATIVE_ABI_OK && scalar(registry, "vm_object_proxy::Destructions") == 4,
              "foreign-thread final release executed Wio cleanup instead of queuing it");
        check(machine->invoke(functionId("RetainForHost")).succeeded() &&
                  scalar(registry, "vm_object_proxy::Destructions") == 5 &&
                  scalar(registry, "vm_object_proxy::ReadStored") == 41,
              "next invocation did not drain queued object cleanup exactly once");
        machine.reset();
        check(scalar(registry, "vm_object_proxy::Destructions") == 6, "shutdown did not clean hosted VM object once");
        Wire stale;
        check(call(registry, "vm_object_proxy::ReadStored", {}, stale.value) == WIO_NATIVE_ABI_STALE_HANDLE,
              "native proxy invoked a destroyed machine");
        Wire cleared;
        check(call(registry, "vm_object_proxy::Clear", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "closed-machine proxy final release failed");
        wio::vm::Machine::collectNativeObjectRetirements();
        check(scalar(registry, "vm_object_proxy::Destructions") == 6, "proxy final release reran Wio destructor");
        return 0;
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3 || argc == 4, "object proxy qualification requires bytecode and producer paths");
        qualifySelection();
        if (argc == 4)
            return qualifyAutomatic(argv[1], argv[2]);
        return qualify(argv[1], argv[2]);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
