#include "wio/lexer/token.h"

#include <ranges>

namespace wio
{
   std::string_view tokenTypeToString(TokenType type)
   {
       switch (type)
       {
       /* ===============================
          Special
          =============================== */
       case TokenType::invalid: return "invalid";
       case TokenType::endOfFile: return "endOfFile";
   
       /* ===============================
          Identifiers & literals
          =============================== */
       case TokenType::identifier: return "identifier";
   
       case TokenType::integerLiteral: return "integerLiteral";
       case TokenType::floatLiteral: return "floatLiteral";
       case TokenType::stringLiteral: return "stringLiteral";
       case TokenType::interpolatedStringLiteral: return "interpolatedStringLiteral";
       case TokenType::charLiteral: return "charLiteral";
       case TokenType::durationLiteral: return "durationLiteral";
       case TokenType::byteLiteral: return "byteLiteral";
   
       /* ===============================
          Keywords – core
          =============================== */
       case TokenType::kwFn: return "kwFn";
       case TokenType::kwLet: return "kwLet";
       case TokenType::kwMut: return "kwMut";
       case TokenType::kwConst: return "kwConst";
       case TokenType::kwType: return "kwType";
       case TokenType::kwRef: return "kwRef";
       case TokenType::kwView: return "kwView";
       case TokenType::kwFit: return "kwFit";
       case TokenType::kwEnum: return "kwEnum";
       case TokenType::kwFlag: return "kwFlag";
       case TokenType::kwFlagset: return "kwFlagset";
   
       /* ===============================
          Keywords – types
          =============================== */
       case TokenType::kwI8: return "kwI8";
       case TokenType::kwI16: return "kwI16";
       case TokenType::kwI32: return "kwI32";
       case TokenType::kwI64: return "kwI64";
       case TokenType::kwU8: return "kwU8";
       case TokenType::kwU16: return "kwU16";
       case TokenType::kwU32: return "kwU32";
       case TokenType::kwU64: return "kwU64";
       case TokenType::kwF32: return "kwF32";
       case TokenType::kwF64: return "kwF64";
       case TokenType::kwIsize: return "kwIsize";
       case TokenType::kwUsize: return "kwUsize";
       case TokenType::kwByte: return "kwByte";
       case TokenType::kwBool: return "kwBool";
       case TokenType::kwChar: return "kwChar";
       case TokenType::kwUchar: return "kwUchar";
       case TokenType::kwString: return "kwString";
       case TokenType::kwComponent: return "kwComponent";
       case TokenType::kwObject: return "kwObject";
       case TokenType::kwInterface: return "kwInterface";
       case TokenType::kwVoid: return "kwVoid";
   
       /* ===============================
          Keywords – boolean & null
          =============================== */
       case TokenType::kwTrue: return "kwTrue";
       case TokenType::kwFalse: return "kwFalse";
       case TokenType::kwNull: return "kwNull";
   
       /* ===============================
          Keywords – control flow
          =============================== */
       case TokenType::kwIf: return "kwIf";
       case TokenType::kwElse: return "kwElse";
       case TokenType::kwMatch: return "kwMatch";
       case TokenType::kwFor: return "kwFor";
       case TokenType::kwForeach: return "kwForeach";
       case TokenType::kwIn: return "kwIn";
       case TokenType::kwWhile: return "kwWhile";
       case TokenType::kwBreak: return "kwBreak";
       case TokenType::kwContinue: return "kwContinue";
       case TokenType::kwReturn: return "kwReturn";
       case TokenType::kwIs: return "kwIs";
       case TokenType::kwWhen: return "kwWhen";
       case TokenType::kwAssumed: return "kwAssumed";
   
       /* ===============================
          Keywords – logic / flow
          =============================== */
       case TokenType::kwAnd: return "kwAnd";
       case TokenType::kwOr: return "kwOr";
       case TokenType::kwNot: return "kwNot";
   
       /* ===============================
          Keywords – time / game oriented
          =============================== */
       case TokenType::kwEvery: return "kwEvery";
       case TokenType::kwAfter: return "kwAfter";
       case TokenType::kwDuring: return "kwDuring";
       case TokenType::kwWait: return "kwWait";
   
       /* ===============================
          Keywords – binding / modules
          =============================== */
       case TokenType::kwUse: return "kwUse";
       case TokenType::kwAs: return "kwAs";
       case TokenType::kwRealm: return "kwRealm";
   
       /* ===============================
          Keywords – access
          =============================== */
       case TokenType::kwPublic: return "kwPublic";
       case TokenType::kwPrivate: return "kwPrivate";
       case TokenType::kwProtected: return "kwProtected";
       case TokenType::kwSuper: return "kwSuper";
       case TokenType::kwSelf: return "kwSelf";
   
       /* ===============================
          Keywords – async / threading
          =============================== */
       case TokenType::kwAsync: return "kwAsync";
       case TokenType::kwAwait: return "kwAwait";
       case TokenType::kwCoroutine: return "kwCoroutine";
       case TokenType::kwYield: return "kwYield";
       case TokenType::kwThread: return "kwThread";
       case TokenType::kwLoop: return "kwLoop";
   
       /* ===============================
          Keywords – app
          =============================== */
       case TokenType::kwSystem: return "kwSystem";
       case TokenType::kwProgram: return "kwProgram";
   
       /* ===============================
          Arrays
          =============================== */
       case TokenType::StaticArray: return "StaticArray";
       case TokenType::DynamicArray: return "DynamicArray";
   
       /* ===============================
          Operators – arithmetic
          =============================== */
       case TokenType::opPlus: return "opPlus";
       case TokenType::opMinus: return "opMinus";
       case TokenType::opStar: return "opStar";
       case TokenType::opSlash: return "opSlash";
       case TokenType::opPercent: return "opPercent";
   
       /* ===============================
          Operators – assignment
          =============================== */
       case TokenType::opAssign: return "opAssign";
       case TokenType::opPlusAssign: return "opPlusAssign";
       case TokenType::opMinusAssign: return "opMinusAssign";
       case TokenType::opStarAssign: return "opStarAssign";
       case TokenType::opSlashAssign: return "opSlashAssign";
       case TokenType::opPercentAssign: return "opPercentAssign";
   
       /* ===============================
          Operators – comparison
          =============================== */
       case TokenType::opEqual: return "opEqual";
       case TokenType::opNotEqual: return "opNotEqual";
       case TokenType::opLess: return "opLess";
       case TokenType::opLessEqual: return "opLessEqual";
       case TokenType::opGreater: return "opGreater";
       case TokenType::opGreaterEqual: return "opGreaterEqual";
   
       /* ===============================
          Operators – bitwise
          =============================== */
       case TokenType::opBitAnd: return "opBitAnd";
       case TokenType::opBitOr: return "opBitOr";
       case TokenType::opBitXor: return "opBitXor";
       case TokenType::opBitNot: return "opBitNot";
       case TokenType::opShiftLeft: return "opShiftLeft";
       case TokenType::opShiftRight: return "opShiftRight";
   
       /* ===============================
          Operators – bitwise assign
          =============================== */
       case TokenType::opBitAndAssign: return "opBitAndAssign";
       case TokenType::opBitOrAssign: return "opBitOrAssign";
       case TokenType::opBitXorAssign: return "opBitXorAssign";
       case TokenType::opBitNotAssign: return "opBitNotAssign";
       case TokenType::opShiftLeftAssign: return "opShiftLeftAssign";
       case TokenType::opShiftRightAssign: return "opShiftRightAssign";
   
       /* ===============================
          Operators – logical
          =============================== */
       case TokenType::opLogicalNot: return "opLogicalNot";
       case TokenType::opLogicalAnd: return "opLogicalAnd";
       case TokenType::opLogicalOr: return "opLogicalOr";
   
       /* ===============================
          Operators – data flow
          =============================== */
       case TokenType::opFlowRight: return "opFlowRight";
       case TokenType::opFlowLeft: return "opFlowLeft";
   
       /* ===============================
          Operators – misc
          =============================== */
       case TokenType::opArrow: return "opArrow";
       case TokenType::opFatArrow: return "opFatArrow";
       case TokenType::opQuestion: return "opQuestion";
       case TokenType::opColon: return "opColon";
       case TokenType::opScope: return "opScope";
       case TokenType::opDot: return "opDot";
          
      /* ===============================
         Operators – range
         =============================== */
       case TokenType::opRangeInclusive: return "opRangeInclusive";
       case TokenType::opRangeExclusive: return "opRangeExclusive";
   
       /* ===============================
          Symbols
          =============================== */
       case TokenType::atSign: return "atSign";
       case TokenType::dollar: return "dollar";
   
       /* ===============================
          Delimiters
          =============================== */
       case TokenType::leftParen: return "leftParen";
       case TokenType::rightParen: return "rightParen";
       case TokenType::leftBrace: return "leftBrace";
       case TokenType::rightBrace: return "rightBrace";
       case TokenType::leftBracket: return "leftBracket";
       case TokenType::rightBracket: return "rightBracket";
   
       case TokenType::comma: return "comma";
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
          type == TokenType::kwNot ||
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
       case TokenType::kwObject:
       case TokenType::kwType:
           return true;
       default:
           return false;
       }
   }
   
   bool Token::isTypeDeclaration() const
   {
      // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
      switch (type)
      {
      case TokenType::kwComponent:
      case TokenType::kwObject:
      case TokenType::kwInterface:
      case TokenType::kwEnum:
      case TokenType::kwFlag:
      case TokenType::kwFlagset:
         return true;
      default:
         return false;
      }
   }

   bool Token::isLiteral() const
   {
      // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
      switch (type)
      {
      case TokenType::integerLiteral:
      case TokenType::floatLiteral:
      case TokenType::stringLiteral:
      case TokenType::interpolatedStringLiteral:
      case TokenType::charLiteral:
      case TokenType::durationLiteral:
      case TokenType::byteLiteral:
         return true;
      default:
         return false;
      }
   }

   bool Token::isComparison() const
   {
      // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
      switch (type)
      {
      case TokenType::opEqual:
      case TokenType::opNotEqual:
      case TokenType::opLess:
      case TokenType::opLessEqual:
      case TokenType::opGreater:
      case TokenType::opGreaterEqual:
      case TokenType::kwIs:
         return true;
      default:
         return false;
      }
   }
}
