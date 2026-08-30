#include "wio/ast/attribute_queries.h"
#include "wio/ast/attribute_contract.h"

#include <algorithm>

namespace wio::attribute_queries
{
    bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, const Attribute target)
    {
        return std::ranges::any_of(attributes, [target](const auto& attribute)
        {
            return attribute && matchesBuiltinAttribute(*attribute, target);
        });
    }

    std::vector<Token> getAllAttributeArgs(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        const Attribute target)
    {
        std::vector<Token> result;
        for (const auto& attribute : attributes)
        {
            if (attribute && matchesBuiltinAttribute(*attribute, target))
                result.insert(result.end(), attribute->args.begin(), attribute->args.end());
        }
        return result;
    }

    std::vector<Token> getFirstAttributeArgs(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        const Attribute target)
    {
        for (const auto& attribute : attributes)
        {
            if (attribute && matchesBuiltinAttribute(*attribute, target))
                return attribute->args;
        }
        return {};
    }

    const Token* getFirstAttributeArg(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        const Attribute target)
    {
        for (const auto& attribute : attributes)
        {
            if (attribute && matchesBuiltinAttribute(*attribute, target) && !attribute->args.empty())
                return &attribute->args.front();
        }
        return nullptr;
    }

    std::optional<Token> getSingleAttributeArg(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        const Attribute target)
    {
        for (const auto& attribute : attributes)
        {
            if (attribute && matchesBuiltinAttribute(*attribute, target) && attribute->args.size() == 1)
                return attribute->args.front();
        }
        return std::nullopt;
    }

    std::vector<const AttributeStatement*> getAttributeStatements(
        const std::vector<NodePtr<AttributeStatement>>& attributes,
        const Attribute target)
    {
        std::vector<const AttributeStatement*> result;
        for (const auto& attribute : attributes)
        {
            if (attribute && matchesBuiltinAttribute(*attribute, target))
                result.push_back(attribute.Get());
        }
        return result;
    }
}
