#pragma once

#include <vector>

#include "../base/exception.h"
#include "ast.h"

namespace wio
{

    class parser
    {
    public:
        parser(const std::vector<token>& tokens) : m_tokens(tokens), m_current_token_index(0) {}

        ref<program> parse();

    private:
        std::vector<token> m_tokens;
        size_t m_current_token_index;

        token peek_token(int offset = 0) const;
        token current_token() const;
        token next_token();
        bool match_token(token_type type);
        bool match_token(token_type type, const std::string& value);
        bool match_token_no_consume(token_type type);
        bool match_token_no_consume(token_type type, const std::string& value);
        token consume_token(token_type type);
        token consume_token(token_type type, const std::string& value);
        bool match_ref();

        void error(const std::string& message, location loc);

        int get_operator_precedence(token tok);
        location get_array_elements(std::vector<ref<expression>>& elements);
        location get_dict_element(std::vector<std::pair<ref<expression>, ref<expression>>>& elements);

        ref<statement> parse_statement();
        ref<expression> parse_expression();
        ref<expression> parse_primary_expression();
        ref<expression> parse_null_expression();
        ref<expression> parse_assignment_expression();
        ref<expression> parse_binary_expression(int precedence = 0);
        ref<expression> parse_unary_expression();
        ref<expression> parse_ref_expression();
        ref<expression> parse_function_call(ref<expression> caller);
        ref<expression> parse_array_access(ref<expression> array);
        ref<expression> parse_member_access(ref<expression> object);
        ref<expression> parse_typeof_expression();

        ref<statement> parse_identifier();
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