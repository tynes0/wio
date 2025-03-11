#pragma once

#include <vector>
#include <cctype>
#include <iostream>

#include "exception.h"
#include "token.h"

namespace wio
{
    class lexer
    {
    public:
        explicit lexer(std::string src) : m_source(std::move(src)) {}

        std::vector<token>& get_tokens()
        {
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
                    m_tokens.push_back(read_identifier());
                }
                else if (std::isdigit(current))
                {
                    m_tokens.push_back(read_number());
                }
                else if (current == '\"')
                {
                    m_tokens.push_back(read_string());
                }
                else if (current == '\'')
                {
                    m_tokens.push_back(read_char());
                }
                else if (current == '!')
                {
                    std::string op(1, advance());
                    m_tokens.push_back({ token_map.at(op), op, m_loc });
                }
                else if (is_operator(current))
                {
                    m_tokens.push_back(read_operator());
                }
                else if (is_seperator(current))
                {
                    std::string op(1, advance());
                    m_tokens.push_back({ token_map.at(op), op, m_loc });
                }
                else if (is_bitwise_op(current))
                {
                    std::string op(1, advance());
                    m_tokens.push_back({ token_map.at(op), op, m_loc });
                }
                else if (current == '\0')
                {
                    m_tokens.push_back({ token_map.at("eof"), "eof", m_loc });
                    break;
                }
                else
                {
                    throw unexpected_character_error("Unexpected char " + current, m_loc);
                }
            }

            return m_tokens;
        }

        void print_tokens()
        {
            for (const auto& token : m_tokens)
            {
                std::cout << "Token: " << frenum::to_string(token.type);
                if (frenum::to_string(token.type).size() < 9)
                    std::cout << "\t\t";
                else if (frenum::to_string(token.type).size() < 17)
                    std::cout << "\t";
                std::cout << "\t";
                std::cout << "Value: " << token.value << std::endl;
            }
        }


    private:
        std::vector<token> m_tokens;
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
                    m_loc.row = 0;
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
            std::string op(1, advance());

            if (token_map.contains(op + peek() + peek(1)))
            {
                op += advance();
                op += advance();
            }
            else if (token_map.contains(op + peek()))
            {
                op += advance();
            }

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

        bool is_bitwise_op(char ch)
        {
            return (ch == '~' || ch == '|' || ch == '&');
        }
    };
}