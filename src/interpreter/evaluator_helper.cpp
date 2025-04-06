#include "evaluator_helper.h"

#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"


#include "../variables/array.h"
#include "../variables/function.h"
#include "../variables/dictionary.h"
#include "../variables/realm.h"

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
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<long long>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(std::to_string(any_cast<long long>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<long long>(right_value)), variable_type::vt_float);
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(std::to_string(any_cast<double>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + any_cast<std::string>(right_value)), variable_type::vt_string);
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + std::to_string(any_cast<long long>(right_value))), variable_type::vt_string);
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + std::to_string(any_cast<double>(right_value))), variable_type::vt_string);
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<vec2>(left_value) + any_cast<double>(right_value)), variable_type::vt_vec2);
                if (rv_ref->get_type() == variable_type::vt_vec2)
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
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<long long>(right_value)), variable_type::vt_float);
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
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<long long>(right_value)), variable_type::vt_float);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(std::string(any_cast<long long>(right_value), any_cast<char>(left_value))), variable_type::vt_string);
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
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<long long>(right_value)), variable_type::vt_float);
                }
            }
            else if (lv_ref->get_type() == variable_type::vt_vec2)
            {
                if (rv_ref->get_type() == variable_type::vt_integer)
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) * (double)any_cast<long long>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_float)
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) * any_cast<double>(right_value)), variable_type::vt_vec2);
                }
                else if (rv_ref->get_type() == variable_type::vt_vec2)
                {
                    if (any_cast<vec2>(right_value).one_of_zero())
                        throw invalid_operation_error("Division by zero error.", loc);

                    return make_ref<variable>(any(any_cast<vec2>(left_value) * any_cast<vec2>(right_value)), variable_type::vt_vec2);
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

            if (lv_ref->get_type() == variable_type::vt_null)
            {
                if (rv_ref->get_base_type() != lv_ref->get_base_type() && rv_ref->get_type() != variable_type::vt_null)
                    throw type_mismatch_error("Types are not matching!", loc);

                lv_ref = rv_ref->clone();

                if (auto v = std::dynamic_pointer_cast<variable>(lv_ref))
                    return v->clone();
                else if (auto a = std::dynamic_pointer_cast<var_array>(lv_ref))
                    return a->clone();
                else if (auto d = std::dynamic_pointer_cast<var_dictionary>(lv_ref))
                    return d->clone();
                else if (auto f = std::dynamic_pointer_cast<var_function>(lv_ref))
                    return f->clone();

                throw exception("Unexpected error!");
            }
            else if (lv_ref->get_base_type() == variable_base_type::variable)
            {
                ref<variable> variable_lhs = std::dynamic_pointer_cast<variable>(lv_ref);
                ref<variable> variable_rhs = std::dynamic_pointer_cast<variable>(rv_ref);

                variable_lhs->set_data(variable_rhs->get_data());
                variable_lhs->set_type(variable_rhs->get_type());

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
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) < any_cast<double>(right_value)), variable_type::vt_bool);
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
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) > any_cast<double>(right_value)), variable_type::vt_bool);
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
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) <= any_cast<double>(right_value)), variable_type::vt_bool);
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
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) >= any_cast<double>(right_value)), variable_type::vt_bool);
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
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<long long>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_float)
            {
                if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<double>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<double>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_character)
            {
                if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any(any_cast<char>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any(any_cast<char>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_bool)
            {
                if (rv_ref->get_type() == variable_type::vt_bool)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == (long long)any_cast<bool>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_integer)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<long long>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_character)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<char>(right_value)), variable_type::vt_bool);
                else if (rv_ref->get_type() == variable_type::vt_float)
                    return make_ref<variable>(any((long long)any_cast<bool>(left_value) == any_cast<double>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_string)
            {
                if (rv_ref->get_type() == variable_type::vt_string)
                    return make_ref<variable>(any(any_cast<std::string>(left_value) == any_cast<std::string>(right_value)), variable_type::vt_bool);
            }
            else if (lv_ref->get_type() == variable_type::vt_array)
            {
                return nullptr;
            }
            else if (lv_ref->get_type() == variable_type::vt_dictionary)
            {
                return nullptr;
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

        ref<variable_base> eval_binary_exp_left_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }

        ref<variable_base> eval_binary_exp_right_shift(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }

        ref<variable_base> eval_binary_exp_left_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }

        ref<variable_base> eval_binary_exp_right_shift_assign(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }

        ref<variable_base> eval_binary_exp_to_left(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }

        ref<variable_base> eval_binary_exp_to_right(ref<variable_base> lv_ref, ref<variable_base> rv_ref, const location& loc)
        {
            return ref<variable_base>();
        }


    }

}
