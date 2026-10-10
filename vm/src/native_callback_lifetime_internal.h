#pragma once

#include "wio/vm/value.h"

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace wio::vm::detail
{
    struct NativeCallbackClosure final
    {
        Value callable;

    private:
        friend class NativeCallbackLifetimeDomain;
        std::shared_ptr<NativeCallbackClosure> nextRetired;
    };

    // Native release can run on a foreign thread. Retire without executing Wio;
    // the machine drains the closures under its execution lock. Shutdown takes
    // every remaining callable, including leases still held by native code.
    class NativeCallbackLifetimeDomain final
    {
    public:
        [[nodiscard]] std::shared_ptr<NativeCallbackClosure> retain(const Value& callable);
        void retire(const std::shared_ptr<NativeCallbackClosure>& closure) noexcept;
        [[nodiscard]] std::vector<Value> takeRetired(bool shutdown);

    private:
        void enqueue(std::shared_ptr<NativeCallbackClosure> closure) noexcept;
        std::mutex mutex_;
        std::unordered_map<NativeCallbackClosure*, std::shared_ptr<NativeCallbackClosure>> live_;
        std::shared_ptr<NativeCallbackClosure> retired_;
        std::size_t retiredCount_ = 0;
    };
} // namespace wio::vm::detail
