#include "overload_list.h"

#include "function_param.h"

namespace wio
{
	overload_list::overload_list() : variable_base(packed_bool{})
	{
	}

	variable_base_type overload_list::get_base_type() const
	{
		return variable_base_type::function;
	}

	variable_type overload_list::get_type() const
	{
		return variable_type::vt_function;
	}

	ref<variable_base> overload_list::clone() const
	{
		return make_ref<overload_list>(*this);
	}

	symbol* overload_list::get(size_t idx)
	{
		return &m_overloads[idx];
	}

	void overload_list::add(symbol overload)
	{
		m_overloads.push_back(overload);
	}

	symbol* overload_list::find(const std::vector<function_param>& parameters)
	{
		for (auto& overload : m_overloads)
			if (std::dynamic_pointer_cast<var_function>(overload.var_ref)->compare_parameters(parameters))
				return &overload;
		return nullptr;
	}

	size_t overload_list::count() const
	{
		return m_overloads.size();
	}

	void overload_list::set_symbol_id(const std::string& id)
	{
		m_symbol_id = id;
	}

	const std::string& overload_list::get_symbol_id()
	{
		return m_symbol_id;
	}
}

