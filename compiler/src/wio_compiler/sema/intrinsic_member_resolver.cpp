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

        std::optional<IntrinsicMemberResolution> resolveArrayIntrinsicMember(TypeContext& typeContext,
                                                                             const Ref<ArrayType>& arrayType,
                                                                             const std::string_view memberName)
        {
            if (!arrayType)
                return std::nullopt;

            const Ref<Type> ownerType = arrayType;
            const Ref<Type> elementType = arrayType->elementType;
            const Ref<Type> dynamicArrayType = makeDynamicArrayType(typeContext, elementType);
            const bool supportsResizableMutation = arrayType->arrayKind == ArrayType::ArrayKind::Dynamic;
            const bool supportsEquality = supportsIntrinsicEquality(elementType);
            const bool supportsOrdering = supportsIntrinsicOrdering(elementType);

            if (memberName == "Count")
                return makeMethodResolution(IntrinsicMember::ArrayCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                return makeMethodResolution(IntrinsicMember::ArrayEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "Capacity")
                return makeMethodResolution(IntrinsicMember::ArrayCapacity, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Contains" && supportsEquality)
                return makeMethodResolution(IntrinsicMember::ArrayContains, typeContext.getBool(), { elementType }, typeContext, false);
            if (memberName == "IndexOf" && supportsEquality)
                return makeMethodResolution(IntrinsicMember::ArrayIndexOf, typeContext.getISize(), { elementType }, typeContext, false);
            if (memberName == "LastIndexOf" && supportsEquality)
                return makeMethodResolution(IntrinsicMember::ArrayLastIndexOf, typeContext.getISize(), { elementType }, typeContext, false);
            if (memberName == "First")
                return makeMethodResolution(IntrinsicMember::ArrayFirst, elementType, {}, typeContext, false);
            if (memberName == "Last")
                return makeMethodResolution(IntrinsicMember::ArrayLast, elementType, {}, typeContext, false);
            if (memberName == "GetOr")
                return makeMethodResolution(IntrinsicMember::ArrayGetOr, elementType, { typeContext.getUSize(), elementType }, typeContext, false);
            if (memberName == "Clone")
                return makeMethodResolution(IntrinsicMember::ArrayClone, ownerType, {}, typeContext, false);
            if (memberName == "Slice")
                return makeMethodResolution(IntrinsicMember::ArraySlice, dynamicArrayType, { typeContext.getUSize(), typeContext.getUSize() }, typeContext, false);
            if (memberName == "Take")
                return makeMethodResolution(IntrinsicMember::ArrayTake, dynamicArrayType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Skip")
                return makeMethodResolution(IntrinsicMember::ArraySkip, dynamicArrayType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Concat")
                return makeMethodResolution(IntrinsicMember::ArrayConcat, dynamicArrayType, { ownerType }, typeContext, false);
            if (memberName == "Reversed")
                return makeMethodResolution(IntrinsicMember::ArrayReversed, ownerType, {}, typeContext, false);
            if (memberName == "Join" && isStringType(elementType))
                return makeMethodResolution(IntrinsicMember::ArrayJoin, typeContext.getString(), { typeContext.getString() }, typeContext, false);
            if (memberName == "Push" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayPush, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "PushFront" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayPushFront, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "Pop" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayPop, elementType, {}, typeContext, true);
            if (memberName == "PopFront" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayPopFront, elementType, {}, typeContext, true);
            if (memberName == "Insert" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayInsert, typeContext.getVoid(), { typeContext.getUSize(), elementType }, typeContext, true);
            if (memberName == "RemoveAt" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayRemoveAt, typeContext.getVoid(), { typeContext.getUSize() }, typeContext, true);
            if (memberName == "Remove" && supportsResizableMutation && supportsEquality)
                return makeMethodResolution(IntrinsicMember::ArrayRemove, typeContext.getBool(), { elementType }, typeContext, true);
            if (memberName == "Clear" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Extend" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayExtend, typeContext.getVoid(), { dynamicArrayType }, typeContext, true);
            if (memberName == "Reserve" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayReserve, typeContext.getVoid(), { typeContext.getUSize() }, typeContext, true);
            if (memberName == "ShrinkToFit" && supportsResizableMutation)
                return makeMethodResolution(IntrinsicMember::ArrayShrinkToFit, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Fill")
                return makeMethodResolution(IntrinsicMember::ArrayFill, typeContext.getVoid(), { elementType }, typeContext, true);
            if (memberName == "Reverse")
                return makeMethodResolution(IntrinsicMember::ArrayReverse, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Sort" && supportsOrdering)
                return makeMethodResolution(IntrinsicMember::ArraySort, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Sorted" && supportsOrdering)
                return makeMethodResolution(IntrinsicMember::ArraySorted, ownerType, {}, typeContext, false);

            return std::nullopt;
        }

        std::optional<IntrinsicMemberResolution> resolveDictionaryIntrinsicMember(TypeContext& typeContext,
                                                                                  const Ref<DictionaryType>& dictType,
                                                                                  const std::string_view memberName)
        {
            if (!dictType)
                return std::nullopt;

            const Ref<Type> ownerType = dictType;
            const Ref<Type> keyType = dictType->keyType;
            const Ref<Type> valueType = dictType->valueType;
            const Ref<Type> keyArrayType = makeDynamicArrayType(typeContext, keyType);
            const Ref<Type> valueArrayType = makeDynamicArrayType(typeContext, valueType);
            const Ref<Type> outValueRefType = typeContext.getOrCreateReferenceType(valueType, true);
            const bool supportsValueEquality = supportsIntrinsicEquality(valueType);

            if (memberName == "Count")
                return makeMethodResolution(IntrinsicMember::DictCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                return makeMethodResolution(IntrinsicMember::DictEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "ContainsKey")
                return makeMethodResolution(IntrinsicMember::DictContainsKey, typeContext.getBool(), { keyType }, typeContext, false);
            if (memberName == "ContainsValue" && supportsValueEquality)
                return makeMethodResolution(IntrinsicMember::DictContainsValue, typeContext.getBool(), { valueType }, typeContext, false);
            if (memberName == "Get")
                return makeMethodResolution(IntrinsicMember::DictGet, valueType, { keyType }, typeContext, false);
            if (memberName == "GetOr")
                return makeMethodResolution(IntrinsicMember::DictGetOr, valueType, { keyType, valueType }, typeContext, false);
            if (memberName == "TryGet")
                return makeMethodResolution(IntrinsicMember::DictTryGet, typeContext.getBool(), { keyType, outValueRefType }, typeContext, false);
            if (memberName == "Set")
                return makeMethodResolution(IntrinsicMember::DictSet, typeContext.getVoid(), { keyType, valueType }, typeContext, true);
            if (memberName == "GetOrAdd")
                return makeMethodResolution(IntrinsicMember::DictGetOrAdd, valueType, { keyType, valueType }, typeContext, true);
            if (memberName == "Keys")
                return makeMethodResolution(IntrinsicMember::DictKeys, keyArrayType, {}, typeContext, false);
            if (memberName == "Values")
                return makeMethodResolution(IntrinsicMember::DictValues, valueArrayType, {}, typeContext, false);
            if (memberName == "Clone")
                return makeMethodResolution(IntrinsicMember::DictClone, ownerType, {}, typeContext, false);
            if (memberName == "Merge")
                return makeMethodResolution(IntrinsicMember::DictMerge, ownerType, { ownerType }, typeContext, false);
            if (memberName == "Extend")
                return makeMethodResolution(IntrinsicMember::DictExtend, typeContext.getVoid(), { ownerType }, typeContext, true);
            if (memberName == "Clear")
                return makeMethodResolution(IntrinsicMember::DictClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Remove")
                return makeMethodResolution(IntrinsicMember::DictRemove, typeContext.getBool(), { keyType }, typeContext, true);

            if (!dictType->isOrdered)
                return std::nullopt;

            if (memberName == "FirstKey")
                return makeMethodResolution(IntrinsicMember::TreeFirstKey, keyType, {}, typeContext, false);
            if (memberName == "FirstValue")
                return makeMethodResolution(IntrinsicMember::TreeFirstValue, valueType, {}, typeContext, false);
            if (memberName == "LastKey")
                return makeMethodResolution(IntrinsicMember::TreeLastKey, keyType, {}, typeContext, false);
            if (memberName == "LastValue")
                return makeMethodResolution(IntrinsicMember::TreeLastValue, valueType, {}, typeContext, false);
            if (memberName == "FloorKeyOr")
                return makeMethodResolution(IntrinsicMember::TreeFloorKeyOr, keyType, { keyType, keyType }, typeContext, false);
            if (memberName == "CeilKeyOr")
                return makeMethodResolution(IntrinsicMember::TreeCeilKeyOr, keyType, { keyType, keyType }, typeContext, false);

            return std::nullopt;
        }

        std::optional<IntrinsicMemberResolution> resolveStringIntrinsicMember(TypeContext& typeContext,
                                                                              const std::string_view memberName)
        {
            const Ref<Type> stringType = typeContext.getString();
            const Ref<Type> stringArrayType = makeDynamicArrayType(typeContext, stringType);

            if (memberName == "Count")
                return makeMethodResolution(IntrinsicMember::StringCount, typeContext.getUSize(), {}, typeContext, false);
            if (memberName == "Empty")
                return makeMethodResolution(IntrinsicMember::StringEmpty, typeContext.getBool(), {}, typeContext, false);
            if (memberName == "Contains")
                return makeMethodResolution(IntrinsicMember::StringContains, typeContext.getBool(), { stringType }, typeContext, false);
            if (memberName == "ContainsChar")
                return makeMethodResolution(IntrinsicMember::StringContainsChar, typeContext.getBool(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "StartsWith")
                return makeMethodResolution(IntrinsicMember::StringStartsWith, typeContext.getBool(), { stringType }, typeContext, false);
            if (memberName == "EndsWith")
                return makeMethodResolution(IntrinsicMember::StringEndsWith, typeContext.getBool(), { stringType }, typeContext, false);
            if (memberName == "IndexOf")
                return makeMethodResolution(IntrinsicMember::StringIndexOf, typeContext.getISize(), { stringType }, typeContext, false);
            if (memberName == "LastIndexOf")
                return makeMethodResolution(IntrinsicMember::StringLastIndexOf, typeContext.getISize(), { stringType }, typeContext, false);
            if (memberName == "IndexOfChar")
                return makeMethodResolution(IntrinsicMember::StringIndexOfChar, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "LastIndexOfChar")
                return makeMethodResolution(IntrinsicMember::StringLastIndexOfChar, typeContext.getISize(), { typeContext.getChar() }, typeContext, false);
            if (memberName == "First")
                return makeMethodResolution(IntrinsicMember::StringFirst, typeContext.getChar(), {}, typeContext, false);
            if (memberName == "Last")
                return makeMethodResolution(IntrinsicMember::StringLast, typeContext.getChar(), {}, typeContext, false);
            if (memberName == "GetOr")
                return makeMethodResolution(IntrinsicMember::StringGetOr, typeContext.getChar(), { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            if (memberName == "Slice")
                return makeMethodResolution(IntrinsicMember::StringSlice, stringType, { typeContext.getUSize(), typeContext.getUSize() }, typeContext, false);
            if (memberName == "SliceFrom")
                return makeMethodResolution(IntrinsicMember::StringSliceFrom, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Take")
                return makeMethodResolution(IntrinsicMember::StringTake, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Skip")
                return makeMethodResolution(IntrinsicMember::StringSkip, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Left")
                return makeMethodResolution(IntrinsicMember::StringLeft, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Right")
                return makeMethodResolution(IntrinsicMember::StringRight, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Trim")
                return makeMethodResolution(IntrinsicMember::StringTrim, stringType, {}, typeContext, false);
            if (memberName == "TrimStart")
                return makeMethodResolution(IntrinsicMember::StringTrimStart, stringType, {}, typeContext, false);
            if (memberName == "TrimEnd")
                return makeMethodResolution(IntrinsicMember::StringTrimEnd, stringType, {}, typeContext, false);
            if (memberName == "ToLower")
                return makeMethodResolution(IntrinsicMember::StringToLower, stringType, {}, typeContext, false);
            if (memberName == "ToUpper")
                return makeMethodResolution(IntrinsicMember::StringToUpper, stringType, {}, typeContext, false);
            if (memberName == "Replace")
                return makeMethodResolution(IntrinsicMember::StringReplace, stringType, { stringType, stringType }, typeContext, false);
            if (memberName == "ReplaceFirst")
                return makeMethodResolution(IntrinsicMember::StringReplaceFirst, stringType, { stringType, stringType }, typeContext, false);
            if (memberName == "Repeat")
                return makeMethodResolution(IntrinsicMember::StringRepeat, stringType, { typeContext.getUSize() }, typeContext, false);
            if (memberName == "Split")
                return makeMethodResolution(IntrinsicMember::StringSplit, stringArrayType, { stringType }, typeContext, false);
            if (memberName == "Lines")
                return makeMethodResolution(IntrinsicMember::StringLines, stringArrayType, {}, typeContext, false);
            if (memberName == "PadLeft")
                return makeMethodResolution(IntrinsicMember::StringPadLeft, stringType, { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            if (memberName == "PadRight")
                return makeMethodResolution(IntrinsicMember::StringPadRight, stringType, { typeContext.getUSize(), typeContext.getChar() }, typeContext, false);
            if (memberName == "Reversed")
                return makeMethodResolution(IntrinsicMember::StringReversed, stringType, {}, typeContext, false);
            if (memberName == "Append")
                return makeMethodResolution(IntrinsicMember::StringAppend, typeContext.getVoid(), { stringType }, typeContext, true);
            if (memberName == "Push")
                return makeMethodResolution(IntrinsicMember::StringPush, typeContext.getVoid(), { typeContext.getChar() }, typeContext, true);
            if (memberName == "Insert")
                return makeMethodResolution(IntrinsicMember::StringInsert, typeContext.getVoid(), { typeContext.getUSize(), stringType }, typeContext, true);
            if (memberName == "Erase")
                return makeMethodResolution(IntrinsicMember::StringErase, typeContext.getVoid(), { typeContext.getUSize(), typeContext.getUSize() }, typeContext, true);
            if (memberName == "Clear")
                return makeMethodResolution(IntrinsicMember::StringClear, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "Reverse")
                return makeMethodResolution(IntrinsicMember::StringReverse, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ReplaceInPlace")
                return makeMethodResolution(IntrinsicMember::StringReplaceInPlace, typeContext.getVoid(), { stringType, stringType }, typeContext, true);
            if (memberName == "TrimInPlace")
                return makeMethodResolution(IntrinsicMember::StringTrimInPlace, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ToLowerInPlace")
                return makeMethodResolution(IntrinsicMember::StringToLowerInPlace, typeContext.getVoid(), {}, typeContext, true);
            if (memberName == "ToUpperInPlace")
                return makeMethodResolution(IntrinsicMember::StringToUpperInPlace, typeContext.getVoid(), {}, typeContext, true);

            return std::nullopt;
        }
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
        Ref<Type> resolvedOwnerType = unwrapAliasType(ownerType);
        if (!resolvedOwnerType)
            return std::nullopt;

        if (resolvedOwnerType->kind() == TypeKind::Array)
            return resolveArrayIntrinsicMember(typeContext, resolvedOwnerType.AsFast<ArrayType>(), memberName);

        if (resolvedOwnerType->kind() == TypeKind::Dictionary)
            return resolveDictionaryIntrinsicMember(typeContext, resolvedOwnerType.AsFast<DictionaryType>(), memberName);

        if (isStringType(resolvedOwnerType))
            return resolveStringIntrinsicMember(typeContext, memberName);

        return std::nullopt;
    }
}
