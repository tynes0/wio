#include "wio/ast/declaration_queries.h"

#include <algorithm>
#include <ranges>

namespace wio::declaration_queries
{
    std::size_t getFixedParameterCount(const FunctionDeclaration& declaration)
    {
        const bool hasParameterPack = std::ranges::any_of(declaration.parameters, [](const Parameter& parameter)
        {
            return parameter.isParameterPack;
        });
        return hasParameterPack && !declaration.parameters.empty()
            ? declaration.parameters.size() - 1
            : declaration.parameters.size();
    }

    std::size_t getRequiredParameterCount(const FunctionDeclaration& declaration)
    {
        std::size_t requiredCount = getFixedParameterCount(declaration);
        while (requiredCount > 0 && declaration.parameters[requiredCount - 1].defaultValue)
            --requiredCount;
        return requiredCount;
    }

    std::size_t getRequiredParameterCount(const FunctionDeclaration* declaration)
    {
        return declaration ? getRequiredParameterCount(*declaration) : 0;
    }

    bool hasDefaultParameters(const FunctionDeclaration& declaration)
    {
        return getRequiredParameterCount(declaration) != getFixedParameterCount(declaration);
    }
}
