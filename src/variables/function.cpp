#include "function.h"
#include "../base/exception.h"
#include "function_param.h"

namespace wio
{
    var_function::var_function(const type& data, variable_type return_type, const std::vector<function_param>& params, bool is_local, bool is_global)
        : m_data(data), m_return_type(return_type), m_params(params), null_var({is_local, is_global})
    {
    }

    variable_base_type var_function::get_base_type() const
    {
        return variable_base_type::function;
    }

    variable_type var_function::get_type() const
    {
        return variable_type::vt_function;
    }

    ref<variable_base> var_function::clone() const
    {
        return make_ref<var_function>(*this);
    }

    ref<variable_base> var_function::call(std::vector<ref<variable_base>>& parameters)
    {
        if (m_data)
            return m_data(m_params, parameters);
        else
            throw exception("Invalid function!");
    }

    var_function::type& var_function::get_data()
    {
        return m_data;
    }

    const var_function::type& var_function::get_data() const
    {
        return m_data;
    }

    variable_type var_function::get_return_type() const
    {
        return m_return_type;
    }

    size_t var_function::parameter_count() const
    {
        return m_params.size();
    }

    const std::vector<function_param>& var_function::get_parameters() const
    {
        return m_params;
    }

    void var_function::set_data(const type& data)
    {
        m_data = data;
    }
    void var_function::add_parameter(const function_param& param)
    {
        m_params.push_back(param);
    }

}
