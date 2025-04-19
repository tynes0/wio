#pragma once

#include <string>
#include "variable_type.h"

namespace wio
{
	struct function_param
	{
		std::string id;
		variable_type type = variable_type::vt_null;
		bool is_ref = false;

		function_param() {}
		function_param(const std::string& id, variable_type type, bool is_ref) : id(id), type(type), is_ref(is_ref) {}

		bool operator==(const function_param& other) const
		{
			return (type == other.type) || type == variable_type::vt_any;
		}
	};

}