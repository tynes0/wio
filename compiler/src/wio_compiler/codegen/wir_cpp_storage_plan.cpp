#include "wio/codegen/wir_cpp_storage_plan.h"

#include <algorithm>

namespace wio::codegen
{
    namespace
    {
        bool primitive(const wir::TypeTable& types, wir::TypeId id)
        {
            const auto* type = types.tryGet(id);
            if (!type || type->ownership != wir::OwnershipModel::Trivial)
                return false;
            switch (type->kind)
            {
            case wir::TypeKind::Bool:
            case wir::TypeKind::I8:
            case wir::TypeKind::I16:
            case wir::TypeKind::I32:
            case wir::TypeKind::I64:
            case wir::TypeKind::ISize:
            case wir::TypeKind::U8:
            case wir::TypeKind::U16:
            case wir::TypeKind::U32:
            case wir::TypeKind::U64:
            case wir::TypeKind::USize:
            case wir::TypeKind::F32:
            case wir::TypeKind::F64:
            case wir::TypeKind::Byte:
            case wir::TypeKind::Char:
                return true;
            default:
                return false;
            }
        }

        bool scalarCellUse(const wir::lowered::Instruction& instruction, std::size_t operand)
        {
            if (operand != 0)
                return false;
            using Opcode = wir::lowered::Opcode;
            switch (instruction.opcode)
            {
            case Opcode::Load:
                return instruction.resultOwnership == wir::typed::ValueOwnership::Trivial;
            case Opcode::PlaceInit:
            case Opcode::Store:
            case Opcode::Replace:
            case Opcode::ReleasePlace:
            case Opcode::DropPlace:
                return true;
            default:
                return false;
            }
        }

        bool stringCellUse(const wir::lowered::Instruction& instruction, std::size_t operand)
        {
            if (instruction.opcode == wir::lowered::Opcode::Load && operand == 0)
                return true;
            if (instruction.opcode == wir::lowered::Opcode::MoveValue && operand == 0)
                return instruction.resultOwnership == wir::typed::ValueOwnership::Owned;
            return scalarCellUse(instruction, operand);
        }

        bool observesString(const wir::lowered::Instruction& instruction, const wir::ValueId value)
        {
            if (std::find(instruction.operands.begin(), instruction.operands.end(), value) ==
                instruction.operands.end())
                return false;
            using Opcode = wir::lowered::Opcode;
            if (instruction.opcode == Opcode::Binary)
            {
                using Operator = wir::typed::BinaryOperator;
                switch (instruction.binaryOperator)
                {
                case Operator::Add:
                case Operator::Equal:
                case Operator::NotEqual:
                case Operator::Less:
                case Operator::LessEqual:
                case Operator::Greater:
                case Operator::GreaterEqual:
                    return true;
                default:
                    return false;
                }
            }
            if (instruction.opcode != Opcode::IntrinsicCall ||
                instruction.intrinsicFamily != wir::IntrinsicFamily::String)
                return false;
            if (instruction.selector == "Append")
                return instruction.operands.size() == 2 && instruction.operands[1] == value;
            // These helpers observe strings or return independent values; none
            // mutate a borrowed source, retain it or call arbitrary Wio code.
            const auto selector = instruction.selector;
            return selector == "Contains" || selector == "StartsWith" || selector == "EndsWith" ||
                   selector == "IndexOf" || selector == "LastIndexOf" || selector == "Split" || selector == "Slice" ||
                   selector == "SliceFrom" || selector == "Take" || selector == "Skip" || selector == "Left" ||
                   selector == "Right" || selector == "Trim" || selector == "TrimStart" || selector == "TrimEnd" ||
                   selector == "Repeat" || selector == "Lines" || selector == "PadLeft" || selector == "PadRight" ||
                   selector == "Count" || selector == "Empty";
        }
    } // namespace

    WirCppStoragePlan WirCppStoragePlan::build(const wir::lowered::Module& module,
                                               const wir::lowered::Function& function)
    {
        WirCppStoragePlan plan;
        // Retain the established coroutine frame/suspend representation.
        if (function.isAsync || function.coroutine)
            return plan;
        plan.directControlFlow = true;
        const auto collect = [&](const wir::lowered::Parameter& parameter)
        {
            if (parameter.ownership == wir::typed::ValueOwnership::Trivial && primitive(module.types, parameter.type))
                plan.plainValues.insert(parameter.id.value());
        };
        for (const auto& parameter : function.parameters)
            collect(parameter);
        for (const auto& block : function.blocks)
        {
            for (const auto& parameter : block.parameters)
                collect(parameter);
            for (const auto& instruction : block.instructions)
            {
                if (instruction.result && instruction.resultOwnership == wir::typed::ValueOwnership::Trivial &&
                    primitive(module.types, instruction.resultType))
                    plan.plainValues.insert(instruction.result.value());
                const auto* type = module.types.tryGet(instruction.resultType);
                if (instruction.opcode == wir::lowered::Opcode::LocalPlace && instruction.result && type &&
                    type->kind == wir::TypeKind::Reference && type->arguments.size() == 1 &&
                    primitive(module.types, type->arguments.front()) &&
                    instruction.storageClass == wir::lowered::StorageClass::Stack &&
                    (instruction.escapeClass == wir::lowered::EscapeClass::Local ||
                     instruction.escapeClass == wir::lowered::EscapeClass::Store))
                    plan.scalarPlaces.insert(instruction.result.value());
                if (instruction.opcode == wir::lowered::Opcode::LocalPlace && instruction.result && type &&
                    type->kind == wir::TypeKind::Reference && type->arguments.size() == 1 &&
                    module.types.get(type->arguments.front()).kind == wir::TypeKind::String &&
                    instruction.storageClass == wir::lowered::StorageClass::Stack &&
                    (instruction.escapeClass == wir::lowered::EscapeClass::Local ||
                     instruction.escapeClass == wir::lowered::EscapeClass::Store))
                    plan.stringPlaces.insert(instruction.result.value());
            }
        }
        // Store classification also includes writes *into* the cell in current
        // canonical metadata. Distinguish those from storing the cell itself.
        // Escape metadata is necessary, not sufficient: prove a closed direct
        // load/store use set. References, projections, phi edges, calls, native
        // borrows and captures keep the full Place representation.
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
            {
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (!scalarCellUse(instruction, index))
                        plan.scalarPlaces.erase(instruction.operands[index].value());
                    if (!stringCellUse(instruction, index))
                        plan.stringPlaces.erase(instruction.operands[index].value());
                }
                if (instruction.borrowOrigin && !(instruction.opcode == wir::lowered::Opcode::Load &&
                                                  instruction.resultOwnership == wir::typed::ValueOwnership::Trivial))
                    plan.scalarPlaces.erase(instruction.borrowOrigin.value());
                if (instruction.borrowOrigin && instruction.opcode != wir::lowered::Opcode::Load)
                    plan.stringPlaces.erase(instruction.borrowOrigin.value());
                for (const auto& target : instruction.targets)
                    for (const auto value : target.arguments)
                    {
                        plan.scalarPlaces.erase(value.value());
                        plan.stringPlaces.erase(value.value());
                    }
                if (instruction.opcode == wir::lowered::Opcode::Release ||
                    instruction.opcode == wir::lowered::Opcode::DropValue)
                    for (const auto value : instruction.operands)
                        plan.plainValues.erase(value.value());
            }
        plan.plainValues.insert(plan.scalarPlaces.begin(), plan.scalarPlaces.end());
        plan.plainValues.insert(plan.stringPlaces.begin(), plan.stringPlaces.end());

        // An adjacent, sole-use snapshot can borrow for a known observer, or
        // Append's self-alias-safe suffix. Count uses across the whole CFG but
        // never move a borrow across a block edge or intervening instruction.
        // Unknown calls, other mutators and explicit cleanup keep the copy.
        std::unordered_map<std::uint32_t, std::size_t> uses;
        for (const auto& block : function.blocks)
        {
            for (const auto& instruction : block.instructions)
            {
                for (const auto operand : instruction.operands)
                    ++uses[operand.value()];
                if (instruction.borrowOrigin)
                    ++uses[instruction.borrowOrigin.value()];
                for (const auto& target : instruction.targets)
                    for (const auto argument : target.arguments)
                        ++uses[argument.value()];
            }
        }
        for (const auto& block : function.blocks)
        {
            const auto& instructions = block.instructions;
            for (std::size_t index = 1; index + 1 < instructions.size(); ++index)
            {
                const auto& load = instructions[index - 1];
                const auto& copy = instructions[index];
                const auto& call = instructions[index + 1];
                if (load.opcode == wir::lowered::Opcode::Load && load.result &&
                    load.resultOwnership == wir::typed::ValueOwnership::Borrowed &&
                    module.types.get(load.resultType).kind == wir::TypeKind::String &&
                    copy.opcode == wir::lowered::Opcode::CopyValue && copy.result &&
                    copy.resultOwnership == wir::typed::ValueOwnership::Owned && copy.resultType == load.resultType &&
                    copy.operands.size() == 1 && copy.operands.front() == load.result &&
                    uses[copy.result.value()] == 1 && observesString(call, copy.result))
                    plan.borrowedStringCopies.emplace(copy.result.value(), load.result);
            }
        }
        return plan;
    }
} // namespace wio::codegen
