#include "wio/lexer/token.h"

namespace wio
{
    Token Token::invalid()
    {
        return {
            .type = TokenType::invalid,
            .value = "",
            .loc = common::Location::invalid()
        };
    }

    bool Token::isValid() const
    {
        Token inv = invalid();
        return !(type == inv.type && value == inv.value);
    }

    bool Token::isKeyword() const
    {
        return std::ranges::find(std::views::values(keywordMap), type) != std::views::values((keywordMap)).end();
    }

    bool Token::isOperator() const
    {
        return std::ranges::find(std::views::values(operatorMap), type) != std::views::values((operatorMap)).end();
    }

    bool Token::isAssignment() const
    {
        return (type == TokenType::opAssign ||
           type == TokenType::opPlusAssign ||
           type == TokenType::opMinusAssign ||
           type == TokenType::opStarAssign ||
           type == TokenType::opSlashAssign ||
           type == TokenType::opPercentAssign ||
           type == TokenType::opShiftLeftAssign ||
           type == TokenType::opShiftRightAssign ||
           type == TokenType::opBitAndAssign ||
           type == TokenType::opBitOrAssign ||
           type == TokenType::opBitXorAssign);
    }

    bool Token::isUnary() const
    {
        return (type == TokenType::kwRef ||
           type == TokenType::kwNot ||
           type == TokenType::opBitNot ||
           type == TokenType::opPlus ||
           type == TokenType::opMinus ||
           type == TokenType::opLogicalNot);
    }

    bool Token::isSymbol() const
    {
        return std::ranges::find(std::views::values(symbolMap), type) != std::views::values((symbolMap)).end();
    }

    bool Token::isIdentifier() const
    {
        return type == TokenType::identifier;
    }

    bool Token::isType() const
    {
        // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
        switch (type)
        {
        case TokenType::kwI8:    
        case TokenType::kwI16:   
        case TokenType::kwI32:   
        case TokenType::kwI64:   
        case TokenType::kwU8:    
        case TokenType::kwU16:   
        case TokenType::kwU32:   
        case TokenType::kwU64:   
        case TokenType::kwF32:   
        case TokenType::kwF64:   
        case TokenType::kwIsize: 
        case TokenType::kwUsize: 
        case TokenType::kwByte:  
        case TokenType::kwBool:  
        case TokenType::kwChar:  
        case TokenType::kwUchar: 
        case TokenType::kwString:
        case TokenType::kwVoid:
        case TokenType::kwType:
        case TokenType::kwResult:
            return true;
        default:
            return false;
        }
    }

    bool Token::isLiteral() const
    {
        return (type == TokenType::integerLiteral ||
           type == TokenType::floatLiteral ||
           type == TokenType::stringLiteral ||
           type == TokenType::interpolatedStringLiteral ||
           type == TokenType::charLiteral);
    }

    bool Token::isComparison() const
    {
        return (type == TokenType::opEqual || 
            type == TokenType::opNotEqual ||
            type == TokenType::opLess || 
            type == TokenType::opLessEqual ||
            type == TokenType::opGreater || 
            type == TokenType::opGreaterEqual
        );
    }
}
