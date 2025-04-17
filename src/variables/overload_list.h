#pragma once

#include "function.h"

#include <vector>

namespace wio
{
	struct function_param;

	class overload_list
	{
	public:
		ref<var_function> get(size_t idx);
		void add(ref<var_function> overload);
		ref<var_function> find(const std::vector<function_param>& parameters);

		size_t count() const;
	private:
		std::vector<ref<var_function>> m_overloads;
	};

}