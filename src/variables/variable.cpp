#include "variable.h"

namespace wio
{
    variable::variable(packed_bool flags) :
        m_data(), m_type(variable_type::vt_null), variable_base(flags)
    {
    }

    variable::variable(const any& data, variable_type type, packed_bool flags) :
        m_data(data), m_type(type), variable_base(flags)
    {
    }

    variable_base_type variable::get_base_type() const
    {
        return variable_base_type::variable;
    }

    variable_type variable::get_type() const
    {
        return m_type;
    }

    ref<variable_base> variable::clone() const
    {
        return make_ref<variable>(*this);
    }

    any& variable::get_data()
    {
        return m_data;
    }
    const any& variable::get_data() const
    {
        return m_data;
    }

    void variable::set_data(const any& new_data)
    {
        m_data = new_data;
    }

    void variable::assign_data(any& new_data)
    {
        m_data.swap(new_data);
    }

    void variable::set_type(variable_type type)
    {
        m_type = type;
    }
}