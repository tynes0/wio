#pragma once

#include <vector>
#include <cctype>

#include "exception.h"
#include "token.h"

namespace wio
{
    class lexer
    {
    public:
        explicit lexer(std::string src) : m_source(std::move(src)) {}

        std::vector<token> get_tokens()
        {
            std::vector<token> tokens;

            m_loc = { 1, 1 };

            while (m_pos < m_source.size())
            {
                bool res = false;
                do
                {
                    if (skip_whitespace() || skip_comments())
                        res = true;
                    else
                        res = false;
                } while (res == true);

                char current = peek();


                if (std::isalpha(current) || current == '_')
                {
                    tokens.push_back(read_identifier());
                }
                else if (std::isdigit(current))
                {
                    tokens.push_back(read_number());
                }
                else if (current == '\"')
                {
                    tokens.push_back(read_string());
                }
                else if (current == '\'')
                {
                    tokens.push_back(read_char());
                }
                else if (is_operator(current))
                {
                    tokens.push_back(read_operator());
                }

                else if (is_seperator(current))
                {
                    std::string op(1, advance());
                    tokens.push_back({ token_map.at(op), op, m_loc });
                }
                else if (current == '\0')
                {
                    tokens.push_back({ token_map.at("eof"), "eof", m_loc });
                    break;
                }
                else
                {
                    throw unexpected_character_error("Unexpected char " + current, m_loc);
                }
            }

            return tokens;
        }


    private:
        std::string m_source;
        size_t m_pos = 0;
        location m_loc;

        char peek() const
        {
            return m_pos < m_source.size() ? m_source[m_pos] : '\0';
        }

        char peek(int offset) const
        {
            return (m_pos + offset) < m_source.size() ? m_source[m_pos + offset] : '\0';
        }

        char advance()
        {
            m_loc.row++;
            return m_pos < m_source.size() ? m_source[m_pos++] : '\0';
        }

        bool skip_whitespace()
        {
            bool result = false;
            if (std::isspace(peek()))
                result = true;
            while (std::isspace(peek()))
            {
                if (advance() == '\n')
                {
                    m_loc.line++;
                    m_loc.row = 1;
                }
            }

            return result;
        }

        bool skip_comments()
        {
            if (peek() == '#' && peek(1) == '#')
            {
                while (peek() != '\n' && peek() != '\0')
                    advance();
                return true;
            }
            else if (peek() == '#' && peek(1) == '*')
            {
                advance(); advance();
                while (peek() != '\0')
                {
                    if (peek() == '*' && peek(1) == '#')
                    {
                        advance(); advance();
                        return true;
                    }
                    advance();
                }
                throw unterminated_comment_error("Unterminated multi-line comment!", m_loc);
            }
            return false;
        }

        token read_identifier()
        {
            std::string result;

            while (std::isalnum(peek()) || peek() == '_')
                result += advance();

            if (token_map.count(result) && frenum::index(token_map.at(result)) < frenum::index(token_type::KW_COUNT))
                return { token_map.at(result), result, m_loc };

            return { token_type::identifier, result, m_loc };
        }

        token read_number()
        {
            std::string result;
            bool is_float = false, is_scientific = false;

            if (peek() == '-')
            {
                result += '-';
                advance();
            }

            if (peek() == '0')
            {
                char next = peek(1);
                if (next == 'b' || next == 'B')
                {
                    result += advance(); result += advance();
                    if (peek() != '0' && peek() != '1')
                        throw invalid_number_error("Invalid binary number!", m_loc);

                    while (peek() == '0' || peek() == '1')
                        result += advance();

                    if (std::isalnum(peek()))
                        throw invalid_number_error("Invalid character in binary number!", m_loc);
                }
                else if (next == 'x' || next == 'X') 
                {
                    result += advance(); result += advance();
                    if (!std::isxdigit(peek()))
                        throw invalid_number_error("Invalid hexadecimal number!", m_loc);

                    while (std::isxdigit(peek()))
                        result += advance();

                    if (std::isalnum(peek()))
                        throw invalid_number_error("Invalid character in hexadecimal number!", m_loc);
                }
                else if (next == 'o' || next == 'O')
                {
                    result += advance(); result += advance();
                    if (peek() < '0' || peek() > '7')
                        throw invalid_number_error("Invalid octal number!", m_loc);

                    while (peek() >= '0' && peek() <= '7')
                        result += advance();

                    if (std::isalnum(peek()))
                        throw invalid_number_error("Invalid character in octal number!", m_loc);
                }
            }

            if (result.empty())
            {
                while (std::isdigit(peek()))
                    result += advance();
            }

            if (peek() == '.')
            {
                is_float = true;
                result += advance();
                if (!std::isdigit(peek()))
                    throw invalid_number_error("Invalid float number!", m_loc);

                while (std::isdigit(peek()))
                    result += advance();
            }

            if (peek() == 'e' || peek() == 'E')
            {
                is_scientific = true;
                result += advance();
                if (peek() == '+' || peek() == '-')
                    advance();

                if (!std::isdigit(peek()))
                    throw invalid_number_error("Invalid scientific notation!", m_loc);

                while (std::isdigit(peek()))
                    result += advance();
            }

            return { token_type::number, result, m_loc };
        }

        token read_string()
        {
            std::string result;
            advance();

            while (peek() != '"' && peek() != '\0')
            {
                if (peek() == '\\')
                {
                    advance();
                    char escape_char = peek();
                    switch (escape_char)
                    {
                    case '"':
                    case '\\':
                        result += advance();
                        break;
                    case 'n':
                        result += '\n';
                        advance();
                        break;
                    case 't':
                        result += '\t';
                        advance();
                        break;
                    case 'r':
                        result += '\r';
                        advance();
                        break;
                    default:
                        throw invalid_string_error("Invalid escape sequence: \\" + std::string(1, escape_char), m_loc);

                    }
                }
                else
                {
                    result += advance();
                }
            }

            advance();

            return { token_type::string, result, m_loc };
        }

        token read_char()
        {
            advance();
            std::string op(1, advance());
            token result({ token_type::character, op, m_loc });
            advance();
            return result;
        }

        token read_operator()
        {
            std::string op(1, peek());

            if (token_map.contains(op + peek(1) + peek(2)))
            {
                op += advance();
                op += advance();
            }
            else if (token_map.contains(op + peek(1)))
            {
                op += advance();
            }

            advance();
            return { token_map.at(op), op, m_loc };
        }

        bool is_operator(char ch)
        {
            std::string tok(1, ch);
            auto it = token_map.find(tok);
            return (it != token_map.end() && it->second == token_type::op);
        }

        bool is_seperator(char ch)
        {
            return (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' || ch == '.' || ch == ',' || ch == ';');
        }
    };
}