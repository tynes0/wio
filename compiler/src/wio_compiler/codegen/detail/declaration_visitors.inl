// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void CppGenerator::visit(ExpressionStatement& node)
    {
        emitSourceDirective(node.location());
        EMIT_TABS();
        node.expression->accept(*this);
        buffer_ << ";\n";
    }

    void CppGenerator::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(AttributeDeclaration& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(DeclarationGroup& node)
    {
        for (auto& declaration : node.declarations)
            if (declaration) declaration->accept(*this);
    }

    void CppGenerator::visit(VariableDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto sym = node.name->referencedSymbol.Lock();

        Ref<sema::Type> varType = (sym && sym->type) ? sym->type : node.name->refType.Lock();
        std::string typeStr = toCppType(varType);
        Ref<sema::Type> resolvedVarType = unwrapAliasType(varType);
        if (resolvedVarType &&
            resolvedVarType->kind() == sema::TypeKind::Array &&
            node.initializer)
        {
            auto arrayType = resolvedVarType.AsFast<sema::ArrayType>();
            if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static &&
                arrayType->size == 0)
            {
                if (const auto* functionCall = node.initializer->as<FunctionCallExpression>())
                {
                    if (const auto* memberAccess = functionCall->callee ? functionCall->callee->as<MemberAccessExpression>() : nullptr;
                        memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
                    {
                        typeStr = "auto";
                    }
                }
            }
        }

        if (typeStr.empty())
            typeStr = "auto";

        std::string prefix;
        std::string suffix;

        if (node.mutability == Mutability::Const)
        {
            std::function<bool(const NodePtr<Expression>&)> containsRuntimeTextValue;
            containsRuntimeTextValue = [&](const NodePtr<Expression>& expression) -> bool
            {
                if (!expression)
                    return false;

                Ref<sema::Type> expressionType = unwrapAliasTypeForCodegen(expression->refType.Lock());
                if (expressionType && expressionType->kind() == sema::TypeKind::Primitive)
                {
                    const std::string& name = expressionType.AsFast<sema::PrimitiveType>()->name;
                    if (name == "string" || name == "text")
                        return true;
                }

                if (const auto* binary = expression->as<BinaryExpression>())
                    return containsRuntimeTextValue(binary->left) || containsRuntimeTextValue(binary->right);
                if (const auto* unary = expression->as<UnaryExpression>())
                    return containsRuntimeTextValue(unary->operand);
                if (const auto* fit = expression->as<FitExpression>())
                    return containsRuntimeTextValue(fit->operand);
                return false;
            };

            const bool requiresRuntimeConstStorage =
                containsRuntimeTextValue(node.initializer) ||
                (resolvedVarType &&
                 resolvedVarType->kind() == sema::TypeKind::Primitive &&
                 (resolvedVarType.AsFast<sema::PrimitiveType>()->name == "string" ||
                  resolvedVarType.AsFast<sema::PrimitiveType>()->name == "text"));
            if (requiresRuntimeConstStorage)
                prefix = currentClassName_.empty() ? "const " : "inline static const ";
            else
                prefix = currentClassName_.empty() ? "constexpr " : "static constexpr ";
        }
        else if (node.mutability == Mutability::Immutable)
        {
            bool isStruct = (varType && varType->kind() == sema::TypeKind::Struct);
            if (!isStruct)
            {
                if (!typeStr.empty() && typeStr.back() == '*')
                    suffix = " const";
                else
                    prefix = "const ";
            }
        }
        EMIT_TABS();

        buffer_ << prefix << typeStr << suffix << " ";

        std::string varName = sanitizeCppIdentifier(node.name->token.value);

        buffer_ << ((sym && sym->flags.get_isGlobal()) ? Mangler::mangleGlobalVar(varName, sym->scopePath) : varName);

        if (node.initializer)
        {
            buffer_ << " = ";
            emitExpressionWithExpectedType(node.initializer, varType, false);
        }
        else
        {
            // Explicitly typed declarations without an initializer use value
            // initialization, keeping scalar defaults deterministic as well.
            buffer_ << "{}";
        }

        buffer_ << ";\n";
    }

    void CppGenerator::visit(TypeAliasDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto sym = node.name->referencedSymbol.Lock();
        const std::string aliasName = sym ? Mangler::mangleStruct(sym->name, sym->scopePath) : node.name->token.value;

        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }

        EMIT_TABS();
        emit("using " + aliasName + " = ");
        if (node.aliasedType)
            node.aliasedType->accept(*this);
        else
            emit("void");
        emitLine(";");
    }

    void CppGenerator::visit(FunctionDeclaration& node)
    {
        auto sym = node.name->referencedSymbol.Lock();
        auto funcType = sym->type.AsFast<sema::FunctionType>();
        Ref<sema::Type> previousFunctionReturnType = currentFunctionReturnType_;
        bool previousFunctionIsAsync = currentFunctionIsAsync_;
        currentFunctionReturnType_ = funcType ? funcType->returnType : nullptr;
        currentFunctionIsAsync_ = node.isAsync;
        if (node.isAsync)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(currentFunctionReturnType_);
            currentFunctionReturnType_ = resolvedTaskType && resolvedTaskType->kind() == sema::TypeKind::AsyncTask
                ? resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType
                : nullptr;
        }
        auto instantiationTypeLists = getInstantiateTypeLists(node);

        std::string returnType = funcType->returnType ? toCppType(funcType->returnType) : "void";
        std::string funcName = node.name->token.value;
        bool isNative = isNativeFunction(node);
        bool isExported = isExportedFunction(node);
        bool hasModuleLifecycleExport = getModuleLifecycleAttribute(node).has_value();
        bool emitsExportWrapper = isExported || hasModuleLifecycleExport;

        struct BehavioralProcessorInstance
        {
            std::string phase;
            std::string cppTypeName;
            std::string hookCppName;
            std::string hookMode;
            Ref<sema::Type> hookValueType;
            std::string variableName;
            std::string finalizedFlagName;
        };
        std::vector<BehavioralProcessorInstance> behavioralProcessors;
        if (!isEmittingPrototypes_ && node.body && !isNative)
        {
            std::vector<const AttributeStatement*> orderedAttributes;
            orderedAttributes.reserve(node.attributes.size());
            for (const auto& attribute : node.attributes)
                if (attribute)
                    orderedAttributes.push_back(attribute.Get());
            std::ranges::stable_sort(
                orderedAttributes,
                {},
                &AttributeStatement::processorOrder);
            for (const auto* attribute : orderedAttributes)
            {
                for (const auto& processor : attribute->processorBindings)
                {
                    if ((processor.phase != "pre" && processor.phase != "post" &&
                         processor.phase != "finally" && processor.phase != "around") ||
                        processor.cppTypeName.empty() || processor.hookCppName.empty())
                    {
                        continue;
                    }
                    if (node.isAsync && processor.phase == "around")
                        continue;
                    const size_t index = behavioralProcessors.size();
                    behavioralProcessors.push_back(BehavioralProcessorInstance{
                        .phase = processor.phase,
                        .cppTypeName = processor.cppTypeName,
                        .hookCppName = processor.hookCppName,
                        .hookMode = processor.hookMode,
                        .hookValueType = processor.hookValueType.Lock(),
                        .variableName = "_wio_attribute_processor_" + std::to_string(index),
                        .finalizedFlagName = "_wio_attribute_finalized_" + std::to_string(index)
                    });
                }
            }
        }

        if (funcName == "Entry" && !node.isAsync &&
            node.genericParameters.empty() &&
            Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!sym || sym->scopePath.empty()))
        {
            if (!isEmittingPrototypes_)
                emitMain(node);
            return;
        }

        emitSourceDirective(node.location());
        emitLine();

        auto instantiateFunctionTypeForCodegen = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            auto bindings = buildExtendedGenericBindings(sym->genericParameterNames, sym->hasGenericParameterPack, instantiationTypes);
            return instantiateGenericType(funcType, bindings).AsFast<sema::FunctionType>();
        };

        auto emitTemplateSpecializationArguments = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            emit(formatTemplateArgumentList(instantiationTypes));
        };

        auto emitGenericParameterArgumentList = [&]()
        {
            if (node.genericParameters.empty())
                return;

            emit("<");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit(node.genericParameters[i]->token.value);
                if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                    emit("...");
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emit(">");
        };

        auto emitExplicitInstantiationDeclaration = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            auto instantiatedFunctionType = instantiateFunctionTypeForCodegen(instantiationTypes);
            if (!instantiatedFunctionType)
                return;

            EMIT_TABS();
            emit("template " + toCppType(instantiatedFunctionType->returnType) + " ");
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : ""));
            emitTemplateSpecializationArguments(instantiationTypes);
            emit("(");
            for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
            {
                emit(toCppType(instantiatedFunctionType->paramTypes[i]));
                if (i + 1 < instantiatedFunctionType->paramTypes.size())
                    emit(", ");
            }
            emitLine(");");
        };

        auto getWrapperParameterTypes = [&](size_t providedFixedParameterCount) -> std::vector<Ref<sema::Type>>
        {
            std::vector<Ref<sema::Type>> parameterTypes;
            if (!funcType)
                return parameterTypes;

            parameterTypes.reserve(providedFixedParameterCount + (funcType->hasParameterPack ? 1 : 0));
            for (size_t i = 0; i < providedFixedParameterCount && i < funcType->paramTypes.size(); ++i)
                parameterTypes.push_back(funcType->paramTypes[i]);

            if (funcType->hasParameterPack && !funcType->paramTypes.empty())
                parameterTypes.push_back(funcType->paramTypes.back());

            return parameterTypes;
        };

        auto emitWrapperParameters = [&](size_t providedFixedParameterCount)
        {
            for (size_t i = 0; i < providedFixedParameterCount; ++i)
            {
                emit(common::formatString(
                    "{} {}",
                    toCppType(funcType->paramTypes[i]),
                    sanitizeCppIdentifier(node.parameters[i].name->token.value)
                ));
                if (i + 1 < providedFixedParameterCount || funcType->hasParameterPack)
                    emit(", ");
            }

            if (funcType->hasParameterPack && !node.parameters.empty())
            {
                const size_t packParameterIndex = node.parameters.size() - 1;
                emit(common::formatString(
                    "{} {}...",
                    toCppType(funcType->paramTypes.back()),
                    sanitizeCppIdentifier(node.parameters[packParameterIndex].name->token.value)
                ));
            }
        };

        auto emitForwardingCallArguments = [&](size_t providedFixedArgumentCount)
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                if (i > 0)
                    emit(", ");

                if (funcType->hasParameterPack && i + 1 == node.parameters.size())
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value) + "...");
                }
                else if (i < providedFixedArgumentCount)
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                }
                else if (node.parameters[i].defaultValue)
                {
                    node.parameters[i].defaultValue->accept(*this);
                }
            }
        };

        auto emitDefaultArgumentWrappers = [&]()
        {
            if ((!node.body && !isNative) || !hasDefaultParameters(node) || (funcType && funcType->hasParameterPack))
                return;

            const size_t requiredParameterCount = getRequiredParameterCount(node);
            const size_t fixedParameterCount = getFixedParameterCount(node);

            if (funcName == "OnConstruct")
            {
                for (size_t wrapperArity = requiredParameterCount; wrapperArity < fixedParameterCount; ++wrapperArity)
                {
                    EMIT_TABS();
                    emit(currentClassName_ + "(");
                    emitWrapperParameters(wrapperArity);
                    emit(") : " + currentClassName_ + "(");
                    emitForwardingCallArguments(wrapperArity);
                    emitLine(") {}");
                }
                return;
            }

            if (funcName == "OnDestruct")
                return;

            const std::string scopePath = sym ? sym->scopePath : "";
            const std::string wrapperFullSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes, scopePath);
            const std::string wrapperMethodFullSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes);

            for (size_t wrapperArity = requiredParameterCount; wrapperArity < fixedParameterCount; ++wrapperArity)
            {
                const auto wrapperParameterTypes = getWrapperParameterTypes(wrapperArity);
                const std::string wrapperSymbol = currentClassName_.empty()
                    ? Mangler::mangleFunction(funcName, wrapperParameterTypes, scopePath)
                    : Mangler::mangleFunction(funcName, wrapperParameterTypes);

                if (!node.genericParameters.empty())
                {
                    EMIT_TABS();
                    emit("template <");
                    for (size_t i = 0; i < node.genericParameters.size(); ++i)
                    {
                        const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                        emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                        if (i + 1 < node.genericParameters.size())
                            emit(", ");
                    }
                    emitLine(">");
                }

                EMIT_TABS();
                emit(returnType + " " + wrapperSymbol + "(");
                emitWrapperParameters(wrapperArity);
                emit(")");

                if (isEmittingPrototypes_)
                {
                    emitLine(";\n");
                    continue;
                }

                emitLine();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");

                emit(currentClassName_.empty() ? wrapperFullSymbol : wrapperMethodFullSymbol);
                emitGenericParameterArgumentList();
                emit("(");
                emitForwardingCallArguments(wrapperArity);
                emit(");");
                emit("\n");
                dedent();
                emitLine("}");
            }
        };

        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i < node.genericParameters.size() - 1)
                    emit(", ");
            }
            emitLine(">");
        }

        EMIT_TABS();

        if (!currentClassName_.empty())
        {
            if (funcName == "OnConstruct")
            {
                emit(currentClassName_ + "(");
            } else if (funcName == "OnDestruct")
            {
                emit("~" + currentClassName_ + "() ");
            }
            else
            {
                if (node.genericParameters.empty())
                    emit("virtual ");
                emit(returnType + " ");
                emit(Mangler::mangleFunction(funcName, funcType->paramTypes) + "(");
            }
        }
        else
        {
            emit(returnType + " ");
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "") + "(");
        }

        if (funcName != "OnDestruct")
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                auto& param = node.parameters[i];
                std::string parameterType = toCppType(param.name->refType.Lock());
                if (param.isParameterPack && parameterType.ends_with("..."))
                    parameterType = parameterType.substr(0, parameterType.size() - 3) + "...";
                emit(common::formatString("{} {}", parameterType, sanitizeCppIdentifier(param.name->token.value)));
                if (isEmittingPrototypes_ && funcType && funcType->hasParameterPack && param.defaultValue && !param.isParameterPack)
                {
                    emit(" = ");
                    param.defaultValue->accept(*this);
                }
                if (i < node.parameters.size() - 1) emit(", ");
            }
            emit(")");

            if (!currentClassName_.empty())
            {
                if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
            }
        }

        if (isEmittingPrototypes_)
        {
            emitLine(";\n");
            emitDefaultArgumentWrappers();
            return;
        }

        if (isNative)
        {
            std::string nativeSymbol = getNativeCppSymbolName(node);
            const bool isNativeMember = !currentClassName_.empty();
            std::function<std::string(const Ref<sema::Type>&)> getNativePodCppTypeName;
            std::function<std::string(const Ref<sema::StructType>&)> getNativePodCppName;

            getNativePodCppName = [&](const Ref<sema::StructType>& structType) -> std::string
            {
                if (!structType)
                    return {};

                std::string cppName = structType->nativeCppName.empty() ? structType->name : structType->nativeCppName;
                if (structType->genericArguments.empty())
                    return cppName;

                cppName += "<";
                for (size_t genericIndex = 0; genericIndex < structType->genericArguments.size(); ++genericIndex)
                {
                    if (genericIndex > 0)
                        cppName += ", ";
                    cppName += getNativePodCppTypeName(structType->genericArguments[genericIndex]);
                }
                cppName += ">";
                return cppName;
            };

            getNativePodCppTypeName = [&](const Ref<sema::Type>& type) -> std::string
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType)
                    return "void";

                if (resolvedType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedType.AsFast<sema::StructType>();
                    if (structType && !structType->isObject && !structType->isInterface && structType->isNativePodComponent)
                        return getNativePodCppName(structType);
                }

                return resolvedType->toCppString();
            };

            auto isDirectStringNativeInteropType = [](const Ref<sema::Type>& type) -> bool
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Primitive)
                    return false;

                auto primitiveType = resolvedType.AsFast<sema::PrimitiveType>();
                return primitiveType && primitiveType->name == "string";
            };

            auto buildNativeReferenceSignatureType = [](const std::string& referredCppType, bool isMutable) -> std::string
            {
                if (isMutable)
                    return referredCppType + "&";

                if (!referredCppType.empty() && referredCppType.back() == '*')
                    return referredCppType + " const&";

                return "const " + referredCppType + "&";
            };

            auto shouldUseNativeReferenceWrapper = [](const Ref<sema::Type>& type) -> bool
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                    return false;

                auto refType = resolvedType.AsFast<sema::ReferenceType>();
                auto referredType = unwrapAliasTypeForCodegen(refType->referredType);
                if (!referredType || referredType->kind() != sema::TypeKind::Struct)
                    return true;

                auto structType = referredType.AsFast<sema::StructType>();
                if (structType->isObject || structType->isInterface || structType->isNativePodComponent)
                    return false;

                return true;
            };

            auto buildNativeReferencePreferredExpr = [&](const std::string& expr, const Ref<sema::Type>& type) -> std::string
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                    return expr;

                return "*" + expr;
            };

            std::function<std::string(const std::string&, const Ref<sema::Type>&, bool)> buildWioToNativePodExpr;
            buildWioToNativePodExpr = [&](const std::string& expr, const Ref<sema::Type>& sourceType, bool sourceIsPointer) -> std::string
            {
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(sourceType);
                if (!nativeStruct)
                    return expr;

                if (usesNativePodAliasModelForCodegen(nativeStruct))
                    return sourceIsPointer ? "*" + expr : expr;

                std::string result = getNativePodCppName(nativeStruct) + "{";
                for (size_t fieldIndex = 0; fieldIndex < nativeStruct->fieldNames.size(); ++fieldIndex)
                {
                    const std::string wioFieldName = sanitizeCppIdentifier(nativeStruct->fieldNames[fieldIndex]);
                    const std::string fieldExpr = expr + (sourceIsPointer ? "->" : ".") + wioFieldName;
                    result += buildWioToNativePodExpr(fieldExpr, nativeStruct->fieldTypes[fieldIndex], false);
                    if (fieldIndex + 1 < nativeStruct->fieldNames.size())
                        result += ", ";
                }
                result += "}";
                return result;
            };

            std::function<void(const std::string&, const Ref<sema::Type>&, const std::string&, bool)> emitNativePodCopyBack;
            emitNativePodCopyBack = [&](const std::string& destinationExpr,
                                        const Ref<sema::Type>& destinationType,
                                        const std::string& sourceExpr,
                                        bool destinationIsPointer)
            {
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(destinationType);
                if (!nativeStruct)
                    return;

                if (usesNativePodAliasModelForCodegen(nativeStruct))
                {
                    EMIT_TABS();
                    emitLine((destinationIsPointer ? "*" + destinationExpr : destinationExpr) + " = " + sourceExpr + ";");
                    return;
                }

                for (size_t fieldIndex = 0; fieldIndex < nativeStruct->fieldNames.size(); ++fieldIndex)
                {
                    const std::string wioFieldName = sanitizeCppIdentifier(nativeStruct->fieldNames[fieldIndex]);
                    const std::string nativeFieldName = nativeStruct->fieldNames[fieldIndex];
                    const std::string destinationFieldExpr = destinationExpr + (destinationIsPointer ? "->" : ".") + wioFieldName;
                    const std::string sourceFieldExpr = sourceExpr + "." + nativeFieldName;
                    auto nestedNativeStruct = getNativePodComponentStructTypeForCodegen(nativeStruct->fieldTypes[fieldIndex]);
                    if (nestedNativeStruct)
                    {
                        emitNativePodCopyBack(destinationFieldExpr, nativeStruct->fieldTypes[fieldIndex], sourceFieldExpr, false);
                    }
                    else
                    {
                        EMIT_TABS();
                        emitLine(destinationFieldExpr + " = " + sourceFieldExpr + ";");
                    }
                }
            };

            struct NativePreparedArgument
            {
                std::string callExpr;
                std::string preferredCallExpr;
                std::string fallbackCallExpr;
                std::string mutableTargetName;
                Ref<sema::Type> mutableTargetType = nullptr;
                bool mutableTargetIsPointer = false;
                bool usesReferenceDispatch = false;
                std::string signatureType;
                std::string preferredSignatureType;
                std::string fallbackSignatureType;
            };

            std::vector<NativePreparedArgument> preparedArguments;
            preparedArguments.reserve(node.parameters.size() + (isNativeMember ? 1 : 0));
            bool usesNativeReferenceWrappers = false;

            emitLine();
            emitLine("{");
            indent();
            emitLine("try {");
            indent();

            if (isNativeMember)
            {
                NativePreparedArgument selfArgument;
                selfArgument.callExpr = "this";
                selfArgument.preferredCallExpr = "this";
                selfArgument.fallbackCallExpr = "this";
                selfArgument.signatureType = currentClassName_ + "*";
                selfArgument.preferredSignatureType = selfArgument.signatureType;
                selfArgument.fallbackSignatureType = selfArgument.signatureType;
                preparedArguments.push_back(std::move(selfArgument));
            }

            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                const std::string parameterName = sanitizeCppIdentifier(node.parameters[i].name->token.value);
                auto parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : nullptr;
                auto resolvedParameterType = unwrapAliasTypeForCodegen(parameterType);
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(parameterType);
                const bool usesNativePodAliasModel =
                    nativeStruct && usesNativePodAliasModelForCodegen(nativeStruct);

                if (!nativeStruct)
                {
                    if (isDirectStringNativeInteropType(parameterType))
                    {
                        preparedArguments.push_back({
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            "",
                            nullptr,
                            false,
                            false,
                            toCppType(parameterType),
                            toCppType(parameterType),
                            toCppType(parameterType)
                        });
                        continue;
                    }

                    if (shouldUseNativeReferenceWrapper(parameterType))
                    {
                        auto referenceType = resolvedParameterType.AsFast<sema::ReferenceType>();
                        const std::string referredCppType = toCppType(referenceType->referredType);
                        const std::string preferredSignatureType =
                            buildNativeReferenceSignatureType(referredCppType, referenceType->isMutable);
                        const std::string fallbackSignatureType = toCppType(parameterType);
                        preparedArguments.push_back({
                            parameterName,
                            buildNativeReferencePreferredExpr(parameterName, parameterType),
                            parameterName,
                            "",
                            nullptr,
                            false,
                            true,
                            fallbackSignatureType,
                            preferredSignatureType,
                            fallbackSignatureType
                        });
                        usesNativeReferenceWrappers = true;
                        continue;
                    }

                    preparedArguments.push_back({
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        "",
                        nullptr,
                        false,
                        false,
                        toCppType(parameterType),
                        toCppType(parameterType),
                        toCppType(parameterType)
                    });
                    continue;
                }

                if (usesNativePodAliasModel)
                {
                    const bool isReferenceParameter =
                        resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference;
                    const bool isMutableReference =
                        isReferenceParameter && resolvedParameterType.AsFast<sema::ReferenceType>()->isMutable;
                    const bool usesExtensionReceiverDispatch =
                        isReferenceParameter && node.isExtensionMethod && i == 0;
                    const std::string nativeSignatureType = getNativePodCppName(nativeStruct);

                    NativePreparedArgument preparedArgument;
                    preparedArgument.callExpr = isReferenceParameter && !usesExtensionReceiverDispatch
                        ? "*" + parameterName
                        : parameterName;
                    preparedArgument.preferredCallExpr = isReferenceParameter ? "*" + parameterName : parameterName;
                    preparedArgument.fallbackCallExpr = usesExtensionReceiverDispatch
                        ? parameterName
                        : preparedArgument.preferredCallExpr;
                    preparedArgument.mutableTargetName = "";
                    preparedArgument.mutableTargetType = nullptr;
                    preparedArgument.mutableTargetIsPointer = false;
                    preparedArgument.usesReferenceDispatch = usesExtensionReceiverDispatch;
                    preparedArgument.signatureType = isReferenceParameter
                        ? (isMutableReference ? nativeSignatureType + "&" : "const " + nativeSignatureType + "&")
                        : nativeSignatureType;
                    preparedArgument.preferredSignatureType = preparedArgument.signatureType;
                    preparedArgument.fallbackSignatureType = usesExtensionReceiverDispatch
                        ? (isMutableReference ? nativeSignatureType + "*" : "const " + nativeSignatureType + "*")
                        : preparedArgument.signatureType;
                    preparedArguments.push_back(std::move(preparedArgument));
                    usesNativeReferenceWrappers = usesNativeReferenceWrappers || usesExtensionReceiverDispatch;
                    continue;
                }

                const std::string nativeTempName = common::formatString("_wio_native_arg{}", i);
                EMIT_TABS();
                emitLine(getNativePodCppName(nativeStruct) + " " + nativeTempName + " = " + buildWioToNativePodExpr(
                    parameterName,
                    parameterType,
                    resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference
                ) + ";");

                bool isMutableReference = false;
                if (resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference)
                    isMutableReference = resolvedParameterType.AsFast<sema::ReferenceType>()->isMutable;

                NativePreparedArgument preparedArgument;
                preparedArgument.callExpr = nativeTempName;
                preparedArgument.preferredCallExpr = nativeTempName;
                preparedArgument.fallbackCallExpr = nativeTempName;
                preparedArgument.mutableTargetName = isMutableReference ? parameterName : "";
                preparedArgument.mutableTargetType = isMutableReference ? parameterType : nullptr;
                preparedArgument.mutableTargetIsPointer = isMutableReference;
                preparedArgument.signatureType = getNativePodCppName(nativeStruct);
                preparedArgument.preferredSignatureType = preparedArgument.signatureType;
                preparedArgument.fallbackSignatureType = preparedArgument.signatureType;
                preparedArguments.push_back(std::move(preparedArgument));
            }

            const auto nativeReturnStruct = getNativePodComponentStructTypeForCodegen(funcType->returnType);
            const bool returnsNativePodComponent = static_cast<bool>(nativeReturnStruct);
            const bool returnsNativePodAliasComponent =
                nativeReturnStruct && usesNativePodAliasModelForCodegen(nativeReturnStruct);
            const bool emitExplicitNativeTemplateArguments =
                !node.genericParameters.empty() &&
                [&]() -> bool
                {
                    auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName);
                    if (!cppNameArg.has_value())
                        return false;

                    if (funcType && funcType->paramTypes.empty())
                        return true;

                    return cppNameArg->value == "wio::runtime::EnumCount" ||
                           cppNameArg->value == "wio::runtime::EnumName" ||
                           cppNameArg->value == "wio::runtime::EnumValue" ||
                           cppNameArg->value == "wio::runtime::EnumIndex" ||
                           cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                           cppNameArg->value == "wio::runtime::EnumSize" ||
                           cppNameArg->value == "wio::runtime::EnumIsValid" ||
                           cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                           cppNameArg->value == "wio::runtime::EnumFromRaw";
                }();

            auto emitNativeSymbolInvocationTarget = [&]()
            {
                emit(nativeSymbol);
                if (emitExplicitNativeTemplateArguments)
                {
                    emit("<");
                    for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
                    {
                        emit(node.genericParameters[genericIndex]->token.value);
                        if (node.hasGenericParameterPack && genericIndex + 1 == node.genericParameters.size())
                            emit("...");
                        if (genericIndex + 1 < node.genericParameters.size())
                            emit(", ");
                    }
                    emit(">");
                }
            };

            auto emitNativeInvocation = [&](bool preferReferenceDispatch)
            {
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");

                    const auto& preparedArgument = preparedArguments[argumentIndex];
                    if (preferReferenceDispatch && preparedArgument.usesReferenceDispatch)
                        emit(preparedArgument.preferredCallExpr);
                    else if (!preferReferenceDispatch && preparedArgument.usesReferenceDispatch)
                        emit(preparedArgument.fallbackCallExpr);
                    else
                        emit(preparedArgument.callExpr);
                }
                emit(")");
            };

            // Keep backend errors in a native symbol expression anchored to the
            // declaration that introduced that symbol. The exception boundary
            // adds generated wrapper lines, so relying on the function-level
            // directive alone would otherwise report non-existent Wio lines.
            emitSourceDirective(node.location());

            if (usesNativeReferenceWrappers)
            {
                EMIT_TABS();
                emit(common::formatString("auto _wio_native_invoke = [&]("));
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(common::formatString("auto&& _wio_native_arg{}", argumentIndex));
                }
                emit(") -> ");
                emit(funcType->returnType && !funcType->returnType->isVoid()
                         ? (returnsNativePodAliasComponent
                                ? returnType
                                : (returnsNativePodComponent ? getNativePodCppName(nativeReturnStruct) : returnType))
                         : "void");
                emitLine(" {");
                indent();
                EMIT_TABS();
                emit("if constexpr (requires { ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    if (preparedArguments[argumentIndex].usesReferenceDispatch)
                    {
                        emit(common::formatString("*_wio_native_arg{}", argumentIndex));
                    }
                    else
                    {
                        emit(common::formatString(
                            "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                            argumentIndex,
                            argumentIndex
                        ));
                    }
                }
                emit("); })");
                emit("\n");
                EMIT_TABS();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    if (preparedArguments[argumentIndex].usesReferenceDispatch)
                    {
                        emit(common::formatString("*_wio_native_arg{}", argumentIndex));
                    }
                    else
                    {
                        emit(common::formatString(
                            "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                            argumentIndex,
                            argumentIndex
                        ));
                    }
                }
                emit(")");
                emit(";");
                emit("\n");
                dedent();
                EMIT_TABS();
                emitLine("}");
                EMIT_TABS();
                emitLine("else");
                EMIT_TABS();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(common::formatString(
                        "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                        argumentIndex,
                        argumentIndex
                    ));
                }
                emit(")");
                emit(";");
                emit("\n");
                dedent();
                EMIT_TABS();
                emitLine("}");
                dedent();
                EMIT_TABS();
                emitLine("};");

                EMIT_TABS();
                if (returnsNativePodComponent)
                {
                    emit("auto _wio_native_result = _wio_native_invoke(");
                }
                else if (funcType->returnType && !funcType->returnType->isVoid())
                {
                    emit("return _wio_native_invoke(");
                }
                else
                {
                    emit("_wio_native_invoke(");
                }
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(preparedArguments[argumentIndex].callExpr);
                }
                emit(");");
            }
            else
            {
                EMIT_TABS();
                if (returnsNativePodComponent)
                {
                    emit("auto _wio_native_result = ");
                }
                else if (funcType->returnType && !funcType->returnType->isVoid())
                {
                    emit("return ");
                }
                emitNativeInvocation(false);
                emit(";");
            }
            emit("\n");
            emitGeneratedDirective();

            for (const auto& preparedArgument : preparedArguments)
            {
                if (preparedArgument.mutableTargetName.empty())
                    continue;

                emitNativePodCopyBack(
                    preparedArgument.mutableTargetName,
                    preparedArgument.mutableTargetType,
                    preparedArgument.callExpr,
                    preparedArgument.mutableTargetIsPointer
                );
            }

            if (returnsNativePodComponent)
            {
                if (returnsNativePodAliasComponent)
                {
                    EMIT_TABS();
                    emitLine("return _wio_native_result;");
                }
                else
                {
                    const std::string wioReturnTypeName = toCppType(funcType->returnType);
                    EMIT_TABS();
                    emitLine(wioReturnTypeName + " _wio_result{};");
                    emitNativePodCopyBack("_wio_result", funcType->returnType, "_wio_native_result", false);
                    EMIT_TABS();
                    emitLine("return _wio_result;");
                }
            }

            dedent();
            emitLine("}");
            emitLine("catch (const wio::runtime::RuntimeException&)");
            emitLine("{");
            indent();
            emitLine("throw;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& ex)");
            emitLine("{");
            indent();
            emitLine(
                "throw wio::runtime::RuntimeException(\"Native call '" +
                common::wioStringToEscapedCppString(nativeSymbol) +
                "' failed: \" + std::string(ex.what()));"
            );
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine(
                "throw wio::runtime::RuntimeException(\"Native call '" +
                common::wioStringToEscapedCppString(nativeSymbol) +
                "' failed with an unknown exception.\");"
            );
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
        }
        else if (node.body)
        {
            auto emitFunctionBody = [&]()
            {
                if (behavioralProcessors.empty() &&
                    !(node.isAsync && currentClassIsObject_ && node.body->is<BlockStatement>()))
                {
                    node.body->accept(*this);
                    return;
                }

                auto* block = node.body->as<BlockStatement>();
                emitLine("{");
                indent();
                if (node.isAsync && currentClassIsObject_)
                    emitLine("auto _wio_async_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this);");

                for (const auto& processor : behavioralProcessors)
                {
                    emitLine("auto " + processor.variableName + " = wio::runtime::Ref<" +
                             processor.cppTypeName + ">::Create();");
                    if (processor.phase == "finally")
                        emitLine("bool " + processor.finalizedFlagName + " = false;");
                }

                const bool hasFinallyProcessor = std::ranges::any_of(
                    behavioralProcessors,
                    [](const BehavioralProcessorInstance& processor) { return processor.phase == "finally"; });
                const bool hasAroundProcessor = std::ranges::any_of(
                    behavioralProcessors,
                    [](const BehavioralProcessorInstance& processor) { return processor.phase == "around"; });
                if (hasAroundProcessor)
                {
                    emitLine("auto _wio_attribute_core = [&]()" +
                             std::string(currentFunctionReturnType_ && !currentFunctionReturnType_->isVoid()
                                 ? " -> " + toCppType(currentFunctionReturnType_)
                                 : ""));
                    emitLine("{");
                    indent();
                }
                if (hasFinallyProcessor)
                {
                    emitLine("try");
                    emitLine("{");
                    indent();
                }

                for (const auto& processor : behavioralProcessors)
                {
                    if (processor.phase == "pre")
                    {
                        std::string arguments;
                        if (processor.hookMode.starts_with("receiver_any"))
                        {
                            arguments = "wio::runtime::Any::FromObject<" + currentClassName_ +
                                ">(wio::runtime::Ref<" + currentClassName_ + ">(this))";
                        }
                        else if (processor.hookMode.starts_with("receiver_typed"))
                        {
                            arguments = "static_cast<" + toCppType(processor.hookValueType) + ">(this)";
                        }
                        const std::string invocation = processor.variableName + "->" +
                            processor.hookCppName + "(" + arguments + ")";
                        if (processor.hookMode.ends_with("_guard"))
                            emitLine("if (!(" + invocation + ")) " +
                                     std::string(node.isAsync ? "co_return;" : "return;"));
                        else
                            emitLine(invocation + ";");
                    }
                }

                const auto previousPostProcessors = currentBehavioralPostProcessors_;
                const auto previousFinallyProcessors = currentBehavioralFinallyProcessors_;
                currentBehavioralPostProcessors_.clear();
                currentBehavioralFinallyProcessors_.clear();
                for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                {
                    if (processor->phase == "post")
                        currentBehavioralPostProcessors_.push_back(
                            processor->variableName + "->" + processor->hookCppName +
                            (processor->hookMode == "result" ? "({result});" : "();"));
                    else if (processor->phase == "finally")
                    {
                        currentBehavioralFinallyProcessors_.push_back(
                            "if (!" + processor->finalizedFlagName + ") { " + processor->finalizedFlagName +
                            " = true; " + processor->variableName + "->" + processor->hookCppName +
                            (processor->hookMode == "outcome_bool" ? "(true); }" : "(); }"));
                    }
                }

                if (block)
                {
                    for (auto& statement : block->statements)
                        statement->accept(*this);
                }
                else
                {
                    node.body->accept(*this);
                }

                if (currentFunctionReturnType_ && currentFunctionReturnType_->isVoid())
                {
                    for (const std::string& invocation : currentBehavioralPostProcessors_)
                        emitLine(invocation);
                    for (const std::string& invocation : currentBehavioralFinallyProcessors_)
                        emitLine(invocation);
                }

                currentBehavioralPostProcessors_ = previousPostProcessors;
                currentBehavioralFinallyProcessors_ = previousFinallyProcessors;

                if (hasFinallyProcessor)
                {
                    dedent();
                    emitLine("}");
                    emitLine("catch (...)");
                    emitLine("{");
                    indent();
                    for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                    {
                        if (processor->phase == "finally")
                        {
                            emitLine("if (!" + processor->finalizedFlagName + ") { " + processor->finalizedFlagName +
                                     " = true; " + processor->variableName + "->" + processor->hookCppName +
                                     (processor->hookMode == "outcome_bool" ? "(false); }" : "(); }"));
                        }
                    }
                    emitLine("throw;");
                    dedent();
                    emitLine("}");
                }

                if (hasAroundProcessor)
                {
                    dedent();
                    emitLine("};");
                    std::string nextProceed = "_wio_attribute_core";
                    size_t aroundIndex = 0;
                    for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                    {
                        if (processor->phase != "around")
                            continue;
                        const std::string wrapperName = "_wio_attribute_around_" + std::to_string(aroundIndex);
                        const std::string stateName = "_wio_attribute_proceed_state_" + std::to_string(aroundIndex);
                        const bool returnsResult = processor->hookMode == "proceed_result";
                        const std::string aroundResultType = returnsResult
                            ? toCppType(currentFunctionReturnType_)
                            : std::string("void");
                        emitLine("auto " + wrapperName + " = [&]()" +
                                 (returnsResult ? " -> " + aroundResultType : ""));
                        emitLine("{");
                        indent();
                        emitLine("auto " + stateName + " = std::make_shared<std::pair<bool, bool>>(true, false);");
                        emitLine("try");
                        emitLine("{");
                        indent();
                        emitLine(std::string(returnsResult ? "return " : "") +
                                 processor->variableName + "->" + processor->hookCppName +
                                 "(std::function<" + aroundResultType + "()>([&, " + stateName + "]()" +
                                 (returnsResult ? " -> " + aroundResultType : ""));
                        emitLine("{");
                        indent();
                        emitLine("if (!" + stateName + "->first) throw wio::runtime::RuntimeException(\"Attribute Proceed escaped its Around invocation.\");");
                        emitLine("if (" + stateName + "->second) throw wio::runtime::RuntimeException(\"Attribute Proceed may be invoked at most once.\");");
                        emitLine(stateName + "->second = true;");
                        emitLine(std::string(returnsResult ? "return " : "") + nextProceed + "();");
                        dedent();
                        emitLine("}));");
                        dedent();
                        emitLine("}");
                        emitLine("catch (...)");
                        emitLine("{");
                        indent();
                        emitLine(stateName + "->first = false;");
                        emitLine("throw;");
                        dedent();
                        emitLine("}");
                        emitLine(stateName + "->first = false;");
                        dedent();
                        emitLine("};");
                        nextProceed = wrapperName;
                        ++aroundIndex;
                    }
                    emitLine(std::string(currentFunctionReturnType_ && !currentFunctionReturnType_->isVoid()
                        ? "return "
                        : "") + nextProceed + "();");
                }
                dedent();
                emitLine("}");
            };

            const bool catchesResultPropagation = [&]()
            {
                auto resolvedReturnType = unwrapAliasTypeForCodegen(currentFunctionReturnType_);
                if (!resolvedReturnType || resolvedReturnType->kind() != sema::TypeKind::Struct)
                    return false;

                auto structType = resolvedReturnType.AsFast<sema::StructType>();
                return structType &&
                    (structType->name == "Result" || structType->name == "ResultValue") &&
                    structType->scopePath == "std" &&
                    structType->genericArguments.size() == 1;
            }();

            emitLine();

            if (catchesResultPropagation || node.whenCondition)
            {
                emitLine("{");
                indent();
            }

            if (catchesResultPropagation)
            {
                emitLine("try {");
                indent();
            }

            if (node.whenCondition)
            {
                EMIT_TABS();
                emit("if (!(");
                node.whenCondition->accept(*this);
                emit(node.isAsync ? ")) co_return" : ")) return");
                if (node.whenFallback)
                {
                    emit(" ");
                    node.whenFallback->accept(*this);
                }
                emit(";\n\n");

                if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    if (node.isAsync && currentClassIsObject_)
                        emitLine("auto _wio_async_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this);");
                    for (auto& stmt : block->statements)
                        stmt->accept(*this);
                }
                else
                {
                    emitFunctionBody();
                }
            }
            else
            {
                emitFunctionBody();
            }

            if (catchesResultPropagation)
            {
                dedent();
                emitLine("}");
                const std::string propagationReturnType = toCppType(currentFunctionReturnType_);
                emitLine("catch (const decltype(std::declval<" + propagationReturnType + ">()->_WF_ErrorValue())& _wio_result_error)");
                emitLine("{");
                indent();
                emitLine(std::string(node.isAsync ? "co_return " : "return ") + propagationReturnType + "::Create(_wio_result_error);");
                dedent();
                emitLine("}");
            }

            if (catchesResultPropagation || node.whenCondition)
            {
                dedent();
                emitLine("}");
            }
        }
        else
        {
            emitLine(";\n");
        }

        emitDefaultArgumentWrappers();

        if (!isEmittingPrototypes_ && !currentClassName_.empty() && sym &&
            !sym->overriddenSymbols.empty() && node.genericParameters.empty())
        {
            const std::string implementationName = Mangler::mangleFunction(funcName, funcType->paramTypes);
            std::unordered_set<std::string> emittedBridgeNames;
            for (const auto& overriddenWeak : sym->overriddenSymbols)
            {
                auto overridden = overriddenWeak.Lock();
                auto overriddenType = overridden && overridden->type
                    ? overridden->type.AsFast<sema::FunctionType>()
                    : nullptr;
                if (!overriddenType)
                    continue;

                const std::string bridgeName = Mangler::mangleFunction(funcName, overriddenType->paramTypes);
                if (bridgeName == implementationName || !emittedBridgeNames.insert(bridgeName).second)
                    continue;

                emitGeneratedDirective();
                EMIT_TABS();
                emit("virtual " + returnType + " " + bridgeName + "(");
                for (size_t parameterIndex = 0; parameterIndex < node.parameters.size(); ++parameterIndex)
                {
                    auto& parameter = node.parameters[parameterIndex];
                    emit(common::formatString(
                        "{} {}",
                        toCppType(parameter.name->refType.Lock()),
                        sanitizeCppIdentifier(parameter.name->token.value)
                    ));
                    if (parameterIndex + 1 < node.parameters.size())
                        emit(", ");
                }
                emitLine(") override");
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emit(implementationName + "(");
                for (size_t parameterIndex = 0; parameterIndex < node.parameters.size(); ++parameterIndex)
                {
                    emit(sanitizeCppIdentifier(node.parameters[parameterIndex].name->token.value));
                    if (parameterIndex + 1 < node.parameters.size())
                        emit(", ");
                }
                emitLine(");");
                dedent();
                emitLine("}");
            }
        }

        if (emitsExportWrapper && !isEmittingPrototypes_ && currentClassName_.empty() && node.body)
        {
            emitGeneratedDirective();
            std::string internalSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "");
            if (!node.genericParameters.empty() && !instantiationTypeLists.empty())
            {
                const std::string exportBaseSymbol = getExportedCppSymbolName(node);
                const size_t fixedDeclaredParameterCount = getFixedParameterCount(node);
                auto getExportWrapperParameterName = [&](size_t parameterIndex) -> std::string
                {
                    if (parameterIndex < fixedDeclaredParameterCount && parameterIndex < node.parameters.size())
                        return sanitizeCppIdentifier(node.parameters[parameterIndex].name->token.value);

                    if (node.parameters.empty())
                        return common::formatString("arg_{}", parameterIndex);

                    const std::string packBaseName =
                        sanitizeCppIdentifier(node.parameters.back().name->token.value);
                    return common::formatString("{}_{}", packBaseName, parameterIndex - fixedDeclaredParameterCount);
                };

                for (const auto& instantiationTypes : instantiationTypeLists)
                {
                    auto instantiatedFunctionType = instantiateFunctionTypeForCodegen(instantiationTypes);
                    if (!instantiatedFunctionType)
                        continue;

                    emitLine();
                    EMIT_TABS();
                    emit("extern \"C\" WIO_EXPORT " + toCppType(instantiatedFunctionType->returnType) + " " +
                         formatInstantiatedExportSymbolName(exportBaseSymbol, instantiationTypes) + "(");
                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        emit(common::formatString(
                            "{} {}",
                            toCppType(instantiatedFunctionType->paramTypes[i]),
                            getExportWrapperParameterName(i)
                        ));
                        if (i + 1 < instantiatedFunctionType->paramTypes.size()) emit(", ");
                    }
                    emit(")");
                    emit("\n");
                    emitLine("{");
                    indent();
                    EMIT_TABS();

                    if (instantiatedFunctionType->returnType && !instantiatedFunctionType->returnType->isVoid())
                        emit("return ");

                    emit(internalSymbol);
                    emitTemplateSpecializationArguments(instantiationTypes);
                    emit("(");
                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        emit(getExportWrapperParameterName(i));
                        if (i + 1 < instantiatedFunctionType->paramTypes.size()) emit(", ");
                    }
                    emit(");");
                    emit("\n");
                    dedent();
                    emitLine("}");
                }
            }
            else
            {
                std::string exportedSymbol = getExportedCppSymbolName(node);

                emitLine();
                EMIT_TABS();
                emit("extern \"C\" WIO_EXPORT " + returnType + " " + exportedSymbol + "(");
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& param = node.parameters[i];
                    emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), sanitizeCppIdentifier(param.name->token.value)));
                    if (i < node.parameters.size() - 1) emit(", ");
                }
                emit(")");
                emit("\n");
                emitLine("{");
                indent();
                EMIT_TABS();

                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");

                emit(internalSymbol + "(");
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                    if (i < node.parameters.size() - 1) emit(", ");
                }
                emit(");");
                emit("\n");
                dedent();
                emitLine("}");
            }
        }

        if (!isEmittingPrototypes_ && currentClassName_.empty() && !node.genericParameters.empty() && !instantiationTypeLists.empty() && (isNative || isExported))
        {
            emitGeneratedDirective();
            emitLine();
            std::unordered_set<std::string> emittedBackendInstantiations;
            for (const auto& instantiationTypes : instantiationTypeLists)
            {
                if (!emittedBackendInstantiations.insert(getBackendInstantiationEquivalenceKey(instantiationTypes)).second)
                    continue;

                emitExplicitInstantiationDeclaration(instantiationTypes);
            }
        }

        if (node.isAsync && funcName == "Entry" && !isEmittingPrototypes_ &&
            node.genericParameters.empty() && Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!sym || sym->scopePath.empty()))
        {
            emitMain(node);
        }

        currentFunctionReturnType_ = previousFunctionReturnType;
        currentFunctionIsAsync_ = previousFunctionIsAsync;
    }

    void CppGenerator::visit(RealmDeclaration& node)
    {
        emitStatements(node.statements);
    }

    void CppGenerator::visit(InterfaceDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto interfaceType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }
        std::string interfaceName = mangleInterfaceTypeName(interfaceType);
        emitLine(common::formatString("struct {}", interfaceName));
        emitLine("{");
        indent();

        uint64_t typeId = common::fnv1a(interfaceName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual ~{}() = default;\n", interfaceName));
        emitLine("virtual wio::runtime::RefCountedObject* _WF_RuntimeObject() noexcept = 0;");

        for (auto& method : node.methods)
        {
            EMIT_TABS();
            auto sym = method->name->referencedSymbol.Lock();
            auto funcType = sym->type.AsFast<sema::FunctionType>();
            std::string retType = funcType->returnType ? toCppType(funcType->returnType) : "void";
            std::string mangledName = Mangler::mangleFunction(method->name->token.value, funcType->paramTypes);

            emit(common::formatString("virtual {} {}(", retType, mangledName));
            for (size_t i = 0; i < method->parameters.size(); ++i) {
                emit(common::formatString("{} {}", toCppType(method->parameters[i].name->refType.Lock()), sanitizeCppIdentifier(method->parameters[i].name->token.value)));
                if (i < method->parameters.size() - 1) emit(", ");
            }
            emit(") = 0;\n");
        }

        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ExtensionDeclaration& node)
    {
        WIO_UNUSED(node.name);
        WIO_UNUSED(node.targetType);
        const bool previousExtensionMethod = currentExtensionMethod_;
        currentExtensionMethod_ = true;
        for (auto& member : node.members)
        {
            if (member.method && member.method->name->referencedSymbol.Lock())
                member.method->accept(*this);
        }
        currentExtensionMethod_ = previousExtensionMethod;
    }

    void CppGenerator::visit(ComponentDeclaration& node)
        {
        emitSourceDirective(node.location());
        auto componentSym = node.name->referencedSymbol.Lock();
        auto componentType = getStructTypeFromSymbol(componentSym);
        auto enclosingScope = componentSym && componentSym->innerScope ? componentSym->innerScope->getParent().Lock() : nullptr;

        if (componentType && componentType->isExplicitSpecialization &&
            usesNativePodAliasModelForCodegen(componentType))
        {
            return;
        }

        if (componentType && componentType->isExplicitSpecialization && !componentType->isPartialSpecialization)
        {
            EMIT_TABS();
            emitLine("template <>");
        }
        else if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }

        std::string structName = mangleStructTypeName(componentType);
        const std::string declaredClassName = Mangler::mangleStruct(
            componentType ? componentType->name : node.name->token.value,
            componentType ? componentType->scopePath : ""
        );
        if (componentType && usesNativePodAliasModelForCodegen(componentType))
        {
            std::string nativeTypeName = componentType->nativeCppName.empty() ? componentType->name : componentType->nativeCppName;
            if (!node.genericParameters.empty())
            {
                nativeTypeName += "<";
                for (size_t i = 0; i < node.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        nativeTypeName += ", ";

                    nativeTypeName += node.genericParameters[i]->token.value;
                    if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                        nativeTypeName += "...";
                }
                nativeTypeName += ">";
            }

            EMIT_TABS();
            emitLine("using " + structName + " = " + nativeTypeName + ";\n");
            return;
        }

        emit("struct " + structName);

        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");

        auto bases = getBaseInterfaces(node.attributes);
        if (!bases.empty())
        {
            emit(" : ");
            for (size_t i = 0; i < bases.size(); ++i)
            {
                auto baseSym = enclosingScope ? resolveQualifiedSymbol(enclosingScope, bases[i]) : nullptr;
                std::string baseName = mangleNamedType(baseSym);
                if (baseName.empty())
                    baseName = Mangler::mangleInterface(bases[i]);

                emit("public " + baseName);
                if (i < bases.size() - 1) emit(", ");
            }
        }
        emitLine("\n{");
        indent();

        auto trustArgs = getFirstAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = enclosingScope ? resolveQualifiedSymbol(enclosingScope, t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }

        const bool previousClassIsObject = currentClassIsObject_;
        currentClassIsObject_ = false;
        currentClassName_ = declaredClassName;
        AccessModifier currentAccess = AccessModifier::Public;

        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                if (vDecl->mutability == Mutability::Const)
                    continue;
                auto sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), sanitizeCppIdentifier(vDecl->name->token.value));
            }
        }

        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;

        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct")
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();

                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1)
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }

                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor))
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }

            if (member.access != currentAccess && member.access != AccessModifier::None)
            {
                dedent();
                if (member.access == AccessModifier::Public) emitLine("public:");
                else if (member.access == AccessModifier::Private) emitLine("private:");
                else if (member.access == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = member.access;
            }
            member.declaration->accept(*this);
        }

        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);

        auto emitValueInitDefaultCtor = [&]()
        {
            EMIT_TABS();
            emit(currentClassName_ + "()");
            if (!memberVars.empty())
            {
                emit(" : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "()");
                    if (i < memberVars.size() - 1) emit(", ");
                }
            }
            emit(" {}\n");
        };

        // Keep components embeddable inside generated C++ objects even when the Wio
        // surface only exposes member constructors.
        if (!hasEmptyCtor && !hasNoDefaultCtor)
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
                currentAccess = AccessModifier::Public;
            }

            emitValueInitDefaultCtor();
        }

        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }

            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");

            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }

        currentClassName_ = "";
        currentClassIsObject_ = previousClassIsObject;
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ObjectDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto symb = node.name->referencedSymbol.Lock();
        auto objectType = getStructTypeFromSymbol(symb);
        if (objectType && objectType->isExplicitSpecialization && !objectType->isPartialSpecialization)
        {
            EMIT_TABS();
            emitLine("template <>");
        }
        else if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }
        std::string structName = mangleStructTypeName(objectType);
        const std::string declaredClassName = Mangler::mangleStruct(
            objectType ? objectType->name : node.name->token.value,
            objectType ? objectType->scopePath : ""
        );
        emit("struct " + structName);

        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");

        auto globalScope = symb->innerScope->getParent().Lock();

        std::vector<Ref<sema::StructType>> bases;
        if (objectType)
        {
            for (const auto& baseType : objectType->baseTypes)
            {
                auto resolvedBaseType = unwrapAliasType(baseType);
                if (!resolvedBaseType || resolvedBaseType->kind() != sema::TypeKind::Struct)
                    continue;

                auto baseStruct = resolvedBaseType.AsFast<sema::StructType>();
                if (baseStruct->name == "object" && baseStruct->scopePath.empty())
                    continue;

                bases.push_back(baseStruct);
            }
        }

        bool hasBaseObject = false;
        for (const auto& baseType : bases)
        {
            if (baseType && !baseType->isInterface)
            {
                hasBaseObject = true;
                break;
            }
        }

        std::string baseList;

        if (!hasBaseObject)
        {
            emit(" : public wio::runtime::RefCountedObject");
        }

        for (const auto& base : bases)
        {
            if (!baseList.empty()) baseList += ", ";
            baseList += "public " + mangleNamedType(base);
        }

        if (!baseList.empty())
        {
            if (hasBaseObject) emitLine(" : " + baseList);
            else emitLine(", " + baseList);
        }
        emitLine("{");
        indent();

        uint64_t typeId = common::fnv1a(structName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual uint64_t _WF_GetTypeID() const {{ return {}ull; }}", typeId));
        emitLine("virtual wio::runtime::RefCountedObject* _WF_RuntimeObject() noexcept override { return this; }");

        emitLine("virtual bool _WF_IsA(uint64_t id) const override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return true;", typeId));
        for (const auto& base : bases) {
            if (base && base->isInterface) {
                emitLine(common::formatString("if (id == {}::TYPE_ID) return true;", mangleNamedType(base)));
            } else {
                emitLine(common::formatString("if ({}::_WF_IsA(id)) return true;", mangleNamedType(base)));
            }
        }
        emitLine("return false;");
        dedent();
        emitLine("}\n");

        emitLine("virtual void* _WF_CastTo(uint64_t id) override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return this;", typeId));
        for (const auto& base : bases)
        {
            if (base && base->isInterface)
            {
                std::string intf = mangleNamedType(base);
                emitLine(common::formatString("if (id == {}::TYPE_ID) return static_cast<{}*>(this);", intf, intf));
            }
            else
            {
                emitLine(common::formatString("if (void* base_cast = {}::_WF_CastTo(id)) return base_cast;", mangleNamedType(base)));
            }
        }
        emitLine("return nullptr;");
        dedent();
        emitLine("}\n");

        std::string objectRefFriend = structName;
        if (!node.genericParameters.empty() && !(objectType && objectType->isExplicitSpecialization))
        {
            objectRefFriend += "<";
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                objectRefFriend += node.genericParameters[i]->token.value;
                if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                    objectRefFriend += "...";
                if (i + 1 < node.genericParameters.size())
                    objectRefFriend += ", ";
            }
            objectRefFriend += ">";
        }
        emitLine("friend class wio::runtime::Ref<" + objectRefFriend + ">;");
        auto trustArgs = getFirstAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = globalScope ? resolveQualifiedSymbol(globalScope, t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }

        const bool previousClassIsObject = currentClassIsObject_;
        currentClassIsObject_ = true;
        currentClassName_ = declaredClassName;
        AccessModifier currentAccess = AccessModifier::Public;

        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                if (vDecl->mutability == Mutability::Const)
                    continue;
                const auto& sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), sanitizeCppIdentifier(vDecl->name->token.value));
            }
        }

        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;

        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct")
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();

                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1)
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }

                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor))
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }

            AccessModifier targetAccess = (member.access == AccessModifier::None) ? AccessModifier::Private : member.access;

            if (targetAccess != currentAccess)
            {
                dedent();
                if (targetAccess == AccessModifier::Public) emitLine("public:");
                else if (targetAccess == AccessModifier::Private) emitLine("private:");
                else if (targetAccess == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = targetAccess;
            }
            member.declaration->accept(*this);
        }

        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);

        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }

            if (!hasEmptyCtor)
                emitLine(currentClassName_ + "() = default;");

            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");

            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }

        currentClassName_ = "";
        currentClassIsObject_ = previousClassIsObject;
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto flagType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string structName = mangleStructTypeName(flagType);
        emitLine(common::formatString("struct {0} {{ explicit {0}() = default; };\n", structName));
    }

    void CppGenerator::visit(EnumDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto enumType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(enumType);
        std::string underType = "int32_t";

        auto typeArgs = getFirstAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty()) {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        // Wio enum members live in the enum's semantic scope. Keep the C++
        // representation scoped as well so two enums in one realm may reuse
        // natural member names such as `pending` or `closed`.
        emitLine("enum class " + enumName + " : " + underType + "\n{");
        indent();

        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }

            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }

        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagsetDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto flagsetType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(flagsetType);

        if (flagsetType && usesNativePodAliasModelForCodegen(flagsetType))
        {
            std::string nativeTypeName = flagsetType->nativeCppName.empty() ? flagsetType->name : flagsetType->nativeCppName;
            EMIT_TABS();
            emitLine("using " + enumName + " = " + nativeTypeName + ";\n");
            return;
        }

        std::string underType = "uint32_t";

        auto typeArgs = getFirstAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty())
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        emitLine("enum class " + enumName + " : " + underType + "\n{");
        indent();

        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }

            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }

        dedent();
        emitLine("};");

        emitLine(common::formatString("inline constexpr {0} operator|({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) | static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator&({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) & static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator^({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) ^ static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator~({0} a) {{ return static_cast<{0}>(~static_cast<{1}>(a)); }}", enumName, underType));
    }
