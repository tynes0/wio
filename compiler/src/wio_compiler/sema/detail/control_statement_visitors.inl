// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(IfStatement& node)
    {
        node.condition->accept(*this);

        const auto narrowedSymbolForNullComparison = [&](bool nonNullBranch) -> Ref<Symbol>
        {
            auto binary = node.condition->as<BinaryExpression>();
            if (!binary ||
                (binary->op.type != TokenType::opEqual && binary->op.type != TokenType::opNotEqual))
            {
                return nullptr;
            }

            const bool leftIsNull = binary->left->is<NullExpression>();
            const bool rightIsNull = binary->right->is<NullExpression>();
            if (leftIsNull == rightIsNull)
                return nullptr;

            auto candidate = leftIsNull ? binary->right : binary->left;
            auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
            Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
            if (!symbolType || symbolType->kind() != TypeKind::Nullable)
                return nullptr;

            const bool conditionMeansNonNull = binary->op.type == TokenType::opNotEqual;
            return conditionMeansNonNull == nonNullBranch ? symbol : nullptr;
        };

        const auto previousNarrowing = nonNullNarrowedSymbols_;
        if (auto thenNarrowed = narrowedSymbolForNullComparison(true))
            nonNullNarrowedSymbols_.insert(thenNarrowed.Get());

        auto ifScope = Ref<Scope>::Create(currentScope_, ScopeKind::Block);
        auto prevScope = currentScope_;
        currentScope_ = ifScope;

        if (node.matchVar.isValid())
        {
            if (node.condition->is<BinaryExpression>())
            {
                auto binExpr = node.condition->as<BinaryExpression>();
                auto typeSym = binExpr->right->referencedSymbol.Lock();
                Ref<StructType> targetStruct = (typeSym && typeSym->kind == SymbolKind::Struct)
                    ? getObjectOrInterfaceStructType(typeSym->type)
                    : nullptr;
                Ref<Type> lhsType = getAutoReadableType(binExpr->left->refType.Lock());
                Ref<Type> targetType = binExpr->right->refType.Lock();

                if (binExpr->op.type == TokenType::kwIs && isAnyType(lhsType) && isSupportedAnyCastTargetType(targetType))
                {
                    auto varSym = createSymbol(node.matchVar.value, targetType, SymbolKind::Variable, node.matchVar.loc);
                    currentScope_->define(node.matchVar.value, varSym);
                }
                else if (binExpr->op.type == TokenType::kwIs && targetStruct)
                {
                    auto refType = Compiler::get().getTypeContext().getOrCreateReferenceType(typeSym->type, false);
                    auto varSym = createSymbol(node.matchVar.value, refType, SymbolKind::Variable, node.matchVar.loc);
                    currentScope_->define(node.matchVar.value, varSym);
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.matchVar.loc, "Pattern matching 'fit' can only be used with the 'is' operator (e.g., target is Boss fit t).");
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.matchVar.loc, "Pattern matching 'fit' can only be used with the 'is' operator (e.g., target is Boss fit t).");
            }
        }

        if (node.thenBranch) node.thenBranch->accept(*this);

        currentScope_ = prevScope;
        nonNullNarrowedSymbols_ = previousNarrowing;
        if (auto elseNarrowed = narrowedSymbolForNullComparison(false))
            nonNullNarrowedSymbols_.insert(elseNarrowed.Get());
        if (node.elseBranch) node.elseBranch->accept(*this);
        nonNullNarrowedSymbols_ = previousNarrowing;

        const auto definitelyReturns = [&](const auto& self, const NodePtr<Statement>& statement) -> bool
        {
            if (!statement)
                return false;
            if (statement->is<ReturnStatement>())
                return true;
            if (auto block = statement->as<BlockStatement>())
                return !block->statements.empty() && self(self, block->statements.back());
            return false;
        };

        if (!node.elseBranch && definitelyReturns(definitelyReturns, node.thenBranch))
        {
            if (auto guardNarrowed = narrowedSymbolForNullComparison(false))
                nonNullNarrowedSymbols_.insert(guardNarrowed.Get());
        }
    }

    void SemanticAnalyzer::visit(WhileStatement& node)
    {
        if (isDeclarationPass_) return;
        node.condition->accept(*this);

        if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
        {
            // Todo: Check null (i.g. it's not works well)
            if (!allowsNumericSemantics(condType) && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null)
            {
                WIO_LOG_ADD_ERROR(
                    node.condition->location(),
                    "While condition must be a boolean, numeric, or reference type. Got: {}",
                    condType->toString()
                );
            }
        }

        const auto previousNarrowing = nonNullNarrowedSymbols_;
        if (auto comparison = node.condition->as<BinaryExpression>();
            comparison && comparison->op.type == TokenType::opNotEqual)
        {
            const bool leftIsNull = comparison->left->is<NullExpression>();
            const bool rightIsNull = comparison->right->is<NullExpression>();
            if (leftIsNull != rightIsNull)
            {
                auto candidate = leftIsNull ? comparison->right : comparison->left;
                auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
                Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
                if (symbol && symbolType && symbolType->kind() == TypeKind::Nullable)
                    nonNullNarrowedSymbols_.insert(symbol.Get());
            }
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;
        nonNullNarrowedSymbols_ = previousNarrowing;
    }

    void SemanticAnalyzer::visit(ForInStatement& node)
    {
        if (isDeclarationPass_) return;

        node.iterable->accept(*this);
        if (node.step)
            node.step->accept(*this);

        Ref<Type> stepType = node.step ? getAutoReadableType(node.step->refType.Lock()) : nullptr;

        Ref<Type> iterableType = node.iterable->refType.Lock();
        while (iterableType && iterableType->kind() == TypeKind::Alias)
            iterableType = iterableType.AsFast<AliasType>()->aliasedType;
        while (iterableType && iterableType->kind() == TypeKind::Reference)
            iterableType = iterableType.AsFast<ReferenceType>()->referredType;
        while (iterableType && iterableType->kind() == TypeKind::Alias)
            iterableType = iterableType.AsFast<AliasType>()->aliasedType;

        enterScope(ScopeKind::Block);

        auto getBindingMode = [&](size_t index) -> ForBindingMode
        {
            if (index < node.bindingModes.size())
                return node.bindingModes[index];

            return ForBindingMode::ValueImmutable;
        };

        auto createLoopBindingType = [&](const Ref<Type>& valueType, ForBindingMode bindingMode) -> Ref<Type>
        {
            switch (bindingMode)
            {
            case ForBindingMode::ValueMutable:
            case ForBindingMode::ValueImmutable:
                return valueType;
            case ForBindingMode::ReferenceMutable:
                return Compiler::get().getTypeContext().getOrCreateReferenceType(valueType, true);
            case ForBindingMode::ReferenceView:
                return Compiler::get().getTypeContext().getOrCreateReferenceType(valueType, false);
            }

            return valueType;
        };

        auto createBindingSymbol = [&](const NodePtr<Identifier>& binding, const Ref<Type>& bindingType, ForBindingMode bindingMode)
        {
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (bindingMode == ForBindingMode::ValueMutable || bindingMode == ForBindingMode::ReferenceMutable)
                flags.set_isMutable(true);

            Ref<Symbol> bindingSym = createSymbol(binding->token.value, bindingType, SymbolKind::Variable, binding->location(), flags);
            currentScope_->define(binding->token.value, bindingSym);
            binding->referencedSymbol = bindingSym;
            binding->refType = bindingType;
        };

        node.bindingAccessors.clear();

        if (node.iterable->is<RangeExpression>())
        {
            auto rangeExpr = node.iterable->as<RangeExpression>();
            Ref<Type> startType = getAutoReadableType(rangeExpr->start->refType.Lock());
            Ref<Type> endType = getAutoReadableType(rangeExpr->end->refType.Lock());

            if (node.bindings.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration requires exactly 1 binding.");
            }
            else if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration does not support 'ref' or 'view' bindings.");
            }
            else if (!allowsIntegerSemantics(startType) || !allowsIntegerSemantics(endType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration currently requires integer bounds.");
            }
            else if (node.step && !allowsIntegerSemantics(stepType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range step expressions must be integer values.");
            }
            else if (node.step && isZeroIntegerLiteralExpression(node.step))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range step cannot be zero.");
            }
            else
            {
                Ref<Type> rangeValueType = startType;
                if (!rangeValueType || !rangeValueType->isCompatibleWith(endType))
                    rangeValueType = endType;

                createBindingSymbol(node.bindings[0], createLoopBindingType(rangeValueType, getBindingMode(0)), getBindingMode(0));
            }
        }
        else if (iterableType && iterableType->kind() == TypeKind::Array)
        {
            auto arrayType = iterableType.AsFast<ArrayType>();
            Ref<Type> elementType = arrayType->elementType;
            Ref<Type> indexType = Compiler::get().getTypeContext().getUSize();

            if (node.step && !allowsIntegerSemantics(stepType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array step expressions must be integer values.");
            }
            else if (node.step && isZeroIntegerLiteralExpression(node.step))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array step cannot be zero.");
            }

            if (node.bindings.size() == 1)
            {
                createBindingSymbol(node.bindings[0], createLoopBindingType(elementType, getBindingMode(0)), getBindingMode(0));
            }
            else if (elementType && elementType->kind() == TypeKind::Struct)
            {
                auto structType = elementType.AsFast<StructType>();
                if (structType->isObject || structType->isInterface)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Destructuring in array loops currently supports component-like POD structs only.");
                }
                else if (node.bindings.size() == structType->fieldNames.size())
                {
                    for (size_t i = 0; i < node.bindings.size(); ++i)
                    {
                        node.bindingAccessors.push_back(structType->fieldNames[i]);
                        createBindingSymbol(
                            node.bindings[i],
                            createLoopBindingType(structType->fieldTypes[i], getBindingMode(i)),
                            getBindingMode(i)
                        );
                    }
                }
                else if (node.bindings.size() == structType->fieldNames.size() + 1)
                {
                    if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                    }
                    else
                    {
                        node.bindingAccessors.emplace_back("__index__");
                        createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));

                        for (size_t i = 1; i < node.bindings.size(); ++i)
                        {
                            node.bindingAccessors.push_back(structType->fieldNames[i - 1]);
                            createBindingSymbol(
                                node.bindings[i],
                                createLoopBindingType(structType->fieldTypes[i - 1], getBindingMode(i)),
                                getBindingMode(i)
                            );
                        }
                    }
                }
                else if (node.bindings.size() == 2)
                {
                    if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                    }
                    else
                    {
                        node.bindingAccessors = { "__index__", "__value__" };
                        createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));
                        createBindingSymbol(node.bindings[1], createLoopBindingType(elementType, getBindingMode(1)), getBindingMode(1));
                    }
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Component binding expects {} names, or {} with an index binding, but {} were provided.",
                        structType->fieldNames.size(), structType->fieldNames.size() + 1, node.bindings.size());
                }
            }
            else if (node.bindings.size() == 2)
            {
                if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                }
                else
                {
                    node.bindingAccessors = { "__index__", "__value__" };
                    createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));
                    createBindingSymbol(node.bindings[1], createLoopBindingType(elementType, getBindingMode(1)), getBindingMode(1));
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array iteration supports either a single value binding or 'index | value'.");
            }
        }
        else if (iterableType && iterableType->kind() == TypeKind::Dictionary)
        {
            if (node.step)
                WIO_LOG_ADD_ERROR(node.location(), "Step clauses are currently supported only for range or array iteration.");

            auto dictType = iterableType.AsFast<DictionaryType>();
            if (node.bindings.size() != 2)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Dictionary iteration requires exactly 2 bindings: key | value.");
            }
            else
            {
                const ForBindingMode keyMode = getBindingMode(0);
                const ForBindingMode valueMode = getBindingMode(1);

                if (keyMode == ForBindingMode::ReferenceMutable)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Dictionary keys are immutable. Use a value binding or 'view' for the key.");
                }
                else
                {
                    node.bindingAccessors = { "first", "second" };
                    createBindingSymbol(node.bindings[0], createLoopBindingType(dictType->keyType, keyMode), keyMode);
                    createBindingSymbol(node.bindings[1], createLoopBindingType(dictType->valueType, valueMode), valueMode);
                }
            }
        }
        else
        {
            const std::string actualType = iterableType ? iterableType->toString() : "<unknown>";
            WIO_LOG_ADD_ERROR(node.location(), "For-in loops currently require an array or dictionary type. Got '{}'.", actualType);
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;

        exitScope();
    }

    void SemanticAnalyzer::visit(CForStatement& node)
    {
        if (isDeclarationPass_) return;

        enterScope(ScopeKind::Block);

        if (node.initializer)
            node.initializer->accept(*this);

        if (node.condition)
        {
            node.condition->accept(*this);

            if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
            {
                if (!allowsNumericSemantics(condType) && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.condition->location(),
                        "For-loop condition must be a boolean, numeric, or reference type. Got: {}",
                        condType->toString()
                    );
                }
            }
        }

        if (node.increment)
            node.increment->accept(*this);

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;

        exitScope();
    }

    void SemanticAnalyzer::visit(BreakStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'break' statement can only be used inside a loop.");
    }

    void SemanticAnalyzer::visit(ContinueStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'continue' statement can only be used inside a loop.");
    }

    void SemanticAnalyzer::visit(ReturnStatement& node)
    {
        if (isDeclarationPass_) return;
        Ref<Type> actualType = Compiler::get().getTypeContext().getVoid();

        if (node.value)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = currentFunctionReturnType_;
            allowContextualNumericLiteralTyping_ = true;
            node.value->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
            actualType = node.value->refType.Lock();
        }

        Ref<Type> resolvedExpectedReturnType = unwrapAliasType(currentFunctionReturnType_);
        Ref<Type> resolvedActualReturnType = unwrapAliasType(actualType);
        if (node.value &&
            resolvedExpectedReturnType && resolvedActualReturnType &&
            resolvedExpectedReturnType->kind() == TypeKind::Reference &&
            resolvedActualReturnType->kind() == TypeKind::Reference)
        {
            const BorrowOrigin origin = classifyBorrowOrigin(node.value);
            if (origin == BorrowOrigin::Local || origin == BorrowOrigin::Temporary)
            {
                WIO_LOG_ADD_ERROR(
                    node.value->location(),
                    "Cannot return a reference borrowed from {}; the borrow would outlive its owner.",
                    borrowOriginName(origin)
                );
            }
        }

        if (currentFunctionReturnType_)
        {
            if (!currentFunctionReturnType_->isUnknown() &&
                actualType &&
                !actualType->isUnknown() &&
                !isAssignmentLikeCompatible(currentFunctionReturnType_, actualType))
            {
                if (isRejectedImplicitNumericConversion(currentFunctionReturnType_, actualType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Implicit narrowing return conversion from '{}' to '{}' requires explicit 'fit'.",
                        actualType->toString(), currentFunctionReturnType_->toString());
                }
                else
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Return type mismatch! Expected '{}', but got '{}'.",
                        currentFunctionReturnType_->toString(),
                        actualType->toString());
                }
            }
        }
        else
        {
            WIO_LOG_ADD_ERROR(node.location(), "Return statement found outside of a function.");
        }
    }

    void SemanticAnalyzer::visit(UseStatement& node)
    {
        if (!isDeclarationPass_) return;
        if (node.isCppHeader) return;

        auto getOrCreateNamespace = [&](const Ref<Scope>& targetScope, const std::string& name) -> Ref<Symbol>
        {
            if (auto existing = targetScope->resolve(name))
            {
                if (existing->kind == SymbolKind::Namespace)
                    return existing;

                WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and is not a namespace.", name);
                return nullptr;
            }

            auto nsScope = Ref<Scope>::Create(targetScope, ScopeKind::Global);

            auto nsSymbol = createSymbol(name, Compiler::get().getTypeContext().getUnknown(), SymbolKind::Namespace, node.location());
            nsSymbol->innerScope = nsScope;

            targetScope->define(name, nsSymbol);
            return nsSymbol;
        };

        auto resolveImportedNamespace = [&]() -> Ref<Symbol>
        {
            std::vector<std::string> namespaceParts;
            if (node.isStdLib)
                namespaceParts.emplace_back("std");

            auto moduleParts = splitModulePath(node.modulePath);
            namespaceParts.insert(namespaceParts.end(), moduleParts.begin(), moduleParts.end());

            if (namespaceParts.empty())
                return nullptr;

            Ref<Symbol> resolvedNamespace = currentScope_->resolve(namespaceParts.front());
            if (!resolvedNamespace || resolvedNamespace->kind != SymbolKind::Namespace)
                return nullptr;

            for (size_t i = 1; i < namespaceParts.size(); ++i)
            {
                if (!resolvedNamespace->innerScope)
                    return nullptr;

                resolvedNamespace = resolvedNamespace->innerScope->resolve(namespaceParts[i]);
                if (!resolvedNamespace || resolvedNamespace->kind != SymbolKind::Namespace)
                    return nullptr;
            }

            return resolvedNamespace;
        };

        if (auto importedNamespace = resolveImportedNamespace())
        {
            if (!node.aliasName.empty())
            {
                if (auto existingAlias = currentScope_->resolveLocally(node.aliasName))
                {
                    if (existingAlias == importedNamespace)
                        return;

                    WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and cannot be used as an import alias.", node.aliasName);
                    return;
                }

                currentScope_->define(node.aliasName, importedNamespace);
                if (!node.importAllIntoScope)
                    return;
            }

            if (!node.importAllIntoScope)
                return;

            if (!importedNamespace->innerScope)
                return;

            for (const auto& [symbolName, importedSymbol] : importedNamespace->innerScope->getSymbols())
            {
                if (!importedSymbol)
                    continue;

                if (auto existingSymbol = currentScope_->resolveLocally(symbolName))
                {
                    if (existingSymbol == importedSymbol)
                        continue;

                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Symbol '{}' already exists and cannot be directly imported from module '{}'.",
                        symbolName,
                        node.modulePath
                    );
                    continue;
                }

                currentScope_->define(symbolName, importedSymbol);
            }
            return;
        }

        if (node.isStdLib)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Standard library module 'std::{}' could not be resolved after merge.", node.modulePath);
            return;
        }

        if (node.aliasName.empty() && !node.importAllIntoScope)
            return;

        std::vector<Ref<Symbol>> importedSymbols;
        importedSymbols.reserve(node.importedSymbols.size());

        for (const auto& importedName : node.importedSymbols)
        {
            auto importedSymbol = currentScope_->resolveLocally(importedName);
            if (!importedSymbol)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Imported symbol '{}' from module '{}' could not be resolved after merge.", importedName, node.modulePath);
                continue;
            }

            importedSymbols.push_back(importedSymbol);
        }

        if (!node.aliasName.empty())
        {
            Ref<Symbol> aliasNamespace = getOrCreateNamespace(currentScope_, node.aliasName);
            if (!aliasNamespace || !aliasNamespace->innerScope)
                return;

            for (const auto& importedSymbol : importedSymbols)
            {
                if (!importedSymbol)
                    continue;

                if (aliasNamespace->innerScope->resolveLocally(importedSymbol->name))
                    continue;

                aliasNamespace->innerScope->define(importedSymbol->name, importedSymbol);
            }

            if (!node.importAllIntoScope)
                return;
        }

        for (const auto& importedSymbol : importedSymbols)
        {
            if (!importedSymbol)
                continue;

            if (auto existingSymbol = currentScope_->resolveLocally(importedSymbol->name))
            {
                if (existingSymbol == importedSymbol)
                    continue;

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Symbol '{}' already exists and cannot be directly imported from module '{}'.",
                    importedSymbol->name,
                    node.modulePath
                );
                continue;
            }

            currentScope_->define(importedSymbol->name, importedSymbol);
        }
    }
