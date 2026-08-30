// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(IntegerLiteral& node)
    {
        IntegerResult result{};
        bool usedContextualType = false;

        if (allowContextualNumericLiteralTyping_ &&
            currentExpectedExpressionType_ &&
            !common::hasIntegerLiteralTypeSuffix(node.token.value))
        {
            if (auto contextualType = tryGetContextualIntegerLiteralType(currentExpectedExpressionType_); contextualType.has_value())
            {
                result = common::getIntegerAsType(node.token.value, *contextualType);
                usedContextualType = true;
            }
        }

        if (!usedContextualType)
            result = common::getInteger(node.token.value);

        if (!result.isValid)
        {
            if (usedContextualType && currentExpectedExpressionType_)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Integer literal '{}' does not fit into expected type '{}'.",
                    node.token.value,
                    currentExpectedExpressionType_->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Invalid integer literal or value out of bounds: '{}'", node.token.value);
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Type::getTypeFromIntegerResult(result);
    }

    void SemanticAnalyzer::visit(FloatLiteral& node)
    {
        FloatResult result{};
        bool usedContextualType = false;

        if (allowContextualNumericLiteralTyping_ &&
            currentExpectedExpressionType_ &&
            !common::hasFloatLiteralTypeSuffix(node.token.value))
        {
            if (auto contextualType = tryGetContextualFloatLiteralType(currentExpectedExpressionType_); contextualType.has_value())
            {
                result = common::getFloatAsType(node.token.value, *contextualType);
                usedContextualType = true;
            }
        }

        if (!usedContextualType)
            result = common::getFloat(node.token.value);

        if (!result.isValid)
        {
            if (usedContextualType && currentExpectedExpressionType_)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Float literal '{}' does not fit into expected type '{}'.",
                    node.token.value,
                    currentExpectedExpressionType_->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Invalid float literal or value out of bounds: '{}'", node.token.value);
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Type::getTypeFromFloatResult(result);
    }

    void SemanticAnalyzer::visit(StringLiteral& node)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        node.refType = node.token.isUnicodeString ? typeContext.getText() : typeContext.getString();
    }

    void SemanticAnalyzer::visit(InterpolatedStringLiteral& node)
    {
        for(auto& part : node.parts)
        {
            part->accept(*this);
        }
        auto& typeContext = Compiler::get().getTypeContext();
        node.refType = node.isUnicode ? typeContext.getText() : typeContext.getString();
    }

    void SemanticAnalyzer::visit(BoolLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getBool();
    }

    void SemanticAnalyzer::visit(CharLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getChar();
    }

    void SemanticAnalyzer::visit(ByteLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getU8();
    }

    void SemanticAnalyzer::visit(DurationLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getF32();
    }

    void SemanticAnalyzer::visit(ArrayLiteral& node)
    {
        Ref<Type> expectedArrayLiteralType = nullptr;
        Ref<ArrayType> expectedArrayType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Array)
            {
                expectedArrayLiteralType = expectedType;
                expectedArrayType = expectedType.AsFast<ArrayType>();
            }
        }

        if (node.elements.empty())
        {
            if (expectedArrayType)
            {
                if (containsInferredArrayExtent(expectedArrayLiteralType))
                {
                    node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(
                        expectedArrayType->elementType,
                        ArrayType::ArrayKind::Static,
                        expectedArrayType->hasInferredExtent ? 0 : expectedArrayType->size,
                        expectedArrayType->extentType
                    );
                }
                else
                {
                    node.refType = expectedArrayLiteralType;
                }
                return;
            }

            // Fall back to an empty unknown array when no surrounding context can
            // provide the intended element type.
            node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(
                Compiler::get().getTypeContext().getUnknown(),
                ArrayType::ArrayKind::Static,
                0
            );
            return;
        }

        auto analyzeElement = [&](NodePtr<Expression>& element)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedArrayType ? expectedArrayType->elementType : previousExpectedExpressionType;
            element->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        analyzeElement(node.elements[0]);
        Ref<Type> baseType =
            expectedArrayType && !containsInferredArrayExtent(expectedArrayType->elementType)
                ? expectedArrayType->elementType
                : node.elements[0]->refType.Lock();
        if (!baseType)
            baseType = node.elements[0]->refType.Lock();

        for (size_t i = 1; i < node.elements.size(); ++i)
        {
            analyzeElement(node.elements[i]);
            if (auto lockedType = node.elements[i]->refType.Lock(); lockedType)
            {
                if (!baseType ||
                    !haveIdenticalFixedArrayShape(baseType, lockedType) ||
                    (!baseType->isCompatibleWith(lockedType) && !(baseType->isNumeric() && lockedType->isNumeric())))
                {
                    const std::string expectedTypeName = baseType ? baseType->toString() : "<unknown>";
                    WIO_LOG_ADD_ERROR(
                        node.elements[i]->location(),
                        "Array elements must be of the same type. Expected '{}', but found '{}'.",
                        expectedTypeName,
                        lockedType->toString()
                    );
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.elements[i]->location(), "Undefined element type in array.");
            }
        }

        if (expectedArrayType)
        {
            if (expectedArrayType->arrayKind == ArrayType::ArrayKind::Static &&
                !expectedArrayType->hasInferredExtent &&
                expectedArrayType->size != node.elements.size())
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Static array expects '{}' elements, but literal provides '{}'.",
                    expectedArrayType->size,
                    node.elements.size()
                );
            }

            if (containsInferredArrayExtent(expectedArrayLiteralType))
            {
                node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(
                    baseType,
                    ArrayType::ArrayKind::Static,
                    expectedArrayType->hasInferredExtent
                        ? node.elements.size()
                        : expectedArrayType->size,
                    expectedArrayType->extentType
                );
            }
            else
            {
                node.refType = expectedArrayLiteralType;
            }
            return;
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(baseType, ArrayType::ArrayKind::Literal, node.elements.size());
    }

    void SemanticAnalyzer::visit(DictionaryLiteral& node)
    {
        Ref<Type> expectedDictionaryLiteralType = nullptr;
        Ref<DictionaryType> expectedDictionaryType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Dictionary)
            {
                expectedDictionaryLiteralType = expectedType;
                expectedDictionaryType = expectedType.AsFast<DictionaryType>();
            }
        }

        if (node.pairs.empty())
        {
            if (expectedDictionaryType)
            {
                if (expectedDictionaryType->isOrdered != node.isOrdered)
                {
                    if (node.isOrdered)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Ordered dictionary literal '{< >}' cannot initialize unordered dictionary type '{}'.",
                            expectedDictionaryType->toString()
                        );
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Unordered dictionary literal '{}' cannot initialize ordered dictionary type '{}'. Use '{< >}' syntax instead.",
                            "{}",
                            expectedDictionaryType->toString()
                        );
                    }
                }

                node.refType = expectedDictionaryLiteralType;
                return;
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (expectedDictionaryType && expectedDictionaryType->isOrdered != node.isOrdered)
        {
            if (node.isOrdered)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ordered dictionary literal '{< >}' cannot initialize unordered dictionary type '{}'.",
                    expectedDictionaryType->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Unordered dictionary literal '{}' cannot initialize ordered dictionary type '{}'. Use '{< >}' syntax instead.",
                    "{}",
                    expectedDictionaryType->toString()
                );
            }
        }

        auto analyzeKey = [&](NodePtr<Expression>& expression)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedDictionaryType ? expectedDictionaryType->keyType : previousExpectedExpressionType;
            expression->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        auto analyzeValue = [&](NodePtr<Expression>& expression)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedDictionaryType ? expectedDictionaryType->valueType : previousExpectedExpressionType;
            expression->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        analyzeKey(node.pairs[0].first);
        analyzeValue(node.pairs[0].second);

        auto keyType = expectedDictionaryType ? expectedDictionaryType->keyType : node.pairs[0].first->refType.Lock();
        auto valType = expectedDictionaryType ? expectedDictionaryType->valueType : node.pairs[0].second->refType.Lock();
        if (!keyType)
            keyType = node.pairs[0].first->refType.Lock();
        if (!valType)
            valType = node.pairs[0].second->refType.Lock();

        for (size_t i = 1; i < node.pairs.size(); ++i)
        {
            analyzeKey(node.pairs[i].first);
            analyzeValue(node.pairs[i].second);

            auto k = node.pairs[i].first->refType.Lock();
            auto v = node.pairs[i].second->refType.Lock();

            if (!keyType || !valType ||
                !k || !v ||
                (!keyType->isCompatibleWith(k) && !(keyType->isNumeric() && k->isNumeric())) ||
                (!valType->isCompatibleWith(v) && !(valType->isNumeric() && v->isNumeric())))
            {
                WIO_LOG_ADD_ERROR(node.pairs[i].first->location(), "All keys and values in a dictionary literal must have the same type.");
            }
        }

        if (expectedDictionaryType)
        {
            node.refType = expectedDictionaryLiteralType;
            return;
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateDictionaryType(keyType, valType, node.isOrdered);
    }

    void SemanticAnalyzer::visit(Identifier& node)
    {
        Ref<Symbol> sym = currentScope_->resolve(node.token.value);

        if (!sym)
        {
            if (node.token.isType())
            {
                if (Ref<Type> builtinType = resolvePrimitiveType(node.token.value))
                {
                    node.refType = builtinType;
                    return;
                }
            }

            if (allowTypePackIdentifierReference_)
            {
                for (auto& genericTypeParameterScope : std::ranges::reverse_view(genericTypeParameterScopes_))
                {
                    if (auto genericTypeIt = genericTypeParameterScope.find(node.token.value); genericTypeIt != genericTypeParameterScope.end())
                    {
                        Ref<Type> genericType = genericTypeIt->second;
                        if (genericType && genericType->kind() == TypeKind::GenericParameterPack)
                        {
                            node.refType = genericType;
                            return;
                        }
                    }
                }
            }

            for (auto& genericTypeParameterScope : std::ranges::reverse_view(genericTypeParameterScopes_))
            {
                auto genericIt = genericTypeParameterScope.find(node.token.value);
                if (genericIt == genericTypeParameterScope.end() || !genericIt->second ||
                    (genericIt->second->kind() != TypeKind::ConstGenericParameter &&
                     genericIt->second->kind() != TypeKind::ConstValue))
                    continue;

                node.refType = genericIt->second->kind() == TypeKind::ConstGenericParameter
                    ? genericIt->second.AsFast<ConstGenericParameterType>()->valueType
                    : genericIt->second.AsFast<ConstValueType>()->valueType;
                return;
            }

            WIO_LOG_ADD_ERROR(node.location(), "Undefined symbol: '{}'", node.token.value);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = sym;
        if (!sym->flags.get_isGlobal() &&
            (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter))
        {
            for (auto& context : lambdaCaptureContexts_)
            {
                if (context.localSymbols.contains(sym.Get()) ||
                    !context.capturedSymbols.insert(sym.Get()).second)
                {
                    continue;
                }
                context.captures.push_back(sym);
            }
        }
        if (nonNullNarrowedSymbols_.contains(sym.Get()))
        {
            Ref<Type> narrowedType = unwrapAliasType(sym->type);
            node.refType = narrowedType && narrowedType->kind() == TypeKind::Nullable
                ? narrowedType.AsFast<NullableType>()->valueType
                : sym->type;
        }
        else
        {
            node.refType = sym->type;
        }
        if (sym->flags.get_isGlobal())
            node.borrowOrigin = BorrowOrigin::Static;
        else if (sym->kind == SymbolKind::Parameter && sym->type && sym->type->kind() == TypeKind::Reference)
            node.borrowOrigin = BorrowOrigin::Caller;
        else if (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)
            node.borrowOrigin = BorrowOrigin::Local;

        if (sym->flags.get_isParameterPack() && !allowParameterPackIdentifierReference_)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Parameter pack '{}' must be expanded as '{}...' inside a function call.",
                node.token.value,
                node.token.value
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
        }
    }

    void SemanticAnalyzer::visit(PackExpansionExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        node.operand->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;

        auto referencedSymbol = node.operand ? node.operand->referencedSymbol.Lock() : nullptr;
        if (!referencedSymbol || !referencedSymbol->flags.get_isParameterPack())
        {
            WIO_LOG_ADD_ERROR(node.location(), "Pack expansion expressions currently support only function parameter packs.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = referencedSymbol;
        node.refType = referencedSymbol->type;
    }

    void SemanticAnalyzer::visit(NullExpression& node)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        Ref<Type> transformedType = currentExpectedExpressionType_
            ? getAutoReadableType(currentExpectedExpressionType_)
            : nullptr;

        if (!transformedType || transformedType->isUnknown())
            transformedType = typeContext.getVoid();

        node.refType = typeContext.getOrCreateNullType(transformedType);
    }
