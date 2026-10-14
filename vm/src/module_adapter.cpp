#include "wio/vm/module_adapter.h"
#include "native_bridge_internal.h"
#include "wio/bytecode/verifier.h"

#include <limits>
#include <stdexcept>
#include <thread>
#include <unordered_set>

namespace wio::vm
{
    namespace
    {
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeNullable = 30;
        constexpr std::uint8_t AsyncTaskIntrinsicFamily = 14;

        bytecode::Module verifiedModule(bytecode::Module module)
        {
            const auto verification = bytecode::Verifier{}.verify(module);
            if (!verification.succeeded())
                throw std::invalid_argument("VM SDK bytecode verification failed: " +
                                            verification.diagnostics().front().message);
            return module;
        }

        std::uint8_t qualifiedMarshalling(const bytecode::Module& module, std::uint32_t type, bool result)
        {
            std::uint8_t marshalling = 0;
            // Do not unwrap ref/view/nullable declarations through scalarType.
            // Only concrete by-value scalars, enum/flagset, string and text are
            // qualified here; broader native codecs require their sidecars.
            if (type >= module.types.size() || module.types[type].kind == TypeReference ||
                module.types[type].kind == TypeNullable || !detail::nativeValueMarshalling(module, type, marshalling) ||
                marshalling > 3 || (!result && marshalling == 0))
                throw std::invalid_argument("VM SDK export has an unsupported value signature");
            return marshalling;
        }

        bool emptyOutput(const WioNativeAbiValue& value) noexcept
        {
            return value.kind == WIO_NATIVE_ABI_VOID && value.flags == 0 && value.typeId == 0 && !value.owner.state &&
                   !value.owner.ops;
        }
    } // namespace

    struct ModuleAdapter::State
    {
        struct Call
        {
            State* state;
            const bytecode::Export* entry;
            std::vector<std::uint8_t> parameters;
            std::uint8_t result;
        };

        bytecode::Module module;
        Machine machine;
        const std::thread::id owner = std::this_thread::get_id();
        std::vector<Call> calls;
        std::vector<WioSdkExportDescriptor> exports;
        std::vector<WioSdkCallEntry> table;
        std::vector<WioSdkReflectionDescriptor> reflection;
        WioSdkModuleContract descriptor{};
        std::string error;
        bool invoking = false;

        explicit State(bytecode::Module input, MachineOptions options)
            : module(verifiedModule(std::move(input))), machine(module, options)
        {
            const auto& lifecycle = module.contract.lifecycle;
            if (module.moduleKind != WIO_SDK_MODULE_WIO_LIBRARY || module.stableId == 0 ||
                module.stableKey >= module.strings.size() ||
                module.stableId != WioStableSdkId(module.strings[module.stableKey].c_str()))
                throw std::invalid_argument("VM SDK adapter requires a stable Wio library module identity");
            if (module.contract.hasApplication || lifecycle.apiVersion != bytecode::InvalidIndex ||
                lifecycle.load != bytecode::InvalidIndex || lifecycle.update != bytecode::InvalidIndex ||
                lifecycle.unload != bytecode::InvalidIndex || lifecycle.saveState != bytecode::InvalidIndex ||
                lifecycle.restoreState != bytecode::InvalidIndex || lifecycle.stateSchemaVersion != 0)
                throw std::invalid_argument("VM SDK application/lifecycle hosting is not qualified yet");
            if (module.contract.exports.size() > (std::numeric_limits<std::uint32_t>::max)())
                throw std::invalid_argument("VM SDK export table is too large");
            // A synchronous export can still start asynchronous helper work.
            // DLL teardown cannot join VM workers under the OS loader lock:
            // reject this whole-module surface until explicit sessions/shutdown
            // are implemented, rather than validating only export signatures.
            for (const auto& function : module.functions)
            {
                if (function.hasCoroutine)
                    throw std::invalid_argument("VM SDK asynchronous module hosting is not qualified yet");
                for (const auto& block : function.blocks)
                    for (const auto& instruction : block.instructions)
                        if (instruction.asyncOperation != 0 || instruction.intrinsicFamily == AsyncTaskIntrinsicFamily)
                            throw std::invalid_argument("VM SDK asynchronous module hosting is not qualified yet");
            }

            const auto cstring = [&](bytecode::StringId id) -> const char*
            {
                if (id >= module.strings.size() || module.strings[id].find('\0') != std::string::npos)
                    throw std::invalid_argument("VM SDK identity contains an invalid C string");
                return module.strings[id].c_str();
            };
            calls.reserve(module.contract.exports.size());
            exports.reserve(module.contract.exports.size());
            table.reserve(module.contract.exports.size());
            std::unordered_set<std::uint64_t> identities;
            for (const auto& entry : module.contract.exports)
            {
                const auto slot = static_cast<std::uint32_t>(calls.size());
                if (entry.kind > WIO_SDK_EXPORT_GENERIC_FUNCTION_SPECIALIZATION ||
                    entry.role > WIO_SDK_EXPORT_EVENT_HOOK || entry.isAsync ||
                    entry.function >= module.functions.size() || entry.callTableSlot != slot ||
                    module.contract.callTableEntries[slot] != entry.stableId ||
                    entry.stableId != WioStableSdkId(cstring(entry.stableKey)) ||
                    !identities.insert(entry.stableId).second)
                    throw std::invalid_argument("VM SDK export identity, slot or callable kind is unsupported");
                const auto& function = module.functions[entry.function];
                if (function.hasCoroutine || function.captureParameterCount != 0 || !function.captures.empty() ||
                    !function.genericParameters.empty() || (function.flags & bytecode::FunctionTemplateDeclaration) ||
                    function.parameters.size() != entry.parameterTypes.size() ||
                    function.returnType != entry.returnType)
                    throw std::invalid_argument("VM SDK export does not match a concrete synchronous function");
                Call call{this, &entry, {}, qualifiedMarshalling(module, entry.returnType, true)};
                for (std::size_t index = 0; index < entry.parameterTypes.size(); ++index)
                {
                    if (function.parameters[index].type != entry.parameterTypes[index])
                        throw std::invalid_argument("VM SDK export parameter differs from the function signature");
                    call.parameters.push_back(qualifiedMarshalling(module, entry.parameterTypes[index], false));
                }
                calls.push_back(std::move(call));
                exports.push_back({entry.stableId, cstring(entry.stableKey), cstring(entry.logicalName),
                                   cstring(entry.symbolName), cstring(entry.roleName), slot,
                                   static_cast<WioSdkExportKind>(entry.kind), static_cast<WioSdkExportRole>(entry.role),
                                   WIO_SDK_CALL_NATIVE_ABI_V2});
                table.push_back({entry.stableId, &dispatch, &calls.back()});
            }
            for (const auto& entry : module.contract.reflection)
                reflection.push_back(
                    {entry.stableTypeId, cstring(entry.logicalName), entry.nominalKind, entry.isExported ? 1u : 0u});
            descriptor = {WIO_SDK_MODULE_CONTRACT_VERSION,
                          sizeof(WioSdkModuleContract),
                          WIO_SDK_MODULE_WIO_LIBRARY,
                          0,
                          module.stableId,
                          cstring(module.stableKey),
                          0,
                          static_cast<std::uint32_t>(exports.size()),
                          exports.empty() ? nullptr : exports.data(),
                          static_cast<std::uint32_t>(reflection.size()),
                          reflection.empty() ? nullptr : reflection.data(),
                          static_cast<std::uint32_t>(table.size()),
                          table.empty() ? nullptr : table.data()};
            if (!WioValidateSdkModuleContract(&descriptor))
                throw std::invalid_argument("VM SDK contract validation failed");
            machine.bindMainExecutor();
        }

        static std::int32_t dispatch(const void* arguments, std::uint32_t count, void* output, void* context) noexcept
        {
            if (!context)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            auto& call = *static_cast<Call*>(context);
            auto& state = *call.state;
            // Do not mutate diagnostic storage from a foreign thread.
            if (state.owner != std::this_thread::get_id())
                return WIO_NATIVE_ABI_WRONG_THREAD;
            if (state.invoking)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            state.invoking = true;
            struct Reset
            {
                bool& active;
                ~Reset()
                {
                    active = false;
                }
            } reset{state.invoking};
            try
            {
                state.error.clear();
                auto* result = static_cast<WioNativeAbiValue*>(output);
                if (count != call.parameters.size() || (count && !arguments) || (!result && call.result != 0) ||
                    (result && !emptyOutput(*result)))
                {
                    state.error = "VM SDK invocation requires exact arguments and an empty result slot";
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                }
                const auto* wire = static_cast<const WioNativeAbiValue*>(arguments);
                std::vector<Value> values;
                values.reserve(count);
                for (std::uint32_t index = 0; index < count; ++index)
                {
                    if (wire[index].flags & ~(WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_OWNED))
                    {
                        state.error = "VM SDK by-value arguments cannot carry reference/consumed flags";
                        return WIO_NATIVE_ABI_TYPE_MISMATCH;
                    }
                    auto value = detail::decodeNativeValue(state.module, wire[index], call.entry->parameterTypes[index],
                                                           call.parameters[index]);
                    if (!value.succeeded())
                    {
                        state.error = std::move(value.error);
                        return WIO_NATIVE_ABI_TYPE_MISMATCH;
                    }
                    values.push_back(std::move(value.value));
                }
                auto execution = state.machine.invoke(call.entry->function, values);
                if (!execution.succeeded())
                {
                    state.error = execution.error().code + ": " + execution.error().message;
                    return WIO_NATIVE_ABI_PANIC;
                }
                WioNativeAbiValue encoded{};
                if (!detail::encodeOwnedNativeValue(state.module, execution.value(), call.entry->returnType,
                                                    call.result, encoded, state.error))
                    return WIO_NATIVE_ABI_TYPE_MISMATCH;
                if (result)
                    *result = encoded;
                return WIO_NATIVE_ABI_OK;
            }
            catch (const std::exception& exception)
            {
                try
                {
                    state.error = exception.what();
                }
                catch (...)
                {
                }
                return WIO_NATIVE_ABI_EXCEPTION;
            }
            catch (...)
            {
                return WIO_NATIVE_ABI_EXCEPTION;
            }
        }
    };

    ModuleAdapter::ModuleAdapter(bytecode::Module module, MachineOptions options)
        : state_(std::make_unique<State>(std::move(module), options))
    {
    }

    ModuleAdapter::~ModuleAdapter() = default;

    const WioSdkModuleContract& ModuleAdapter::contract() const noexcept
    {
        return state_->descriptor;
    }

    std::string_view ModuleAdapter::lastError() const noexcept
    {
        return state_->error;
    }

    bool ModuleAdapter::loadNativeLibrary(const std::filesystem::path& path, std::string* error)
    {
        if (state_->owner != std::this_thread::get_id() || state_->invoking)
        {
            if (error)
                *error = "VM SDK native registration requires the idle owning thread";
            return false;
        }
        return state_->machine.loadNativeLibrary(path, error);
    }
} // namespace wio::vm
