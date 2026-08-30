#include "wio/sema/type_queries.h"

#include <algorithm>

namespace wio::sema::type_queries
{
    Ref<Type> unwrapAliasType(Ref<Type> type)
    {
        while (type && type->kind() == TypeKind::Alias)
            type = type.AsFast<AliasType>()->aliasedType;
        return type;
    }

    bool isPrimitiveNamed(const Ref<Type>& type, const std::string_view name)
    {
        const Ref<Type> resolved = unwrapAliasType(type);
        return resolved && resolved->kind() == TypeKind::Primitive &&
               resolved.AsFast<PrimitiveType>()->name == name;
    }

    bool isStdLibraryScopePath(const std::string_view scopePath)
    {
        return scopePath == "std" || scopePath.starts_with("std::") || scopePath.starts_with("std_");
    }

    Ref<StructType> getStdValueStructType(const Ref<Type>& type, const std::string_view expectedName)
    {
        const Ref<Type> resolved = unwrapAliasType(type);
        if (!resolved || resolved->kind() != TypeKind::Struct)
            return nullptr;

        auto structure = resolved.AsFast<StructType>();
        return structure && structure->name == expectedName && isStdLibraryScopePath(structure->scopePath)
            ? structure
            : nullptr;
    }

    bool isSdkValueBridgeType(const Ref<Type>& type)
    {
        const Ref<Type> resolved = unwrapAliasType(type);
        if (!resolved)
            return false;

        if (resolved->kind() == TypeKind::Primitive)
        {
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name != "void" && name != "object" && name != "any" && name != "opaque";
        }

        if (resolved->kind() == TypeKind::Array)
        {
            const auto array = resolved.AsFast<ArrayType>();
            return array && isSdkValueBridgeType(array->elementType);
        }

        if (resolved->kind() == TypeKind::Dictionary)
        {
            const auto dictionary = resolved.AsFast<DictionaryType>();
            return dictionary && isSdkValueBridgeType(dictionary->keyType) &&
                   isSdkValueBridgeType(dictionary->valueType);
        }

        if (const auto unit = getStdValueStructType(resolved, "ResultUnit"))
            return unit->genericArguments.empty();
        if (const auto span = getStdValueStructType(resolved, "Span"))
            return span->genericArguments.empty();
        if (const auto byteBuffer = getStdValueStructType(resolved, "ByteBuffer"))
            return byteBuffer->genericArguments.empty();

        if (const auto tuple = getStdValueStructType(resolved, "Tuple"))
        {
            return std::all_of(tuple->genericArguments.begin(), tuple->genericArguments.end(), [](const Ref<Type>& argument)
            {
                return isSdkValueBridgeType(argument);
            });
        }

        const Ref<StructType> unaryContainer = [&]() -> Ref<StructType>
        {
            for (const std::string_view name : {"Option", "Result", "Queue", "UnorderedSet", "OrderedSet"})
            {
                if (auto structure = getStdValueStructType(resolved, name))
                    return structure;
            }
            return nullptr;
        }();

        return unaryContainer && unaryContainer->genericArguments.size() == 1 &&
               isSdkValueBridgeType(unaryContainer->genericArguments.front());
    }

    bool shouldAutoReadReferenceType(const Ref<Type>& type)
    {
        Ref<Type> current = unwrapAliasType(type);
        if (!current || current->kind() != TypeKind::Reference)
            return false;

        while (current && current->kind() == TypeKind::Reference)
        {
            current = unwrapAliasType(current.AsFast<ReferenceType>()->referredType);
            if (!current)
                return false;
            if (current->kind() == TypeKind::Struct)
            {
                const auto structure = current.AsFast<StructType>();
                if (structure->isObject || structure->isInterface)
                    return false;
            }
        }
        return true;
    }

    Ref<Type> getAutoReadableType(const Ref<Type>& type)
    {
        Ref<Type> current = unwrapAliasType(type);
        if (!shouldAutoReadReferenceType(type))
            return current;
        while (current && current->kind() == TypeKind::Reference)
            current = unwrapAliasType(current.AsFast<ReferenceType>()->referredType);
        return current;
    }

    std::size_t getAutoReadableReferenceDepth(const Ref<Type>& type)
    {
        if (!shouldAutoReadReferenceType(type))
            return 0;

        std::size_t depth = 0;
        Ref<Type> current = unwrapAliasType(type);
        while (current && current->kind() == TypeKind::Reference)
        {
            ++depth;
            current = unwrapAliasType(current.AsFast<ReferenceType>()->referredType);
        }
        return depth;
    }
}
