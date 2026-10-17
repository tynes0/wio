#include "wio/codegen/wir_cpp_string_transfer_plan.h"

#include <vector>

namespace wio::codegen
{
    namespace
    {
        bool freshStringProducer(const wir::lowered::Instruction& instruction)
        {
            using Opcode = wir::lowered::Opcode;
            // Loads/copies/references/native resource values are not fresh
            // prvalues. Do not coalesce them through this proof.
            switch (instruction.opcode)
            {
            case Opcode::Constant:
            case Opcode::Interpolate:
            case Opcode::Call:
            case Opcode::MethodCall:
            case Opcode::ExtensionCall:
            case Opcode::VirtualCall:
            case Opcode::InterfaceCall:
            case Opcode::NativeInvoke:
                return true;
            default:
                return false;
            }
        }
    } // namespace

    WirCppStringTransferPlan WirCppStringTransferPlan::build(const wir::lowered::Module& module,
                                                             const wir::lowered::Function& function)
    {
        using namespace wir;
        using Opcode = lowered::Opcode;
        WirCppStringTransferPlan plan;
        if (function.isAsync || function.coroutine || function.blocks.size() < 2)
            return plan;
        struct Candidate
        {
            ValueId source;
            TypeId type;
            BlockId destination;
        };
        std::vector<Candidate> candidates;
        for (const auto& block : function.blocks)
        {
            const auto& code = block.instructions;
            if (code.size() < 2)
                continue;
            const auto& producer = code[code.size() - 2];
            const auto& edge = code.back();
            const auto* type = module.types.tryGet(producer.resultType);
            if (edge.opcode != Opcode::Jump || edge.targets.size() != 1 || edge.targets[0].arguments.size() != 1 ||
                !producer.result || edge.targets[0].arguments[0] != producer.result || !freshStringProducer(producer) ||
                producer.resultOwnership != typed::ValueOwnership::Owned || producer.borrowOrigin ||
                !producer.targets.empty() || !type || type->kind != TypeKind::String)
                continue;
            candidates.push_back({producer.result, producer.resultType, edge.targets[0].block});
        }
        // Most functions have no candidate: avoid a whole-function use index.
        if (candidates.empty())
            return plan;
        std::unordered_map<std::uint32_t, std::size_t> uses;
        for (const auto& candidate : candidates)
            uses.emplace(candidate.source.value(), 0);
        const auto count = [&](ValueId value)
        {
            if (const auto found = uses.find(value.value()); found != uses.end())
                ++found->second;
        };
        std::unordered_map<std::uint32_t, const lowered::BasicBlock*> blocks;
        blocks.reserve(function.blocks.size());
        for (const auto& block : function.blocks)
        {
            blocks.emplace(block.id.value(), &block);
            for (const auto& instruction : block.instructions)
            {
                for (const auto operand : instruction.operands)
                    count(operand);
                if (instruction.borrowOrigin)
                    count(instruction.borrowOrigin);
                for (const auto& target : instruction.targets)
                    for (const auto argument : target.arguments)
                        count(argument);
            }
        }
        for (const auto& candidate : candidates)
        {
            const auto destination = blocks.find(candidate.destination.value());
            if (uses.at(candidate.source.value()) != 1 || destination == blocks.end() ||
                destination->second->parameters.size() != 1)
                continue;
            const auto& parameter = destination->second->parameters.front();
            if (parameter.type == candidate.type && parameter.ownership == typed::ValueOwnership::Owned &&
                parameter.id != candidate.source)
                plan.edgeDestinations.emplace(candidate.source.value(), parameter.id);
        }
        return plan;
    }
} // namespace wio::codegen
