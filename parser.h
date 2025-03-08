#pragma once

#include <vector>
#include <string>

#include "base.h"
#include "exception.h"
#include "token.h"
#include "ast.h"

namespace wio
{

    class parser
    {
    public:
        parser(const std::vector<token>& tokens) : m_tokens(tokens), m_current_token_index(0) {}

        ref<ast_node> parse();

    private:
        std::vector<token> m_tokens;
        size_t m_current_token_index;

        token peek_token(int offset = 0) const;
        token current_token() const;
        token next_token();
        bool match_token(token_type type);
        bool match_token(token_type type, const std::string& value);
        token consume_token(token_type type);
        token consume_token(token_type type, const std::string& value);

        void error(const std::string& message, location loc);

        int get_operator_precedence(token tok);

        ref<statement> parse_statement();
        ref<expression> parse_expression();
        ref<expression> parse_primary_expression();
        ref<expression> parse_assignment_expression();
        ref<expression> parse_binary_expression(int precedence = 0);
        ref<expression> parse_unary_expression();
        ref<expression> parse_function_call(ref<expression> caller);
        ref<expression> parse_array_access(ref<expression> array);
        ref<expression> parse_dictionary_access(ref<expression> dict);
        ref<expression> parse_member_access(ref<expression> object);
        ref<expression> parse_typeof_expression();

        ref<statement> parse_assignment_or_function_call();
        ref<statement> parse_variable_declaration(bool is_const, bool is_local, bool is_global);
        ref<statement> parse_array_declaration(bool is_const, bool is_local, bool is_global);
        ref<statement> parse_dictionary_declaration(bool is_const, bool is_local, bool is_global);
        ref<statement> parse_function_declaration(bool is_local, bool is_global);
        ref<statement> parse_parameter_declaration();
        ref<statement> parse_if_statement();
        ref<statement> parse_for_statement();
        ref<statement> parse_foreach_statement();
        ref<statement> parse_while_statement();
        ref<statement> parse_break_statement(location loc);
        ref<statement> parse_continue_statement(location loc);
        ref<statement> parse_return_statement(location loc);
        ref<statement> parse_import_statement();
        ref<block_statement> parse_block_statement();
    };

} // namespace wio