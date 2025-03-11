#include "variable.h"

namespace wio
{
    variable::variable( const any& data, variable_type type, bool is_const, bool is_local, bool is_global) :
        m_data(data), m_type(type)
    {
        init_flags(is_const, is_local, is_global);
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

    void variable::set_type(variable_type type)
    {
        m_type = type;
    }
}