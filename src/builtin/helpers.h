#pragma once

#include "../variables/variable.h"
#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"

namespace wio
{
	namespace builtin
	{
		namespace helper
		{
			float_t var_as_float(ref<variable_base> var, bool nothrow = false);
			integer_t var_as_integer(ref<variable_base> var, bool nothrow = false);
			character_t var_as_char(ref<variable_base> var, bool nothrow = false);
			ref<variable> create_pair(ref<variable_base> first, ref<variable_base> second);
			ref<variable> create_vec2(ref<variable_base> xvb, ref<variable_base> yvb);
			ref<variable> create_vec3(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb);
			ref<variable> create_vec4(ref<variable_base> xvb, ref<variable_base> yvb, ref<variable_base> zvb, ref<variable_base> wvb);
			ref<variable> make_variable(const vec2& result_vec);
			ref<variable> make_variable(const vec3& result_vec);
			ref<variable> make_variable(const vec4& result_vec);
			ref<variable_base> string_as_array(ref<variable_base> base_str);
		}
	}
}