#pragma once

#include "wio/bytecode/module.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace wio::vm::detail
{
    // Runtime storage is one intrusive object allocation. WIR field indices
    // are owner-local, so inherited fields need an explicit owner -> offset map.
    class ObjectLayouts final
    {
    public:
        struct ConstructionStep
        {
            std::uint32_t function;
            std::size_t ownerIndex;
        };

        struct Layout
        {
            std::vector<std::uint32_t> fieldTypes;
            std::unordered_map<std::uint32_t, std::size_t> offsets;
            std::vector<std::uint32_t> constructionOwners;
            std::vector<ConstructionStep> baseInitializers;
        };

        [[nodiscard]] bool build(const bytecode::Module& module, std::string& error);
        [[nodiscard]] const Layout* get(std::uint32_t type) const noexcept;

    private:
        std::vector<Layout> layouts_;
    };
} // namespace wio::vm::detail
