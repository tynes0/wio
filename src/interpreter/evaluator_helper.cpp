#include "evaluator_helper.h"

#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/file_wrapper.h"

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
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + any_cast<char>(right_value)), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_character_ref)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + *any_cast<char*>(right_value)), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + std::to_string(any_cast<long long>(right_value))), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + util::double_to_string(any_cast<double>(right_value))), variable_type::vt_string);
                else if (rv_ref->get_type() == variable_type::vt_float_ref)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + util::double_to_string(*any_cast<double*>(right_value))), variable_type::vt_string);
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
                return nullptr;
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
                }
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
                return nullptr;
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
                return nullptr;
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
                    if (any_cast<vec2>(right_value).one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) / any_cast<vec2>(right_value)), variable_type::vt_vec2);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec3)
            {
                return nullptr;
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
                    variable_lhs->set_data(variable_rhs->get_data());
                    variable_lhs->set_type(variable_rhs->get_type());
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
                    array_lhs->set_data(array_rhs->get_data());
                else
                    array_lhs->set_data({});

                return array_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::dictionary)
            {
                if (rv_ref->get_base_type() != variable_base_type::dictionary && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'dict' but source type is not!", loc);
                ref<var_dictionary> dict_lhs = std::dynamic_pointer_cast<var_dictionary>(lv_ref);
                ref<var_dictionary> dict_rhs = std::dynamic_pointer_cast<var_dictionary>(rv_ref);

                if (dict_rhs)
                    dict_lhs->set_data(dict_rhs->get_data());
                else
                    dict_lhs->set_data({});

                return dict_lhs->clone();
            }
            else if (lv_ref->get_base_type() == variable_base_type::function)
            {
                if (rv_ref->get_base_type() != variable_base_type::function && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Target type is 'func' but source type is not!", loc);
                ref<var_function> func_lhs = std::dynamic_pointer_cast<var_function>(lv_ref);
                ref<var_function> func_rhs = std::dynamic_pointer_cast<var_function>(rv_ref);

                if (func_rhs)
                    func_lhs->set_data(func_rhs->get_data());
                else
                    func_lhs->set_data({});

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
                    realm_lhs->load_members(realm_rhs->get_members());
                }
                else
                {
                    realm_lhs->load_members(nullptr);
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

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_addition(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_subtract_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_subtraction(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_multiply_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_multiplication(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_divide_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_division(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_modulo_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_modulo(lv_ref, rv_ref, loc), loc);
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
                return nullptr; // TODOO
            }
            else if (lv_ref->get_type() == variable_type::vt_dictionary)
            {
                return nullptr; // TODOO
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
                return nullptr;
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

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_left_shift(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_right_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            if (lv_ref->is_constant())
                throw constant_value_assignment_error("Constant values cannot be changed!", loc);

            return eval_binary_exp_assignment(lv_ref, eval_binary_exp_right_shift(lv_ref, rv_ref, loc), loc);
        }

        ref<variable_base> eval_binary_exp_to_left(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            any left_value;

            if (lv_ref->get_base_type() == variable_base_type::variable)
                left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();

            if (lv_ref->get_type() == variable_type::vt_file)
            {
                filesystem::write_file(any_cast<file_wrapper>(left_value).get_file(), builtin::helpers::var_to_string(rv_ref));
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
                //else if (v_ref->get_type() == variable_type::vt_vec3)
                //    return make_ref<variable>(any(-any_cast<vec3>(value)), variable_type::vt_vec3);
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
            any value;

            if (v_ref->get_base_type() == variable_base_type::variable)
                value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();

            if (v_ref->get_type() == variable_type::vt_integer)
            {
                long long& ll_value = any_cast<long long&>(value);
                long long old_value = ll_value;

                ll_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? ll_value : old_value), variable_type::vt_integer);

            }
            else if (v_ref->get_type() == variable_type::vt_float)
            {
                double& d_value = any_cast<double&>(value);
                double old_value = d_value;

                d_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_float_ref)
            {
                double& d_value = *any_cast<double*>(value);
                double old_value = d_value;

                d_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_character)
            {
                char& c_value = any_cast<char&>(value);
                char old_value = c_value;

                c_value++;
                    
                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_character_ref)
            {
                char& c_value = *any_cast<char*>(value);
                char old_value = c_value;

                c_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_vec2)
            {
                vec2& v2_value = any_cast<vec2&>(value);
                vec2 old_value = v2_value;

                v2_value++;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v2_value : old_value), variable_type::vt_vec2);
            }
            else if (v_ref->get_type() == variable_type::vt_vec3)
            {
                return nullptr;
            }
            else if (v_ref->get_type() == variable_type::vt_vec4)
            {
                return nullptr;
            }

            throw invalid_type_error("Unary '++' operators require a numeric or character or vec operand.", loc);
        }

        ref<variable_base> eval_unary_exp_decrement(ref<variable_base> v_ref, unary_operator_type op_type, const location& loc)
        {
            any value;

            if (v_ref->get_base_type() == variable_base_type::variable)
                value = std::dynamic_pointer_cast<variable>(v_ref)->get_data();

            if (v_ref->get_type() == variable_type::vt_integer)
            {
                long long& ll_value = any_cast<long long&>(value);
                long long old_value = ll_value;

                ll_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? ll_value : old_value), variable_type::vt_integer);

            }
            else if (v_ref->get_type() == variable_type::vt_float)
            {
                double& d_value = any_cast<double&>(value);
                double old_value = d_value;

                d_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_float_ref)
            {
                double& d_value = *any_cast<double*>(value);
                double old_value = d_value;

                d_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? d_value : old_value), variable_type::vt_float);
            }
            else if (v_ref->get_type() == variable_type::vt_character)
            {
                char& c_value = any_cast<char&>(value);
                char old_value = c_value;

                c_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_character_ref)
            {
                char& c_value = *any_cast<char*>(value);
                char old_value = c_value;

                c_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? c_value : old_value), variable_type::vt_character);
            }
            else if (v_ref->get_type() == variable_type::vt_vec2)
            {
                vec2& v2_value = any_cast<vec2&>(value);
                vec2 old_value = v2_value;

                v2_value--;

                return make_ref<variable>(any(op_type == unary_operator_type::prefix ? v2_value : old_value), variable_type::vt_vec2);
            }
            else if (v_ref->get_type() == variable_type::vt_vec3)
            {
                return nullptr;
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
    }

}
