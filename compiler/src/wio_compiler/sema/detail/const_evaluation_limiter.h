#pragma once

#include "wio/sema/constant_evaluator.h"

#include <cstddef>
#include <unordered_set>

namespace wio::sema::detail
{
    enum class ConstEvaluationLimitStatus
    {
        Valid,
        Cycle,
        DepthLimit,
        NodeLimit,
        TextSizeLimit
    };

    class ConstEvaluationLimiter final
    {
    public:
        static constexpr std::size_t MaxDepth = 128;
        static constexpr std::size_t MaxNodes = 16384;
        static constexpr std::size_t MaxTextBytes = 1024 * 1024;

        explicit ConstEvaluationLimiter(const ConstVariableDeclarationMap& declarations);

        void markActive(const Symbol* symbol);
        ConstEvaluationLimitStatus validate(const NodePtr<Expression>& expression);

    private:
        ConstEvaluationLimitStatus validate(const NodePtr<Expression>& expression, std::size_t depth);

        const ConstVariableDeclarationMap& declarations_;
        std::size_t visitedNodes_ = 0;
        std::size_t foldedTextBytes_ = 0;
        std::unordered_set<const Symbol*> activeSymbols_;
    };
}
