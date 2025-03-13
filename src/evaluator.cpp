#include "evaluator.h"
#include "exception.h"
#include "lexer.h"
#include "parser.h"
#include "utils/filesystem.h"

#include "variables/variable.h"
#include "variables/array.h"
#include "variables/dictionary.h"
#include "variables/function.h"

#include "builtin.h"

#include <filesystem>

namespace wio 
{
    evaluator::evaluator() 
    {
        current_scope = builtin::load();
    }

    void evaluator::evaluate_program(ref<program> program_node) 
    {
        enter_scope(scope_type::global);

        for (auto& stmt : program_node->m_statements) 
            evaluate_statement(stmt);

        exit_scope();
    }

    std::map<std::string, symbol>& evaluator::get_symbols()
    {
        return current_scope->get_symbols();
    }

    ref<variable_base> evaluator::evaluate_expression(ref<expression> node)
    {
        return get_value(node);
    }

    void evaluator::evaluate_statement(ref<statement> node)
    {
        if (m_call_states.b1 || m_call_states.b2 || m_call_states.b3)
            return;

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
        if (auto break_s = std::dynamic_pointer_cast<break_statement>(node))
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
                return make_ref<variable>(any(char_reference(node->m_token.value[0])), variable_type::vt_character);
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
            {
                if(sym->var_ref->is_ref() || node->m_is_lhs)
                    return var;
                return var->clone();
            }
            else if (auto arr = std::dynamic_pointer_cast<var_array>(sym->var_ref))
            {
                if (sym->var_ref->is_ref() || node->m_is_lhs)
                    return arr;
                return arr->clone();
            }
            else if (auto dict = std::dynamic_pointer_cast<var_dictionary>(sym->var_ref))
            {
                if (sym->var_ref->is_ref() || node->m_is_lhs)
                    return dict;
                return dict->clone();
            }
            //else if (auto func = std::dynamic_pointer_cast<function>(sym->var_ref)) 
            //    return func;
            else 
                throw evaluation_error("Identifier '" + node->m_token.value + "' refers to an unsupported type.", node->get_location());
        }
        return get_null_var();
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
                else if (op.value == "==")
                {
                    // NOT COMPAREABLE PROBABLY
                    if (lv_ref->get_type() == rv_ref->get_type() && lv_ref->get_type() == variable_type::vt_null)
                        return make_ref<variable>(any(true), variable_type::vt_bool);
                    return make_ref<variable>(any(false), variable_type::vt_bool);
                }
                else if (op.value == "!=")
                {
                    // NOT COMPAREABLE PROBABLY
                    if (lv_ref->get_type() == rv_ref->get_type() && lv_ref->get_type() == variable_type::vt_null)
                        return make_ref<variable>(any(false), variable_type::vt_bool);
                    return make_ref<variable>(any(true), variable_type::vt_bool);

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
        any& operand_value = std::dynamic_pointer_cast<variable>(operand_ref)->get_data();

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
                    std::dynamic_pointer_cast<member_access_expression>(node->m_operand)))
                    throw  invalid_operation_error("Invalid operand to increment/decrement operator.", node->get_location());
                if (operand_ref->is_constant())
                    throw constant_value_assignment_error("Constant values cannot be changed!", node->get_location());

                if (operand_value.type() == typeid(long long)) 
                {
                    long long& value = any_cast<long long&>(operand_value);
                    long long old_value = value;

                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_integer);

                }
                else if (operand_value.type() == typeid(double)) 
                {
                    double& value = any_cast<double&>(operand_value);
                    double old_value = value;
                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_float);

                }
                else if (operand_value.type() == typeid(char_reference)) 
                {
                    char_reference& value = any_cast<char_reference&>(operand_value);
                    char old_value = value;
                    if (op.value == "++") 
                        value.get()++;
                    else 
                        value.get()--;

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : char_reference(old_value)), variable_type::vt_character);
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
        else if (op.type == token_type::kw_typeof) // WE DONT NEED THIS PROBABLY
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

        if (variable_lhs->is_constant())
            throw constant_value_assignment_error("Constant values cannot be changed!", node->get_location());

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
            else if (lhs_value.type() == typeid(std::string) && rhs_value.type() == typeid(char_reference))
                any_cast<std::string&>(lhs_value) += any_cast<char_reference>(rhs_value).get();
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
        return lhs->clone();
    }

    ref<variable_base> evaluator::evaluate_typeof_expression(ref<typeof_expression> node)
    {
        ref<variable_base> value_base = evaluate_expression(node->m_expr);
        return make_ref<variable>(frenum::to_string(value_base->get_type()), variable_type::vt_string);
    }

    ref<variable_base> evaluator::evaluate_array_access_expression(ref<array_access_expression> node)
    {
        symbol* sym = lookup(node->m_array->m_token.value);

        if(!sym)
            throw local_exception("Variable '" + node->m_array->m_token.value + "' is not exists", node->m_array->get_location());

        variable_base_type base_t = sym->var_ref->get_base_type();

        if (base_t == variable_base_type::array)
        {
            ref<variable_base> index_base = evaluate_expression(node->m_key_or_index);
            if (index_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> index_var = std::dynamic_pointer_cast<variable>(index_base);
            if (index_var->get_type() != variable_type::vt_integer)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<var_array> array_var = std::dynamic_pointer_cast<var_array>(sym->var_ref);

            if (node->m_array->m_is_ref || node->m_array->m_is_lhs)
            {
                auto var = array_var->get_element(index_var->get_data_as<long long>());
                return var;
            }
            return array_var->get_element(index_var->get_data_as<long long>())->clone();
        }
        else if (base_t == variable_base_type::variable)
        {
            if(sym->var_ref->get_type() != variable_type::vt_string)
            throw invalid_type_error("Type should be array or string or dictionary!", node->m_array->get_location());

            ref<variable_base> index_base = evaluate_expression(node->m_key_or_index);
            if (index_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> index_var = std::dynamic_pointer_cast<variable>(index_base);
            if (index_var->get_type() != variable_type::vt_integer)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> str_var = std::dynamic_pointer_cast<variable>(sym->var_ref);
            std::string& value = str_var->get_data_as<std::string>();
            long long index = index_var->get_data_as<long long>();
            if (index < 0 || index >= long long(value.size()))
                throw out_of_bounds_error("String index out of the bounds!");

            char_reference ch = (node->m_array->m_is_ref || node->m_array->m_is_lhs) ? char_reference(value, index) : char_reference(value[index]);

            return make_ref<variable>(any(ch), variable_type::vt_character);
        }
        else if (base_t == variable_base_type::dictionary)
        {
            ref<variable_base> key_base = evaluate_expression(node->m_key_or_index);
            if (key_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be variable!", node->m_key_or_index->get_location());

            ref<var_dictionary> dict_var = std::dynamic_pointer_cast<var_dictionary>(sym->var_ref);

            if (node->m_array->m_is_ref || node->m_array->m_is_lhs)
            {
                auto var = dict_var->get_element(key_base);
                return var;
            }
            return dict_var->get_element(key_base)->clone();
        }
        else
        {
            throw invalid_type_error("Type should be array or string or dictionary!", node->m_array->get_location());
        }
    }

    ref<variable_base> evaluator::evaluate_function_call(ref<function_call> node)
    {
        ref<identifier> id = std::dynamic_pointer_cast<identifier>(node->m_caller);
        std::string name = id->m_token.value;
        symbol* sym = lookup(name);

        if(!sym)
            throw local_exception("Function '" + name + "' not found!", id->get_location());
        if(sym->type != variable_type::vt_function)
           throw invalid_type_error("Type should be function!", id->get_location());

        ref<var_function> func = std::dynamic_pointer_cast<var_function>(sym->var_ref);

        if(!func)
           throw undefined_identifier_error("Invalid function!", id->get_location());
        if(node->m_arguments.size() != func->parameter_count())
           throw invalid_argument_count_error("Invalid parameter count!", id->get_location());

        std::vector<ref<variable_base>> parameters;
        for (auto& arg : node->m_arguments)
            parameters.push_back(get_value(arg));

        const auto& real_params = func->get_parameters();
        for (size_t i = 0; i < real_params.size(); ++i)
            if (parameters[i]->get_type() != real_params[i].type && real_params[i].type != variable_type::vt_any)
                throw type_mismatch_error("Parameter types not matching!");

        return func->call(parameters);
    }

    void evaluator::evaluate_block_statement(ref<block_statement> node)
    {
        enter_scope(scope_type::block);
        for (auto& stmt : node->m_statements)
            evaluate_statement(stmt);
        exit_scope();
    }

    void evaluator::evaluate_expression_statement(ref<expression_statement> node)
    {
        evaluate_expression(node->m_expression);
    }

    void evaluator::evaluate_if_statement(ref<if_statement> node)
    {
        ref<variable_base> cond = get_value(node->m_condition);
        
        if (cond->get_base_type() != variable_base_type::variable)
            throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

        ref<variable> var = std::dynamic_pointer_cast<variable>(cond);

        if (cond->get_type() == variable_type::vt_bool)
        {
            bool value = var->get_data_as<bool>();
            if (value)
                evaluate_statement(node->m_then_branch);
            else
                evaluate_statement(node->m_else_branch);
        }
        else if (cond->get_type() == variable_type::vt_integer)
        {
            long long value = var->get_data_as<long long>();
            if (value)
                evaluate_statement(node->m_then_branch);
            else
                evaluate_statement(node->m_else_branch);
        }
        else if (cond->get_type() == variable_type::vt_float)
        {
            double value = var->get_data_as<double>();
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
        enter_scope(scope_type::block);
        evaluate_statement(node->m_initialization);

        m_loop_depth++;
        while(true)
        {
            ref<variable_base> cond = get_value(node->m_condition);
            if (cond->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

            ref<variable> var = std::dynamic_pointer_cast<variable>(cond);
            if (var->get_type() != variable_type::vt_bool)
                throw invalid_type_error("Type should be boolean!", node->m_condition->get_location());

            bool value = var->get_data_as<bool>();
            if (!value)
            {
                m_call_states.b1 = false;
                m_call_states.b2 = false;
                break;
            }

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            if (m_call_states.b1)
            {
                m_call_states.b1 = false;
                break;
            }
            else if (m_call_states.b2)
            {
                evaluate_expression(node->m_increment);
                m_call_states.b2 = false;
            }
        }
        m_loop_depth--;

        exit_scope();
    }

    void evaluator::evaluate_foreach_statement(ref<foreach_statement> node)
    {
        enter_scope(scope_type::block);
        m_loop_depth++;
        evaluate_variable_declaration(make_ref<variable_declaration>(node->m_item, nullptr, false, false, false, false));
        ref<variable_base> col_base = evaluate_expression(node->m_collection);
        if (col_base->get_base_type() == variable_base_type::array)
        {
            ref<var_array> array = std::dynamic_pointer_cast<var_array>(col_base);
            for (size_t i = 0; i < array->size(); ++i)
            {
                symbol* sym = lookup(node->m_item->m_token.value);
                sym->var_ref = array->get_element(i);

                for (auto& stmt : node->m_body->m_statements)
                    evaluate_statement(stmt);

                if (m_call_states.b1)
                {
                    m_call_states.b1 = false;
                    break;
                }
                else if (m_call_states.b2)
                {
                    m_call_states.b2 = false;
                }
            }
        }
        else if (col_base->get_base_type() == variable_base_type::array)
        {
            // TODO
        }
        else if (col_base->get_base_type() == variable_base_type::array)
        {
            // TODO
        }
        else
        {
            throw type_mismatch_error("Type should be array or dict or string", node->m_collection->get_location());
        }

        m_loop_depth--;
        exit_scope();
    }

    void evaluator::evaluate_while_statement(ref<while_statement> node)
    {
        enter_scope(scope_type::block);
        m_loop_depth++;
        while (true)
        {
            ref<variable_base> cond = get_value(node->m_condition);
            if (cond->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Type should be variable!", node->m_condition->get_location());

            ref<variable> var = std::dynamic_pointer_cast<variable>(cond);
            if (var->get_type() != variable_type::vt_bool)
                throw invalid_type_error("Type should be boolean!", node->m_condition->get_location());

            bool value = var->get_data_as<bool>();
            if (!value)
            {
                m_call_states.b1 = false;
                m_call_states.b2 = false;
                break;
            }

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            if (m_call_states.b1)
            {
                m_call_states.b1 = false;
                break;
            }
            else if (m_call_states.b2)
            {
                m_call_states.b2 = false;
            }
        }
        m_loop_depth--;

        exit_scope();
    }

    void evaluator::evaluate_break_statement(ref<break_statement> node)
    {
        if (!m_loop_depth)
            throw invalid_break_statement("Break statement called out of the loop!", node->m_loc);
        m_call_states.b1 = true;
    }

    void evaluator::evaluate_continue_statement(ref<continue_statement> node)
    {
        if (!m_loop_depth)
            throw invalid_continue_statement("Continue statement called out of the loop!", node->m_loc);
        m_call_states.b1 = true;
    }

    void evaluator::evaluate_import_statement(ref<import_statement> node)
    {
        std::string filepath = node->m_module_path;
        if (!check_extension(filepath, ".wio"))
        {
            if(std::filesystem::path(filepath).has_extension())
                throw file_error("\'" + filepath + "\' is not a wio file!");
            if (has_prefix(filepath, "wio."))
            {
                // BUILTINS
            }
            else
            {
                filepath += ".wio";
            }
        }
        if (!file_exists(filepath))
        {
            throw file_error("\'" + filepath + "\' not exists!");
        }

        std::string content = read_file(filepath);
        std::filesystem::path cp = std::filesystem::current_path();

        std::filesystem::path temp_cp = std::filesystem::current_path();
        temp_cp /= filepath;
        std::filesystem::path rd = temp_cp.parent_path();
        std::filesystem::current_path(rd);

        lexer l_lexer(content);
        auto& tokens = l_lexer.get_tokens();
        parser l_parser(tokens);
        ref<program> tree = l_parser.parse();
        evaluator eval;
        eval.evaluate_program(tree);
        auto& symbols = eval.get_symbols();

        for (auto& [key, value] : symbols)
        {
            if (value.flags.b1)
                continue;
            current_scope->insert(key, value);
        }

        std::filesystem::current_path(cp);
    }

    void evaluator::evaluate_return_statement(ref<return_statement> node)
    {
        if (current_scope->get_type() == scope_type::function)
        {
            m_last_return_value = evaluate_expression(node->m_value);
            m_call_states.b3 = true;
        }
        else
            throw invalid_return_statement("Return statement out of the function", node->get_location());
    }

    void evaluator::evaluate_variable_declaration(ref<variable_declaration> node, bool is_parameter)
    {
        std::string name = node->m_id->m_token.value;
        variable_type type = node->m_type;

        if (is_parameter)
        {
            if (lookup_current_and_global(name) != nullptr)
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
            if (node->m_type == variable_type::vt_array)
                evaluate_array_declaration(make_ref<array_declaration>(node->m_id, std::vector<ref<expression>>{}, false, false, false));
            if (node->m_type == variable_type::vt_dictionary)
                evaluate_dictionary_declaration(make_ref<dictionary_declaration>(node->m_id, std::vector<std::pair<ref<expression>, ref<expression>>>{}, false, false, false));
        }
        else
        {
            if (lookup(name) != nullptr)
            {
                if (node->m_flags.b3)
                    return; // GLOBAL VALUE DETECTED -> not works fine TODOOOOO
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
            }
        }


        if (node->m_initializer)
        {
            auto var = std::dynamic_pointer_cast<variable>(evaluate_expression(node->m_initializer));

            if (node->m_type != variable_type::vt_var_param)
            {
                auto initializer_expression_type = node->m_initializer->get_expression_type();
                if (initializer_expression_type != node->m_type || !var)
                    throw local_exception("initializer has an invalid type.", node->m_initializer->get_location());
            }

            node->m_id->var_ref = var;
            if (!var->is_ref())
            {
                node->m_id->var_ref->set_flags({ node->m_flags.b2, node->m_flags.b3 });
            }
            else
            {
                if (node->m_flags.b1)
                    node->m_id->var_ref->set_const(true);
            }
        }

        symbol symbol(name, type, current_scope->get_type(), node->m_id->var_ref, {node->m_flags.b2, node->m_flags.b3});
        if (node->m_flags.b3)
            current_scope->insert_to_global(name, symbol);
        else
            current_scope->insert(name, symbol);
    }

    void evaluator::evaluate_array_declaration(ref<array_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        if (lookup(name) != nullptr)
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());

        std::vector<ref<variable_base>> elements;

        for (auto& item : node->m_elements)
        {
            ref<variable_base> var = evaluate_expression(item)->clone();
            var->set_const(node->m_flags.b1);
            elements.push_back(var);
        }

        node->m_id->var_ref = make_ref<var_array>(elements, packed_bool{ node->m_flags.b1, node->m_flags.b4 });
        symbol array_symbol(name, variable_type::vt_array, current_scope->get_type(), node->m_id->var_ref, { node->m_flags.b2, node->m_flags.b3 });
        if (node->m_flags.b3)
            current_scope->insert_to_global(name, array_symbol);
        else
            current_scope->insert(name, array_symbol);
    }

    void evaluator::evaluate_dictionary_declaration(ref<dictionary_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        if (lookup(name) != nullptr)
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());

        var_dictionary::map_t elements;

        for (auto& item : node->m_pairs)
        {
            ref<variable_base> var = evaluate_expression(item.second)->clone();
            var->set_const(node->m_flags.b1);
            elements[var_dictionary::as_key(evaluate_expression(item.first))] = var;
        }

        node->m_id->var_ref = make_ref<var_dictionary>(elements, packed_bool{ node->m_flags.b1, node->m_flags.b4 });
        symbol dict_symbol(name, variable_type::vt_dictionary, current_scope->get_type(), node->m_id->var_ref, { node->m_flags.b2, node->m_flags.b3 });
        if (node->m_flags.b3)
            current_scope->insert_to_global(name, dict_symbol);
        else
            current_scope->insert(name, dict_symbol);
    }

    void evaluator::evaluate_function_declaration(ref<function_declaration> node)
    {
        std::string name = node->m_id->m_token.value;
        variable_type return_type = node->m_return_type;

        std::vector<function_param> param_types;

        for (auto& param : node->m_params)
            param_types.emplace_back(param->m_type, param->m_id->m_token.value);

        if (lookup(name) != nullptr)
            throw invalid_declaration_error("Symbol '" + name + "' already defined in this scope.", node->m_id->get_location());

        for (auto& stmt : node->m_body->m_statements)
            if (auto var_decl = std::dynamic_pointer_cast<variable_declaration>(stmt))
                if (var_decl->m_flags.b3)
                    evaluate_variable_declaration(var_decl);

        auto func_lambda = [this, node](const std::vector<function_param>& param_ids, std::vector<ref<variable_base>>& parameters)->ref<variable_base>
            {
                enter_scope(scope_type::function);
                for (auto& param : node->m_params)
                {
                    if (param->m_type == variable_type::vt_var_param)
                        evaluate_variable_declaration(param, true);
                }

                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    symbol* sym = lookup(param_ids[i].id);
                    sym->var_ref = parameters[i];
                }

                for (auto& stmt : node->m_body->m_statements)
                {
                    evaluate_statement(stmt);
                    if (m_call_states.b3)
                    {
                        m_call_states.b3 = false;
                        ref<variable_base> retval = m_last_return_value;
                        m_last_return_value = nullptr;
                        exit_scope();
                        return retval;
                    }
                }

                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    symbol* sym = lookup(param_ids[i].id);
                    sym->var_ref = nullptr;
                }

                return get_null_var();
                exit_scope();
            };


        ref<var_function> func = make_ref<var_function>(func_lambda, variable_type::vt_null, param_types, node->m_is_local);
        symbol function_symbol(name, variable_type::vt_function, current_scope->get_type(), func, { node->m_is_local, node->m_is_global });
        current_scope->insert(name, function_symbol);
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

    symbol* evaluator::lookup_current_and_global(const std::string& name)
    {
        return current_scope->lookup_current_and_global(name);
    }

    ref<variable_base> evaluator::get_null_var()
    {
        return make_ref<variable>(any(), variable_type::vt_null);
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
            return get_null_var();
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
        else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node)) 
            return evaluate_member_access_expression(member_access);
        else if (auto func_call = std::dynamic_pointer_cast<function_call>(node)) 
            return evaluate_function_call(func_call);
        else 
            throw evaluation_error("Cannot evaluate expression of unknown type.");
    }    
} // namespace wio