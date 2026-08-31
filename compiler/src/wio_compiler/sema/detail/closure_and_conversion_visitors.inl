// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::validateExecutorTransfer(
        FunctionCallExpression& node,
        const Ref<Symbol>& functionSymbol)
    {
        if (!functionSymbol || node.arguments.empty())
            return;

        const bool isRun = functionSymbol->name == "Run";
        const bool isRunBlocking = functionSymbol->name == "RunBlocking";
        const bool isScopeWorker = functionSymbol->name == "SpawnWorker";
        const bool isScopeBlocking = functionSymbol->name == "SpawnBlocking";
        if (!isRun && !isRunBlocking && !isScopeWorker && !isScopeBlocking)
            return;

        bool isExecutorCall = functionSymbol->scopePath == "std_async";
        auto attributes = attributeListsBySymbol_.find(functionSymbol.Get());
        if (!isExecutorCall && attributes != attributeListsBySymbol_.end() && attributes->second)
        {
            if (const Token* cppName = getFirstAttributeArg(*attributes->second, Attribute::CppName))
                isExecutorCall =
                    cppName->value == "wio::runtime::RunBlockingAsync" ||
                    cppName->value == "wio::runtime::RunAsync";
        }
        if (!isExecutorCall)
            return;

        const std::string executorOperation = isScopeWorker
            ? "spawn worker"
            : isScopeBlocking ? "spawn blocking" : functionSymbol->name;

        const LambdaExpression* lambda = node.arguments.front()->as<LambdaExpression>();
        Ref<Symbol> actionSymbol = node.arguments.front()->referencedSymbol.Lock();
        if (!lambda && actionSymbol &&
            (actionSymbol->kind == SymbolKind::Variable || actionSymbol->kind == SymbolKind::Parameter))
        {
            auto declaration = variableDeclarationsBySymbol_.find(actionSymbol.Get());
            if (declaration != variableDeclarationsBySymbol_.end() && declaration->second &&
                declaration->second->initializer)
            {
                lambda = declaration->second->initializer->as<LambdaExpression>();
            }
        }

        if (lambda)
        {
            for (const auto& weakCapture : lambda->capturedSymbols)
            {
                Ref<Symbol> capture = weakCapture.Lock();
                if (!capture || isExecutorTransferSafe(capture->type))
                    continue;
                WIO_LOG_ADD_ERROR(
                    node.arguments.front()->location(),
                    "{} cannot transfer capture '{}' of type '{}' to an async executor. Use an owning transfer-safe value or implement std::async::Send after synchronizing the type.",
                    executorOperation,
                    capture->name,
                    capture->type ? capture->type->toString() : "<unknown>"
                );
            }

            if (lambda->capturesSelf && !isExecutorTransferSafe(currentStructType_))
            {
                WIO_LOG_ADD_ERROR(
                    node.arguments.front()->location(),
                    "{} cannot capture 'self' because '{}' is not executor-transfer-safe. Implement std::async::Send only after the object owns suitable synchronization.",
                    executorOperation,
                    currentStructType_ ? currentStructType_->toString() : "<unknown>"
                );
            }
        }
        else if (!actionSymbol || actionSymbol->kind != SymbolKind::Function)
        {
            WIO_LOG_ADD_ERROR(
                node.arguments.front()->location(),
                "{} requires a direct function or a lambda whose captures can be proven executor-transfer-safe.",
                executorOperation
            );
        }
    }

    void SemanticAnalyzer::visit(LambdaExpression& node)
    {
        Ref<FunctionType> expectedFunctionType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Function)
                expectedFunctionType = expectedType.AsFast<FunctionType>();
        }

        if (expectedFunctionType && expectedFunctionType->paramTypes.size() != node.parameters.size())
            expectedFunctionType = nullptr;

        const bool shouldEnforceExpectedReturnType =
            node.returnType ||
            (expectedFunctionType &&
             expectedFunctionType->returnType &&
             !containsGenericParameterType(expectedFunctionType->returnType));

        node.capturedSymbols.clear();
        node.capturesSelf = false;
        lambdaCaptureContexts_.push_back(LambdaCaptureContext{.node = &node});
        enterScope(ScopeKind::Function);
        Ref<Type> previousFunctionReturnType = currentFunctionReturnType_;
        bool previousFunctionIsAsync = currentFunctionIsAsync_;
        currentFunctionIsAsync_ = false;

        std::vector<Ref<Type>> paramTypes;
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
            if (param.type)
            {
                param.type->accept(*this);
                pType = param.type->refType.Lock();
            }
            else if (expectedFunctionType && i < expectedFunctionType->paramTypes.size())
            {
                pType = expectedFunctionType->paramTypes[i];
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Lambda parameters must have explicit types.");
            }
            paramTypes.push_back(pType);

            Ref<Symbol> paramSym = createSymbol(param.name->token.value, pType, SymbolKind::Variable, param.name->location());
            currentScope_->define(param.name->token.value, paramSym);
            param.name->referencedSymbol = paramSym;
            param.name->refType = pType;
        }

        Ref<Type> retType = Compiler::get().getTypeContext().getVoid();
        if (node.returnType)
        {
            node.returnType->accept(*this);
            retType = node.returnType->refType.Lock();
        }
        else if (shouldEnforceExpectedReturnType && expectedFunctionType)
        {
            retType = expectedFunctionType->returnType;
        }

        currentFunctionReturnType_ = shouldEnforceExpectedReturnType
            ? retType
            : Compiler::get().getTypeContext().getUnknown();

        if (node.body)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            if (node.body->is<ExpressionStatement>() && shouldEnforceExpectedReturnType)
                currentExpectedExpressionType_ = retType;

            node.body->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;

            if (!node.returnType && !shouldEnforceExpectedReturnType)
            {
                if (node.body->is<ExpressionStatement>())
                {
                    retType = node.body->as<ExpressionStatement>()->expression->refType.Lock();
                }
                else if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    for (auto& stmt : block->statements)
                    {
                        if (stmt->is<ReturnStatement>())
                        {
                            auto retStmt = stmt->as<ReturnStatement>();
                            if (retStmt->value)
                            {
                                retType = retStmt->value->refType.Lock();
                            }
                            break;
                        }
                    }
                }
            }
            else if (!node.returnType && shouldEnforceExpectedReturnType && node.body->is<ExpressionStatement>())
            {
                Ref<Type> actualReturnType = node.body->as<ExpressionStatement>()->expression->refType.Lock();
                if (actualReturnType &&
                    !retType->isCompatibleWith(actualReturnType) &&
                    !(retType->isNumeric() && actualReturnType->isNumeric()))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Lambda return type mismatch! Expected '{}', but got '{}'.",
                        retType->toString(),
                        actualReturnType->toString()
                    );
                }
            }
        }

        exitScope();
        auto captureContext = std::move(lambdaCaptureContexts_.back());
        lambdaCaptureContexts_.pop_back();
        node.capturedSymbols.reserve(captureContext.captures.size());
        for (const auto& capture : captureContext.captures)
            node.capturedSymbols.push_back(capture);
        currentFunctionReturnType_ = previousFunctionReturnType;
        currentFunctionIsAsync_ = previousFunctionIsAsync;

        node.refType = Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes);
    }

    void SemanticAnalyzer::visit(RefExpression& node)
    {
        node.operand->accept(*this);

        if (!isAddressableRefOperand(node.operand))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "The 'ref' operator requires an addressable variable, member, or indexed element."
            );
        }

        bool isMut = isMutableAddressableOperand(node.operand);

        node.isMut = isMut;
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(node.operand->refType.Lock(), isMut);
        node.borrowOrigin = classifyBorrowOrigin(node.operand);
    }

    void SemanticAnalyzer::visit(FitExpression& node)
    {
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
        currentExpectedExpressionType_ = nullptr;
        allowContextualNumericLiteralTyping_ = false;
        node.operand->accept(*this);
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
        node.targetType->accept(*this);

        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();

        auto tryResolveFitOperatorOverload = [&]() -> bool
        {
            const auto overloadName = common::getConversionOperatorOverloadName(TokenType::kwFit);
            if (!overloadName.has_value() || !srcType || !destType)
                return false;

            struct OperatorCandidate
            {
                Ref<Symbol> symbol;
                Ref<FunctionType> functionType;
                Ref<StructType> ownerType;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = 0;
            };

            auto deduceBindingsFromFitArgument = [&](const Ref<Type>& expectedType,
                                                     const Ref<Type>& argumentType,
                                                     const NodePtr<Expression>& argumentExpression,
                                                     std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                if (argumentType != readableArgumentType)
                {
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto expectedReferenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    if (expectedReferenceType)
                    {
                        Ref<Type> referredType = expectedReferenceType->referredType;
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression && isAddressableRefOperand(argumentExpression))
                        {
                            Ref<Type> resolvedArgumentType = unwrapAliasType(argumentType);
                            if (resolvedArgumentType && resolvedArgumentType->kind() == TypeKind::Reference)
                                resolvedArgumentType = resolvedArgumentType.AsFast<ReferenceType>()->referredType;

                            if (resolvedArgumentType && deduceGenericBindings(referredType, resolvedArgumentType, bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreFitArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                        const Ref<Type>& rawArgumentType,
                                                        const Ref<Type>& readableArgumentType,
                                                        const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !rawArgumentType)
                    return std::nullopt;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    if (argumentExpression && isAddressableRefOperand(argumentExpression))
                    {
                        Ref<Type> addressableArgumentType = rawArgumentType;
                        Ref<Type> resolvedAddressableType = unwrapAliasType(addressableArgumentType);
                        if (resolvedAddressableType && resolvedAddressableType->kind() == TypeKind::Reference)
                            addressableArgumentType = resolvedAddressableType.AsFast<ReferenceType>()->referredType;

                        if (isAssignmentLikeCompatible(parameterType, addressableArgumentType))
                            return referenceType->isMutable ? 4 : 3;
                    }

                    if (readableArgumentType && isAssignmentLikeCompatible(referenceType->referredType, readableArgumentType))
                        return referenceType->isMutable ? 2 : 1;

                    return std::nullopt;
                }

                if (isAssignmentLikeCompatible(parameterType, rawArgumentType))
                    return 4;
                if (readableArgumentType && isAssignmentLikeCompatible(parameterType, readableArgumentType))
                    return 2;
                return std::nullopt;
            };

            auto scoreFitReturnAgainstTarget = [&](const Ref<Type>& targetType, const Ref<Type>& returnType) -> std::optional<int>
            {
                if (!targetType || !returnType)
                    return std::nullopt;

                if (isExactType(targetType, returnType))
                    return 4;
                if (isAssignmentLikeCompatible(targetType, returnType))
                    return 2;
                return std::nullopt;
            };

            std::vector<OperatorCandidate> candidates;

            auto appendMemberCandidates = [&](const Ref<Type>& candidateReceiverType)
            {
                Ref<Type> currentType = unwrapAliasType(candidateReceiverType);
                while (currentType && currentType->kind() == TypeKind::Reference)
                    currentType = unwrapAliasType(currentType.AsFast<ReferenceType>()->referredType);

                if (!currentType || currentType->kind() != TypeKind::Struct)
                    return;

                Ref<Type> ownerType = nullptr;
                Ref<Symbol> candidateSymbol = findStructMemberInHierarchy(currentType, std::string(*overloadName), &ownerType);
                if (!candidateSymbol)
                    return;

                if (!validateStructMemberAccess(currentStructType_, ownerType, candidateSymbol, node.location()))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                auto ownerStructType = ownerType ? unwrapAliasType(ownerType).AsFast<StructType>() : nullptr;
                if (!ownerStructType)
                    return;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceGenericBindings(candidateFunctionType->returnType, destType, genericBindings))
                            continue;

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                            continue;
                    }

                    auto returnScore = scoreFitReturnAgainstTarget(destType, candidateFunctionType->returnType);
                    if (!returnScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = ownerStructType,
                        .dispatchKind = OperatorDispatchKind::Member,
                        .score = *returnScore + 2
                    });
                }
            };

            appendMemberCandidates(srcType);
            Ref<Type> readableSrcType = getAutoReadableType(srcType);
            if (readableSrcType != srcType)
                appendMemberCandidates(readableSrcType);

            std::unordered_set<const Symbol*> seenFreeSymbols;
            auto appendFreeSymbol = [&](const Ref<Symbol>& candidateSymbol)
            {
                if (!candidateSymbol || seenFreeSymbols.contains(candidateSymbol.Get()))
                    return;

                seenFreeSymbols.insert(candidateSymbol.Get());

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromFitArgument(candidateFunctionType->paramTypes[0], srcType, node.operand, genericBindings) ||
                            !deduceGenericBindings(candidateFunctionType->returnType, destType, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto argumentScore = scoreFitArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        srcType,
                        getAutoReadableType(srcType),
                        node.operand
                    );
                    if (!argumentScore.has_value())
                        continue;

                    auto returnScore = scoreFitReturnAgainstTarget(destType, candidateFunctionType->returnType);
                    if (!returnScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *argumentScore + *returnScore
                    });
                }
            };

            appendFreeSymbol(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);
            if (Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front())
            {
                auto appendAssociatedScopeSymbol = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendFreeSymbol(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeSymbol(srcType);
                appendAssociatedScopeSymbol(readableSrcType);
                appendAssociatedScopeSymbol(destType);
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (!bestCandidate.has_value())
                return false;

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator 'fit' from '{}' to '{}'.",
                    srcType ? srcType->toString() : "<unknown>",
                    destType ? destType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType
                ? bestCandidate->functionType->returnType
                : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveFitOperatorOverload())
            return;

        struct GenericFitConstraintInfo
        {
            bool isKnown = false;
            bool hasApplicableConstraints = false;
            bool isIncompatible = false;
            bool allowsNumeric = false;
            bool allowsObjectLike = false;
        };

        auto resolveGenericFitConstraintInfo = [&](const Ref<Type>& candidateType) -> GenericFitConstraintInfo
        {
            GenericFitConstraintInfo info;

            Ref<Type> resolvedCandidateType = unwrapAliasType(candidateType);
            if (!resolvedCandidateType || resolvedCandidateType->kind() != TypeKind::GenericParameter)
                return info;

            const std::string& genericParameterName = resolvedCandidateType.AsFast<GenericParameterType>()->name;

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

                auto attributeIt = attributeListsBySymbol_.find(activeSymbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return {};

                const size_t genericParameterIndex = static_cast<size_t>(
                    std::distance(activeSymbol->genericParameterNames.begin(), genericParameterIt)
                );
                const auto applyAttributes = getAttributeStatements(*attributeIt->second, Attribute::Apply);
                if (applyAttributes.empty())
                    return {};

                bool sawApplicableConstraint = false;
                bool allConstraintsAreFitCompatible = true;
                auto& typeContext = Compiler::get().getTypeContext();

                for (const auto* applyAttribute : applyAttributes)
                {
                    if (!applyAttribute)
                        continue;

                    if (!hasValidGroupedApplyConstraintShape(*applyAttribute, activeSymbol->genericParameterNames) &&
                        activeSymbol->genericParameterNames.size() != 1 &&
                        applyAttribute->args.size() != activeSymbol->genericParameterNames.size())
                        continue;

                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, activeSymbol->genericParameterNames, genericParameterIndex);
                    if (constraintArguments.empty())
                        continue;

                    sawApplicableConstraint = true;
                    bool attributeAllowsNumeric = false;
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
                                if (predicateTrait->kind == GenericConstraintTraitKind::IsInteger ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsNumeric ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsFloating ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsSigned ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsUnsigned)
                                {
                                    attributeAllowsNumeric = true;
                                    continue;
                                }

                                if (predicateTrait->kind == GenericConstraintTraitKind::IsObject ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsInterface)
                                {
                                    attributeAllowsObjectLike = true;
                                    continue;
                                }

                                attributeIsCompatible = false;
                                break;
                            }

                            Ref<Type> exactType = unwrapAliasType(argument.typeSpecifier->refType.Lock());
                            if (!exactType || exactType->isUnknown())
                            {
                                attributeIsCompatible = false;
                                break;
                            }

                            if (isNumericConstraintType(exactType))
                            {
                                attributeAllowsNumeric = true;
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

                    if (!attributeIsCompatible || (attributeAllowsNumeric && attributeAllowsObjectLike))
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if (!attributeAllowsNumeric && !attributeAllowsObjectLike)
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if ((info.allowsNumeric && attributeAllowsObjectLike) ||
                        (info.allowsObjectLike && attributeAllowsNumeric))
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if (attributeAllowsNumeric)
                        info.allowsNumeric = true;
                    if (attributeAllowsObjectLike)
                        info.allowsObjectLike = true;
                }

                if (!sawApplicableConstraint || !allConstraintsAreFitCompatible)
                {
                    info.hasApplicableConstraints = sawApplicableConstraint;
                    info.isIncompatible = sawApplicableConstraint;
                    return info;
                }

                info.hasApplicableConstraints = true;
                info.isKnown = info.allowsNumeric || info.allowsObjectLike;
                info.isIncompatible = !info.isKnown;
                return info;
            }

            return info;
        };

        if (srcType && destType && !srcType->isUnknown() && !destType->isUnknown())
        {
            Ref<Type> resolvedSrcType = unwrapAliasType(srcType);
            Ref<Type> resolvedDestType = unwrapAliasType(destType);

            if (resolvedSrcType && resolvedDestType && resolvedSrcType->isNumeric() && resolvedDestType->isNumeric())
            {
                node.refType = destType;
                return;
            }

            if (getObjectOrInterfaceStructType(srcType) && getObjectOrInterfaceStructType(destType))
            {
                node.refType = destType;
                return;
            }

            if (isAnyType(resolvedSrcType))
            {
                if (isSupportedAnyCastTargetType(resolvedDestType))
                {
                    node.refType = destType;
                    return;
                }

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The 'fit' operator can only convert 'any' to concrete runtime-storable types or interfaces."
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (resolvedDestType && resolvedDestType->kind() == TypeKind::GenericParameter)
            {
                const GenericFitConstraintInfo constraintInfo = resolveGenericFitConstraintInfo(resolvedDestType);
                const bool sourceIsNumeric = resolvedSrcType && resolvedSrcType->isNumeric();
                const bool sourceIsObjectLike = getObjectOrInterfaceStructType(srcType) != nullptr;
                const bool isNumericFit =
                    sourceIsNumeric &&
                    (!constraintInfo.hasApplicableConstraints || constraintInfo.allowsNumeric);
                const bool isObjectLikeFit =
                    sourceIsObjectLike &&
                    (!constraintInfo.hasApplicableConstraints || constraintInfo.allowsObjectLike);

                if (isNumericFit || isObjectLikeFit)
                {
                    node.refType = destType;
                    return;
                }

                if ((sourceIsNumeric || sourceIsObjectLike) && constraintInfo.isIncompatible)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "The 'fit' target generic parameter '{}' does not allow {} conversions.",
                        resolvedDestType.AsFast<GenericParameterType>()->name,
                        sourceIsNumeric ? "numeric" : "object/interface"
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }

            WIO_LOG_ADD_ERROR(
                node.location(),
                "The 'fit' operator can only be used with numeric types or object/interface casts."
            );
        }

        node.refType = Compiler::get().getTypeContext().getUnknown();
    }

    void SemanticAnalyzer::visit(SelfExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;

        if (!currentStructType_ && !currentExtensionTargetType_) {
            WIO_LOG_ADD_ERROR(node.location(), "'self' can only be used inside a component or object method.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        Ref<Type> selfType = currentExtensionTargetType_ ? currentExtensionTargetType_ : currentStructType_;
        for (auto& context : lambdaCaptureContexts_)
        {
            if (context.node)
                context.node->capturesSelf = true;
        }
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(
            selfType, currentExtensionTargetType_ ? currentExtensionMutableReceiver_ : true);
        node.borrowOrigin = BorrowOrigin::Caller;
    }

    void SemanticAnalyzer::visit(SuperExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;

        if (!currentStructType_ || !currentBaseStructType_)
        {
            WIO_LOG_ADD_ERROR(node.location(), "'super' can only be used inside an object method that has a base class.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(currentBaseStructType_, true);
    }

    void SemanticAnalyzer::visit(RangeExpression& node)
    {
        node.start->accept(*this);
        node.end->accept(*this);

        auto sType = node.start->refType.Lock();
        auto eType = node.end->refType.Lock();

        if (sType && eType && (!allowsNumericSemantics(sType) || !allowsNumericSemantics(eType)))
        {
            WIO_LOG_ADD_ERROR(node.location(), "Range bounds must be numeric types.");
        }
        node.refType = Compiler::get().getTypeContext().getUnknown();
    }

    void SemanticAnalyzer::visit(MatchExpression& node)
    {
        node.value->accept(*this);

        Ref<Type> matchValueType = node.value->refType.Lock();
        Ref<Type> commonReturnType = currentExpectedExpressionType_;
        if (commonReturnType && (commonReturnType->isVoid() || commonReturnType->isUnknown()))
            commonReturnType = nullptr;

        bool allBodiesAreExpressionStatements = true;
        bool hasAssumedCase = false;
        size_t assumedCaseCount = 0;
        std::unordered_set<std::string> seenUnguardedVariants;

        Ref<Type> algebraicType = unwrapAliasType(matchValueType);
        while (algebraicType && algebraicType->kind() == TypeKind::Reference)
            algebraicType = unwrapAliasType(algebraicType.AsFast<ReferenceType>()->referredType);
        Ref<StructType> algebraicStruct = algebraicType && algebraicType->kind() == TypeKind::Struct
            ? algebraicType.AsFast<StructType>()
            : nullptr;
        const bool isOptionMatch = algebraicStruct && algebraicStruct->name == "Option";
        const bool isResultMatch = algebraicStruct && algebraicStruct->name == "Result";
        const bool isEnumMatch = algebraicStruct && algebraicStruct->isEnum;
        Ref<ArrayType> matchedArray = algebraicType && algebraicType->kind() == TypeKind::Array
            ? algebraicType.AsFast<ArrayType>()
            : nullptr;
        bool sawSome = false;
        bool sawNone = false;
        bool sawOk = false;
        bool sawErr = false;
        std::unordered_set<const Symbol*> enumMembers;
        std::unordered_set<const Symbol*> seenUnguardedEnumMembers;
        if (isEnumMatch)
        {
            if (auto enumScope = algebraicStruct->structScope.Lock())
            {
                for (const auto& [name, symbol] : enumScope->getSymbols())
                {
                    WIO_UNUSED(name);
                    if (symbol && symbol->kind == SymbolKind::Variable &&
                        symbol->flags.get_isReadOnly() &&
                        unwrapAliasType(symbol->type) == algebraicStruct)
                    {
                        enumMembers.insert(symbol.Get());
                    }
                }
            }
        }

        auto analyzeMatchGuard = [&](MatchCase& matchCase)
        {
            if (!matchCase.guard)
                return;

            matchCase.guard->accept(*this);
            Ref<Type> guardType = unwrapAliasType(matchCase.guard->refType.Lock());
            Ref<Type> boolType = Compiler::get().getTypeContext().getBool();
            if (guardType && !guardType->isUnknown() &&
                (!guardType->isCompatibleWith(boolType) || !boolType->isCompatibleWith(guardType)))
            {
                WIO_LOG_ADD_ERROR(matchCase.guard->location(), "Match guards must have type 'bool'.");
            }
        };

        for (size_t caseIndex = 0; caseIndex < node.cases.size(); ++caseIndex)
        {
            auto& matchCase = node.cases[caseIndex];
            const bool isVariantPattern = !matchCase.variantName.empty();

            if (matchCase.matchValues.empty() && !isVariantPattern)
            {
                hasAssumedCase = true;
                assumedCaseCount++;

                if (assumedCaseCount > 1)
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Match expressions can only contain one 'assumed' case."
                    );
                }

                if (caseIndex + 1 != node.cases.size())
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "The 'assumed' match case must be the last case."
                    );
                }
            }

            Ref<Type> bindingType = nullptr;
            if (isVariantPattern)
            {
                const std::string& variant = matchCase.variantName;
                const bool isArrayPattern = variant == "__array";
                const bool validOptionVariant = isOptionMatch && (variant == "Some" || variant == "None");
                const bool validResultVariant = isResultMatch && (variant == "Ok" || variant == "Err");
                if (isArrayPattern && !matchedArray)
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Array destructuring patterns require an array value.");
                }
                else if (!isArrayPattern && !validOptionVariant && !validResultVariant)
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Pattern '{}(...)' requires a matching std::Option or std::Result value.",
                        variant);
                }

                const std::string patternIdentity = isArrayPattern
                    ? variant + std::to_string(matchCase.bindings.size())
                    : variant;
                if (seenUnguardedVariants.contains(patternIdentity))
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Unreachable '{}' match pattern because an earlier unguarded case already covers it.",
                        isArrayPattern ? "array-length" : variant);
                }
                else if (!matchCase.guard)
                {
                    seenUnguardedVariants.insert(patternIdentity);
                }

                const size_t expectedBindings =
                    (variant == "Some" || variant == "Ok" || variant == "Err") ? 1 : 0;
                if (!isArrayPattern && matchCase.bindings.size() != expectedBindings)
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Pattern '{}' expects {} binding(s), but got {}.",
                        variant, expectedBindings, matchCase.bindings.size());
                }

                if ((variant == "Some" || variant == "Ok") &&
                    algebraicStruct && !algebraicStruct->genericArguments.empty())
                {
                    bindingType = algebraicStruct->genericArguments.front();
                }
                else if (variant == "Err")
                {
                    if (auto errorSymbol = resolveQualifiedSymbol(currentScope_, "std::ResultError"))
                        bindingType = errorSymbol->type;
                }
                else if (isArrayPattern && matchedArray)
                {
                    bindingType = matchedArray->elementType;
                    if (matchedArray->arrayKind == ArrayType::ArrayKind::Static &&
                        matchedArray->size != matchCase.bindings.size())
                    {
                        WIO_LOG_ADD_ERROR(
                            matchCase.body ? matchCase.body->location() : node.location(),
                            "Static array pattern expects {} binding(s), but got {}.",
                            matchedArray->size,
                            matchCase.bindings.size());
                    }
                }

                sawSome = sawSome || (variant == "Some" && !matchCase.guard);
                sawNone = sawNone || (variant == "None" && !matchCase.guard);
                sawOk = sawOk || (variant == "Ok" && !matchCase.guard);
                sawErr = sawErr || (variant == "Err" && !matchCase.guard);
            }

            for (auto& val : matchCase.matchValues)
            {
                Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
                bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
                currentExpectedExpressionType_ = matchValueType;
                allowContextualNumericLiteralTyping_ = true;
                val->accept(*this);
                currentExpectedExpressionType_ = previousExpectedExpressionType;
                allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

                Ref<Type> caseValueType = val->refType.Lock();
                if (!matchValueType || matchValueType->isUnknown())
                    continue;

                if (val->is<RangeExpression>())
                {
                    if (!allowsNumericSemantics(matchValueType))
                    {
                        WIO_LOG_ADD_ERROR(
                            val->location(),
                            "Range match cases require the matched value to be numeric, but got '{}'.",
                            matchValueType->toString()
                        );
                    }

                    continue;
                }

                if (!caseValueType || caseValueType->isUnknown())
                    continue;

                if (!areMatchTypesCompatible(matchValueType, caseValueType))
                {
                    WIO_LOG_ADD_ERROR(
                        val->location(),
                        "Match case value type '{}' is not compatible with matched value type '{}'.",
                        caseValueType->toString(),
                        matchValueType->toString()
                    );
                }

                if (isEnumMatch)
                {
                    Ref<Symbol> enumMember = val->referencedSymbol.Lock();
                    if (enumMember && enumMembers.contains(enumMember.Get()))
                    {
                        if (seenUnguardedEnumMembers.contains(enumMember.Get()))
                        {
                            WIO_LOG_ADD_ERROR(
                                val->location(),
                                "Unreachable enum match case '{}' because an earlier unguarded case already covers it.",
                                enumMember->name);
                        }
                        else if (!matchCase.guard)
                        {
                            seenUnguardedEnumMembers.insert(enumMember.Get());
                        }
                    }
                }
            }

            if (isVariantPattern)
            {
                enterScope(ScopeKind::Block);
                std::unordered_set<std::string> bindingNames;
                for (auto& binding : matchCase.bindings)
                {
                    if (!bindingNames.insert(binding->token.value).second)
                    {
                        WIO_LOG_ADD_ERROR(
                            binding->location(),
                            "Pattern binding '{}' is duplicated in the same pattern.",
                            binding->token.value);
                    }
                    Ref<Symbol> bindingSymbol = createSymbol(
                        binding->token.value,
                        bindingType ? bindingType : Compiler::get().getTypeContext().getUnknown(),
                        SymbolKind::Variable,
                        binding->location());
                    currentScope_->define(binding->token.value, bindingSymbol);
                    binding->referencedSymbol = bindingSymbol;
                    binding->refType = bindingSymbol->type;
                }

                analyzeMatchGuard(matchCase);

                matchCase.body->accept(*this);
                exitScope();
            }
            else
            {
                if (matchCase.guard && matchCase.matchValues.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.guard->location(),
                        "The final 'assumed' match case cannot have a guard.");
                }
                else
                {
                    analyzeMatchGuard(matchCase);
                }
                matchCase.body->accept(*this);
            }

            if (!matchCase.body->is<ExpressionStatement>())
            {
                allBodiesAreExpressionStatements = false;
            }

            if (matchCase.body->is<ExpressionStatement>())
            {
                auto exprStmt = matchCase.body->as<ExpressionStatement>();
                auto bodyType = exprStmt->expression->refType.Lock();
                if (!bodyType || bodyType->isUnknown())
                    continue;

                if (!commonReturnType)
                {
                    commonReturnType = bodyType;
                    continue;
                }

                if (!areMatchTypesCompatible(commonReturnType, bodyType))
                {
                    WIO_LOG_ADD_ERROR(
                        exprStmt->location(),
                        "All value-producing match cases must return compatible types. Expected '{}', but got '{}'.",
                        commonReturnType->toString(),
                        bodyType->toString()
                    );
                    commonReturnType = Compiler::get().getTypeContext().getUnknown();
                    continue;
                }

                if (!commonReturnType->isCompatibleWith(bodyType) && bodyType->isCompatibleWith(commonReturnType))
                    commonReturnType = bodyType;
            }
        }

        if (allBodiesAreExpressionStatements)
        {
            const bool algebraicExhaustive = (isOptionMatch && sawSome && sawNone) ||
                                             (isResultMatch && sawOk && sawErr);
            const bool enumExhaustive = isEnumMatch && !enumMembers.empty() &&
                                        seenUnguardedEnumMembers.size() == enumMembers.size();
            if (!hasAssumedCase && !algebraicExhaustive && !enumExhaustive)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    isEnumMatch
                        ? "Value-producing enum matches must cover every member or include an 'assumed' fallback case."
                        : "Value-producing match expressions must include an 'assumed' fallback case."
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = commonReturnType ? commonReturnType : Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Compiler::get().getTypeContext().getVoid();
    }
