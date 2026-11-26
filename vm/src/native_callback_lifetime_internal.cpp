#include "native_callback_lifetime_internal.h"

namespace wio::vm::detail
{
    std::shared_ptr<NativeCallbackClosure> NativeCallbackLifetimeDomain::retain(const Value& callable)
    {
        auto closure = std::make_shared<NativeCallbackClosure>();
        closure->callable = callable;
        std::scoped_lock lock{mutex_};
        live_.emplace(closure.get(), closure);
        return closure;
    }

    void NativeCallbackLifetimeDomain::enqueue(std::shared_ptr<NativeCallbackClosure> closure) noexcept
    {
        closure->nextRetired = std::move(retired_);
        retired_ = std::move(closure);
        ++retiredCount_;
    }

    void NativeCallbackLifetimeDomain::retire(const std::shared_ptr<NativeCallbackClosure>& closure) noexcept
    {
        std::scoped_lock lock{mutex_};
        const auto found = live_.find(closure.get());
        if (found == live_.end())
            return;
        enqueue(std::move(found->second));
        live_.erase(found);
    }

    std::vector<Value> NativeCallbackLifetimeDomain::takeRetired(bool shutdown)
    {
        std::scoped_lock lock{mutex_};
        std::vector<Value> values;
        values.reserve(retiredCount_ + (shutdown ? live_.size() : 0));
        if (shutdown)
        {
            for (auto& [_, closure] : live_)
                enqueue(std::move(closure));
            live_.clear();
        }
        while (retired_)
        {
            auto closure = std::move(retired_);
            retired_ = std::move(closure->nextRetired);
            values.push_back(std::move(closure->callable));
        }
        retiredCount_ = 0;
        return values;
    }
} // namespace wio::vm::detail
