// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(Program& node)
    {
        enterScope(ScopeKind::Global);

        struct ExecutableEntrySurface
        {
            size_t applicationCount = 0;
            size_t ordinaryEntryCount = 0;
            common::Location firstApplicationLocation = common::Location::invalid();
            common::Location secondApplicationLocation = common::Location::invalid();
            common::Location ordinaryEntryLocation = common::Location::invalid();
            common::Location nestedApplicationLocation = common::Location::invalid();
            std::string firstApplicationName;
        } entrySurface;
        auto collectEntrySurface = [&](auto&& self, const NodePtr<Statement>& statement, bool insideRealm) -> void
        {
            if (!statement) return;
            if (auto group = statement.As<DeclarationGroup>())
            {
                for (const auto& declaration : group->declarations)
                    self(self, declaration, insideRealm);
                return;
            }
            if (auto usingAttribute = statement.As<UsingAttributeStatement>())
            {
                if (usingAttribute->body)
                    for (const auto& declaration : usingAttribute->body->declarations)
                        self(self, declaration, insideRealm);
                return;
            }
            if (auto realm = statement.As<RealmDeclaration>())
            {
                for (const auto& declaration : realm->statements)
                    self(self, declaration, true);
                return;
            }
            auto function = statement.As<FunctionDeclaration>();
            if (!function || !function->name || function->name->token.value != "Entry") return;
            if (function->isApplicationEntry)
            {
                ++entrySurface.applicationCount;
                if (entrySurface.applicationCount == 1)
                {
                    entrySurface.firstApplicationLocation = function->location();
                    entrySurface.firstApplicationName = function->applicationName;
                }
                else if (entrySurface.applicationCount == 2)
                    entrySurface.secondApplicationLocation = function->location();
                if (insideRealm && !entrySurface.nestedApplicationLocation.isValid())
                    entrySurface.nestedApplicationLocation = function->location();
            }
            else
            {
                ++entrySurface.ordinaryEntryCount;
                if (!entrySurface.ordinaryEntryLocation.isValid())
                    entrySurface.ordinaryEntryLocation = function->location();
            }
        };
        for (const auto& statement : node.statements)
            collectEntrySurface(collectEntrySurface, statement, false);

        if (entrySurface.applicationCount > 1)
        {
            WIO_LOG_ADD_ERROR(entrySurface.secondApplicationLocation,
                "An executable may declare only one application root.");
        }
        if (entrySurface.applicationCount > 0 && entrySurface.ordinaryEntryCount > 0)
        {
            WIO_LOG_ADD_ERROR(entrySurface.ordinaryEntryLocation,
                "An executable cannot define both application '{}' and an ordinary Entry function.",
                entrySurface.firstApplicationName);
        }
        if (entrySurface.nestedApplicationLocation.isValid())
        {
            WIO_LOG_ADD_ERROR(entrySurface.nestedApplicationLocation,
                "An application root must be declared at module top level, not inside a realm.");
        }
        if (entrySurface.applicationCount > 0 && Compiler::get().getBuildTarget() == BuildTarget::StaticLibrary)
        {
            WIO_LOG_ADD_ERROR(entrySurface.firstApplicationLocation,
                "Application roots require an executable target or a shared-library target with the host application ABI.");
        }

        isDeclarationPass_ = true;
        activeScopedAttributes_.clear();
        for (auto& stmt : node.statements)
            stmt->accept(*this);

        isDeclarationPass_ = false;
        isStructResolutionPass_ = true;
        activeScopedAttributes_.clear();
        for (auto& stmt : node.statements)
        {
            if (stmt->is<ComponentDeclaration>() ||
                stmt->is<ExtensionDeclaration>() ||
                stmt->is<ObjectDeclaration>() ||
                stmt->is<EnumDeclaration>() ||
                stmt->is<FlagsetDeclaration>() ||
                stmt->is<FlagDeclaration>() ||
                stmt->is<DeclarationGroup>() ||
                stmt->is<UsingAttributeStatement>() ||
                stmt->is<RealmDeclaration>())
            {
                stmt->accept(*this);
            }
        }

        isStructResolutionPass_ = false;

        // Freeze processor contracts after all type/member signatures exist,
        // then register checked derive members before ordinary bodies resolve
        // member access. Both passes are declaration-order independent.
        isAttributeContractPass_ = true;
        for (auto& stmt : node.statements)
        {
            if (stmt->is<AttributeDeclaration>() ||
                stmt->is<DeclarationGroup>() ||
                stmt->is<RealmDeclaration>())
            {
                stmt->accept(*this);
            }
        }
        isAttributeContractPass_ = false;

        isDeriveExpansionPass_ = true;
        for (auto& stmt : node.statements)
        {
            if (stmt->is<ComponentDeclaration>() ||
                stmt->is<ObjectDeclaration>() ||
                stmt->is<DeclarationGroup>() ||
                stmt->is<RealmDeclaration>())
            {
                stmt->accept(*this);
            }
        }
        isDeriveExpansionPass_ = false;

        activeScopedAttributes_.clear();
        for (auto& stmt : node.statements)
            stmt->accept(*this);

        auto entrySym = currentScope_->resolveLocally("Entry");
        if (Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!entrySym || (entrySym->kind != SymbolKind::Function && entrySym->kind != SymbolKind::FunctionGroup)))
        {
            WIO_LOG_ADD_ERROR(common::Location::invalid(), "No 'Entry' function found! An executable Wio program must define an 'Entry' function.");
        }
        exitScope();
    }

    void SemanticAnalyzer::visit(BlockStatement& node)
    {
        if (isDeclarationPass_) return;
        enterScope(ScopeKind::Block);
        const size_t scopedAttributeCount = activeScopedAttributes_.size();

        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }

        activeScopedAttributes_.resize(scopedAttributeCount);
        exitScope();
    }

    void SemanticAnalyzer::visit(TypeSpecifier& node)
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

        auto applyNullableSuffix = [&](Ref<Type> type) -> Ref<Type>
        {
            if (!node.isNullable)
                return type;

            if (!isNullableCapableType(type))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Type '{}' cannot be nullable. Only object/interface handles, ref/view, function, and opaque types support '?'.",
                    type ? type->toString() : "<unknown>"
                );
                return Compiler::get().getTypeContext().getUnknown();
            }

            return Compiler::get().getTypeContext().getOrCreateNullableType(std::move(type));
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
                    node.name.value,
                    formatConcreteInstantiationSignature(explicitTypeArguments)
                );
                return false;
            }

            return true;
        };

        if (node.name.type == TokenType::integerLiteral)
        {
            IntegerResult parsed = common::getInteger(node.name.value);
            Ref<Type> valueType = Type::getTypeFromIntegerResult(parsed);
            node.refType = Compiler::get().getTypeContext().getOrCreateConstValueType(node.name.value, valueType);
            return;
        }

        if (node.name.type == TokenType::stringLiteral)
        {
            Ref<Type> valueType = node.name.isUnicodeString
                ? Compiler::get().getTypeContext().getText()
                : Compiler::get().getTypeContext().getString();
            node.refType = Compiler::get().getTypeContext().getOrCreateConstValueType(
                node.name.value,
                valueType);
            return;
        }

        if (node.name.type == TokenType::kwFn)
        {
            node.generics[0]->accept(*this);
            auto retType = node.generics[0]->refType.Lock();

            std::vector<Ref<Type>> paramTypes;
            bool hasParameterPack = false;
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                Ref<Type> parameterType = node.generics[i]->refType.Lock();
                if (containsGenericParameterPackType(parameterType))
                {
                    if (i + 1 != node.generics.size())
                    {
                        WIO_LOG_ADD_ERROR(node.generics[i]->location(), "Function type parameter packs must be trailing.");
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return;
                    }
                    hasParameterPack = true;
                }
                paramTypes.push_back(parameterType);
            }

            if (containsGenericParameterPackType(retType))
            {
                WIO_LOG_ADD_ERROR(node.generics[0]->location(), "Function type return positions cannot use generic parameter packs.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = applyNullableSuffix(
                Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes, hasParameterPack)
            );
            return;
        }

        if (node.name.type == TokenType::kwCoroutine)
        {
            if (node.generics.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "'coroutine' requires exactly one result type argument.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.generics[0]->accept(*this);
            node.refType = applyNullableSuffix(
                Compiler::get().getTypeContext().getOrCreateAsyncTaskType(
                    node.generics[0]->refType.Lock()
                )
            );
            return;
        }

        if (node.name.value == "Dict" || node.name.value == "Tree")
        {
            bool isOrdered = (node.name.value == "Tree");

            if (node.generics.size() != 2)
            {
                WIO_LOG_ADD_ERROR(node.location(), "'{}' requires exactly 2 generic arguments (Key, Value).", node.name.value);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.generics[0]->accept(*this);
            node.generics[1]->accept(*this);

            node.refType = applyNullableSuffix(Compiler::get().getTypeContext().getOrCreateDictionaryType(
                node.generics[0]->refType.Lock(),
                node.generics[1]->refType.Lock(),
                isOrdered
            ));
            return;
        }

        Ref<Type> type = resolvePrimitiveType(node.name.value);

        if (!type &&
            node.name.type == TokenType::identifier &&
            node.name.value.find("::") == std::string::npos)
        {
            for (auto& genericTypeParameterScope : std::ranges::reverse_view(genericTypeParameterScopes_))
            {
                if (auto genericTypeIt = genericTypeParameterScope.find(node.name.value); genericTypeIt != genericTypeParameterScope.end())
                {
                    type = genericTypeIt->second;
                    break;
                }
            }
        }

        if (!type)
        {
            if (node.name.type == TokenType::StaticArray)
            {
                node.generics[0]->accept(*this);
                Ref<Type> extentType = nullptr;
                if (node.arrayExtent)
                {
                    node.arrayExtent->accept(*this);
                    extentType = node.arrayExtent->refType.Lock();
                    Ref<Type> extentValueType = extentType && extentType->kind() == TypeKind::ConstGenericParameter
                        ? extentType.AsFast<ConstGenericParameterType>()->valueType
                        : extentType && extentType->kind() == TypeKind::ConstValue
                            ? extentType.AsFast<ConstValueType>()->valueType
                            : nullptr;
                    if (!extentType ||
                        (extentType->kind() != TypeKind::ConstGenericParameter && extentType->kind() != TypeKind::ConstValue) ||
                        !isConstGenericIntegerType(extentValueType))
                    {
                        WIO_LOG_ADD_ERROR(node.arrayExtent->location(), "Static array extent must be an integer const generic parameter or integer literal.");
                        extentType = Compiler::get().getTypeContext().getUnknown();
                    }
                }
                if (node.hasInferredArrayExtent && !allowInferredStaticArrayExtent_)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Inferred static array extent '[T; _]' is supported only on variable declarations with an initializer."
                    );
                }
                type = Compiler::get().getTypeContext().getOrCreateArrayType(
                    node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Static,
                    node.size, extentType, node.hasInferredArrayExtent);
            }
            else if (node.name.type == TokenType::DynamicArray)
            {
                node.generics[0]->accept(*this);
                type = Compiler::get().getTypeContext().getOrCreateArrayType(node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Dynamic);
            }
            else if (node.name.type == TokenType::kwRef || node.name.type == TokenType::kwView)
            {
                node.generics[0]->accept(*this);
                bool isMut = (node.name.type == TokenType::kwRef);
                type = Compiler::get().getTypeContext().getOrCreateReferenceType(node.generics[0]->refType.Lock(), isMut);
            }
            else if (node.name.type == TokenType::identifier)
            {
                if (auto sym = resolveQualifiedSymbol(currentScope_, node.name.value))
                {
                    if (sym->kind == SymbolKind::Variable && sym->flags.get_isConst())
                    {
                        auto declarationIt = variableDeclarationsBySymbol_.find(sym.Get());
                        if (declarationIt == variableDeclarationsBySymbol_.end() ||
                            !declarationIt->second || !declarationIt->second->initializer)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "Const generic argument '{}' has no evaluable initializer.", node.name.value);
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return;
                        }

                        Ref<Type> constType = unwrapAliasType(sym->type);
                        if (constType && constType->kind() == TypeKind::Primitive)
                        {
                            const std::string& constTypeName = constType.AsFast<PrimitiveType>()->name;
                            if (constTypeName == "string" || constTypeName == "text")
                            {
                                std::unordered_set<const Symbol*> activeSymbols{sym.Get()};
                                auto value = tryEvaluateStaticAttributeConstant(
                                    declarationIt->second->initializer,
                                    variableDeclarationsBySymbol_,
                                    activeSymbols);
                                if (!value || value->type != TokenType::stringLiteral ||
                                    value->isUnicodeString != (constTypeName == "text"))
                                {
                                    WIO_LOG_ADD_ERROR(
                                        node.location(),
                                        "Const generic argument '{}' must evaluate to '{}'.",
                                        node.name.value,
                                        constTypeName);
                                    node.refType = Compiler::get().getTypeContext().getUnknown();
                                    return;
                                }

                                node.refType = Compiler::get().getTypeContext().getOrCreateConstValueType(
                                    value->value,
                                    sym->type);
                                return;
                            }
                        }

                        auto value = ConstExpressionEvaluator(variableDeclarationsBySymbol_)
                            .evaluateInteger(declarationIt->second->initializer);
                        if (!value || *value < 0 || !isConstGenericIntegerType(sym->type))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "Const generic argument '{}' must evaluate to a non-negative integer.", node.name.value);
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return;
                        }

                        node.refType = Compiler::get().getTypeContext().getOrCreateConstValueType(
                            std::to_string(*value), sym->type);
                        return;
                    }

                    std::vector<Ref<Type>> explicitTypeArguments;
                    explicitTypeArguments.reserve(node.generics.size());
                    for (auto& genericArgument : node.generics)
                    {
                        genericArgument->accept(*this);
                        explicitTypeArguments.push_back(genericArgument->refType.Lock());
                    }

                    if (sym->kind == SymbolKind::Struct)
                    {
                        auto structType = sym->type.AsFast<StructType>();
                        if (!structType->genericParameterNames.empty())
                        {
                            if (!validateGenericArgumentKinds(
                                    structType->genericParameterNames,
                                    structType->genericParameterTypes,
                                    explicitTypeArguments,
                                    node.location()))
                            {
                                type = Compiler::get().getTypeContext().getUnknown();
                                node.refType = type;
                                return;
                            }
                            const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                                structType->genericParameterNames,
                                structType->hasGenericParameterPack
                            );
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
                                    node.name.value,
                                    structType->hasGenericParameterPack
                                        ? common::formatString("at least {}", requiredArgumentCount)
                                        : requiredArgumentCount == fixedArgumentCount
                                            ? std::to_string(fixedArgumentCount)
                                            : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                explicitTypeArguments = std::move(*completedArguments);
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

                                type = instantiateGenericStructType(structType, explicitTypeArguments, node.location());
                            }
                        }
                        else
                        {
                            if (!explicitTypeArguments.empty())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type '{}' does not accept generic arguments.",
                                    node.name.value
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                type = sym->type;
                            }
                        }
                    }
                    else if (sym->kind == SymbolKind::TypeAlias)
                    {
                        if (!sym->genericParameterNames.empty())
                        {
                            if (!validateGenericArgumentKinds(
                                    sym->genericParameterNames,
                                    sym->genericParameterTypes,
                                    explicitTypeArguments,
                                    node.location()))
                            {
                                type = Compiler::get().getTypeContext().getUnknown();
                                node.refType = type;
                                return;
                            }
                            const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                                sym->genericParameterNames,
                                sym->hasGenericParameterPack
                            );
                            const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                                sym->genericParameterDefaults, fixedArgumentCount);
                            auto completedArguments = completeGenericTypeArguments(
                                sym->genericParameterNames,
                                sym->genericParameterDefaults,
                                sym->hasGenericParameterPack,
                                explicitTypeArguments);
                            if (!completedArguments)
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type alias '{}' expects {} generic arguments, but got {}.",
                                    node.name.value,
                                    sym->hasGenericParameterPack
                                        ? common::formatString("at least {}", requiredArgumentCount)
                                        : requiredArgumentCount == fixedArgumentCount
                                            ? std::to_string(fixedArgumentCount)
                                            : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                explicitTypeArguments = std::move(*completedArguments);
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type alias"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

                                const auto bindings = buildExtendedGenericBindings(
                                    sym->genericParameterNames,
                                    sym->hasGenericParameterPack,
                                    explicitTypeArguments
                                );

                                Ref<Type> instantiatedAliasTarget = instantiateGenericType(sym->aliasTargetType, bindings);
                                type = Compiler::get().getTypeContext().getOrCreateAliasType(
                                    formatAppliedTypeName(node.name.value, explicitTypeArguments),
                                    instantiatedAliasTarget
                                );
                            }
                        }
                        else
                        {
                            if (!explicitTypeArguments.empty())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type alias '{}' does not accept generic arguments.",
                                    node.name.value
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                type = sym->type;
                            }
                        }
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "'{}' is not a type.", node.name.value);
                    }
                }
            }
        }

        if (!type)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Unknown type: '{}'", node.name.value);
            type = Compiler::get().getTypeContext().getUnknown();
        }

        if (node.packIndex)
        {
            node.packIndex->accept(*this);
            const auto expectedPackName =
                (type && type->kind() == TypeKind::GenericParameterPack) ? std::optional<std::string_view>(type.AsFast<GenericParameterPackType>()->name) :
                (type && type->kind() == TypeKind::TypePackView) ? std::optional<std::string_view>(type.AsFast<TypePackViewType>()->packName) :
                std::nullopt;
            auto indexBinding = tryEvaluatePackIndexBinding(node.packIndex, variableDeclarationsBySymbol_, expectedPackName);
            if (!indexBinding.has_value())
            {
                WIO_LOG_ADD_ERROR(node.packIndex->location(), "Pack type indexing requires a non-negative compile-time integer index.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
            else if (indexBinding->kind == PackElementBindingKind::FromEnd && indexBinding->value == 0)
            {
                WIO_LOG_ADD_ERROR(node.packIndex->location(), "Pack type indexing from '.size' must subtract at least 1.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
            else if (type && type->kind() == TypeKind::GenericParameterPack)
            {
                ParsedPackElementBinding reboundBinding = *indexBinding;
                reboundBinding.packName = type.AsFast<GenericParameterPackType>()->name;
                type = makeSyntheticPackElementType(reboundBinding);
            }
            else if (type && type->kind() == TypeKind::TypePackView)
            {
                auto packViewType = type.AsFast<TypePackViewType>();
                if (!packViewType->elementTypes.empty())
                {
                    if (auto resolvedIndex = tryResolveConcretePackElementIndex(*indexBinding, packViewType->elementTypes.size()))
                    {
                        type = packViewType->elementTypes[*resolvedIndex];
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            node.packIndex->location(),
                            "Pack type index is out of range for size {}.",
                            packViewType->elementTypes.size()
                        );
                        type = Compiler::get().getTypeContext().getUnknown();
                    }
                }
                else
                {
                    ParsedPackElementBinding reboundBinding = *indexBinding;
                    reboundBinding.packName = packViewType->packName;
                    type = makeSyntheticPackElementType(reboundBinding);
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Only generic type packs can be indexed in type position.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
        }

        if (node.isPackExpansion)
        {
            if (!type || type->kind() != TypeKind::GenericParameterPack)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Only generic parameter packs can use '...' in type position.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
        }
        else if (type && type->kind() == TypeKind::GenericParameterPack)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Generic parameter packs must be written with '...' in type position.");
            type = Compiler::get().getTypeContext().getUnknown();
        }

        node.refType = applyNullableSuffix(type);
    }
