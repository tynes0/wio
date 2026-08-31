#include "wio/wir/typed_ir_verifier.h"

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

        bool literalMatches(const Literal& literal, const TypeKind kind)
        {
            if (std::holds_alternative<NullLiteral>(literal))
                return kind == TypeKind::Nullable;
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
            if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                (type.kind != TypeKind::Named || type.nominalKind != NominalKind::Component))
            {
                report("WIR1008", "Native POD representation requires a named component type.");
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
            auto defineValue = [&](const Parameter& value,
                                   const BlockId block,
                                   const std::optional<std::size_t> instructionIndex = std::nullopt)
            {
                if (!value.id)
                {
                    report("WIR1200", "Typed WIR value has an invalid id.", value.source, function.id, block);
                    return;
                }
                if (!module.types.tryGet(value.type))
                    report("WIR1201", "Typed WIR value has an invalid type.", value.source, function.id, block);
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
                        .source = instruction.source
                    };
                    defineValue(resultValue, block.id, instructionIndex);
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
                    if (instruction.opcode == Opcode::Call && instruction.callee)
                    {
                        const auto calleeIt = functions.find(instruction.callee.value());
                        if (calleeIt != functions.end())
                        {
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                        }
                    }
                    const bool shouldHaveResult = producesValue(instruction.opcode) && !callReturnsVoid;
                    if (shouldHaveResult && (!instruction.result || !module.types.tryGet(instruction.resultType)))
                        report("WIR1401", "Value-producing Typed WIR instruction requires a typed result.", instruction.source, function.id, block.id);
                    if (!shouldHaveResult && instruction.opcode != Opcode::Call && instruction.result)
                        report("WIR1402", "Typed WIR terminator cannot produce a value.", instruction.source, function.id, block.id);

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
                            if (callee.parameters.size() != instruction.operands.size())
                            {
                                report("WIR1410", "Typed WIR call argument count does not match its callee.", instruction.source, function.id, block.id);
                            }
                            else
                            {
                                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                                {
                                    if (valueType(instruction.operands[index]) != callee.parameters[index].type)
                                        report("WIR1411", "Typed WIR call argument type does not match its parameter.", instruction.source, function.id, block.id);
                                }
                            }
                            const Type* calleeReturnType = module.types.tryGet(callee.returnType);
                            const bool calleeReturnsVoid = calleeReturnType && calleeReturnType->kind == TypeKind::Void;
                            if (!calleeReturnType ||
                                (calleeReturnsVoid ? instruction.result.isValid() : instruction.resultType != callee.returnType))
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
        }

        return result;
    }
}
