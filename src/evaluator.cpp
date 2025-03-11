// evaluator.cpp
#include "evaluator.h"
#include "exception.h"
#include "variables/variable.h" // Deðerleri temsil etmek için
#include "variables/array.h"
#include "variables/dictionary.h"
#include "variables/function.h"

#include <iostream>

namespace wio 
{
    evaluator::evaluator() 
    {
        current_scope = make_ref<scope>(scope_type::global);
    }

    void evaluator::evaluate_program(ref<program> program_node) 
    {
        enter_scope(scope_type::global);

        //preprocess_declarations(program_node);

        for (auto& stmt : program_node->m_statements) 
            evaluate_statement(stmt);

        exit_scope();
    }

    void evaluator::preprocess_declarations(ref<ast_node> node)
    {
        if (auto program_node = std::dynamic_pointer_cast<program>(node)) 
        {
            for (auto& stmt : program_node->m_statements)
            {
                if (auto decl = std::dynamic_pointer_cast<variable_declaration>(stmt))
                    evaluate_variable_declaration(decl);
                else if (auto decl = std::dynamic_pointer_cast<function_declaration>(stmt))
                    evaluate_function_declaration(decl);
                else if (auto decl = std::dynamic_pointer_cast<array_declaration>(stmt))
                    evaluate_array_declaration(decl);
                else if (auto decl = std::dynamic_pointer_cast<dictionary_declaration>(stmt)) 
                    evaluate_dictionary_declaration(decl);
                else if (auto import_stmt = std::dynamic_pointer_cast<import_statement>(stmt)) 
                    evaluate_import_statement(import_stmt);
            }
        }
        else if (auto func_decl = std::dynamic_pointer_cast<function_declaration>(node)) 
        {
            enter_scope(scope_type::function);
            if (func_decl->m_body != nullptr)
            {
                if (auto block_stmt = std::dynamic_pointer_cast<block_statement>(func_decl->m_body)) 
                {
                    for (auto& stmt : block_stmt->m_statements) 
                    {
                        if (auto decl = std::dynamic_pointer_cast<variable_declaration>(stmt)) 
                            evaluate_variable_declaration(decl);
                        else if (auto decl = std::dynamic_pointer_cast<function_declaration>(stmt)) 
                            evaluate_function_declaration(decl);
                        else if (auto decl = std::dynamic_pointer_cast<array_declaration>(stmt)) 
                            evaluate_array_declaration(decl);
                        else if (auto decl = std::dynamic_pointer_cast<dictionary_declaration>(stmt)) 
                            evaluate_dictionary_declaration(decl);
                    }
                }

            }
            exit_scope();
        }
    }

    ref<variable_base> evaluator::evaluate_expression(ref<expression> node)
    {
        return get_value(node);
    }

    void evaluator::evaluate_statement(ref<statement> node)
    {
        if (auto decl = std::dynamic_pointer_cast<variable_declaration>(node))
            evaluate_variable_declaration(decl);
        else if (auto decl = std::dynamic_pointer_cast<function_declaration>(node))
            evaluate_function_declaration(decl);
        else if (auto decl = std::dynamic_pointer_cast<array_declaration>(node))
            evaluate_array_declaration(decl);
        else if (auto decl = std::dynamic_pointer_cast<dictionary_declaration>(node))
            evaluate_dictionary_declaration(decl);
        else if (auto import_s = std::dynamic_pointer_cast<import_statement>(node))
            evaluate_import_statement(import_s);
        if (auto block_s = std::dynamic_pointer_cast<block_statement>(node))
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
    }

    ref<variable_base> evaluator::evaluate_literal(ref<literal> node)
    {
        if (node->m_token.type == token_type::number) 
        {
            if (node->get_expression_type() == variable_type::vt_integer)
                return make_ref<variable>(any(std::stoll(node->m_token.value)), variable_type::vt_integer);
            else if (node->get_expression_type() == variable_type::vt_float)
                return make_ref<variable>(any(std::stod(node->m_token.value)), variable_type::vt_float);
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

    ref<variable_base> evaluator::evaluate_identifier(ref<identifier> node)
    {
        symbol* sym = lookup(node->m_token.value);

        if (!sym) 
            throw undefined_identifier_error("Undefined identifier: " + node->m_token.value, node->get_location());

        if (sym->var_ref) 
        {
            if (auto var = std::dynamic_pointer_cast<variable>(sym->var_ref)) 
                return var;
            else if (auto arr = std::dynamic_pointer_cast<var_array>(sym->var_ref)) 
                return arr;
            else if (auto dict = std::dynamic_pointer_cast<var_dictionary>(sym->var_ref)) 
                return dict;
            //else if (auto func = std::dynamic_pointer_cast<function>(sym->var_ref)) 
            //    return func;
            else 
                throw evaluation_error("Identifier '" + node->m_token.value + "' refers to an unsupported type.", node->get_location());
        }
        else 
        {
            throw evaluation_error("Identifier '" + node->m_token.value + "' does not refer to a value.", node->get_location());
        }
    }

    ref<variable_base> evaluator::evaluate_binary_expression(ref<binary_expression> node) 
    {
        ref<variable_base> lv_ref = get_value(node->m_left);
        ref<variable_base> rv_ref = get_value(node->m_right);

        // TODO: array (+ operator)
        any left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
        any right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

        token op = node->m_operator;

        if (op.type == token_type::op) 
        {
            if (op.value == "+") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<long long>(left_value) + any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(double))
                    return make_ref<variable>(any(any_cast<long long>(left_value) + any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) + any_cast<long long>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(std::string) && right_value.type() == typeid(std::string)) 
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else if (left_value.type() == typeid(std::string) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + std::to_string(any_cast<long long>(right_value))), variable_type::vt_string);
                else if (left_value.type() == typeid(std::string) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<std::string>(left_value) + std::to_string(any_cast<double>(right_value))), variable_type::vt_string);
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(std::string)) 
                    return make_ref<variable>(any(std::to_string(any_cast<long long>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(std::string)) 
                    return make_ref<variable>(any(std::to_string(any_cast<double>(left_value)) + any_cast<std::string>(right_value)), variable_type::vt_string);
                else 
                    throw type_mismatch_error("Invalid operand types for '+' operator.", node->get_location());
            }
            else if (op.value == "-") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<long long>(left_value) - any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<long long>(left_value) - any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) - any_cast<long long>(right_value)), variable_type::vt_float);
                else 
                    throw type_mismatch_error("Invalid operand types for '-' operator.", node->get_location());
            }
            else if (op.value == "*") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<long long>(left_value) * any_cast<long long>(right_value)), variable_type::vt_integer);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(double)) 
                    return make_ref<variable>(any(any_cast<long long>(left_value) * any_cast<double>(right_value)), variable_type::vt_float);
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<double>(left_value) * any_cast<long long>(right_value)), variable_type::vt_float);
                else 
                    throw type_mismatch_error("Invalid operand types for '*' operator.", node->get_location());
            }
            else if (op.value == "/") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long)) 
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", node->get_location());
                    return make_ref<variable>(any(any_cast<long long>(left_value) / any_cast<long long>(right_value)), variable_type::vt_integer);
                }
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(double)) 
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", node->get_location());
                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(double)) 
                {
                    if (any_cast<double>(right_value) == 0.0)
                        throw invalid_operation_error("Division by zero error.", node->get_location());
                    return make_ref<variable>(any(any_cast<long long>(left_value) / any_cast<double>(right_value)), variable_type::vt_float);
                }
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(long long)) 
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", node->get_location());
                    return make_ref<variable>(any(any_cast<double>(left_value) / any_cast<long long>(right_value)), variable_type::vt_float);
                }
                else 
                {
                    throw type_mismatch_error("Invalid operand types for '/' operator.", node->get_location());
                }
            }
            else if (op.value == "%") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long))
                {
                    if (any_cast<long long>(right_value) == 0)
                        throw invalid_operation_error("Division by zero error.", node->get_location());
                    return make_ref<variable>(any(any_cast<long long>(left_value) % any_cast<long long>(right_value)), variable_type::vt_integer);
                }
                else
                {
                    throw type_mismatch_error("Invalid operand types for '%' operator.", node->get_location());
                }

            }
            else if (op.value == "<" || op.value == ">" || op.value == "<=" || op.value == ">=" || op.value == "==" || op.value == "!=") 
            {
                if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long))
                {
                    long long left = any_cast<long long>(left_value);
                    long long right = any_cast<long long>(right_value);
                    if (op.value == "<")
                        return make_ref<variable>(any(left < right), variable_type::vt_bool);
                    else if (op.value == ">")
                        return make_ref<variable>(any(left > right), variable_type::vt_bool);
                    else if (op.value == "<=")
                        return make_ref<variable>(any(left <= right), variable_type::vt_bool);
                    else if (op.value == ">=")
                        return make_ref<variable>(any(left >= right), variable_type::vt_bool);
                    else if (op.value == "==")
                        return make_ref<variable>(any(left == right), variable_type::vt_bool);
                    else if (op.value == "!=")
                        return make_ref<variable>(any(left != right), variable_type::vt_bool);
                }
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(double))
                {
                    double left = any_cast<double>(left_value);
                    double right = any_cast<double>(right_value);
                    if (op.value == "<")
                        return make_ref<variable>(any(left < right), variable_type::vt_bool);
                    else if (op.value == ">")
                        return make_ref<variable>(any(left > right), variable_type::vt_bool);
                    else if (op.value == "<=")
                        return make_ref<variable>(any(left <= right), variable_type::vt_bool);
                    else if (op.value == ">=")
                        return make_ref<variable>(any(left >= right), variable_type::vt_bool);
                    else if (op.value == "==")
                        return make_ref<variable>(any(left == right), variable_type::vt_bool);
                    else if (op.value == "!=")
                        return make_ref<variable>(any(left != right), variable_type::vt_bool);
                }
                else if (left_value.type() == typeid(long long) && right_value.type() == typeid(double))
                {
                    long long left = any_cast<long long>(left_value);
                    double right = any_cast<double>(right_value);
                    if (op.value == "<")
                        return make_ref<variable>(any(left < right), variable_type::vt_bool);
                    else if (op.value == ">")
                        return make_ref<variable>(any(left > right), variable_type::vt_bool);
                    else if (op.value == "<=")
                        return make_ref<variable>(any(left <= right), variable_type::vt_bool);
                    else if (op.value == ">=")
                        return make_ref<variable>(any(left >= right), variable_type::vt_bool);
                    else if (op.value == "==")
                        return make_ref<variable>(any(left == right), variable_type::vt_bool);
                    else if (op.value == "!=")
                        return make_ref<variable>(any(left != right), variable_type::vt_bool);
                }
                else if (left_value.type() == typeid(double) && right_value.type() == typeid(long long))
                {
                    double left = any_cast<double>(left_value);
                    long long right = any_cast<long long>(right_value);
                    if (op.value == "<")
                        return make_ref<variable>(any(left < right), variable_type::vt_bool);
                    else if (op.value == ">")
                        return make_ref<variable>(any(left > right), variable_type::vt_bool);
                    else if (op.value == "<=")
                        return make_ref<variable>(any(left <= right), variable_type::vt_bool);
                    else if (op.value == ">=")
                        return make_ref<variable>(any(left >= right), variable_type::vt_bool);
                    else if (op.value == "==")
                        return make_ref<variable>(any(left == right), variable_type::vt_bool);
                    else if (op.value == "!=")
                        return make_ref<variable>(any(left != right), variable_type::vt_bool);
                }
                else if (left_value.type() == typeid(std::string) && right_value.type() == typeid(std::string))
                {
                    std::string left = any_cast<std::string>(left_value);
                    std::string right = any_cast<std::string>(right_value);
                    if (op.value == "<")
                        return make_ref<variable>(any(left < right), variable_type::vt_bool);
                    else if (op.value == ">")
                        return make_ref<variable>(any(left > right), variable_type::vt_bool);
                    else if (op.value == "<=")
                        return make_ref<variable>(any(left <= right), variable_type::vt_bool);
                    else if (op.value == ">=")
                        return make_ref<variable>(any(left >= right), variable_type::vt_bool);
                    else if (op.value == "==")
                        return make_ref<variable>(any(left == right), variable_type::vt_bool);
                    else if (op.value == "!=")
                        return make_ref<variable>(any(left != right), variable_type::vt_bool);
                }
                else if (op.value == "&&") 
                { 
                    if (left_value.type() == typeid(bool) && right_value.type() == typeid(bool))
                        return make_ref<variable>(any(any_cast<bool>(left_value) && wio::any_cast<bool>(right_value)), variable_type::vt_bool);
                    else 
                        throw type_mismatch_error("Invalid operand types for '&&' operator (logical AND). Operands must be booleans.", node->get_location());
                }
                else if (op.value == "||") 
                {
                    if (left_value.type() == typeid(bool) && right_value.type() == typeid(bool))
                        return make_ref<variable>(any(any_cast<bool>(left_value) | wio::any_cast<bool>(right_value)), variable_type::vt_bool);
                    else 
                        throw type_mismatch_error("Invalid operand types for '||' operator (logical OR). Operands must be booleans.", node->get_location());
                }
                else
                {
                    throw type_mismatch_error("Invalid operand types for compare operator.", node->get_location());
                }

            }
        }
        else if (node->m_operator.type == token_type::bitwise_and)
        {
            if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long))
                return make_ref<variable>(any(any_cast<long long>(left_value) & any_cast<long long>(right_value)), variable_type::vt_integer);
            else
                throw type_mismatch_error("Invalid operand types for '&' operator (bitwise AND). Operands must be integers.", node->get_location());
        }
        else if (node->m_operator.type == token_type::bitwise_or)
        {
            if (left_value.type() == typeid(long long) && right_value.type() == typeid(long long))
                return make_ref<variable>(any(any_cast<long long>(left_value) || any_cast<long long>(right_value)), variable_type::vt_integer);
            else
                throw type_mismatch_error("Invalid operand types for '|' operator (bitwise OR). Operands must be integers.", node->get_location());
        }

        throw invalid_operator_error("Invalid binary operator: " + node->m_operator.value, node->get_location());
    }

    ref<variable_base> evaluator::evaluate_unary_expression(ref<unary_expression> node)
    {
        ref<variable_base> operand_ref = get_value(node->m_operand);
        any operand_value = std::dynamic_pointer_cast<variable>(operand_ref)->get_data();

        token op = node->m_operator;

        if (op.type == token_type::op) 
        {
            if (op.value == "+") 
            {
                if (operand_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(any_cast<long long>(operand_value)), variable_type::vt_integer);
                else if (operand_value.type() == typeid(double))
                    return make_ref<variable>(any(any_cast<double>(operand_value)), variable_type::vt_float);
                else 
                    throw invalid_type_error("Unary '+' operator requires a numeric operand.", node->get_location());
            }
            else if (op.value == "-") 
            {
                if (operand_value.type() == typeid(long long)) 
                    return make_ref<variable>(any(-any_cast<long long>(operand_value)), variable_type::vt_integer);
                else if (operand_value.type() == typeid(double)) 
                    return make_ref<variable>(any(-any_cast<double>(operand_value)), variable_type::vt_float);
                else 
                    throw invalid_type_error("Unary '-' operator requires a numeric operand.", node->get_location());
            }
            else if (op.value == "++" || op.value == "--") 
            {
                if (!(std::dynamic_pointer_cast<identifier>(node->m_operand) ||
                    std::dynamic_pointer_cast<array_access_expression>(node->m_operand) ||
                    std::dynamic_pointer_cast<dictionary_access_expression>(node->m_operand) ||
                    std::dynamic_pointer_cast<member_access_expression>(node->m_operand)))
                {
                    throw  invalid_operation_error("Invalid operand to increment/decrement operator.", node->get_location());
                }

                if (operand_value.type() == typeid(long long)) 
                {
                    long long value = wio::any_cast<long long>(operand_value);
                    long long old_value = value;

                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    if (auto id_node = std::dynamic_pointer_cast<identifier>(node->m_operand)) 
                    {
                        if (id_node->var_ref && id_node->var_ref->get_base_type() == variable_base_type::variable) 
                            std::dynamic_pointer_cast<variable>(id_node->var_ref)->get_data() = any(value);
                    }
                    else if (auto array_access = std::dynamic_pointer_cast<array_access_expression>(node->m_operand)) 
                    {
                        // TODO: 
                        throw invalid_operation_error("array access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto dict_access = std::dynamic_pointer_cast<dictionary_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("dict access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node->m_operand)) 
                    {
                        // TODO: 
                        throw invalid_operation_error("member access ++ and -- not supported(for now).", node->get_location());
                    }

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_integer);

                }
                else if (operand_value.type() == typeid(double)) 
                {
                    double value = wio::any_cast<double>(operand_value);
                    double old_value = value;
                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    if (auto id_node = std::dynamic_pointer_cast<identifier>(node->m_operand))
                    {
                        if (id_node->var_ref && id_node->var_ref->get_base_type() == variable_base_type::variable) 
                            std::dynamic_pointer_cast<variable>(id_node->var_ref)->get_data() = any(value);
                    }
                    else if (auto array_access = std::dynamic_pointer_cast<array_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("array access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto dict_access = std::dynamic_pointer_cast<dictionary_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("dict access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("member access ++ and -- not supported(for now).", node->get_location());
                    }

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_float);

                }
                else if (operand_value.type() == typeid(char)) 
                {
                    char value = wio::any_cast<char>(operand_value);
                    char old_value = value;
                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    if (auto id_node = std::dynamic_pointer_cast<identifier>(node->m_operand))
                    {
                        if (id_node->var_ref && id_node->var_ref->get_base_type() == variable_base_type::variable) 
                            std::dynamic_pointer_cast<variable>(id_node->var_ref)->get_data() = any(value);
                    }
                    else if (auto array_access = std::dynamic_pointer_cast<array_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("array access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto dict_access = std::dynamic_pointer_cast<dictionary_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("dict access ++ and -- not supported(for now).", node->get_location());
                    }
                    else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node->m_operand))
                    {
                        // TODO: 
                        throw invalid_operation_error("member access ++ and -- not supported(for now).", node->get_location());
                    }

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_character);
                }
                else
                {
                    throw invalid_type_error("Unary '++' and '--' operators require a numeric or character operand.", node->get_location());
                }
            }
        }
        else if (op.type == token_type::bang) 
        {
            if (operand_value.type() == typeid(bool)) 
                return make_ref<variable>(any(!wio::any_cast<bool>(operand_value)), variable_type::vt_bool);
            else 
                throw invalid_type_error("Unary '!' operator requires a boolean operand.", node->get_location());
        }
        else if (op.type == token_type::bitwise_not)
        {
            if (operand_value.type() == typeid(long long))
                return make_ref<variable>(any(~any_cast<long long>(operand_value)), variable_type::vt_integer);
            else
                throw invalid_type_error("Unary '~' operator requires an integer operand.", node->get_location());
        }
        else if (op.type == token_type::kw_typeof)
        {
            return make_ref<variable>(any(frenum::to_string(node->m_operand->get_expression_type())), variable_type::vt_string); // TODOOOOO
        }

        throw invalid_operator_error("Invalid unary operator: " + op.value, node->get_location());
    }

    ref<variable_base> evaluator::evaluate_assignment_expression(ref<assignment_expression> node)
    {
        ref<variable_base> lhs = get_value(node->m_target);
        ref<variable_base> rhs = get_value(node->m_value);
        ref<variable> variable_lhs = std::dynamic_pointer_cast<variable>(lhs);
        ref<variable> variable_rhs = std::dynamic_pointer_cast<variable>(rhs);
        any& lhs_value = variable_lhs->get_data();
        any& rhs_value = variable_rhs->get_data();
        token op = node->m_operator;

        if (op.value == "=")
        {
            variable_lhs->set_data(variable_rhs->get_data());
            variable_lhs->set_type(variable_rhs->get_type());
        }
        else if (op.value == "+=")
        {
            if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(long long))
                any_cast<long long&>(lhs_value) += any_cast<long long>(rhs_value);
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(double))
                any_cast<double&>(lhs_value) += any_cast<double>(rhs_value);
            else if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(double))
                any_cast<long long&>(lhs_value) += static_cast<long long>(any_cast<double>(rhs_value));
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(long long))
                any_cast<double&>(lhs_value) += any_cast<long long>(rhs_value);
            else if (lhs_value.type() == typeid(std::string) && rhs_value.type() == typeid(std::string))
                any_cast<std::string&>(lhs_value) += any_cast<std::string>(rhs_value);
            else if (lhs_value.type() == typeid(std::string) && rhs_value.type() == typeid(char))
                any_cast<std::string&>(lhs_value) += any_cast<char>(rhs_value);
            else if (lhs_value.type() == typeid(std::string) && rhs_value.type() == typeid(long long))
                any_cast<std::string&>(lhs_value) += std::to_string(any_cast<long long>(rhs_value));
            else if (lhs_value.type() == typeid(std::string) && rhs_value.type() == typeid(double))
                any_cast<std::string&>(lhs_value) += std::to_string(any_cast<double>(rhs_value));
            else
                throw invalid_type_error("Invalid addition assignment: (Type '" + frenum::to_string(variable_rhs->get_type()) + "' cannot be added to type '" + frenum::to_string(variable_lhs->get_type()) + "')", node->get_location());
        }
        else if (op.value == "-=")
        {
            if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(long long))
                any_cast<long long&>(lhs_value) -= any_cast<long long>(rhs_value);
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(double))
                any_cast<double&>(lhs_value) -= any_cast<double>(rhs_value);
            else if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(double))
                any_cast<long long&>(lhs_value) -= static_cast<long long>(any_cast<double>(rhs_value));
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(long long))
                any_cast<double&>(lhs_value) -= any_cast<long long>(rhs_value);
            else
                throw invalid_type_error("Invalid subtraction assignment: (Type '" + frenum::to_string(variable_rhs->get_type()) + "' cannot be subtract from type '" + frenum::to_string(variable_lhs->get_type()) + "')", node->get_location());
        }
        else if (op.value == "*=")
        {
            if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(long long))
                any_cast<long long&>(lhs_value) *= any_cast<long long>(rhs_value);
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(double))
                any_cast<double&>(lhs_value) *= any_cast<double>(rhs_value);
            else if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(double))
                any_cast<long long&>(lhs_value) *= static_cast<long long>(any_cast<double>(rhs_value));
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(long long))
                any_cast<double&>(lhs_value) *= any_cast<long long>(rhs_value);
            else
                throw invalid_type_error("Invalid multiplication assignment: (Type '" + frenum::to_string(variable_rhs->get_type()) + "' cannot be multiplied by type '" + frenum::to_string(variable_lhs->get_type()) + "')", node->get_location());
        }
        else if (op.value == "/=")
        {
            if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(long long))
                any_cast<long long&>(lhs_value) /= any_cast<long long>(rhs_value);
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(double))
                any_cast<double&>(lhs_value) /= any_cast<double>(rhs_value);
            else if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(double))
                any_cast<long long&>(lhs_value) /= static_cast<long long>(any_cast<double>(rhs_value));
            else if (lhs_value.type() == typeid(double) && rhs_value.type() == typeid(long long))
                any_cast<double&>(lhs_value) /= any_cast<long long>(rhs_value);
            else
                throw invalid_type_error("Invalid division assignment: (Type '" + frenum::to_string(variable_rhs->get_type()) + "' cannot be divided by type '" + frenum::to_string(variable_lhs->get_type()) + "')", node->get_location());
        }
        else if (op.value == "%=")
        {
            if (lhs_value.type() == typeid(long long) && rhs_value.type() == typeid(long long))
                any_cast<long long&>(lhs_value) %= any_cast<long long>(rhs_value);
            else
                throw invalid_type_error("Invalid modulo assignment: Types should be integer!", node->get_location());
        }
        else
            throw invalid_operator_error("Invalid assignment error!", node->get_location());
        return lhs;
    }

    ref<variable_base> evaluator::evaluate_typeof_expression(ref<typeof_expression> node)
    {
        ref<variable_base> value_base = evaluate_expression(node->m_expr);
        return make_ref<variable>(frenum::to_string(value_base->get_type()), variable_type::vt_string);
    }

    void evaluator::evaluate_expression_statement(ref<expression_statement> node)
    {
        evaluate_expression(node->m_expression);
    }

    void evaluator::evaluate_variable_declaration(ref<variable_declaration> node) 
    {
        std::string name = node->m_id->m_token.value;
        variable_type type = node->m_type;

        if (lookup(name) != nullptr) 
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

        any initializer_value;

        if (node->m_initializer)
        {
            initializer_value = std::dynamic_pointer_cast<variable>(evaluate_expression(node->m_initializer))->get_data();

            if (node->m_type != variable_type::vt_any) 
            {
                auto initializer_expression_type = node->m_initializer->get_expression_type();
                if (initializer_expression_type != node->m_type)
                    throw local_exception("initializer has an invalid type.", node->m_initializer->get_location());
            }
            
            node->m_id->var_ref = std::make_shared<variable>(initializer_value, type, node->m_is_const, node->m_is_local, node->m_is_global);
        }

        symbol symbol(name, type, current_scope->get_type(), node->m_is_const, node->m_id->var_ref);
        current_scope->insert(name, symbol);
    }

    void evaluator::evaluate_function_declaration(ref<function_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        variable_type return_type = node->m_return_type;

        if (lookup(name) != nullptr) 
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());

        symbol function_symbol(name, variable_type::vt_function, current_scope->get_type(), false);
        current_scope->insert(name, function_symbol);

        std::vector<variable_type> param_types;
        for (auto& param : node->m_params) 
            param_types.push_back(param->m_type);

        //auto func = make_ref<wio::function>(name, param_types, return_type);
        //node->m_id->var_ref = func;

        current_function = node;
        enter_scope(scope_type::function);

        for (ref<variable_declaration>& param : node->m_params)
        {
            std::string param_name = param->m_id->m_token.value;
            variable_type param_type = param->m_type;
            symbol param_symbol(param_name, param_type, scope_type::function, param->m_is_const);

            if (lookup(param_name) != nullptr)
            {
                throw invalid_declaration_error("Duplicate parameter name: " + param_name, param->m_id->get_location());
                exit_scope();
                current_function = nullptr;
                return;
            }
            current_scope->insert(param_name, param_symbol);

            param->m_id->var_ref = std::make_shared<variable>(any(), param_type, false);
        }

        //preprocess_declarations(node);

        exit_scope();
        current_function = nullptr;
    }

    void evaluator::evaluate_array_declaration(ref<array_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        if (lookup(name) != nullptr)
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());
        if (current_scope->get_type() == scope_type::function && (node->m_is_local || node->m_is_global)) 
            throw invalid_declaration_error("Arrays declared in function parameters cannot be local or global.", node->m_id->get_location());

        std::vector<ref<variable_base>> elements;

        for (auto& item : node->m_elements)
            elements.push_back(evaluate_expression(item));

       node->m_id->var_ref = make_ref<var_array>(elements, node->m_is_const, node->m_is_local, node->m_is_global);
       symbol array_symbol(name, variable_type::vt_array, current_scope->get_type(), node->m_is_const, node->m_id->var_ref);
       current_scope->insert(name, array_symbol);
    }

    void evaluator::evaluate_dictionary_declaration(ref<dictionary_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        if (lookup(name) != nullptr)
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());
        if (current_scope->get_type() == scope_type::function && (node->m_is_local || node->m_is_global))
            throw invalid_declaration_error("Dictionaries declared in function parameters cannot be local or global.", node->m_id->get_location());

        var_dictionary::map_t elements;

        for (auto& item : node->m_pairs)
            elements[var_dictionary::as_key(evaluate_expression(item.first))] = evaluate_expression(item.second);
            
        node->m_id->var_ref = make_ref<var_dictionary>(elements, node->m_is_const, node->m_is_local, node->m_is_global);
        symbol dict_symbol(name, variable_type::vt_dictionary, current_scope->get_type(), node->m_is_const, node->m_id->var_ref);
        current_scope->insert(name, dict_symbol);
    }

    void evaluator::evaluate_import_statement(ref<import_statement> node)
    {
    }

    void evaluator::enter_scope(scope_type type) 
    {
        current_scope = make_ref<scope>(type, current_scope);
    }

    void evaluator::exit_scope() 
    {
        if (current_scope->get_type() == scope_type::global)
            return;
        if (current_scope->get_parent()) 
            current_scope = current_scope->get_parent();
    }

    symbol* evaluator::lookup(const std::string& name) 
    {
        return current_scope->lookup(name);
    }

    ref<variable_base> evaluator::get_value(ref<expression> node)
    {
        if (auto lit = std::dynamic_pointer_cast<literal>(node))
            return evaluate_literal(lit);
        else if (auto str_lit = std::dynamic_pointer_cast<string_literal>(node))
            return evaluate_string_literal(str_lit);
        else if (auto id = std::dynamic_pointer_cast<identifier>(node))
            return evaluate_identifier(id);
        else if (auto null_exp = std::dynamic_pointer_cast<null_expression>(node))
            return make_ref<variable>(any(), variable_type::vt_null);
        else if (auto bin_expr = std::dynamic_pointer_cast<binary_expression>(node)) 
            return evaluate_binary_expression(bin_expr);
        else if (auto unary_expr = std::dynamic_pointer_cast<unary_expression>(node)) 
            return evaluate_unary_expression(unary_expr);
        else if (auto assign_expr = std::dynamic_pointer_cast<assignment_expression>(node)) 
            return evaluate_assignment_expression(assign_expr);
        else if (auto typeof_expr = std::dynamic_pointer_cast<typeof_expression>(node)) 
            return evaluate_typeof_expression(typeof_expr);
        else if (auto arr_access = std::dynamic_pointer_cast<array_access_expression>(node)) 
            return evaluate_array_access_expression(arr_access);
        else if (auto dict_access = std::dynamic_pointer_cast<dictionary_access_expression>(node)) 
            return evaluate_dictionary_access_expression(dict_access);
        else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node)) 
            return evaluate_member_access_expression(member_access);
        else if (auto func_call = std::dynamic_pointer_cast<function_call>(node)) 
            return evaluate_function_call(func_call);
        else 
            throw evaluation_error("Cannot evaluate expression of unknown type.");
    }

    variable_type evaluator::get_expression_type(ref<expression> node) 
    {
        if (auto lit = std::dynamic_pointer_cast<literal>(node))
            return lit->get_expression_type();
        else if (auto str_lit = std::dynamic_pointer_cast<string_literal>(node))
            return str_lit->get_expression_type();
        else if (auto id = std::dynamic_pointer_cast<identifier>(node))
            return id->get_expression_type();
        else if (auto bin_expr = std::dynamic_pointer_cast<binary_expression>(node))
            bin_expr->get_expression_type();
        else if (auto unary_expr = std::dynamic_pointer_cast<unary_expression>(node))
            return unary_expr->get_expression_type();
        else if (auto assign_expr = std::dynamic_pointer_cast<assignment_expression>(node))
            return assign_expr->get_expression_type();
        else if (auto typeof_expr = std::dynamic_pointer_cast<typeof_expression>(node))
            return typeof_expr->get_expression_type();
        else if (auto arr_access = std::dynamic_pointer_cast<array_access_expression>(node)) 
            return arr_access->get_expression_type();
        else if (auto dict_access = std::dynamic_pointer_cast<dictionary_access_expression>(node))
            return dict_access->get_expression_type();
        else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node)) 
            member_access->get_expression_type();
        else if (auto func_call = std::dynamic_pointer_cast<function_call>(node)) 
            return func_call->get_expression_type();
        throw evaluation_error("Cannot get expression type of unknown type.", node->get_location());
    }
    
} // namespace wio