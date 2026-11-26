#include "wio/codegen/wir_cpp_receiver_plan.h"
#include "wio/codegen/wir_cpp_storage_plan.h"

#include <algorithm>
#include <unordered_map>

namespace wio::codegen
{
    WirCppReceiverPlan WirCppReceiverPlan::build(const wir::lowered::Module& module,
                                                 const wir::lowered::Function& function)
    {
        using namespace wir;
        using Opcode = lowered::Opcode;
        if (!function.isMethod || function.isExternal || function.isAbstract || function.isExtension ||
            function.isClosureBody || function.isAsync || function.coroutine || function.nativeBinding ||
            function.parameters.size() != 1 || !function.captures.empty() || function.captureParameterCount ||
            !function.genericParameters.empty() || function.genericOrigin || function.blocks.size() != 1 ||
            !function.blocks.front().parameters.empty())
            return {};
        const auto* owner = module.types.tryGet(function.ownerType);
        const auto* receiver = module.types.tryGet(function.parameters.front().type);
        const auto* result = module.types.tryGet(function.returnType);
        if (!owner || owner->nominalKind != NominalKind::Object || owner->nativeBinding ||
            owner->nominalRepresentation != NominalRepresentation::Wio || !owner->arguments.empty() || !receiver ||
            receiver->kind != TypeKind::Reference || receiver->arguments.size() != 1 ||
            receiver->arguments.front() != function.ownerType ||
            function.parameters.front().ownership != typed::ValueOwnership::Borrowed || !result ||
            (result->kind != TypeKind::Void && result->kind != TypeKind::String &&
             !WirCppStoragePlan::isPlainScalar(module.types, function.returnType)))
            return {};
        // Ordinary objects carry the empty builtin root as an implicit base.
        // More complex hierarchy bodies retain the general entry for now.
        for (const auto baseId : owner->baseTypes)
        {
            const auto* base = module.types.tryGet(baseId);
            if (!base || base->nominalKind != NominalKind::Object || base->nativeBinding ||
                base->nominalRepresentation != NominalRepresentation::Wio || !base->fields.empty() ||
                !base->methods.empty() || !base->baseTypes.empty() || !base->arguments.empty())
                return {};
        }
        for (const auto& attribute : module.contract.attributes)
            if (attribute.targetFunction == function.id && !attribute.processors.empty())
                return {};
        const auto& code = function.blocks.front().instructions;
        if (code.empty() || code.size() > 128 || code.back().opcode != Opcode::Return)
            return {};
        const auto storage = WirCppStoragePlan::build(module, function);
        if (storage.scalarFieldPlaces.empty())
            return {};
        WirCppReceiverPlan plan;
        plan.receiverValues.insert(function.parameters.front().id.value());
        std::unordered_set<std::uint32_t> fields;
        std::unordered_map<std::uint32_t, TypeId> types;
        types.emplace(function.parameters.front().id.value(), function.parameters.front().type);
        const auto scalar = [&](ValueId id)
        {
            const auto found = types.find(id.value());
            return found != types.end() && WirCppStoragePlan::isPlainScalar(module.types, found->second);
        };
        const auto string = [&](ValueId id)
        {
            const auto found = types.find(id.value());
            return found != types.end() && module.types.get(found->second).kind == TypeKind::String;
        };
        bool barrier = false;
        for (const auto& instruction : code)
        {
            // Get() in the ABI wrapper must not move a borrow/uninitialized
            // check before an earlier divide/convert failure. Constants and
            // readonly alias construction alone are effect-free here.
            if (fields.empty() && instruction.opcode != Opcode::Constant && instruction.opcode != Opcode::Borrow &&
                instruction.opcode != Opcode::FieldPlace)
                return {};
            if (instruction.result)
                types.emplace(instruction.result.value(), instruction.resultType);
            const auto touchesReceiver = [&](ValueId value)
            { return plan.receiverValues.contains(value.value()) || fields.contains(value.value()); };
            if (instruction.borrowOrigin &&
                !(instruction.opcode == Opcode::Borrow && instruction.operands.size() == 1 &&
                  instruction.borrowOrigin == instruction.operands[0] &&
                  plan.receiverValues.contains(instruction.borrowOrigin.value())) &&
                !(instruction.opcode == Opcode::Load && instruction.operands.size() == 1 &&
                  instruction.borrowOrigin == instruction.operands[0] &&
                  fields.contains(instruction.borrowOrigin.value())))
                return {};
            if (!instruction.targets.empty() ||
                (barrier && (std::ranges::any_of(instruction.operands, touchesReceiver) ||
                             (instruction.borrowOrigin && touchesReceiver(instruction.borrowOrigin)))))
                return {};
            switch (instruction.opcode)
            {
            case Opcode::Borrow:
            {
                const auto* view = module.types.tryGet(instruction.resultType);
                if (instruction.operands.size() != 1 ||
                    !plan.receiverValues.contains(instruction.operands[0].value()) || !view ||
                    view->kind != TypeKind::Reference || view->isMutable || view->arguments != receiver->arguments)
                    return {};
                plan.receiverValues.insert(instruction.result.value());
                break;
            }
            case Opcode::FieldPlace:
                if (instruction.operands.size() != 1 ||
                    !plan.receiverValues.contains(instruction.operands[0].value()) ||
                    !storage.scalarFieldPlaces.contains(instruction.result.value()) ||
                    instruction.targetType != function.ownerType)
                    return {};
                fields.insert(instruction.result.value());
                break;
            case Opcode::Load:
                if (instruction.operands.size() != 1 || !fields.contains(instruction.operands[0].value()) ||
                    !scalar(instruction.result) || instruction.resultOwnership != typed::ValueOwnership::Trivial)
                    return {};
                break;
            case Opcode::Store:
            case Opcode::Replace:
                if (instruction.operands.size() != 2 || !fields.contains(instruction.operands[0].value()) ||
                    !scalar(instruction.operands[1]))
                    return {};
                break;
            case Opcode::Constant:
                if (!scalar(instruction.result) && !string(instruction.result))
                    return {};
                barrier = barrier || string(instruction.result);
                break;
            case Opcode::Unary:
            case Opcode::Binary:
            case Opcode::Convert:
                if (!scalar(instruction.result) || !std::ranges::all_of(instruction.operands, scalar))
                    return {};
                break;
            case Opcode::Interpolate:
                if (!string(instruction.result) || !std::ranges::all_of(instruction.operands, [&](ValueId value)
                                                                        { return scalar(value) || string(value); }))
                    return {};
                barrier = true;
                break;
            case Opcode::CopyValue:
            case Opcode::MoveValue:
            case Opcode::Retain:
            case Opcode::Release:
            case Opcode::DropValue:
                if (!std::ranges::all_of(instruction.operands,
                                         [&](ValueId value) { return scalar(value) || string(value); }))
                    return {};
                barrier = barrier || std::ranges::any_of(instruction.operands, string);
                break;
            case Opcode::Return:
                if (!std::ranges::all_of(instruction.operands,
                                         [&](ValueId value) { return scalar(value) || string(value); }))
                    return {};
                break;
            default:
                return {};
            }
            // Before the first receiver access only pure primitive setup and
            // view aliases are permitted. Do not hoist checks across effects.
            if (fields.empty() && barrier)
                return {};
        }
        return fields.empty() ? WirCppReceiverPlan{} : plan;
    }
} // namespace wio::codegen
