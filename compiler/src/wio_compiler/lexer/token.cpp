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
    case TokenType::floatLiteral: return "floatLiteral";
    case TokenType::stringLiteral: return "stringLiteral";
    case TokenType::interpolatedStringLiteral: return "interpolatedStringLiteral";
    case TokenType::charLiteral: return "charLiteral";
    case TokenType::durationLiteral: return "durationLiteral";
    case TokenType::byteLiteral: return "byteLiteral";

    // keywords
    case TokenType::kwFn: return "fn";
    case TokenType::kwLet: return "let";
    case TokenType::kwMut: return "mut";
    case TokenType::kwConst: return "const";
    case TokenType::kwType: return "type";
    case TokenType::kwRef: return "ref";
    case TokenType::kwFit: return "fit";
    case TokenType::kwView: return "view";
    case TokenType::kwEnum: return "enum";
    case TokenType::kwFlagset: return "flagset";

    case TokenType::kwI8: return "i8";
    case TokenType::kwI16: return "i16";
    case TokenType::kwI32: return "i32";
    case TokenType::kwI64: return "i64";
    case TokenType::kwU8: return "u8";
    case TokenType::kwU16: return "u16";
    case TokenType::kwU32: return "u32";
    case TokenType::kwU64: return "u64";
    case TokenType::kwF32: return "f32";
    case TokenType::kwF64: return "f64";
    case TokenType::kwIsize: return "isize";
    case TokenType::kwUsize: return "usize";
    case TokenType::kwByte: return "byte";
    case TokenType::kwBool: return "bool";
    case TokenType::kwChar: return "char";
    case TokenType::kwUchar: return "uchar";
    case TokenType::kwString: return "string";
    case TokenType::kwVoid: return "void";

    case TokenType::kwComponent: return "component";
    case TokenType::kwSystem: return "system";
    case TokenType::kwProgram: return "program";
    case TokenType::kwObject: return "object";

    case TokenType::kwTrue: return "true";
    case TokenType::kwFalse: return "false";

    case TokenType::kwIf: return "if";
    case TokenType::kwElse: return "else";
    case TokenType::kwMatch: return "match";
    case TokenType::kwFor: return "for";
    case TokenType::kwIn: return "in";
    case TokenType::kwWhile: return "while";
    case TokenType::kwBreak: return "break";
    case TokenType::kwContinue: return "continue";
    case TokenType::kwReturn: return "return";

    case TokenType::kwWhen: return "when";
    case TokenType::kwAnd: return "and";
    case TokenType::kwOr: return "or";
    case TokenType::kwNot: return "not";

    case TokenType::kwEvery: return "every";
    case TokenType::kwAfter: return "after";
    case TokenType::kwDuring: return "during";
    case TokenType::kwWait: return "wait";

    case TokenType::kwResult: return "result";
    case TokenType::kwNull: return "null";

    case TokenType::kwUse: return "use";
    case TokenType::kwAs: return "as";
    case TokenType::kwSuper: return "super";
    case TokenType::kwSelf: return "self";
    case TokenType::kwExtern: return "extern";
    case TokenType::kwEngine: return "engine";

    case TokenType::kwAsync: return "async";
    case TokenType::kwAwait: return "await";
    case TokenType::kwCoroutine: return "coroutine";
    case TokenType::kwYield: return "yield";
    case TokenType::kwThread: return "thread";
    case TokenType::kwLoop: return "loop";

    case TokenType::StaticArray: return "StaticArray";
    case TokenType::DynamicArray: return "DynamicArray";

    // operators
    case TokenType::opPlus: return "opPlus";
    case TokenType::opMinus: return "opMinus";
    case TokenType::opStar: return "opStar";
    case TokenType::opSlash: return "opSlash";
    case TokenType::opPercent: return "opPercent";

    case TokenType::opAssign: return "opAssign";
    case TokenType::opPlusAssign: return "opPlusAssign";
    case TokenType::opMinusAssign: return "opMinusAssign";
    case TokenType::opStarAssign: return "opStarAssign";
    case TokenType::opSlashAssign: return "opSlashAssign";
    case TokenType::opPercentAssign: return "opPercentAssign";

    case TokenType::opEqual: return "opEqual";
    case TokenType::opNotEqual: return "opNotEqual";
    case TokenType::opLess: return "opLess";
    case TokenType::opLessEqual: return "opLessEqual";
    case TokenType::opGreater: return "opGreater";
    case TokenType::opGreaterEqual: return "opGreaterEqual";

    case TokenType::opBitAnd: return "opBitAnd";
    case TokenType::opBitOr: return "opBitOr";
    case TokenType::opBitXor: return "opBitXor";
    case TokenType::opBitNot: return "opBitNot";

    case TokenType::opShiftLeft: return "opShiftLeft";
    case TokenType::opShiftRight: return "opShiftRight";

    case TokenType::opBitAndAssign: return "opBitAndAssign";
    case TokenType::opBitOrAssign: return "opBitOrAssign";
    case TokenType::opBitXorAssign: return "opBitXorAssign";
    case TokenType::opBitNotAssign: return "opBitNotAssign";
    case TokenType::opShiftLeftAssign: return "opShiftLeftAssign";
    case TokenType::opShiftRightAssign: return "opShiftRightAssign";

    case TokenType::opLogicalNot: return "opLogicalNot";
    case TokenType::opLogicalAnd: return "opLogicalAnd";
    case TokenType::opLogicalOr: return "opLogicalOr";

    case TokenType::opFlowRight: return "opFlowRight";
    case TokenType::opFlowLeft: return "opFlowLeft";
    case TokenType::opArrow: return "opArrow";

    case TokenType::opQuestion: return "opQuestion";
    case TokenType::opColon: return "opColon";
    case TokenType::opScope: return "opScope";

    case TokenType::atSign: return "atSign";
    case TokenType::dollar: return "dollar";

    case TokenType::leftParen: return "leftParen";
    case TokenType::rightParen: return "rightParen";
    case TokenType::leftBrace: return "leftBrace";
    case TokenType::rightBrace: return "rightBrace";
    case TokenType::leftBracket: return "leftBracket";
    case TokenType::rightBracket: return "rightBracket";

    case TokenType::comma: return "comma";
    case TokenType::dot: return "dot";
    case TokenType::semicolon: return "semicolon";
    case TokenType::newline: return "newline";
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
