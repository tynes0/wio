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
				if (lhs->is_constant() || rhs->is_constant())
					throw constant_value_assignment_error("Constant values cannot be changed!");

				if (lhs->get_base_type() != rhs->get_base_type())
					throw type_mismatch_error("The base types of the values to be swapped must be the same!");

				if (lhs->get_base_type() == variable_base_type::variable)
				{
					ref<variable> left_v = std::dynamic_pointer_cast<variable>(lhs);
					ref<variable> right_v = std::dynamic_pointer_cast<variable>(rhs);

					any temp_v = left_v->get_data();
					variable_type temp_vt = left_v->get_type();

					left_v->set_data(right_v->get_data());
					left_v->set_type(right_v->get_type());

					right_v->set_data(temp_v);
					right_v->set_type(temp_vt);
				}
				else if (lhs->get_base_type() == variable_base_type::array)
				{
					ref<var_array> left_v = std::dynamic_pointer_cast<var_array>(lhs);
					ref<var_array> right_v = std::dynamic_pointer_cast<var_array>(rhs);

					std::vector<ref<variable_base>> temp = left_v->get_data();
					left_v->set_data(right_v->get_data());
					right_v->set_data(temp);
				}
				else if (lhs->get_base_type() == variable_base_type::dictionary)
				{
					ref<var_dictionary> left_v = std::dynamic_pointer_cast<var_dictionary>(lhs);
					ref<var_dictionary> right_v = std::dynamic_pointer_cast<var_dictionary>(rhs);

					var_dictionary::map_t temp = left_v->get_data();
					left_v->set_data(right_v->get_data());
					right_v->set_data(temp);
				}
				else if (lhs->get_base_type() == variable_base_type::function)
				{
					ref<var_function> left_v = std::dynamic_pointer_cast<var_function>(lhs);
					ref<var_function> right_v = std::dynamic_pointer_cast<var_function>(rhs);

					var_function::fun_type temp = left_v->get_data();
					left_v->set_data(right_v->get_data());
					right_v->set_data(temp);
				}
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

			static ref<variable_base> b_compare(ref<variable_base> lhs, ref<variable_base> rhs)
			{
				return wio::helper::eval_binary_exp_compare_all(lhs, rhs, location());
			}
		}

		void utility::load(ref<scope> target_scope)
		{
			if (!target_scope)
				target_scope = builtin_scope;

			loader::load_function_old<2>(target_scope, "Swap",		detail::b_swap,			{ variable_type::vt_any, variable_type::vt_any }, std::bitset<2>("11"));
			loader::load_function_old<1>(target_scope, "ToString",	detail::b_to_string,	{ variable_type::vt_any });
			loader::load_function_old<2>(target_scope, "Pair",		detail::b_pair,			{ variable_type::vt_any, variable_type::vt_any });
			loader::load_function_old<2>(target_scope, "Compare",	detail::b_compare,		{ variable_type::vt_any, variable_type::vt_any });
		}
	}
}