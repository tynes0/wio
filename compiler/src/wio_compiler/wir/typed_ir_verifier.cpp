#include "wio/wir/typed_ir_verifier.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <utility>

namespace wio::wir::typed
{
    namespace
    {
        using ValueTypeMap = std::unordered_map<ValueId::ValueType, TypeId>;
        using FunctionMap = std::unordered_map<FunctionId::ValueType, const Function*>;
        using BlockMap = std::unordered_map<BlockId::ValueType, const BasicBlock*>;

        struct ValueDefinition
        {
            BlockId block;
            std::optional<std::size_t> instructionIndex;
        };

        using ValueDefinitionMap = std::unordered_map<ValueId::ValueType, ValueDefinition>;
        using BlockSet = std::unordered_set<BlockId::ValueType>;

        bool isComparison(const BinaryOperator op)
        {
            return op == BinaryOperator::Equal || op == BinaryOperator::NotEqual ||
                   op == BinaryOperator::Less || op == BinaryOperator::LessEqual ||
                   op == BinaryOperator::Greater || op == BinaryOperator::GreaterEqual;
        }

        bool isNumeric(const TypeKind kind)
        {
            return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                   kind == TypeKind::I64 || kind == TypeKind::ISize ||
                   kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                   kind == TypeKind::U64 || kind == TypeKind::USize ||
                   kind == TypeKind::F32 || kind == TypeKind::F64;
        }

        bool isInteger(const TypeKind kind)
        {
            return isNumeric(kind) && kind != TypeKind::F32 && kind != TypeKind::F64;
        }

        ValueOwnership expectedOwnership(const Type& type)
        {
            if (type.ownership == OwnershipModel::Borrowed)
                return ValueOwnership::Borrowed;
            return requiresCleanup(type) ? ValueOwnership::Owned : ValueOwnership::Trivial;
        }

        bool literalMatches(const Literal& literal, const TypeKind kind)
        {
            if (std::holds_alternative<NullLiteral>(literal))
                return kind == TypeKind::Nullable || kind == TypeKind::Any;
            if (kind == TypeKind::Bool)
                return std::holds_alternative<bool>(literal);
            if (kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                kind == TypeKind::I64 || kind == TypeKind::ISize)
            {
                return std::holds_alternative<std::int64_t>(literal);
            }
            if (kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                kind == TypeKind::U64 || kind == TypeKind::USize || kind == TypeKind::Byte ||
                kind == TypeKind::Char)
            {
                return std::holds_alternative<std::uint64_t>(literal);
            }
            if (kind == TypeKind::F32 || kind == TypeKind::F64)
                return std::holds_alternative<double>(literal);
            if (kind == TypeKind::String || kind == TypeKind::Text)
                return std::holds_alternative<std::string>(literal);
            return false;
        }

        const FieldLayout* findFieldLayout(
            const TypeTable& types,
            const Type& owner,
            const std::string_view name,
            std::unordered_set<TypeId::ValueType>& visited)
        {
            const auto field = std::ranges::find_if(
                owner.fields,
                [&](const FieldLayout& layout) { return layout.name == name; });
            if (field != owner.fields.end())
                return &*field;
            for (const TypeId baseTypeId : owner.baseTypes)
            {
                if (!baseTypeId || !visited.insert(baseTypeId.value()).second)
                    continue;
                const Type* baseType = types.tryGet(baseTypeId);
                if (baseType)
                {
                    if (const FieldLayout* inherited = findFieldLayout(types, *baseType, name, visited))
                        return inherited;
                }
            }
            return nullptr;
        }

        const Type* underlyingObjectType(const TypeTable& types, TypeId typeId, TypeId* nominalId = nullptr)
        {
            const Type* type = types.tryGet(typeId);
            if (type && type->kind == TypeKind::Reference && type->arguments.size() == 1)
            {
                typeId = type->arguments.front();
                type = types.tryGet(typeId);
            }
            if (!type || type->kind != TypeKind::Named ||
                (type->nominalKind != NominalKind::Object && type->nominalKind != NominalKind::Interface))
                return nullptr;
            if (nominalId)
                *nominalId = typeId;
            return type;
        }

        bool nominalDerivesFrom(
            const TypeTable& types,
            const TypeId source,
            const TypeId destination,
            std::unordered_set<TypeId::ValueType>& visited)
        {
            if (source == destination)
                return true;
            if (!source || !visited.insert(source.value()).second)
                return false;
            const Type* type = types.tryGet(source);
            return type && std::ranges::any_of(type->baseTypes, [&](const TypeId base)
                { return nominalDerivesFrom(types, base, destination, visited); });
        }
    }

    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code,
                          std::string message,
                          const SourceSpan& source = {},
                          const FunctionId function = {},
                          const BlockId block = {})
        {
            result.diagnostics_.push_back(VerificationDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .source = source,
                .function = function,
                .block = block
            });
        };

        if (module.name.empty())
            report("WIR1000", "Typed WIR module name cannot be empty.");

        for (std::size_t typeIndex = 0; typeIndex < module.types.size(); ++typeIndex)
        {
            const TypeId typeId{static_cast<TypeId::ValueType>(typeIndex)};
            const Type& type = module.types.get(typeId);
            if (type.kind == TypeKind::Invalid)
                report("WIR1001", "Typed WIR type table contains an invalid type.");
            for (const TypeId argument : type.arguments)
            {
                if (!module.types.tryGet(argument))
                    report("WIR1002", "Typed WIR type references an unknown type id.");
            }
            if (type.kind == TypeKind::Named && type.name.empty())
                report("WIR1003", "Named Typed WIR types require a stable logical name.");
            if (type.kind == TypeKind::GenericParameter && type.name.empty())
                report("WIR1014", "Generic parameter Typed WIR types require a stable source name.");
            if ((type.kind == TypeKind::Reference || type.kind == TypeKind::Nullable ||
                 type.kind == TypeKind::Array || type.kind == TypeKind::AsyncTask) &&
                type.arguments.size() != 1)
            {
                report("WIR1004", "Unary constructed Typed WIR type requires exactly one type argument.");
            }
            if (type.kind == TypeKind::Dictionary && type.arguments.size() != 2)
                report("WIR1005", "Dictionary Typed WIR type requires key and value arguments.");
            if (type.staticExtent.has_value() && type.kind != TypeKind::Array)
                report("WIR1006", "Only array Typed WIR types may carry a static extent.");
            if (type.kind != TypeKind::Named && type.nominalKind != NominalKind::None)
                report("WIR1007", "Only named Typed WIR types may carry a nominal kind.");
            if (type.kind != TypeKind::Named && type.nominalValueModel != NominalValueModel::Regular)
                report("WIR1015", "Only named Typed WIR types may carry a nominal value model.");
            if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                (type.kind != TypeKind::Named || type.nominalKind != NominalKind::Component))
            {
                report("WIR1008", "Native POD representation requires a named component type.");
            }
            if (type.kind != TypeKind::Named &&
                (!type.baseTypes.empty() || !type.fields.empty() || !type.methods.empty() ||
                 type.hasConstructor || type.hasDestructor))
            {
                report("WIR1009", "Only named Typed WIR types may carry layout or lifecycle metadata.");
            }
            if ((type.ownership == OwnershipModel::Trivial && type.cleanup != CleanupKind::None) ||
                (type.ownership == OwnershipModel::Borrowed && type.cleanup != CleanupKind::None) ||
                (type.ownership == OwnershipModel::ReferenceCounted &&
                 type.cleanup != CleanupKind::ReleaseReference) ||
                (type.cleanup == CleanupKind::ReleaseReference &&
                 type.ownership != OwnershipModel::ReferenceCounted))
            {
                report("WIR1016", "Typed WIR ownership and cleanup metadata are inconsistent.");
            }
            std::unordered_set<std::string> fieldNames;
            for (const FieldLayout& field : type.fields)
            {
                if (field.name.empty() || !module.types.tryGet(field.type) || !fieldNames.insert(field.name).second)
                    report("WIR1010", "Typed WIR field layouts require unique names and valid field types.");
            }
            for (const TypeId baseTypeId : type.baseTypes)
            {
                const Type* baseType = module.types.tryGet(baseTypeId);
                if (!baseType || baseType->kind != TypeKind::Named)
                    report("WIR1011", "Typed WIR nominal base layouts must reference named types.");
            }
            std::unordered_set<std::uint32_t> methodSlots;
            for (const MethodLayout& method : type.methods)
            {
                const bool signatureValid = module.types.tryGet(method.returnType) &&
                    std::ranges::all_of(method.parameterTypes, [&](const TypeId parameter)
                        { return module.types.tryGet(parameter) != nullptr; });
                if (method.name.empty() || !method.function || !signatureValid ||
                    !methodSlots.insert(method.slot).second)
                {
                    report("WIR1012", "Typed WIR method layouts require names, functions, valid signatures, and unique slots.");
                }
            }
        }

        FunctionMap functions;
        for (const Function& function : module.functions)
        {
            if (!function.id)
            {
                report("WIR1100", "Typed WIR function has an invalid id.", function.source);
                continue;
            }
            if (!functions.emplace(function.id.value(), &function).second)
                report("WIR1101", "Typed WIR function id is duplicated.", function.source, function.id);
            if (function.name.empty())
                report("WIR1102", "Typed WIR function name cannot be empty.", function.source, function.id);
            if (!module.types.tryGet(function.returnType))
                report("WIR1103", "Typed WIR function return type is invalid.", function.source, function.id);
            const Type* callableType = module.types.tryGet(function.callableType);
            if (!callableType || callableType->kind != TypeKind::Function || callableType->arguments.empty() ||
                callableType->arguments.back() != function.returnType)
            {
                report("WIR1108", "Typed WIR function requires a callable type ending in its return type.", function.source, function.id);
            }
            else
            {
                const std::size_t hiddenParameterCount = function.captureParameterCount + (function.isMethod ? 1u : 0u);
                bool signatureMatches = function.parameters.size() >= hiddenParameterCount &&
                    callableType->arguments.size() == function.parameters.size() - hiddenParameterCount + 1;
                for (std::size_t index = hiddenParameterCount;
                     signatureMatches && index < function.parameters.size(); ++index)
                {
                    signatureMatches = function.parameters[index].type ==
                        callableType->arguments[index - hiddenParameterCount];
                }
                if (!signatureMatches)
                    report("WIR1112", "Typed WIR visible parameters must match the callable type after hidden receiver/capture parameters.", function.source, function.id);
            }
            if (!std::ranges::all_of(function.genericParameters, [&](const TypeId parameter)
                {
                    const Type* type = module.types.tryGet(parameter);
                    return type && type->kind == TypeKind::GenericParameter;
                }))
            {
                report("WIR1109", "Typed WIR generic parameter metadata must reference generic parameter types.", function.source, function.id);
            }
            if (function.captureParameterCount != function.captures.size() ||
                function.captureParameterCount > function.parameters.size() ||
                (!function.isClosureBody && !function.captures.empty()))
            {
                report("WIR1110", "Typed WIR closure capture layout and hidden parameter count are inconsistent.", function.source, function.id);
            }
            for (std::size_t captureIndex = 0; captureIndex < function.captures.size(); ++captureIndex)
            {
                const CaptureLayout& capture = function.captures[captureIndex];
                const Type* hiddenType = module.types.tryGet(function.parameters[captureIndex].type);
                const bool retainedSelf = capture.kind == CaptureKind::RetainedSelf;
                const bool validHiddenType = retainedSelf
                    ? function.parameters[captureIndex].type == capture.type
                    : hiddenType && hiddenType->kind == TypeKind::Reference && hiddenType->arguments.size() == 1 &&
                        hiddenType->arguments.front() == capture.type;
                if (capture.name.empty() || !module.types.tryGet(capture.type) || !validHiddenType)
                    report("WIR1111", "Typed WIR closure capture layout does not match its hidden parameter.", function.source, function.id);
            }
            if (function.isMethod)
            {
                const Type* owner = module.types.tryGet(function.ownerType);
                const Type* receiver = !function.parameters.empty()
                    ? module.types.tryGet(function.parameters.front().type)
                    : nullptr;
                if (!owner || owner->kind != TypeKind::Named || !receiver ||
                    receiver->kind != TypeKind::Reference || receiver->arguments.size() != 1 ||
                    receiver->arguments.front() != function.ownerType)
                {
                    report("WIR1106", "Typed WIR methods require a named owner and leading self reference parameter.", function.source, function.id);
                }
            }
            else if (function.ownerType)
            {
                report("WIR1107", "Non-method Typed WIR functions cannot carry an owner type.", function.source, function.id);
            }
        }


        for (const Type& type : module.types.types())
        {
            for (const MethodLayout& method : type.methods)
            {
                const auto function = functions.find(method.function.value());
                if (function == functions.end() || !function->second->isMethod)
                    report("WIR1013", "Typed WIR method layout references an unknown or non-method function.");
            }
        }

        for (const Function& function : module.functions)
        {
            if (function.isExternal)
            {
                if (!function.blocks.empty())
                    report("WIR1104", "External Typed WIR functions cannot contain blocks.", function.source, function.id);
                continue;
            }
            if (function.blocks.empty())
            {
                report("WIR1105", "Defined Typed WIR functions require at least one block.", function.source, function.id);
                continue;
            }

            BlockMap blocks;
            ValueTypeMap values;
            ValueDefinitionMap definitions;
            std::unordered_map<ValueId::ValueType, Opcode> producerOpcodes;
            auto defineValue = [&](const Parameter& value,
                                   const BlockId block,
                                   const std::optional<std::size_t> instructionIndex = std::nullopt,
                                   const bool enforceTypeOwnership = true)
            {
                if (!value.id)
                {
                    report("WIR1200", "Typed WIR value has an invalid id.", value.source, function.id, block);
                    return;
                }
                if (!module.types.tryGet(value.type))
                    report("WIR1201", "Typed WIR value has an invalid type.", value.source, function.id, block);
                else if (enforceTypeOwnership && value.ownership != expectedOwnership(module.types.get(value.type)))
                    report("WIR1204", "Typed WIR parameter ownership does not match its type contract.", value.source, function.id, block);
                if ((value.ownership == ValueOwnership::Borrowed) !=
                    (value.borrowLifetime != BorrowLifetime::None))
                {
                    report("WIR1205", "Borrowed Typed WIR values require lifetime metadata, and owned values cannot carry it.", value.source, function.id, block);
                }
                if (!values.emplace(value.id.value(), value.type).second)
                    report("WIR1202", "Typed WIR value id is defined more than once.", value.source, function.id, block);
                else
                    definitions.emplace(value.id.value(), ValueDefinition{block, instructionIndex});
            };

            for (const Parameter& parameter : function.parameters)
                defineValue(parameter, {});

            for (const BasicBlock& block : function.blocks)
            {
                if (!block.id)
                    report("WIR1300", "Typed WIR block has an invalid id.", block.source, function.id);
                else if (!blocks.emplace(block.id.value(), &block).second)
                    report("WIR1301", "Typed WIR block id is duplicated.", block.source, function.id, block.id);
                for (const Parameter& parameter : block.parameters)
                    defineValue(parameter, block.id);
                for (std::size_t instructionIndex = 0; instructionIndex < block.instructions.size(); ++instructionIndex)
                {
                    const Instruction& instruction = block.instructions[instructionIndex];
                    if (!instruction.result)
                        continue;
                    Parameter resultValue{
                        .id = instruction.result,
                        .type = instruction.resultType,
                        .ownership = instruction.resultOwnership,
                        .borrowLifetime = instruction.borrowLifetime,
                        .source = instruction.source
                    };
                    defineValue(resultValue, block.id, instructionIndex, false);
                    producerOpcodes.emplace(instruction.result.value(), instruction.opcode);
                }
            }

            auto valueType = [&](const ValueId id) -> TypeId
            {
                if (!id)
                    return {};
                const auto found = values.find(id.value());
                return found == values.end() ? TypeId{} : found->second;
            };

            std::unordered_map<BlockId::ValueType, std::vector<BlockId::ValueType>> predecessors;
            std::unordered_set<ValueId::ValueType> initializedPlaces;
            for (const BasicBlock& block : function.blocks)
                predecessors[block.id.value()];
            for (const BasicBlock& block : function.blocks)
            {
                if (block.instructions.empty())
                    continue;
                for (const BlockId target : block.instructions.back().targets)
                {
                    if (target && blocks.contains(target.value()))
                        predecessors[target.value()].push_back(block.id.value());
                }
            }

            BlockSet reachableBlocks;
            if (!function.blocks.empty() && function.blocks.front().id)
            {
                std::vector<BlockId::ValueType> pending{function.blocks.front().id.value()};
                while (!pending.empty())
                {
                    const BlockId::ValueType blockId = pending.back();
                    pending.pop_back();
                    if (!reachableBlocks.insert(blockId).second)
                        continue;
                    const BasicBlock* block = blocks.contains(blockId) ? blocks.at(blockId) : nullptr;
                    if (!block || block->instructions.empty())
                        continue;
                    for (const BlockId target : block->instructions.back().targets)
                    {
                        if (target && blocks.contains(target.value()))
                            pending.push_back(target.value());
                    }
                }
            }

            std::unordered_map<BlockId::ValueType, BlockSet> dominators;
            if (!function.blocks.empty() && function.blocks.front().id)
            {
                const BlockId::ValueType entryId = function.blocks.front().id.value();
                for (const BlockId::ValueType blockId : reachableBlocks)
                    dominators[blockId] = blockId == entryId ? BlockSet{entryId} : reachableBlocks;

                bool changed = true;
                while (changed)
                {
                    changed = false;
                    for (const BlockId::ValueType blockId : reachableBlocks)
                    {
                        if (blockId == entryId)
                            continue;

                        BlockSet intersection;
                        bool hasReachablePredecessor = false;
                        for (const BlockId::ValueType predecessor : predecessors[blockId])
                        {
                            if (!reachableBlocks.contains(predecessor))
                                continue;
                            if (!hasReachablePredecessor)
                            {
                                intersection = dominators[predecessor];
                                hasReachablePredecessor = true;
                                continue;
                            }
                            for (auto iterator = intersection.begin(); iterator != intersection.end();)
                            {
                                if (!dominators[predecessor].contains(*iterator))
                                    iterator = intersection.erase(iterator);
                                else
                                    ++iterator;
                            }
                        }
                        intersection.insert(blockId);
                        if (intersection != dominators[blockId])
                        {
                            dominators[blockId] = std::move(intersection);
                            changed = true;
                        }
                    }
                }
            }

            for (const BasicBlock& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    report("WIR1302", "Typed WIR block requires a terminator.", block.source, function.id, block.id);
                    continue;
                }

                for (std::size_t instructionIndex = 0; instructionIndex < block.instructions.size(); ++instructionIndex)
                {
                    const Instruction& instruction = block.instructions[instructionIndex];
                    const bool terminator = isTerminator(instruction.opcode);
                    if (terminator != (instructionIndex + 1 == block.instructions.size()))
                    {
                        report(
                            terminator ? "WIR1303" : "WIR1304",
                            terminator ? "Typed WIR terminator must be the final instruction in its block."
                                       : "Typed WIR block must end with a terminator.",
                            instruction.source,
                            function.id,
                            block.id);
                    }

                    for (const ValueId operand : instruction.operands)
                    {
                        if (!valueType(operand))
                        {
                            report("WIR1400", "Typed WIR instruction references an unknown value.", instruction.source, function.id, block.id);
                            continue;
                        }

                        const ValueDefinition& definition = definitions.at(operand.value());
                        if (!definition.block)
                            continue;
                        if (definition.block == block.id)
                        {
                            if (definition.instructionIndex.has_value() &&
                                *definition.instructionIndex >= instructionIndex)
                            {
                                report("WIR1420", "Typed WIR value is used before its definition in the same block.", instruction.source, function.id, block.id);
                            }
                            continue;
                        }
                        if (reachableBlocks.contains(block.id.value()) &&
                            (!dominators.contains(block.id.value()) ||
                             !dominators.at(block.id.value()).contains(definition.block.value())))
                        {
                            report("WIR1421", "Typed WIR value definition does not dominate its use.", instruction.source, function.id, block.id);
                        }
                    }

                    bool callReturnsVoid = false;
                    const bool isCallInstruction = instruction.opcode == Opcode::Call ||
                        instruction.opcode == Opcode::IndirectCall ||
                        instruction.opcode == Opcode::ExtensionCall ||
                        instruction.opcode == Opcode::MethodCall ||
                        instruction.opcode == Opcode::VirtualCall ||
                        instruction.opcode == Opcode::InterfaceCall ||
                        instruction.opcode == Opcode::IntrinsicCall;
                    if (isCallInstruction && instruction.callee)
                    {
                        const auto calleeIt = functions.find(instruction.callee.value());
                        if (calleeIt != functions.end())
                        {
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                        }
                    }
                    if (instruction.opcode == Opcode::IndirectCall && !instruction.operands.empty())
                    {
                        const Type* callable = module.types.tryGet(valueType(instruction.operands.front()));
                        const Type* returnType = callable && callable->kind == TypeKind::Function && !callable->arguments.empty()
                            ? module.types.tryGet(callable->arguments.back())
                            : nullptr;
                        callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                    }
                    if (instruction.opcode == Opcode::IntrinsicCall && !instruction.result)
                        callReturnsVoid = true;
                    const bool shouldHaveResult = producesValue(instruction.opcode) && !callReturnsVoid;
                    if (shouldHaveResult && (!instruction.result || !module.types.tryGet(instruction.resultType)))
                        report("WIR1401", "Value-producing Typed WIR instruction requires a typed result.", instruction.source, function.id, block.id);
                    if (!shouldHaveResult && !isCallInstruction && instruction.result)
                        report("WIR1402", "Typed WIR terminator cannot produce a value.", instruction.source, function.id, block.id);
                    if (!std::ranges::all_of(instruction.genericArguments, [&](const TypeId type)
                        { return module.types.tryGet(type) != nullptr; }))
                    {
                        report("WIR1444", "Typed WIR callable generic arguments must reference valid types.", instruction.source, function.id, block.id);
                    }

                    if (instruction.opcode == Opcode::Constant && module.types.tryGet(instruction.resultType) &&
                        !literalMatches(instruction.literal, module.types.get(instruction.resultType).kind))
                    {
                        report("WIR1403", "Typed WIR constant literal does not match its result type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Unary)
                    {
                        if (instruction.operands.size() != 1 || valueType(instruction.operands.front()) != instruction.resultType)
                            report("WIR1404", "Typed WIR unary instruction requires one operand matching its result type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Binary)
                    {
                        if (instruction.operands.size() != 2 ||
                            valueType(instruction.operands[0]) != valueType(instruction.operands[1]))
                        {
                            report("WIR1405", "Typed WIR binary operands must have the same type.", instruction.source, function.id, block.id);
                        }
                        else if (isComparison(instruction.binaryOperator))
                        {
                            if (instruction.resultType != module.types.boolType())
                                report("WIR1406", "Typed WIR comparison result must be bool.", instruction.source, function.id, block.id);
                        }
                        else if (instruction.resultType != valueType(instruction.operands[0]))
                        {
                            report("WIR1407", "Typed WIR binary result must match its operand type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Convert)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* destinationType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || !destinationType ||
                            !isNumeric(sourceType->kind) || !isNumeric(destinationType->kind))
                        {
                            report("WIR1422", "Typed WIR numeric conversion requires one numeric operand and a numeric result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantTest)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named ||
                            instruction.resultType != module.types.boolType() || instruction.selector.empty())
                        {
                            report("WIR1423", "Typed WIR variant test requires one named value, a variant selector, and a bool result.", instruction.source, function.id, block.id);
                        }
                        else if ((sourceType->nominalValueModel == NominalValueModel::Option &&
                                  instruction.selector != "Some" && instruction.selector != "None") ||
                                 (sourceType->nominalValueModel == NominalValueModel::Result &&
                                  instruction.selector != "Ok" && instruction.selector != "Err"))
                        {
                            report("WIR1455", "Typed WIR Option/Result variant test has an invalid variant selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantPayload)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named || instruction.selector.empty())
                        {
                            report("WIR1424", "Typed WIR variant payload requires one named value and a variant selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayLength)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Array ||
                            !resultType || resultType->kind != TypeKind::USize)
                        {
                            report("WIR1425", "Typed WIR array length requires one array value and a usize result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayElement)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Array || sourceType->arguments.size() != 1 ||
                            sourceType->arguments.front() != instruction.resultType)
                        {
                            report("WIR1426", "Typed WIR array element projection must return its array element type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayCreate)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        bool valid = resultType && resultType->kind == TypeKind::Array &&
                            resultType->arguments.size() == 1;
                        if (valid)
                        {
                            valid = std::ranges::all_of(
                                instruction.operands,
                                [&](const ValueId operand)
                                {
                                    return valueType(operand) == resultType->arguments.front();
                                });
                            valid = valid && (!resultType->staticExtent.has_value() ||
                                *resultType->staticExtent == instruction.operands.size());
                        }
                        if (!valid)
                            report("WIR1427", "Typed WIR array creation requires element operands matching its array type and extent.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ArrayGet)
                    {
                        const Type* arrayType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        if (!arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) ||
                            arrayType->arguments.front() != instruction.resultType)
                        {
                            report("WIR1428", "Typed WIR array get requires an array, an integer index, and its element result type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::DictionaryCreate)
                    {
                        const Type* dictionaryType = module.types.tryGet(instruction.resultType);
                        bool valid = dictionaryType && dictionaryType->kind == TypeKind::Dictionary &&
                            dictionaryType->arguments.size() == 2 && instruction.operands.size() % 2 == 0 &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            (instruction.selector == "ordered" || instruction.selector == "unordered");
                        for (std::size_t entryIndex = 0; valid && entryIndex < instruction.operands.size(); ++entryIndex)
                        {
                            const TypeId expected = dictionaryType->arguments[entryIndex % 2];
                            valid = valueType(instruction.operands[entryIndex]) == expected &&
                                instruction.signatureTypes[entryIndex] == expected;
                        }
                        if (!valid)
                            report("WIR1456", "Typed WIR dictionary creation requires alternating key/value operands matching its concrete dictionary type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::DictionaryGet)
                    {
                        const Type* dictionaryType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        if (!dictionaryType || dictionaryType->kind != TypeKind::Dictionary ||
                            dictionaryType->arguments.size() != 2 ||
                            valueType(instruction.operands[1]) != dictionaryType->arguments[0] ||
                            instruction.resultType != dictionaryType->arguments[1])
                        {
                            report("WIR1457", "Typed WIR dictionary get requires matching dictionary, key, and mapped result types.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Interpolate)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const bool familyMatches = resultType &&
                            ((resultType->kind == TypeKind::String && instruction.intrinsicFamily == IntrinsicFamily::String) ||
                             (resultType->kind == TypeKind::Text && instruction.intrinsicFamily == IntrinsicFamily::Text));
                        bool signaturesMatch = instruction.signatureTypes.size() == instruction.operands.size();
                        for (std::size_t operandIndex = 0;
                             signaturesMatch && operandIndex < instruction.operands.size(); ++operandIndex)
                        {
                            signaturesMatch = instruction.signatureTypes[operandIndex] ==
                                valueType(instruction.operands[operandIndex]);
                        }
                        if (!familyMatches || instruction.stringSegments.size() != instruction.operands.size() + 1 ||
                            !signaturesMatch)
                        {
                            report("WIR1458", "Typed WIR interpolation requires string/text identity, one more segment than values, and concrete value signatures.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::EnumConstant)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!instruction.operands.empty() || instruction.selector.empty() ||
                            instruction.targetType != instruction.resultType || !resultType ||
                            resultType->kind != TypeKind::Named ||
                            (resultType->nominalKind != NominalKind::Enum && resultType->nominalKind != NominalKind::Flagset))
                        {
                            report("WIR1459", "Typed WIR enum constant requires a named enum/flagset result and stable member selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::IntrinsicCall)
                    {
                        bool signaturesMatch = instruction.signatureTypes.size() == instruction.operands.size();
                        for (std::size_t operandIndex = 0;
                             signaturesMatch && operandIndex < instruction.operands.size(); ++operandIndex)
                        {
                            signaturesMatch = instruction.signatureTypes[operandIndex] ==
                                valueType(instruction.operands[operandIndex]);
                        }
                        if (instruction.intrinsicFamily == IntrinsicFamily::None || instruction.selector.empty() ||
                            instruction.operands.empty() || instruction.signatureTypes.size() != instruction.operands.size() ||
                            !module.types.tryGet(instruction.targetType) || !signaturesMatch)
                        {
                            report("WIR1460", "Typed WIR intrinsic call requires a family, selector, receiver, target type, and concrete operand signature.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::AnyBox)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (instruction.operands.size() != 1 || !resultType || resultType->kind != TypeKind::Any ||
                            instruction.targetType != valueType(instruction.operands.front()) ||
                            instruction.signatureTypes != std::vector<TypeId>{instruction.targetType})
                        {
                            report("WIR1461", "Typed WIR any box requires one operand and preserves its concrete source type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::AnyCheckedCast || instruction.opcode == Opcode::AnyTypeTest)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const bool validResult = instruction.opcode == Opcode::AnyTypeTest
                            ? instruction.resultType == module.types.boolType()
                            : instruction.resultType == instruction.targetType;
                        if (!sourceType || sourceType->kind != TypeKind::Any ||
                            !module.types.tryGet(instruction.targetType) || !validResult)
                        {
                            report("WIR1462", "Typed WIR any test/cast requires one any operand and a valid concrete target type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::NullableWrap)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (instruction.operands.size() != 1 || !resultType || resultType->kind != TypeKind::Nullable ||
                            resultType->arguments.size() != 1 || resultType->arguments.front() != valueType(instruction.operands.front()) ||
                            instruction.targetType != instruction.resultType)
                        {
                            report("WIR1463", "Typed WIR nullable wrap requires one value matching the nullable payload type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::LocalPlace)
                    {
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        if (!instruction.operands.empty() || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || instruction.selector.empty())
                        {
                            report("WIR1429", "Typed WIR local place requires a named reference result and no operands.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::PlaceInit || instruction.opcode == Opcode::Store ||
                             instruction.opcode == Opcode::Replace)
                    {
                        const Type* placeType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const bool mutabilityValid = instruction.opcode == Opcode::PlaceInit ||
                            (placeType && placeType->isMutable);
                        const Type* storedType = placeType && placeType->kind == TypeKind::Reference &&
                            placeType->arguments.size() == 1
                            ? module.types.tryGet(placeType->arguments.front())
                            : nullptr;
                        const bool ownershipOperationValid = instruction.opcode == Opcode::PlaceInit ||
                            (instruction.opcode == Opcode::Replace
                                ? storedType && requiresCleanup(*storedType)
                                : !storedType || !requiresCleanup(*storedType));
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            valueType(instruction.operands[1]) != placeType->arguments.front() || !mutabilityValid ||
                            !ownershipOperationValid)
                        {
                            report(
                                instruction.opcode == Opcode::PlaceInit ? "WIR1430" : "WIR1431",
                                instruction.opcode == Opcode::PlaceInit
                                    ? "Typed WIR place initialization requires a reference place and a matching value."
                                    : "Typed WIR store/replace requires a mutable reference place, matching value, and correct cleanup semantics.",
                                instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::PlaceInit && instruction.operands.size() == 2)
                        {
                            const ValueId place = instruction.operands.front();
                            const auto producer = producerOpcodes.find(place.value());
                            if (producer == producerOpcodes.end() || producer->second != Opcode::LocalPlace ||
                                !initializedPlaces.insert(place.value()).second)
                            {
                                report("WIR1436", "Typed WIR place initialization must target an uninitialized local place exactly once.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::Load)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.resultType != placeType->arguments.front())
                        {
                            report("WIR1432", "Typed WIR load requires a reference place and its referred result type.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            const Type& loaded = module.types.get(instruction.resultType);
                            const ValueOwnership expected = requiresCleanup(loaded)
                                ? ValueOwnership::Borrowed
                                : expectedOwnership(loaded);
                            if (instruction.resultOwnership != expected)
                                report("WIR1465", "Typed WIR load ownership must be borrowed for cleanup-bearing values.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::FieldPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* baseValueType = baseType;
                        if (baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1)
                            baseValueType = module.types.tryGet(baseType->arguments.front());
                        const bool strengthensMutability = baseType && baseType->kind == TypeKind::Reference &&
                            placeType && placeType->isMutable && !baseType->isMutable;
                        if (!baseValueType || baseValueType->kind != TypeKind::Named || !placeType ||
                            placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.selector.empty() || strengthensMutability)
                        {
                            report("WIR1433", "Typed WIR field place requires a named base, field selector, and non-strengthening reference result.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            std::unordered_set<TypeId::ValueType> visited;
                            const FieldLayout* field = findFieldLayout(module.types, *baseValueType, instruction.selector, visited);
                            if (!field || field->type != placeType->arguments.front() ||
                                (placeType->isMutable && !field->isMutable))
                            {
                                report("WIR1437", "Typed WIR field place must match the nominal field layout and field mutability.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* arrayType = baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1
                            ? module.types.tryGet(baseType->arguments.front())
                            : nullptr;
                        if (!baseType || !arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || placeType->arguments.front() != arrayType->arguments.front() ||
                            (placeType->isMutable && !baseType->isMutable))
                        {
                            report("WIR1434", "Typed WIR array place requires an array reference, integer index, and non-strengthening element reference.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::DictionaryPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* dictionaryType = baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1
                            ? module.types.tryGet(baseType->arguments.front())
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        if (!baseType || !dictionaryType || dictionaryType->kind != TypeKind::Dictionary ||
                            dictionaryType->arguments.size() != 2 || valueType(instruction.operands[1]) != dictionaryType->arguments[0] ||
                            !placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            placeType->arguments.front() != dictionaryType->arguments[1] ||
                            (placeType->isMutable && !baseType->isMutable))
                        {
                            report("WIR1464", "Typed WIR dictionary place requires a dictionary reference, matching key, and mapped-value reference.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Borrow)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Reference || sourceType->arguments.size() != 1 ||
                            !resultType || resultType->kind != TypeKind::Reference || resultType->arguments.size() != 1 ||
                            sourceType->arguments.front() != resultType->arguments.front() ||
                            (resultType->isMutable && !sourceType->isMutable))
                        {
                            report("WIR1435", "Typed WIR borrow cannot change the referred type or strengthen mutability.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ConstructComponent || instruction.opcode == Opcode::ConstructObject)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const NominalKind expectedKind = instruction.opcode == Opcode::ConstructObject
                            ? NominalKind::Object
                            : NominalKind::Component;
                        if (!resultType || resultType->kind != TypeKind::Named || resultType->nominalKind != expectedKind ||
                            !resultType->hasConstructor || instruction.selector.empty() ||
                            instruction.signatureTypes.size() != instruction.operands.size() ||
                            !std::ranges::all_of(instruction.signatureTypes, [&](const TypeId type) { return module.types.tryGet(type) != nullptr; }) ||
                            !std::ranges::equal(
                                instruction.signatureTypes,
                                instruction.operands,
                                {},
                                [](const TypeId type) { return type; },
                                [&](const ValueId value) { return valueType(value); }))
                        {
                            report("WIR1438", "Typed WIR construction requires a matching constructible component/object result and typed constructor signature.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Copy)
                    {
                        const Type* copiedType = module.types.tryGet(instruction.resultType);
                        if (instruction.operands.size() != 1 ||
                            valueType(instruction.operands.front()) != instruction.resultType ||
                            !copiedType || !requiresCleanup(*copiedType) ||
                            instruction.resultOwnership != ValueOwnership::Owned)
                        {
                            report("WIR1466", "Typed WIR copy must create one owned claim for a cleanup-bearing value.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Move)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* movedType = module.types.tryGet(instruction.resultType);
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            placeType->arguments.front() != instruction.resultType || !movedType ||
                            !requiresCleanup(*movedType) || instruction.resultOwnership != ValueOwnership::Owned)
                        {
                            report("WIR1467", "Typed WIR move must transfer a cleanup-bearing place into one owned value.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Release)
                    {
                        const Type* releasedType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!releasedType || !requiresCleanup(*releasedType))
                            report("WIR1468", "Typed WIR release requires one cleanup-bearing owned value.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Drop)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* valueTypeInfo = placeType && placeType->kind == TypeKind::Reference && placeType->arguments.size() == 1
                            ? module.types.tryGet(placeType->arguments.front())
                            : nullptr;
                        if (!valueTypeInfo || !requiresCleanup(*valueTypeInfo))
                        {
                            report("WIR1439", "Typed WIR drop requires a place containing a cleanup-bearing value.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Select)
                    {
                        if (instruction.operands.size() != 3 ||
                            valueType(instruction.operands[0]) != module.types.boolType() ||
                            valueType(instruction.operands[1]) != instruction.resultType ||
                            valueType(instruction.operands[2]) != instruction.resultType)
                        {
                            report("WIR1408", "Typed WIR select requires bool condition and matching result arms.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::FunctionReference)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const bool valid = calleeIt != functions.end() && resultType &&
                            resultType->kind == TypeKind::Function && instruction.operands.empty() &&
                            (calleeIt->second->genericParameters.empty()
                                ? instruction.resultType == calleeIt->second->callableType
                                : !instruction.specializationKey.empty());
                        if (!valid)
                            report("WIR1445", "Typed WIR function reference must pin a known callable declaration and callable type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ClosureCreate)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        bool valid = calleeIt != functions.end() && calleeIt->second->isClosureBody &&
                            resultType && resultType->kind == TypeKind::Function &&
                            instruction.resultType == calleeIt->second->callableType &&
                            instruction.operands.size() == instruction.signatureTypes.size() &&
                            instruction.operands.size() == instruction.captureKinds.size() &&
                            instruction.operands.size() == calleeIt->second->captures.size();
                        if (valid)
                        {
                            for (std::size_t captureIndex = 0; captureIndex < instruction.operands.size(); ++captureIndex)
                            {
                                valid = valid && valueType(instruction.operands[captureIndex]) == instruction.signatureTypes[captureIndex] &&
                                    instruction.signatureTypes[captureIndex] == calleeIt->second->captures[captureIndex].type &&
                                    instruction.captureKinds[captureIndex] == calleeIt->second->captures[captureIndex].kind;
                            }
                        }
                        if (!valid)
                            report("WIR1446", "Typed WIR closure creation must match its closure body capture layout exactly.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::IndirectCall)
                    {
                        const Type* callable = !instruction.operands.empty()
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        bool valid = callable && callable->kind == TypeKind::Function && !callable->arguments.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            instruction.signatureTypes.front() == valueType(instruction.operands.front()) &&
                            callable->arguments.size() == instruction.operands.size();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                            {
                                valid = valid && valueType(instruction.operands[argumentIndex]) == instruction.signatureTypes[argumentIndex] &&
                                    instruction.signatureTypes[argumentIndex] == callable->arguments[argumentIndex - 1];
                            }
                            const TypeId returnType = callable->arguments.back();
                            const Type* returnTypeInfo = module.types.tryGet(returnType);
                            valid = valid && returnTypeInfo &&
                                (returnTypeInfo->kind == TypeKind::Void ? !instruction.result : instruction.resultType == returnType);
                        }
                        if (!valid)
                            report("WIR1447", "Typed WIR indirect call must match its function value signature.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ExtensionCall)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* target = module.types.tryGet(instruction.targetType);
                        bool valid = calleeIt != functions.end() && calleeIt->second->isExtension && target &&
                            target->kind == TypeKind::Named && !instruction.operands.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            instruction.selector.size() > 0 && !instruction.specializationKey.empty();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                valid = valid && valueType(instruction.operands[argumentIndex]) == instruction.signatureTypes[argumentIndex];
                            const Type* receiverSignature = module.types.tryGet(instruction.signatureTypes.front());
                            const TypeId receiverValue = valueType(instruction.operands.front());
                            valid = valid && (receiverValue == instruction.signatureTypes.front() ||
                                (receiverSignature && receiverSignature->kind == TypeKind::Reference &&
                                 receiverSignature->arguments.size() == 1 && receiverSignature->arguments.front() == receiverValue));
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            valid = valid && returnType &&
                                (returnType->kind == TypeKind::Void
                                    ? !instruction.result
                                    : calleeIt->second->genericParameters.empty()
                                        ? instruction.resultType == calleeIt->second->returnType
                                        : module.types.tryGet(instruction.resultType) && !instruction.specializationKey.empty());
                        }
                        if (!valid)
                            report("WIR1448", "Typed WIR extension call must pin its implementation, receiver, signature, and specialization.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::MethodCall ||
                             instruction.opcode == Opcode::VirtualCall ||
                             instruction.opcode == Opcode::InterfaceCall)
                    {
                        const Type* owner = module.types.tryGet(instruction.targetType);
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const auto method = owner ? std::ranges::find_if(owner->methods, [&](const MethodLayout& layout)
                        {
                            return layout.slot == instruction.projectionIndex &&
                                layout.name == instruction.selector && layout.function == instruction.callee;
                        }) : std::vector<MethodLayout>::const_iterator{};
                        const bool dispatchKindValid = owner && owner->kind == TypeKind::Named &&
                            (instruction.opcode != Opcode::VirtualCall || owner->nominalKind == NominalKind::Object) &&
                            (instruction.opcode != Opcode::InterfaceCall || owner->nominalKind == NominalKind::Interface);
                        bool valid = dispatchKindValid && calleeIt != functions.end() &&
                            method != owner->methods.end() && !instruction.operands.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                            {
                                valid = valid && valueType(instruction.operands[argumentIndex]) ==
                                    instruction.signatureTypes[argumentIndex];
                            }
                            TypeId receiverNominal;
                            std::unordered_set<TypeId::ValueType> visited;
                            valid = valid && underlyingObjectType(module.types, valueType(instruction.operands.front()), &receiverNominal) &&
                                nominalDerivesFrom(module.types, receiverNominal, instruction.targetType, visited);
                            const Function& callee = *calleeIt->second;
                            const Type* returnType = module.types.tryGet(callee.returnType);
                            const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                            valid = valid && returnType &&
                                (returnsVoid
                                    ? !instruction.result
                                    : callee.genericParameters.empty()
                                        ? instruction.resultType == callee.returnType
                                        : module.types.tryGet(instruction.resultType) && !instruction.specializationKey.empty());
                        }
                        if (!valid)
                            report("WIR1440", "Typed WIR method dispatch must match its owner slot, receiver, signature, and callee.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Upcast || instruction.opcode == Opcode::CheckedCast)
                    {
                        TypeId sourceNominal;
                        TypeId targetNominal;
                        const bool hasObjectTypes = instruction.operands.size() == 1 &&
                            underlyingObjectType(module.types, valueType(instruction.operands.front()), &sourceNominal) &&
                            underlyingObjectType(module.types, instruction.targetType, &targetNominal);
                        bool valid = hasObjectTypes && instruction.resultType == instruction.targetType;
                        if (valid && instruction.opcode == Opcode::Upcast)
                        {
                            std::unordered_set<TypeId::ValueType> visited;
                            valid = nominalDerivesFrom(module.types, sourceNominal, targetNominal, visited);
                        }
                        if (!valid)
                            report("WIR1441", "Typed WIR object cast requires compatible object/interface source and target types.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::TypeTest)
                    {
                        if (instruction.operands.size() != 1 ||
                            !underlyingObjectType(module.types, valueType(instruction.operands.front())) ||
                            !underlyingObjectType(module.types, instruction.targetType) ||
                            instruction.resultType != module.types.boolType())
                        {
                            report("WIR1442", "Typed WIR type test requires an object/interface value, target type, and bool result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::IdentityEqual)
                    {
                        if (instruction.operands.size() != 2 ||
                            !underlyingObjectType(module.types, valueType(instruction.operands[0])) ||
                            !underlyingObjectType(module.types, valueType(instruction.operands[1])) ||
                            instruction.resultType != module.types.boolType() ||
                            (instruction.binaryOperator != BinaryOperator::Equal &&
                             instruction.binaryOperator != BinaryOperator::NotEqual))
                        {
                            report("WIR1443", "Typed WIR identity equality requires two object/interface values and a bool result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Call)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        if (calleeIt == functions.end())
                        {
                            report("WIR1409", "Typed WIR call references an unknown function.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            const Function& callee = *calleeIt->second;
                            const bool concreteSignature = instruction.signatureTypes.size() == instruction.operands.size() &&
                                std::ranges::all_of(instruction.signatureTypes, [&](const TypeId type)
                                    { return module.types.tryGet(type) != nullptr; });
                            if (callee.parameters.size() != instruction.operands.size() || !concreteSignature)
                            {
                                report("WIR1410", "Typed WIR call argument count does not match its callee.", instruction.source, function.id, block.id);
                            }
                            else
                            {
                                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                                {
                                    if (valueType(instruction.operands[index]) != instruction.signatureTypes[index] ||
                                        (callee.genericParameters.empty() && instruction.signatureTypes[index] != callee.parameters[index].type))
                                        report("WIR1411", "Typed WIR call argument type does not match its parameter.", instruction.source, function.id, block.id);
                                }
                            }
                            const Type* calleeReturnType = module.types.tryGet(callee.returnType);
                            const bool calleeReturnsVoid = calleeReturnType && calleeReturnType->kind == TypeKind::Void;
                            const bool resultMatches = callee.genericParameters.empty()
                                ? instruction.resultType == callee.returnType
                                : module.types.tryGet(instruction.resultType) != nullptr && !instruction.specializationKey.empty();
                            if (!calleeReturnType ||
                                (calleeReturnsVoid ? instruction.result.isValid() : !resultMatches))
                                report("WIR1412", "Typed WIR call result does not match its callee return type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Return)
                    {
                        const bool returnsVoid = module.types.tryGet(function.returnType) &&
                            module.types.get(function.returnType).kind == TypeKind::Void;
                        if (returnsVoid ? !instruction.operands.empty()
                                        : instruction.operands.size() != 1 || valueType(instruction.operands.front()) != function.returnType)
                        {
                            report("WIR1413", "Typed WIR return value does not match its function return type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Branch)
                    {
                        if (instruction.targets.size() != 1)
                            report("WIR1414", "Typed WIR branch requires exactly one target.", instruction.source, function.id, block.id);
                        else if (instruction.targets.front() && blocks.contains(instruction.targets.front().value()))
                        {
                            const BasicBlock& target = *blocks.at(instruction.targets.front().value());
                            if (instruction.operands.size() != target.parameters.size())
                            {
                                report("WIR1417", "Typed WIR branch argument count does not match its target block.", instruction.source, function.id, block.id);
                            }
                            else
                            {
                                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                                {
                                    if (valueType(instruction.operands[index]) != target.parameters[index].type)
                                        report("WIR1418", "Typed WIR branch argument type does not match its target block parameter.", instruction.source, function.id, block.id);
                                }
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::CondBranch)
                    {
                        if (instruction.targets.size() != 2 || instruction.operands.size() != 1 ||
                            valueType(instruction.operands.front()) != module.types.boolType())
                        {
                            report("WIR1415", "Typed WIR conditional branch requires a bool condition and two targets.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            for (const BlockId targetId : instruction.targets)
                            {
                                if (targetId && blocks.contains(targetId.value()) &&
                                    !blocks.at(targetId.value())->parameters.empty())
                                {
                                    report("WIR1419", "Typed WIR conditional branch targets cannot require block arguments.", instruction.source, function.id, block.id);
                                }
                            }
                        }
                    }

                    for (const BlockId target : instruction.targets)
                    {
                        if (!target || !blocks.contains(target.value()))
                            report("WIR1416", "Typed WIR instruction references an unknown block.", instruction.source, function.id, block.id);
                    }
                }
            }

            // Verify cleanup as a control-flow property, not just an opcode
            // shape. Every reachable edge must agree on which cleanup-bearing
            // local places are live, and every exit must consume them once.
            using LivePlaces = std::unordered_set<ValueId::ValueType>;
            std::unordered_set<ValueId::ValueType> cleanupPlaces;
            for (const BasicBlock& block : function.blocks)
            {
                for (const Instruction& instruction : block.instructions)
                {
                    if (instruction.opcode != Opcode::LocalPlace || !instruction.result)
                        continue;
                    const Type* placeType = module.types.tryGet(instruction.resultType);
                    const Type* storedType = placeType && placeType->kind == TypeKind::Reference &&
                        placeType->arguments.size() == 1
                        ? module.types.tryGet(placeType->arguments.front())
                        : nullptr;
                    if (storedType && requiresCleanup(*storedType))
                        cleanupPlaces.insert(instruction.result.value());
                }
            }

            if (!function.blocks.empty() && function.blocks.front().id)
            {
                std::unordered_map<BlockId::ValueType, LivePlaces> entryStates;
                std::vector<BlockId::ValueType> pending{function.blocks.front().id.value()};
                entryStates.emplace(function.blocks.front().id.value(), LivePlaces{});
                std::unordered_set<BlockId::ValueType> mismatchReported;
                while (!pending.empty())
                {
                    const BlockId::ValueType blockId = pending.back();
                    pending.pop_back();
                    const BasicBlock* block = blocks.contains(blockId) ? blocks.at(blockId) : nullptr;
                    if (!block)
                        continue;
                    LivePlaces live = entryStates.at(blockId);
                    for (const Instruction& instruction : block->instructions)
                    {
                        if (instruction.opcode == Opcode::PlaceInit && !instruction.operands.empty() &&
                            cleanupPlaces.contains(instruction.operands.front().value()))
                        {
                            if (!live.insert(instruction.operands.front().value()).second)
                                report("WIR1469", "Cleanup-bearing place is initialized while already live.", instruction.source, function.id, block->id);
                        }
                        else if ((instruction.opcode == Opcode::Drop || instruction.opcode == Opcode::Move) &&
                                 !instruction.operands.empty() &&
                                 cleanupPlaces.contains(instruction.operands.front().value()))
                        {
                            if (live.erase(instruction.operands.front().value()) != 1)
                                report("WIR1470", "Cleanup-bearing place is moved or dropped more than once.", instruction.source, function.id, block->id);
                        }
                        else if (instruction.opcode == Opcode::Replace && !instruction.operands.empty() &&
                                 cleanupPlaces.contains(instruction.operands.front().value()) &&
                                 !live.contains(instruction.operands.front().value()))
                        {
                            report("WIR1471", "Replace requires an initialized cleanup-bearing place.", instruction.source, function.id, block->id);
                        }
                        if (instruction.opcode == Opcode::Return && !live.empty())
                            report("WIR1472", "Function exit leaves cleanup-bearing local places live.", instruction.source, function.id, block->id);
                    }

                    if (block->instructions.empty())
                        continue;
                    for (const BlockId successor : block->instructions.back().targets)
                    {
                        if (!successor || !blocks.contains(successor.value()))
                            continue;
                        const auto [found, inserted] = entryStates.emplace(successor.value(), live);
                        if (inserted)
                            pending.push_back(successor.value());
                        else if (found->second != live && mismatchReported.insert(successor.value()).second)
                            report("WIR1473", "Control-flow merge disagrees on live cleanup-bearing places.", blocks.at(successor.value())->source, function.id, successor);
                    }
                }
            }
        }

        return result;
    }
}
