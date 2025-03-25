#include "ast.h"
#include "exception.h"

namespace wio
{

    literal::literal(token tok)  : m_token(tok)
    {
        switch (tok.type)
        {
        case token_type::number:
            m_type = tok.value.find('.') != std::string::npos ? variable_type::vt_float : variable_type::vt_integer;
            break;
        case token_type::string:
            m_type = variable_type::vt_string;
            break;
        case token_type::character:
            m_type = variable_type::vt_character;
            break;
        case token_type::kw_true:
        case token_type::kw_false:
            m_type = variable_type::vt_bool;
            break;
        default:
            m_type = variable_type::vt_null;
        }
    }

    std::string literal::to_string() const
    {
        return "Literal(" + m_token.value + ":" + frenum::to_string(m_type) + ")";
    }


    std::string string_literal::to_string() const
    {
        return "StringLiteral(" + m_token.value + ")";
    }

    std::string array_literal::to_string() const
    {
        std::string result = "[";
        for (size_t i = 0; i < m_elements.size(); ++i)
        {
            result += m_elements[i]->to_string();
            if (i < m_elements.size() - 1)
                result += ", ";
        }
        result += "];";
        return result;
    }

    std::string dictionary_literal::to_string() const
    {
        std::string result = "{";
        for (size_t i = 0; i < m_pairs.size(); ++i)
        {
            result += "[" + m_pairs[i].first->to_string() + ", ";
            result += m_pairs[i].second->to_string() + "]";
            if (i < m_pairs.size() - 1)
                result += ", ";
        }
        result += "};";
        return result;
    }

    null_expression::null_expression(token tok) : m_token(tok)
    {
    }

    variable_type null_expression::get_expression_type() const
    {
        return variable_type();
    }

    std::string null_expression::to_string() const
    {
        return "null";
    }

    identifier::identifier(token tok, bool is_ref, bool is_lhs) : m_token(tok), m_is_ref(is_ref), m_is_lhs(is_lhs)
    {
        if (tok.type != token_type::identifier)
            throw unexpected_token_error("When creating identifier, the token type must be identifier!");
    }

    variable_type identifier::get_expression_type() const
    {
        return variable_type::vt_any;
    }

    std::string identifier::to_string() const
    {
        return "Identifier(" + m_token.value + ")";
    }

    location program::get_location() const
    {
        if (m_statements.size() > 0)
            return m_statements[0]->get_location();

        return location{ 0, 0 };
    }

    std::string program::to_string() const
    {
        std::string result;
        for (const auto& stmt : m_statements)
            result += stmt->to_string() + "\n";
        return result;
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
        return variable_type::vt_null;
    }

    std::string binary_expression::to_string() const
    {
        return "BinaryExpr(" + m_left->to_string() + " " + m_operator.value + " " + m_right->to_string() + ")";
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
        return variable_type::vt_null;
    }

    std::string unary_expression::to_string() const
    {
        if (m_op_type == unary_operator_type::prefix)
            return "UnaryExpr(" + m_operator.value + " " + m_operand->to_string() + ")";
        return "UnaryExpr(" + m_operand->to_string() + " " + m_operator.value + ")";
    }

    variable_type assignment_expression::get_expression_type() const
    {
        return m_value->get_expression_type();
    }

    std::string assignment_expression::to_string() const
    {
        return "AssignmentExpr(" + m_target->to_string() + " " + m_operator.value + " " + m_value->to_string() + ")";
    }

    std::string typeof_expression::to_string() const
    {
        return "typeof " + m_expr->to_string();
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

        return variable_type::vt_null;
    }

    std::string array_access_expression::to_string() const
    {
        return m_array->to_string() + "[" + m_key_or_index->to_string() + "]";
    }

    variable_type member_access_expression::get_expression_type() const
    {
        variable_type object_type = m_object->get_expression_type();

        if (object_type == variable_type::vt_dictionary)
        {
            // TODO:
            return variable_type::vt_null;
        }
        else if (object_type == variable_type::vt_array)
        {

        }

        return variable_type::vt_null;
    }

    std::string member_access_expression::to_string() const
    {
        return m_object->to_string() + "." + m_member->to_string();
    }

    std::string block_statement::to_string() const
    {
        std::string result = "{";
        for (const auto& stmt : m_statements)
            result += "\n    " + stmt->to_string();
        result += "\n}";
        return result;
    }

    std::string expression_statement::to_string() const
    {
        return m_expression->to_string() + ";";
    }

    std::string if_statement::to_string() const
    {
        std::string result = "if (" + m_condition->to_string() + ") " + m_then_branch->to_string();
        if (m_else_branch)
            result += " else " + m_else_branch->to_string();
        return result;
    }

    location for_statement::get_location() const
    {
        if (m_initialization != nullptr)
            return m_initialization->get_location();
        else if (m_condition != nullptr)
            return m_condition->get_location();
        return m_body->get_location();
    }

    std::string for_statement::to_string() const
    {
        std::string result = "for (";
        if (m_initialization)
            result += m_initialization->to_string();
        result += "; ";
        if (m_condition)
            result += m_condition->to_string();
        result += "; ";
        if (m_increment)
            result += m_increment->to_string();
        result += ") " + m_body->to_string();
        return result;
    }

    std::string foreach_statement::to_string() const
    {
        return "foreach (" + m_item->to_string() + " in " + m_collection->to_string() + ") " + m_body->to_string();
    }

    std::string while_statement::to_string() const
    {
        return "while (" + m_condition->to_string() + ") " + m_body->to_string();
    }

    std::string break_statement::to_string() const
    {
        return "break;";
    }

    std::string continue_statement::to_string() const
    {
        return "continue;";
    }

    std::string return_statement::to_string() const
    {
        std::string result = "return";
        if (m_value)
            result += " " + m_value->to_string();
        result += ";";
        return result;
    }

    std::string import_statement::to_string() const
    {
        return "import " + m_module_path + ";";
    }

    token_type variable_declaration::get_type() const
    {
        switch (m_type)
        {
        case wio::variable_type::vt_array:
            return token_type::kw_array;
        case wio::variable_type::vt_dictionary:
            return token_type::kw_dict;
        case wio::variable_type::vt_function:
            return token_type::kw_func;
        default:
            if (m_flags.b1)
                return token_type::kw_const;
            return token_type::kw_var;
        }
    }

    std::string variable_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += frenum::to_string(m_type) + " " + m_id->to_string();
        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ";";
        return result;
    }

    std::string array_declaration::to_string() const
    {
        std::string result = (m_flags.b1 ? "const " : "") + std::string("array ") + m_id->to_string() + " = [";
        for (size_t i = 0; i < m_elements.size(); ++i)
        {
            result += m_elements[i]->to_string();
            if (i < m_elements.size() - 1)
                result += ", ";
        }
        result += "];";
        return result;
    }

    std::string dictionary_declaration::to_string() const
    {
        std::string result = (m_flags.b1 ? "const " : "") + std::string("dict ") + m_id->to_string() + " = {";
        for (size_t i = 0; i < m_pairs.size(); ++i)
        {
            result += "[" + m_pairs[i].first->to_string() + ", ";
            result += m_pairs[i].second->to_string() + "]";
            if (i < m_pairs.size() - 1)
                result += ", ";
        }
        result += "};";
        return result;
    }

    std::string function_declaration::to_string() const
    {
        std::string result = "func " + m_id->to_string() + "(";
        for (size_t i = 0; i < m_params.size(); ++i)
        {
            result += m_params[i]->to_string();

            if (i < m_params.size() - 1)
                result += ", ";
        }
        result += ") " + m_body->to_string();
        return result;
    }

    variable_type function_call::get_expression_type() const
    {
        if (m_caller->get_type() == token_type::identifier)
        {
            /*ref<identifier> id = std::dynamic_pointer_cast<identifier>(m_caller);
            if (id && id->var_ref && id->var_ref->get_base_type() == variable_base_type::function)
            {
                ref<function> func = std::dynamic_pointer_cast<function>(id->var_ref);
                return func->get_return_type();
            }*/
        }
        else if (m_caller->get_type() == token_type::dot) // member function call
        {
            // TODO: Implement member function calls (e.g., obj.method())
            return variable_type::vt_null;
        }
        else if (m_caller->get_type() == token_type::left_bracket) // array or dict access
        {
            // TODO
            return variable_type::vt_null;
        }
        return variable_type::vt_null;
    }

    std::string function_call::to_string() const 
    {
        std::string result = m_caller->to_string() + "(";
        for (size_t i = 0; i < m_arguments.size(); ++i)
        {
            result += m_arguments[i]->to_string();
            if (i < m_arguments.size() - 1)
                result += ", ";
        }
        result += ")";
        return result;
    }
    std::string function_definition::to_string() const
    {
        std::string result = "func " + m_id->to_string() + "(";
        for (size_t i = 0; i < m_params.size(); ++i)
        {
            result += m_params[i]->to_string();

            if (i < m_params.size() - 1)
                result += ", ";
        }
        result += ");";
        return result;
    }
}