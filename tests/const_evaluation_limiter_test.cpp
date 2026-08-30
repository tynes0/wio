#include "wio_compiler/sema/detail/const_evaluation_limiter.h"

#include <iostream>
#include <string>

namespace
{
    bool expect(const bool condition, const char* message)
    {
        if (condition)
            return true;
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace wio;
    using namespace wio::sema;
    using namespace wio::sema::detail;

    const ConstVariableDeclarationMap declarations;
    bool ok = true;

    {
        ConstEvaluationLimiter limiter(declarations);
        const auto literal = makeNodePtr<IntegerLiteral>(Token{
            .type = TokenType::integerLiteral,
            .value = "42"
        });
        ok &= expect(
            limiter.validate(literal) == ConstEvaluationLimitStatus::Valid,
            "ordinary constant expressions must stay within the evaluation budget");
    }

    {
        NodePtr<Expression> expression = makeNodePtr<IntegerLiteral>(Token{
            .type = TokenType::integerLiteral,
            .value = "1"
        });
        for (std::size_t depth = 0; depth <= ConstEvaluationLimiter::MaxDepth; ++depth)
        {
            expression = makeNodePtr<UnaryExpression>(
                Token{.type = TokenType::opMinus, .value = "-"},
                std::move(expression));
        }

        ConstEvaluationLimiter limiter(declarations);
        ok &= expect(
            limiter.validate(expression) == ConstEvaluationLimitStatus::DepthLimit,
            "deep constant expressions must report the depth limit");
    }

    {
        const auto oversizedText = makeNodePtr<StringLiteral>(Token{
            .type = TokenType::stringLiteral,
            .value = std::string(ConstEvaluationLimiter::MaxTextBytes + 1, 'x')
        });
        ConstEvaluationLimiter limiter(declarations);
        ok &= expect(
            limiter.validate(oversizedText) == ConstEvaluationLimitStatus::TextSizeLimit,
            "oversized folded strings must report the text-size limit");
    }

    return ok ? 0 : 1;
}
