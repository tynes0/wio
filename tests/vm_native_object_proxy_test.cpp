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
    WioNativeObjectCreateFn partialObjectFactory = nullptr;
    bool failObjectFactory = false;
    int objectFactoryCallsBeforeFailure = 0;

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
        {
            auto trial = module;
            trial.functions[0].parameters.push_back({.type = receiver});
            check(Emitter::eligibleTypes(trial) == std::set{object}, "object ref method parameter was rejected");
            const auto view =
                trial.types.intern(Type{.kind = TypeKind::Reference, .arguments = {object}, .isMutable = false});
            trial.functions[0].parameters.back().type = view;
            check(Emitter::eligibleTypes(trial) == std::set{object}, "object view method parameter was rejected");
            trial.functions[0].returnType = receiver;
            check(Emitter::eligibleTypes(trial).empty(), "borrowed method return was silently proxied");
        }
        rejects(
            [&](auto& trial)
            {
                const auto scalarRef = trial.types.intern(Type{.kind = TypeKind::Reference, .arguments = {integer}});
                trial.functions[0].parameters.push_back({.type = scalarRef});
            },
            "scalar reference method parameter was silently qualified by the object-only borrow slice");
        {
            auto trial = module;
            trial.functions[0].returnType = object;
            trial.contract.reflection[0].methods[0].returnType = object;
            check(Emitter::eligibleTypes(trial) == std::set{object}, "self-returning object method was rejected");
            const auto dependency = trial.types.internNominal(Type{
                .kind = TypeKind::Named,
                .name = "Badge",
                .nominalKind = NominalKind::Object,
                .fields = {{.name = "value", .type = integer}},
            });
            trial.contract.reflection.push_back(
                {.stableTypeId = 9002, .type = dependency, .nominalKind = NominalKind::Object});
            trial.functions[0].returnType = dependency;
            trial.contract.reflection[0].methods[0].returnType = dependency;
            check(Emitter::eligibleTypes(trial) == std::set{object, dependency},
                  "object method dependency missing from proxy closure");
            const auto dependencyReceiver =
                trial.types.intern(Type{.kind = TypeKind::Reference, .arguments = {dependency}});
            trial.functions.push_back({.id = FunctionId{2},
                                       .name = "Back",
                                       .parameters = {{.type = dependencyReceiver}},
                                       .returnType = object,
                                       .ownerType = dependency,
                                       .isMethod = true});
            trial.types.getMutable(dependency).methods = {
                {.name = "Back", .returnType = object, .function = FunctionId{2}}};
            trial.contract.reflection.back().methods = {
                {.stableId = 1002, .name = "Back", .function = FunctionId{2}, .returnType = object}};
            check(Emitter::eligibleTypes(trial) == std::set{object, dependency},
                  "mutually dependent object method signatures did not converge");
            trial.types.getMutable(dependency).fields[0].visibility = FieldVisibility::Public;
            check(Emitter::eligibleTypes(trial).empty(), "unsupported dependency left a partially usable proxy");
        }
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

    void qualifyArgumentGuards()
    {
        static_assert(!std::is_copy_constructible_v<abi::ObjectArguments<2>>);
        int released = 0;
        const WioNativeAbiHandleOps operations{WIO_NATIVE_ABI_VERSION, 0, [](void*) noexcept {},
                                               [](void* pointer) noexcept { ++*static_cast<int*>(pointer); },
                                               [](const void*) noexcept -> std::uint64_t { return 1; }};
        try
        {
            abi::ObjectArguments<2> arguments;
            arguments[0].kind = WIO_NATIVE_ABI_BYTES;
            arguments[0].flags = WIO_NATIVE_ABI_VALUE_OWNED;
            arguments[0].owner = {&released, &operations, 1};
            throw std::runtime_error("second argument conversion failed");
        }
        catch (const std::runtime_error&)
        {
        }
        check(released == 1, "partial object-method argument initialization leaked an owned wire");
        {
            abi::ObjectArguments<1> arguments;
            arguments[0].kind = WIO_NATIVE_ABI_BYTES;
            arguments[0].flags = WIO_NATIVE_ABI_VALUE_OWNED;
            arguments[0].owner = {&released, &operations, 1};
        }
        check(released == 2, "successful object-method call did not release its owned argument");
    }

    int qualifyReferences(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "object reference method bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getObjects()->typeCount == 1 && getRegistry, "object reference producer API missing");
        const auto& adapter = getObjects()->types[0];
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        for (const auto* name : {"Entry", "PanicEntry"})
        {
            const auto entry = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == name; });
            check(entry != decoded.module.functions.end(), "object reference method entry missing");
            const auto result = machine.invoke(entry->id);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            check(result.value().asSignedInteger() == 0, "object reference alias/copy-back/lifecycle check failed");
        }
        const auto capture = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                  { return decoded.module.string(function.name) == "CaptureEntry"; });
        check(capture != decoded.module.functions.end(), "object reference capture entry missing");
        const auto captured = machine.invoke(capture->id);
        check(captured.succeeded(), "object reference capture failed");
        const auto token = *reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
        const auto reflected =
            std::ranges::find(decoded.module.contract.reflection, token.typeId, &bc::Reflection::stableTypeId);
        check(reflected != decoded.module.contract.reflection.end(), "object reference method metadata missing");
        const auto makeTarget = std::ranges::find_if(reflected->methods, [&](const auto& method)
                                                     { return decoded.module.string(method.name) == "MakeTarget"; });
        check(makeTarget != reflected->methods.end(), "mutable object reference method missing");
        Wire cell;
        check(adapter.create(&token, &cell.value, nullptr) == WIO_NATIVE_ABI_OK, "object reference cell setup failed");
        WioNativeAbiValue reference{};
        reference.kind = WIO_NATIVE_ABI_REFERENCE;
        reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        reference.payload.mutableSlice = {&cell.value, sizeof(WioNativeAbiValue)};
        WioNativeAbiValue seed{};
        seed.kind = WIO_NATIVE_ABI_I64;
        seed.payload.signedInteger = 99;
        const auto beforeInvalid = scalar(registry, "vm_object_references::Constructions");
        for (int kind = 0; kind < 5; ++kind)
        {
            auto invalid = reference;
            if (kind == 0)
                invalid.flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            if (kind == 1)
                invalid.payload.mutableSlice.size = 1;
            if (kind == 2)
                invalid.payload.mutableSlice.data = nullptr;
            if (kind == 3)
                invalid.flags |= WIO_NATIVE_ABI_VALUE_PROJECTED;
            if (kind == 4)
                invalid.payload.mutableSlice.data = reinterpret_cast<std::byte*>(&cell.value) + 1;
            const std::array arguments{invalid, seed};
            Wire output;
            check(token.invoke(&token, makeTarget->stableId, arguments.data(), arguments.size(), &output.value,
                               nullptr) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                      output.value.kind == WIO_NATIVE_ABI_VOID,
                  "malformed/readonly hosted ref was accepted or changed its output");
        }
        check(scalar(registry, "vm_object_references::Constructions") == beforeInvalid,
              "invalid object ref invoked the method body");
        Wire cleared;
        check(call(registry, "vm_object_references::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "object reference capture cleanup failed");

        auto failingDescriptor = adapter;
        partialObjectFactory = adapter.create;
        failingDescriptor.create =
            [](const WioNativeObjectHost* host, WioNativeAbiValue* output, WioNativeAbiFailure* failure) noexcept
        {
            const auto status = partialObjectFactory(host, output, failure);
            if (status != WIO_NATIVE_ABI_OK || !failObjectFactory || objectFactoryCallsBeforeFailure-- > 0)
                return status;
            return WIO_NATIVE_ABI_EXCEPTION; // Leave an owned partial output for failure cleanup.
        };
        const WioNativeObjectApi failingApi{WIO_NATIVE_OBJECT_VERSION, 1, &failingDescriptor};
        wio::vm::Machine failedMachine{decoded.module};
        check(failedMachine.registerNativeRegistry(registry, failingApi, &error), error.c_str());
        const auto failureCaptured = failedMachine.invoke(capture->id);
        check(failureCaptured.succeeded(), "failed copy-back host capture failed");
        const auto failureToken =
            *reinterpret_cast<const WioNativeObjectHost*>(failureCaptured.value().asUnsignedInteger());
        Wire failureCell;
        check(adapter.create(&failureToken, &failureCell.value, nullptr) == WIO_NATIVE_ABI_OK,
              "failed copy-back argument setup failed");
        reference.payload.mutableSlice.data = &failureCell.value;
        const auto originalPointer = failureCell.value.payload.handle.state;
        const auto beforeConstructed = scalar(registry, "vm_object_references::Constructions");
        const auto beforeDestroyed = scalar(registry, "vm_object_references::Destructions");
        failObjectFactory = true;
        const std::array failureArguments{reference, seed};
        Wire failed;
        check(failureToken.invoke(&failureToken, makeTarget->stableId, failureArguments.data(), failureArguments.size(),
                                  &failed.value, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
              "failed object reference copy-back lost its boundary status");
        failObjectFactory = false;
        check(failureCell.value.payload.handle.state == originalPointer,
              "failed object reference copy-back partially replaced the caller's cell");
        const auto safePoint = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == "SafePoint"; });
        check(safePoint != decoded.module.functions.end() && failedMachine.invoke(safePoint->id).succeeded(),
              "failed object reference cleanup safe point missing");
        check(scalar(registry, "vm_object_references::Constructions") == beforeConstructed + 1 &&
                  scalar(registry, "vm_object_references::Destructions") == beforeDestroyed + 1,
              "failed object reference copy-back lost or repeated Wio lifecycle cleanup");
        const auto makePair = std::ranges::find_if(reflected->methods, [&](const auto& method)
                                                   { return decoded.module.string(method.name) == "MakePair"; });
        check(makePair != reflected->methods.end(), "transactional object reference method missing");
        Wire secondCell;
        check(adapter.create(&failureToken, &secondCell.value, nullptr) == WIO_NATIVE_ABI_OK,
              "transactional object reference cell setup failed");
        auto secondReference = reference;
        secondReference.payload.mutableSlice.data = &secondCell.value;
        const auto secondPointer = secondCell.value.payload.handle.state;
        const auto beforePairConstructed = scalar(registry, "vm_object_references::Constructions");
        const auto beforePairDestroyed = scalar(registry, "vm_object_references::Destructions");
        objectFactoryCallsBeforeFailure = 1;
        failObjectFactory = true;
        const std::array pairArguments{reference, secondReference, seed};
        check(failureToken.invoke(&failureToken, makePair->stableId, pairArguments.data(), pairArguments.size(),
                                  &failed.value, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
              "second-cell proxy factory failure lost its boundary status");
        failObjectFactory = false;
        check(failureCell.value.payload.handle.state == originalPointer &&
                  secondCell.value.payload.handle.state == secondPointer,
              "second-cell copy-back failure committed the first reference cell");
        check(failedMachine.invoke(safePoint->id).succeeded(), "transactional copy-back cleanup safe point failed");
        check(scalar(registry, "vm_object_references::Constructions") == beforePairConstructed + 2 &&
                  scalar(registry, "vm_object_references::Destructions") == beforePairDestroyed + 2,
              "transactional reference copy-back failure leaked staged VM objects");
        check(call(registry, "vm_object_references::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "failed copy-back capture cleanup failed");
        return 0;
    }

    int qualifyMethods(const char* bytecode, const char* producer)
    {
        qualifyArgumentGuards();
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "object-valued method bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        check(getObjects && getObjects()->typeCount == 2, "method-only dependency did not emit an object adapter");
        const auto types = std::span{getObjects()->types, getObjects()->typeCount};
        for (const char* name : {"Counter", "Badge"})
        {
            const auto reflected = std::ranges::find_if(decoded.module.contract.reflection, [&](const auto& type)
                                                        { return decoded.module.string(type.logicalName) == name; });
            check(reflected != decoded.module.contract.reflection.end(), "object-valued method reflection missing");
            check(std::ranges::find(types, reflected->stableTypeId, &WioNativeObjectDescriptor::typeId) != types.end(),
                  "method-only object dependency lost its stable producer identity");
        }
        auto machine = std::make_unique<wio::vm::Machine>(decoded.module);
        std::string error;
        check(machine->loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto entry = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                { return decoded.module.string(function.name) == "Entry"; });
        check(entry != decoded.module.functions.end(), "object-valued method entry missing");
        const auto result = machine->invoke(entry->id);
        if (!result.succeeded())
            throw std::runtime_error(result.error().code + ": " + result.error().message);
        check(result.value().asSignedInteger() == 0, "object-valued method identity/lifecycle qualification failed");
        const auto captureEntry =
            std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                 { return decoded.module.string(function.name) == "CaptureEntry"; });
        check(captureEntry != decoded.module.functions.end(), "hosted method capture entry missing");
        const auto captured = machine->invoke(captureEntry->id);
        check(captured.succeeded(), "hosted method capture failed");
        const auto* token = reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
        check(token && token->owner.state && token->invoke, "hosted method token missing");
        const auto reflected =
            std::ranges::find(decoded.module.contract.reflection, token->typeId, &bc::Reflection::stableTypeId);
        check(reflected != decoded.module.contract.reflection.end(), "captured method owner type missing");
        const auto choose = std::ranges::find_if(reflected->methods, [&](const auto& method)
                                                 { return decoded.module.string(method.name) == "Choose"; });
        check(choose != reflected->methods.end(), "captured object-valued method missing");
        const auto adapter = std::ranges::find(types, token->typeId, &WioNativeObjectDescriptor::typeId);
        Wire argument;
        check(adapter->create(token, &argument.value, nullptr) == WIO_NATIVE_ABI_OK, "captured argument proxy failed");
        Weak argumentWeak;
        check(adapter->weak(&argument.value, &argumentWeak.value, nullptr) == WIO_NATIVE_ABI_OK,
              "object-valued method argument weak probe failed");
        Wire returned;
        check(token->invoke(token, choose->stableId, &argument.value, 1, &returned.value, nullptr) == WIO_NATIVE_ABI_OK,
              "hosted object-valued method rejected its own machine's object");
        WioNativeObjectHost identity{};
        check(adapter->host(&returned.value, &identity, nullptr) == WIO_NATIVE_ABI_OK &&
                  identity.objectId == token->objectId && identity.owner.state == token->owner.state,
              "hosted method return lost canonical original-object identity");
        auto foreign = std::make_shared<bridge::NativeObjectBridge>(
            [](const Value&, std::uint64_t, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
               WioNativeAbiFailure*) { return WIO_NATIVE_ABI_INVALID_ARGUMENT; },
            []() -> std::shared_ptr<void> { return {}; });
        Value foreignObject = Value::object(reflected->type, {Value::signedInteger(13)});
        Wire foreignWire;
        check(foreign->encode(foreignObject, reflected->type, *adapter, foreignWire.value, error), error.c_str());
        Wire rejected;
        WioNativeAbiFailure failure{};
        check(token->invoke(token, choose->stableId, &foreignWire.value, 1, &rejected.value, &failure) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH &&
                  rejected.value.kind == WIO_NATIVE_ABI_VOID,
              "hosted method accepted a different machine's object argument");
        check(token->invoke(token, choose->stableId, nullptr, 0, &rejected.value, &failure) ==
                  WIO_NATIVE_ABI_INVALID_ARGUMENT,
              "hosted method accepted an invalid object argument count");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getRegistry, "object-valued method registry missing");
        const auto& registry = *getRegistry();
        const std::array failArguments{argument.value};
        {
            Wire producerObject;
            check(call(registry, "vm_object_methods::MakeForeign", failArguments, producerObject.value) ==
                      WIO_NATIVE_ABI_OK,
                  "producer-created object setup failed");
            check(token->invoke(token, choose->stableId, &producerObject.value, 1, &rejected.value, &failure) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "hosted method accepted a producer-owned object without VM field synchronization");
        }
        Wire failed;
        check(call(registry, "vm_object_methods::Fail", failArguments, failed.value) == WIO_NATIVE_ABI_PANIC,
              "object-valued method failure lost its VM panic status");
        WioNativeAbiReleaseValue(&argument.value);
        WioNativeAbiReleaseValue(&returned.value);
        Wire weakProbe;
        check(adapter->lock(&argumentWeak.value, &weakProbe.value, nullptr) == WIO_NATIVE_ABI_STALE_HANDLE,
              "failed object-valued method leaked an owned argument proxy claim");
        WioNativeAbiValue counterKind{};
        counterKind.kind = WIO_NATIVE_ABI_I64;
        const std::array counterArguments{counterKind};
        check(scalar(registry, "vm_object_methods::Constructions", counterArguments) == 3 &&
                  scalar(registry, "vm_object_methods::Destructions", counterArguments) == 3,
              "object method failure changed original/producer lifecycle counts");
        machine.reset();
        check(scalar(registry, "vm_object_methods::Destructions", counterArguments) == 4,
              "object-valued method shutdown did not destroy its VM receiver exactly once");
        check(token->invoke(token, choose->stableId, &foreignWire.value, 1, &rejected.value, &failure) ==
                  WIO_NATIVE_ABI_STALE_HANDLE,
              "closed machine accepted an object-valued method call");
        Wire cleared;
        check(call(registry, "vm_object_methods::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "captured method proxy final release failed");
        wio::vm::Machine::collectNativeObjectRetirements();
        check(scalar(registry, "vm_object_methods::Destructions", counterArguments) == 4,
              "object-valued method proxy final release repeated Wio lifecycle");
        const auto badge = std::ranges::find_if(decoded.module.contract.reflection, [&](const auto& type)
                                                { return decoded.module.string(type.logicalName) == "Badge"; });
        check(badge != decoded.module.contract.reflection.end(), "failed-return object metadata missing");
        std::vector<WioNativeObjectDescriptor> failingTypes{types.begin(), types.end()};
        for (auto& type : failingTypes)
            if (type.typeId == badge->stableTypeId)
            {
                partialObjectFactory = type.create;
                type.create = [](const WioNativeObjectHost* host, WioNativeAbiValue* output,
                                 WioNativeAbiFailure* failure) noexcept
                {
                    const auto status = partialObjectFactory(host, output, failure);
                    if (status != WIO_NATIVE_ABI_OK)
                        return status;
                    if (failure)
                    {
                        *failure = {};
                        failure->status = WIO_NATIVE_ABI_EXCEPTION;
                    }
                    return WIO_NATIVE_ABI_EXCEPTION; // Deliberately leave one owned partial proxy output.
                };
            }
        WioNativeAbiValue badgeKind{};
        badgeKind.kind = WIO_NATIVE_ABI_I64;
        badgeKind.payload.signedInteger = 1;
        const std::array badgeArguments{badgeKind};
        const auto beforeBadgeConstructors = scalar(registry, "vm_object_methods::Constructions", badgeArguments);
        const auto beforeBadgeDestructors = scalar(registry, "vm_object_methods::Destructions", badgeArguments);
        const WioNativeObjectApi failingApi{WIO_NATIVE_OBJECT_VERSION, static_cast<std::uint32_t>(failingTypes.size()),
                                            failingTypes.data()};
        wio::vm::Machine failedMachine{decoded.module};
        check(failedMachine.registerNativeRegistry(registry, failingApi, &error), error.c_str());
        const auto failedReturn = failedMachine.invoke(entry->id);
        check(!failedReturn.succeeded() && failedReturn.error().code == "WVM1222",
              "partial proxy factory failure did not cross the native boundary safely");
        check(scalar(registry, "vm_object_methods::Constructions", badgeArguments) == beforeBadgeConstructors + 1 &&
                  scalar(registry, "vm_object_methods::Destructions", badgeArguments) == beforeBadgeDestructors + 1,
              "failed method result materialization lost or repeated the returned object's Wio destructor");
        return 0;
    }
} // namespace

int main(int argc, char** argv)
{
    try
    {
        check(argc == 3 || argc == 4, "object proxy qualification requires bytecode and producer paths");
        qualifySelection();
        if (argc == 4 && std::string_view(argv[3]) == "references")
            return qualifyReferences(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "methods")
            return qualifyMethods(argv[1], argv[2]);
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
