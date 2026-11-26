#include "wio/bytecode/module.h"

namespace wio::bytecode
{
    std::string_view Module::string(const StringId id) const noexcept
    {
        return id < strings.size() ? std::string_view{strings[id]} : std::string_view{};
    }

    std::vector<bool> Module::openTypes() const
    {
        // Propagate openness once over the reverse dependency graph. This is
        // linear in the type table and handles cycles without recursive scans
        // or a full-size visited allocation for every function parameter.
        std::vector<bool> open(types.size());
        std::vector<std::vector<std::uint32_t>> users(types.size());
        std::vector<std::uint32_t> pending;
        for (std::uint32_t id = 0; id < types.size(); ++id)
        {
            const auto& type = types[id];
            // Pinned TypeKind encodings: unresolved type/value/pack parameters.
            // A materialized pack is open only through its elements; a named
            // empty pack still denotes an unresolved pack parameter.
            if (type.kind == 21 || type.kind == 22 || type.kind == 24 ||
                (type.kind >= 25 && type.kind <= 27 && type.arguments.empty() && !string(type.name).empty()))
            {
                open[id] = true;
                pending.push_back(id);
            }
            const auto depend = [&](std::uint32_t dependency)
            {
                if (dependency < types.size())
                    users[dependency].push_back(id);
            };
            depend(type.extentParameter);
            for (auto argument : type.arguments)
                depend(argument);
            for (auto base : type.baseTypes)
                depend(base);
            for (const auto& field : type.fields)
                depend(field.type);
        }
        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();
            for (const auto user : users[current])
                if (!open[user])
                {
                    open[user] = true;
                    pending.push_back(user);
                }
        }
        return open;
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
