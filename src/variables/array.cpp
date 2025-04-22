#include "array.h"
#include "variable.h"
#include "../interpreter/evaluator_helper.h"

namespace wio
{
    var_array::var_array(packed_bool flags) : m_data(), variable_base(flags)
    {
        this->init_members();
    }

    var_array::var_array(const std::vector<ref<variable_base>>& data, packed_bool flags) : m_data(data), variable_base(flags)
    {
        this->init_members();
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

    const std::vector<ref<variable_base>>& var_array::get_data() const
    {
        return m_data;
    }

    ref<variable_base> var_array::get_element(long long index)
    {
        return m_data[normalize_index(index)];
    }

    size_t var_array::size() const
    {
        return m_data.size();
    }

    void var_array::push(ref<variable_base> data)
    {
        m_data.push_back(create_null_variable());
        if(data->get_type() != variable_type::vt_null)
            helper::container_element_assignment(m_data.back(), data->clone());
    }

    ref<variable_base> var_array::pop()
    {
        ref<variable_base> result = m_data.back();
        m_data.pop_back();
        return result;
    }

    void var_array::insert(long long idx, ref<variable_base> data)
    {
        m_data.insert(m_data.begin() + idx, create_null_variable());
        if (data->get_type() != variable_type::vt_null)
            helper::container_element_assignment(m_data[idx], data->clone());
    }

    ref<variable_base> var_array::erase(long long idx)
    {
        ref<variable_base> result = m_data[idx];
        m_data.erase(m_data.begin() + idx, m_data.begin() + idx + 1);
        return result;
    }

    void var_array::clear()
    {
        m_data.clear();
    }

    void var_array::set_data(const std::vector<ref<variable_base>>& data)
    {
        m_data.clear();
        for (const auto& item : data)
            m_data.push_back(item->clone());
    }

    void var_array::set_element(long long index, ref<variable_base> value)
    {
        helper::container_element_assignment(m_data[normalize_index(index)], value->clone());
    }

    long long var_array::normalize_index(long long index) const
    {
        if (index < 0)
            index = (long long)m_data.size() + index;

        if (index >= (long long)m_data.size() || index < 0)
            throw out_of_bounds_error("Array index out of the bounds!");

        return index;
    }
}