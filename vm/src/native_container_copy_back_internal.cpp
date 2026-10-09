#include "native_container_copy_back_internal.h"
#include "native_dictionary_internal.h"

#include <algorithm>
#include <limits>

namespace wio::vm::detail
{
    bool NativeContainerCopyBackPlanner::plan(Value& target, Value& replacement,
                                              std::vector<NativeCopyBackAction>& actions, std::string& error,
                                              unsigned depth)
    {
        if (depth > 64)
        {
            error = "native container copy-back exceeds the bridge depth limit";
            return false;
        }
        if (target.kind() == Value::Kind::Component && replacement.kind() == Value::Kind::Component &&
            target.aggregateType() == replacement.aggregateType() && target.fieldCount() == replacement.fieldCount())
        {
            actions.push_back({NativeCopyBackAction::Kind::RefreshPod, &target, &replacement, {}});
            for (std::size_t index = 0; index < target.fieldCount(); ++index)
                if (!planNativeCopyBack(*target.mutableField(index), *replacement.mutableField(index), actions, error,
                                        depth + 1))
                    return false;
            return true;
        }
        if (target.kind() == Value::Kind::Array && replacement.kind() == Value::Kind::Array &&
            target.elementCount() == replacement.elementCount())
        {
            for (std::size_t index = 0; index < target.elementCount(); ++index)
                if (!planNativeCopyBack(*target.mutableElement(index), *replacement.mutableElement(index), actions,
                                        error, depth + 1))
                    return false;
            return true;
        }
        if (target.kind() == Value::Kind::Dictionary && replacement.kind() == Value::Kind::Dictionary)
        {
            std::vector<std::size_t> order;
            if (!nativeDictionaryCopyBackMapping(target, replacement, order))
            {
                error = "native dictionary copy-back has an incompatible key/type contract";
                return false;
            }
            constexpr auto Missing = (std::numeric_limits<std::size_t>::max)();
            bool unchanged = target.dictionaryCount() == replacement.dictionaryCount();
            for (std::size_t index = 0; index < order.size(); ++index)
            {
                if (order[index] == Missing)
                    unchanged = false;
                else if (!planNativeCopyBack(*target.mutableDictionaryValue(index),
                                             *replacement.mutableDictionaryValue(order[index]), actions, error,
                                             depth + 1))
                    return false;
            }
            if (!unchanged)
            {
                if (target.strongReferenceCount() != 1)
                {
                    error = "native structural dictionary copy-back requires exclusive storage; iterator/snapshot "
                            "aliases are not qualified";
                    return false;
                }
                actions.push_back(
                    {NativeCopyBackAction::Kind::RebuildDictionary, &target, &replacement, std::move(order)});
            }
            return true;
        }
        if (target.kind() == Value::Kind::Array && target.strongReferenceCount() != 1)
        {
            error = "native structural array copy-back requires exclusive storage; iterator/snapshot aliases are not "
                    "qualified";
            return false;
        }
        actions.push_back({NativeCopyBackAction::Kind::Replace, &target, &replacement, {}});
        return true;
    }
} // namespace wio::vm::detail
