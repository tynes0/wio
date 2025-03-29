#pragma once

#include "variable.h"
#include <functional>

namespace wio
{
	class function_call;

	struct function_param;

	class var_function : public null_var
	{
	public:
		using type = std::function<ref<variable_base>(const std::vector<function_param>&, std::vector<ref<variable_base>>&)>;

		var_function(const type& data, variable_type return_type, const std::vector<function_param>& params, bool is_local = false, bool is_global = false);

		virtual variable_base_type get_base_type() const override;
		virtual variable_type get_type() const override;
		virtual ref<variable_base> clone() const override;

		ref<variable_base> call(std::vector<ref<variable_base>>& parameters);

		type& get_data();
		const type& get_data() const;
		variable_type get_return_type() const;
		size_t parameter_count() const;
		const std::vector<function_param>& get_parameters() const;

		void set_data(const type& data);
		void add_parameter(const function_param& param);
	private:
		type m_data;
		std::vector<function_param> m_params;
		variable_type m_return_type;
	};
}