#pragma once
#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "../common/location.h"

namespace wio
{
   enum class TokenType :  uint8_t
   {
      /* ===============================
         Special
         =============================== */
      invalid = 0,
      endOfFile,
   
      /* ===============================
         Identifiers & literals
         =============================== */
      identifier,
   
      integerLiteral,
      floatLiteral,
      stringLiteral,
      interpolatedStringLiteral,
      charLiteral,
      durationLiteral,
      byteLiteral,
   
      /* ===============================
         Keywords – core
         =============================== */
      kwFn,
      kwLet,
      kwMut,
      kwConst,
      kwType,
      kwRef,
      kwView,
      kwFit,
      kwEnum,
      kwFlag,
      kwFlagset,

      /* ===============================
         Keywords – types
         =============================== */
      kwI8,
      kwI16,
      kwI32,
      kwI64,
      kwU8,
      kwU16,
      kwU32,
      kwU64,
      kwF32,
      kwF64,
      kwIsize,
      kwUsize,
      kwByte,
      kwBool,
      kwChar,
      kwUchar,
      kwString,
      kwComponent,
      kwObject,
      kwInterface,
      kwVoid,
      
      /* ===============================
         Keywords – boolean & null
         =============================== */
      kwTrue,
      kwFalse,
      kwNull,
      
      /* ===============================
         Keywords – control flow
         =============================== */
      kwIf,
      kwElse,
      kwMatch,
      kwFor,
      kwIn,
      kwWhile, // todo: rename with loop
      kwBreak,
      kwContinue,
      kwReturn,
      kwIs,
      kwWhen,
      kwAssumed,
   
      /* ===============================
         Keywords – logic / flow
         =============================== */
      kwAnd,
      kwOr,
      kwNot,
   
      /* ===============================
         Keywords – time / game oriented
         =============================== */
      kwEvery,
      kwAfter,
      kwDuring,
      kwWait,
   
      /* ===============================
         Keywords – binding / modules
         =============================== */
      kwUse,
      kwAs,

      /* ===============================
         Keywords – access
         =============================== */
      kwPublic,
      kwPrivate,
      kwProtected,
      kwSuper,
      kwSelf,
      
      /* ===============================
         Keywords – binding / modules
         =============================== */
      kwAsync,
      kwAwait,
      kwCoroutine,
      kwYield,
      kwThread,
      kwLoop,

      /* ===============================
         Keywords – app
         =============================== */
      kwSystem,
      kwProgram,

      /* ===============================
         Arrays
         =============================== */
      StaticArray,       // [i32, 5] 
      DynamicArray,       // i32[] 
   
      /* ===============================
         Operators – arithmetic
         =============================== */
      opPlus,            // +
      opMinus,           // -
      opStar,            // *
      opSlash,           // /
      opPercent,         // %
   
      /* ===============================
         Operators – assignment
         =============================== */
      opAssign,          // =
      opPlusAssign,      // +=
      opMinusAssign,     // -=
      opStarAssign,      // *=
      opSlashAssign,     // /=
      opPercentAssign,   // %=
   
      /* ===============================
         Operators – comparison
         =============================== */
      opEqual,           // ==
      opNotEqual,        // !=
      opLess,            // <
      opLessEqual,       // <=
      opGreater,         // >
      opGreaterEqual,    // >=
   
      /* ===============================
         Operators – bitwise
         =============================== */
      opBitAnd,          // &
      opBitOr,           // |
      opBitXor,          // ^
      opBitNot,          // ~
      opShiftLeft,       // <<
      opShiftRight,      // >>

      /* ===============================
         Operators – bitwise assign
         =============================== */
      opBitAndAssign,          // &=
      opBitOrAssign,           // |=
      opBitXorAssign,          // ^=
      opBitNotAssign,          // ~=
      opShiftLeftAssign,       // <<=
      opShiftRightAssign,      // >>=

      /* ===============================
         Operators – bitwise assign
         =============================== */
      opLogicalNot,      // !
      opLogicalAnd,      // &&
      opLogicalOr,       // ||
      
      /* ===============================
         Operators – data flow
         =============================== */
      opFlowRight,       // |>
      opFlowLeft,        // <|
      
      /* ===============================
         Operators – misc
         =============================== */
      opArrow,           // ->
      opFatArrow,        // =>
      opQuestion,        // ?
      opColon,           // :
      opScope,           // ::
      opDot,             // .

      /* ===============================
         Operators – range
         =============================== */

      opRangeInclusive,  // ...
      opRangeExclusive,  // ..<
      
      /* ===============================
         Symbols
         =============================== */
      atSign,            // @
      dollar,            // $
   
      /* ===============================
         Delimiters
         =============================== */
      leftParen,         // (
      rightParen,        // )
      leftBrace,         // {
      rightBrace,        // }
      leftBracket,       // [
      rightBracket,      // ]
   
      comma,             // ,
      semicolon,         // ;
      newline            // statement boundary
   };

   /* ===============================
   Keywords
   =============================== */
   inline const std::unordered_map<std::string_view, TokenType> keywordMap = {
      { "fn",        TokenType::kwFn },
      { "let",       TokenType::kwLet },
      { "mut",       TokenType::kwMut },
      { "const",     TokenType::kwConst },
      { "type",      TokenType::kwType },
      { "ref",       TokenType::kwRef },
      { "view",      TokenType::kwView },
      { "fit",       TokenType::kwFit },
      { "enum",      TokenType::kwEnum },
      { "flag",      TokenType::kwFlag },
      { "flagset",   TokenType::kwFlagset },
      
      { "i8",        TokenType::kwI8 },
      { "i16",       TokenType::kwI16 },
      { "i32",       TokenType::kwI32 },
      { "i64",       TokenType::kwI64 },
      { "u8",        TokenType::kwU8 },
      { "u16",       TokenType::kwU16 },
      { "u32",       TokenType::kwU32 },
      { "u64",       TokenType::kwU64 },
      { "f32",       TokenType::kwF32 },
      { "f64",       TokenType::kwF64 },
      { "isize",     TokenType::kwIsize },
      { "usize",     TokenType::kwUsize },
      { "byte",      TokenType::kwByte },
      { "bool",      TokenType::kwBool },
      { "char",      TokenType::kwChar },
      { "uchar",     TokenType::kwUchar },
      { "string",    TokenType::kwString },
      { "component", TokenType::kwComponent },
      { "object",    TokenType::kwObject },
      { "interface", TokenType::kwInterface },
      { "void",      TokenType::kwVoid },

      { "true",      TokenType::kwTrue },
      { "false",     TokenType::kwFalse },
      { "null",      TokenType::kwNull },
   
      { "if",        TokenType::kwIf },
      { "else",      TokenType::kwElse },
      { "match",     TokenType::kwMatch },
      { "for",       TokenType::kwFor },
      { "in",        TokenType::kwIn },
      { "while",     TokenType::kwWhile },
      { "break",     TokenType::kwBreak },
      { "continue",  TokenType::kwContinue },
      { "return",    TokenType::kwReturn },
      { "is",        TokenType::kwIs },
      { "when",      TokenType::kwWhen },
      { "assumed",   TokenType::kwAssumed },
      
      { "and",       TokenType::kwAnd },
      { "or",        TokenType::kwOr },
      { "not",       TokenType::kwNot },
   
      { "every",     TokenType::kwEvery },
      { "after",     TokenType::kwAfter },
      { "during",    TokenType::kwDuring },
      { "wait",      TokenType::kwWait },
   
      { "use",       TokenType::kwUse },
      { "as",        TokenType::kwAs },

      { "public",    TokenType::kwPublic },
      { "private",   TokenType::kwPrivate },
      { "protected", TokenType::kwProtected },
      { "super",     TokenType::kwSuper },
      { "self",      TokenType::kwSelf },

      { "async",     TokenType::kwAsync },
      { "await",     TokenType::kwAwait },
      { "coroutine", TokenType::kwCoroutine },
      { "yield",     TokenType::kwYield },
      { "thread",    TokenType::kwThread },
      { "loop",      TokenType::kwLoop },
      
      { "system",    TokenType::kwSystem },
      { "program",   TokenType::kwProgram },
   };

   /* ===============================
      Operators (longest first)
      =============================== */
   
   inline const std::unordered_map<std::string_view, TokenType> operatorMap = {
      { "<<=", TokenType::opShiftLeftAssign },
      { ">>=", TokenType::opShiftRightAssign },
      { "&=",  TokenType::opBitAndAssign },
      { "|=",  TokenType::opBitOrAssign },
      { "^=",  TokenType::opBitXorAssign },
      { "~=",  TokenType::opBitNotAssign },
      
      { "|>",  TokenType::opFlowRight },
      { "<|",  TokenType::opFlowLeft },
   
      { "<<",  TokenType::opShiftLeft },
      { ">>",  TokenType::opShiftRight },
   
      { "==",  TokenType::opEqual },
      { "!=",  TokenType::opNotEqual },
      { "<=",  TokenType::opLessEqual },
      { ">=",  TokenType::opGreaterEqual },
   
      { "+=",  TokenType::opPlusAssign },
      { "-=",  TokenType::opMinusAssign },
      { "*=",  TokenType::opStarAssign },
      { "/=",  TokenType::opSlashAssign },
      { "%=",  TokenType::opPercentAssign },
      
      { "::",  TokenType::opScope },
      { ".",   TokenType::opDot },
   
      { "+",   TokenType::opPlus },
      { "-",   TokenType::opMinus },
      { "*",   TokenType::opStar },
      { "/",   TokenType::opSlash },
      { "%",   TokenType::opPercent },
   
      { "<",   TokenType::opLess },
      { ">",   TokenType::opGreater },
   
      { "=",   TokenType::opAssign },
   
      { "&",   TokenType::opBitAnd },
      { "|",   TokenType::opBitOr },
      { "^",   TokenType::opBitXor },
      { "~",   TokenType::opBitNot },
      
      { "!",   TokenType::opLogicalNot },
      { "&&",  TokenType::opLogicalAnd },
      { "||",  TokenType::opLogicalOr },
   
      { "->",  TokenType::opArrow },
      { "=>",  TokenType::opFatArrow },
      { "?",   TokenType::opQuestion },
      { ":",   TokenType::opColon },
      
      { "...", TokenType::opRangeInclusive },
      { "..<", TokenType::opRangeExclusive },
   };
   
   /* ===============================
      Single-character symbols
      =============================== */
   
   inline const std::unordered_map<char, TokenType> symbolMap = {
       { '(', TokenType::leftParen },
       { ')', TokenType::rightParen },
       { '{', TokenType::leftBrace },
       { '}', TokenType::rightBrace },
       { '[', TokenType::leftBracket },
       { ']', TokenType::rightBracket },
   
       { ',', TokenType::comma },
       { ';', TokenType::semicolon },
   
       { '@', TokenType::atSign },
       { '$', TokenType::dollar },
   };

   std::string_view tokenTypeToString(TokenType type);

   struct Token
   {
      TokenType type = TokenType::invalid;
      std::string value;
      common::Location loc;

      [[nodiscard]] static Token invalid();
      
      [[nodiscard]] bool isValid() const;
      [[nodiscard]] bool isKeyword() const;
      [[nodiscard]] bool isOperator() const;
      [[nodiscard]] bool isAssignment() const;
      [[nodiscard]] bool isUnary() const;
      [[nodiscard]] bool isSymbol() const;
      [[nodiscard]] bool isIdentifier() const;
      [[nodiscard]] bool isType() const;
      [[nodiscard]] bool isTypeDeclaration() const;
      [[nodiscard]] bool isLiteral() const;
      [[nodiscard]] bool isComparison() const;
   };
   
} // namespace wio