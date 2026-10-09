#include "native_object_internal.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <thread>

namespace wio::vm::detail
{
    bool mergeNativeObjects(const bytecode::Module& module, const WioNativeObjectApi& api, NativePodLayouts& layouts,
                            std::string& error)
    {
        const auto reject = [&](const char* message)
        {
            error = message;
            return false;
        };
        if (api.version != WIO_NATIVE_OBJECT_VERSION || (api.typeCount && !api.types))
            return reject("native object API version or descriptor array is invalid");
        auto candidate = layouts;
        for (std::uint32_t index = 0; index < api.typeCount; ++index)
        {
            const auto& descriptor = api.types[index];
            constexpr auto Flags = WIO_NATIVE_OBJECT_HOST_PROXY | WIO_NATIVE_OBJECT_WEAK_CACHE;
            if (!descriptor.typeId || descriptor.flags != Flags || !descriptor.create || !descriptor.host ||
                !descriptor.weak || !descriptor.lock)
                return reject("native object descriptor is incomplete or has incompatible capabilities");
            const auto reflected = std::ranges::find_if(module.contract.reflection, [&](const auto& entry)
                                                        { return entry.stableTypeId == descriptor.typeId; });
            if (reflected == module.contract.reflection.end())
                continue;
            if (reflected->type >= module.types.size())
                return reject("native object descriptor references an invalid VM type");
            const auto& type = module.types[reflected->type];
            if (type.kind != 28 || type.nominalKind != 2)
                return reject("native object descriptor does not describe a concrete VM object");
            const auto [identity, unique] = candidate.objectTypes.emplace(descriptor.typeId, reflected->type);
            if (!unique && identity->second != reflected->type)
                return reject("native object stable identity names multiple concrete VM types");
            const auto [found, inserted] = candidate.objects.emplace(reflected->type, descriptor);
            const auto& previous = found->second;
            if (!inserted && (previous.typeId != descriptor.typeId || previous.flags != descriptor.flags ||
                              previous.create != descriptor.create || previous.host != descriptor.host ||
                              previous.weak != descriptor.weak || previous.lock != descriptor.lock))
                return reject("native object type is already registered with a different producer adapter");
            std::vector<std::uint32_t> hierarchy{reflected->type};
            for (std::size_t cursor = 0; cursor < hierarchy.size(); ++cursor)
            {
                const auto contract = hierarchy[cursor];
                if (contract >= module.types.size())
                    return reject("native object hierarchy references an invalid VM type");
                if (candidate.objectContracts.insert(contract).second)
                    hierarchy.insert(hierarchy.end(), module.types[contract].baseTypes.begin(),
                                     module.types[contract].baseTypes.end());
            }
        }
        layouts = std::move(candidate);
        return true;
    }

    namespace
    {
        WioNativeAbiStatus objectFailure(WioNativeAbiFailure* failure, WioNativeAbiStatus status,
                                         const std::string& message) noexcept
        {
            if (failure)
            {
                *failure = {};
                failure->status = status;
                try
                {
                    thread_local std::string diagnostic;
                    diagnostic = message;
                    failure->message = {diagnostic.data(), diagnostic.size()};
                }
                catch (...)
                {
                }
            }
            return status;
        }

        struct WireGuard
        {
            WioNativeAbiValue& value;
            bool active = true;
            ~WireGuard()
            {
                if (active)
                    WioNativeAbiReleaseValue(&value);
            }
        };
    } // namespace

    struct NativeObjectBridge::State
    {
        Value object;
        std::uint32_t type;
        std::uint64_t typeId;
        std::uint64_t id;
        std::uint64_t generation;
        std::thread::id caller;
        std::shared_ptr<void> code;
        std::shared_ptr<State> nextRetired;
        std::shared_ptr<State> nextCode;
    };

    std::mutex NativeObjectBridge::retiredCodeMutex_;
    std::shared_ptr<NativeObjectBridge::State> NativeObjectBridge::retiredCode_;

    struct NativeObjectBridge::Owner
    {
        std::atomic_uint32_t references{1};
        std::shared_ptr<NativeObjectBridge> bridge;
        std::shared_ptr<State> state;
    };

    struct NativeObjectBridge::Cache
    {
        WioNativeObjectDescriptor adapter;
        WioNativeAbiHandle weak{};
        std::shared_ptr<void> code;
        ~Cache()
        {
            WioNativeAbiRelease(&weak);
        }
    };

    const WioNativeAbiHandleOps NativeObjectBridge::ownerOps{
        WIO_NATIVE_ABI_VERSION, 0, [](void* pointer) noexcept { ++static_cast<Owner*>(pointer)->references; },
        [](void* pointer) noexcept
        {
            auto* owner = static_cast<Owner*>(pointer);
            if (--owner->references == 0)
            {
                owner->bridge->retire(owner->state);
                delete owner;
            }
        },
        [](const void* pointer) noexcept { return static_cast<const Owner*>(pointer)->state->typeId; }};

    NativeObjectBridge::NativeObjectBridge(Invoke invoke, CodeLease lease)
        : invoke_(std::move(invoke)), lease_(std::move(lease))
    {
    }
    NativeObjectBridge::~NativeObjectBridge() = default;

    void NativeObjectBridge::retire(const std::shared_ptr<State>& state)
    {
        std::scoped_lock lock{mutex_};
        if (!state->object.isEmpty())
        {
            state->nextRetired = std::move(retired_);
            retired_ = state;
            ++retiredCount_;
        }
        const auto found = states_.find(state->id);
        if (found != states_.end() && found->second == state)
            states_.erase(found);
        if (closed_ && state->code)
        {
            // Releasing a last module lease here could unload producer code
            // while its OnZeroStrong is still returning. Host safe points drain it.
            std::scoped_lock codeLock{retiredCodeMutex_};
            state->nextCode = std::move(retiredCode_);
            retiredCode_ = state;
        }
    }

    WioNativeAbiStatus NativeObjectBridge::dispatch(const WioNativeObjectHost* token, std::uint64_t method,
                                                    const WioNativeAbiValue* arguments, std::uint64_t count,
                                                    WioNativeAbiValue* output, WioNativeAbiFailure* failure) noexcept
    {
        if (output)
            *output = {};
        if (failure)
            *failure = {};
        if (!token || !output || token->version != WIO_NATIVE_OBJECT_VERSION || token->flags != 0 ||
            !token->owner.state || token->owner.ops != &ownerOps || (count && !arguments))
            return objectFailure(failure, WIO_NATIVE_ABI_INVALID_ARGUMENT, "invalid VM object dispatch token");
        try
        {
            auto& owner = *static_cast<Owner*>(token->owner.state);
            Value object;
            {
                std::scoped_lock lock{owner.bridge->mutex_};
                const auto& state = *owner.state;
                if (owner.bridge->closed_ || state.object.isEmpty() || state.id != token->objectId ||
                    state.generation != token->generation || token->owner.generation != state.generation ||
                    state.typeId != token->typeId || token->invoke != &dispatch)
                    return objectFailure(failure, WIO_NATIVE_ABI_STALE_HANDLE, "VM object host is no longer alive");
                if (state.caller != std::this_thread::get_id())
                    return objectFailure(failure, WIO_NATIVE_ABI_WRONG_THREAD,
                                         "VM object dispatch requires its caller thread");
                object = state.object;
            }
            return owner.bridge->invoke_(object, method, arguments, count, output, failure);
        }
        catch (const std::exception& error)
        {
            return objectFailure(failure, WIO_NATIVE_ABI_EXCEPTION, error.what());
        }
        catch (...)
        {
            return objectFailure(failure, WIO_NATIVE_ABI_EXCEPTION, "VM object dispatch failed");
        }
    }

    bool NativeObjectBridge::encode(const Value& object, std::uint32_t type, const WioNativeObjectDescriptor& adapter,
                                    WioNativeAbiValue& output, std::string& error)
    {
        output = {};
        std::scoped_lock lock{mutex_};
        if (closed_ || !object.objectIdentity() || object.aggregateType() != type)
        {
            error = "VM object materialization requires a live exact concrete object type";
            return false;
        }
        const auto id = object.objectIdentity();
        if (const auto cached = cache_.find(id); cached != cache_.end())
        {
            WioNativeAbiFailure failure{};
            const auto status = cached->second->adapter.lock(&cached->second->weak, &output, &failure);
            if (status == WIO_NATIVE_ABI_OK)
            {
                bool matched = false;
                const auto recovered = recover(output, type, adapter, matched);
                if (matched && recovered.succeeded() && recovered.value.objectIdentity() == id &&
                    output.kind == WIO_NATIVE_ABI_OBJECT && output.typeId == adapter.typeId &&
                    (output.flags & WIO_NATIVE_ABI_VALUE_OWNED))
                {
                    error.clear();
                    return true;
                }
                WioNativeAbiReleaseValue(&output);
                error = "native object weak lock returned an incompatible host identity";
                return false;
            }
            error = nativeFailureMessage(failure);
            WioNativeAbiReleaseValue(&output);
            if (status != WIO_NATIVE_ABI_STALE_HANDLE)
                return false;
            cache_.erase(cached);
        }
        if (generation_ == std::numeric_limits<std::uint64_t>::max())
        {
            error = "VM object host generation space exhausted";
            return false;
        }
        auto state = std::make_shared<State>(State{object.retainObject(), type, adapter.typeId, id, ++generation_,
                                                   std::this_thread::get_id(), lease_()});
        auto* owner = new Owner{.bridge = shared_from_this(), .state = state};
        WioNativeAbiHandle claim{owner, &ownerOps, state->generation};
        struct ClaimGuard
        {
            WioNativeAbiHandle& claim;
            ~ClaimGuard()
            {
                WioNativeAbiRelease(&claim);
            }
        } claimGuard{claim};
        states_.insert_or_assign(id, state);
        WireGuard outputGuard{output};
        const WioNativeObjectHost token{.typeId = adapter.typeId,
                                        .objectId = id,
                                        .generation = state->generation,
                                        .owner = claim,
                                        .invoke = &dispatch};
        WioNativeAbiFailure failure{};
        auto status = adapter.create(&token, &output, &failure);
        WioNativeObjectHost recovered{};
        if (status == WIO_NATIVE_ABI_OK)
        {
            if (output.kind != WIO_NATIVE_ABI_OBJECT || output.typeId != adapter.typeId ||
                !(output.flags & WIO_NATIVE_ABI_VALUE_OWNED) || owner->references < 2)
                status = WIO_NATIVE_ABI_TYPE_MISMATCH;
            else
                status = adapter.host(&output, &recovered, &failure);
            if (status == WIO_NATIVE_ABI_OK &&
                (recovered.version != WIO_NATIVE_OBJECT_VERSION || recovered.flags != 0 ||
                 recovered.owner.state != owner || recovered.owner.ops != &ownerOps || recovered.objectId != id ||
                 recovered.owner.generation != state->generation || recovered.generation != state->generation ||
                 recovered.typeId != adapter.typeId || recovered.invoke != &dispatch))
                status = WIO_NATIVE_ABI_TYPE_MISMATCH;
        }
        auto cached = std::make_unique<Cache>();
        cached->adapter = adapter;
        cached->code = state->code;
        if (status == WIO_NATIVE_ABI_OK)
            status = adapter.weak(&output, &cached->weak, &failure);
        if (status == WIO_NATIVE_ABI_OK &&
            (!cached->weak.state || !cached->weak.ops || cached->weak.ops->abiVersion != WIO_NATIVE_ABI_VERSION ||
             !cached->weak.ops->retain || !cached->weak.ops->release || !cached->weak.ops->typeId ||
             cached->weak.ops->typeId(cached->weak.state) != adapter.typeId))
            status = WIO_NATIVE_ABI_TYPE_MISMATCH;
        if (status != WIO_NATIVE_ABI_OK)
        {
            error = "native object factory/identity/weak-cache contract failed: " + nativeFailureMessage(failure);
            return false;
        }
        cache_.insert_or_assign(id, std::move(cached));
        // Bound expired weak metadata growth without a strong VM ownership cache.
        if (++materializations_ % 64 == 0)
            for (auto entry = cache_.begin(); entry != cache_.end();)
            {
                WioNativeAbiValue probe{};
                WireGuard guard{probe};
                const auto probeStatus = entry->second->adapter.lock(&entry->second->weak, &probe, nullptr);
                if (probeStatus == WIO_NATIVE_ABI_STALE_HANDLE)
                    entry = cache_.erase(entry);
                else
                    ++entry;
            }
        error.clear();
        // Transfer the owned wire; guards retain only their temporary host claim.
        outputGuard.active = false;
        return true;
    }

    NativeValueResult NativeObjectBridge::recover(const WioNativeAbiValue& value, std::uint32_t type,
                                                  const WioNativeObjectDescriptor& adapter, bool& matched)
    {
        matched = false;
        WioNativeObjectHost host{};
        WioNativeAbiFailure failure{};
        const auto status = adapter.host(&value, &host, &failure);
        if (status == WIO_NATIVE_ABI_TYPE_MISMATCH)
            return {};
        matched = true;
        if (status != WIO_NATIVE_ABI_OK)
            return {{}, "native object identity recovery failed: " + nativeFailureMessage(failure)};
        std::scoped_lock lock{mutex_};
        if (closed_ || host.version != WIO_NATIVE_OBJECT_VERSION || host.flags != 0 || host.owner.ops != &ownerOps ||
            !host.owner.state || host.invoke != &dispatch)
            return {{}, "native object belongs to an unavailable or different VM host"};
        auto& owner = *static_cast<Owner*>(host.owner.state);
        const auto& state = *owner.state;
        if (owner.bridge.get() != this || state.object.isEmpty() || state.type != type ||
            state.typeId != adapter.typeId || host.typeId != state.typeId || host.objectId != state.id ||
            host.generation != state.generation || host.owner.generation != state.generation)
            return {{}, "native object host identity or generation is incompatible"};
        return {state.object, {}};
    }

    std::vector<Value> NativeObjectBridge::takeRetired()
    {
        std::scoped_lock lock{mutex_};
        std::vector<Value> values;
        values.reserve(retiredCount_);
        while (retired_)
        {
            auto state = std::move(retired_);
            retired_ = std::move(state->nextRetired);
            values.push_back(std::move(state->object));
        }
        retiredCount_ = 0;
        return values;
    }

    std::vector<Value> NativeObjectBridge::shutdown()
    {
        std::scoped_lock lock{mutex_};
        closed_ = true;
        for (auto& [id, state] : states_)
            if (!state->object.isEmpty())
            {
                state->nextRetired = std::move(retired_);
                retired_ = state;
                ++retiredCount_;
            }
        states_.clear();
        cache_.clear();
        return takeRetired();
    }

    void NativeObjectBridge::collectRetiredCode()
    {
        std::shared_ptr<State> states;
        {
            std::scoped_lock lock{retiredCodeMutex_};
            states = std::move(retiredCode_);
        }
        while (states)
        {
            auto state = std::move(states);
            states = std::move(state->nextCode);
        }
    }
} // namespace wio::vm::detail
