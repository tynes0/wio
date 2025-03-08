#include "ast.h"

namespace wio
{

    variable_type identifier::get_expression_type() const
    {
        if (var_ref != nullptr)
        {
            if (var_ref->get_base_type() == variable_base_type::variable)
                return std::dynamic_pointer_cast<variable>(var_ref)->get_type();
            else if (var_ref->get_base_type() == variable_base_type::array)
                return variable_type::vt_array;
            else if (var_ref->get_base_type() == variable_base_type::dictionary)
                return variable_type::vt_dictionary;
            else if (var_ref->get_base_type() == variable_base_type::function)
                return variable_type::vt_function;
        }
        return variable_type::vt_none;
    }

    variable_type binary_expression::get_expression_type() const
    {
        variable_type left_type = m_left->get_expression_type();
        variable_type right_type = m_right->get_expression_type();

        switch (m_operator.type)
        {
        case token_type::op:
            if (m_operator.value == "+" || m_operator.value == "-" || m_operator.value == "*" || m_operator.value == "/")
            {
                if (left_type == variable_type::vt_float || right_type == variable_type::vt_float)
                    return variable_type::vt_float;
                if (left_type == variable_type::vt_integer && right_type == variable_type::vt_integer)
                    return variable_type::vt_integer;
                if (left_type == variable_type::vt_string && right_type == variable_type::vt_string && m_operator.value == "+")
                    return variable_type::vt_string;
            }
            else if (m_operator.value == "<" || m_operator.value == ">" || m_operator.value == "<=" || m_operator.value == ">=" || m_operator.value == "==" || m_operator.value == "!=")
            {
                return variable_type::vt_bool;
            }
            break;

            //case token_type::logical_and:
            //case token_type::logical_or:
            //    return variable_type::vt_bool;

                // TODO: Add more cases for other operators (e.g., modulo, bitwise operators)
        default:
            break;
        }
        return variable_type::vt_none;
    }

    variable_type unary_expression::get_expression_type() const
    {
        variable_type operand_type = m_operand->get_expression_type();

        if (m_operator.type == token_type::op && m_operator.value == "!")
        {
            return variable_type::vt_bool;
        }
        else if (m_operator.type == token_type::op && m_operator.value == "-")
        {
            if (operand_type == variable_type::vt_integer) return variable_type::vt_integer;
            if (operand_type == variable_type::vt_float) return variable_type::vt_float;
        }
        return variable_type::vt_none;
    }

    variable_type assignment_expression::get_expression_type() const
    {
        return m_value->get_expression_type();
    }

    variable_type function_call::get_expression_type() const
    {
        if (m_caller->get_type() == token_type::identifier)
        {
            ref<identifier> id = std::dynamic_pointer_cast<identifier>(m_caller);
            if (id && id->var_ref && id->var_ref->get_base_type() == variable_base_type::function)
            {
                ref<function> func = std::dynamic_pointer_cast<function>(id->var_ref);
                return func->get_return_type();
            }
        }
        else if (m_caller->get_type() == token_type::dot) // member function call
        {
            // TODO: Implement member function calls (e.g., obj.method())
            return variable_type::vt_none;
        }
        else if (m_caller->get_type() == token_type::left_bracket) // array or dict access
        {
            // TODO
            return variable_type::vt_none;
        }
        return variable_type::vt_none;
    }

    variable_type array_access_expression::get_expression_type() const
    {
        auto arr_type = m_array->get_expression_type();
        if (arr_type == variable_type::vt_array)
        {
            // TODO
        }
        else if (arr_type == variable_type::vt_string)
        {
            return variable_type::vt_character;
        }

        return variable_type::vt_none;
    }

    variable_type dictionary_access_expression::get_expression_type() const
    {
        auto dict_type = m_dictionary->get_expression_type();
        if (dict_type == variable_type::vt_dictionary)
        {
            // TODO
        }
        return variable_type::vt_none;
    }


    variable_type member_access_expression::get_expression_type() const
    {
        variable_type object_type = m_object->get_expression_type();

        if (object_type == variable_type::vt_dictionary)
        {
            // TODO:
            return variable_type::vt_none;
        }
        else if (object_type == variable_type::vt_array)
        {
            if (m_member->m_token.value == "length") // ext.
            {
                return variable_type::vt_integer;
            }
        }

        return variable_type::vt_none;
    }

}