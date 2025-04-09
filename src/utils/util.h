#pragma once

#include <string>
#include "../variables/variable_type.h"

namespace wio
{
	namespace util
	{
		std::string double_to_string(double d);
		std::string type_to_string(variable_type vt);
		char get_escape_seq(char ch);
	}
}