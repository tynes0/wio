#include "wio/sema/generic_support.h"

#include "wio/sema/intrinsic_member_resolver.h"
#include "wio/sema/type_queries.h"

#include <algorithm>

namespace wio::sema::generic_support
{
    std::unordered_map<std::string, Ref<Type>> buildGenericTypeBindings(
        const std::vector<std::string>& parameterNames,
        const std::vector<Ref<Type>>& typeArguments)
    {
        std::unordered_map<std::string, Ref<Type>> bindings;
        const std::size_t count = std::min(parameterNames.size(), typeArguments.size());
        bindings.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
            bindings.emplace(parameterNames[index], typeArguments[index]);
        return bindings;
    }

    std::string makePackElementBindingName(const std::string& packName, const std::size_t index)
    {
        return packName + "[" + std::to_string(index) + "]";
    }

    std::string makePackTailElementBindingName(const std::string& packName, const std::size_t distanceFromEnd)
    {
        return distanceFromEnd <= 1
            ? packName + "[last]"
            : packName + "[last-" + std::to_string(distanceFromEnd - 1) + "]";
    }

    std::string makePackElementBindingName(const ParsedPackElementBinding& binding)
    {
        return binding.kind == PackElementBindingKind::FromEnd
            ? makePackTailElementBindingName(binding.packName, binding.value)
            : makePackElementBindingName(binding.packName, binding.value);
    }

    std::optional<ParsedPackElementBinding> tryParsePackElementBindingName(const std::string_view name)
    {
        const std::size_t openBracket = name.find('[');
        if (openBracket == std::string_view::npos || !name.ends_with("]"))
            return std::nullopt;

        const std::string_view packName = name.substr(0, openBracket);
        const std::string_view indexText = name.substr(openBracket + 1, name.size() - openBracket - 2);
        if (packName.empty() || indexText.empty())
            return std::nullopt;

        if (indexText == "last")
            return ParsedPackElementBinding{std::string(packName), 1, PackElementBindingKind::FromEnd};

        std::string_view digits = indexText;
        PackElementBindingKind kind = PackElementBindingKind::Absolute;
        std::size_t adjustment = 0;
        if (indexText.starts_with("last-"))
        {
            digits = indexText.substr(5);
            kind = PackElementBindingKind::FromEnd;
            adjustment = 1;
        }
        if (digits.empty())
            return std::nullopt;

        std::size_t value = 0;
        for (const char character : digits)
        {
            if (character < '0' || character > '9')
                return std::nullopt;
            value = (value * 10) + static_cast<std::size_t>(character - '0');
        }
        return ParsedPackElementBinding{std::string(packName), value + adjustment, kind};
    }

    std::optional<std::size_t> tryResolveConcretePackElementIndex(
        const ParsedPackElementBinding& binding,
        const std::size_t packSize)
    {
        if (binding.kind == PackElementBindingKind::Absolute)
            return binding.value < packSize ? std::optional<std::size_t>(binding.value) : std::nullopt;
        if (binding.value == 0 || binding.value > packSize)
            return std::nullopt;
        return packSize - binding.value;
    }

    std::optional<std::string> tryGetSymbolicPackReferenceName(const Ref<Type>& type)
    {
        const Ref<Type> resolved = type_queries::unwrapAliasType(type);
        if (!resolved)
            return std::nullopt;

        switch (resolved->kind())
        {
        case TypeKind::GenericParameterPack:
            return resolved.AsFast<GenericParameterPackType>()->name;
        case TypeKind::ValuePackView:
        {
            const auto view = resolved.AsFast<ValuePackViewType>();
            return view->elementTypes.empty() ? std::optional<std::string>(view->packName) : std::nullopt;
        }
        case TypeKind::TypePackView:
        {
            const auto view = resolved.AsFast<TypePackViewType>();
            return view->elementTypes.empty() ? std::optional<std::string>(view->packName) : std::nullopt;
        }
        case TypeKind::PackStorage:
        {
            const auto storage = resolved.AsFast<PackStorageType>();
            return storage->elementTypes.empty() ? std::optional<std::string>(storage->packName) : std::nullopt;
        }
        default:
            return std::nullopt;
        }
    }

    std::optional<std::string> tryGetNormalizedSymbolicPackName(const Ref<Type>& type)
    {
        const Ref<Type> resolved = type_queries::unwrapAliasType(type);
        if (!resolved)
            return std::nullopt;
        if (const auto direct = tryGetSymbolicPackReferenceName(resolved))
            return direct;

        const auto aliasFromElements = [](const std::vector<Ref<Type>>& elements) -> std::optional<std::string>
        {
            return elements.size() == 1 ? tryGetSymbolicPackReferenceName(elements.front()) : std::nullopt;
        };
        switch (resolved->kind())
        {
        case TypeKind::ValuePackView:
            return aliasFromElements(resolved.AsFast<ValuePackViewType>()->elementTypes);
        case TypeKind::TypePackView:
            return aliasFromElements(resolved.AsFast<TypePackViewType>()->elementTypes);
        case TypeKind::PackStorage:
            return aliasFromElements(resolved.AsFast<PackStorageType>()->elementTypes);
        default:
            return std::nullopt;
        }
    }

    std::size_t getMinimumGenericArgumentCount(
        const std::vector<std::string>& parameterNames,
        const bool hasGenericParameterPack)
    {
        return parameterNames.empty() ? 0 :
            parameterNames.size() - static_cast<std::size_t>(hasGenericParameterPack);
    }

    GenericBindingSet buildExtendedGenericBindings(
        const std::vector<std::string>& parameterNames,
        const bool hasGenericParameterPack,
        const std::vector<Ref<Type>>& typeArguments)
    {
        GenericBindingSet bindings;
        const std::size_t fixedCount = getMinimumGenericArgumentCount(parameterNames, hasGenericParameterPack);
        bindings.directBindings.reserve(fixedCount + typeArguments.size());
        for (std::size_t index = 0; index < fixedCount && index < typeArguments.size(); ++index)
            bindings.directBindings.emplace(parameterNames[index], typeArguments[index]);

        if (!hasGenericParameterPack || parameterNames.empty())
            return bindings;

        const std::string& packName = parameterNames.back();
        std::vector<Ref<Type>> packTypes;
        if (typeArguments.size() > fixedCount)
        {
            if (typeArguments.size() == fixedCount + 1)
            {
                if (const auto alias = tryGetSymbolicPackReferenceName(typeArguments[fixedCount]))
                {
                    bindings.packAliases.emplace(packName, *alias);
                    bindings.packBindings.emplace(packName, std::move(packTypes));
                    return bindings;
                }
            }

            packTypes.reserve(typeArguments.size() - fixedCount);
            for (std::size_t index = fixedCount; index < typeArguments.size(); ++index)
            {
                packTypes.push_back(typeArguments[index]);
                bindings.directBindings.emplace(
                    makePackElementBindingName(packName, index - fixedCount),
                    typeArguments[index]);
            }
        }
        bindings.packBindings.emplace(packName, std::move(packTypes));
        return bindings;
    }

    std::optional<std::size_t> tryEvaluateStaticPackIndex(
        const NodePtr<Expression>& expression,
        const ConstVariableDeclarationMap& declarations)
    {
        const auto value = ConstExpressionEvaluator(declarations).evaluateInteger(expression);
        return value && *value >= 0 ? std::optional<std::size_t>(static_cast<std::size_t>(*value)) : std::nullopt;
    }

    std::optional<PackSizeReference> tryResolvePackSizeReference(
        const NodePtr<Expression>& expression,
        const SymbolicPackNameMode nameMode)
    {
        if (!expression)
            return std::nullopt;
        const auto* access = expression->as<MemberAccessExpression>();
        if (!access || !access->member ||
            (access->intrinsicMember != IntrinsicMember::PackSize && access->member->token.value != "size"))
        {
            return std::nullopt;
        }

        const Ref<Type> objectType = type_queries::unwrapAliasType(
            access->object ? access->object->refType.Lock() : nullptr);
        if (!objectType)
            return std::nullopt;

        PackSizeReference result;
        const auto resolvePackName = [nameMode](const Ref<Type>& type)
        {
            return nameMode == SymbolicPackNameMode::Normalized
                ? tryGetNormalizedSymbolicPackName(type)
                : tryGetSymbolicPackReferenceName(type);
        };
        switch (objectType->kind())
        {
        case TypeKind::GenericParameterPack:
            result.packName = objectType.AsFast<GenericParameterPackType>()->name;
            return result;
        case TypeKind::ValuePackView:
        {
            const auto view = objectType.AsFast<ValuePackViewType>();
            result.packName = resolvePackName(objectType);
            if (!view->elementTypes.empty()) result.concreteSize = view->elementTypes.size();
            return result;
        }
        case TypeKind::TypePackView:
        {
            const auto view = objectType.AsFast<TypePackViewType>();
            result.packName = resolvePackName(objectType);
            if (!view->elementTypes.empty()) result.concreteSize = view->elementTypes.size();
            return result;
        }
        case TypeKind::PackStorage:
        {
            const auto storage = objectType.AsFast<PackStorageType>();
            result.packName = resolvePackName(objectType);
            if (!storage->elementTypes.empty()) result.concreteSize = storage->elementTypes.size();
            return result;
        }
        default:
            return std::nullopt;
        }
    }

    std::optional<ParsedPackElementBinding> tryEvaluatePackIndexBinding(
        const NodePtr<Expression>& expression,
        const ConstVariableDeclarationMap& declarations,
        const std::optional<std::string_view> expectedPackName,
        const SymbolicPackNameMode nameMode)
    {
        if (!expression)
            return std::nullopt;
        if (const auto index = tryEvaluateStaticPackIndex(expression, declarations))
        {
            return ParsedPackElementBinding{
                expectedPackName ? std::string(*expectedPackName) : std::string(),
                *index,
                PackElementBindingKind::Absolute};
        }

        const auto* binary = expression->as<BinaryExpression>();
        if (!binary || binary->op.type != TokenType::opMinus)
            return std::nullopt;
        const auto size = tryResolvePackSizeReference(binary->left, nameMode);
        const auto rhs = tryEvaluateStaticPackIndex(binary->right, declarations);
        if (!size || !rhs)
            return std::nullopt;
        if (expectedPackName && size->packName && *size->packName != *expectedPackName)
            return std::nullopt;

        if (size->concreteSize)
        {
            if (*rhs > *size->concreteSize)
                return std::nullopt;
            return ParsedPackElementBinding{
                expectedPackName ? std::string(*expectedPackName) : size->packName.value_or(std::string()),
                *size->concreteSize - *rhs,
                PackElementBindingKind::Absolute};
        }
        if (!size->packName)
            return std::nullopt;
        return ParsedPackElementBinding{*size->packName, *rhs, PackElementBindingKind::FromEnd};
    }
}
