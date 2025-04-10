#include "utility.h"

#include "builtin_base.h"
#include "helpers.h"

#include "../variables/function.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"

#include "../interpreter/evaluator_helper.h"
#include "../utils/util.h"

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
				return create_null_variable();
			}

			static ref<variable_base> b_to_string(ref<variable_base> base)
			{
                return make_ref<variable>(util::var_to_string(base), variable_type::vt_string);
            }

            static ref<variable_base> b_pair(ref<variable_base> first, ref<variable_base> second)
            {
				return helper::create_pair(first, second);
            }

			static ref<variable_base> b_vec2(ref<variable_base> x, ref<variable_base> y)
			{
				return helper::create_vec2(x, y);
			}

			static ref<variable_base> b_vec2_2(ref<variable_base> scalar)
			{
				return helper::create_vec2(scalar, scalar);
			}

			static ref<variable_base> b_vec2_3()
			{
				auto ref_0 = make_ref<variable>(any(0.0), variable_type::vt_float);
				return helper::create_vec2(ref_0, ref_0);;
			}

			static ref<variable_base> b_compare(ref<variable_base> lhs, ref<variable_base> rhs)
			{
				return wio::helper::eval_binary_exp_compare_all(lhs, rhs, location());
			}
		}

		void utility::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = builtin_scope;

			loader::load_function<2>(target_scope, "Swap",		detail::b_swap,			{ variable_type::vt_any, variable_type::vt_any }, std::bitset<2>("11"));
			loader::load_function<1>(target_scope, "ToString",	detail::b_to_string,	{ variable_type::vt_any });
			loader::load_function<2>(target_scope, "Pair",		detail::b_pair,			{ variable_type::vt_any, variable_type::vt_any });
			loader::load_function<2>(target_scope, "Compare",	detail::b_compare,		{ variable_type::vt_any, variable_type::vt_any });

			auto vec2_result = loader::load_function<2>(target_scope, "Vec2", detail::b_vec2, { variable_type::vt_any, variable_type::vt_any });
			loader::load_overload<1>(vec2_result, detail::b_vec2_2, { variable_type::vt_any });
			loader::load_overload<0>(vec2_result, detail::b_vec2_3, {});
		}
	}
}