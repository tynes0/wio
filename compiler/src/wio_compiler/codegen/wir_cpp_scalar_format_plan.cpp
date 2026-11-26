#include "wio/codegen/wir_cpp_scalar_format_plan.h"

#include <algorithm>

namespace wio::codegen
{
    WirCppScalarFormatPlan WirCppScalarFormatPlan::build(const wir::lowered::Module& module,
                                                         const wir::lowered::Function& function)
    {
        using namespace wir;
        using Opcode = lowered::Opcode;
        WirCppScalarFormatPlan plan;
        if (function.isAsync || function.coroutine)
            return plan;
        if (std::ranges::none_of(function.blocks,
                                 [](const auto& block)
                                 {
                                     return std::ranges::any_of(block.instructions,
                                                                [](const auto& instruction)
                                                                {
                                                                    return instruction.opcode == Opcode::Interpolate &&
                                                                           instruction.operands.size() == 1;
                                                                });
                                 }))
            return plan;
        struct Candidate
        {
            Storage storage;
            std::size_t consumers = 0, consumerBlock = 0, consumerIndex = 0;
            std::vector<std::pair<std::size_t, std::size_t>> cleanup{};
            bool safe = true;
        };
        std::unordered_map<std::uint32_t, Candidate> candidates;
        std::unordered_map<std::uint32_t, TypeId> types;
        for (const auto& parameter : function.parameters)
            types.emplace(parameter.id.value(), parameter.type);
        for (const auto& block : function.blocks)
        {
            for (const auto& parameter : block.parameters)
                types.emplace(parameter.id.value(), parameter.type);
            for (const auto& instruction : block.instructions)
                if (instruction.result)
                    types.emplace(instruction.result.value(), instruction.resultType);
        }
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
            {
                if (instruction.opcode != Opcode::Interpolate || !instruction.result ||
                    instruction.resultType != module.types.stringType() ||
                    instruction.resultOwnership != typed::ValueOwnership::Owned || instruction.borrowOrigin ||
                    !instruction.targets.empty() || instruction.operands.size() != 1 ||
                    instruction.signatureTypes.size() != 1 || instruction.stringSegments.size() != 2 ||
                    !instruction.stringSegments[0].empty() || !instruction.stringSegments[1].empty())
                    continue;
                const auto found = types.find(instruction.operands[0].value());
                const auto* type = found == types.end() ? nullptr : module.types.tryGet(found->second);
                if (!type || found->second != instruction.signatureTypes[0])
                    continue;
                if (type->kind == TypeKind::Bool)
                    candidates.emplace(instruction.result.value(), Candidate{Storage::BooleanView});
                else if (type->kind == TypeKind::I16 || type->kind == TypeKind::I32 || type->kind == TypeKind::I64 ||
                         type->kind == TypeKind::ISize || type->kind == TypeKind::U16 || type->kind == TypeKind::U32 ||
                         type->kind == TypeKind::U64 || type->kind == TypeKind::USize)
                    candidates.emplace(instruction.result.value(), Candidate{Storage::IntegerBytes});
            }
        if (candidates.empty())
            return plan;
        for (std::size_t block = 0; block < function.blocks.size(); ++block)
            for (std::size_t index = 0; index < function.blocks[block].instructions.size(); ++index)
            {
                const auto& instruction = function.blocks[block].instructions[index];
                for (std::size_t operand = 0; operand < instruction.operands.size(); ++operand)
                {
                    const auto found = candidates.find(instruction.operands[operand].value());
                    if (found == candidates.end())
                        continue;
                    auto& candidate = found->second;
                    if ((instruction.opcode == Opcode::DropValue || instruction.opcode == Opcode::Release) &&
                        instruction.operands.size() == 1 && !instruction.result && instruction.targets.empty())
                        candidate.cleanup.emplace_back(block, index);
                    else if (instruction.opcode == Opcode::Interpolate &&
                             instruction.signatureTypes.size() == instruction.operands.size() &&
                             instruction.stringSegments.size() == instruction.operands.size() + 1 &&
                             instruction.signatureTypes[operand] == module.types.stringType())
                    {
                        ++candidate.consumers;
                        candidate.consumerBlock = block;
                        candidate.consumerIndex = index;
                    }
                    else
                        candidate.safe = false;
                }
                if (const auto found = candidates.find(instruction.borrowOrigin.value());
                    instruction.borrowOrigin && found != candidates.end())
                    found->second.safe = false;
                for (const auto& target : instruction.targets)
                    for (const auto argument : target.arguments)
                        if (const auto found = candidates.find(argument.value()); found != candidates.end())
                            found->second.safe = false;
            }
        for (const auto& [id, candidate] : candidates)
        {
            bool safe = candidate.safe && candidate.consumers == 1;
            for (const auto& [block, index] : candidate.cleanup)
                safe = safe && block == candidate.consumerBlock && index > candidate.consumerIndex;
            if (safe)
                plan.fragments.emplace(id, candidate.storage);
        }
        return plan;
    }
} // namespace wio::codegen
