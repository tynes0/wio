#include "utility.h"

#include "builtin_base.h"
#include "helpers.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"

#include <sstream>

namespace wio
{
	namespace builtin
	{
        using namespace std::string_literals;

		namespace detail
		{
			static ref<variable_base> b_swap(ref<variable_base> lhs, ref<variable_base> rhs)
			{
				return ref<null_var>();
			}

			static ref<variable_base> b_to_string(ref<variable_base> base)
			{
                return make_ref<variable>(helpers::var_to_string(base), variable_type::vt_string);
            }

            static ref<variable_base> b_pair(ref<variable_base> first, ref<variable_base> second)
            {
				return helpers::create_pair(first, second);
            }
		}

		void utility::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = builtin_scope;

			loader::load_function<2>(target_scope, "Swap",		detail::b_swap,			{ variable_type::vt_any, variable_type::vt_any }, std::bitset<2>("11"));
			loader::load_function<1>(target_scope, "ToString",	detail::b_to_string,	{ variable_type::vt_any });
			loader::load_function<2>(target_scope, "Pair",		detail::b_pair,			{ variable_type::vt_any, variable_type::vt_any });
		}
	}
}