#pragma once

#include "function.h"
#include "function_param.h"
#include "../interpreter/symbol.h"

#include <vector>

namespace wio
{
	class overload_list : public variable_base
	{
	public:
		overload_list();

		virtual variable_base_type get_base_type() const override;
		virtual variable_type get_type() const override;
		virtual ref<variable_base> clone() const override;

		symbol* get(size_t idx);
		void add(symbol overload);
		symbol* find(const std::vector<function_param>& parameters);

		size_t count() const;

		void set_symbol_id(const std::string& id);
		const std::string& get_symbol_id();
	private:
		std::vector<symbol> m_overloads;
		std::string m_symbol_id;
	};

}