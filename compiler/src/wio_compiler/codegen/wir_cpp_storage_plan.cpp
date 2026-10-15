#include "wio/codegen/wir_cpp_storage_plan.h"

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
                    if (!scalarCellUse(instruction, index))
                        plan.scalarPlaces.erase(instruction.operands[index].value());
                if (instruction.borrowOrigin && !(instruction.opcode == wir::lowered::Opcode::Load &&
                                                  instruction.resultOwnership == wir::typed::ValueOwnership::Trivial))
                    plan.scalarPlaces.erase(instruction.borrowOrigin.value());
                for (const auto& target : instruction.targets)
                    for (const auto value : target.arguments)
                        plan.scalarPlaces.erase(value.value());
                if (instruction.opcode == wir::lowered::Opcode::Release ||
                    instruction.opcode == wir::lowered::Opcode::DropValue)
                    for (const auto value : instruction.operands)
                        plan.plainValues.erase(value.value());
            }
        plan.plainValues.insert(plan.scalarPlaces.begin(), plan.scalarPlaces.end());
        return plan;
    }
} // namespace wio::codegen
