#pragma once

#include "wio/sema/constant_evaluator.h"
#include "wio/sema/type.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wio::sema::generic_support
{
    struct GenericBindingSet
    {
        std::unordered_map<std::string, Ref<Type>> directBindings;
        std::unordered_map<std::string, std::vector<Ref<Type>>> packBindings;
        std::unordered_map<std::string, std::string> packAliases;
    };

    enum class PackElementBindingKind : std::uint8_t
    {
        Absolute,
        FromEnd
    };

    enum class SymbolicPackNameMode : std::uint8_t
    {
        Direct,
        Normalized
    };

    struct ParsedPackElementBinding
    {
        std::string packName;
        std::size_t value = 0;
        PackElementBindingKind kind = PackElementBindingKind::Absolute;
    };

    struct PackSizeReference
    {
        std::optional<std::string> packName;
        std::optional<std::size_t> concreteSize;
    };

    std::unordered_map<std::string, Ref<Type>> buildGenericTypeBindings(
        const std::vector<std::string>& parameterNames,
        const std::vector<Ref<Type>>& typeArguments);

    std::string makePackElementBindingName(const std::string& packName, std::size_t index);
    std::string makePackTailElementBindingName(const std::string& packName, std::size_t distanceFromEnd);
    std::string makePackElementBindingName(const ParsedPackElementBinding& binding);
    std::optional<ParsedPackElementBinding> tryParsePackElementBindingName(std::string_view name);
    std::optional<std::size_t> tryResolveConcretePackElementIndex(
        const ParsedPackElementBinding& binding,
        std::size_t packSize);
    std::optional<std::string> tryGetSymbolicPackReferenceName(const Ref<Type>& type);
    std::optional<std::string> tryGetNormalizedSymbolicPackName(const Ref<Type>& type);
    std::size_t getMinimumGenericArgumentCount(
        const std::vector<std::string>& parameterNames,
        bool hasGenericParameterPack);
    GenericBindingSet buildExtendedGenericBindings(
        const std::vector<std::string>& parameterNames,
        bool hasGenericParameterPack,
        const std::vector<Ref<Type>>& typeArguments);

    std::optional<std::size_t> tryEvaluateStaticPackIndex(
        const NodePtr<Expression>& expression,
        const ConstVariableDeclarationMap& declarations);
    std::optional<PackSizeReference> tryResolvePackSizeReference(
        const NodePtr<Expression>& expression,
        SymbolicPackNameMode nameMode = SymbolicPackNameMode::Normalized);
    std::optional<ParsedPackElementBinding> tryEvaluatePackIndexBinding(
        const NodePtr<Expression>& expression,
        const ConstVariableDeclarationMap& declarations,
        std::optional<std::string_view> expectedPackName = std::nullopt,
        SymbolicPackNameMode nameMode = SymbolicPackNameMode::Normalized);
}
