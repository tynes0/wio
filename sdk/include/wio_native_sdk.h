#pragma once
#include "wio_native_reflection.h"
#include "wio_sdk.h"
#include "wio_module_tasks.h"

#include <cstring>
#include <atomic>
#include <mutex>
#include <thread>
#include <new>
#include <chrono>
#include <functional>
#include <unordered_set>

namespace wio::sdk
{
    // Move-only owner of a canonical value. Keep the producing DLL loaded until
    // its release callback has run, including values surviving their Module view.
    class NativeValue
    {
        WioNativeAbiValue value_{};
        std::shared_ptr<void> keepAlive_;
        NativeValue* borrowed_ = nullptr;
        std::weak_ptr<void> taskSession_;
        friend class NativeModule;
        void pin(std::shared_ptr<void> owner)
        {
            if (!keepAlive_ || keepAlive_.get() == owner.get())
                keepAlive_ = std::move(owner);
            else
                keepAlive_ = std::make_shared<std::pair<std::shared_ptr<void>, std::shared_ptr<void>>>(
                    keepAlive_, std::move(owner));
        }

    public:
        NativeValue() = default;
        NativeValue(const NativeValue&) = delete;
        NativeValue& operator=(const NativeValue&) = delete;
        NativeValue(NativeValue&& other) noexcept
            : value_(std::exchange(other.value_, {})), keepAlive_(std::move(other.keepAlive_)),
              borrowed_(std::exchange(other.borrowed_, nullptr)), taskSession_(std::move(other.taskSession_))
        {
        }
        NativeValue& operator=(NativeValue&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                value_ = std::exchange(other.value_, {});
                keepAlive_ = std::move(other.keepAlive_);
                borrowed_ = std::exchange(other.borrowed_, nullptr);
                taskSession_ = std::move(other.taskSession_);
            }
            return *this;
        }
        ~NativeValue()
        {
            reset();
        }
        void reset() noexcept
        {
            WioNativeAbiReleaseValue(&value_);
            keepAlive_.reset();
            borrowed_ = nullptr;
            taskSession_.reset();
        }
        static NativeValue integer(std::int64_t n)
        {
            NativeValue v;
            v.value_.kind = WIO_NATIVE_ABI_I64;
            v.value_.payload.signedInteger = n;
            return v;
        }
        static NativeValue unsignedInteger(std::uint64_t n)
        {
            NativeValue v;
            v.value_.kind = WIO_NATIVE_ABI_U64;
            v.value_.payload.unsignedInteger = n;
            return v;
        }
        static NativeValue boolean(bool b)
        {
            NativeValue v;
            v.value_.kind = WIO_NATIVE_ABI_BOOL;
            v.value_.payload.boolean = b;
            return v;
        }
        static NativeValue string(std::string text)
        {
            NativeValue v;
            auto storage = std::make_shared<std::string>(std::move(text));
            v.value_.kind = WIO_NATIVE_ABI_BYTES;
            v.value_.payload.slice = {storage->data(), storage->size()};
            v.keepAlive_ = storage;
            return v;
        }
        static NativeValue text(std::u32string text)
        {
            NativeValue v;
            auto storage = std::make_shared<std::u32string>(std::move(text));
            for (char32_t c : *storage)
                if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
                    throw std::invalid_argument("expected Unicode scalar values");
            v.value_.kind = WIO_NATIVE_ABI_TEXT_UTF32;
            v.value_.payload.slice = {storage->data(), storage->size() * sizeof(char32_t)};
            v.keepAlive_ = storage;
            return v;
        }
        NativeValue borrow(bool mutableAccess = true) &
        {
            NativeValue v;
            v.value_.kind = WIO_NATIVE_ABI_REFERENCE;
            v.value_.typeId = value_.typeId;
            v.value_.flags = WIO_NATIVE_ABI_VALUE_BORROWED | (mutableAccess ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0);
            v.value_.payload.mutableSlice = {&value_, sizeof(value_)};
            v.borrowed_ = this;
            return v;
        }
        NativeValue clone() const
        {
            NativeValue copy;
            copy.value_ = value_;
            copy.keepAlive_ = keepAlive_;
            copy.borrowed_ = borrowed_;
            copy.taskSession_ = taskSession_;
            if (value_.flags & WIO_NATIVE_ABI_VALUE_OWNED)
            {
                WioNativeAbiRetain(value_.owner);
                if ((value_.kind == WIO_NATIVE_ABI_OBJECT || value_.kind == WIO_NATIVE_ABI_RUNTIME_VALUE) &&
                    !(value_.flags & WIO_NATIVE_ABI_VALUE_TASK_SOURCE))
                    WioNativeAbiRetain(value_.payload.handle);
                if (value_.kind == WIO_NATIVE_ABI_CALLBACK && value_.payload.callback.ops &&
                    value_.payload.callback.ops->retain)
                    value_.payload.callback.ops->retain(value_.payload.callback.userdata);
            }
            return copy;
        }
        std::int64_t asInteger() const
        {
            if (value_.kind != WIO_NATIVE_ABI_I64)
                throw std::runtime_error("expected signed ABI integer");
            return value_.payload.signedInteger;
        }
        bool asBoolean() const
        {
            if (value_.kind != WIO_NATIVE_ABI_BOOL)
                throw std::runtime_error("expected ABI bool");
            return value_.payload.boolean;
        }
        std::string asString() const
        {
            if (value_.kind != WIO_NATIVE_ABI_BYTES)
                throw std::runtime_error("expected ABI string");
            return value_.payload.slice.size ? std::string(static_cast<const char*>(value_.payload.slice.data),
                                                           static_cast<std::size_t>(value_.payload.slice.size))
                                             : std::string{};
        }
        const WioNativeAbiValue& raw() const noexcept
        {
            return value_;
        }
        std::u32string asText() const
        {
            if (value_.kind != WIO_NATIVE_ABI_TEXT_UTF32 || value_.payload.slice.size % sizeof(char32_t) ||
                (!value_.payload.slice.data && value_.payload.slice.size))
                throw std::runtime_error("expected ABI text");
            std::u32string result(value_.payload.slice.size / sizeof(char32_t), U'\0');
            if (!result.empty())
                std::memcpy(result.data(), value_.payload.slice.data, value_.payload.slice.size);
            return result;
        }
    };

    class NativeModule
    {
        struct SessionCode
        {
            decltype(detail::openLibrary(std::filesystem::path{})) handle{};
            std::shared_ptr<void> attachedOwner;
            ~SessionCode()
            {
                if (handle)
                    detail::closeLibrary(handle);
            }
        };

        struct SessionOwner
        {
            const WioSdkModuleSessionApi* api{};
            const WioSdkModuleTaskApi* tasks{};
            WioSdkModuleSession session{};
            std::shared_ptr<SessionCode> code;
            std::uint64_t owner{};
            SessionOwner* next = nullptr;

            struct Retirements
            {
                std::mutex mutex;
                SessionOwner* head = nullptr;
                // Deliberately do not destroy pending affine instances during
                // static/DLL teardown. The owning thread must collect them.
            };

            static std::uint64_t threadToken() noexcept
            {
                static std::atomic_uint64_t next{1};
                thread_local const auto token = next.fetch_add(1, std::memory_order_relaxed);
                return token;
            }

            static Retirements& retirements()
            {
                // Allocation-free, process-lifetime queue: static destruction must
                // not tear down its mutex before late affine owners are retired.
                alignas(Retirements) static std::byte storage[sizeof(Retirements)]{};
                static auto* queue = ::new (static_cast<void*>(storage)) Retirements{};
                return *queue;
            }

            static void release(SessionOwner* value) noexcept
            {
                if (value->owner == threadToken())
                {
                    (void)value->api->close(&value->session);
                    if (!value->session.context && !value->session.contract)
                    {
                        delete value;
                        return;
                    }
                }
                auto& queue = retirements();
                std::scoped_lock lock{queue.mutex};
                value->next = queue.head;
                queue.head = value;
            }

            static std::uint64_t collect() noexcept
            {
                auto& queue = retirements();
                SessionOwner* selected = nullptr;
                {
                    std::scoped_lock lock{queue.mutex};
                    auto** cursor = &queue.head;
                    while (*cursor)
                    {
                        auto* value = *cursor;
                        if (value->owner == threadToken())
                        {
                            *cursor = value->next;
                            value->next = selected;
                            selected = value;
                        }
                        else
                            cursor = &value->next;
                    }
                }
                std::uint64_t count = 0;
                while (selected)
                {
                    auto* value = selected;
                    selected = value->next;
                    (void)value->api->close(&value->session);
                    if (!value->session.context && !value->session.contract)
                    {
                        delete value;
                        ++count;
                    }
                    else
                        release(value);
                }
                return count;
            }
        };

        static std::shared_ptr<SessionOwner> openSession(const WioSdkModuleSessionApi& api,
                                                         std::shared_ptr<SessionCode> code,
                                                         const WioSdkModuleTaskApi* tasks = nullptr)
        {
            if (!WioValidateSdkModuleSessionApi(&api))
                throw std::invalid_argument("invalid canonical module session API");
            if (tasks && !WioValidateSdkModuleTaskApi(tasks, &api))
                throw std::invalid_argument("invalid canonical session task API");
            auto value = std::shared_ptr<SessionOwner>(
                new SessionOwner{&api, tasks, {}, std::move(code), SessionOwner::threadToken()},
                &SessionOwner::release);
            const auto status = api.open(api.factory, &value->session);
            if (status != WIO_NATIVE_ABI_OK || !value->session.context ||
                !WioValidateSdkModuleContract(value->session.contract))
                throw std::runtime_error("cannot open canonical module session");
            for (std::uint32_t i = 0; i < value->session.contract->exportCount; ++i)
                if (value->session.contract->exports[i].reserved != WIO_SDK_CALL_NATIVE_ABI_V2)
                    throw std::runtime_error("unsupported canonical session value ABI");
            return value;
        }

        struct State
        {
            decltype(detail::openLibrary(std::filesystem::path{})) handle{};
            const WioSdkModuleContract* contract{};
            const WioModuleApi* legacy{};
            const WioNativeTaskApi* tasks{};
            const WioNativeTaskCompletionApi* taskCompletions{};
            const WioNativeAbiRegistry* natives{};
            const WioNativeReflectionApi* reflection{};
            std::shared_ptr<void> attachedOwner;
            std::shared_ptr<SessionOwner> sessionOwner;
            std::shared_ptr<detail::BindingState> binding = std::make_shared<detail::BindingState>();
            bool started = false;
            ~State()
            {
                if (tasks && tasks->shutdown)
                    tasks->shutdown();
                if (started && legacy && legacy->unload)
                    legacy->unload();
                if (handle)
                    detail::closeLibrary(handle);
            }
        };
        std::shared_ptr<State> state_;

        static std::shared_ptr<SessionOwner> taskOwner(const std::shared_ptr<State>& state, const NativeValue& task)
        {
            if (!state || !state->contract || !state->sessionOwner || !state->sessionOwner->tasks)
                throw std::runtime_error("canonical session task API unavailable");
            const auto provenance = task.taskSession_.lock();
            if (!provenance || provenance.get() != state->sessionOwner.get())
                throw std::runtime_error("canonical task belongs to a different/closed session");
            return state->sessionOwner;
        }

        struct CompletionSubscription
        {
            NativeValue lease;
            std::function<void(std::int32_t)> callback;
            static void deliver(void* userdata, WioNativeAbiStatus status) noexcept
            {
                std::unique_ptr<CompletionSubscription> value{static_cast<CompletionSubscription*>(userdata)};
                try
                {
                    value->callback(status);
                }
                catch (...)
                {
                }
            }
        };

    public:
        static std::uint64_t collectSessions() noexcept
        {
            return SessionOwner::collect();
        }

        static NativeModule attachSession(const WioSdkModuleSessionApi& api, std::shared_ptr<void> owner,
                                          const WioSdkModuleTaskApi* tasks = nullptr)
        {
            if (!owner)
                throw std::invalid_argument("canonical session attachment requires a code/factory lifetime owner");
            auto code = std::make_shared<SessionCode>();
            code->attachedOwner = std::move(owner);
            auto instance = openSession(api, std::move(code), tasks);
            NativeModule module;
            module.state_ = std::make_shared<State>();
            module.state_->contract = instance->session.contract;
            module.state_->sessionOwner = std::move(instance);
            return module;
        }
        // The owner pins descriptor/context storage and producer code, including
        // owning results that survive close(). No module lifecycle is inferred.
        static NativeModule attach(const WioSdkModuleContract& contract, std::shared_ptr<void> owner)
        {
            if (!owner || !WioValidateSdkModuleContract(&contract))
                throw std::invalid_argument("canonical attachment requires a valid contract and lifetime owner");
            for (std::uint32_t i = 0; i < contract.exportCount; ++i)
                if (contract.exports[i].reserved != WIO_SDK_CALL_NATIVE_ABI_V2)
                    throw std::invalid_argument("unsupported canonical attachment value ABI");
            if (contract.lifecycleFlags != 0 || contract.stateSchemaVersion != 0)
                throw std::invalid_argument("canonical attachment lifecycle requires a dedicated host adapter");
            NativeModule module;
            module.state_ = std::make_shared<State>();
            module.state_->contract = &contract;
            module.state_->attachedOwner = std::move(owner);
            return module;
        }

        static NativeModule open(const std::filesystem::path& path)
        {
            NativeModule module;
            auto state = std::make_shared<State>();
            state->handle = detail::openLibrary(std::filesystem::absolute(path));
            if (!state->handle)
                throw std::runtime_error("cannot open canonical Wio module");
            auto sessionGet =
                detail::loadSymbol<WioGetSdkModuleSessionApiFn>(state->handle, "WioGetSdkModuleSessionApi");
            if (sessionGet)
            {
                const auto* api = sessionGet();
                if (!api)
                    throw std::runtime_error("invalid canonical Wio module descriptor");
                if (!WioValidateSdkModuleSessionApi(api))
                    throw std::runtime_error("invalid canonical module session API");
                auto code = std::make_shared<SessionCode>();
                code->handle = std::exchange(state->handle, {});
                const WioSdkModuleTaskApi* tasks = nullptr;
                auto taskGet = detail::loadSymbol<WioGetSdkModuleTaskApiFn>(code->handle, "WioGetSdkModuleTaskApi");
                if (taskGet)
                {
                    tasks = taskGet();
                    if (!WioValidateSdkModuleTaskApi(tasks, api))
                        throw std::runtime_error("invalid canonical session task API");
                }
                state->sessionOwner = openSession(*api, std::move(code), tasks);
                state->contract = state->sessionOwner->session.contract;
                module.state_ = std::move(state);
                return module;
            }
            auto get = detail::loadSymbol<WioGetSdkModuleContractFn>(state->handle, "WioGetSdkModuleContract");
            if (!get || !WioValidateSdkModuleContract(state->contract = get()))
                throw std::runtime_error("invalid canonical Wio module descriptor");
            for (std::uint32_t i = 0; i < state->contract->exportCount; ++i)
                if (state->contract->exports[i].reserved != WIO_SDK_CALL_NATIVE_ABI_V2)
                    throw std::runtime_error("unsupported canonical value ABI");
            auto legacy = detail::loadSymbol<WioModuleGetApiFn>(state->handle, "WioModuleGetApi");
            if (legacy)
            {
                state->legacy = legacy();
                detail::validateModuleApi(state->legacy);
            }
            auto natives = detail::loadSymbol<WioGetNativeAbiRegistryFn>(state->handle, "WioGetNativeAbiRegistry");
            if (natives)
            {
                state->natives = natives();
                if (!state->natives || state->natives->abiVersion != WIO_NATIVE_ABI_VERSION ||
                    (state->natives->functionCount && !state->natives->functions))
                    throw std::runtime_error("invalid native ABI registry");
                for (std::uint32_t i = 0; i < state->natives->functionCount; ++i)
                {
                    const auto& f = state->natives->functions[i];
                    if (f.abiVersion != WIO_NATIVE_ABI_VERSION || !f.thunk || !f.stableKey || !f.nativeSymbol ||
                        !f.thunkSymbol)
                        throw std::runtime_error("invalid native ABI entry");
                }
            }
            auto tasks = detail::loadSymbol<WioGetNativeTaskApiFn>(state->handle, "WioGetNativeTaskApi");
            if (tasks)
            {
                const auto* api = tasks();
                if (!api || api->abiVersion != WIO_NATIVE_ABI_VERSION || !api->ready || !api->cancel || !api->read ||
                    !api->bindMain || !api->pumpMain || !api->shutdown)
                    throw std::runtime_error("unsupported task ABI");
                state->tasks = api;
            }
            auto taskCompletions =
                detail::loadSymbol<WioGetNativeTaskCompletionApiFn>(state->handle, "WioGetNativeTaskCompletionApi");
            if (taskCompletions)
            {
                const auto* api = taskCompletions();
                if (!api || api->abiVersion != WIO_NATIVE_ABI_VERSION || !api->onComplete)
                    throw std::runtime_error("unsupported task completion ABI");
                state->taskCompletions = api;
            }
            auto reflect = detail::loadSymbol<WioGetNativeReflectionApiFn>(state->handle, "WioGetNativeReflectionApi");
            if (reflect)
            {
                state->reflection = reflect();
                if (!state->reflection || state->reflection->version != WIO_NATIVE_REFLECTION_VERSION ||
                    (state->reflection->typeCount && !state->reflection->types) ||
                    (state->reflection->attributeCount && !state->reflection->attributes))
                    throw std::runtime_error("invalid reflection sidecar");
                std::unordered_set<std::uint64_t> typeIds, attributeIds;
                for (std::uint32_t i = 0; i < state->reflection->typeCount; ++i)
                {
                    const auto& t = state->reflection->types[i];
                    if (!t.stableId || !t.name || !typeIds.insert(t.stableId).second || (t.fieldCount && !t.fields) ||
                        (t.methodCount && !t.methods) || (t.caseCount && !t.cases) ||
                        (t.constructorCount && !t.constructors))
                        throw std::runtime_error("invalid reflected type descriptor");
                    for (std::uint32_t f = 0; f < t.fieldCount; ++f)
                        if (!t.fields[f].stableId || !t.fields[f].name || !t.fields[f].typeId)
                            throw std::runtime_error("invalid reflected field descriptor");
                    for (std::uint32_t m = 0; m < t.methodCount; ++m)
                        if (!t.methods[m].stableId || !t.methods[m].name || !t.methods[m].resultTypeId ||
                            (t.methods[m].parameterCount && !t.methods[m].parameterTypes))
                            throw std::runtime_error("invalid reflected method descriptor");
                }
                for (std::uint32_t i = 0; i < state->reflection->attributeCount; ++i)
                {
                    const auto& a = state->reflection->attributes[i];
                    if (!a.stableId || !a.name || !a.targetId || !attributeIds.insert(a.stableId).second ||
                        (a.argumentCount && !a.arguments) || (a.processorCount && !a.processors))
                        throw std::runtime_error("invalid reflected attribute descriptor");
                }
            }
            module.state_ = std::move(state);
            return module;
        }
        void start()
        {
            const auto state = state_;
            if (!state)
                throw std::runtime_error("closed canonical module");
            if (state->sessionOwner && state->sessionOwner->owner != SessionOwner::threadToken())
                throw std::runtime_error("canonical session start requires its owning thread");
            if (!state->started)
            {
                if (state->sessionOwner)
                {
                    const auto& session = *state->sessionOwner;
                    const auto status = session.api->start(&session.session);
                    if (status != WIO_NATIVE_ABI_OK)
                        throw std::runtime_error("canonical session start failed with status " +
                                                 std::to_string(status));
                }
                if (state->tasks && state->tasks->bindMain)
                    state->tasks->bindMain();
                if (state->legacy && state->legacy->load && state->legacy->load() != 0)
                    throw std::runtime_error("module load failed");
                state->started = true;
            }
        }
        void close() noexcept
        {
            state_.reset();
        }
        void stop()
        {
            const auto state = state_;
            if (!state || !state->sessionOwner)
                throw std::runtime_error("canonical session required");
            auto& instance = *state->sessionOwner;
            if (instance.owner != SessionOwner::threadToken())
                throw std::runtime_error("canonical session stop requires its owning thread");
            const auto status = instance.api->close(&instance.session);
            if (!instance.session.context)
            {
                state->contract = nullptr;
                state->started = false;
            }
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical session stop failed with status " + std::to_string(status));
        }

        void update(double deltaSeconds) const
        {
            const auto state = state_;
            if (!state || !state->sessionOwner)
                throw std::runtime_error("canonical session required");
            const auto& instance = *state->sessionOwner;
            const auto status = instance.api->update(&instance.session, deltaSeconds);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical session update failed with status " + std::to_string(status));
        }

        std::uint32_t apiVersion() const
        {
            const auto state = state_;
            if (!state || !state->sessionOwner)
                throw std::runtime_error("canonical session required");
            const auto& instance = *state->sessionOwner;
            std::uint32_t result = 0;
            const auto status = instance.api->apiVersion(&instance.session, &result);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical session version failed with status " + std::to_string(status));
            return result;
        }

        NativeValue saveState() const
        {
            const auto state = state_;
            if (!state || !state->sessionOwner)
                throw std::runtime_error("canonical session required");
            const auto& instance = *state->sessionOwner;
            NativeValue result;
            result.keepAlive_ = instance.code;
            const auto status = instance.api->saveState(&instance.session, &result.value_);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical session save failed with status " + std::to_string(status));
            return result;
        }

        void restoreState(const NativeValue& value) const
        {
            const auto state = state_;
            if (!state || !state->sessionOwner)
                throw std::runtime_error("canonical session required");
            const auto& instance = *state->sessionOwner;
            const auto status = instance.api->restoreState(&instance.session, &value.value_);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical session restore failed with status " + std::to_string(status));
        }
        ApplicationHost application() const
        {
            if (!state_ || !state_->started || !state_->legacy || !state_->legacy->application)
                throw std::runtime_error("started canonical application module required");
            detail::publishBindingState(state_->binding, state_->legacy);
            return ApplicationHost(state_->legacy->application, state_->binding, state_->binding->generation, state_);
        }
        const WioSdkModuleContract* contract() const noexcept
        {
            return state_ ? state_->contract : nullptr;
        }
        const WioModuleApi* legacyApi() const noexcept
        {
            return state_ ? state_->legacy : nullptr;
        }
        const WioNativeAbiRegistry* nativeRegistry() const noexcept
        {
            return state_ ? state_->natives : nullptr;
        }
        const WioNativeReflectionApi* reflection() const noexcept
        {
            return state_ ? state_->reflection : nullptr;
        }
        std::span<const WioNativeTypeDescriptor> types() const noexcept
        {
            auto* api = reflection();
            return api ? std::span(api->types, api->typeCount) : std::span<const WioNativeTypeDescriptor>{};
        }
        std::span<const WioNativeAttributeDescriptor> attributes() const noexcept
        {
            auto* api = reflection();
            return api ? std::span(api->attributes, api->attributeCount)
                       : std::span<const WioNativeAttributeDescriptor>{};
        }
        std::vector<const WioNativeAttributeDescriptor*> attributesFor(std::uint64_t target) const
        {
            std::vector<const WioNativeAttributeDescriptor*> result;
            for (const auto& attribute : attributes())
                if (attribute.targetId == target)
                    result.push_back(&attribute);
            return result;
        }
        const WioNativeTypeDescriptor& type(std::string_view name) const
        {
            auto* api = reflection();
            if (api)
                for (std::uint32_t i = 0; i < api->typeCount; ++i)
                    if (name == api->types[i].name)
                        return api->types[i];
            throw std::runtime_error("reflected type not found");
        }
        const WioNativeTypeDescriptor& type(const NativeValue& value) const
        {
            auto* api = reflection();
            if (api)
                for (std::uint32_t i = 0; i < api->typeCount; ++i)
                    if (value.raw().typeId == api->types[i].stableId)
                        return api->types[i];
            throw std::runtime_error("reflected value type not found");
        }
        NativeValue invokeNative(WioNativeAbiThunk thunk, std::span<const NativeValue> args = {}) const
        {
            if (!state_ || !thunk)
                throw std::runtime_error("reflected operation is not callable");
            std::vector<WioNativeAbiValue> wire;
            for (const auto& value : args)
                wire.push_back(value.raw());
            NativeValue result;
            result.keepAlive_ = state_;
            WioNativeAbiFailure error;
            auto status = thunk(wire.data(), wire.size(), &result.value_, &error);
            for (const auto& arg : args)
                if (arg.borrowed_ && (arg.raw().flags & WIO_NATIVE_ABI_VALUE_MUTABLE))
                    arg.borrowed_->pin(state_);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("reflected call failed with status " + std::to_string(status));
            return result;
        }
        NativeValue construct(std::string_view name, std::span<const NativeValue> args = {}) const
        {
            const auto& t = type(name);
            WioNativeAbiThunk selected = nullptr;
            for (std::uint32_t i = 0; i < t.constructorCount; ++i)
                if (t.constructors[i].parameterCount == args.size())
                {
                    if (selected)
                        throw std::runtime_error("ambiguous constructor; select a descriptor and use invokeNative");
                    selected = t.constructors[i].invoke;
                }
            return invokeNative(selected, args);
        }
        NativeValue getField(const NativeValue& value, std::string_view name) const
        {
            const auto& t = type(value);
            for (std::uint32_t i = 0; i < t.fieldCount; ++i)
                if (name == t.fields[i].name)
                    return invokeNative(t.fields[i].get, std::span(&value, 1));
            throw std::runtime_error("reflected field not found");
        }
        void setField(NativeValue& value, std::string_view name, const NativeValue& input) const
        {
            const auto& t = type(value);
            for (std::uint32_t i = 0; i < t.fieldCount; ++i)
                if (name == t.fields[i].name)
                {
                    std::vector<NativeValue> args;
                    args.push_back(value.borrow());
                    args.push_back(input.clone());
                    invokeNative(t.fields[i].set, args);
                    return;
                }
            throw std::runtime_error("reflected field not found");
        }
        NativeValue callMethod(NativeValue& value, std::string_view name, std::span<const NativeValue> input = {}) const
        {
            const auto& t = type(value);
            const WioNativeMethodDescriptor* selected = nullptr;
            for (std::uint32_t i = 0; i < t.methodCount; ++i)
                if (name == t.methods[i].name && t.methods[i].parameterCount == input.size())
                {
                    if (selected)
                        throw std::runtime_error("ambiguous method; select a descriptor and use invokeNative");
                    selected = &t.methods[i];
                }
            std::vector<NativeValue> args;
            args.push_back(value.borrow());
            for (const auto& arg : input)
                args.push_back(arg.clone());
            return invokeNative(selected ? selected->invoke : nullptr, args);
        }
        std::uint64_t pumpMain() const
        {
            const auto state = state_;
            if (state && state->sessionOwner && state->sessionOwner->tasks)
            {
                const auto& instance = *state->sessionOwner;
                std::uint64_t result = 0;
                const auto status = instance.tasks->pumpMain(&instance.session, &result);
                if (status != WIO_NATIVE_ABI_OK)
                    throw std::runtime_error("session main pump failed with status " + std::to_string(status));
                return result;
            }
            if (!state_ || !state_->tasks || !state_->tasks->pumpMain)
                return 0;
            return state_->tasks->pumpMain();
        }
        bool taskReady(const NativeValue& task) const
        {
            const auto state = state_;
            if (state && state->sessionOwner)
            {
                const auto instance = taskOwner(state, task);
                auto lease = task.clone();
                bool ready = false;
                const auto status = instance->tasks->ready(&instance->session, &lease.value_, &ready);
                if (status != WIO_NATIVE_ABI_OK)
                    throw std::runtime_error("session task query failed with status " + std::to_string(status));
                return ready;
            }
            bool ready = false;
            if (!state_ || !state_->tasks || state_->tasks->ready(&task.value_, &ready) != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("invalid canonical task");
            return ready;
        }
        void cancelTask(const NativeValue& task) const
        {
            const auto state = state_;
            if (state && state->sessionOwner)
            {
                const auto instance = taskOwner(state, task);
                auto lease = task.clone();
                const auto status = instance->tasks->cancel(&instance->session, &lease.value_);
                if (status != WIO_NATIVE_ABI_OK)
                    throw std::runtime_error("session task cancellation failed with status " + std::to_string(status));
                return;
            }
            if (!state_ || !state_->tasks || state_->tasks->cancel(&task.value_) != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("invalid canonical task");
        }
        NativeValue readTask(const NativeValue& task) const
        {
            const auto state = state_;
            if (state && state->sessionOwner)
            {
                const auto instance = taskOwner(state, task);
                auto lease = task.clone();
                NativeValue result;
                result.keepAlive_ = lease.keepAlive_;
                WioNativeAbiFailure error{};
                const auto status = instance->tasks->read(&instance->session, &lease.value_, &result.value_, &error);
                if (status != WIO_NATIVE_ABI_OK)
                {
                    const auto message = error.message.data && error.message.size
                                             ? std::string{static_cast<const char*>(error.message.data),
                                                           static_cast<std::size_t>(error.message.size)}
                                             : std::string{};
                    throw std::runtime_error("session task read failed with status " + std::to_string(status) +
                                             (message.empty() ? "" : ": " + message));
                }
                return result;
            }
            if (!state_ || !state_->tasks)
                throw std::runtime_error("canonical task API unavailable");
            NativeValue result;
            result.keepAlive_ = state_;
            WioNativeAbiFailure error;
            auto status = state_->tasks->read(&task.value_, &result.value_, &error);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("task read failed with status " + std::to_string(status));
            return result;
        }
        void onTaskComplete(const NativeValue& task, std::function<void(std::int32_t)> callback) const
        {
            const auto state = state_;
            const auto instance = taskOwner(state, task);
            if (!callback)
                throw std::invalid_argument("task completion callback cannot be empty");
            auto lease = task.clone();
            auto subscription =
                std::make_unique<CompletionSubscription>(CompletionSubscription{lease.clone(), std::move(callback)});
            auto* raw = subscription.release();
            const auto status =
                instance->tasks->onComplete(&instance->session, &lease.value_, &CompletionSubscription::deliver, raw);
            if (status != WIO_NATIVE_ABI_OK)
            {
                delete raw;
                throw std::runtime_error("session task subscription failed with status " + std::to_string(status));
            }
        }

        bool waitTaskReady(const NativeValue& task, std::chrono::milliseconds timeout) const
        {
            if (timeout.count() < 0)
                throw std::invalid_argument("task wait timeout cannot be negative");
            auto lease = task.clone();
            auto view = *this;
            const auto start = std::chrono::steady_clock::now();
            while (!view.taskReady(lease))
            {
                (void)view.pumpMain();
                if (view.taskReady(lease))
                    return true;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start) >=
                    timeout)
                    return false;
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            return true;
        }

        NativeValue waitTask(const NativeValue& task) const
        {
            auto lease = task.clone();
            auto view = *this;
            while (!view.taskReady(lease))
            {
                (void)view.pumpMain();
                std::this_thread::sleep_for(std::chrono::milliseconds{1});
            }
            return view.readTask(lease);
        }
        NativeValue invoke(std::uint64_t stableId, std::span<const NativeValue> arguments = {}) const
        {
            const auto state = state_;
            if (!state || !state->contract)
                throw std::runtime_error("closed canonical module");
            const auto* entry = WioFindSdkExport(state->contract, stableId);
            if (!entry)
                throw std::runtime_error("canonical export not found");
            std::vector<WioNativeAbiValue> wire;
            for (const auto& arg : arguments)
            {
                if (state->sessionOwner && (arg.value_.flags & WIO_NATIVE_ABI_VALUE_TASK_SOURCE))
                    (void)taskOwner(state, arg);
                wire.push_back(arg.value_);
            }
            NativeValue result;
            result.keepAlive_ = state->sessionOwner ? std::shared_ptr<void>(state->sessionOwner->code) : state;
            const auto& call = state->contract->callTable[entry->callTableSlot];
            auto status =
                call.invoke(wire.data(), static_cast<std::uint32_t>(wire.size()), &result.value_, call.context);
            for (const auto& arg : arguments)
                if (arg.borrowed_ && (arg.value_.flags & WIO_NATIVE_ABI_VALUE_MUTABLE))
                    arg.borrowed_->pin(state);
            if (status != WIO_NATIVE_ABI_OK)
                throw std::runtime_error("canonical invocation failed with status " + std::to_string(status));
            if (state->sessionOwner && (result.value_.flags & WIO_NATIVE_ABI_VALUE_TASK_SOURCE))
                result.taskSession_ = state->sessionOwner;
            return result;
        }
        NativeValue invoke(std::string_view name, std::span<const NativeValue> arguments = {}) const
        {
            if (!state_ || !state_->contract)
                throw std::runtime_error("closed canonical module");
            for (std::uint32_t i = 0; i < state_->contract->exportCount; ++i)
            {
                const auto& entry = state_->contract->exports[i];
                if (entry.logicalName == name)
                    return invoke(entry.stableId, arguments);
            }
            throw std::runtime_error("canonical export name not found");
        }
    };
} // namespace wio::sdk
