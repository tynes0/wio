#pragma once

#include "wio/ast/ast.h"
#include "wio/sema/symbol.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace wio::sema
{
    using ConstVariableDeclarationMap =
        std::unordered_map<const Symbol*, const VariableDeclaration*>;

    class ConstExpressionEvaluator
    {
    public:
        explicit ConstExpressionEvaluator(const ConstVariableDeclarationMap& declarations);

        std::optional<std::int64_t> evaluateInteger(const NodePtr<Expression>& expression) const;
        std::optional<std::int64_t> evaluateInteger(
            const NodePtr<Expression>& expression,
            std::unordered_set<const Symbol*>& activeSymbols) const;

    private:
        const ConstVariableDeclarationMap& declarations_;
    };
}
