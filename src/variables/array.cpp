#include "array.h"

namespace wio
{
    var_array::var_array(const std::vector<ref<variable_base>>& data, bool is_const, bool is_local, bool is_global) : m_data(data)
    {
        init_flags(is_const, is_local, is_global);
    }

    variable_base_type var_array::get_base_type() const
    {
        return variable_base_type::array;
    }

    variable_type var_array::get_type() const
    {
        return variable_type::vt_array;
    }

    ref<variable_base> var_array::clone() const
    {
        return make_ref<var_array>(*this);
    }

    std::vector<ref<variable_base>>& var_array::get_data()
    {
        return m_data;
    }

    const std::vector<ref<variable_base>>& var_array::get_data() const
    {
        return m_data;
    }

    ref<variable_base> var_array::get_element(long long index)
    {
        check_index(index);
        return m_data[index];
    }

    void var_array::set_data(const std::vector<ref<variable_base>>& data)
    {
        m_data.clear();
        for (const auto& item : data)
            m_data.push_back(item->clone());
    }

    void var_array::set_element(long long index, ref<variable_base> value)
    {
        check_index(index);
        m_data[index] = value->clone();
    }

    void var_array::check_index(long long index) const
    {
        if (index >= (long long)m_data.size() || index < 0)
            throw out_of_bounds_error("Array index out of the bounds!");
    }
}