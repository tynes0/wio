        using attribute_queries::getFirstAttributeArgs;
        using attribute_queries::getSingleAttributeArg;
        using attribute_queries::hasAttribute;
        using cpp_identifier::isCppReservedIdentifier;
        using cpp_identifier::replaceCppIdentifier;
        using cpp_identifier::sanitizeCppIdentifier;
        using declaration_queries::getFixedParameterCount;
        using declaration_queries::getRequiredParameterCount;
        using declaration_queries::hasDefaultParameters;
        using sema::scope_lookup::resolveQualifiedSymbol;
        using sema::generic_support::GenericBindingSet;
        using sema::generic_support::PackElementBindingKind;
        using sema::generic_support::ParsedPackElementBinding;
        using sema::generic_support::SymbolicPackNameMode;
        using sema::generic_support::buildExtendedGenericBindings;
        using sema::generic_support::buildGenericTypeBindings;
        using sema::generic_support::getMinimumGenericArgumentCount;
        using sema::generic_support::makePackElementBindingName;
        using sema::generic_support::makePackTailElementBindingName;
        using sema::generic_support::tryEvaluatePackIndexBinding;
        using sema::generic_support::tryEvaluateStaticPackIndex;
        using sema::generic_support::tryGetSymbolicPackReferenceName;
        using sema::generic_support::tryParsePackElementBindingName;
        using sema::generic_support::tryResolveConcretePackElementIndex;
        using sema::type_queries::getAutoReadableReferenceDepth;
        using sema::type_queries::getStdValueStructType;
        using sema::type_queries::isSdkValueBridgeType;
        using sema::type_queries::isStdLibraryScopePath;
        using sema::type_queries::shouldAutoReadReferenceType;
        using sema::type_queries::unwrapAliasType;

        std::string formatCppTemplateParameter(
            const NodePtr<Identifier>& parameter,
            const bool isPack,
            std::string_view nameOverride = {})
        {
            if (!parameter)
                return "typename _wio_missing";

            const std::string backendName = sanitizeCppIdentifier(
                nameOverride.empty() ? parameter->token.value : nameOverride);

            if (parameter->isConstGenericParameter)
            {
                Ref<sema::Type> valueType = parameter->genericValueType
                    ? parameter->genericValueType->refType.Lock()
                    : nullptr;
                Ref<sema::Type> resolvedValueType = valueType;
                while (resolvedValueType && resolvedValueType->kind() == sema::TypeKind::Alias)
                    resolvedValueType = resolvedValueType.AsFast<sema::AliasType>()->aliasedType;
                if (resolvedValueType && resolvedValueType->kind() == sema::TypeKind::Primitive)
                {
                    const std::string& name = resolvedValueType.AsFast<sema::PrimitiveType>()->name;
                    if (name == "string" || name == "text")
                    {
                        return std::string(name == "text"
                            ? "wio::runtime::ConstText "
                            : "wio::runtime::ConstString ") +
                            backendName;
                    }
                }
                return (valueType ? valueType->toCppString() : "std::size_t") +
                       " " + backendName;
            }

            return std::string(isPack ? "typename... " : "typename ") +
                   backendName;
        }

        #include "backend_type_queries.inl"

        #include "generic_codegen.inl"

        std::vector<std::vector<Ref<sema::Type>>> getInstantiateTypeLists(const FunctionDeclaration& node)
        {
            if (auto functionSymbol = node.name ? node.name->referencedSymbol.Lock() : nullptr)
                return functionSymbol->resolvedGenericInstantiations;

            return {};
        }

        std::string formatInstantiatedLogicalName(const std::string& baseName, const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = baseName + "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                result += instantiationTypes[i] ? instantiationTypes[i]->toString() : "unknown";
                if (i + 1 < instantiationTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }

        std::string formatInstantiatedExportSymbolName(const std::string& baseName, const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = baseName;
            for (const auto& instantiationType : instantiationTypes)
            {
                result += "__";
                std::string fragment = Mangler::mangleType(instantiationType);
                std::ranges::replace(fragment, ':', '_');
                result += fragment;
            }
            return result;
        }

        std::string formatTemplateArgumentList(const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                result += toCppType(instantiationTypes[i]);
                if (i + 1 < instantiationTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }

        bool shouldTreatReferenceAsHandleForAssignment(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                return false;

            auto referenceType = resolvedType.AsFast<sema::ReferenceType>();
            Ref<sema::Type> referredType = unwrapAliasType(referenceType->referredType);
            if (!referredType || referredType->kind() != sema::TypeKind::Struct)
                return false;

            auto structType = referredType.AsFast<sema::StructType>();
            return structType && (structType->isObject || structType->isInterface);
        }

        std::string_view getIntrinsicHelperName(const IntrinsicMember member)
        {
            switch (member)
            {
            case IntrinsicMember::TaskStart:
                return "TaskStart";
            case IntrinsicMember::TaskCancel:
                return "TaskCancel";
            case IntrinsicMember::TaskIsReady:
                return "TaskIsReady";
            case IntrinsicMember::TaskIsCancelled:
                return "TaskIsCancelled";
            case IntrinsicMember::TaskIsFaulted:
                return "TaskIsFaulted";
            case IntrinsicMember::TaskWaitFor:
                return "TaskWaitFor";
            case IntrinsicMember::TaskBlock:
                return "TaskBlock";
            case IntrinsicMember::TaskCancelAfter:
                return "TaskCancelAfter";
            case IntrinsicMember::TaskDetach:
                return "TaskDetach";
            case IntrinsicMember::ArrayCount:
            case IntrinsicMember::DictCount:
            case IntrinsicMember::StringCount:
                return "Count";
            case IntrinsicMember::ArrayEmpty:
            case IntrinsicMember::DictEmpty:
            case IntrinsicMember::StringEmpty:
                return "Empty";
            case IntrinsicMember::TextCount:
                return "TextCount";
            case IntrinsicMember::TextByteCount:
                return "TextByteCount";
            case IntrinsicMember::TextEmpty:
                return "TextEmpty";
            case IntrinsicMember::TextSlice:
                return "TextSlice";
            case IntrinsicMember::TextToString:
                return "TextToString";
            case IntrinsicMember::TextContains:
                return "TextContains";
            case IntrinsicMember::TextStartsWith:
                return "TextStartsWith";
            case IntrinsicMember::TextEndsWith:
                return "TextEndsWith";
            case IntrinsicMember::TextGraphemeCount:
                return "TextGraphemeCount";
            case IntrinsicMember::TextSliceGraphemes:
                return "TextSliceGraphemes";
            case IntrinsicMember::TextDisplayWidth:
                return "TextDisplayWidth";
            case IntrinsicMember::TextCaseFold:
                return "TextCaseFold";
            case IntrinsicMember::TextCodePoints:
                return "TextCodePoints";
            case IntrinsicMember::TextGraphemes:
                return "TextGraphemes";
            case IntrinsicMember::ArrayCapacity:
                return "ArrayCapacity";
            case IntrinsicMember::ArrayContains:
                return "ArrayContains";
            case IntrinsicMember::ArrayIndexOf:
                return "ArrayIndexOf";
            case IntrinsicMember::ArrayLastIndexOf:
                return "ArrayLastIndexOf";
            case IntrinsicMember::ArrayFirst:
                return "ArrayFirst";
            case IntrinsicMember::ArrayLast:
                return "ArrayLast";
            case IntrinsicMember::ArrayGet:
                return "Index";
            case IntrinsicMember::ArrayGetOr:
                return "ArrayGetOr";
            case IntrinsicMember::ArrayClone:
                return "ArrayClone";
            case IntrinsicMember::ArraySlice:
                return "ArraySlice";
            case IntrinsicMember::ArrayTake:
                return "ArrayTake";
            case IntrinsicMember::ArraySkip:
                return "ArraySkip";
            case IntrinsicMember::ArrayConcat:
                return "ArrayConcat";
            case IntrinsicMember::ArrayReversed:
                return "ArrayReversed";
            case IntrinsicMember::ArrayJoin:
                return "ArrayJoin";
            case IntrinsicMember::ArrayPush:
                return "ArrayPush";
            case IntrinsicMember::ArrayPushFront:
                return "ArrayPushFront";
            case IntrinsicMember::ArrayPop:
                return "ArrayPop";
            case IntrinsicMember::ArrayPopFront:
                return "ArrayPopFront";
            case IntrinsicMember::ArrayInsert:
                return "ArrayInsert";
            case IntrinsicMember::ArrayClear:
                return "ArrayClear";
            case IntrinsicMember::ArrayRemoveAt:
                return "ArrayRemoveAt";
            case IntrinsicMember::ArrayRemove:
                return "ArrayRemove";
            case IntrinsicMember::ArrayExtend:
                return "ArrayExtend";
            case IntrinsicMember::ArrayReserve:
                return "ArrayReserve";
            case IntrinsicMember::ArrayShrinkToFit:
                return "ArrayShrinkToFit";
            case IntrinsicMember::ArrayFill:
                return "ArrayFill";
            case IntrinsicMember::ArrayReverse:
                return "ArrayReverse";
            case IntrinsicMember::ArraySort:
                return "ArraySort";
            case IntrinsicMember::ArraySorted:
                return "ArraySorted";
            case IntrinsicMember::DictContainsKey:
                return "DictContainsKey";
            case IntrinsicMember::DictContainsValue:
                return "DictContainsValue";
            case IntrinsicMember::DictGet:
                return "DictGet";
            case IntrinsicMember::DictGetOr:
                return "DictGetOr";
            case IntrinsicMember::DictTryGet:
                return "DictTryGet";
            case IntrinsicMember::DictSet:
                return "DictSet";
            case IntrinsicMember::DictGetOrAdd:
                return "DictGetOrAdd";
            case IntrinsicMember::DictKeys:
                return "DictKeys";
            case IntrinsicMember::DictValues:
                return "DictValues";
            case IntrinsicMember::DictClone:
                return "DictClone";
            case IntrinsicMember::DictMerge:
                return "DictMerge";
            case IntrinsicMember::DictExtend:
                return "DictExtend";
            case IntrinsicMember::DictClear:
                return "DictClear";
            case IntrinsicMember::DictRemove:
                return "DictRemove";
            case IntrinsicMember::TreeFirstKey:
                return "TreeFirstKey";
            case IntrinsicMember::TreeFirstValue:
                return "TreeFirstValue";
            case IntrinsicMember::TreeLastKey:
                return "TreeLastKey";
            case IntrinsicMember::TreeLastValue:
                return "TreeLastValue";
            case IntrinsicMember::TreeFloorKeyOr:
                return "TreeFloorKeyOr";
            case IntrinsicMember::TreeCeilKeyOr:
                return "TreeCeilKeyOr";
            case IntrinsicMember::StringContains:
                return "StringContains";
            case IntrinsicMember::StringContainsChar:
                return "StringContainsChar";
            case IntrinsicMember::StringStartsWith:
                return "StringStartsWith";
            case IntrinsicMember::StringEndsWith:
                return "StringEndsWith";
            case IntrinsicMember::StringIndexOf:
                return "StringIndexOf";
            case IntrinsicMember::StringLastIndexOf:
                return "StringLastIndexOf";
            case IntrinsicMember::StringIndexOfChar:
                return "StringIndexOfChar";
            case IntrinsicMember::StringLastIndexOfChar:
                return "StringLastIndexOfChar";
            case IntrinsicMember::StringFirst:
                return "StringFirst";
            case IntrinsicMember::StringLast:
                return "StringLast";
            case IntrinsicMember::StringGet:
                return "Index";
            case IntrinsicMember::StringGetOr:
                return "StringGetOr";
            case IntrinsicMember::StringSlice:
                return "StringSlice";
            case IntrinsicMember::StringSliceFrom:
                return "StringSliceFrom";
            case IntrinsicMember::StringTake:
                return "StringTake";
            case IntrinsicMember::StringSkip:
                return "StringSkip";
            case IntrinsicMember::StringLeft:
                return "StringLeft";
            case IntrinsicMember::StringRight:
                return "StringRight";
            case IntrinsicMember::StringTrim:
                return "StringTrim";
            case IntrinsicMember::StringTrimStart:
                return "StringTrimStart";
            case IntrinsicMember::StringTrimEnd:
                return "StringTrimEnd";
            case IntrinsicMember::StringToLower:
                return "StringToLower";
            case IntrinsicMember::StringToUpper:
                return "StringToUpper";
            case IntrinsicMember::StringReplace:
                return "StringReplace";
            case IntrinsicMember::StringReplaceFirst:
                return "StringReplaceFirst";
            case IntrinsicMember::StringRepeat:
                return "StringRepeat";
            case IntrinsicMember::StringSplit:
                return "StringSplit";
            case IntrinsicMember::StringLines:
                return "StringLines";
            case IntrinsicMember::StringPadLeft:
                return "StringPadLeft";
            case IntrinsicMember::StringPadRight:
                return "StringPadRight";
            case IntrinsicMember::StringReversed:
                return "StringReversed";
            case IntrinsicMember::StringToI8:
                return "StringToI8";
            case IntrinsicMember::StringToI16:
                return "StringToI16";
            case IntrinsicMember::StringToI32:
                return "StringToI32";
            case IntrinsicMember::StringToI64:
                return "StringToI64";
            case IntrinsicMember::StringToU8:
                return "StringToU8";
            case IntrinsicMember::StringToU16:
                return "StringToU16";
            case IntrinsicMember::StringToU32:
                return "StringToU32";
            case IntrinsicMember::StringToU64:
                return "StringToU64";
            case IntrinsicMember::StringToISize:
                return "StringToISize";
            case IntrinsicMember::StringToUSize:
                return "StringToUSize";
            case IntrinsicMember::StringToF32:
                return "StringToF32";
            case IntrinsicMember::StringToF64:
                return "StringToF64";
            case IntrinsicMember::StringToBool:
                return "StringToBool";
            case IntrinsicMember::StringAppend:
                return "StringAppend";
            case IntrinsicMember::StringPush:
                return "StringPush";
            case IntrinsicMember::StringInsert:
                return "StringInsert";
            case IntrinsicMember::StringErase:
                return "StringErase";
            case IntrinsicMember::StringClear:
                return "StringClear";
            case IntrinsicMember::StringReverse:
                return "StringReverse";
            case IntrinsicMember::StringReplaceInPlace:
                return "StringReplaceInPlace";
            case IntrinsicMember::StringTrimInPlace:
                return "StringTrimInPlace";
            case IntrinsicMember::StringToLowerInPlace:
                return "StringToLowerInPlace";
            case IntrinsicMember::StringToUpperInPlace:
                return "StringToUpperInPlace";
            case IntrinsicMember::EnumName:
            case IntrinsicMember::FlagsetName:
                return "EnumName";
            case IntrinsicMember::EnumRawValue:
                return "EnumRawValue";
            case IntrinsicMember::EnumIsValid:
                return "EnumIsValid";
            case IntrinsicMember::FlagsetHasAll:
                return "FlagsetHasAll";
            case IntrinsicMember::FlagsetHasAny:
                return "FlagsetHasAny";
            case IntrinsicMember::FlagsetWith:
                return "FlagsetWith";
            case IntrinsicMember::FlagsetWithout:
                return "FlagsetWithout";
            case IntrinsicMember::FlagsetToggle:
                return "FlagsetToggle";
            case IntrinsicMember::FlagsetClear:
                return "FlagsetClear";
            case IntrinsicMember::TaskPoll:
            case IntrinsicMember::TaskWithin:
            case IntrinsicMember::None:
                return {};
            }

            return {};
        }

        #include "module_export_index.inl"

        #include "sdk_dynamic_bridge.inl"

        #include "named_type_mangling.inl"