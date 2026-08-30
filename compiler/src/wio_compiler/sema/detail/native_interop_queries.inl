// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        Ref<Symbol> resolveAttributeSymbol(const Ref<Scope>& startScope, const Token& token)
        {
            if (token.type != TokenType::identifier)
                return nullptr;

            return resolveQualifiedSymbol(startScope, token.value);
        }

        Ref<StructType> resolveTrustedStructType(SemanticAnalyzer& analyzer,
                                                 const Ref<Scope>& startScope,
                                                 const AttributeTypeArgument& trustArg,
                                                 common::Location errorLocation)
        {
            Ref<Type> trustedType = nullptr;

            if (trustArg.typeSpecifier)
            {
                trustArg.typeSpecifier->accept(analyzer);
                trustedType = trustArg.typeSpecifier->refType.Lock();
            }
            else if (auto trustSym = resolveAttributeSymbol(startScope, trustArg.token))
            {
                trustedType = trustSym->type;
            }

            trustedType = unwrapAliasType(trustedType);
            if (!trustedType || trustedType->kind() != TypeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(errorLocation, "@Trust expects object/component/interface type names.");
                return nullptr;
            }

            return trustedType.AsFast<StructType>();
        }

        bool isCAbiSafeExportType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return false;

            if (current->kind() != TypeKind::Primitive)
                return false;

            const std::string typeName = current->toString();
            return typeName != "string" && typeName != "object";
        }

        bool isSdkExportableFieldType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return false;

            switch (current->kind())
            {
            case TypeKind::Nullable:
                return isSdkExportableFieldType(current.AsFast<NullableType>()->valueType);
            case TypeKind::Primitive:
            {
                const std::string typeName = current->toString();
                return typeName != "void" && typeName != "object";
            }
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return arrayType && isSdkValueBridgeType(arrayType->elementType);
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return dictType &&
                    isSdkValueBridgeType(dictType->keyType) &&
                    isSdkValueBridgeType(dictType->valueType);
            }
            case TypeKind::Function:
            {
                auto functionType = current.AsFast<FunctionType>();
                if (!functionType || !isSdkExportableFieldType(functionType->returnType))
                    return false;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (!isSdkExportableFieldType(parameterType))
                        return false;
                }

                return true;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                if (!structType || structType->isInterface)
                    return false;

                if ((structType->name == "Option" || structType->name == "Result" ||
                     structType->name == "Queue" || structType->name == "UnorderedSet" ||
                     structType->name == "OrderedSet") &&
                    isStdLibraryScopePath(structType->scopePath))
                {
                    return structType->genericArguments.size() == 1 &&
                        isSdkValueBridgeType(structType->genericArguments.front());
                }

                if (structType->name == "Tuple" && isStdLibraryScopePath(structType->scopePath))
                {
                    return std::all_of(
                        structType->genericArguments.begin(),
                        structType->genericArguments.end(),
                        [](const Ref<Type>& argument) { return isSdkValueBridgeType(argument); }
                    );
                }

                if (!structType->genericArguments.empty() &&
                    structType->genericPrimaryType.Lock() &&
                    !structType->isExplicitSpecialization)
                {
                    return false;
                }

                return true;
            }
            default:
                return false;
            }
        }

        bool isNativePodInteropFieldType(const Ref<Type>& type, const bool allowGenericPlaceholders)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return false;

            switch (current->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string typeName = current->toString();
                return typeName != "void" && typeName != "string" && typeName != "object";
            }
            case TypeKind::GenericParameter:
            case TypeKind::ConstGenericParameter:
                return allowGenericPlaceholders;
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                if (!arrayType || arrayType->arrayKind != ArrayType::ArrayKind::Static)
                    return false;

                return isNativePodInteropFieldType(arrayType->elementType, allowGenericPlaceholders);
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                    return false;

                if (allowGenericPlaceholders)
                    return true;

                return std::ranges::all_of(structType->fieldTypes, [](const Ref<Type>& fieldType)
                {
                    return isNativePodInteropFieldType(fieldType, false);
                });
            }
            default:
                return false;
            }
        }

        void validateInstantiatedNativePodComponent(const Ref<StructType>& structType,
                                                    const common::Location& errorLocation)
        {
            if (!structType || !structType->isNativePodComponent)
                return;

            const bool hasConcreteInstantiation =
                structType->genericParameterNames.empty() ||
                (!structType->genericArguments.empty() &&
                 structType->genericArguments.size() == structType->genericParameterNames.size() &&
                 std::ranges::all_of(structType->genericArguments, [](const Ref<Type>& genericArgument)
                 {
                     return genericArgument && !genericArgument->isUnknown() && !containsGenericParameterType(genericArgument);
                 }));

            if (!hasConcreteInstantiation)
                return;

            for (size_t fieldIndex = 0; fieldIndex < structType->fieldTypes.size() && fieldIndex < structType->fieldNames.size(); ++fieldIndex)
            {
                if (isNativePodInteropFieldType(structType->fieldTypes[fieldIndex], false))
                    continue;

                WIO_LOG_ADD_ERROR(
                    errorLocation,
                    "Declaration-level @Native component '{}' field '{}' resolves to type '{}' which is not POD-native-compatible yet. Supported field types are primitives, POD-compatible static arrays, and other declaration-level @Native components.",
                    structType->toString(),
                    structType->fieldNames[fieldIndex],
                    structType->fieldTypes[fieldIndex] ? structType->fieldTypes[fieldIndex]->toString() : "<unknown>"
                );
            }
        }

        Ref<StructType> getNativePodComponentStructType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return nullptr;

            if (current->kind() == TypeKind::Reference)
                current = current.AsFast<ReferenceType>()->referredType;

            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current || current->kind() != TypeKind::Struct)
                return nullptr;

            auto structType = current.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                return nullptr;

            if (!structType->genericArguments.empty())
            {
                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto baseSymbol = structScope->resolve(structType->name);
                        baseSymbol && baseSymbol->kind == SymbolKind::Struct)
                    {
                        auto baseStruct = baseSymbol->type.AsFast<StructType>();
                        if (baseStruct && baseStruct.Get() != structType.Get() && !baseStruct->genericParameterNames.empty())
                            structType = instantiateGenericStructType(baseStruct, structType->genericArguments).AsFast<StructType>();
                    }
                }
            }

            return structType;
        }

        bool isExactType(const Ref<Type>& actual, const Ref<Type>& expected)
        {
            Ref<Type> lhs = actual;
            Ref<Type> rhs = expected;

            while (lhs && lhs->kind() == TypeKind::Alias)
                lhs = lhs.AsFast<AliasType>()->aliasedType;

            while (rhs && rhs->kind() == TypeKind::Alias)
                rhs = rhs.AsFast<AliasType>()->aliasedType;

            return lhs && rhs && lhs->isCompatibleWith(rhs) && rhs->isCompatibleWith(lhs);
        }
