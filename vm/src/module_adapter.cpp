#include "wio/vm/module_adapter.h"
#include "native_bridge_internal.h"
#include "wio/bytecode/verifier.h"

#include <limits>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <atomic>

namespace wio::vm
{
    namespace
    {
        constexpr std::uint8_t TypeReference = 29;
        constexpr std::uint8_t TypeNullable = 30;
        constexpr std::uint8_t AsyncTaskIntrinsicFamily = 14;

        std::uint64_t currentOwnerToken() noexcept
        {
            static std::atomic_uint64_t next{1};
            thread_local const auto token = next.fetch_add(1, std::memory_order_relaxed);
            return token;
        }

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
        const std::uint64_t owner = currentOwnerToken();
        std::vector<Call> calls;
        std::vector<WioSdkExportDescriptor> exports;
        std::vector<WioSdkCallEntry> table;
        std::vector<WioSdkReflectionDescriptor> reflection;
        WioSdkModuleContract descriptor{};
        std::string error;
        bool invoking = false;
        bool started = false;
        bool closed = false;
        bool unloadRequired = false;

        template <class F> std::int32_t protect(F action, bool allowClosed = false) noexcept
        {
            if (owner != currentOwnerToken())
                return WIO_NATIVE_ABI_WRONG_THREAD;
            if (invoking)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            if (closed && !allowClosed)
                return WIO_NATIVE_ABI_STALE_HANDLE;
            invoking = true;
            struct Reset
            {
                bool& active;
                ~Reset()
                {
                    active = false;
                }
            } reset{invoking};
            try
            {
                error.clear();
                return action();
            }
            catch (const std::exception& exception)
            {
                try
                {
                    error = exception.what();
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

        std::int32_t statusCall(std::uint32_t function, std::span<const Value> arguments = {})
        {
            if (function == bytecode::InvalidIndex)
                return WIO_NATIVE_ABI_OK;
            auto execution = machine.invoke(function, arguments);
            if (!execution.succeeded())
            {
                error = execution.error().code + ": " + execution.error().message;
                return WIO_NATIVE_ABI_PANIC;
            }
            if (module.types[module.functions[function].returnType].kind == 5 &&
                (execution.value().kind() != Value::Kind::SignedInteger || execution.value().asSignedInteger() != 0))
            {
                error = "VM SDK lifecycle callback returned a nonzero status";
                return WIO_NATIVE_ABI_EXCEPTION;
            }
            return WIO_NATIVE_ABI_OK;
        }

        bool needsStart() const noexcept
        {
            return (descriptor.lifecycleFlags & ~WIO_SDK_LIFECYCLE_API_VERSION) != 0;
        }

        bool lifecycleFunction(std::uint32_t function) const noexcept
        {
            const auto& life = module.contract.lifecycle;
            return function == life.apiVersion || function == life.load || function == life.update ||
                   function == life.unload || function == life.saveState || function == life.restoreState;
        }

        explicit State(bytecode::Module input, MachineOptions options)
            : module(verifiedModule(std::move(input))), machine(module, options)
        {
            const auto& lifecycle = module.contract.lifecycle;
            if (module.moduleKind != WIO_SDK_MODULE_WIO_LIBRARY || module.stableId == 0 ||
                module.stableKey >= module.strings.size() ||
                module.stableId != WioStableSdkId(module.strings[module.stableKey].c_str()))
                throw std::invalid_argument("VM SDK adapter requires a stable Wio library module identity");
            if (module.contract.hasApplication)
                throw std::invalid_argument("VM SDK application hosting is not qualified yet");
            const auto checkLifecycle =
                [&](std::uint32_t id, std::uint8_t resultKind, std::initializer_list<std::uint8_t> parameters)
            {
                if (id == bytecode::InvalidIndex)
                    return;
                const auto& fn = module.functions[id];
                if (fn.hasCoroutine || !fn.genericParameters.empty() || !fn.captures.empty() ||
                    fn.captureParameterCount || fn.parameters.size() != parameters.size() ||
                    fn.returnType >= module.types.size() || module.types[fn.returnType].kind != resultKind)
                    throw std::invalid_argument("VM SDK lifecycle has an unsupported function signature");
                std::size_t index = 0;
                for (auto kind : parameters)
                {
                    const auto type = fn.parameters[index++].type;
                    if (type >= module.types.size() || module.types[type].kind != kind)
                        throw std::invalid_argument("VM SDK lifecycle parameter has an unsupported type");
                }
            };
            checkLifecycle(lifecycle.apiVersion, 10, {});
            checkLifecycle(lifecycle.load, 5, {});
            checkLifecycle(lifecycle.update, 1, {13});
            checkLifecycle(lifecycle.unload, 1, {});
            const bool saves = lifecycle.saveState != bytecode::InvalidIndex;
            if (saves != (lifecycle.restoreState != bytecode::InvalidIndex) ||
                (!saves && lifecycle.stateSchemaVersion != 0))
                throw std::invalid_argument("VM SDK save/restore callbacks must be paired");
            if (saves)
            {
                const auto& save = module.functions[lifecycle.saveState];
                const auto& restore = module.functions[lifecycle.restoreState];
                if (qualifiedMarshalling(module, save.returnType, true) == 0)
                    throw std::invalid_argument("VM SDK save payload cannot be void");
                checkLifecycle(lifecycle.restoreState, 5, {module.types[save.returnType].kind});
                if (!save.parameters.empty() || save.hasCoroutine || !save.genericParameters.empty() ||
                    !save.captures.empty() || save.captureParameterCount ||
                    restore.parameters[0].type != save.returnType)
                    throw std::invalid_argument("VM SDK save/restore payload is unsupported");
            }
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
                          lifecycle.stateSchemaVersion,
                          module.stableId,
                          cstring(module.stableKey),
                          (lifecycle.apiVersion != bytecode::InvalidIndex ? WIO_SDK_LIFECYCLE_API_VERSION : 0u) |
                              (lifecycle.load != bytecode::InvalidIndex ? WIO_SDK_LIFECYCLE_LOAD : 0u) |
                              (lifecycle.update != bytecode::InvalidIndex ? WIO_SDK_LIFECYCLE_UPDATE : 0u) |
                              (lifecycle.unload != bytecode::InvalidIndex ? WIO_SDK_LIFECYCLE_UNLOAD : 0u) |
                              (saves ? WIO_SDK_LIFECYCLE_SAVE_STATE | WIO_SDK_LIFECYCLE_RESTORE_STATE : 0u),
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
            return state.protect(
                [&]() -> std::int32_t
                {
                    if ((state.needsStart() && !state.started) || state.lifecycleFunction(call.entry->function))
                        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
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
                        auto value = detail::decodeNativeValue(
                            state.module, wire[index], call.entry->parameterTypes[index], call.parameters[index]);
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
                });
        }
    };

    ModuleAdapter::ModuleAdapter(bytecode::Module module, MachineOptions options)
        : state_(std::make_unique<State>(std::move(module), options))
    {
    }

    ModuleAdapter::~ModuleAdapter()
    {
        (void)close();
    }

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
        if (state_->owner != currentOwnerToken() || state_->invoking || state_->closed)
        {
            if (error)
                *error = "VM SDK native registration requires the idle owning thread";
            return false;
        }
        return state_->machine.loadNativeLibrary(path, error);
    }

    std::int32_t ModuleAdapter::start() noexcept
    {
        const auto status = state_->protect(
            [&]() -> std::int32_t
            {
                if (state_->started)
                    return WIO_NATIVE_ABI_OK;
                state_->unloadRequired = true;
                const auto loaded = state_->statusCall(state_->module.contract.lifecycle.load);
                if (loaded == WIO_NATIVE_ABI_OK)
                    state_->started = true;
                return loaded;
            });
        if (status != WIO_NATIVE_ABI_OK && state_->owner == currentOwnerToken() && !state_->invoking &&
            state_->unloadRequired && !state_->started)
        {
            auto primary = std::move(state_->error);
            (void)close();
            state_->error = std::move(primary);
        }
        return status;
    }

    std::int32_t ModuleAdapter::close() noexcept
    {
        return state_->protect(
            [&]() -> std::int32_t
            {
                if (state_->closed)
                    return WIO_NATIVE_ABI_OK;
                state_->closed = true;
                state_->started = false;
                const bool unload = std::exchange(state_->unloadRequired, false);
                return unload ? state_->statusCall(state_->module.contract.lifecycle.unload) : WIO_NATIVE_ABI_OK;
            },
            true);
    }

    std::int32_t ModuleAdapter::update(double deltaSeconds) noexcept
    {
        return state_->protect(
            [&]() -> std::int32_t
            {
                if (!state_->started || state_->module.contract.lifecycle.update == bytecode::InvalidIndex ||
                    !std::isfinite(deltaSeconds) || deltaSeconds < 0 ||
                    deltaSeconds > (std::numeric_limits<float>::max)())
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                const Value delta = Value::floating(static_cast<float>(deltaSeconds));
                return state_->statusCall(state_->module.contract.lifecycle.update, {&delta, 1});
            });
    }

    std::int32_t ModuleAdapter::apiVersion(std::uint32_t& output) noexcept
    {
        return state_->protect(
            [&]() -> std::int32_t
            {
                const auto function = state_->module.contract.lifecycle.apiVersion;
                if (function == bytecode::InvalidIndex)
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                auto execution = state_->machine.invoke(function);
                if (!execution.succeeded())
                {
                    state_->error = execution.error().code + ": " + execution.error().message;
                    return WIO_NATIVE_ABI_PANIC;
                }
                if (execution.value().kind() != Value::Kind::UnsignedInteger ||
                    execution.value().asUnsignedInteger() > (std::numeric_limits<std::uint32_t>::max)())
                    return WIO_NATIVE_ABI_TYPE_MISMATCH;
                output = static_cast<std::uint32_t>(execution.value().asUnsignedInteger());
                return WIO_NATIVE_ABI_OK;
            });
    }

    std::int32_t ModuleAdapter::saveState(WioNativeAbiValue& output) noexcept
    {
        return state_->protect(
            [&]() -> std::int32_t
            {
                const auto function = state_->module.contract.lifecycle.saveState;
                if (!state_->started || function == bytecode::InvalidIndex || !emptyOutput(output))
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                auto execution = state_->machine.invoke(function);
                if (!execution.succeeded())
                {
                    state_->error = execution.error().code + ": " + execution.error().message;
                    return WIO_NATIVE_ABI_PANIC;
                }
                const auto type = state_->module.functions[function].returnType;
                return detail::encodeOwnedNativeValue(state_->module, execution.value(), type,
                                                      qualifiedMarshalling(state_->module, type, true), output,
                                                      state_->error)
                           ? WIO_NATIVE_ABI_OK
                           : WIO_NATIVE_ABI_TYPE_MISMATCH;
            });
    }

    std::int32_t ModuleAdapter::restoreState(const WioNativeAbiValue& input) noexcept
    {
        return state_->protect(
            [&]() -> std::int32_t
            {
                const auto function = state_->module.contract.lifecycle.restoreState;
                if (!state_->started || function == bytecode::InvalidIndex ||
                    (input.flags & ~(WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_OWNED)))
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                const auto type = state_->module.functions[function].parameters[0].type;
                auto value = detail::decodeNativeValue(state_->module, input, type,
                                                       qualifiedMarshalling(state_->module, type, false));
                if (!value.succeeded())
                {
                    state_->error = std::move(value.error);
                    return WIO_NATIVE_ABI_TYPE_MISMATCH;
                }
                return state_->statusCall(function, {&value.value, 1});
            });
    }
} // namespace wio::vm
