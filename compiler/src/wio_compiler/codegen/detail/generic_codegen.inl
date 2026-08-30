// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        Ref<sema::StructType> getNativePodComponentStructTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            if (current->kind() == sema::TypeKind::Reference)
                current = unwrapAliasType(current.AsFast<sema::ReferenceType>()->referredType);

            if (!current || current->kind() != sema::TypeKind::Struct)
                return nullptr;

            auto structType = current.AsFast<sema::StructType>();
            if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                return nullptr;

            if (!structType->genericArguments.empty())
            {
                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto baseSymbol = structScope->resolve(structType->name);
                        baseSymbol && baseSymbol->kind == sema::SymbolKind::Struct)
                    {
                        auto baseStruct = baseSymbol->type.AsFast<sema::StructType>();
                        if (baseStruct && baseStruct.Get() != structType.Get() && !baseStruct->genericParameterNames.empty())
                            structType = instantiateGenericStructType(baseStruct, structType->genericArguments).AsFast<sema::StructType>();
                    }
                }
            }

            return structType;
        }

        bool usesNativePodAliasModelForCodegen(const Ref<sema::StructType>& structType)
        {
            return static_cast<bool>(structType) &&
                   !structType->isObject &&
                   !structType->isInterface &&
                   structType->isNativePodComponent;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type, const GenericBindingSet& bindings);
        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings);

        std::vector<Ref<sema::Type>> getLeadingParameterTypes(const Ref<sema::FunctionType>& functionType, size_t arity)
        {
            std::vector<Ref<sema::Type>> parameterTypes;
            if (!functionType)
                return parameterTypes;

            const size_t cappedArity = std::min(arity, functionType->paramTypes.size());
            parameterTypes.reserve(cappedArity);
            for (size_t i = 0; i < cappedArity; ++i)
                parameterTypes.push_back(functionType->paramTypes[i]);

            return parameterTypes;
        }

        bool containsGenericParameterTypeForCodegen(const Ref<sema::Type>& type)
        {
            if (!type)
                return false;

            Ref<sema::Type> resolvedType = unwrapAliasTypeForCodegen(type);
            if (!resolvedType)
                return false;

            switch (resolvedType->kind())
            {
            case sema::TypeKind::GenericParameter:
            case sema::TypeKind::ConstGenericParameter:
            case sema::TypeKind::GenericParameterPack:
            case sema::TypeKind::ValuePackView:
            case sema::TypeKind::TypePackView:
            case sema::TypeKind::PackStorage:
                return true;
            case sema::TypeKind::Reference:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::ReferenceType>()->referredType);
            case sema::TypeKind::Nullable:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::NullableType>()->valueType);
            case sema::TypeKind::AsyncTask:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::AsyncTaskType>()->valueType);
            case sema::TypeKind::Array:
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                return containsGenericParameterTypeForCodegen(arrayType->elementType) ||
                       (arrayType->extentType && containsGenericParameterTypeForCodegen(arrayType->extentType));
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                return containsGenericParameterTypeForCodegen(dictionaryType->keyType) ||
                       containsGenericParameterTypeForCodegen(dictionaryType->valueType);
            }
            case sema::TypeKind::Function:
            {
                auto functionType = resolvedType.AsFast<sema::FunctionType>();
                if (containsGenericParameterTypeForCodegen(functionType->returnType))
                    return true;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (containsGenericParameterTypeForCodegen(parameterType))
                        return true;
                }
                return false;
            }
            case sema::TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                {
                    if (containsGenericParameterTypeForCodegen(genericArgument))
                        return true;
                }
                return false;
            }
            default:
                return false;
            }
        }

        Ref<sema::Type> instantiateGenericStructType(const Ref<sema::StructType>& structType,
                                                     const std::vector<Ref<sema::Type>>& explicitTypeArguments)
        {
            if (!structType)
                return nullptr;

            if (structType->genericParameterNames.empty())
                return structType;

            if (auto specializationIt = structType->explicitSpecializations.find(
                    sema::getGenericSpecializationKey(explicitTypeArguments));
                specializationIt != structType->explicitSpecializations.end())
            {
                if (auto specialization = specializationIt->second.Lock(); specialization)
                    return specialization;
            }

            const auto bindings = buildExtendedGenericBindings(
                structType->genericParameterNames,
                structType->hasGenericParameterPack,
                explicitTypeArguments
            );
            auto instantiatedScope = structType->structScope.Lock();
            auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                structType->name,
                instantiatedScope,
                structType->isObject,
                structType->isInterface
            ).AsFast<sema::StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericParameterTypes = structType->genericParameterTypes;
            instantiatedType->genericArguments = explicitTypeArguments;
            instantiatedType->genericPrimaryType = structType;
            instantiatedType->hasGenericParameterPack = structType->hasGenericParameterPack;
            instantiatedType->fieldNames = structType->fieldNames;
            instantiatedType->trustedTypeKeys = structType->trustedTypeKeys;
            instantiatedType->isFinal = structType->isFinal;
            instantiatedType->isNativePodComponent = structType->isNativePodComponent;
            instantiatedType->nativeCppName = structType->nativeCppName;
            instantiatedType->nativeCppHeader = structType->nativeCppHeader;

            instantiatedType->fieldTypes.clear();
            instantiatedType->fieldTypes.reserve(structType->fieldTypes.size());
            for (const auto& fieldType : structType->fieldTypes)
                instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, bindings));

            instantiatedType->baseTypes.clear();
            instantiatedType->baseTypes.reserve(structType->baseTypes.size());
            for (const auto& baseType : structType->baseTypes)
                instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, bindings));

            return instantiatedType;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings)
        {
            GenericBindingSet wrappedBindings;
            wrappedBindings.directBindings = bindings;
            return instantiateGenericType(type, wrappedBindings);
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const GenericBindingSet& bindings)
        {
            Ref<sema::Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            auto& ctx = Compiler::get().getTypeContext();

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case sema::TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<sema::GenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto aliasIt = bindings.packAliases.find(parsedPackElement->packName); aliasIt != bindings.packAliases.end())
                    {
                        ParsedPackElementBinding reboundBinding = *parsedPackElement;
                        reboundBinding.packName = aliasIt->second;
                        return ctx.getOrCreateGenericParameterType(makePackElementBindingName(reboundBinding));
                    }

                    if (auto packIt = bindings.packBindings.find(parsedPackElement->packName);
                        packIt != bindings.packBindings.end() && !packIt->second.empty())
                    {
                        if (auto resolvedIndex = tryResolveConcretePackElementIndex(*parsedPackElement, packIt->second.size()))
                            return packIt->second[*resolvedIndex];
                    }
                }
                return current;
            }
            case sema::TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<sema::ConstGenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;
                return current;
            }
            case sema::TypeKind::ConstValue:
                return current;
            case sema::TypeKind::GenericParameterPack:
            {
                auto genericPack = current.AsFast<sema::GenericParameterPackType>();
                if (auto aliasIt = bindings.packAliases.find(genericPack->name); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(genericPack->name); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(genericPack->name, it->second);
                return current;
            }
            case sema::TypeKind::ValuePackView:
            {
                auto viewType = current.AsFast<sema::ValuePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(viewType->elementTypes.size());
                    for (const auto& elementType : viewType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreateValuePackViewType(viewType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(viewType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateValuePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(viewType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreateValuePackViewType(viewType->packName, it->second);
                return current;
            }
            case sema::TypeKind::TypePackView:
            {
                auto viewType = current.AsFast<sema::TypePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(viewType->elementTypes.size());
                    for (const auto& elementType : viewType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreateTypePackViewType(viewType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(viewType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(viewType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(viewType->packName, it->second);
                return current;
            }
            case sema::TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<sema::PackStorageType>();
                if (!storageType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(storageType->elementTypes.size());
                    for (const auto& elementType : storageType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreatePackStorageType(storageType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(storageType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreatePackStorageType(aliasIt->second);
                if (auto it = bindings.packBindings.find(storageType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreatePackStorageType(storageType->packName, it->second);
                return current;
            }
            case sema::TypeKind::Reference:
            {
                auto refType = current.AsFast<sema::ReferenceType>();
                return ctx.getOrCreateReferenceType(
                    instantiateGenericType(refType->referredType, bindings),
                    refType->isMutable
                );
            }
            case sema::TypeKind::Nullable:
                return ctx.getOrCreateNullableType(
                    instantiateGenericType(current.AsFast<sema::NullableType>()->valueType, bindings)
                );
            case sema::TypeKind::Array:
            {
                auto arrayType = current.AsFast<sema::ArrayType>();
                Ref<sema::Type> instantiatedExtent = arrayType->extentType
                    ? instantiateGenericType(arrayType->extentType, bindings)
                    : nullptr;
                size_t concreteSize = arrayType->size;
                if (instantiatedExtent && instantiatedExtent->kind() == sema::TypeKind::ConstValue)
                {
                    const std::string rawValue = common::stripIntegerLiteralTypeSuffix(
                        instantiatedExtent.AsFast<sema::ConstValueType>()->value);
                    concreteSize = static_cast<size_t>(std::stoull(rawValue));
                }
                return ctx.getOrCreateArrayType(
                    instantiateGenericType(arrayType->elementType, bindings),
                    arrayType->arrayKind,
                    concreteSize,
                    instantiatedExtent
                );
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<sema::DictionaryType>();
                return ctx.getOrCreateDictionaryType(
                    instantiateGenericType(dictType->keyType, bindings),
                    instantiateGenericType(dictType->valueType, bindings),
                    dictType->isOrdered
                );
            }
            case sema::TypeKind::Function:
            {
                auto functionType = current.AsFast<sema::FunctionType>();
                std::vector<Ref<sema::Type>> instantiatedParamTypes;
                instantiatedParamTypes.reserve(functionType->paramTypes.size() + 4);

                if (functionType->hasParameterPack && !functionType->paramTypes.empty())
                {
                    const size_t fixedParameterCount = functionType->paramTypes.size() - 1;
                    for (size_t i = 0; i < fixedParameterCount; ++i)
                        instantiatedParamTypes.push_back(instantiateGenericType(functionType->paramTypes[i], bindings));

                    auto trailingType = unwrapAliasType(functionType->paramTypes.back());
                    if (trailingType && trailingType->kind() == sema::TypeKind::GenericParameterPack)
                    {
                        const std::string& packName = trailingType.AsFast<sema::GenericParameterPackType>()->name;
                        if (auto aliasIt = bindings.packAliases.find(packName); aliasIt != bindings.packAliases.end())
                        {
                            instantiatedParamTypes.push_back(ctx.getOrCreateGenericParameterPackType(aliasIt->second));
                            return ctx.getOrCreateFunctionType(
                                instantiateGenericType(functionType->returnType, bindings),
                                instantiatedParamTypes,
                                true
                            );
                        }
                        if (auto it = bindings.packBindings.find(packName); it != bindings.packBindings.end())
                        {
                            if (it->second.empty())
                            {
                                return ctx.getOrCreateFunctionType(
                                    instantiateGenericType(functionType->returnType, bindings),
                                    instantiatedParamTypes,
                                    false
                                );
                            }
                            if (!it->second.empty())
                            {
                                for (const auto& packType : it->second)
                                    instantiatedParamTypes.push_back(packType);
                                return ctx.getOrCreateFunctionType(
                                    instantiateGenericType(functionType->returnType, bindings),
                                    instantiatedParamTypes,
                                    false
                                );
                            }
                        }
                    }

                    instantiatedParamTypes.push_back(instantiateGenericType(functionType->paramTypes.back(), bindings));
                }
                else
                {
                    for (const auto& paramType : functionType->paramTypes)
                        instantiatedParamTypes.push_back(instantiateGenericType(paramType, bindings));
                }

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(functionType->returnType, bindings),
                    instantiatedParamTypes,
                    functionType->hasParameterPack
                );
            }
            case sema::TypeKind::Struct:
            {
                auto structType = current.AsFast<sema::StructType>();
                if (!structType->genericArguments.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedArguments;
                    instantiatedArguments.reserve(structType->genericArguments.size());
                    for (const auto& genericArgument : structType->genericArguments)
                    {
                        auto instantiatedArgument = instantiateGenericType(genericArgument, bindings);
                        auto resolvedArgument = unwrapAliasType(instantiatedArgument);
                        if (structType->hasGenericParameterPack && resolvedArgument &&
                            resolvedArgument->kind() == sema::TypeKind::TypePackView)
                        {
                            const auto packView = resolvedArgument.AsFast<sema::TypePackViewType>();
                            instantiatedArguments.insert(
                                instantiatedArguments.end(),
                                packView->elementTypes.begin(),
                                packView->elementTypes.end()
                            );
                        }
                        else
                        {
                            instantiatedArguments.push_back(instantiatedArgument);
                        }
                    }

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                if (!structType->genericParameterNames.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedArguments;
                    const size_t fixedCount = getMinimumGenericArgumentCount(
                        structType->genericParameterNames,
                        structType->hasGenericParameterPack
                    );
                    instantiatedArguments.reserve(fixedCount + 4);
                    for (size_t i = 0; i < fixedCount; ++i)
                    {
                        if (auto it = bindings.directBindings.find(structType->genericParameterNames[i]); it != bindings.directBindings.end())
                            instantiatedArguments.push_back(it->second);
                        else
                            return current;
                    }

                    if (structType->hasGenericParameterPack)
                    {
                        const std::string& packName = structType->genericParameterNames.back();
                        auto bindingIt = bindings.packBindings.find(packName);
                        if (bindingIt == bindings.packBindings.end())
                            return current;

                        for (const auto& boundType : bindingIt->second)
                            instantiatedArguments.push_back(boundType);
                    }

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                return current;
            }
            default:
                return current;
            }
        }
