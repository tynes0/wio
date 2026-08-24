#pragma once

#include "wio/ast/ast.h"

#include <optional>
#include <span>
#include <string_view>

namespace wio
{
    struct BuiltinAttributeContract
    {
        Attribute attribute = Attribute::Unknown;
        std::string_view canonicalName;
        std::span<const std::string_view> targets;
        std::span<const std::string_view> requiredAttributes;
        std::span<const std::string_view> requiredAnyAttributes;
        std::span<const std::string_view> conflictingAttributes;
        bool repeatable = false;
    };

    // Stable public identities shared by semantic validation, reflection,
    // tooling, and the SDK. Attribute remains a lowering discriminator while
    // the v0.15 migration is in progress.
    const BuiltinAttributeContract* getBuiltinAttributeContract(Attribute attribute);
    std::optional<Attribute> resolveBuiltinAttribute(std::string_view name);
    std::string_view canonicalBuiltinAttributeName(Attribute attribute);
    bool matchesBuiltinAttribute(const AttributeStatement& statement, Attribute attribute);
}
