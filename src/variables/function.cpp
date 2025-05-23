#include "function.h"
#include "../base/exception.h"
#include "function_param.h"

namespace wio
{
    var_function::var_function(const std::vector<function_param>& params) 
        : m_params(params), variable_base({ true }), m_declared(false), m_early_decl(false), m_bounded_file_id(0)
    {
    }
    var_function::var_function(const fun_type& data, const std::vector<function_param>& params)
        : m_data(data), m_params(params), variable_base({ true }), m_declared(true), m_early_decl(false), m_bounded_file_id(0)
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

    var_function::fun_type& var_function::get_data()
    {
        return m_data;
    }

    const var_function::fun_type& var_function::get_data() const
    {
        return m_data;
    }

    size_t var_function::parameter_count() const
    {
        return m_params.size();
    }

    const std::vector<function_param>& var_function::get_parameters() const
    {
        return m_params;
    }

    bool var_function::declared() const
    {
        return m_declared;
    }

    bool var_function::early_declared() const
    {
        return m_early_decl;
    }

    bool var_function::compare_parameters(const std::vector<function_param>& params) const
    {
        return std::equal(m_params.begin(), m_params.end(), params.begin(), params.end());
    }

    id_t var_function::get_file_id() const
    {
        return m_bounded_file_id;
    }

    void var_function::set_data(const fun_type& data)
    {
        m_declared = bool(data);
        m_data = data;
    }
    void var_function::add_parameter(const function_param& param)
    {
        m_params.push_back(param);
    }
    
    void var_function::set_early_declared(bool decl)
    {
        m_early_decl = decl;
    }

    void var_function::set_bounded_file_id(id_t id)
    {
        m_bounded_file_id = id;
    }
}
