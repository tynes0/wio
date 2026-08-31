#include "wio/wir/lowered_ir_verifier.h"

#include <unordered_map>
#include <utility>

namespace wio::wir::lowered
{
    namespace
    {
        using FunctionMap = std::unordered_map<FunctionId::ValueType, const Function*>;
        using BlockMap = std::unordered_map<BlockId::ValueType, const BasicBlock*>;
        using ValueTypeMap = std::unordered_map<ValueId::ValueType, TypeId>;

        bool isComparison(const typed::BinaryOperator op)
        {
            return op == typed::BinaryOperator::Equal || op == typed::BinaryOperator::NotEqual ||
                   op == typed::BinaryOperator::Less || op == typed::BinaryOperator::LessEqual ||
                   op == typed::BinaryOperator::Greater || op == typed::BinaryOperator::GreaterEqual;
        }

        bool isNumeric(const TypeKind kind)
        {
            return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                   kind == TypeKind::I64 || kind == TypeKind::ISize ||
                   kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                   kind == TypeKind::U64 || kind == TypeKind::USize ||
                   kind == TypeKind::F32 || kind == TypeKind::F64;
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
            report("LIR1000", "Lowered WIR module name cannot be empty.");
        for (const Type& type : module.types.types())
        {
            if (type.kind == TypeKind::Invalid)
                report("LIR1001", "Lowered WIR type table contains an invalid type.");
            for (const TypeId argument : type.arguments)
            {
                if (!module.types.tryGet(argument))
                    report("LIR1002", "Lowered WIR type references an unknown type id.");
            }
        }

        FunctionMap functions;
        for (const Function& function : module.functions)
        {
            if (!function.id)
                report("LIR1100", "Lowered WIR function has an invalid id.", function.source);
            else if (!functions.emplace(function.id.value(), &function).second)
                report("LIR1101", "Lowered WIR function id is duplicated.", function.source, function.id);
            if (function.name.empty())
                report("LIR1102", "Lowered WIR function name cannot be empty.", function.source, function.id);
            if (!module.types.tryGet(function.returnType))
                report("LIR1103", "Lowered WIR function return type is invalid.", function.source, function.id);
        }

        for (const Function& function : module.functions)
        {
            ValueTypeMap values;
            auto defineValue = [&](const Parameter& parameter, const BlockId block)
            {
                if (!parameter.id)
                {
                    report("LIR1200", "Lowered WIR value has an invalid id.", parameter.source, function.id, block);
                    return;
                }
                if (!module.types.tryGet(parameter.type))
                    report("LIR1201", "Lowered WIR value has an invalid type.", parameter.source, function.id, block);
                if (!values.emplace(parameter.id.value(), parameter.type).second)
                    report("LIR1202", "Lowered WIR value id is defined more than once.", parameter.source, function.id, block);
            };
            for (const Parameter& parameter : function.parameters)
                defineValue(parameter, {});

            if (function.isExternal)
            {
                if (!function.blocks.empty())
                    report("LIR1104", "External Lowered WIR functions cannot contain blocks.", function.source, function.id);
                continue;
            }
            if (function.blocks.empty())
            {
                report("LIR1105", "Defined Lowered WIR functions require at least one block.", function.source, function.id);
                continue;
            }

            BlockMap blocks;
            for (const BasicBlock& block : function.blocks)
            {
                if (!block.id)
                    report("LIR1300", "Lowered WIR block has an invalid id.", block.source, function.id);
                else if (!blocks.emplace(block.id.value(), &block).second)
                    report("LIR1301", "Lowered WIR block id is duplicated.", block.source, function.id, block.id);
                for (const Parameter& parameter : block.parameters)
                    defineValue(parameter, block.id);
                for (const Instruction& instruction : block.instructions)
                {
                    if (instruction.result)
                    {
                        defineValue(Parameter{
                            .id = instruction.result,
                            .type = instruction.resultType,
                            .source = instruction.source
                        }, block.id);
                    }
                }
            }

            auto valueType = [&](const ValueId value) -> TypeId
            {
                if (!value)
                    return {};
                const auto found = values.find(value.value());
                return found == values.end() ? TypeId{} : found->second;
            };

            for (const BasicBlock& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    report("LIR1302", "Lowered WIR block requires a terminator.", block.source, function.id, block.id);
                    continue;
                }

                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    const bool terminator = isTerminator(instruction.opcode);
                    if (terminator != (index + 1 == block.instructions.size()))
                    {
                        report(
                            terminator ? "LIR1303" : "LIR1304",
                            terminator ? "Lowered WIR terminator must be the final instruction in its block."
                                       : "Lowered WIR block must end with a terminator.",
                            instruction.source,
                            function.id,
                            block.id);
                    }

                    for (const ValueId operand : instruction.operands)
                    {
                        if (!valueType(operand))
                            report("LIR1400", "Lowered WIR instruction references an unknown value.", instruction.source, function.id, block.id);
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
                        report("LIR1401", "Value-producing Lowered WIR instruction requires a typed result.", instruction.source, function.id, block.id);
                    if (!shouldHaveResult && instruction.opcode != Opcode::Call && instruction.result)
                        report("LIR1402", "Lowered WIR terminator cannot produce a value.", instruction.source, function.id, block.id);

                    if (instruction.opcode == Opcode::Unary)
                    {
                        if (instruction.operands.size() != 1 || valueType(instruction.operands.front()) != instruction.resultType)
                            report("LIR1403", "Lowered WIR unary operand must match its result type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Binary)
                    {
                        if (instruction.operands.size() != 2 || valueType(instruction.operands[0]) != valueType(instruction.operands[1]))
                            report("LIR1404", "Lowered WIR binary operands must have the same type.", instruction.source, function.id, block.id);
                        else if (isComparison(instruction.binaryOperator) && instruction.resultType != module.types.boolType())
                            report("LIR1405", "Lowered WIR comparison result must be bool.", instruction.source, function.id, block.id);
                        else if (!isComparison(instruction.binaryOperator) && instruction.resultType != valueType(instruction.operands[0]))
                            report("LIR1406", "Lowered WIR binary result must match its operand type.", instruction.source, function.id, block.id);
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
                            report("LIR1417", "Lowered WIR numeric conversion requires one numeric operand and a numeric result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Call)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        if (calleeIt == functions.end())
                        {
                            report("LIR1407", "Lowered WIR call references an unknown function.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            const Function& callee = *calleeIt->second;
                            if (callee.parameters.size() != instruction.operands.size())
                                report("LIR1408", "Lowered WIR call argument count does not match its callee.", instruction.source, function.id, block.id);
                            else
                            {
                                for (std::size_t argumentIndex = 0; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                {
                                    if (valueType(instruction.operands[argumentIndex]) != callee.parameters[argumentIndex].type)
                                        report("LIR1409", "Lowered WIR call argument type does not match its parameter.", instruction.source, function.id, block.id);
                                }
                            }
                            const Type* returnType = module.types.tryGet(callee.returnType);
                            const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                            if (!returnType || (returnsVoid ? instruction.result.isValid() : instruction.resultType != callee.returnType))
                                report("LIR1410", "Lowered WIR call result does not match its callee.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Return)
                    {
                        const Type* returnType = module.types.tryGet(function.returnType);
                        const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                        if (!returnType ||
                            (returnsVoid ? !instruction.operands.empty()
                                         : instruction.operands.size() != 1 || valueType(instruction.operands.front()) != function.returnType))
                        {
                            report("LIR1411", "Lowered WIR return value does not match its function.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::CondJump)
                    {
                        const std::size_t expectedTargets = instruction.opcode == Opcode::Jump ? 1 : 2;
                        const std::size_t expectedOperands = instruction.opcode == Opcode::Jump ? 0 : 1;
                        if (instruction.targets.size() != expectedTargets || instruction.operands.size() != expectedOperands)
                        {
                            report("LIR1412", "Lowered WIR jump shape is invalid.", instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::CondJump && !instruction.operands.empty() &&
                            valueType(instruction.operands.front()) != module.types.boolType())
                        {
                            report("LIR1413", "Lowered WIR conditional jump condition must be bool.", instruction.source, function.id, block.id);
                        }
                    }

                    for (const BranchTarget& target : instruction.targets)
                    {
                        const auto targetIt = target.block ? blocks.find(target.block.value()) : blocks.end();
                        if (targetIt == blocks.end())
                        {
                            report("LIR1414", "Lowered WIR jump references an unknown block.", instruction.source, function.id, block.id);
                            continue;
                        }
                        const BasicBlock& targetBlock = *targetIt->second;
                        if (target.arguments.size() != targetBlock.parameters.size())
                        {
                            report("LIR1415", "Lowered WIR jump argument count does not match target block parameters.", instruction.source, function.id, block.id);
                            continue;
                        }
                        for (std::size_t argumentIndex = 0; argumentIndex < target.arguments.size(); ++argumentIndex)
                        {
                            if (valueType(target.arguments[argumentIndex]) != targetBlock.parameters[argumentIndex].type)
                                report("LIR1416", "Lowered WIR jump argument type does not match target block parameter.", instruction.source, function.id, block.id);
                        }
                    }
                }
            }
        }
        return result;
    }
}
