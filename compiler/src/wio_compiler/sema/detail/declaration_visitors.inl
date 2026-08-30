// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(VariableDeclaration& node)
    {
        applyActiveScopedAttributes(
            node.attributes,
            currentScope_->getKind() == ScopeKind::Struct ? "field" : "variable");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(
                node.attributes,
                currentScope_->getKind() == ScopeKind::Struct ? "field" : "variable");
        if (hasAttribute(node.attributes, Attribute::Specialize) &&
            (isDeclarationPass_ || currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");
        }

        if (isDeclarationPass_)
        {
            if (currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block)
                return;

              Ref<Type> declaredType = nullptr;
              if (node.type)
              {
                  const bool previousAllowInferredStaticArrayExtent = allowInferredStaticArrayExtent_;
                  allowInferredStaticArrayExtent_ = true;
                  node.type->accept(*this);
                  allowInferredStaticArrayExtent_ = previousAllowInferredStaticArrayExtent;
                  declaredType = node.type->refType.Lock();
              }

              if (node.isPackField)
              {
                  if (!declaredType || declaredType->kind() != TypeKind::GenericParameterPack)
                  {
                      WIO_LOG_ADD_ERROR(node.location(), "Pack fields must declare a trailing generic pack type such as 'pack values: Args...;'.");
                      declaredType = Compiler::get().getTypeContext().getUnknown();
                  }
                  else
                  {
                      declaredType = Compiler::get().getTypeContext().getOrCreatePackStorageType(
                          declaredType.AsFast<GenericParameterPackType>()->name
                      );
                  }
              }

              SymbolFlags flags = SymbolFlags::createAllFalse();
              if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (node.mutability == Mutability::Const) flags.set_isConst(true);
            if (currentScope_->getKind() == ScopeKind::Global) flags.set_isGlobal(true);

              Ref<Symbol> sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
              currentScope_->define(node.name->token.value, sym);
              node.name->referencedSymbol = sym;
              variableDeclarationsBySymbol_[sym.Get()] = &node;

              return;
          }

        Ref<Symbol> sym = node.name->referencedSymbol.Lock();
        if (sym &&
            (currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            Ref<Symbol> localSymbol = currentScope_->resolveLocally(node.name->token.value);
            if (localSymbol.Get() != sym.Get())
                sym = nullptr;
        }

        if (!sym)
        {
              Ref<Type> declaredType = nullptr;
              if (node.type)
              {
                  const bool previousAllowInferredStaticArrayExtent = allowInferredStaticArrayExtent_;
                  allowInferredStaticArrayExtent_ = true;
                  node.type->accept(*this);
                  allowInferredStaticArrayExtent_ = previousAllowInferredStaticArrayExtent;
                  declaredType = node.type->refType.Lock();
              }

              if (node.isPackField)
              {
                  if (!declaredType || declaredType->kind() != TypeKind::GenericParameterPack)
                  {
                      WIO_LOG_ADD_ERROR(node.location(), "Pack fields must declare a trailing generic pack type such as 'pack values: Args...;'.");
                      declaredType = Compiler::get().getTypeContext().getUnknown();
                  }
                  else
                  {
                      declaredType = Compiler::get().getTypeContext().getOrCreatePackStorageType(
                          declaredType.AsFast<GenericParameterPackType>()->name
                      );
                  }
              }

              SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (node.mutability == Mutability::Const) flags.set_isConst(true);

                sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
                currentScope_->define(node.name->token.value, sym);
                node.name->referencedSymbol = sym;
                variableDeclarationsBySymbol_[sym.Get()] = &node;
            }
            else if (sym->type && containsGenericParameterType(sym->type))
            {
              GenericBindingSet bindingSet;
              for (const auto& genericScope : genericTypeParameterScopes_)
              {
                  for (const auto& [name, boundType] : genericScope)
                      bindingSet.directBindings.insert_or_assign(name, boundType);
              }

              Ref<Type> instantiatedType = instantiateGenericType(sym->type, bindingSet);
              if (instantiatedType && !instantiatedType->isUnknown() && !containsGenericParameterType(instantiatedType))
              {
                  SymbolFlags flags = SymbolFlags::createAllFalse();
                  if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
                  if (node.mutability == Mutability::Const) flags.set_isConst(true);

                    sym = createSymbol(node.name->token.value, instantiatedType, SymbolKind::Variable, node.location(), flags);
                    currentScope_->define(node.name->token.value, sym);
                    node.name->referencedSymbol = sym;
                    node.name->refType = instantiatedType;
                    variableDeclarationsBySymbol_[sym.Get()] = &node;
                }
                else if (!currentScope_->resolveLocally(node.name->token.value))
                {
                  currentScope_->define(node.name->token.value, sym);
              }
          }
          else if (!currentScope_->resolveLocally(node.name->token.value))
            {
                currentScope_->define(node.name->token.value, sym);
            }

            if (sym)
                variableDeclarationsBySymbol_[sym.Get()] = &node;

          if (node.initializer)
          {
            auto shouldAutoReadInferredInitializer = [&](const NodePtr<Expression>& initializer,
                                                         const Ref<Type>& initializerType) -> bool
            {
                if (!initializer || !shouldAutoReadReferenceType(initializerType))
                    return false;

                if (initializer->is<RefExpression>())
                    return false;

                if (const auto* unary = initializer->as<UnaryExpression>())
                {
                    if (unary->op.type == TokenType::kwDeref)
                        return false;
                }

                return true;
            };

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = sym->type;
            allowContextualNumericLiteralTyping_ = true;
            node.initializer->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
            Ref<Type> initType = node.initializer->refType.Lock();

            if (containsInferredArrayExtent(sym->type))
            {
                Ref<Type> resolvedInitializer = unwrapAliasType(initType);
                if (resolvedInitializer &&
                    resolvedInitializer->kind() == TypeKind::Array &&
                    !containsInferredArrayExtent(resolvedInitializer) &&
                    resolvedInitializer.AsFast<ArrayType>()->arrayKind != ArrayType::ArrayKind::Dynamic &&
                    sym->type->isCompatibleWith(initType))
                {
                    sym->type = initType;
                    node.name->refType = initType;
                    if (node.type)
                        node.type->refType = initType;
                }
                else if (!resolvedInitializer ||
                         resolvedInitializer->kind() != TypeKind::Array ||
                         containsInferredArrayExtent(resolvedInitializer) ||
                         resolvedInitializer.AsFast<ArrayType>()->arrayKind == ArrayType::ArrayKind::Dynamic)
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Inferred static array extent requires an array literal or concrete fixed-size array initializer."
                    );
                }
            }

            Ref<Type> resolvedDeclaredType = unwrapAliasType(sym->type);
            Ref<Type> resolvedInitializerType = unwrapAliasType(initType);
            if (resolvedDeclaredType && resolvedInitializerType &&
                resolvedDeclaredType->kind() == TypeKind::Reference &&
                resolvedInitializerType->kind() == TypeKind::Reference &&
                classifyBorrowOrigin(node.initializer) == BorrowOrigin::Temporary)
            {
                WIO_LOG_ADD_ERROR(
                    node.initializer->location(),
                    "Cannot store a reference borrowed from a temporary value; the borrow would outlive its owner."
                );
            }

            if (!sym->type || sym->type->isUnknown())
            {
                if (resolvedInitializerType && resolvedInitializerType->kind() == TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Cannot infer a variable type from null. Add an explicit nullable type."
                    );
                    sym->type = Compiler::get().getTypeContext().getUnknown();
                    node.name->refType = sym->type;
                    return;
                }

                Ref<Type> inferredType = initType;
                if (shouldAutoReadInferredInitializer(node.initializer, initType))
                    inferredType = getAutoReadableType(initType);

                sym->type = inferredType;
                node.name->refType = inferredType;
            }
            else if (initType && !initType->isUnknown() && !isAssignmentLikeCompatible(sym->type, initType))
            {
                if (isRejectedImplicitNumericConversion(sym->type, initType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Implicit narrowing conversion from '{}' to '{}' requires explicit 'fit'.",
                        initType->toString(), sym->type->toString());
                }
                else if (resolvedInitializerType && resolvedInitializerType->kind() == TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Type '{}' cannot be initialized with null.",
                        sym->type->toString()
                    );
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Type mismatch for '{}'.", node.name->token.value);
                }
            }

            if (node.mutability == Mutability::Const)
            {
                if (!isConstScalarType(sym->type))
                {
                    const std::string actualType = sym->type ? sym->type->toString() : "<unknown>";
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Const declarations support scalar, string, text, enum, and flagset values. Got '{}'.",
                        actualType
                    );
                }
                else
                {
                    ConstEvaluationLimiter limiter(variableDeclarationsBySymbol_);
                    limiter.markActive(sym.Get());
                    const auto limitStatus = limiter.validate(node.initializer);
                    if (limitStatus == ConstEvaluationLimitStatus::Cycle)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.initializer->location(),
                            "Const initializer contains a cyclic const dependency."
                        );
                    }
                    else if (limitStatus == ConstEvaluationLimitStatus::DepthLimit)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.initializer->location(),
                            "Const initializer exceeds the maximum evaluation depth of {}.",
                            ConstEvaluationLimiter::MaxDepth
                        );
                    }
                    else if (limitStatus == ConstEvaluationLimitStatus::NodeLimit)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.initializer->location(),
                            "Const initializer exceeds the maximum evaluation node count of {}.",
                            ConstEvaluationLimiter::MaxNodes
                        );
                    }
                    else if (limitStatus == ConstEvaluationLimitStatus::TextSizeLimit)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.initializer->location(),
                            "Const string/text evaluation exceeds the maximum folded size of {} bytes.",
                            ConstEvaluationLimiter::MaxTextBytes
                        );
                    }
                    else if (!isConstEvaluableExpression(node.initializer))
                    {
                        WIO_LOG_ADD_ERROR(
                            node.initializer->location(),
                            "Const initializer must be a compile-time expression and may reference only other const declarations."
                        );
                    }
                }
            }
        }
        else if (sym &&
                 currentScope_->getKind() != ScopeKind::Struct &&
                 requiresExplicitNonNullInitialization(sym->type))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Non-null type '{}' requires an initializer. Use '{}?' if an empty state is required.",
                sym->type->toString(),
                sym->type->toString()
            );
        }

        if (!node.initializer && sym && containsInferredArrayExtent(sym->type))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Inferred static array extent '[T; _]' requires an initializer."
            );
        }
    }

    void SemanticAnalyzer::visit(TypeAliasDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "type");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "type");
        if (hasAttribute(node.attributes, Attribute::Specialize) &&
            (isDeclarationPass_ || currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");
        }

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this type alias.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (!isDeclarationPass_)
        {
            if (!node.name->referencedSymbol.Lock())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Local type aliases are not supported yet. Declare type aliases at global or realm scope.");
            }
            return;
        }

        auto genericScope = buildGenericTypeParameterScope();
        genericTypeParameterScopes_.push_back(genericScope);

        std::vector<std::string> genericParameterNames;
        genericParameterNames.reserve(node.genericParameters.size());
        for (const auto& genericParameter : node.genericParameters)
        {
            if (genericParameter)
                genericParameterNames.push_back(genericParameter->token.value);
        }

        auto genericParameterDefaults = resolveGenericParameterDefaults(
            *this, node.genericParameters, node.hasGenericParameterPack, "type alias");

        validateApplyAttributes(*this, node.attributes, genericParameterNames, "type alias", node.location());

        node.aliasedType->accept(*this);
        Ref<Type> aliasedType = node.aliasedType->refType.Lock();
        Ref<Type> aliasType = Compiler::get().getTypeContext().getOrCreateAliasType(node.name->token.value, aliasedType);
        Ref<Symbol> aliasSym = createSymbol(node.name->token.value, aliasType, SymbolKind::TypeAlias, node.location());
        aliasSym->aliasTargetType = aliasedType;
        aliasSym->genericParameterNames = genericParameterNames;
        aliasSym->genericParameterTypes = collectGenericParameterSemanticTypes(node.genericParameters);
        aliasSym->genericParameterDefaults = std::move(genericParameterDefaults);
        aliasSym->hasGenericParameterPack = node.hasGenericParameterPack;

        currentScope_->define(node.name->token.value, aliasSym);
        attributeListsBySymbol_[aliasSym.Get()] = &node.attributes;
        node.name->referencedSymbol = aliasSym;
        node.name->refType = aliasType;

        genericTypeParameterScopes_.pop_back();
    }

    void SemanticAnalyzer::visit(FunctionDeclaration& node)
    {
        const std::string attributeTarget = !node.attributeTargetOverride.empty()
            ? node.attributeTargetOverride
            : (currentScope_->getKind() == ScopeKind::Struct ? "method" : "fn");
        applyActiveScopedAttributes(
            node.attributes,
            attributeTarget);
        if (!isDeclarationPass_ && !isStructResolutionPass_)
        {
            validateAttributeApplications(
                node.attributes,
                attributeTarget);
            const bool hasBehavioralProcessor = std::ranges::any_of(
                node.attributes,
                [](const NodePtr<AttributeStatement>& attribute)
                {
                    return attribute && std::ranges::any_of(
                        attribute->processorBindings,
                        [](const AttributeStatement::ProcessorBinding& processor)
                        {
                            return processor.phase == "pre" || processor.phase == "post" ||
                                   processor.phase == "finally" || processor.phase == "around";
                        });
                });
            if (hasBehavioralProcessor && (!node.body || hasAttribute(node.attributes, Attribute::Native)))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Behavioral attribute processors require a Wio function body and cannot wrap a native declaration.");
            }
            if (hasBehavioralProcessor && node.name && node.name->token.value == "Entry" &&
                currentScope_->getKind() != ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Behavioral attribute processors cannot target Entry in this checkpoint; wrap an ordinary startup function instead.");
            }
            if (hasBehavioralProcessor && node.whenCondition)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Behavioral attribute processors cannot be combined with a function 'when' guard in this checkpoint.");
            }
            const bool hasAroundProcessor = std::ranges::any_of(
                node.attributes,
                [](const NodePtr<AttributeStatement>& attribute)
                {
                    return attribute && std::ranges::any_of(
                        attribute->processorBindings,
                        [](const AttributeStatement::ProcessorBinding& processor)
                        {
                            return processor.phase == "around";
                        });
                });
            Ref<Symbol> attributedFunctionSymbol = node.name ? node.name->referencedSymbol.Lock() : nullptr;
            Ref<FunctionType> attributedFunctionType = attributedFunctionSymbol
                ? attributedFunctionSymbol->type.AsFast<FunctionType>()
                : nullptr;
            Ref<Type> attributedResultType = attributedFunctionType
                ? unwrapAliasType(attributedFunctionType->returnType)
                : nullptr;
            if (node.isAsync && attributedResultType && attributedResultType->kind() == TypeKind::AsyncTask)
                attributedResultType = unwrapAliasType(attributedResultType.AsFast<AsyncTaskType>()->valueType);
            for (const auto& attribute : node.attributes)
            {
                if (!attribute)
                    continue;
                for (const auto& processor : attribute->processorBindings)
                {
                    if (processor.phase == "post" && processor.hookMode == "result")
                    {
                        Ref<Type> hookResultType = unwrapAliasType(processor.hookValueType.Lock());
                        if (!attributedResultType || attributedResultType->isVoid() || !hookResultType ||
                            !attributedResultType->isCompatibleWith(hookResultType))
                        {
                            WIO_LOG_ADD_ERROR(
                                attribute->location(),
                                "Post processor '{}' expects result type '{}', but target '{}' returns '{}'.",
                                processor.canonicalTypeName,
                                hookResultType ? hookResultType->toString() : "<unknown>",
                                node.name ? node.name->token.value : "<anonymous>",
                                attributedResultType ? attributedResultType->toString() : "<unknown>");
                        }
                    }
                    if (processor.phase != "pre")
                        continue;
                    if (processor.hookMode.starts_with("receiver_any") &&
                        (attributeTarget != "method" || !currentStructType_ ||
                         currentStructType_->kind() != TypeKind::Struct ||
                         !currentStructType_.AsFast<StructType>()->isObject))
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Receiver-aware pre processor '{}' requires an object method target.",
                            processor.canonicalTypeName);
                    }
                    if (processor.hookMode.starts_with("receiver_typed"))
                    {
                        Ref<Type> hookReceiver = unwrapAliasType(processor.hookValueType.Lock());
                        auto receiverReference = hookReceiver && hookReceiver->kind() == TypeKind::Reference
                            ? hookReceiver.AsFast<ReferenceType>()
                            : nullptr;
                        Ref<Type> requiredTarget = receiverReference
                            ? unwrapAliasType(receiverReference->referredType)
                            : nullptr;
                        if (attributeTarget != "method" || !currentStructType_ ||
                            currentStructType_->kind() != TypeKind::Struct ||
                            !currentStructType_.AsFast<StructType>()->isObject || !requiredTarget ||
                            !isTypeDerivedFrom(currentStructType_, requiredTarget))
                        {
                            WIO_LOG_ADD_ERROR(
                                attribute->location(),
                                "Typed receiver pre processor '{}' requires an object method target compatible with '{}'.",
                                processor.canonicalTypeName,
                                requiredTarget ? requiredTarget->toString() : "<unknown>");
                        }
                    }
                    if (processor.hookMode.ends_with("_guard") &&
                        (!attributedResultType || !attributedResultType->isVoid()))
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Boolean pre guard '{}' can skip only a unit-returning function or method.",
                            processor.canonicalTypeName);
                    }
                }
            }
            for (const auto& attribute : node.attributes)
            {
                if (!attribute)
                    continue;
                for (const auto& processor : attribute->processorBindings)
                {
                    if (processor.phase != "around")
                        continue;
                    Ref<Type> aroundResultType = unwrapAliasType(processor.hookValueType.Lock());
                    if (!attributedResultType || !aroundResultType ||
                        !attributedResultType->isCompatibleWith(aroundResultType))
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Around processor '{}' expects result type '{}', but target '{}' returns '{}'.",
                            processor.canonicalTypeName,
                            aroundResultType ? aroundResultType->toString() : "<unknown>",
                            node.name ? node.name->token.value : "<anonymous>",
                            attributedResultType ? attributedResultType->toString() : "<unknown>");
                    }
                }
            }
            if (hasAroundProcessor && node.isAsync)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "AroundProcessor on async functions is not available; use pre/post/finally processors so cancellation and suspension remain explicit.");
            }
        }
        for (auto& parameter : node.parameters)
        {
            applyActiveScopedAttributes(parameter.attributes, "parameter");
            if (!isDeclarationPass_ && !isStructResolutionPass_)
                validateAttributeApplications(parameter.attributes, "parameter");
        }
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this function.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();

                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Specialize is supported only on generic object and component declarations."
                );
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "function");

            Ref<Type> returnType = Compiler::get().getTypeContext().getVoid();
            if (node.returnType)
            {
                node.returnType->accept(*this);
                returnType = node.returnType->refType.Lock();
            }

            std::vector<Ref<Type>> paramTypes;
            bool hasParameterPack = false;
            for (auto& param : node.parameters)
            {
                Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
                if (param.type)
                {
                    param.type->accept(*this);
                    pType = param.type->refType.Lock();
                }
                hasParameterPack = hasParameterPack || param.isParameterPack;
                paramTypes.push_back(pType);
            }

            Ref<Type> callableReturnType = node.isAsync
                ? Compiler::get().getTypeContext().getOrCreateAsyncTaskType(returnType)
                : returnType;
            auto funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(callableReturnType, paramTypes, hasParameterPack);

            Ref<Symbol> funcSym = createSymbol(node.name->token.value, funcType, SymbolKind::Function, node.location());
            if (node.isExtensionMethod)
            {
                funcSym->flags.set_isExtension(true);
                funcSym->extensionTargetType = currentExtensionTargetType_;
                funcSym->extensionMemberName = node.extensionMemberName;
                node.extensionTargetType = currentExtensionTargetType_;
            }
            funcSym->innerScope = currentScope_;
            funcSym->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    funcSym->genericParameterNames.push_back(genericParameter->token.value);
            }
            funcSym->genericParameterTypes = collectGenericParameterSemanticTypes(node.genericParameters);
            funcSym->hasGenericParameterPack = node.hasGenericParameterPack;
            funcSym->genericParameterDefaults = std::move(genericParameterDefaults);
            currentScope_->define(node.name->token.value, funcSym);
            functionDeclarationsBySymbol_[funcSym.Get()] = &node;
            attributeListsBySymbol_[funcSym.Get()] = &node.attributes;
            if (!funcSym->genericParameterNames.empty())
            {
                validateApplyAttributes(*this, node.attributes, funcSym->genericParameterNames, "function", node.location());
                funcSym->resolvedGenericInstantiations = resolveInstantiateAttributes(
                    *this,
                    node.attributes,
                    funcSym->genericParameterNames,
                    funcSym->genericParameterDefaults,
                    funcSym->hasGenericParameterPack
                );
            }

            node.name->refType = funcType;
            node.name->referencedSymbol = funcSym;
            genericTypeParameterScopes_.pop_back();
            return;
        }

        auto funcSym = node.name->referencedSymbol.Lock();
        // A duplicate or otherwise rejected declaration is not retained by
        // its scope, so the AST's weak symbol reference may have expired
        // between declaration and resolution passes. The defining pass has
        // already emitted the diagnostic; do not dereference a missing or
        // non-function recovery symbol while walking the malformed body.
        if (!funcSym || !funcSym->type || funcSym->type->kind() != TypeKind::Function)
            return;
        auto funcType = funcSym->type.AsFast<FunctionType>();

        bool isNative = hasAttribute(node.attributes, Attribute::Native);
        bool isExported = hasAttribute(node.attributes, Attribute::Export);
        bool isCommand = hasAttribute(node.attributes, Attribute::Command);
        bool isEvent = hasAttribute(node.attributes, Attribute::Event);
        std::vector<Attribute> moduleLifecycleAttributes = getModuleLifecycleAttributes(node.attributes);
        bool hasModuleLifecycle = !moduleLifecycleAttributes.empty();
        bool isGenericFunction = !node.genericParameters.empty();
        bool hasApply = hasAttribute(node.attributes, Attribute::Apply);
        bool isStructMethod = currentScope_ && currentScope_->getKind() == ScopeKind::Struct;
        auto currentStruct = currentStructType_ ? currentStructType_.AsFast<StructType>() : nullptr;
        const bool isLifecycleMethod =
            node.name->token.value == "OnConstruct" || node.name->token.value == "OnDestruct";
        const bool isOperatorMethod = common::isOperatorOverloadName(node.name->token.value);
        const bool isUnaryOperatorMethod = common::isUnaryOperatorOverloadName(node.name->token.value);
        const bool isConversionOperatorMethod = common::isConversionOperatorOverloadName(node.name->token.value);
        const bool isIndexOperatorMethod = common::isIndexOperatorOverloadName(node.name->token.value);
        const bool isCallOperatorMethod = common::isCallOperatorOverloadName(node.name->token.value);
        const bool isComponentMethodContext = currentStruct && !currentStruct->isObject && !currentStruct->isInterface;
        bool hasInstantiate = hasAttribute(node.attributes, Attribute::Instantiate);
        const bool hasFunctionParameterPack = std::ranges::any_of(node.parameters, [](const Parameter& parameter)
        {
            return parameter.isParameterPack;
        });
        const bool isPackFunction = node.hasGenericParameterPack || hasFunctionParameterPack;
        Ref<Type> declaredResultType = funcType->returnType;
        if (node.isAsync)
        {
            Ref<Type> resolvedTaskType = unwrapAliasType(funcType->returnType);
            declaredResultType = resolvedTaskType && resolvedTaskType->kind() == TypeKind::AsyncTask
                ? resolvedTaskType.AsFast<AsyncTaskType>()->valueType
                : Compiler::get().getTypeContext().getUnknown();
        }

        if (node.isAsync && isLifecycleMethod)
            WIO_LOG_ADD_ERROR(node.location(), "{} cannot be async.", node.name->token.value);
        if (node.isAsync && isOperatorMethod)
            WIO_LOG_ADD_ERROR(node.location(), "Operator overloads cannot be async.");
        if (node.isAsync && (isCommand || isEvent || hasModuleLifecycle))
            WIO_LOG_ADD_ERROR(node.location(), "Async functions cannot use command, event, or module-lifecycle ABI attributes.");
        if (node.isAsync && isComponentMethodContext)
            WIO_LOG_ADD_ERROR(node.location(), "Stack-resident component methods cannot be async; use an async method on an owning object or pass a component value to an async function.");
        if (node.isAsync && node.isExtensionMethod)
            WIO_LOG_ADD_ERROR(node.location(), "Async extension methods are not yet lifetime-safe because extension receivers are borrowed.");
        if (node.isAsync)
        {
            Ref<Type> resolvedResultType = unwrapAliasType(declaredResultType);
            if (resolvedResultType && resolvedResultType->kind() == TypeKind::Reference)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Async functions cannot return ref/view values across suspension points.");
            }

            for (size_t parameterIndex = 0; parameterIndex < funcType->paramTypes.size(); ++parameterIndex)
            {
                Ref<Type> resolvedParameterType = unwrapAliasType(funcType->paramTypes[parameterIndex]);
                if (!resolvedParameterType || resolvedParameterType->kind() != TypeKind::Reference)
                    continue;

                const std::string parameterName = parameterIndex < node.parameters.size() && node.parameters[parameterIndex].name
                    ? node.parameters[parameterIndex].name->token.value
                    : common::formatString("param{}", parameterIndex);
                WIO_LOG_ADD_ERROR(
                    node.parameters[parameterIndex].name ? node.parameters[parameterIndex].name->location() : node.location(),
                    "Async parameter '{}' cannot be ref/view because the borrow may outlive its caller.",
                    parameterName
                );
            }
        }

        if (isLifecycleMethod && declaredResultType && !declaredResultType->isVoid())
        {
            WIO_LOG_ADD_ERROR(node.location(), "{} must return void.", node.name->token.value);
        }

        if (node.name->token.value == "OnDestruct")
        {
            if (!node.parameters.empty())
                WIO_LOG_ADD_ERROR(node.location(), "OnDestruct must not declare parameters.");
            if (node.whenCondition || node.whenFallback)
                WIO_LOG_ADD_ERROR(node.location(), "OnDestruct does not support when/else clauses.");
        }

        if (isNative)
        {
            Ref<Type> nativeReturnType = unwrapAliasType(funcType->returnType);
            if (nativeReturnType && nativeReturnType->kind() == TypeKind::Reference)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Native functions cannot return ref/view values because the native borrow lifetime cannot be proven. Return an owning value or handle instead."
                );
            }

            for (size_t index = 0; index < funcSym->genericParameterTypes.size(); ++index)
            {
                if (!isTextualConstGenericParameterType(funcSym->genericParameterTypes[index]))
                    continue;
                WIO_LOG_ADD_ERROR(
                    index < node.genericParameters.size() && node.genericParameters[index]
                        ? node.genericParameters[index]->location()
                        : node.location(),
                    "@Native functions support only integer const generic parameters; string/text const values use a Wio-owned structural backend representation."
                );
            }
        }
        std::string activeFunctionPackName = node.hasGenericParameterPack && !node.genericParameters.empty()
            ? node.genericParameters.back()->token.value
            : "";
        if (isCommand && isEvent)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Command and @Event cannot be combined on the same function.");
        }

        if (isNative && isExported)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Export cannot be combined with @Native.");
        }

        if (hasInstantiate && !isGenericFunction)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Instantiate can only be used on generic functions.");
        }

        if (hasApply && !isGenericFunction)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Apply can only be used on generic functions.");
        }

        if (isOperatorMethod)
        {
            if (node.whenCondition || node.whenFallback)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads do not support when/else clauses.");
            }

            if (hasFunctionParameterPack || node.hasGenericParameterPack)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads cannot use parameter packs.");
            }

            const bool isAssignmentOperator = common::isAssignmentOperatorOverloadName(node.name->token.value);
            if (isCallOperatorMethod && !isStructMethod)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Call operator overloads must be declared as member functions."
                );
            }
            else if (!isCallOperatorMethod)
            {
                const size_t expectedParameterCount = isStructMethod
                    ? (isUnaryOperatorMethod || isConversionOperatorMethod ? 0u : 1u)
                    : (isUnaryOperatorMethod || isConversionOperatorMethod ? 1u : 2u);
                if (node.parameters.size() != expectedParameterCount)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        isStructMethod
                            ? (isUnaryOperatorMethod
                                ? "Member unary operator overloads must declare zero parameters."
                                : (isConversionOperatorMethod
                                    ? "Member conversion operator overloads must declare zero parameters."
                                    : (isIndexOperatorMethod
                                        ? "Member subscript operator overloads must declare exactly one parameter."
                                        : (isAssignmentOperator
                                            ? "Member assignment operator overloads must declare exactly one parameter."
                                            : "Member binary operator overloads must declare exactly one parameter."))))
                            : (isUnaryOperatorMethod
                                ? "Free unary operator overloads must declare exactly one parameter."
                                : (isConversionOperatorMethod
                                    ? "Free conversion operator overloads must declare exactly one parameter."
                                    : (isIndexOperatorMethod
                                        ? "Free subscript operator overloads must declare exactly two parameters."
                                        : (isAssignmentOperator
                                            ? "Free assignment operator overloads must declare exactly two parameters."
                                            : "Free binary operator overloads must declare exactly two parameters."))))
                    );
                }
            }

            for (const auto& parameter : node.parameters)
            {
                if (parameter.defaultValue)
                {
                    WIO_LOG_ADD_ERROR(parameter.name->location(), "Operator overload parameters cannot declare default values.");
                }
            }

            if (!funcType->returnType || isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads must return a value.");
            }

            const bool requiresBoolReturn =
                node.name->token.value == "__op_equal" ||
                node.name->token.value == "__op_not_equal" ||
                node.name->token.value == "__op_less" ||
                node.name->token.value == "__op_less_equal" ||
                node.name->token.value == "__op_greater" ||
                node.name->token.value == "__op_greater_equal" ||
                node.name->token.value == "__op_logical_not";
            if (requiresBoolReturn &&
                !isExactType(funcType->returnType, Compiler::get().getTypeContext().getBool()))
            {
                auto operatorDisplay = common::getOperatorDisplayText(node.name->token.value);
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Operator '{}' must return bool.",
                    operatorDisplay.has_value() ? std::string(*operatorDisplay) : node.name->token.value
                );
            }
        }

        if (isGenericFunction)
        {
            if (isPackFunction && isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Generic parameter packs are currently supported only on top-level functions.");
            }

            if (isStructMethod)
            {
                if (!isOperatorMethod && (!currentStruct || !currentStruct->isObject))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic methods are currently supported only on object methods.");
                }
                else if (!isOperatorMethod && (node.name->token.value == "OnConstruct" || node.name->token.value == "OnDestruct"))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic constructors and destructors are not supported yet.");
                }
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as a generic function.");
            }

            if (hasInstantiate && isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Instantiate is currently supported only on top-level generic functions.");
            }

            if (hasInstantiate && !isNative && !isExported)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Instantiate is currently supported only together with @Native or @Export.");
            }

            const bool allowsOpenNativeTemplateByConstraint =
                isNative && !isExported &&
                (isPackFunction || hasApply || isOpenNativeTemplateIntrinsic(node.attributes));

            if ((isNative || isExported) && !hasInstantiate && !allowsOpenNativeTemplateByConstraint)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic {} functions require at least one @Instantiate(...) declaration.",
                    isNative ? "@Native" : "@Export"
                );
            }

            if (isCommand || isEvent || hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Generic functions cannot currently use C ABI or module export attributes.");
            }
        }
        else if (hasInstantiate)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Instantiate can only be used on generic functions.");
        }

        if (isPackFunction)
        {
            if (!activeFunctionPackName.empty())
            {
                // already declared on the function itself
            }
            else
            {
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& parameter = node.parameters[i];
                    Ref<Type> parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : Compiler::get().getTypeContext().getUnknown();
                    std::string containedPackName;
                    const bool containsPack = containsGenericParameterPackType(parameterType, &containedPackName);
                    if (parameter.isParameterPack && containsPack)
                    {
                        if (activeFunctionPackName.empty())
                            activeFunctionPackName = containedPackName;
                        else if (activeFunctionPackName != containedPackName)
                            WIO_LOG_ADD_ERROR(parameter.name->location(), "Function parameter packs must reference a single trailing generic pack.");
                    }
                }
            }

            if (activeFunctionPackName.empty())
                WIO_LOG_ADD_ERROR(node.location(), "Function parameter packs require a matching trailing generic parameter pack.");

            if (isCommand || isEvent || hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic pack functions currently support interop only through @Native and @Export. @Command, @Event, and module lifecycle attributes are not allowed."
                );
            }

            if (isExported && !hasInstantiate)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic @Export pack functions must declare at least one concrete @Instantiate(...)."
                );
            }

            if (node.whenCondition || node.whenFallback)
            {
                WIO_LOG_ADD_ERROR(node.location(), "when/else clauses are not supported on generic pack functions yet.");
            }

            if (node.returnType)
            {
                std::string returnPackName;
                if (containsGenericParameterPackType(funcType->returnType, &returnPackName) &&
                    (!activeFunctionPackName.empty() ? returnPackName != activeFunctionPackName : !returnPackName.empty()))
                {
                    WIO_LOG_ADD_ERROR(node.returnType->location(), "Generic parameter packs in function return positions must match the active trailing generic pack.");
                }
            }

            if (!activeFunctionPackName.empty())
            {
                const std::string& expectedPackName = activeFunctionPackName;
                bool sawParameterPack = false;

                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& parameter = node.parameters[i];
                    Ref<Type> parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : Compiler::get().getTypeContext().getUnknown();
                    std::string containedPackName;
                    const bool containsPack = containsGenericParameterPackType(parameterType, &containedPackName);

                    if (parameter.isParameterPack)
                    {
                        sawParameterPack = true;

                        if (i + 1 != node.parameters.size())
                        {
                            WIO_LOG_ADD_ERROR(parameter.name->location(), "Function parameter packs must be trailing.");
                        }

                        if (!containsPack || containedPackName != expectedPackName)
                        {
                            WIO_LOG_ADD_ERROR(
                                parameter.name->location(),
                                "Function parameter pack '{}' requires the trailing generic pack '{}...'.",
                                parameter.name->token.value,
                                expectedPackName
                            );
                        }
                    }
                    else if (containsPack)
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter.name->location(),
                            "Generic pack parameter '{}...' is currently supported only in the trailing function parameter position.",
                            containedPackName
                        );
                    }

                    if (parameter.isParameterPack && parameter.defaultValue)
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter.name->location(),
                            "Parameter pack '{}' cannot declare a default value. Default values apply only to fixed parameters before the trailing pack.",
                            parameter.name->token.value
                        );
                    }
                }

                if (!sawParameterPack && !node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic parameter pack '{}...' requires a matching trailing function parameter pack.", expectedPackName);
                }
            }
        }

        if (node.name->token.value == "Entry")
        {
            auto& typeContext = Compiler::get().getTypeContext();
            const Ref<Type> expectedArgsType = typeContext.getOrCreateArrayType(typeContext.getString(), ArrayType::ArrayKind::Dynamic);

            if (isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry is currently supported only for top-level functions.");
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry functions must define a Wio body.");
            }

            if (!isExactType(declaredResultType, typeContext.getI32()) &&
                !isExactType(declaredResultType, typeContext.getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry must return i32 or void.");
            }

            if (node.parameters.size() > 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry must declare zero parameters or exactly one string[] parameter.");
            }
            else if (node.parameters.size() == 1 && !isExactType(funcType->paramTypes[0], expectedArgsType))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Entry parameter must be string[] when present. Got '{}'.",
                    funcType->paramTypes[0] ? funcType->paramTypes[0]->toString() : "<unknown>"
                );
            }
        }

        if ((isCommand || isEvent) && !isExported)
        {
            WIO_LOG_ADD_ERROR(node.location(), "{} requires @Export on the same function.", isCommand ? "@Command" : "@Event");
        }

        if (isCommand)
        {
            auto commandArgs = getAllAttributeArgs(node.attributes, Attribute::Command);
            if (commandArgs.size() > 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Command accepts at most one command name argument.");
            }
            else if (!commandArgs.empty() &&
                     commandArgs.front().type != TokenType::identifier &&
                     commandArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Command expects an identifier path or a string literal command name.");
            }
        }

        if (isEvent)
        {
            auto eventArgs = getAllAttributeArgs(node.attributes, Attribute::Event);
            if (eventArgs.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event expects exactly one event name argument.");
            }
            else if (eventArgs.front().type != TokenType::identifier &&
                     eventArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event expects an identifier path or a string literal event name.");
            }

            if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event hooks must return void.");
            }
        }

        if (isNative)
        {
            if (node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Native functions cannot define a Wio body. Declare them with ';' only.");
            }

            if (node.isExtensionMethod)
            {
                auto extensionTarget = unwrapAliasType(currentExtensionTargetType_);
                auto extensionComponent = extensionTarget && extensionTarget->kind() == TypeKind::Struct
                    ? extensionTarget.AsFast<StructType>()
                    : nullptr;
                if (!extensionComponent || !extensionComponent->isNativePodComponent)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native extension methods require a declaration-level @Native component target. Use a Wio-bodied extension wrapper for ordinary components."
                    );
                }
            }

            if (isStructMethod)
            {
                if (!currentStruct || currentStruct->isInterface)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions."
                    );
                }
                else if (isComponentMethodContext && !isLifecycleMethod && !isOperatorMethod)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions."
                    );
                }
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as @Native.");
            }
        }

        if (isExported && !isGenericFunction)
        {
            if (isNative)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export cannot be combined with @Native.");
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export functions must define a Wio body.");
            }

            if (currentScope_ && currentScope_->getKind() == ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export is currently supported only for top-level functions.");
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as @Export.");
            }

            for (size_t i = 0; i < funcType->paramTypes.size(); ++i)
            {
                if (!isCAbiSafeExportType(funcType->paramTypes[i]))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export currently supports only primitive parameter types. Parameter {} in '{}' uses '{}'.",
                        i,
                        node.name->token.value,
                        funcType->paramTypes[i] ? funcType->paramTypes[i]->toString() : "<unknown>"
                    );
                }
            }

            const Ref<Type> exportedResultType = node.isAsync ? declaredResultType : funcType->returnType;
            if (!isCAbiSafeExportType(exportedResultType))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Export currently supports only primitive or void return types. '{}' returns '{}'.",
                    node.name->token.value,
                    exportedResultType ? exportedResultType->toString() : "<unknown>"
                );
            }
        }

        if (moduleLifecycleAttributes.size() > 1)
        {
            WIO_LOG_ADD_ERROR(node.location(), "A function can declare only one module lifecycle attribute.");
        }

        if (hasModuleLifecycle)
        {
            Attribute lifecycleAttribute = moduleLifecycleAttributes.front();
            bool* seenLifecycleFlag = nullptr;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (lifecycleAttribute)
            {
            case Attribute::ModuleApiVersion: seenLifecycleFlag = &seenModuleApiVersion_; break;
            case Attribute::ModuleLoad: seenLifecycleFlag = &seenModuleLoad_; break;
            case Attribute::ModuleUpdate: seenLifecycleFlag = &seenModuleUpdate_; break;
            case Attribute::ModuleUnload: seenLifecycleFlag = &seenModuleUnload_; break;
            case Attribute::ModuleSaveState: seenLifecycleFlag = &seenModuleSaveState_; break;
            case Attribute::ModuleRestoreState: seenLifecycleFlag = &seenModuleRestoreState_; break;
            default: break;
            }

            if (seenLifecycleFlag && *seenLifecycleFlag)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Only one {} function may be declared per compilation unit.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }
            else if (seenLifecycleFlag)
            {
                *seenLifecycleFlag = true;
            }

            if (isNative)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} cannot be combined with @Native.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (isExported)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} already defines a fixed exported symbol and cannot be combined with @Export.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} functions must define a Wio body.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (currentScope_ && currentScope_->getKind() == ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} is currently supported only for top-level functions.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Entry cannot be declared as {}.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (hasAttribute(node.attributes, Attribute::CppName))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} uses a fixed exported symbol and cannot be combined with @CppName.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (lifecycleAttribute)
            {
            case Attribute::ModuleApiVersion:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleApiVersion must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getU32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleApiVersion must return u32.");
                }
                break;
            case Attribute::ModuleLoad:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleLoad must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleLoad must return i32.");
                }
                break;
            case Attribute::ModuleUpdate:
                if (node.parameters.size() != 1 ||
                    !isExactType(funcType->paramTypes[0], Compiler::get().getTypeContext().getF32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUpdate must declare exactly one f32 parameter.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUpdate must return void.");
                }
                break;
            case Attribute::ModuleUnload:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUnload must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUnload must return void.");
                }
                break;
            case Attribute::ModuleSaveState:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleSaveState must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleSaveState must return i32.");
                }
                break;
            case Attribute::ModuleRestoreState:
                if (node.parameters.size() != 1 ||
                    !isExactType(funcType->paramTypes[0], Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleRestoreState must declare exactly one i32 parameter.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleRestoreState must return i32.");
                }
                break;
            default:
                break;
            }
        }

        if (hasAttribute(node.attributes, Attribute::CppHeader))
        {
            auto headerArgs = getAllAttributeArgs(node.attributes, Attribute::CppHeader);
            if (!isNative)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader can only be used together with @Native.");
            }
            else if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader expects exactly one string literal argument.");
            }
        }

        if (hasAttribute(node.attributes, Attribute::CppName))
        {
            auto cppNameArgs = getAllAttributeArgs(node.attributes, Attribute::CppName);
            if (!isNative && !isExported && !hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName can only be used together with @Native or @Export.");
            }
            else if (hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "{} uses a fixed exported symbol and cannot be combined with @CppName.", getModuleLifecycleAttributeName(moduleLifecycleAttributes.front()));
            }
            else if (cppNameArgs.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName expects exactly one target symbol argument.");
            }
            else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
            {
                if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName expects an identifier path like foo::bar or a string literal.");
                }
                else if (!isValidCppSymbolPath(cppNameArg->value, isNative))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        isNative
                            ? "@CppName for @Native must be a valid C++ identifier path like foo::bar."
                            : "@CppName for @Export must be a plain C/C++ symbol name like FooBar."
                    );
                }
            }
        }

        if (isNative)
        {
            auto validateNativeComponentInteropType = [&](const Ref<Type>& nativeType, std::string_view role, std::string_view displayName)
            {
                auto nativeComponent = getNativePodComponentStructType(nativeType);
                if (!nativeComponent)
                    return;

                validateInstantiatedNativePodComponent(nativeComponent, node.location());

                if (nativeComponent->nativeCppName.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native {} '{}' uses component '{}' directly, but that component is missing a declaration-level @CppName for its native C++ POD type.",
                        role,
                        displayName,
                        nativeComponent->name
                    );
                }
            };

            for (size_t parameterIndex = 0; parameterIndex < funcType->paramTypes.size(); ++parameterIndex)
            {
                const std::string parameterName =
                    parameterIndex < node.parameters.size() && node.parameters[parameterIndex].name
                        ? node.parameters[parameterIndex].name->token.value
                        : common::formatString("param{}", parameterIndex);
                validateNativeComponentInteropType(funcType->paramTypes[parameterIndex], "parameter", parameterName);
            }

            if (funcType->returnType && !funcType->returnType->isVoid())
                validateNativeComponentInteropType(funcType->returnType, "return type of", node.name->token.value);
        }

        Ref<Type> prevRetType = currentFunctionReturnType_;
        bool previousFunctionIsAsync = currentFunctionIsAsync_;
        Ref<Symbol> prevFunctionParameterPackSymbol = currentFunctionParameterPackSymbol_;
        Ref<Type> prevFunctionParameterPackType = currentFunctionParameterPackType_;
        currentFunctionReturnType_ = declaredResultType;
        currentFunctionIsAsync_ = false;
        currentFunctionParameterPackSymbol_ = nullptr;
        currentFunctionParameterPackType_ = nullptr;

        auto genericScope = buildGenericTypeParameterScope();
        genericTypeParameterScopes_.push_back(genericScope);
        if (!funcSym->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(funcSym);

        bool sawDefaultParameter = false;
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            if (!param.defaultValue)
            {
                if (sawDefaultParameter)
                {
                    const bool isTrailingParameterPack =
                        param.isParameterPack &&
                        i + 1 == node.parameters.size() &&
                        funcType->hasParameterPack;
                    if (!isTrailingParameterPack)
                    {
                        WIO_LOG_ADD_ERROR(
                            param.name ? param.name->location() : node.location(),
                            "Parameters with default values must be trailing."
                        );
                    }
                }
                continue;
            }

            sawDefaultParameter = true;

            if (!param.type || !funcType->paramTypes[i] || funcType->paramTypes[i]->isUnknown())
            {
                WIO_LOG_ADD_ERROR(
                    param.name ? param.name->location() : param.defaultValue->location(),
                    "Default parameters require an explicit parameter type."
                );
                continue;
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(param.defaultValue->location(), "Entry cannot declare default parameters.");
            }

            if (hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "{} cannot declare default parameters.",
                    getModuleLifecycleAttributeName(moduleLifecycleAttributes.front())
                );
            }

            if (!node.body && !isNative)
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "Default parameters are currently supported only on functions with Wio bodies or on @Native declarations."
                );
            }

            if (param.isParameterPack)
            {
                continue;
            }

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = funcType->paramTypes[i];
            allowContextualNumericLiteralTyping_ = true;
            param.defaultValue->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

            Ref<Type> defaultType = param.defaultValue->refType.Lock();
            if (funcType->paramTypes[i] &&
                !funcType->paramTypes[i]->isUnknown() &&
                defaultType &&
                !defaultType->isUnknown() &&
                !isAssignmentLikeCompatible(funcType->paramTypes[i], defaultType))
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "Default argument type mismatch for parameter '{}'. Expected '{}', but got '{}'.",
                    param.name ? param.name->token.value : "<parameter>",
                    funcType->paramTypes[i]->toString(),
                    defaultType->toString()
                );
            }
        }

        if (auto overloadSymbol = currentScope_ ? currentScope_->resolve(node.name->token.value) : nullptr;
            overloadSymbol && overloadSymbol->kind == SymbolKind::FunctionGroup)
        {
            if (!funcType->hasParameterPack)
            {
                const size_t currentRequiredParameterCount = getRequiredParameterCount(node);
                const size_t currentTotalParameterCount = getFixedParameterCount(node);

                for (const auto& overload : overloadSymbol->overloads)
                {
                    if (!overload || overload == funcSym || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    auto foundDeclaration = functionDeclarationsBySymbol_.find(overload.Get());
                    if (foundDeclaration == functionDeclarationsBySymbol_.end() || !foundDeclaration->second)
                        continue;

                    const auto* otherDeclaration = foundDeclaration->second;
                    auto otherFunctionType = overload->type.AsFast<FunctionType>();
                    if (otherFunctionType && otherFunctionType->hasParameterPack)
                        continue;
                    const size_t otherRequiredParameterCount = getRequiredParameterCount(otherDeclaration);
                    const size_t otherTotalParameterCount = getFixedParameterCount(*otherDeclaration);

                    if (currentRequiredParameterCount != currentTotalParameterCount &&
                        otherTotalParameterCount >= currentRequiredParameterCount &&
                        otherTotalParameterCount < currentTotalParameterCount)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Default parameters on '{}' would synthesize an overload with {} arguments, but that signature is already declared explicitly.",
                            node.name->token.value,
                            otherTotalParameterCount
                        );
                    }

                    if (otherRequiredParameterCount != otherTotalParameterCount &&
                        currentTotalParameterCount >= otherRequiredParameterCount &&
                        currentTotalParameterCount < otherTotalParameterCount)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Function '{}' conflicts with a default-parameter overload that already covers {} arguments.",
                            node.name->token.value,
                            currentTotalParameterCount
                        );
                    }
                }
            }
        }

        if (hasInstantiate)
        {
            for (const auto& instantiationTypes : funcSym->resolvedGenericInstantiations)
            {
                if (isExported)
                {
                    auto instantiationBindings = buildExtendedGenericBindings(
                        funcSym->genericParameterNames,
                        funcSym->hasGenericParameterPack,
                        instantiationTypes
                    );
                    Ref<Type> instantiatedFunctionTypeRef = instantiateGenericType(funcType, instantiationBindings);
                    auto instantiatedFunctionType = instantiatedFunctionTypeRef ? instantiatedFunctionTypeRef.AsFast<FunctionType>() : nullptr;

                    if (!instantiatedFunctionType)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Instantiate failed to produce a concrete exported signature.");
                        continue;
                    }

                    const std::string instantiationSignature = formatConcreteInstantiationSignature(instantiationTypes);

                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        if (!isCAbiSafeExportType(instantiatedFunctionType->paramTypes[i]))
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "@Export instantiated with '{}' produces a non-C-ABI-safe parameter {} of type '{}'.",
                                instantiationSignature,
                                i,
                                instantiatedFunctionType->paramTypes[i] ? instantiatedFunctionType->paramTypes[i]->toString() : "<unknown>"
                            );
                        }
                    }

                    Ref<Type> instantiatedExportResult = instantiatedFunctionType->returnType;
                    if (node.isAsync)
                    {
                        Ref<Type> resolvedTask = unwrapAliasType(instantiatedExportResult);
                        instantiatedExportResult = resolvedTask && resolvedTask->kind() == TypeKind::AsyncTask
                            ? resolvedTask.AsFast<AsyncTaskType>()->valueType
                            : Compiler::get().getTypeContext().getUnknown();
                    }
                    if (!isCAbiSafeExportType(instantiatedExportResult))
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Export instantiated with '{}' produces a non-C-ABI-safe return type '{}'.",
                            instantiationSignature,
                            instantiatedExportResult ? instantiatedExportResult->toString() : "<unknown>"
                        );
                    }
                }
            }
        }

        std::unordered_set<std::string> localExportSymbols;
        auto registerExportedSymbol = [&](const std::string& symbolName)
        {
            if (symbolName.empty() || !localExportSymbols.insert(symbolName).second)
                return;

            auto [it, inserted] = exportedCppSymbolLocations_.emplace(symbolName, node.location());
            if (!inserted)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Exported C++ symbol '{}' is already declared by another @Export or module lifecycle function.",
                    symbolName
                );
            }
        };

        if (hasModuleLifecycle || isExported)
        {
            const std::string baseExportSymbolName = getDeclaredExportSymbolName(node, hasModuleLifecycle);
            if (!node.genericParameters.empty() && isExported)
            {
                for (const auto& instantiationTypes : funcSym->resolvedGenericInstantiations)
                {
                    registerExportedSymbol(formatInstantiatedExportSymbolName(baseExportSymbolName, instantiationTypes));
                }
            }
            else
            {
                registerExportedSymbol(baseExportSymbolName);
            }
        }

        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];

            SymbolFlags parameterFlags = SymbolFlags::createAllFalse();
            if (param.isParameterPack)
                parameterFlags.set_isParameterPack(true);

            Ref<Symbol> pSym = createSymbol(param.name->token.value, funcType->paramTypes[i], SymbolKind::Parameter, param.name->location(), parameterFlags);
            currentScope_->define(param.name->token.value, pSym);

            param.name->referencedSymbol = pSym;
            param.name->refType = funcType->paramTypes[i];

            if (param.isParameterPack)
            {
                currentFunctionParameterPackSymbol_ = pSym;
                currentFunctionParameterPackType_ = funcType->paramTypes[i];
            }
        }

        currentFunctionIsAsync_ = node.isAsync;

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
                Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
                bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
                currentExpectedExpressionType_ = currentFunctionReturnType_;
                allowContextualNumericLiteralTyping_ = true;
                node.whenFallback->accept(*this);
                currentExpectedExpressionType_ = previousExpectedExpressionType;
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

        if (node.body && !isNative && requiresReturnValue && !allPathsReturn)
        {
            WIO_LOG_ADD_ERROR(
                node.name ? node.name->location() : node.location(),
                "Non-void function '{}' must return a value on all control-flow paths.",
                node.name ? node.name->token.value : "<function>"
            );
        }

        exitScope();
        genericTypeParameterScopes_.pop_back();
        if (!funcSym->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentFunctionReturnType_ = prevRetType;
        currentFunctionIsAsync_ = previousFunctionIsAsync;
        currentFunctionParameterPackSymbol_ = prevFunctionParameterPackSymbol;
        currentFunctionParameterPackType_ = prevFunctionParameterPackType;
    }

    void SemanticAnalyzer::visit(RealmDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            Ref<Symbol> realmSym = currentScope_->resolveLocally(node.name->token.value);
            if (realmSym)
            {
                if (realmSym->kind != SymbolKind::Namespace)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and is not a realm.", node.name->token.value);
                    return;
                }

                if (!realmSym->innerScope)
                {
                    auto realmScope = Ref<Scope>::Create(currentScope_, ScopeKind::Global);
                    scopes_.push_back(realmScope);
                    realmSym->innerScope = realmScope;
                }
            }
            else
            {
                auto realmScope = Ref<Scope>::Create(currentScope_, ScopeKind::Global);
                scopes_.push_back(realmScope);

                realmSym = createSymbol(node.name->token.value, Compiler::get().getTypeContext().getUnknown(), SymbolKind::Namespace, node.location());
                realmSym->innerScope = realmScope;
                currentScope_->define(node.name->token.value, realmSym);
            }

            node.name->referencedSymbol = realmSym;
            node.name->refType = realmSym->type;
        }

        auto realmSym = node.name->referencedSymbol.Lock();
        if (!realmSym || !realmSym->innerScope)
            return;

        auto prevScope = currentScope_;
        const size_t scopedAttributeCount = activeScopedAttributes_.size();
        currentScope_ = realmSym->innerScope;
        currentNamespacePath_.push_back(node.name->token.value);

        for (auto& statement : node.statements)
        {
            if (isAttributeContractPass_)
            {
                if (statement->is<AttributeDeclaration>() ||
                    statement->is<DeclarationGroup>() ||
                    statement->is<RealmDeclaration>())
                {
                    statement->accept(*this);
                }
            }
            else if (isDeriveExpansionPass_)
            {
                if (statement->is<ComponentDeclaration>() ||
                    statement->is<ObjectDeclaration>() ||
                    statement->is<DeclarationGroup>() ||
                    statement->is<RealmDeclaration>())
                {
                    statement->accept(*this);
                }
            }
            else if (isStructResolutionPass_)
            {
                if (statement->is<ComponentDeclaration>() ||
                    statement->is<ExtensionDeclaration>() ||
                    statement->is<ObjectDeclaration>() ||
                    statement->is<EnumDeclaration>() ||
                    statement->is<FlagsetDeclaration>() ||
                    statement->is<FlagDeclaration>() ||
                    statement->is<RealmDeclaration>())
                {
                    statement->accept(*this);
                }
            }
            else
            {
                statement->accept(*this);
            }
        }

        currentNamespacePath_.pop_back();
        activeScopedAttributes_.resize(scopedAttributeCount);
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(InterfaceDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "interface");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "interface");
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this interface.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Specialize is supported only on generic object and component declarations."
                );
            }

            auto interfaceScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(interfaceScope);

            Ref<Type> interfaceType = Ref<StructType>::Create(node.name->token.value, interfaceScope, false, true);
            interfaceType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            interfaceType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    interfaceType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
            }
            interfaceType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "interface");
            validateApplyAttributes(*this, node.attributes, interfaceType.AsFast<StructType>()->genericParameterNames, "interface", node.location());
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> interfaceSym = createSymbol(node.name->token.value, interfaceType, SymbolKind::Struct, node.location());
            interfaceSym->innerScope = interfaceScope;
            interfaceSym->flags.set_isInterface(true);
            interfaceSym->genericParameterNames = interfaceType.AsFast<StructType>()->genericParameterNames;
            interfaceType.AsFast<StructType>()->genericParameterTypes = collectGenericParameterSemanticTypes(node.genericParameters);
            interfaceSym->genericParameterTypes = interfaceType.AsFast<StructType>()->genericParameterTypes;
            interfaceType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            interfaceSym->genericParameterDefaults = std::move(genericParameterDefaults);
            interfaceSym->hasGenericParameterPack = node.hasGenericParameterPack;
            currentScope_->define(node.name->token.value, interfaceSym);
            attributeListsBySymbol_[interfaceSym.Get()] = &node.attributes;

            node.name->refType = interfaceType;
            node.name->referencedSymbol = interfaceSym;

            auto prevScope = currentScope_;
            currentScope_ = interfaceScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& method : node.methods) method->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());

        for (auto& method : node.methods)
            method->accept(*this);

        genericTypeParameterScopes_.pop_back();
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ExtensionDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "extension");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "extension");
        if (isDeclarationPass_)
            return;

        if (isStructResolutionPass_)
        {
            node.targetType->accept(*this);
            Ref<Type> targetType = unwrapAliasType(node.targetType->refType.Lock());
            if (!targetType || targetType->kind() != TypeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.targetType->location(), "Extension target must be a component type.");
                return;
            }

            auto targetStruct = targetType.AsFast<StructType>();
            if (targetStruct->isObject || targetStruct->isInterface)
            {
                WIO_LOG_ADD_ERROR(node.targetType->location(), "Extensions currently support component types only.");
                return;
            }

            Ref<Type> previousExtensionTarget = currentExtensionTargetType_;
            currentExtensionTargetType_ = targetType;
            const bool previousDeclarationPass = isDeclarationPass_;
            isDeclarationPass_ = true;

            for (auto& member : node.members)
            {
                auto& method = member.method;
                if (!method)
                    continue;
                if (member.access == AccessModifier::Private ||
                    member.access == AccessModifier::Protected)
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension methods are external APIs and must be public.");
                    continue;
                }
                if (common::isOperatorOverloadName(method->extensionMemberName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension operator overloads are not supported.");
                    continue;
                }
                const std::string publicName = method->extensionMemberName;
                if (auto scope = targetStruct->structScope.Lock();
                    scope && scope->resolveLocally(publicName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension method '{}' conflicts with a component member.", publicName);
                    continue;
                }

                method->name->token.value =
                    "__extension_" + node.name->token.value + "_" + publicName;
                currentExtensionMutableReceiver_ = member.mutableReceiver;
                method->accept(*this);

                auto symbol = method->name->referencedSymbol.Lock();
                if (!symbol)
                    continue;
                symbol->flags.set_isExtension(true);
                symbol->extensionTargetType = targetType;
                symbol->extensionMemberName = publicName;
                method->extensionTargetType = targetType;

                auto& methods = extensionMethods_[targetType.Get()];
                if (methods.contains(publicName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension method '{}' is ambiguous for '{}'.",
                        publicName, targetType->toString());
                }
                else
                {
                    methods.emplace(publicName, symbol);
                }
            }

            isDeclarationPass_ = previousDeclarationPass;
            currentExtensionTargetType_ = previousExtensionTarget;
            currentExtensionMutableReceiver_ = false;
            return;
        }

        Ref<Type> previousExtensionTarget = currentExtensionTargetType_;
        const bool previousMutableReceiver = currentExtensionMutableReceiver_;
        currentExtensionTargetType_ = unwrapAliasType(node.targetType->refType.Lock());
        for (auto& member : node.members)
        {
            if (!member.method || !member.method->name->referencedSymbol.Lock())
                continue;
            currentExtensionMutableReceiver_ = member.mutableReceiver;
            member.method->accept(*this);
        }
        currentExtensionTargetType_ = previousExtensionTarget;
        currentExtensionMutableReceiver_ = previousMutableReceiver;
    }

    void SemanticAnalyzer::visit(ComponentDeclaration& node)
    {
        const std::string attributeTarget = node.attributeTargetOverride.empty()
            ? "component"
            : node.attributeTargetOverride;
        if (isDeriveExpansionPass_)
        {
            Ref<Symbol> symbol = node.name ? node.name->referencedSymbol.Lock() : nullptr;
            registerDerivedMethods(node.attributes, symbol ? symbol->type : nullptr, attributeTarget);
            return;
        }
        applyActiveScopedAttributes(node.attributes, attributeTarget);
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, attributeTarget);
        const bool isExplicitSpecialization = hasAttribute(node.attributes, Attribute::Specialize);

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this component.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            Ref<Symbol> genericPrimarySymbol = nullptr;
            Ref<StructType> genericPrimaryType = nullptr;
            std::vector<Ref<Type>> specializationArguments;
            bool specializationCanRegister = false;

            if (isExplicitSpecialization)
            {
                const bool hasDeclarationLevelNativeInterop = hasAttribute(node.attributes, Attribute::Native);

                auto specializationScope = buildGenericTypeParameterScope();
                genericTypeParameterScopes_.push_back(specializationScope);
                specializationArguments = resolveExplicitSpecializationArguments(
                    *this, node.attributes, node.location(), !node.genericParameters.empty());
                genericTypeParameterScopes_.pop_back();

                genericPrimarySymbol = currentScope_->resolveLocally(node.name->token.value);
                if (!genericPrimarySymbol || genericPrimarySymbol->kind != SymbolKind::Struct ||
                    !genericPrimarySymbol->type || genericPrimarySymbol->type->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Specialize component '{}' requires an earlier generic component declaration with the same name.",
                        node.name->token.value
                    );
                }
                else
                {
                    genericPrimaryType = genericPrimarySymbol->type.AsFast<StructType>();
                    if (genericPrimaryType->isObject || genericPrimaryType->isInterface)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Specialize component '{}' must target a generic component, not an object or interface.",
                            node.name->token.value
                        );
                    }
                    else if (genericPrimaryType->genericParameterNames.empty())
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Specialize target '{}' is not generic.", node.name->token.value);
                    }
                    else
                    {
                        if (hasDeclarationLevelNativeInterop && !genericPrimaryType->isNativePodComponent)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "A declaration-level @Native specialization requires an @Native generic component primary. Native ABI identity cannot begin at a specialization."
                            );
                        }
                        const size_t minimumArgumentCount = getMinimumGenericArgumentCount(
                            genericPrimaryType->genericParameterNames,
                            genericPrimaryType->hasGenericParameterPack
                        );
                        const bool invalidArity = genericPrimaryType->hasGenericParameterPack
                            ? specializationArguments.size() < minimumArgumentCount
                            : specializationArguments.size() != genericPrimaryType->genericParameterNames.size();
                        if (invalidArity)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                genericPrimaryType->hasGenericParameterPack
                                    ? "@Specialize for '{}' expects at least {} type arguments, but got {}."
                                    : "@Specialize for '{}' expects {} type arguments, but got {}.",
                                node.name->token.value,
                                genericPrimaryType->hasGenericParameterPack
                                    ? minimumArgumentCount
                                    : genericPrimaryType->genericParameterNames.size(),
                                specializationArguments.size()
                            );
                        }
                        else
                        {
                            specializationCanRegister = std::ranges::all_of(
                                specializationArguments,
                                [](const Ref<Type>& type)
                                {
                                    return type && !type->isUnknown();
                                }
                            );
                        }
                    }
                }
            }

            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);

            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            if (isExplicitSpecialization)
            {
                structType.AsFast<StructType>()->genericArguments = specializationArguments;
                structType.AsFast<StructType>()->genericPrimaryType = genericPrimaryType;
                structType.AsFast<StructType>()->isExplicitSpecialization = true;
                structType.AsFast<StructType>()->isPartialSpecialization = !node.genericParameters.empty();
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            else
            {
                structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);
            const bool isNativePodComponent = isExplicitSpecialization && genericPrimaryType
                ? genericPrimaryType->isNativePodComponent
                : hasAttribute(node.attributes, Attribute::Native);
            structType.AsFast<StructType>()->isNativePodComponent = isNativePodComponent;
            structType.AsFast<StructType>()->nativeCppName =
                isExplicitSpecialization && genericPrimaryType
                    ? genericPrimaryType->nativeCppName
                    : node.name ? node.name->token.value : "";
            structType.AsFast<StructType>()->nativeCppHeader =
                isExplicitSpecialization && genericPrimaryType
                    ? genericPrimaryType->nativeCppHeader
                    : "";

            if (isNativePodComponent)
            {
                const bool inheritsNativeMapping = isExplicitSpecialization && genericPrimaryType;
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAllAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native component expects exactly one string literal argument.");
                    }
                    else if (inheritsNativeMapping && headerArgs.front().value != genericPrimaryType->nativeCppHeader)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "A native component specialization must inherit @CppHeader from its generic primary.");
                    }
                    else
                    {
                        structType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAllAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component must be a valid C++ identifier path like foo::bar.");
                        }
                        else if (inheritsNativeMapping && cppNameArg->value != genericPrimaryType->nativeCppName)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "A native component specialization must inherit @CppName from its generic primary.");
                        }
                        else
                        {
                            structType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on components currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on components currently requires declaration-level @Native.");
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "component");
            if (!isExplicitSpecialization)
                validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "component", node.location());
            else if (hasAttribute(node.attributes, Attribute::Apply))
                WIO_LOG_ADD_ERROR(node.location(), "@Apply cannot be declared on an explicit specialization; constrain the generic primary declaration.");
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> compSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            compSym->innerScope = structScope;
            compSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            structType.AsFast<StructType>()->genericParameterTypes = collectGenericParameterSemanticTypes(node.genericParameters);
            compSym->genericParameterTypes = structType.AsFast<StructType>()->genericParameterTypes;
            if (isNativePodComponent)
            {
                for (size_t index = 0; index < compSym->genericParameterTypes.size(); ++index)
                {
                    if (!isTextualConstGenericParameterType(compSym->genericParameterTypes[index]))
                        continue;
                    WIO_LOG_ADD_ERROR(
                        index < node.genericParameters.size() && node.genericParameters[index]
                            ? node.genericParameters[index]->location()
                            : node.location(),
                        "@Native components support only integer const generic parameters; string/text const values use a Wio-owned structural backend representation."
                    );
                }
            }
            structType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            compSym->genericParameterDefaults = std::move(genericParameterDefaults);
            compSym->hasGenericParameterPack = structType.AsFast<StructType>()->hasGenericParameterPack;
            if (isExplicitSpecialization)
            {
                if (specializationCanRegister && genericPrimaryType)
                {
                    const std::string specializationKey = getGenericSpecializationKey(specializationArguments);
                    const bool isPartialSpecialization = structType.AsFast<StructType>()->isPartialSpecialization;
                    const bool duplicatePartial = isPartialSpecialization && std::ranges::any_of(
                        genericPrimaryType->partialSpecializations,
                        [&](const WeakRef<StructType>& candidate)
                        {
                            auto partial = candidate.Lock();
                            return partial && getGenericSpecializationKey(partial->genericArguments) == specializationKey;
                        });
                    if ((!isPartialSpecialization && genericPrimaryType->explicitSpecializations.contains(specializationKey)) || duplicatePartial)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Duplicate explicit specialization for '{}{}'.",
                            node.name->token.value,
                            formatConcreteInstantiationSignature(specializationArguments)
                        );
                    }
                    else
                    {
                        if (isPartialSpecialization)
                            genericPrimaryType->partialSpecializations.emplace_back(structType.AsFast<StructType>());
                        else
                            genericPrimaryType->explicitSpecializations.emplace(specializationKey, structType.AsFast<StructType>());
                    }
                }
            }
            else
            {
                currentScope_->define(node.name->token.value, compSym);
            }
            attributeListsBySymbol_[compSym.Get()] = &node.attributes;

            node.name->refType = structType;
            node.name->referencedSymbol = compSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& member : node.members) member.declaration->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();

        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            Ref<Type> generatedSelfType = structType;
            if (!structType->genericParameterNames.empty())
            {
                std::vector<Ref<Type>> genericSelfArguments;
                genericSelfArguments.reserve(structType->genericParameterNames.size());
                for (const auto& genericParameterName : structType->genericParameterNames)
                {
                    if (auto genericIt = genericScope.find(genericParameterName); genericIt != genericScope.end())
                        genericSelfArguments.push_back(genericIt->second);
                }

                if (genericSelfArguments.size() == structType->genericParameterNames.size())
                    generatedSelfType = instantiateGenericStructType(structType, genericSelfArguments);
            }

            bool hasCustomCtor = false;
            bool hasEmptyCtor = false;
            bool hasCopyCtor = false;
            bool hasMemberCtor = false;

            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
            auto bases = getAllAttributeArgs(node.attributes, Attribute::From);
            if (!bases.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Components must be POD (Plain Old Data) and cannot inherit from objects or interfaces.");
            }

            AccessModifier defaultAccess = getDefaultAccessModifier(node.attributes, AccessModifier::Public);
            structType->trustedTypeKeys.clear();

            if (structType->isNativePodComponent)
            {
                for (const auto& member : node.members)
                {
                    if (!member.declaration)
                        continue;

                    if (!member.declaration->is<VariableDeclaration>())
                    {
                        WIO_LOG_ADD_ERROR(
                            member.declaration->location(),
                            "Declaration-level @Native components currently support only POD-style fields. Lifecycle functions and methods are not supported on the component declaration itself."
                        );
                        continue;
                    }
                }
            }

            for (const auto& trustArg : getAttributeTypeArgs(node.attributes, Attribute::Trust))
            {
                auto trustedStruct = resolveTrustedStructType(*this, prevScope, trustArg, node.location());
                if (!trustedStruct)
                    continue;

                const std::string trustedKey = getStructIdentityKey(trustedStruct);
                if (!trustedKey.empty() &&
                    std::ranges::find(structType->trustedTypeKeys, trustedKey) == structType->trustedTypeKeys.end())
                {
                    structType->trustedTypeKeys.push_back(trustedKey);
                }
            }

            structType->fieldNames.clear();
            structType->fieldTypes.clear();

            // PASS 1: Variables
            std::vector<Ref<Type>> memberTypes;
            for (auto& member : node.members)
            {
                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    auto memberSym = varDecl->name->referencedSymbol.Lock();
                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);

                    if (memberSym && memberSym->type)
                    {
                        memberTypes.push_back(memberSym->type);
                        structType->fieldNames.push_back(varDecl->name->token.value);
                        structType->fieldTypes.push_back(memberSym->type);

                        if (structType->isNativePodComponent && !isNativePodInteropFieldType(memberSym->type, true))
                        {
                            WIO_LOG_ADD_ERROR(
                                varDecl->location(),
                                "Declaration-level @Native component '{}' field '{}' uses type '{}' which is not POD-native-compatible yet. Supported field types are primitives, POD-compatible static arrays, and other declaration-level @Native components.",
                                node.name->token.value,
                                varDecl->name->token.value,
                                memberSym->type ? memberSym->type->toString() : "<unknown>"
                            );
                        }
                    }

                    if (member.access == AccessModifier::None) member.access = defaultAccess;
                    if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            // PASS 2: Functions
            for (auto& member : node.members)
            {
                if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    auto memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;
                    const bool isOperatorMethod = common::isOperatorOverloadName(funcName);

                    if (funcName != "OnConstruct" && funcName != "OnDestruct" && !isOperatorMethod)
                    {
                        WIO_LOG_ADD_ERROR(
                            funcDecl->location(),
                            "Components cannot define ordinary methods. Use an object for behavior, an operator overload, or OnConstruct/OnDestruct for lifecycle."
                        );
                    }

                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                        size_t pCount = funcDecl->parameters.size();

                        if (pCount == 0) hasEmptyCtor = true;
                        else if (pCount == 1)
                        {
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                if (fType->paramTypes[0]->kind() == TypeKind::Reference &&
                                    fType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType) {
                                    hasCopyCtor = true;
                                }
                            }
                        }

                        if (pCount == memberTypes.size() && !(pCount == 1 && hasCopyCtor))
                        {
                            bool typesMatch = true;
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                for (size_t i = 0; i < pCount; ++i) {
                                    if (!fType->paramTypes[i]->isCompatibleWith(memberTypes[i])) {
                                        typesMatch = false; break;
                                    }
                                }
                            }
                            if (typesMatch) hasMemberCtor = true;
                        }
                    }

                    if (memberSym)
                    {
                        if (member.access == AccessModifier::None) member.access = defaultAccess;
                        if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                        else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                        else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                    }
                }
            }

            // PASS 3: Generate Constructors
            if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                if (!hasEmptyCtor) {
                    auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
                    Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", defaultSym);
                }

                if (!hasMemberCtor && !memberTypes.empty()) {
                    auto memberCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                if (!hasCopyCtor) {
                    auto copyParamType = Compiler::get().getTypeContext().getOrCreateReferenceType(generatedSelfType, false);
                    auto copyCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, { copyParamType });
                    Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", copySym);
                }
            }

            if (hasAttribute(node.attributes, Attribute::Export))
            {
                if (!node.genericParameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@Export is not yet supported for generic components.");
                }

                bool hasHostCallableConstructor = false;

                for (const auto& member : node.members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor =
                        functionType->paramTypes.size() == 1 &&
                        functionType->paramTypes[0]->kind() == TypeKind::Reference &&
                        functionType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType;
                    if (isCopyCtor)
                        continue;

                    if (member.access != AccessModifier::Public)
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export component '{}' constructor must be public to be host-callable.",
                            node.name->token.value
                        );
                    }

                    bool allParametersAbiSafe = true;
                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                            continue;

                        allParametersAbiSafe = false;
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export component '{}' constructor parameter {} uses non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            parameterIndex,
                            functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                        );
                    }

                    if (member.access == AccessModifier::Public && allParametersAbiSafe)
                        hasHostCallableConstructor = true;
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        hasHostCallableConstructor = true;

                    if (!hasMemberCtor && !memberTypes.empty())
                    {
                        bool memberCtorAbiSafe = true;
                        for (const auto& memberType : memberTypes)
                        {
                            if (isCAbiSafeExportType(memberType))
                                continue;

                            memberCtorAbiSafe = false;
                            break;
                        }

                        if (memberCtorAbiSafe)
                            hasHostCallableConstructor = true;
                    }
                }

                if (!hasHostCallableConstructor)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export component '{}' must expose at least one public host-callable constructor with only C-ABI-safe parameters.",
                        node.name->token.value
                    );
                }

                for (const auto& member : node.members)
                {
                    if (member.access != AccessModifier::Public || !member.declaration || !member.declaration->is<VariableDeclaration>())
                        continue;

                    auto variableDecl = member.declaration->as<VariableDeclaration>();
                    auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                    Ref<Type> fieldType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock();
                    if (!isSdkExportableFieldType(fieldType))
                    {
                        WIO_LOG_ADD_ERROR(
                            variableDecl->location(),
                            "@Export component '{}' exposes public field '{}' with type '{}' that is not yet SDK-exportable.",
                            node.name->token.value,
                            variableDecl->name->token.value,
                            fieldType ? fieldType->toString() : "<unknown>"
                        );
                    }
                }
            }

            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(sym);

        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
                validateAttributeApplications(member.declaration->as<VariableDeclaration>()->attributes, "field");
            else if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);
        }

        genericTypeParameterScopes_.pop_back();
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ObjectDeclaration& node)
    {
        if (isDeriveExpansionPass_)
        {
            Ref<Symbol> symbol = node.name ? node.name->referencedSymbol.Lock() : nullptr;
            registerDerivedMethods(node.attributes, symbol ? symbol->type : nullptr, "object");
            return;
        }
        applyActiveScopedAttributes(node.attributes, "object");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "object");
        const bool isExplicitSpecialization = hasAttribute(node.attributes, Attribute::Specialize);

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this object.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = createGenericParameterSemanticType(
                    *this, genericParameter, isGenericParameterPack, "generic declaration");
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            Ref<Symbol> genericPrimarySymbol = nullptr;
            Ref<StructType> genericPrimaryType = nullptr;
            std::vector<Ref<Type>> specializationArguments;
            bool specializationCanRegister = false;

            if (isExplicitSpecialization)
            {
                auto specializationScope = buildGenericTypeParameterScope();
                genericTypeParameterScopes_.push_back(specializationScope);
                specializationArguments = resolveExplicitSpecializationArguments(
                    *this, node.attributes, node.location(), !node.genericParameters.empty());
                genericTypeParameterScopes_.pop_back();

                genericPrimarySymbol = currentScope_->resolveLocally(node.name->token.value);
                if (!genericPrimarySymbol || genericPrimarySymbol->kind != SymbolKind::Struct ||
                    !genericPrimarySymbol->type || genericPrimarySymbol->type->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Specialize object '{}' requires an earlier generic object declaration with the same name.",
                        node.name->token.value
                    );
                }
                else
                {
                    genericPrimaryType = genericPrimarySymbol->type.AsFast<StructType>();
                    if (!genericPrimaryType->isObject || genericPrimaryType->isInterface)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Specialize object '{}' must target a generic object, not a component or interface.",
                            node.name->token.value
                        );
                    }
                    else if (genericPrimaryType->genericParameterNames.empty())
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Specialize target '{}' is not generic.", node.name->token.value);
                    }
                    else
                    {
                        const size_t minimumArgumentCount = getMinimumGenericArgumentCount(
                            genericPrimaryType->genericParameterNames,
                            genericPrimaryType->hasGenericParameterPack
                        );
                        const bool invalidArity = genericPrimaryType->hasGenericParameterPack
                            ? specializationArguments.size() < minimumArgumentCount
                            : specializationArguments.size() != genericPrimaryType->genericParameterNames.size();
                        if (invalidArity)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                genericPrimaryType->hasGenericParameterPack
                                    ? "@Specialize for '{}' expects at least {} type arguments, but got {}."
                                    : "@Specialize for '{}' expects {} type arguments, but got {}.",
                                node.name->token.value,
                                genericPrimaryType->hasGenericParameterPack
                                    ? minimumArgumentCount
                                    : genericPrimaryType->genericParameterNames.size(),
                                specializationArguments.size()
                            );
                        }
                        else
                        {
                            specializationCanRegister = std::ranges::all_of(
                                specializationArguments,
                                [](const Ref<Type>& type)
                                {
                                    return type && !type->isUnknown();
                                }
                            );
                        }
                    }
                }
            }

            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);

            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope, true);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            if (isExplicitSpecialization)
            {
                structType.AsFast<StructType>()->genericArguments = specializationArguments;
                structType.AsFast<StructType>()->genericPrimaryType = genericPrimaryType;
                structType.AsFast<StructType>()->isExplicitSpecialization = true;
                structType.AsFast<StructType>()->isPartialSpecialization = !node.genericParameters.empty();
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            else
            {
                structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);

            if (hasAttribute(node.attributes, Attribute::Native))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Declaration-level @Native POD interop is currently supported only on components. Objects are not supported yet.");
            }
            if (hasAttribute(node.attributes, Attribute::CppHeader))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on objects is reserved for future declaration-level native object interop.");
            }
            if (hasAttribute(node.attributes, Attribute::CppName))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName on objects is reserved for future declaration-level native object interop.");
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "object");
            if (!isExplicitSpecialization)
                validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "object", node.location());
            else if (hasAttribute(node.attributes, Attribute::Apply))
                WIO_LOG_ADD_ERROR(node.location(), "@Apply cannot be declared on an explicit specialization; constrain the generic primary declaration.");
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> objSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            objSym->innerScope = structScope;
            objSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            structType.AsFast<StructType>()->genericParameterTypes = collectGenericParameterSemanticTypes(node.genericParameters);
            objSym->genericParameterTypes = structType.AsFast<StructType>()->genericParameterTypes;
            structType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            objSym->genericParameterDefaults = std::move(genericParameterDefaults);
            objSym->hasGenericParameterPack = structType.AsFast<StructType>()->hasGenericParameterPack;
            if (isExplicitSpecialization)
            {
                if (specializationCanRegister && genericPrimaryType)
                {
                    const std::string specializationKey = getGenericSpecializationKey(specializationArguments);
                    const bool isPartialSpecialization = structType.AsFast<StructType>()->isPartialSpecialization;
                    const bool duplicatePartial = isPartialSpecialization && std::ranges::any_of(
                        genericPrimaryType->partialSpecializations,
                        [&](const WeakRef<StructType>& candidate)
                        {
                            auto partial = candidate.Lock();
                            return partial && getGenericSpecializationKey(partial->genericArguments) == specializationKey;
                        });
                    if ((!isPartialSpecialization && genericPrimaryType->explicitSpecializations.contains(specializationKey)) || duplicatePartial)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Duplicate explicit specialization for '{}{}'.",
                            node.name->token.value,
                            formatConcreteInstantiationSignature(specializationArguments)
                        );
                    }
                    else
                    {
                        if (isPartialSpecialization)
                            genericPrimaryType->partialSpecializations.emplace_back(structType.AsFast<StructType>());
                        else
                            genericPrimaryType->explicitSpecializations.emplace(specializationKey, structType.AsFast<StructType>());
                    }
                }
            }
            else
            {
                currentScope_->define(node.name->token.value, objSym);
            }
            attributeListsBySymbol_[objSym.Get()] = &node.attributes;

            node.name->refType = structType;
            node.name->referencedSymbol = objSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& member : node.members) member.declaration->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();

        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            Ref<Type> generatedSelfType = structType;
            if (!structType->genericParameterNames.empty())
            {
                std::vector<Ref<Type>> genericSelfArguments;
                genericSelfArguments.reserve(structType->genericParameterNames.size());
                for (const auto& genericParameterName : structType->genericParameterNames)
                {
                    if (auto genericIt = genericScope.find(genericParameterName); genericIt != genericScope.end())
                        genericSelfArguments.push_back(genericIt->second);
                }

                if (genericSelfArguments.size() == structType->genericParameterNames.size())
                    generatedSelfType = instantiateGenericStructType(structType, genericSelfArguments);
            }

            bool hasCustomCtor = false;
            bool hasEmptyCtor = false;
            bool hasCopyCtor = false;
            bool hasMemberCtor = false;

            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
            auto baseArgs = getAttributeTypeArgs(node.attributes, Attribute::From);

            AccessModifier defaultAccess = getDefaultAccessModifier(node.attributes, AccessModifier::Private);
            structType->trustedTypeKeys.clear();

            for (const auto& trustArg : getAttributeTypeArgs(node.attributes, Attribute::Trust))
            {
                auto trustedStruct = resolveTrustedStructType(*this, prevScope, trustArg, node.location());
                if (!trustedStruct)
                    continue;

                const std::string trustedKey = getStructIdentityKey(trustedStruct);
                if (!trustedKey.empty() &&
                    std::ranges::find(structType->trustedTypeKeys, trustedKey) == structType->trustedTypeKeys.end())
                {
                    structType->trustedTypeKeys.push_back(trustedKey);
                }
            }

            auto resolveBaseType = [&](const AttributeTypeArgument& baseArg) -> Ref<StructType>
            {
                Ref<Type> resolvedType = nullptr;

                if (baseArg.typeSpecifier)
                {
                    baseArg.typeSpecifier->accept(*this);
                    resolvedType = baseArg.typeSpecifier->refType.Lock();
                }
                else if (auto baseSym = resolveAttributeSymbol(prevScope, baseArg.token))
                {
                    resolvedType = baseSym->type;
                }

                while (resolvedType && resolvedType->kind() == TypeKind::Alias)
                    resolvedType = resolvedType.AsFast<AliasType>()->aliasedType;

                if (!resolvedType || resolvedType->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from '{}'.", node.name->token.value, baseArg.token.value);
                    return nullptr;
                }

                return resolvedType.AsFast<StructType>();
            };

            structType->baseTypes.clear();

            std::vector<Ref<StructType>> resolvedBaseTypes;
            resolvedBaseTypes.reserve(baseArgs.size());
            int structBaseCount = 0;
            for (const auto& baseArg : baseArgs)
            {
                if (auto baseType = resolveBaseType(baseArg))
                {
                    if (baseType->isInterface)
                    {
                        structType->baseTypes.emplace_back(baseType);
                        resolvedBaseTypes.push_back(baseType);
                        continue;
                    }

                    if (!baseType->isObject)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Object '{}' cannot inherit from component '{}'. Objects may inherit from one object and any number of interfaces.",
                            node.name->token.value,
                            baseArg.token.value
                        );
                        continue;
                    }

                    if (baseType->isFinal)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Object '{}' cannot inherit from final object '{}'.",
                            node.name->token.value,
                            baseArg.token.value
                        );
                        continue;
                    }

                    structBaseCount++;
                    if (structBaseCount > 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from multiple objects/components. Single object inheritance only!", node.name->token.value);
                        continue;
                    }

                    structType->baseTypes.emplace_back(baseType);
                    resolvedBaseTypes.push_back(baseType);

                    bool hasDefaultCtor = false;

                    if (auto baseScope = baseType->structScope.Lock();
                        baseScope && baseScope->resolveLocally("OnConstruct"))
                    {
                        auto ctorSym = baseScope->resolveLocally("OnConstruct");
                        if (ctorSym->kind == SymbolKind::FunctionGroup)
                        {
                            for (auto& overload : ctorSym->overloads) {
                                if (overload->type && overload->type.AsFast<FunctionType>()->paramTypes.empty()) {
                                    hasDefaultCtor = true; break;
                                }
                            }
                        }
                        else if (ctorSym->type && ctorSym->type.AsFast<FunctionType>()->paramTypes.empty())
                        {
                            hasDefaultCtor = true;
                        }
                    }
                    else
                    {
                        hasDefaultCtor = true;
                    }
                    if (!hasDefaultCtor)
                    {
                        WIO_LOG_ADD_ERROR(node.location(),
                            "Base object '{}' lacks a default (parameterless) constructor. Derived object '{}' cannot be instantiated safely. Hint: Add '@GenerateCtors' or an explicit 'OnConstruct() {{}}' to the base object.",
                            baseArg.token.value, node.name->token.value);
                    }
                }
            }

            if (structBaseCount == 0)
                structType->baseTypes.push_back(Compiler::get().getTypeContext().getObject());

            structType->fieldNames.clear();
            structType->fieldTypes.clear();

            // PASS 1: Variables
            std::vector<Ref<Type>> memberTypes;
            for (auto& member : node.members)
            {
                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    auto memberSym = varDecl->name->referencedSymbol.Lock();

                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);

                    if (memberSym && memberSym->type)
                    {
                        memberTypes.push_back(memberSym->type);
                        structType->fieldNames.push_back(varDecl->name->token.value);
                        structType->fieldTypes.push_back(memberSym->type);
                    }

                    if (member.access == AccessModifier::None) member.access = defaultAccess;
                    if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            // PASS 2: Functions
            for (auto& member : node.members)
            {
                if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    auto memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;

                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                        size_t pCount = funcDecl->parameters.size();

                        if (pCount == 0) hasEmptyCtor = true;
                        else if (pCount == 1)
                        {
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                if (fType->paramTypes[0]->kind() == TypeKind::Reference &&
                                    fType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType) {
                                    hasCopyCtor = true;
                                }
                            }
                        }

                        if (pCount == memberTypes.size() && !(pCount == 1 && hasCopyCtor))
                        {
                            bool typesMatch = true;
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                for (size_t i = 0; i < pCount; ++i) {
                                    if (!fType->paramTypes[i]->isCompatibleWith(memberTypes[i])) {
                                        typesMatch = false; break;
                                    }
                                }
                            }
                            if (typesMatch) hasMemberCtor = true;
                        }
                    }
                    else if (funcName != "OnDestruct")
                    {
                        bool isOverride = false;
                        for (const auto& baseType : resolvedBaseTypes)
                        {
                            if (baseType)
                            {
                                if (auto lockedScope = baseType->structScope.Lock(); lockedScope)
                                {
                                    auto baseMember = lockedScope->resolveLocally(funcName);
                                    if (baseMember)
                                    {
                                        std::vector<Ref<Symbol>> candidates;
                                        if (baseMember->kind == SymbolKind::FunctionGroup)
                                            candidates = baseMember->overloads;
                                        else if (baseMember->kind == SymbolKind::Function)
                                            candidates.push_back(baseMember);

                                        auto genericOwner = baseType->genericPrimaryType.Lock();
                                        const auto& parameterNames = genericOwner
                                            ? genericOwner->genericParameterNames
                                            : baseType->genericParameterNames;
                                        const bool hasParameterPack = genericOwner
                                            ? genericOwner->hasGenericParameterPack
                                            : baseType->hasGenericParameterPack;
                                        const auto bindings = buildExtendedGenericBindings(
                                            parameterNames,
                                            hasParameterPack,
                                            baseType->genericArguments
                                        );
                                        auto memberFunctionType = memberSym && memberSym->type
                                            ? memberSym->type.AsFast<FunctionType>()
                                            : nullptr;

                                        for (const auto& candidate : candidates)
                                        {
                                            auto candidateType = candidate && candidate->type
                                                ? candidate->type.AsFast<FunctionType>()
                                                : nullptr;
                                            auto instantiatedCandidate = candidateType
                                                ? instantiateGenericType(candidateType, bindings).AsFast<FunctionType>()
                                                : nullptr;
                                            if (!memberFunctionType || !instantiatedCandidate ||
                                                memberFunctionType->paramTypes.size() != instantiatedCandidate->paramTypes.size())
                                                continue;

                                            bool signatureMatches = isExactType(
                                                memberFunctionType->returnType,
                                                instantiatedCandidate->returnType
                                            );
                                            for (size_t parameterIndex = 0;
                                                 signatureMatches && parameterIndex < memberFunctionType->paramTypes.size();
                                                 ++parameterIndex)
                                            {
                                                signatureMatches = isExactType(
                                                    memberFunctionType->paramTypes[parameterIndex],
                                                    instantiatedCandidate->paramTypes[parameterIndex]
                                                );
                                            }
                                            if (!signatureMatches)
                                                continue;

                                            isOverride = true;
                                            memberSym->overriddenSymbols.emplace_back(candidate);
                                        }
                                    }
                                }
                            }
                        }
                        if (isOverride)
                        {
                            memberSym->flags.set_isOverride(true);
                        }
                    }

                    if (memberSym)
                    {
                        if (member.access == AccessModifier::None) member.access = defaultAccess;
                        if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                        else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                        else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                    }
                }
            }

            // PASS 3: Generate Constructors
            if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                if (!hasEmptyCtor) {
                    auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
                    Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", defaultSym);
                }

                if (!hasMemberCtor && !memberTypes.empty()) {
                    auto memberCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                if (!hasCopyCtor) {
                    auto copyParamType = Compiler::get().getTypeContext().getOrCreateReferenceType(generatedSelfType, false);
                    auto copyCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, { copyParamType });
                    Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", copySym);
                }
            }

            if (hasAttribute(node.attributes, Attribute::Export))
            {
                if (!node.genericParameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@Export is not yet supported for generic objects.");
                }

                bool hasHostCallableConstructor = false;

                for (const auto& member : node.members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor =
                        functionType->paramTypes.size() == 1 &&
                        functionType->paramTypes[0]->kind() == TypeKind::Reference &&
                        functionType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType;
                    if (isCopyCtor)
                        continue;

                    if (member.access != AccessModifier::Public)
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' constructor must be public to be host-callable.",
                            node.name->token.value
                        );
                    }

                    bool allParametersAbiSafe = true;
                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                            continue;

                        allParametersAbiSafe = false;
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' constructor parameter {} uses non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            parameterIndex,
                            functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                        );
                    }

                    if (member.access == AccessModifier::Public && allParametersAbiSafe)
                        hasHostCallableConstructor = true;
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        hasHostCallableConstructor = true;

                    if (!hasMemberCtor && !memberTypes.empty())
                    {
                        bool memberCtorAbiSafe = true;
                        for (const auto& memberType : memberTypes)
                        {
                            if (isCAbiSafeExportType(memberType))
                                continue;

                            memberCtorAbiSafe = false;
                            break;
                        }

                        if (memberCtorAbiSafe)
                            hasHostCallableConstructor = true;
                    }
                }

                if (!hasHostCallableConstructor)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export object '{}' must expose at least one public host-callable constructor with only C-ABI-safe parameters.",
                        node.name->token.value
                    );
                }

                for (const auto& member : node.members)
                {
                    if (member.access != AccessModifier::Public || !member.declaration)
                        continue;

                    if (member.declaration->is<VariableDeclaration>())
                    {
                        auto variableDecl = member.declaration->as<VariableDeclaration>();
                        auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                        Ref<Type> fieldType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock();
                        if (!isSdkExportableFieldType(fieldType))
                        {
                            WIO_LOG_ADD_ERROR(
                                variableDecl->location(),
                                "@Export object '{}' exposes public field '{}' with type '{}' that is not yet SDK-exportable.",
                                node.name->token.value,
                                variableDecl->name->token.value,
                                fieldType ? fieldType->toString() : "<unknown>"
                            );
                        }

                        continue;
                    }

                    if (!member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    const std::string& functionName = functionDecl->name->token.value;
                    if (functionName == "OnConstruct" || functionName == "OnDestruct")
                        continue;

                    if (!functionDecl->genericParameters.empty())
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' public method '{}' cannot be generic yet.",
                            node.name->token.value,
                            functionName
                        );
                        continue;
                    }

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (!isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                        {
                            WIO_LOG_ADD_ERROR(
                                functionDecl->location(),
                                "@Export object '{}' public method '{}' parameter {} uses non-C-ABI-safe type '{}'.",
                                node.name->token.value,
                                functionName,
                                parameterIndex,
                                functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                            );
                        }
                    }

                    if (!isCAbiSafeExportType(functionType->returnType))
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' public method '{}' returns non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            functionName,
                            functionType->returnType ? functionType->returnType->toString() : "<unknown>"
                        );
                    }
                }
            }

            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        currentBaseStructType_ = nullptr;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(sym);

        for (const auto& baseType : structType->baseTypes)
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

        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
                validateAttributeApplications(member.declaration->as<VariableDeclaration>()->attributes, "field");
            else if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);
        }

        genericTypeParameterScopes_.pop_back();
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentStructType_ = nullptr;
        currentBaseStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(FlagDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "flag");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "flag");
        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);

            Ref<Type> flagType = Ref<StructType>::Create(node.name->token.value, structScope);
            flagType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            Ref<Symbol> flagSym = createSymbol(node.name->token.value, flagType, SymbolKind::Struct, node.location());

            flagSym->innerScope = structScope;
            flagSym->flags.set_isFlag(true);

            currentScope_->define(node.name->token.value, flagSym);
            node.name->refType = flagType;
            node.name->referencedSymbol = flagSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;

            auto voidType = Compiler::get().getTypeContext().getVoid();
            auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
            Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
            currentScope_->define("OnConstruct", defaultSym);

            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(EnumDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "enum");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "enum");
        for (auto& member : node.members)
        {
            applyActiveScopedAttributes(member.attributes, "enum_case");
            if (!isDeclarationPass_ && !isStructResolutionPass_)
                validateAttributeApplications(member.attributes, "enum_case");
        }
        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

            auto enumScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(enumScope);

            Ref<Type> enumType = Ref<StructType>::Create(node.name->token.value, enumScope);
            enumType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            enumType.AsFast<StructType>()->isEnum = true;
            const bool isNativeEnum = hasAttribute(node.attributes, Attribute::Native);
            enumType.AsFast<StructType>()->isNativePodComponent = isNativeEnum;
            enumType.AsFast<StructType>()->nativeCppName = node.name ? node.name->token.value : "";
            enumType.AsFast<StructType>()->nativeCppHeader.clear();

            if (isNativeEnum)
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAllAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native enum expects exactly one string literal argument.");
                    }
                    else
                    {
                        enumType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAllAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum must be a valid C++ identifier path like foo::bar.");
                        }
                        else
                        {
                            enumType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on enums currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on enums currently requires declaration-level @Native.");
            }

            Ref<Symbol> enumSym = createSymbol(node.name->token.value, enumType, SymbolKind::Struct, node.location());

            enumSym->innerScope = enumScope;
            enumSym->flags.set_isEnum(true);

            currentScope_->define(node.name->token.value, enumSym);
            node.name->refType = enumType;
            node.name->referencedSymbol = enumSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;

            auto targetType = Compiler::get().getTypeContext().getI32();

            if (auto typeArgs = getAllAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for enum.");
                }
            }

            if (sym && sym->type && sym->type->kind() == TypeKind::Struct)
                sym->type.AsFast<StructType>()->enumUnderlyingType = targetType;

            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);

                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;

                if (member.value)
                    member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(FlagsetDeclaration& node)
    {
        applyActiveScopedAttributes(node.attributes, "flagset");
        if (!isDeclarationPass_ && !isStructResolutionPass_)
            validateAttributeApplications(node.attributes, "flagset");
        for (auto& member : node.members)
        {
            applyActiveScopedAttributes(member.attributes, "enum_case");
            if (!isDeclarationPass_ && !isStructResolutionPass_)
                validateAttributeApplications(member.attributes, "enum_case");
        }
        if (isDeclarationPass_) {
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

            auto flagsetScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(flagsetScope);

            Ref<Type> flagsetType = Ref<StructType>::Create(node.name->token.value, flagsetScope);
            flagsetType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            flagsetType.AsFast<StructType>()->isFlagset = true;
            const bool isNativeFlagset = hasAttribute(node.attributes, Attribute::Native);
            flagsetType.AsFast<StructType>()->isNativePodComponent = isNativeFlagset;
            flagsetType.AsFast<StructType>()->nativeCppName = node.name ? node.name->token.value : "";
            flagsetType.AsFast<StructType>()->nativeCppHeader.clear();

            if (isNativeFlagset)
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAllAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native flagset expects exactly one string literal argument.");
                    }
                    else
                    {
                        flagsetType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAllAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset must be a valid C++ identifier path like foo::bar.");
                        }
                        else
                        {
                            flagsetType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on flagsets currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on flagsets currently requires declaration-level @Native.");
            }
            Ref<Symbol> flagsetSym = createSymbol(node.name->token.value, flagsetType, SymbolKind::Struct, node.location());

            flagsetSym->innerScope = flagsetScope;
            flagsetSym->flags.set_isFlagset(true);

            currentScope_->define(node.name->token.value, flagsetSym);
            node.name->refType = flagsetType;
            node.name->referencedSymbol = flagsetSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;

            auto targetType = Compiler::get().getTypeContext().getU32();

            if (auto typeArgs = getAllAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for flagset.");
                }
            }

            if (sym && sym->type && sym->type->kind() == TypeKind::Struct)
                sym->type.AsFast<StructType>()->enumUnderlyingType = targetType;

            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);

                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;

                if (member.value) member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }
