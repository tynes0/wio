#pragma once

#include "wio/ast/ast.h"

#include <cstddef>

namespace wio::declaration_queries
{
    std::size_t getFixedParameterCount(const FunctionDeclaration& declaration);
    std::size_t getRequiredParameterCount(const FunctionDeclaration& declaration);
    std::size_t getRequiredParameterCount(const FunctionDeclaration* declaration);
    bool hasDefaultParameters(const FunctionDeclaration& declaration);
}
