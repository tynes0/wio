#pragma once

#include "variable.h"
#include <functional>
#include <vector>

namespace wio
{
	class function_call;

	struct function_param;

	class var_function : public variable_base
	{
	public:
		using fun_type = std::function<ref<variable_base>(const std::vector<function_param>&, std::vector<ref<variable_base>>&)>;

		var_function(const std::vector<function_param>& params, bool is_local = false, bool is_global = false);
		var_function(const fun_type& data, const std::vector<function_param>& params, bool is_local = false, bool is_global = false);

		virtual variable_base_type get_base_type() const override;
		virtual variable_type get_type() const override;
		virtual ref<variable_base> clone() const override;

		ref<variable_base> call(std::vector<ref<variable_base>>& parameters);

		fun_type& get_data();
		const fun_type& get_data() const;
		symbol* get_overload(size_t idx);
		const std::vector<function_param>& get_parameters() const;
		size_t parameter_count() const;
		bool declared() const;
		bool early_declared() const;
		bool compare_parameters(const std::vector<function_param>& params) const;
		const std::string& get_symbol_id() const;

		void set_data(const fun_type& data);
		void add_parameter(const function_param& param);
		void add_overload(symbol overload);
		void set_symbol_id(const std::string& id);
		void set_early_declared(bool decl);

		symbol* find_overload(const std::vector<function_param>& parameters);

		size_t overload_count() const;
	private:
		fun_type m_data;
		std::vector<function_param> m_params;
		std::vector<symbol> m_overloads;
		std::string m_symbol_id;
		bool m_declared;
		bool m_early_decl;
	};
}