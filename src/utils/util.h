#pragma once

#include <string>
#include "../variables/variable_type.h"
#include "../variables/variable_base.h"

namespace wio
{
	namespace util
	{
		string_t float_t_to_string(float_t d);
		string_t type_to_string(variable_type vt);
		char get_escape_seq(char ch);
		string_t var_to_string(ref<variable_base> base);
	}
}