#pragma once

#include "native_pod_internal.h"

#include <functional>
#include <memory>
#include <mutex>

namespace wio::vm::detail
{
    [[nodiscard]] bool mergeNativeObjects(const bytecode::Module& module, const WioNativeObjectApi& api,
                                          NativePodLayouts& layouts, std::string& error);

    class NativeObjectBridge final : public std::enable_shared_from_this<NativeObjectBridge>
    {
    public:
        using Invoke = std::function<WioNativeAbiStatus(const Value&, std::uint64_t, const WioNativeAbiValue*,
                                                        std::uint64_t, WioNativeAbiValue*, WioNativeAbiFailure*)>;
        using CodeLease = std::function<std::shared_ptr<void>()>;
        NativeObjectBridge(Invoke invoke, CodeLease lease);
        ~NativeObjectBridge();
        [[nodiscard]] bool encode(const Value& object, std::uint32_t declaredType,
                                  const WioNativeObjectDescriptor& adapter, WioNativeAbiValue& output,
                                  std::string& error);
        // matched=false preserves genuinely producer-owned (non-hosted) objects.
        [[nodiscard]] NativeValueResult recover(const WioNativeAbiValue& value, std::uint32_t declaredType,
                                                const WioNativeObjectDescriptor& adapter, bool& matched);
        [[nodiscard]] std::vector<Value> takeRetired();
        [[nodiscard]] std::vector<Value> shutdown();
        // Call only from host code after producer ABI calls have returned.
        static void collectRetiredCode();

    private:
        struct State;
        struct Owner;
        struct Cache;
        static const WioNativeAbiHandleOps ownerOps;
        static WioNativeAbiStatus dispatch(const WioNativeObjectHost*, std::uint64_t, const WioNativeAbiValue*,
                                           std::uint64_t, WioNativeAbiValue*, WioNativeAbiFailure*) noexcept;
        void retire(const std::shared_ptr<State>& state);
        std::recursive_mutex mutex_;
        Invoke invoke_;
        CodeLease lease_;
        std::unordered_map<std::uint64_t, std::shared_ptr<State>> states_;
        std::unordered_map<std::uint64_t, std::unique_ptr<Cache>> cache_;
        std::shared_ptr<State> retired_;
        std::size_t retiredCount_ = 0;
        static std::mutex retiredCodeMutex_;
        static std::shared_ptr<State> retiredCode_;
        std::uint64_t generation_ = 0;
        std::uint32_t materializations_ = 0;
        bool closed_ = false;
    };
} // namespace wio::vm::detail
