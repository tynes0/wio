// Member implementation group for the owning compiler subsystem.
// Included inside its existing wio namespace to keep one definition surface.

    void SemanticAnalyzer::visit(ExpressionStatement& node)
    {
        if (isDeclarationPass_) return;
        node.expression->accept(*this);
    }

    void SemanticAnalyzer::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }

    void SemanticAnalyzer::visit(DeclarationGroup& node)
    {
        for (auto& declaration : node.declarations)
        {
            if (!declaration)
                continue;
            if (isAttributeContractPass_ &&
                !declaration->is<AttributeDeclaration>() &&
                !declaration->is<DeclarationGroup>() &&
                !declaration->is<RealmDeclaration>())
            {
                continue;
            }
            if (isDeriveExpansionPass_ &&
                !declaration->is<ComponentDeclaration>() &&
                !declaration->is<ObjectDeclaration>() &&
                !declaration->is<DeclarationGroup>() &&
                !declaration->is<RealmDeclaration>())
            {
                continue;
            }
            declaration->accept(*this);
        }
    }

    void SemanticAnalyzer::validateAttributeApplications(
        std::vector<NodePtr<AttributeStatement>>& attributes,
        std::string_view target,
        bool validateTarget)
    {
        std::unordered_map<const Symbol*, size_t> applicationCounts;
        std::unordered_map<Attribute, size_t> builtinApplicationCounts;
        std::unordered_map<std::string, const Symbol*> conflictOwners;
        std::unordered_map<const AttributeStatement*, std::vector<const Symbol*>> compositionChains;

        // Bind every application to a stable identity before expanding user
        // composition. Built-ins and user declarations deliberately share the
        // validation passes below; the enum is no longer their identity.
        for (const auto& application : attributes)
        {
            if (!application)
                continue;

            if (application->attribute != Attribute::Unknown)
            {
                const auto* contract = getBuiltinAttributeContract(application->attribute);
                if (!contract)
                    continue;
                application->canonicalName = std::string(contract->canonicalName);

                if (validateTarget &&
                    std::ranges::find(contract->targets, std::string_view(target)) == contract->targets.end())
                {
                    std::string allowedTargets;
                    for (const std::string_view allowed : contract->targets)
                    {
                        if (!allowedTargets.empty()) allowedTargets += " | ";
                        allowedTargets += allowed;
                    }
                    WIO_LOG_ADD_ERROR(
                        application->location(),
                        "Attribute '{}' cannot target '{}'. Allowed targets: {}.",
                        contract->canonicalName,
                        target,
                        allowedTargets);
                }

                const size_t count = ++builtinApplicationCounts[application->attribute];
                if (count > 1 && !contract->repeatable)
                {
                    WIO_LOG_ADD_ERROR(
                        application->location(),
                        "Attribute '{}' is not repeatable on the same declaration.",
                        contract->canonicalName);
                }
                continue;
            }

            Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, application->qualifiedName);
            if (symbol && symbol->kind == SymbolKind::Attribute)
                application->canonicalName = symbol->attributeCanonicalName;
        }

        for (size_t attributeIndex = 0; attributeIndex < attributes.size(); ++attributeIndex)
        {
            // Composition appends to the same vector. Keep an owning copy so
            // vector reallocation cannot invalidate the current application.
            const auto attribute = attributes[attributeIndex];
            if (!attribute || attribute->attribute != Attribute::Unknown)
                continue;

            Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, attribute->qualifiedName);
            if (!symbol || symbol->kind != SymbolKind::Attribute)
            {
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Unknown attribute '{}'. Declare it with 'attribute' or use a built-in attribute name.",
                    attribute->qualifiedName.empty() ? "<unnamed>" : attribute->qualifiedName
                );
                continue;
            }
            attribute->canonicalName = symbol->attributeCanonicalName;
            attribute->processorBindings.clear();
            for (size_t processorIndex = 0;
                 processorIndex < symbol->attributeProcessorPhases.size();
                 ++processorIndex)
            {
                attribute->processorBindings.push_back(AttributeStatement::ProcessorBinding{
                    .canonicalTypeName = processorIndex < symbol->attributeProcessorCanonicalTypes.size()
                        ? symbol->attributeProcessorCanonicalTypes[processorIndex]
                        : std::string{},
                    .cppTypeName = processorIndex < symbol->attributeProcessorCppTypes.size()
                        ? symbol->attributeProcessorCppTypes[processorIndex]
                        : std::string{},
                    .phase = symbol->attributeProcessorPhases[processorIndex],
                    .hookCppName = processorIndex < symbol->attributeProcessorHookCppNames.size()
                        ? symbol->attributeProcessorHookCppNames[processorIndex]
                        : std::string{},
                    .hookMode = processorIndex < symbol->attributeProcessorHookModes.size()
                        ? symbol->attributeProcessorHookModes[processorIndex]
                        : std::string{},
                    .hookValueType = processorIndex < symbol->attributeProcessorHookValueTypes.size()
                        ? symbol->attributeProcessorHookValueTypes[processorIndex]
                        : nullptr
                });
            }
            for (const auto& processor : attribute->processorBindings)
            {
                if ((processor.phase == "pre" || processor.phase == "post" ||
                     processor.phase == "finally" || processor.phase == "around") &&
                    target != "fn" && target != "method")
                {
                    WIO_LOG_ADD_ERROR(
                        attribute->location(),
                        "Behavioral attribute processor '{}' can target only functions or methods, not '{}'.",
                        processor.canonicalTypeName,
                        target);
                }
                if (processor.phase == "derive")
                {
                    if (target != "component" && target != "object")
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "DeriveProcessor '{}' can target only components or objects, not '{}'.",
                            processor.canonicalTypeName,
                            target);
                    }
                }
            }
            for (size_t processorIndex = 0;
                 processorIndex < symbol->attributeProcessorPhases.size();
                 ++processorIndex)
            {
                if (symbol->attributeProcessorPhases[processorIndex] != "validation" ||
                    processorIndex >= symbol->attributeProcessorTargetTypes.size())
                    continue;

                Ref<Type> validatorTarget = unwrapAliasType(symbol->attributeProcessorTargetTypes[processorIndex]);
                const bool targetIsAny = validatorTarget &&
                    validatorTarget->kind() == TypeKind::Primitive &&
                    validatorTarget.AsFast<PrimitiveType>()->name == "any";
                if (validatorTarget && !targetIsAny && target != "component" && target != "object")
                {
                    WIO_LOG_ADD_ERROR(
                        attribute->location(),
                        "Typed Validator<{}> can target only components or objects; use Validator<any> for '{}'.",
                        validatorTarget->toString(),
                        target);
                }
            }

            attribute->runtimeRetained = std::ranges::find(
                symbol->attributeRetention, std::string("runtime")) != symbol->attributeRetention.end();

            for (size_t processorIndex = 0;
                 processorIndex < symbol->attributeProcessorPhases.size();
                 ++processorIndex)
            {
                if (symbol->attributeProcessorPhases[processorIndex] != "validation" ||
                    processorIndex >= symbol->attributeProcessorValidationResults.size() ||
                    symbol->attributeProcessorValidationResults[processorIndex] != 0)
                {
                    continue;
                }

                const std::string processorName = processorIndex < symbol->attributeProcessorTypes.size()
                    ? symbol->attributeProcessorTypes[processorIndex]
                    : std::string("<validator>");
                const std::string diagnostic = processorIndex < symbol->attributeProcessorDiagnostics.size() &&
                                               !symbol->attributeProcessorDiagnostics[processorIndex].empty()
                    ? symbol->attributeProcessorDiagnostics[processorIndex]
                    : common::formatString(
                        "Attribute validator '{}' rejected application of '{}'.",
                        processorName,
                        attribute->canonicalName);
                WIO_LOG_ADD_ERROR(attribute->location(), "{}", diagnostic);
            }

            // Attribute applications are compile-time metadata. Resolve const
            // identifiers to their folded scalar value before named-argument
            // ordering and type validation so metadata never retains a source
            // identifier in place of its value.
            for (Token& argument : attribute->args)
            {
                if (argument.type != TokenType::identifier)
                    continue;

                Ref<Symbol> argumentSymbol = resolveQualifiedSymbol(currentScope_, argument.value);
                if (!argumentSymbol || !argumentSymbol->flags.get_isConst())
                    continue;

                auto declarationIt = variableDeclarationsBySymbol_.find(argumentSymbol.Get());
                if (declarationIt == variableDeclarationsBySymbol_.end() ||
                    !declarationIt->second || !declarationIt->second->initializer)
                {
                    continue;
                }

                std::unordered_set<const Symbol*> activeSymbols{argumentSymbol.Get()};
                if (auto folded = tryEvaluateStaticAttributeConstant(
                        declarationIt->second->initializer,
                        variableDeclarationsBySymbol_,
                        activeSymbols))
                {
                    folded->loc = argument.loc;
                    argument = std::move(*folded);
                }
            }

            const bool hasNamedArguments = std::ranges::any_of(
                attribute->argumentNames,
                [](const std::string& name) { return !name.empty(); });
            if (hasNamedArguments)
            {
                if (attribute->argumentNames.size() != attribute->args.size() ||
                    attribute->typeArgs.size() != attribute->args.size())
                {
                    WIO_LOG_ADD_ERROR(
                        attribute->location(),
                        "Attribute '{}' has inconsistent argument metadata.",
                        attribute->qualifiedName);
                    continue;
                }

                std::vector<std::optional<size_t>> assignedSources(symbol->attributeParameterNames.size());
                size_t nextPositionalIndex = 0;
                bool invalidNamedArguments = false;
                for (size_t sourceIndex = 0; sourceIndex < attribute->args.size(); ++sourceIndex)
                {
                    const std::string& argumentName = attribute->argumentNames[sourceIndex];
                    size_t parameterIndex = 0;
                    if (argumentName.empty())
                    {
                        while (nextPositionalIndex < assignedSources.size() && assignedSources[nextPositionalIndex].has_value())
                            ++nextPositionalIndex;
                        parameterIndex = nextPositionalIndex++;
                        if (parameterIndex >= assignedSources.size())
                        {
                            WIO_LOG_ADD_ERROR(
                                attribute->args[sourceIndex].loc,
                                "Attribute '{}' received too many positional arguments.",
                                attribute->qualifiedName);
                            invalidNamedArguments = true;
                            continue;
                        }
                    }
                    else
                    {
                        const auto parameter = std::ranges::find(symbol->attributeParameterNames, argumentName);
                        if (parameter == symbol->attributeParameterNames.end())
                        {
                            WIO_LOG_ADD_ERROR(
                                attribute->args[sourceIndex].loc,
                                "Attribute '{}' has no parameter named '{}'.",
                                attribute->qualifiedName,
                                argumentName);
                            invalidNamedArguments = true;
                            continue;
                        }
                        parameterIndex = static_cast<size_t>(std::distance(symbol->attributeParameterNames.begin(), parameter));
                    }

                    if (assignedSources[parameterIndex].has_value())
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->args[sourceIndex].loc,
                            "Attribute parameter '{}' is assigned more than once.",
                            symbol->attributeParameterNames[parameterIndex]);
                        invalidNamedArguments = true;
                        continue;
                    }
                    assignedSources[parameterIndex] = sourceIndex;
                }

                size_t requiredArgumentCount = symbol->attributeParameterTypes.size();
                while (requiredArgumentCount > 0 &&
                       symbol->attributeParameterHasDefault[requiredArgumentCount - 1])
                {
                    --requiredArgumentCount;
                }
                for (size_t parameterIndex = 0; parameterIndex < requiredArgumentCount; ++parameterIndex)
                {
                    if (!assignedSources[parameterIndex].has_value())
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Attribute '{}' is missing required argument '{}'.",
                            attribute->qualifiedName,
                            symbol->attributeParameterNames[parameterIndex]);
                        invalidNamedArguments = true;
                    }
                }

                if (invalidNamedArguments)
                    continue;

                std::vector<Token> normalizedArguments;
                std::vector<NodePtr<TypeSpecifier>> normalizedTypeArguments;
                std::vector<std::string> normalizedArgumentNames;
                std::vector<bool> normalizedUsedDefaults;
                const size_t normalizedCount = assignedSources.size();
                normalizedArguments.reserve(normalizedCount);
                normalizedTypeArguments.reserve(normalizedCount);
                normalizedArgumentNames.reserve(normalizedCount);
                normalizedUsedDefaults.reserve(normalizedCount);
                for (size_t parameterIndex = 0; parameterIndex < normalizedCount; ++parameterIndex)
                {
                    if (assignedSources[parameterIndex].has_value())
                    {
                        const size_t sourceIndex = assignedSources[parameterIndex].value();
                        normalizedArguments.push_back(std::move(attribute->args[sourceIndex]));
                        normalizedTypeArguments.push_back(std::move(attribute->typeArgs[sourceIndex]));
                        normalizedUsedDefaults.push_back(false);
                    }
                    else if (parameterIndex < symbol->attributeParameterDefaults.size() &&
                             symbol->attributeParameterDefaults[parameterIndex].isValid())
                    {
                        Token defaultValue = symbol->attributeParameterDefaults[parameterIndex];
                        defaultValue.loc = attribute->location();
                        normalizedArguments.push_back(std::move(defaultValue));
                        normalizedTypeArguments.emplace_back(nullptr);
                        normalizedUsedDefaults.push_back(true);
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Attribute '{}' could not materialize default argument '{}'.",
                            attribute->qualifiedName,
                            symbol->attributeParameterNames[parameterIndex]);
                        invalidNamedArguments = true;
                        continue;
                    }
                    normalizedArgumentNames.push_back(symbol->attributeParameterNames[parameterIndex]);
                }
                if (invalidNamedArguments)
                    continue;
                attribute->args = std::move(normalizedArguments);
                attribute->typeArgs = std::move(normalizedTypeArguments);
                attribute->argumentNames = std::move(normalizedArgumentNames);
                attribute->argumentUsedDefaults = std::move(normalizedUsedDefaults);
            }

            const bool targetAllowed = !validateTarget || std::ranges::find(
                symbol->attributeTargets, std::string(target)) != symbol->attributeTargets.end();
            if (validateTarget && !targetAllowed)
            {
                std::string allowedTargets;
                for (size_t index = 0; index < symbol->attributeTargets.size(); ++index)
                {
                    if (index != 0)
                        allowedTargets += " | ";
                    allowedTargets += symbol->attributeTargets[index];
                }
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Attribute '{}' cannot target '{}'. Allowed targets: {}.",
                    attribute->qualifiedName,
                    target,
                    allowedTargets
                );
            }

            const size_t applicationCount = ++applicationCounts[symbol.Get()];
            if (applicationCount > 1 && !symbol->attributeRepeatable)
            {
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Attribute '{}' is not repeatable on the same declaration.",
                    attribute->qualifiedName
                );
            }

            for (const auto& group : symbol->attributeConflictGroups)
            {
                auto [owner, inserted] = conflictOwners.emplace(group, symbol.Get());
                if (!inserted && owner->second != symbol.Get())
                {
                    WIO_LOG_ADD_ERROR(
                        attribute->location(),
                        "Attribute '{}' conflicts with attribute '{}' through conflict group '{}'.",
                        attribute->qualifiedName,
                        owner->second->name,
                        group
                    );
                }
            }

            size_t requiredArgumentCount = symbol->attributeParameterTypes.size();
            while (requiredArgumentCount > 0 &&
                   symbol->attributeParameterHasDefault[requiredArgumentCount - 1])
            {
                --requiredArgumentCount;
            }

            if (attribute->args.size() < requiredArgumentCount ||
                attribute->args.size() > symbol->attributeParameterTypes.size())
            {
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Attribute '{}' expects {} to {} arguments, but got {}.",
                    attribute->qualifiedName,
                    requiredArgumentCount,
                    symbol->attributeParameterTypes.size(),
                    attribute->args.size()
                );
                continue;
            }

            while (attribute->args.size() < symbol->attributeParameterTypes.size())
            {
                const size_t parameterIndex = attribute->args.size();
                if (parameterIndex >= symbol->attributeParameterDefaults.size() ||
                    !symbol->attributeParameterDefaults[parameterIndex].isValid())
                {
                    WIO_LOG_ADD_ERROR(
                        attribute->location(),
                        "Attribute '{}' could not materialize default argument '{}'.",
                        attribute->qualifiedName,
                        parameterIndex < symbol->attributeParameterNames.size()
                            ? symbol->attributeParameterNames[parameterIndex]
                            : std::to_string(parameterIndex));
                    break;
                }

                Token defaultValue = symbol->attributeParameterDefaults[parameterIndex];
                defaultValue.loc = attribute->location();
                attribute->args.push_back(std::move(defaultValue));
                attribute->typeArgs.emplace_back(nullptr);
                attribute->argumentNames.push_back(
                    parameterIndex < symbol->attributeParameterNames.size()
                        ? symbol->attributeParameterNames[parameterIndex]
                        : std::string{});
                attribute->argumentUsedDefaults.push_back(true);
            }

            if (attribute->argumentUsedDefaults.size() < attribute->args.size())
                attribute->argumentUsedDefaults.resize(attribute->args.size(), false);
            if (attribute->argumentNames.size() < attribute->args.size())
                attribute->argumentNames.resize(attribute->args.size());
            for (size_t parameterIndex = 0;
                 parameterIndex < attribute->argumentNames.size() &&
                 parameterIndex < symbol->attributeParameterNames.size();
                 ++parameterIndex)
            {
                if (attribute->argumentNames[parameterIndex].empty())
                    attribute->argumentNames[parameterIndex] = symbol->attributeParameterNames[parameterIndex];
            }

            for (size_t index = 0; index < attribute->args.size(); ++index)
            {
                Ref<Type> expectedType = unwrapAliasType(symbol->attributeParameterTypes[index]);
                if (!expectedType || expectedType->isUnknown())
                    continue;

                const Token& argument = attribute->args[index];
                bool compatible = true;
                if (expectedType->kind() == TypeKind::Primitive)
                {
                    const std::string& primitiveName = expectedType.AsFast<PrimitiveType>()->name;
                    if (primitiveName == "string")
                        compatible = argument.type == TokenType::stringLiteral && !argument.isUnicodeString;
                    else if (primitiveName == "text")
                        compatible = argument.type == TokenType::stringLiteral && argument.isUnicodeString;
                    else if (primitiveName == "bool")
                        compatible = argument.type == TokenType::kwTrue || argument.type == TokenType::kwFalse;
                    else if (primitiveName == "f32" || primitiveName == "f64")
                        compatible = argument.type == TokenType::floatLiteral || argument.type == TokenType::integerLiteral;
                    else if (primitiveName != "void")
                        compatible = argument.type == TokenType::integerLiteral || argument.type == TokenType::byteLiteral;
                }

                if (!compatible)
                {
                    WIO_LOG_ADD_ERROR(
                        argument.loc,
                        "Argument {} of attribute '{}' must be '{}'.",
                        index + 1,
                        attribute->qualifiedName,
                        expectedType->toString()
                    );
                }
            }

            if (!symbol->attributeComposition.empty())
            {
                std::vector<const Symbol*> chain;
                if (const auto chainIt = compositionChains.find(attribute.Get()); chainIt != compositionChains.end())
                    chain = chainIt->second;
                else
                    chain.push_back(symbol.Get());

                for (const auto& composedTemplate : symbol->attributeComposition)
                {
                    if (!composedTemplate)
                        continue;

                    Ref<Symbol> composedSymbol;
                    if (composedTemplate->attribute == Attribute::Unknown)
                        composedSymbol = resolveQualifiedSymbol(currentScope_, composedTemplate->qualifiedName);
                    if (composedSymbol && composedSymbol->kind == SymbolKind::Attribute &&
                        std::ranges::find(chain, composedSymbol.Get()) != chain.end())
                    {
                        WIO_LOG_ADD_ERROR(
                            attribute->location(),
                            "Attribute composition cycle detected while expanding '{}' through '{}'.",
                            attribute->qualifiedName,
                            composedTemplate->qualifiedName);
                        continue;
                    }

                    std::vector<Token> composedArguments = composedTemplate->args;
                    std::vector<NodePtr<TypeSpecifier>> composedTypeArguments = composedTemplate->typeArgs;
                    for (size_t argumentIndex = 0; argumentIndex < composedArguments.size(); ++argumentIndex)
                    {
                        if (composedArguments[argumentIndex].type != TokenType::identifier)
                            continue;
                        const auto parameter = std::ranges::find(
                            symbol->attributeParameterNames,
                            composedArguments[argumentIndex].value);
                        if (parameter == symbol->attributeParameterNames.end())
                            continue;
                        const size_t parameterIndex = static_cast<size_t>(
                            std::distance(symbol->attributeParameterNames.begin(), parameter));
                        if (parameterIndex >= attribute->args.size())
                            continue;
                        composedArguments[argumentIndex] = attribute->args[parameterIndex];
                        composedArguments[argumentIndex].loc = attribute->location();
                        if (argumentIndex < composedTypeArguments.size() &&
                            parameterIndex < attribute->typeArgs.size())
                        {
                            composedTypeArguments[argumentIndex] = attribute->typeArgs[parameterIndex];
                        }
                    }

                    auto expanded = makeNodePtr<AttributeStatement>(
                        composedTemplate->attribute,
                        std::move(composedArguments),
                        std::move(composedTypeArguments),
                        attribute->location(),
                        composedTemplate->qualifiedName,
                        composedTemplate->argumentNames);
                    expanded->origin = AttributeOrigin::Composed;
                    expanded->originParent = attribute->qualifiedName;
                    std::vector<const Symbol*> expandedChain = chain;
                    if (composedSymbol && composedSymbol->kind == SymbolKind::Attribute)
                        expandedChain.push_back(composedSymbol.Get());
                    compositionChains.emplace(expanded.Get(), std::move(expandedChain));
                    attributes.push_back(std::move(expanded));
                }
            }
        }

        auto nameTail = [](std::string_view name)
        {
            const size_t separator = name.rfind("::");
            return std::string(separator == std::string_view::npos ? name : name.substr(separator + 2));
        };
        auto applicationMatches = [&](const NodePtr<AttributeStatement>& application,
                                      std::string_view requiredName)
        {
            if (!application)
                return false;
            if (application->canonicalName == requiredName ||
                nameTail(application->canonicalName) == nameTail(requiredName))
                return true;
            if (application->qualifiedName == requiredName ||
                nameTail(application->qualifiedName) == nameTail(requiredName))
                return true;
            if (application->attribute != Attribute::Unknown)
            {
                const std::string builtInName = std::string(canonicalBuiltinAttributeName(application->attribute));
                return builtInName == requiredName || nameTail(builtInName) == nameTail(requiredName);
            }
            return false;
        };
        auto hasEffectiveAttribute = [&](std::string_view requiredName)
        {
            return std::ranges::any_of(attributes, [&](const auto& application)
            {
                return applicationMatches(application, requiredName);
            });
        };

        for (const auto& attribute : attributes)
        {
            if (!attribute || attribute->attribute == Attribute::Unknown)
                continue;
            const auto* contract = getBuiltinAttributeContract(attribute->attribute);
            if (!contract)
                continue;

            for (const std::string_view required : contract->requiredAttributes)
            {
                if (!hasEffectiveAttribute(required))
                    WIO_LOG_ADD_ERROR(attribute->location(), "Attribute '{}' requires attribute '{}'.", contract->canonicalName, required);
            }
            if (!contract->requiredAnyAttributes.empty() &&
                !std::ranges::any_of(contract->requiredAnyAttributes, hasEffectiveAttribute))
            {
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Attribute '{}' requires at least one compatible attribute from its RequiresAny policy.",
                    contract->canonicalName);
            }
            for (const std::string_view conflict : contract->conflictingAttributes)
            {
                if (hasEffectiveAttribute(conflict))
                    WIO_LOG_ADD_ERROR(attribute->location(), "Attribute '{}' conflicts with attribute '{}'.", contract->canonicalName, conflict);
            }
        }

        for (const auto& attribute : attributes)
        {
            if (!attribute || attribute->attribute != Attribute::Unknown)
                continue;
            Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, attribute->qualifiedName);
            if (!symbol || symbol->kind != SymbolKind::Attribute)
                continue;

            const size_t count = applicationCounts[symbol.Get()];
            if (symbol->attributeHasExplicitCardinality &&
                (count < symbol->attributeCardinalityMin || count > symbol->attributeCardinalityMax))
            {
                WIO_LOG_ADD_ERROR(
                    attribute->location(),
                    "Attribute '{}' requires cardinality {}..{}, but the effective declaration has {} application(s).",
                    attribute->qualifiedName,
                    symbol->attributeCardinalityMin,
                    symbol->attributeCardinalityMax,
                    count);
            }

            for (const std::string& required : symbol->attributeRequiredAttributes)
            {
                if (!hasEffectiveAttribute(required))
                    WIO_LOG_ADD_ERROR(attribute->location(), "Attribute '{}' requires attribute '{}'.", attribute->qualifiedName, required);
            }
            if (!symbol->attributeRequiredAnyAttributes.empty() &&
                !std::ranges::any_of(symbol->attributeRequiredAnyAttributes, hasEffectiveAttribute))
            {
                WIO_LOG_ADD_ERROR(attribute->location(), "Attribute '{}' requires at least one compatible attribute from its RequiresAny policy.", attribute->qualifiedName);
            }
            for (const std::string& conflict : symbol->attributeConflictingAttributes)
            {
                if (hasEffectiveAttribute(conflict))
                    WIO_LOG_ADD_ERROR(attribute->location(), "Attribute '{}' conflicts with attribute '{}'.", attribute->qualifiedName, conflict);
            }
            if (!symbol->attributeOnlyWithAttributes.empty())
            {
                for (const auto& other : attributes)
                {
                    if (!other || other.Get() == attribute.Get() || applicationMatches(other, attribute->qualifiedName))
                        continue;
                    const bool allowed = std::ranges::any_of(
                        symbol->attributeOnlyWithAttributes,
                        [&](const std::string& allowedName) { return applicationMatches(other, allowedName); });
                    if (!allowed)
                    {
                        WIO_LOG_ADD_ERROR(
                            other->location(),
                            "Attribute '{}' is not permitted alongside '{}' by its OnlyWith policy.",
                            other->qualifiedName,
                            attribute->qualifiedName);
                    }
                }
            }
        }

        std::unordered_map<std::string, std::vector<std::string>> orderingEdges;
        for (const auto& attribute : attributes)
        {
            if (!attribute || attribute->attribute != Attribute::Unknown)
                continue;
            Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, attribute->qualifiedName);
            if (!symbol || symbol->kind != SymbolKind::Attribute)
                continue;
            const std::string sourceName = nameTail(attribute->qualifiedName);
            for (const std::string& before : symbol->attributeBeforeAttributes)
                if (hasEffectiveAttribute(before))
                    orderingEdges[sourceName].push_back(nameTail(before));
            for (const std::string& after : symbol->attributeAfterAttributes)
                if (hasEffectiveAttribute(after))
                    orderingEdges[nameTail(after)].push_back(sourceName);
        }

        enum class OrderingVisit : std::uint8_t { None, Active, Complete };
        std::unordered_map<std::string, OrderingVisit> orderingVisits;
        std::vector<std::string> orderingPath;
        std::function<bool(const std::string&)> visitOrdering = [&](const std::string& name)
        {
            OrderingVisit& state = orderingVisits[name];
            if (state == OrderingVisit::Complete)
                return false;
            if (state == OrderingVisit::Active)
            {
                std::string cycle;
                const auto start = std::ranges::find(orderingPath, name);
                for (auto it = start; it != orderingPath.end(); ++it)
                {
                    if (!cycle.empty()) cycle += " -> ";
                    cycle += *it;
                }
                if (!cycle.empty()) cycle += " -> ";
                cycle += name;
                WIO_LOG_ADD_ERROR(
                    attributes.empty() ? common::Location::invalid() : attributes.front()->location(),
                    "Attribute processor ordering cycle detected: {}.",
                    cycle);
                return true;
            }

            state = OrderingVisit::Active;
            orderingPath.push_back(name);
            bool foundCycle = false;
            if (const auto edge = orderingEdges.find(name); edge != orderingEdges.end())
            {
                for (const std::string& destination : edge->second)
                    foundCycle = visitOrdering(destination) || foundCycle;
            }
            orderingPath.pop_back();
            state = OrderingVisit::Complete;
            return foundCycle;
        };
        for (const auto& [name, destinations] : orderingEdges)
        {
            WIO_UNUSED(destinations);
            if (visitOrdering(name))
                break;
        }

        // Materialize one deterministic order for codegen/tooling. Source
        // order breaks ties; Before/After contributes graph edges. Exit hooks
        // reverse this order when they unwind.
        std::vector<std::string> orderedNames;
        std::unordered_map<std::string, size_t> firstSeen;
        for (const auto& attribute : attributes)
        {
            if (!attribute || attribute->processorBindings.empty())
                continue;
            const std::string name = nameTail(
                attribute->canonicalName.empty() ? attribute->qualifiedName : attribute->canonicalName);
            if (!firstSeen.contains(name))
            {
                firstSeen.emplace(name, firstSeen.size());
                orderedNames.push_back(name);
            }
        }

        std::unordered_map<std::string, size_t> indegrees;
        for (const std::string& name : orderedNames)
            indegrees[name] = 0;
        for (const auto& [source, destinations] : orderingEdges)
        {
            indegrees.try_emplace(source, 0);
            for (const std::string& destination : destinations)
            {
                indegrees.try_emplace(destination, 0);
                ++indegrees[destination];
            }
        }

        std::vector<std::string> ready;
        for (const auto& [name, degree] : indegrees)
            if (degree == 0)
                ready.push_back(name);
        auto sourceRank = [&](const std::string& name)
        {
            const auto found = firstSeen.find(name);
            return found == firstSeen.end() ? firstSeen.size() : found->second;
        };
        std::ranges::sort(ready, [&](const std::string& left, const std::string& right)
        {
            return sourceRank(left) < sourceRank(right);
        });

        std::vector<std::string> topologicalOrder;
        while (!ready.empty())
        {
            std::string source = ready.front();
            ready.erase(ready.begin());
            topologicalOrder.push_back(source);
            if (const auto edge = orderingEdges.find(source); edge != orderingEdges.end())
            {
                for (const std::string& destination : edge->second)
                {
                    auto degree = indegrees.find(destination);
                    if (degree == indegrees.end() || degree->second == 0 || --degree->second != 0)
                        continue;
                    ready.push_back(destination);
                    std::ranges::sort(ready, [&](const std::string& left, const std::string& right)
                    {
                        return sourceRank(left) < sourceRank(right);
                    });
                }
            }
        }

        std::unordered_map<std::string, size_t> processorRanks;
        for (size_t index = 0; index < topologicalOrder.size(); ++index)
            processorRanks[topologicalOrder[index]] = index;
        for (size_t sourceIndex = 0; sourceIndex < attributes.size(); ++sourceIndex)
        {
            const auto& attribute = attributes[sourceIndex];
            if (!attribute)
                continue;
            const std::string name = nameTail(
                attribute->canonicalName.empty() ? attribute->qualifiedName : attribute->canonicalName);
            const auto rank = processorRanks.find(name);
            attribute->processorOrder = rank == processorRanks.end()
                ? sourceIndex
                : rank->second;
        }
    }

    void SemanticAnalyzer::applyActiveScopedAttributes(
        std::vector<NodePtr<AttributeStatement>>& attributes,
        std::string_view target)
    {
        for (const auto& active : activeScopedAttributes_)
        {
            if (!active) continue;
            Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, active->qualifiedName);
            if (!symbol || symbol->kind != SymbolKind::Attribute || !symbol->attributeScoped)
                continue;
            if (std::ranges::find(symbol->attributeTargets, std::string(target)) == symbol->attributeTargets.end())
                continue;
            const bool alreadyApplied = std::ranges::any_of(attributes, [&](const auto& existing)
            {
                if (!existing)
                    return false;

                // Declarations are visited by several semantic passes. A
                // scoped application is materialized into the declaration on
                // the first visit, so pointer identity cannot recognize it on
                // later visits. The source location identifies the original
                // `using` application while still allowing a nested, distinct
                // application of the same non-repeatable attribute to be
                // diagnosed normally.
                const auto& existingLocation = existing->location();
                const auto& activeLocation = active->location();
                return existing->origin == AttributeOrigin::Scoped &&
                    existing->qualifiedName == active->qualifiedName &&
                    existing->originParent == active->qualifiedName &&
                    existingLocation.file == activeLocation.file &&
                    existingLocation.line == activeLocation.line &&
                    existingLocation.column == activeLocation.column;
            });
            if (!alreadyApplied)
            {
                auto scoped = makeNodePtr<AttributeStatement>(
                    active->attribute,
                    active->args,
                    active->typeArgs,
                    active->location(),
                    active->qualifiedName,
                    active->argumentNames);
                scoped->argumentUsedDefaults = active->argumentUsedDefaults;
                scoped->runtimeRetained = active->runtimeRetained;
                scoped->origin = AttributeOrigin::Scoped;
                scoped->originParent = active->qualifiedName;
                attributes.push_back(std::move(scoped));
            }
        }
    }

    void SemanticAnalyzer::registerDerivedMethods(
        std::vector<NodePtr<AttributeStatement>>& attributes,
        const Ref<Type>& targetType,
        std::string_view target)
    {
        Ref<Type> resolvedTarget = unwrapAliasType(targetType);
        if (!resolvedTarget || resolvedTarget->kind() != TypeKind::Struct)
            return;

        auto targetStruct = resolvedTarget.AsFast<StructType>();
        if (!targetStruct || (target != "component" && target != "object"))
            return;

        auto findDeriveMemberMarker = [&](const FunctionDeclaration* declaration)
            -> const AttributeStatement*
        {
            if (!declaration)
                return nullptr;
            for (const auto& application : declaration->attributes)
            {
                if (!application || application->attribute != Attribute::Unknown)
                    continue;
                std::string canonicalName = application->canonicalName;
                if (canonicalName.empty())
                {
                    Ref<Symbol> marker = resolveQualifiedSymbol(currentScope_, application->qualifiedName);
                    if (marker && marker->kind == SymbolKind::Attribute)
                        canonicalName = marker->attributeCanonicalName;
                }
                if (canonicalName == "std::attribute::DeriveMember")
                    return application.Get();
            }
            return nullptr;
        };

        std::vector<std::pair<const AttributeStatement*, Ref<Symbol>>> effectiveAttributes;
        std::function<void(const AttributeStatement*, const Ref<Symbol>&, std::unordered_set<const Symbol*>&)>
            collectEffectiveAttribute =
                [&](const AttributeStatement* sourceApplication,
                    const Ref<Symbol>& symbol,
                    std::unordered_set<const Symbol*>& active)
                {
                    if (!sourceApplication || !symbol || symbol->kind != SymbolKind::Attribute)
                        return;
                    if (!active.insert(symbol.Get()).second)
                        return;

                    effectiveAttributes.emplace_back(sourceApplication, symbol);
                    for (const auto& composed : symbol->attributeComposition)
                    {
                        if (!composed || composed->attribute != Attribute::Unknown)
                            continue;
                        Ref<Symbol> composedSymbol = resolveQualifiedSymbol(currentScope_, composed->qualifiedName);
                        collectEffectiveAttribute(sourceApplication, composedSymbol, active);
                    }
                    active.erase(symbol.Get());
                };

        for (const auto& application : attributes)
        {
            if (!application || application->attribute != Attribute::Unknown)
                continue;

            Ref<Symbol> attributeSymbol = resolveQualifiedSymbol(currentScope_, application->qualifiedName);
            if (!attributeSymbol || attributeSymbol->kind != SymbolKind::Attribute)
                continue;

            std::unordered_set<const Symbol*> active;
            collectEffectiveAttribute(application.Get(), attributeSymbol, active);
        }

        for (const auto& [application, attributeSymbol] : effectiveAttributes)
        {
            for (size_t processorIndex = 0;
                 processorIndex < attributeSymbol->attributeProcessorPhases.size();
                 ++processorIndex)
            {
                if (attributeSymbol->attributeProcessorPhases[processorIndex] == "validation")
                {
                    Ref<Type> validatorTarget =
                        processorIndex < attributeSymbol->attributeProcessorTargetTypes.size()
                            ? unwrapAliasType(attributeSymbol->attributeProcessorTargetTypes[processorIndex])
                            : nullptr;
                    const bool validatorTargetIsAny = validatorTarget &&
                        validatorTarget->kind() == TypeKind::Primitive &&
                        validatorTarget.AsFast<PrimitiveType>()->name == "any";
                    if (validatorTarget && !validatorTargetIsAny &&
                        !isTypeDerivedFrom(resolvedTarget, validatorTarget))
                    {
                        WIO_LOG_ADD_ERROR(
                            application->location(),
                            "Validator '{}' requires a target compatible with '{}', but attribute '{}' is applied to '{}'.",
                            processorIndex < attributeSymbol->attributeProcessorCanonicalTypes.size()
                                ? attributeSymbol->attributeProcessorCanonicalTypes[processorIndex]
                                : std::string("<validator>"),
                            validatorTarget->toString(),
                            attributeSymbol->attributeCanonicalName,
                            resolvedTarget->toString());
                    }
                    continue;
                }
                if (attributeSymbol->attributeProcessorPhases[processorIndex] != "derive")
                    continue;

                const std::string processorName =
                    processorIndex < attributeSymbol->attributeProcessorCanonicalTypes.size()
                        ? attributeSymbol->attributeProcessorCanonicalTypes[processorIndex]
                        : std::string{};
                Ref<Symbol> processorSymbol = resolveQualifiedSymbol(currentScope_, processorName);
                if (!processorSymbol || !processorSymbol->innerScope)
                    continue;

                bool hasDefaultConstructor = true;
                if (Ref<Symbol> constructors = processorSymbol->innerScope->resolveLocally("OnConstruct"))
                {
                    hasDefaultConstructor = false;
                    std::vector<Ref<Symbol>> constructorCandidates;
                    if (constructors->kind == SymbolKind::FunctionGroup)
                        constructorCandidates = constructors->overloads;
                    else if (constructors->kind == SymbolKind::Function)
                        constructorCandidates.push_back(constructors);
                    for (const Ref<Symbol>& constructor : constructorCandidates)
                    {
                        Ref<FunctionType> constructorType = constructor && constructor->type
                            ? constructor->type.AsFast<FunctionType>()
                            : nullptr;
                        if (constructorType && constructorType->paramTypes.empty())
                        {
                            hasDefaultConstructor = true;
                            break;
                        }
                    }
                }
                if (!hasDefaultConstructor)
                {
                    WIO_LOG_ADD_ERROR(
                        application->location(),
                        "DeriveProcessor '{}' must be default constructible because each derived call owns an isolated processor instance.",
                        processorName);
                    continue;
                }

                const std::string processorCppType =
                    processorIndex < attributeSymbol->attributeProcessorCppTypes.size()
                        ? attributeSymbol->attributeProcessorCppTypes[processorIndex]
                        : std::string{};
                Ref<Type> processorTargetType =
                    processorIndex < attributeSymbol->attributeProcessorTargetTypes.size()
                        ? unwrapAliasType(attributeSymbol->attributeProcessorTargetTypes[processorIndex])
                        : nullptr;
                const bool processorTargetIsAny = processorTargetType &&
                    processorTargetType->kind() == TypeKind::Primitive &&
                    processorTargetType.AsFast<PrimitiveType>()->name == "any";
                if (processorTargetType && !processorTargetIsAny &&
                    !isTypeDerivedFrom(resolvedTarget, processorTargetType))
                {
                    WIO_LOG_ADD_ERROR(
                        application->location(),
                        "DeriveProcessor '{}' requires a target compatible with '{}', but attribute '{}' is applied to '{}'.",
                        processorName,
                        processorTargetType->toString(),
                        attributeSymbol->attributeCanonicalName,
                        resolvedTarget->toString());
                    continue;
                }

                bool sawDerivedMember = false;
                for (const auto& [_, member] : processorSymbol->innerScope->getSymbols())
                {
                    std::vector<Ref<Symbol>> candidates;
                    if (member && member->kind == SymbolKind::FunctionGroup)
                        candidates = member->overloads;
                    else if (member && member->kind == SymbolKind::Function)
                        candidates.push_back(member);

                    for (const Ref<Symbol>& methodSymbol : candidates)
                    {
                        auto declarationIt = functionDeclarationsBySymbol_.find(methodSymbol.Get());
                        const FunctionDeclaration* declaration =
                            declarationIt == functionDeclarationsBySymbol_.end()
                                ? nullptr
                                : declarationIt->second;
                        const AttributeStatement* marker = findDeriveMemberMarker(declaration);
                        if (!marker)
                            continue;
                        sawDerivedMember = true;

                        Ref<FunctionType> methodType = methodSymbol && methodSymbol->type
                            ? methodSymbol->type.AsFast<FunctionType>()
                            : nullptr;
                        Ref<Type> receiverType = methodType && !methodType->paramTypes.empty()
                            ? unwrapAliasType(methodType->paramTypes.front())
                            : nullptr;
                        const bool receiverIsAny = receiverType &&
                            receiverType->kind() == TypeKind::Primitive &&
                            receiverType.AsFast<PrimitiveType>()->name == "any";
                        const auto receiverReference = receiverType && receiverType->kind() == TypeKind::Reference
                            ? receiverType.AsFast<ReferenceType>()
                            : nullptr;
                        Ref<Type> typedReceiverTarget = receiverReference
                            ? unwrapAliasType(receiverReference->referredType)
                            : nullptr;
                        const bool receiverMatchesTargetContract = processorTargetType &&
                            !processorTargetIsAny && receiverReference && !receiverReference->isMutable &&
                            typedReceiverTarget && typedReceiverTarget->isCompatibleWith(processorTargetType);
                        const bool hasDefaultedPublicParameter = declaration &&
                            std::ranges::any_of(
                                declaration->parameters | std::views::drop(1),
                                [](const Parameter& parameter)
                                {
                                    return parameter.defaultValue != nullptr;
                                });
                        if (!declaration || !methodType ||
                            (!receiverIsAny && !receiverMatchesTargetContract) ||
                            !methodSymbol->flags.get_isPublic() || declaration->isAsync ||
                            !declaration->genericParameters.empty() ||
                            methodType->hasParameterPack || hasDefaultedPublicParameter ||
                            hasAttribute(declaration->attributes, Attribute::Native))
                        {
                            WIO_LOG_ADD_ERROR(
                                marker->location(),
                                "Derived member '{}.{}' must be a public, synchronous, non-generic Wio method whose first parameter is 'any' or an immutable view of its DeriveProcessor target contract, and whose public parameters have no defaults or packs.",
                                processorName,
                                methodSymbol ? methodSymbol->name : std::string("<unknown>"));
                            continue;
                        }

                        std::string exposedName = methodSymbol->name;
                        if (!marker->args.empty() && !marker->args.front().value.empty())
                            exposedName = marker->args.front().value;

                        if (exposedName == "OnConstruct" ||
                            common::isOperatorOverloadName(exposedName))
                        {
                            WIO_LOG_ADD_ERROR(
                                marker->location(),
                                "Derived member '{}' cannot define constructors or operators.",
                                exposedName);
                            continue;
                        }

                        if (auto scope = targetStruct->structScope.Lock();
                            scope && scope->resolveLocally(exposedName))
                        {
                            WIO_LOG_ADD_ERROR(
                                application->location(),
                                "Derived member '{}' conflicts with an existing member on '{}'.",
                                exposedName,
                                resolvedTarget->toString());
                            continue;
                        }

                        auto& methods = extensionMethods_[resolvedTarget.Get()];
                        if (methods.contains(exposedName))
                        {
                            WIO_LOG_ADD_ERROR(
                                application->location(),
                                "Derived member '{}' is ambiguous for '{}'.",
                                exposedName,
                                resolvedTarget->toString());
                            continue;
                        }

                        Ref<Symbol> derived = createSymbol(
                            methodSymbol->name,
                            methodType,
                            SymbolKind::Function,
                            application->location());
                        derived->scopePath = methodSymbol->scopePath;
                        derived->flags.set_isExtension(true);
                        derived->flags.set_isDerived(true);
                        derived->extensionTargetType = resolvedTarget;
                        derived->extensionMemberName = exposedName;
                        derived->extensionImplementation = methodSymbol;
                        derived->derivedProcessorCppType = processorCppType;
                        methods.emplace(exposedName, std::move(derived));
                    }
                }
                if (!sawDerivedMember)
                {
                    WIO_LOG_ADD_ERROR(
                        application->location(),
                        "DeriveProcessor '{}' must expose at least one method with [std::attribute::DeriveMember].",
                        processorName);
                }
            }
        }
    }

    void SemanticAnalyzer::visit(UsingAttributeStatement& node)
    {
        if (!node.attribute) return;

        Ref<Symbol> symbol = resolveQualifiedSymbol(currentScope_, node.attribute->qualifiedName);
        bool canActivate = symbol && symbol->kind == SymbolKind::Attribute && symbol->attributeScoped;
        if (isDeclarationPass_)
        {
            std::vector<NodePtr<AttributeStatement>> single{node.attribute};
            validateAttributeApplications(single, "", false);
            if (symbol && symbol->kind == SymbolKind::Attribute && !symbol->attributeScoped)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Attribute '{}' cannot be activated with 'using' because it is not declared scoped.",
                    node.attribute->qualifiedName);
            }
        }

        if (!canActivate) return;
        const size_t previousCount = activeScopedAttributes_.size();
        activeScopedAttributes_.push_back(node.attribute);
        if (node.body)
        {
            node.body->accept(*this);
            activeScopedAttributes_.resize(previousCount);
        }
    }

    void SemanticAnalyzer::visit(AttributeDeclaration& node)
    {
        if (isStructResolutionPass_)
            return;

        if (isDeclarationPass_)
        {
            static const std::unordered_set<std::string> supportedTargets = {
                "fn", "method", "component", "object", "interface", "type",
                "field", "variable", "parameter", "generic_parameter",
                "enum", "flagset", "flag", "enum_case", "extension", "realm",
                "attribute", "module", "import_scope", "constructor",
                "application", "system", "handler", "generated"
            };
            static const std::unordered_set<std::string> supportedRetention = {
                "source", "compile", "runtime"
            };

            std::unordered_set<std::string> parameterNames;
            bool sawDefault = false;
            std::vector<Ref<Type>> parameterTypes;
            std::vector<std::string> names;
            std::vector<bool> hasDefaults;
            std::vector<Token> defaults;
            parameterTypes.reserve(node.parameters.size());
            names.reserve(node.parameters.size());
            hasDefaults.reserve(node.parameters.size());
            defaults.reserve(node.parameters.size());

            for (auto& parameter : node.parameters)
            {
                if (!parameter.name || !parameter.type)
                    continue;

                const std::string& parameterName = parameter.name->token.value;
                if (!parameterNames.insert(parameterName).second)
                    WIO_LOG_ADD_ERROR(parameter.name->location(), "Attribute parameter '{}' is duplicated.", parameterName);

                parameter.type->accept(*this);
                parameterTypes.push_back(parameter.type->refType.Lock());
                names.push_back(parameterName);
                hasDefaults.push_back(parameter.defaultValue != nullptr);
                Token defaultToken = Token::invalid();
                if (parameter.defaultValue)
                {
                    std::unordered_set<const Symbol*> activeSymbols;
                    if (auto folded = tryEvaluateStaticAttributeConstant(
                            parameter.defaultValue,
                            variableDeclarationsBySymbol_,
                            activeSymbols))
                    {
                        defaultToken = std::move(*folded);
                    }
                }
                defaults.push_back(std::move(defaultToken));

                if (parameter.defaultValue)
                    sawDefault = true;
                else if (sawDefault)
                    WIO_LOG_ADD_ERROR(parameter.name->location(), "Required attribute parameters cannot follow defaulted parameters.");
            }

            for (const auto& target : node.targets)
            {
                if (!supportedTargets.contains(target))
                    WIO_LOG_ADD_ERROR(node.location(), "Unknown attribute target '{}'.", target);
            }
            for (const auto& retention : node.retention)
            {
                if (!supportedRetention.contains(retention))
                    WIO_LOG_ADD_ERROR(node.location(), "Unknown attribute retention '{}'.", retention);
            }

            Ref<Symbol> symbol = createSymbol(
                node.name->token.value,
                Compiler::get().getTypeContext().getUnknown(),
                SymbolKind::Attribute,
                node.location()
            );
            for (const std::string& realm : currentNamespacePath_)
            {
                if (!symbol->attributeCanonicalName.empty())
                    symbol->attributeCanonicalName += "::";
                symbol->attributeCanonicalName += realm;
            }
            if (!symbol->attributeCanonicalName.empty())
                symbol->attributeCanonicalName += "::";
            symbol->attributeCanonicalName += node.name->token.value;
            symbol->attributeTargets = node.targets;
            symbol->attributeRetention = node.retention;
            symbol->attributeConflictGroups = node.conflictGroups;
            symbol->attributeComposition = node.composedAttributes;
            symbol->attributeRequiredAttributes = node.requiredAttributes;
            symbol->attributeRequiredAnyAttributes = node.requiredAnyAttributes;
            symbol->attributeConflictingAttributes = node.conflictingAttributes;
            symbol->attributeOnlyWithAttributes = node.onlyWithAttributes;
            symbol->attributeBeforeAttributes = node.beforeAttributes;
            symbol->attributeAfterAttributes = node.afterAttributes;
            symbol->attributeImpliedAttributes = node.impliedAttributes;
            symbol->attributeProcessorTypes = node.processorTypes;
            symbol->attributeParameterNames = std::move(names);
            symbol->attributeParameterTypes = std::move(parameterTypes);
            symbol->attributeParameterHasDefault = std::move(hasDefaults);
            symbol->attributeParameterDefaults = std::move(defaults);
            symbol->attributeCardinalityMin = node.cardinalityMin;
            symbol->attributeCardinalityMax = node.cardinalityMax;
            symbol->attributeHasExplicitCardinality = node.hasExplicitCardinality;
            symbol->attributeRepeatable = node.repeatable;
            symbol->attributeInherited = node.inherited;
            symbol->attributeScoped = node.scoped;
            currentScope_->define(node.name->token.value, symbol);
            node.name->referencedSymbol = symbol;
            node.name->refType = symbol->type;
            return;
        }

        if (!isAttributeContractPass_)
        {
            for (size_t index = 0; index < node.parameters.size(); ++index)
            {
                auto& parameter = node.parameters[index];
                if (!parameter.defaultValue)
                    continue;

                parameter.defaultValue->accept(*this);
                Ref<Type> actualType = parameter.defaultValue->refType.Lock();
                Ref<Type> expectedType = parameter.type ? parameter.type->refType.Lock() : nullptr;
                if (expectedType && actualType &&
                    !expectedType->isUnknown() && !actualType->isUnknown() &&
                    !isAssignmentLikeCompatible(expectedType, actualType))
                {
                    WIO_LOG_ADD_ERROR(
                        parameter.defaultValue->location(),
                        "Default value for attribute parameter '{}' must be '{}', got '{}'.",
                        parameter.name ? parameter.name->token.value : std::to_string(index),
                        expectedType->toString(),
                        actualType->toString()
                    );
                }

                Ref<Symbol> symbol = node.name ? node.name->referencedSymbol.Lock() : nullptr;
                if (symbol && index < symbol->attributeParameterDefaults.size())
                {
                    std::unordered_set<const Symbol*> activeSymbols;
                    if (auto folded = tryEvaluateStaticAttributeConstant(
                            parameter.defaultValue,
                            variableDeclarationsBySymbol_,
                            activeSymbols))
                    {
                        symbol->attributeParameterDefaults[index] = std::move(*folded);
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter.defaultValue->location(),
                            "Default value for attribute parameter '{}' must be a compile-time scalar, string, or text expression.",
                            parameter.name ? parameter.name->token.value : std::to_string(index)
                        );
                    }
                }
            }
        }

        Ref<Symbol> attributeSymbol = node.name ? node.name->referencedSymbol.Lock() : nullptr;
        if (!attributeSymbol)
            return;

        if (!isAttributeContractPass_ && !attributeSymbol->attributeProcessorPhases.empty())
            return;

        attributeSymbol->attributeProcessorPhases.clear();
        attributeSymbol->attributeProcessorTargetTypes.clear();
        attributeSymbol->attributeProcessorCanonicalTypes.clear();
        attributeSymbol->attributeProcessorCppTypes.clear();
        attributeSymbol->attributeProcessorHookCppNames.clear();
        attributeSymbol->attributeProcessorHookModes.clear();
        attributeSymbol->attributeProcessorHookValueTypes.clear();
        attributeSymbol->attributeProcessorValidationResults.clear();
        attributeSymbol->attributeProcessorDiagnostics.clear();
        for (const std::string& processorName : node.processorTypes)
        {
            Ref<Symbol> processorSymbol = resolveQualifiedSymbol(currentScope_, processorName);
            Ref<Type> processorType = processorSymbol ? unwrapAliasType(processorSymbol->type) : nullptr;
            if (!processorSymbol || processorSymbol->kind != SymbolKind::Struct ||
                !processorType || processorType->kind() != TypeKind::Struct ||
                !processorType.AsFast<StructType>()->isObject)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Attribute processor '{}' must name an object implementing one phase interface.",
                    processorName);
                continue;
            }

            std::unordered_set<std::string> phases;
            Ref<Type> processorTargetType = nullptr;
            std::unordered_set<const Type*> visitedTypes;
            std::function<void(const Ref<Type>&)> collectPhases = [&](const Ref<Type>& candidate)
            {
                Ref<Type> resolved = unwrapAliasType(candidate);
                if (!resolved || resolved->kind() != TypeKind::Struct || !visitedTypes.insert(resolved.Get()).second)
                    return;
                auto structure = resolved.AsFast<StructType>();
                const std::string& typeName = structure->name;
                if (typeName == "Validator" || typeName == "DeriveProcessor")
                {
                    phases.insert(typeName == "Validator" ? "validation" : "derive");
                    if (!structure->genericArguments.empty())
                        processorTargetType = structure->genericArguments.front();
                }
                else if (typeName == "PreProcessor") phases.insert("pre");
                else if (typeName == "PostProcessor") phases.insert("post");
                else if (typeName == "FinallyProcessor") phases.insert("finally");
                else if (typeName == "AroundProcessor") phases.insert("around");
                for (const auto& base : structure->baseTypes)
                    collectPhases(base);
            };
            collectPhases(processorType);

            if (phases.size() != 1)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Attribute processor '{}' must implement exactly one of Validator, DeriveProcessor, PreProcessor, PostProcessor, FinallyProcessor, or AroundProcessor.",
                    processorName);
                continue;
            }

            const std::string phase = *phases.begin();
            attributeSymbol->attributeProcessorPhases.push_back(phase);
            attributeSymbol->attributeProcessorTargetTypes.push_back(processorTargetType);
            attributeSymbol->attributeProcessorCanonicalTypes.push_back(
                processorSymbol->scopePath.empty()
                    ? processorSymbol->name
                    : processorSymbol->scopePath + "::" + processorSymbol->name);
            attributeSymbol->attributeProcessorCppTypes.push_back(
                codegen::Mangler::mangleStruct(processorSymbol->name, processorSymbol->scopePath));
            attributeSymbol->attributeProcessorHookCppNames.emplace_back();
            attributeSymbol->attributeProcessorHookModes.emplace_back();
            attributeSymbol->attributeProcessorHookValueTypes.emplace_back();
            attributeSymbol->attributeProcessorValidationResults.push_back(-1);
            attributeSymbol->attributeProcessorDiagnostics.emplace_back();

            auto findProcessorMethod = [&](std::string_view methodName) -> const FunctionDeclaration*
            {
                if (!processorSymbol->innerScope)
                    return nullptr;
                Ref<Symbol> method = processorSymbol->innerScope->resolveLocally(std::string(methodName));
                if (!method)
                    return nullptr;
                if (method->kind == SymbolKind::FunctionGroup && !method->overloads.empty())
                    method = method->overloads.front();
                const auto declaration = functionDeclarationsBySymbol_.find(method.Get());
                return declaration == functionDeclarationsBySymbol_.end() ? nullptr : declaration->second;
            };

            if (phase == "pre" || phase == "post" || phase == "finally")
            {
                const std::string_view methodName = phase == "pre"
                    ? std::string_view("Before")
                    : (phase == "post" ? std::string_view("After") : std::string_view("Finally"));
                const FunctionDeclaration* hook = findProcessorMethod(methodName);
                Ref<Symbol> hookSymbol = hook && hook->name ? hook->name->referencedSymbol.Lock() : nullptr;
                Ref<FunctionType> hookType = hookSymbol ? hookSymbol->type.AsFast<FunctionType>() : nullptr;
                bool validHook = hook && hookType && hookType->returnType;
                std::string hookMode = "no_args";
                if (validHook && phase == "pre" && hook->parameters.size() == 1)
                {
                    Ref<Type> receiverType = hookType->paramTypes.empty()
                        ? nullptr
                        : unwrapAliasType(hookType->paramTypes.front());
                    const bool receiverIsAny = receiverType && receiverType->kind() == TypeKind::Primitive &&
                        receiverType.AsFast<PrimitiveType>()->name == "any";
                    const auto receiverReference = receiverType && receiverType->kind() == TypeKind::Reference
                        ? receiverType.AsFast<ReferenceType>()
                        : nullptr;
                    Ref<Type> typedReceiverTarget = receiverReference
                        ? unwrapAliasType(receiverReference->referredType)
                        : nullptr;
                    const bool receiverIsTypedView = receiverReference && !receiverReference->isMutable &&
                        typedReceiverTarget && typedReceiverTarget->kind() == TypeKind::Struct;
                    validHook = receiverIsAny || receiverIsTypedView;
                    hookMode = receiverIsAny ? "receiver_any" : "receiver_typed";
                    if (receiverIsTypedView)
                        attributeSymbol->attributeProcessorHookValueTypes.back() = receiverType;
                }
                else if (validHook && phase == "post" && hook->parameters.size() == 1)
                {
                    attributeSymbol->attributeProcessorHookValueTypes.back() = hookType->paramTypes.front();
                    hookMode = "result";
                }
                else if (validHook && phase == "finally" && hook->parameters.size() == 1)
                {
                    Ref<Type> outcomeType = unwrapAliasType(hookType->paramTypes.front());
                    validHook = outcomeType == Compiler::get().getTypeContext().getBool();
                    hookMode = "outcome_bool";
                }
                else if (validHook && !hook->parameters.empty())
                {
                    validHook = false;
                }

                const bool returnsVoid = validHook && hookType->returnType->isVoid();
                const bool returnsBool = validHook &&
                    unwrapAliasType(hookType->returnType) == Compiler::get().getTypeContext().getBool();
                if (phase != "pre")
                    validHook = validHook && returnsVoid;
                else
                    validHook = validHook && (returnsVoid || returnsBool);

                if (!validHook)
                {
                    if (phase == "pre")
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "pre processor '{}' must declare Before with no arguments, receiver: any, or an immutable typed receiver view; it may return bool only as a unit-target guard.",
                            processorName);
                    }
                    else
                    {
                        if (phase == "post")
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "post processor '{}' must declare 'fn After()' or 'fn After(result: T)'.",
                                processorName);
                        }
                        else
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "finally processor '{}' must declare 'fn Finally()' or 'fn Finally(succeeded: bool)'.",
                                processorName);
                        }
                    }
                }
                else
                {
                    attributeSymbol->attributeProcessorHookCppNames.back() =
                        codegen::Mangler::mangleFunction(
                            std::string(methodName),
                            hookType->paramTypes);
                    if (returnsBool)
                        hookMode += "_guard";
                    attributeSymbol->attributeProcessorHookModes.back() = std::move(hookMode);
                }
                continue;
            }

            if (phase == "around")
            {
                const FunctionDeclaration* hook = findProcessorMethod("Around");
                Ref<Symbol> hookSymbol = hook && hook->name ? hook->name->referencedSymbol.Lock() : nullptr;
                Ref<FunctionType> hookType = hookSymbol ? hookSymbol->type.AsFast<FunctionType>() : nullptr;
                Ref<Type> proceedType = hookType && hookType->paramTypes.size() == 1
                    ? unwrapAliasType(hookType->paramTypes.front())
                    : nullptr;
                Ref<FunctionType> proceedFunction = proceedType && proceedType->kind() == TypeKind::Function
                    ? proceedType.AsFast<FunctionType>()
                    : nullptr;
                if (!hook || !hookType || hook->parameters.size() != 1 ||
                    !hookType->returnType ||
                    !proceedFunction || !proceedFunction->paramTypes.empty() ||
                    !proceedFunction->returnType ||
                    !hookType->returnType->isCompatibleWith(proceedFunction->returnType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "around processor '{}' must declare 'fn Around(proceed: fn() -> T) -> T' (unit T may omit the return spelling).",
                        processorName);
                }
                else
                {
                    attributeSymbol->attributeProcessorHookCppNames.back() =
                        codegen::Mangler::mangleFunction("Around", hookType->paramTypes);
                    attributeSymbol->attributeProcessorHookValueTypes.back() = proceedFunction->returnType;
                    attributeSymbol->attributeProcessorHookModes.back() =
                        proceedFunction->returnType->isVoid() ? "proceed" : "proceed_result";
                }
                continue;
            }

            if (phase != "validation")
                continue;

            auto getSingleReturn = [](const FunctionDeclaration* function) -> const ReturnStatement*
            {
                if (!function || !function->body)
                    return nullptr;
                if (const auto* directReturn = function->body->as<ReturnStatement>())
                    return directReturn;
                const auto* block = function->body->as<BlockStatement>();
                if (!block || block->statements.size() != 1 || !block->statements.front())
                    return nullptr;
                return block->statements.front()->as<ReturnStatement>();
            };

            const FunctionDeclaration* validateMethod = findProcessorMethod("Validate");
            const ReturnStatement* validateReturn = getSingleReturn(validateMethod);
            if (!validateMethod || !validateReturn || !validateReturn->value ||
                !validateMethod->parameters.empty())
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Validator '{}' must declare parameterless 'fn Validate() -> bool' with one compile-time return expression.",
                    processorName);
                continue;
            }

            std::unordered_set<const Symbol*> activeSymbols;
            const auto foldedValidation = tryEvaluateStaticAttributeConstant(
                validateReturn->value,
                variableDeclarationsBySymbol_,
                activeSymbols);
            if (!foldedValidation ||
                (foldedValidation->type != TokenType::kwTrue && foldedValidation->type != TokenType::kwFalse))
            {
                WIO_LOG_ADD_ERROR(
                    validateReturn->location(),
                    "Validator '{}.Validate' must fold to a compile-time bool.",
                    processorName);
                continue;
            }
            attributeSymbol->attributeProcessorValidationResults.back() =
                foldedValidation->type == TokenType::kwTrue ? 1 : 0;

            if (const FunctionDeclaration* diagnosticMethod = findProcessorMethod("Diagnostic"))
            {
                const ReturnStatement* diagnosticReturn = getSingleReturn(diagnosticMethod);
                if (!diagnosticReturn || !diagnosticReturn->value || !diagnosticMethod->parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        diagnosticMethod->location(),
                        "Validator '{}.Diagnostic' must be parameterless and return one compile-time string expression.",
                        processorName);
                    continue;
                }
                activeSymbols.clear();
                const auto foldedDiagnostic = tryEvaluateStaticAttributeConstant(
                    diagnosticReturn->value,
                    variableDeclarationsBySymbol_,
                    activeSymbols);
                if (!foldedDiagnostic || foldedDiagnostic->type != TokenType::stringLiteral || foldedDiagnostic->isUnicodeString)
                {
                    WIO_LOG_ADD_ERROR(
                        diagnosticReturn->location(),
                        "Validator '{}.Diagnostic' must fold to a compile-time string.",
                        processorName);
                    continue;
                }
                attributeSymbol->attributeProcessorDiagnostics.back() = foldedDiagnostic->value;
            }
        }
    }
