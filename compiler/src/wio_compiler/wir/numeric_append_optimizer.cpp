#include "wio/wir/numeric_append_optimizer.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace wio::wir
{
    namespace
    {
        using Opcode = lowered::Opcode;

        bool numeric(const Type* type)
        {
            if (!type || type->ownership != OwnershipModel::Trivial)
                return false;
            switch (type->kind)
            {
            case TypeKind::Bool:
            case TypeKind::I8:
            case TypeKind::I16:
            case TypeKind::I32:
            case TypeKind::I64:
            case TypeKind::ISize:
            case TypeKind::U8:
            case TypeKind::U16:
            case TypeKind::U32:
            case TypeKind::U64:
            case TypeKind::USize:
            case TypeKind::Byte:
            case TypeKind::F32:
            case TypeKind::F64:
                return true;
            default:
                return false;
            }
        }

        struct Use
        {
            BlockId block;
            std::size_t index;
            bool cleanup;
        };
    } // namespace

    std::size_t NumericAppendOptimizer::optimize(lowered::Module& module)
    {
        std::size_t fused = 0;
        for (auto& function : module.functions)
        {
            if (function.isAsync || function.coroutine)
                continue;
            bool hasFormatting = false;
            for (const auto& block : function.blocks)
                for (const auto& instruction : block.instructions)
                    hasFormatting = hasFormatting || instruction.opcode == Opcode::Interpolate;
            if (!hasFormatting)
                continue;
            std::unordered_map<ValueId::ValueType, std::vector<Use>> uses;
            for (const auto& block : function.blocks)
            {
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const auto& instruction = block.instructions[index];
                    const bool cleanup =
                        (instruction.opcode == Opcode::Release || instruction.opcode == Opcode::DropValue) &&
                        instruction.operands.size() == 1 && !instruction.result && instruction.targets.empty() &&
                        !instruction.borrowOrigin;
                    for (const auto operand : instruction.operands)
                        uses[operand.value()].push_back({block.id, index, cleanup});
                    for (const auto& target : instruction.targets)
                        for (const auto argument : target.arguments)
                            uses[argument.value()].push_back({block.id, index, false});
                    if (instruction.borrowOrigin)
                        uses[instruction.borrowOrigin.value()].push_back({block.id, index, false});
                }
            }

            for (auto& block : function.blocks)
            {
                std::unordered_set<std::size_t> removed;
                for (std::size_t index = 0; index + 1 < block.instructions.size(); ++index)
                {
                    const auto& format = block.instructions[index];
                    auto& append = block.instructions[index + 1];
                    if (format.opcode != Opcode::Interpolate || !format.result ||
                        format.resultType != module.types.stringType() ||
                        format.intrinsicFamily != IntrinsicFamily::String || format.operands.size() != 1 ||
                        format.signatureTypes.size() != 1 || format.stringSegments.size() != 2 ||
                        !format.stringSegments[0].empty() || !format.stringSegments[1].empty() || format.borrowOrigin ||
                        !format.targets.empty() || format.resultOwnership != typed::ValueOwnership::Owned ||
                        !numeric(module.types.tryGet(format.signatureTypes[0])) ||
                        append.opcode != Opcode::IntrinsicCall || append.intrinsicFamily != IntrinsicFamily::String ||
                        append.selector != "Append" || append.result || append.operands.size() != 2 ||
                        append.signatureTypes.size() != 2 || append.operands[1] != format.result ||
                        append.signatureTypes[1] != module.types.stringType() ||
                        append.targetType != module.types.stringType())
                        continue;
                    const auto* receiver = module.types.tryGet(append.signatureTypes[0]);
                    if (!receiver || receiver->kind != TypeKind::Reference || !receiver->isMutable ||
                        receiver->arguments.size() != 1 || receiver->arguments[0] != module.types.stringType())
                        continue;
                    bool safe = true;
                    std::size_t consumers = 0;
                    std::size_t cleanups = 0;
                    for (const auto& use : uses[format.result.value()])
                    {
                        if (use.block != block.id ||
                            (use.index != index + 1 && (!use.cleanup || use.index <= index + 1)))
                            safe = false;
                        if (!use.cleanup)
                            ++consumers;
                        else
                            ++cleanups;
                    }
                    if (!safe || consumers != 1 || cleanups > 1)
                        continue;
                    append.operands[1] = format.operands[0];
                    append.signatureTypes[1] = format.signatureTypes[0];
                    removed.insert(index);
                    for (const auto& use : uses[format.result.value()])
                        if (use.cleanup)
                            removed.insert(use.index);
                    ++fused;
                }

                std::size_t retained = 0;
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    if (removed.contains(index))
                        continue;
                    if (retained != index)
                        block.instructions[retained] = std::move(block.instructions[index]);
                    ++retained;
                }
                block.instructions.resize(retained);
            }
        }

        return fused;
    }
} // namespace wio::wir
