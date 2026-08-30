// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    SemanticAnalyzer::SemanticAnalyzer() = default;

    SemanticAnalyzer::~SemanticAnalyzer()
    {
        for (auto& sym : symbols_)
        {
            if (sym)
            {
                sym->innerScope = nullptr;
                sym->overloads.clear();
            }
        }

        for (auto& scope : scopes_)
        {
            if (scope)
            {
                scope->clear();
            }
        }
    }

    void SemanticAnalyzer::analyze(const Ref<Program>& program)
    {
        scopes_.clear();
        symbols_.clear();
        functionDeclarationsBySymbol_.clear();
        variableDeclarationsBySymbol_.clear();
        attributeListsBySymbol_.clear();
        exportedCppSymbolLocations_.clear();
        validatedGenericFunctionBodyKeys_.clear();
        validatingGenericFunctionBodyKeys_.clear();
        currentScope_ = nullptr;
        currentExpectedExpressionType_ = nullptr;
        currentNamespacePath_.clear();
        seenModuleApiVersion_ = false;
        seenModuleLoad_ = false;
        seenModuleUpdate_ = false;
        seenModuleUnload_ = false;
        seenModuleSaveState_ = false;
        seenModuleRestoreState_ = false;

        program->accept(*this);
    }

    void SemanticAnalyzer::enterScope(ScopeKind kind)
    {
        auto scope = Ref<Scope>::Create(currentScope_, kind);
        currentScope_ = scope;
        scopes_.push_back(std::move(scope));
    }

    void SemanticAnalyzer::exitScope()
    {
        if (currentScope_)
            currentScope_ = currentScope_->getParent().Lock();
    }

    std::string SemanticAnalyzer::getCurrentNamespacePath() const
    {
        std::string namespacePath;

        for (size_t i = 0; i < currentNamespacePath_.size(); ++i)
        {
            if (i > 0)
                namespacePath += "_";

            namespacePath += currentNamespacePath_[i];
        }

        return namespacePath;
    }

    Ref<Symbol> SemanticAnalyzer::createSymbol(std::string name, Ref<Type> type, SymbolKind kind, common::Location loc, SymbolFlags flags)
    {
        auto symbol = Ref<Symbol>::Create(std::move(name), std::move(type), kind, flags, loc);
        symbol->scopePath = getCurrentNamespacePath();
        symbols_.push_back(symbol);
        if (kind == SymbolKind::Variable || kind == SymbolKind::Parameter)
        {
            for (auto& context : lambdaCaptureContexts_)
                context.localSymbols.insert(symbol.Get());
        }
        return symbol;
    }

    bool SemanticAnalyzer::validateConcreteGenericFunctionBody(
        const FunctionDeclaration& node,
        const Ref<Symbol>& funcSym,
        const Ref<FunctionType>& declaredFunctionType,
        const Ref<StructType>& concreteOwnerType,
        const std::unordered_map<std::string, Ref<Type>>& directBindings,
        const std::unordered_map<std::string, std::vector<Ref<Type>>>& packBindings,
        const std::unordered_map<std::string, std::string>& packAliases)
    {
        if (!funcSym || !declaredFunctionType)
            return true;

        const bool hasFunctionGenericContext = !funcSym->genericParameterNames.empty();
        const bool hasOwnerGenericDeclarations =
            concreteOwnerType &&
            !concreteOwnerType->genericParameterNames.empty();
        const size_t minimumOwnerGenericArgumentCount =
            concreteOwnerType
                ? getMinimumGenericArgumentCount(
                    concreteOwnerType->genericParameterNames,
                    concreteOwnerType->hasGenericParameterPack
                )
                : 0u;
        const bool hasOwnerGenericContext =
            concreteOwnerType &&
            !concreteOwnerType->genericParameterNames.empty() &&
            concreteOwnerType->genericArguments.size() >= minimumOwnerGenericArgumentCount &&
            std::ranges::all_of(concreteOwnerType->genericArguments, [](const Ref<Type>& type)
            {
                return type && !type->isUnknown() && !containsGenericParameterType(type);
            });
        if (!hasFunctionGenericContext && !hasOwnerGenericDeclarations)
            return true;

        GenericBindingSet bindingSet;
        if (hasOwnerGenericContext)
        {
            bindingSet = buildExtendedGenericBindings(
                concreteOwnerType->genericParameterNames,
                concreteOwnerType->hasGenericParameterPack,
                concreteOwnerType->genericArguments
            );
        }

        for (const auto& [name, type] : directBindings)
            bindingSet.directBindings[name] = type;
        for (const auto& [name, types] : packBindings)
            bindingSet.packBindings[name] = types;
        for (const auto& [name, alias] : packAliases)
            bindingSet.packAliases[name] = alias;

        ValidationAnnotationSnapshot annotationSnapshot;
        annotationSnapshot.capture(node.whenCondition.Get());
        annotationSnapshot.capture(node.whenFallback.Get());
        annotationSnapshot.capture(node.body.Get());

        std::optional<std::vector<Ref<Type>>> materializedInstantiation;
        if (hasFunctionGenericContext)
        {
            materializedInstantiation = tryMaterializeConcreteInstantiation(
                funcSym->genericParameterNames,
                funcSym->hasGenericParameterPack,
                bindingSet
            );
            if (!materializedInstantiation.has_value())
                return true;
        }

        std::string validationKey = common::formatString(
            "{}::{}::owner<{}>::func<{}>",
            funcSym.Get(),
            node.name ? node.name->token.value : "<function>",
            hasOwnerGenericContext ? concreteOwnerType->toString() : std::string("<none>"),
            materializedInstantiation.has_value() ? formatDiagnosticTypeList(*materializedInstantiation) : std::string("<none>")
        );
        if (validatedGenericFunctionBodyKeys_.contains(validationKey))
            return true;
        if (validatingGenericFunctionBodyKeys_.contains(validationKey))
            return true;

        validatingGenericFunctionBodyKeys_.insert(validationKey);

        auto finalizeValidation = [&](const bool succeeded)
        {
            validatingGenericFunctionBodyKeys_.erase(validationKey);
            if (succeeded)
                validatedGenericFunctionBodyKeys_.insert(validationKey);
        };

        std::unordered_map<std::string, Ref<Type>> ownerGenericScope;
        if (hasOwnerGenericDeclarations)
        {
            ownerGenericScope.reserve(concreteOwnerType->genericParameterNames.size());
            for (size_t genericIndex = 0; genericIndex < concreteOwnerType->genericParameterNames.size(); ++genericIndex)
            {
                const auto& parameterName = concreteOwnerType->genericParameterNames[genericIndex];
                const bool isGenericParameterPack =
                    concreteOwnerType->hasGenericParameterPack &&
                    genericIndex + 1 == concreteOwnerType->genericParameterNames.size();

                if (isGenericParameterPack)
                {
                    ownerGenericScope.emplace(
                        parameterName,
                        Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    );
                }
                else if (genericIndex < concreteOwnerType->genericArguments.size() &&
                         concreteOwnerType->genericArguments[genericIndex] &&
                         !concreteOwnerType->genericArguments[genericIndex]->isUnknown() &&
                         !containsGenericParameterType(concreteOwnerType->genericArguments[genericIndex]))
                {
                    ownerGenericScope.emplace(parameterName, concreteOwnerType->genericArguments[genericIndex]);
                }
                else
                {
                    ownerGenericScope.emplace(
                        parameterName,
                        Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName)
                    );
                }
            }
        }

        std::unordered_map<std::string, Ref<Type>> functionGenericScope;
        if (hasFunctionGenericContext)
        {
            functionGenericScope.reserve(node.genericParameters.size());
            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();

                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                functionGenericScope.emplace(
                    parameterName,
                    isGenericParameterPack
                        ? parameterType
                        : instantiateGenericType(parameterType, bindingSet)
                );
            }
        }

        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        Ref<Type> previousFunctionReturnType = currentFunctionReturnType_;
        bool previousFunctionIsAsync = currentFunctionIsAsync_;
        Ref<Type> previousCurrentStructType = currentStructType_;
        Ref<Type> previousCurrentBaseStructType = currentBaseStructType_;
        Ref<Symbol> previousFunctionParameterPackSymbol = currentFunctionParameterPackSymbol_;
        Ref<Type> previousFunctionParameterPackType = currentFunctionParameterPackType_;
        Ref<Scope> previousScope = currentScope_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;

        currentExpectedExpressionType_ = nullptr;
        currentFunctionReturnType_ = instantiateGenericType(declaredFunctionType->returnType, bindingSet);
        if (node.isAsync)
        {
            Ref<Type> resolvedTaskType = unwrapAliasType(currentFunctionReturnType_);
            currentFunctionReturnType_ = resolvedTaskType && resolvedTaskType->kind() == TypeKind::AsyncTask
                ? resolvedTaskType.AsFast<AsyncTaskType>()->valueType
                : Compiler::get().getTypeContext().getUnknown();
        }
        currentFunctionIsAsync_ = node.isAsync;
        currentStructType_ = concreteOwnerType;
        currentBaseStructType_ = nullptr;
        if (concreteOwnerType)
        {
            for (const auto& baseType : concreteOwnerType->baseTypes)
            {
                if (!baseType || baseType->kind() != TypeKind::Struct)
                    continue;

                auto structBase = baseType.AsFast<StructType>();
                if (!structBase->isInterface && !(structBase->name == "object" && structBase->scopePath.empty()))
                {
                    currentBaseStructType_ = structBase;
                    break;
                }
            }
        }
        currentFunctionParameterPackSymbol_ = nullptr;
        currentFunctionParameterPackType_ = nullptr;

        if (hasOwnerGenericDeclarations)
            genericTypeParameterScopes_.push_back(ownerGenericScope);
        if (hasFunctionGenericContext)
            genericTypeParameterScopes_.push_back(functionGenericScope);

        Ref<Symbol> ownerConstraintSymbol = nullptr;
        if (hasOwnerGenericDeclarations && !scopes_.empty())
        {
            Ref<Scope> globalScope = scopes_.front();
            const std::string qualifiedOwnerName = concreteOwnerType->scopePath.empty()
                ? concreteOwnerType->name
                : concreteOwnerType->scopePath + "::" + concreteOwnerType->name;
            ownerConstraintSymbol = resolveQualifiedSymbol(globalScope, qualifiedOwnerName);
        }

        if (ownerConstraintSymbol)
            activeGenericConstraintSymbols_.push_back(ownerConstraintSymbol);
        if (hasFunctionGenericContext)
            activeGenericConstraintSymbols_.push_back(funcSym);
        if (funcSym->innerScope)
            currentScope_ = funcSym->innerScope;
        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            Ref<Type> sourceParameterType =
                funcSym->type && funcSym->type->kind() == TypeKind::Function
                    ? funcSym->type.AsFast<FunctionType>()->paramTypes[i]
                    : declaredFunctionType->paramTypes[i];
            Ref<Type> instantiatedParameterType = instantiateGenericType(sourceParameterType, bindingSet);

            if (param.isParameterPack)
            {
                std::string packName;
                if (containsGenericParameterPackType(sourceParameterType, &packName) && !packName.empty())
                {
                    auto& typeContext = Compiler::get().getTypeContext();
                    if (auto aliasIt = bindingSet.packAliases.find(packName); aliasIt != bindingSet.packAliases.end())
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(aliasIt->second);
                    }
                    else if (auto packIt = bindingSet.packBindings.find(packName); packIt != bindingSet.packBindings.end())
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(packName, packIt->second);
                    }
                    else
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(packName);
                    }
                }
            }

            SymbolFlags parameterFlags = SymbolFlags::createAllFalse();
            if (param.isParameterPack)
                parameterFlags.set_isParameterPack(true);

            Ref<Symbol> parameterSymbol = createSymbol(
                param.name->token.value,
                instantiatedParameterType,
                SymbolKind::Parameter,
                param.name->location(),
                parameterFlags
            );
            currentScope_->define(param.name->token.value, parameterSymbol);

            if (param.isParameterPack)
            {
                currentFunctionParameterPackSymbol_ = parameterSymbol;
                currentFunctionParameterPackType_ = instantiatedParameterType;
            }
        }

        const int32_t previousErrorCount = Logger::get().getErrorCount();

        if (node.whenCondition)
        {
            node.whenCondition->accept(*this);

            if (auto conditionType = node.whenCondition->refType.Lock();
                !(conditionType == Compiler::get().getTypeContext().getBool() ||
                  allowsNumericSemantics(conditionType) ||
                  (conditionType && (conditionType->kind() == TypeKind::Reference || conditionType->kind() == TypeKind::Null))))
            {
                WIO_LOG_ADD_ERROR(
                    node.whenCondition->location(),
                    "When guard condition must be a boolean, numeric, or reference type. Got: {}",
                    conditionType->toString()
                );
            }

            if (node.whenFallback)
            {
                currentExpectedExpressionType_ = currentFunctionReturnType_;
                allowContextualNumericLiteralTyping_ = true;
                node.whenFallback->accept(*this);
                currentExpectedExpressionType_ = nullptr;
                allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

                Ref<Type> fallbackType = node.whenFallback->refType.Lock();
                if (currentFunctionReturnType_ &&
                    !currentFunctionReturnType_->isUnknown() &&
                    fallbackType &&
                    !fallbackType->isUnknown() &&
                    !isAssignmentLikeCompatible(currentFunctionReturnType_, fallbackType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.whenFallback->location(),
                        "When guard fallback type mismatch! Expected '{}', but got '{}'.",
                        currentFunctionReturnType_->toString(),
                        fallbackType->toString()
                    );
                }
            }
            else if (currentFunctionReturnType_ != Compiler::get().getTypeContext().getVoid())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Functions with a return value must provide an 'else' fallback for 'when' guards.");
            }
        }

        if (node.body)
            node.body->accept(*this);

        const bool requiresReturnValue = currentFunctionReturnType_ &&
                                         !currentFunctionReturnType_->isUnknown() &&
                                         !currentFunctionReturnType_->isVoid();
        const bool allPathsReturn = statementDefinitelyReturns(node.body);

        if (node.body && requiresReturnValue && !allPathsReturn)
        {
            WIO_LOG_ADD_ERROR(
                node.name ? node.name->location() : node.location(),
                "Non-void function '{}' must return a value on all control-flow paths.",
                node.name ? node.name->token.value : "<function>"
            );
        }

        const bool succeeded = Logger::get().getErrorCount() == previousErrorCount;

        exitScope();
        if (hasFunctionGenericContext)
            activeGenericConstraintSymbols_.pop_back();
        if (ownerConstraintSymbol)
            activeGenericConstraintSymbols_.pop_back();
        if (hasFunctionGenericContext)
            genericTypeParameterScopes_.pop_back();
        if (hasOwnerGenericContext)
            genericTypeParameterScopes_.pop_back();
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        currentFunctionReturnType_ = previousFunctionReturnType;
        currentFunctionIsAsync_ = previousFunctionIsAsync;
        currentStructType_ = previousCurrentStructType;
        currentBaseStructType_ = previousCurrentBaseStructType;
        currentFunctionParameterPackSymbol_ = previousFunctionParameterPackSymbol;
        currentFunctionParameterPackType_ = previousFunctionParameterPackType;
        currentScope_ = previousScope;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
        annotationSnapshot.restore();

        finalizeValidation(succeeded);
        return succeeded;
    }

    SemanticAnalyzer::GenericConstraintCapabilities SemanticAnalyzer::resolveGenericConstraintCapabilities(const Ref<Type>& type) const
    {
        GenericConstraintCapabilities info;

        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType || resolvedType->kind() != TypeKind::GenericParameter)
            return info;

        const std::string& genericParameterName = resolvedType.AsFast<GenericParameterType>()->name;

        for (auto symbolIt = activeGenericConstraintSymbols_.rbegin();
             symbolIt != activeGenericConstraintSymbols_.rend();
             ++symbolIt)
        {
            const Ref<Symbol>& activeSymbol = *symbolIt;
            if (!activeSymbol)
                continue;

            auto genericParameterIt = std::ranges::find(activeSymbol->genericParameterNames, genericParameterName);
            if (genericParameterIt == activeSymbol->genericParameterNames.end())
                continue;

            const size_t genericParameterIndex = static_cast<size_t>(std::distance(activeSymbol->genericParameterNames.begin(), genericParameterIt));

            auto attributeIt = attributeListsBySymbol_.find(activeSymbol.Get());
            if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                return info;

            std::vector<const AttributeStatement*> applyAttributes;
            for (const auto& attributeNode : *attributeIt->second)
            {
                if (!attributeNode || attributeNode->attribute != Attribute::Apply)
                    continue;

                applyAttributes.push_back(attributeNode.Get());
            }

            if (applyAttributes.empty())
                return info;

            bool sawApplicableConstraint = false;
            bool allConstraintsCompatible = true;
            auto& typeContext = Compiler::get().getTypeContext();

            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (!hasValidGroupedApplyConstraintShape(*applyAttribute, activeSymbol->genericParameterNames) &&
                    activeSymbol->genericParameterNames.size() != 1 &&
                    applyAttribute->args.size() != activeSymbol->genericParameterNames.size())
                {
                    continue;
                }

                const auto constraintArguments =
                    getApplyConstraintArguments(*applyAttribute, activeSymbol->genericParameterNames, genericParameterIndex);
                if (constraintArguments.empty())
                    continue;

                sawApplicableConstraint = true;
                bool attributeAllowsInteger = false;
                bool attributeAllowsNumeric = false;
                bool attributeAllowsEnum = false;
                bool attributeAllowsFlagset = false;
                bool attributeAllowsObjectLike = false;
                bool attributeIsCompatible = true;

                for (const auto& argument : constraintArguments)
                {
                    if (argument.typeSpecifier)
                    {
                        if (const auto* predicateTrait =
                                findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value,
                                                                     argument.typeSpecifier->refType.Lock()))
                        {
                            switch (predicateTrait->kind)
                            {
                            case GenericConstraintTraitKind::IsInteger:
                                attributeAllowsInteger = true;
                                attributeAllowsNumeric = true;
                                continue;
                            case GenericConstraintTraitKind::IsNumeric:
                            case GenericConstraintTraitKind::IsFloating:
                            case GenericConstraintTraitKind::IsSigned:
                            case GenericConstraintTraitKind::IsUnsigned:
                                attributeAllowsNumeric = true;
                                continue;
                            case GenericConstraintTraitKind::IsEnum:
                                attributeAllowsEnum = true;
                                continue;
                            case GenericConstraintTraitKind::IsFlagset:
                                attributeAllowsFlagset = true;
                                continue;
                            case GenericConstraintTraitKind::IsObject:
                            case GenericConstraintTraitKind::IsInterface:
                                attributeAllowsObjectLike = true;
                                continue;
                            case GenericConstraintTraitKind::IsComponent:
                            case GenericConstraintTraitKind::IsArray:
                            case GenericConstraintTraitKind::IsReference:
                                attributeIsCompatible = false;
                                break;
                            }
                        }

                        Ref<Type> exactType = unwrapAliasType(argument.typeSpecifier->refType.Lock());
                        if (!exactType || exactType->isUnknown() || containsGenericParameterType(exactType))
                        {
                            attributeIsCompatible = false;
                            break;
                        }

                        if (isIntegerConstraintType(exactType))
                        {
                            attributeAllowsInteger = true;
                            attributeAllowsNumeric = true;
                            continue;
                        }

                        if (isNumericConstraintType(exactType))
                        {
                            attributeAllowsNumeric = true;
                            continue;
                        }

                        if (isEnumConstraintType(exactType))
                        {
                            attributeAllowsEnum = true;
                            continue;
                        }

                        if (isFlagsetConstraintType(exactType))
                        {
                            attributeAllowsFlagset = true;
                            continue;
                        }

                        if (getObjectOrInterfaceStructType(exactType) || isExactType(exactType, typeContext.getObject()))
                        {
                            attributeAllowsObjectLike = true;
                            continue;
                        }
                    }

                    attributeIsCompatible = false;
                    break;
                }

                const bool attributeAllowsAny =
                    attributeAllowsInteger ||
                    attributeAllowsNumeric ||
                    attributeAllowsEnum ||
                    attributeAllowsFlagset ||
                    attributeAllowsObjectLike;

                const bool hasConflictingCategories =
                    (attributeAllowsObjectLike &&
                     (attributeAllowsInteger || attributeAllowsNumeric || attributeAllowsEnum || attributeAllowsFlagset)) ||
                    (attributeAllowsEnum && attributeAllowsFlagset);

                if (!attributeIsCompatible || !attributeAllowsAny || hasConflictingCategories)
                {
                    allConstraintsCompatible = false;
                    break;
                }

                if ((info.allowsObjectLike &&
                     (attributeAllowsInteger || attributeAllowsNumeric || attributeAllowsEnum || attributeAllowsFlagset)) ||
                    (attributeAllowsObjectLike &&
                     (info.allowsInteger || info.allowsNumeric || info.allowsEnum || info.allowsFlagset)) ||
                    (info.allowsEnum && attributeAllowsFlagset) ||
                    (info.allowsFlagset && attributeAllowsEnum))
                {
                    allConstraintsCompatible = false;
                    break;
                }

                if (attributeAllowsInteger)
                    info.allowsInteger = true;
                if (attributeAllowsNumeric)
                    info.allowsNumeric = true;
                if (attributeAllowsEnum)
                    info.allowsEnum = true;
                if (attributeAllowsFlagset)
                    info.allowsFlagset = true;
                if (attributeAllowsObjectLike)
                    info.allowsObjectLike = true;
            }

            if (!sawApplicableConstraint || !allConstraintsCompatible)
            {
                info.hasApplicableConstraints = sawApplicableConstraint;
                info.isIncompatible = sawApplicableConstraint;
                return info;
            }

            info.hasApplicableConstraints = true;
            info.isKnown =
                info.allowsInteger ||
                info.allowsNumeric ||
                info.allowsEnum ||
                info.allowsFlagset ||
                info.allowsObjectLike;
            info.isIncompatible = !info.isKnown;
            return info;
        }

        return info;
    }

    bool SemanticAnalyzer::allowsNumericSemantics(const Ref<Type>& type) const
    {
        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType)
            return false;

        if (resolvedType->isNumeric())
            return true;

        const GenericConstraintCapabilities capabilities = resolveGenericConstraintCapabilities(resolvedType);
        return capabilities.allowsNumeric || capabilities.allowsInteger;
    }

    bool SemanticAnalyzer::allowsIntegerSemantics(const Ref<Type>& type) const
    {
        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType)
            return false;

        if (isIntegralLikeType(resolvedType))
            return true;

        const GenericConstraintCapabilities capabilities = resolveGenericConstraintCapabilities(resolvedType);
        return capabilities.allowsInteger;
    }
