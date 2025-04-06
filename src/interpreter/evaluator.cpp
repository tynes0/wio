#include "evaluator.h"

#include "lexer.h"
#include "parser.h"
#include "evaluator_helper.h"

#include "../base/exception.h"
#include "../utils/filesystem.h"

#include "../variables/variable.h"
#include "../variables/array.h"
#include "../variables/dictionary.h"
#include "../variables/function.h"
#include "../variables/realm.h"

#include "../builtin/builtin_base.h"
#include "../builtin/helpers.h"
#include "../builtin/builtin_manager.h"

#include "../interpreter.h"
#include "../types/vec2.h"
#include "../types/vec3.h"
#include "../types/vec4.h"
#include "../types/pair.h"

#include <filesystem>

namespace wio 
{
    namespace util
    {
        static bool is_var_t(variable_type vt)
        {
            switch (vt)
            {
            case wio::variable_type::vt_null:
            case wio::variable_type::vt_var_param:
            case wio::variable_type::vt_integer:
            case wio::variable_type::vt_float:
            case wio::variable_type::vt_string:
            case wio::variable_type::vt_character:
            case wio::variable_type::vt_bool:
            case wio::variable_type::vt_any:
                return true;
            case wio::variable_type::vt_array:
            case wio::variable_type::vt_dictionary:
            case wio::variable_type::vt_function:
            default:
                return false;
            }
        }
    }

    evaluator::evaluator(packed_bool flags) : m_program_flags(flags)
    {
        m_current_scope = make_ref<scope>(scope_type::global);
        builtin_manager::load(m_constant_object_members);
    }

    void evaluator::evaluate_program(ref<program> program_node) 
    {
        enter_statement_stack(&program_node->m_statements);

        for (auto& stmt : program_node->m_statements) 
            evaluate_statement(stmt);

        exit_statement_stack();
    }

    symbol_table_t& evaluator::get_symbols()
    {
        return m_current_scope->get_symbols();
    }

    ref<variable_base> evaluator::evaluate_expression(ref<expression> node)
    {
        return get_value(node);
    }

    void evaluator::evaluate_statement(ref<statement> node)
    {
        // Should evaluate statement be made or not? Check the flags
        if (m_eval_flags.b1 || m_eval_flags.b2 || m_eval_flags.b3)
            return;
        
        if (auto decl = std::dynamic_pointer_cast<variable_declaration>(node))
            evaluate_variable_declaration(decl);
        else if (auto fdecl = std::dynamic_pointer_cast<function_declaration>(node))
            evaluate_function_declaration(fdecl);
        else if (auto def = std::dynamic_pointer_cast<function_definition>(node))
            evaluate_function_definition(def);
        else if (auto adecl = std::dynamic_pointer_cast<array_declaration>(node))
            evaluate_array_declaration(adecl);
        else if (auto ddecl = std::dynamic_pointer_cast<dictionary_declaration>(node))
            evaluate_dictionary_declaration(ddecl);
        else if (auto rdecl = std::dynamic_pointer_cast<realm_declaration>(node))
            evaluate_realm_declaration(rdecl);
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
        if (auto break_s = std::dynamic_pointer_cast<break_statement>(node))
            evaluate_break_statement(break_s);
        else if (auto continue_s = std::dynamic_pointer_cast<continue_statement>(node))
            evaluate_continue_statement(continue_s);
        else if (auto return_s = std::dynamic_pointer_cast<return_statement>(node))
            evaluate_return_statement(return_s);
        // MAYBE: UNKNOWN STATEMENT ERR
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
            elements[var_dictionary::as_key(evaluate_expression(item.first))] = var;
        }

        return make_ref<var_dictionary>(elements);
    }

    ref<variable_base> evaluator::evaluate_typeof_expression(ref<typeof_expression> node)
    {
        ref<variable_base> value_base = evaluate_expression(node->m_expr);
        return make_ref<variable>(value_base->get_type(), variable_type::vt_type);
    }

    ref<variable_base> evaluator::evaluate_binary_expression(ref<binary_expression> node, ref<variable_base> object, bool is_ref)
    {
        ref<variable_base> lv_ref = get_value(node->m_left, object, is_ref);
        ref<variable_base> rv_ref = get_value(node->m_right);

        any left_value;
        any right_value;

        if(lv_ref->get_base_type() == variable_base_type::variable)
            left_value = std::dynamic_pointer_cast<variable>(lv_ref)->get_data();
        if(rv_ref->get_base_type() == variable_base_type::variable)
            right_value = std::dynamic_pointer_cast<variable>(rv_ref)->get_data();

        token op = node->m_operator;

        if (op.type == token_type::op) 
        {
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
            else if (op.value == "&&")
                return helper::eval_binary_exp_logical_and(lv_ref, rv_ref, node->get_location());
            else if (op.value == "||")
                return helper::eval_binary_exp_logical_or(lv_ref, rv_ref, node->get_location());
            else if (op.value == "&")
                return helper::eval_binary_exp_bitwise_and(lv_ref, rv_ref, node->get_location());
            else if (op.value == "|")
                return helper::eval_binary_exp_bitwise_or(lv_ref, rv_ref, node->get_location());
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

    ref<variable_base> evaluator::evaluate_unary_expression(ref<unary_expression> node, ref<variable_base> object, bool is_ref)
    {
        ref<variable_base> operand_ref = get_value(node->m_operand, object, node->m_is_ref);
        token op = node->m_operator;

        if (operand_ref->get_base_type() != variable_base_type::variable)
        {
            if (op.value == "?")
            {
                if (operand_ref->get_type() == variable_type::vt_null)
                    return make_ref<variable>(any(false), variable_type::vt_bool);

                switch (operand_ref->get_base_type())
                {
                case wio::variable_base_type::array:
                {
                    ref<var_array> array = std::dynamic_pointer_cast<var_array>(operand_ref);
                    return make_ref<variable>(any((array->size() != 0)), variable_type::vt_bool);
                }
                case wio::variable_base_type::dictionary:
                {
                    ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(operand_ref);
                    return make_ref<variable>(any((dict->size() != 0)), variable_type::vt_bool);
                }
                case wio::variable_base_type::function:
                {
                    ref<var_function> func = std::dynamic_pointer_cast<var_function>(operand_ref);
                    return make_ref<variable>(any(((bool)func->get_data())), variable_type::vt_bool);
                }
                case wio::variable_base_type::variable:
                default:
                    break;
                }
            }
            throw invalid_type_error("Invalid type with '?'", node->m_operand->get_location());

        }

        any& operand_value = std::dynamic_pointer_cast<variable>(operand_ref)->get_data();

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
                else if (operand_value.type() == typeid(char)) 
                {
                    char& value = any_cast<char&>(operand_value);
                    char old_value = value;
                    if (op.value == "++") 
                        value++;
                    else 
                        value--;

                    return make_ref<variable>(any(node->m_op_type == unary_operator_type::prefix ? value : old_value), variable_type::vt_character);
                }
                else
                {
                    throw invalid_type_error("Unary '++' and '--' operators require a numeric or character operand.", node->get_location());
                }
            }
            else if (op.value == "!")
            {
                if (operand_value.type() == typeid(bool))
                    return make_ref<variable>(any(!wio::any_cast<bool>(operand_value)), variable_type::vt_bool);
                else
                    throw invalid_type_error("Unary '!' operator requires a boolean operand.", node->get_location());
            }
            else if (op.value == "?")
            {
                return make_ref<variable>(any(!operand_value.empty()), variable_type::vt_bool);
            }
            else if (op.value == "~")
            {
                if (operand_value.type() == typeid(long long))
                    return make_ref<variable>(any(~any_cast<long long>(operand_value)), variable_type::vt_integer);
                else
                    throw invalid_type_error("Unary '~' operator requires an integer operand.", node->get_location());
            }
        }

        throw invalid_operator_error("Invalid unary operator: " + op.value, node->get_location());
    }

    ref<variable_base> evaluator::evaluate_identifier(ref<identifier> node, ref<variable_base> object, bool is_ref)
    {
        symbol* sym = lookup_member(node->m_token.value, object);

        if (!sym)
        {
            if(object)
                throw undefined_identifier_error("Undefined member: " + node->m_token.value, node->get_location());
            throw undefined_identifier_error("Undefined identifier: " + node->m_token.value, node->get_location());

        }

        if (node->m_is_ref)
            sym->var_ref->set_ref(true);

        if (sym->var_ref)
        {
            if (auto var = std::dynamic_pointer_cast<variable>(sym->var_ref)) // test.length
            {
                if (sym->var_ref->is_ref() || node->m_is_lhs || is_ref)
                    return var;
                return var->clone();
            }
            else if (auto arr = std::dynamic_pointer_cast<var_array>(sym->var_ref))
            {
                if (sym->var_ref->is_ref() || node->m_is_lhs || is_ref)
                    return arr;
                return arr->clone();
            }
            else if (auto dict = std::dynamic_pointer_cast<var_dictionary>(sym->var_ref))
            {
                if (sym->var_ref->is_ref() || node->m_is_lhs || is_ref)
                    return dict;
                return dict->clone();
            }
            else if (auto func = std::dynamic_pointer_cast<var_function>(sym->var_ref))
            {
                return func->clone();
            }
            else if (auto null = std::dynamic_pointer_cast<null_var>(sym->var_ref))
            {
                return null->clone();
            }
            else
                throw evaluation_error("Identifier '" + node->m_token.value + "' refers to an unsupported type.", node->get_location());
        }
        return get_null_var();
    }

    ref<variable_base> evaluator::evaluate_array_access_expression(ref<array_access_expression> node, ref<variable_base> object, bool is_ref)
    {
        ref<variable_base> item = get_value(node->m_array, object, is_ref);

        variable_base_type base_t = item->get_base_type();

        if (base_t == variable_base_type::array)
        {
            ref<variable_base> index_base = evaluate_expression(node->m_key_or_index);
            if (index_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<variable> index_var = std::dynamic_pointer_cast<variable>(index_base);
            if (index_var->get_type() != variable_type::vt_integer)
                throw invalid_type_error("Index's type should be integer!", node->m_key_or_index->get_location());

            ref<var_array> array_var = std::dynamic_pointer_cast<var_array>(item);

            if (node->m_is_ref || node->m_is_lhs || is_ref)
                return array_var->get_element(index_var->get_data_as<long long>());
            return array_var->get_element(index_var->get_data_as<long long>())->clone();
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
            long long index = index_var->get_data_as<long long>();
            if (index < 0 || index >= long long(value.size()))
                throw out_of_bounds_error("String index out of the bounds!");

            return make_ref<variable>(any(value[index]), variable_type::vt_character);
        }
        else if (base_t == variable_base_type::dictionary)
        {
            ref<variable_base> key_base = evaluate_expression(node->m_key_or_index);
            if (key_base->get_base_type() != variable_base_type::variable)
                throw invalid_type_error("Index's type should be variable!", node->m_key_or_index->get_location());

            ref<var_dictionary> dict_var = std::dynamic_pointer_cast<var_dictionary>(item);

            if (node->m_is_ref || node->m_is_lhs || is_ref)
                return dict_var->get_element(key_base);
            return dict_var->get_element(key_base)->clone();
        }
        else
        {
            throw invalid_type_error("Type should be array or string or dictionary!", node->m_array->get_location());
        }
    }

    ref<variable_base> evaluator::evaluate_member_access_expression(ref<member_access_expression> node, ref<variable_base> object, bool is_ref)
    {
        ref<variable_base> object_value = get_value(node->m_object, object, is_ref);
        ref<variable_base> member = get_value(node->m_member, object_value, node->m_is_lhs || node->m_is_ref);
        return member;
    }

    ref<variable_base> evaluator::evaluate_function_call(ref<function_call> node, ref<variable_base> object, bool is_ref)
    {
        if (m_program_flags.b2)
            return get_null_var();

        ref<variable_base> caller_value = get_value(node->m_caller, object);

        if (!caller_value)
           throw undefined_identifier_error("Invalid function!", node->m_caller->get_location());
        else if (caller_value->get_type() != variable_type::vt_function)
           throw invalid_type_error("Type should be function!", node->m_caller->get_location());

        ref<var_function> func = std::dynamic_pointer_cast<var_function>(caller_value);
        ref<var_function> func_res;

        if(!func)
           throw undefined_identifier_error("Invalid function!", node->m_caller->get_location());

        std::vector<ref<variable_base>> parameters;
        
        int padding = 0;
        if (object && object->get_type() != variable_type::vt_realm)
        {
            object->set_pf_return_ref(is_ref);
            padding = 1;
            parameters.push_back(object);
        }

        for (size_t i = 0; i < func->overload_count(); ++i)
        {
            func_res = std::dynamic_pointer_cast<var_function>(func->get_overload(i)->var_ref);
            const auto& real_params = func_res->get_parameters();

            if ((node->m_arguments.size() + padding) != real_params.size())
            {
                if (i == (func->overload_count() - 1))
                    throw invalid_argument_count_error("Invalid parameter count!", node->m_caller->get_location());
                continue;
            }

            for (size_t i = 0; i < node->m_arguments.size(); ++i)
                parameters.push_back(get_value(node->m_arguments[i], nullptr, real_params[padding + i].is_ref));


            for (size_t i = 0; i < real_params.size(); ++i)
            {
                if (parameters[i]->get_type() != real_params[i].type && real_params[i].type != variable_type::vt_any &&
                    !(parameters[i]->get_base_type() == variable_base_type::variable && util::is_var_t(real_params[i].type)))
                {
                    if (i == (func->overload_count() - 1))
                        throw type_mismatch_error("Parameter types not matching!");
                    continue;
                }
            }
            break;
        }

        if (!func_res->declared())
        {
            auto decl = get_func_decl(func_res->get_symbol_id(), func_res->get_parameters());
            if(!decl)
                throw local_exception("Function '" + func_res->get_symbol_id() + "' defined but not declared!", node->m_caller->get_location());

            evaluate_function_declaration(decl);
            func_res->set_early_declared(true);
        }

        ref<variable_base> result = func_res->call(parameters);
        if (object)
            object->set_pf_return_ref(false);
        return result;
    }

    ref<scope> evaluator::evaluate_block_statement(ref<block_statement> node)
    {
        enter_scope(scope_type::block);
        enter_statement_stack(&node->m_statements);

        for (auto& stmt : node->m_statements)
            evaluate_statement(stmt);

        exit_statement_stack();
        return exit_scope();
    }

    void evaluator::evaluate_expression_statement(ref<expression_statement> node)
    {
        if (m_eval_flags.b4)
            return;
        evaluate_expression(node->m_expression);
    }

    void evaluator::evaluate_if_statement(ref<if_statement> node)
    {
        if (m_eval_flags.b4)
        {
            evaluate_statement(node->m_then_branch);
            evaluate_statement(node->m_else_branch);
            return;
        }

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
        if (m_eval_flags.b4)
        {
            enter_scope(scope_type::block);
            enter_statement_stack(&node->m_body->m_statements);

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            exit_statement_stack();
            exit_scope();
            return;
        }

        enter_scope(scope_type::block);
        evaluate_statement(node->m_initialization);

        m_loop_depth++;
        while(true)
        {
            ref<variable_base> cond = get_value(node->m_condition);
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
                    m_eval_flags.b1 = false;
                    m_eval_flags.b2 = false;
                    break;
                }
            }

            enter_statement_stack(&node->m_body->m_statements);

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            exit_statement_stack();

            if (m_eval_flags.b1)
            {
                m_eval_flags.b1 = false;
                break;
            }
            else if (m_eval_flags.b2)
            {
                m_eval_flags.b2 = false;
            }
            evaluate_expression(node->m_increment);
        }
        m_loop_depth--;

        exit_scope();
    }

    void evaluator::evaluate_foreach_statement(ref<foreach_statement> node)
    {
        if (m_eval_flags.b4)
        {
            enter_scope(scope_type::block);
            enter_statement_stack(&node->m_body->m_statements);

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            exit_statement_stack();
            exit_scope();
            return;
        }

        enter_scope(scope_type::block);
        m_loop_depth++;
        evaluate_variable_declaration(make_ref<variable_declaration>(node->m_item, nullptr, false, false, false, false));
        ref<variable_base> col_base = evaluate_expression(node->m_collection);
        symbol* sym = lookup(node->m_item->m_token.value);
        if (col_base->get_base_type() == variable_base_type::array)
        {
            ref<var_array> array = std::dynamic_pointer_cast<var_array>(col_base);
            for (size_t i = 0; i < array->size(); ++i)
            {
                sym->var_ref = array->get_element(i);


                enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                    evaluate_statement(stmt);
                exit_statement_stack();

                if (m_eval_flags.b1)
                {
                    m_eval_flags.b1 = false;
                    break;
                }
                else if (m_eval_flags.b2)
                {
                    m_eval_flags.b2 = false;
                }
            }
        }
        else if (col_base->get_base_type() == variable_base_type::dictionary)
        {
            ref<var_dictionary> dict = std::dynamic_pointer_cast<var_dictionary>(col_base);
            for (auto& it : dict->get_data())
            {
                sym->var_ref = builtin::helpers::create_pair(make_ref<variable>(it.first, variable_type::vt_string), it.second);

                enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                    evaluate_statement(stmt);
                exit_statement_stack();

                if (m_eval_flags.b1)
                {
                    m_eval_flags.b1 = false;
                    break;
                }
                else if (m_eval_flags.b2)
                {
                    m_eval_flags.b2 = false;
                }
            }
        }
        else if (col_base->get_type() == variable_type::vt_string)
        {
            ref<variable> str = std::dynamic_pointer_cast<variable>(col_base);
            ref<var_array> array = std::dynamic_pointer_cast<var_array>(builtin::helpers::string_as_array(str));
            for (size_t i = 0; i < array->size(); ++i)
            {
                sym->var_ref = array->get_element(i);

                enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                    evaluate_statement(stmt);
                exit_statement_stack();

                if (m_eval_flags.b1)
                {
                    m_eval_flags.b1 = false;
                    break;
                }
                else if (m_eval_flags.b2)
                {
                    m_eval_flags.b2 = false;
                }
            }
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
        if (m_eval_flags.b4)
        {
            enter_scope(scope_type::block);
            enter_statement_stack(&node->m_body->m_statements);

            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);

            exit_statement_stack();
            exit_scope();
            return;
        }

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
                m_eval_flags.b1 = false;
                m_eval_flags.b2 = false;
                break;
            }

            enter_statement_stack(&node->m_body->m_statements);
            for (auto& stmt : node->m_body->m_statements)
                evaluate_statement(stmt);
            exit_statement_stack();

            if (m_eval_flags.b1)
            {
                m_eval_flags.b1 = false;
                break;
            }
            else if (m_eval_flags.b2)
            {
                m_eval_flags.b2 = false;
            }
        }
        m_loop_depth--;

        exit_scope();
    }

    void evaluator::evaluate_break_statement(ref<break_statement> node)
    {
        if (m_eval_flags.b4)
            return;
        if (!m_loop_depth)
            throw invalid_break_statement("Break statement called out of the loop!", node->m_loc);
        m_eval_flags.b1 = true;
    }

    void evaluator::evaluate_continue_statement(ref<continue_statement> node)
    {
        if (m_eval_flags.b4)
            return;
        if (!m_loop_depth)
            throw invalid_continue_statement("Continue statement called out of the loop!", node->m_loc);
        m_eval_flags.b2 = true;
    }

    void evaluator::evaluate_import_statement(ref<import_statement> node)
    {
        if (m_eval_flags.b4)
            return;

        ref<scope> target_scope = nullptr;
        ref<realm> realm_var = nullptr;

        if (node->m_flags.b2)
        {
            if (!node->m_realm_id)
                throw local_exception("Unexpected error!", node->get_location());

            std::string name = node->m_realm_id->m_token.value;

            if (lookup(name))
                throw local_exception("Duplicate variable declaration: " + name, node->m_realm_id->get_location());

            ref<realm> realm_var = make_ref<realm>();
            realm_var->init_members();

            target_scope = realm_var->get_members();

            if (target_scope)
            {
                symbol realm_symbol(name, realm_var, { false, true });

                builtin_scope->insert(name, realm_symbol);
            }

        }
        
        raw_buffer buf = interpreter::get().run_f(node->m_module_path.c_str(), { m_program_flags.b1, node->m_flags.b1, false, m_program_flags.b4 }, target_scope);

        if (!buf)
            return;

        auto& symbols = buf.load<symbol_table_t>();

        if (!node->m_flags.b2)
        {
            for (auto& [key, value] : symbols)
            {
                if (value.flags.b1)
                    continue;
                m_current_scope->insert(key, value);
            }
        }
        else
        {
            realm_var->load_members(symbols);

            symbol realm_symbol(node->m_realm_id->m_token.value, realm_var, { false, false, m_eval_flags.b5 });

            m_current_scope->insert(node->m_realm_id->m_token.value, realm_symbol);
        }
    }

    void evaluator::evaluate_return_statement(ref<return_statement> node)
    {
        if (m_eval_flags.b4 == true)
            return;
        ref<scope> temp = m_current_scope;
        while (temp && temp->get_type() != scope_type::function_body)
            temp = temp->get_parent();
        if(!temp)
            throw invalid_return_statement("Return statement out of the function", node->get_location());

        m_last_return_value = evaluate_expression(node->m_value);
        m_eval_flags.b3 = true;
    }

    void evaluator::evaluate_variable_declaration(ref<variable_declaration> node, bool is_parameter)
    {
        if (m_eval_flags.b4 && !node->m_flags.b3)
            return;

        std::string name = node->m_id->m_token.value;
        variable_type type = node->m_type;

        if (is_parameter)
        {
            if (lookup_current_and_global(name) != nullptr)
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
            if (node->m_type == variable_type::vt_array)
                evaluate_array_declaration(make_ref<array_declaration>(node->m_id, nullptr, std::vector<ref<expression>>{}, false, false, false, true));
            if (node->m_type == variable_type::vt_dictionary)
                evaluate_dictionary_declaration(make_ref<dictionary_declaration>(node->m_id, nullptr, std::vector<std::pair<ref<expression>, ref<expression>>>{}, false, false, false, true));
        }
        else
        {
            symbol* sym = lookup(name);
            if (sym != nullptr)
            {
                if (node->m_flags.b3 && !sym->flags.b3 && m_eval_flags.b5)
                {
                    sym->flags.b3 = true;
                    return;
                }
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
            }
        }

        ref<variable_base> var;

        if (node->m_initializer)
        {
            auto exp = evaluate_expression(node->m_initializer);
            if (exp->get_base_type() == variable_base_type::variable)
            {
                var = std::dynamic_pointer_cast<variable>(exp);

                if (node->m_flags.b1)
                    var->set_const(true);
            }
            else if (exp->get_type() == variable_type::vt_null)
            {
                var = get_null_var();
            }
            else
            {
                throw local_exception("initializer has an invalid type.", node->m_initializer->get_location());
            }
        }

        symbol symbol(name, var, { node->m_flags.b2, node->m_flags.b3, m_eval_flags.b5 });
        if (node->m_flags.b3)
            m_current_scope->insert_to_global(name, symbol);
        else
            m_current_scope->insert(name, symbol);
    }

    void evaluator::evaluate_array_declaration(ref<array_declaration> node)
    {
        if (m_eval_flags.b4 && !node->m_flags.b3)
            return;

        std::string name = node->m_id->m_token.value;

        symbol* sym = lookup(name);
        if (sym != nullptr)
        {
            if (node->m_flags.b3 && !sym->flags.b3 && m_eval_flags.b5)
            {
                sym->flags.b3 = true;
                return;
            }
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
        }

        ref<variable_base> var;

        if (node->m_flags.b4)
        {
            std::vector<ref<variable_base>> elements;

            for (auto& item : node->m_elements)
            {
                ref<variable_base> var = evaluate_expression(item)->clone();
                var->set_const(node->m_flags.b1);
                elements.push_back(var);
            }

            var = make_ref<var_array>(elements, packed_bool{ node->m_flags.b1, node->m_id->m_is_ref });
        }
        else
        {
            if (node->m_initializer)
            {
                ref<variable_base> value = get_value(node->m_initializer);
                if (value->get_base_type() != variable_base_type::array)
                    throw local_exception("initializer has an invalid type.", node->m_initializer->get_location());

                var = std::dynamic_pointer_cast<var_array>(value);
                
                if (node->m_flags.b1)
                    var->set_const(true);
            }
            else
            {
                var = make_ref<var_array>(std::vector<ref<variable_base>>{}, packed_bool{ node->m_flags.b1, node->m_id->m_is_ref });
            }
        }


        symbol array_symbol(name, var, { node->m_flags.b2, node->m_flags.b3, m_eval_flags.b5 });
        if (node->m_flags.b3)
            m_current_scope->insert_to_global(name, array_symbol);
        else
            m_current_scope->insert(name, array_symbol);
    }

    void evaluator::evaluate_dictionary_declaration(ref<dictionary_declaration> node)
    {
        if (m_eval_flags.b4 && !node->m_flags.b3)
            return;

        std::string name = node->m_id->m_token.value;

        symbol* sym = lookup(name);
        if (sym != nullptr)
        {
            if (node->m_flags.b3 && !sym->flags.b3 && m_eval_flags.b5)
            {
                sym->flags.b3 = true;
                return;
            }
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
        }

        ref<variable_base> var;

        if (node->m_flags.b4)
        {
            var_dictionary::map_t elements;

            for (auto& item : node->m_pairs)
            {
                ref<variable_base> var = evaluate_expression(item.second)->clone();
                var->set_const(node->m_flags.b1);
                elements[var_dictionary::as_key(evaluate_expression(item.first))] = var;
            }

            var = make_ref<var_dictionary>(elements, packed_bool{ node->m_flags.b1, node->m_id->m_is_ref });
        }
        else
        {
            if (node->m_initializer)
            {
                ref<variable_base> value = get_value(node->m_initializer);
                if (value->get_base_type() != variable_base_type::dictionary)
                    throw local_exception("initializer has an invalid type.", node->m_initializer->get_location());

                var = std::dynamic_pointer_cast<var_dictionary>(value);

                
                if (node->m_flags.b1)
                    var->set_const(true);
                
            }
            else
            {
                var = make_ref<var_dictionary>(var_dictionary::map_t{}, packed_bool{ node->m_flags.b1, node->m_id->m_is_ref });
            }
        }

        symbol dict_symbol(name, var, { node->m_flags.b2, node->m_flags.b3, m_eval_flags.b5 });
        if (node->m_flags.b3)
            m_current_scope->insert_to_global(name, dict_symbol);
        else
            m_current_scope->insert(name, dict_symbol);
    }

    void evaluator::evaluate_function_declaration(ref<function_declaration> node)
    {
        if (m_eval_flags.b4 && !node->m_is_global)
            return;

        std::string name = node->m_id->m_token.value;

        // parameter types and ids
        std::vector<function_param> parameters;
        for (auto& param : node->m_params)
            parameters.emplace_back(param->m_id->m_token.value, param->m_type, (bool)param->m_flags.b4);

        packed_bool flags = {}; // 1- add overload 2- declare

        symbol* sym = lookup(name);
        symbol* overload = nullptr;

        if (sym != nullptr)
        {
            ref<var_function> func_ref = std::dynamic_pointer_cast<var_function>(sym->var_ref);
            if (!func_ref)
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

            overload = func_ref->find_overload(parameters);

            if (overload)
            {
                if (node->m_is_global && !overload->flags.b3 && m_eval_flags.b5)
                {
                    overload->flags.b3 = true;
                    return;
                }

                auto fref = std::dynamic_pointer_cast<var_function>(overload->var_ref);

                if (!fref->declared())
                {
                    flags.b2 = true;
                }
                else if (fref->early_declared())
                {
                    fref->set_early_declared(false);
                    return;
                }
                else
                {
                    throw local_exception("Duplicate function declaration: " + name, node->m_id->get_location());
                }
            }
            else
            {
                flags.b1 = true;
            }
        }

        enter_scope(scope_type::function);
        enter_statement_stack(&node->m_body->m_statements);
        evaluate_only_global_declarations();
        exit_statement_stack();
        exit_scope();

        // Generate func body
        auto func_lambda = [this, node](const std::vector<function_param>& param_ids, std::vector<ref<variable_base>>& parameters)->ref<variable_base>
            {
                enter_scope(scope_type::function_body);

                // Create parameters
                for (auto& param : node->m_params)
                {
                    if (param->m_type == variable_type::vt_var_param)
                        evaluate_variable_declaration(param, true);
                }

                // use variable ref's
                for (size_t i = 0; i < parameters.size(); ++i)
                {
                    symbol* s = lookup(param_ids[i].id);
                    s->var_ref = parameters[i];
                }

                enter_statement_stack(&node->m_body->m_statements);
                for (auto& stmt : node->m_body->m_statements)
                {
                    evaluate_statement(stmt);
                    // return check
                    if (m_eval_flags.b3)
                    {
                        m_eval_flags.b3 = false;
                        ref<variable_base> retval = m_last_return_value;
                        m_last_return_value = nullptr;
                        exit_statement_stack();
                        exit_scope();
                        return retval;
                    }
                }
                exit_statement_stack();

                exit_scope();
                return get_null_var();
            };

        if (flags.b1)
        {
            symbol function_symbol(name, make_ref<var_function>(func_lambda, parameters, node->m_is_local), { node->m_is_local, node->m_is_global, m_eval_flags.b5 });

            std::dynamic_pointer_cast<var_function>(sym->var_ref)->add_overload(function_symbol);
        }
        else if (flags.b2)
        {
            std::dynamic_pointer_cast<var_function>(overload->var_ref)->set_data(func_lambda);
        }
        else
        {
            symbol function_symbol(name, make_ref<var_function>(func_lambda, parameters, node->m_is_local), { node->m_is_local, node->m_is_global, m_eval_flags.b5 });

            if (node->m_is_global)
                m_current_scope->insert_to_global(name, function_symbol);
            else
                m_current_scope->insert(name, function_symbol);
        }
    }

    void evaluator::evaluate_function_definition(ref<function_definition> node)
    {
        if (m_eval_flags.b4 && !node->m_is_global)
            return;

        // Check
        std::string name = node->m_id->m_token.value;

        // Parameter types and id's
        std::vector<function_param> parameters;
        for (auto& param : node->m_params)
            parameters.emplace_back(param->m_id->m_token.value, param->m_type, (bool)param->m_flags.b4);

        symbol* sym = lookup(name);
        if (sym != nullptr)
        {
            auto func_ref = std::dynamic_pointer_cast<var_function>(sym->var_ref);
            if (!func_ref)
                throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());

            auto overload_ref = func_ref->find_overload(parameters);
            if (overload_ref)
                throw local_exception("Duplicate function definition: " + name, node->m_id->get_location());

            auto fun_v = make_ref<var_function>(parameters);
            fun_v->set_symbol_id(name);

            func_ref->add_overload(symbol("", fun_v, { node->m_is_local, node->m_is_global }));
        }
        else
        {
            auto fun_v = make_ref<var_function>(parameters);
            std::dynamic_pointer_cast<var_function>(fun_v->get_overload(0)->var_ref)->set_symbol_id(name);
            symbol function_symbol(node->m_id->m_token.value, fun_v, { node->m_is_local, node->m_is_global });

            if (node->m_is_global)
                m_current_scope->insert_to_global(name, function_symbol);
            else
                m_current_scope->insert(name, function_symbol);
        }
    }

    void evaluator::evaluate_realm_declaration(ref<realm_declaration> node)
    {
        if (m_eval_flags.b4 && !node->m_is_global)
            return;

        std::string name = node->m_id->m_token.value;

        symbol* sym = lookup(name);
        if (sym != nullptr)
        {
            if (node->m_is_global && !sym->flags.b3 && m_eval_flags.b5)
            {
                sym->flags.b3 = true;
                return;
            }
            throw local_exception("Duplicate variable declaration: " + name, node->m_id->get_location());
        }

        ref<realm> realm_var = make_ref<realm>();
        realm_var->load_members(evaluate_block_statement(node->m_body));
        symbol realm_symbol(name, realm_var, { node->m_is_local, node->m_is_global, m_eval_flags.b5 });
        if (node->m_is_global)
            m_current_scope->insert_to_global(name, realm_symbol);
        else
            m_current_scope->insert(name, realm_symbol);
    }

    void evaluator::evaluate_only_global_declarations()
    {
        m_eval_flags.b4 = true;
        for (auto& stmt : *m_statement_stack->list)
            evaluate_statement(stmt);
        m_eval_flags.b4 = false;
    }

    void evaluator::enter_scope(scope_type type)
    {
        if (type == scope_type::function_body)
        {
            ++m_func_body_counter;
            m_eval_flags.b5 = true;
        }
        m_current_scope = make_ref<scope>(type, m_current_scope);
    }

    ref<scope> evaluator::exit_scope()
    {
        if (m_current_scope->get_type() == scope_type::global)
            return nullptr;
        if (m_current_scope->get_type() == scope_type::function_body)
        {
            if (m_func_body_counter)
            {
                m_func_body_counter--;
                if (!m_func_body_counter)
                    m_eval_flags.b5 = false;
            }
            else
            {
                throw exception("Unexpected error!");
            }
        }

        ref<scope> child = m_current_scope;
        if (m_current_scope->get_parent()) 
            m_current_scope = m_current_scope->get_parent();
        return child;
    }

    void evaluator::enter_statement_stack(std::vector<ref<statement>>* list)
    {
        m_statement_stack = make_ref<statement_stack>(list, m_statement_stack);
    }

    void evaluator::exit_statement_stack()
    {
        if (m_statement_stack->parent)
            m_statement_stack = m_statement_stack->parent;
    }

    symbol* evaluator::lookup_member(const std::string& name, ref<variable_base> object)
    {
        symbol* sym = nullptr;
        if (object)
        {
            if (m_constant_object_members[object->get_type()])
            {
                sym = m_constant_object_members[object->get_type()]->lookup(name);
            }
            if (!sym)
            {
                ref<scope> member_scope = object->get_members();
                if (!member_scope)
                    return nullptr;
                return member_scope->lookup(name);
            }
            else
                return sym;
        }
        return lookup(name);
    }

    symbol* evaluator::lookup(const std::string& name) 
    {
        return m_current_scope->lookup(name);
    }

    symbol* evaluator::lookup_current_and_global(const std::string& name)
    {
        return m_current_scope->lookup_current_and_global(name);
    }

    ref<function_declaration> evaluator::get_func_decl(const std::string& name, std::vector<function_param> real_parameters)
    {
        for (auto& stmt : *m_statement_stack->list)
        {
            if (auto func_decl = std::dynamic_pointer_cast<function_declaration>(stmt))
            {
                if (func_decl->m_id->m_token.value == name)
                {
                    std::vector<function_param> parameters;
                    for (auto& param : func_decl->m_params)
                        parameters.emplace_back(param->m_id->m_token.value, param->m_type, (bool)param->m_flags.b4);

                    if (std::equal(parameters.begin(), parameters.end(), real_parameters.begin(), real_parameters.end()))
                        return func_decl;
                }
            }
        }

        return nullptr;
    }

    ref<variable_base> evaluator::get_null_var(variable_base_type vbt)
    {
        return make_ref<null_var>(vbt);
    }

    ref<variable_base> evaluator::get_value(ref<expression> node, ref<variable_base> object, bool is_ref)
    {
        if (auto lit = std::dynamic_pointer_cast<literal>(node))
            return evaluate_literal(lit);
        else if (auto str_lit = std::dynamic_pointer_cast<string_literal>(node))
            return evaluate_string_literal(str_lit);
        else if (auto arr_lit = std::dynamic_pointer_cast<array_literal>(node))
            return evaluate_array_literal(arr_lit);
        else if (auto dict_lit = std::dynamic_pointer_cast<dictionary_literal>(node))
            return evaluate_dictionary_literal(dict_lit);
        else if (auto null_exp = std::dynamic_pointer_cast<null_expression>(node))
            return get_null_var();
        else if (auto typeof_expr = std::dynamic_pointer_cast<typeof_expression>(node))
            return evaluate_typeof_expression(typeof_expr);
        else if (auto bin_expr = std::dynamic_pointer_cast<binary_expression>(node))
            return evaluate_binary_expression(bin_expr, object, is_ref);
        else if (auto unary_expr = std::dynamic_pointer_cast<unary_expression>(node))
            return evaluate_unary_expression(unary_expr, object, is_ref);
        else if (auto id = std::dynamic_pointer_cast<identifier>(node))
            return evaluate_identifier(id, object, is_ref);
        else if (auto arr_access = std::dynamic_pointer_cast<array_access_expression>(node))
            return evaluate_array_access_expression(arr_access, object, is_ref);
        else if (auto member_access = std::dynamic_pointer_cast<member_access_expression>(node))
            return evaluate_member_access_expression(member_access, object, is_ref);
        else if (auto func_call = std::dynamic_pointer_cast<function_call>(node))
            return evaluate_function_call(func_call, object, is_ref);
        else
            return nullptr;
    }
} // namespace wio