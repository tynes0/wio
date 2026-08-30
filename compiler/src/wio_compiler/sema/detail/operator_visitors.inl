// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(BinaryExpression& node)
    {
        node.left->accept(*this);
        const auto previousNarrowing = nonNullNarrowedSymbols_;
        const bool isShortCircuitAnd =
            node.op.type == TokenType::opLogicalAnd || node.op.type == TokenType::kwAnd;
        const bool isShortCircuitOr =
            node.op.type == TokenType::opLogicalOr || node.op.type == TokenType::kwOr;
        if (isShortCircuitAnd || isShortCircuitOr)
        {
            auto comparison = node.left->as<BinaryExpression>();
            if (comparison &&
                (comparison->op.type == TokenType::opEqual || comparison->op.type == TokenType::opNotEqual))
            {
                const bool leftIsNull = comparison->left->is<NullExpression>();
                const bool rightIsNull = comparison->right->is<NullExpression>();
                if (leftIsNull != rightIsNull)
                {
                    auto candidate = leftIsNull ? comparison->right : comparison->left;
                    auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
                    Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
                    const bool trueMeansNonNull = comparison->op.type == TokenType::opNotEqual;
                    const bool rightExecutesOnTrue = isShortCircuitAnd;
                    if (symbol && symbolType && symbolType->kind() == TypeKind::Nullable &&
                        trueMeansNonNull == rightExecutesOnTrue)
                    {
                        nonNullNarrowedSymbols_.insert(symbol.Get());
                    }
                }
            }
        }
        node.right->accept(*this);
        nonNullNarrowedSymbols_ = previousNarrowing;

        const Ref<Type> initialLeftType = node.left->refType.Lock();
        const Ref<Type> initialRightType = node.right->refType.Lock();
        if (!initialLeftType || !initialRightType ||
            initialLeftType->isPoisoned() || initialRightType->isPoisoned())
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.type == TokenType::kwIn && node.right->is<RangeExpression>())
        {
            auto leftType = node.left->refType.Lock();
            if (leftType && !allowsNumericSemantics(leftType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "The left operand of 'in' operator must be numeric when used with a range.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
            return;
        }
        if (node.op.type == TokenType::kwIs)
        {
            Ref<Type> lhsType = getAutoReadableType(node.left->refType.Lock());
            Ref<Type> rhsType = node.right->refType.Lock();

            if (isAnyType(lhsType))
            {
                if (!isSupportedAnyCastTargetType(rhsType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.right->location(),
                        "The right side of the 'is' operator must be a concrete runtime-storable type or interface when the left side is 'any'."
                    );
                }

                node.refType = Compiler::get().getTypeContext().getBool();
                return;
            }

            auto typeSym = node.right->referencedSymbol.Lock();
            Ref<StructType> targetStruct = (typeSym && typeSym->kind == SymbolKind::Struct)
                ? getObjectOrInterfaceStructType(typeSym->type)
                : nullptr;

            if (!targetStruct)
            {
                WIO_LOG_ADD_ERROR(node.right->location(), "The right side of the 'is' operator must be an object or interface type.");
            }

            if (!getObjectOrInterfaceStructType(node.left->refType.Lock()))
            {
                const std::string actualType = node.left->refType.Lock()
                    ? node.left->refType.Lock()->toString()
                    : "<unknown>";
                WIO_LOG_ADD_ERROR(
                    node.left->location(),
                    "The left side of the 'is' operator must be an object or interface value/reference. Got '{}'.",
                    actualType
                );
            }

            node.refType = Compiler::get().getTypeContext().getBool();
            return;
        }

        auto tryResolveBinaryOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getBinaryOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> lhsType = node.left->refType.Lock();
            Ref<Type> rhsType = node.right->refType.Lock();
            Ref<Type> readableLhsType = getAutoReadableType(lhsType);
            Ref<Type> readableRhsType = getAutoReadableType(rhsType);

            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                Ref<Type> ownerType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto deduceBindingsFromOperatorArgument = [&](const Ref<Type>& expectedType,
                                                          const Ref<Type>& argumentType,
                                                          const NodePtr<Expression>& argumentExpression,
                                                          std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                if (shouldAutoReadReferenceType(argumentType))
                {
                    Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    Ref<Type> referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (deduceGenericBindings(referredType, unwrapAliasType(argumentType), bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreOperatorArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                            const Ref<Type>& argumentType,
                                                            const Ref<Type>& readableArgumentType,
                                                            const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 880 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 840 : 820;
                        }

                        if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                            return 80;

                        if (isAssignmentLikeCompatible(referredType, argumentType))
                            return 75;
                    }
                }

                Ref<Type> readableParameterType = getAutoReadableType(parameterType);
                if (readableParameterType && readableArgumentType && isExactType(readableArgumentType, readableParameterType))
                    return 850;
                if (readableParameterType && readableArgumentType && isAssignmentLikeCompatible(readableParameterType, readableArgumentType))
                    return 70;

                return std::nullopt;
            };

            auto collectFreeOperatorCandidates = [&]() -> std::vector<Ref<Symbol>>
            {
                std::vector<Ref<Symbol>> collected;
                std::unordered_set<const Symbol*> seen;

                auto appendCandidate = [&](const Ref<Symbol>& symbol)
                {
                    if (!symbol || seen.contains(symbol.Get()))
                        return;

                    seen.insert(symbol.Get());
                    collected.push_back(symbol);
                };

                appendCandidate(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);

                Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front();
                auto appendAssociatedScopeCandidate = [&](const Ref<Type>& type)
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
                    appendCandidate(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeCandidate(lhsType);
                appendAssociatedScopeCandidate(rhsType);
                appendAssociatedScopeCandidate(readableLhsType);
                appendAssociatedScopeCandidate(readableRhsType);

                return collected;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableLhsType ? readableLhsType : lhsType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        std::unordered_map<std::string, Ref<Type>> genericBindings;
                        if (!candidateSymbol->genericParameterNames.empty())
                        {
                            auto declaredFunctionType = candidateType.AsFast<FunctionType>();
                            if (!declaredFunctionType || declaredFunctionType->paramTypes.size() != 1)
                                continue;

                            if (!deduceBindingsFromOperatorArgument(
                                    declaredFunctionType->paramTypes[0],
                                    rhsType,
                                    node.right,
                                    genericBindings))
                            {
                                continue;
                            }

                            candidateType = instantiateGenericType(candidateType, genericBindings);
                        }

                        auto candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;

                        auto score = scoreOperatorArgumentAgainstParameter(
                            candidateFunctionType->paramTypes[0],
                            rhsType,
                            readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                            node.right
                        );
                        if (!score.has_value())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .ownerType = ownerType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = *score + 2
                        });
                    }
                }
            }

            for (const auto& candidateSymbol : collectFreeOperatorCandidates())
            {
                if (!candidateSymbol)
                    continue;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    continue;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromOperatorArgument(candidateFunctionType->paramTypes[0], lhsType, node.left, genericBindings) ||
                            !deduceBindingsFromOperatorArgument(candidateFunctionType->paramTypes[1], rhsType, node.right, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto lhsScore = scoreOperatorArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        lhsType,
                        readableLhsType ? readableLhsType : getAutoReadableType(lhsType),
                        node.left
                    );
                    if (!lhsScore.has_value())
                        continue;

                    auto rhsScore = scoreOperatorArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        rhsType,
                        readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                        node.right
                    );
                    if (!rhsScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *lhsScore + *rhsScore
                    });
                }
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
                    "Ambiguous overload for operator '{}' with operand types '{}' and '{}'.",
                    node.op.value,
                    lhsType ? lhsType->toString() : "<unknown>",
                    rhsType ? rhsType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveBinaryOperatorOverload())
            return;

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        Ref<Type> resolvedLhsType = unwrapAliasType(lhsType);
        Ref<Type> resolvedRhsType = unwrapAliasType(rhsType);
        if (resolvedLhsType && resolvedRhsType &&
            resolvedLhsType->kind() == TypeKind::Reference &&
            resolvedRhsType->kind() == TypeKind::Reference &&
            classifyBorrowOrigin(node.right) == BorrowOrigin::Temporary)
        {
            WIO_LOG_ADD_ERROR(
                node.right->location(),
                "Cannot assign a reference borrowed from a temporary value; the borrow would outlive its owner."
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        Ref<Type> readableLhsType = getAutoReadableType(lhsType);
        Ref<Type> readableRhsType = getAutoReadableType(rhsType);

        if (!lhsType || !rhsType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        const Ref<Type> semanticLhsType = readableLhsType ? readableLhsType : lhsType;
        const Ref<Type> semanticRhsType = readableRhsType ? readableRhsType : rhsType;
        const Ref<Type> commonNumericType = getCommonNumericType(semanticLhsType, semanticRhsType);

        const auto primitiveName = [](const Ref<Type>& type) -> std::string_view
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->kind() == TypeKind::Primitive
                ? std::string_view(resolved.AsFast<PrimitiveType>()->name)
                : std::string_view{};
        };
        const std::string_view lhsPrimitive = primitiveName(semanticLhsType);
        const std::string_view rhsPrimitive = primitiveName(semanticRhsType);
        const bool lhsTextual = lhsPrimitive == "string" || lhsPrimitive == "text";
        const bool rhsTextual = rhsPrimitive == "string" || rhsPrimitive == "text";

        if (lhsTextual && rhsTextual && lhsPrimitive != rhsPrimitive &&
            node.op.type != TokenType::opAssign)
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Mixed 'string' and 'text' operations require an explicit conversion."
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (lhsTextual && rhsTextual && lhsPrimitive == rhsPrimitive)
        {
            const bool supported =
                node.op.type == TokenType::opPlus ||
                node.op.type == TokenType::opAssign ||
                node.op.type == TokenType::opPlusAssign ||
                node.op.type == TokenType::opEqual ||
                node.op.type == TokenType::opNotEqual ||
                node.op.type == TokenType::opLess ||
                node.op.type == TokenType::opLessEqual ||
                node.op.type == TokenType::opGreater ||
                node.op.type == TokenType::opGreaterEqual;
            if (!supported)
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Textual values do not support operator '{}'.",
                    node.op.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        bool isCompatible = lhsType->isCompatibleWith(rhsType);
        if (!node.op.isAssignment() && commonNumericType)
            isCompatible = true;
        if (!isCompatible && readableLhsType && readableRhsType &&
            (shouldAutoReadReferenceType(lhsType) || shouldAutoReadReferenceType(rhsType)))
        {
            isCompatible = readableLhsType->isCompatibleWith(readableRhsType);
        }

        const bool isEqualityComparison =
            node.op.type == TokenType::opEqual || node.op.type == TokenType::opNotEqual;
        const bool isOrderingComparison =
            node.op.type == TokenType::opLess || node.op.type == TokenType::opLessEqual ||
            node.op.type == TokenType::opGreater || node.op.type == TokenType::opGreaterEqual;
        const bool isAnyComparison = isEqualityComparison || isOrderingComparison;

        if (!isCompatible && isAnyComparison)
        {
            isCompatible = rhsType->isCompatibleWith(lhsType);
            if (!isCompatible && readableLhsType && readableRhsType &&
                (shouldAutoReadReferenceType(lhsType) || shouldAutoReadReferenceType(rhsType)))
            {
                isCompatible = readableRhsType->isCompatibleWith(readableLhsType);
            }
        }

        if (!isCompatible)
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Type mismatch in binary operation '{}': Cannot operate on '{}' and '{}'.",
                node.op.value,
                lhsType->toString(),
                rhsType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        const bool requiresIntegerOperands =
            node.op.type == TokenType::opPercent ||
            node.op.type == TokenType::opBitAnd ||
            node.op.type == TokenType::opBitOr ||
            node.op.type == TokenType::opBitXor ||
            node.op.type == TokenType::opShiftLeft ||
            node.op.type == TokenType::opShiftRight;
        if (requiresIntegerOperands && commonNumericType &&
            (!isIntegralLikeType(semanticLhsType) || !isIntegralLikeType(semanticRhsType)))
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Operator '{}' requires integer operands, but got '{}' and '{}'.",
                node.op.value,
                semanticLhsType->toString(),
                semanticRhsType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (isAnyType(semanticLhsType) || isAnyType(semanticRhsType))
        {
            const bool isAssignment = node.op.type == TokenType::opAssign;
            const bool comparesWithNull =
                isEqualityComparison &&
                ((unwrapAliasType(lhsType) && unwrapAliasType(lhsType)->kind() == TypeKind::Null) ||
                 (unwrapAliasType(rhsType) && unwrapAliasType(rhsType)->kind() == TypeKind::Null));

            if (!isAssignment && !comparesWithNull)
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null. Operator '{}' is not supported.",
                    node.op.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        if (isOpaqueType(semanticLhsType) || isOpaqueType(semanticRhsType))
        {
            const bool isAssignment = node.op.type == TokenType::opAssign;
            if (!isAssignment && !isEqualityComparison)
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Opaque values support only assignment and equality comparisons. Operator '{}' is not supported.",
                    node.op.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        if (isAnyComparison ||
            node.op.type == TokenType::opLogicalAnd ||
            node.op.type == TokenType::opLogicalOr ||
            node.op.type == TokenType::kwAnd ||
            node.op.type == TokenType::kwOr ||
            node.op.type == TokenType::kwIn)
        {
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else
        {
            const bool producesCommonNumericType =
                node.op.type == TokenType::opPlus ||
                node.op.type == TokenType::opMinus ||
                node.op.type == TokenType::opStar ||
                node.op.type == TokenType::opSlash ||
                node.op.type == TokenType::opPercent ||
                node.op.type == TokenType::opBitAnd ||
                node.op.type == TokenType::opBitOr ||
                node.op.type == TokenType::opBitXor;
            node.refType = producesCommonNumericType && commonNumericType
                ? commonNumericType
                : semanticLhsType;
        }
    }

    void SemanticAnalyzer::visit(ConditionalExpression& node)
    {
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;

        currentExpectedExpressionType_ = Compiler::get().getTypeContext().getBool();
        allowContextualNumericLiteralTyping_ = false;
        node.condition->accept(*this);

        Ref<Type> conditionType = getAutoReadableType(node.condition->refType.Lock());
        if (conditionType &&
            !conditionType->isUnknown() &&
            conditionType != Compiler::get().getTypeContext().getBool())
        {
            WIO_LOG_ADD_ERROR(
                node.condition->location(),
                "Conditional operator condition must be bool, but got '{}'.",
                conditionType->toString()
            );
        }

        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = true;
        node.whenTrue->accept(*this);
        node.whenFalse->accept(*this);

        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

        Ref<Type> trueType = getAutoReadableType(node.whenTrue->refType.Lock());
        Ref<Type> falseType = getAutoReadableType(node.whenFalse->refType.Lock());

        if (!trueType || trueType->isUnknown())
        {
            node.refType = falseType ? falseType : Compiler::get().getTypeContext().getUnknown();
            return;
        }
        if (!falseType || falseType->isUnknown())
        {
            node.refType = trueType;
            return;
        }

        if (!isAssignmentLikeCompatible(trueType, falseType) &&
            !isAssignmentLikeCompatible(falseType, trueType))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Conditional operator branches must have compatible types. Got '{}' and '{}'.",
                trueType->toString(),
                falseType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (previousExpectedExpressionType &&
            !previousExpectedExpressionType->isUnknown() &&
            isAssignmentLikeCompatible(previousExpectedExpressionType, trueType) &&
            isAssignmentLikeCompatible(previousExpectedExpressionType, falseType))
        {
            node.refType = previousExpectedExpressionType;
        }
        else if (isAssignmentLikeCompatible(trueType, falseType))
        {
            node.refType = trueType;
        }
        else
        {
            node.refType = falseType;
        }
    }

    void SemanticAnalyzer::visit(TypeExpression& node)
    {
        if (!node.type)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.type->accept(*this);
        node.refType = node.type->refType.Lock();

        if (node.type->name.type == TokenType::identifier)
            node.referencedSymbol = resolveQualifiedSymbol(currentScope_, node.type->name.value);
    }

    void SemanticAnalyzer::visit(UnaryExpression& node)
    {
        if (node.isMainExecutorAwait)
        {
            if (!currentFunctionIsAsync_)
            {
                WIO_LOG_ADD_ERROR(node.location(), "'await main' can only be used inside an async function or method.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = Compiler::get().getTypeContext().getVoid();
            return;
        }

        node.operand->accept(*this);
        Ref<Type> opType = node.operand->refType.Lock();

        if (!opType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.type == TokenType::kwAwait)
        {
            if (!currentFunctionIsAsync_)
            {
                WIO_LOG_ADD_ERROR(node.location(), "'await' can only be used inside an async function or method.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            Ref<Type> resolvedTaskType = unwrapAliasType(opType);
            if (!resolvedTaskType || resolvedTaskType->kind() != TypeKind::AsyncTask)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The 'await' operand must be a coroutine<T>, but got '{}'.",
                    opType->toString()
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = resolvedTaskType.AsFast<AsyncTaskType>()->valueType;
            return;
        }

        auto tryResolveUnaryOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getUnaryOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> readableOperandType = getAutoReadableType(opType);
            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto scoreUnaryArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                          const Ref<Type>& argumentType,
                                                          const Ref<Type>& readableArgumentType,
                                                          const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 880 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 840 : 820;
                        }
                    }
                }

                return std::nullopt;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableOperandType ? readableOperandType : opType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        auto candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = 1002
                        });
                    }
                }
            }

            std::unordered_set<const Symbol*> seenFreeSymbols;
            auto appendFreeSymbol = [&](const Ref<Symbol>& symbol)
            {
                if (!symbol || seenFreeSymbols.contains(symbol.Get()))
                    return;
                seenFreeSymbols.insert(symbol.Get());

                std::vector<Ref<Symbol>> overloads;
                if (symbol->kind == SymbolKind::FunctionGroup)
                    overloads = symbol->overloads;
                else if (symbol->kind == SymbolKind::Function)
                    overloads.push_back(symbol);
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
                        if (!deduceGenericBindings(candidateFunctionType->paramTypes[0], opType, genericBindings))
                        {
                            if (!(readableOperandType && deduceGenericBindings(candidateFunctionType->paramTypes[0], readableOperandType, genericBindings)))
                                continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto score = scoreUnaryArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        opType,
                        readableOperandType ? readableOperandType : getAutoReadableType(opType),
                        node.operand
                    );
                    if (!score.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *score
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

                appendAssociatedScopeSymbol(opType);
                appendAssociatedScopeSymbol(readableOperandType);
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

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for unary operator '{}' with operand type '{}'.",
                    node.op.value,
                    opType ? opType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (!bestCandidate.has_value())
                return false;

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveUnaryOperatorOverload())
            return;

        if (node.op.type == TokenType::kwDeref)
        {
            Ref<Type> resolvedType = unwrapAliasType(opType);
            if (!resolvedType || resolvedType->kind() != TypeKind::Reference)
            {
                WIO_LOG_ADD_ERROR(node.location(), "The 'deref' operator requires a readable reference.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = resolvedType.AsFast<ReferenceType>()->referredType;
            return;
        }

        if (node.op.type == TokenType::kwNot || node.op.type == TokenType::opLogicalNot)
        {
            if (opType != Compiler::get().getTypeContext().getBool())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Logical NOT (!) operator requires boolean operand.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else if (node.op.type == TokenType::opMinus)
        {
            if (!allowsNumericSemantics(opType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Unary minus (-) operator requires numeric operand.");
            }
            node.refType = opType;
        }
        else if (node.op.type == TokenType::opBitNot)
        {
            if (!allowsIntegerSemantics(opType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Bitwise NOT (~) operator requires integer operand.");
            }
            node.refType = opType;
        }
        else
        {
            // Others (bitwise not vs.) should return the same type for now.
            node.refType = opType;
        }
    }

    void SemanticAnalyzer::visit(AssignmentExpression& node)
    {
        node.left->accept(*this);
        Ref<Symbol> directlyAssignedSymbol = node.left->is<Identifier>()
            ? node.left->referencedSymbol.Lock()
            : nullptr;
        Ref<Type> declaredAssignmentType = directlyAssignedSymbol
            ? directlyAssignedSymbol->type
            : node.left->refType.Lock();
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
        currentExpectedExpressionType_ = getAutoReadableType(declaredAssignmentType);
        allowContextualNumericLiteralTyping_ = true;
        node.right->accept(*this);
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

        Ref<Type> lhsType = declaredAssignmentType;
        Ref<Type> rhsType = node.right->refType.Lock();
        if (directlyAssignedSymbol)
            nonNullNarrowedSymbols_.erase(directlyAssignedSymbol.Get());

        auto emitWriteabilityDiagnosticsForSymbol = [&](const Ref<Symbol>& referSym)
        {
            if (!referSym)
                return;

            if (!referSym->flags.get_isMutable() && !referSym->flags.get_isReadOnly())
            {
                WIO_LOG_ADD_ERROR(node.op.loc, "Cannot assign to immutable variable '{0}'. Hint: Declare it as 'mut {0}'.", referSym->name);
            }
            if (referSym->flags.get_isReadOnly())
            {
                bool isInsideObject = false;
                auto currentSearch = currentScope_;
                while (currentSearch)
                {
                    if (currentSearch->resolveLocally(referSym->name)) { isInsideObject = true; break; }
                    currentSearch = currentSearch->getParent().Lock();
                }

                if (!isInsideObject)
                    WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify @Readonly member '{0}' from outside its object.", referSym->name);
            }
        };

        auto tryResolveAssignmentOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getAssignmentOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> readableLhsType = getAutoReadableType(lhsType);
            Ref<Type> readableRhsType = getAutoReadableType(rhsType);

            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                Ref<Type> ownerType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto deduceBindingsFromAssignmentArgument = [&](const Ref<Type>& expectedType,
                                                           const Ref<Type>& argumentType,
                                                           const NodePtr<Expression>& argumentExpression,
                                                           std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                if (shouldAutoReadReferenceType(argumentType))
                {
                    Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (deduceGenericBindings(referredType, unwrapAliasType(argumentType), bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreAssignmentArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                               const Ref<Type>& argumentType,
                                                               const Ref<Type>& readableArgumentType,
                                                               const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 900 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 860 : 820;
                        }

                        if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                            return 80;
                    }
                }

                return std::nullopt;
            };

            auto collectFreeOperatorCandidates = [&]() -> std::vector<Ref<Symbol>>
            {
                std::vector<Ref<Symbol>> collected;
                std::unordered_set<const Symbol*> seen;

                auto appendCandidate = [&](const Ref<Symbol>& symbol)
                {
                    if (!symbol || seen.contains(symbol.Get()))
                        return;

                    seen.insert(symbol.Get());
                    collected.push_back(symbol);
                };

                appendCandidate(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);

                Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front();
                auto appendAssociatedScopeCandidate = [&](const Ref<Type>& type)
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
                    appendCandidate(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeCandidate(lhsType);
                appendAssociatedScopeCandidate(rhsType);
                appendAssociatedScopeCandidate(readableLhsType);
                appendAssociatedScopeCandidate(readableRhsType);

                return collected;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableLhsType ? readableLhsType : lhsType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        std::unordered_map<std::string, Ref<Type>> genericBindings;
                        auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;

                        if (!candidateSymbol->genericParameterNames.empty())
                        {
                            if (!deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[0], rhsType, node.right, genericBindings))
                                continue;

                            candidateType = instantiateGenericType(candidateType, genericBindings);
                            candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                            if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                                continue;
                        }

                        auto score = scoreAssignmentArgumentAgainstParameter(
                            candidateFunctionType->paramTypes[0],
                            rhsType,
                            readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                            node.right
                        );
                        if (!score.has_value())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .ownerType = ownerType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = *score + 2
                        });
                    }
                }
            }

            for (const auto& candidateSymbol : collectFreeOperatorCandidates())
            {
                if (!candidateSymbol)
                    continue;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    continue;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[0], lhsType, node.left, genericBindings) ||
                            !deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[1], rhsType, node.right, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto lhsScore = scoreAssignmentArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        lhsType,
                        readableLhsType ? readableLhsType : getAutoReadableType(lhsType),
                        node.left
                    );
                    if (!lhsScore.has_value())
                        continue;

                    auto rhsScore = scoreAssignmentArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        rhsType,
                        readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                        node.right
                    );
                    if (!rhsScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *lhsScore + *rhsScore
                    });
                }
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

            if (!isAddressableRefOperand(node.left))
            {
                WIO_LOG_ADD_ERROR(node.op.loc, "Operator-assignment requires an assignable left operand.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
                emitWriteabilityDiagnosticsForSymbol(referSym);

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator '{}' with operand types '{}' and '{}'.",
                    node.op.value,
                    lhsType ? lhsType->toString() : "<unknown>",
                    rhsType ? rhsType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveAssignmentOperatorOverload())
            return;

        bool isCompatible = false;
        bool isAutoDeref = false;

        if (lhsType && rhsType)
        {
            isCompatible = isAssignmentLikeCompatible(lhsType, rhsType);

            if (!isCompatible && lhsType->kind() == TypeKind::Reference)
            {
                Ref<Type> currentRef = lhsType;
                bool canMutate = true;

                while (currentRef && currentRef->kind() == TypeKind::Reference)
                {
                    auto rType = currentRef.AsFast<ReferenceType>();

                    if (!rType->isMutable) canMutate = false;

                    if (isAssignmentLikeCompatible(rType->referredType, rhsType))
                    {
                        isAutoDeref = true;
                        if (!canMutate) WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify data through a read-only reference (view).");
                        else isCompatible = true;
                        break;
                    }
                    currentRef = rType->referredType;
                }
            }
        }

        if (lhsType && rhsType && !lhsType->isUnknown() && !rhsType->isUnknown() && !isCompatible)
        {
            if (isRejectedImplicitNumericConversion(lhsType, rhsType))
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Implicit narrowing conversion from '{}' to '{}' requires explicit 'fit'.",
                    rhsType->toString(), lhsType->toString());
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                    rhsType->toString(), lhsType->toString());
            }
        }

        if (auto* arrayAccess = node.left->as<ArrayAccessExpression>())
        {
            Ref<Type> receiverType = unwrapAliasType(arrayAccess->object ? arrayAccess->object->refType.Lock() : nullptr);
            if (receiverType)
            {
                if (isTextType(receiverType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Text indexing is read-only; build a new text value instead."
                    );
                }
                else if (receiverType->kind() == TypeKind::GenericParameterPack ||
                    receiverType->kind() == TypeKind::ValuePackView ||
                    receiverType->kind() == TypeKind::TypePackView)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Cannot assign through raw pack values or pack views. Assign to a mutable pack storage field instead."
                    );
                }
                else if (receiverType->kind() == TypeKind::PackStorage)
                {
                    emitWriteabilityDiagnosticsForSymbol(arrayAccess->object ? arrayAccess->object->referencedSymbol.Lock() : nullptr);
                }
            }

            if (arrayAccess->operatorDispatchKind != OperatorDispatchKind::None)
            {
                Ref<Type> indexedType = unwrapAliasType(arrayAccess->refType.Lock());
                if (!indexedType || indexedType->kind() != TypeKind::Reference)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Subscript assignment requires operator '[]' to return a mutable reference."
                    );
                }
                else if (!indexedType.AsFast<ReferenceType>()->isMutable)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Cannot assign through a read-only subscript result."
                    );
                }
            }
        }

        if (!isAutoDeref)
        {
            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
                emitWriteabilityDiagnosticsForSymbol(referSym);
        }

        node.refType = Compiler::get().getTypeContext().getVoid();
    }
