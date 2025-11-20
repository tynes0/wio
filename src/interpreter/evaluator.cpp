#include "evaluator.h"

#include "lexer.h"
#include "parser.h"
#include "evaluator_helper.h"

#include "../base/exception.h"
#include "../utils/filesystem.h"
#include "../utils/util.h"

#include "../variables/variable.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../variables/function.h"
#include "../variables/realm.h"
#include "../variables/enum.h"
#include "../variables/unit.h"

#include "../builtin/builtin_base.h"
#include "../builtin/helpers.h"
#include "../builtin/builtin_manager.h"

#include "../interpreter.h"
#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/pair.h"

#include "eval_base.h"
#include "main_table.h"

#include <filesystem>
#include <stack>

namespace wio
{
    struct evaluator_data
    {
        id_t id = 0;
        packed_bool flags{}; // 1- pure 2- realm
    };

    static std::stack<evaluator_data> s_data_stack;

    void evaluator::evaluate_program(ref<program> program_node, uint32_t id, packed_bool flags)
    {
        main_table::get().add(id);
        
        evaluator_data data
        {
            .id = id,
            .flags = flags
        };

        s_data_stack.push(data);

        eval_base::get().enter_statement_stack(&program_node->m_statements);

        for (auto& stmt : program_node->m_statements) 
            evaluate_statement(stmt);

        eval_base::get().exit_statement_stack();

        s_data_stack.pop();
    }

    ref<variable_base> evaluator::evaluate_single_expr(ref<expression> node)
    {
        return evaluate_expression(node);
    }

    ref<variable_base> evaluator::evaluate_expression(ref<expression> node)
    {
        return as_value(node);
    }

    void evaluator::evaluate_statement(ref<statement> node)
    {
        // Should evaluate statement be made or not? Check the flags
        if (eval_base::get().break_called() || eval_base::get().continue_called() || eval_base::get().return_called())
            return;

        if (!node)
            return;
        
        if (auto decl = std::dynamic_pointer_cast<variable_declaration>(node))
            evaluate_variable_declaration(decl);
        else if (auto def = std::dynamic_pointer_cast<function_definition>(node))
            evaluate_function_definition(def);
        else if (auto fdecl = std::dynamic_pointer_cast<function_declaration>(node))
            evaluate_function_declaration(fdecl);
        else if (auto ldecl = std::dynamic_pointer_cast<lambda_declaration>(node))
            evaluate_lambda_declaration(ldecl);
        else if (auto adecl = std::dynamic_pointer_cast<array_declaration>(node))
            evaluate_array_declaration(adecl);
        else if (auto ddecl = std::dynamic_pointer_cast<dictionary_declaration>(node))
            evaluate_dictionary_declaration(ddecl);
        else if (auto rdecl = std::dynamic_pointer_cast<realm_declaration>(node))
            evaluate_realm_declaration(rdecl);
        else if (auto odecl = std::dynamic_pointer_cast<omni_declaration>(node))
            evaluate_omni_declaration(odecl);
        else if (auto edecl = std::dynamic_pointer_cast<enum_declaration>(node))
            evaluate_enum_declaration(edecl);
        else if (auto udecl = std::dynamic_pointer_cast<unit_declaration>(node))
            evaluate_unit_declaration(udecl);
        else if (auto umdecl = std::dynamic_pointer_cast<unit_member_declaration>(node))
            evaluate_unit_member_declaration(umdecl);
        else if (auto import_s = std::dynamic_pointer_cast<import_statement>(node))
            evaluate_import_statement(import_s);
        else if (auto block_s = std::dynamic_pointer_cast<block_statement>(node))
            evaluate_block_statement(block_s);
        else if (auto expression_s = std::dynamic_pointer_cast<expression_statement>(node))
            evaluate_expression_statement(expression_s);
        else if (auto if_s = std::dynamic_pointer_cast<if_statement>(node))
            evaluate_if_statement(if_s);
        else if (auto for_s = std::dynamic_pointer_cast<for_statement>(node))
            evaluate_for_statement(for_s);
        else if (auto foreach_s = std::dynamic_pointer_cast<foreach_statement>(node))
            evaluate_foreach_statement(foreach_s);
        else if (auto while_s = std::dynamic_pointer_cast<while_statement>(node))
            evaluate_while_statement(while_s);
        else if (auto break_s = std::dynamic_pointer_cast<break_statement>(node))
            evaluate_break_statement(break_s);
        else if (auto continue_s = std::dynamic_pointer_cast<continue_statement>(node))
            evaluate_continue_statement(continue_s);
        else if (auto return_s = std::dynamic_pointer_cast<return_statement>(node))
            evaluate_return_statement(return_s);
        else
            throw local_exception("Unknown statement!", node->get_location());
    }

    ref<variable_base> evaluator::evaluate_declaration_statement(ref<statement> node)
    {
        // Should evaluate statement be made or not? Check the flags
        if (eval_base::get().break_called() || eval_base::get().continue_called() || eval_base::get().return_called())
            return nullptr;

        if (!node)
            return nullptr;

        if (auto decl = std::dynamic_pointer_cast<variable_declaration>(node))
            return evaluate_variable_declaration(decl);
        else if (auto def = std::dynamic_pointer_cast<function_definition>(node))
            return evaluate_function_definition(def);
        else if (auto fdecl = std::dynamic_pointer_cast<function_declaration>(node))
            return evaluate_function_declaration(fdecl);
        else if (auto ldecl = std::dynamic_pointer_cast<lambda_declaration>(node))
            return evaluate_lambda_declaration(ldecl);
        else if (auto adecl = std::dynamic_pointer_cast<array_declaration>(node))
            return evaluate_array_declaration(adecl);
        else if (auto ddecl = std::dynamic_pointer_cast<dictionary_declaration>(node))
            return evaluate_dictionary_declaration(ddecl);
        else if (auto rdecl = std::dynamic_pointer_cast<realm_declaration>(node))
            return evaluate_realm_declaration(rdecl);
        else if (auto odecl = std::dynamic_pointer_cast<omni_declaration>(node))
            return evaluate_omni_declaration(odecl);
        else if (auto edecl = std::dynamic_pointer_cast<enum_declaration>(node))
            return evaluate_enum_declaration(edecl);
        else if (auto udecl = std::dynamic_pointer_cast<unit_declaration>(node))
            return evaluate_unit_declaration(udecl);
        else if (auto umdecl = std::dynamic_pointer_cast<unit_member_declaration>(node))
            return evaluate_unit_member_declaration(umdecl);

        return nullptr;
    }

    ref<variable_base> evaluator::evaluate_literal(ref<literal> node)
    {
        if (node->m_token.type == token_type::number) 
        {
            if (node->get_literal_type() == lit_type::lt_decimal)
                return make_ref<variable>(any(std::stoll(node->m_token.value, nullptr, 10)), variable_type::vt_integer);
            else if (node->get_literal_type() == lit_type::lt_float)
                return make_ref<variable>(any(std::stod(node->m_token.value)), variable_type::vt_float);
            else if (node->get_literal_type() == lit_type::lt_binary)
                return make_ref<variable>(any(std::stoll(node->m_token.value.substr(2), nullptr, 2)), variable_type::vt_integer);
            else if (node->get_literal_type() == lit_type::lt_octal)
                return make_ref<variable>(any(std::stoll(node->m_token.value.substr(2), nullptr, 8)), variable_type::vt_integer);
            else if (node->get_literal_type() == lit_type::lt_hexadeximal)
                return make_ref<variable>(any(std::stoll(node->m_token.value.substr(2), nullptr, 16)), variable_type::vt_integer);
            else
                throw evaluation_error("Invalid number literal type.", node->get_location());
        }
        else if (node->m_token.type == token_type::kw_true) 
        {
            return make_ref<variable>(any(true), variable_type::vt_bool);
        }
        else if (node->m_token.type == token_type::kw_false) 
        {
            return make_ref<variable>(any(false), variable_type::vt_bool);
        }
        else if (node->m_token.type == token_type::character) 
        {
            if (node->m_token.value.size() == 1) 
                return make_ref<variable>(any(node->m_token.value[0]), variable_type::vt_character);
            else 
                throw evaluation_error("Invalid character literal", node->get_location());
        }
        else if (node->m_token.type == token_type::string)
        {
            return evaluate_string_literal(std::dynamic_pointer_cast<string_literal>(node));
        }
        else 
        {
            throw evaluation_error("Invalid literal type.", node->get_location());
        }
    }

    ref<variable_base> evaluator::evaluate_string_literal(ref<string_literal> node)
    {
        return make_ref<variable>(any(node->m_token.value), variable_type::vt_string);
    }

    ref<variable_base> evaluator::evaluate_array_literal(ref<array_literal> node)
    {
        std::vector<ref<variable_base>> elements;

        for (auto& item : node->m_elements)
        {
            ref<variable_base> var = evaluate_expression(item)->clone();
            elements.push_back(var);
        }

        return make_ref<var_array>(elements);
    }

    ref<variable_base> evaluator::evaluate_dictionary_literal(ref<dictionary_literal> node)
    {
        var_dictionary::map_t elements;

        for (auto& item : node->m_pairs)
        {
            ref<variable_base> var = evaluate_expression(item.second)->clone();

            std::string key = var_dictionary::as_key(evaluate_expression(item.first));

            if (elements.find(key) != elements.end())
                throw invalid_key_error("Key '" + key + "' was used more than once.");

            elements[key] = var;
        }

        return make_ref<var_dictionary>(elements);
    }

    ref<variable_base> evaluator::evaluate_lambda_literal(ref<lambda_literal> node)
    {
        std::vector<function_param> parameters;
        for (auto& param : node->m_params)
            parameters.emplace_back(param->m_id->m_token.value, param->m_type, param->m_is_ref);

        auto result_fun_v = make_ref<var_function>(parameters);

        auto func_lambda = [node, result_fun_v](const std::vector<function_param>& param_ids, std::vector<ref<variable_base>>& parameters)->ref<variable_base>
            {
                eval_base::get().enter_scope(s_data_stack.top().id, scope_type::function_body, result_fun_v->get_file_id());

                // Create parameters
                for (auto& param : node->m_params)
                    evaluate_parameter_declaration(param);

                // use variable ref's
                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    symbol* s = eval_base::get().search(s_data_stack.top().id, param_ids[i].id);
                    s->var_ref = parameters[i];
                }

                eval_base::get().enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                {
                    evaluate_statement(stmt);
                    // return check
                    if (eval_base::get().return_called())
                    {
                        eval_base::get().call_return(false);
                        ref<variable_base> retval = eval_base::get().get_last_return_value();
                        eval_base::get().exit_statement_stack();
                        eval_base::get().exit_scope(s_data_stack.top().id);
                        return retval;
                    }
                }
                eval_base::get().exit_statement_stack();

                eval_base::get().exit_scope(s_data_stack.top().id);
                return create_null_variable();
            };

        result_fun_v->set_bounded_file_id(s_data_stack.top().id);
        result_fun_v->set_data(func_lambda);

        ref<overload_list> result = make_ref<overload_list>();
        result->add(symbol(result_fun_v));

        return result;
    }

    ref<variable_base> evaluator::evaluate_typeof_expression(ref<typeof_expression> node)
    {
        ref<variable_base> value_base = evaluate_expression(node->m_expr);
        return make_ref<variable>(value_base->get_type(), variable_type::vt_type);
    }

    ref<variable_base> evaluator::evaluate_binary_expression(ref<binary_expression> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        token op = node->m_operator;

        if (op.type == token_type::op) 
        {
            bool right_is_ref = false;
            if (op.value == "->")
                right_is_ref = true;

            ref<variable_base> lv_ref = as_value(node->m_left, object, is_ref || node->is_assignment, access_level);
            ref<variable_base> rv_ref = as_value(node->m_right, nullptr, right_is_ref);

            if (op.value == "+") 
                return helper::eval_binary_exp_addition(lv_ref, rv_ref, node->get_location());
            else if (op.value == "-") 
                return helper::eval_binary_exp_subtraction(lv_ref, rv_ref, node->get_location());
            else if (op.value == "*") 
                return helper::eval_binary_exp_multiplication(lv_ref, rv_ref, node->get_location());
            else if (op.value == "/") 
                return helper::eval_binary_exp_division(lv_ref, rv_ref, node->get_location());
            else if (op.value == "%") 
                return helper::eval_binary_exp_modulo(lv_ref, rv_ref, node->get_location());
            else if (op.value == "=" && lv_ref->is_omni())
                return helper::eval_omni_assignment(*eval_base::get().get_last_left_value(), rv_ref);
            else if (op.value == "=")
                return helper::eval_binary_exp_assignment(lv_ref, rv_ref, node->get_location());
            else if (op.value == "+=")
                return helper::eval_binary_exp_add_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "-=")
                return helper::eval_binary_exp_subtract_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "*=")
                return helper::eval_binary_exp_multiply_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "/=")
                return helper::eval_binary_exp_divide_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "%=")
                return helper::eval_binary_exp_modulo_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<")
                return helper::eval_binary_exp_less_than(lv_ref, rv_ref, node->get_location());
            else if (op.value == ">")
                return helper::eval_binary_exp_greater_than(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<=")
                return helper::eval_binary_exp_less_than_or_equal_to(lv_ref, rv_ref, node->get_location());
            else if (op.value == ">=")
                return helper::eval_binary_exp_greater_than_or_equal_to(lv_ref, rv_ref, node->get_location());
            else if (op.value == "==")
                return helper::eval_binary_exp_equal_to(lv_ref, rv_ref, node->get_location());
            else if (op.value == "!=")
                return helper::eval_binary_exp_not_equal_to(lv_ref, rv_ref, node->get_location());
            else if (op.value == "=?")
                return helper::eval_binary_exp_type_equal(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<=>")
                return helper::eval_binary_exp_compare_all(lv_ref, rv_ref, node->get_location());
            else if (op.value == "&&")
                return helper::eval_binary_exp_logical_and(lv_ref, rv_ref, node->get_location());
            else if (op.value == "||")
                return helper::eval_binary_exp_logical_or(lv_ref, rv_ref, node->get_location());
            else if (op.value == "^^")
                return helper::eval_binary_exp_logical_xor(lv_ref, rv_ref, node->get_location());
            else if (op.value == "&")
                return helper::eval_binary_exp_bitwise_and(lv_ref, rv_ref, node->get_location());
            else if (op.value == "|")
                return helper::eval_binary_exp_bitwise_or(lv_ref, rv_ref, node->get_location());
            else if (op.value == "^")
                return helper::eval_binary_exp_bitwise_xor(lv_ref, rv_ref, node->get_location());
            else if (op.value == "&=")
                return helper::eval_binary_exp_bitwise_and_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "|=")
                return helper::eval_binary_exp_bitwise_or_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "^=")
                return helper::eval_binary_exp_bitwise_xor_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<<")
                return helper::eval_binary_exp_left_shift(lv_ref, rv_ref, node->get_location());
            else if (op.value == ">>")
                return helper::eval_binary_exp_right_shift(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<<=")
                return helper::eval_binary_exp_left_shift_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == ">>=")
                return helper::eval_binary_exp_right_shift_assign(lv_ref, rv_ref, node->get_location());
            else if (op.value == "<-")
                return helper::eval_binary_exp_to_left(lv_ref, rv_ref, node->get_location());
            else if (op.value == "->")
                return helper::eval_binary_exp_to_right(lv_ref, rv_ref, node->get_location());
        }

        throw invalid_operator_error("Invalid binary operator: " + node->m_operator.value, node->get_location());
    }

    ref<variable_base> evaluator::evaluate_unary_expression(ref<unary_expression> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        ref<variable_base> operand_ref = as_value(node->m_operand, object, node->m_is_ref || is_ref, access_level);
        token op = node->m_operator;

        auto& t = main_table::get();

        if (op.type == token_type::op) 
        {
            if (op.value == "+")
            {
                return helper::eval_unary_exp_positive(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "-")
            {
                return helper::eval_unary_exp_negative(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "!")
            {
                return helper::eval_unary_exp_logical_not(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "?")
            {
                return helper::eval_unary_exp_is_not_null(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "~")
            {
                return helper::eval_unary_exp_bitwise_not(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "++")
            {
                if (!(std::dynamic_pointer_cast<identifier>(node->m_operand) || std::dynamic_pointer_cast<array_access_expression>(node->m_operand) || std::dynamic_pointer_cast<member_access_expression>(node->m_operand)))
                    throw  invalid_operation_error("Invalid operand to increment operator.", node->get_location());

                if (operand_ref->is_constant())
                    throw constant_value_assignment_error("Constant values cannot be changed!", node->get_location());

                return helper::eval_unary_exp_increment(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
            else if (op.value == "--")
            {
                if (!(std::dynamic_pointer_cast<identifier>(node->m_operand) || std::dynamic_pointer_cast<array_access_expression>(node->m_operand) || std::dynamic_pointer_cast<member_access_expression>(node->m_operand)))
                    throw  invalid_operation_error("Invalid operand to decrement operator.", node->get_location());

                if (operand_ref->is_constant())
                    throw constant_value_assignment_error("Constant values cannot be changed!", node->get_location());

                return helper::eval_unary_exp_decrement(operand_ref, node->m_op_type, node->m_operand->get_location());
            }
        }

        throw invalid_operator_error("Invalid unary operator: " + op.value, node->get_location());
    }

    ref<variable_base> evaluator::evaluate_identifier(ref<identifier> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        symbol* sym = eval_base::get().search_member(s_data_stack.top().id, node->m_token.value, object);

        if (!sym)
        {
            if(object)
                throw undefined_identifier_error("Undefined member of type '" + util::type_to_string(object->get_type()) + "': " + node->m_token.value, node->get_location());
            throw undefined_identifier_error("Undefined identifier: " + node->m_token.value, node->get_location());
        }

        if (sym->var_ref)
        {
            if (object && object->get_base_type() == variable_base_type::unit_instance)
            {
                auto obj_access_level = access_level_of(std::dynamic_pointer_cast<unit_instance>(object)->access_type_of(node->m_token.value));
                if (obj_access_level > access_level)
                    throw invalid_access_error("Invalid access!", node->get_location());
            }
            if (node->m_is_lhs || is_ref)
            {
                eval_base::get().set_last_left_value(&sym->var_ref);
                return sym->var_ref;
            }
            else if (sym->is_ref())
            {
                return sym->var_ref;
            }
            return sym->var_ref->clone();
        }

        // It must be unattainable, because all symbols have a value. (We assume null as a value.)
        throw evaluation_error("Identifier '" + node->m_token.value + "' has no value.", node->get_location());
    }

    ref<variable_base> evaluator::evaluate_array_access_expression(ref<array_access_expression> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        ref<variable_base> item = as_value(node->m_array, object, is_ref, access_level);
        auto& tbl = main_table::get();

        variable_base_type base_t = item->get_base_type();

        if (base_t == variable_base_type::array)
        {
            ref<variable_base> index_base = evaluate_expression(node->m_key_or_index);

            ref<var_array> array_var = std::dynamic_pointer_cast<var_array>(item);

            if (node->m_is_lhs || is_ref)
            {
                auto& result = array_var->get_element(builtin::helper::var_as_integer(index_base));
                eval_base::get().set_last_left_value(&result);
                return result;
            }
            else if (node->m_is_ref)
            {
                return array_var->get_element(builtin::helper::var_as_integer(index_base));
            }
            return array_var->get_element(builtin::helper::var_as_integer(index_base))->clone();
        }
        else if (base_t == variable_base_type::variable)
        {
            if(item->get_type() != variable_type::vt_string)
                throw invalid_type_error("Type should be array or string or dictionary!", node->m_array->get_location());

            ref<variable_base> index_base = evaluate_expression(node->m_key_or_index);
            if (index_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> index_var = std::dynamic_pointer_cast<variable>(index_base);
            if (index_var->get_type() != variable_type::vt_integer)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> str_var = std::dynamic_pointer_cast<variable>(item);
            std::string& value = str_var->get_data_as<std::string>();
            integer_t index = index_var->get_data_as<integer_t>();
            if (index < 0 || index >= integer_t(value.size()))
                throw out_of_bounds_error("String index out of the bounds!");

            return (node->m_is_ref || node->m_is_lhs || is_ref) ?
                make_ref<variable>(any(&value[index]), variable_type::vt_character_ref, packed_bool{ false, false, true }) :
                make_ref<variable>(any(value[index]), variable_type::vt_character);
        }
        else if (base_t == variable_base_type::dictionary)
        {
            ref<variable_base> key_base = evaluate_expression(node->m_key_or_index);
            if (key_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be variable!", node->m_key_or_index->get_location());

            ref<var_dictionary> dict_var = std::dynamic_pointer_cast<var_dictionary>(item);

            if (node->m_is_lhs || is_ref)
            {
                auto& result = dict_var->get_element(key_base);
                eval_base::get().set_last_left_value(&result);
                return result;
            }
            else if (node->m_is_ref)
            {
                return dict_var->get_element(key_base);
            }
            return dict_var->get_element(key_base)->clone();
        }
        else
        {
            throw invalid_type_error("Type should be array or string or dictionary!", node->m_array->get_location());
        }
    }

    ref<variable_base> evaluator::evaluate_member_access_expression(ref<member_access_expression> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        ref<variable_base> object_value = as_value(node->m_object, object, is_ref, access_level);
        auto unit_v = std::dynamic_pointer_cast<unit_instance>(object_value);
        ref<variable_base> member;

        if (unit_v)
        {
            if (eval_base::get().is_in_unit_decl())
            {
                if (*unit_v->get_source() == *eval_base::get().active_unit())
                {
                    member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::hidden));
                }
                else
                {
                    auto& trust_list = eval_base::get().active_unit()->get_trust_list();
                    for (auto& trusted_unit : trust_list)
                    {
                        if (unit_v->get_source() == trusted_unit)
                        {
                            member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::hidden));
                            break;
                        }
                    }

                    if(!member)
                        member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::exposed));
                }
            }
            else
            {
                auto luwmfic = eval_base::get().last_unit_whose_member_function_is_called(); // last_unit_whose_member_function_is_called

                if (!luwmfic)
                {
                    member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::exposed));
                }
                else if (*unit_v->get_source() == *luwmfic->get_source())
                {
                    member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::hidden));
                }
                else
                {
                    auto& trust_list = luwmfic->get_source()->get_trust_list();
                    for (auto& trusted_unit : trust_list)
                    {
                        if (unit_v->get_source() == trusted_unit)
                        {
                            member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::hidden));
                            break;
                        }
                    }

                    if (!member)
                        member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::exposed));
                }
            }
        }
        else
        {
            member = as_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref, access_level_of(unit_access_type::exposed));
        }
        return member;
    }

    ref<variable_base> evaluator::evaluate_function_call(ref<function_call> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        if (s_data_stack.top().flags.b1)
            return create_null_variable();

        std::vector<ref<variable_base>> parameters;
        std::vector<function_param> type_parameters;
        bool pop_unit = false;

        int padding = 0;


        if (object)
        {
            if ((object->get_type() != variable_type::vt_realm && object->get_type() != variable_type::vt_unit_instance))
            {
                object->set_pf_return_ref(is_ref);
                padding = 1;
                parameters.push_back(object);
                type_parameters.push_back(function_param("", object->get_base_type(), false));
            }
            else if (object->get_base_type() == variable_base_type::unit_instance)
            {
                eval_base::get().push_unit_whose_member_function_is_called(std::dynamic_pointer_cast<unit_instance>(object));
                pop_unit = true;
            }
        }

        eval_base::get().set_ref_error_activity(false);
        for (size_t i = 0; i < node->m_arguments.size(); ++i)
        {
            auto value = as_value(node->m_arguments[i], nullptr, node->m_arguments[i]->is_ref());
            parameters.push_back(value);
            type_parameters.push_back(function_param("", value->get_base_type(), false));
        }
        eval_base::get().set_ref_error_activity(true);

        eval_base::get().set_last_parameters(type_parameters);

        ref<variable_base> caller_value = as_value(node->m_caller, object, false, access_level);

        if (!caller_value)
           throw undefined_identifier_error("Invalid function!", node->m_caller->get_location());
        else if (caller_value->get_type() != variable_type::vt_function)
           throw invalid_type_error("Type should be function!", node->m_caller->get_location());

        ref<overload_list> olist = std::dynamic_pointer_cast<overload_list>(caller_value);
        ref<var_function> func_res;

        if (!olist)
        {
            func_res = std::dynamic_pointer_cast<var_function>(caller_value);
        }
        else
        {
            symbol* fref = olist->find(type_parameters);

            if (!fref)
                throw type_mismatch_error("Parameter types not matching!", node->get_location());

            func_res = std::dynamic_pointer_cast<var_function>(fref->var_ref);
        }

        if (!func_res)
            throw undefined_identifier_error("Invalid function!", node->m_caller->get_location());

        if (!func_res->declared())
        {
            auto decl = eval_base::get().get_func_definition(olist->get_symbol_id(), func_res->get_parameters());
            if(!decl)
                throw local_exception("Function '" + olist->get_symbol_id() + "' defined but not declared!", node->m_caller->get_location());

            evaluate_function_definition(decl);
            func_res->set_early_declared(true);
        }

        if (object)
        {
            eval_base::get().set_object_state(true);

            if (object->get_members())
            {
                ref<scope> scp = make_ref<scope>(scope_type::object);
                scp->set_symbols(object->get_members()->get_symbols());

                eval_base::get().enter_this_scope(s_data_stack.top().id, scp);
            }

            ref<variable_base> result = func_res->call(parameters);
            
            object->set_pf_return_ref(false);

            if (object->get_members())
            {
                eval_base::get().exit_scope(s_data_stack.top().id);
            }

            if(pop_unit)
                eval_base::get().pop_unit_whose_member_function_is_called();

            eval_base::get().set_object_state(false);

            return result;
        }
        else
        {
            ref<variable_base> result = func_res->call(parameters);
            return result;
        }
    }

    ref<variable_base> evaluator::evaluate_new_unit_instance_call(ref<new_unit_instance_call> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        std::vector<ref<variable_base>> parameters;
        std::vector<function_param> type_parameters;

        int padding = 0;

        if (object)
        {
            if ((object->get_type() != variable_type::vt_realm && object->get_type() != variable_type::vt_unit_instance))
            {
                object->set_pf_return_ref(is_ref);
                padding = 1;
                parameters.push_back(object);
                type_parameters.push_back(function_param("", object->get_base_type(), false));
            }
        }

        eval_base::get().set_ref_error_activity(false);
        for (size_t i = 0; i < node->m_arguments.size(); ++i)
        {
            auto value = as_value(node->m_arguments[i], nullptr, node->m_arguments[i]->is_ref());
            parameters.push_back(value);
            type_parameters.push_back(function_param("", value->get_base_type(), false));
        }
        eval_base::get().set_ref_error_activity(true);

        eval_base::get().set_last_parameters(type_parameters);

        ref<variable_base> caller_value = as_value(node->m_caller, object, false, access_level);

        if (!caller_value)
            throw undefined_identifier_error("Invalid unit!", node->m_caller->get_location());
        else if (caller_value->get_type() != variable_type::vt_unit)
            throw invalid_type_error("Type should be unit!", node->m_caller->get_location());

        ref<unit> unit_v = std::dynamic_pointer_cast<unit>(caller_value);

        eval_base::get().push_unit_whose_member_function_is_called(make_ref<unit_instance>(unit_v));

        ref<overload_list> olist = unit_v->get_ctor();
        ref<var_function> func_res;
        symbol* fref = olist->find(type_parameters);

        if (!fref)
            throw type_mismatch_error("Parameter types not matching!", node->get_location());

        func_res = std::dynamic_pointer_cast<var_function>(fref->var_ref);

        if (!func_res)
            throw undefined_identifier_error("Invalid function!", node->m_caller->get_location());

        if (!func_res->declared())
        {
            auto decl = eval_base::get().get_func_definition(olist->get_symbol_id(), func_res->get_parameters());
            if (!decl)
                throw local_exception("Function '" + olist->get_symbol_id() + "' defined but not declared!", node->m_caller->get_location());

            evaluate_function_definition(decl);
            func_res->set_early_declared(true);
        }

        ref<unit_instance> result = make_ref<unit_instance>(unit_v);

        eval_base::get().set_object_state(true);

        if (object->get_members())
        {
            ref<scope> scp = make_ref<scope>(scope_type::object);
            scp->set_symbols(result->get_members()->get_symbols());
            eval_base::get().enter_this_scope(s_data_stack.top().id, scp);
        }

        func_res->call(parameters);
        if (object)
            object->set_pf_return_ref(false);

        eval_base::get().pop_unit_whose_member_function_is_called();
        if (object->get_members())
        {
            eval_base::get().exit_scope(s_data_stack.top().id);
        }

        eval_base::get().set_object_state(false);

        return result;
    }

    ref<scope> evaluator::evaluate_block_statement(ref<block_statement> node)
    {
        eval_base::get().enter_statement_stack_and_scope(s_data_stack.top().id, &node->m_statements);

        for (auto& stmt : node->m_statements)
            evaluate_statement(stmt);

        return eval_base::get().exit_statement_stack_and_scope(s_data_stack.top().id);
    }

    void evaluator::evaluate_expression_statement(ref<expression_statement> node)
    {
        evaluate_expression(node->m_expression);
    }

    void evaluator::evaluate_if_statement(ref<if_statement> node)
    {
        ref<variable_base> cond = as_value(node->m_condition);
        
        if (cond->get_base_type() != variable_base_type::variable)
            throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

        ref<variable> var = std::dynamic_pointer_cast<variable>(cond);

        if (cond->get_type() == variable_type::vt_bool)
        {
            boolean_t value = var->get_data_as<boolean_t>();
            if (value)
                evaluate_statement(node->m_then_branch);
            else
                evaluate_statement(node->m_else_branch);
        }
        else if (cond->get_type() == variable_type::vt_integer)
        {
            integer_t value = var->get_data_as<integer_t>();
            if (value)
                evaluate_statement(node->m_then_branch);
            else
                evaluate_statement(node->m_else_branch);
        }
        else if (cond->get_type() == variable_type::vt_float)
        {
            float_t value = var->get_data_as<float_t>();
            if (value)
                evaluate_statement(node->m_then_branch);
            else
                evaluate_statement(node->m_else_branch);
        }
        else
            throw invalid_type_error("Type should be boolean or integer or float!", node->m_condition->get_location());
    }

    void evaluator::evaluate_for_statement(ref<for_statement> node)
    {
        eval_base::get().enter_scope(s_data_stack.top().id, scope_type::block);
        evaluate_statement(node->m_initialization);

        eval_base::get().inc_loop_depth();
        while(true)
        {
            ref<variable_base> cond = as_value(node->m_condition);
            if(cond)
            {
                if (cond->get_base_type() != variable_base_type::variable)
                    throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

                ref<variable> var = std::dynamic_pointer_cast<variable>(cond);
                if (var->get_type() != variable_type::vt_bool)
                    throw invalid_type_error("Type should be boolean!", node->m_condition->get_location());

                bool value = var->get_data_as<bool>();
                if (!value)
                {
                    eval_base::get().call_break(false);
                    eval_base::get().call_continue(false);
                    break;
                }
            }

            eval_base::get().enter_statement_stack_and_scope(s_data_stack.top().id, &node->m_body->m_statements);

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            eval_base::get().exit_statement_stack_and_scope(s_data_stack.top().id);

            if (eval_base::get().break_called())
            {
                eval_base::get().call_break(false);
                break;
            }
            else if (eval_base::get().return_called())
            {
                break;
            }
            else if (eval_base::get().continue_called())
            {
                eval_base::get().call_continue(false);
            }
            evaluate_expression(node->m_increment);
        }
        eval_base::get().dec_loop_depth();

        eval_base::get().exit_scope(s_data_stack.top().id);
    }

    void evaluator::evaluate_foreach_statement(ref<foreach_statement> node)
    {
        static auto foreach_body = [](std::vector<ref<statement>>* list)
            {
                eval_base::get().enter_statement_stack_and_scope(s_data_stack.top().id, list);
                for (auto& stmt : *list)
                    evaluate_statement(stmt);
                eval_base::get().exit_statement_stack_and_scope(s_data_stack.top().id);

                if (eval_base::get().break_called())
                {
                    eval_base::get().call_break(false);
                    return true;
                }
                else if (eval_base::get().return_called())
                {
                    return true;
                }
                else if (eval_base::get().continue_called())
                {
                    eval_base::get().call_continue(false);
                }
                return false;
            };

        eval_base::get().enter_scope(s_data_stack.top().id, scope_type::block);
        eval_base::get().inc_loop_depth();

        evaluate_variable_declaration(make_ref<variable_declaration>(node->m_item, nullptr, false, false, false));
        ref<variable_base> col_base = as_value(node->m_collection, nullptr, true);

        symbol* sym = eval_base::get().search(s_data_stack.top().id, node->m_item->m_token.value);

        if (col_base->get_base_type() == variable_base_type::array)
        {
            ref<var_array> array = std::dynamic_pointer_cast<var_array>(col_base);

            if (array->is_constant())
            {
                for (size_t i = 0; i < array->size(); ++i)
                {
                    sym->var_ref = array->get_element(i)->clone();

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
            else
            {
                for (size_t i = 0; i < array->size(); ++i)
                {
                    sym->var_ref = array->get_element(i);

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
        }
        else if (col_base->get_base_type() == variable_base_type::dictionary)
        {
            ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(col_base);

            if (dict->is_constant())
            {
                for (auto& it : dict->get_data())
                {
                    sym->var_ref = builtin::helper::create_pair(make_ref<variable>(it.first, variable_type::vt_string), it.second->clone());

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
            else
            {
                for (auto& it : dict->get_data())
                {
                    sym->var_ref = builtin::helper::create_pair(make_ref<variable>(it.first, variable_type::vt_string), it.second);

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
            
        }
        else if (col_base->get_type() == variable_type::vt_string)
        {
            ref<variable> str = std::dynamic_pointer_cast<variable>(col_base);
            std::string& data = str->get_data_as<std::string>();

            if (str->is_constant())
            {
                for (size_t i = 0; i < data.size(); ++i)
                {
                    sym->var_ref = make_ref<variable>(data[i], variable_type::vt_character);

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
            else
            {
                for (size_t i = 0; i < data.size(); ++i)
                {
                    sym->var_ref = make_ref<variable>(&data[i], variable_type::vt_character_ref);

                    if (foreach_body(&node->m_body->m_statements))
                        break;
                }
            }
        }
        else
        {
            throw type_mismatch_error("Type should be array or dict or string", node->m_collection->get_location());
        }

        eval_base::get().dec_loop_depth();
        eval_base::get().exit_scope(s_data_stack.top().id);
    }

    void evaluator::evaluate_while_statement(ref<while_statement> node)
    {
        eval_base::get().enter_scope(s_data_stack.top().id, scope_type::block);
        eval_base::get().inc_loop_depth();
        while (true)
        {
            ref<variable_base> cond = as_value(node->m_condition);
            if (cond->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

            ref<variable> var = std::dynamic_pointer_cast<variable>(cond);
            if (var->get_type() != variable_type::vt_bool)
                throw invalid_type_error("Type should be boolean!", node->m_condition->get_location());

            bool value = var->get_data_as<bool>();
            if (!value)
            {
                eval_base::get().call_break(false);
                eval_base::get().call_continue(false);
                break;
            }

            eval_base::get().enter_statement_stack_and_scope(s_data_stack.top().id, &node->m_body->m_statements);
            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);
            eval_base::get().exit_statement_stack_and_scope(s_data_stack.top().id);

            if (eval_base::get().break_called())
            {
                eval_base::get().call_break(false);
                break;
            }
            else if (eval_base::get().return_called())
            {
                break;
            }
            else if (eval_base::get().continue_called())
            {
                eval_base::get().call_continue(false);
            }
        }
        eval_base::get().dec_loop_depth();
        eval_base::get().exit_scope(s_data_stack.top().id);
    }

    void evaluator::evaluate_break_statement(ref<break_statement> node)
    {
        if (!eval_base::get().get_loop_depth())
            throw invalid_break_statement("Break statement called out of the loop!", node->m_loc);
        eval_base::get().call_break(true);
    }

    void evaluator::evaluate_continue_statement(ref<continue_statement> node)
    {
        if (!eval_base::get().get_loop_depth())
            throw invalid_continue_statement("Continue statement called out of the loop!", node->m_loc);
        eval_base::get().call_continue(true);
    }

    void evaluator::evaluate_import_statement(ref<import_statement> node)
    {
        symbol_map* target_map = nullptr;
        ref<realm> realm_var = nullptr;

        if (node->m_flags.b2)
        {
            if (!node->m_realm_id)
                throw local_exception("Unexpected error!", node->get_location());

            std::string name = node->m_realm_id->m_token.value;

             if (eval_base::get().search(s_data_stack.top().id, name))
                 throw local_exception("Duplicate variable declaration: " + name, node->m_realm_id->get_location());
            
            realm_var = make_ref<realm>();
            realm_var->init_members();
            
            target_map = &realm_var->get_members()->get_symbols();
            
            if (target_map)
            {
                symbol realm_symbol(realm_var, false, true);
            
                main_table::get().get_scope(s_data_stack.top().id)->insert(name, realm_symbol);
            }
        }

        if (!target_map)
            target_map = &main_table::get().get_scope(s_data_stack.top().id)->get_symbols();
        
        id_t id = interpreter::get().run_file(node->m_module_path.c_str(), { node->m_flags.b1, false }, target_map);

        if(!target_map)
            main_table::get().add_imported_module(id);
    }

    void evaluator::evaluate_return_statement(ref<return_statement> node)
    {
        ref<scope> temp = eval_base::get().get_scope(s_data_stack.top().id);
        while (temp && temp->get_type() != scope_type::function_body)
            temp = temp->get_parent();

        if(!temp)
            throw invalid_return_statement("Return statement out of the function", node->get_location());

        eval_base::get().set_last_return_value(as_value(node->m_value, nullptr, node->m_value ? node->m_value->is_ref() : false));
        eval_base::get().call_return(true);
    }

    ref<variable_base> evaluator::evaluate_variable_declaration(ref<variable_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search(s_data_stack.top().id, name))
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        ref<variable_base> var = create_null_variable();

        if (node->m_initializer)
        {
            auto value = as_value(node->m_initializer, nullptr, node->m_initializer->is_ref());
            
            if (value->get_base_type() == variable_base_type::variable)
            {
                if (node->m_initializer->is_ref())
                {
                    var = value;
                }
                else
                {
                    helper::eval_binary_exp_assignment(var, value, node->m_id->get_location());

                    if (node->m_flags.b1)
                        var->set_const(true);
                }
            }
            else
            {
                throw invalid_declaration_error("initializer has an invalid type.", node->m_initializer->get_location());
            }
        }

        symbol symbol(var, node->m_flags.b2, node->m_flags.b3, node->m_initializer ? node->m_initializer->is_ref() : false);
        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, symbol);

        return var;
    }

    ref<variable_base> evaluator::evaluate_array_declaration(ref<array_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search(s_data_stack.top().id, name))
            throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());

        ref<variable_base> var = make_ref<var_array>(std::vector<ref<variable_base>>{}, packed_bool{ false });

        if (node->m_initializer)
        {
            auto value = as_value(node->m_initializer, nullptr, node->m_initializer->is_ref());

            if (value->get_base_type() == variable_base_type::array || value->get_type() == variable_type::vt_null)
            {
                if (node->m_initializer->is_ref())
                {
                    var = value;
                }
                else
                {
                    helper::eval_binary_exp_assignment(var, value, node->m_id->get_location());

                    if (node->m_flags.b1)
                        var->set_const(true);
                }
            }
            else
            {
                throw invalid_declaration_error("initializer has an invalid type.", node->m_initializer->get_location());
            }
        }

        symbol array_symbol(var, node->m_flags.b2, node->m_flags.b3, node->m_initializer ? node->m_initializer->is_ref() : false);
        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, array_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, array_symbol);

        return var;
    }

    ref<variable_base> evaluator::evaluate_dictionary_declaration(ref<dictionary_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        ref<variable_base> var = make_ref<var_dictionary>(var_dictionary::map_t{}, packed_bool{ false });

        if (node->m_initializer)
        {
            auto value = as_value(node->m_initializer, nullptr, node->m_initializer->is_ref());

            if (value->get_base_type() == variable_base_type::dictionary || value->get_type() == variable_type::vt_null)
            {
                if (node->m_initializer->is_ref())
                {
                    var = value;
                }
                else
                {
                    helper::eval_binary_exp_assignment(var, value, node->m_id->get_location());

                    if (node->m_flags.b1)
                        var->set_const(true);
                }
            }
            else
            {
                throw invalid_declaration_error("initializer has an invalid type.", node->m_initializer->get_location());
            }
        }

        symbol dict_symbol(var, node->m_flags.b2, node->m_flags.b3, node->m_initializer ? node->m_initializer->is_ref() : false);
        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, dict_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, dict_symbol);

        return var;
    }

    ref<variable_base> evaluator::evaluate_function_definition(ref<function_definition> node)
    {
        std::string name = node->m_id->m_token.value;

        // parameter types and ids
        std::vector<function_param> parameters;

        for (auto& param : node->m_params)
            parameters.emplace_back(param->m_id->m_token.value, param->m_type, param->m_is_ref);

        bool add_overload = false;
        bool declare = false;

        auto is_valid_result = eval_base::get().is_function_valid(s_data_stack.top().id, name, parameters);
        symbol* overload = nullptr;

        if (!is_valid_result.first)
            throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());

        if (is_valid_result.second)
        {
            ref<overload_list> ol_ref = std::dynamic_pointer_cast<overload_list>(is_valid_result.second->var_ref);

            if (!ol_ref)
                throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());

            overload = ol_ref->find(parameters);

            if (overload)
            {
                ref<var_function> func_ref = std::dynamic_pointer_cast<var_function>(overload->var_ref);

                if (!func_ref->declared())
                {
                    declare = true;
                }
                else if (func_ref->early_declared())
                {
                    func_ref->set_early_declared(false);
                    return ol_ref;
                }
                else
                {
                    throw invalid_declaration_error("Duplicate function declaration: " + name, node->m_id->get_location());
                }
            }
            else
            {
                add_overload = true;
            }
        }
        ref<var_function> result_fun_v;
        if (declare)
            result_fun_v = std::dynamic_pointer_cast<var_function>(overload->var_ref);
        else 
            result_fun_v = make_ref<var_function>(parameters);

        // Generate func body
        auto func_lambda = [node, result_fun_v](const std::vector<function_param>& param_ids, std::vector<ref<variable_base>>& parameters)->ref<variable_base>
            {
                eval_base::get().enter_scope(s_data_stack.top().id, scope_type::function_body, result_fun_v->get_file_id());

                // Create parameters
                for (auto& param : node->m_params)
                    evaluate_parameter_declaration(param);

                // use variable ref's
                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    symbol* s = eval_base::get().search(s_data_stack.top().id, param_ids[i].id);
                    s->var_ref = parameters[i];
                }

                eval_base::get().enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                {
                    evaluate_statement(stmt);
                    // return check
                    if (eval_base::get().return_called())
                    {
                        eval_base::get().call_return(false);
                        ref<variable_base> retval = eval_base::get().get_last_return_value();
                        eval_base::get().exit_statement_stack();
                        eval_base::get().exit_scope(s_data_stack.top().id);
                        return retval;
                    }
                }
                eval_base::get().exit_statement_stack();

                eval_base::get().exit_scope(s_data_stack.top().id);
                return create_null_variable();
            };

        if (add_overload)
        {
            result_fun_v->set_bounded_file_id(s_data_stack.top().id);
            result_fun_v->set_data(func_lambda);

            symbol function_symbol(result_fun_v, node->is_local(), node->is_global());
         
            std::dynamic_pointer_cast<overload_list>(is_valid_result.second->var_ref)->add(function_symbol);
            return is_valid_result.second->var_ref;
        }
        else if (declare)
        {
            result_fun_v->set_data(func_lambda);
            return overload->var_ref;
        }
        else
        {
            result_fun_v->set_bounded_file_id(s_data_stack.top().id);
            result_fun_v->set_data(func_lambda);

            ref<overload_list> result_olist = make_ref<overload_list>();
            result_olist->set_symbol_id(name);
            result_olist->add(symbol(result_fun_v, node->is_local(), node->is_global()));

            symbol olist_symbol(result_olist);

            if (node->is_global())
                eval_base::get().insert_to_global(s_data_stack.top().id, name, olist_symbol);
            else
                eval_base::get().insert(s_data_stack.top().id, name, olist_symbol);

            return result_olist;
        }
    }

    ref<variable_base> evaluator::evaluate_function_declaration(ref<function_declaration> node)
    {
       std::string name = node->m_id->m_token.value;
       
       // Parameter types and id's
       std::vector<function_param> parameters;

       for (auto& param : node->m_params)
           parameters.emplace_back(param->m_id->m_token.value, param->m_type, param->m_is_ref);
       
       auto is_valid_result = eval_base::get().is_function_valid(s_data_stack.top().id, name, parameters);

       if(!is_valid_result.first)
           throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());

       if (is_valid_result.second)
       {
           auto olist = std::dynamic_pointer_cast<overload_list>(is_valid_result.second->var_ref);
           if (!olist)
               throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());
       
           auto fun_v = make_ref<var_function>(parameters);
           fun_v->set_bounded_file_id(s_data_stack.top().id);
       
           olist->add(symbol(fun_v, node->is_local(), node->is_global()));
           return olist;
       }
       else
       {
           auto fun_v = make_ref<var_function>(parameters);
           fun_v->set_bounded_file_id(s_data_stack.top().id);
       
           ref<overload_list> result_olist = make_ref<overload_list>();
           result_olist->set_symbol_id(name);
           result_olist->add(symbol(fun_v, node->is_local(), node->is_global()));

           symbol result_symbol(result_olist, node->is_local(), node->is_global());
       
           if (node->is_global())
               eval_base::get().insert_to_global(s_data_stack.top().id, name, result_symbol);
           else
               eval_base::get().insert(s_data_stack.top().id, name, result_symbol);
           return result_olist;
       }
    }

    ref<variable_base> evaluator::evaluate_lambda_declaration(ref<lambda_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        ref<overload_list> result = nullptr;

        if (eval_base::get().search_current(s_data_stack.top().id, name))
            throw invalid_declaration_error("Duplicate variable declaration: " + name, node->m_id->get_location());

        if (node->m_initializer)
        {
            auto value = as_value(node->m_initializer);
            auto olist = std::dynamic_pointer_cast<overload_list>(value);

            if(!olist)
                throw invalid_declaration_error("Wrong initializer type!", node->m_id->get_location());

            olist->set_symbol_id(name);
            result = olist;
        }
        else
        {
            result = make_ref<overload_list>();
            result->set_symbol_id(name);
        }

        symbol result_symbol(result, node->m_flags.b2, node->m_flags.b3);

        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, result_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, result_symbol);
        return result;
    }

    ref<variable_base> evaluator::evaluate_enum_declaration(ref<enum_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search(s_data_stack.top().id, name))
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        ref<symbol_table> items = make_ref<symbol_table>();
        std::vector<string_t> id_list;
        id_list.reserve(node->m_items.size());

        integer_t last_integer_v = 0;

        for (auto& item : node->m_items)
        {
            string_t id = item.first->m_token.value;
            id_list.push_back(id);

            ref<variable> value = nullptr;

            if (item.second)
            {
                ref<variable_base> value_base = as_value(item.second);
                last_integer_v = builtin::helper::var_as_integer(value_base);
                value = make_ref<variable>(any(last_integer_v), variable_type::vt_integer);
            }
            else
            {
                value = make_ref<variable>(any(last_integer_v), variable_type::vt_integer);
            }

            items->insert(id, symbol(value));
            last_integer_v++;
        }

        ref<var_enum> enum_var = make_ref<var_enum>(id_list);
        enum_var->load_members(items);

        symbol enum_symbol(enum_var, node->m_is_local, node->m_is_global);
        if (node->m_is_global)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, enum_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, enum_symbol);
        return enum_var;
    }

    ref<variable_base> evaluator::evaluate_realm_declaration(ref<realm_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search(s_data_stack.top().id, name))
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        ref<realm> realm_var = make_ref<realm>();

        auto result = evaluate_block_statement(node->m_body);

        realm_var->load_members(result);
        symbol realm_symbol(realm_var, node->m_is_local, node->m_is_global);
        if (node->m_is_global)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, realm_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, realm_symbol);
        return realm_var;
    }

    ref<variable_base> evaluator::evaluate_omni_declaration(ref<omni_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search_current(s_data_stack.top().id, name))
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        ref<variable_base> result = create_null_variable();

        if (node->m_initializer)
        {
            auto value = as_value(node->m_initializer, nullptr, node->m_initializer->is_ref());

            switch (value->get_base_type())
            {
            case wio::variable_base_type::variable:
            case wio::variable_base_type::array:
            case wio::variable_base_type::dictionary:
            case wio::variable_base_type::unit_instance:
            {
                if (node->m_initializer->is_ref())
                {
                    result = value;
                }
                else
                {
                    helper::eval_omni_assignment(result, value);

                    if (node->m_flags.b1)
                        result->set_const(true);
                }
                break;
            }
            case wio::variable_base_type::function:
            {
                auto olist = std::dynamic_pointer_cast<overload_list>(value);

                olist->set_symbol_id(name);
                result = olist;
                
                break;
            }
            case wio::variable_base_type::realm:
            case wio::variable_base_type::omni:
            case wio::variable_base_type::enumeration:
            case wio::variable_base_type::unit:
            case wio::variable_base_type::none:
            default:
                throw invalid_type_error("Invalid type for omni!");
                break;
            }
        }

        result->set_omni(true);
        symbol symbol(result, node->m_flags.b2, node->m_flags.b3, node->m_initializer ? node->m_initializer->is_ref() : false);
        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, symbol);
        return result;
    }

    ref<variable_base> evaluator::evaluate_parameter_declaration(ref<parameter_declaration> node)
    {
        if (node->m_type == variable_base_type::variable)
            return evaluate_variable_declaration(make_ref<variable_declaration>(node->m_id, nullptr, false, false, false));
        else if (node->m_type == variable_base_type::array)
            return evaluate_array_declaration(make_ref<array_declaration>(node->m_id, nullptr, false, false, false));
        else if (node->m_type == variable_base_type::dictionary)
            return evaluate_dictionary_declaration(make_ref<dictionary_declaration>(node->m_id, nullptr, false, false, false));
        else if (node->m_type == variable_base_type::function)
            return evaluate_lambda_declaration(make_ref<lambda_declaration>(node->m_id, nullptr, false, false, false));
        else if (node->m_type == variable_base_type::omni)
            return evaluate_omni_declaration(make_ref<omni_declaration>(node->m_id, nullptr, false, false, false));
        else
            throw invalid_type_error("Undefined parameter type!", node->get_location());
    }

    ref<variable_base> evaluator::evaluate_unit_declaration(ref<unit_declaration> node)
    {
        std::string name = node->m_id->m_token.value;

        if (eval_base::get().search_current(s_data_stack.top().id, name))
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        std::vector<ref<unit>> parents;

        for (auto& item : node->m_parent_list)
        {
            auto value = as_value(item);

            if (value->get_base_type() != variable_base_type::unit)
                throw type_mismatch_error("Parent type should be unit!");

            ref<unit> parent = std::dynamic_pointer_cast<unit>(value);

            if(parent->is_final())
                throw invalid_declaration_error("Parent '" + parent->get_identifier() + "' marked as final!");

            parents.push_back(parent);
        }

        std::vector<ref<unit>> trust_list;

        for (auto& item : node->m_trust_list)
        {
            auto value = as_value(item);

            if (value->get_base_type() != variable_base_type::unit)
                throw type_mismatch_error("Trusted type should be unit!");

            trust_list.push_back(std::dynamic_pointer_cast<unit>(value));
        }

        ref<unit> unit_v = make_ref<unit>(name, parents, trust_list, (bool)node->m_flags.b1);

        switch (node->m_default_decl)
        {
        case wio::unit_access_type::exposed:
            unit_v->set_exposed(true);
            break;
        case wio::unit_access_type::hidden:
            unit_v->set_hidden(true);
            break;
        case wio::unit_access_type::shared:
            unit_v->set_shared(true);
            break;
        case wio::unit_access_type::none:
        default:
            throw exception("Unexpected error in unit!"); // Unreachable (should be)
            break;
        }

        eval_base::get().push_unit(unit_v);
        auto body_scope = evaluate_block_statement(node->m_body);
        eval_base::get().pop_unit();

        auto& symbols = body_scope->get_symbols();

        bool has_ctor = false;
        bool members_loaded = false;

        ref<symbol_table> members;

        for (auto& item : symbols)
        {
            if (token_constants::ctor_str == item.first)
                has_ctor = true;

            if (item.second.var_ref->is_outer())
            {
                if (!members_loaded)
                {
                    members = make_ref<symbol_table>();
                    unit_v->load_members(members);
                    members_loaded = true;
                }

                members->insert(item.first, symbol(item.second.var_ref));
            }
            else
            {
                unit_v->add_data(item.first, item.second.var_ref);
            }

        }

        if (!has_ctor)
        {
            unit_v->add_data(token_constants::ctor_str, builtin::loader::create_function(token_constants::ctor_str, 
                [](const std::vector<ref<variable_base>>&)->ref<variable_base> { return create_null_variable(); }, 
                builtin::detail::pa<0>{ }));
        }

        symbol unit_symbol(unit_v, node->m_flags.b2, node->m_flags.b3);

        if (node->m_flags.b3)
            eval_base::get().insert_to_global(s_data_stack.top().id, name, unit_symbol);
        else
            eval_base::get().insert(s_data_stack.top().id, name, unit_symbol);
        return unit_v;
    }

    ref<variable_base> evaluator::evaluate_unit_member_declaration(ref<unit_member_declaration> node)
    {
        auto value = evaluate_declaration_statement(node->m_decl_statement);

        value->set_outer(node->m_is_outer);

        switch (node->m_decl_type)
        {
        case wio::unit_access_type::exposed:
        {
            value->set_exposed(true);
            break;
        }
        case wio::unit_access_type::shared:
        {
            value->set_shared(true);
            break;
        }
        case wio::unit_access_type::hidden:
        {
            value->set_hidden(true);
            break;
        }
        case wio::unit_access_type::none:
        default:
        {
            if (eval_base::get().active_unit()->is_exposed())
                value->set_exposed(true);
            else if(eval_base::get().active_unit()->is_shared())
                value->set_shared(true);
            else 
                value->set_hidden(true);
        }
        } 

        return value;
    }

    ref<variable_base> evaluator::as_value(ref<expression> node, ref<variable_base> object, bool is_ref, uint32_t access_level)
    {
        if (is_ref && eval_base::get().is_ref_error_active())
        {
            if (auto bin_expr = std::dynamic_pointer_cast<binary_expression>(node))
                return evaluate_binary_expression(bin_expr, object, is_ref, access_level);
            else if (auto unary_expr = std::dynamic_pointer_cast<unary_expression>(node))
                return evaluate_unary_expression(unary_expr, object, is_ref, access_level);
            else if (auto id = std::dynamic_pointer_cast<identifier>(node))
                return evaluate_identifier(id, object, is_ref, access_level);
            else if (auto arr_access = std::dynamic_pointer_cast<array_access_expression>(node))
                return evaluate_array_access_expression(arr_access, object, is_ref, access_level);
            else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node))
                return evaluate_member_access_expression(member_access, object, is_ref, access_level);
            else if (auto func_call = std::dynamic_pointer_cast<function_call>(node))
                return evaluate_function_call(func_call, object, is_ref, access_level);
            else if (auto nui_call = std::dynamic_pointer_cast<new_unit_instance_call>(node))
                return evaluate_new_unit_instance_call(nui_call, object, is_ref, access_level);
            else
                throw invalid_type_error("Invalid ref call!");
        }
        else
        {
            if (auto lit = std::dynamic_pointer_cast<literal>(node))
                return evaluate_literal(lit);
            else if (auto str_lit = std::dynamic_pointer_cast<string_literal>(node))
                return evaluate_string_literal(str_lit);
            else if (auto arr_lit = std::dynamic_pointer_cast<array_literal>(node))
                return evaluate_array_literal(arr_lit);
            else if (auto dict_lit = std::dynamic_pointer_cast<dictionary_literal>(node))
                return evaluate_dictionary_literal(dict_lit);
            else if (auto lambda_lit = std::dynamic_pointer_cast<lambda_literal>(node))
                return evaluate_lambda_literal(lambda_lit);
            else if (auto null_exp = std::dynamic_pointer_cast<null_expression>(node))
                return create_null_variable();
            else if (auto typeof_expr = std::dynamic_pointer_cast<typeof_expression>(node))
                return evaluate_typeof_expression(typeof_expr);
            else if (auto bin_expr = std::dynamic_pointer_cast<binary_expression>(node))
                return evaluate_binary_expression(bin_expr, object, is_ref, access_level);
            else if (auto unary_expr = std::dynamic_pointer_cast<unary_expression>(node))
                return evaluate_unary_expression(unary_expr, object, is_ref, access_level);
            else if (auto id = std::dynamic_pointer_cast<identifier>(node))
                return evaluate_identifier(id, object, is_ref, access_level);
            else if (auto arr_access = std::dynamic_pointer_cast<array_access_expression>(node))
                return evaluate_array_access_expression(arr_access, object, is_ref, access_level);
            else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node))
                return evaluate_member_access_expression(member_access, object, is_ref, access_level);
            else if (auto func_call = std::dynamic_pointer_cast<function_call>(node))
                return evaluate_function_call(func_call, object, is_ref, access_level);
            else if (auto nui_call = std::dynamic_pointer_cast<new_unit_instance_call>(node))
                return evaluate_new_unit_instance_call(nui_call, object, is_ref, access_level);
            else
                return nullptr;
        }
    }
} // namespace wio