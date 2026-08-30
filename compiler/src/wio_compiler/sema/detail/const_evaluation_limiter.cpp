#include "const_evaluation_limiter.h"

#include <algorithm>

namespace wio::sema::detail
{
    ConstEvaluationLimiter::ConstEvaluationLimiter(const ConstVariableDeclarationMap& declarations)
        : declarations_(declarations)
    {
    }

    void ConstEvaluationLimiter::markActive(const Symbol* symbol)
    {
        if (symbol)
            activeSymbols_.insert(symbol);
    }

    ConstEvaluationLimitStatus ConstEvaluationLimiter::validate(const NodePtr<Expression>& expression)
    {
        return validate(expression, 0);
    }

    ConstEvaluationLimitStatus ConstEvaluationLimiter::validate(
        const NodePtr<Expression>& expression,
        const std::size_t depth)
    {
        if (!expression)
            return ConstEvaluationLimitStatus::Valid;
        if (depth > MaxDepth)
            return ConstEvaluationLimitStatus::DepthLimit;
        if (++visitedNodes_ > MaxNodes)
            return ConstEvaluationLimitStatus::NodeLimit;

        if (const auto* literal = expression->as<StringLiteral>())
        {
            if (literal->token.value.size() > MaxTextBytes - std::min(foldedTextBytes_, MaxTextBytes))
                return ConstEvaluationLimitStatus::TextSizeLimit;
            foldedTextBytes_ += literal->token.value.size();
            return ConstEvaluationLimitStatus::Valid;
        }

        if (const auto* identifier = expression->as<Identifier>())
        {
            Ref<Symbol> symbol = identifier->referencedSymbol.Lock();
            if (!symbol || !symbol->flags.get_isConst())
                return ConstEvaluationLimitStatus::Valid;

            const auto declarationIt = declarations_.find(symbol.Get());
            if (declarationIt == declarations_.end() ||
                !declarationIt->second || !declarationIt->second->initializer)
            {
                return ConstEvaluationLimitStatus::Valid;
            }

            if (!activeSymbols_.insert(symbol.Get()).second)
                return ConstEvaluationLimitStatus::Cycle;
            const auto result = validate(declarationIt->second->initializer, depth + 1);
            activeSymbols_.erase(symbol.Get());
            return result;
        }

        const auto validateChild = [&](const NodePtr<Expression>& child)
        {
            return validate(child, depth + 1);
        };
        const auto validateChildren = [&](const auto& children)
        {
            for (const auto& child : children)
            {
                const auto result = validateChild(child);
                if (result != ConstEvaluationLimitStatus::Valid)
                    return result;
            }
            return ConstEvaluationLimitStatus::Valid;
        };

        if (const auto* interpolated = expression->as<InterpolatedStringLiteral>())
            return validateChildren(interpolated->parts);
        if (const auto* unary = expression->as<UnaryExpression>())
            return validateChild(unary->operand);
        if (const auto* binary = expression->as<BinaryExpression>())
        {
            const auto left = validateChild(binary->left);
            return left == ConstEvaluationLimitStatus::Valid
                ? validateChild(binary->right)
                : left;
        }
        if (const auto* fit = expression->as<FitExpression>())
            return validateChild(fit->operand);

        return ConstEvaluationLimitStatus::Valid;
    }
}
