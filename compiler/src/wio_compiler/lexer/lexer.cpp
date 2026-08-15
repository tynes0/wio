#include "wio/lexer/lexer.h"
#include "wio/common/exception.h"
#include "wio/common/utility.h"

#include <ranges>

namespace
{
    bool isValidUtf8(const std::string_view input) noexcept
    {
        std::size_t offset = 0;
        while (offset < input.size())
        {
            const auto first = static_cast<unsigned char>(input[offset]);
            std::size_t length = 0;
            std::uint32_t value = 0;
            std::uint32_t minimum = 0;

            if (first < 0x80u) { length = 1; value = first; }
            else if ((first & 0xe0u) == 0xc0u) { length = 2; value = first & 0x1fu; minimum = 0x80u; }
            else if ((first & 0xf0u) == 0xe0u) { length = 3; value = first & 0x0fu; minimum = 0x800u; }
            else if ((first & 0xf8u) == 0xf0u) { length = 4; value = first & 0x07u; minimum = 0x10000u; }
            else return false;

            if (offset + length > input.size()) return false;
            for (std::size_t index = 1; index < length; ++index)
            {
                const auto next = static_cast<unsigned char>(input[offset + index]);
                if ((next & 0xc0u) != 0x80u) return false;
                value = (value << 6u) | (next & 0x3fu);
            }

            if (value < minimum || value > 0x10ffffu ||
                (value >= 0xd800u && value <= 0xdfffu))
                return false;
            offset += length;
        }
        return true;
    }
}

namespace wio
{
    using namespace common;
    
    Lexer::Lexer(std::string source, std::string sourceName)
        : source_(std::move(source)), position_(0), location_ { .file = std::move(sourceName), .line = 0, .column = 0 }
    {
    }

    std::vector<Token> Lexer::lex()
    {
        position_ = 0;
        tokens_.clear();
        interpolationStack_.clear();
        flags_ = LexerFlags::createAllFalse();
        location_.line = 1;
        location_.column = 1;
        
        
        while (!isAtEnd())
        {
            if (!interpolationStack_.empty() && !interpolationStack_.back().inExpression)
            {
                readString();
                continue;
            }
            bool res;
            do {
                res = (skipWhitespaces() || skipComments());
            } while (res);

            if (multiMatch("u$\""))
            {
                flags_.set_nextStringUnicode(true);
                flags_.set_nextStringMultiLine(true);
                advance(2); // u$
                readString();
            }
            else if (multiMatch("u\""))
            {
                flags_.set_nextStringUnicode(true);
                advance(); // u
                readString();
            }
            else if (std::isalpha(upeek()) || match('_'))
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
            const std::string typeStr{tokenTypeToString(token.type)};

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
            if (!skipNewline())
                advance();
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
            }
            else if (matchOneOf("xX"))
            {
                result += advance();
                if (!std::isxdigit(upeek()))
                    throw InvalidNumberError("Invalid hexadecimal number!", location_);

                while (std::isxdigit(upeek()))
                    result += advance();
            }
            else if (matchOneOf("oO"))
            {
                result += advance();
                if (!matchOneOf("01234567"))
                    throw InvalidNumberError("Invalid octal number!", location_);

                while (matchOneOf("01234567"))
                    result += advance();
            }
        }

        if (result.empty() || result == "0")
        {
            while (std::isdigit(upeek()))
                result += advance();
        }

        if (peek() == '.')
        {
            if (peek(1) != '.') 
            {
                isFloat = true;
                result += advance();

                while (std::isdigit(upeek()))
                    result += advance();
            }
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

        std::string suffix;
        while (std::isalnum(upeek()) || match('_'))
        {
            suffix += advance();
        }
        
        result += suffix;

        TokenType type = isFloat ? TokenType::floatLiteral : TokenType::integerLiteral;

        if (!suffix.empty())
        {
            if (suffix == "s" || suffix == "ms" || suffix == "us" || suffix == "ns" || suffix == "m" || suffix == "h")
            {
                type = TokenType::durationLiteral;
            }
            else if (suffix == "f32" || suffix == "f64" || suffix == "f")
            {
                type = TokenType::floatLiteral;
            }
            else if (suffix == "i8" || suffix == "u8" || suffix == "i16" || suffix == "u16" || 
                     suffix == "i32" || suffix == "u32" || suffix == "i64" || suffix == "u64" || 
                     suffix == "isize" || suffix == "usize" ||
                     suffix == "isz" || suffix == "usz" || suffix == "i" || suffix == "u")
            {
                if (isFloat)
                {
                    throw InvalidNumberError("Float literal cannot have an integer suffix!", location_);
                }
                type = TokenType::integerLiteral;
            }
            else
            {
                throw InvalidNumberError(("Unknown literal suffix: '" + suffix + "'").c_str(), location_);
            }
        }

        return {
            .type = type,
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

        if (!interpolationStack_.empty() && interpolationStack_.back().inExpression)
        {
            auto& frame = interpolationStack_.back();
            if (match('{'))
            {
                ++frame.braceDepth;
            }
            else if (match('}'))
            {
                if (frame.braceDepth > 0)
                    --frame.braceDepth;
                if (frame.braceDepth == 0)
                    frame.inExpression = false;
            }
        }
        
        advance();
        return tok;
    }

    void Lexer::readString()
    {
        Location start = location_;
        std::string buffer;

        const bool isContinuation =
            !interpolationStack_.empty() && !interpolationStack_.back().inExpression;
        bool isMultiline = isContinuation
            ? interpolationStack_.back().multiline
            : false;
        bool isUnicode = isContinuation
            ? interpolationStack_.back().unicode
            : false;

        if (!isContinuation)
        {
            if (flags_.get_nextStringMultiLine())
            {
                flags_.set_nextStringMultiLine(false);
                isMultiline = true;
            }
            if (flags_.get_nextStringUnicode())
            {
                flags_.set_nextStringUnicode(false);
                isUnicode = true;
            }
            if (match('\"'))
            {
                advance(); // "
            }
            else
            {
                throw UnexpectedCharError("Invalid string start", location_);
            }

        }

        const auto emitStringSegment = [&](std::string value, const Location& segmentLocation)
        {
            if (isUnicode && !isValidUtf8(value))
                throw InvalidStringError("Unicode text literal contains invalid UTF-8", segmentLocation);
            tokens_.push_back(Token{
                .type = TokenType::stringLiteral,
                .value = std::move(value),
                .loc = segmentLocation,
                .isUnicodeString = isUnicode
            });
        };

        while (true)
        {
            if (isAtEnd())
                throw UnterminatedStringError("Unterminated string literal", start);

            if (match('\"'))
            {
                advance(); // closing "
                emitStringSegment(std::move(buffer), start);

                if (isContinuation)
                    interpolationStack_.pop_back();
                return;
            }

            // interpolation: ${ ... }
            if (multiMatch("${"))
            {
                emitStringSegment(std::move(buffer), start);
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

                if (isContinuation)
                {
                    auto& frame = interpolationStack_.back();
                    frame.inExpression = true;
                    frame.braceDepth = 1;
                }
                else
                {
                    interpolationStack_.push_back(InterpolationFrame{
                        .multiline = isMultiline,
                        .unicode = isUnicode,
                        .inExpression = true,
                        .braceDepth = 1
                    });
                }

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
