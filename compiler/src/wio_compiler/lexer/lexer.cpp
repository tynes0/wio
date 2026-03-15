#include "wio/lexer/lexer.h"
#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/common/utility.h"

#include <ranges>

namespace wio
{
    using namespace common;
    
    Lexer::Lexer(std::string source)
        : source_(std::move(source)), position_(0), location_ { .line = 0, .column = 0 }
    {
    }

    std::vector<Token> Lexer::lex()
    {
        location_ = {
            .line = 1,
            .column = 1
        };
        
        
        while (!isAtEnd())
        {
            if (flags_.get_inInterpolatedString() && !flags_.get_inInterpolatedExpr())
            {
                readString();
                continue;
            }
            bool res;
            do {
                res = (skipWhitespaces() || skipComments());
            } while (res);

            if (std::isalpha(upeek()) || match('_'))
            {
                tokens_.push_back(readIdentifier());
            }
            else if (std::isdigit(upeek()))
            {
                tokens_.push_back(readNumber());
            }
            else if (match('\"'))
            {
                readString();
            }
            else if (match('\''))
            {
                tokens_.push_back(readChar());
            }
            else if (isOperator(peek()))
            {
                tokens_.push_back(readOperator());
            }
            else if (isSymbol(peek()))
            {
                Token symToken = readSymbol();
                if (symToken.isValid()) // Skip Multiline $ sign
                    tokens_.push_back(symToken);
            }
            else if (isAtEnd())
            {
                // It's probably not necessary, but it's okay to have it here for security reasons.
                tokens_.emplace_back(TokenType::endOfFile, "eof", location_);
                break;
            }
            else
            {
                throw UnexpectedCharError((std::string("Unexpected char (") + peek() + ")").c_str(), location_);
            }
        }

        return tokens_;
    }

    std::string Lexer::toString() const
    {
        auto escapeForDebug = [](std::string_view s)
        {
            std::string out;
            out.reserve(s.size());

            for (unsigned char c : s)
            {
                switch (c)
                {
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                case '\v': out += "\\v"; break;
                case '\f': out += "\\f"; break;
                case '\\': out += "\\\\"; break;
                case '\"': out += "\\\""; break;
                case '\'': out += "\\\'"; break;
                case '\0': out += "\\0"; break;

                default:
                    if (std::isprint(c))
                    {
                        out += static_cast<char>(c);
                    }
                    else
                    {
                        char buf[5];
                        (void)std::snprintf(buf, sizeof(buf), "\\x%02X", c);
                        out += buf;
                    }
                }
            }
            return out;
        };

        std::stringstream ss;

        for (const auto& token : tokens_)
        {
            const std::string typeStr = frenum::to_string(token.type);

            ss << "Token: " << typeStr;
            if (typeStr.size() < 9)
                ss << "\t\t";
            else if (typeStr.size() < 17)
                ss << "\t";
            ss << "\t";

            ss << "Value: " << escapeForDebug(token.value) << '\n';
        }

        return ss.str();
    }


    char Lexer::peek(int offset) const
    {
        return position_ + offset < source_.length() ? source_[position_ + offset] : static_cast<char>(0);
    }

    unsigned char Lexer::upeek(int offset) const
    {
        return static_cast<unsigned char>(peek(offset));
    }
    
    bool Lexer::match(char c, int offset) const
    {
        return peek(offset) == c;
    }

    bool Lexer::multiMatch(std::string_view chars, int offset) const
    {
        for (size_t i = 0; i < chars.size(); ++i)
        {
            if (!match(chars[i], offset + static_cast<int>(i)))
                return false;
        }
        return true;
    }

    bool Lexer::matchOneOf(std::string_view chars, int offset) const
    {
        return std::ranges::any_of(chars, [&](const char c) { return peek(offset) == c; });
    }

    char Lexer::advance()
    {
        location_.column++;
        return position_ < source_.size() ? source_[position_++] : '\0';
    }

    void Lexer::advance(int count)
    {
        while (count--) advance();
    }
    
    bool Lexer::skipWhitespaces()
    {
        bool result = false;

        while (std::isspace(upeek()))
        {
            result = true;
            advance();
            skipNewline();
        }

        return result;
    }


    bool Lexer::skipComments()
    {
        if (match('#'))
        {
            if (match('*', 1))
            {
                advance(2);
                while (!isAtEnd())
                {
                    skipNewline();
                    if (multiMatch("*#"))
                    {
                        advance(2);
                        return true;
                    }
                    advance();
                }
                throw UnterminatedCommentError("Unterminated multi-line comment!", location_);
            }
            
            while (!matchOneOf("\r\n") && !isAtEnd())
                advance();
            return true;
        }

        if (multiMatch("//"))
        {
            while (!matchOneOf("\r\n") && !isAtEnd())
                advance();
            return true;
        }
        
        return false;
    }

    bool Lexer::skipNewline()
    {
        // CRLF or CR
        if (match('\r'))
        {
            advance(); // '\r'

            if (match('\n'))
            {
                advance(); // '\n' (CRLF)
            }
        }
        // LF
        else if (match('\n'))
        {
            advance(); // '\n'
        }
        else
        {
            return false;
        }

        // Location update
        location_.line++;
        location_.column = 1;
        return true;
    }


    Token Lexer::readIdentifier()
    {
        Location start = location_;
        std::string result;

        while (std::isalnum(upeek()) || match('_'))
            result += advance();

        TokenType tType = TokenType::identifier;
        
        if (auto it = keywordMap.find(result); it != keywordMap.end())
            tType = it->second;
        
        return Token{
            .type = tType,
            .value = std::move(result),
            .loc = start
        };
    }
    
    Token Lexer::readNumber()
    {
        Location start = location_;
        std::string result;
        bool isFloat = false;

        if (match('0'))
        {
            result += advance();
            if (matchOneOf("bB"))
            {
                result += advance();
                
                if (!matchOneOf("01"))
                    throw InvalidNumberError("Invalid binary number!", location_);

                while (matchOneOf("01"))
                    result += advance();

                if (std::isalnum((upeek())))
                    throw InvalidNumberError("Invalid character in binary number!", location_);
            }
            else if (matchOneOf("xX"))
            {
                result += advance();
                
                if (!std::isxdigit(upeek()))
                    throw InvalidNumberError("Invalid hexadecimal number!", location_);

                while (std::isxdigit(upeek()))
                    result += advance();

                if (std::isalnum(peek()))
                    throw InvalidNumberError("Invalid character in hexadecimal number!", location_);
            }
            else if (matchOneOf("oO"))
            {
                result += advance();
                
                if (!matchOneOf("01234567"))
                    throw InvalidNumberError("Invalid octal number!", location_);

                while (matchOneOf("01234567"))
                    result += advance();

                if (std::isalnum(upeek()))
                    throw InvalidNumberError("Invalid character in octal number!", location_);
            }
            else if (std::isalpha(upeek()))
            {
                throw InvalidNumberError("Invalid character in number!", location_);
            }
        }

        if (result.empty() || result == "0")
        {
            while (std::isdigit(upeek()))
                result += advance();
        }

        if (peek() == '.')
        {
            isFloat = true;
            result += advance();

            while (std::isdigit(upeek()))
                result += advance();
        }

        if (matchOneOf("eE"))
        {
            result += advance();
            if (matchOneOf("+-"))
                result += advance();

            if (!std::isdigit(upeek()))
                throw InvalidNumberError("Invalid scientific notation!", location_);

            while (std::isdigit(upeek()))
                result += advance();
        }

        if (std::isalpha(upeek()))
            throw InvalidNumberError("Invalid character in number!", location_);
            

        return {
            .type = isFloat ? TokenType::floatLiteral : TokenType::integerLiteral,
            .value = result,
            .loc = start
        };
    }

    Token Lexer::readChar()
    {
        Location start = location_;
        advance();

        std::string result;

        if (match('\\'))
        {
            advance();
            result = std::string(1, common::getEscapeSeq(advance(), location_));
        }
        else
        {
            result = advance();
        }

        if (isAtEnd() || matchOneOf("\r\n"))
            throw UnterminatedCharError("Unterminated character literal", location_);


        Token token_result{
            .type = TokenType::charLiteral,
            .value = result,
            .loc = start
        };

        if (match('\''))
            advance();
        else
            throw InvalidCharError("Invalid character literal!", location_);
        
        return token_result;
    }

    Token Lexer::readOperator()
    {
        Location start = location_;
        std::string op(1, advance());

        if (operatorMap.contains(op + peek() + peek(1)))
        {
            op += advance();
            op += advance();
        }
        else if (operatorMap.contains(op + peek()))
        {
            op += advance();
        }

        return {
            .type = operatorMap.at(op),
            .value = op,
            .loc = start
        };
    }

    Token Lexer::readSymbol()
    {
        Location start = location_;

        Token tok {
            .type = symbolMap.at(peek()),
            .value = std::string(1, peek()),
            .loc = start
        };

        if(multiMatch("$\""))
        {
            flags_.set_nextStringMultiLine(true);
            advance();
            return Token::invalid();
        }

        if (match('}') && flags_.get_inInterpolatedExpr())
        {
            flags_.set_inInterpolatedExpr(false);
        }
        
        advance();
        return tok;
    }

    void Lexer::readString()
    {
        Location start = location_;
        std::string buffer;

        bool isMultiline = false;

        if (!flags_.get_inInterpolatedString() && !flags_.get_inInterpolatedExpr())
        {
            if (flags_.get_nextStringMultiLine())
            {
                flags_.set_nextStringMultiLine(false);
                flags_.set_interpolatedStringIsMultiline(true);
                isMultiline = true;
            }
            if (match('\"'))
            {
                advance(); // "
            }
            else
            {
                throw UnexpectedCharError("Invalid string start", location_);
            }

            flags_.set_inInterpolatedString(true);
        }
        else
        {
            if (flags_.get_interpolatedStringIsMultiline())
                isMultiline = true;
        }

        while (true)
        {
            if (isAtEnd())
                throw UnterminatedStringError("Unterminated string literal", start);

            if (match('\"'))
            {
                advance(); // closing "
                tokens_.emplace_back(
                    TokenType::stringLiteral,
                    buffer,
                    start
                );

                flags_.set_inInterpolatedString(false);
                flags_.set_interpolatedStringIsMultiline(false);
                return;
            }

            // interpolation: ${ ... }
            if (multiMatch("${"))
            {
                tokens_.emplace_back(
                    TokenType::stringLiteral,
                    buffer,
                    start
                );
                buffer.clear();

                // $
                tokens_.emplace_back(
                    TokenType::dollar,
                    "$",
                    location_
                );
                advance();

                // {
                tokens_.emplace_back(
                    TokenType::leftBrace,
                    "{",
                    location_
                );
                advance();

                flags_.set_inInterpolatedExpr(true);

                return;
            }

            // escape sequence
            if (match('\\'))
            {
                advance(); // '\'
                buffer += common::getEscapeSeq(advance(), location_);
                continue;
            }

            if (matchOneOf("\r\n"))
            {
                if (!isMultiline)
                    throw UnterminatedStringError("Unterminated string literal", location_);

                skipNewline();
                buffer += '\n';
                continue;
            }

            buffer += advance();
        }
    }

    bool Lexer::isAtEnd() const
    {
        return position_ >= source_.size();
    }

    bool Lexer::isOperator(char c)
    {
        return std::ranges::any_of(
            std::views::keys(operatorMap).begin(),
            std::views::keys(operatorMap).end(),
            [c](const auto& op)
            {
                return !op.empty() && op[0] == c;
            });
    }
    
    bool Lexer::isSymbol(char c)
    {
        return symbolMap.contains(c);
    }
}
