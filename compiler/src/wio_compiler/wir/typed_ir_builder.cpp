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
                BasicBlock* block = nullptr;
                ValueId::ValueType nextValue = 0;
                std::unordered_map<const sema::Symbol*, ValueId> values;
            };

            BuildResult& result_;
            std::vector<const FunctionDeclaration*> declarations_;
            std::unordered_map<const sema::Symbol*, FunctionId> functionsBySymbol_;
            std::unordered_map<const sema::Type*, TypeId> typesBySemanticType_;

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
                }

                if (!function.isExternal)
                {
                    function.blocks.push_back(BasicBlock{
                        .id = BlockId{0},
                        .name = "entry",
                        .source = SourceSpan::at(declaration.body->location())
                    });
                    state.block = &function.blocks.back();
                    buildStatement(declaration.body, state);
                    if (state.block->instructions.empty() || !isTerminator(state.block->instructions.back().opcode))
                    {
                        const Type* returnType = result_.module_.types.tryGet(function.returnType);
                        if (returnType && returnType->kind == TypeKind::Void)
                            state.block->instructions.push_back(Instruction{.opcode = Opcode::Return});
                        else
                        {
                            report("WIR2102", "Non-void function does not end with a return in the initial Typed WIR slice.", &declaration);
                            state.block->instructions.push_back(Instruction{.opcode = Opcode::Unreachable});
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
                    for (const auto& child : block->statements)
                    {
                        if (!state.block->instructions.empty() && isTerminator(state.block->instructions.back().opcode))
                        {
                            report("WIR2200", "Statement appears after a terminator.", child.Get());
                            break;
                        }
                        buildStatement(child, state);
                    }
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
                    state.block->instructions.push_back(std::move(instruction));
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
                    state.block->instructions.push_back(std::move(instruction));
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
                        state.block->instructions.push_back(std::move(instruction));
                        return {};
                    }
                    return appendValue(std::move(instruction));
                }

                report("WIR2305", "Expression kind '" + getKindNameStr(expression->kind()) + "' is not supported by the initial Typed WIR slice.", expression.Get());
                return {};
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
                case TokenType::opLogicalAnd:
                case TokenType::kwAnd: return BinaryOperator::LogicalAnd;
                case TokenType::opLogicalOr:
                case TokenType::kwOr: return BinaryOperator::LogicalOr;
                case TokenType::opBitAnd: return BinaryOperator::BitwiseAnd;
                case TokenType::opBitOr: return BinaryOperator::BitwiseOr;
                case TokenType::opBitXor: return BinaryOperator::BitwiseXor;
                case TokenType::opShiftLeft: return BinaryOperator::ShiftLeft;
                case TokenType::opShiftRight: return BinaryOperator::ShiftRight;
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
