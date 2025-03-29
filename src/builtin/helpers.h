#pragma once

#include "builtin_base.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"

namespace wio
{
	namespace builtin
	{
		namespace helpers
		{
			std::string var_to_string(ref<variable_base> base);
			ref<variable_base> create_pair(ref<variable_base> first, ref<variable_base> second);
			ref<variable_base> string_as_array(ref<variable_base> base_str);
		}
	}
}