#pragma once

#include <vector>
#include <string>

#include "token.h"
#include "../common/auto_flags.h"

namespace wio
{
#define LEXER_FLAGS(X) X(inInterpolatedString) X(inInterpolatedExpr) X(nextStringMultiLine) X(interpolatedStringIsMultiline)
    DEFINE_FLAGS(LexerFlags, LEXER_FLAGS);
#undef LEXER_FLAGS
    
    class Lexer
    {
    public:
        explicit Lexer(std::string source, std::string sourceName = {});
        std::vector<Token> lex();
        [[nodiscard]] std::string toString() const;
    private:
        [[nodiscard]] char peek(int offset = 0) const;
        [[nodiscard]] unsigned char upeek(int offset = 0) const;
        [[nodiscard]] bool match(char c, int offset = 0) const;
        [[nodiscard]] bool multiMatch(std::string_view chars, int offset = 0) const;
        [[nodiscard]] bool matchOneOf(std::string_view chars, int offset = 0) const;
        
        char advance();
        void advance(int count);
        
        bool skipWhitespaces();
        bool skipComments();
        bool skipNewline();
        
        [[nodiscard]] Token readIdentifier();
        [[nodiscard]] Token readNumber();
        [[nodiscard]] Token readChar();
        [[nodiscard]] Token readOperator();
        [[nodiscard]] Token readSymbol();
        void readString();
        
        [[nodiscard]] bool isAtEnd() const;
        [[nodiscard]] static bool isOperator(char c);
        [[nodiscard]] static bool isSymbol(char c);
        
        std::vector<Token> tokens_;
        std::string source_;
        uint64_t position_;
        common::Location location_;
        LexerFlags flags_;
    };
    
}
