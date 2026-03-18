#include "wio/lexer/token.h"

#include <ranges>

namespace wio
{
    std::string_view tokenTypeToString(TokenType type)
    {
        switch (type)
        {
        case TokenType::invalid: return "invalid";
        case TokenType::endOfFile: return "eof";
        case TokenType::identifier: return "identifier";
        case TokenType::integerLiteral: return "integerLiteral";
        case TokenType::floatLiteral:
        case TokenType::stringLiteral:
        case TokenType::interpolatedStringLiteral:
        case TokenType::charLiteral:
        case TokenType::durationLiteral:
        case TokenType::byteLiteral:
        case TokenType::kwFn:
        case TokenType::kwLet:
        case TokenType::kwMut:
        case TokenType::kwConst:
        case TokenType::kwType:
        case TokenType::kwRef:
        case TokenType::kwView:
        case TokenType::kwEnum:
        case TokenType::kwFlagset:
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
        case TokenType::kwComponent:
        case TokenType::kwSystem:
        case TokenType::kwProgram:
        case TokenType::kwObject:
        case TokenType::kwTrue:
        case TokenType::kwFalse:
        case TokenType::kwIf:
        case TokenType::kwElse:
        case TokenType::kwMatch:
        case TokenType::kwFor:
        case TokenType::kwIn:
        case TokenType::kwWhile:
        case TokenType::kwBreak:
        case TokenType::kwContinue:
        case TokenType::kwReturn:
        case TokenType::kwWhen:
        case TokenType::kwAnd:
        case TokenType::kwOr:
        case TokenType::kwNot:
        case TokenType::kwEvery:
        case TokenType::kwAfter:
        case TokenType::kwDuring:
        case TokenType::kwWait:
        case TokenType::kwResult:
        case TokenType::kwNull:
        case TokenType::kwUse:
        case TokenType::kwAs:
        case TokenType::kwSuper:
        case TokenType::kwSelf:
        case TokenType::kwExtern:
        case TokenType::kwEngine:
        case TokenType::kwAsync:
        case TokenType::kwAwait:
        case TokenType::kwCoroutine:
        case TokenType::kwYield:
        case TokenType::kwThread:
        case TokenType::kwLoop:
        case TokenType::StaticArray:
        case TokenType::DynamicArray:
        case TokenType::opPlus:
        case TokenType::opMinus:
        case TokenType::opStar:
        case TokenType::opSlash:
        case TokenType::opPercent:
        case TokenType::opAssign:
        case TokenType::opPlusAssign:
        case TokenType::opMinusAssign:
        case TokenType::opStarAssign:
        case TokenType::opSlashAssign:
        case TokenType::opPercentAssign:
        case TokenType::opEqual:
        case TokenType::opNotEqual:
        case TokenType::opLess:
        case TokenType::opLessEqual:
        case TokenType::opGreater:
        case TokenType::opGreaterEqual:
        case TokenType::opBitAnd:
        case TokenType::opBitOr:
        case TokenType::opBitXor:
        case TokenType::opBitNot:
        case TokenType::opShiftLeft:
        case TokenType::opShiftRight:
        case TokenType::opBitAndAssign:
        case TokenType::opBitOrAssign:
        case TokenType::opBitXorAssign:
        case TokenType::opBitNotAssign:
        case TokenType::opShiftLeftAssign:
        case TokenType::opShiftRightAssign:
        case TokenType::opLogicalNot:
        case TokenType::opLogicalAnd:
        case TokenType::opLogicalOr:
        case TokenType::opFlowRight:
        case TokenType::opFlowLeft:
        case TokenType::opArrow:
        case TokenType::opQuestion:
        case TokenType::opColon:
        case TokenType::opScope:
        case TokenType::atSign:
        case TokenType::dollar:
        case TokenType::leftParen:
        case TokenType::rightParen:
        case TokenType::leftBrace:
        case TokenType::rightBrace:
        case TokenType::leftBracket:
        case TokenType::rightBracket:
        case TokenType::comma:
        case TokenType::dot:
        case TokenType::semicolon:
        case TokenType::newline:
            return "newline";
        }
        
        return "invalid";
    }

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
