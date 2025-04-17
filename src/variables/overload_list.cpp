#include "overload_list.h"

#include "function_param.h"

namespace wio
{
	ref<var_function> overload_list::get(size_t idx)
	{
		return m_overloads[idx];
	}

	void overload_list::add(ref<var_function> overload)
	{
		m_overloads.push_back(overload);
	}

	ref<var_function> overload_list::find(const std::vector<function_param>& parameters)
	{
		for (auto& overload : m_overloads)
			if (overload->m_params == parameters)
				return overload;
		return nullptr;
	}

	size_t overload_list::count() const
	{
		return m_overloads.size();
	}
}

