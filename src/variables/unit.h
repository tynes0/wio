#pragma once 

#include "../base/base.h"

#include "variable_base.h"
#include "overload_list.h"

#include "../utils/any.h"
#include "../utils/lucid.h"
#include "../interpreter/ast.h"

#include <string>

namespace wio
{
    constexpr inline uint32_t access_level_of(unit_access_type uat)
    {
        return frenum::value<unit_access_type>(uat);
    }

    namespace detail
    {
        struct unit_item
        {
            std::string id;
            ref<variable_base> data;
            unit_access_type access_type;
            bool is_outer;
        };

        using ud_id_index = lucid::unique_index<unit_item, std::string, &unit_item::id>;
        using ud_decl_type_index = lucid::non_unique_index<unit_item, unit_access_type, &unit_item::access_type>;
        using ud_outer_index = lucid::non_unique_index<unit_item, bool, &unit_item::is_outer>;
    }

    using unit_data = lucid::multi_index<detail::unit_item, detail::ud_id_index, detail::ud_decl_type_index, detail::ud_outer_index>;

    class unit : public variable_base
    {
    public:
        unit(packed_bool flags = {});
        unit(const std::string& identifier, const std::vector<ref<unit>>& parents, const std::vector<ref<unit>>& trust_list, bool is_final, packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;

        void set_identifier(const std::string& identifier);
        const std::string& get_identifier();

        void add_data(const std::string& id, const ref<variable_base>& value);
        unit_data& get_all();

        ref<overload_list> get_ctor();

        unit_access_type access_type_of(const std::string& id) const;

        const std::vector<ref<unit>>& get_parents() const;
        const std::vector<ref<unit>>& get_trust_list() const;
        bool is_final() const;

        bool operator==(const unit& right) const noexcept;
    private:
        unit_data m_data;

        std::vector<ref<unit>> m_parents;
        std::vector<ref<unit>> m_trust_list;
        std::string m_identifier;
        bool m_is_final;
    };

    class unit_instance : public variable_base
    {
    public:
        unit_instance(packed_bool flags = {});
        unit_instance(ref<unit> source, packed_bool flags = {});
        virtual variable_base_type get_base_type() const override;
        virtual variable_type get_type() const override;
        virtual ref<variable_base> clone() const override;

        void set_source(ref<unit> source);
        ref<unit> get_source();

        unit_access_type access_type_of(const std::string& id) const;
    private:
        ref<unit> m_source;
    };
} // namespace wio