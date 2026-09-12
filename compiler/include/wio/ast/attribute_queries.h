#pragma once

#include "wio/ast/ast.h"

#include <optional>
#include <vector>

namespace wio::attribute_queries
{
    bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute target);
    std::vector<Token> getAllAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute target);
    std::vector<Token> getFirstAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute target);
    const Token* getFirstAttributeArg(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute target);
    std::optional<Token> getSingleAttributeArg(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute target);
    std::vector<const AttributeStatement*> getAttributeStatements(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        Attribute target);
}
