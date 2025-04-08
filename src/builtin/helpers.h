#pragma once

#include "../variables/variable_base.h"

namespace wio
{
	namespace builtin
	{
		namespace helpers
		{
			std::string var_to_string(ref<variable_base> base);
			ref<variable_base> create_pair(ref<variable_base> first, ref<variable_base> second);
			ref<variable_base> create_vec2(ref<variable_base> xvb, ref<variable_base> yvb);
			ref<variable_base> string_as_array(ref<variable_base> base_str);
		}
	}
}