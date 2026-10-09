#include "native_bridge_internal.h"
#include "native_object_internal.h"
#include "object_layout_internal.h"
#include "wio/ast/attribute_contract.h"
#include "wio/bytecode/codec.h"
#include "wio/bytecode/verifier.h"
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
    WioNativeAbiValue malformedBorrowOutput{};
    WioNativeAbiValue polymorphicWrongResult{};

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
        {
            auto trial = module;
            trial.types.getMutable(object).arguments = {integer};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "concrete generic object was rejected");
            const auto unresolved = trial.types.intern(Type{.kind = TypeKind::GenericParameter, .name = "T"});
            trial.types.getMutable(object).arguments = {unresolved};
            check(Emitter::eligibleTypes(trial).empty(), "open generic object was silently proxied");
            const auto nested = trial.types.intern(Type{.kind = TypeKind::Array, .arguments = {unresolved}});
            trial.types.getMutable(object).arguments = {nested};
            check(Emitter::eligibleTypes(trial).empty(), "nested unresolved generic identity was proxied");
            const auto constant =
                trial.types.intern(Type{.kind = TypeKind::ConstGenericParameter, .name = "N", .arguments = {integer}});
            trial.types.getMutable(object).arguments = {constant};
            check(Emitter::eligibleTypes(trial).empty(), "unresolved const generic identity was proxied");
            const auto pack = trial.types.intern(Type{.kind = TypeKind::GenericParameterPack, .name = "Args"});
            trial.types.getMutable(object).arguments = {pack};
            check(Emitter::eligibleTypes(trial).empty(), "unresolved generic pack identity was proxied");
            trial.types.getMutable(object).arguments = {integer};
            trial.functions[0].genericParameters = {unresolved};
            check(Emitter::eligibleTypes(trial).empty(), "open generic method was silently qualified");
        }
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
            check(Emitter::eligibleTypes(trial).empty(), "unproven borrowed method return was silently proxied");
        }
        {
            auto trial = module;
            auto& method = trial.functions[0];
            method.parameters[0].id = ValueId{0};
            method.returnType = receiver;
            trial.contract.reflection[0].methods[0].returnType = receiver;
            method.blocks = {
                {.id = BlockId{0}, .instructions = {{.opcode = lowered::Opcode::Return, .operands = {ValueId{0}}}}}};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "receiver-rooted result was rejected");
            method.parameters.push_back({.id = ValueId{1}, .type = receiver});
            method.blocks[0].instructions[0].operands = {ValueId{1}};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "parameter-rooted result was rejected");
            const auto view =
                trial.types.intern(Type{.kind = TypeKind::Reference, .arguments = {object}, .isMutable = false});
            method.parameters[1].type = view;
            check(Emitter::eligibleTypes(trial).empty(), "readonly parameter returned a mutable borrow");
            method.returnType = view;
            check(Emitter::eligibleTypes(trial) == std::set{object}, "readonly parameter result was rejected");
            method.parameters[1].type = receiver;
            method.blocks[0].instructions = {
                {.opcode = lowered::Opcode::Borrow, .result = ValueId{2}, .resultType = view, .operands = {ValueId{1}}},
                {.opcode = lowered::Opcode::Return, .operands = {ValueId{2}}}};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "readonly borrow forwarding was rejected");
            method.returnType = receiver;
            check(Emitter::eligibleTypes(trial).empty(), "readonly intermediate strengthened a returned borrow");
            method.returnType = view;
            method.blocks[0].instructions[0].opcode = lowered::Opcode::GlobalPlace;
            check(Emitter::eligibleTypes(trial).empty(), "unqualified global result was proxied");
            method.blocks[0].instructions[0].opcode = lowered::Opcode::FieldPlace;
            check(Emitter::eligibleTypes(trial).empty(), "unqualified projection result was proxied");
            method.blocks[0].instructions[0].opcode = lowered::Opcode::Borrow;
            method.parameters[1].type = object;
            check(Emitter::eligibleTypes(trial).empty(), "owned argument storage escaped as a borrowed result");
            method.parameters[1].type = receiver;
            method.parameters.push_back({.id = ValueId{2}, .type = trial.types.boolType()});
            method.returnType = receiver;
            method.blocks = {{.id = BlockId{0},
                              .instructions = {{.opcode = lowered::Opcode::CondJump,
                                                .operands = {ValueId{2}},
                                                .targets = {{.block = BlockId{1}, .arguments = {ValueId{0}}},
                                                            {.block = BlockId{1}, .arguments = {ValueId{1}}}}}}},
                             {.id = BlockId{1},
                              .parameters = {{.id = ValueId{3}, .type = receiver}},
                              .instructions = {{.opcode = lowered::Opcode::Return, .operands = {ValueId{3}}}}}};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "caller-rooted CFG merge was rejected");
            method.blocks[1].parameters[0].type = view;
            check(Emitter::eligibleTypes(trial).empty(), "readonly CFG merge strengthened a borrow");
            method.blocks[1].parameters[0].type = receiver;
            method.blocks[0].instructions[0].targets[1].arguments = {ValueId{99}};
            check(Emitter::eligibleTypes(trial).empty(), "unproven CFG predecessor escaped as a borrow");
            method.blocks[0].instructions[0].targets[1].arguments = {ValueId{3}};
            check(Emitter::eligibleTypes(trial).empty(), "cyclic borrow provenance was silently accepted");
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
            const auto nullable = trial.types.intern(Type{.kind = TypeKind::Nullable, .arguments = {object}});
            trial.functions[1].nativeBinding->parameters[0].type = nullable;
            trial.functions[0].parameters.push_back({.type = nullable});
            trial.functions[0].returnType = nullable;
            trial.contract.reflection[0].methods[0].returnType = nullable;
            check(Emitter::eligibleTypes(trial) == std::set{object},
                  "nullable object root, parameter or result was rejected");
            const auto borrowedNullable =
                trial.types.intern(Type{.kind = TypeKind::Reference, .arguments = {nullable}, .isMutable = true});
            trial.functions[0].parameters.back().type = borrowedNullable;
            check(Emitter::eligibleTypes(trial) == std::set{object}, "nullable ref method parameter was rejected");
            trial.types.getMutable(borrowedNullable).isMutable = false;
            check(Emitter::eligibleTypes(trial) == std::set{object}, "nullable view method parameter was rejected");
            trial.functions[0].returnType = borrowedNullable;
            check(Emitter::eligibleTypes(trial).empty(), "borrowed nullable method result was silently proxied");
            trial.functions[0].parameters.back().type = nullable;
            trial.functions[0].returnType =
                trial.types.intern(Type{.kind = TypeKind::Nullable, .arguments = {integer}});
            check(Emitter::eligibleTypes(trial).empty(), "nullable scalar method result was silently proxied");
        }
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
            const auto nullableDependency =
                trial.types.intern(Type{.kind = TypeKind::Nullable, .arguments = {dependency}});
            trial.functions[0].returnType = nullableDependency;
            trial.contract.reflection[0].methods[0].returnType = nullableDependency;
            check(Emitter::eligibleTypes(trial) == std::set{object, dependency},
                  "nullable-only method dependency missing from proxy closure");
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
                "cyclic inheritance was silently proxied");
        {
            auto trial = module;
            const auto base = trial.types.intern(Type{.kind = TypeKind::Named,
                                                      .name = "Base",
                                                      .nominalKind = NominalKind::Object,
                                                      .nominalRepresentation = NominalRepresentation::Wio,
                                                      .fields = {{.name = "seed", .type = integer}}});
            const auto contract = trial.types.intern(Type{.kind = TypeKind::Named,
                                                          .name = "IRead",
                                                          .nominalKind = NominalKind::Interface,
                                                          .nominalRepresentation = NominalRepresentation::Wio});
            trial.types.getMutable(object).baseTypes = {base, contract};
            check(Emitter::eligibleTypes(trial) == std::set{object}, "qualified concrete hierarchy was rejected");
            check(Emitter::hostedHierarchyTypes(trial, {object}) == std::set{object, base, contract},
                  "hosted lifecycle/wrapper closure lost non-materialized ancestors");
            trial.types.getMutable(base).fields[0].visibility = FieldVisibility::Public;
            check(Emitter::eligibleTypes(trial).empty(), "public ancestor field exposed an unsynchronized projection");
            trial.types.getMutable(base).fields[0].visibility = FieldVisibility::Private;
            trial.types.getMutable(base).fields[0].type = object;
            check(Emitter::eligibleTypes(trial).empty(), "ancestor object field ran a dummy lifecycle");
            trial.types.getMutable(base).fields[0].type = integer;
            trial.types.getMutable(base).baseTypes = {object};
            check(Emitter::eligibleTypes(trial).empty(), "indirect hierarchy cycle was proxied");
        }
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
            bridge::ObjectLayouts layouts;
            std::string error;
            if (!layouts.build(context->module, error))
                return;
            const auto* layout = layouts.get(object.aggregateType());
            if (!layout)
                return;
            for (auto iterator = layout->constructionOwners.rbegin(); iterator != layout->constructionOwners.rend();
                 ++iterator)
            {
                const auto owner = *iterator;
                const auto destructor = context->module.types[owner].destructor;
                if (destructor == bc::InvalidIndex)
                    continue;
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

    int qualifyNullable(const char* bytecode, const char* producer)
    {
        using NullableHandle = std::optional<wio::runtime::Ref<wio::runtime::RefCountedObject>>;
        using NullableCodec = abi::Codec<NullableHandle, 991>;
        for (const auto& absent :
             {NullableHandle{}, NullableHandle{wio::runtime::Ref<wio::runtime::RefCountedObject>{}}})
        {
            Wire encoded;
            encoded.value = NullableCodec::write(absent);
            check(encoded.value.kind == WIO_NATIVE_ABI_VOID && encoded.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE &&
                      encoded.value.typeId == 991 && !NullableCodec::read(encoded.value),
                  "nullable object codec did not normalize an empty C++ handle to null");
        }
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "nullable object method bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        check(getObjects && getObjects()->typeCount == 2, "nullable method-only dependency adapter missing");
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto findFunction = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == name; });
            check(found != decoded.module.functions.end(), "nullable qualification function missing");
            return found->id;
        };
        const auto entry = machine.invoke(findFunction("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "nullable object native/VM identity or lifecycle failed");
        const auto captured = machine.invoke(findFunction("CaptureEntry"));
        check(captured.succeeded(), "nullable method host capture failed");
        const auto token = *reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
        const auto reflected =
            std::ranges::find(decoded.module.contract.reflection, token.typeId, &bc::Reflection::stableTypeId);
        check(reflected != decoded.module.contract.reflection.end(), "nullable method reflection missing");
        const auto methodId = [&](const char* name)
        {
            const auto found = std::ranges::find_if(reflected->methods, [&](const auto& method)
                                                    { return decoded.module.string(method.name) == name; });
            check(found != reflected->methods.end(), "nullable qualification method missing");
            return found->stableId;
        };
        WioNativeAbiValue null{};
        null.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
        null.typeId = token.typeId;
        Wire output;
        check(token.invoke(&token, methodId("Echo"), &null, 1, &output.value, nullptr) == WIO_NATIVE_ABI_OK &&
                  output.value.kind == WIO_NATIVE_ABI_VOID && output.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE &&
                  output.value.typeId == token.typeId,
              "nullable method null wire was not canonical");
        check(token.invoke(&token, methodId("Required"), &null, 1, &output.value, nullptr) ==
                  WIO_NATIVE_ABI_TYPE_MISMATCH,
              "non-nullable object method accepted a null wire");
        for (int kind = 0; kind < 3; ++kind)
        {
            auto invalid = null;
            if (kind == 0)
                invalid.flags = 0;
            if (kind == 1)
                invalid.typeId ^= 1;
            if (kind == 2)
                invalid.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            check(token.invoke(&token, methodId("Echo"), &invalid, 1, &output.value, nullptr) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "malformed nullable object wire was accepted");
        }
        const auto types = std::span{getObjects()->types, getObjects()->typeCount};
        const auto adapter = std::ranges::find(types, token.typeId, &WioNativeObjectDescriptor::typeId);
        check(adapter != types.end(), "nullable owner adapter missing");
        Wire argument;
        check(adapter->create(&token, &argument.value, nullptr) == WIO_NATIVE_ABI_OK,
              "nullable object argument setup failed");
        Wire same;
        check(token.invoke(&token, methodId("Echo"), &argument.value, 1, &same.value, nullptr) == WIO_NATIVE_ABI_OK,
              "nullable method rejected a non-null object wire");
        WioNativeObjectHost identity{};
        check(adapter->host(&same.value, &identity, nullptr) == WIO_NATIVE_ABI_OK &&
                  identity.objectId == token.objectId && identity.owner.state == token.owner.state,
              "nullable method result changed original VM identity");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getRegistry, "nullable object producer registry missing");
        const auto& registry = *getRegistry();
        for (int kind = 0; kind < 5; ++kind)
        {
            auto invalid = null;
            if (kind == 0)
                invalid.flags = 0;
            if (kind == 1)
                invalid.typeId ^= 1;
            if (kind == 2)
                invalid.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
            if (kind == 3)
                invalid.owner.state = &invalid;
            if (kind == 4)
                invalid.kind = WIO_NATIVE_ABI_OBJECT;
            const std::array arguments{invalid};
            Wire rejected;
            check(call(registry, "vm_object_nullable::Echo", arguments, rejected.value) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "free native nullable object codec accepted a malformed null wire");
        }
        Wire cleared;
        check(call(registry, "vm_object_nullable::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "nullable method capture cleanup failed");

        const auto badge = std::ranges::find_if(decoded.module.contract.reflection, [&](const auto& type)
                                                { return decoded.module.string(type.logicalName) == "Badge"; });
        check(badge != decoded.module.contract.reflection.end(), "nullable factory result metadata missing");
        std::vector<WioNativeObjectDescriptor> failingTypes{types.begin(), types.end()};
        for (auto& type : failingTypes)
            if (type.typeId == badge->stableTypeId)
            {
                partialObjectFactory = type.create;
                type.create =
                    [](const WioNativeObjectHost* host, WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
                {
                    const auto status = partialObjectFactory(host, result, nullptr);
                    return status == WIO_NATIVE_ABI_OK ? WIO_NATIVE_ABI_EXCEPTION : status;
                }; // Deliberately retain an owned partial output on failure.
            }
        const WioNativeObjectApi failingApi{WIO_NATIVE_OBJECT_VERSION, static_cast<std::uint32_t>(failingTypes.size()),
                                            failingTypes.data()};
        wio::vm::Machine failedMachine{decoded.module};
        check(failedMachine.registerNativeRegistry(registry, failingApi, &error), error.c_str());
        const auto failedCapture = failedMachine.invoke(findFunction("CaptureEntry"));
        check(failedCapture.succeeded(), "nullable factory failure receiver capture failed");
        const auto failedToken =
            *reinterpret_cast<const WioNativeObjectHost*>(failedCapture.value().asUnsignedInteger());
        WioNativeAbiValue badgeKind{};
        badgeKind.kind = WIO_NATIVE_ABI_I64;
        badgeKind.payload.signedInteger = 1;
        const std::array badgeArguments{badgeKind};
        const auto beforeConstructors = scalar(registry, "vm_object_nullable::Constructions", badgeArguments);
        const auto beforeDestructors = scalar(registry, "vm_object_nullable::Destructions", badgeArguments);
        WioNativeAbiValue seed{};
        seed.kind = WIO_NATIVE_ABI_I64;
        seed.payload.signedInteger = 99;
        WioNativeAbiValue enabled{};
        enabled.kind = WIO_NATIVE_ABI_BOOL;
        enabled.payload.boolean = 1;
        const std::array makeArguments{seed, enabled};
        Wire failedOutput;
        check(failedToken.invoke(&failedToken, methodId("MakeBadge"), makeArguments.data(), makeArguments.size(),
                                 &failedOutput.value, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH &&
                  failedOutput.value.kind == WIO_NATIVE_ABI_VOID && !failedOutput.value.owner.state,
              "nullable result factory failure leaked its partial output across the boundary");
        check(failedMachine.invoke(findFunction("SafePoint")).succeeded(), "nullable cleanup safe point failed");
        check(scalar(registry, "vm_object_nullable::Constructions", badgeArguments) == beforeConstructors + 1 &&
                  scalar(registry, "vm_object_nullable::Destructions", badgeArguments) == beforeDestructors + 1,
              "nullable factory failure lost or repeated returned-object destruction");
        check(call(registry, "vm_object_nullable::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "nullable factory failure receiver cleanup failed");
        return 0;
    }

    int qualifyHierarchy(const char* bytecode, const char* producer)
    {
        {
            const std::array aliases{std::pair{"ReadOnly", "readonly"},
                                     std::pair{"Default", "default"},
                                     std::pair{"NoDefaultCtor", "no_default_ctor"},
                                     std::pair{"GenerateCtors", "generate_ctors"},
                                     std::pair{"From", "from"},
                                     std::pair{"From", "conversion::from"},
                                     std::pair{"Trust", "trust"},
                                     std::pair{"Final", "final"},
                                     std::pair{"Type", "type"},
                                     std::pair{"Type", "abi::type"},
                                     std::pair{"Native", "native"},
                                     std::pair{"CppHeader", "cpp::header"},
                                     std::pair{"CppName", "cpp::name"},
                                     std::pair{"Instantiate", "instantiate"},
                                     std::pair{"Specialize", "specialize"},
                                     std::pair{"Apply", "apply"},
                                     std::pair{"Export", "export"},
                                     std::pair{"Export", "export::c"},
                                     std::pair{"Command", "command"},
                                     std::pair{"Event", "event"},
                                     std::pair{"ModuleApiVersion", "module::api_version"},
                                     std::pair{"ModuleLoad", "module::load"},
                                     std::pair{"ModuleUpdate", "module::update"},
                                     std::pair{"ModuleUnload", "module::unload"},
                                     std::pair{"ModuleSaveState", "module::save_state"},
                                     std::pair{"ModuleRestoreState", "module::restore_state"},
                                     std::pair{"Start", "application::start"},
                                     std::pair{"Update", "application::update"},
                                     std::pair{"Close", "application::close"},
                                     std::pair{"Fixed", "application::fixed"},
                                     std::pair{"After", "application::after"},
                                     std::pair{"Main", "application::main"},
                                     std::pair{"Worker", "application::worker"}};
            for (const auto& [name, alias] : aliases)
            {
                const auto canonical = wio::resolveBuiltinAttribute(std::string{"std::attribute::"} + name);
                check(canonical && wio::resolveBuiltinAttribute(name) == canonical &&
                          wio::resolveBuiltinAttribute(alias) == canonical,
                      "a canonical built-in attribute and its alias resolved to different contracts");
            }
            check(!wio::resolveBuiltinAttribute("NATIVE"), "built-in aliases became unrestricted case folding");
        }
        {
            bc::Module graph;
            graph.types = {{.kind = 28, .nominalKind = 3},
                           {.kind = 28, .nominalKind = 2, .baseTypes = {0}, .fields = {{.type = 4}}},
                           {.kind = 28, .nominalKind = 2, .baseTypes = {1, 0}, .fields = {{.type = 4}}},
                           {.kind = 28, .nominalKind = 2, .baseTypes = {2}},
                           {.kind = 8}};
            bridge::ObjectLayouts layouts;
            std::string error;
            check(layouts.build(graph, error), "valid object layout graph rejected");
            const auto* leaf = layouts.get(3);
            check(leaf && leaf->fieldTypes.size() == 2 && leaf->offsets.at(1) != leaf->offsets.at(2) &&
                      leaf->constructionOwners == std::vector<std::uint32_t>({0, 1, 2, 3}),
                  "object layout overlapped base fields or repeated diamond owners");
            graph.types[0].baseTypes = {3};
            check(!layouts.build(graph, error), "cyclic object layout graph accepted");
            graph.types[0].baseTypes = {bc::InvalidIndex};
            check(!layouts.build(graph, error), "invalid base layout index accepted");
            graph.types[0].baseTypes = {4};
            check(!layouts.build(graph, error), "non-nominal inheritance base accepted");
            check(!layouts.get(3), "failed graph validation published a partial layout");
            graph.types.assign(129, bc::Type{.kind = 28, .nominalKind = 2});
            for (std::uint32_t index = 1; index < graph.types.size(); ++index)
                graph.types[index].baseTypes = {index - 1};
            check(!layouts.build(graph, error), "cached bases bypassed the inheritance depth limit");
            graph.types.pop_back();
            check(layouts.build(graph, error), "128-level inheritance boundary rejected");
        }
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "hierarchy bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getObjects()->typeCount == 2 && getRegistry, "concrete hierarchy adapters missing");
        const auto& api = *getObjects();
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto invoke = [&](const char* name)
        {
            const auto function = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                       { return decoded.module.string(item.name) == name; });
            check(function != decoded.module.functions.end(), "hierarchy entry missing");
            auto result = machine.invoke(function->id);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            return result;
        };
        check(invoke("Entry").value().asSignedInteger() == 0, "native base/interface dispatch or lifecycle failed");
        std::uint64_t inheritedMethod = 0;
        std::set<std::uint64_t> concreteTypes;
        const std::array entries{"CaptureDerivedEntry", "CaptureLeafEntry"};
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            Wire reset;
            check(call(registry, "vm_object_hierarchy::ResetOrder", {}, reset.value) == WIO_NATIVE_ABI_OK,
                  "hierarchy order reset failed");
            const auto captured = invoke(entries[index]);
            const auto token = *reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
            const auto adapters = std::span{api.types, api.typeCount};
            const auto adapter = std::ranges::find(adapters, token.typeId, &WioNativeObjectDescriptor::typeId);
            check(adapter != adapters.end(), "captured concrete hierarchy adapter missing");
            const auto reflected =
                std::ranges::find(decoded.module.contract.reflection, token.typeId, &bc::Reflection::stableTypeId);
            check(reflected != decoded.module.contract.reflection.end(), "hierarchy reflection missing");
            const auto methodId = [&](const char* name)
            {
                const auto method = std::ranges::find_if(reflected->methods, [&](const auto& item)
                                                         { return decoded.module.string(item.name) == name; });
                check(method != reflected->methods.end(), "inherited reflected method missing");
                return method->stableId;
            };
            check(concreteTypes.insert(token.typeId).second, "different concrete types shared a type identity");
            if (index == 0)
                inheritedMethod = methodId("BaseOnly");
            else
                check(methodId("BaseOnly") == inheritedMethod,
                      "an unchanged inherited declaration lost its stable method identity");
            Wire proxy;
            check(adapter->create(&token, &proxy.value, nullptr) == WIO_NATIVE_ABI_OK,
                  "secondary hierarchy proxy creation failed");
            WioNativeObjectHost recovered{};
            check(adapter->host(&proxy.value, &recovered, nullptr) == WIO_NATIVE_ABI_OK &&
                      recovered.owner.state == token.owner.state && recovered.objectId == token.objectId,
                  "hierarchy proxy recovered a different host");
            Wire read;
            check(token.invoke(&token, methodId("Read"), nullptr, 0, &read.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      read.value.payload.signedInteger == (index ? 137 : 37),
                  "leaf/derived override dispatched to a base declaration");
            WioNativeAbiValue amount{};
            amount.kind = WIO_NATIVE_ABI_I64;
            amount.payload.signedInteger = 2;
            Wire base;
            check(token.invoke(&token, methodId("BaseOnly"), &amount, 1, &base.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      base.value.payload.signedInteger == 9,
                  "inherited method did not update original base field storage");
            Wire super;
            check(token.invoke(&token, methodId("BaseRead"), nullptr, 0, &super.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      super.value.payload.signedInteger == 9,
                  "super call accidentally redispatched through the override");
            Weak weak;
            check(adapter->weak(&proxy.value, &weak.value, nullptr) == WIO_NATIVE_ABI_OK,
                  "hierarchy proxy weak storage failed");
            Wire cleared;
            check(call(registry, "vm_object_hierarchy::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
                  "hierarchy capture cleanup failed");
            WioNativeAbiReleaseValue(&proxy.value);
            invoke("SafePoint");
            Wire expired;
            check(adapter->lock(&weak.value, &expired.value, nullptr) == WIO_NATIVE_ABI_STALE_HANDLE &&
                      expired.value.kind == WIO_NATIVE_ABI_VOID,
                  "hierarchy proxy weak storage kept a host claim alive");
            check(scalar(registry, "vm_object_hierarchy::Order") == (index ? 321 : 21),
                  "hosted base/derived destructors were skipped, duplicated or reordered");
        }
        for (std::int32_t kind = 0; kind < 3; ++kind)
        {
            WioNativeAbiValue argument{};
            argument.kind = WIO_NATIVE_ABI_I64;
            argument.payload.signedInteger = kind;
            const auto arguments = std::span{&argument, 1};
            const auto count = kind == 2 ? 31 : 62;
            check(scalar(registry, "vm_object_hierarchy::Constructions", arguments) == count &&
                      scalar(registry, "vm_object_hierarchy::Destructions", arguments) == count,
                  "hierarchy proxy ran a duplicate constructor/destructor body");
        }
        const auto findFunction = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                    { return decoded.module.string(item.name) == name; });
            check(found != decoded.module.functions.end(), "failure-path entry missing");
            return found->id;
        };
        const auto failConstruction = [&](const char* entry, std::int32_t failingKind, std::int32_t order)
        {
            Wire ignored;
            check(call(registry, "vm_object_hierarchy::ResetOrder", {}, ignored.value) == WIO_NATIVE_ABI_OK,
                  "failure-path order reset failed");
            WioNativeAbiValue kind{};
            kind.kind = WIO_NATIVE_ABI_I64;
            kind.payload.signedInteger = failingKind;
            const auto args = std::span{&kind, 1};
            check(call(registry, "vm_object_hierarchy::SetFailure", args, ignored.value) == WIO_NATIVE_ABI_OK,
                  "constructor failure injection failed");
            const auto failed = machine.invoke(findFunction(entry));
            kind.payload.signedInteger = -1;
            check(call(registry, "vm_object_hierarchy::SetFailure", args, ignored.value) == WIO_NATIVE_ABI_OK,
                  "constructor failure reset failed");
            check(!failed.succeeded() && failed.error().cleanupFailures.empty(),
                  "constructor failure escaped or generated a secondary cleanup failure");
            check(scalar(registry, "vm_object_hierarchy::Order") == order,
                  "partial construction destroyed an incomplete base or skipped a completed owner");
        };
        failConstruction("CaptureDerivedEntry", 0, 0);
        failConstruction("CaptureDerivedEntry", 1, 21);
        failConstruction("CaptureLeafEntry", 1, 1);
        failConstruction("CaptureLeafEntry", 2, 321);
        return 0;
    }

    int qualifyObjectAny(const char* bytecode, const char* producer, bool nullable = false)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "object any bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getAny = library.symbol<WioGetNativeAnyApiFn>("WioGetNativeAnyApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getAny && getRegistry && getObjects()->typeCount == (nullable ? 4u : 5u),
              "any-only boundaries lost concrete/generic object adapters");
        const auto& registry = *getRegistry();
        const auto& objects = *getObjects();
        const auto& any = *getAny();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                    { return decoded.module.string(item.name) == name; });
            check(found != decoded.module.functions.end(), "object any entry missing");
            return found->id;
        };
        const auto reflection = [&](const char* name) -> const bc::Reflection&
        {
            const auto found = std::ranges::find_if(decoded.module.contract.reflection, [&](const auto& item)
                                                    { return decoded.module.string(item.logicalName) == name; });
            check(found != decoded.module.contract.reflection.end(), "object any metadata missing");
            return *found;
        };
        const auto entry = machine.invoke(function("Entry"));
        if (!entry.succeeded())
            throw std::runtime_error(entry.error().code + ": " + entry.error().message);
        check(entry.value().asSignedInteger() == 0, "object any lifecycle/identity entry failed");
        {
            const auto escaped = machine.invoke(function("Escape"));
            check(escaped.succeeded() && escaped.value().kind() == wio::vm::Value::Kind::Any &&
                      escaped.value().anyType() == reflection("Leaf").type && escaped.value().anyPayload() &&
                      escaped.value().anyPayload()->kind() == wio::vm::Value::Kind::Object &&
                      escaped.value().anyPayload()->aggregateType() == reflection("Leaf").type,
                  "native any decoded a foreign/sliced object instead of its VM host");
        }
        check(machine.invoke(function("SafePoint")).succeeded(), "object any cleanup safe point failed");
        if (nullable)
        {
            const auto empty = machine.invoke(function("EscapeNull"));
            check(empty.succeeded() && empty.value().kind() == wio::vm::Value::Kind::Null,
                  "nullable null object became an invented typed Any cell");
            for (const auto* name : {"NullCast", "NullOpaqueCast"})
            {
                const auto invalid = machine.invoke(function(name));
                check(!invalid.succeeded() && invalid.error().code == "WVM1184",
                      "null Any checked cast fabricated a nullable payload");
            }
        }
        wio::vm::Machine missing{decoded.module};
        check(!missing.registerNativeRegistry(registry, any, &error) &&
                  error.find("unsupported structural materialization") != std::string::npos,
              "object any accepted registration without qualified object adapters");

        std::vector<WioNativeAnyPayloadDescriptor> payloads(any.payloads, any.payloads + any.payloadCount);
        const auto leaf =
            std::ranges::find(payloads, reflection("Leaf").stableTypeId, &WioNativeAnyPayloadDescriptor::typeId);
        check(leaf != payloads.end(), "object any payload descriptor missing");
        static WioNativeAnyReadFn originalRead;
        static std::uint64_t forgedType;
        originalRead = leaf->read;
        forgedType = reflection("Sibling").stableTypeId;
        leaf->read = [](const WioNativeAbiValue* value, WioNativeAbiValue* result, bool* matched,
                        WioNativeAbiFailure* failure) noexcept
        {
            const auto status = originalRead(value, result, matched, failure);
            if (status == WIO_NATIVE_ABI_OK && matched && *matched)
                result->typeId = forgedType;
            return status;
        };
        auto forgedAny = any;
        forgedAny.payloads = payloads.data();
        wio::vm::Machine forged{decoded.module};
        check(forged.registerNativeRegistry(registry, objects, &error, &forgedAny), error.c_str());
        const auto invalid = forged.invoke(function("Escape"));
        check(!invalid.succeeded() && invalid.error().code == "WVM1224",
              "forged object any payload escaped concrete type/operations validation");
        check(forged.invoke(function("SafePoint")).succeeded(), "forged any cleanup failed");
        for (std::int32_t index = 0; index < 4; ++index)
        {
            WioNativeAbiValue kind{};
            kind.kind = WIO_NATIVE_ABI_I64;
            kind.payload.signedInteger = index;
            check(scalar(registry, "vm_object_any::Constructions", std::span{&kind, 1}) ==
                      scalar(registry, "vm_object_any::Destructions", std::span{&kind, 1}),
                  "object any final release lost or repeated Wio lifecycle");
        }
        return 0;
    }

    void qualifyPolymorphicReferenceSelection()
    {
        using namespace wio::wir;
        using Emitter = wio::codegen::WirNativeObjectEmitter;
        lowered::Module module;
        const auto reader = module.types.internNominal(
            Type{.kind = TypeKind::Named, .name = "IRead", .nominalKind = NominalKind::Interface});
        const auto base =
            module.types.internNominal(Type{.kind = TypeKind::Named,
                                            .name = "Base",
                                            .nominalKind = NominalKind::Object,
                                            .baseTypes = {reader},
                                            .fields = {{.name = "value", .type = module.types.i32Type()}}});
        const auto leaf = module.types.internNominal(
            Type{.kind = TypeKind::Named, .name = "Leaf", .nominalKind = NominalKind::Object, .baseTypes = {base}});
        const auto sibling =
            module.types.internNominal(Type{.kind = TypeKind::Named,
                                            .name = "Sibling",
                                            .nominalKind = NominalKind::Object,
                                            .baseTypes = {reader},
                                            .fields = {{.name = "value", .type = module.types.i32Type()}}});
        const auto borrowed =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {reader}, .isMutable = true});
        const auto owner = module.types.internNominal(
            Type{.kind = TypeKind::Named,
                 .name = "Owner",
                 .nominalKind = NominalKind::Object,
                 .methods = {{.name = "Borrow", .returnType = borrowed, .function = FunctionId{0}}}});
        const auto self =
            module.types.intern(Type{.kind = TypeKind::Reference, .arguments = {owner}, .isMutable = true});
        module.functions = {
            {.id = FunctionId{0},
             .name = "Borrow",
             .parameters = {{.id = ValueId{0}, .type = self}, {.id = ValueId{1}, .type = borrowed}},
             .returnType = borrowed,
             .ownerType = owner,
             .blocks = {{.id = BlockId{0},
                         .instructions = {{.opcode = lowered::Opcode::Return, .operands = {ValueId{1}}}}}},
             .isMethod = true},
            {.id = FunctionId{1},
             .name = "NativeOwner",
             .returnType = module.types.i32Type(),
             .nativeBinding = NativeBinding{.parameters = {{.type = owner}}}}};
        for (const auto type : {reader, base, leaf, sibling, owner})
            module.contract.reflection.push_back({.stableTypeId = 9000 + type.value(), .type = type});
        module.contract.reflection.back().methods = {
            {.stableId = 1001, .name = "Borrow", .function = FunctionId{0}, .returnType = borrowed}};
        check(Emitter::eligibleTypes(module) == std::set{base, leaf, sibling, owner},
              "method-only interface signature lost descendant adapters");
        module.types.getMutable(base).fields[0].visibility = FieldVisibility::Public;
        check(Emitter::eligibleTypes(module) == std::set{sibling, owner},
              "interface signature did not preserve only qualified implementation families");
        module.types.getMutable(sibling).fields[0].visibility = FieldVisibility::Public;
        check(Emitter::eligibleTypes(module).empty(),
              "interface method was qualified without any compatible concrete implementation");
    }

    int qualifyPolymorphicReferences(const char* bytecode, const char* producer)
    {
        qualifyPolymorphicReferenceSelection();
        // Same token identity does not permit different C++ storage shapes,
        // even if the wire encodings happen to have the same static id/kind.
        const auto place = []<class T>(T value, const void* address)
        {
            struct Cell
            {
                T value;
                const void* address;
                bool canRebind() const
                {
                    return true;
                }
                const void* borrowIdentity() const
                {
                    return address;
                }
                T read() const
                {
                    return value;
                }
                void write(T replacement)
                {
                    value = replacement;
                }
            };
            return Cell{value, address};
        };
        std::int32_t identity = 0;
        abi::ObjectReferences<2> contracts;
        contracts.borrow<std::int32_t, 17>(0, place(std::int32_t{3}, &identity), true);
        bool rejected = false;
        try
        {
            contracts.borrow<std::uint32_t, 17>(1, place(std::uint32_t{3}, &identity), false);
        }
        catch (const abi::Error& error)
        {
            rejected = error.status == WIO_NATIVE_ABI_TYPE_MISMATCH;
        }
        check(rejected, "producer reference cells merged different static storage types");
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "polymorphic reference bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getObjects()->typeCount == 4 && getRegistry, "polymorphic reference adapters missing");
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto invoke = [&](const char* name)
        {
            const auto function = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                       { return decoded.module.string(item.name) == name; });
            check(function != decoded.module.functions.end(), "polymorphic reference entry missing");
            auto result = machine.invoke(function->id);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            return result;
        };
        check(invoke("Entry").value().asSignedInteger() == 0, "polymorphic reference alias/lifecycle checks failed");
        const auto ownerResult = invoke("CaptureOwnerEntry");
        const auto leafResult = invoke("CaptureLeafEntry");
        const auto owner = *reinterpret_cast<const WioNativeObjectHost*>(ownerResult.value().asUnsignedInteger());
        const auto leaf = *reinterpret_cast<const WioNativeObjectHost*>(leafResult.value().asUnsignedInteger());
        const auto reflection = [&](const char* name) -> const bc::Reflection&
        {
            const auto found = std::ranges::find_if(decoded.module.contract.reflection, [&](const auto& type)
                                                    { return decoded.module.string(type.logicalName) == name; });
            check(found != decoded.module.contract.reflection.end(), "reference contract metadata missing");
            return *found;
        };
        const auto& ownerType = reflection("Owner");
        const auto method = [&](const char* name)
        {
            const auto found = std::ranges::find_if(ownerType.methods, [&](const auto& item)
                                                    { return decoded.module.string(item.name) == name; });
            check(found != ownerType.methods.end(), "reference method metadata missing");
            return found->stableId;
        };
        const auto adapters = std::span{getObjects()->types, getObjects()->typeCount};
        const auto adapter = std::ranges::find(adapters, leaf.typeId, &WioNativeObjectDescriptor::typeId);
        check(adapter != adapters.end(), "concrete reference adapter missing");
        {
            Wire cell;
            check(adapter->create(&leaf, &cell.value, nullptr) == WIO_NATIVE_ABI_OK, "reference cell setup failed");
            WioNativeAbiValue reference{};
            reference.kind = WIO_NATIVE_ABI_REFERENCE;
            reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
            reference.payload.mutableSlice = {&cell.value, sizeof(WioNativeAbiValue)};
            const auto verify = [&](const char* name, std::uint64_t staticId, bool writable)
            {
                auto argument = reference;
                if (!writable)
                    argument.flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
                Wire output;
                check(owner.invoke(&owner, method(name), &argument, 1, &output.value, nullptr) == WIO_NATIVE_ABI_OK &&
                          output.value.kind == WIO_NATIVE_ABI_REFERENCE && output.value.typeId == staticId &&
                          output.value.payload.mutableSlice.data == &cell.value &&
                          output.value.payload.mutableSlice.size == sizeof(WioNativeAbiValue) &&
                          !output.value.owner.state && !output.value.owner.ops &&
                          output.value.flags ==
                              (WIO_NATIVE_ABI_VALUE_BORROWED | (writable ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0u)) &&
                          (cell.value.kind == WIO_NATIVE_ABI_VOID || cell.value.typeId == leaf.typeId),
                      "borrowed polymorphic result lost the caller cell or concrete identity");
            };
            verify("BorrowBase", reflection("Base").stableTypeId, true);
            verify("ViewBase", reflection("Base").stableTypeId, false);
            verify("BorrowReader", reflection("IRead").stableTypeId, true);
            verify("ViewReader", reflection("IRead").stableTypeId, false);
            verify("NullableBase", reflection("Base").stableTypeId, true);
            verify("NullableReader", reflection("IRead").stableTypeId, true);
            Wire output;
            auto readonly = reference;
            readonly.flags &= ~WIO_NATIVE_ABI_VALUE_MUTABLE;
            check(owner.invoke(&owner, method("BorrowBase"), &readonly, 1, &output.value, nullptr) ==
                      WIO_NATIVE_ABI_INVALID_ARGUMENT,
                  "readonly polymorphic cell became writable");
            auto malformed = reference;
            malformed.payload.mutableSlice.size -= 1;
            check(owner.invoke(&owner, method("BorrowBase"), &malformed, 1, &output.value, nullptr) ==
                      WIO_NATIVE_ABI_INVALID_ARGUMENT,
                  "malformed polymorphic cell accepted");
            const auto concreteId = cell.value.typeId;
            cell.value.typeId = reflection("Sibling").stableTypeId;
            check(owner.invoke(&owner, method("BorrowBase"), &reference, 1, &output.value, nullptr) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "incompatible concrete reference reached a Base method");
            check(owner.invoke(&owner, method("BorrowReader"), &reference, 1, &output.value, nullptr) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "forged concrete reference operations were accepted");
            cell.value.typeId = concreteId;
            const std::array mixed{reference, readonly};
            check(owner.invoke(&owner, method("Mixed"), mixed.data(), mixed.size(), &output.value, nullptr) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "different static cell contracts were silently merged");
            const std::array nullableMixed{reference, reference};
            check(call(registry, "vm_object_polymorphic_references::MixedNullable", nullableMixed, output.value) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "nullable and non-nullable native storage were merged");
            WioNativeAbiReleaseValue(&cell.value);
            cell.value.kind = WIO_NATIVE_ABI_VOID;
            cell.value.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
            cell.value.typeId = reflection("IRead").stableTypeId;
            verify("NullableReader", reflection("IRead").stableTypeId, true);
        }
        Wire cleared;
        check(call(registry, "vm_object_polymorphic_references::ClearCaptures", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "polymorphic reference claims did not clear");
        invoke("SafePoint");
        WioNativeAbiValue kind{};
        kind.kind = WIO_NATIVE_ABI_I64;
        const std::array expected{61, 31, 30};
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            kind.payload.signedInteger = static_cast<std::int64_t>(index);
            check(scalar(registry, "vm_object_polymorphic::Constructions", std::span{&kind, 1}) == expected[index] &&
                      scalar(registry, "vm_object_polymorphic::Destructions", std::span{&kind, 1}) == expected[index],
                  "polymorphic reference final release was lost or repeated");
        }
        return 0;
    }

    int qualifyPolymorphic(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "polymorphic bytecode decode failed");
        auto library = std::make_shared<Library>(std::filesystem::u8path(producer));
        const auto getObjects = library->symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library->symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getRegistry && getObjects()->typeCount == 4,
              "base/interface-only boundaries did not discover all concrete descendants");
        const auto& api = *getObjects();
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto function = [&](const char* name)
        {
            const auto found = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                    { return decoded.module.string(item.name) == name; });
            check(found != decoded.module.functions.end(), "polymorphic entry missing");
            return found->id;
        };
        const auto entry = machine.invoke(function("Entry"));
        check(entry.succeeded() && entry.value().asSignedInteger() == 0, "polymorphic lifecycle/identity gate failed");
        const auto made = machine.invoke(function("MakeLeaf"));
        check(made.succeeded(), "leaf construction failed");
        const std::array arguments{made.value()};
        const auto recovered = machine.invoke(function("Roundtrip"), arguments);
        check(recovered.succeeded() && recovered.value().aggregateType() == made.value().aggregateType() &&
                  recovered.value().objectIdentity() == made.value().objectIdentity(),
              "base-returning native roundtrip erased concrete type or changed VM identity");
        const auto sibling = machine.invoke(function("MakeSibling"));
        check(sibling.succeeded(), "sibling construction failed");
        const std::array siblingArguments{sibling.value()};
        const auto rejected = machine.invoke(function("Roundtrip"), siblingArguments);
        check(!rejected.succeeded() && rejected.error().code == "WVM1223" &&
                  rejected.error().message.find("not assignable") != std::string::npos,
              "unrelated VM sibling was accepted as a native Base parameter");

        const auto leafType = made.value().aggregateType();
        std::vector<WioNativeObjectDescriptor> missing;
        for (const auto& adapter : std::span{api.types, api.typeCount})
        {
            const auto reflected =
                std::ranges::find(decoded.module.contract.reflection, adapter.typeId, &bc::Reflection::stableTypeId);
            if (reflected != decoded.module.contract.reflection.end() && reflected->type != leafType)
                missing.push_back(adapter);
        }
        const WioNativeObjectApi missingApi{WIO_NATIVE_OBJECT_VERSION, static_cast<std::uint32_t>(missing.size()),
                                            missing.data()};
        wio::vm::Machine missingMachine{decoded.module};
        check(missingMachine.registerNativeRegistry(registry, missingApi, &error), error.c_str());
        const auto missingResult = missingMachine.invoke(function("Roundtrip"), arguments);
        check(!missingResult.succeeded() && missingResult.error().code == "WVM1223" &&
                  missingResult.error().message.find("concrete object has no qualified") != std::string::npos,
              "missing concrete adapter silently sliced the object to Base");

        const auto proxy = [&](const char* name, const char* make)
        {
            auto context = std::make_shared<Context>();
            context->module = decoded.module;
            context->machine = std::make_shared<wio::vm::Machine>(context->module);
            context->library = library;
            for (const auto& reflected : context->module.contract.reflection)
                if (context->module.string(reflected.logicalName) == name)
                    context->objectType = &reflected;
            check(context->objectType, "polymorphic proxy reflection missing");
            check(context->machine->loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
            auto object = context->machine->invoke(function(make));
            check(object.succeeded(), "proxy host construction failed");
            auto* host = new Host{.context = context, .object = object.value()};
            WioNativeAbiHandle claim{host, &operations, 19};
            const WioNativeObjectHost token{.typeId = context->objectType->stableTypeId,
                                            .objectId = 801,
                                            .generation = 19,
                                            .owner = claim,
                                            .invoke = &dispatch};
            const auto adapters = std::span{api.types, api.typeCount};
            const auto adapter = std::ranges::find(adapters, token.typeId, &WioNativeObjectDescriptor::typeId);
            check(adapter != adapters.end(), "concrete proxy adapter missing");
            auto wire = std::make_unique<Wire>();
            const auto status = adapter->create(&token, &wire->value, nullptr);
            WioNativeAbiRelease(&claim);
            check(status == WIO_NATIVE_ABI_OK, "concrete proxy materialization failed");
            return wire;
        };
        auto leafWire = proxy("Leaf", "MakeLeaf");
        auto siblingWire = proxy("Sibling", "MakeSibling");
        for (const char* symbol :
             {"vm_object_polymorphic::EchoBase", "vm_object_polymorphic::EchoReader",
              "vm_object_polymorphic::EchoNullableBase", "vm_object_polymorphic::EchoNullableReader"})
        {
            Wire output;
            check(call(registry, symbol, std::span{&leafWire->value, 1}, output.value) == WIO_NATIVE_ABI_OK &&
                      output.value.typeId == leafWire->value.typeId &&
                      output.value.payload.handle.state == leafWire->value.payload.handle.state &&
                      output.value.payload.handle.ops == leafWire->value.payload.handle.ops,
                  "generated base/interface codec retagged or reallocated a concrete proxy");
        }
        check(scalar(registry, "vm_object_polymorphic::ReadBase", std::span{&leafWire->value, 1}) == 137 &&
                  scalar(registry, "vm_object_polymorphic::ReadReader", std::span{&leafWire->value, 1}) == 137,
              "generated polymorphic thunk lost concrete virtual dispatch");
        Wire invalid;
        check(call(registry, "vm_object_polymorphic::EchoBase", std::span{&siblingWire->value, 1}, invalid.value) ==
                      WIO_NATIVE_ABI_TYPE_MISMATCH &&
                  invalid.value.kind == WIO_NATIVE_ABI_VOID,
              "generated Base codec accepted an unrelated concrete wire");
        auto forged = leafWire->value;
        forged.payload.handle.ops = siblingWire->value.payload.handle.ops;
        check(call(registry, "vm_object_polymorphic::EchoReader", std::span{&forged, 1}, invalid.value) ==
                  WIO_NATIVE_ABI_TYPE_MISMATCH,
              "polymorphic codec trusted typeId without matching concrete handle operations");

        std::vector<WioNativeAbiFunctionDescriptor> badFunctions(registry.functions,
                                                                 registry.functions + registry.functionCount);
        for (auto& descriptor : badFunctions)
            if (std::string_view(descriptor.nativeSymbol) == "vm_object_polymorphic::EchoBase")
                descriptor.thunk = [](const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                      WioNativeAbiFailure*) noexcept
                {
                    *result = polymorphicWrongResult;
                    WioNativeAbiRetain(result->payload.handle);
                    return WIO_NATIVE_ABI_OK;
                };
        const WioNativeAbiRegistry badRegistry{WIO_NATIVE_ABI_VERSION, static_cast<std::uint32_t>(badFunctions.size()),
                                               badFunctions.data()};
        wio::vm::Machine badMachine{decoded.module};
        check(badMachine.registerNativeRegistry(badRegistry, api, &error), error.c_str());
        polymorphicWrongResult = siblingWire->value;
        const auto badResult = badMachine.invoke(function("Roundtrip"), arguments);
        polymorphicWrongResult = {};
        check(!badResult.succeeded() && badResult.error().code == "WVM1224" &&
                  badResult.error().message.find("not assignable") != std::string::npos,
              "VM accepted an incompatible concrete native result as Base");
        return 0;
    }

    int qualifyConcreteGenerics(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "generic object bytecode decode failed");
        {
            bc::Module graph;
            graph.strings = {"Pack"};
            graph.types = {{.kind = 28, .arguments = {1}},
                           {.kind = 28, .arguments = {0}},
                           {.kind = 21},
                           {.kind = 28, .arguments = {4, 2}},
                           {.kind = 28, .arguments = {3}},
                           {.kind = 29, .arguments = {4}},
                           {.kind = 28, .baseTypes = {4}},
                           {.kind = 28, .fields = {{.type = 5}}},
                           {.kind = 31, .extentParameter = 2},
                           {.kind = 28, .arguments = {bc::InvalidIndex}},
                           {.kind = 24},
                           {.kind = 25, .arguments = {0}},
                           {.kind = 26, .arguments = {2}},
                           {.kind = 27, .arguments = {0}},
                           {.kind = 25},
                           {.kind = 26, .name = 0},
                           {.kind = 27, .name = 0},
                           {.kind = 22},
                           {.kind = 23}};
            check(graph.openTypes() == std::vector<bool>({false, false, true, true, true, true, true, true, true, false,
                                                          true, false, true, false, false, true, true, true, false}),
                  "type openness lost recursive, reference, field, base or extent dependencies");
        }
        {
            auto encoded = bc::encode(decoded.module);
            check(encoded[10] == std::byte{1} && encoded[11] == std::byte{0},
                  "generic declarations were emitted without WIOB 1.1 versioning");
            encoded[10] = std::byte{2};
            check(!bc::decode(encoded).succeeded(), "unknown future bytecode minor was accepted");
            encoded[10] = std::byte{0};
            check(!bc::decode(encoded).succeeded(), "template declaration was accepted as WIOB 1.0");
            auto legacy = bc::encode(bc::Module{});
            legacy[10] = std::byte{0};
            check(bc::decode(legacy).succeeded(), "WIOB 1.0 without template declarations lost loader compatibility");
        }
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getObjects()->typeCount == 3 && getRegistry, "generic object producer adapters missing");
        const auto& api = *getObjects();
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto invoke = [&](const char* name)
        {
            const auto function = std::ranges::find_if(decoded.module.functions, [&](const auto& item)
                                                       { return decoded.module.string(item.name) == name; });
            check(function != decoded.module.functions.end(), "generic object entry missing");
            auto result = machine.invoke(function->id);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            return result;
        };
        check(invoke("Entry").value().asSignedInteger() == 0, "generic object identity/lifecycle check failed");
        const auto open = std::ranges::find_if(decoded.module.functions,
                                               [&](const auto& function)
                                               {
                                                   return decoded.module.string(function.name) == "TagValue" &&
                                                          (function.flags & bc::FunctionTemplateDeclaration) != 0;
                                               });
        check(open != decoded.module.functions.end() && open->blocks.empty(),
              "open generic method leaked executable code into bytecode");
        const auto invalidInvocation = machine.invoke(open->id);
        check(!invalidInvocation.succeeded() && invalidInvocation.error().code == "WVM1240",
              "VM attempted to execute an open generic declaration");
        {
            auto asyncDeclaration = decoded.module;
            asyncDeclaration.functions[open->id].flags |= 0x0001u;
            check(bc::Verifier{}.verify(asyncDeclaration).succeeded(), "async template declaration metadata rejected");
            wio::vm::Machine asyncMachine{asyncDeclaration};
            const auto result = asyncMachine.invoke(open->id);
            check(!result.succeeded() && result.error().code == "WVM1240",
                  "async template invocation created a task instead of rejecting the declaration");
        }
        {
            auto malformed = decoded.module;
            malformed.functions[open->id].flags &= ~bc::FunctionTemplateDeclaration;
            check(!bc::Verifier{}.verify(malformed).succeeded(), "bodyless template lost its declaration marker");
            malformed = decoded.module;
            malformed.functions[open->id].blocks = {{.id = 0, .instructions = {{.opcode = bc::Opcode::Unreachable}}}};
            check(!bc::Verifier{}.verify(malformed).succeeded(), "template declaration accepted an executable body");
            malformed = decoded.module;
            const auto closed = std::ranges::find_if(malformed.functions, [&](const auto& function)
                                                     { return malformed.string(function.name) == "Entry"; });
            check(closed != malformed.functions.end(), "concrete entry missing");
            closed->flags |= bc::FunctionTemplateDeclaration;
            closed->blocks.clear();
            check(!bc::Verifier{}.verify(malformed).succeeded(), "concrete function forged a template declaration");
        }
        std::set<std::uint64_t> identities;
        const std::array entries{"CaptureIntEntry", "CaptureOtherEntry", "CaptureStringEntry"};
        const std::array<std::uint64_t, 3> tags{7, 9, 5};
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const auto captured = invoke(entries[index]);
            const auto token = *reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
            check(identities.insert(token.typeId).second, "generic type/const specializations shared a native type id");
            const auto adapters = std::span{api.types, api.typeCount};
            const auto adapter = std::ranges::find(adapters, token.typeId, &WioNativeObjectDescriptor::typeId);
            check(adapter != adapters.end(), "concrete generic adapter missing");
            const auto reflected =
                std::ranges::find(decoded.module.contract.reflection, token.typeId, &bc::Reflection::stableTypeId);
            check(reflected != decoded.module.contract.reflection.end(), "concrete generic reflection missing");
            const auto& type = decoded.module.types[reflected->type];
            check(type.arguments.size() == 2, "generic reflection lost type/const arguments");
            const auto methodId = [&](const char* name)
            {
                const auto method = std::ranges::find_if(reflected->methods, [&](const auto& item)
                                                         { return decoded.module.string(item.name) == name; });
                check(method != reflected->methods.end(), "generic reflected method missing");
                check(decoded.module.functions[method->function].ownerType == reflected->type,
                      "generic method dispatch retained the open declaration owner");
                const auto& implementation = decoded.module.functions[method->function];
                check((implementation.flags & bc::FunctionTemplateDeclaration) == 0 && !implementation.blocks.empty() &&
                          implementation.genericOrigin < decoded.module.functions.size() &&
                          (decoded.module.functions[implementation.genericOrigin].flags &
                           bc::FunctionTemplateDeclaration),
                      "concrete method lost its executable specialization or template origin");
                return method->stableId;
            };
            Wire tagged;
            check(token.invoke(&token, methodId("TagValue"), nullptr, 0, &tagged.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      tagged.value.kind == WIO_NATIVE_ABI_U64 && tagged.value.payload.unsignedInteger == tags[index],
                  "const generic method dispatched with another specialization's value");
            Wire same;
            check(token.invoke(&token, methodId("Same"), nullptr, 0, &same.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      same.value.kind == WIO_NATIVE_ABI_OBJECT && same.value.typeId == token.typeId,
                  "generic owned self return lost its specialization");
            WioNativeObjectHost recovered{};
            check(adapter->host(&same.value, &recovered, nullptr) == WIO_NATIVE_ABI_OK &&
                      recovered.owner.state == token.owner.state && recovered.objectId == token.objectId,
                  "generic self return changed VM identity");
            Wire clone;
            check(token.invoke(&token, methodId("Clone"), nullptr, 0, &clone.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      clone.value.typeId == token.typeId &&
                      adapter->host(&clone.value, &recovered, nullptr) == WIO_NATIVE_ABI_OK &&
                      recovered.objectId != token.objectId,
                  "generic constructor result reused its receiver or another type");
            Wire read;
            check(token.invoke(&token, methodId("Read"), nullptr, 0, &read.value, nullptr) == WIO_NATIVE_ABI_OK,
                  "generic value method failed");
            if (index == 2)
                check(read.value.kind == WIO_NATIVE_ABI_BYTES &&
                          std::string_view{static_cast<const char*>(read.value.payload.slice.data),
                                           static_cast<std::size_t>(read.value.payload.slice.size)} == "wio-generic",
                      "string generic method used scalar storage");
            else
                check(read.value.kind == WIO_NATIVE_ABI_I64 &&
                          read.value.payload.signedInteger == (index == 0 ? 37 : 41),
                      "integer generic method used a different type/const specialization");
            for (const auto& wrong : adapters)
                if (wrong.typeId != token.typeId)
                {
                    Wire rejected;
                    check(wrong.create(&token, &rejected.value, nullptr) == WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                              rejected.value.kind == WIO_NATIVE_ABI_VOID &&
                              wrong.host(&same.value, &recovered, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                          "different generic type/const specializations accepted one another's object handles");
                }
        }
        Wire cleared;
        check(call(registry, "vm_object_generic::ClearCapture", {}, cleared.value) == WIO_NATIVE_ABI_OK,
              "generic capture cleanup failed");
        invoke("SafePoint");
        check(scalar(registry, "vm_object_generic::Constructions") ==
                  scalar(registry, "vm_object_generic::Destructions"),
              "generic object constructors/destructors were lost or repeated");
        return 0;
    }

    int qualifyBorrowedResults(const char* bytecode, const char* producer)
    {
        std::ifstream input(std::filesystem::u8path(bytecode), std::ios::binary);
        const std::string bytes{std::istreambuf_iterator<char>{input}, {}};
        auto decoded = bc::decode(std::as_bytes(std::span{bytes.data(), bytes.size()}));
        check(decoded.succeeded(), "borrowed result bytecode decode failed");
        Library library{std::filesystem::u8path(producer)};
        const auto getObjects = library.symbol<WioGetNativeObjectApiFn>("WioGetNativeObjectApi");
        const auto getRegistry = library.symbol<WioGetNativeAbiRegistryFn>("WioGetNativeAbiRegistry");
        check(getObjects && getObjects()->typeCount == 1 && getRegistry, "borrowed result producer API missing");
        const auto& adapter = getObjects()->types[0];
        const auto& registry = *getRegistry();
        wio::vm::Machine machine{decoded.module};
        std::string error;
        check(machine.loadNativeLibrary(std::filesystem::u8path(producer), &error), error.c_str());
        const auto invoke = [&](const char* name)
        {
            const auto entry = std::ranges::find_if(decoded.module.functions, [&](const auto& function)
                                                    { return decoded.module.string(function.name) == name; });
            check(entry != decoded.module.functions.end(), "borrowed result entry missing");
            auto result = machine.invoke(entry->id);
            if (!result.succeeded())
                throw std::runtime_error(result.error().code + ": " + result.error().message);
            return result;
        };
        check(invoke("Entry").value().asSignedInteger() == 0, "borrowed result alias/lifecycle checks failed");
        const auto captured = invoke("CaptureEntry");
        const auto token = *reinterpret_cast<const WioNativeObjectHost*>(captured.value().asUnsignedInteger());
        const auto reflected =
            std::ranges::find(decoded.module.contract.reflection, token.typeId, &bc::Reflection::stableTypeId);
        check(reflected != decoded.module.contract.reflection.end(), "borrowed result metadata missing");
        const auto methodId = [&](const char* name)
        {
            const auto method = std::ranges::find_if(reflected->methods, [&](const auto& item)
                                                     { return decoded.module.string(item.name) == name; });
            check(method != reflected->methods.end(), "borrowed result method missing");
            return method->stableId;
        };
        for (const auto* name : {"AsRef", "AsView"})
        {
            Wire output;
            check(token.invoke(&token, methodId(name), nullptr, 0, &output.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      output.value.kind == WIO_NATIVE_ABI_OBJECT && output.value.typeId == token.typeId,
                  "receiver borrow lost its transport identity");
            WioNativeObjectHost recovered{};
            check(adapter.host(&output.value, &recovered, nullptr) == WIO_NATIVE_ABI_OK &&
                      recovered.owner.state == token.owner.state && recovered.objectId == token.objectId &&
                      recovered.generation == token.generation,
                  "receiver borrow returned a different object");
        }
        Wire first;
        Wire second;
        check(adapter.create(&token, &first.value, nullptr) == WIO_NATIVE_ABI_OK &&
                  adapter.create(&token, &second.value, nullptr) == WIO_NATIVE_ABI_OK,
              "borrowed result cell setup failed");
        WioNativeAbiValue reference{};
        reference.kind = WIO_NATIVE_ABI_REFERENCE;
        reference.flags = WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE;
        reference.payload.mutableSlice = {&first.value, sizeof(WioNativeAbiValue)};
        auto other = reference;
        other.payload.mutableSlice.data = &second.value;
        Wire exercise;
        check(call(registry, "vm_object_borrowed_results::CheckReceiver", std::span{&first.value, 1}, exercise.value) ==
                      WIO_NATIVE_ABI_OK &&
                  exercise.value.payload.signedInteger == 0,
              "borrowed results failed on a separately materialized proxy for the same VM object");
        const auto verify = [&](const char* name, std::span<const WioNativeAbiValue> arguments,
                                WioNativeAbiValue* expected, bool mutableResult)
        {
            Wire output;
            check(token.invoke(&token, methodId(name), arguments.data(), arguments.size(), &output.value, nullptr) ==
                          WIO_NATIVE_ABI_OK &&
                      output.value.kind == WIO_NATIVE_ABI_REFERENCE && output.value.typeId == token.typeId &&
                      output.value.flags ==
                          (WIO_NATIVE_ABI_VALUE_BORROWED | (mutableResult ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0)) &&
                      output.value.payload.mutableSlice.data == expected &&
                      output.value.payload.mutableSlice.size == sizeof(WioNativeAbiValue) &&
                      !output.value.owner.state && !output.value.owner.ops,
                  "borrowed result did not preserve the caller's exact cell and mutability");
        };
        verify("BorrowTarget", std::span{&reference, 1}, &first.value, true);
        auto readonly = reference;
        readonly.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
        verify("ViewTarget", std::span{&readonly, 1}, &first.value, false);
        for (const bool left : {false, true})
        {
            WioNativeAbiValue choice{};
            choice.kind = WIO_NATIVE_ABI_BOOL;
            choice.payload.unsignedInteger = left;
            const std::array arguments{reference, other, choice};
            verify("Choose", arguments, left ? &first.value : &second.value, true);
        }
        Wire missing;
        missing.value.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
        missing.value.typeId = token.typeId;
        auto nullable = reference;
        nullable.payload.mutableSlice.data = &missing.value;
        verify("NullableTarget", std::span{&nullable, 1}, &missing.value, true);
        nullable.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
        verify("NullableView", std::span{&nullable, 1}, &missing.value, false);
        Wire rejected;
        check(token.invoke(&token, methodId("BorrowTarget"), &readonly, 1, &rejected.value, nullptr) ==
                      WIO_NATIVE_ABI_INVALID_ARGUMENT &&
                  rejected.value.kind == WIO_NATIVE_ABI_VOID,
              "readonly input was strengthened by a mutable borrowed result");
        auto malformedHost = token;
        malformedHost.invoke = [](const WioNativeObjectHost*, std::uint64_t, const WioNativeAbiValue*, std::uint64_t,
                                  WioNativeAbiValue* output, WioNativeAbiFailure*) noexcept
        {
            *output = malformedBorrowOutput;
            if (output->kind == WIO_NATIVE_ABI_OBJECT && output->flags & WIO_NATIVE_ABI_VALUE_OWNED)
                WioNativeAbiRetain(output->payload.handle);
            return WIO_NATIVE_ABI_OK;
        };
        for (int malformed = 0; malformed < 3; ++malformed)
        {
            malformedBorrowOutput = reference;
            malformedBorrowOutput.typeId = token.typeId;
            if (malformed == 1)
                malformedBorrowOutput.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            if (malformed == 2)
                malformedBorrowOutput = first.value; // Different dispatcher identity, despite the same object id.
            Wire proxy;
            check(adapter.create(&malformedHost, &proxy.value, nullptr) == WIO_NATIVE_ABI_OK,
                  "malformed borrowed result proxy setup failed");
            Wire failure;
            check(call(registry, "vm_object_borrowed_results::CheckReceiver", std::span{&proxy.value, 1},
                       failure.value) == WIO_NATIVE_ABI_TYPE_MISMATCH &&
                      failure.value.kind == WIO_NATIVE_ABI_VOID,
                  "generated proxy accepted an unrelated cell, weakened ref or mismatched receiver dispatcher");
        }
        malformedBorrowOutput = {};
        check(call(registry, "vm_object_references::ClearCapture", {}, rejected.value) == WIO_NATIVE_ABI_OK,
              "borrowed result capture cleanup failed");
        return 0;
    }

    int qualifyReferences(const char* bytecode, const char* producer, bool nullable = false)
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
        const auto makeTarget =
            std::ranges::find_if(reflected->methods, [&](const auto& method)
                                 { return decoded.module.string(method.name) == (nullable ? "Make" : "MakeTarget"); });
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
        if (nullable)
        {
            const auto methodId = [&](const char* name)
            {
                const auto method = std::ranges::find_if(reflected->methods, [&](const auto& item)
                                                         { return decoded.module.string(item.name) == name; });
                check(method != reflected->methods.end(), "nullable reference qualification method missing");
                return method->stableId;
            };
            Wire missing;
            missing.value.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
            missing.value.typeId = token.typeId;
            auto mutableNull = reference;
            mutableNull.payload.mutableSlice.data = &missing.value;
            auto viewNull = mutableNull;
            viewNull.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            auto sameCellView = reference;
            sameCellView.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            const std::array mixedAliasArguments{reference, sameCellView};
            const std::array mixedNullArguments{mutableNull, viewNull};
            for (const auto& arguments : {mixedAliasArguments, mixedNullArguments})
            {
                Wire rejected;
                check(token.invoke(&token, methodId("Mixed"), arguments.data(), arguments.size(), &rejected.value,
                                   nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                      "mixed nullable/non-nullable references accepted an incompatible alias or null cell");
            }
            Wire read;
            check(token.invoke(&token, methodId("ReadFrom"), &viewNull, 1, &read.value, nullptr) == WIO_NATIVE_ABI_OK &&
                      read.value.kind == WIO_NATIVE_ABI_I64 && read.value.payload.signedInteger == -1,
                  "nullable view rejected an empty object handle cell");
            for (int kind = 0; kind < 4; ++kind)
            {
                auto invalid = missing.value;
                if (kind == 0)
                    invalid.flags = 0;
                if (kind == 1)
                    invalid.typeId ^= 1;
                if (kind == 2)
                    invalid.flags |= WIO_NATIVE_ABI_VALUE_OWNED;
                if (kind == 3)
                    invalid.owner.state = &invalid;
                auto badReference = mutableNull;
                badReference.payload.mutableSlice.data = &invalid;
                const auto expected = invalid;
                Wire rejected;
                check(token.invoke(&token, methodId("Clear"), &badReference, 1, &rejected.value, nullptr) ==
                              WIO_NATIVE_ABI_TYPE_MISMATCH &&
                          invalid.kind == expected.kind && invalid.flags == expected.flags &&
                          invalid.typeId == expected.typeId && invalid.owner.state == expected.owner.state,
                      "nullable reference accepted a malformed null cell");
            }
            const std::array nullArguments{mutableNull, seed};
            Wire created;
            check(token.invoke(&token, makeTarget->stableId, nullArguments.data(), nullArguments.size(), &created.value,
                               nullptr) == WIO_NATIVE_ABI_OK &&
                      missing.value.kind == WIO_NATIVE_ABI_OBJECT,
                  "nullable reference did not copy a constructed object into a null cell");
            check(token.invoke(&token, methodId("Clear"), &mutableNull, 1, &created.value, nullptr) ==
                          WIO_NATIVE_ABI_OK &&
                      missing.value.kind == WIO_NATIVE_ABI_VOID &&
                      missing.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE && missing.value.typeId == token.typeId,
                  "nullable reference did not copy canonical null back to its caller");
        }
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
        if (nullable)
        {
            const auto clearAndMake =
                std::ranges::find_if(reflected->methods, [&](const auto& method)
                                     { return decoded.module.string(method.name) == "ClearAndMake"; });
            check(clearAndMake != reflected->methods.end(), "nullable transactional clear method missing");
            const auto beforeClearConstructed = scalar(registry, "vm_object_references::Constructions");
            const auto beforeClearDestroyed = scalar(registry, "vm_object_references::Destructions");
            objectFactoryCallsBeforeFailure = 0;
            failObjectFactory = true;
            check(failureToken.invoke(&failureToken, clearAndMake->stableId, pairArguments.data(), pairArguments.size(),
                                      &failed.value, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "nullable second-cell factory failure lost its status");
            failObjectFactory = false;
            check(failureCell.value.payload.handle.state == originalPointer &&
                      secondCell.value.payload.handle.state == secondPointer,
                  "nullable copy-back committed the first cell's null before the second factory failed");
            check(failedMachine.invoke(safePoint->id).succeeded(), "nullable transaction cleanup safe point failed");
            check(scalar(registry, "vm_object_references::Constructions") == beforeClearConstructed + 1 &&
                      scalar(registry, "vm_object_references::Destructions") == beforeClearDestroyed + 1,
                  "nullable transactional clear failure lost or repeated object cleanup");
            for (auto* value : {&failureCell.value, &secondCell.value})
            {
                WioNativeAbiReleaseValue(value);
                value->flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                value->typeId = failureToken.typeId;
            }
            const auto beforeNullConstructed = scalar(registry, "vm_object_references::Constructions");
            const auto beforeNullDestroyed = scalar(registry, "vm_object_references::Destructions");
            objectFactoryCallsBeforeFailure = 1;
            failObjectFactory = true;
            check(failureToken.invoke(&failureToken, makePair->stableId, pairArguments.data(), pairArguments.size(),
                                      &failed.value, nullptr) == WIO_NATIVE_ABI_TYPE_MISMATCH,
                  "nullable null-to-object transactional failure lost its status");
            failObjectFactory = false;
            check(failureCell.value.kind == WIO_NATIVE_ABI_VOID && secondCell.value.kind == WIO_NATIVE_ABI_VOID &&
                      failureCell.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE &&
                      secondCell.value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE,
                  "nullable failed transaction partially replaced initially null cells");
            check(failedMachine.invoke(safePoint->id).succeeded(), "nullable staged-object cleanup safe point failed");
            check(scalar(registry, "vm_object_references::Constructions") == beforeNullConstructed + 2 &&
                      scalar(registry, "vm_object_references::Destructions") == beforeNullDestroyed + 2,
                  "nullable failed transaction leaked staged objects from initially null cells");
        }
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
        if (argc == 4 && std::string_view(argv[3]) == "object-any")
            return qualifyObjectAny(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "nullable-any")
            return qualifyObjectAny(argv[1], argv[2], true);
        if (argc == 4 && std::string_view(argv[3]) == "polymorphic-references")
            return qualifyPolymorphicReferences(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "polymorphic")
            return qualifyPolymorphic(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "hierarchy")
            return qualifyHierarchy(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "concrete-generics")
            return qualifyConcreteGenerics(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "borrowed-results")
            return qualifyBorrowedResults(argv[1], argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "nullable-references")
            return qualifyReferences(argv[1], argv[2], true);
        if (argc == 4 && std::string_view(argv[3]) == "nullable")
            return qualifyNullable(argv[1], argv[2]);
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
