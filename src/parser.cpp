#include "parser.h"

namespace wio
{
    ref<program> parser::parse()
    {
        std::vector<ref<statement>> statements;

        while (current_token().type != token_type::end_of_file)
        {
            ref<statement> stmt = parse_statement();
            if (stmt)
                statements.push_back(stmt);
            else
                throw parse_error("Parsing failed!");
        }

        return make_ref<program>(statements);
    }
    token parser::peek_token(int offset) const
    {
        size_t index = m_current_token_index + offset;
        if (index < m_tokens.size())
            return m_tokens[index];
        else
            return token(token_type::end_of_file, "", location());
    }

    token parser::current_token() const
    {
        return peek_token(0);
    }

    token parser::next_token()
    {
        token current = current_token();
        if (m_current_token_index < m_tokens.size())
            m_current_token_index++;
        return current;
    }

    bool parser::match_token(token_type type)
    {
        if (current_token().type == type)
        {
            next_token();
            return true;
        }
        return false;
    }

    bool parser::match_token(token_type type, const std::string& value)
    {
        if (current_token().type == type && current_token().value == value)
        {
            next_token();
            return true;
        }
        return false;
    }

    bool parser::match_token_no_consume(token_type type)
    {
        return (current_token().type == type);
    }

    bool parser::match_token_no_consume(token_type type, const std::string& value)
    {
        return (current_token().type == type && current_token().value == value);
    }

    token parser::consume_token(token_type type)
    {
        if (current_token().type == type)
            return next_token();
        else
            error("Unexpected token: expected " + frenum::to_string(type) + ", but got " +
                frenum::to_string(current_token().type) + '!', current_token().loc);
        return token(token_type::end_of_file, "", location());
    }

    token parser::consume_token(token_type type, const std::string& value)
    {
        if (current_token().type == type && current_token().value == value)
            return next_token();
        else
            error("Unexpected token: expected " + frenum::to_string(type) + " with value of " + value + ", but got " +
                frenum::to_string(current_token().type) + " with value of " + current_token().value + '.', current_token().loc);
        return token(token_type::end_of_file, "", location());
    }

    bool parser::match_ref()
    {
        return (match_token(token_type::kw_ref));
    }

    void parser::error(const std::string& message, location loc)
    {
        throw local_exception(message, loc);
    }

    int parser::get_operator_precedence(token tok)
    {
        if (tok.type == token_type::op)
        {
            std::string op = tok.value;
            if (op == "=") return 1;
            if (op == "||") return 2;
            if (op == "&&") return 3;
            if (op == "==" || op == "!=") return 4;
            if (op == "<" || op == "<=" || op == ">" || op == ">=") return 5;
            if (op == "+" || op == "-") return 6;
            if (op == "*" || op == "/" || op == "%") return 7;
        }
        else if (tok.type == token_type::bang /*|| tok.type == token_type::minus*/) // TODO: keywords.h
        {
            return 8;
        }
        return -1;
    }

    ref<statement> parser::parse_statement()
    {
        token current = current_token();
        bool is_local = false;
        bool is_global = false;

        if (current.value == "global")
        {
            is_global = true;
            next_token();
            current = current_token();
        }
        if (current.value == "local")
        {
            is_local = true;
            next_token();
            current = current_token();
        }

        if (frenum::index(current.type) < frenum::index(token_type::KW_COUNT))
        {
            if (current.type == token_type::kw_const)
            {
                next_token();
                if (current_token().type == token_type::kw_var || current_token().type == token_type::kw_array || current_token().type == token_type::kw_dict)
                {
                    if (current_token().type == token_type::kw_var)
                    {
                        next_token();
                        return parse_variable_declaration(true, is_local, is_global);
                    }
                    else if (current_token().type == token_type::kw_array)
                    {
                        next_token();
                        return parse_array_declaration(true, is_local, is_global);
                    }
                    else if (current_token().type == token_type::kw_dict)
                    {
                        next_token();
                        return parse_dictionary_declaration(true, is_local, is_global);
                    }

                }
                else
                {
                    return parse_variable_declaration(true, is_local, is_global);
                }

            }
            else if (current.type == token_type::kw_var)
            {
                next_token();
                return parse_variable_declaration(false, is_local, is_global);
            }
            else if (current.type == token_type::kw_array)
            {
                next_token();
                return parse_array_declaration(false, is_local, is_global);
            }
            else if (current.type == token_type::kw_dict)
            {
                next_token();
                return parse_dictionary_declaration(false, is_local, is_global);
            }
            else if (current.type == token_type::kw_if)
            {
                next_token();
                return parse_if_statement();
            }
            else if (current.type == token_type::kw_for)
            {
                next_token();
                return parse_for_statement();
            }
            else if (current.type == token_type::kw_while)
            {
                next_token();
                return parse_while_statement();
            }
            else if (current.type == token_type::kw_foreach)
            {
                next_token();
                return parse_foreach_statement();
            }
            else if (current.type == token_type::kw_break)
            {
                location loc = current_token().loc;
                next_token();
                return parse_break_statement(loc);
            }
            else if (current.type == token_type::kw_continue)
            {
                location loc = current_token().loc;
                next_token();
                return parse_continue_statement(loc);
            }
            else if (current.type == token_type::kw_return)
            {
                location loc = current_token().loc;
                next_token();
                return parse_return_statement(loc);
            }
            else if (current.type == token_type::kw_import)
            {
                next_token();
                return parse_import_statement();
            }
            else if (current.type == token_type::kw_func)
            {
                next_token();
                return parse_function_declaration(is_local, is_global);
            }
        }
        else if (current.type == token_type::identifier)
        {
            return parse_identifier();
        }
        else if (current.type == token_type::left_curly_bracket)
        {
            return parse_block_statement();
        }
        else if (current.type == token_type::op)
        {
            auto stmt = make_ref<expression_statement>(parse_unary_expression());
            consume_token(token_type::semicolon);
            return stmt;
        }

        error("Unexpected token in statement: " + frenum::to_string(current.type), current.loc);
        return nullptr;
    }

    ref<expression> parser::parse_expression()
    {
        ref<expression> left = nullptr;
        token current = current_token();

        if (current.type == token_type::kw_typeof)
        {
            left = parse_typeof_expression();
            return left;
        }
        else if (current.type == token_type::kw_null)
        {
            left = parse_null_expression();
            return left;
        }
        else if (current.type == token_type::kw_ref)
        {
            left = parse_ref_expression();
            return left;
        }
        else
        {
            left = parse_binary_expression();
        }

        if (current.type == token_type::left_parenthesis)
            return parse_function_call(left);
        if (current.type == token_type::dot)
            return parse_member_access(left);
        if (current.type == token_type::left_bracket)
            return parse_array_access(left);

        return left;
    }

    ref<expression> parser::parse_primary_expression()
    {
        token current = current_token();

        if (current.type == token_type::number)
        {
            next_token();
            return make_ref<literal>(current);
        }
        else if (current.type == token_type::identifier)
        {
            next_token();
            ref<expression> primary = make_ref<identifier>(current);
            
            if (current_token().type == token_type::op && (current_token().value == "++" || current_token().value == "--"))
            {
                token op = next_token();
                primary = make_ref<unary_expression>(op, primary, unary_operator_type::postfix);
            }
            return primary;
        }
        else if (current.type == token_type::string)
        {
            next_token();
            return make_ref<string_literal>(current);
        }
        else if (current.type == token_type::character)
        {
            next_token();
            return make_ref<literal>(current);
        }
        else if (current.type == token_type::left_parenthesis)
        {
            next_token();
            ref<expression> expr = parse_expression();
            consume_token(token_type::right_parenthesis);
            return expr;
        }
        else if (current_token().value == "true" || current_token().value == "false")
        {
            next_token();
            return make_ref<literal>(current);
        }

        error("Unexpected token in expression: " + frenum::to_string(current.type) + " (" + current.value + ")", current.loc);
        return nullptr;
    }

    ref<expression> parser::parse_null_expression()
    {
        token tok = current_token();
        consume_token(token_type::kw_null);
        return make_ref<null_expression>(tok);
    }


    ref<expression> parser::parse_assignment_expression()
    {
        ref<expression> target = parse_binary_expression(0);

        if (match_token(token_type::op, "="))
        {
            token op = current_token();
            ref<expression> value = parse_assignment_expression();

            if (target->get_type() != token_type::identifier)
            {
                error("Invalid left-hand side of assignment.", op.loc);
                return nullptr;
            }
            return make_ref<assignment_expression>(target, op, value);
        }
        return target;
    }

    ref<expression> parser::parse_binary_expression(int precedence)
    {
        ref<expression> left = parse_unary_expression();

        while (true)
        {
            token op = current_token();
            int current_precedence = get_operator_precedence(op);

            if (current_precedence < precedence)
                break;

            next_token();

            ref<expression> right = parse_binary_expression(current_precedence + 1);

            left = make_ref<binary_expression>(left, op, right);
        }

        return left;
    }


    ref<expression> parser::parse_unary_expression()
    {
        token current = current_token();
        if (current.type == token_type::op && (current.value == "+" || current.value == "-" || current.value == "!" || current.value == "~" || current.value == "++" || current.value == "--"))
        {
            next_token();
            ref<expression> operand = parse_unary_expression();
            return std::make_shared<unary_expression>(current, operand, unary_operator_type::prefix);
        }
        else if (current.type == token_type::bang)
        {
            next_token();
            ref<expression> operand = parse_unary_expression();
            return std::make_shared<unary_expression>(current, operand, unary_operator_type::prefix);
        }
        else if (current.type == token_type::bitwise_not)
        {
            next_token();
            ref<expression> operand = parse_unary_expression();
            return std::make_shared<unary_expression>(current, operand, unary_operator_type::prefix);
        }
        else
        {
            ref<expression> primary = parse_primary_expression();
            current = current_token();
            if (current.type == token_type::op && (current.value == "++" || current.value == "--"))
            {
                token op = next_token();
                return make_ref<unary_expression>(op, primary, unary_operator_type::postfix);
            }
            else if (current.type == token_type::left_bracket)
            {
                return parse_array_access(primary);
            }
            else if (current.type == token_type::dot)
            {
                return parse_member_access(primary);
            }
            else if(current.type == token_type::left_parenthesis)
            { 
                return parse_function_call(primary);
            }
            return primary;
        }
    }

    ref<expression> parser::parse_ref_expression()
    {
        consume_token(token_type::kw_ref);
        token current = current_token();
        if (current.type == token_type::identifier)
        {
            next_token();
            ref<expression> primary = make_ref<identifier>(current, true);

            if (current_token().type == token_type::left_bracket)
                return parse_array_access(primary);
            else if (current_token().type == token_type::dot)
                return parse_member_access(primary);

            return primary;
        }
        throw unexpected_token_error("Expected identifier after 'ref' keyword!");
    }

    ref<expression> parser::parse_function_call(ref<expression> caller)
    {
        std::vector<ref<expression>> arguments;

        consume_token(token_type::left_parenthesis);

        if (!match_token(token_type::right_parenthesis))
        {
            do
            {
                ref<expression> arg = parse_expression();
                arguments.push_back(arg);
            } while (match_token(token_type::comma));

            consume_token(token_type::right_parenthesis);
        }

        return make_ref<function_call>(caller, arguments);
    }

    ref<expression> parser::parse_array_access(ref<expression> array)
    {
        consume_token(token_type::left_bracket);
        ref<expression> index_or_key = parse_expression();
        consume_token(token_type::right_bracket);
        return make_ref<array_access_expression>(std::dynamic_pointer_cast<identifier>(array), index_or_key);
    }

    ref<expression> parser::parse_member_access(ref<expression> object)
    {
        consume_token(token_type::dot);
        token member_name_token = consume_token(token_type::identifier);
        ref<identifier> member_name = make_ref<identifier>(member_name_token);
        return make_ref<member_access_expression>(object, member_name);
    }

    ref<expression> parser::parse_typeof_expression()
    {
        consume_token(token_type::kw_typeof);
        ref<expression> expr = parse_expression();
        return make_ref<typeof_expression>(expr);
    }

    ref<statement> parser::parse_identifier()
    {
        ref<identifier> id = make_ref<identifier>(current_token(), false, true);
        next_token();
        token current = current_token();

        if (match_token_no_consume(token_type::op, "=") ||
            match_token_no_consume(token_type::op, "+=") ||
            match_token_no_consume(token_type::op, "-=") ||
            match_token_no_consume(token_type::op, "*=") || 
            match_token_no_consume(token_type::op, "/=") ||
            match_token_no_consume(token_type::op, "%="))
        {
            token tok = next_token();
            ref<expression> value = parse_assignment_expression();
            consume_token(token_type::semicolon);
            return make_ref<expression_statement>(make_ref<assignment_expression>(id, tok, value));
        }
        else if (match_token_no_consume(token_type::op, "++") || match_token_no_consume(token_type::op, "++"))
        {
            token tok = next_token();
            consume_token(token_type::semicolon);
            return make_ref<expression_statement>(make_ref<unary_expression>(tok, id));
        }
        else if (current_token().type == token_type::left_parenthesis)
        {
            ref<expression> call = parse_function_call(id);
            consume_token(token_type::semicolon);
            return make_ref<expression_statement>(call);
        }
        else if (current_token().type == token_type::left_bracket)
        {
            ref<expression> access = parse_array_access(id);
            token tok = current_token();
            if (match_token_no_consume(token_type::op, "=") ||
                match_token_no_consume(token_type::op, "+=") ||
                match_token_no_consume(token_type::op, "-=") ||
                match_token_no_consume(token_type::op, "*=") ||
                match_token_no_consume(token_type::op, "/=") ||
                match_token_no_consume(token_type::op, "%="))
            {
                token tok = next_token();
                ref<expression> value = parse_assignment_expression();
                consume_token(token_type::semicolon);
                return make_ref<expression_statement>(make_ref<assignment_expression>(access, tok, value));
            }
            else
            {
                consume_token(token_type::semicolon);
                return make_ref<expression_statement>(access);
            }

        }
        else
        {
            error("Expected '=' or '(' or '[' after identifier.", current.loc);
            return nullptr;
        }
    }

    ref<statement> parser::parse_variable_declaration(bool is_const, bool is_local, bool is_global)
    {
        token id_token = consume_token(token_type::identifier);
        ref<identifier> id = make_ref<identifier>(id_token);

        ref<expression> initializer = nullptr;
        if (match_token(token_type::op, "="))
            initializer = parse_expression();

        consume_token(token_type::semicolon);

        return make_ref<variable_declaration>(id, initializer, is_const, is_local, is_global, false);
    }

    ref<statement> parser::parse_array_declaration(bool is_const, bool is_local, bool is_global)
    {
        token id_token = consume_token(token_type::identifier);
        ref<identifier> id = make_ref<identifier>(id_token);

        bool is_element_initializer = true;
        ref<expression> exp;
        std::vector<ref<expression>> elements;

        if (match_token(token_type::op, "="))
        {
            if (match_token(token_type::left_bracket))
            {
                if (!match_token(token_type::right_bracket))
                {
                    do {
                        ref<expression> element = parse_expression();
                        elements.push_back(element);
                    } while (match_token(token_type::comma));

                    consume_token(token_type::right_bracket);
                }
            }
            else if (current_token().type == (token_type::kw_null))
            {
                next_token();
                is_element_initializer = false;
                exp = nullptr;
            }
            else if (current_token().type == (token_type::kw_ref))
            {
                consume_token(token_type::kw_ref);
                token current = current_token();
                ref<expression> result;
                if (current.type == token_type::identifier)
                {
                    next_token();
                    result = make_ref<identifier>(current, true);
                }
                else
                {
                    error("Expected identifier after 'ref' keyword!", current.loc);
                }
                is_element_initializer = false;
                exp = result;
            }
            else if (current_token().type == (token_type::identifier))
            {
                ref<identifier> result = make_ref<identifier>(current_token());
                is_element_initializer = false;
                next_token();
                exp = result;
            }
            else
            {
                error("Expected '[' or identifier after '=' in array declaration.", current_token().loc);
                return nullptr;
            }

        }
        else if (match_token(token_type::left_bracket))
        {
            if (!match_token(token_type::right_bracket))
            {
                do {
                    ref<expression> element = parse_expression();
                    elements.push_back(element);
                } while (match_token(token_type::comma));

                consume_token(token_type::right_bracket);
            }
        }
        consume_token(token_type::semicolon);
        return make_ref<array_declaration>(id, exp, elements, is_const, is_local, is_global, is_element_initializer);
    }

    ref<statement> parser::parse_dictionary_declaration(bool is_const, bool is_local, bool is_global)
    {
        bool is_ref = match_ref();
        token id_token = consume_token(token_type::identifier);
        ref<identifier> id = make_ref<identifier>(id_token);

        std::vector<std::pair<ref<expression>, ref<expression>>> pairs;

        bool has_initializer = false;
        if (match_token(token_type::op, "="))
        {
            has_initializer = true;
            consume_token(token_type::left_curly_bracket);
        }
        else if (match_token(token_type::left_curly_bracket))
        {
            has_initializer = true;
        }

        if (has_initializer)
        {
            if (!match_token(token_type::right_curly_bracket))
            {
                do {
                    consume_token(token_type::left_bracket);
                    ref<expression> key = parse_expression();
                    consume_token(token_type::comma);
                    ref<expression> value = parse_expression();
                    consume_token(token_type::right_bracket);

                    pairs.push_back(std::make_pair(key, value));
                } while (match_token(token_type::comma));

                consume_token(token_type::right_curly_bracket);
            }
        }

        consume_token(token_type::semicolon);
        return make_ref<dictionary_declaration>(id, pairs, is_const, is_local, is_global);
    }

    ref<statement> parser::parse_function_declaration(bool is_local, bool is_global)
    {
        token id_token = consume_token(token_type::identifier);
        ref<identifier> id = make_ref<identifier>(id_token);

        consume_token(token_type::left_parenthesis);

        std::vector<ref<variable_declaration>> params;
        if (!match_token(token_type::right_parenthesis))
        {
            do {
                ref<statement> param_decl = parse_parameter_declaration();
                if (auto var_decl = std::dynamic_pointer_cast<variable_declaration>(param_decl))
                {
                    params.push_back(var_decl);
                }
                else
                {
                    error("Invalid parameter declaration.", current_token().loc);
                    return nullptr;
                }

            } while (match_token(token_type::comma));

            consume_token(token_type::right_parenthesis);
        }

        if(match_token(token_type::semicolon))
            return make_ref<function_definition>(id, params, is_local, is_global);

        ref<block_statement> body = parse_block_statement();
        return make_ref<function_declaration>(id, params, body, variable_type::vt_null, is_local, is_global);
    }

    ref<statement> parser::parse_parameter_declaration()
    {
        token type_token = next_token();
        if (type_token.type != token_type::kw_var && type_token.type != token_type::kw_array && type_token.type != token_type::kw_dict)
        {
            error("Expected 'var', 'array' or 'dict' in parameter declaration.", type_token.loc);
            return nullptr;
        }

        bool is_ref = match_ref();
        token id_token = consume_token(token_type::identifier);
        ref<identifier> id = make_ref<identifier>(id_token);

        variable_type type = variable_type::vt_null;
        if (type_token.value == "var")
            type = variable_type::vt_var_param;
        else if (type_token.value == "array")
            type = variable_type::vt_array;
        else if (type_token.value == "dict")
            type = variable_type::vt_dictionary;

        return make_ref<variable_declaration>(id, nullptr, false, true, false, is_ref, type);
    }

    ref<statement> parser::parse_if_statement()
    {
        consume_token(token_type::left_parenthesis);

        ref<expression> condition = parse_expression();
        consume_token(token_type::right_parenthesis);

        ref<statement> then_branch;
        if (current_token().type == token_type::left_curly_bracket)
            then_branch = parse_block_statement();
        else
            then_branch = parse_statement();

        ref<statement> else_branch = nullptr;
        if (current_token().type == token_type::kw_else)
        {
            next_token();
            if (current_token().type == token_type::left_curly_bracket)
                else_branch = parse_block_statement();
            else
                else_branch = parse_statement();
        }

        return make_ref<if_statement>(condition, then_branch, else_branch);
    }

    ref<statement> parser::parse_for_statement()
    {
        consume_token(token_type::left_parenthesis);

        ref<statement> initialization = nullptr;
        if (current_token().type == token_type::semicolon)
            next_token();
        else
            initialization = parse_statement();

        ref<expression> condition = nullptr;
        if (current_token().type != token_type::semicolon)
            condition = parse_assignment_expression();
        consume_token(token_type::semicolon);

        ref<expression> increment = nullptr;
        if (current_token().type != token_type::right_parenthesis)
            increment = parse_expression();

        consume_token(token_type::right_parenthesis);

        ref<block_statement> body;
        if (current_token().type == token_type::left_curly_bracket)
        {
            body = parse_block_statement();
        }
        else
        {
            body = std::static_pointer_cast<block_statement>(parse_statement());
            std::vector<ref<statement>> statements;
            statements.push_back(body);
            body = make_ref<block_statement>(statements);
        }

        return make_ref<for_statement>(initialization, condition, increment, body);
    }

    ref<statement> parser::parse_foreach_statement()
    {
        consume_token(token_type::left_parenthesis);

        token item_token = consume_token(token_type::identifier);
        ref<identifier> item = make_ref<identifier>(item_token);

        consume_token(token_type::kw_in);

        ref<expression> collection = parse_expression();

        consume_token(token_type::right_parenthesis);

        ref<block_statement> body = parse_block_statement();

        return make_ref<foreach_statement>(item, collection, body);
    }

    ref<statement> parser::parse_while_statement()
    {
        consume_token(token_type::left_parenthesis);
        ref<expression> condition = parse_expression();
        consume_token(token_type::right_parenthesis);
        ref<block_statement> body = parse_block_statement();
        return make_ref<while_statement>(condition, body);
    }

    ref<statement> parser::parse_break_statement(location loc)
    {
        consume_token(token_type::semicolon);
        return make_ref<break_statement>(loc);
    }

    ref<statement> parser::parse_continue_statement(location loc)
    {
        consume_token(token_type::semicolon);
        return make_ref<continue_statement>(loc);
    }

    ref<statement> parser::parse_return_statement(location loc)
    {
        ref<expression> return_value = nullptr;
        if (current_token().type != token_type::semicolon)
            return_value = parse_expression();

        consume_token(token_type::semicolon);
        return make_ref<return_statement>(loc, return_value);
    }

    ref<statement> parser::parse_import_statement()
    {
        token current = current_token();
        ref<expression> module_path_expr = parse_primary_expression();

        if (module_path_expr->get_expression_type() != variable_type::vt_string)
        {
            error("Expected a string literal for the module path in import statement.", module_path_expr->get_location());
            return nullptr;
        }

        std::string module_path = std::dynamic_pointer_cast<literal>(module_path_expr)->m_token.value;

        consume_token(token_type::semicolon);

        return make_ref<import_statement>(module_path_expr->get_location(), module_path);
    }

    ref<block_statement> parser::parse_block_statement()
    {
        consume_token(token_type::left_curly_bracket);

        std::vector<ref<statement>> statements;
        while (!match_token(token_type::right_curly_bracket))
        {
            ref<statement> stmt = parse_statement();
            if (stmt)
            {
                statements.push_back(stmt);
            }
            else
            {
                error("Error parsing statement inside block.", current_token().loc);
                return nullptr;

            }
        }

        return make_ref<block_statement>(statements);
    }

} // namespace wio