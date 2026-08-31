#include "wio/wir/typed_ir_builder.h"

#include "wio/common/utility.h"
#include "wio/sema/symbol.h"
#include "wio/sema/type.h"

#include <optional>
#include <unordered_map>

namespace wio::wir::typed
{
    class BuildContext final
    {
    public:
        explicit BuildContext(BuildResult& result) : result_(result) {}

        void build(const Ref<Program>& program)
        {
            result_.module_.name = program && program->location().hasFile()
                ? program->location().file
                : "module";
            if (!program)
            {
                report("WIR2000", "Cannot build Typed WIR from a null program.");
                return;
            }

            collectFunctions(program->statements);
            for (const FunctionDeclaration* declaration : declarations_)
                buildFunction(*declaration);
        }

    private:
        struct FunctionState
        {
            Function* function = nullptr;
            std::size_t blockIndex = 0;
            ValueId::ValueType nextValue = 0;
            BlockId::ValueType nextBlock = 0;
            std::unordered_map<const sema::Symbol*, ValueId> values;
            std::vector<const sema::Symbol*> valueOrder;
        };

        struct LoopContext
        {
            BlockId continueTarget;
            BlockId breakTarget;
            std::vector<const sema::Symbol*> carriedSymbols;
        };

        BuildResult& result_;
        std::vector<const FunctionDeclaration*> declarations_;
        std::unordered_map<const sema::Symbol*, FunctionId> functionsBySymbol_;
        std::unordered_map<const sema::Type*, TypeId> typesBySemanticType_;
        std::vector<LoopContext> loopContexts_;

        static BasicBlock& currentBlock(FunctionState& state)
        {
            return state.function->blocks.at(state.blockIndex);
        }

        static bool blockIsTerminated(FunctionState& state)
        {
            const BasicBlock& block = currentBlock(state);
            return !block.instructions.empty() && isTerminator(block.instructions.back().opcode);
        }

        static std::size_t createBlock(
            FunctionState& state,
            std::string name,
            const SourceSpan& source)
        {
            const std::size_t index = state.function->blocks.size();
            state.function->blocks.push_back(BasicBlock{
                .id = BlockId{state.nextBlock++},
                .name = std::move(name),
                .source = source
            });
            return index;
        }

        void report(std::string code, std::string message, const ASTNode* node = nullptr)
        {
            result_.diagnostics_.push_back(BuildDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .source = node ? SourceSpan::at(node->location()) : SourceSpan{}
            });
        }

        void collectFunctions(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* function = statement->as<FunctionDeclaration>())
                {
                    const FunctionId id{static_cast<FunctionId::ValueType>(declarations_.size())};
                    declarations_.push_back(function);
                    const Ref<sema::Symbol> symbol = function->name
                        ? function->name->referencedSymbol.Lock()
                        : nullptr;
                    if (symbol)
                        functionsBySymbol_[symbol.Get()] = id;
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectFunctions(group->declarations);
                    continue;
                }
                if (const auto* realm = statement->as<RealmDeclaration>())
                    collectFunctions(realm->statements);
            }
        }

        TypeId mapType(Ref<sema::Type> type, const ASTNode* source)
        {
            if (!type)
            {
                report("WIR2001", "Semantic type is missing while building Typed WIR.", source);
                return {};
            }
            if (const auto found = typesBySemanticType_.find(type.Get()); found != typesBySemanticType_.end())
                return found->second;

            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;
            if (!type)
                return {};
            if (const auto found = typesBySemanticType_.find(type.Get()); found != typesBySemanticType_.end())
                return found->second;

            Type wirType;
            switch (type->kind())
            {
            case sema::TypeKind::Primitive:
            {
                const std::string& name = type.AsFast<sema::PrimitiveType>()->name;
                static const std::unordered_map<std::string, TypeKind> primitiveKinds{
                    {"void", TypeKind::Void}, {"bool", TypeKind::Bool},
                    {"i8", TypeKind::I8}, {"i16", TypeKind::I16}, {"i32", TypeKind::I32},
                    {"i64", TypeKind::I64}, {"isize", TypeKind::ISize},
                    {"u8", TypeKind::U8}, {"u16", TypeKind::U16}, {"u32", TypeKind::U32},
                    {"u64", TypeKind::U64}, {"usize", TypeKind::USize},
                    {"f32", TypeKind::F32}, {"f64", TypeKind::F64},
                    {"byte", TypeKind::Byte}, {"char", TypeKind::Char},
                    {"string", TypeKind::String}, {"text", TypeKind::Text}
                };
                const auto found = primitiveKinds.find(name);
                if (found == primitiveKinds.end())
                {
                    report("WIR2002", "Unsupported semantic primitive type '" + name + "'.", source);
                    return {};
                }
                wirType.kind = found->second;
                break;
            }
            case sema::TypeKind::Reference:
            {
                const auto reference = type.AsFast<sema::ReferenceType>();
                wirType.kind = TypeKind::Reference;
                wirType.arguments.push_back(mapType(reference->referredType, source));
                wirType.isMutable = reference->isMutable;
                break;
            }
            case sema::TypeKind::Nullable:
            {
                wirType.kind = TypeKind::Nullable;
                wirType.arguments.push_back(mapType(type.AsFast<sema::NullableType>()->valueType, source));
                break;
            }
            case sema::TypeKind::Array:
            {
                const auto array = type.AsFast<sema::ArrayType>();
                wirType.kind = TypeKind::Array;
                wirType.arguments.push_back(mapType(array->elementType, source));
                if (array->arrayKind == sema::ArrayType::ArrayKind::Static)
                    wirType.staticExtent = array->size;
                break;
            }
            case sema::TypeKind::Dictionary:
            {
                const auto dictionary = type.AsFast<sema::DictionaryType>();
                wirType.kind = TypeKind::Dictionary;
                wirType.name = dictionary->isOrdered ? "ordered" : "unordered";
                wirType.arguments = {
                    mapType(dictionary->keyType, source),
                    mapType(dictionary->valueType, source)
                };
                break;
            }
            case sema::TypeKind::Function:
            {
                const auto function = type.AsFast<sema::FunctionType>();
                wirType.kind = TypeKind::Function;
                for (const auto& parameterType : function->paramTypes)
                    wirType.arguments.push_back(mapType(parameterType, source));
                wirType.arguments.push_back(mapType(function->returnType, source));
                break;
            }
            case sema::TypeKind::AsyncTask:
                wirType.kind = TypeKind::AsyncTask;
                wirType.arguments.push_back(mapType(type.AsFast<sema::AsyncTaskType>()->valueType, source));
                break;
            case sema::TypeKind::Struct:
            {
                const auto structure = type.AsFast<sema::StructType>();
                wirType.kind = TypeKind::Named;
                wirType.name = structure->scopePath.empty()
                    ? structure->name
                    : structure->scopePath + "::" + structure->name;
                for (const auto& argument : structure->genericArguments)
                    wirType.arguments.push_back(mapType(argument, source));
                break;
            }
            default:
                report("WIR2003", "Semantic type '" + type->toString() + "' is not representable in the initial Typed WIR slice.", source);
                return {};
            }

            for (const TypeId argument : wirType.arguments)
            {
                if (!argument)
                    return {};
            }
            const TypeId id = result_.module_.types.intern(std::move(wirType));
            typesBySemanticType_[type.Get()] = id;
            return id;
        }

        void buildFunction(const FunctionDeclaration& declaration)
        {
            const Ref<sema::Symbol> symbol = declaration.name
                ? declaration.name->referencedSymbol.Lock()
                : nullptr;
            const auto functionType = symbol && symbol->type && symbol->type->kind() == sema::TypeKind::Function
                ? symbol->type.AsFast<sema::FunctionType>()
                : nullptr;
            if (!symbol || !functionType)
            {
                report("WIR2100", "Function declaration is missing its resolved semantic function type.", &declaration);
                return;
            }

            Function function;
            function.id = functionsBySymbol_.at(symbol.Get());
            function.name = symbol->scopePath.empty()
                ? symbol->name
                : symbol->scopePath + "::" + symbol->name;
            function.returnType = mapType(functionType->returnType, &declaration);
            function.source = SourceSpan::at(declaration.location());
            function.isAsync = declaration.isAsync;
            function.isExternal = declaration.body == nullptr;

            FunctionState state{.function = &function};
            for (std::size_t index = 0; index < declaration.parameters.size(); ++index)
            {
                const wio::Parameter& parameter = declaration.parameters[index];
                const Ref<sema::Symbol> parameterSymbol = parameter.name
                    ? parameter.name->referencedSymbol.Lock()
                    : nullptr;
                if (!parameterSymbol || index >= functionType->paramTypes.size())
                {
                    report("WIR2101", "Function parameter is missing its resolved semantic symbol or type.", parameter.name.Get());
                    continue;
                }
                const ValueId value{state.nextValue++};
                function.parameters.push_back(Parameter{
                    .id = value,
                    .name = parameterSymbol->name,
                    .type = mapType(functionType->paramTypes[index], parameter.name.Get()),
                    .source = SourceSpan::at(parameter.name->location())
                });
                state.values[parameterSymbol.Get()] = value;
                state.valueOrder.push_back(parameterSymbol.Get());
            }

            if (!function.isExternal)
            {
                state.blockIndex = createBlock(
                    state,
                    "entry",
                    SourceSpan::at(declaration.body->location()));
                buildStatement(declaration.body, state);
                if (!blockIsTerminated(state))
                {
                    const Type* returnType = result_.module_.types.tryGet(function.returnType);
                    if (returnType && returnType->kind == TypeKind::Void)
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Return});
                    else
                    {
                        report("WIR2102", "Non-void function does not end with a return in the initial Typed WIR slice.", &declaration);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Unreachable});
                    }
                }
            }

            result_.module_.functions.push_back(std::move(function));
        }

        void buildStatement(const NodePtr<Statement>& statement, FunctionState& state)
        {
            if (!statement)
                return;
            if (const auto* block = statement->as<BlockStatement>())
            {
                const std::size_t visibleValueCount = state.valueOrder.size();
                for (const auto& child : block->statements)
                {
                    if (blockIsTerminated(state))
                    {
                        report("WIR2200", "Statement appears after a terminator.", child.Get());
                        break;
                    }
                    buildStatement(child, state);
                }
                while (state.valueOrder.size() > visibleValueCount)
                {
                    state.values.erase(state.valueOrder.back());
                    state.valueOrder.pop_back();
                }
                return;
            }
            if (const auto* declaration = statement->as<VariableDeclaration>())
            {
                const Ref<sema::Symbol> symbol = declaration->name
                    ? declaration->name->referencedSymbol.Lock()
                    : nullptr;
                if (!symbol)
                {
                    report("WIR2202", "Local variable declaration is missing its semantic symbol.", declaration);
                    return;
                }

                ValueId value;
                if (declaration->initializer)
                    value = buildExpression(declaration->initializer, state);
                else
                    value = buildDefaultValue(mapType(symbol->type, declaration), declaration, state);
                if (!value)
                    return;
                state.values[symbol.Get()] = value;
                state.valueOrder.push_back(symbol.Get());
                return;
            }
            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                buildIfStatement(*ifStatement, state);
                return;
            }
            if (const auto* whileStatement = statement->as<WhileStatement>())
            {
                buildWhileStatement(*whileStatement, state);
                return;
            }
            if (const auto* cForStatement = statement->as<CForStatement>())
            {
                buildCForStatement(*cForStatement, state);
                return;
            }
            if (statement->is<BreakStatement>())
            {
                buildLoopTransfer(statement.Get(), false, state);
                return;
            }
            if (statement->is<ContinueStatement>())
            {
                buildLoopTransfer(statement.Get(), true, state);
                return;
            }
            if (const auto* returnStatement = statement->as<ReturnStatement>())
            {
                Instruction instruction{
                    .opcode = Opcode::Return,
                    .source = SourceSpan::at(returnStatement->location())
                };
                if (returnStatement->value)
                {
                    if (const ValueId value = buildExpression(returnStatement->value, state))
                        instruction.operands.push_back(value);
                }
                currentBlock(state).instructions.push_back(std::move(instruction));
                return;
            }
            if (const auto* expressionStatement = statement->as<ExpressionStatement>())
            {
                buildExpression(expressionStatement->expression, state);
                return;
            }
            report("WIR2201", "Statement kind '" + getKindNameStr(statement->kind()) + "' is not supported by the initial Typed WIR slice.", statement.Get());
        }

        ValueId buildExpression(const NodePtr<Expression>& expression, FunctionState& state)
        {
            if (!expression)
                return {};
            auto appendValue = [&](Instruction instruction)
            {
                instruction.result = ValueId{state.nextValue++};
                instruction.resultType = mapType(expression->refType.Lock(), expression.Get());
                const ValueId result = instruction.result;
                currentBlock(state).instructions.push_back(std::move(instruction));
                return result;
            };

            if (const auto* integer = expression->as<IntegerLiteral>())
            {
                try
                {
                    const std::string value = common::stripIntegerLiteralTypeSuffix(integer->token.value);
                    return appendValue(Instruction{
                        .opcode = Opcode::Constant,
                        .literal = static_cast<std::int64_t>(std::stoll(value)),
                        .source = SourceSpan::at(expression->location())
                    });
                }
                catch (...)
                {
                    report("WIR2300", "Integer literal cannot be represented by the initial Typed WIR literal model.", expression.Get());
                    return {};
                }
            }
            if (const auto* boolean = expression->as<BoolLiteral>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = boolean->token.type == TokenType::kwTrue,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* string = expression->as<StringLiteral>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = string->token.value,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* identifier = expression->as<Identifier>())
            {
                const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                const auto found = symbol ? state.values.find(symbol.Get()) : state.values.end();
                if (found != state.values.end())
                    return found->second;
                report("WIR2301", "Identifier is not a value available in the current Typed WIR function.", expression.Get());
                return {};
            }
            if (const auto* assignment = expression->as<AssignmentExpression>())
            {
                const auto* target = assignment->left
                    ? assignment->left->as<Identifier>()
                    : nullptr;
                const Ref<sema::Symbol> symbol = target
                    ? target->referencedSymbol.Lock()
                    : nullptr;
                const auto current = symbol
                    ? state.values.find(symbol.Get())
                    : state.values.end();
                if (!symbol || current == state.values.end())
                {
                    report("WIR2307", "Initial Typed WIR assignments require a local identifier target.", expression.Get());
                    return {};
                }

                const ValueId right = buildExpression(assignment->right, state);
                if (!right)
                    return {};
                ValueId assigned = right;
                if (assignment->op.type != TokenType::opAssign)
                {
                    const auto compoundOperator = mapCompoundAssignmentOperator(assignment->op.type);
                    if (!compoundOperator)
                    {
                        report("WIR2308", "Compound assignment operator is not supported by Typed WIR.", expression.Get());
                        return {};
                    }
                    assigned = ValueId{state.nextValue++};
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::Binary,
                        .result = assigned,
                        .resultType = mapType(symbol->type, target),
                        .operands = {current->second, right},
                        .binaryOperator = *compoundOperator,
                        .source = SourceSpan::at(expression->location())
                    });
                }
                state.values[symbol.Get()] = assigned;
                return assigned;
            }
            if (const auto* unary = expression->as<UnaryExpression>())
            {
                const auto op = mapUnaryOperator(unary->op.type);
                const ValueId operand = buildExpression(unary->operand, state);
                if (!op || !operand)
                {
                    report("WIR2302", "Unary expression is not supported by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Unary,
                    .operands = {operand},
                    .unaryOperator = *op,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* binary = expression->as<BinaryExpression>())
            {
                if (isLogicalAnd(binary->op.type) || isLogicalOr(binary->op.type))
                {
                    return buildShortCircuitExpression(
                        *binary,
                        isLogicalAnd(binary->op.type),
                        state);
                }
                const auto op = mapBinaryOperator(binary->op.type);
                const ValueId left = buildExpression(binary->left, state);
                const ValueId right = buildExpression(binary->right, state);
                if (!op || !left || !right)
                {
                    report("WIR2303", "Binary expression is not supported by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Binary,
                    .operands = {left, right},
                    .binaryOperator = *op,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* conditional = expression->as<ConditionalExpression>())
            {
                if (!isSideEffectFree(conditional->whenTrue) || !isSideEffectFree(conditional->whenFalse))
                {
                    report(
                        "WIR2306",
                        "Conditional expressions with side effects require region-based Typed WIR lowering.",
                        expression.Get());
                    return {};
                }
                const ValueId condition = buildExpression(conditional->condition, state);
                const ValueId whenTrue = buildExpression(conditional->whenTrue, state);
                const ValueId whenFalse = buildExpression(conditional->whenFalse, state);
                if (!condition || !whenTrue || !whenFalse)
                    return {};
                return appendValue(Instruction{
                    .opcode = Opcode::Select,
                    .operands = {condition, whenTrue, whenFalse},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* call = expression->as<FunctionCallExpression>())
            {
                const Ref<sema::Symbol> calleeSymbol = call->callee
                    ? call->callee->referencedSymbol.Lock()
                    : nullptr;
                const auto functionIt = calleeSymbol
                    ? functionsBySymbol_.find(calleeSymbol.Get())
                    : functionsBySymbol_.end();
                if (functionIt == functionsBySymbol_.end())
                {
                    report("WIR2304", "Call target is not a function indexed by the initial Typed WIR slice.", expression.Get());
                    return {};
                }
                Instruction instruction{
                    .opcode = Opcode::Call,
                    .callee = functionIt->second,
                    .source = SourceSpan::at(expression->location())
                };
                for (const auto& argument : call->arguments)
                {
                    const ValueId value = buildExpression(argument, state);
                    if (!value)
                        return {};
                    instruction.operands.push_back(value);
                }
                const TypeId resultType = mapType(expression->refType.Lock(), expression.Get());
                const Type* type = result_.module_.types.tryGet(resultType);
                if (type && type->kind == TypeKind::Void)
                {
                    currentBlock(state).instructions.push_back(std::move(instruction));
                    return {};
                }
                return appendValue(std::move(instruction));
            }

            report("WIR2305", "Expression kind '" + getKindNameStr(expression->kind()) + "' is not supported by the initial Typed WIR slice.", expression.Get());
            return {};
        }

        ValueId buildDefaultValue(
            const TypeId typeId,
            const ASTNode* source,
            FunctionState& state)
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            if (!type)
                return {};

            Literal literal;
            switch (type->kind)
            {
            case TypeKind::Bool: literal = false; break;
            case TypeKind::I8:
            case TypeKind::I16:
            case TypeKind::I32:
            case TypeKind::I64:
            case TypeKind::ISize: literal = std::int64_t{0}; break;
            case TypeKind::U8:
            case TypeKind::U16:
            case TypeKind::U32:
            case TypeKind::U64:
            case TypeKind::USize:
            case TypeKind::Byte:
            case TypeKind::Char: literal = std::uint64_t{0}; break;
            case TypeKind::F32:
            case TypeKind::F64: literal = 0.0; break;
            case TypeKind::String:
            case TypeKind::Text: literal = std::string{}; break;
            default:
                report(
                    "WIR2203",
                    "Default initialization for type '" + std::string(typeKindName(type->kind)) +
                        "' is not supported by Typed WIR.",
                    source);
                return {};
            }

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Constant,
                .result = result,
                .resultType = typeId,
                .literal = std::move(literal),
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            return result;
        }

        ValueId buildShortCircuitExpression(
            const BinaryExpression& expression,
            const bool logicalAnd,
            FunctionState& state)
        {
            const ValueId left = buildExpression(expression.left, state);
            if (!left)
                return {};

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::string prefix = logicalAnd ? "logical.and" : "logical.or";
            const std::size_t rightBlockIndex = createBlock(
                state, prefix + ".rhs", SourceSpan::at(expression.right->location()));
            const std::size_t shortBlockIndex = createBlock(
                state, prefix + ".short", SourceSpan::at(expression.left->location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, prefix + ".merge", SourceSpan::at(expression.location()));

            const BlockId rightBlock = currentBlockAt(state, rightBlockIndex).id;
            const BlockId shortBlock = currentBlockAt(state, shortBlockIndex).id;
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;
            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {left},
                .targets = logicalAnd
                    ? std::vector<BlockId>{rightBlock, shortBlock}
                    : std::vector<BlockId>{shortBlock, rightBlock},
                .source = SourceSpan::at(expression.location())
            });

            FunctionState rightState = state;
            rightState.blockIndex = rightBlockIndex;
            rightState.values = incomingValues;
            rightState.valueOrder = incomingOrder;
            const ValueId right = buildExpression(expression.right, rightState);
            if (!right)
                return {};
            state.nextValue = rightState.nextValue;
            state.nextBlock = rightState.nextBlock;

            FunctionState shortState = state;
            shortState.blockIndex = shortBlockIndex;
            shortState.values = incomingValues;
            shortState.valueOrder = incomingOrder;
            const ValueId shortValue{shortState.nextValue++};
            currentBlock(shortState).instructions.push_back(Instruction{
                .opcode = Opcode::Constant,
                .result = shortValue,
                .resultType = result_.module_.types.boolType(),
                .literal = !logicalAnd,
                .source = SourceSpan::at(expression.left->location())
            });
            state.nextValue = shortState.nextValue;
            state.nextBlock = shortState.nextBlock;

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            const ValueId result{state.nextValue++};
            merge.parameters.push_back(Parameter{
                .id = result,
                .name = "logical.result",
                .type = mapType(expression.refType.Lock(), &expression),
                .source = SourceSpan::at(expression.location())
            });

            std::vector<ValueId> rightArguments{right};
            std::vector<ValueId> shortArguments{shortValue};
            auto mergedValues = incomingValues;
            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId rightValue = rightState.values.contains(symbol)
                    ? rightState.values.at(symbol)
                    : incoming;
                if (rightValue == incoming)
                    continue;

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".logical",
                    .type = mapType(symbol->type, &expression),
                    .source = SourceSpan::at(expression.location())
                });
                rightArguments.push_back(rightValue);
                shortArguments.push_back(incoming);
                mergedValues[symbol] = merged;
            }

            currentBlock(rightState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(rightArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.right->location())
            });
            currentBlock(shortState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(shortArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.left->location())
            });

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
            return result;
        }

        void buildIfStatement(const IfStatement& statement, FunctionState& state)
        {
            const ValueId condition = buildExpression(statement.condition, state);
            if (!condition)
                return;

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::size_t thenBlockIndex = createBlock(
                state, "if.then", SourceSpan::at(statement.thenBranch->location()));
            const std::size_t elseBlockIndex = createBlock(
                state,
                statement.elseBranch ? "if.else" : "if.else.passthrough",
                SourceSpan::at(statement.elseBranch ? statement.elseBranch->location() : statement.location()));
            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, thenBlockIndex).id,
                    currentBlockAt(state, elseBlockIndex).id
                },
                .source = SourceSpan::at(statement.location())
            });

            FunctionState thenState = state;
            thenState.blockIndex = thenBlockIndex;
            thenState.values = incomingValues;
            thenState.valueOrder = incomingOrder;
            buildStatement(statement.thenBranch, thenState);
            state.nextValue = thenState.nextValue;
            state.nextBlock = thenState.nextBlock;

            FunctionState elseState = state;
            elseState.blockIndex = elseBlockIndex;
            elseState.values = incomingValues;
            elseState.valueOrder = incomingOrder;
            if (statement.elseBranch)
                buildStatement(statement.elseBranch, elseState);
            state.nextValue = elseState.nextValue;
            state.nextBlock = elseState.nextBlock;

            const bool thenFallsThrough = !blockIsTerminated(thenState);
            const bool elseFallsThrough = !blockIsTerminated(elseState);
            if (!thenFallsThrough && !elseFallsThrough)
            {
                state.blockIndex = thenState.blockIndex;
                state.values = incomingValues;
                state.valueOrder = incomingOrder;
                return;
            }

            const std::size_t mergeBlockIndex = createBlock(
                state, "if.merge", SourceSpan::at(statement.location()));
            BasicBlock& mergeBlock = currentBlockAt(state, mergeBlockIndex);
            std::vector<ValueId> thenArguments;
            std::vector<ValueId> elseArguments;
            auto mergedValues = incomingValues;

            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId thenValue = thenState.values.contains(symbol)
                    ? thenState.values.at(symbol)
                    : incoming;
                const ValueId elseValue = elseState.values.contains(symbol)
                    ? elseState.values.at(symbol)
                    : incoming;

                if (thenFallsThrough && elseFallsThrough && thenValue != elseValue)
                {
                    const ValueId merged{state.nextValue++};
                    mergeBlock.parameters.push_back(Parameter{
                        .id = merged,
                        .name = symbol->name,
                        .type = mapType(symbol->type, &statement),
                        .source = SourceSpan::at(statement.location())
                    });
                    thenArguments.push_back(thenValue);
                    elseArguments.push_back(elseValue);
                    mergedValues[symbol] = merged;
                }
                else if (thenFallsThrough)
                    mergedValues[symbol] = thenValue;
                else
                    mergedValues[symbol] = elseValue;
            }

            if (thenFallsThrough)
            {
                currentBlock(thenState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(thenArguments),
                    .targets = {mergeBlock.id},
                    .source = SourceSpan::at(statement.thenBranch->location())
                });
            }
            if (elseFallsThrough)
            {
                currentBlock(elseState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(elseArguments),
                    .targets = {mergeBlock.id},
                    .source = SourceSpan::at(statement.elseBranch ? statement.elseBranch->location() : statement.location())
                });
            }

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
        }

        std::unordered_map<const sema::Symbol*, ValueId> addBlockParameters(
            FunctionState& state,
            const std::size_t blockIndex,
            const std::vector<const sema::Symbol*>& symbols,
            const ASTNode& source,
            const std::string_view nameSuffix)
        {
            auto values = state.values;
            BasicBlock& block = currentBlockAt(state, blockIndex);
            for (const sema::Symbol* symbol : symbols)
            {
                const ValueId value{state.nextValue++};
                block.parameters.push_back(Parameter{
                    .id = value,
                    .name = symbol->name + std::string(nameSuffix),
                    .type = mapType(symbol->type, &source),
                    .source = SourceSpan::at(source.location())
                });
                values[symbol] = value;
            }
            return values;
        }

        std::vector<ValueId> collectCarriedValues(
            const std::unordered_map<const sema::Symbol*, ValueId>& availableValues,
            const std::vector<const sema::Symbol*>& symbols,
            const ASTNode* source)
        {
            std::vector<ValueId> values;
            values.reserve(symbols.size());
            for (const sema::Symbol* symbol : symbols)
            {
                const auto found = availableValues.find(symbol);
                if (found == availableValues.end())
                {
                    report("WIR2204", "Loop-carried local value is unavailable on a control-flow edge.", source);
                    values.push_back(ValueId{});
                }
                else
                    values.push_back(found->second);
            }
            return values;
        }

        void buildLoopTransfer(
            const ASTNode* statement,
            const bool isContinue,
            FunctionState& state)
        {
            if (loopContexts_.empty())
            {
                report(
                    isContinue ? "WIR2206" : "WIR2205",
                    isContinue
                        ? "Continue statement has no active Typed WIR loop target."
                        : "Break statement has no active Typed WIR loop target.",
                    statement);
                return;
            }

            const LoopContext& loop = loopContexts_.back();
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(state.values, loop.carriedSymbols, statement),
                .targets = {isContinue ? loop.continueTarget : loop.breakTarget},
                .source = statement ? SourceSpan::at(statement->location()) : SourceSpan{}
            });
        }

        void buildWhileStatement(const WhileStatement& statement, FunctionState& state)
        {
            const auto carriedSymbols = state.valueOrder;
            const auto incomingValues = state.values;
            const std::size_t preheaderBlockIndex = state.blockIndex;
            const std::size_t headerBlockIndex = createBlock(
                state, "while.header", SourceSpan::at(statement.location()));
            const std::size_t bodyBlockIndex = createBlock(
                state, "while.body", SourceSpan::at(statement.body->location()));
            const std::size_t conditionExitBlockIndex = createBlock(
                state, "while.condition-exit", SourceSpan::at(statement.condition->location()));
            const std::size_t exitBlockIndex = createBlock(
                state, "while.exit", SourceSpan::at(statement.location()));

            const auto headerValues = addBlockParameters(
                state, headerBlockIndex, carriedSymbols, statement, ".loop");
            const auto exitValues = addBlockParameters(
                state, exitBlockIndex, carriedSymbols, statement, ".after");

            currentBlockAt(state, preheaderBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(incomingValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, headerBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            state.blockIndex = headerBlockIndex;
            state.values = headerValues;
            state.valueOrder = carriedSymbols;
            const ValueId condition = buildExpression(statement.condition, state);
            if (!condition)
                return;

            const auto conditionValues = state.values;
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, bodyBlockIndex).id,
                    currentBlockAt(state, conditionExitBlockIndex).id
                },
                .source = SourceSpan::at(statement.condition->location())
            });
            currentBlockAt(state, conditionExitBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(conditionValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, exitBlockIndex).id},
                .source = SourceSpan::at(statement.condition->location())
            });

            FunctionState bodyState = state;
            bodyState.blockIndex = bodyBlockIndex;
            bodyState.values = conditionValues;
            bodyState.valueOrder = carriedSymbols;
            loopContexts_.push_back(LoopContext{
                .continueTarget = currentBlockAt(state, headerBlockIndex).id,
                .breakTarget = currentBlockAt(state, exitBlockIndex).id,
                .carriedSymbols = carriedSymbols
            });
            buildStatement(statement.body, bodyState);
            loopContexts_.pop_back();
            state.nextValue = bodyState.nextValue;
            state.nextBlock = bodyState.nextBlock;

            if (!blockIsTerminated(bodyState))
            {
                currentBlock(bodyState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(bodyState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, headerBlockIndex).id},
                    .source = SourceSpan::at(statement.body->location())
                });
            }

            state.blockIndex = exitBlockIndex;
            state.values = exitValues;
            state.valueOrder = carriedSymbols;
        }

        void buildCForStatement(const CForStatement& statement, FunctionState& state)
        {
            const std::size_t outerValueCount = state.valueOrder.size();
            if (statement.initializer)
                buildStatement(statement.initializer, state);
            if (blockIsTerminated(state))
                return;

            const auto carriedSymbols = state.valueOrder;
            const auto incomingValues = state.values;
            const std::size_t preheaderBlockIndex = state.blockIndex;
            const std::size_t headerBlockIndex = createBlock(
                state, "for.header", SourceSpan::at(statement.location()));
            const std::size_t bodyBlockIndex = createBlock(
                state, "for.body", SourceSpan::at(statement.body->location()));
            const std::size_t incrementBlockIndex = createBlock(
                state, "for.increment", SourceSpan::at(
                    statement.increment ? statement.increment->location() : statement.location()));
            const std::size_t conditionExitBlockIndex = createBlock(
                state, "for.condition-exit", SourceSpan::at(
                    statement.condition ? statement.condition->location() : statement.location()));
            const std::size_t exitBlockIndex = createBlock(
                state, "for.exit", SourceSpan::at(statement.location()));

            const auto headerValues = addBlockParameters(
                state, headerBlockIndex, carriedSymbols, statement, ".loop");
            const auto incrementValues = addBlockParameters(
                state, incrementBlockIndex, carriedSymbols, statement, ".next");
            const auto exitValues = addBlockParameters(
                state, exitBlockIndex, carriedSymbols, statement, ".after");

            currentBlockAt(state, preheaderBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(incomingValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, headerBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            state.blockIndex = headerBlockIndex;
            state.values = headerValues;
            state.valueOrder = carriedSymbols;
            ValueId condition;
            if (statement.condition)
                condition = buildExpression(statement.condition, state);
            else
            {
                condition = ValueId{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Constant,
                    .result = condition,
                    .resultType = result_.module_.types.boolType(),
                    .literal = true,
                    .source = SourceSpan::at(statement.location())
                });
            }
            if (!condition)
                return;

            const auto conditionValues = state.values;
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {
                    currentBlockAt(state, bodyBlockIndex).id,
                    currentBlockAt(state, conditionExitBlockIndex).id
                },
                .source = SourceSpan::at(
                    statement.condition ? statement.condition->location() : statement.location())
            });
            currentBlockAt(state, conditionExitBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = collectCarriedValues(conditionValues, carriedSymbols, &statement),
                .targets = {currentBlockAt(state, exitBlockIndex).id},
                .source = SourceSpan::at(statement.location())
            });

            FunctionState bodyState = state;
            bodyState.blockIndex = bodyBlockIndex;
            bodyState.values = conditionValues;
            bodyState.valueOrder = carriedSymbols;
            loopContexts_.push_back(LoopContext{
                .continueTarget = currentBlockAt(state, incrementBlockIndex).id,
                .breakTarget = currentBlockAt(state, exitBlockIndex).id,
                .carriedSymbols = carriedSymbols
            });
            buildStatement(statement.body, bodyState);
            loopContexts_.pop_back();
            state.nextValue = bodyState.nextValue;
            state.nextBlock = bodyState.nextBlock;
            if (!blockIsTerminated(bodyState))
            {
                currentBlock(bodyState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(bodyState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, incrementBlockIndex).id},
                    .source = SourceSpan::at(statement.body->location())
                });
            }

            FunctionState incrementState = state;
            incrementState.blockIndex = incrementBlockIndex;
            incrementState.values = incrementValues;
            incrementState.valueOrder = carriedSymbols;
            if (statement.increment)
                buildExpression(statement.increment, incrementState);
            if (!blockIsTerminated(incrementState))
            {
                currentBlock(incrementState).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = collectCarriedValues(incrementState.values, carriedSymbols, &statement),
                    .targets = {currentBlockAt(state, headerBlockIndex).id},
                    .source = SourceSpan::at(
                        statement.increment ? statement.increment->location() : statement.location())
                });
            }
            state.nextValue = incrementState.nextValue;
            state.nextBlock = incrementState.nextBlock;

            state.blockIndex = exitBlockIndex;
            state.values = exitValues;
            state.valueOrder = carriedSymbols;
            while (state.valueOrder.size() > outerValueCount)
            {
                state.values.erase(state.valueOrder.back());
                state.valueOrder.pop_back();
            }
        }

        static BasicBlock& currentBlockAt(FunctionState& state, const std::size_t index)
        {
            return state.function->blocks.at(index);
        }

        static bool isSideEffectFree(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return true;
            if (expression->is<IntegerLiteral>() || expression->is<FloatLiteral>() ||
                expression->is<StringLiteral>() || expression->is<BoolLiteral>() ||
                expression->is<CharLiteral>() || expression->is<ByteLiteral>() ||
                expression->is<Identifier>())
            {
                return true;
            }
            if (const auto* unary = expression->as<UnaryExpression>())
                return isSideEffectFree(unary->operand);
            if (const auto* binary = expression->as<BinaryExpression>())
                return isSideEffectFree(binary->left) && isSideEffectFree(binary->right);
            if (const auto* conditional = expression->as<ConditionalExpression>())
            {
                return isSideEffectFree(conditional->condition) &&
                       isSideEffectFree(conditional->whenTrue) &&
                       isSideEffectFree(conditional->whenFalse);
            }
            return false;
        }

        static std::optional<UnaryOperator> mapUnaryOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opMinus: return UnaryOperator::Negate;
            case TokenType::opLogicalNot:
            case TokenType::kwNot: return UnaryOperator::LogicalNot;
            case TokenType::opBitNot: return UnaryOperator::BitwiseNot;
            default: return std::nullopt;
            }
        }

        static bool isLogicalAnd(const TokenType type)
        {
            return type == TokenType::opLogicalAnd || type == TokenType::kwAnd;
        }

        static bool isLogicalOr(const TokenType type)
        {
            return type == TokenType::opLogicalOr || type == TokenType::kwOr;
        }

        static std::optional<BinaryOperator> mapBinaryOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opPlus: return BinaryOperator::Add;
            case TokenType::opMinus: return BinaryOperator::Subtract;
            case TokenType::opStar: return BinaryOperator::Multiply;
            case TokenType::opSlash: return BinaryOperator::Divide;
            case TokenType::opPercent: return BinaryOperator::Remainder;
            case TokenType::opEqual: return BinaryOperator::Equal;
            case TokenType::opNotEqual: return BinaryOperator::NotEqual;
            case TokenType::opLess: return BinaryOperator::Less;
            case TokenType::opLessEqual: return BinaryOperator::LessEqual;
            case TokenType::opGreater: return BinaryOperator::Greater;
            case TokenType::opGreaterEqual: return BinaryOperator::GreaterEqual;
            case TokenType::opBitAnd: return BinaryOperator::BitwiseAnd;
            case TokenType::opBitOr: return BinaryOperator::BitwiseOr;
            case TokenType::opBitXor: return BinaryOperator::BitwiseXor;
            case TokenType::opShiftLeft: return BinaryOperator::ShiftLeft;
            case TokenType::opShiftRight: return BinaryOperator::ShiftRight;
            default: return std::nullopt;
            }
        }

        static std::optional<BinaryOperator> mapCompoundAssignmentOperator(const TokenType type)
        {
            switch (type)
            {
            case TokenType::opPlusAssign: return BinaryOperator::Add;
            case TokenType::opMinusAssign: return BinaryOperator::Subtract;
            case TokenType::opStarAssign: return BinaryOperator::Multiply;
            case TokenType::opSlashAssign: return BinaryOperator::Divide;
            case TokenType::opPercentAssign: return BinaryOperator::Remainder;
            case TokenType::opBitAndAssign: return BinaryOperator::BitwiseAnd;
            case TokenType::opBitOrAssign: return BinaryOperator::BitwiseOr;
            case TokenType::opBitXorAssign: return BinaryOperator::BitwiseXor;
            case TokenType::opShiftLeftAssign: return BinaryOperator::ShiftLeft;
            case TokenType::opShiftRightAssign: return BinaryOperator::ShiftRight;
            default: return std::nullopt;
            }
        }
    };

    BuildResult Builder::build(const Ref<Program>& program) const
    {
        BuildResult result;
        BuildContext{result}.build(program);
        return result;
    }
}
