#include "enum.h"

namespace wio
{
    var_enum::var_enum(packed_bool flags) : variable_base(flags)
    {
    }

    var_enum::var_enum(const std::vector<string_t>& name_list, packed_bool flags) : variable_base(flags), m_name_list(name_list)
    {
    }

    variable_base_type var_enum::get_base_type() const
    {
        return variable_base_type::enumeration;
    }

    variable_type var_enum::get_type() const
    {
        return variable_type::vt_enumeration;
    }

    ref<variable_base> var_enum::clone() const
    {
        return make_ref<var_enum>(*this);
    }

    integer_t var_enum::index_of(integer_t value) const
    {
        return index_of(to_string(value));
    }

    integer_t var_enum::index_of(const string_t& item) const
    {
        for (size_t i = 0; i < m_name_list.size(); ++i)
            if (m_name_list[i] == item)
                return static_cast<integer_t>(i);
        return integer_t(-1);
    }

    integer_t var_enum::value_of(const string_t& item) const
    {
        auto& symbols = m_members->get_symbols();
        auto it = symbols.find(item);
        if (it != symbols.end())
            return std::dynamic_pointer_cast<variable>(it->second.var_ref)->get_data_as<integer_t>();
        return integer_t(-1);
    }

    string_t var_enum::to_string(integer_t value) const
    {
        for (auto& item_v : m_members->get_symbols())
            if (std::dynamic_pointer_cast<variable>(item_v.second.var_ref)->get_data_as<integer_t>() == value)
                return item_v.first;
        return string_t();
    }

    string_t var_enum::to_string_n(integer_t index) const
    {
        if (index < (integer_t)m_name_list.size())
            return string_t();
        return m_name_list[index];
    }

    bool var_enum::contains(integer_t value) const
    {
        return index_of(value) != -1 ? true : false;
    }

    bool var_enum::contains(const string_t& item) const
    {
        return m_members->get_symbols().count(item) != 0;
    }

    integer_t var_enum::size() const
    {
        return m_members->get_symbols().size();
    }
}

