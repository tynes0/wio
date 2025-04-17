#include "function.h"
#include "../base/exception.h"
#include "function_param.h"

namespace wio
{
    var_function::var_function(const std::vector<function_param>& params, bool is_local, bool is_global) 
        : m_params(params), variable_base({ is_local, is_global }), m_declared(false), m_early_decl(false), m_bounded_file_id(0)
    {
        symbol sym(make_ref<var_function>(*this), { is_local, is_global, true });
        m_overloads.push_back(sym);
    }
    var_function::var_function(const fun_type& data, const std::vector<function_param>& params, bool is_local, bool is_global)
        : m_data(data), m_params(params), variable_base({is_local, is_global}), m_declared(true), m_early_decl(false), m_bounded_file_id(0)
    {
        symbol sym(make_ref<var_function>(*this), { is_local, is_global, true });
        m_overloads.push_back(sym);
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

    const std::string& var_function::get_symbol_id() const
    {
        return m_symbol_id;
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

    void var_function::add_overload(symbol overload)
    {
        m_overloads.push_back(overload);
    }

    void var_function::set_symbol_id(const std::string& id)
    {
        m_symbol_id = id;
    }

    void var_function::set_early_declared(bool decl)
    {
        m_early_decl = decl;
    }

    void var_function::set_bounded_file_id(id_t id)
    {
        m_bounded_file_id = id;
    }

    symbol* var_function::get_overload(size_t idx)
    {
        return &m_overloads.at(idx);
    }
    symbol* var_function::find_overload(const std::vector<function_param>& parameters)
    {
        for (auto& overload : m_overloads)
            if (std::dynamic_pointer_cast<var_function>(overload.var_ref)->m_params == parameters)
                return &overload;
        return nullptr;
    }
    size_t var_function::overload_count() const
    {
        return m_overloads.size();
    }
}
