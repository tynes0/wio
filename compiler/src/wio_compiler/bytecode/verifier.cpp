#include "wio/bytecode/verifier.h"

#include <unordered_set>

namespace wio::bytecode
{
    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code, std::string message, const std::uint32_t function = InvalidIndex,
                          const std::uint32_t block = InvalidIndex, const std::uint32_t instruction = InvalidIndex)
        {
            result.diagnostics_.push_back(
                {std::move(code), std::move(message), function, block, instruction});
        };
        auto validString = [&](const StringId id) { return id == InvalidIndex || id < module.strings.size(); };
        auto validType = [&](const std::uint32_t id) { return id == InvalidIndex || id < module.types.size(); };

        if (!validString(module.name) || !validString(module.logicalName) || !validString(module.stableKey))
            report("WBC1001", "Module manifest references an invalid string id");
        for (std::size_t i = 0; i < module.constants.size(); ++i)
        {
            const Constant& constant = module.constants[i];
            if (constant.kind == ConstantKind::String && !validString(constant.string))
                report("WBC1002", "String constant references an invalid string id");
            if (constant.kind > ConstantKind::String)
                report("WBC1003", "Constant has an unknown kind");
        }
        for (std::size_t i = 0; i < module.types.size(); ++i)
        {
            const Type& type = module.types[i];
            if (!validString(type.name))
                report("WBC1004", "Type references an invalid name string");
            for (const std::uint32_t argument : type.arguments)
            {
                if (!validType(argument))
                    report("WBC1005", "Type argument references an invalid type id");
            }
            if (!validType(type.extentParameter))
                report("WBC1006", "Type extent parameter references an invalid type id");
        }

        std::unordered_set<std::uint32_t> globalIds;
        for (const Global& global : module.globals)
        {
            if (global.id == InvalidIndex || !globalIds.insert(global.id).second)
                report("WBC1007", "Global ids must be valid and unique");
            if (!validString(global.name) || !validType(global.type))
                report("WBC1008", "Global references invalid string or type metadata");
        }

        std::unordered_set<std::uint32_t> functionIds;
        for (const Function& function : module.functions)
        {
            if (function.id == InvalidIndex || !functionIds.insert(function.id).second)
                report("WBC1009", "Function ids must be valid and unique", function.id);
            if (!validString(function.name) || !validString(function.specializationKey) ||
                !validType(function.returnType) || !validType(function.callableType) || !validType(function.ownerType))
                report("WBC1010", "Function references invalid string or type metadata", function.id);

            std::unordered_set<std::uint32_t> blockIds;
            for (const Block& block : function.blocks)
            {
                if (block.id == InvalidIndex || !blockIds.insert(block.id).second)
                    report("WBC1011", "Block ids must be valid and unique within a function", function.id, block.id);
            }
            for (const Block& block : function.blocks)
            {
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    if (!isKnownOpcode(instruction.opcode))
                        report("WBC1012", "Instruction contains an unknown opcode", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (!validType(instruction.resultType) || !validType(instruction.targetType))
                        report("WBC1013", "Instruction references an invalid type id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (instruction.constant >= module.constants.size())
                        report("WBC1014", "Instruction references an invalid constant id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (!validString(instruction.selector) || !validString(instruction.specializationKey))
                        report("WBC1015", "Instruction references an invalid string id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    for (const StringId segment : instruction.stringSegments)
                    {
                        if (!validString(segment))
                            report("WBC1016", "Interpolation segment references an invalid string id", function.id,
                                   block.id, static_cast<std::uint32_t>(index));
                    }
                    for (const BranchTarget& target : instruction.targets)
                    {
                        if (!blockIds.contains(target.block))
                            report("WBC1017", "Branch target references a block outside the function", function.id,
                                   block.id, static_cast<std::uint32_t>(index));
                    }
                }
            }
        }
        return result;
    }
} // namespace wio::bytecode
