#pragma once

#include <vector>
#include <string>

#include "token.h"

namespace wio
{
    class lexer
    {
    public:
        explicit lexer(const std::string& src) : m_source(src) {}

        std::vector<token>& get_tokens();
        std::string to_string();
    private:
        char peek() const;
        char peek(int offset) const;
        char advance();
        bool is_operator(char ch);
        bool skip_whitespace();
        bool skip_comments();
        token read_identifier();
        token read_number();
        token read_string();
        token read_char();
        token read_operator();

        std::vector<token> m_tokens;
        std::string m_source;
        size_t m_pos = 0;
        location m_loc;
    };
}