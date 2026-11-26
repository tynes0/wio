#include "wio/codegen/wir_cpp_string_init_plan.h"
#include "wio/codegen/wir_cpp_string_transfer_plan.h"

#include <deque>

namespace wio::codegen
{
    WirCppStringInitPlan WirCppStringInitPlan::build(const wir::lowered::Module& module,
                                                     const wir::lowered::Function& function,
                                                     const WirCppStoragePlan& storage)
    {
        using namespace wir;
        using Opcode = lowered::Opcode;
        WirCppStringInitPlan plan;
        // Proof budgets, not language limits. Unknown/large functions retain
        // their original storage and exception behavior.
        if (function.isAsync || function.coroutine || function.blocks.empty() || function.blocks.size() > 512 ||
            storage.stringPlaces.empty() || storage.stringPlaces.size() > 64)
            return plan;

        struct Definition
        {
            TypeId type;
            typed::ValueOwnership ownership;
            const lowered::Instruction* instruction;
            std::size_t block, index;
        };
        std::unordered_map<std::uint32_t, Definition> definitions;
        for (const auto& parameter : function.parameters)
            definitions.emplace(parameter.id.value(), Definition{parameter.type, parameter.ownership, nullptr, 0, 0});
        for (std::size_t block = 0; block < function.blocks.size(); ++block)
        {
            for (const auto& parameter : function.blocks[block].parameters)
                definitions.emplace(parameter.id.value(),
                                    Definition{parameter.type, parameter.ownership, nullptr, block, 0});
            const auto& code = function.blocks[block].instructions;
            for (std::size_t index = 0; index < code.size(); ++index)
                if (code[index].result)
                    definitions.emplace(
                        code[index].result.value(),
                        Definition{code[index].resultType, code[index].resultOwnership, &code[index], block, index});
        }

        std::unordered_map<std::uint32_t, std::uint64_t> cellBits;
        for (const auto cell : storage.stringPlaces)
            cellBits.emplace(cell, std::uint64_t{1} << cellBits.size());
        struct Effects
        {
            std::uint64_t set = 0, clear = 0;

            std::uint64_t apply(std::uint64_t incoming) const
            {
                return (incoming & ~clear) | set;
            }
        };
        const auto update = [&](Effects& effects, const lowered::Instruction& instruction)
        {
            const bool local = instruction.opcode == Opcode::LocalPlace;
            const ValueId cell = local                          ? instruction.result
                                 : instruction.operands.empty() ? ValueId{}
                                                                : instruction.operands[0];
            const auto found = cellBits.find(cell.value());
            if (!cell || found == cellBits.end())
                return;
            const auto bit = found->second;
            if (local || instruction.opcode == Opcode::DropPlace || instruction.opcode == Opcode::ReleasePlace ||
                instruction.opcode == Opcode::MoveValue)
            {
                effects.clear |= bit;
                effects.set &= ~bit;
            }
            else if (instruction.opcode == Opcode::PlaceInit || instruction.opcode == Opcode::Store ||
                     instruction.opcode == Opcode::Replace)
            {
                effects.set |= bit;
                effects.clear &= ~bit;
            }
        };
        struct Candidate
        {
            ValueId source, cell;
            std::size_t block;
            const lowered::Instruction *local, *init;
            Effects prefix;
        };
        std::vector<Candidate> candidates;
        for (std::size_t block = 0; block < function.blocks.size(); ++block)
        {
            const auto& code = function.blocks[block].instructions;
            for (std::size_t index = 2; index < code.size(); ++index)
            {
                const auto& init = code[index];
                const auto& local = code[index - 1];
                if (init.opcode != Opcode::PlaceInit || init.operands.size() != 2 ||
                    local.opcode != Opcode::LocalPlace || local.result != init.operands[0] ||
                    !cellBits.contains(local.result.value()))
                    continue;
                const auto source = definitions.find(init.operands[1].value());
                const auto* reference = module.types.tryGet(local.resultType);
                if (source == definitions.end() || !source->second.instruction || source->second.block != block ||
                    source->second.index >= index - 1 || index - source->second.index > 32 || !reference ||
                    reference->kind != TypeKind::Reference || reference->arguments.size() != 1 ||
                    reference->arguments[0] != source->second.type ||
                    !WirCppStringTransferPlan::isFreshStringProducer(module, *source->second.instruction))
                    continue;
                bool safe = true;
                for (std::size_t between = source->second.index + 1; safe && between < index - 1; ++between)
                {
                    const auto& drop = code[between];
                    if ((drop.opcode != Opcode::DropValue && drop.opcode != Opcode::Release) ||
                        drop.operands.size() != 1 || drop.result || drop.borrowOrigin || !drop.targets.empty())
                    {
                        safe = false;
                        break;
                    }
                    const auto value = definitions.find(drop.operands[0].value());
                    const auto* type = value == definitions.end() ? nullptr : module.types.tryGet(value->second.type);
                    safe = type && type->kind == TypeKind::String &&
                           value->second.ownership == typed::ValueOwnership::Owned;
                }
                if (safe)
                    candidates.push_back({init.operands[1], local.result, block, &local, &init, {}});
            }
        }
        if (candidates.empty() || candidates.size() > 64)
            return plan;
        struct SourceUse
        {
            std::size_t candidate, count;
        };
        std::unordered_map<std::uint32_t, SourceUse> uses;
        for (std::size_t index = 0; index < candidates.size(); ++index)
            if (!uses.emplace(candidates[index].source.value(), SourceUse{index, 0}).second)
                return plan;
        const auto count = [&](ValueId value)
        {
            if (auto found = uses.find(value.value()); found != uses.end())
                ++found->second.count;
        };
        std::unordered_map<std::uint32_t, std::size_t> blocks;
        for (std::size_t index = 0; index < function.blocks.size(); ++index)
            blocks.emplace(function.blocks[index].id.value(), index);
        std::vector<Effects> effects(function.blocks.size());
        std::vector<std::vector<std::size_t>> successors(function.blocks.size());
        for (std::size_t block = 0; block < function.blocks.size(); ++block)
        {
            const auto& code = function.blocks[block].instructions;
            for (std::size_t index = 0; index < code.size(); ++index)
            {
                const auto& instruction = code[index];
                if (instruction.result)
                    if (const auto found = uses.find(instruction.result.value()); found != uses.end())
                        candidates[found->second.candidate].prefix = effects[block];
                update(effects[block], instruction);
                for (const auto operand : instruction.operands)
                    count(operand);
                if (instruction.borrowOrigin)
                    count(instruction.borrowOrigin);
                for (const auto& target : instruction.targets)
                    for (const auto argument : target.arguments)
                        count(argument);
            }
            if (!code.empty())
                for (const auto& target : code.back().targets)
                {
                    const auto found = blocks.find(target.block.value());
                    if (found == blocks.end())
                        return plan;
                    successors[block].push_back(found->second);
                }
        }

        // May-initialized dataflow. Join is OR: one live predecessor is enough
        // to reject forwarding. Crucially, check BEFORE the producer, not after
        // the later LocalPlace reset. Backedges to the entry are joined too.
        std::vector<std::uint64_t> incoming(function.blocks.size(), 0);
        std::vector<bool> reachable(function.blocks.size(), false), queued(function.blocks.size(), false);
        std::deque<std::size_t> pending{0};
        reachable[0] = queued[0] = true;
        while (!pending.empty())
        {
            const auto block = pending.front();
            pending.pop_front();
            queued[block] = false;
            const auto outgoing = effects[block].apply(incoming[block]);
            for (const auto next : successors[block])
            {
                const auto joined = incoming[next] | outgoing;
                if (!reachable[next] || incoming[next] != joined)
                {
                    reachable[next] = true;
                    incoming[next] = joined;
                    if (!queued[next])
                    {
                        queued[next] = true;
                        pending.push_back(next);
                    }
                }
            }
        }
        for (const auto& candidate : candidates)
            if (reachable[candidate.block] && uses.at(candidate.source.value()).count == 1 &&
                !(candidate.prefix.apply(incoming[candidate.block]) & cellBits.at(candidate.cell.value())))
            {
                plan.producerCells.emplace(candidate.source.value(), candidate.cell);
                plan.suppressedInstructions.insert(candidate.local);
                plan.suppressedInstructions.insert(candidate.init);
            }
        return plan;
    }
} // namespace wio::codegen
