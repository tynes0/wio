#include "wio/sema/constant_evaluator.h"

#include "wio/common/utility.h"

#include <limits>

namespace wio::sema
{
    namespace
    {
        std::optional<std::int64_t> convertIntegerLiteral(const IntegerLiteral& literal)
        {
            const IntegerResult result = common::getInteger(literal.token.value);
            if (!result.isValid)
                return std::nullopt;

            switch (result.type)
            {
            case IntegerType::i8: return static_cast<std::int64_t>(result.value.v_i8);
            case IntegerType::i16: return static_cast<std::int64_t>(result.value.v_i16);
            case IntegerType::i32: return static_cast<std::int64_t>(result.value.v_i32);
            case IntegerType::i64: return result.value.v_i64;
            case IntegerType::u8: return static_cast<std::int64_t>(result.value.v_u8);
            case IntegerType::u16: return static_cast<std::int64_t>(result.value.v_u16);
            case IntegerType::u32: return static_cast<std::int64_t>(result.value.v_u32);
            case IntegerType::u64:
                if (result.value.v_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    return std::nullopt;
                return static_cast<std::int64_t>(result.value.v_u64);
            case IntegerType::isize: return static_cast<std::int64_t>(result.value.v_isize);
            case IntegerType::usize:
                if (result.value.v_usize > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()))
                    return std::nullopt;
                return static_cast<std::int64_t>(result.value.v_usize);
            case IntegerType::Unknown: return std::nullopt;
            }
            return std::nullopt;
        }
    }

    ConstExpressionEvaluator::ConstExpressionEvaluator(const ConstVariableDeclarationMap& declarations)
        : declarations_(declarations)
    {
    }

    std::optional<std::int64_t> ConstExpressionEvaluator::evaluateInteger(
        const NodePtr<Expression>& expression) const
    {
        std::unordered_set<const Symbol*> activeSymbols;
        return evaluateInteger(expression, activeSymbols);
    }

    std::optional<std::int64_t> ConstExpressionEvaluator::evaluateInteger(
        const NodePtr<Expression>& expression,
        std::unordered_set<const Symbol*>& activeSymbols) const
    {
        if (!expression)
            return std::nullopt;

        if (const auto* literal = expression->as<IntegerLiteral>())
            return convertIntegerLiteral(*literal);

        if (const auto* identifier = expression->as<Identifier>())
        {
            Ref<Symbol> symbol = identifier->referencedSymbol.Lock();
            if (!symbol || !symbol->flags.get_isConst())
                return std::nullopt;

            const auto declaration = declarations_.find(symbol.Get());
            if (declaration == declarations_.end() || !declaration->second || !declaration->second->initializer)
                return std::nullopt;
            if (!activeSymbols.insert(symbol.Get()).second)
                return std::nullopt;

            auto value = evaluateInteger(declaration->second->initializer, activeSymbols);
            activeSymbols.erase(symbol.Get());
            return value;
        }

        if (const auto* unary = expression->as<UnaryExpression>())
        {
            const auto operand = evaluateInteger(unary->operand, activeSymbols);
            if (!operand)
                return std::nullopt;

            switch (unary->op.type)
            {
            case TokenType::opPlus: return *operand;
            case TokenType::opMinus:
                if (*operand == std::numeric_limits<std::int64_t>::min())
                    return std::nullopt;
                return -*operand;
            case TokenType::opBitNot: return ~*operand;
            default: return std::nullopt;
            }
        }

        if (const auto* fit = expression->as<FitExpression>())
            return evaluateInteger(fit->operand, activeSymbols);

        if (const auto* binary = expression->as<BinaryExpression>())
        {
            const auto lhs = evaluateInteger(binary->left, activeSymbols);
            const auto rhs = evaluateInteger(binary->right, activeSymbols);
            if (!lhs || !rhs)
                return std::nullopt;

            switch (binary->op.type)
            {
            case TokenType::opPlus: return *lhs + *rhs;
            case TokenType::opMinus: return *lhs - *rhs;
            case TokenType::opStar: return *lhs * *rhs;
            case TokenType::opSlash: return *rhs == 0 ? std::nullopt : std::optional<std::int64_t>(*lhs / *rhs);
            case TokenType::opPercent: return *rhs == 0 ? std::nullopt : std::optional<std::int64_t>(*lhs % *rhs);
            case TokenType::opBitAnd: return *lhs & *rhs;
            case TokenType::opBitOr: return *lhs | *rhs;
            case TokenType::opBitXor: return *lhs ^ *rhs;
            case TokenType::opShiftLeft:
                return *rhs < 0 || *rhs >= 63 ? std::nullopt : std::optional<std::int64_t>(*lhs << *rhs);
            case TokenType::opShiftRight:
                return *rhs < 0 || *rhs >= 63 ? std::nullopt : std::optional<std::int64_t>(*lhs >> *rhs);
            default: return std::nullopt;
            }
        }

        return std::nullopt;
    }
}
