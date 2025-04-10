#pragma once

#include <string>
#include "../variables/variable_type.h"
#include "../variables/variable_base.h"

namespace wio
{
	namespace util
	{
		std::string double_to_string(double d);
		std::string type_to_string(variable_type vt);
		char get_escape_seq(char ch);
		std::string var_to_string(ref<variable_base> base);
	}
}