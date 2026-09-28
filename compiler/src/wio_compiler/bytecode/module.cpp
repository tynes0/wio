#include "wio/bytecode/module.h"

namespace wio::bytecode
{
    std::string_view Module::string(const StringId id) const noexcept
    {
        return id < strings.size() ? std::string_view{strings[id]} : std::string_view{};
    }

    StringTableBuilder::StringTableBuilder(Module& module) : module_(module)
    {
        ids_.reserve(module.strings.size());
        for (StringId id = 0; id < module.strings.size(); ++id)
            ids_.try_emplace(module.strings[id], id);
    }

    StringId StringTableBuilder::intern(const std::string_view value)
    {
        if (const auto found = ids_.find(std::string{value}); found != ids_.end())
            return found->second;
        const StringId id = static_cast<StringId>(module_.strings.size());
        module_.strings.emplace_back(value);
        ids_.try_emplace(module_.strings.back(), id);
        return id;
    }
} // namespace wio::bytecode
