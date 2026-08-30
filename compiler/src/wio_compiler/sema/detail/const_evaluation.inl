// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        std::optional<Token> tryEvaluateStaticAttributeConstant(
            const NodePtr<Expression>& expression,
            const ConstVariableDeclarationMap& variableDeclarationsBySymbol,
            std::unordered_set<const Symbol*>& activeSymbols,
            const size_t depth = 0,
            size_t* visitedNodes = nullptr,
            size_t* foldedTextBytes = nullptr)
        {
            if (!expression)
                return std::nullopt;

            size_t localVisitedNodes = 0;
            size_t localFoldedTextBytes = 0;
            if (!visitedNodes)
                visitedNodes = &localVisitedNodes;
            if (!foldedTextBytes)
                foldedTextBytes = &localFoldedTextBytes;
            if (depth > ConstEvaluationLimiter::MaxDepth ||
                ++(*visitedNodes) > ConstEvaluationLimiter::MaxNodes)
            {
                return std::nullopt;
            }

            auto preserveLocation = [&](Token token)
            {
                token.loc = expression->location();
                return token;
            };

            if (const auto* literal = expression->as<StringLiteral>())
            {
                if (literal->token.value.size() > ConstEvaluationLimiter::MaxTextBytes -
                        std::min(*foldedTextBytes, ConstEvaluationLimiter::MaxTextBytes))
                {
                    return std::nullopt;
                }
                *foldedTextBytes += literal->token.value.size();
                return preserveLocation(literal->token);
            }
            if (const auto* literal = expression->as<IntegerLiteral>())
                return preserveLocation(literal->token);
            if (const auto* literal = expression->as<FloatLiteral>())
                return preserveLocation(literal->token);
            if (const auto* literal = expression->as<BoolLiteral>())
                return preserveLocation(literal->token);
            if (const auto* literal = expression->as<CharLiteral>())
                return preserveLocation(literal->token);
            if (const auto* literal = expression->as<ByteLiteral>())
                return preserveLocation(literal->token);

            if (const auto* identifier = expression->as<Identifier>())
            {
                Ref<Symbol> symbol = identifier->referencedSymbol.Lock();
                if (!symbol || !symbol->flags.get_isConst())
                    return std::nullopt;

                auto declarationIt = variableDeclarationsBySymbol.find(symbol.Get());
                if (declarationIt == variableDeclarationsBySymbol.end() ||
                    !declarationIt->second || !declarationIt->second->initializer)
                {
                    return std::nullopt;
                }

                if (!activeSymbols.insert(symbol.Get()).second)
                    return std::nullopt;
                auto value = tryEvaluateStaticAttributeConstant(
                    declarationIt->second->initializer,
                    variableDeclarationsBySymbol,
                    activeSymbols,
                    depth + 1,
                    visitedNodes,
                    foldedTextBytes);
                activeSymbols.erase(symbol.Get());
                if (value)
                    value->loc = expression->location();
                return value;
            }

            if (const auto* binary = expression->as<BinaryExpression>();
                binary && binary->op.type == TokenType::opPlus)
            {
                auto left = tryEvaluateStaticAttributeConstant(
                    binary->left, variableDeclarationsBySymbol, activeSymbols,
                    depth + 1, visitedNodes, foldedTextBytes);
                auto right = tryEvaluateStaticAttributeConstant(
                    binary->right, variableDeclarationsBySymbol, activeSymbols,
                    depth + 1, visitedNodes, foldedTextBytes);
                if (left && right &&
                    left->type == TokenType::stringLiteral &&
                    right->type == TokenType::stringLiteral &&
                    left->isUnicodeString == right->isUnicodeString)
                {
                    left->value += right->value;
                    left->loc = expression->location();
                    return left;
                }
            }

            if (const auto* interpolated = expression->as<InterpolatedStringLiteral>())
            {
                Token result{
                    .type = TokenType::stringLiteral,
                    .value = {},
                    .loc = expression->location(),
                    .isUnicodeString = interpolated->isUnicode
                };
                for (const auto& part : interpolated->parts)
                {
                    auto value = tryEvaluateStaticAttributeConstant(
                        part, variableDeclarationsBySymbol, activeSymbols,
                        depth + 1, visitedNodes, foldedTextBytes);
                    if (!value)
                        return std::nullopt;

                    if (value->type == TokenType::integerLiteral)
                        result.value += common::stripIntegerLiteralTypeSuffix(value->value);
                    else if (value->type == TokenType::floatLiteral)
                        result.value += common::stripFloatLiteralTypeSuffix(value->value);
                    else if (value->type == TokenType::stringLiteral ||
                             value->type == TokenType::kwTrue ||
                             value->type == TokenType::kwFalse ||
                             value->type == TokenType::charLiteral ||
                             value->type == TokenType::byteLiteral)
                        result.value += value->value;
                    else
                        return std::nullopt;
                }
                return result;
            }

            Ref<Type> expressionType = unwrapAliasType(expression->refType.Lock());
            const bool isIntegerExpression = expressionType &&
                expressionType->kind() == TypeKind::Primitive &&
                [&]()
                {
                    const std::string& name = expressionType.AsFast<PrimitiveType>()->name;
                    return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                           name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                           name == "isize" || name == "usize" || name == "byte";
                }();
            if (isIntegerExpression)
            {
                auto value = ConstExpressionEvaluator(variableDeclarationsBySymbol)
                    .evaluateInteger(expression, activeSymbols);
                if (value)
                {
                    return Token{
                        .type = TokenType::integerLiteral,
                        .value = std::to_string(*value),
                        .loc = expression->location()
                    };
                }
            }

            return std::nullopt;
        }
