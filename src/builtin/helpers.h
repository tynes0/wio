#pragma once

#include "../variables/variable_base.h"

namespace wio
{
	namespace builtin
	{
		namespace helper
		{
			double var_as_double(ref<variable_base> var, bool nothrow = false);
			char var_as_char(ref<variable_base> var, bool nothrow = false);
			ref<variable_base> create_pair(ref<variable_base> first, ref<variable_base> second);
			ref<variable_base> create_vec2(ref<variable_base> xvb, ref<variable_base> yvb);
			ref<variable_base> create_vec3(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb);
			ref<variable_base> create_vec4(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb, ref<variable_base> wvb);
			ref<variable_base> string_as_array(ref<variable_base> base_str);
		}
	}
}