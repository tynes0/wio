// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(ArrayAccessExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        const bool previousAllowTypePackIdentifierReference = allowTypePackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        allowTypePackIdentifierReference_ = true;
        node.object->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;
        allowTypePackIdentifierReference_ = previousAllowTypePackIdentifierReference;
        Ref<Type> objType = node.object->refType.Lock();
        Ref<Type> resolvedObjType = getAutoReadableType(objType);
        resolvedObjType = unwrapAliasType(resolvedObjType);

        node.index->accept(*this);
        Ref<Type> idxType = getAutoReadableType(node.index->refType.Lock());

        if (!resolvedObjType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto tryResolveIndexOperatorOverload = [&]() -> bool
        {
            const auto overloadName = common::getIndexOperatorOverloadName(TokenType::leftBracket);
            if (!overloadName.has_value())
                return false;

            struct OperatorCandidate
            {
                Ref<Symbol> symbol;
                Ref<FunctionType> functionType;
                Ref<StructType> ownerType;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = 0;
            };

            auto deduceBindingsFromIndexArgument = [&](const Ref<Type>& expectedType,
                                                       const Ref<Type>& argumentType,
                                                       const NodePtr<Expression>& argumentExpression,
                                                       std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                if (argumentExpression)
                    readableArgumentType = getAutoReadableType(argumentType);

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

            auto scoreIndexArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
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
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[0], idxType, node.index, genericBindings))
                            continue;

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto score = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        idxType,
                        getAutoReadableType(idxType),
                        node.index
                    );
                    if (!score.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = ownerStructType,
                        .dispatchKind = OperatorDispatchKind::Member,
                        .score = *score + 2
                    });
                }
            };

            appendMemberCandidates(objType);
            if (resolvedObjType != objType)
                appendMemberCandidates(resolvedObjType);

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
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[0], objType, node.object, genericBindings) ||
                            !deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[1], idxType, node.index, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto objectScore = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        objType,
                        getAutoReadableType(objType),
                        node.object
                    );
                    if (!objectScore.has_value())
                        continue;

                    auto indexScore = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        idxType,
                        getAutoReadableType(idxType),
                        node.index
                    );
                    if (!indexScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *objectScore + *indexScore
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

                appendAssociatedScopeSymbol(objType);
                appendAssociatedScopeSymbol(resolvedObjType);
                appendAssociatedScopeSymbol(idxType);
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
                    "Ambiguous overload for operator '[]' with operand types '{}' and '{}'.",
                    objType ? objType->toString() : "<unknown>",
                    idxType ? idxType->toString() : "<unknown>"
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

        if (tryResolveIndexOperatorOverload())
            return;

        const auto resolvePackElementType = [&](const std::string& packName,
                                                const std::vector<Ref<Type>>& elementTypes) -> Ref<Type>
        {
            auto indexBinding = tryEvaluatePackIndexBinding(node.index, variableDeclarationsBySymbol_, packName);
            if (!indexBinding.has_value())
            {
                WIO_LOG_ADD_ERROR(node.index->location(), "Pack indexing requires a non-negative compile-time integer index.");
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (indexBinding->kind == PackElementBindingKind::FromEnd && indexBinding->value == 0)
            {
                WIO_LOG_ADD_ERROR(node.index->location(), "Pack indexing from '.size' must subtract at least 1.");
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (!elementTypes.empty())
            {
                if (elementTypes.size() == 1)
                {
                    if (auto symbolicPackName = tryGetSymbolicPackReferenceName(elementTypes.front()))
                    {
                        ParsedPackElementBinding reboundBinding = *indexBinding;
                        reboundBinding.packName = *symbolicPackName;
                        return makeSyntheticPackElementType(reboundBinding);
                    }
                }

                if (auto resolvedIndex = tryResolveConcretePackElementIndex(*indexBinding, elementTypes.size()))
                    return elementTypes[*resolvedIndex];

                WIO_LOG_ADD_ERROR(
                    node.index->location(),
                    "Pack index is out of range for size {}.",
                    elementTypes.size()
                );
                return Compiler::get().getTypeContext().getUnknown();
            }

            ParsedPackElementBinding reboundBinding = *indexBinding;
            reboundBinding.packName = packName;
            return makeSyntheticPackElementType(reboundBinding);
        };

        if (resolvedObjType->kind() == TypeKind::GenericParameterPack)
        {
            node.refType = resolvePackElementType(
                resolvedObjType.AsFast<GenericParameterPackType>()->name,
                {}
            );
            return;
        }

        if (resolvedObjType->kind() == TypeKind::ValuePackView)
        {
            auto viewType = resolvedObjType.AsFast<ValuePackViewType>();
            node.refType = resolvePackElementType(viewType->packName, viewType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() == TypeKind::PackStorage)
        {
            auto storageType = resolvedObjType.AsFast<PackStorageType>();
            node.refType = resolvePackElementType(storageType->packName, storageType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() == TypeKind::TypePackView)
        {
            auto viewType = resolvedObjType.AsFast<TypePackViewType>();
            node.refType = resolvePackElementType(viewType->packName, viewType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() != TypeKind::Array &&
            !isStringType(resolvedObjType) &&
            !isTextType(resolvedObjType))
        {
            WIO_LOG_ADD_ERROR(node.object->location(), "Type '{}' is not an array, string, or text and cannot be indexed.", objType->toString());
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (!allowsIntegerSemantics(idxType))
        {
            WIO_LOG_ADD_ERROR(node.index->location(), "Array, string, and text indices must be integer values.");
        }

        if (resolvedObjType->kind() == TypeKind::Array)
        {
            auto arrType = resolvedObjType.AsFast<ArrayType>();

            if (auto staticIndex = tryEvaluateStaticPackIndex(node.index, variableDeclarationsBySymbol_);
                staticIndex.has_value() &&
                (arrType->arrayKind == ArrayType::ArrayKind::Static || arrType->arrayKind == ArrayType::ArrayKind::Literal) &&
                !arrType->hasInferredExtent &&
                *staticIndex >= arrType->size)
            {
                WIO_LOG_ADD_ERROR(
                    node.index->location(),
                    "Index {} is out of range for compile-time array size {}.",
                    *staticIndex,
                    arrType->size
                );
            }

            node.refType = arrType->elementType;
            node.borrowOrigin = classifyBorrowOrigin(node.object);
            return;
        }

        node.refType = isTextType(resolvedObjType)
            ? Compiler::get().getTypeContext().getText()
            : Compiler::get().getTypeContext().getChar();
    }

    void SemanticAnalyzer::visit(MemberAccessExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        const bool previousAllowTypePackIdentifierReference = allowTypePackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        allowTypePackIdentifierReference_ = true;
        node.object->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;
        allowTypePackIdentifierReference_ = previousAllowTypePackIdentifierReference;
        node.intrinsicMember = IntrinsicMember::None;
        node.intrinsicOverloadMembers.clear();
        node.intrinsicOverloadTypes.clear();

        Ref<Symbol> leftSymbol = node.object->referencedSymbol.Lock();
        Ref<Type> leftType = nullptr;
        Ref<Symbol> foundMember = nullptr;

        if (auto lockedType = node.object->refType.Lock(); lockedType)
        {
            bool isNamespace = (leftSymbol && leftSymbol->kind == SymbolKind::Namespace);

            if (!isNamespace && (!lockedType || lockedType->isUnknown()))
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            leftType = lockedType;
        }

        Ref<Type> resolvedLeftType = unwrapAliasType(leftType);
        if (resolvedLeftType && resolvedLeftType->kind() == TypeKind::Nullable)
        {
            WIO_LOG_ADD_ERROR(
                node.object->location(),
                "Member access requires a non-null value, but '{}' is nullable. Check it against null first.",
                leftType ? leftType->toString() : "<unknown>"
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        if (resolvedLeftType &&
            (resolvedLeftType->kind() == TypeKind::GenericParameterPack ||
             resolvedLeftType->kind() == TypeKind::ValuePackView ||
             resolvedLeftType->kind() == TypeKind::TypePackView ||
             resolvedLeftType->kind() == TypeKind::PackStorage))
        {
            auto& typeContext = Compiler::get().getTypeContext();
            const auto memberName = node.member->token.value;

            auto resolvePackName = [&]() -> std::string
            {
                auto normalizePackName = [&](const std::string& fallbackName, const std::vector<Ref<Type>>& elementTypes) -> std::string
                {
                    if (elementTypes.size() == 1)
                    {
                        if (auto symbolicPackName = tryGetSymbolicPackReferenceName(elementTypes.front()))
                            return *symbolicPackName;
                    }
                    return fallbackName;
                };

                switch (resolvedLeftType->kind())
                {
                case TypeKind::GenericParameterPack:
                    return resolvedLeftType.AsFast<GenericParameterPackType>()->name;
                case TypeKind::ValuePackView:
                {
                    auto viewType = resolvedLeftType.AsFast<ValuePackViewType>();
                    return normalizePackName(viewType->packName, viewType->elementTypes);
                }
                case TypeKind::TypePackView:
                {
                    auto viewType = resolvedLeftType.AsFast<TypePackViewType>();
                    return normalizePackName(viewType->packName, viewType->elementTypes);
                }
                case TypeKind::PackStorage:
                {
                    auto storageType = resolvedLeftType.AsFast<PackStorageType>();
                    return normalizePackName(storageType->packName, storageType->elementTypes);
                }
                default:
                    return {};
                }
            };

            auto resolvePackElements = [&]() -> std::vector<Ref<Type>>
            {
                auto normalizePackElements = [](const std::vector<Ref<Type>>& elementTypes) -> std::vector<Ref<Type>>
                {
                    if (elementTypes.size() == 1 && tryGetSymbolicPackReferenceName(elementTypes.front()).has_value())
                        return {};
                    return elementTypes;
                };

                switch (resolvedLeftType->kind())
                {
                case TypeKind::ValuePackView:
                    return normalizePackElements(resolvedLeftType.AsFast<ValuePackViewType>()->elementTypes);
                case TypeKind::TypePackView:
                    return normalizePackElements(resolvedLeftType.AsFast<TypePackViewType>()->elementTypes);
                case TypeKind::PackStorage:
                    return normalizePackElements(resolvedLeftType.AsFast<PackStorageType>()->elementTypes);
                default:
                    return {};
                }
            };

            if (memberName == "size")
            {
                node.intrinsicMember = IntrinsicMember::PackSize;
                node.refType = typeContext.getUSize();
                node.member->refType = node.refType.Lock();
                return;
            }

            if (memberName == "array")
            {
                node.intrinsicMember = IntrinsicMember::PackArray;
                const std::string packName = resolvePackName();
                const auto packElements = resolvePackElements();
                const bool isValuePackReference =
                    resolvedLeftType->kind() == TypeKind::ValuePackView ||
                    resolvedLeftType->kind() == TypeKind::PackStorage ||
                    (resolvedLeftType->kind() == TypeKind::GenericParameterPack &&
                     leftSymbol &&
                     (leftSymbol->kind == SymbolKind::Variable || leftSymbol->kind == SymbolKind::Parameter));

                if (isValuePackReference)
                {
                    node.refType = typeContext.getOrCreateValuePackViewType(packName, packElements);
                }
                else
                {
                    node.refType = typeContext.getOrCreateTypePackViewType(packName, packElements);
                }
                node.member->refType = node.refType.Lock();
                return;
            }

            if (memberName == "ToStaticArray")
            {
                node.intrinsicMember = IntrinsicMember::PackToStaticArray;
                node.refType = typeContext.getOrCreateFunctionType(typeContext.getUnknown(), {});
                node.member->refType = node.refType.Lock();
                return;
            }
        }

        std::function<Ref<Symbol>(Ref<Type>, const std::string&, Ref<Type>*)> findMemberInHierarchy =
            [&](const Ref<Type>& t, const std::string& name, Ref<Type>* ownerType) -> Ref<Symbol> {
                if (!t || t->kind() != TypeKind::Struct)
                    return nullptr;
                auto sType = t.AsFast<StructType>();

                if (auto lockedScope = sType->structScope.Lock(); lockedScope)
                {
                    if (auto found = lockedScope->resolveLocally(name); found)
                    {
                        if (ownerType)
                            *ownerType = t;
                        return found;
                    }
                }
                for (auto& base : sType->baseTypes)
                {
                    if (auto found = findMemberInHierarchy(base, name, ownerType); found)
                        return found;
                }
                return nullptr;
        };

        Ref<Type> actualStructType = nullptr;
        auto resolveIntrinsicMemberOnType = [&](const Ref<Type>& candidateType) -> bool
        {
            auto overloads = resolveIntrinsicMemberOverloads(Compiler::get().getTypeContext(), candidateType, node.member->token.value);
            if (!overloads.empty())
            {
                if (overloads.size() == 1)
                {
                    auto resolution = overloads.front();
                    if (node.member->token.value == "Get" &&
                        (resolution.member == IntrinsicMember::ArrayGet ||
                         resolution.member == IntrinsicMember::DictGet ||
                         resolution.member == IntrinsicMember::StringGet))
                    {
                        Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                        Ref<Type> optionPayloadType = nullptr;
                        if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Array)
                            optionPayloadType = resolvedCandidate.AsFast<ArrayType>()->elementType;
                        else if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Dictionary)
                            optionPayloadType = resolvedCandidate.AsFast<DictionaryType>()->valueType;
                        else if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Primitive &&
                                 resolvedCandidate.AsFast<PrimitiveType>()->name == "string")
                            optionPayloadType = Compiler::get().getTypeContext().getChar();
                        Ref<Symbol> optionSymbol = resolveQualifiedSymbol(currentScope_, "std::Option");
                        auto optionStruct = optionSymbol && optionSymbol->type && optionSymbol->type->kind() == TypeKind::Struct
                            ? optionSymbol->type.AsFast<StructType>()
                            : nullptr;
                        if (!optionPayloadType || !optionStruct)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.member->location(),
                                "Container Get requires the built-in std::Option<T> module."
                            );
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return true;
                        }

                        Ref<Type> optionType = instantiateGenericStructType(
                            optionStruct,
                            { optionPayloadType },
                            node.location()
                        );
                        resolution.memberType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            optionType,
                            resolution.memberType.AsFast<FunctionType>()->paramTypes
                        );
                    }
                    else if (resolution.member == IntrinsicMember::TaskPoll)
                    {
                        Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                        auto taskType = resolvedCandidate && resolvedCandidate->kind() == TypeKind::AsyncTask
                            ? resolvedCandidate.AsFast<AsyncTaskType>()
                            : nullptr;
                        Ref<Type> pollType = nullptr;
                        if (taskType && taskType->valueType && taskType->valueType->isVoid())
                        {
                            Ref<Symbol> pollSymbol = resolveQualifiedSymbol(currentScope_, "std::async::VoidTaskPoll");
                            if (pollSymbol)
                                pollType = pollSymbol->type;
                        }
                        else if (taskType)
                        {
                            Ref<Symbol> pollSymbol = resolveQualifiedSymbol(currentScope_, "std::async::TaskPoll");
                            auto pollStruct = pollSymbol && pollSymbol->type && pollSymbol->type->kind() == TypeKind::Struct
                                ? pollSymbol->type.AsFast<StructType>()
                                : nullptr;
                            if (pollStruct)
                                pollType = instantiateGenericStructType(pollStruct, {taskType->valueType}, node.location());
                        }
                        if (!pollType)
                        {
                            WIO_LOG_ADD_ERROR(node.member->location(),
                                "Task Poll requires the built-in std::async task module.");
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return true;
                        }
                        resolution.memberType = Compiler::get().getTypeContext().getOrCreateFunctionType(pollType, {});
                    }
                    else if (resolution.member == IntrinsicMember::TaskWithin)
                    {
                        Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                        auto taskType = resolvedCandidate && resolvedCandidate->kind() == TypeKind::AsyncTask
                            ? resolvedCandidate.AsFast<AsyncTaskType>()
                            : nullptr;
                        Ref<Type> resultType = nullptr;
                        if (taskType && taskType->valueType && taskType->valueType->isVoid())
                        {
                            resultType = Compiler::get().getTypeContext().getOrCreateAsyncTaskType(
                                Compiler::get().getTypeContext().getBool());
                        }
                        else if (taskType)
                        {
                            Ref<Symbol> optionSymbol = resolveQualifiedSymbol(currentScope_, "std::Option");
                            auto optionStruct = optionSymbol && optionSymbol->type && optionSymbol->type->kind() == TypeKind::Struct
                                ? optionSymbol->type.AsFast<StructType>()
                                : nullptr;
                            if (optionStruct)
                            {
                                Ref<Type> optionType = instantiateGenericStructType(
                                    optionStruct, {taskType->valueType}, node.location());
                                resultType = Compiler::get().getTypeContext().getOrCreateAsyncTaskType(optionType);
                            }
                        }
                        if (!resultType)
                        {
                            WIO_LOG_ADD_ERROR(node.member->location(),
                                "Task Within requires the built-in std::async and std::Option modules.");
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return true;
                        }
                        resolution.memberType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            resultType, {Compiler::get().getTypeContext().getU64()});
                    }
                    node.intrinsicMember = resolution.member;
                    node.refType = resolution.memberType;

                    if (resolution.requiresMutableReceiver && !canMutateIntrinsicReceiver(node.object))
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Container member '{}' requires a mutable receiver.", node.member->token.value);
                    }

                    if (auto memberId = node.member.Get(); memberId)
                        memberId->refType = resolution.memberType;
                }
                else
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    node.intrinsicMember = IntrinsicMember::None;
                    node.intrinsicOverloadMembers.reserve(overloads.size());
                    node.intrinsicOverloadTypes.reserve(overloads.size());

                    for (const auto& overload : overloads)
                    {
                        node.intrinsicOverloadMembers.push_back(overload.member);
                        node.intrinsicOverloadTypes.emplace_back(overload.memberType);
                    }

                    if (auto memberId = node.member.Get(); memberId)
                        memberId->refType = Compiler::get().getTypeContext().getUnknown();
                }

                return true;
            }

            if (isUnsupportedStaticArrayMember(candidateType, node.member->token.value))
            {
                WIO_LOG_ADD_ERROR(
                    node.member->location(),
                    "Static arrays do not support member '{}'. Use a dynamic array instead.",
                    node.member->token.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            return false;
        };

        if (leftSymbol && leftSymbol->kind == SymbolKind::Namespace)
        {
            if (!leftSymbol->innerScope)
            {
                WIO_LOG_ADD_ERROR(node.location(), "The namespace contents are inaccessible. (No Scope): {}", leftSymbol->name);
                return;
            }
            foundMember = leftSymbol->innerScope->resolve(node.member->token.value);
        }
        else
        {
            Ref<Type> baseType = leftType;
            while (baseType && baseType->kind() == TypeKind::Alias)
                baseType = baseType.AsFast<AliasType>()->aliasedType;

            if (!baseType)
            {
                WIO_LOG_ADD_ERROR(node.member->location(), "Cannot access member '{}'. The left-hand side has no resolved type.", node.member->token.value);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (baseType->kind() == TypeKind::Struct)
            {
                actualStructType = baseType;
                foundMember = findMemberInHierarchy(actualStructType, node.member->token.value, &actualStructType);
                if (!foundMember && resolveIntrinsicMemberOnType(actualStructType))
                    return;
            }
            else if (isIntrinsicReceiverType(baseType))
            {
                if (resolveIntrinsicMemberOnType(baseType))
                    return;
            }
            else if (baseType->kind() == TypeKind::Reference)
            {
                Ref<Type> referredType = baseType.AsFast<ReferenceType>()->referredType;

                while (referredType)
                {
                    while (referredType && referredType->kind() == TypeKind::Alias)
                        referredType = referredType.AsFast<AliasType>()->aliasedType;

                    if (referredType && referredType->kind() == TypeKind::Reference)
                    {
                        referredType = referredType.AsFast<ReferenceType>()->referredType;
                        continue;
                    }

                    break;
                }

                if (referredType && referredType->kind() == TypeKind::Struct)
                {
                    actualStructType = referredType;
                    foundMember = findMemberInHierarchy(actualStructType, node.member->token.value, &actualStructType);
                    if (!foundMember && resolveIntrinsicMemberOnType(actualStructType))
                        return;
                }
                else if (referredType && isIntrinsicReceiverType(referredType))
                {
                    if (resolveIntrinsicMemberOnType(referredType))
                        return;
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.member->location(),
                        "Cannot access member '{}'. Reference points to '{}', which is not a struct, array, dictionary, or string.",
                        node.member->token.value,
                        referredType ? referredType->toString() : "Unknown");
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }
        }

        if (!foundMember && actualStructType)
        {
            auto findExtensionMethod = [&](const Ref<Type>& receiverType) -> Ref<Symbol>
            {
                auto receiverStruct = receiverType ? receiverType.AsFast<StructType>() : nullptr;
                if (!receiverStruct)
                    return nullptr;

                auto findDirect = [&](const Type* candidate) -> Ref<Symbol>
                {
                    auto typeMethods = extensionMethods_.find(candidate);
                    if (typeMethods == extensionMethods_.end())
                        return nullptr;
                    auto method = typeMethods->second.find(node.member->token.value);
                    return method == typeMethods->second.end() ? nullptr : method->second;
                };

                if (Ref<Symbol> exact = findDirect(receiverStruct.Get()))
                    return exact;

                // Instantiated generic types are distinct semantic type nodes.
                // Extensions and checked derives declared on the generic
                // primary remain part of every concrete instantiation.
                if (Ref<StructType> primary = receiverStruct->genericPrimaryType.Lock())
                    return findDirect(primary.Get());
                return nullptr;
            };

            if (Ref<Symbol> extensionSymbol = findExtensionMethod(actualStructType))
            {
                    auto fullType = extensionSymbol->type.AsFast<FunctionType>();
                    if (fullType && !fullType->paramTypes.empty())
                    {
                        std::vector<Ref<Type>> visibleParameters(
                            fullType->paramTypes.begin() + 1, fullType->paramTypes.end());
                        Ref<Type> visibleType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            fullType->returnType, visibleParameters, fullType->hasParameterPack);

                        auto receiverType = fullType->paramTypes.front();
                        auto receiverReference = receiverType && receiverType->kind() == TypeKind::Reference
                            ? receiverType.AsFast<ReferenceType>()
                            : nullptr;
                        if (receiverReference && receiverReference->isMutable &&
                            !canMutateIntrinsicReceiver(node.object))
                        {
                            WIO_LOG_ADD_ERROR(node.location(),
                                "Extension method '{}' requires a mutable receiver.",
                                node.member->token.value);
                        }

                        Ref<Symbol> callableSymbol = createSymbol(
                            extensionSymbol->name, visibleType, SymbolKind::Function,
                            extensionSymbol->definitionLoc);
                        callableSymbol->scopePath = extensionSymbol->scopePath;
                        callableSymbol->flags.set_isExtension(true);
                        if (extensionSymbol->flags.get_isDerived())
                            callableSymbol->flags.set_isDerived(true);
                        callableSymbol->extensionTargetType = extensionSymbol->extensionTargetType;
                        callableSymbol->extensionMemberName = extensionSymbol->extensionMemberName;
                        callableSymbol->extensionImplementation = extensionSymbol;
                        callableSymbol->derivedProcessorCppType = extensionSymbol->derivedProcessorCppType;
                        callableSymbol->genericParameterNames = extensionSymbol->genericParameterNames;
                        callableSymbol->genericParameterTypes = extensionSymbol->genericParameterTypes;
                        callableSymbol->genericParameterDefaults = extensionSymbol->genericParameterDefaults;
                        callableSymbol->hasGenericParameterPack = extensionSymbol->hasGenericParameterPack;
                        callableSymbol->resolvedGenericInstantiations = extensionSymbol->resolvedGenericInstantiations;
                        if (auto attributes = attributeListsBySymbol_.find(extensionSymbol.Get());
                            attributes != attributeListsBySymbol_.end())
                        {
                            attributeListsBySymbol_[callableSymbol.Get()] = attributes->second;
                        }

                        node.referencedSymbol = callableSymbol;
                        node.refType = visibleType;
                        node.member->referencedSymbol = callableSymbol;
                        node.member->refType = visibleType;
                        return;
                    }
            }
        }

        if (!foundMember)
        {
            std::string ownerName = leftType ? leftType->toString() : (leftSymbol ? leftSymbol->name : "<unknown>");
            WIO_LOG_ADD_ERROR(node.member->location(), "Member not found: '{}' in '{}'", node.member->token.value, ownerName);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        bool isInsideHierarchy = false;
        bool isInsideSameObject = false;
        bool isTrustedAccess = false;

        if (currentStructType_ && actualStructType)
        {
            if (currentStructType_ == actualStructType ||
                isTypeDerivedFrom(currentStructType_, actualStructType) ||
                isTypeDerivedFrom(actualStructType, currentStructType_))
            {
                isInsideHierarchy = true;
            }

            isInsideSameObject = currentStructType_ == actualStructType;

            if (auto ownerStruct = actualStructType.AsFast<StructType>(); ownerStruct)
            {
                const std::string trustedKey = getStructIdentityKey(currentStructType_.AsFast<StructType>());
                isTrustedAccess =
                    !trustedKey.empty() &&
                    std::ranges::find(ownerStruct->trustedTypeKeys, trustedKey) != ownerStruct->trustedTypeKeys.end();
            }
        }

        const std::string ownerTypeName = formatAccessContextType(actualStructType);
        const std::string currentContextTypeName = formatAccessContextType(currentStructType_);

        if (foundMember->flags.get_isPrivate() && !isInsideSameObject && !isTrustedAccess)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Cannot access private member '{}' declared on '{}' from '{}'.",
                foundMember->name,
                ownerTypeName,
                currentContextTypeName
            );
        }

        if (foundMember->flags.get_isProtected() && !isInsideHierarchy && !isTrustedAccess)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Cannot access protected member '{}' declared on '{}' from '{}'.",
                foundMember->name,
                ownerTypeName,
                currentContextTypeName
            );
        }

        Ref<Type> memberType = foundMember->type;
        if (auto instantiatedStructType = actualStructType ? actualStructType.AsFast<StructType>() : nullptr;
            instantiatedStructType && !instantiatedStructType->genericParameterNames.empty() && !instantiatedStructType->genericArguments.empty())
        {
            auto bindings = buildExtendedGenericBindings(
                instantiatedStructType->genericParameterNames,
                instantiatedStructType->hasGenericParameterPack,
                instantiatedStructType->genericArguments
            );
            memberType = instantiateGenericType(memberType, bindings);
        }

        node.referencedSymbol = foundMember;
        node.refType = memberType;

        if (auto memberId = node.member.Get(); memberId)
        {
            memberId->referencedSymbol = foundMember;
            memberId->refType = memberType;
        }
    }

    void SemanticAnalyzer::visit(FunctionCallExpression& node)
    {
        auto formatAppliedTypeName = [](const std::string& baseName, const std::vector<Ref<Type>>& typeArguments) -> std::string
        {
            if (typeArguments.empty())
                return baseName;

            std::string result = baseName + "<";
            for (size_t i = 0; i < typeArguments.size(); ++i)
            {
                result += typeArguments[i] ? typeArguments[i]->toString() : "<unknown>";
                if (i + 1 < typeArguments.size())
                    result += ", ";
            }
            result += ">";
            return result;
        };

        auto satisfiesApplyForSymbol = [&](const Ref<Symbol>& symbol,
                                           const std::vector<Ref<Type>>& explicitTypeArguments,
                                           std::string_view declarationKind) -> bool
        {
            if (!symbol || symbol->genericParameterNames.empty())
                return true;

            auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
            if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                return true;

            if (!matchesApplyConstraints(
                    *attributeIt->second,
                    symbol->genericParameterNames,
                    symbol->hasGenericParameterPack,
                    explicitTypeArguments))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic {} '{}' rejects type arguments {} because of @Apply constraints.",
                    declarationKind,
                    symbol->name,
                    formatConcreteInstantiationSignature(explicitTypeArguments)
                );
                return false;
            }

            return true;
        };

        auto finalizeCallResultType = [&](const Ref<Type>& resultType) -> bool
        {
            if (!node.unwrapResult && !node.propagateResult)
            {
                node.refType = resultType;
                Ref<Type> resolvedResultType = unwrapAliasType(resultType);
                if (resolvedResultType && resolvedResultType->kind() == TypeKind::Reference)
                {
                    if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
                    {
                        node.borrowOrigin = classifyBorrowOrigin(memberAccess->object);
                    }
                    else if (node.operatorDispatchKind == OperatorDispatchKind::Member)
                    {
                        node.borrowOrigin = classifyBorrowOrigin(node.callee);
                    }
                    else
                    {
                        Ref<Type> callableType = node.callee ? unwrapAliasType(node.callee->refType.Lock()) : nullptr;
                        auto functionType = callableType ? callableType.AsFast<FunctionType>() : nullptr;
                        node.borrowOrigin = BorrowOrigin::Static;
                        if (functionType)
                        {
                            const size_t argumentCount = std::min(node.arguments.size(), functionType->paramTypes.size());
                            for (size_t i = 0; i < argumentCount; ++i)
                            {
                                Ref<Type> parameterType = unwrapAliasType(functionType->paramTypes[i]);
                                if (parameterType && parameterType->kind() == TypeKind::Reference)
                                {
                                    node.borrowOrigin = classifyBorrowOrigin(node.arguments[i]);
                                    break;
                                }
                            }
                        }
                    }
                }
                return true;
            }

            if (auto payloadType = tryGetResultPayloadType(resultType))
            {
                if (node.propagateResult && !tryGetResultPayloadType(currentFunctionReturnType_))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "The '?()' propagation syntax requires the enclosing function to return std::Result<T>."
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return false;
                }

                node.refType = *payloadType;
                return true;
            }

            if (node.propagateResult)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The '?()' propagation syntax requires the called function to return std::Result<T>."
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The '!()' unwrap syntax requires the called function to return std::Result<T>."
                );
            }
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return false;
        };

        node.callee->accept(*this);
        Ref<Symbol> calleeSym = node.callee->referencedSymbol.Lock();
        Ref<Symbol> genericOwnerSym = calleeSym;
        node.operatorDispatchKind = OperatorDispatchKind::None;
        node.overloadFunctionType = nullptr;
        std::vector<Ref<Type>> explicitTypeArguments;
        explicitTypeArguments.reserve(node.explicitTypeArguments.size());
        for (auto& explicitTypeArgument : node.explicitTypeArguments)
        {
            explicitTypeArgument->accept(*this);
            explicitTypeArguments.push_back(explicitTypeArgument->refType.Lock());
        }

        if (calleeSym && calleeSym->kind == SymbolKind::Function &&
            !explicitTypeArguments.empty() && !calleeSym->genericParameterNames.empty() &&
            !validateGenericArgumentKinds(
                calleeSym->genericParameterNames,
                calleeSym->genericParameterTypes,
                explicitTypeArguments,
                node.location()))
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (auto* memberAccess = node.callee->as<MemberAccessExpression>();
            memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
        {
            if (!node.arguments.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Pack ToStaticArray does not accept runtime arguments.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (explicitTypeArguments.size() != 1 || !explicitTypeArguments.front())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Pack ToStaticArray requires exactly one explicit element type argument.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            Ref<Type> receiverType = unwrapAliasType(memberAccess->object->refType.Lock());
            if (!receiverType || receiverType->kind() == TypeKind::TypePackView)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Type packs cannot be converted to runtime static arrays.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            size_t arraySize = 0;
            if (receiverType->kind() == TypeKind::ValuePackView)
                arraySize = receiverType.AsFast<ValuePackViewType>()->elementTypes.size();
            else if (receiverType->kind() == TypeKind::PackStorage)
                arraySize = receiverType.AsFast<PackStorageType>()->elementTypes.size();

            finalizeCallResultType(Compiler::get().getTypeContext().getOrCreateArrayType(
                explicitTypeArguments.front(),
                ArrayType::ArrayKind::Static,
                arraySize
            ));
            return;
        }

        bool isConstructorCall = false;
        Ref<Type> structReturnType = nullptr;
        Ref<StructType> constructorStructType = nullptr;
        std::vector<std::string> constructorGenericParameterNames;
        std::unordered_map<std::string, Ref<Type>> constructorGenericBindings;
        GenericBindingSet constructorGenericBindingSet;
        bool useExplicitFunctionTypeArguments = false;

        if (calleeSym && calleeSym->kind == SymbolKind::TypeAlias)
        {
            Ref<Type> resolvedAliasTargetType = nullptr;
            Ref<Type> resolvedAliasResultType = calleeSym->type;

            if (!calleeSym->genericParameterNames.empty())
            {
                const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                    calleeSym->genericParameterNames,
                    calleeSym->hasGenericParameterPack
                );
                const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                    calleeSym->genericParameterDefaults, fixedArgumentCount);
                auto completedArguments = completeGenericTypeArguments(
                    calleeSym->genericParameterNames,
                    calleeSym->genericParameterDefaults,
                    calleeSym->hasGenericParameterPack,
                    explicitTypeArguments);
                if (!completedArguments)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Type alias '{}' expects {} generic arguments, but got {}.",
                        calleeSym->name,
                        calleeSym->hasGenericParameterPack
                            ? common::formatString("at least {}", requiredArgumentCount)
                            : requiredArgumentCount == fixedArgumentCount
                                ? std::to_string(fixedArgumentCount)
                                : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                        explicitTypeArguments.size()
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                explicitTypeArguments = std::move(*completedArguments);

                if (!satisfiesApplyForSymbol(calleeSym, explicitTypeArguments, "type alias"))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                constructorGenericBindingSet = buildExtendedGenericBindings(
                    calleeSym->genericParameterNames,
                    calleeSym->hasGenericParameterPack,
                    explicitTypeArguments
                );
                constructorGenericBindings = constructorGenericBindingSet.directBindings;
                resolvedAliasTargetType = instantiateGenericType(calleeSym->aliasTargetType, constructorGenericBindingSet);
                resolvedAliasResultType = Compiler::get().getTypeContext().getOrCreateAliasType(
                    formatAppliedTypeName(calleeSym->name, explicitTypeArguments),
                    resolvedAliasTargetType
                );
            }
            else
            {
                if (!explicitTypeArguments.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Type alias '{}' does not accept generic arguments.",
                        calleeSym->name
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                resolvedAliasTargetType = calleeSym->aliasTargetType
                    ? calleeSym->aliasTargetType
                    : unwrapAliasType(calleeSym->type);
            }

            Ref<Type> resolvedConstructorTarget = unwrapAliasType(resolvedAliasTargetType);
            if (resolvedConstructorTarget && resolvedConstructorTarget->kind() == TypeKind::Struct)
            {
                isConstructorCall = true;
                auto structType = resolvedConstructorTarget.AsFast<StructType>();
                constructorStructType = structType;
                constructorGenericParameterNames = structType->genericParameterNames;
                structReturnType = resolvedAliasResultType;
                node.callee->refType = structReturnType;

                if (!constructorStructType->genericParameterNames.empty() &&
                    !constructorStructType->genericArguments.empty())
                {
                    constructorGenericBindings = buildGenericTypeBindings(
                        constructorStructType->genericParameterNames,
                        constructorStructType->genericArguments
                    );
                    constructorGenericBindingSet = buildExtendedGenericBindings(
                        constructorStructType->genericParameterNames,
                        constructorStructType->hasGenericParameterPack,
                        constructorStructType->genericArguments
                    );
                }

                if (auto lockedScope = constructorStructType->structScope.Lock())
                    calleeSym = lockedScope->resolveLocally("OnConstruct");

                if (!calleeSym)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "No constructor found for type '{}'.", constructorStructType->name);
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }
        }

        if (calleeSym && calleeSym->kind == SymbolKind::Struct)
        {
            isConstructorCall = true;
            auto structType = calleeSym->type.AsFast<StructType>();
            constructorStructType = structType;
            constructorGenericParameterNames = structType->genericParameterNames;
            structReturnType = structType;

            if (!structType->genericParameterNames.empty())
            {
                  const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                      structType->genericParameterNames,
                      structType->hasGenericParameterPack
                  );
                  if (!explicitTypeArguments.empty())
                  {
                      const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                          structType->genericParameterDefaults, fixedArgumentCount);
                      auto completedArguments = completeGenericTypeArguments(
                          structType->genericParameterNames,
                          structType->genericParameterDefaults,
                          structType->hasGenericParameterPack,
                          explicitTypeArguments);
                      if (!completedArguments)
                      {
                          WIO_LOG_ADD_ERROR(
                              node.location(),
                              "Generic type '{}' expects {} generic arguments, but got {}.",
                              structType->name,
                              structType->hasGenericParameterPack
                                  ? common::formatString("at least {}", requiredArgumentCount)
                                  : requiredArgumentCount == fixedArgumentCount
                                      ? std::to_string(fixedArgumentCount)
                                      : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                              explicitTypeArguments.size()
                          );
                          node.refType = Compiler::get().getTypeContext().getUnknown();
                          return;
                      }
                      explicitTypeArguments = std::move(*completedArguments);
                  }

                if (!explicitTypeArguments.empty())
                {
                      constructorGenericBindings = buildGenericTypeBindings(structType->genericParameterNames, explicitTypeArguments);
                      constructorGenericBindingSet = buildExtendedGenericBindings(
                          structType->genericParameterNames,
                          structType->hasGenericParameterPack,
                          explicitTypeArguments
                      );
                    auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                    if (attributeIt != attributeListsBySymbol_.end() &&
                        attributeIt->second &&
                        !matchesApplyConstraints(
                            *attributeIt->second,
                            constructorGenericParameterNames,
                            structType->hasGenericParameterPack,
                            explicitTypeArguments))
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Generic type '{}' rejects type arguments {} because of @Apply constraints.",
                            structType->name,
                            formatConcreteInstantiationSignature(explicitTypeArguments)
                        );
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return;
                    }

                    structReturnType = instantiateGenericStructType(structType, explicitTypeArguments, node.location());
                    node.callee->refType = structReturnType;
                }
                else if (currentExpectedExpressionType_)
                {
                    Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
                    if (expectedType && expectedType->kind() == TypeKind::Struct)
                    {
                        auto expectedStructType = expectedType.AsFast<StructType>();
                        if (expectedStructType &&
                            expectedStructType->name == structType->name &&
                            expectedStructType->scopePath == structType->scopePath &&
                              (structType->hasGenericParameterPack
                                  ? expectedStructType->genericArguments.size() >= fixedArgumentCount
                                  : expectedStructType->genericArguments.size() == structType->genericParameterNames.size()))
                        {
                            bool hasConcreteExpectedArguments = true;
                            for (const auto& genericArgument : expectedStructType->genericArguments)
                            {
                                if (!genericArgument || genericArgument->isUnknown() || containsGenericParameterType(genericArgument))
                                {
                                    hasConcreteExpectedArguments = false;
                                    break;
                                }
                            }

                            if (hasConcreteExpectedArguments)
                            {
                                  constructorGenericBindings = buildGenericTypeBindings(
                                      structType->genericParameterNames,
                                      expectedStructType->genericArguments
                                  );
                                  constructorGenericBindingSet = buildExtendedGenericBindings(
                                      structType->genericParameterNames,
                                      structType->hasGenericParameterPack,
                                      expectedStructType->genericArguments
                                  );
                                auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                                if (attributeIt != attributeListsBySymbol_.end() &&
                                    attributeIt->second &&
                                    !matchesApplyConstraints(
                                        *attributeIt->second,
                                        constructorGenericParameterNames,
                                        structType->hasGenericParameterPack,
                                        expectedStructType->genericArguments))
                                {
                                    WIO_LOG_ADD_ERROR(
                                        node.location(),
                                        "Generic type '{}' rejects type arguments {} because of @Apply constraints.",
                                        structType->name,
                                        formatConcreteInstantiationSignature(expectedStructType->genericArguments)
                                    );
                                    node.refType = Compiler::get().getTypeContext().getUnknown();
                                    return;
                                }

                                structReturnType = instantiateGenericStructType(structType, expectedStructType->genericArguments, node.location());
                                node.callee->refType = structReturnType;
                            }
                        }
                    }
                }
            }
            else if (!explicitTypeArguments.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Type '{}' does not accept generic arguments.", structType->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            Ref<StructType> resolvedConstructorStructType = structType;
            if (auto resolvedReturnType = unwrapAliasType(structReturnType);
                resolvedReturnType && resolvedReturnType->kind() == TypeKind::Struct)
            {
                resolvedConstructorStructType = resolvedReturnType.AsFast<StructType>();
                constructorStructType = resolvedConstructorStructType;
            }

            if (auto lockedScope = resolvedConstructorStructType->structScope.Lock())
            {
                calleeSym = lockedScope->resolveLocally("OnConstruct");
            }

            if (!calleeSym)
            {
                WIO_LOG_ADD_ERROR(node.location(), "No constructor found for type '{}'.", structType->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        useExplicitFunctionTypeArguments = !isConstructorCall && !explicitTypeArguments.empty();

        std::optional<size_t> packExpansionIndex;
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            if (!node.arguments[i] || !node.arguments[i]->is<PackExpansionExpression>())
                continue;

            if (packExpansionIndex.has_value())
            {
                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), "Only one pack expansion argument is supported in a function call.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (i + 1 != node.arguments.size())
            {
                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), "Pack expansion arguments must be trailing.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            packExpansionIndex = i;
        }

        auto isSafeRefCast = [&](const Ref<Type>& dest, const Ref<Type>& src) -> bool
        {
            if (dest && src && dest->kind() == TypeKind::Reference && src->kind() == TypeKind::Reference)
            {
                auto dRef = dest.AsFast<ReferenceType>();
                auto sRef = src.AsFast<ReferenceType>();

                bool isCompatible = isTypeDerivedFrom(sRef->referredType, dRef->referredType);

                if (!dRef->isMutable && isCompatible) return true;
                if (dRef->isMutable && sRef->isMutable && isCompatible) return true;
            }
            return false;
        };

        auto isImplicitObjectViewBridge = [&](const Ref<Type>& dest, const Ref<Type>& src) -> bool
        {
            Ref<Type> resolvedDest = unwrapAliasType(dest);
            Ref<Type> resolvedSrc = unwrapAliasType(src);

            if (!resolvedDest || !resolvedSrc || resolvedDest->kind() != TypeKind::Reference)
                return false;

            auto expectedRef = resolvedDest.AsFast<ReferenceType>();
            if (expectedRef->isMutable || resolvedSrc->kind() != TypeKind::Struct)
                return false;

            auto expectedTarget = unwrapAliasType(expectedRef->referredType);
            if (!expectedTarget || expectedTarget->kind() != TypeKind::Struct)
                return false;

            auto expectedStruct = expectedTarget.AsFast<StructType>();
            auto actualStruct = resolvedSrc.AsFast<StructType>();
            if ((!expectedStruct->isObject && !expectedStruct->isInterface) ||
                (!actualStruct->isObject && !actualStruct->isInterface))
            {
                return false;
            }

            return expectedTarget->isCompatibleWith(resolvedSrc) ||
                   isTypeDerivedFrom(resolvedSrc, expectedTarget);
        };

        auto isContextSensitiveArgument = [&](const NodePtr<Expression>& argument) -> bool
        {
            if (!argument)
                return false;

            if (argument->is<LambdaExpression>())
                return true;

            if (auto* arrayLiteral = argument->as<ArrayLiteral>())
                return arrayLiteral->elements.empty();

            if (auto* dictionaryLiteral = argument->as<DictionaryLiteral>())
                return dictionaryLiteral->pairs.empty();

            return false;
        };

        auto analyzeArgumentWithExpectedType = [&](const NodePtr<Expression>& argument,
                                                   const Ref<Type>& expectedType,
                                                   bool suppressDiagnostics) -> std::optional<Ref<Type>>
        {
            if (!argument)
                return std::nullopt;

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = expectedType ? getAutoReadableType(expectedType) : nullptr;
            allowContextualNumericLiteralTyping_ = !suppressDiagnostics;

            if (suppressDiagnostics)
                Logger::get().beginDiagnosticProbe();

            argument->accept(*this);

            const int32_t suppressedErrors = suppressDiagnostics ? Logger::get().endDiagnosticProbe() : 0;
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

            if (suppressDiagnostics && suppressedErrors > 0)
                return std::nullopt;

            Ref<Type> analyzedType = argument->refType.Lock();
            if (!analyzedType)
                return std::nullopt;

            if (suppressDiagnostics && analyzedType->isUnknown())
                return std::nullopt;

            return analyzedType;
        };

        auto getFunctionDeclarationForSymbol = [&](const Ref<Symbol>& symbol) -> const FunctionDeclaration*
        {
            if (!symbol)
                return nullptr;

            auto foundDeclaration = functionDeclarationsBySymbol_.find(symbol.Get());
            if (foundDeclaration != functionDeclarationsBySymbol_.end())
                return foundDeclaration->second;

            if (symbol->flags.get_isExtension() && symbol->extensionImplementation)
            {
                foundDeclaration = functionDeclarationsBySymbol_.find(symbol->extensionImplementation.Get());
                if (foundDeclaration != functionDeclarationsBySymbol_.end())
                    return foundDeclaration->second;
            }

            return nullptr;
        };

        auto getRequiredArgumentCountForDeclaration = [&](const FunctionDeclaration* declaration,
                                                          const Ref<FunctionType>& callableType) -> size_t
        {
            if (!declaration)
                return callableType && callableType->hasParameterPack
                    ? (callableType->paramTypes.empty() ? 0 : callableType->paramTypes.size() - 1)
                    : (callableType ? callableType->paramTypes.size() : 0);

            size_t requiredCount = getRequiredParameterCount(declaration);
            const bool hidesExtensionReceiver =
                declaration->isExtensionMethod && callableType &&
                declaration->parameters.size() == callableType->paramTypes.size() + 1;
            if (hidesExtensionReceiver && requiredCount > 0)
                --requiredCount;
            return requiredCount;
        };

        auto getRequiredArgumentCountForCallable = [&](const Ref<Symbol>& symbol,
                                                       const Ref<FunctionType>& functionType) -> size_t
        {
            if (const auto* functionDeclaration = getFunctionDeclarationForSymbol(symbol))
            {
                size_t requiredCount = getRequiredArgumentCountForDeclaration(functionDeclaration, functionType);
                if (symbol && symbol->flags.get_isDerived() && requiredCount > 0)
                    --requiredCount;
                return requiredCount;
            }

            if (functionType && functionType->hasParameterPack)
                return functionType->paramTypes.empty() ? 0 : functionType->paramTypes.size() - 1;

            return functionType ? functionType->paramTypes.size() : 0;
        };

        auto buildCallableSurfaceFunctionType = [&](const Ref<FunctionType>& functionType,
                                                   const std::vector<Ref<Type>>& actualArgumentTypes,
                                                   bool usesPackExpansion) -> Ref<FunctionType>
        {
            if (!functionType)
                return nullptr;

            if (!functionType->hasParameterPack)
            {
                const size_t surfaceArity = std::min(actualArgumentTypes.size(), functionType->paramTypes.size());
                std::vector<Ref<Type>> surfaceParamTypes;
                surfaceParamTypes.reserve(surfaceArity);
                for (size_t i = 0; i < surfaceArity; ++i)
                    surfaceParamTypes.push_back(functionType->paramTypes[i]);

                return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes)
                    .AsFast<FunctionType>();
            }

            const size_t fixedParameterCount = functionType->paramTypes.empty() ? 0 : functionType->paramTypes.size() - 1;
            std::vector<Ref<Type>> surfaceParamTypes;
            surfaceParamTypes.reserve(std::max(actualArgumentTypes.size(), functionType->paramTypes.size()));
            for (size_t i = 0; i < fixedParameterCount; ++i)
                surfaceParamTypes.push_back(functionType->paramTypes[i]);

            if (usesPackExpansion)
            {
                surfaceParamTypes.push_back(functionType->paramTypes.back());
                return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes, true)
                    .AsFast<FunctionType>();
            }

            for (size_t i = fixedParameterCount; i < actualArgumentTypes.size(); ++i)
                surfaceParamTypes.push_back(actualArgumentTypes[i]);

            return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes, false)
                .AsFast<FunctionType>();
        };

        auto analyzeArgumentsForResolvedFunctionType = [&](const Ref<FunctionType>& functionType,
                                                           const FunctionDeclaration* functionDeclaration,
                                                           bool suppressDiagnostics,
                                                           bool requireExactArity) -> std::optional<std::vector<Ref<Type>>>
        {
            if (!functionType)
                return std::nullopt;

            const bool usesPackExpansion = packExpansionIndex.has_value();
            const bool hasParameterPack = functionType->hasParameterPack;
            const size_t fixedParameterCount = hasParameterPack && !functionType->paramTypes.empty()
                ? functionType->paramTypes.size() - 1
                : functionType->paramTypes.size();
            const size_t requiredArgumentCount = functionDeclaration
                ? getRequiredArgumentCountForDeclaration(functionDeclaration, functionType)
                : (hasParameterPack ? fixedParameterCount : functionType->paramTypes.size());
            const size_t totalParameterCount = functionType->paramTypes.size();

            if (usesPackExpansion)
            {
                if (!hasParameterPack)
                    return std::nullopt;

                if (*packExpansionIndex != fixedParameterCount)
                    return std::nullopt;
            }
            else
            {
                if (node.arguments.size() < requiredArgumentCount)
                    return std::nullopt;

                if (!hasParameterPack && node.arguments.size() > totalParameterCount)
                    return std::nullopt;
            }

            if (requireExactArity)
            {
                if (usesPackExpansion)
                {
                    if (!hasParameterPack || node.arguments.size() != fixedParameterCount + 1)
                        return std::nullopt;
                }
                else if (hasParameterPack)
                {
                    if (node.arguments.size() < fixedParameterCount)
                        return std::nullopt;
                }
                else if (functionType->paramTypes.size() != node.arguments.size())
                {
                    return std::nullopt;
                }
            }

            std::vector<Ref<Type>> resolvedArgumentTypes;
            resolvedArgumentTypes.reserve(node.arguments.size());

            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                Ref<Type> expectedType = nullptr;
                if (usesPackExpansion)
                {
                    if (i < fixedParameterCount)
                        expectedType = functionType->paramTypes[i];
                    else if (i == *packExpansionIndex)
                        expectedType = functionType->paramTypes.back();
                }
                else if (!hasParameterPack && i < functionType->paramTypes.size())
                    expectedType = functionType->paramTypes[i];
                else if (hasParameterPack && i < fixedParameterCount)
                    expectedType = functionType->paramTypes[i];

                auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedType, suppressDiagnostics);
                if (!analyzedType.has_value())
                    return std::nullopt;

                resolvedArgumentTypes.push_back(*analyzedType);
            }

            return resolvedArgumentTypes;
        };

        std::vector<Ref<Symbol>> candidateSymbols;
        bool requiresOverloadResolution = false;
        bool isCallOperatorInvocation = false;
        bool callOperatorLookupFailed = false;
        OperatorDispatchKind callOperatorDispatchKind = OperatorDispatchKind::None;

        auto getCallableDisplayName = [&]() -> std::string
        {
            if (isConstructorCall && constructorStructType)
                return constructorStructType->name;

            if (isCallOperatorInvocation)
                return "operator()";

            if (calleeSym)
                return calleeSym->name;

            if (auto* memberAccess = node.callee->as<MemberAccessExpression>())
                return memberAccess->member ? memberAccess->member->token.value : "<member>";

            if (auto* identifier = node.callee->as<Identifier>())
                return identifier->token.value;

            return "<callable>";
        };

        std::optional<std::vector<Ref<Type>>> cachedUncontextualizedArgumentTypes;
        auto getUncontextualizedArgumentTypes = [&]() -> const std::vector<Ref<Type>>&
        {
            if (!cachedUncontextualizedArgumentTypes.has_value())
            {
                std::vector<Ref<Type>> argumentTypes;
                argumentTypes.reserve(node.arguments.size());

                for (const auto& argument : node.arguments)
                {
                    auto analyzedType = analyzeArgumentWithExpectedType(argument, nullptr, true);
                    argumentTypes.push_back(analyzedType.has_value() ? *analyzedType : Compiler::get().getTypeContext().getUnknown());
                }

                cachedUncontextualizedArgumentTypes = std::move(argumentTypes);
            }

            return *cachedUncontextualizedArgumentTypes;
        };

        auto formatCallArgumentTypesForDiagnostic = [&]() -> std::string
        {
            return formatDiagnosticTypeList(getUncontextualizedArgumentTypes());
        };

        auto diagnosePoisonedCallArguments = [&]() -> bool
        {
            const auto& argumentTypes = getUncontextualizedArgumentTypes();
            bool hasPoisonedArgument = false;
            for (size_t i = 0; i < argumentTypes.size(); ++i)
            {
                if (!argumentTypes[i] || !argumentTypes[i]->isPoisoned())
                    continue;

                hasPoisonedArgument = true;
                // Candidate probing intentionally suppresses diagnostics. Replay
                // only poisoned arguments outside the probe so the root error is
                // preserved without adding a derivative overload diagnostic.
                (void)analyzeArgumentWithExpectedType(node.arguments[i], nullptr, false);
            }

            if (hasPoisonedArgument)
                node.refType = Compiler::get().getTypeContext().getUnknown();
            return hasPoisonedArgument;
        };

        auto scoreResolvedCall = [&](const Ref<FunctionType>& functionType,
                                     const std::vector<Ref<Type>>& actualArgumentTypes,
                                     size_t requiredArgumentCount,
                                     bool isGenericCandidate,
                                     size_t genericParameterCount) -> std::optional<int>
        {
            if (!functionType ||
                actualArgumentTypes.size() < requiredArgumentCount ||
                (!functionType->hasParameterPack && actualArgumentTypes.size() > functionType->paramTypes.size()))
                return std::nullopt;

            int currentScore = 0;
            const size_t fixedParameterCount = functionType->hasParameterPack && !functionType->paramTypes.empty()
                ? functionType->paramTypes.size() - 1
                : functionType->paramTypes.size();
            for (size_t i = 0; i < actualArgumentTypes.size(); ++i)
            {
                if (functionType->hasParameterPack && i >= fixedParameterCount)
                {
                    currentScore += actualArgumentTypes[i] && actualArgumentTypes[i]->kind() == TypeKind::GenericParameterPack ? 4 : 8;
                    continue;
                }

                auto dest = functionType->paramTypes[i];
                const auto& src = actualArgumentTypes[i];

                if (dest->isCompatibleWith(src) || (dest->isNumeric() && src->isNumeric()))
                {
                    if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive &&
                        dest.AsFast<PrimitiveType>()->name == src.AsFast<PrimitiveType>()->name)
                    {
                        currentScore += 1000;
                    }
                    else if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive)
                    {
                        currentScore += 100;
                        auto destName = dest.AsFast<PrimitiveType>()->name;
                        auto srcName = src.AsFast<PrimitiveType>()->name;
                        bool destIsUn = destName.starts_with('u');
                        bool srcIsUn = srcName.starts_with('u');
                        bool destIsInt = destName.starts_with('i');
                        bool srcIsInt = srcName.starts_with('i');
                        bool destIsFlt = destName.starts_with('f');
                        bool srcIsFlt = srcName.starts_with('f');

                        if ((destIsUn && srcIsUn) || (destIsInt && srcIsInt) || (destIsFlt && srcIsFlt))
                            currentScore += 50;

                        auto getSize = [](const std::string& s) -> int
                        {
                            if (s.ends_with("8")) return 1;
                            if (s.ends_with("16")) return 2;
                            if (s.ends_with("32")) return 4;
                            if (s.ends_with("64") || s == "isize" || s == "usize") return 8;
                            return 4;
                        };

                        int sizeDiff = getSize(destName) - getSize(srcName);
                        if (sizeDiff >= 0) currentScore += (10 - sizeDiff);
                    }
                    else
                    {
                        currentScore += 10;
                    }
                }
                else if (isImplicitObjectViewBridge(dest, src))
                {
                    currentScore += 9;
                }
                else if (isSafeRefCast(dest, src))
                {
                    currentScore += 5;
                }
                else
                {
                    return std::nullopt;
                }
            }

            if (!functionType->hasParameterPack)
                currentScore -= static_cast<int>((functionType->paramTypes.size() - actualArgumentTypes.size()) * 25);

              if (genericParameterCount > 0)
                  currentScore -= static_cast<int>(genericParameterCount * 25);

              if (isGenericCandidate)
                  currentScore -= static_cast<int>(std::max<size_t>(1, genericParameterCount) * 10);

            return currentScore;
        };

        auto appendMemberCallOperatorCandidates = [&]() -> bool
        {
            const auto overloadName = common::getCallOperatorOverloadName(TokenType::leftParen);
            if (!overloadName.has_value())
                return false;

            std::unordered_set<const Symbol*> seenSymbols;
            bool accessViolation = false;

            auto appendCandidateSymbol = [&](const Ref<Type>& candidateReceiverType)
            {
                Ref<Type> currentType = unwrapAliasType(candidateReceiverType);
                while (currentType && currentType->kind() == TypeKind::Reference)
                    currentType = unwrapAliasType(currentType.AsFast<ReferenceType>()->referredType);

                if (!currentType || currentType->kind() != TypeKind::Struct)
                    return;

                Ref<Type> ownerType = nullptr;
                Ref<Symbol> candidateSymbol = findStructMemberInHierarchy(currentType, std::string(*overloadName), &ownerType);
                if (!candidateSymbol || seenSymbols.contains(candidateSymbol.Get()))
                    return;

                seenSymbols.insert(candidateSymbol.Get());

                if (!validateStructMemberAccess(currentStructType_, ownerType, candidateSymbol, node.location()))
                {
                    accessViolation = true;
                    return;
                }

                auto ownerStructType = ownerType ? unwrapAliasType(ownerType).AsFast<StructType>() : nullptr;
                if (!ownerStructType)
                    return;

                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    candidateSymbols = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    candidateSymbols = { candidateSymbol };
                else
                    return;

                isCallOperatorInvocation = !candidateSymbols.empty();
                callOperatorDispatchKind = OperatorDispatchKind::Member;
            };

            Ref<Type> calleeTypeForCallOperator = node.callee ? node.callee->refType.Lock() : nullptr;
            appendCandidateSymbol(calleeTypeForCallOperator);
            if (!isCallOperatorInvocation)
            {
                Ref<Type> readableCalleeType = getAutoReadableType(calleeTypeForCallOperator);
                if (readableCalleeType != calleeTypeForCallOperator)
                    appendCandidateSymbol(readableCalleeType);
            }

            if (accessViolation)
            {
                callOperatorLookupFailed = true;
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (isCallOperatorInvocation)
                requiresOverloadResolution = true;

            return isCallOperatorInvocation;
        };

        const bool calleeAlreadyCallable =
            (calleeSym && (calleeSym->kind == SymbolKind::Function || calleeSym->kind == SymbolKind::FunctionGroup)) ||
            (node.callee->refType.Lock() && node.callee->refType.Lock()->kind() == TypeKind::Function);
        if (!calleeAlreadyCallable)
            appendMemberCallOperatorCandidates();
        if (callOperatorLookupFailed)
            return;

        if (calleeSym && calleeSym->kind == SymbolKind::FunctionGroup)
        {
            candidateSymbols = calleeSym->overloads;
            requiresOverloadResolution = true;
        }
        else if (calleeSym && calleeSym->kind == SymbolKind::Function)
        {
            candidateSymbols.push_back(calleeSym);
            Ref<Type> resolvedCalleeType = node.callee->refType.Lock();
            if (!resolvedCalleeType || resolvedCalleeType->kind() != TypeKind::Function)
                resolvedCalleeType = calleeSym->type;
            if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
                resolvedCalleeType = instantiateGenericType(resolvedCalleeType, constructorGenericBindingSet);

            requiresOverloadResolution = (!calleeSym->genericParameterNames.empty() && containsGenericParameterType(resolvedCalleeType)) ||
                                         useExplicitFunctionTypeArguments ||
                                         (isConstructorCall && !constructorGenericParameterNames.empty()) ||
                                          !constructorGenericBindings.empty() ||
                                          !constructorGenericBindingSet.packBindings.empty();
        }

        if (!requiresOverloadResolution)
        {
            for (const auto& candidateSymbol : candidateSymbols)
            {
                const auto* candidateDeclaration = getFunctionDeclarationForSymbol(candidateSymbol);
                if (candidateDeclaration &&
                    getRequiredArgumentCountForDeclaration(
                        candidateDeclaration,
                        candidateSymbol->type.AsFast<FunctionType>()) !=
                        candidateSymbol->type.AsFast<FunctionType>()->paramTypes.size())
                {
                    requiresOverloadResolution = true;
                    break;
                }
            }
        }

        if (requiresOverloadResolution)
        {
            struct ResolvedFunctionCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<Type> functionType = nullptr;
                Ref<FunctionType> fullFunctionType = nullptr;
                int score = -1;
                std::unordered_map<std::string, Ref<Type>> genericBindings;
                GenericBindingSet bindingSet;
                bool usedGenericDefaults = false;
            };

            auto formatCandidateSignature = [&](const Ref<Symbol>& overload) -> std::string
            {
                if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                    return "<invalid overload>";

                const auto functionType = overload->type.AsFast<FunctionType>();
                const auto& genericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                const std::string callableName =
                    isConstructorCall && constructorStructType ? constructorStructType->name :
                    isCallOperatorInvocation ? std::string("operator()") :
                    overload->name;

                return formatFunctionDiagnosticSignature(
                    callableName,
                    genericParameterNames,
                    functionType,
                    isConstructorCall,
                    overload->hasGenericParameterPack
                );
            };

            auto buildCandidateSummary = [&](size_t maxCount = 4) -> std::string
            {
                std::vector<std::string> signatures;
                signatures.reserve(candidateSymbols.size());

                for (const auto& candidate : candidateSymbols)
                {
                    std::string signature = formatCandidateSignature(candidate);
                    appendUniqueValue(signatures, signature);
                }

                std::string summary;
                const size_t displayCount = std::min(maxCount, signatures.size());
                for (size_t i = 0; i < displayCount; ++i)
                {
                    summary += signatures[i];
                    if (i + 1 < displayCount)
                        summary += "; ";
                }

                if (signatures.size() > maxCount)
                    summary += "; ...";

                return summary;
            };

            auto buildAvailableGenericAritySummary = [&]() -> std::vector<size_t>
            {
                std::vector<size_t> arities;
                for (const auto& candidate : candidateSymbols)
                {
                    if (!candidate)
                        continue;

                    const auto& genericParameterNames =
                        isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                    if (!genericParameterNames.empty())
                        appendUniqueValue(arities, genericParameterNames.size());
                }

                std::ranges::sort(arities);
                return arities;
            };

            auto candidateAcceptsExplicitTypeArity = [&](const Ref<Symbol>& candidate, const size_t explicitArity) -> bool
            {
                if (!candidate)
                    return false;

                const auto& genericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                if (genericParameterNames.empty())
                    return false;

                const bool hasCandidatePack = isConstructorCall && constructorStructType
                    ? constructorStructType->hasGenericParameterPack
                    : candidate->hasGenericParameterPack;
                const auto& defaults = isConstructorCall && constructorStructType
                    ? constructorStructType->genericParameterDefaults
                    : candidate->genericParameterDefaults;
                const size_t fixedArity = getMinimumGenericArgumentCount(genericParameterNames, hasCandidatePack);
                const size_t minimumArity = getRequiredGenericArgumentCount(defaults, fixedArity);
                return explicitArity >= minimumArity && (hasCandidatePack || explicitArity <= fixedArity);
            };

            auto formatInstantiationSignature = [&](const std::vector<Ref<Type>>& types) -> std::string
            {
                std::string signature = "<";
                for (size_t i = 0; i < types.size(); ++i)
                {
                    signature += types[i] ? types[i]->toString() : "<unknown>";
                    if (i + 1 < types.size())
                        signature += ", ";
                }
                signature += ">";
                return signature;
            };

            auto isAllowedInstantiateBinding = [&](const Ref<Symbol>& overload,
                                                   const std::vector<std::string>& activeGenericParameterNames,
                                                   const bool hasActiveGenericParameterPack,
                                                   const GenericBindingSet& bindings) -> bool
              {
                  if (!overload || activeGenericParameterNames.empty())
                      return true;

                  auto foundDeclaration = functionDeclarationsBySymbol_.find(overload.Get());
                  if (foundDeclaration == functionDeclarationsBySymbol_.end() || !foundDeclaration->second)
                      return true;

                const auto* functionDeclaration = foundDeclaration->second;
                if (!hasAttribute(functionDeclaration->attributes, Attribute::Native) &&
                    !hasAttribute(functionDeclaration->attributes, Attribute::Export))
                {
                    return true;
                }

                if (overload->resolvedGenericInstantiations.empty())
                    return true;

                auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
                if (!resolvedBindingTypes.has_value())
                    return true;

                for (const auto& instantiationTypes : overload->resolvedGenericInstantiations)
                {
                    if (instantiationTypes.size() != resolvedBindingTypes->size())
                        continue;

                    bool matches = true;
                    for (size_t i = 0; i < instantiationTypes.size(); ++i)
                    {
                        if (!isExactConstraintTypeMatch(instantiationTypes[i], (*resolvedBindingTypes)[i]))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (matches)
                        return true;
                }

                return false;
            };

            auto satisfiesApplyBinding = [&](const Ref<Symbol>& symbol,
                                             const std::vector<std::string>& activeGenericParameterNames,
                                             const bool hasActiveGenericParameterPack,
                                             const GenericBindingSet& bindings) -> bool
            {
                if (!symbol || activeGenericParameterNames.empty())
                    return true;

                auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return true;

                return matchesApplyConstraints(
                    *attributeIt->second,
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
            };

            auto satisfiesOpenNativeTemplateIntrinsicBinding = [&](const Ref<Symbol>& symbol,
                                                                   const std::vector<std::string>& activeGenericParameterNames,
                                                                   const bool hasActiveGenericParameterPack,
                                                                   const GenericBindingSet& bindings) -> bool
            {
                if (!symbol || activeGenericParameterNames.empty())
                    return true;

                auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return true;

                if (!isOpenNativeTemplateIntrinsic(*attributeIt->second))
                    return true;

                auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
                if (!resolvedBindingTypes.has_value())
                    return true;

                return matchesOpenNativeTemplateIntrinsicConstraints(*attributeIt->second, *resolvedBindingTypes);
            };

            bool rejectedByInstantiationWhitelist = false;
            std::string rejectedInstantiationFunctionName;
            std::string rejectedInstantiationSignature;
            bool rejectedByApplyConstraints = false;
            std::string rejectedApplyTargetName;
            std::string rejectedApplySignature;
            bool rejectedByOpenNativeTemplateIntrinsic = false;
            std::string rejectedOpenNativeTemplateTargetName;
            std::string rejectedOpenNativeTemplateSignature;

            auto tryResolveFunctionCandidate = [&](const Ref<Symbol>& overload) -> std::optional<ResolvedFunctionCandidate>
            {
                if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                    return std::nullopt;

                Ref<Type> resolvedFunctionType = overload->type;
                if (overload == calleeSym)
                {
                    if (auto calleeResolvedType = node.callee->refType.Lock();
                        calleeResolvedType && calleeResolvedType->kind() == TypeKind::Function)
                    {
                        resolvedFunctionType = calleeResolvedType;
                    }
                }
                if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, constructorGenericBindingSet);

                auto declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                if (!declaredFunctionType)
                    return std::nullopt;
                const bool declaredFunctionHadParameterPack = declaredFunctionType->hasParameterPack;

                const std::vector<std::string>& activeGenericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                const bool hasGenericParameterPack = isConstructorCall && constructorStructType
                    ? constructorStructType->hasGenericParameterPack
                    : overload->hasGenericParameterPack;
                const std::vector<Ref<Type>>& genericParameterDefaults = isConstructorCall && constructorStructType
                    ? constructorStructType->genericParameterDefaults
                    : overload->genericParameterDefaults;
                GenericBindingSet bindingSet;
                bool usedGenericDefaults = false;
                if (isConstructorCall &&
                    (!constructorGenericBindingSet.directBindings.empty() ||
                     !constructorGenericBindingSet.packBindings.empty() ||
                     !constructorGenericBindingSet.packAliases.empty()))
                {
                    bindingSet = constructorGenericBindingSet;
                }
                bool isGenericCandidate = containsGenericParameterType(resolvedFunctionType);

                if (useExplicitFunctionTypeArguments)
                {
                    if (!isGenericCandidate && activeGenericParameterNames.empty())
                        return std::nullopt;

                    auto completedExplicitArguments = completeGenericTypeArguments(
                        activeGenericParameterNames,
                        genericParameterDefaults,
                        hasGenericParameterPack,
                        explicitTypeArguments);
                    if (!completedExplicitArguments)
                    {
                        return std::nullopt;
                    }
                    usedGenericDefaults = completedExplicitArguments->size() > explicitTypeArguments.size();

                    for (const auto& explicitTypeArgument : *completedExplicitArguments)
                    {
                        if (!explicitTypeArgument ||
                            explicitTypeArgument->isUnknown())
                        {
                            return std::nullopt;
                        }
                    }

                    bindingSet = buildExtendedGenericBindings(
                        activeGenericParameterNames,
                        hasGenericParameterPack,
                        *completedExplicitArguments
                    );
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, bindingSet);
                    declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                    if (!declaredFunctionType)
                        return std::nullopt;

                    isGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                }

                const bool candidateHasParameterPack = declaredFunctionType->hasParameterPack;
                const size_t fixedParameterCount = candidateHasParameterPack && !declaredFunctionType->paramTypes.empty()
                    ? declaredFunctionType->paramTypes.size() - 1
                    : declaredFunctionType->paramTypes.size();
                const auto* candidateDeclaration = getFunctionDeclarationForSymbol(overload);
                size_t requiredArgumentCount = candidateDeclaration
                    ? getRequiredArgumentCountForDeclaration(candidateDeclaration, declaredFunctionType)
                    : (candidateHasParameterPack ? fixedParameterCount : declaredFunctionType->paramTypes.size());

                if (useExplicitFunctionTypeArguments && hasGenericParameterPack && declaredFunctionHadParameterPack && !candidateHasParameterPack && !activeGenericParameterNames.empty())
                {
                    auto packIt = bindingSet.packBindings.find(activeGenericParameterNames.back());
                    if (packIt != bindingSet.packBindings.end())
                        requiredArgumentCount += packIt->second.size();
                }

                if (packExpansionIndex.has_value())
                {
                    if (!candidateHasParameterPack || *packExpansionIndex != fixedParameterCount)
                        return std::nullopt;
                }
                else if (node.arguments.size() < requiredArgumentCount || (!candidateHasParameterPack && node.arguments.size() > declaredFunctionType->paramTypes.size()))
                {
                    return std::nullopt;
                }

                std::vector<std::string> deducibleGenericParameterNames = activeGenericParameterNames;
                if (hasGenericParameterPack && !deducibleGenericParameterNames.empty())
                    deducibleGenericParameterNames.pop_back();

                if (useExplicitFunctionTypeArguments && !isGenericCandidate)
                    deducibleGenericParameterNames.clear();

                std::unordered_map<std::string, const Type*> activeGenericParameterInstances;
                if (isGenericCandidate)
                    collectGenericParameterInstances(resolvedFunctionType, activeGenericParameterNames, activeGenericParameterInstances);

                std::unordered_map<std::string, Ref<Type>> bindings = bindingSet.directBindings;
                std::vector<Ref<Type>> candidateArgTypes(node.arguments.size());
                std::vector<bool> analyzedArguments(node.arguments.size(), false);

                auto getExpectedParameterType = [&](size_t index) -> Ref<Type>
                {
                    Ref<Type> expectedParameterType = nullptr;
                    if (candidateHasParameterPack)
                    {
                        if (index < fixedParameterCount)
                            expectedParameterType = declaredFunctionType->paramTypes[index];
                        else if (packExpansionIndex.has_value() && index == *packExpansionIndex)
                            expectedParameterType = declaredFunctionType->paramTypes.back();
                    }
                    else if (index < declaredFunctionType->paramTypes.size())
                    {
                        expectedParameterType = declaredFunctionType->paramTypes[index];
                    }

                    if (!bindingSet.directBindings.empty() ||
                        !bindingSet.packBindings.empty() ||
                        !bindingSet.packAliases.empty())
                    {
                        expectedParameterType = instantiateGenericType(expectedParameterType, bindingSet);
                    }
                    return expectedParameterType;
                };

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    const bool usesPackExpansionArgument = packExpansionIndex.has_value() && i == *packExpansionIndex;
                    const bool isConcretePackTailArgument = candidateHasParameterPack && !packExpansionIndex.has_value() && i >= fixedParameterCount;
                    bool shouldDeferArgument =
                        isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        isContextSensitiveArgument(node.arguments[i]) &&
                        containsNamedGenericParameterType(expectedParameterType, deducibleGenericParameterNames);

                    if (shouldDeferArgument)
                        continue;

                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }

                    bindingSet.directBindings = bindings;
                }

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    if (analyzedArguments[i])
                        continue;

                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    const bool usesPackExpansionArgument = packExpansionIndex.has_value() && i == *packExpansionIndex;
                    const bool isConcretePackTailArgument = candidateHasParameterPack && !packExpansionIndex.has_value() && i >= fixedParameterCount;
                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }

                    bindingSet.directBindings = bindings;
                }

                if (std::ranges::any_of(analyzedArguments, [](bool analyzed) { return !analyzed; }))
                    return std::nullopt;

                // Deduction wins for parameters mentioned by the function
                // signature. Defaults then fill only the remaining holes.
                const size_t fixedGenericParameterCount = getMinimumGenericArgumentCount(
                    activeGenericParameterNames, hasGenericParameterPack);
                for (size_t genericIndex = 0; genericIndex < fixedGenericParameterCount; ++genericIndex)
                {
                    const std::string& parameterName = activeGenericParameterNames[genericIndex];
                    if (bindings.contains(parameterName) && bindings.at(parameterName) && !bindings.at(parameterName)->isUnknown())
                        continue;
                    if (genericIndex >= genericParameterDefaults.size() || !genericParameterDefaults[genericIndex])
                        continue;

                    bindingSet.directBindings = bindings;
                    Ref<Type> defaultType = instantiateGenericType(genericParameterDefaults[genericIndex], bindingSet);
                    bindings[parameterName] = defaultType;
                    bindingSet.directBindings[parameterName] = defaultType;
                    usedGenericDefaults = true;
                }

                if (isGenericCandidate)
                {
                    for (const auto& genericParameterName : deducibleGenericParameterNames)
                    {
                        if (!bindings.contains(genericParameterName) ||
                            !bindings.at(genericParameterName) ||
                            bindings.at(genericParameterName)->isUnknown())
                        {
                            return std::nullopt;
                        }
                    }
                }

                bindingSet.directBindings = bindings;
                if (hasGenericParameterPack &&
                    candidateHasParameterPack &&
                    !activeGenericParameterNames.empty() &&
                    !packExpansionIndex.has_value())
                {
                    const std::string& packName = activeGenericParameterNames.back();
                    if (!bindingSet.packBindings.contains(packName) && !bindingSet.packAliases.contains(packName))
                    {
                        std::vector<Ref<Type>> packBindingTypes;
                        if (candidateArgTypes.size() > fixedParameterCount)
                        {
                            packBindingTypes.reserve(candidateArgTypes.size() - fixedParameterCount);
                            for (size_t i = fixedParameterCount; i < candidateArgTypes.size(); ++i)
                            {
                                packBindingTypes.push_back(candidateArgTypes[i]);
                                bindingSet.directBindings.emplace(
                                    makePackElementBindingName(packName, i - fixedParameterCount),
                                    candidateArgTypes[i]
                                );
                            }
                        }

                        bindingSet.packBindings.emplace(packName, std::move(packBindingTypes));
                    }
                }

                if (!activeGenericParameterNames.empty())
                {
                    if (!isAllowedInstantiateBinding(overload, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByInstantiationWhitelist = true;
                        rejectedInstantiationFunctionName = overload->name;
                        rejectedInstantiationSignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }

                    Ref<Symbol> applyConstraintOwner = isConstructorCall ? genericOwnerSym : overload;
                    if (!satisfiesApplyBinding(applyConstraintOwner, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByApplyConstraints = true;
                        rejectedApplyTargetName =
                            isConstructorCall && constructorStructType ? constructorStructType->name : overload->name;
                        rejectedApplySignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }

                    if (!satisfiesOpenNativeTemplateIntrinsicBinding(overload, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByOpenNativeTemplateIntrinsic = true;
                        rejectedOpenNativeTemplateTargetName = overload->name;
                        rejectedOpenNativeTemplateSignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }
                }

                if (!bindingSet.directBindings.empty() ||
                    !bindingSet.packBindings.empty() ||
                    !bindingSet.packAliases.empty())
                {
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, bindingSet);
                    declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                    if (!declaredFunctionType)
                        return std::nullopt;
                }

                const bool remainsGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                if (auto score = scoreResolvedCall(
                        resolvedFunctionType.AsFast<FunctionType>(),
                        candidateArgTypes,
                        requiredArgumentCount,
                        remainsGenericCandidate,
                        activeGenericParameterNames.size()
                    ); score.has_value())
                {
                    auto callableSurfaceType = buildCallableSurfaceFunctionType(
                        resolvedFunctionType.AsFast<FunctionType>(),
                        candidateArgTypes,
                        packExpansionIndex.has_value()
                    );
                    return ResolvedFunctionCandidate{
                        .symbol = overload,
                        .functionType = callableSurfaceType ? callableSurfaceType : resolvedFunctionType,
                        .fullFunctionType = resolvedFunctionType.AsFast<FunctionType>(),
                        .score = *score,
                        .genericBindings = bindings,
                        .bindingSet = bindingSet,
                        .usedGenericDefaults = usedGenericDefaults
                    };
                }

                return std::nullopt;
            };

            std::optional<ResolvedFunctionCandidate> bestMatch;
            bool isAmbiguous = false;

            for (const auto& overload : candidateSymbols)
            {
                auto resolvedCandidate = tryResolveFunctionCandidate(overload);
                if (!resolvedCandidate.has_value())
                    continue;

                if (!bestMatch.has_value() || resolvedCandidate->score > bestMatch->score)
                {
                    bestMatch = resolvedCandidate;
                    isAmbiguous = false;
                }
                else if (resolvedCandidate->score == bestMatch->score)
                {
                    isAmbiguous = true;
                }
            }

            if (isAmbiguous)
            {
                if (diagnosePoisonedCallArguments())
                    return;

                const std::string candidateSummary = buildCandidateSummary();
                if (candidateSummary.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Ambiguous function call to '{}' with arguments {}.",
                        getCallableDisplayName(),
                        formatCallArgumentTypesForDiagnostic()
                    );
                }
                else
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Ambiguous function call to '{}' with arguments {}. Candidates: {}.",
                        getCallableDisplayName(),
                        formatCallArgumentTypesForDiagnostic(),
                        candidateSummary
                    );
                }
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            if (!bestMatch.has_value())
            {
                if (diagnosePoisonedCallArguments())
                    return;

                if (rejectedByInstantiationWhitelist)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic interop function '{}' does not declare @Instantiate{}.",
                        rejectedInstantiationFunctionName,
                        rejectedInstantiationSignature
                    );
                }
                else if (rejectedByApplyConstraints)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic callable '{}' rejects type arguments {} because of @Apply constraints.",
                        rejectedApplyTargetName,
                        rejectedApplySignature
                    );
                }
                else if (rejectedByOpenNativeTemplateIntrinsic)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic native callable '{}' accepts only enum or flagset type arguments. Got {}.",
                        rejectedOpenNativeTemplateTargetName,
                        rejectedOpenNativeTemplateSignature
                    );
                }
                else
                {
                    const auto availableGenericArities = buildAvailableGenericAritySummary();
                    const bool providedExplicitTypeArguments = useExplicitFunctionTypeArguments || (isConstructorCall && !explicitTypeArguments.empty());
                    bool anyCandidateAcceptsExplicitTypeArity = false;
                    if (providedExplicitTypeArguments)
                    {
                        for (const auto& candidate : candidateSymbols)
                        {
                            if (candidateAcceptsExplicitTypeArity(candidate, explicitTypeArguments.size()))
                            {
                                anyCandidateAcceptsExplicitTypeArity = true;
                                break;
                            }
                        }
                    }

                    if (providedExplicitTypeArguments &&
                        !availableGenericArities.empty() &&
                        !anyCandidateAcceptsExplicitTypeArity)
                    {
                        std::vector<size_t> exactArities;
                        std::vector<size_t> minimumPackArities;
                        for (const auto& candidate : candidateSymbols)
                        {
                            if (!candidate)
                                continue;

                            const auto& genericParameterNames =
                                isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                            if (genericParameterNames.empty())
                                continue;

                            if (candidate->hasGenericParameterPack)
                                appendUniqueValue(minimumPackArities, getMinimumGenericArgumentCount(genericParameterNames, true));
                            else
                                appendUniqueValue(exactArities, genericParameterNames.size());
                        }

                        std::ranges::sort(exactArities);
                        std::ranges::sort(minimumPackArities);

                        if (exactArities.empty() && minimumPackArities.size() == 1)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "Generic call to '{}' expects at least {} explicit type arguments, but got {}.",
                                getCallableDisplayName(),
                                minimumPackArities.front(),
                                explicitTypeArguments.size()
                            );
                        }
                        else if (availableGenericArities.size() == 1 && minimumPackArities.empty())
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "Generic call to '{}' expects {} explicit type arguments, but got {}.",
                                getCallableDisplayName(),
                                availableGenericArities.front(),
                                explicitTypeArguments.size()
                            );
                        }
                        else
                        {
                            std::string arityList;
                            bool needsSeparator = false;
                            for (size_t i = 0; i < exactArities.size(); ++i)
                            {
                                if (needsSeparator)
                                    arityList += ", ";
                                arityList += std::to_string(exactArities[i]);
                                needsSeparator = true;
                            }

                            for (size_t i = 0; i < minimumPackArities.size(); ++i)
                            {
                                if (needsSeparator)
                                    arityList += ", ";
                                arityList += common::formatString("{}+", minimumPackArities[i]);
                                needsSeparator = true;
                            }

                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No overload of '{}' accepts {} explicit type arguments. Available generic arities: {}.",
                                getCallableDisplayName(),
                                explicitTypeArguments.size(),
                                arityList
                            );
                        }
                    }
                    else
                    {
                        const std::string candidateSummary = buildCandidateSummary();
                        if (candidateSummary.empty())
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No matching overload for '{}' with arguments {}.",
                                getCallableDisplayName(),
                                formatCallArgumentTypesForDiagnostic()
                            );
                        }
                        else
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No matching overload for '{}' with arguments {}. Candidates: {}.",
                                getCallableDisplayName(),
                                formatCallArgumentTypesForDiagnostic(),
                                candidateSummary
                            );
                        }
                    }
                }
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (isConstructorCall &&
                constructorStructType &&
                !constructorGenericParameterNames.empty() &&
                constructorGenericBindings.empty() &&
                constructorGenericBindingSet.packBindings.empty())
            {
                auto deducedGenericArguments = tryMaterializeConcreteInstantiation(
                    constructorGenericParameterNames,
                    constructorStructType->hasGenericParameterPack,
                    bestMatch->bindingSet
                );
                if (!deducedGenericArguments.has_value())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Cannot infer generic arguments for constructor '{}'. Use explicit type arguments.",
                        constructorStructType->name
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                constructorGenericBindings = bestMatch->bindingSet.directBindings;
                constructorGenericBindingSet = bestMatch->bindingSet;
                structReturnType = instantiateGenericStructType(constructorStructType, *deducedGenericArguments, node.location());
                node.callee->refType = structReturnType;
            }

            auto getConcreteCallableOwnerType = [&]() -> Ref<StructType>
            {
                auto resolveStructReceiverType = [&](const Ref<Type>& candidateType) -> Ref<StructType>
                {
                    Ref<Type> resolvedType = unwrapAliasType(candidateType);
                    while (resolvedType && resolvedType->kind() == TypeKind::Reference)
                        resolvedType = unwrapAliasType(resolvedType.AsFast<ReferenceType>()->referredType);
                    return resolvedType && resolvedType->kind() == TypeKind::Struct
                        ? resolvedType.AsFast<StructType>()
                        : nullptr;
                };

                if (isConstructorCall)
                    return resolveStructReceiverType(structReturnType);
                if (isCallOperatorInvocation)
                    return resolveStructReceiverType(node.callee ? node.callee->refType.Lock() : nullptr);
                if (const auto* memberAccess = node.callee ? node.callee->as<MemberAccessExpression>() : nullptr)
                    return resolveStructReceiverType(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
                return nullptr;
            };

            if (bestMatch->symbol)
            {
                if (!isConstructorCall && bestMatch->usedGenericDefaults && !bestMatch->symbol->genericParameterNames.empty())
                {
                    if (auto resolvedArguments = tryMaterializeConcreteInstantiation(
                            bestMatch->symbol->genericParameterNames,
                            bestMatch->symbol->hasGenericParameterPack,
                            bestMatch->bindingSet))
                    {
                        node.resolvedGenericArguments.clear();
                        node.resolvedGenericArguments.reserve(resolvedArguments->size());
                        for (const auto& argument : *resolvedArguments)
                            node.resolvedGenericArguments.emplace_back(argument);
                    }
                }

                Ref<Symbol> validationSymbol = bestMatch->symbol->flags.get_isExtension() &&
                                               bestMatch->symbol->extensionImplementation
                    ? bestMatch->symbol->extensionImplementation
                    : bestMatch->symbol;
                const FunctionDeclaration* validationDeclaration =
                    getFunctionDeclarationForSymbol(validationSymbol);
                Ref<FunctionType> validationFunctionType = bestMatch->fullFunctionType;
                if (validationSymbol != bestMatch->symbol && validationSymbol->type &&
                    validationSymbol->type->kind() == TypeKind::Function)
                {
                    Ref<Type> instantiatedValidationType = instantiateGenericType(
                        validationSymbol->type,
                        bestMatch->bindingSet);
                    validationFunctionType = instantiatedValidationType.AsFast<FunctionType>();
                }
                Ref<StructType> concreteOwnerType = getConcreteCallableOwnerType();
                if (validationDeclaration &&
                    validationFunctionType &&
                    !validateConcreteGenericFunctionBody(
                        *validationDeclaration,
                        validationSymbol,
                        validationFunctionType,
                        concreteOwnerType,
                        bestMatch->bindingSet.directBindings,
                        bestMatch->bindingSet.packBindings,
                        bestMatch->bindingSet.packAliases))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }

            if (isCallOperatorInvocation)
            {
                node.referencedSymbol = bestMatch->symbol;
                node.operatorDispatchKind = callOperatorDispatchKind;
                node.overloadFunctionType = bestMatch->functionType.AsFast<Type>();
            }
            else if (!isConstructorCall)
            {
                node.callee->referencedSymbol = bestMatch->symbol;
                node.callee->refType = bestMatch->functionType;
            }
            else if (constructorStructType && !constructorGenericParameterNames.empty() && (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty()))
            {
                node.callee->refType = structReturnType;
            }

            if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    bestMatch->functionType.AsFast<FunctionType>(),
                    nullptr,
                    false,
                    false
                ); !resolvedArgumentTypes.has_value())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            validateExecutorTransfer(node, bestMatch->symbol);

            if (!finalizeCallResultType(isConstructorCall ? structReturnType : bestMatch->functionType.AsFast<FunctionType>()->returnType))
                return;
            return;
        }

        Ref<Type> calleeType = node.callee->refType.Lock();
        if (auto* memberAccess = node.callee->as<MemberAccessExpression>();
            memberAccess && !memberAccess->intrinsicOverloadMembers.empty())
        {
            std::optional<size_t> bestIndex;
            std::optional<int> bestScore;
            bool isAmbiguous = false;

            for (size_t i = 0; i < memberAccess->intrinsicOverloadTypes.size(); ++i)
            {
                Ref<Type> overloadType = memberAccess->intrinsicOverloadTypes[i].Lock();
                if (!overloadType || overloadType->kind() != TypeKind::Function)
                    continue;

                auto overloadArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    overloadType.AsFast<FunctionType>(),
                    nullptr,
                    true,
                    true
                );
                if (!overloadArgumentTypes.has_value())
                    continue;

                auto score = scoreResolvedCall(overloadType.AsFast<FunctionType>(), *overloadArgumentTypes, overloadType.AsFast<FunctionType>()->paramTypes.size(), false, 0);
                if (!score.has_value())
                    continue;

                if (!bestIndex.has_value() || *score > *bestScore)
                {
                    bestIndex = i;
                    bestScore = *score;
                    isAmbiguous = false;
                }
                else if (*score == *bestScore)
                {
                    isAmbiguous = true;
                }
            }

            if (isAmbiguous)
            {
                if (diagnosePoisonedCallArguments())
                    return;

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous function call to intrinsic member '{}' with arguments {}.",
                    memberAccess->member->token.value,
                    formatCallArgumentTypesForDiagnostic()
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!bestIndex.has_value())
            {
                if (diagnosePoisonedCallArguments())
                    return;

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "No matching overload for intrinsic member '{}' with arguments {}.",
                    memberAccess->member->token.value,
                    formatCallArgumentTypesForDiagnostic()
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            memberAccess->intrinsicMember = memberAccess->intrinsicOverloadMembers[*bestIndex];
            memberAccess->refType = memberAccess->intrinsicOverloadTypes[*bestIndex].Lock();
            node.callee->refType = memberAccess->refType;
            calleeType = memberAccess->refType.Lock();

            if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    calleeType.AsFast<FunctionType>(),
                    nullptr,
                    false,
                    false
                ); !resolvedArgumentTypes.has_value())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (isMutatingIntrinsicMember(memberAccess->intrinsicMember) && !canMutateIntrinsicReceiver(memberAccess->object))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Container member '{}' requires a mutable receiver.", memberAccess->member->token.value);
            }

            if (!calleeType || calleeType->kind() != TypeKind::Function)
            {
                WIO_LOG_ADD_ERROR(node.location(), node.isPipelineCall
                    ? "Pipeline target is not callable."
                    : "Called expression is undefined.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!finalizeCallResultType(calleeType.AsFast<FunctionType>()->returnType))
                return;
            return;
        }

        if (!calleeSym && (!calleeType || calleeType->kind() != TypeKind::Function))
        {
            if (calleeType && calleeType->isPoisoned())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            WIO_LOG_ADD_ERROR(node.location(), node.isPipelineCall
                ? "Pipeline target is not callable."
                : "Called expression is undefined.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (!calleeType || calleeType->kind() != TypeKind::Function)
            calleeType = calleeSym->type;
        if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
            calleeType = instantiateGenericType(calleeType, constructorGenericBindingSet);

        if (!calleeType || calleeType->kind() != TypeKind::Function)
        {
            WIO_LOG_ADD_ERROR(node.location(), node.isPipelineCall
                ? "Pipeline target is not callable."
                : "Called expression is not a function or struct.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto funcType = calleeType.AsFast<FunctionType>();
        std::vector<Ref<Type>> argTypes;
        const auto* directFunctionDeclaration = getFunctionDeclarationForSymbol(calleeSym);
        if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(funcType, directFunctionDeclaration, false, false);
            resolvedArgumentTypes.has_value())
        {
            argTypes = std::move(*resolvedArgumentTypes);
        }
        else
        {
            const size_t requiredArgumentCount = getRequiredArgumentCountForCallable(calleeSym, funcType);
            const size_t totalParameterCount = funcType ? funcType->paramTypes.size() : 0;
            if (node.arguments.size() < requiredArgumentCount || (!funcType->hasParameterPack && node.arguments.size() > totalParameterCount))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Function expects '{}' arguments, but got '{}'.",
                    funcType->hasParameterPack
                        ? common::formatString("{} or more", requiredArgumentCount)
                        : formatExpectedArgumentCountDescription(requiredArgumentCount, totalParameterCount),
                    node.arguments.size()
                );
            }
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        const size_t requiredArgumentCount = getRequiredArgumentCountForCallable(calleeSym, funcType);
        if (argTypes.size() < requiredArgumentCount || (!funcType->hasParameterPack && argTypes.size() > funcType->paramTypes.size()))
            WIO_LOG_ADD_ERROR(node.location(), "Function expects '{}' arguments, but got '{}'.",
                funcType->hasParameterPack
                    ? common::formatString("{} or more", requiredArgumentCount)
                    : formatExpectedArgumentCountDescription(requiredArgumentCount, funcType->paramTypes.size()),
                argTypes.size());

        const size_t fixedParameterCount = funcType->hasParameterPack && !funcType->paramTypes.empty()
            ? funcType->paramTypes.size() - 1
            : funcType->paramTypes.size();
        size_t argCount = funcType->hasParameterPack ? std::min(argTypes.size(), fixedParameterCount) : std::min(argTypes.size(), funcType->paramTypes.size());
        for (size_t i = 0; i < argCount; ++i)
        {
            auto expectedType = funcType->paramTypes[i];
            const auto& actualType = argTypes[i];

            if (!expectedType || !actualType || expectedType->isPoisoned() || actualType->isPoisoned())
                continue;

            if (!isAssignmentLikeCompatible(expectedType, actualType) &&
                !isImplicitObjectViewBridge(expectedType, actualType) &&
                !isSafeRefCast(expectedType, actualType))
            {
                if (isRejectedImplicitNumericConversion(expectedType, actualType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.arguments[i]->location(),
                        "Implicit narrowing argument conversion from '{}' to '{}' at index {} requires explicit 'fit'.",
                        actualType->toString(), expectedType->toString(), i);
                    continue;
                }

                std::string extraHint;

                if (expectedType->kind() == TypeKind::Reference && actualType->kind() == TypeKind::Reference)
                {
                    auto expRef = expectedType.AsFast<ReferenceType>();
                    auto actRef = actualType.AsFast<ReferenceType>();

                    if (expRef->isMutable && !actRef->isMutable)
                    {
                        extraHint = " Hint: The function expects a mutable reference ('ref'), but a read-only reference ('view') or immutable variable ('let') was provided.";
                    }
                    else if (!isTypeDerivedFrom(actRef->referredType, expRef->referredType))
                    {
                        extraHint = " Hint: Type '" + actRef->referredType->toString() + "' does not inherit from '" + expRef->referredType->toString() + "'.";
                    }
                }

                WIO_LOG_ADD_ERROR(node.arguments[i]->location(),
                    "Argument mismatch at index {}: Expected '{}', but got '{}'.{}",
                    i,
                    expectedType->toString(),
                    actualType->toString(),
                    extraHint);
            }
        }

        validateExecutorTransfer(node, calleeSym);

        finalizeCallResultType(isConstructorCall ? structReturnType : funcType->returnType);
    }
