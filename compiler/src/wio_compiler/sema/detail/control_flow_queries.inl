// Internal compiler detail extracted from the owning translation unit.
// This file is included inside that translation unit's anonymous namespace.

        std::optional<bool> tryEvaluateStaticTruthiness(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return std::nullopt;

            if (const auto* literal = expression->as<BoolLiteral>())
                return literal->token.value == "true";

            if (const auto* literal = expression->as<IntegerLiteral>())
            {
                const IntegerResult result = common::getInteger(literal->token.value);
                if (!result.isValid)
                    return std::nullopt;

                switch (result.type)
                {
                case IntegerType::i8: return result.value.v_i8 != 0;
                case IntegerType::i16: return result.value.v_i16 != 0;
                case IntegerType::i32: return result.value.v_i32 != 0;
                case IntegerType::i64: return result.value.v_i64 != 0;
                case IntegerType::u8: return result.value.v_u8 != 0;
                case IntegerType::u16: return result.value.v_u16 != 0;
                case IntegerType::u32: return result.value.v_u32 != 0;
                case IntegerType::u64: return result.value.v_u64 != 0;
                case IntegerType::isize: return result.value.v_isize != 0;
                case IntegerType::usize: return result.value.v_usize != 0;
                case IntegerType::Unknown: return std::nullopt;
                }
            }

            if (const auto* literal = expression->as<FloatLiteral>())
            {
                const FloatResult result = common::getFloat(literal->token.value);
                if (!result.isValid)
                    return std::nullopt;

                switch (result.type)
                {
                case FloatType::f32: return result.value.v_f32 != 0.0f;
                case FloatType::f64: return result.value.v_f64 != 0.0;
                case FloatType::Unknown: return std::nullopt;
                }
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                if (unary->op.type == TokenType::kwNot)
                {
                    if (auto operandTruthiness = tryEvaluateStaticTruthiness(unary->operand))
                        return !*operandTruthiness;

                    return std::nullopt;
                }

                if (unary->op.type == TokenType::opPlus || unary->op.type == TokenType::opMinus)
                    return tryEvaluateStaticTruthiness(unary->operand);
            }

            if (const auto* fit = expression->as<FitExpression>())
                return tryEvaluateStaticTruthiness(fit->operand);

            return std::nullopt;
        }

        bool statementMayBreakCurrentLoop(const NodePtr<Statement>& statement);

        bool statementDefinitelyReturns(const NodePtr<Statement>& statement)
        {
            if (!statement)
                return false;

            if (statement->is<ReturnStatement>())
                return true;

            if (const auto* block = statement->as<BlockStatement>())
            {
                for (const auto& nestedStatement : block->statements)
                {
                    if (statementDefinitelyReturns(nestedStatement))
                        return true;
                }

                return false;
            }

            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(ifStatement->condition))
                {
                    return *truthiness
                        ? statementDefinitelyReturns(ifStatement->thenBranch)
                        : statementDefinitelyReturns(ifStatement->elseBranch);
                }

                return statementDefinitelyReturns(ifStatement->thenBranch) &&
                       statementDefinitelyReturns(ifStatement->elseBranch);
            }

            if (const auto* whileStatement = statement->as<WhileStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(whileStatement->condition);
                    truthiness.has_value() && *truthiness)
                {
                    return statementDefinitelyReturns(whileStatement->body) &&
                           !statementMayBreakCurrentLoop(whileStatement->body);
                }

                return false;
            }

            if (const auto* cForStatement = statement->as<CForStatement>())
            {
                std::optional<bool> truthiness = cForStatement->condition
                    ? tryEvaluateStaticTruthiness(cForStatement->condition)
                    : std::optional<bool>(true);

                if (truthiness.has_value() && *truthiness)
                {
                    return statementDefinitelyReturns(cForStatement->body) &&
                           !statementMayBreakCurrentLoop(cForStatement->body);
                }

                return false;
            }

            return false;
        }

        bool statementMayBreakCurrentLoop(const NodePtr<Statement>& statement)
        {
            if (!statement)
                return false;

            if (statement->is<BreakStatement>())
                return true;

            if (const auto* block = statement->as<BlockStatement>())
            {
                for (const auto& nestedStatement : block->statements)
                {
                    if (statementMayBreakCurrentLoop(nestedStatement))
                        return true;
                }

                return false;
            }

            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(ifStatement->condition))
                {
                    return *truthiness
                        ? statementMayBreakCurrentLoop(ifStatement->thenBranch)
                        : statementMayBreakCurrentLoop(ifStatement->elseBranch);
                }

                return statementMayBreakCurrentLoop(ifStatement->thenBranch) ||
                       statementMayBreakCurrentLoop(ifStatement->elseBranch);
            }

            if (statement->is<WhileStatement>() ||
                statement->is<CForStatement>() ||
                statement->is<ForInStatement>())
            {
                return false;
            }

            return false;
        }
