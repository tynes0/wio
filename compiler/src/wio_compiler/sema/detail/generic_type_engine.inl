// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        Ref<Type> makeSyntheticPackElementType(const std::string& packName, const size_t index)
        {
            return Compiler::get().getTypeContext().getOrCreateGenericParameterType(makePackElementBindingName(packName, index));
        }

        Ref<Type> makeSyntheticPackElementType(const ParsedPackElementBinding& binding)
        {
            return Compiler::get().getTypeContext().getOrCreateGenericParameterType(
                makePackElementBindingName(binding)
            );
        }

        std::string formatDiagnosticType(const Ref<Type>& type)
        {
            if (!type)
                return "<unresolved>";

            return type->toString();
        }

        std::string formatDiagnosticTypeList(const std::vector<Ref<Type>>& types)
        {
            std::string result = "(";
            for (size_t i = 0; i < types.size(); ++i)
            {
                result += formatDiagnosticType(types[i]);
                if (i + 1 < types.size())
                    result += ", ";
            }
            result += ")";
            return result;
        }

        std::string formatFunctionDiagnosticSignature(std::string_view name,
                                                      const std::vector<std::string>& genericParameterNames,
                                                      const Ref<FunctionType>& functionType,
                                                      bool isConstructor = false,
                                                      bool hasGenericParameterPack = false)
        {
            std::string signature(name);

            if (!genericParameterNames.empty())
            {
                signature += "<";
                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    signature += genericParameterNames[i];
                    if (hasGenericParameterPack && i + 1 == genericParameterNames.size())
                        signature += "...";
                    if (i + 1 < genericParameterNames.size())
                        signature += ", ";
                }
                signature += ">";
            }

            if (!functionType)
                return signature + "(<invalid>)";

            signature += formatDiagnosticTypeList(functionType->paramTypes);
            if (!isConstructor)
                signature += " -> " + formatDiagnosticType(functionType->returnType);

            return signature;
        }

        template <typename T>
        void appendUniqueValue(std::vector<T>& values, const T& value)
        {
            if (std::ranges::find(values, value) == values.end())
                values.push_back(value);
        }

        bool containsGenericParameterType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            case TypeKind::ConstGenericParameter:
            case TypeKind::GenericParameterPack:
                return true;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (packViewType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (packViewType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (storageType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(storageType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::Reference:
                return containsGenericParameterType(current.AsFast<ReferenceType>()->referredType);
            case TypeKind::Nullable:
                return containsGenericParameterType(current.AsFast<NullableType>()->valueType);
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return containsGenericParameterType(arrayType->elementType) ||
                       (arrayType->extentType && containsGenericParameterType(arrayType->extentType));
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterType(dictType->keyType) ||
                       containsGenericParameterType(dictType->valueType);
            }
            case TypeKind::AsyncTask:
                return containsGenericParameterType(current.AsFast<AsyncTaskType>()->valueType);
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsGenericParameterType(funcType->returnType))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [](const Ref<Type>& paramType)
                {
                    return containsGenericParameterType(paramType);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                if (!structType->genericParameterNames.empty() && structType->genericArguments.empty())
                    return true;

                return std::ranges::any_of(structType->genericArguments, [](const Ref<Type>& genericArgument)
                {
                    return containsGenericParameterType(genericArgument);
                });
            }
            case TypeKind::Alias:
                return containsGenericParameterType(current.AsFast<AliasType>()->aliasedType);
            default:
                return false;
            }
        }

        bool containsNamedGenericParameterType(const Ref<Type>& type, const std::vector<std::string>& genericParameterNames)
        {
            if (genericParameterNames.empty())
                return false;

            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end())
                    return true;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                    return std::ranges::find(genericParameterNames, parsedPackElement->packName) != genericParameterNames.end();

                return false;
            }
            case TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<ConstGenericParameterType>();
                return std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end();
            }
            case TypeKind::GenericParameterPack:
            {
                auto genericParam = current.AsFast<GenericParameterPackType>();
                return std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end();
            }
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (std::ranges::find(genericParameterNames, packViewType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (std::ranges::find(genericParameterNames, packViewType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (std::ranges::find(genericParameterNames, storageType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(storageType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
            }
            case TypeKind::Reference:
                return containsNamedGenericParameterType(current.AsFast<ReferenceType>()->referredType, genericParameterNames);
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return containsNamedGenericParameterType(arrayType->elementType, genericParameterNames) ||
                       (arrayType->extentType && containsNamedGenericParameterType(arrayType->extentType, genericParameterNames));
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsNamedGenericParameterType(dictType->keyType, genericParameterNames) ||
                       containsNamedGenericParameterType(dictType->valueType, genericParameterNames);
            }
            case TypeKind::AsyncTask:
                return containsNamedGenericParameterType(
                    current.AsFast<AsyncTaskType>()->valueType, genericParameterNames);
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsNamedGenericParameterType(funcType->returnType, genericParameterNames))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [&](const Ref<Type>& paramType)
                {
                    return containsNamedGenericParameterType(paramType, genericParameterNames);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                return std::ranges::any_of(structType->genericArguments, [&](const Ref<Type>& genericArgument)
                {
                    return containsNamedGenericParameterType(genericArgument, genericParameterNames);
                });
            }
            case TypeKind::Alias:
                return containsNamedGenericParameterType(current.AsFast<AliasType>()->aliasedType, genericParameterNames);
            default:
                return false;
            }
        }

        void collectGenericParameterInstances(const Ref<Type>& type,
                                              const std::vector<std::string>& genericParameterNames,
                                              std::unordered_map<std::string, const Type*>& instances)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end() &&
                    !instances.contains(genericParam->name))
                {
                    instances.emplace(genericParam->name, current.Get());
                }
                else if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name);
                         parsedPackElement &&
                         std::ranges::find(genericParameterNames, parsedPackElement->packName) != genericParameterNames.end() &&
                         !instances.contains(parsedPackElement->packName))
                {
                    instances.emplace(parsedPackElement->packName, current.Get());
                }
                return;
            }
            case TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<ConstGenericParameterType>();
                if (std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end() &&
                    !instances.contains(genericParam->name))
                    instances.emplace(genericParam->name, current.Get());
                return;
            }
            case TypeKind::GenericParameterPack:
                return;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                for (const auto& elementType : packViewType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
                return;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                for (const auto& elementType : packViewType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
                return;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                for (const auto& elementType : storageType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
                return;
            }
            case TypeKind::Reference:
                collectGenericParameterInstances(current.AsFast<ReferenceType>()->referredType, genericParameterNames, instances);
                return;
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                collectGenericParameterInstances(arrayType->elementType, genericParameterNames, instances);
                if (arrayType->extentType)
                    collectGenericParameterInstances(arrayType->extentType, genericParameterNames, instances);
                return;
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                collectGenericParameterInstances(dictType->keyType, genericParameterNames, instances);
                collectGenericParameterInstances(dictType->valueType, genericParameterNames, instances);
                return;
            }
            case TypeKind::AsyncTask:
                collectGenericParameterInstances(
                    current.AsFast<AsyncTaskType>()->valueType, genericParameterNames, instances);
                return;
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                collectGenericParameterInstances(funcType->returnType, genericParameterNames, instances);
                for (const auto& paramType : funcType->paramTypes)
                    collectGenericParameterInstances(paramType, genericParameterNames, instances);
                return;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                    collectGenericParameterInstances(genericArgument, genericParameterNames, instances);
                return;
            }
            case TypeKind::Alias:
                collectGenericParameterInstances(current.AsFast<AliasType>()->aliasedType, genericParameterNames, instances);
                return;
            default:
                return;
            }
        }

        bool containsGenericParameterInstance(const Ref<Type>& type,
                                              const std::unordered_map<std::string, const Type*>& instances)
        {
            if (instances.empty())
                return false;

            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (auto foundInstance = instances.find(genericParam->name); foundInstance != instances.end())
                    return foundInstance->second == current.Get();

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto foundInstance = instances.find(parsedPackElement->packName); foundInstance != instances.end())
                        return foundInstance->second == current.Get();
                }

                return false;
            }
            case TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<ConstGenericParameterType>();
                if (auto foundInstance = instances.find(genericParam->name); foundInstance != instances.end())
                    return foundInstance->second == current.Get();
                return false;
            }
            case TypeKind::GenericParameterPack:
                return true;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                return std::ranges::any_of(storageType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
            }
            case TypeKind::Reference:
                return containsGenericParameterInstance(current.AsFast<ReferenceType>()->referredType, instances);
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return containsGenericParameterInstance(arrayType->elementType, instances) ||
                       (arrayType->extentType && containsGenericParameterInstance(arrayType->extentType, instances));
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterInstance(dictType->keyType, instances) ||
                       containsGenericParameterInstance(dictType->valueType, instances);
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsGenericParameterInstance(funcType->returnType, instances))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [&](const Ref<Type>& paramType)
                {
                    return containsGenericParameterInstance(paramType, instances);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                return std::ranges::any_of(structType->genericArguments, [&](const Ref<Type>& genericArgument)
                {
                    return containsGenericParameterInstance(genericArgument, instances);
                });
            }
            case TypeKind::Alias:
                return containsGenericParameterInstance(current.AsFast<AliasType>()->aliasedType, instances);
            default:
                return false;
            }
        }

        std::string getGenericParameterPackName(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->kind() != TypeKind::GenericParameterPack)
                return {};

            return current.AsFast<GenericParameterPackType>()->name;
        }

        bool containsGenericParameterPackType(const Ref<Type>& type, std::string* outPackName = nullptr)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            switch (current->kind())
            {
            case TypeKind::GenericParameterPack:
                if (outPackName)
                    *outPackName = current.AsFast<GenericParameterPackType>()->name;
                return true;
            case TypeKind::Reference:
                return containsGenericParameterPackType(current.AsFast<ReferenceType>()->referredType, outPackName);
            case TypeKind::Array:
                return containsGenericParameterPackType(current.AsFast<ArrayType>()->elementType, outPackName);
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterPackType(dictType->keyType, outPackName) ||
                       containsGenericParameterPackType(dictType->valueType, outPackName);
            }
            case TypeKind::Function:
            {
                auto functionType = current.AsFast<FunctionType>();
                if (containsGenericParameterPackType(functionType->returnType, outPackName))
                    return true;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (containsGenericParameterPackType(parameterType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                {
                    if (containsGenericParameterPackType(genericArgument, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (packViewType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = packViewType->packName;
                    return true;
                }
                for (const auto& elementType : packViewType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (packViewType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = packViewType->packName;
                    return true;
                }
                for (const auto& elementType : packViewType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (storageType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = storageType->packName;
                    return true;
                }
                for (const auto& elementType : storageType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::Alias:
                return containsGenericParameterPackType(current.AsFast<AliasType>()->aliasedType, outPackName);
            default:
                return false;
            }
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings);
        Ref<Type> instantiateGenericType(const Ref<Type>& type, const GenericBindingSet& bindings);

        bool matchSpecializationPattern(const Ref<Type>& pattern,
                                        const Ref<Type>& actual,
                                        std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            Ref<Type> expected = unwrapAliasType(pattern);
            Ref<Type> candidate = unwrapAliasType(actual);
            if (!expected || !candidate)
                return false;
            if (expected->kind() == TypeKind::GenericParameter)
            {
                const std::string& name = expected.AsFast<GenericParameterType>()->name;
                if (auto found = bindings.find(name); found != bindings.end())
                    return getGenericSpecializationKey({found->second}) == getGenericSpecializationKey({candidate});
                bindings.emplace(name, candidate);
                return true;
            }
            if (expected->kind() == TypeKind::ConstGenericParameter)
            {
                const std::string& name = expected.AsFast<ConstGenericParameterType>()->name;
                if (auto found = bindings.find(name); found != bindings.end())
                    return getGenericSpecializationKey({found->second}) == getGenericSpecializationKey({candidate});
                if (candidate->kind() != TypeKind::ConstValue && candidate->kind() != TypeKind::ConstGenericParameter)
                    return false;
                bindings.emplace(name, candidate);
                return true;
            }
            if (expected->kind() != candidate->kind())
                return false;

            switch (expected->kind())
            {
            case TypeKind::Primitive:
                return expected.AsFast<PrimitiveType>()->name == candidate.AsFast<PrimitiveType>()->name;
            case TypeKind::ConstValue:
                return Type::matchTypes(expected, candidate);
            case TypeKind::Reference:
            {
                auto expectedRef = expected.AsFast<ReferenceType>();
                auto candidateRef = candidate.AsFast<ReferenceType>();
                return expectedRef->isMutable == candidateRef->isMutable &&
                       matchSpecializationPattern(expectedRef->referredType, candidateRef->referredType, bindings);
            }
            case TypeKind::Nullable:
                return matchSpecializationPattern(expected.AsFast<NullableType>()->valueType,
                                                  candidate.AsFast<NullableType>()->valueType,
                                                  bindings);
            case TypeKind::Array:
            {
                auto expectedArray = expected.AsFast<ArrayType>();
                auto candidateArray = candidate.AsFast<ArrayType>();
                return expectedArray->arrayKind == candidateArray->arrayKind &&
                       expectedArray->size == candidateArray->size &&
                       matchSpecializationPattern(expectedArray->elementType, candidateArray->elementType, bindings);
            }
            case TypeKind::Dictionary:
            {
                auto expectedDict = expected.AsFast<DictionaryType>();
                auto candidateDict = candidate.AsFast<DictionaryType>();
                return expectedDict->isOrdered == candidateDict->isOrdered &&
                       matchSpecializationPattern(expectedDict->keyType, candidateDict->keyType, bindings) &&
                       matchSpecializationPattern(expectedDict->valueType, candidateDict->valueType, bindings);
            }
            case TypeKind::Function:
            {
                auto expectedFunction = expected.AsFast<FunctionType>();
                auto candidateFunction = candidate.AsFast<FunctionType>();
                if (expectedFunction->hasParameterPack != candidateFunction->hasParameterPack ||
                    expectedFunction->paramTypes.size() != candidateFunction->paramTypes.size() ||
                    !matchSpecializationPattern(expectedFunction->returnType, candidateFunction->returnType, bindings))
                    return false;
                for (size_t index = 0; index < expectedFunction->paramTypes.size(); ++index)
                {
                    if (!matchSpecializationPattern(expectedFunction->paramTypes[index], candidateFunction->paramTypes[index], bindings))
                        return false;
                }
                return true;
            }
            case TypeKind::Struct:
            {
                auto expectedStruct = expected.AsFast<StructType>();
                auto candidateStruct = candidate.AsFast<StructType>();
                if (expectedStruct->name != candidateStruct->name ||
                    expectedStruct->scopePath != candidateStruct->scopePath ||
                    expectedStruct->genericArguments.size() != candidateStruct->genericArguments.size())
                    return false;
                for (size_t index = 0; index < expectedStruct->genericArguments.size(); ++index)
                {
                    if (!matchSpecializationPattern(expectedStruct->genericArguments[index], candidateStruct->genericArguments[index], bindings))
                        return false;
                }
                return true;
            }
            default:
                return getGenericSpecializationKey({expected}) == getGenericSpecializationKey({candidate});
            }
        }

        bool matchesSpecializationPatternList(const std::vector<Ref<Type>>& patterns,
                                              const std::vector<Ref<Type>>& actuals)
        {
            if (patterns.size() != actuals.size())
                return false;

            std::unordered_map<std::string, Ref<Type>> bindings;
            for (size_t index = 0; index < patterns.size(); ++index)
            {
                if (!matchSpecializationPattern(patterns[index], actuals[index], bindings))
                    return false;
            }
            return true;
        }

        Ref<Type> instantiateGenericStructType(const Ref<StructType>& structType,
                                               const std::vector<Ref<Type>>& explicitTypeArguments,
                                               const common::Location& errorLocation = common::Location::invalid())
        {
            if (!structType)
                return nullptr;

            if (structType->genericParameterNames.empty())
                return structType;

            if (auto specializationIt = structType->explicitSpecializations.find(
                    getGenericSpecializationKey(explicitTypeArguments));
                specializationIt != structType->explicitSpecializations.end())
            {
                if (auto specialization = specializationIt->second.Lock(); specialization)
                    return specialization;
            }

            struct MatchingPartialSpecialization
            {
                Ref<StructType> specialization;
                std::unordered_map<std::string, Ref<Type>> bindings;
            };
            std::vector<MatchingPartialSpecialization> matchingPartialSpecializations;
            for (const auto& weakSpecialization : structType->partialSpecializations)
            {
                auto specialization = weakSpecialization.Lock();
                if (!specialization || specialization->genericArguments.size() != explicitTypeArguments.size())
                    continue;

                std::unordered_map<std::string, Ref<Type>> bindings;
                bool matches = true;
                for (size_t index = 0; index < explicitTypeArguments.size(); ++index)
                {
                    if (!matchSpecializationPattern(specialization->genericArguments[index], explicitTypeArguments[index], bindings))
                    {
                        matches = false;
                        break;
                    }
                }
                if (!matches)
                    continue;
                matchingPartialSpecializations.push_back({specialization, std::move(bindings)});
            }

            Ref<StructType> selectedPartialSpecialization = nullptr;
            std::unordered_map<std::string, Ref<Type>> selectedBindings;
            bool ambiguousPartialSpecialization = false;
            for (size_t candidateIndex = 0;
                 candidateIndex < matchingPartialSpecializations.size();
                 ++candidateIndex)
            {
                const auto& candidate = matchingPartialSpecializations[candidateIndex];
                bool isUniquelyMostSpecialized = true;
                for (size_t otherIndex = 0;
                     otherIndex < matchingPartialSpecializations.size();
                     ++otherIndex)
                {
                    if (candidateIndex == otherIndex)
                        continue;

                    const auto& other = matchingPartialSpecializations[otherIndex];
                    // Candidate is at least as specialized as other when the
                    // other pattern accepts every structural relation encoded
                    // by candidate. The reverse match distinguishes a strict
                    // ordering from equivalent/renamed patterns.
                    const bool otherAcceptsCandidate = matchesSpecializationPatternList(
                        other.specialization->genericArguments,
                        candidate.specialization->genericArguments);
                    const bool candidateAcceptsOther = matchesSpecializationPatternList(
                        candidate.specialization->genericArguments,
                        other.specialization->genericArguments);
                    if (!otherAcceptsCandidate || candidateAcceptsOther)
                    {
                        isUniquelyMostSpecialized = false;
                        break;
                    }
                }

                if (!isUniquelyMostSpecialized && matchingPartialSpecializations.size() > 1)
                    continue;
                if (selectedPartialSpecialization)
                {
                    ambiguousPartialSpecialization = true;
                    break;
                }
                selectedPartialSpecialization = candidate.specialization;
                selectedBindings = candidate.bindings;
            }

            if (!matchingPartialSpecializations.empty() && !selectedPartialSpecialization)
                ambiguousPartialSpecialization = true;

            if (ambiguousPartialSpecialization)
            {
                WIO_LOG_ADD_ERROR(errorLocation,
                    "Ambiguous partial specialization for '{}{}'.",
                    structType->name,
                    formatDiagnosticTypeList(explicitTypeArguments));
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (selectedPartialSpecialization)
            {
                auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                    selectedPartialSpecialization->name,
                    selectedPartialSpecialization->structScope.Lock(),
                    selectedPartialSpecialization->isObject,
                    selectedPartialSpecialization->isInterface
                ).AsFast<StructType>();
                instantiatedType->scopePath = selectedPartialSpecialization->scopePath;
                instantiatedType->genericParameterNames = selectedPartialSpecialization->genericParameterNames;
                instantiatedType->genericParameterTypes = selectedPartialSpecialization->genericParameterTypes;
                instantiatedType->genericParameterDefaults = selectedPartialSpecialization->genericParameterDefaults;
                instantiatedType->genericArguments = explicitTypeArguments;
                instantiatedType->genericPrimaryType = structType;
                instantiatedType->isExplicitSpecialization = true;
                instantiatedType->isPartialSpecialization = true;
                instantiatedType->fieldNames = selectedPartialSpecialization->fieldNames;
                instantiatedType->trustedTypeKeys = selectedPartialSpecialization->trustedTypeKeys;
                instantiatedType->isFinal = selectedPartialSpecialization->isFinal;
                instantiatedType->isNativePodComponent = selectedPartialSpecialization->isNativePodComponent;
                instantiatedType->nativeCppName = selectedPartialSpecialization->nativeCppName;
                instantiatedType->nativeCppHeader = selectedPartialSpecialization->nativeCppHeader;
                for (const auto& fieldType : selectedPartialSpecialization->fieldTypes)
                    instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, selectedBindings));
                for (const auto& baseType : selectedPartialSpecialization->baseTypes)
                    instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, selectedBindings));
                return instantiatedType;
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
            ).AsFast<StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericParameterTypes = structType->genericParameterTypes;
            instantiatedType->genericParameterDefaults = structType->genericParameterDefaults;
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

            validateInstantiatedNativePodComponent(instantiatedType, errorLocation);

            return instantiatedType;
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            GenericBindingSet bindingSet;
            bindingSet.directBindings = bindings;
            return instantiateGenericType(type, bindingSet);
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const GenericBindingSet& bindings)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            auto& ctx = Compiler::get().getTypeContext();

            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto aliasIt = bindings.packAliases.find(parsedPackElement->packName); aliasIt != bindings.packAliases.end())
                    {
                        ParsedPackElementBinding reboundBinding = *parsedPackElement;
                        reboundBinding.packName = aliasIt->second;
                        return makeSyntheticPackElementType(reboundBinding);
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
            case TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<ConstGenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;
                return current;
            }
            case TypeKind::ConstValue:
                return current;
            case TypeKind::GenericParameterPack:
            {
                auto genericPack = current.AsFast<GenericParameterPackType>();
                if (auto aliasIt = bindings.packAliases.find(genericPack->name); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(genericPack->name); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(genericPack->name, it->second);
                return current;
            }
            case TypeKind::ValuePackView:
            {
                auto viewType = current.AsFast<ValuePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::TypePackView:
            {
                auto viewType = current.AsFast<TypePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (!storageType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::Reference:
            {
                auto refType = current.AsFast<ReferenceType>();
                return ctx.getOrCreateReferenceType(
                    instantiateGenericType(refType->referredType, bindings),
                    refType->isMutable
                );
            }
            case TypeKind::Nullable:
                return ctx.getOrCreateNullableType(
                    instantiateGenericType(current.AsFast<NullableType>()->valueType, bindings)
                );
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                Ref<Type> instantiatedExtent = arrayType->extentType
                    ? instantiateGenericType(arrayType->extentType, bindings)
                    : nullptr;
                size_t concreteSize = arrayType->size;
                if (instantiatedExtent && instantiatedExtent->kind() == TypeKind::ConstValue)
                {
                    const std::string rawValue = common::stripIntegerLiteralTypeSuffix(
                        instantiatedExtent.AsFast<ConstValueType>()->value);
                    concreteSize = static_cast<size_t>(std::stoull(rawValue));
                }
                return ctx.getOrCreateArrayType(
                    instantiateGenericType(arrayType->elementType, bindings),
                    arrayType->arrayKind,
                    concreteSize,
                    instantiatedExtent
                );
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return ctx.getOrCreateDictionaryType(
                    instantiateGenericType(dictType->keyType, bindings),
                    instantiateGenericType(dictType->valueType, bindings),
                    dictType->isOrdered
                );
            }
            case TypeKind::AsyncTask:
            {
                auto taskType = current.AsFast<AsyncTaskType>();
                return ctx.getOrCreateAsyncTaskType(
                    instantiateGenericType(taskType->valueType, bindings)
                );
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                std::vector<Ref<Type>> instantiatedParams;
                instantiatedParams.reserve(funcType->paramTypes.size());
                bool hasParameterPack = funcType->hasParameterPack;

                if (funcType->hasParameterPack &&
                    !funcType->paramTypes.empty() &&
                    unwrapAliasType(funcType->paramTypes.back()) &&
                    unwrapAliasType(funcType->paramTypes.back())->kind() == TypeKind::GenericParameterPack)
                {
                    const auto packType = unwrapAliasType(funcType->paramTypes.back()).AsFast<GenericParameterPackType>();
                    for (size_t i = 0; i + 1 < funcType->paramTypes.size(); ++i)
                        instantiatedParams.push_back(instantiateGenericType(funcType->paramTypes[i], bindings));

                    if (auto aliasIt = bindings.packAliases.find(packType->name); aliasIt != bindings.packAliases.end())
                    {
                        instantiatedParams.push_back(
                            Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(aliasIt->second)
                        );
                    }
                    else if (auto it = bindings.packBindings.find(packType->name); it != bindings.packBindings.end())
                    {
                        if (it->second.empty())
                        {
                            hasParameterPack = false;
                        }
                        else
                        {
                            for (const auto& boundType : it->second)
                                instantiatedParams.push_back(boundType);
                            hasParameterPack = false;
                        }
                    }
                    else
                    {
                        instantiatedParams.push_back(funcType->paramTypes.back());
                    }
                }
                else
                {
                    for (const auto& paramType : funcType->paramTypes)
                        instantiatedParams.push_back(instantiateGenericType(paramType, bindings));
                }

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(funcType->returnType, bindings),
                    instantiatedParams,
                    hasParameterPack
                );
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                std::vector<Ref<Type>> instantiatedArguments;

                if (!structType->genericArguments.empty())
                {
                    instantiatedArguments.reserve(structType->genericArguments.size());
                    for (const auto& genericArgument : structType->genericArguments)
                    {
                        auto instantiatedArgument = instantiateGenericType(genericArgument, bindings);
                        auto resolvedArgument = unwrapAliasType(instantiatedArgument);
                        if (structType->hasGenericParameterPack && resolvedArgument &&
                            resolvedArgument->kind() == TypeKind::TypePackView)
                        {
                            const auto packView = resolvedArgument.AsFast<TypePackViewType>();
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
                    const size_t fixedCount = getMinimumGenericArgumentCount(structType->genericParameterNames, structType->hasGenericParameterPack);
                    instantiatedArguments.reserve(fixedCount);
                    for (size_t i = 0; i < fixedCount; ++i)
                    {
                        auto bindingIt = bindings.directBindings.find(structType->genericParameterNames[i]);
                        if (bindingIt == bindings.directBindings.end())
                            return current;

                        instantiatedArguments.push_back(bindingIt->second);
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
            case TypeKind::Alias:
                return instantiateGenericType(current.AsFast<AliasType>()->aliasedType, bindings);
            default:
                return current;
            }
        }

        bool isConstGenericIntegerType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = current.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "isize" || name == "u8" || name == "u16" || name == "u32" ||
                   name == "u64" || name == "usize";
        }

        bool isConstGenericValueType(const Ref<Type>& type)
        {
            if (isConstGenericIntegerType(type))
                return true;

            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = current.AsFast<PrimitiveType>()->name;
            return name == "string" || name == "text";
        }

        bool areConstGenericValueTypesCompatible(const Ref<Type>& declared, const Ref<Type>& actual)
        {
            Ref<Type> resolvedDeclared = unwrapAliasType(declared);
            Ref<Type> resolvedActual = unwrapAliasType(actual);
            if (!resolvedDeclared || !resolvedActual)
                return false;

            auto textualName = [](const Ref<Type>& type) -> std::string_view
            {
                if (!type || type->kind() != TypeKind::Primitive)
                    return {};
                const std::string& name = type.AsFast<PrimitiveType>()->name;
                return name == "string" || name == "text" ? std::string_view(name) : std::string_view{};
            };
            const std::string_view declaredText = textualName(resolvedDeclared);
            const std::string_view actualText = textualName(resolvedActual);
            if (!declaredText.empty() || !actualText.empty())
                return declaredText == actualText && !declaredText.empty();

            return resolvedDeclared->isCompatibleWith(resolvedActual);
        }

        bool isTextualConstGenericParameterType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::ConstGenericParameter)
                return false;
            Ref<Type> valueType = unwrapAliasType(
                resolved.AsFast<ConstGenericParameterType>()->valueType);
            if (!valueType || valueType->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = valueType.AsFast<PrimitiveType>()->name;
            return name == "string" || name == "text";
        }

        Ref<Type> createGenericParameterSemanticType(SemanticAnalyzer& analyzer,
                                                     const NodePtr<Identifier>& parameter,
                                                     const bool isPack,
                                                     std::string_view declarationKind)
        {
            if (!parameter)
                return Compiler::get().getTypeContext().getUnknown();

            if (parameter->genericParameterTypeValidated)
            {
                if (Ref<Type> previous = parameter->refType.Lock())
                    return previous;
            }
            parameter->genericParameterTypeValidated = true;

            const std::string& name = parameter->token.value;
            if (!parameter->isConstGenericParameter)
            {
                return isPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(name)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(name);
            }

            if (isPack)
            {
                WIO_LOG_ADD_ERROR(parameter->location(), "Const generic parameter packs are not supported.");
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (!parameter->genericValueType)
            {
                WIO_LOG_ADD_ERROR(parameter->location(), "Const generic parameter '{}' on {} requires an integer, string, or text value type.", name, declarationKind);
                return Compiler::get().getTypeContext().getUnknown();
            }

            parameter->genericValueType->accept(analyzer);
            Ref<Type> valueType = parameter->genericValueType->refType.Lock();
            if (!isConstGenericValueType(valueType))
            {
                WIO_LOG_ADD_ERROR(
                    parameter->genericValueType->location(),
                    "Const generic parameter '{}' on {} must use an integer, string, or text type, but got '{}'.",
                    name, declarationKind, valueType ? valueType->toString() : "<unknown>"
                );
                return Compiler::get().getTypeContext().getUnknown();
            }

            return Compiler::get().getTypeContext().getOrCreateConstGenericParameterType(name, valueType);
        }

        std::vector<Ref<Type>> collectGenericParameterSemanticTypes(const std::vector<NodePtr<Identifier>>& parameters)
        {
            std::vector<Ref<Type>> result;
            result.reserve(parameters.size());
            for (const auto& parameter : parameters)
                result.push_back(parameter ? parameter->refType.Lock() : nullptr);
            return result;
        }

        bool validateGenericArgumentKinds(const std::vector<std::string>& parameterNames,
                                          const std::vector<Ref<Type>>& parameterTypes,
                                          const std::vector<Ref<Type>>& arguments,
                                          const common::Location& location)
        {
            bool valid = true;
            const size_t count = std::min(parameterTypes.size(), arguments.size());
            for (size_t index = 0; index < count; ++index)
            {
                const Ref<Type>& parameterType = parameterTypes[index];
                const Ref<Type>& argument = arguments[index];
                if (!parameterType || !argument)
                    continue;

                const bool expectsValue = parameterType->kind() == TypeKind::ConstGenericParameter;
                const bool receivesValue = argument->kind() == TypeKind::ConstValue ||
                                           argument->kind() == TypeKind::ConstGenericParameter;
                if (expectsValue != receivesValue)
                {
                    WIO_LOG_ADD_ERROR(
                        location,
                        expectsValue
                            ? "Generic parameter '{}' expects a compile-time value, but got type '{}'."
                            : "Generic parameter '{}' expects a type, but got compile-time value '{}'.",
                        index < parameterNames.size() ? parameterNames[index] : "<unknown>",
                        argument->toString()
                    );
                    valid = false;
                    continue;
                }

                if (expectsValue)
                {
                    auto declared = parameterType.AsFast<ConstGenericParameterType>();
                    Ref<Type> actualValueType = argument->kind() == TypeKind::ConstValue
                        ? argument.AsFast<ConstValueType>()->valueType
                        : argument.AsFast<ConstGenericParameterType>()->valueType;
                    if (!areConstGenericValueTypesCompatible(declared->valueType, actualValueType))
                    {
                        WIO_LOG_ADD_ERROR(
                            location,
                            "Const generic argument '{}' is not compatible with parameter '{}: {}'.",
                            argument->toString(),
                            index < parameterNames.size() ? parameterNames[index] : "<unknown>",
                            declared->valueType->toString()
                        );
                        valid = false;
                    }
                }
            }
            return valid;
        }

        std::vector<Ref<Type>> resolveGenericParameterDefaults(
            SemanticAnalyzer& analyzer,
            const std::vector<NodePtr<Identifier>>& parameters,
            const bool hasGenericParameterPack,
            const std::string_view declarationKind)
        {
            std::vector<Ref<Type>> defaults(parameters.size());
            std::vector<std::string> parameterNames;
            parameterNames.reserve(parameters.size());
            for (const auto& parameter : parameters)
                parameterNames.push_back(parameter ? parameter->token.value : std::string{});

            for (size_t index = 0; index < parameters.size(); ++index)
            {
                const auto& parameter = parameters[index];
                if (!parameter || !parameter->genericDefaultType)
                    continue;

                if (hasGenericParameterPack && index + 1 == parameters.size())
                {
                    WIO_LOG_ADD_ERROR(parameter->location(), "Generic parameter packs cannot have default arguments.");
                    continue;
                }

                parameter->genericDefaultType->accept(analyzer);
                Ref<Type> defaultType = parameter->genericDefaultType->refType.Lock();
                defaults[index] = defaultType;

                if (parameter->isConstGenericParameter)
                {
                    Ref<Type> declaredParameterType = parameter->refType.Lock();
                    auto constParameter = declaredParameterType && declaredParameterType->kind() == TypeKind::ConstGenericParameter
                        ? declaredParameterType.AsFast<ConstGenericParameterType>()
                        : nullptr;
                    Ref<Type> defaultValueType = defaultType && defaultType->kind() == TypeKind::ConstValue
                        ? defaultType.AsFast<ConstValueType>()->valueType
                        : defaultType && defaultType->kind() == TypeKind::ConstGenericParameter
                            ? defaultType.AsFast<ConstGenericParameterType>()->valueType
                            : nullptr;
                    if (!constParameter || !defaultValueType ||
                        !areConstGenericValueTypesCompatible(constParameter->valueType, defaultValueType))
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter->genericDefaultType->location(),
                            "Default for const generic parameter '{}' must be a compile-time value compatible with '{}'.",
                            parameter->token.value,
                            constParameter && constParameter->valueType ? constParameter->valueType->toString() : "<unknown>"
                        );
                    }
                }
                else if (defaultType && (defaultType->kind() == TypeKind::ConstValue ||
                                         defaultType->kind() == TypeKind::ConstGenericParameter))
                {
                    WIO_LOG_ADD_ERROR(
                        parameter->genericDefaultType->location(),
                        "Default for type parameter '{}' must be a type, not a const value.",
                        parameter->token.value
                    );
                }

                std::unordered_map<std::string, const Type*> referencedParameters;
                collectGenericParameterInstances(defaultType, parameterNames, referencedParameters);
                for (size_t referencedIndex = index; referencedIndex < parameterNames.size(); ++referencedIndex)
                {
                    if (!parameterNames[referencedIndex].empty() &&
                        referencedParameters.contains(parameterNames[referencedIndex]))
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter->genericDefaultType->location(),
                            "Default for generic parameter '{}' on {} cannot reference '{}' because defaults may only use earlier parameters.",
                            parameter->token.value,
                            declarationKind,
                            parameterNames[referencedIndex]
                        );
                    }
                }
            }

            return defaults;
        }

        size_t getRequiredGenericArgumentCount(const std::vector<Ref<Type>>& defaults,
                                               const size_t fixedParameterCount)
        {
            size_t requiredCount = fixedParameterCount;
            while (requiredCount > 0 &&
                   requiredCount - 1 < defaults.size() &&
                   defaults[requiredCount - 1])
            {
                --requiredCount;
            }
            return requiredCount;
        }

        std::optional<std::vector<Ref<Type>>> completeGenericTypeArguments(
            const std::vector<std::string>& parameterNames,
            const std::vector<Ref<Type>>& defaults,
            const bool hasGenericParameterPack,
            const std::vector<Ref<Type>>& providedArguments)
        {
            const size_t fixedCount = getMinimumGenericArgumentCount(parameterNames, hasGenericParameterPack);
            const size_t requiredCount = getRequiredGenericArgumentCount(defaults, fixedCount);
            if (providedArguments.size() < requiredCount ||
                (!hasGenericParameterPack && providedArguments.size() > fixedCount))
            {
                return std::nullopt;
            }

            std::vector<Ref<Type>> completed = providedArguments;
            const size_t providedFixedCount = std::min(providedArguments.size(), fixedCount);
            GenericBindingSet bindings;
            for (size_t index = 0; index < providedFixedCount; ++index)
                bindings.directBindings[parameterNames[index]] = providedArguments[index];

            for (size_t index = providedFixedCount; index < fixedCount; ++index)
            {
                if (index >= defaults.size() || !defaults[index])
                    return std::nullopt;

                Ref<Type> instantiatedDefault = instantiateGenericType(defaults[index], bindings);
                completed.insert(completed.begin() + static_cast<std::ptrdiff_t>(index), instantiatedDefault);
                bindings.directBindings[parameterNames[index]] = instantiatedDefault;
            }

            return completed;
        }

        bool deduceGenericBindings(const Ref<Type>& expected,
                                   const Ref<Type>& actual,
                                   std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            Ref<Type> resolvedExpected = unwrapAliasType(expected);
            Ref<Type> resolvedActual = unwrapAliasType(actual);

            auto findMatchingBaseTypeInstance = [&](auto&& self, const Ref<Type>& candidateType, const Ref<Type>& targetType) -> Ref<Type>
            {
                Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                Ref<Type> resolvedTarget = unwrapAliasType(targetType);

                if (!resolvedCandidate || !resolvedTarget ||
                    resolvedCandidate->kind() != TypeKind::Struct ||
                    resolvedTarget->kind() != TypeKind::Struct)
                {
                    return nullptr;
                }

                auto candidateStruct = resolvedCandidate.AsFast<StructType>();
                auto targetStruct = resolvedTarget.AsFast<StructType>();

                for (const auto& baseType : candidateStruct->baseTypes)
                {
                    Ref<Type> resolvedBaseType = unwrapAliasType(baseType);
                    if (!resolvedBaseType || resolvedBaseType->kind() != TypeKind::Struct)
                        continue;

                    auto baseStruct = resolvedBaseType.AsFast<StructType>();
                    if (baseStruct->name == targetStruct->name &&
                        baseStruct->scopePath == targetStruct->scopePath)
                    {
                        return resolvedBaseType;
                    }

                    if (auto nestedMatch = self(self, resolvedBaseType, resolvedTarget))
                        return nestedMatch;
                }

                return nullptr;
            };

            if (!resolvedExpected || !resolvedActual || resolvedActual->isUnknown())
                return false;

            if (resolvedExpected->kind() == TypeKind::GenericParameter)
            {
                if (resolvedActual->kind() == TypeKind::Null)
                    return false;

                auto genericParam = resolvedExpected.AsFast<GenericParameterType>();
                if (auto it = bindings.find(genericParam->name); it != bindings.end())
                    return it->second->isCompatibleWith(resolvedActual) &&
                           resolvedActual->isCompatibleWith(it->second);

                bindings.emplace(genericParam->name, resolvedActual);
                return true;
            }

            if (resolvedExpected->kind() == TypeKind::ConstGenericParameter)
            {
                if (resolvedActual->kind() != TypeKind::ConstValue &&
                    resolvedActual->kind() != TypeKind::ConstGenericParameter)
                    return false;

                auto genericParam = resolvedExpected.AsFast<ConstGenericParameterType>();
                if (auto it = bindings.find(genericParam->name); it != bindings.end())
                    return it->second->isCompatibleWith(resolvedActual) &&
                           resolvedActual->isCompatibleWith(it->second);

                bindings.emplace(genericParam->name, resolvedActual);
                return true;
            }

            if (resolvedExpected->kind() == TypeKind::GenericParameterPack)
                return resolvedActual->kind() == TypeKind::GenericParameterPack &&
                       resolvedExpected.AsFast<GenericParameterPackType>()->name == resolvedActual.AsFast<GenericParameterPackType>()->name;

            if (resolvedExpected->kind() == TypeKind::Reference &&
                resolvedActual->kind() == TypeKind::Reference)
            {
                auto expectedRef = resolvedExpected.AsFast<ReferenceType>();
                auto actualRef = resolvedActual.AsFast<ReferenceType>();

                if (expectedRef->isMutable && !actualRef->isMutable)
                    return false;

                if (containsGenericParameterType(expectedRef->referredType))
                    return deduceGenericBindings(expectedRef->referredType, actualRef->referredType, bindings);

                return expectedRef->referredType->isCompatibleWith(actualRef->referredType) ||
                       isTypeDerivedFrom(actualRef->referredType, expectedRef->referredType);
            }

            if (resolvedExpected->kind() == TypeKind::Reference &&
                resolvedActual->kind() == TypeKind::Struct)
            {
                auto expectedRef = resolvedExpected.AsFast<ReferenceType>();
                auto expectedTarget = unwrapAliasType(expectedRef->referredType);

                if (!expectedRef->isMutable &&
                    expectedTarget && expectedTarget->kind() == TypeKind::Struct)
                {
                    auto expectedStruct = expectedTarget.AsFast<StructType>();
                    auto actualStruct = resolvedActual.AsFast<StructType>();

                    if ((expectedStruct->isObject || expectedStruct->isInterface) &&
                        (actualStruct->isObject || actualStruct->isInterface))
                    {
                        if (auto matchingBaseType = findMatchingBaseTypeInstance(findMatchingBaseTypeInstance, resolvedActual, expectedTarget))
                        {
                            if (containsGenericParameterType(expectedTarget))
                                return deduceGenericBindings(expectedTarget, matchingBaseType, bindings);

                            return expectedTarget->isCompatibleWith(matchingBaseType) ||
                                   isTypeDerivedFrom(matchingBaseType, expectedTarget);
                        }

                        if (containsGenericParameterType(expectedTarget))
                            return deduceGenericBindings(expectedTarget, resolvedActual, bindings);

                        return expectedTarget->isCompatibleWith(resolvedActual) ||
                               isTypeDerivedFrom(resolvedActual, expectedTarget);
                    }
                }
            }

            if (resolvedExpected->kind() == TypeKind::Array &&
                resolvedActual->kind() == TypeKind::Array)
            {
                auto expectedArray = resolvedExpected.AsFast<ArrayType>();
                auto actualArray = resolvedActual.AsFast<ArrayType>();

                if (expectedArray->arrayKind == ArrayType::ArrayKind::Dynamic)
                    return deduceGenericBindings(expectedArray->elementType, actualArray->elementType, bindings);

                if (actualArray->arrayKind == ArrayType::ArrayKind::Dynamic)
                    return false;

                Ref<Type> expectedExtent = expectedArray->extentType;
                Ref<Type> actualExtent = actualArray->extentType;
                if (!actualExtent)
                {
                    actualExtent = Compiler::get().getTypeContext().getOrCreateConstValueType(
                        std::to_string(actualArray->size), Compiler::get().getTypeContext().getUSize());
                }

                if (expectedExtent)
                {
                    if (!deduceGenericBindings(expectedExtent, actualExtent, bindings))
                        return false;
                }
                else if (actualArray->size > expectedArray->size)
                    return false;

                return deduceGenericBindings(expectedArray->elementType, actualArray->elementType, bindings);
            }

            if (resolvedExpected->kind() == TypeKind::Dictionary &&
                resolvedActual->kind() == TypeKind::Dictionary)
            {
                auto expectedDict = resolvedExpected.AsFast<DictionaryType>();
                auto actualDict = resolvedActual.AsFast<DictionaryType>();

                if (expectedDict->isOrdered != actualDict->isOrdered)
                    return false;

                return deduceGenericBindings(expectedDict->keyType, actualDict->keyType, bindings) &&
                       deduceGenericBindings(expectedDict->valueType, actualDict->valueType, bindings);
            }

            if (resolvedExpected->kind() == TypeKind::AsyncTask &&
                resolvedActual->kind() == TypeKind::AsyncTask)
            {
                return deduceGenericBindings(
                    resolvedExpected.AsFast<AsyncTaskType>()->valueType,
                    resolvedActual.AsFast<AsyncTaskType>()->valueType,
                    bindings
                );
            }

            if (resolvedExpected->kind() == TypeKind::Function &&
                resolvedActual->kind() == TypeKind::Function)
            {
                auto expectedFunc = resolvedExpected.AsFast<FunctionType>();
                auto actualFunc = resolvedActual.AsFast<FunctionType>();

                if (expectedFunc->paramTypes.size() != actualFunc->paramTypes.size())
                    return false;

                if (!deduceGenericBindings(expectedFunc->returnType, actualFunc->returnType, bindings))
                    return false;

                for (size_t i = 0; i < expectedFunc->paramTypes.size(); ++i)
                {
                    if (!deduceGenericBindings(expectedFunc->paramTypes[i], actualFunc->paramTypes[i], bindings))
                        return false;
                }

                return true;
            }

            if (resolvedExpected->kind() == TypeKind::Struct &&
                resolvedActual->kind() == TypeKind::Struct)
            {
                auto expectedStruct = resolvedExpected.AsFast<StructType>();
                auto actualStruct = resolvedActual.AsFast<StructType>();

                if (expectedStruct->name != actualStruct->name ||
                    expectedStruct->scopePath != actualStruct->scopePath ||
                    expectedStruct->genericArguments.size() != actualStruct->genericArguments.size())
                {
                    return false;
                }

                for (size_t i = 0; i < expectedStruct->genericArguments.size(); ++i)
                {
                    if (!deduceGenericBindings(expectedStruct->genericArguments[i], actualStruct->genericArguments[i], bindings))
                        return false;
                }

                return true;
            }

            return resolvedExpected->isCompatibleWith(resolvedActual) ||
                   (resolvedExpected->isNumeric() && resolvedActual->isNumeric());
        }
