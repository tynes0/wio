#include "variable.h"

#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../builtin/helpers.h"

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
        if (m_type == variable_type::vt_float_ref)
        {
            auto result = make_ref<variable>(*this);
            result->m_data = (*any_cast<float_ref_t>(result->m_data));
            result->m_type = variable_type::vt_float;
            return result;
        }
        else if (m_type == variable_type::vt_character_ref)
        {
            auto result = make_ref<variable>(*this);
            result->m_data = (*any_cast<character_ref_t>(result->m_data));
            result->m_type = variable_type::vt_character;
            return result;
        }
        else if (m_type == variable_type::vt_vec2)
        {
            return builtin::helper::make_variable(get_data_as<vec2>());
        }
        else if (m_type == variable_type::vt_vec3)
        {
            return builtin::helper::make_variable(get_data_as<vec3>());
        }
        else if (m_type == variable_type::vt_vec4)
        {
            return builtin::helper::make_variable(get_data_as<vec4>());
        }

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