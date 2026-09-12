// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        std::vector<std::string> getBaseInterfaces(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<std::string> bases;
            for (const auto& attr : attributes)
                if (attr && matchesBuiltinAttribute(*attr, Attribute::From))
                    for (const auto& arg : attr->args)
                        if (arg.type == TokenType::identifier)
                            bases.push_back(arg.value);
            return bases;
        }

        bool isNativeFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Native);
        }

        bool isExportedFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        bool isCommandFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Command);
        }

        bool isEventFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Event);
        }

        bool isExportedComponent(const ComponentDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        bool isExportedObject(const ObjectDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        std::optional<Attribute> getModuleLifecycleAttribute(const FunctionDeclaration& node)
        {
            for (const auto& attr : node.attributes)
            {
                if (!attr)
                    continue;

                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (attr->attribute)
                {
                case Attribute::ModuleApiVersion:
                case Attribute::ModuleLoad:
                case Attribute::ModuleUpdate:
                case Attribute::ModuleUnload:
                case Attribute::ModuleSaveState:
                case Attribute::ModuleRestoreState:
                    return attr->attribute;
                default:
                    break;
                }
            }

            return std::nullopt;
        }

        std::string getNativeCppSymbolName(const FunctionDeclaration& node)
        {
            if (auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName); cppNameArg.has_value())
                return cppNameArg->value;

            if (node.isExtensionMethod && !node.extensionMemberName.empty())
                return node.extensionMemberName;

            return node.name ? node.name->token.value : "";
        }

        std::string getExportedCppSymbolName(const FunctionDeclaration& node)
        {
            if (std::optional<Attribute> lifecycleAttribute = getModuleLifecycleAttribute(node); lifecycleAttribute.has_value())
            {
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (*lifecycleAttribute)
                {
                case Attribute::ModuleApiVersion: return "WioModuleApiVersion";
                case Attribute::ModuleLoad: return "WioModuleLoad";
                case Attribute::ModuleUpdate: return "WioModuleUpdate";
                case Attribute::ModuleUnload: return "WioModuleUnload";
                case Attribute::ModuleSaveState: return "WioModuleSaveState";
                case Attribute::ModuleRestoreState: return "WioModuleRestoreState";
                default: break;
                }
            }

            if (auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName); cppNameArg.has_value())
                return cppNameArg->value;

            return node.name ? node.name->token.value : "";
        }

        struct ModuleLifecycleFunctions
        {
            const FunctionDeclaration* apiVersion = nullptr;
            const FunctionDeclaration* load = nullptr;
            const FunctionDeclaration* update = nullptr;
            const FunctionDeclaration* unload = nullptr;
            const FunctionDeclaration* saveState = nullptr;
            const FunctionDeclaration* restoreState = nullptr;

            bool hasAny() const
            {
                return apiVersion || load || update || unload || saveState || restoreState;
            }
        };

        void setLifecycleFunctionIfEmpty(const FunctionDeclaration*& slot, const FunctionDeclaration* declaration)
        {
            if (slot == nullptr)
                slot = declaration;
        }

        struct ExportedFunctionInfo
        {
            enum class FieldAccessorKind : uint8_t
            {
                Value,
                ObjectHandle,
                ComponentHandle
            };

            const FunctionDeclaration* declaration = nullptr;
            Ref<sema::FunctionType> functionType = nullptr;
            std::string logicalName;
            std::string symbolName;
            std::string internalSymbol;
            std::vector<Ref<sema::Type>> templateArguments;
            std::optional<std::string> commandName;
            std::optional<std::string> eventName;
            enum class SyntheticKind : uint8_t
            {
                None,
                TypeConstruct,
                TypeDestroy,
                TypeFieldGet,
                TypeFieldSet,
                TypeMethod
            } syntheticKind = SyntheticKind::None;
            std::string ownerCppTypeName;
            std::string memberCppName;
            std::string memberCppTypeName;
            bool ownerIsObject = false;
            FieldAccessorKind fieldAccessorKind = FieldAccessorKind::Value;
            bool valueRequiresBridgeCast = false;
        };

        struct ExportedFieldInfo
        {
            const VariableDeclaration* declaration = nullptr;
            std::string fieldName;
            Ref<sema::Type> fieldType = nullptr;
            bool isReadOnly = false;
            AccessModifier accessModifier = AccessModifier::Public;
            std::string memberCppName;
            std::string memberCppTypeName;
            ExportedFunctionInfo::FieldAccessorKind accessorKind = ExportedFunctionInfo::FieldAccessorKind::Value;
            std::optional<std::string> dynamicGetterSymbol;
            std::optional<std::string> dynamicSetterSymbol;
            size_t getterExportIndex = 0;
            std::optional<size_t> setterExportIndex;
        };

        struct ExportedMethodInfo
        {
            const FunctionDeclaration* declaration = nullptr;
            std::string methodName;
            size_t exportIndex = 0;
        };

        struct ExportedConstructorInfo
        {
            size_t exportIndex = 0;
        };

        struct ExportedTypeInfo
        {
            const std::vector<NodePtr<AttributeStatement>>* attributes = nullptr;
            std::string logicalName;
            std::string symbolName;
            std::string cppTypeName;
            bool isObject = false;
            std::optional<size_t> createExportIndex;
            size_t destroyExportIndex = 0;
            std::vector<ExportedConstructorInfo> constructors;
            std::vector<ExportedFieldInfo> fields;
            std::vector<ExportedMethodInfo> methods;
        };

        Ref<sema::StructType> getStructTypeFromSymbol(const Ref<sema::Symbol>& symbol);
        std::string mangleStructTypeName(const Ref<sema::StructType>& type);

        std::string getAbiTypeEnumName(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return "WIO_ABI_UNKNOWN";

            if (resolvedType->kind() == sema::TypeKind::Nullable)
                return getAbiTypeEnumName(resolvedType.AsFast<sema::NullableType>()->valueType);

            if (resolvedType->isVoid())
                return "WIO_ABI_VOID";

            if (resolvedType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                if (structType && (structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                    return getAbiTypeEnumName(structType->enumUnderlyingType);
            }

            if (resolvedType->kind() != sema::TypeKind::Primitive)
                return "WIO_ABI_UNKNOWN";

            const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
            if (primitiveName == "bool") return "WIO_ABI_BOOL";
            if (primitiveName == "char") return "WIO_ABI_CHAR";
            if (primitiveName == "uchar") return "WIO_ABI_UCHAR";
            if (primitiveName == "byte") return "WIO_ABI_BYTE";
            if (primitiveName == "i8") return "WIO_ABI_I8";
            if (primitiveName == "i16") return "WIO_ABI_I16";
            if (primitiveName == "i32") return "WIO_ABI_I32";
            if (primitiveName == "i64") return "WIO_ABI_I64";
            if (primitiveName == "u8") return "WIO_ABI_U8";
            if (primitiveName == "u16") return "WIO_ABI_U16";
            if (primitiveName == "u32") return "WIO_ABI_U32";
            if (primitiveName == "u64") return "WIO_ABI_U64";
            if (primitiveName == "isize") return "WIO_ABI_ISIZE";
            if (primitiveName == "usize") return "WIO_ABI_USIZE";
            if (primitiveName == "f32") return "WIO_ABI_F32";
            if (primitiveName == "f64") return "WIO_ABI_F64";
            return "WIO_ABI_UNKNOWN";
        }

        std::string getAbiValueFieldName(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return {};

            if (resolvedType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                if (structType && (structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                    return getAbiValueFieldName(structType->enumUnderlyingType);
            }

            if (resolvedType->kind() != sema::TypeKind::Primitive)
                return {};

            const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
            if (primitiveName == "bool") return "v_bool";
            if (primitiveName == "char") return "v_char";
            if (primitiveName == "uchar") return "v_uchar";
            if (primitiveName == "byte") return "v_byte";
            if (primitiveName == "i8") return "v_i8";
            if (primitiveName == "i16") return "v_i16";
            if (primitiveName == "i32") return "v_i32";
            if (primitiveName == "i64") return "v_i64";
            if (primitiveName == "u8") return "v_u8";
            if (primitiveName == "u16") return "v_u16";
            if (primitiveName == "u32") return "v_u32";
            if (primitiveName == "u64") return "v_u64";
            if (primitiveName == "isize") return "v_isize";
            if (primitiveName == "usize") return "v_usize";
            if (primitiveName == "f32") return "v_f32";
            if (primitiveName == "f64") return "v_f64";
            return {};
        }

        void collectModuleLifecycleFunctions(const std::vector<NodePtr<Statement>>& statements, ModuleLifecycleFunctions& lifecycleFunctions)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectModuleLifecycleFunctions(realmDecl->statements, lifecycleFunctions);
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectModuleLifecycleFunctions(group->declarations, lifecycleFunctions);
                    continue;
                }

                const auto* fnDecl = statement->as<FunctionDeclaration>();
                if (!fnDecl)
                    continue;

                if (std::optional<Attribute> lifecycleAttribute = getModuleLifecycleAttribute(*fnDecl); lifecycleAttribute.has_value())
                {
                    // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                    switch (*lifecycleAttribute)
                    {
                    case Attribute::ModuleApiVersion: setLifecycleFunctionIfEmpty(lifecycleFunctions.apiVersion, fnDecl); break;
                    case Attribute::ModuleLoad: setLifecycleFunctionIfEmpty(lifecycleFunctions.load, fnDecl); break;
                    case Attribute::ModuleUpdate: setLifecycleFunctionIfEmpty(lifecycleFunctions.update, fnDecl); break;
                    case Attribute::ModuleUnload: setLifecycleFunctionIfEmpty(lifecycleFunctions.unload, fnDecl); break;
                    case Attribute::ModuleSaveState: setLifecycleFunctionIfEmpty(lifecycleFunctions.saveState, fnDecl); break;
                    case Attribute::ModuleRestoreState: setLifecycleFunctionIfEmpty(lifecycleFunctions.restoreState, fnDecl); break;
                    default: break;
                    }
                }
            }
        }

        const FunctionDeclaration* findApplicationEntry(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    if (const auto* nested = findApplicationEntry(realmDecl->statements))
                        return nested;
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    if (const auto* nested = findApplicationEntry(group->declarations))
                        return nested;
                    continue;
                }
                if (const auto* fnDecl = statement->as<FunctionDeclaration>();
                    fnDecl && fnDecl->isApplicationEntry)
                    return fnDecl;
            }
            return nullptr;
        }

        void collectExportedFunctions(const std::vector<NodePtr<Statement>>& statements, std::vector<ExportedFunctionInfo>& exportedFunctions)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectExportedFunctions(realmDecl->statements, exportedFunctions);
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectExportedFunctions(group->declarations, exportedFunctions);
                    continue;
                }

                const auto* fnDecl = statement->as<FunctionDeclaration>();
                if (!fnDecl || !isExportedFunction(*fnDecl))
                    continue;

                auto exportSymbol = fnDecl->name ? fnDecl->name->referencedSymbol.Lock() : nullptr;
                auto declaredFunctionType = exportSymbol && exportSymbol->type
                    ? exportSymbol->type.AsFast<sema::FunctionType>()
                    : nullptr;
                const std::string baseLogicalName = fnDecl->name ? fnDecl->name->token.value : "";
                const std::string baseSymbolName = getExportedCppSymbolName(*fnDecl);
                const std::string internalSymbol = exportSymbol
                    ? Mangler::mangleFunction(fnDecl->name->token.value, declaredFunctionType->paramTypes, exportSymbol->scopePath)
                    : "";

                auto appendCommandAndEventMetadata = [&](ExportedFunctionInfo& info)
                {
                    if (isCommandFunction(*fnDecl))
                    {
                        if (auto commandArg = getSingleAttributeArg(fnDecl->attributes, Attribute::Command); commandArg.has_value())
                            info.commandName = commandArg->value;
                        else
                            info.commandName = info.logicalName;
                    }

                    if (isEventFunction(*fnDecl))
                    {
                        if (auto eventArg = getSingleAttributeArg(fnDecl->attributes, Attribute::Event); eventArg.has_value())
                            info.eventName = eventArg->value;
                    }
                };

                auto instantiations = getInstantiateTypeLists(*fnDecl);
                if (!fnDecl->genericParameters.empty() && !instantiations.empty())
                {
                    for (const auto& instantiationTypes : instantiations)
                    {
                        ExportedFunctionInfo info;
                        info.declaration = fnDecl;
                        info.logicalName = formatInstantiatedLogicalName(baseLogicalName, instantiationTypes);
                        info.symbolName = formatInstantiatedExportSymbolName(baseSymbolName, instantiationTypes);
                        info.internalSymbol = internalSymbol;
                        info.templateArguments = instantiationTypes;

                        if (exportSymbol)
                        {
                            auto bindings = buildExtendedGenericBindings(
                                exportSymbol->genericParameterNames,
                                exportSymbol->hasGenericParameterPack,
                                instantiationTypes
                            );
                            info.functionType = instantiateGenericType(exportSymbol->type, bindings).AsFast<sema::FunctionType>();
                        }

                        appendCommandAndEventMetadata(info);
                        exportedFunctions.push_back(std::move(info));
                    }
                    continue;
                }

                ExportedFunctionInfo info;
                info.declaration = fnDecl;
                info.functionType = declaredFunctionType;
                info.logicalName = baseLogicalName;
                info.symbolName = baseSymbolName;
                info.internalSymbol = internalSymbol;
                appendCommandAndEventMetadata(info);
                exportedFunctions.push_back(std::move(info));
            }
        }

        void indexStructDeclarations(
            const std::vector<NodePtr<Statement>>& statements,
            std::unordered_map<const sema::StructType*, const ObjectDeclaration*>& objectDeclarations,
            std::unordered_map<const sema::StructType*, const ComponentDeclaration*>& componentDeclarations)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    indexStructDeclarations(realmDecl->statements, objectDeclarations, componentDeclarations);
                    continue;
                }

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (auto componentType = getStructTypeFromSymbol(componentDecl->name ? componentDecl->name->referencedSymbol.Lock() : nullptr); componentType)
                        componentDeclarations.try_emplace(componentType.Get(), componentDecl);
                    continue;
                }

                if (const auto* objectDecl = statement->as<ObjectDeclaration>())
                {
                    if (auto objectType = getStructTypeFromSymbol(objectDecl->name ? objectDecl->name->referencedSymbol.Lock() : nullptr); objectType)
                        objectDeclarations.try_emplace(objectType.Get(), objectDecl);
                }
            }
        }

        void collectExportedTypes(const std::vector<NodePtr<Statement>>& statements,
                                  std::vector<ExportedFunctionInfo>& exportedFunctions,
                                  std::vector<ExportedTypeInfo>& exportedTypes,
                                  const std::unordered_map<const sema::StructType*, const ObjectDeclaration*>& objectDeclarations,
                                  const std::unordered_map<const sema::StructType*, const ComponentDeclaration*>& componentDeclarations)
        {
            auto& typeContext = Compiler::get().getTypeContext();
            auto isAbiSafeType = [&](const Ref<sema::Type>& type)
            {
                return getAbiTypeEnumName(type) != "WIO_ABI_UNKNOWN";
            };

            auto isExactType = [&](const Ref<sema::Type>& lhs, const Ref<sema::Type>& rhs)
            {
                Ref<sema::Type> left = unwrapAliasType(lhs);
                Ref<sema::Type> right = unwrapAliasType(rhs);
                return left && right && left->isCompatibleWith(right) && right->isCompatibleWith(left);
            };

            auto isCopyConstructorSignature = [&](const Ref<sema::StructType>& selfType,
                                                  const Ref<sema::FunctionType>& functionType)
            {
                if (!selfType || !functionType || functionType->paramTypes.size() != 1)
                    return false;

                Ref<sema::Type> parameterType = unwrapAliasType(functionType->paramTypes[0]);
                if (!parameterType || parameterType->kind() != sema::TypeKind::Reference)
                    return false;

                auto referenceType = parameterType.AsFast<sema::ReferenceType>();
                return isExactType(referenceType->referredType, selfType);
            };

            auto formatConstructorLogicalName = [&](const std::string& typeLogicalName,
                                                    const std::vector<Ref<sema::Type>>& parameterTypes)
            {
                if (parameterTypes.empty())
                    return typeLogicalName + ".__create";

                std::string result = typeLogicalName + ".__create(";
                for (size_t i = 0; i < parameterTypes.size(); ++i)
                {
                    result += parameterTypes[i] ? parameterTypes[i]->toString() : "unknown";
                    if (i + 1 < parameterTypes.size())
                        result += ", ";
                }
                result += ")";
                return result;
            };

            auto formatConstructorSymbolName = [&](const std::string& typeSymbolName,
                                                   const std::vector<Ref<sema::Type>>& parameterTypes)
            {
                std::string result = "WioCreateType__" + typeSymbolName;
                if (parameterTypes.empty())
                    return result;

                for (const auto& parameterType : parameterTypes)
                {
                    result += "__";
                    std::string fragment = Mangler::mangleType(parameterType);
                    std::ranges::replace(fragment, ':', '_');
                    result += fragment;
                }

                return result;
            };

            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectExportedTypes(realmDecl->statements, exportedFunctions, exportedTypes, objectDeclarations, componentDeclarations);
                    continue;
                }

                auto appendExportedField = [&](const VariableDeclaration& variableDecl,
                                               ExportedTypeInfo& typeInfo,
                                               bool isObjectType,
                                               std::unordered_set<std::string>& seenFieldNames)
                {
                    const std::string fieldName = variableDecl.name ? variableDecl.name->token.value : "";
                    if (fieldName.empty() || !seenFieldNames.insert(fieldName).second)
                        return;

                    auto variableSymbol = variableDecl.name ? variableDecl.name->referencedSymbol.Lock() : nullptr;
                    if (!variableSymbol || !variableSymbol->flags.get_isPublic())
                        return;

                    Ref<sema::Type> fieldType = variableSymbol->type ? variableSymbol->type : variableDecl.name->refType.Lock();
                    if (!fieldType)
                        return;

                    auto resolvedFieldType = unwrapAliasType(fieldType);
                    auto bridgeResolvedFieldType = resolvedFieldType;
                    if (bridgeResolvedFieldType && bridgeResolvedFieldType->kind() == sema::TypeKind::Nullable)
                    {
                        bridgeResolvedFieldType = unwrapAliasType(
                            bridgeResolvedFieldType.AsFast<sema::NullableType>()->valueType
                        );
                    }
                    ExportedFunctionInfo::FieldAccessorKind accessorKind = ExportedFunctionInfo::FieldAccessorKind::Value;
                    Ref<sema::Type> accessorBridgeType = fieldType;
                    std::string fieldBridgeCppTypeName = toCppType(fieldType);
                    bool valueRequiresBridgeCast = false;

                    if (bridgeResolvedFieldType && bridgeResolvedFieldType->kind() == sema::TypeKind::Struct)
                    {
                        if (auto structType = bridgeResolvedFieldType.AsFast<sema::StructType>(); structType)
                        {
                            if ((structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                            {
                                accessorBridgeType = structType->enumUnderlyingType;
                                valueRequiresBridgeCast = true;
                            }
                            else if (objectDeclarations.contains(structType.Get()) &&
                                     getStdValueStructType(structType, "ByteBuffer") == nullptr)
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ObjectHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                            else if (componentDeclarations.contains(structType.Get()) &&
                                     getStdValueStructType(structType, "ResultUnit") == nullptr &&
                                     getStdValueStructType(structType, "Span") == nullptr)
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ComponentHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                        }
                    }

                    ExportedFieldInfo fieldInfo;
                    fieldInfo.declaration = &variableDecl;
                    fieldInfo.fieldName = fieldName;
                    fieldInfo.fieldType = fieldType;
                    fieldInfo.isReadOnly = variableSymbol && variableSymbol->flags.get_isReadOnly();
                    fieldInfo.memberCppName = sanitizeCppIdentifier(fieldInfo.fieldName);
                    fieldInfo.memberCppTypeName = fieldBridgeCppTypeName;
                    fieldInfo.accessorKind = accessorKind;
                    if (variableSymbol->flags.get_isProtected())
                        fieldInfo.accessModifier = AccessModifier::Protected;
                    else if (variableSymbol->flags.get_isPrivate())
                        fieldInfo.accessModifier = AccessModifier::Private;
                    else
                        fieldInfo.accessModifier = AccessModifier::Public;

                    const bool isTextField = resolvedFieldType && resolvedFieldType->kind() == sema::TypeKind::Primitive &&
                        resolvedFieldType.AsFast<sema::PrimitiveType>()->name == "text";
                    auto optionFieldType = getStdValueStructType(resolvedFieldType, "Option");
                    const bool isOptionField = optionFieldType && optionFieldType->genericArguments.size() == 1 &&
                        isSdkValueBridgeType(optionFieldType->genericArguments.front());
                    auto resultFieldType = getStdValueStructType(resolvedFieldType, "Result");
                    const bool isResultField = resultFieldType && resultFieldType->genericArguments.size() == 1 &&
                        isSdkValueBridgeType(resultFieldType->genericArguments.front());
                    const bool isUnitField = getStdValueStructType(resolvedFieldType, "ResultUnit") != nullptr;
                    const bool isSpanField = getStdValueStructType(resolvedFieldType, "Span") != nullptr;
                    const bool isByteBufferField = getStdValueStructType(resolvedFieldType, "ByteBuffer") != nullptr;
                    auto queueFieldType = getStdValueStructType(resolvedFieldType, "Queue");
                    auto unorderedSetFieldType = getStdValueStructType(resolvedFieldType, "UnorderedSet");
                    auto orderedSetFieldType = getStdValueStructType(resolvedFieldType, "OrderedSet");
                    auto tupleFieldType = getStdValueStructType(resolvedFieldType, "Tuple");
                    const bool isTupleField = tupleFieldType && std::all_of(
                        tupleFieldType->genericArguments.begin(),
                        tupleFieldType->genericArguments.end(),
                        [](const Ref<sema::Type>& argument) { return isSdkValueBridgeType(argument); }
                    );
                    const bool isSequenceContainerField =
                        (queueFieldType && queueFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(queueFieldType->genericArguments.front())) ||
                        (unorderedSetFieldType && unorderedSetFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(unorderedSetFieldType->genericArguments.front())) ||
                        (orderedSetFieldType && orderedSetFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(orderedSetFieldType->genericArguments.front()));
                    const bool needsDynamicBridge = resolvedFieldType &&
                        (isTextField || isOptionField || isResultField || isUnitField || isSpanField || isByteBufferField ||
                         isSequenceContainerField || isTupleField ||
                         resolvedFieldType->kind() == sema::TypeKind::Array ||
                         resolvedFieldType->kind() == sema::TypeKind::Dictionary ||
                         resolvedFieldType->kind() == sema::TypeKind::Function);
                    if (needsDynamicBridge)
                    {
                        fieldInfo.dynamicGetterSymbol = common::formatString("WioDynamicGetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                        if (!fieldInfo.isReadOnly)
                            fieldInfo.dynamicSetterSymbol = common::formatString("WioDynamicSetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                    }

                    ExportedFunctionInfo getterExport;
                    getterExport.functionType = typeContext.getOrCreateFunctionType(accessorBridgeType, { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                    getterExport.logicalName = common::formatString("{}.{}.get", typeInfo.logicalName, fieldInfo.fieldName);
                    getterExport.symbolName = common::formatString("WioGetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                    getterExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeFieldGet;
                    getterExport.ownerCppTypeName = typeInfo.cppTypeName;
                    getterExport.memberCppName = fieldInfo.memberCppName;
                    getterExport.memberCppTypeName = fieldInfo.memberCppTypeName;
                    getterExport.ownerIsObject = isObjectType;
                    getterExport.fieldAccessorKind = fieldInfo.accessorKind;
                    getterExport.valueRequiresBridgeCast = valueRequiresBridgeCast;
                    fieldInfo.getterExportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(getterExport));

                    if (!fieldInfo.isReadOnly)
                    {
                        ExportedFunctionInfo setterExport;
                        setterExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize(), accessorBridgeType }).AsFast<sema::FunctionType>();
                        setterExport.logicalName = common::formatString("{}.{}.set", typeInfo.logicalName, fieldInfo.fieldName);
                        setterExport.symbolName = common::formatString("WioSetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                        setterExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeFieldSet;
                        setterExport.ownerCppTypeName = typeInfo.cppTypeName;
                        setterExport.memberCppName = fieldInfo.memberCppName;
                        setterExport.memberCppTypeName = fieldInfo.memberCppTypeName;
                        setterExport.ownerIsObject = isObjectType;
                        setterExport.fieldAccessorKind = fieldInfo.accessorKind;
                        setterExport.valueRequiresBridgeCast = valueRequiresBridgeCast;
                        fieldInfo.setterExportIndex = exportedFunctions.size();
                        exportedFunctions.push_back(std::move(setterExport));
                    }

                    typeInfo.fields.push_back(std::move(fieldInfo));
                };

                auto appendExportedMethod = [&](const FunctionDeclaration& functionDecl,
                                                ExportedTypeInfo& typeInfo,
                                                std::unordered_set<std::string>& seenMethodKeys)
                {
                    if (!functionDecl.name || !functionDecl.genericParameters.empty())
                        return;

                    const std::string functionName = functionDecl.name->token.value;
                    if (functionName == "OnConstruct" || functionName == "OnDestruct")
                        return;

                    auto functionSymbol = functionDecl.name->referencedSymbol.Lock();
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                    if (!functionSymbol || !functionSymbol->flags.get_isPublic() || !functionType)
                        return;

                    const std::string methodKey = functionName + "|" + Mangler::mangleFunction(functionName, functionType->paramTypes);
                    if (!seenMethodKeys.insert(methodKey).second)
                        return;

                    if (getAbiTypeEnumName(functionType->returnType) == "WIO_ABI_UNKNOWN")
                        return;

                    bool allParametersAbiSafe = true;
                    std::vector<Ref<sema::Type>> exportedParameterTypes;
                    exportedParameterTypes.reserve(functionType->paramTypes.size() + 1);
                    exportedParameterTypes.push_back(typeContext.getUSize());

                    for (const auto& parameterType : functionType->paramTypes)
                    {
                        if (getAbiTypeEnumName(parameterType) == "WIO_ABI_UNKNOWN")
                        {
                            allParametersAbiSafe = false;
                            break;
                        }

                        exportedParameterTypes.push_back(parameterType);
                    }

                    if (!allParametersAbiSafe)
                        return;

                    ExportedFunctionInfo methodExport;
                    methodExport.declaration = &functionDecl;
                    methodExport.functionType = typeContext.getOrCreateFunctionType(functionType->returnType, exportedParameterTypes).AsFast<sema::FunctionType>();
                    methodExport.logicalName = typeInfo.logicalName + "." + functionName;
                    methodExport.symbolName = common::formatString("WioMethod__{}__{}__{}", typeInfo.symbolName, functionName, Mangler::mangleFunction(functionName, functionType->paramTypes));
                    methodExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeMethod;
                    methodExport.ownerCppTypeName = typeInfo.cppTypeName;
                    methodExport.memberCppName = Mangler::mangleFunction(functionName, functionType->paramTypes);
                    methodExport.ownerIsObject = true;

                    ExportedMethodInfo methodInfo;
                    methodInfo.declaration = &functionDecl;
                    methodInfo.methodName = functionName;
                    methodInfo.exportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(methodExport));
                    typeInfo.methods.push_back(std::move(methodInfo));
                };

                auto appendConstructorExport = [&](ExportedTypeInfo& typeInfo,
                                                   const std::vector<Ref<sema::Type>>& parameterTypes,
                                                   bool isObjectType,
                                                   std::unordered_set<std::string>& seenConstructorSignatures)
                {
                    for (const auto& parameterType : parameterTypes)
                    {
                        if (!isAbiSafeType(parameterType))
                            return;
                    }

                    std::string signatureKey = formatConstructorSymbolName(typeInfo.symbolName, parameterTypes);
                    if (!seenConstructorSignatures.insert(signatureKey).second)
                        return;

                    ExportedFunctionInfo constructorExport;
                    constructorExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getUSize(), parameterTypes).AsFast<sema::FunctionType>();
                    constructorExport.logicalName = formatConstructorLogicalName(typeInfo.logicalName, parameterTypes);
                    constructorExport.symbolName = std::move(signatureKey);
                    constructorExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeConstruct;
                    constructorExport.ownerCppTypeName = typeInfo.cppTypeName;
                    constructorExport.ownerIsObject = isObjectType;

                    const size_t exportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(constructorExport));
                    typeInfo.constructors.push_back({ exportIndex });

                    if (parameterTypes.empty() && !typeInfo.createExportIndex.has_value())
                        typeInfo.createExportIndex = exportIndex;
                };

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (!isExportedComponent(*componentDecl) || !componentDecl->name || !componentDecl->genericParameters.empty())
                        continue;

                    auto componentSymbol = componentDecl->name->referencedSymbol.Lock();
                    auto componentType = getStructTypeFromSymbol(componentSymbol);
                    if (!componentType)
                        continue;

                    ExportedTypeInfo typeInfo;
                    typeInfo.attributes = &componentDecl->attributes;
                    typeInfo.logicalName = componentType->scopePath.empty()
                        ? componentType->name
                        : common::formatString("{}::{}", componentType->scopePath, componentType->name);
                    typeInfo.symbolName = Mangler::mangleStruct(componentType->name, componentType->scopePath);
                    typeInfo.cppTypeName = mangleStructTypeName(componentType);
                    typeInfo.isObject = false;

                    ExportedFunctionInfo destroyExport;
                    destroyExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                    destroyExport.logicalName = typeInfo.logicalName + ".__destroy";
                    destroyExport.symbolName = "WioDestroyType__" + typeInfo.symbolName;
                    destroyExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeDestroy;
                    destroyExport.ownerCppTypeName = typeInfo.cppTypeName;
                    typeInfo.destroyExportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(destroyExport));

                    std::vector<Ref<sema::Type>> memberTypes;
                    memberTypes.reserve(componentDecl->members.size());
                    bool hasCustomCtor = false;
                    bool hasEmptyCtor = false;
                    bool hasMemberCtor = false;
                    const bool hasNoDefaultCtor = hasAttribute(componentDecl->attributes, Attribute::NoDefaultCtor);
                    const bool forceGenerateCtors = hasAttribute(componentDecl->attributes, Attribute::GenerateCtors);
                    std::unordered_set<std::string> seenConstructorSignatures;

                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                            continue;

                        auto* variableDecl = member.declaration->as<VariableDeclaration>();
                        auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                        if (Ref<sema::Type> memberType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock(); memberType)
                            memberTypes.push_back(memberType);
                    }

                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                            continue;

                        auto* functionDecl = member.declaration->as<FunctionDeclaration>();
                        if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                            continue;

                        hasCustomCtor = true;
                        auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                        auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                        if (!functionType)
                            continue;

                        const bool isCopyCtor = isCopyConstructorSignature(componentType, functionType);
                        if (functionType->paramTypes.empty())
                            hasEmptyCtor = true;

                        if (!isCopyCtor && functionType->paramTypes.size() == memberTypes.size())
                        {
                            bool isMemberCtor = true;
                            for (size_t i = 0; i < memberTypes.size(); ++i)
                            {
                                if (!isExactType(functionType->paramTypes[i], memberTypes[i]))
                                {
                                    isMemberCtor = false;
                                    break;
                                }
                            }

                            if (isMemberCtor)
                                hasMemberCtor = true;
                        }

                        if (!isCopyCtor && member.access == AccessModifier::Public)
                            appendConstructorExport(typeInfo, functionType->paramTypes, /*isObjectType=*/false, seenConstructorSignatures);
                    }

                    if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                    {
                        if (!hasEmptyCtor)
                            appendConstructorExport(typeInfo, {}, /*isObjectType=*/false, seenConstructorSignatures);

                        if (!hasMemberCtor && !memberTypes.empty())
                            appendConstructorExport(typeInfo, memberTypes, /*isObjectType=*/false, seenConstructorSignatures);
                    }

                    std::unordered_set<std::string> seenFieldNames;
                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                            continue;

                        appendExportedField(*member.declaration->as<VariableDeclaration>(), typeInfo, false, seenFieldNames);
                    }

                    exportedTypes.push_back(std::move(typeInfo));
                    continue;
                }

                const auto* objectDecl = statement->as<ObjectDeclaration>();
                if (!objectDecl || !isExportedObject(*objectDecl) || !objectDecl->name || !objectDecl->genericParameters.empty())
                    continue;

                auto objectSymbol = objectDecl->name->referencedSymbol.Lock();
                auto objectType = getStructTypeFromSymbol(objectSymbol);
                if (!objectType)
                    continue;

                ExportedTypeInfo typeInfo;
                typeInfo.attributes = &objectDecl->attributes;
                typeInfo.logicalName = objectType->scopePath.empty()
                    ? objectType->name
                    : common::formatString("{}::{}", objectType->scopePath, objectType->name);

                typeInfo.symbolName = Mangler::mangleStruct(objectType->name, objectType->scopePath);
                typeInfo.cppTypeName = mangleStructTypeName(objectType);
                typeInfo.isObject = true;

                ExportedFunctionInfo destroyExport;
                destroyExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                destroyExport.logicalName = typeInfo.logicalName + ".__destroy";
                destroyExport.symbolName = "WioDestroyType__" + typeInfo.symbolName;
                destroyExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeDestroy;
                destroyExport.ownerCppTypeName = typeInfo.cppTypeName;
                destroyExport.ownerIsObject = true;
                typeInfo.destroyExportIndex = exportedFunctions.size();
                exportedFunctions.push_back(std::move(destroyExport));

                std::vector<Ref<sema::Type>> memberTypes;
                memberTypes.reserve(objectDecl->members.size());
                bool hasCustomCtor = false;
                bool hasEmptyCtor = false;
                bool hasMemberCtor = false;
                const bool hasNoDefaultCtor = hasAttribute(objectDecl->attributes, Attribute::NoDefaultCtor);
                const bool forceGenerateCtors = hasAttribute(objectDecl->attributes, Attribute::GenerateCtors);
                std::unordered_set<std::string> seenConstructorSignatures;

                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                        continue;

                    auto* variableDecl = member.declaration->as<VariableDeclaration>();
                    auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                    if (Ref<sema::Type> memberType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock(); memberType)
                        memberTypes.push_back(memberType);
                }

                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto* functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    hasCustomCtor = true;
                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor = isCopyConstructorSignature(objectType, functionType);
                    if (functionType->paramTypes.empty())
                        hasEmptyCtor = true;

                    if (!isCopyCtor && functionType->paramTypes.size() == memberTypes.size())
                    {
                        bool isMemberCtor = true;
                        for (size_t i = 0; i < memberTypes.size(); ++i)
                        {
                            if (!isExactType(functionType->paramTypes[i], memberTypes[i]))
                            {
                                isMemberCtor = false;
                                break;
                            }
                        }

                        if (isMemberCtor)
                            hasMemberCtor = true;
                    }

                    if (!isCopyCtor && member.access == AccessModifier::Public)
                        appendConstructorExport(typeInfo, functionType->paramTypes, /*isObjectType=*/true, seenConstructorSignatures);
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        appendConstructorExport(typeInfo, {}, /*isObjectType=*/true, seenConstructorSignatures);

                    if (!hasMemberCtor && !memberTypes.empty())
                        appendConstructorExport(typeInfo, memberTypes, /*isObjectType=*/true, seenConstructorSignatures);
                }

                std::unordered_set<std::string> seenFieldNames;
                std::unordered_set<std::string> seenMethodKeys;
                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration)
                        continue;

                    if (member.declaration->is<VariableDeclaration>())
                    {
                        appendExportedField(*member.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                        continue;
                    }

                    if (!member.declaration->is<FunctionDeclaration>())
                        continue;

                    appendExportedMethod(*member.declaration->as<FunctionDeclaration>(), typeInfo, seenMethodKeys);
                }

                std::function<void(const Ref<sema::StructType>&)> appendInheritedMembers = [&](const Ref<sema::StructType>& derivedType)
                {
                    if (!derivedType)
                        return;

                    for (const auto& baseType : derivedType->baseTypes)
                    {
                        auto resolvedBaseType = unwrapAliasType(baseType);
                        if (!resolvedBaseType || resolvedBaseType->kind() != sema::TypeKind::Struct)
                            continue;

                        auto baseStruct = resolvedBaseType.AsFast<sema::StructType>();
                        if (!baseStruct || baseStruct->isInterface || (baseStruct->name == "object" && baseStruct->scopePath.empty()))
                            continue;

                        if (auto objectIt = objectDeclarations.find(baseStruct.Get()); objectIt != objectDeclarations.end())
                        {
                            if (const auto* baseObjectDecl = objectIt->second; baseObjectDecl)
                            {
                                for (const auto& baseMember : baseObjectDecl->members)
                                {
                                    if (!baseMember.declaration)
                                        continue;

                                    if (baseMember.declaration->is<VariableDeclaration>())
                                    {
                                        appendExportedField(*baseMember.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                                        continue;
                                    }

                                    if (baseMember.declaration->is<FunctionDeclaration>())
                                        appendExportedMethod(*baseMember.declaration->as<FunctionDeclaration>(), typeInfo, seenMethodKeys);
                                }
                            }

                            appendInheritedMembers(baseStruct);
                            continue;
                        }

                        if (auto componentIt = componentDeclarations.find(baseStruct.Get()); componentIt != componentDeclarations.end())
                        {
                            const auto* baseComponentDecl = componentIt->second;
                            if (!baseComponentDecl)
                                continue;

                            for (const auto& baseMember : baseComponentDecl->members)
                            {
                                if (!baseMember.declaration || !baseMember.declaration->is<VariableDeclaration>())
                                    continue;

                                appendExportedField(*baseMember.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                            }
                        }
                    }
                };

                appendInheritedMembers(objectType);

                exportedTypes.push_back(std::move(typeInfo));
            }
        }

        void appendNativeHeader(std::string header,
                                std::unordered_set<std::string>& seenHeaders,
                                std::vector<std::string>& orderedHeaders)
        {
            if (!header.empty() && seenHeaders.insert(header).second)
                orderedHeaders.push_back(std::move(header));
        }

        void collectCppHeadersFromFunction(const FunctionDeclaration& declaration,
                                           std::unordered_set<std::string>& seenHeaders,
                                           std::vector<std::string>& orderedHeaders)
        {
            if (!isNativeFunction(declaration))
                return;

            auto headerArg = getSingleAttributeArg(declaration.attributes, Attribute::CppHeader);
            if (!headerArg.has_value() || headerArg->type != TokenType::stringLiteral)
                return;

            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
        }

        void collectCppHeaders(const std::vector<NodePtr<Statement>>& statements, std::unordered_set<std::string>& seenHeaders, std::vector<std::string>& orderedHeaders)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectCppHeaders(realmDecl->statements, seenHeaders, orderedHeaders);
                    continue;
                }

                if (const auto* useStmt = statement->as<UseStatement>())
                {
                    if (useStmt->isCppHeader)
                    {
                        appendNativeHeader(useStmt->modulePath, seenHeaders, orderedHeaders);
                        continue;
                    }
                }

                if (const auto* fnDecl = statement->as<FunctionDeclaration>())
                {
                    collectCppHeadersFromFunction(*fnDecl, seenHeaders, orderedHeaders);
                    continue;
                }

                if (const auto* objectDecl = statement->as<ObjectDeclaration>())
                {
                    if (hasAttribute(objectDecl->attributes, Attribute::Native))
                    {
                        if (auto headerArg = getSingleAttributeArg(objectDecl->attributes, Attribute::CppHeader); headerArg.has_value() && headerArg->type == TokenType::stringLiteral)
                            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
                    }

                    for (const auto& member : objectDecl->members)
                    {
                        if (member.declaration && member.declaration->is<FunctionDeclaration>())
                            collectCppHeadersFromFunction(*member.declaration->as<FunctionDeclaration>(), seenHeaders, orderedHeaders);
                    }
                    continue;
                }

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (hasAttribute(componentDecl->attributes, Attribute::Native))
                    {
                        if (auto headerArg = getSingleAttributeArg(componentDecl->attributes, Attribute::CppHeader); headerArg.has_value() && headerArg->type == TokenType::stringLiteral)
                            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
                    }

                    for (const auto& member : componentDecl->members)
                    {
                        if (member.declaration && member.declaration->is<FunctionDeclaration>())
                            collectCppHeadersFromFunction(*member.declaration->as<FunctionDeclaration>(), seenHeaders, orderedHeaders);
                    }
                    continue;
                }

                if (const auto* interfaceDecl = statement->as<InterfaceDeclaration>())
                {
                    for (const auto& method : interfaceDecl->methods)
                    {
                        if (method)
                            collectCppHeadersFromFunction(*method, seenHeaders, orderedHeaders);
                    }
                }
            }
        }

        std::string mangleStructTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};
            std::string name = Mangler::mangleStruct(type->name, type->scopePath);
            if (!type->genericArguments.empty())
            {
                name += "<";
                for (size_t i = 0; i < type->genericArguments.size(); ++i)
                {
                    name += toCppType(type->genericArguments[i]);
                    if (i + 1 < type->genericArguments.size())
                        name += ", ";
                }
                name += ">";
            }

            return name;
        }
