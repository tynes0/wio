#include "ast.h"
#include "../base/exception.h"
#include "../utils/util.h"

namespace wio
{
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

    literal::literal(token tok)  : m_token(tok)
    {
        switch (tok.type)
        {
        case token_type::number:
        {
            std::string_view sv(tok.value);

            if (sv.find('.') != std::string_view::npos)
                m_type = lit_type::lt_float;
            else if(sv.starts_with("0b") || sv.starts_with("0B"))
                m_type = lit_type::lt_binary;
            else if(sv.starts_with("0o") || sv.starts_with("0O"))
                m_type = lit_type::lt_octal;
            else if(sv.starts_with("0x") || sv.starts_with("0X"))
                m_type = lit_type::lt_hexadeximal;
            else
                m_type = lit_type::lt_decimal;
            break;
        }
        case token_type::string:
        {
            m_type = lit_type::lt_string;
            break;
        }
        case token_type::character:
        {
            m_type = lit_type::lt_character;
            break;
        }
        case token_type::kw_true:
        case token_type::kw_false:
        {
            m_type = lit_type::lt_bool;
            break;
        }
        default:
        {
            m_type = lit_type::lt_null;
        }
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

    std::string lambda_literal::to_string() const
    {
        return std::string();
    }

    null_expression::null_expression(token tok) : m_token(tok)
    {
    }

    std::string null_expression::to_string() const
    {
        return "null";
    }

    identifier::identifier(token tok, bool is_ref, bool is_lhs) : m_token(tok), m_is_ref(is_ref), m_is_lhs(is_lhs)
    {
    }

    std::string identifier::to_string() const
    {
        return "Identifier(" + m_token.value + ")";
    }

    std::string binary_expression::to_string() const
    {
        return "BinaryExpr(" + m_left->to_string() + " " + m_operator.value + " " + m_right->to_string() + ")";
    }

    std::string unary_expression::to_string() const
    {
        if (m_op_type == unary_operator_type::prefix)
            return "UnaryExpr(" + m_operator.value + " " + m_operand->to_string() + ")";
        return "UnaryExpr(" + m_operand->to_string() + " " + m_operator.value + ")";
    }

    std::string typeof_expression::to_string() const
    {
        return "typeof " + m_expr->to_string();
    }

    std::string array_access_expression::to_string() const
    {
        return m_array->to_string() + "[" + m_key_or_index->to_string() + "]";
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

    std::string realm_declaration::to_string() const
    {
        return "realm " + m_id->to_string() + "\n{\n" + m_body->to_string() + "\n}";
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

    std::string variable_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += "var " + m_id->to_string();
        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ";";
        return result;
    }

    std::string array_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += std::string("array ") + m_id->to_string();

        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ";";
        return result;
    }

    std::string dictionary_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += std::string("dict ") + m_id->to_string();

        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ";";
        return result;
    }

    std::string function_definition::to_string() const
    {
        std::string result;
        if (is_global())
            result += "global ";
        if (is_local())
            result += "local ";

        result += "func " + m_id->to_string() + "(";
        for (size_t i = 0; i < m_params.size(); ++i)
        {
            result += m_params[i]->to_string();

            if (i < m_params.size() - 1)
                result += ", ";
        }
        result += ") " + m_body->to_string();
        return result;
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

    std::string new_unit_instance_call::to_string() const
    {
        std::string result = m_caller->to_string() + "{";
        for (size_t i = 0; i < m_arguments.size(); ++i)
        {
            result += m_arguments[i]->to_string();
            if (i < m_arguments.size() - 1)
                result += ", ";
        }
        result += "}";
        return result;
    }

    std::string function_declaration::to_string() const
    {
        std::string result;
        if (is_global())
            result += "global ";
        if (is_local())
            result += "local ";

        result += "func " + m_id->to_string() + "(";
        for (size_t i = 0; i < m_params.size(); ++i)
        {
            result += m_params[i]->to_string();

            if (i < m_params.size() - 1)
                result += ", ";
        }
        result += ");";
        return result;
    }

    std::string lambda_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += "func " + m_id->to_string();
        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ';';
        return result;
    }

    std::string omni_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";
        if (m_flags.b1)
            result += "const ";

        result += "omni " + m_id->to_string();
        if (m_initializer)
            result += " = " + m_initializer->to_string();
        result += ';';
        return result;
    }

    std::string enum_declaration::to_string() const
    {
        std::string result;
        if (m_is_global)
            result += "global ";
        if (m_is_local)
            result += "local ";

        result += "enum " + m_id->to_string();
        result += '{';
        for (size_t i = 0; i < m_items.size(); ++i)
        {
            result += m_items[i].first->to_string();
            if (m_items[i].second)
                result += " = " + m_items[i].second->to_string();
            if (i != m_items.size() - 1)
                result += ", ";
        }
        result += '}';
        return result;
    }

    std::string parameter_declaration::to_string() const
    {
        std::string result;
        if (m_is_ref)
            result += "ref";
        result += frenum::to_string(m_type) + m_id->to_string();
        return result;
    }

    std::string unit_declaration::to_string() const
    {
        std::string result;
        if (m_flags.b3)
            result += "global ";
        if (m_flags.b2)
            result += "local ";

        result += "unit " + m_id->to_string();

        if (m_flags.b1)
            result += " final ";
        result += "access ";

        //if (m_default_decl == unit_declaration_type::hidden)
        //    result += "hidden ";
        //else if (m_default_decl == unit_declaration_type::exposed)
        //    result += "exposed ";
        //else if (m_default_decl == unit_declaration_type::shared)
        //    result += "shared ";

        if (!m_parent_list.empty())
            result += "from ";

        for (size_t i = 0; i < m_parent_list.size(); ++i)
        {
            result += m_parent_list[i]->to_string();
            if (i != m_parent_list.size() - 1)
                result += ", ";
        }

        if (!m_trust_list.empty())
            result += "trust ";

        for (size_t i = 0; i < m_trust_list.size(); ++i)
        {
            result += m_trust_list[i]->to_string();
            if (i != m_trust_list.size() - 1)
                result += ", ";
        }


        return std::string();
    }

    std::string unit_member_declaration::to_string() const
    {
        std::string result;

        //if (m_default_decl == unit_declaration_type::hidden)
        //    result += "hidden ";
        //else if (m_default_decl == unit_declaration_type::exposed)
        //    result += "exposed ";
        //else if (m_default_decl == unit_declaration_type::shared)
        //    result += "shared ";
        //else if (m_flags.b4)
        //    result += "outer ";

        result += m_decl_statement->to_string();
        return result;
    }
}