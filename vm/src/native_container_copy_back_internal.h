#pragma once

#include "wio/vm/value.h"

namespace wio::vm::detail
{
    struct NativeCopyBackAction final
    {
        enum class Kind
        {
            Replace,
            RefreshPod,
            RebuildDictionary
        };
        Kind kind;
        Value* target;
        Value* replacement;
        std::vector<std::size_t> dictionaryOrder;
    };

    class NativeContainerCopyBackPlanner final
    {
    public:
        [[nodiscard]] static bool plan(Value& target, Value& replacement, std::vector<NativeCopyBackAction>& actions,
                                       std::string& error, unsigned depth);
    };

    // Keep both graphs live until commit. Planning never changes caller storage;
    // a shape-changing incoming array may detach/reserve before its action is
    // published. Do not otherwise mutate graphs between planning and commit.
    // Plan children before their dictionary parent is rebuilt.
    [[nodiscard]] inline bool planNativeCopyBack(Value& target, Value& replacement,
                                                 std::vector<NativeCopyBackAction>& actions, std::string& error,
                                                 unsigned depth = 0)
    {
        return NativeContainerCopyBackPlanner::plan(target, replacement, actions, error, depth);
    }
} // namespace wio::vm::detail
