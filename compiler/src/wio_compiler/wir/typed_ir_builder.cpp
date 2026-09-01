#include "wio/wir/typed_ir_builder.h"

#include "wio/common/utility.h"
#include "wio/sema/scope.h"
#include "wio/sema/symbol.h"
#include "wio/sema/type.h"

#include <algorithm>
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
            std::unordered_map<const sema::Symbol*, ValueId> places;
            std::vector<const sema::Symbol*> placeOrder;
        };

        struct LoopContext
        {
            BlockId continueTarget;
            BlockId breakTarget;
            std::vector<const sema::Symbol*> carriedSymbols;
            std::size_t placeDepth = 0;
        };

        BuildResult& result_;
        std::vector<const FunctionDeclaration*> declarations_;
        std::unordered_map<const sema::Symbol*, FunctionId> functionsBySymbol_;
        std::unordered_map<const sema::Type*, TypeId> typesBySemanticType_;
        std::vector<LoopContext> loopContexts_;

        TypeId referenceType(const TypeId referredType, const bool isMutable)
        {
            return result_.module_.types.intern(Type{
                .kind = TypeKind::Reference,
                .arguments = {referredType},
                .isMutable = isMutable
            });
        }

        bool autoReadableReference(const Type& reference) const
        {
            if (reference.kind != TypeKind::Reference || reference.arguments.size() != 1)
                return false;
            const Type* referred = result_.module_.types.tryGet(reference.arguments.front());
            return referred && !(referred->kind == TypeKind::Named &&
                (referred->nominalKind == NominalKind::Object || referred->nominalKind == NominalKind::Interface));
        }

        ValueId emitLoad(
            const ValueId place,
            const TypeId valueType,
            const ASTNode* source,
            FunctionState& state)
        {
            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Load,
                .result = result,
                .resultType = valueType,
                .operands = {place},
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            return result;
        }

        bool isLifecycleValue(const TypeId typeId) const
        {
            const Type* type = result_.module_.types.tryGet(typeId);
            return type && type->kind == TypeKind::Named &&
                (type->nominalKind == NominalKind::Component || type->nominalKind == NominalKind::Object);
        }

        void emitDropsFrom(
            const std::size_t firstPlace,
            FunctionState& state,
            const ASTNode* source)
        {
            for (std::size_t index = state.placeOrder.size(); index > firstPlace; --index)
            {
                const sema::Symbol* symbol = state.placeOrder[index - 1];
                const auto place = state.places.find(symbol);
                if (place == state.places.end())
                    continue;
                const TypeId valueType = mapType(symbol->type, source);
                if (!isLifecycleValue(valueType))
                    continue;
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Drop,
                    .operands = {place->second},
                    .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
                });
            }
        }

        ValueId adaptPlaceMutability(
            const ValueId place,
            const TypeId placeTypeId,
            const bool needsMutable,
            const ASTNode* source,
            FunctionState& state)
        {
            const Type* placeType = result_.module_.types.tryGet(placeTypeId);
            if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1)
            {
                report("WIR2323", "Addressable expression did not produce a reference place.", source);
                return {};
            }
            if (needsMutable && !placeType->isMutable)
            {
                report("WIR2324", "A mutable place was requested from a read-only view.", source);
                return {};
            }
            if (needsMutable || !placeType->isMutable)
                return place;

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Borrow,
                .result = result,
                .resultType = referenceType(placeType->arguments.front(), false),
                .operands = {place},
                .source = source ? SourceSpan::at(source->location()) : SourceSpan{}
            });
            return result;
        }

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
            case sema::TypeKind::Null:
                return mapType(type.AsFast<sema::NullType>()->transformedType, source);
            case sema::TypeKind::Array:
            {
                const auto array = type.AsFast<sema::ArrayType>();
                wirType.kind = TypeKind::Array;
                wirType.arguments.push_back(mapType(array->elementType, source));
                if (array->arrayKind == sema::ArrayType::ArrayKind::Static ||
                    array->arrayKind == sema::ArrayType::ArrayKind::Literal)
                {
                    wirType.staticExtent = array->size;
                }
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
                wirType.nominalKind = structure->isFlagset
                    ? NominalKind::Flagset
                    : structure->isEnum
                        ? NominalKind::Enum
                        : structure->isInterface
                            ? NominalKind::Interface
                            : structure->isObject
                                ? NominalKind::Object
                                : NominalKind::Component;
                wirType.nominalRepresentation = structure->isNativePodComponent
                    ? NominalRepresentation::NativePod
                    : NominalRepresentation::Wio;
                for (const TypeId argument : wirType.arguments)
                {
                    if (!argument)
                        return {};
                }

                const TypeId id = result_.module_.types.internNominal(std::move(wirType));
                typesBySemanticType_[type.Get()] = id;

                std::vector<TypeId> baseTypes;
                baseTypes.reserve(structure->baseTypes.size());
                for (const Ref<sema::Type>& baseType : structure->baseTypes)
                    baseTypes.push_back(mapType(baseType, source));

                std::vector<FieldLayout> fields;
                fields.reserve(structure->fieldNames.size());
                const Ref<sema::Scope> structScope = structure->structScope.Lock();
                for (std::size_t index = 0;
                     index < structure->fieldNames.size() && index < structure->fieldTypes.size();
                     ++index)
                {
                    const std::string& fieldName = structure->fieldNames[index];
                    const Ref<sema::Symbol> fieldSymbol = structScope
                        ? structScope->resolveLocally(fieldName)
                        : nullptr;
                    FieldVisibility visibility = FieldVisibility::Private;
                    if (fieldSymbol && fieldSymbol->flags.get_isPublic())
                        visibility = FieldVisibility::Public;
                    else if (fieldSymbol && fieldSymbol->flags.get_isProtected())
                        visibility = FieldVisibility::Protected;
                    fields.push_back(FieldLayout{
                        .name = fieldName,
                        .type = mapType(structure->fieldTypes[index], source),
                        .isMutable = !fieldSymbol || !fieldSymbol->flags.get_isReadOnly(),
                        .visibility = visibility
                    });
                }

                Type& storedType = result_.module_.types.getMutable(id);
                storedType.baseTypes = std::move(baseTypes);
                storedType.fields = std::move(fields);
                storedType.hasConstructor = structScope && structScope->resolveLocally("OnConstruct");
                storedType.hasDestructor = structScope && structScope->resolveLocally("OnDestruct");
                return id;
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
            if (!function.isExternal)
            {
                state.blockIndex = createBlock(
                    state,
                    "entry",
                    SourceSpan::at(declaration.body->location()));
            }
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
                const TypeId parameterType = mapType(functionType->paramTypes[index], parameter.name.Get());
                const Type* parameterTypeInfo = result_.module_.types.tryGet(parameterType);
                if (function.isExternal || (parameterTypeInfo && parameterTypeInfo->kind == TypeKind::Reference))
                {
                    state.values[parameterSymbol.Get()] = value;
                    state.valueOrder.push_back(parameterSymbol.Get());
                }
                else
                {
                    const TypeId placeType = referenceType(parameterType, parameterSymbol->flags.get_isMutable());
                    const ValueId place{state.nextValue++};
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::LocalPlace,
                        .result = place,
                        .resultType = placeType,
                        .selector = parameterSymbol->name,
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::PlaceInit,
                        .operands = {place, value},
                        .source = SourceSpan::at(parameter.name->location())
                    });
                    state.places[parameterSymbol.Get()] = place;
                    state.placeOrder.push_back(parameterSymbol.Get());
                }
            }

            if (!function.isExternal)
            {
                buildStatement(declaration.body, state);
                if (!blockIsTerminated(state))
                {
                    const Type* returnType = result_.module_.types.tryGet(function.returnType);
                    if (returnType && returnType->kind == TypeKind::Void)
                    {
                        emitDropsFrom(0, state, &declaration);
                        currentBlock(state).instructions.push_back(Instruction{.opcode = Opcode::Return});
                    }
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
                const std::size_t visiblePlaceCount = state.placeOrder.size();
                for (const auto& child : block->statements)
                {
                    if (blockIsTerminated(state))
                    {
                        report("WIR2200", "Statement appears after a terminator.", child.Get());
                        break;
                    }
                    buildStatement(child, state);
                }
                if (!blockIsTerminated(state))
                    emitDropsFrom(visiblePlaceCount, state, block);
                while (state.valueOrder.size() > visibleValueCount)
                {
                    state.values.erase(state.valueOrder.back());
                    state.valueOrder.pop_back();
                }
                while (state.placeOrder.size() > visiblePlaceCount)
                {
                    state.places.erase(state.placeOrder.back());
                    state.placeOrder.pop_back();
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
                    value = buildExpressionAs(
                        declaration->initializer,
                        mapType(symbol->type, declaration),
                        state);
                else
                    value = buildDefaultValue(mapType(symbol->type, declaration), declaration, state);
                if (!value)
                    return;
                const TypeId valueType = mapType(symbol->type, declaration);
                const TypeId placeType = referenceType(valueType, symbol->flags.get_isMutable());
                const ValueId place{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::LocalPlace,
                    .result = place,
                    .resultType = placeType,
                    .selector = symbol->name,
                    .source = SourceSpan::at(declaration->location())
                });
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::PlaceInit,
                    .operands = {place, value},
                    .source = SourceSpan::at(declaration->location())
                });
                state.places[symbol.Get()] = place;
                state.placeOrder.push_back(symbol.Get());
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
                    if (const ValueId value = buildExpressionAs(
                            returnStatement->value,
                            state.function->returnType,
                            state))
                        instruction.operands.push_back(value);
                }
                emitDropsFrom(0, state, returnStatement);
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
                const TypeId typeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* type = result_.module_.types.tryGet(typeId);
                const auto integerType = type ? mapIntegerType(type->kind) : std::nullopt;
                if (!integerType)
                {
                    report("WIR2300", "Integer literal has no representable WIR integer type.", expression.Get());
                    return {};
                }
                const IntegerResult parsed = common::getIntegerAsType(integer->token.value, *integerType);
                const auto literal = integerLiteral(parsed);
                if (!parsed.isValid || !literal)
                {
                    report("WIR2300", "Integer literal cannot be represented by the initial Typed WIR literal model.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = *literal,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* floating = expression->as<FloatLiteral>())
            {
                const TypeId typeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* type = result_.module_.types.tryGet(typeId);
                const auto floatType = type ? mapFloatType(type->kind) : std::nullopt;
                if (!floatType)
                {
                    report("WIR2309", "Float literal has no representable WIR floating-point type.", expression.Get());
                    return {};
                }
                const FloatResult parsed = common::getFloatAsType(floating->token.value, *floatType);
                if (!parsed.isValid)
                {
                    report("WIR2309", "Float literal cannot be represented by the Typed WIR literal model.", expression.Get());
                    return {};
                }
                const double value = parsed.type == FloatType::f32
                    ? static_cast<double>(parsed.value.v_f32)
                    : parsed.value.v_f64;
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = value,
                    .source = SourceSpan::at(expression->location())
                });
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
            if (const auto* character = expression->as<CharLiteral>())
            {
                if (character->token.value.size() != 1)
                {
                    report("WIR2310", "Char literal must contain exactly one byte-sized character.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = static_cast<std::uint64_t>(
                        static_cast<unsigned char>(character->token.value.front())),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* byte = expression->as<ByteLiteral>())
            {
                const IntegerResult parsed = common::getIntegerAsType(byte->token.value, IntegerType::u8);
                if (!parsed.isValid)
                {
                    report("WIR2311", "Byte literal is outside the u8 range.", expression.Get());
                    return {};
                }
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = static_cast<std::uint64_t>(parsed.value.v_u8),
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* array = expression->as<ArrayLiteral>())
            {
                const TypeId arrayTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* arrayType = result_.module_.types.tryGet(arrayTypeId);
                if (!arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1)
                {
                    report("WIR2320", "Array literal requires a resolved WIR array element type.", expression.Get());
                    return {};
                }
                const TypeId elementType = arrayType->arguments.front();

                Instruction instruction{
                    .opcode = Opcode::ArrayCreate,
                    .source = SourceSpan::at(expression->location())
                };
                instruction.operands.reserve(array->elements.size());
                for (const auto& element : array->elements)
                {
                    const ValueId value = buildExpressionAs(element, elementType, state);
                    if (!value)
                        return {};
                    instruction.operands.push_back(value);
                }
                return appendValue(std::move(instruction));
            }
            if (expression->is<NullExpression>())
            {
                return appendValue(Instruction{
                    .opcode = Opcode::Constant,
                    .literal = NullLiteral{},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* fit = expression->as<FitExpression>())
            {
                if (fit->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2312", "Overloaded fit conversion is not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                const TypeId sourceTypeId = mapType(fit->operand->refType.Lock(), fit->operand.Get());
                const TypeId destinationTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* sourceType = result_.module_.types.tryGet(sourceTypeId);
                const Type* destinationType = result_.module_.types.tryGet(destinationTypeId);
                if (!sourceType || !destinationType ||
                    !isNumericType(sourceType->kind) || !isNumericType(destinationType->kind))
                {
                    report("WIR2313", "Initial Typed WIR fit lowering supports numeric conversions only.", expression.Get());
                    return {};
                }
                const ValueId operand = buildExpression(fit->operand, state);
                if (!operand)
                    return {};
                return appendValue(Instruction{
                    .opcode = Opcode::Convert,
                    .operands = {operand},
                    .conversionKind = ConversionKind::NumericFit,
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* identifier = expression->as<Identifier>())
            {
                const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                const auto place = symbol ? state.places.find(symbol.Get()) : state.places.end();
                if (place != state.places.end())
                    return emitLoad(place->second, mapType(symbol->type, expression.Get()), expression.Get(), state);
                const auto found = symbol ? state.values.find(symbol.Get()) : state.values.end();
                if (found != state.values.end())
                    return found->second;
                report("WIR2301", "Identifier is not a value available in the current Typed WIR function.", expression.Get());
                return {};
            }
            if (const auto* reference = expression->as<RefExpression>())
            {
                const TypeId referenceTypeId = mapType(expression->refType.Lock(), expression.Get());
                const Type* referenceTypeInfo = result_.module_.types.tryGet(referenceTypeId);
                return buildPlace(
                    reference->operand,
                    referenceTypeInfo && referenceTypeInfo->kind == TypeKind::Reference && referenceTypeInfo->isMutable,
                    state);
            }
            if (const auto* member = expression->as<MemberAccessExpression>())
            {
                if (!member->referencedSymbol.Lock() ||
                    member->referencedSymbol.Lock()->kind != sema::SymbolKind::Variable)
                {
                    report("WIR2325", "Initial Typed WIR member reads currently require a resolved data field.", expression.Get());
                    return {};
                }
                const ValueId place = buildPlace(expression, false, state);
                if (!place)
                    return {};
                return emitLoad(place, mapType(expression->refType.Lock(), expression.Get()), expression.Get(), state);
            }
            if (const auto* access = expression->as<ArrayAccessExpression>())
            {
                if (access->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2321", "Overloaded index access is not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                TypeId objectTypeId = mapType(access->object->refType.Lock(), access->object.Get());
                const Type* objectType = result_.module_.types.tryGet(objectTypeId);
                if (objectType && objectType->kind == TypeKind::Reference && autoReadableReference(*objectType))
                {
                    objectTypeId = objectType->arguments.front();
                    objectType = result_.module_.types.tryGet(objectTypeId);
                }
                if (!objectType || objectType->kind != TypeKind::Array)
                {
                    report("WIR2322", "Initial Typed WIR index access supports arrays only.", expression.Get());
                    return {};
                }
                const ValueId object = buildExpressionAs(access->object, objectTypeId, state);
                const ValueId index = buildAutoReadableExpression(access->index, state);
                if (!object || !index)
                    return {};
                return appendValue(Instruction{
                    .opcode = Opcode::ArrayGet,
                    .operands = {object, index},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* assignment = expression->as<AssignmentExpression>())
            {
                TypeId targetType = mapType(assignment->left->refType.Lock(), assignment->left.Get());
                const Type* targetTypeInfo = result_.module_.types.tryGet(targetType);
                const bool assignsThroughReadableReference = targetTypeInfo &&
                    targetTypeInfo->kind == TypeKind::Reference && autoReadableReference(*targetTypeInfo);
                const ValueId target = assignsThroughReadableReference
                    ? buildExpression(assignment->left, state)
                    : buildPlace(assignment->left, true, state);
                if (!target)
                    return {};
                if (assignsThroughReadableReference)
                    targetType = targetTypeInfo->arguments.front();
                const ValueId right = buildExpressionAs(assignment->right, targetType, state);
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
                    const ValueId currentValue = emitLoad(target, targetType, assignment->left.Get(), state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::Binary,
                        .result = assigned,
                        .resultType = targetType,
                        .operands = {currentValue, right},
                        .binaryOperator = *compoundOperator,
                        .source = SourceSpan::at(expression->location())
                    });
                }
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Store,
                    .operands = {target, assigned},
                    .source = SourceSpan::at(expression->location())
                });
                return assigned;
            }
            if (const auto* unary = expression->as<UnaryExpression>())
            {
                if (unary->op.type == TokenType::kwDeref)
                {
                    const ValueId place = buildExpression(unary->operand, state);
                    if (!place)
                        return {};
                    return emitLoad(place, mapType(expression->refType.Lock(), expression.Get()), expression.Get(), state);
                }
                const auto op = mapUnaryOperator(unary->op.type);
                const ValueId operand = buildAutoReadableExpression(unary->operand, state);
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
                const ValueId left = buildAutoReadableExpression(binary->left, state);
                const ValueId right = buildAutoReadableExpression(binary->right, state);
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
                    return buildConditionalControlFlow(*conditional, state);
                const ValueId condition = buildExpression(conditional->condition, state);
                const TypeId resultType = mapType(expression->refType.Lock(), expression.Get());
                const ValueId whenTrue = buildExpressionAs(conditional->whenTrue, resultType, state);
                const ValueId whenFalse = buildExpressionAs(conditional->whenFalse, resultType, state);
                if (!condition || !whenTrue || !whenFalse)
                    return {};
                return appendValue(Instruction{
                    .opcode = Opcode::Select,
                    .operands = {condition, whenTrue, whenFalse},
                    .source = SourceSpan::at(expression->location())
                });
            }
            if (const auto* match = expression->as<MatchExpression>())
                return buildMatchExpression(*match, state);
            if (const auto* call = expression->as<FunctionCallExpression>())
            {
                Ref<sema::Symbol> calleeSymbol = call->referencedSymbol.Lock();
                if (!calleeSymbol && call->callee)
                    calleeSymbol = call->callee->referencedSymbol.Lock();
                const TypeId callResultType = mapType(expression->refType.Lock(), expression.Get());
                const Type* callResultTypeInfo = result_.module_.types.tryGet(callResultType);
                const bool hasNominalConstructionResult = callResultTypeInfo && callResultTypeInfo->kind == TypeKind::Named &&
                    (callResultTypeInfo->nominalKind == NominalKind::Component ||
                     callResultTypeInfo->nominalKind == NominalKind::Object);
                const bool isConstructor = hasNominalConstructionResult && calleeSymbol &&
                    (calleeSymbol->name == "OnConstruct" || calleeSymbol->kind == sema::SymbolKind::Struct ||
                     calleeSymbol->kind == sema::SymbolKind::TypeAlias);
                if (isConstructor)
                {
                    const NominalKind nominalKind = callResultTypeInfo->nominalKind;
                    Ref<sema::Symbol> constructorSymbol = calleeSymbol;
                    Ref<sema::Type> constructorOwnerType = calleeSymbol->kind == sema::SymbolKind::TypeAlias
                        ? calleeSymbol->aliasTargetType
                        : calleeSymbol->type;
                    while (constructorOwnerType && constructorOwnerType->kind() == sema::TypeKind::Alias)
                        constructorOwnerType = constructorOwnerType.AsFast<sema::AliasType>()->aliasedType;
                    if (calleeSymbol->name != "OnConstruct" && constructorOwnerType &&
                        constructorOwnerType->kind() == sema::TypeKind::Struct)
                    {
                        const Ref<sema::Scope> ownerScope = constructorOwnerType.AsFast<sema::StructType>()->structScope.Lock();
                        if (ownerScope)
                            constructorSymbol = ownerScope->resolveLocally("OnConstruct");
                    }
                    const auto constructorType = constructorSymbol && constructorSymbol->type &&
                        constructorSymbol->type->kind() == sema::TypeKind::Function
                        ? constructorSymbol->type.AsFast<sema::FunctionType>()
                        : nullptr;
                    Instruction instruction{
                        .opcode = nominalKind == NominalKind::Object
                            ? Opcode::ConstructObject
                            : Opcode::ConstructComponent,
                        .selector = callResultTypeInfo->name + "::OnConstruct",
                        .source = SourceSpan::at(expression->location())
                    };
                    for (std::size_t index = 0; index < call->arguments.size(); ++index)
                    {
                        const auto& argument = call->arguments[index];
                        const TypeId expectedType = constructorType && index < constructorType->paramTypes.size()
                            ? mapType(constructorType->paramTypes[index], argument.Get())
                            : mapType(argument->refType.Lock(), argument.Get());
                        const ValueId value = buildExpressionAs(argument, expectedType, state);
                        if (!value)
                            return {};
                        instruction.operands.push_back(value);
                        instruction.signatureTypes.push_back(expectedType);
                    }
                    return appendValue(std::move(instruction));
                }

                if (call->callee)
                {
                    if (const Ref<sema::Symbol> directSymbol = call->callee->referencedSymbol.Lock())
                        calleeSymbol = directSymbol;
                }
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
                const auto functionType = calleeSymbol->type &&
                    calleeSymbol->type->kind() == sema::TypeKind::Function
                    ? calleeSymbol->type.AsFast<sema::FunctionType>()
                    : nullptr;
                for (std::size_t index = 0; index < call->arguments.size(); ++index)
                {
                    const auto& argument = call->arguments[index];
                    const TypeId expectedType = functionType && index < functionType->paramTypes.size()
                        ? mapType(functionType->paramTypes[index], argument.Get())
                        : mapType(argument->refType.Lock(), argument.Get());
                    const ValueId value = buildExpressionAs(argument, expectedType, state);
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

        ValueId buildPlace(
            const NodePtr<Expression>& expression,
            const bool needsMutable,
            FunctionState& state)
        {
            if (!expression)
                return {};

            if (const auto* identifier = expression->as<Identifier>())
            {
                const Ref<sema::Symbol> symbol = identifier->referencedSymbol.Lock();
                if (!symbol)
                {
                    report("WIR2326", "Addressable identifier is missing its semantic symbol.", expression.Get());
                    return {};
                }
                if (const auto place = state.places.find(symbol.Get()); place != state.places.end())
                {
                    return adaptPlaceMutability(
                        place->second,
                        referenceType(mapType(symbol->type, expression.Get()), symbol->flags.get_isMutable()),
                        needsMutable,
                        expression.Get(),
                        state);
                }
                if (const auto value = state.values.find(symbol.Get()); value != state.values.end())
                {
                    const TypeId valueType = mapType(symbol->type, expression.Get());
                    return adaptPlaceMutability(value->second, valueType, needsMutable, expression.Get(), state);
                }
                report("WIR2327", "Addressable identifier is not available in the current Typed WIR function.", expression.Get());
                return {};
            }

            if (const auto* unary = expression->as<UnaryExpression>();
                unary && unary->op.type == TokenType::kwDeref)
            {
                const ValueId place = buildExpression(unary->operand, state);
                const TypeId placeType = mapType(unary->operand->refType.Lock(), unary->operand.Get());
                return place ? adaptPlaceMutability(place, placeType, needsMutable, expression.Get(), state) : ValueId{};
            }

            if (const auto* access = expression->as<ArrayAccessExpression>())
            {
                if (access->operatorDispatchKind != OperatorDispatchKind::None)
                {
                    report("WIR2328", "Overloaded index places are not yet supported by Typed WIR.", expression.Get());
                    return {};
                }
                const ValueId base = buildPlace(access->object, needsMutable, state);
                const ValueId index = buildAutoReadableExpression(access->index, state);
                if (!base || !index)
                    return {};
                const ValueId result{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::ArrayPlace,
                    .result = result,
                    .resultType = referenceType(mapType(expression->refType.Lock(), expression.Get()), needsMutable),
                    .operands = {base, index},
                    .source = SourceSpan::at(expression->location())
                });
                return result;
            }

            if (const auto* member = expression->as<MemberAccessExpression>())
            {
                const Ref<sema::Symbol> memberSymbol = member->referencedSymbol.Lock();
                if (!memberSymbol || memberSymbol->kind != sema::SymbolKind::Variable)
                {
                    report("WIR2329", "Addressable member must resolve to a data field.", expression.Get());
                    return {};
                }
                const ValueId base = buildPlace(member->object, needsMutable, state);
                if (!base)
                    return {};
                const ValueId result{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::FieldPlace,
                    .result = result,
                    .resultType = referenceType(mapType(expression->refType.Lock(), expression.Get()), needsMutable),
                    .operands = {base},
                    .selector = memberSymbol->name,
                    .source = SourceSpan::at(expression->location())
                });
                return result;
            }

            if (const auto* call = expression->as<FunctionCallExpression>())
            {
                const TypeId callType = mapType(expression->refType.Lock(), expression.Get());
                const Type* callTypeInfo = result_.module_.types.tryGet(callType);
                if (callTypeInfo && callTypeInfo->kind == TypeKind::Reference)
                {
                    const ValueId place = buildExpression(expression, state);
                    return place ? adaptPlaceMutability(place, callType, needsMutable, expression.Get(), state) : ValueId{};
                }
            }

            report("WIR2330", "Expression kind '" + getKindNameStr(expression->kind()) + "' is not an addressable Typed WIR place.", expression.Get());
            return {};
        }

        ValueId buildAutoReadableExpression(
            const NodePtr<Expression>& expression,
            FunctionState& state)
        {
            ValueId value = buildExpression(expression, state);
            TypeId currentType = mapType(expression->refType.Lock(), expression.Get());
            const Type* type = result_.module_.types.tryGet(currentType);
            while (value && type && autoReadableReference(*type))
            {
                currentType = type->arguments.front();
                value = emitLoad(value, currentType, expression.Get(), state);
                type = result_.module_.types.tryGet(currentType);
            }
            return value;
        }

        ValueId buildExpressionAs(
            const NodePtr<Expression>& expression,
            const TypeId destinationType,
            FunctionState& state)
        {
            ValueId value = buildExpression(expression, state);
            if (!value)
                return {};
            TypeId sourceType = mapType(expression->refType.Lock(), expression.Get());
            if (sourceType == destinationType)
                return value;

            const Type* source = result_.module_.types.tryGet(sourceType);
            const Type* destination = result_.module_.types.tryGet(destinationType);
            if (source && destination && source->kind == TypeKind::Reference &&
                destination->kind == TypeKind::Reference && source->arguments == destination->arguments &&
                source->isMutable && !destination->isMutable)
            {
                const ValueId borrowed{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Borrow,
                    .result = borrowed,
                    .resultType = destinationType,
                    .operands = {value},
                    .source = SourceSpan::at(expression->location())
                });
                return borrowed;
            }
            while (source && autoReadableReference(*source) && sourceType != destinationType)
            {
                sourceType = source->arguments.front();
                value = emitLoad(value, sourceType, expression.Get(), state);
                source = result_.module_.types.tryGet(sourceType);
            }
            if (sourceType == destinationType)
                return value;

            if (!source || !destination ||
                !isSafeNumericWiden(source->kind, destination->kind))
            {
                report(
                    "WIR2314",
                    "Expression requires a conversion that is not a safe implicit numeric widening.",
                    expression.Get());
                return {};
            }

            const ValueId converted{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Convert,
                .result = converted,
                .resultType = destinationType,
                .operands = {value},
                .conversionKind = ConversionKind::NumericWiden,
                .source = SourceSpan::at(expression->location())
            });
            return converted;
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

        ValueId appendBinaryValue(
            const BinaryOperator op,
            const ValueId left,
            const ValueId right,
            const TypeId resultType,
            const ASTNode& source,
            FunctionState& state)
        {
            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::Binary,
                .result = result,
                .resultType = resultType,
                .operands = {left, right},
                .binaryOperator = op,
                .source = SourceSpan::at(source.location())
            });
            return result;
        }

        ValueId buildDestructuringMatchTest(
            const MatchCase& matchCase,
            const ValueId subject,
            const ASTNode& source,
            FunctionState& state)
        {
            if (matchCase.variantName == "__array")
            {
                const TypeId usizeType = result_.module_.types.intern(Type{.kind = TypeKind::USize});
                const ValueId length{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::ArrayLength,
                    .result = length,
                    .resultType = usizeType,
                    .operands = {subject},
                    .source = SourceSpan::at(source.location())
                });
                const ValueId expected{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = Opcode::Constant,
                    .result = expected,
                    .resultType = usizeType,
                    .literal = static_cast<std::uint64_t>(matchCase.bindings.size()),
                    .source = SourceSpan::at(source.location())
                });
                return appendBinaryValue(
                    BinaryOperator::Equal,
                    length,
                    expected,
                    result_.module_.types.boolType(),
                    source,
                    state);
            }

            if (matchCase.variantName != "Some" && matchCase.variantName != "None" &&
                matchCase.variantName != "Ok" && matchCase.variantName != "Err")
            {
                report(
                    "WIR2316",
                    "Typed WIR does not recognize destructuring variant '" + matchCase.variantName + "'.",
                    &source);
                return {};
            }

            const ValueId result{state.nextValue++};
            currentBlock(state).instructions.push_back(Instruction{
                .opcode = Opcode::VariantTest,
                .result = result,
                .resultType = result_.module_.types.boolType(),
                .operands = {subject},
                .selector = matchCase.variantName,
                .source = SourceSpan::at(source.location())
            });
            return result;
        }

        bool buildMatchBindings(
            const MatchCase& matchCase,
            const ValueId subject,
            FunctionState& state,
            std::unordered_map<const sema::Symbol*, ValueId>* captured = nullptr)
        {
            for (std::size_t index = 0; index < matchCase.bindings.size(); ++index)
            {
                const auto& binding = matchCase.bindings[index];
                const Ref<sema::Symbol> symbol = binding ? binding->referencedSymbol.Lock() : nullptr;
                if (!symbol)
                {
                    report("WIR2319", "Match binding is missing its semantic symbol.", binding.Get());
                    return false;
                }

                const TypeId bindingType = mapType(symbol->type, binding.Get());
                if (!bindingType)
                    return false;
                const ValueId value{state.nextValue++};
                currentBlock(state).instructions.push_back(Instruction{
                    .opcode = matchCase.variantName == "__array"
                        ? Opcode::ArrayElement
                        : Opcode::VariantPayload,
                    .result = value,
                    .resultType = bindingType,
                    .operands = {subject},
                    .selector = matchCase.variantName,
                    .projectionIndex = static_cast<std::uint32_t>(index),
                    .source = SourceSpan::at(binding->location())
                });
                state.values[symbol.Get()] = value;
                if (captured)
                    (*captured)[symbol.Get()] = value;
            }
            return true;
        }

        void buildMatchCaseTests(
            const MatchCase& matchCase,
            const ValueId subject,
            const TypeId subjectType,
            const BlockId successTarget,
            const BlockId failureTarget,
            FunctionState& state)
        {
            for (std::size_t index = 0; index < matchCase.matchValues.size(); ++index)
            {
                const auto& pattern = matchCase.matchValues[index];
                const bool hasNextPattern = index + 1 < matchCase.matchValues.size();
                const std::size_t nextPatternIndex = hasNextPattern
                    ? createBlock(state, "match.pattern.next", SourceSpan::at(pattern->location()))
                    : state.blockIndex;
                const BlockId patternFailure = hasNextPattern
                    ? currentBlockAt(state, nextPatternIndex).id
                    : failureTarget;

                if (const auto* range = pattern->as<RangeExpression>())
                {
                    const ValueId start = buildExpressionAs(range->start, subjectType, state);
                    if (!start)
                        return;
                    const ValueId lower = appendBinaryValue(
                        BinaryOperator::GreaterEqual,
                        subject,
                        start,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    const std::size_t upperBlockIndex = createBlock(
                        state, "match.range.upper", SourceSpan::at(range->end->location()));
                    const BlockId upperBlock = currentBlockAt(state, upperBlockIndex).id;
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {lower},
                        .targets = {upperBlock, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });

                    state.blockIndex = upperBlockIndex;
                    const ValueId end = buildExpressionAs(range->end, subjectType, state);
                    if (!end)
                        return;
                    const ValueId upper = appendBinaryValue(
                        range->isInclusive ? BinaryOperator::LessEqual : BinaryOperator::Less,
                        subject,
                        end,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {upper},
                        .targets = {successTarget, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });
                }
                else
                {
                    const ValueId expected = buildExpressionAs(pattern, subjectType, state);
                    if (!expected)
                        return;
                    const ValueId equal = appendBinaryValue(
                        BinaryOperator::Equal,
                        subject,
                        expected,
                        result_.module_.types.boolType(),
                        *pattern,
                        state);
                    currentBlock(state).instructions.push_back(Instruction{
                        .opcode = Opcode::CondBranch,
                        .operands = {equal},
                        .targets = {successTarget, patternFailure},
                        .source = SourceSpan::at(pattern->location())
                    });
                }

                if (hasNextPattern)
                    state.blockIndex = nextPatternIndex;
            }
        }

        ValueId buildMatchExpression(const MatchExpression& expression, FunctionState& state)
        {
            if (expression.cases.empty())
            {
                report("WIR2315", "Match expression requires at least one case.", &expression);
                return {};
            }
            const TypeId resultType = mapType(expression.refType.Lock(), &expression);
            const Type* resultTypeInfo = result_.module_.types.tryGet(resultType);
            if (!resultTypeInfo)
                return {};
            const bool producesValue = resultTypeInfo->kind != TypeKind::Void;
            if (producesValue)
            {
                for (const MatchCase& matchCase : expression.cases)
                {
                    if (!matchCase.body || !matchCase.body->is<ExpressionStatement>())
                    {
                        report("WIR2317", "Value-producing Typed WIR match cases require expression bodies.", &expression);
                        return {};
                    }
                }
            }

            const ValueId subject = buildExpression(expression.value, state);
            if (!subject)
                return {};
            const TypeId subjectType = mapType(expression.value->refType.Lock(), expression.value.Get());
            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t subjectBlockIndex = state.blockIndex;

            std::vector<std::size_t> caseEntryIndices;
            std::vector<std::size_t> caseBodyIndices;
            caseEntryIndices.reserve(expression.cases.size());
            caseBodyIndices.reserve(expression.cases.size());
            for (std::size_t index = 0; index < expression.cases.size(); ++index)
            {
                caseEntryIndices.push_back(createBlock(
                    state, "match.case." + std::to_string(index) + ".test", SourceSpan::at(expression.location())));
                caseBodyIndices.push_back(createBlock(
                    state, "match.case." + std::to_string(index) + ".body", SourceSpan::at(expression.cases[index].body->location())));
            }
            const std::size_t unmatchedBlockIndex = createBlock(
                state, "match.unmatched", SourceSpan::at(expression.location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, "match.merge", SourceSpan::at(expression.location()));
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;
            ValueId result;
            if (producesValue)
            {
                result = ValueId{state.nextValue++};
                currentBlockAt(state, mergeBlockIndex).parameters.push_back(Parameter{
                    .id = result,
                    .name = "match.result",
                    .type = resultType,
                    .source = SourceSpan::at(expression.location())
                });
            }
            const bool hasAssumed = std::ranges::any_of(
                expression.cases,
                [](const MatchCase& matchCase)
                {
                    return matchCase.matchValues.empty() && matchCase.variantName.empty();
                });
            const auto hasUnguardedVariant = [&](const std::string_view name)
            {
                return std::ranges::any_of(
                    expression.cases,
                    [&](const MatchCase& matchCase)
                    {
                        return matchCase.variantName == name && !matchCase.guard;
                    });
            };
            const bool hasExhaustiveDestructuring =
                (hasUnguardedVariant("Some") && hasUnguardedVariant("None")) ||
                (hasUnguardedVariant("Ok") && hasUnguardedVariant("Err"));
            const bool closesUnmatchedPath = hasAssumed || hasExhaustiveDestructuring;
            if (producesValue || closesUnmatchedPath)
            {
                currentBlockAt(state, unmatchedBlockIndex).instructions.push_back(Instruction{
                    .opcode = Opcode::Unreachable,
                    .source = SourceSpan::at(expression.location())
                });
            }
            currentBlockAt(state, subjectBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .targets = {currentBlockAt(state, caseEntryIndices.front()).id},
                .source = SourceSpan::at(expression.location())
            });

            std::vector<FunctionState> fallthroughStates;
            std::vector<ValueId> pathResults;
            for (std::size_t index = 0; index < expression.cases.size(); ++index)
            {
                const MatchCase& matchCase = expression.cases[index];
                const BlockId bodyBlock = currentBlockAt(state, caseBodyIndices[index]).id;
                const BlockId nextCase = index + 1 < expression.cases.size()
                    ? currentBlockAt(state, caseEntryIndices[index + 1]).id
                    : currentBlockAt(state, unmatchedBlockIndex).id;
                const bool destructuring = !matchCase.variantName.empty();
                const bool assumed = matchCase.matchValues.empty() && !destructuring;
                std::unordered_map<const sema::Symbol*, ValueId> caseBindings;

                FunctionState testState = state;
                testState.blockIndex = caseEntryIndices[index];
                testState.values = incomingValues;
                testState.valueOrder = incomingOrder;
                if (assumed)
                {
                    currentBlock(testState).instructions.push_back(Instruction{
                        .opcode = Opcode::Branch,
                        .targets = {bodyBlock},
                        .source = SourceSpan::at(matchCase.body->location())
                    });
                }
                else
                {
                    BlockId successTarget = bodyBlock;
                    std::optional<std::size_t> guardBlockIndex;
                    if (matchCase.guard)
                    {
                        guardBlockIndex = createBlock(
                            testState, "match.case." + std::to_string(index) + ".guard", SourceSpan::at(matchCase.guard->location()));
                        successTarget = currentBlockAt(testState, *guardBlockIndex).id;
                    }
                    if (destructuring)
                    {
                        const ValueId matched = buildDestructuringMatchTest(
                            matchCase, subject, *matchCase.body.Get(), testState);
                        if (!matched)
                            return {};
                        currentBlock(testState).instructions.push_back(Instruction{
                            .opcode = Opcode::CondBranch,
                            .operands = {matched},
                            .targets = {successTarget, nextCase},
                            .source = SourceSpan::at(matchCase.body->location())
                        });
                    }
                    else
                    {
                        buildMatchCaseTests(
                            matchCase, subject, subjectType, successTarget, nextCase, testState);
                    }
                    if (guardBlockIndex)
                    {
                        testState.blockIndex = *guardBlockIndex;
                        if (destructuring &&
                            !buildMatchBindings(matchCase, subject, testState, &caseBindings))
                        {
                            return {};
                        }
                        const ValueId guard = buildExpression(matchCase.guard, testState);
                        if (!guard)
                            return {};
                        currentBlock(testState).instructions.push_back(Instruction{
                            .opcode = Opcode::CondBranch,
                            .operands = {guard},
                            .targets = {bodyBlock, nextCase},
                            .source = SourceSpan::at(matchCase.guard->location())
                        });
                    }
                }
                state.nextValue = testState.nextValue;
                state.nextBlock = testState.nextBlock;

                FunctionState bodyState = state;
                bodyState.blockIndex = caseBodyIndices[index];
                bodyState.values = incomingValues;
                bodyState.valueOrder = incomingOrder;
                if (destructuring)
                {
                    if (matchCase.guard)
                    {
                        bodyState.values.insert(caseBindings.begin(), caseBindings.end());
                    }
                    else if (!buildMatchBindings(matchCase, subject, bodyState))
                    {
                        return {};
                    }
                }
                if (producesValue)
                {
                    const auto* expressionBody = matchCase.body->as<ExpressionStatement>();
                    const ValueId bodyValue = buildExpressionAs(expressionBody->expression, resultType, bodyState);
                    if (!bodyValue)
                        return {};
                    fallthroughStates.push_back(bodyState);
                    pathResults.push_back(bodyValue);
                }
                else
                {
                    buildStatement(matchCase.body, bodyState);
                    if (!blockIsTerminated(bodyState))
                        fallthroughStates.push_back(bodyState);
                }
                state.nextValue = bodyState.nextValue;
                state.nextBlock = bodyState.nextBlock;
            }

            if (!producesValue && !closesUnmatchedPath)
            {
                FunctionState unmatchedState = state;
                unmatchedState.blockIndex = unmatchedBlockIndex;
                unmatchedState.values = incomingValues;
                unmatchedState.valueOrder = incomingOrder;
                fallthroughStates.push_back(std::move(unmatchedState));
            }

            if (fallthroughStates.empty())
            {
                currentBlockAt(state, mergeBlockIndex).instructions.push_back(Instruction{
                    .opcode = Opcode::Unreachable,
                    .source = SourceSpan::at(expression.location())
                });
                state.blockIndex = mergeBlockIndex;
                state.values = incomingValues;
                state.valueOrder = incomingOrder;
                return {};
            }

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            auto mergedValues = incomingValues;
            std::vector<const sema::Symbol*> mergedSymbols;
            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const auto valueIn = [symbol, incoming](const FunctionState& path)
                {
                    const auto found = path.values.find(symbol);
                    return found != path.values.end() ? found->second : incoming;
                };
                const ValueId firstValue = valueIn(fallthroughStates.front());
                const bool differs = std::ranges::any_of(
                    fallthroughStates,
                    [&](const FunctionState& path) { return valueIn(path) != firstValue; });
                if (!differs)
                {
                    mergedValues[symbol] = firstValue;
                    continue;
                }

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".match",
                    .type = mapType(symbol->type, &expression),
                    .source = SourceSpan::at(expression.location())
                });
                mergedSymbols.push_back(symbol);
                mergedValues[symbol] = merged;
            }

            for (std::size_t index = 0; index < fallthroughStates.size(); ++index)
            {
                FunctionState& path = fallthroughStates[index];
                std::vector<ValueId> arguments;
                arguments.reserve(mergedSymbols.size() + (producesValue ? 1 : 0));
                if (producesValue)
                    arguments.push_back(pathResults[index]);
                for (const sema::Symbol* symbol : mergedSymbols)
                {
                    const auto found = path.values.find(symbol);
                    arguments.push_back(found != path.values.end()
                        ? found->second
                        : incomingValues.at(symbol));
                }
                currentBlock(path).instructions.push_back(Instruction{
                    .opcode = Opcode::Branch,
                    .operands = std::move(arguments),
                    .targets = {mergeBlock},
                    .source = SourceSpan::at(expression.location())
                });
            }

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
            return result;
        }

        ValueId buildConditionalControlFlow(
            const ConditionalExpression& expression,
            FunctionState& state)
        {
            const ValueId condition = buildExpression(expression.condition, state);
            if (!condition)
                return {};

            const auto incomingValues = state.values;
            const auto incomingOrder = state.valueOrder;
            const std::size_t conditionBlockIndex = state.blockIndex;
            const std::size_t trueBlockIndex = createBlock(
                state, "conditional.true", SourceSpan::at(expression.whenTrue->location()));
            const std::size_t falseBlockIndex = createBlock(
                state, "conditional.false", SourceSpan::at(expression.whenFalse->location()));
            const std::size_t mergeBlockIndex = createBlock(
                state, "conditional.merge", SourceSpan::at(expression.location()));
            const BlockId trueBlock = currentBlockAt(state, trueBlockIndex).id;
            const BlockId falseBlock = currentBlockAt(state, falseBlockIndex).id;
            const BlockId mergeBlock = currentBlockAt(state, mergeBlockIndex).id;

            currentBlockAt(state, conditionBlockIndex).instructions.push_back(Instruction{
                .opcode = Opcode::CondBranch,
                .operands = {condition},
                .targets = {trueBlock, falseBlock},
                .source = SourceSpan::at(expression.condition->location())
            });

            const TypeId resultType = mapType(expression.refType.Lock(), &expression);
            FunctionState trueState = state;
            trueState.blockIndex = trueBlockIndex;
            trueState.values = incomingValues;
            trueState.valueOrder = incomingOrder;
            const ValueId whenTrue = buildExpressionAs(expression.whenTrue, resultType, trueState);
            if (!whenTrue)
                return {};
            state.nextValue = trueState.nextValue;
            state.nextBlock = trueState.nextBlock;

            FunctionState falseState = state;
            falseState.blockIndex = falseBlockIndex;
            falseState.values = incomingValues;
            falseState.valueOrder = incomingOrder;
            const ValueId whenFalse = buildExpressionAs(expression.whenFalse, resultType, falseState);
            if (!whenFalse)
                return {};
            state.nextValue = falseState.nextValue;
            state.nextBlock = falseState.nextBlock;

            BasicBlock& merge = currentBlockAt(state, mergeBlockIndex);
            const ValueId result{state.nextValue++};
            merge.parameters.push_back(Parameter{
                .id = result,
                .name = "conditional.result",
                .type = resultType,
                .source = SourceSpan::at(expression.location())
            });
            std::vector<ValueId> trueArguments{whenTrue};
            std::vector<ValueId> falseArguments{whenFalse};
            auto mergedValues = incomingValues;

            for (const sema::Symbol* symbol : incomingOrder)
            {
                const ValueId incoming = incomingValues.at(symbol);
                const ValueId trueValue = trueState.values.contains(symbol)
                    ? trueState.values.at(symbol)
                    : incoming;
                const ValueId falseValue = falseState.values.contains(symbol)
                    ? falseState.values.at(symbol)
                    : incoming;
                if (trueValue == falseValue)
                    continue;

                const ValueId merged{state.nextValue++};
                merge.parameters.push_back(Parameter{
                    .id = merged,
                    .name = symbol->name + ".conditional",
                    .type = mapType(symbol->type, &expression),
                    .source = SourceSpan::at(expression.location())
                });
                trueArguments.push_back(trueValue);
                falseArguments.push_back(falseValue);
                mergedValues[symbol] = merged;
            }

            currentBlock(trueState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(trueArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.whenTrue->location())
            });
            currentBlock(falseState).instructions.push_back(Instruction{
                .opcode = Opcode::Branch,
                .operands = std::move(falseArguments),
                .targets = {mergeBlock},
                .source = SourceSpan::at(expression.whenFalse->location())
            });

            state.blockIndex = mergeBlockIndex;
            state.values = std::move(mergedValues);
            state.valueOrder = incomingOrder;
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
            emitDropsFrom(loop.placeDepth, state, statement);
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
                .carriedSymbols = carriedSymbols,
                .placeDepth = state.placeOrder.size()
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
            const std::size_t outerPlaceCount = state.placeOrder.size();
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
                .carriedSymbols = carriedSymbols,
                .placeDepth = state.placeOrder.size()
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
            emitDropsFrom(outerPlaceCount, state, &statement);
            while (state.placeOrder.size() > outerPlaceCount)
            {
                state.places.erase(state.placeOrder.back());
                state.placeOrder.pop_back();
            }
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

        static std::optional<IntegerType> mapIntegerType(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::I8: return IntegerType::i8;
            case TypeKind::I16: return IntegerType::i16;
            case TypeKind::I32: return IntegerType::i32;
            case TypeKind::I64: return IntegerType::i64;
            case TypeKind::ISize: return IntegerType::isize;
            case TypeKind::U8:
            case TypeKind::Byte: return IntegerType::u8;
            case TypeKind::U16: return IntegerType::u16;
            case TypeKind::U32: return IntegerType::u32;
            case TypeKind::U64: return IntegerType::u64;
            case TypeKind::USize: return IntegerType::usize;
            default: return std::nullopt;
            }
        }

        static std::optional<FloatType> mapFloatType(const TypeKind kind)
        {
            if (kind == TypeKind::F32)
                return FloatType::f32;
            if (kind == TypeKind::F64)
                return FloatType::f64;
            return std::nullopt;
        }

        static bool isNumericType(const TypeKind kind)
        {
            return mapIntegerType(kind).has_value() || mapFloatType(kind).has_value();
        }

        struct NumericTypeInfo
        {
            int bits;
            bool isSigned;
            bool isFloat;
        };

        static std::optional<NumericTypeInfo> numericTypeInfo(const TypeKind kind)
        {
            switch (kind)
            {
            case TypeKind::I8: return NumericTypeInfo{8, true, false};
            case TypeKind::I16: return NumericTypeInfo{16, true, false};
            case TypeKind::I32: return NumericTypeInfo{32, true, false};
            case TypeKind::I64:
            case TypeKind::ISize: return NumericTypeInfo{64, true, false};
            case TypeKind::U8: return NumericTypeInfo{8, false, false};
            case TypeKind::U16: return NumericTypeInfo{16, false, false};
            case TypeKind::U32: return NumericTypeInfo{32, false, false};
            case TypeKind::U64:
            case TypeKind::USize: return NumericTypeInfo{64, false, false};
            case TypeKind::F32: return NumericTypeInfo{32, true, true};
            case TypeKind::F64: return NumericTypeInfo{64, true, true};
            default: return std::nullopt;
            }
        }

        static bool isSafeNumericWiden(const TypeKind sourceKind, const TypeKind destinationKind)
        {
            if (sourceKind == destinationKind)
                return true;
            const auto source = numericTypeInfo(sourceKind);
            const auto destination = numericTypeInfo(destinationKind);
            if (!source || !destination)
                return false;
            if (destination->isFloat)
            {
                if (source->isFloat)
                    return destination->bits >= source->bits;
                const int exactIntegerBits = destination->bits == 32 ? 24 : 53;
                return source->bits - (source->isSigned ? 1 : 0) <= exactIntegerBits;
            }
            if (source->isFloat)
                return false;
            if (destination->isSigned == source->isSigned)
                return destination->bits >= source->bits;
            return destination->isSigned && !source->isSigned && destination->bits > source->bits;
        }

        static std::optional<Literal> integerLiteral(const IntegerResult& value)
        {
            if (!value.isValid)
                return std::nullopt;
            switch (value.type)
            {
            case IntegerType::i8: return Literal{static_cast<std::int64_t>(value.value.v_i8)};
            case IntegerType::i16: return Literal{static_cast<std::int64_t>(value.value.v_i16)};
            case IntegerType::i32: return Literal{static_cast<std::int64_t>(value.value.v_i32)};
            case IntegerType::i64: return Literal{static_cast<std::int64_t>(value.value.v_i64)};
            case IntegerType::isize: return Literal{static_cast<std::int64_t>(value.value.v_isize)};
            case IntegerType::u8: return Literal{static_cast<std::uint64_t>(value.value.v_u8)};
            case IntegerType::u16: return Literal{static_cast<std::uint64_t>(value.value.v_u16)};
            case IntegerType::u32: return Literal{static_cast<std::uint64_t>(value.value.v_u32)};
            case IntegerType::u64: return Literal{static_cast<std::uint64_t>(value.value.v_u64)};
            case IntegerType::usize: return Literal{static_cast<std::uint64_t>(value.value.v_usize)};
            case IntegerType::Unknown: return std::nullopt;
            }
            return std::nullopt;
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
