#include "evaluator_helper.h"

#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/file_wrapper.h"
#include "../types/comparator.h"

#include "../builtin/helpers.h"

#include "../variables/array.h"
#include "../variables/function.h"
#include "../variables/dictionary.h"
#include "../variables/realm.h"

#include "../utils/filesystem.h"
#include "../utils/util.h"

namespace wio
{
    namespace helper
    {
        ref<variable_base> eval_binary_exp_addition(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) + any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) + *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(std::to_string(any_cast<long long>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) + *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<long long>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(util::double_to_string(any_cast<double>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) + *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) + any_cast<long long>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(util::double_to_string(*any_cast<double*>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                return make_ref<variable>(any(any_cast<std::string>(left_value) + util::var_to_string(rv_ref)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<char>(left_value) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else if(rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) + std::string(1, any_cast<char>(right_value))), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) + std::string(1, *any_cast<char*>(right_value))), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) + std::string(1, any_cast<char>(right_value))), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) + std::string(1, *any_cast<char*>(right_value))), variable_type::vt_string);

            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) + (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) + any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) + *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) + any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw type_mismatch_error("Invalid operand types for '+' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_subtraction(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) - any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) - *any_cast<double*>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) - *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) - *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) - any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) - (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) - any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) - *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) - any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) - (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) - any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) - *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) - any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw type_mismatch_error("Invalid operand types for '-' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_multiplication(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) * any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) * *any_cast<double*>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) * *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) * *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) * any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(std::string(any_cast<long long>(right_value), any_cast<char>(left_value))), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(std::string(any_cast<long long>(right_value), *any_cast<char*>(left_value))), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    long long ll_right = any_cast<long long>(right_value);
                    std::string str_left = any_cast<std::string>(left_value);

                    if (ll_right < 0)
                        throw invalid_operation_error("Invalid right value!", loc);

                    while (ll_right--)
                        str_left.append(str_left);

                    return make_ref<variable>(any(str_left), variable_type::vt_string);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) * (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) * any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) * *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) * any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) * (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) * any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) * *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) * any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }
            throw type_mismatch_error("Invalid operand types for '*' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_division(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long>(left_value) / any_cast<long long>(right_value)), variable_type::vt_integer);
                }
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long>(left_value) / *any_cast<double*>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double>(left_value) / *any_cast<double*>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<long long>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(left_value) / *any_cast<double*>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(left_value) / any_cast<long long>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) / (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) / any_cast<double>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) / *any_cast<double*>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                {
                    if (any_cast<vec2>(right_value).is_one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) / any_cast<vec2>(right_value)), variable_type::vt_vec2);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3>(left_value) / (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3>(left_value) / any_cast<double>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3>(left_value) / *any_cast<double*>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                {
                    if (any_cast<vec3>(right_value).is_one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3>(left_value) / any_cast<vec3>(right_value)), variable_type::vt_vec3);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }
            throw type_mismatch_error("Invalid operand types for '/' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_modulo(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long>(left_value) % any_cast<long long>(right_value)), variable_type::vt_integer);
                }
            }
            throw type_mismatch_error("Invalid operand types for '%' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_assignment(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            if (lv_ref->get_base_type() == variable_base_type::variable)
            {
                if (rv_ref->get_base_type() != variable_base_type::variable)
                    throw type_mismatch_error("Target type is 'var' but source type is not!", loc);

                ref<variable> variable_lhs = std::dynamic_pointer_cast<variable>(lv_ref);
                ref<variable> variable_rhs = std::dynamic_pointer_cast<variable>(rv_ref);

                if (variable_lhs->get_type() == variable_type::vt_character_ref)
                {
                    if (variable_rhs->get_type() == variable_type::vt_character)
                        *variable_lhs->get_data_as<char*>() = variable_rhs->get_data_as<char>();
                    else if (variable_rhs->get_type() == variable_type::vt_character_ref)
                        *variable_lhs->get_data_as<char*>() = *variable_rhs->get_data_as<char*>();
                    else
                        throw type_mismatch_error("Target type is 'character' but source type is not!", loc);
                }
                else if (variable_lhs->get_type() == variable_type::vt_float_ref)
                {
                    if (variable_rhs->get_type() == variable_type::vt_float)
                        *variable_lhs->get_data_as<double*>() = variable_rhs->get_data_as<double>();
                    else if (variable_rhs->get_type() == variable_type::vt_integer)
                        *variable_lhs->get_data_as<double*>() = (double)variable_rhs->get_data_as<long long>();
                    else if (variable_rhs->get_type() == variable_type::vt_float_ref)
                        *variable_lhs->get_data_as<double*>() = *variable_rhs->get_data_as<double*>();
                    else
                        throw type_mismatch_error("Target type is 'float' but source type is not!", loc);
                }
                else
                {
                    if (variable_rhs->get_type() == variable_type::vt_vec2)
                    {
                        auto members = variable_rhs->get_members();
                        auto& symbols = members->get_symbols();
                        auto result_vec = builtin::helper::create_vec2(symbols["X"].var_ref, symbols["Y"].var_ref);
                        variable_lhs->assign_data(result_vec->get_data());
                        variable_lhs->set_type(variable_type::vt_vec2);
                        variable_lhs->load_members(result_vec->get_members());
                    }
                    else if (variable_rhs->get_type() == variable_type::vt_vec3)
                    {
                        auto members = variable_rhs->get_members();
                        auto& symbols = members->get_symbols();
                        auto result_vec = builtin::helper::create_vec3(symbols["X"].var_ref, symbols["Y"].var_ref, symbols["Z"].var_ref);
                        variable_lhs->assign_data(result_vec->get_data());
                        variable_lhs->set_type(variable_type::vt_vec3);
                        variable_lhs->load_members(result_vec->get_members());
                    }
                    else if (variable_rhs->get_type() == variable_type::vt_vec4)
                    {
                        auto members = variable_rhs->get_members();
                        auto& symbols = members->get_symbols();
                        auto result_vec = builtin::helper::create_vec4(symbols["X"].var_ref, symbols["Y"].var_ref, symbols["Z"].var_ref, symbols["W"].var_ref);
                        variable_lhs->assign_data(result_vec->get_data());
                        variable_lhs->set_type(variable_type::vt_vec4);
                        variable_lhs->load_members(result_vec->get_members());
                    }
                    else
                    {
                        if (auto members = variable_rhs->get_members())
                        {
                            auto& symbols = members->get_symbols();
                            variable_lhs->init_members();
                            auto& lhs_symbols = variable_lhs->get_members()->get_symbols();

                            for (auto& item : symbols)
                                lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                        }

                        if (variable_rhs->get_type() == variable_type::vt_float_ref)
                        {
                            variable_lhs->set_data(any(*variable_rhs->get_data_as<double*>()));
                            variable_lhs->set_type(variable_type::vt_float);
                        }
                        else if (variable_rhs->get_type() == variable_type::vt_character_ref)
                        {
                            variable_lhs->set_data(any(*variable_rhs->get_data_as<char*>()));
                            variable_lhs->set_type(variable_type::vt_character);
                        }
                        else
                        {
                            variable_lhs->set_data(variable_rhs->get_data());
                            variable_lhs->set_type(variable_rhs->get_type());
                        }
                    }
                }
                return variable_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::array)
            {
                if (rv_ref->get_base_type() != variable_base_type::array && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'array' but source type is not!", loc);
                ref<var_array> array_lhs = std::dynamic_pointer_cast<var_array>(lv_ref);
                ref<var_array> array_rhs = std::dynamic_pointer_cast<var_array>(rv_ref);

                if (array_rhs)
                {
                    if (auto members = array_rhs->get_members())
                    {
                        auto& symbols = members->get_symbols();
                        array_lhs->init_members();
                        auto& lhs_symbols = array_lhs->get_members()->get_symbols();

                        for (auto& item : symbols)
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                    array_lhs->set_data(array_rhs->get_data());
                }
                else
                {
                    array_lhs->set_data({});
                }

                return array_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::dictionary)
            {
                if (rv_ref->get_base_type() != variable_base_type::dictionary && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'dict' but source type is not!", loc);
                ref<var_dictionary> dict_lhs = std::dynamic_pointer_cast<var_dictionary>(lv_ref);
                ref<var_dictionary> dict_rhs = std::dynamic_pointer_cast<var_dictionary>(rv_ref);

                if (dict_rhs)
                {
                    if (auto members = dict_rhs->get_members())
                    {
                        auto& symbols = members->get_symbols();
                        dict_lhs->init_members();
                        auto& lhs_symbols = dict_lhs->get_members()->get_symbols();

                        for (auto& item : symbols)
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                }
                else
                {
                    dict_lhs->set_data({});
                }

                return dict_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::function)
            {
                if (rv_ref->get_base_type() != variable_base_type::function && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'func' but source type is not!", loc);
                ref<var_function> func_lhs = std::dynamic_pointer_cast<var_function>(lv_ref);
                ref<var_function> func_rhs = std::dynamic_pointer_cast<var_function>(rv_ref);

                if (auto members = func_rhs->get_members())
                {
                    auto& symbols = members->get_symbols();
                    func_lhs->init_members();
                    auto& lhs_symbols = func_lhs->get_members()->get_symbols();

                    for (auto& item : symbols)
                        lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                }
                return func_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::realm)
            {
                if (rv_ref->get_base_type() != variable_base_type::realm && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'realm' but source type is not!", loc);
                ref<realm> realm_lhs = std::dynamic_pointer_cast<realm>(lv_ref);
                ref<realm> realm_rhs = std::dynamic_pointer_cast<realm>(rv_ref);

                if (realm_rhs)
                {
                    auto members = realm_rhs->get_members();

                    auto& symbols = members->get_symbols();
                    realm_lhs->init_members();
                    auto& lhs_symbols = realm_lhs->get_members()->get_symbols();

                    for (auto& item : symbols)
                    {
                        if (item.second.var_ref->get_type() == variable_type::vt_float_ref)
                            lhs_symbols[item.first] = symbol(make_ref<variable>(any(*std::dynamic_pointer_cast<variable>(item.second.var_ref)->get_data_as<double*>()), variable_type::vt_float), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                        else if (item.second.var_ref->get_type() == variable_type::vt_character_ref)
                            lhs_symbols[item.first] = symbol(make_ref<variable>(any(*std::dynamic_pointer_cast<variable>(item.second.var_ref)->get_data_as<char*>()), variable_type::vt_character), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                        else
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                }
                else
                {
                    realm_lhs->init_members();
                }



                return realm_lhs->clone();
            }

            throw invalid_operator_error("Invalid assignment error!", loc);
        }

        ref<variable_base> eval_binary_exp_add_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_array)
            {
                std::dynamic_pointer_cast<var_array>(lv_ref)->push(rv_ref);
                return lv_ref->clone();
            }
            else if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += (long long)any_cast<double>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += (long long)*any_cast<double*>(right_value)), variable_type::vt_integer);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                return make_ref<variable>(any(any_cast<std::string&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += util::var_to_string(rv_ref)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) += any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw type_mismatch_error("Invalid operand types for '+=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_subtract_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= (long long)any_cast<double>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= (long long)*any_cast<double*>(right_value)), variable_type::vt_integer);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_array)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    ref<var_array> left_array = std::dynamic_pointer_cast<var_array>(lv_ref);
                    long long ll_right = any_cast<long long>(right_value);

                    if ((long long)left_array->size() < ll_right || ll_right < 0)
                        throw invalid_operation_error("Invalid right value!", loc);

                    while (ll_right--)
                        left_array->pop();
                    return left_array->clone();
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) -= any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw type_mismatch_error("Invalid operand types for '-=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_multiply_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float) 
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= (long long)any_cast<double>(right_value)), variable_type::vt_integer);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= (long long)*any_cast<double*>(right_value)), variable_type::vt_integer);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= *any_cast<double*>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    long long ll_right = any_cast<long long>(right_value);
                    std::string str_left = any_cast<std::string&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data());

                    if (ll_right < 0)
                        throw invalid_operation_error("Invalid right value!", loc);

                    while (ll_right--)
                        str_left.append(str_left);

                    return make_ref<variable>(any(str_left), variable_type::vt_string);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<double>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= *any_cast<double*>(right_value)), variable_type::vt_vec2);
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<vec2>(right_value)), variable_type::vt_vec2);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<double>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= *any_cast<double*>(right_value)), variable_type::vt_vec3);
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) *= any_cast<vec3>(right_value)), variable_type::vt_vec3);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }
            throw type_mismatch_error("Invalid operand types for '*=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_divide_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<long long>(right_value)), variable_type::vt_integer);
                }
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= (long long)any_cast<double>(right_value)), variable_type::vt_integer);
                }
                if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= (long long)*any_cast<double*>(right_value)), variable_type::vt_integer);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= *any_cast<double*>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<long long>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= *any_cast<double*>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(*any_cast<double*>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<long long>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<double>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= *any_cast<double*>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                {
                    if (any_cast<vec2>(right_value).is_one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<vec2>(right_value)), variable_type::vt_vec2);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= (double)any_cast<long long>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<double>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                {
                    if (*any_cast<double*>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= *any_cast<double*>(right_value)), variable_type::vt_vec3);
                }
                else if (rv_ref->get_type() == variable_type::vt_vec3)
                {
                    if (any_cast<vec3>(right_value).is_one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec3&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) /= any_cast<vec3>(right_value)), variable_type::vt_vec3);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }
            throw type_mismatch_error("Invalid operand types for '/=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_modulo_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) %= any_cast<long long>(right_value)), variable_type::vt_integer);
                }
            }
            throw type_mismatch_error("Invalid operand types for '%=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_less_than(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < *any_cast<double*>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) < any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            throw type_mismatch_error("Invalid operand types for '<' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_greater_than(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > *any_cast<double*>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) > any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            throw type_mismatch_error("Invalid operand types for '>' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_less_than_or_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= *any_cast<double*>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) <= any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            throw type_mismatch_error("Invalid operand types for '<=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_greater_than_or_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= *any_cast<double*>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) >= any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            throw type_mismatch_error("Invalid operand types for '>=' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<double>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<double*>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<char>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character_ref)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(*any_cast<char*>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == *any_cast<char*>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == *any_cast<double*>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) == any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_file)
            {
                if (rv_ref->get_type() == variable_type::vt_file)
                    return make_ref<variable>(any(any_cast<file_wrapper>(left_value) == any_cast<file_wrapper>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_array)
            {
                if (rv_ref->get_type() == variable_type::vt_array)
                {
                    ref<var_array> left_arr = std::dynamic_pointer_cast<var_array>(lv_ref);
                    ref<var_array> right_arr = std::dynamic_pointer_cast<var_array>(rv_ref);

                    if (left_arr->size() != right_arr->size())
                        return make_ref<variable>(any(false), variable_type::vt_bool);

                    for (size_t i = 0; i < left_arr->size(); ++i)
                    {
                        ref<variable_base> item_result_base = eval_binary_exp_equal_to(left_arr->get_element(i), right_arr->get_element(i), loc);
                        ref<variable> item_result = std::dynamic_pointer_cast<variable>(item_result_base);

                        if (!item_result->get_data_as<bool>())
                            return make_ref<variable>(any(false), variable_type::vt_bool);
                    }

                    return make_ref<variable>(any(true), variable_type::vt_bool);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_dictionary)
            {
                if (rv_ref->get_type() == variable_type::vt_dictionary)
                {
                    ref<var_dictionary> left_dict = std::dynamic_pointer_cast<var_dictionary>(lv_ref);
                    ref<var_dictionary> right_dict = std::dynamic_pointer_cast<var_dictionary>(rv_ref);

                    if (left_dict->size() != right_dict->size())
                        return make_ref<variable>(any(false), variable_type::vt_bool);

                    auto& left_data = left_dict->get_data();
                    auto& right_data = right_dict->get_data();

                    for (auto& [left_key, left_value] : left_data)
                    {
                        if (right_data.find(left_key) == right_data.end())
                            return make_ref<variable>(any(false), variable_type::vt_bool);

                        ref<variable_base> item_result_base = eval_binary_exp_equal_to(left_value, right_data[left_key], loc);
                        ref<variable> item_result = std::dynamic_pointer_cast<variable>(item_result_base);

                        if (!item_result->get_data_as<bool>())
                            return make_ref<variable>(any(false), variable_type::vt_bool);
                    }

                    return make_ref<variable>(any(true), variable_type::vt_bool);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_pair)
            {
                if (rv_ref->get_type() == variable_type::vt_pair)
                {
                    pair_t left_pair = any_cast<pair_t>(left_value);
                    pair_t right_pair = any_cast<pair_t>(right_value);

                    ref<variable_base> first_result = eval_binary_exp_equal_to(left_pair.first, right_pair.first, loc);

                    if (std::dynamic_pointer_cast<variable>(first_result)->get_data_as<bool>())
                        return eval_binary_exp_equal_to(left_pair.second, right_pair.second, loc);

                    return make_ref<variable>(any(false), variable_type::vt_bool);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) == any_cast<vec2>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                if (rv_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(any_cast<vec3>(left_value) == any_cast<vec3>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }
            else if (lv_ref->get_type() == variable_type::vt_null)
            {
                return make_ref<variable>(any(rv_ref->get_type() == variable_type::vt_null), variable_type::vt_bool);
            }
            throw type_mismatch_error("Invalid operand types for '==' operator.", loc);
        }

        ref<variable_base> eval_binary_exp_not_equal_to(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return make_ref<variable>(any(!std::dynamic_pointer_cast<variable>(eval_binary_exp_equal_to(lv_ref, rv_ref, loc))->get_data_as<bool>()), variable_type::vt_bool);
        }

        ref<variable_base> eval_binary_exp_type_equal(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return make_ref<variable>(any(lv_ref->get_type() == rv_ref->get_type()), variable_type::vt_bool);
        }

        ref<variable_base> eval_binary_exp_compare_all(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            comparator comp;

            try
            {
                comp.equal = eval_binary_exp_equal_to(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.equal = create_null_variable();
            }
            comp.equal->set_const(true);

            try
            {
                comp.not_equal = eval_binary_exp_not_equal_to(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.not_equal = create_null_variable();
            }
            comp.not_equal ->set_const(true);

            try
            {
                comp.less = eval_binary_exp_less_than(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.less = create_null_variable();
            }
            comp.less->set_const(true);

            try
            {
                comp.greater = eval_binary_exp_greater_than(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.greater = create_null_variable();
            }
            comp.greater->set_const(true);

            try
            {
                comp.less_or_equal = eval_binary_exp_less_than_or_equal_to(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.less_or_equal = create_null_variable();
            }
            comp.less_or_equal->set_const(true);

            try
            {
                comp.greater_or_equal = eval_binary_exp_greater_than_or_equal_to(lv_ref, rv_ref, loc);
            }
            catch (const type_mismatch_error&)
            {
                comp.greater_or_equal = create_null_variable();
            }
            comp.greater_or_equal->set_const(true);

            comp.type_equal = eval_binary_exp_type_equal(lv_ref, rv_ref, loc);
            comp.type_equal->set_const(true);

            auto result = make_ref<variable>(any(comp), variable_type::vt_comparator);

            result->init_members();
            auto members = result->get_members();

            members->insert("Less", symbol(comp.less));
            members->insert("Greater", symbol(comp.greater));
            members->insert("LessOrEqual", symbol(comp.less_or_equal));
            members->insert("GreaterOrEqual", symbol(comp.greater_or_equal));
            members->insert("Equal", symbol(comp.equal));
            members->insert("NotEqual", symbol(comp.not_equal));
            members->insert("TypeEqual", symbol(comp.type_equal));

            return result;
        }

        ref<variable_base> eval_binary_exp_logical_and(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if(rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<bool>(left_value) && any_cast<bool>(right_value)), variable_type::vt_bool);
            }

            throw type_mismatch_error("Invalid operand types for '&&' operator (logical AND). Operands must be booleans.", loc);
        }

        ref<variable_base> eval_binary_exp_logical_or(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<bool>(left_value) || any_cast<bool>(right_value)), variable_type::vt_bool);
            }

            throw type_mismatch_error("Invalid operand types for '||' operator (logical OR). Operands must be booleans.", loc);
        }

        ref<variable_base> eval_binary_exp_logical_xor(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<bool>(left_value) != any_cast<bool>(right_value)), variable_type::vt_bool);
            }

            throw type_mismatch_error("Invalid operand types for '^^' operator (logical XOR). Operands must be booleans.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_and(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if(rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) & any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '&' operator (bitwise AND). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_or(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) | any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '|' operator (bitwise OR). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_xor(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) ^ any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '^' operator (bitwise XOR). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_and_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) &= any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '&=' operator (bitwise AND assign). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_or_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) |= any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '|=' operator (bitwise OR assign). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_bitwise_xor_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) ^= any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '^=' operator (bitwise XOR assign). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_left_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) << any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '<<' operator (left shift). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_right_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;
            any right_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >> any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '>>' operator (right shift). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_left_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) <<= any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '<<=' operator (left shift assign). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_right_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            any right_value;

            if (rv_ref->get_base_type() == variable_base_type::variable)
                right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_integer)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<long long&>(std::dynamic_pointer_cast<variable>(lv_ref)->get_data()) >>= any_cast<long long>(right_value)), variable_type::vt_integer);
            }

            throw type_mismatch_error("Invalid operand types for '>>=' operator (right shift assign). Operands must be integers.", loc);
        }

        ref<variable_base> eval_binary_exp_to_left(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_file)
            {
                filesystem::write_file(any_cast<file_wrapper>(left_value).get_file(), util::var_to_string(rv_ref));
                return lv_ref->clone();
            }

            throw type_mismatch_error("Invalid operand types for '<-' operator!", loc);
        }

        ref<variable_base> eval_binary_exp_to_right(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_file)
            {
                if (rv_ref->get_base_type() == variable_base_type::variable)
                {
                    auto right = std::dynamic_pointer_cast<variable>(rv_ref);

                    file_wrapper wrapped_file = any_cast<file_wrapper>(left_value);
                    raw_buffer buf;

                    if (wrapped_file != wrapped_stdin)
                        buf = filesystem::read_file(wrapped_file.get_file());
                    else
                        buf = filesystem::read_stdout();

                    right->set_data(std::string(buf.as<char>(), buf.size));
                    right->set_type(variable_type::vt_string);

                    return lv_ref->clone();
                }
            }

            throw type_mismatch_error("Invalid operand types for '->' operator!", loc);
        }

        ref<variable_base> eval_unary_exp_positive(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            if (op_type == unary_operator_type::prefix)
            {
                if (v_ref->get_type() == variable_type::vt_integer ||
                    v_ref->get_type() == variable_type::vt_float ||
                    v_ref->get_type() == variable_type::vt_vec2 ||
                    v_ref->get_type() == variable_type::vt_vec3 ||
                    v_ref->get_type() == variable_type::vt_vec4 ||
                    v_ref->get_type() == variable_type::vt_character ||
                    v_ref->get_type() == variable_type::vt_bool)
                    return std::dynamic_pointer_cast<variable>(v_ref)->clone();
                else if (v_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(*std::dynamic_pointer_cast<variable>(v_ref)->get_data_as<char*>(), variable_type::vt_character);
                else if (v_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(*std::dynamic_pointer_cast<variable>(v_ref)->get_data_as<double*>(), variable_type::vt_float);

                throw invalid_type_error("Unary '+' operator requires a numeric operand or vec!", loc);
            }

            throw invalid_operation_error("Unary '+' operator should be prefix!", loc);
        }

        ref<variable_base> eval_unary_exp_negative(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            any value;

            if (v_ref->get_base_type() == variable_base_type::variable)
                value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();

            if (op_type == unary_operator_type::prefix)
            {
                if (v_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(-any_cast<long long>(value)), variable_type::vt_integer);
                else if (v_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(-any_cast<double>(value)), variable_type::vt_float);
                else if (v_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(-(*any_cast<double*>(value))), variable_type::vt_float);
                else if (v_ref->get_type() == variable_type::vt_vec2)
                    return make_ref<variable>(any(-any_cast<vec2>(value)), variable_type::vt_vec2);
                else if (v_ref->get_type() == variable_type::vt_vec3)
                    return make_ref<variable>(any(-any_cast<vec3>(value)), variable_type::vt_vec3);
                //else if (v_ref->get_type() == variable_type::vt_vec4)
                //    return make_ref<variable>(any(-any_cast<vec4>(value)), variable_type::vt_vec4);
                else if (v_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(-any_cast<char>(value)), variable_type::vt_character);
                else if (v_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(-(*any_cast<char*>(value))), variable_type::vt_character);
                else if (v_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<bool>(value) ? false : true), variable_type::vt_bool);

                throw invalid_type_error("Unary '-' operator requires a numeric operand.", loc);
            }

            throw invalid_operation_error("Unary '-' operator should be prefix!", loc);
        }

        ref<variable_base> eval_unary_exp_increment(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            if (v_ref->get_type() == variable_type::vt_integer)
            {
                long long& ll_value = any_cast<long long&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                long long old_value = ll_value;

                ll_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? ll_value : old_value), variable_type::vt_integer);

            }
            else if (v_ref->get_type() == variable_type::vt_float)
            {
                double& d_value = any_cast<double&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                double old_value = d_value;

                d_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_float_ref)
            {
                double& d_value = *any_cast<double*>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                double old_value = d_value;

                d_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_character)
            {
                char& c_value = any_cast<char&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                char old_value = c_value;

                c_value++;
                    
                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_character_ref)
            {
                char& c_value = *any_cast<char*>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                char old_value = c_value;

                c_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_vec2)
            {
                vec2& v2_value = any_cast<vec2&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                vec2 old_value = v2_value;

                v2_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v2_value : old_value), variable_type::vt_vec2);
            }
            else if (v_ref->get_type() == variable_type::vt_vec3)
            {
                vec3& v3_value = any_cast<vec3&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                vec3 old_value = v3_value;

                v3_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v3_value : old_value), variable_type::vt_vec3);
            }
            else if (v_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw invalid_type_error("Unary '++' operators require a numeric or character or vec operand.", loc);
        }

        ref<variable_base> eval_unary_exp_decrement(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            if (v_ref->get_type() == variable_type::vt_integer)
            {
                long long& ll_value = any_cast<long long&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                long long old_value = ll_value;

                ll_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? ll_value : old_value), variable_type::vt_integer);

            }
            else if (v_ref->get_type() == variable_type::vt_float)
            {
                double& d_value = any_cast<double&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                double old_value = d_value;

                d_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_float_ref)
            {
                double& d_value = *any_cast<double*>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                double old_value = d_value;

                d_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_character)
            {
                char& c_value = any_cast<char&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                char old_value = c_value;

                c_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_character_ref)
            {
                char& c_value = *any_cast<char*>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                char old_value = c_value;

                c_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_vec2)
            {
                vec2& v2_value = any_cast<vec2&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                vec2 old_value = v2_value;

                v2_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v2_value : old_value), variable_type::vt_vec2);
            }
            else if (v_ref->get_type() == variable_type::vt_vec3)
            {
                vec3& v3_value = any_cast<vec3&>(std::dynamic_pointer_cast<variable>(v_ref)->get_data());
                vec3 old_value = v3_value;

                v3_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v3_value : old_value), variable_type::vt_vec3); return nullptr;
            }
            else if (v_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw invalid_type_error("Unary '--' operators require a numeric or character or vec operand.", loc);
        }

        ref<variable_base> eval_unary_exp_bitwise_not(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            any value;

            if (v_ref->get_base_type() == variable_base_type::variable)
                value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();

            if (op_type == unary_operator_type::prefix)
            {
                if (v_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(~any_cast<long long>(value)), variable_type::vt_integer);

                throw invalid_type_error("Unary '~' operator requires an integer operand.", loc);
            }

            throw invalid_operation_error("Unary '~' operator should be prefix!", loc);
        }

        ref<variable_base> eval_unary_exp_logical_not(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            any value;

            if (v_ref->get_base_type() == variable_base_type::variable)
                value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();

            if (op_type == unary_operator_type::prefix)
            {
                if (v_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(!any_cast<bool>(value)), variable_type::vt_bool);

                throw invalid_type_error("Unary '!' operator requires a boolean operand.", loc);
            }

            throw invalid_operation_error("Unary '!' operator should be prefix!", loc);
        }

        ref<variable_base> eval_unary_exp_is_not_null(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            if (v_ref->get_type() == variable_type::vt_null)
                return make_ref<variable>(any(false), variable_type::vt_bool);

            if (v_ref->get_type() == variable_type::vt_array)
            {
                ref<var_array> array = std::dynamic_pointer_cast<var_array>(v_ref);
                return make_ref<variable>(any((array->size() != 0)), variable_type::vt_bool);
            }
            else if (v_ref->get_type() == variable_type::vt_dictionary)
            {
                ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(v_ref);
                return make_ref<variable>(any((dict->size() != 0)), variable_type::vt_bool);
            }
            else if (v_ref->get_type() == variable_type::vt_function)
            {
                ref<var_function> func = std::dynamic_pointer_cast<var_function>(v_ref);
                return make_ref<variable>(any(((bool)func->get_data())), variable_type::vt_bool);
            }
            else
            {
                any value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();
                return make_ref<variable>(any(!value.empty()), variable_type::vt_bool);
            }
        }

        ref<variable_base> container_element_assignment(ref<variable_base>& cont_item_ref, ref<variable_base> rv_ref)
        {
            if (cont_item_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!");

            if (rv_ref->get_base_type() == variable_base_type::variable)
            {
                ref<variable> variable_rhs = std::dynamic_pointer_cast<variable>(rv_ref);
                ref<variable> result = make_ref<variable>();
                
                if (variable_rhs->get_type() == variable_type::vt_vec2)
                {
                    auto members = variable_rhs->get_members();
                    auto& symbols = members->get_symbols();
                    auto result_vec = builtin::helper::create_vec2(symbols["X"].var_ref, symbols["Y"].var_ref);
                    result->assign_data(result_vec->get_data());
                    result->set_type(variable_type::vt_vec2);
                    result->load_members(result_vec->get_members());
                }
                else if (variable_rhs->get_type() == variable_type::vt_vec3)
                {
                    auto members = variable_rhs->get_members();
                    auto& symbols = members->get_symbols();
                    auto result_vec = builtin::helper::create_vec3(symbols["X"].var_ref, symbols["Y"].var_ref, symbols["Z"].var_ref);
                    result->assign_data(result_vec->get_data());
                    result->set_type(variable_type::vt_vec3);
                    result->load_members(result_vec->get_members());
                }
                else if (variable_rhs->get_type() == variable_type::vt_vec4)
                {
                    auto members = variable_rhs->get_members();
                    auto& symbols = members->get_symbols();
                    auto result_vec = builtin::helper::create_vec4(symbols["X"].var_ref, symbols["Y"].var_ref, symbols["Z"].var_ref, symbols["W"].var_ref);
                    result->assign_data(result_vec->get_data());
                    result->set_type(variable_type::vt_vec4);
                    result->load_members(result_vec->get_members());
                }
                else
                {
                    if (auto members = variable_rhs->get_members())
                    {
                        auto& symbols = members->get_symbols();
                        result->init_members();
                        auto& lhs_symbols = result->get_members()->get_symbols();

                        for (auto& item : symbols)
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }

                    if (variable_rhs->get_type() == variable_type::vt_float_ref)
                    {
                        result->set_data(any(*variable_rhs->get_data_as<double*>()));
                        result->set_type(variable_type::vt_float);
                    }
                    else if (variable_rhs->get_type() == variable_type::vt_character_ref)
                    {
                        result->set_data(any(*variable_rhs->get_data_as<char*>()));
                        result->set_type(variable_type::vt_character);
                    }
                    else
                    {
                        result->set_data(variable_rhs->get_data());
                        result->set_type(variable_rhs->get_type());
                    }
                }

                cont_item_ref = result;
                return cont_item_ref->clone();
            }
            else if (rv_ref->get_base_type() == variable_base_type::array)
            {
                ref<var_array> array_rhs = std::dynamic_pointer_cast<var_array>(rv_ref);
                ref<var_array> result = make_ref<var_array>();

                if (array_rhs)
                {
                    if (auto members = array_rhs->get_members())
                    {
                        auto& symbols = members->get_symbols();
                        result->init_members();
                        auto& lhs_symbols = result->get_members()->get_symbols();

                        for (auto& item : symbols)
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                    result->set_data(array_rhs->get_data());
                }
                else
                {
                    result->set_data({});
                }

                cont_item_ref = result;
                return cont_item_ref->clone();
            }
            else if (rv_ref->get_base_type() == variable_base_type::dictionary)
            {
                ref<var_dictionary> dict_rhs = std::dynamic_pointer_cast<var_dictionary>(rv_ref);
                ref<var_dictionary> result = make_ref<var_dictionary>();

                if (dict_rhs)
                {
                    if (auto members = dict_rhs->get_members())
                    {
                        auto& symbols = members->get_symbols();
                        result->init_members();
                        auto& lhs_symbols = result->get_members()->get_symbols();

                        for (auto& item : symbols)
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                }
                else
                {
                    result->set_data({});
                }

                cont_item_ref = result;
                return cont_item_ref->clone();
            }
            else if (rv_ref->get_base_type() == variable_base_type::function)
            {
                ref<var_function> func_rhs = std::dynamic_pointer_cast<var_function>(rv_ref);
                ref<var_function> result = make_ref<var_function>(func_rhs->get_parameters());

                if (auto members = func_rhs->get_members())
                {
                    auto& symbols = members->get_symbols();
                    result->init_members();
                    auto& lhs_symbols = result->get_members()->get_symbols();

                    for (auto& item : symbols)
                        lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                }

                cont_item_ref = result;
                return cont_item_ref->clone();
            }
            else if (rv_ref->get_base_type() == variable_base_type::realm)
            {
                ref<realm> realm_rhs = std::dynamic_pointer_cast<realm>(rv_ref);
                ref<realm> result = make_ref<realm>();

                if (realm_rhs)
                {
                    auto members = realm_rhs->get_members();

                    auto& symbols = members->get_symbols();
                    result->init_members();
                    auto& lhs_symbols = result->get_members()->get_symbols();

                    for (auto& item : symbols)
                    {
                        if (item.second.var_ref->get_type() == variable_type::vt_float_ref)
                            lhs_symbols[item.first] = symbol(make_ref<variable>(any(*std::dynamic_pointer_cast<variable>(item.second.var_ref)->get_data_as<double*>()), variable_type::vt_float), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                        else if (item.second.var_ref->get_type() == variable_type::vt_character_ref)
                            lhs_symbols[item.first] = symbol(make_ref<variable>(any(*std::dynamic_pointer_cast<variable>(item.second.var_ref)->get_data_as<char*>()), variable_type::vt_character), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                        else
                            lhs_symbols[item.first] = symbol(item.second.var_ref->clone(), item.second.is_local(), item.second.is_global(), item.second.is_ref());
                    }
                }
                else
                {
                    result->init_members();
                }

                cont_item_ref = result;
                return cont_item_ref->clone();
            }

            throw invalid_operator_error("Invalid assignment error!");
        }
    }

}
