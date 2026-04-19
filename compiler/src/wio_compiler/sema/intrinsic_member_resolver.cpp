#include "wio/sema/intrinsic_member_resolver.h"

namespace wio::sema
{
    namespace
    {
        Ref<Type> unwrapAliasType(Ref<Type> type)
        {
            while (type && type->kind() == TypeKind::Alias)
                type = type.AsFast<AliasType>()->aliasedType;
            return type;
        }

        bool isPrimitiveNamed(const Ref<Type>& type, const std::string_view name)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == name;
        }

        bool isStringType(const Ref<Type>& type)
        {
            return isPrimitiveNamed(type, "string");
        }

        bool supportsIntrinsicEquality(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (resolvedType->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string& name = resolvedType.AsFast<PrimitiveType>()->name;
                return name != "void" && name != "<unknown>";
            }
            case TypeKind::Null:
            case TypeKind::GenericParameter:
                return true;
            case TypeKind::Array:
                return supportsIntrinsicEquality(resolvedType.AsFast<ArrayType>()->elementType);
            case TypeKind::Dictionary:
            {
                auto dictType = resolvedType.AsFast<DictionaryType>();
                return supportsIntrinsicEquality(dictType->keyType) &&
                       supportsIntrinsicEquality(dictType->valueType);
            }
            default:
                return false;
            }
        }

        bool supportsIntrinsicOrdering(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (resolvedType->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string& name = resolvedType.AsFast<PrimitiveType>()->name;
                return name != "void" && name != "<unknown>";
            }
            case TypeKind::GenericParameter:
                return true;
            default:
                return false;
            }
        }

        IntrinsicMemberResolution makeMethodResolution(IntrinsicMember member,
                                                       const Ref<Type>& returnType,
                                                       std::vector<Ref<Type>> paramTypes,
                                                       TypeContext& typeContext,
                                                       const bool requiresMutableReceiver)
        {
            return IntrinsicMemberResolution{
                .member = member,
                .memberType = typeContext.getOrCreateFunctionType(returnType, std::move(paramTypes)),
                .requiresMutableReceiver = requiresMutableReceiver
            };
        }

        Ref<Type> makeDynamicArrayType(TypeContext& typeContext, const Ref<Type>& elementType)
        {
            return typeContext.getOrCreateArrayType(elementType, ArrayType::ArrayKind::Dynamic);
        }

        void appendMethodResolution(std::vector<IntrinsicMemberResolution>& overloads,
                                    IntrinsicMember member,
                                    const Ref<Type>& returnType,
                                    std::vector<Ref<Type>> paramTypes,
                                    TypeContext& typeContext,
                                    const bool requiresMutableReceiver)
        {
            overloads.push_back(makeMethodResolution(member, returnType, std::move(paramTypes), typeContext, requiresMutableReceiver));
        }

        std::vector<IntrinsicMemberResolution> resolveArrayIntrinsicMember(TypeContext& typeContext,
                                                                           const Ref<ArrayType>& arrayType,
                                                                           const std::string_view memberName)
        {
            std::vector<IntrinsicMemberResolution> overloads;
            if (!arrayType)
                return overloads;

            const Ref<Type> ownerType = arrayType;
            const Ref<Type> elementType = arrayType->elementType;
            const Ref<Type> dynamicArrayType = makeDynamicArrayType(typeContext, elementType);
            const bool supportsResizableMutation = arrayType->arrayKind == ArrayType::ArrayKind::Dynamic;
            const bool supportsEquality = supportsIntrinsicEquality(elementType);
            const bool supportsOrdering = supportsIntrinsicOrdering(elementType);

            if (memberName == "Count")
                appendMethodResolution(overloads, IntrinsicMember::ArrayCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                appendMethodResolution(overloads, IntrinsicMember::ArrayEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "Capacity")
                appendMethodResolution(overloads, IntrinsicMember::ArrayCapacity, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Contains" && supportsEquality)
                appendMethodResolution(overloads, IntrinsicMember::ArrayContains, typeContext.getBool(), { elementType }, typeContext, false);
            if (memberName == "IndexOf" && supportsEquality)
                appendMethodResolution(overloads, IntrinsicMember::ArrayIndexOf, typeContext.getISize(), { elementType }, typeContext, false);
            if (memberName == "LastIndexOf" && supportsEquality)
                appendMethodResolution(overloads, IntrinsicMember::ArrayLastIndexOf, typeContext.getISize(), { elementType }, typeContext, false);
            if (memberName == "First")
                appendMethodResolution(overloads, IntrinsicMember::ArrayFirst, elementType, {}, typeContext, false);
            if (memberName == "Last")
                appendMethodResolution(overloads, IntrinsicMember::ArrayLast, elementType, {}, typeContext, false);
            if (memberName == "GetOr")
                appendMethodResolution(overloads, IntrinsicMember::ArrayGetOr, elementType, { typeContext.getUSize(), elementType }, typeContext, false);
            if (memberName == "Clone")
                appendMethodResolution(overloads, IntrinsicMember::ArrayClone, ownerType, {}, typeContext, false);
            if (memberName == "Slice")
            {
                appendMethodResolution(overloads, IntrinsicMember::ArraySlice, dynamicArrayType, { typeContext.getUSize() }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::ArraySlice, dynamicArrayType, { typeContext.getUSize(), typeContext.getUSize() }, typeContext, false);
            }
            if (memberName == "Take")
                appendMethodResolution(overloads, IntrinsicMember::ArrayTake, dynamicArrayType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Skip")
                appendMethodResolution(overloads, IntrinsicMember::ArraySkip, dynamicArrayType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Concat")
                appendMethodResolution(overloads, IntrinsicMember::ArrayConcat, dynamicArrayType, { ownerType }, typeContext, false);
            if (memberName == "Reversed")
                appendMethodResolution(overloads, IntrinsicMember::ArrayReversed, ownerType, {}, typeContext, false);
            if (memberName == "Join" && isStringType(elementType))
                appendMethodResolution(overloads, IntrinsicMember::ArrayJoin, typeContext.getString(), { typeContext.getString() }, typeContext, false);
            if (memberName == "Push" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayPush, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "PushFront" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayPushFront, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "Pop" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayPop, elementType, {}, typeContext, true);
            if (memberName == "PopFront" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayPopFront, elementType, {}, typeContext, true);
            if (memberName == "Insert" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayInsert, typeContext.getVoid(), { typeContext.getUSize(), elementType }, typeContext, true);
            if (memberName == "RemoveAt" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayRemoveAt, typeContext.getVoid(), { typeContext.getUSize() }, typeContext, true);
            if (memberName == "Remove" && supportsResizableMutation && supportsEquality)
                appendMethodResolution(overloads, IntrinsicMember::ArrayRemove, typeContext.getBool(), { elementType }, typeContext, true);
            if (memberName == "Clear" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Extend" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayExtend, typeContext.getVoid(), { dynamicArrayType }, typeContext, true);
            if (memberName == "Reserve" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayReserve, typeContext.getVoid(), { typeContext.getUSize() }, typeContext, true);
            if (memberName == "ShrinkToFit" && supportsResizableMutation)
                appendMethodResolution(overloads, IntrinsicMember::ArrayShrinkToFit, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Fill")
                appendMethodResolution(overloads, IntrinsicMember::ArrayFill, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "Reverse")
                appendMethodResolution(overloads, IntrinsicMember::ArrayReverse, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Sort" && supportsOrdering)
                appendMethodResolution(overloads, IntrinsicMember::ArraySort, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Sorted" && supportsOrdering)
                appendMethodResolution(overloads, IntrinsicMember::ArraySorted, ownerType, {}, typeContext, false);

            return overloads;
        }

        std::vector<IntrinsicMemberResolution> resolveDictionaryIntrinsicMember(TypeContext& typeContext,
                                                                                const Ref<DictionaryType>& dictType,
                                                                                const std::string_view memberName)
        {
            std::vector<IntrinsicMemberResolution> overloads;
            if (!dictType)
                return overloads;

            const Ref<Type> ownerType = dictType;
            const Ref<Type> keyType = dictType->keyType;
            const Ref<Type> valueType = dictType->valueType;
            const Ref<Type> keyArrayType = makeDynamicArrayType(typeContext, keyType);
            const Ref<Type> valueArrayType = makeDynamicArrayType(typeContext, valueType);
            const Ref<Type> outValueRefType = typeContext.getOrCreateReferenceType(valueType, true);
            const bool supportsValueEquality = supportsIntrinsicEquality(valueType);

            if (memberName == "Count")
                appendMethodResolution(overloads, IntrinsicMember::DictCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                appendMethodResolution(overloads, IntrinsicMember::DictEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "ContainsKey")
                appendMethodResolution(overloads, IntrinsicMember::DictContainsKey, typeContext.getBool(), { keyType }, typeContext, false);
            if (memberName == "ContainsValue" && supportsValueEquality)
                appendMethodResolution(overloads, IntrinsicMember::DictContainsValue, typeContext.getBool(), { valueType }, typeContext, false);
            if (memberName == "Get")
                appendMethodResolution(overloads, IntrinsicMember::DictGet, valueType, { keyType }, typeContext, false);
            if (memberName == "GetOr")
                appendMethodResolution(overloads, IntrinsicMember::DictGetOr, valueType, { keyType, valueType }, typeContext, false);
            if (memberName == "TryGet")
                appendMethodResolution(overloads, IntrinsicMember::DictTryGet, typeContext.getBool(), { keyType, outValueRefType }, typeContext, false);
            if (memberName == "Set")
                appendMethodResolution(overloads, IntrinsicMember::DictSet, typeContext.getVoid(), { keyType, valueType }, typeContext, true);
            if (memberName == "GetOrAdd")
                appendMethodResolution(overloads, IntrinsicMember::DictGetOrAdd, valueType, { keyType, valueType }, typeContext, true);
            if (memberName == "Keys")
                appendMethodResolution(overloads, IntrinsicMember::DictKeys, keyArrayType, {}, typeContext, false);
            if (memberName == "Values")
                appendMethodResolution(overloads, IntrinsicMember::DictValues, valueArrayType, {}, typeContext, false);
            if (memberName == "Clone")
                appendMethodResolution(overloads, IntrinsicMember::DictClone, ownerType, {}, typeContext, false);
            if (memberName == "Merge")
                appendMethodResolution(overloads, IntrinsicMember::DictMerge, ownerType, { ownerType }, typeContext, false);
            if (memberName == "Extend")
                appendMethodResolution(overloads, IntrinsicMember::DictExtend, typeContext.getVoid(), { ownerType }, typeContext, true);
            if (memberName == "Clear")
                appendMethodResolution(overloads, IntrinsicMember::DictClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Remove")
                appendMethodResolution(overloads, IntrinsicMember::DictRemove, typeContext.getBool(), { keyType }, typeContext, true);

            if (!dictType->isOrdered)
                return overloads;

            if (memberName == "FirstKey")
                appendMethodResolution(overloads, IntrinsicMember::TreeFirstKey, keyType, {}, typeContext, false);
            if (memberName == "FirstValue")
                appendMethodResolution(overloads, IntrinsicMember::TreeFirstValue, valueType, {}, typeContext, false);
            if (memberName == "LastKey")
                appendMethodResolution(overloads, IntrinsicMember::TreeLastKey, keyType, {}, typeContext, false);
            if (memberName == "LastValue")
                appendMethodResolution(overloads, IntrinsicMember::TreeLastValue, valueType, {}, typeContext, false);
            if (memberName == "FloorKeyOr")
                appendMethodResolution(overloads, IntrinsicMember::TreeFloorKeyOr, keyType, { keyType, keyType }, typeContext, false);
            if (memberName == "CeilKeyOr")
                appendMethodResolution(overloads, IntrinsicMember::TreeCeilKeyOr, keyType, { keyType, keyType }, typeContext, false);

            return overloads;
        }

        std::vector<IntrinsicMemberResolution> resolveStringIntrinsicMember(TypeContext& typeContext,
                                                                            const std::string_view memberName)
        {
            std::vector<IntrinsicMemberResolution> overloads;
            const Ref<Type> stringType = typeContext.getString();
            const Ref<Type> stringArrayType = makeDynamicArrayType(typeContext, stringType);

            if (memberName == "Count")
                appendMethodResolution(overloads, IntrinsicMember::StringCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                appendMethodResolution(overloads, IntrinsicMember::StringEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "Contains")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringContains, typeContext.getBool(), { stringType }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringContains, typeContext.getBool(), { typeContext.getChar() }, typeContext, false);
            }
            if (memberName == "ContainsChar")
                appendMethodResolution(overloads, IntrinsicMember::StringContainsChar, typeContext.getBool(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "StartsWith")
                appendMethodResolution(overloads, IntrinsicMember::StringStartsWith, typeContext.getBool(), { stringType }, typeContext, false);
            if (memberName == "EndsWith")
                appendMethodResolution(overloads, IntrinsicMember::StringEndsWith, typeContext.getBool(), { stringType }, typeContext, false);
            if (memberName == "IndexOf")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringIndexOf, typeContext.getISize(), { stringType }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringIndexOf, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            }
            if (memberName == "LastIndexOf")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringLastIndexOf, typeContext.getISize(), { stringType }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringLastIndexOf, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            }
            if (memberName == "IndexOfChar")
                appendMethodResolution(overloads, IntrinsicMember::StringIndexOfChar, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "LastIndexOfChar")
                appendMethodResolution(overloads, IntrinsicMember::StringLastIndexOfChar, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "First")
                appendMethodResolution(overloads, IntrinsicMember::StringFirst, typeContext.getChar(), {}, typeContext, false);
            if (memberName == "Last")
                appendMethodResolution(overloads, IntrinsicMember::StringLast, typeContext.getChar(), {}, typeContext, false);
            if (memberName == "GetOr")
                appendMethodResolution(overloads, IntrinsicMember::StringGetOr, typeContext.getChar(), { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            if (memberName == "Slice")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringSlice, stringType, { typeContext.getUSize() }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringSlice, stringType, { typeContext.getUSize(), typeContext.getUSize() }, typeContext, false);
            }
            if (memberName == "SliceFrom")
                appendMethodResolution(overloads, IntrinsicMember::StringSliceFrom, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Take")
                appendMethodResolution(overloads, IntrinsicMember::StringTake, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Skip")
                appendMethodResolution(overloads, IntrinsicMember::StringSkip, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Left")
                appendMethodResolution(overloads, IntrinsicMember::StringLeft, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Right")
                appendMethodResolution(overloads, IntrinsicMember::StringRight, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Trim")
                appendMethodResolution(overloads, IntrinsicMember::StringTrim, stringType, {}, typeContext, false);
            if (memberName == "TrimStart")
                appendMethodResolution(overloads, IntrinsicMember::StringTrimStart, stringType, {}, typeContext, false);
            if (memberName == "TrimEnd")
                appendMethodResolution(overloads, IntrinsicMember::StringTrimEnd, stringType, {}, typeContext, false);
            if (memberName == "ToLower")
                appendMethodResolution(overloads, IntrinsicMember::StringToLower, stringType, {}, typeContext, false);
            if (memberName == "ToUpper")
                appendMethodResolution(overloads, IntrinsicMember::StringToUpper, stringType, {}, typeContext, false);
            if (memberName == "Replace")
                appendMethodResolution(overloads, IntrinsicMember::StringReplace, stringType, { stringType, stringType }, typeContext, false);
            if (memberName == "ReplaceFirst")
                appendMethodResolution(overloads, IntrinsicMember::StringReplaceFirst, stringType, { stringType, stringType }, typeContext, false);
            if (memberName == "Repeat")
                appendMethodResolution(overloads, IntrinsicMember::StringRepeat, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Split")
                appendMethodResolution(overloads, IntrinsicMember::StringSplit, stringArrayType, { stringType }, typeContext, false);
            if (memberName == "Lines")
                appendMethodResolution(overloads, IntrinsicMember::StringLines, stringArrayType, {}, typeContext, false);
            if (memberName == "PadLeft")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringPadLeft, stringType, { typeContext.getUSize() }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringPadLeft, stringType, { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            }
            if (memberName == "PadRight")
            {
                appendMethodResolution(overloads, IntrinsicMember::StringPadRight, stringType, { typeContext.getUSize() }, typeContext, false);
                appendMethodResolution(overloads, IntrinsicMember::StringPadRight, stringType, { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            }
            if (memberName == "Reversed")
                appendMethodResolution(overloads, IntrinsicMember::StringReversed, stringType, {}, typeContext, false);
            if (memberName == "Append")
                appendMethodResolution(overloads, IntrinsicMember::StringAppend, typeContext.getVoid(), { stringType }, typeContext, true);
            if (memberName == "Push")
                appendMethodResolution(overloads, IntrinsicMember::StringPush, typeContext.getVoid(), { typeContext.getChar() }, typeContext, true);
            if (memberName == "Insert")
                appendMethodResolution(overloads, IntrinsicMember::StringInsert, typeContext.getVoid(), { typeContext.getUSize(), stringType }, typeContext, true);
            if (memberName == "Erase")
                appendMethodResolution(overloads, IntrinsicMember::StringErase, typeContext.getVoid(), { typeContext.getUSize(), typeContext.getUSize() }, typeContext, true);
            if (memberName == "Clear")
                appendMethodResolution(overloads, IntrinsicMember::StringClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Reverse")
                appendMethodResolution(overloads, IntrinsicMember::StringReverse, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ReplaceInPlace")
                appendMethodResolution(overloads, IntrinsicMember::StringReplaceInPlace, typeContext.getVoid(), { stringType, stringType }, typeContext, true);
            if (memberName == "TrimInPlace")
                appendMethodResolution(overloads, IntrinsicMember::StringTrimInPlace, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ToLowerInPlace")
                appendMethodResolution(overloads, IntrinsicMember::StringToLowerInPlace, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ToUpperInPlace")
                appendMethodResolution(overloads, IntrinsicMember::StringToUpperInPlace, typeContext.getVoid(), {}, typeContext, true);

            return overloads;
        }
    }

    std::vector<IntrinsicMemberResolution> resolveIntrinsicMemberOverloads(TypeContext& typeContext,
                                                                           const Ref<Type>& ownerType,
                                                                           const std::string_view memberName)
    {
        Ref<Type> resolvedOwnerType = unwrapAliasType(ownerType);
        if (!resolvedOwnerType)
            return {};

        if (resolvedOwnerType->kind() == TypeKind::Array)
            return resolveArrayIntrinsicMember(typeContext, resolvedOwnerType.AsFast<ArrayType>(), memberName);

        if (resolvedOwnerType->kind() == TypeKind::Dictionary)
            return resolveDictionaryIntrinsicMember(typeContext, resolvedOwnerType.AsFast<DictionaryType>(), memberName);

        if (isStringType(resolvedOwnerType))
            return resolveStringIntrinsicMember(typeContext, memberName);

        return {};
    }

    bool isDynamicArrayOnlyIntrinsicMemberName(const std::string_view memberName) noexcept
    {
        return memberName == "Push" ||
               memberName == "PushFront" ||
               memberName == "Pop" ||
               memberName == "PopFront" ||
               memberName == "Insert" ||
               memberName == "RemoveAt" ||
               memberName == "Remove" ||
               memberName == "Clear" ||
               memberName == "Extend" ||
               memberName == "Reserve" ||
               memberName == "ShrinkToFit";
    }

    bool isMutatingIntrinsicMember(const IntrinsicMember member) noexcept
    {
        switch (member)
        {
        case IntrinsicMember::ArrayPush:
        case IntrinsicMember::ArrayPushFront:
        case IntrinsicMember::ArrayPop:
        case IntrinsicMember::ArrayPopFront:
        case IntrinsicMember::ArrayInsert:
        case IntrinsicMember::ArrayClear:
        case IntrinsicMember::ArrayRemoveAt:
        case IntrinsicMember::ArrayRemove:
        case IntrinsicMember::ArrayExtend:
        case IntrinsicMember::ArrayReserve:
        case IntrinsicMember::ArrayShrinkToFit:
        case IntrinsicMember::ArrayFill:
        case IntrinsicMember::ArrayReverse:
        case IntrinsicMember::ArraySort:
        case IntrinsicMember::DictSet:
        case IntrinsicMember::DictGetOrAdd:
        case IntrinsicMember::DictExtend:
        case IntrinsicMember::DictClear:
        case IntrinsicMember::DictRemove:
        case IntrinsicMember::StringAppend:
        case IntrinsicMember::StringPush:
        case IntrinsicMember::StringInsert:
        case IntrinsicMember::StringErase:
        case IntrinsicMember::StringClear:
        case IntrinsicMember::StringReverse:
        case IntrinsicMember::StringReplaceInPlace:
        case IntrinsicMember::StringTrimInPlace:
        case IntrinsicMember::StringToLowerInPlace:
        case IntrinsicMember::StringToUpperInPlace:
            return true;
        default:
            return false;
        }
    }

    std::optional<IntrinsicMemberResolution> resolveIntrinsicMember(TypeContext& typeContext,
                                                                    const Ref<Type>& ownerType,
                                                                    const std::string_view memberName)
    {
        auto overloads = resolveIntrinsicMemberOverloads(typeContext, ownerType, memberName);
        if (overloads.size() == 1)
            return overloads.front();
        return std::nullopt;
    }
}
