#include "object_layout_internal.h"
#include <algorithm>
#include <functional>
#include <unordered_set>

namespace wio::vm::detail
{
    namespace
    {
        constexpr std::uint8_t NamedType = 28;
        constexpr std::uint8_t ObjectNominal = 2;
        constexpr std::uint8_t InterfaceNominal = 3;
    } // namespace

    bool ObjectLayouts::build(const bytecode::Module& module, std::string& error)
    {
        layouts_.clear();
        layouts_.resize(module.types.size());
        std::vector<std::uint8_t> state(module.types.size());
        std::vector<std::size_t> heights(module.types.size());
        const auto nominal = [&](std::uint32_t id)
        {
            return id < module.types.size() && module.types[id].kind == NamedType &&
                   (module.types[id].nominalKind == ObjectNominal || module.types[id].nominalKind == InterfaceNominal);
        };
        const std::function<bool(std::uint32_t, std::size_t)> visit = [&](std::uint32_t id, std::size_t depth)
        {
            if (!nominal(id) || depth >= 128 || state[id] == 1)
                return false;
            if (state[id] == 2)
                return true;
            state[id] = 1;
            auto& layout = layouts_[id];
            layout.offsets.emplace(id, 0);
            for (const auto& field : module.types[id].fields)
                layout.fieldTypes.push_back(field.type);
            std::unordered_set<std::uint32_t> seen;
            std::size_t height = 1;
            for (const auto base : module.types[id].baseTypes)
            {
                if (!visit(base, depth + 1))
                    return false;
                height = std::max(height, heights[base] + 1);
                for (const auto owner : layouts_[base].constructionOwners)
                    if (seen.insert(owner).second)
                    {
                        layout.constructionOwners.push_back(owner);
                        if (owner != id && !layout.offsets.contains(owner))
                        {
                            layout.offsets.emplace(owner, layout.fieldTypes.size());
                            for (const auto& field : module.types[owner].fields)
                                layout.fieldTypes.push_back(field.type);
                        }
                    }
            }
            if (height > 128)
                return false;
            heights[id] = height;
            layout.constructionOwners.push_back(id);
            for (std::size_t index = 0; index + 1 < layout.constructionOwners.size(); ++index)
            {
                const auto& base = module.types[layout.constructionOwners[index]];
                if (base.nominalKind != ObjectNominal)
                    continue;
                const auto initializer =
                    base.fieldInitializer != bytecode::InvalidIndex ? base.fieldInitializer : base.defaultConstructor;
                if (initializer != bytecode::InvalidIndex)
                    layout.baseInitializers.push_back({initializer, index});
            }
            state[id] = 2;
            return true;
        };
        for (std::uint32_t id = 0; id < module.types.size(); ++id)
            if (nominal(id) && !visit(id, 0))
            {
                error = "VM object inheritance graph is invalid, cyclic or exceeds 128 levels";
                layouts_.clear();
                return false;
            }
        error.clear();
        return true;
    }

    const ObjectLayouts::Layout* ObjectLayouts::get(std::uint32_t type) const noexcept
    {
        return type < layouts_.size() && !layouts_[type].offsets.empty() ? &layouts_[type] : nullptr;
    }
} // namespace wio::vm::detail
