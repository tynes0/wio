#pragma once

#include <optional>
#include <string_view>

#include "wio/lexer/token.h"

namespace wio::common
{
    inline std::optional<std::string_view> getBinaryOperatorOverloadName(TokenType type)
    {
        switch (type)
        {
        case TokenType::opPlus: return "__op_binary_plus";
        case TokenType::opMinus: return "__op_binary_minus";
        case TokenType::opStar: return "__op_binary_multiply";
        case TokenType::opSlash: return "__op_binary_divide";
        case TokenType::opPercent: return "__op_binary_modulo";
        case TokenType::opEqual: return "__op_equal";
        case TokenType::opNotEqual: return "__op_not_equal";
        case TokenType::opLess: return "__op_less";
        case TokenType::opLessEqual: return "__op_less_equal";
        case TokenType::opGreater: return "__op_greater";
        case TokenType::opGreaterEqual: return "__op_greater_equal";
        case TokenType::opBitAnd: return "__op_bit_and";
        case TokenType::opBitOr: return "__op_bit_or";
        case TokenType::opBitXor: return "__op_bit_xor";
        case TokenType::opShiftLeft: return "__op_shift_left";
        case TokenType::opShiftRight: return "__op_shift_right";
        default:
            return std::nullopt;
        }
    }

    inline std::optional<std::string_view> getUnaryOperatorOverloadName(TokenType type)
    {
        switch (type)
        {
        case TokenType::opPlus: return "__op_unary_plus";
        case TokenType::opMinus: return "__op_unary_minus";
        case TokenType::opBitNot: return "__op_bit_not";
        case TokenType::opLogicalNot:
        case TokenType::kwNot:
            return "__op_logical_not";
        default:
            return std::nullopt;
        }
    }

    inline std::optional<std::string_view> getOperatorOverloadName(TokenType type, std::size_t parameterCount)
    {
        if (parameterCount == 0)
            return getUnaryOperatorOverloadName(type);
        if (parameterCount == 1)
            return getBinaryOperatorOverloadName(type);
        return std::nullopt;
    }

    inline bool isOperatorOverloadName(std::string_view name)
    {
        return name.starts_with("__op_");
    }

    inline bool isBinaryOperatorOverloadName(std::string_view name)
    {
        return name == "__op_binary_plus" ||
               name == "__op_binary_minus" ||
               name == "__op_binary_multiply" ||
               name == "__op_binary_divide" ||
               name == "__op_binary_modulo" ||
               name == "__op_equal" ||
               name == "__op_not_equal" ||
               name == "__op_less" ||
               name == "__op_less_equal" ||
               name == "__op_greater" ||
               name == "__op_greater_equal" ||
               name == "__op_bit_and" ||
               name == "__op_bit_or" ||
               name == "__op_bit_xor" ||
               name == "__op_shift_left" ||
               name == "__op_shift_right";
    }

    inline bool isUnaryOperatorOverloadName(std::string_view name)
    {
        return name == "__op_unary_plus" ||
               name == "__op_unary_minus" ||
               name == "__op_bit_not" ||
               name == "__op_logical_not";
    }

    inline std::optional<std::string_view> getOperatorDisplayText(std::string_view name)
    {
        if (name == "__op_binary_plus" || name == "__op_unary_plus") return "+";
        if (name == "__op_binary_minus" || name == "__op_unary_minus") return "-";
        if (name == "__op_binary_multiply") return "*";
        if (name == "__op_binary_divide") return "/";
        if (name == "__op_binary_modulo") return "%";
        if (name == "__op_equal") return "==";
        if (name == "__op_not_equal") return "!=";
        if (name == "__op_less") return "<";
        if (name == "__op_less_equal") return "<=";
        if (name == "__op_greater") return ">";
        if (name == "__op_greater_equal") return ">=";
        if (name == "__op_bit_and") return "&";
        if (name == "__op_bit_or") return "|";
        if (name == "__op_bit_xor") return "^";
        if (name == "__op_shift_left") return "<<";
        if (name == "__op_shift_right") return ">>";
        if (name == "__op_bit_not") return "~";
        if (name == "__op_logical_not") return "!";
        return std::nullopt;
    }
}
