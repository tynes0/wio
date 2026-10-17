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

        std::unordered_set<std::uint32_t> scalarFields(const wir::lowered::Module& module,
                                                       const wir::lowered::Function& function)
        {
            using namespace wir;
            using Opcode = lowered::Opcode;
            struct Interval
            {
                std::size_t block, begin, end;
                TypeId valueType;
            };
            std::unordered_map<std::uint32_t, Interval> candidates;
            for (std::size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex)
            {
                const auto& block = function.blocks[blockIndex];
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const auto& instruction = block.instructions[index];
                    if (instruction.opcode != Opcode::FieldPlace || !instruction.result ||
                        instruction.operands.size() != 1)
                        continue;
                    const auto* reference = module.types.tryGet(instruction.resultType);
                    const auto* owner = module.types.tryGet(instruction.targetType);
                    if (!reference || reference->kind != TypeKind::Reference || reference->arguments.size() != 1 ||
                        !primitive(module.types, reference->arguments.front()) || !owner || owner->nativeBinding ||
                        owner->nominalRepresentation != NominalRepresentation::Wio ||
                        (owner->nominalKind != NominalKind::Object && owner->nominalKind != NominalKind::Component) ||
                        instruction.projectionIndex >= owner->fields.size() ||
                        owner->fields[instruction.projectionIndex].type != reference->arguments.front())
                        continue;
                    candidates.emplace(instruction.result.value(),
                                       Interval{blockIndex, index, index, reference->arguments.front()});
                }
            }
            if (candidates.empty())
                return {};
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
            const auto scalarValue = [&](const ValueId value)
            {
                const auto found = types.find(value.value());
                return found != types.end() && primitive(module.types, found->second);
            };
            for (std::size_t blockIndex = 0; blockIndex < function.blocks.size(); ++blockIndex)
            {
                const auto& instructions = function.blocks[blockIndex].instructions;
                for (std::size_t index = 0; index < instructions.size(); ++index)
                {
                    const auto& instruction = instructions[index];
                    for (std::size_t operand = 0; operand < instruction.operands.size(); ++operand)
                    {
                        const auto found = candidates.find(instruction.operands[operand].value());
                        if (found == candidates.end())
                            continue;
                        const bool load = instruction.opcode == Opcode::Load &&
                                          instruction.resultOwnership == typed::ValueOwnership::Trivial &&
                                          instruction.resultType == found->second.valueType;
                        const bool store =
                            (instruction.opcode == Opcode::Store || instruction.opcode == Opcode::Replace) &&
                            instruction.operands.size() == 2 && scalarValue(instruction.operands[1]);
                        if (operand != 0 || blockIndex != found->second.block || index <= found->second.begin ||
                            (!load && !store))
                            candidates.erase(found);
                        else
                            found->second.end = index;
                    }
                    if (instruction.borrowOrigin &&
                        (instruction.opcode != Opcode::Load || instruction.operands.size() != 1 ||
                         instruction.operands[0] != instruction.borrowOrigin ||
                         instruction.resultOwnership != typed::ValueOwnership::Trivial))
                        candidates.erase(instruction.borrowOrigin.value());
                    for (const auto& target : instruction.targets)
                        for (const auto argument : target.arguments)
                            candidates.erase(argument.value());
                }
            }
            std::unordered_set<std::uint32_t> result;
            for (const auto& [id, interval] : candidates)
            {
                // Bound both proof work and the lifetime window. A missed
                // opportunity falls back to Place; it never weakens a borrow.
                if (interval.end == interval.begin || interval.end - interval.begin > 32)
                    continue;
                bool safe = true;
                const auto& instructions = function.blocks[interval.block].instructions;
                for (std::size_t index = interval.begin + 1; safe && index <= interval.end; ++index)
                {
                    const auto& instruction = instructions[index];
                    switch (instruction.opcode)
                    {
                    case Opcode::Constant:
                        safe = primitive(module.types, instruction.resultType);
                        break;
                    case Opcode::Unary:
                    case Opcode::Binary:
                        safe = primitive(module.types, instruction.resultType) &&
                               std::ranges::all_of(instruction.operands, scalarValue);
                        break;
                    case Opcode::Load:
                    case Opcode::Store:
                    case Opcode::Replace:
                        safe = !instruction.operands.empty() && instruction.operands[0].value() == id;
                        break;
                    default:
                        safe = false;
                        break;
                    }
                    if (instruction.borrowOrigin && instruction.borrowOrigin.value() != id)
                        safe = false;
                    if (!instruction.targets.empty())
                        safe = false;
                }
                if (safe)
                    result.insert(id);
            }
            return result;
        }

        bool stringCellUse(const wir::lowered::Instruction& instruction, std::size_t operand)
        {
            if (operand == 0 && instruction.opcode == wir::lowered::Opcode::IntrinsicCall &&
                instruction.intrinsicFamily == wir::IntrinsicFamily::String && instruction.selector == "Append" &&
                instruction.operands.size() == 2 && !instruction.result)
                return true;
            if (instruction.opcode == wir::lowered::Opcode::Load && operand == 0)
                return true;
            if (instruction.opcode == wir::lowered::Opcode::MoveValue && operand == 0)
                return instruction.resultOwnership == wir::typed::ValueOwnership::Owned;
            return scalarCellUse(instruction, operand);
        }

        bool callScopedStringUse(const wir::lowered::Module& module, const wir::lowered::Instruction& instruction,
                                 const std::size_t operand)
        {
            using namespace wir;
            if (instruction.opcode != lowered::Opcode::NativeInvoke || !instruction.callee)
                return false;
            const auto offset = instruction.callee.value();
            const lowered::Function* callee =
                offset < module.functions.size() && module.functions[offset].id == instruction.callee
                    ? &module.functions[offset]
                    : nullptr;
            if (!callee)
            {
                const auto found = std::ranges::find(module.functions, instruction.callee, &lowered::Function::id);
                if (found != module.functions.end())
                    callee = &*found;
            }
            if (!callee || !WirCppStoragePlan::supportsCallScopedStringAbi(module, *callee))
                return false;
            const NativeBinding& binding = *callee->nativeBinding;
            if (binding.parameters.size() != instruction.operands.size() ||
                instruction.signatureTypes.size() != instruction.operands.size() ||
                callee->parameters.size() != instruction.operands.size() || operand >= binding.parameters.size())
                return false;
            for (std::size_t index = 0; index < binding.parameters.size(); ++index)
                if (binding.parameters[index].type != instruction.signatureTypes[index])
                    return false;
            return binding.parameters[operand].marshalling == NativeMarshallingKind::Utf8String;
        }

        bool callScopedStringAbi(const wir::lowered::Module& module, const wir::lowered::Function& callee)
        {
            using namespace wir;
            if (!callee.isExternal || !callee.nativeBinding || callee.isAsync || callee.isMethod || callee.isExtension)
                return false;
            const auto& binding = *callee.nativeBinding;
            const Type* result = module.types.tryGet(callee.returnType);
            if (binding.receiver != NativeReceiverKind::None || !result ||
                (binding.result.passing != NativePassingMode::Value &&
                 binding.result.passing != NativePassingMode::ReturnOwned) ||
                (result->kind != TypeKind::Void && !primitive(module.types, callee.returnType)) ||
                (binding.result.marshalling != NativeMarshallingKind::Void &&
                 binding.result.marshalling != NativeMarshallingKind::Scalar) ||
                binding.result.callbackLifetime != NativeCallbackLifetime::Call ||
                binding.result.callbackThread != NativeCallbackThread::Caller ||
                binding.parameters.size() != callee.parameters.size())
                return false;
            bool hasString = false;
            for (std::size_t index = 0; index < binding.parameters.size(); ++index)
            {
                const auto& parameter = binding.parameters[index];
                const Type* type = module.types.tryGet(parameter.type);
                if (!type || parameter.nullable || parameter.type != callee.parameters[index].type ||
                    parameter.callbackLifetime != NativeCallbackLifetime::Call ||
                    parameter.callbackThread != NativeCallbackThread::Caller)
                    return false;
                if (parameter.marshalling == NativeMarshallingKind::Scalar &&
                    parameter.passing == NativePassingMode::Value && primitive(module.types, parameter.type))
                    continue;
                if (parameter.marshalling != NativeMarshallingKind::Utf8String ||
                    (parameter.passing != NativePassingMode::Borrow &&
                     parameter.passing != NativePassingMode::BorrowMut) ||
                    type->kind != TypeKind::Reference || type->arguments.size() != 1 ||
                    type->isMutable != (parameter.passing == NativePassingMode::BorrowMut) ||
                    module.types.get(type->arguments.front()).kind != TypeKind::String)
                    return false;
                hasString = true;
            }
            return hasString;
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

    bool WirCppStoragePlan::supportsCallScopedStringAbi(const wir::lowered::Module& module,
                                                        const wir::lowered::Function& function)
    {
        return callScopedStringAbi(module, function);
    }

    WirCppStoragePlan WirCppStoragePlan::build(const wir::lowered::Module& module,
                                               const wir::lowered::Function& function)
    {
        WirCppStoragePlan plan;
        // Retain the established coroutine frame/suspend representation.
        if (function.isAsync || function.coroutine)
            return plan;
        plan.directControlFlow = true;
        plan.scalarFieldPlaces = scalarFields(module, function);
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
                    if (plan.stringPlaces.contains(instruction.operands[index].value()) &&
                        !stringCellUse(instruction, index) && !callScopedStringUse(module, instruction, index))
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
        plan.plainValues.insert(plan.scalarFieldPlaces.begin(), plan.scalarFieldPlaces.end());
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

        // Cleanup is not an observation of a string's contents. It remains in
        // the output after a consumed operand is moved from. Borrow metadata and
        // CFG arguments ARE observations and must prevent last-use inference.
        std::unordered_map<std::uint32_t, std::size_t> semanticUses;
        std::unordered_map<std::uint32_t, wir::TypeId> concatTypes;
        for (const auto& parameter : function.parameters)
            concatTypes.emplace(parameter.id.value(), parameter.type);
        for (const auto& block : function.blocks)
        {
            for (const auto& parameter : block.parameters)
                concatTypes.emplace(parameter.id.value(), parameter.type);
            for (const auto& instruction : block.instructions)
            {
                if (instruction.result)
                    concatTypes.emplace(instruction.result.value(), instruction.resultType);
                if (instruction.opcode != wir::lowered::Opcode::DropValue &&
                    instruction.opcode != wir::lowered::Opcode::Release)
                    for (const auto operand : instruction.operands)
                        ++semanticUses[operand.value()];
                if (instruction.borrowOrigin)
                    ++semanticUses[instruction.borrowOrigin.value()];
                for (const auto& target : instruction.targets)
                    for (const auto argument : target.arguments)
                        ++semanticUses[argument.value()];
            }
        }

        // A local load can safely separate concat nodes: it cannot call a
        // native proxy getter. Calls, fields, reference parameters, projections,
        // stores and all unknown effects remain hard barriers.
        std::unordered_map<std::uint32_t, wir::TypeId> stableLocals;
        std::unordered_map<std::uint32_t, wir::TypeId> appendLocals;
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
            {
                const auto* type = module.types.tryGet(instruction.resultType);
                const bool localCell = instruction.opcode == wir::lowered::Opcode::LocalPlace;
                const bool directCell =
                    localCell ||
                    (instruction.opcode == wir::lowered::Opcode::FieldPlace && instruction.operands.size() == 1) ||
                    (instruction.opcode == wir::lowered::Opcode::GlobalPlace && instruction.global);
                if (directCell && instruction.result && type && type->kind == wir::TypeKind::Reference &&
                    type->arguments.size() == 1)
                {
                    if (localCell)
                        stableLocals.emplace(instruction.result.value(), type->arguments.front());
                    if (type->isMutable)
                        appendLocals.emplace(instruction.result.value(), type->arguments.front());
                }
            }
        for (const auto& block : function.blocks)
        {
            const auto& instructions = block.instructions;
            std::unordered_map<std::uint32_t, std::size_t> definitions;
            for (std::size_t index = 0; index < instructions.size(); ++index)
                if (instructions[index].result)
                    definitions.emplace(instructions[index].result.value(), index);

            const auto concat = [&](const wir::lowered::Instruction& instruction, const wir::TypeId type)
            {
                return instruction.opcode == wir::lowered::Opcode::Binary && instruction.result &&
                       instruction.resultType == type &&
                       instruction.binaryOperator == wir::typed::BinaryOperator::Add &&
                       instruction.operands.size() == 2 && concatTypes.contains(instruction.operands[0].value()) &&
                       concatTypes.contains(instruction.operands[1].value()) &&
                       concatTypes.at(instruction.operands[0].value()) == type &&
                       concatTypes.at(instruction.operands[1].value()) == type &&
                       instruction.resultOwnership == wir::typed::ValueOwnership::Owned;
            };
            for (std::size_t index = 1; index < instructions.size(); ++index)
            {
                const auto& store = instructions[index];
                if ((store.opcode != wir::lowered::Opcode::Store && store.opcode != wir::lowered::Opcode::Replace) ||
                    store.operands.size() != 2)
                    continue;
                const auto root = definitions.find(store.operands[1].value());
                if (root == definitions.end() || root->second >= index)
                    continue;
                const auto& add = instructions[root->second];
                const auto* type = module.types.tryGet(add.resultType);
                if (!type || (type->kind != wir::TypeKind::String && type->kind != wir::TypeKind::Text) ||
                    !concat(add, add.resultType) || semanticUses[add.result.value()] != 1)
                    continue;
                const auto local = appendLocals.find(store.operands[0].value());
                if (local == appendLocals.end() || local->second != add.resultType)
                    continue;

                std::vector<wir::ValueId> suffixes;
                std::unordered_set<std::uint32_t> sums;
                const auto* node = &add;
                while (concat(*node, add.resultType) && semanticUses[node->result.value()] == 1)
                {
                    // Bound generated variadic templates and proof work. Longer
                    // expressions retain the linear rvalue-buffer fallback.
                    if (sums.size() >= 64)
                    {
                        node = nullptr;
                        break;
                    }

                    sums.insert(node->result.value());
                    suffixes.push_back(node->operands[1]);
                    const auto previous = definitions.find(node->operands[0].value());
                    if (previous == definitions.end() || previous->second >= definitions.at(node->result.value()))
                    {
                        node = nullptr;
                        break;
                    }

                    node = &instructions[previous->second];
                }

                if (!node || node->opcode != wir::lowered::Opcode::Load || !node->result ||
                    node->resultType != add.resultType || node->operands.size() != 1 ||
                    node->resultOwnership != wir::typed::ValueOwnership::Borrowed ||
                    semanticUses[node->result.value()] != 1 || node->operands[0] != store.operands[0])
                    continue;

                bool safe = true;
                for (auto cursor = definitions.at(node->result.value()); cursor < index; ++cursor)
                {
                    const auto& step = instructions[cursor];
                    if (step.result && sums.contains(step.result.value()))
                        continue;
                    if ((step.opcode == wir::lowered::Opcode::DropValue ||
                         step.opcode == wir::lowered::Opcode::Release) &&
                        step.operands.size() == 1 && sums.contains(step.operands[0].value()))
                        continue;
                    if (&step == node)
                        continue;
                    if (step.opcode == wir::lowered::Opcode::Load && step.operands.size() == 1 &&
                        step.resultType == add.resultType && stableLocals.contains(step.operands[0].value()) &&
                        stableLocals.at(step.operands[0].value()) == step.resultType)
                        continue;
                    if (step.opcode == wir::lowered::Opcode::Constant && step.resultType == add.resultType)
                        continue;

                    safe = false;
                    break;
                }

                if (safe)
                {
                    std::reverse(suffixes.begin(), suffixes.end());
                    plan.localAppendSuffixes.emplace(add.result.value(), std::move(suffixes));
                    plan.elidedConcatValues.insert(sums.begin(), sums.end());
                }
            }

            // Flatten complete pure trees, including parenthesized RHS trees.
            // Visit roots in reverse so a larger tree claims its interiors first.
            // Retain every leaf evaluation at its original position. Unknown
            // effects between nodes forbid flattening (allocation failure must
            // not move across user-visible work).
            for (std::size_t reverse = instructions.size(); reverse > 0; --reverse)
            {
                const auto index = reverse - 1;
                const auto& add = instructions[index];
                const auto* type = module.types.tryGet(add.resultType);
                if (!type || (type->kind != wir::TypeKind::String && type->kind != wir::TypeKind::Text) ||
                    !concat(add, add.resultType) || plan.elidedConcatValues.contains(add.result.value()))
                    continue;

                std::vector<wir::ValueId> leaves;
                std::unordered_set<std::uint32_t> sums;
                bool safe = true;
                std::size_t begin = index;
                const auto visit = [&](const auto& self, const wir::ValueId id, const std::size_t before) -> void
                {
                    if (!safe)
                        return;
                    const auto found = definitions.find(id.value());
                    if (found == definitions.end() || found->second >= before)
                    {
                        safe = false;
                        return;
                    }

                    const auto& node = instructions[found->second];
                    if (concat(node, add.resultType) && semanticUses[id.value()] == 1 &&
                        !plan.elidedConcatValues.contains(id.value()))
                    {
                        if (sums.size() >= 64)
                        {
                            safe = false;
                            return;
                        }

                        sums.insert(id.value());
                        self(self, node.operands[0], found->second);
                        self(self, node.operands[1], found->second);
                        return;
                    }

                    if (node.resultType != add.resultType ||
                        !((node.opcode == wir::lowered::Opcode::Load && node.operands.size() == 1 &&
                           node.resultOwnership == wir::typed::ValueOwnership::Borrowed &&
                           stableLocals.contains(node.operands[0].value()) &&
                           stableLocals.at(node.operands[0].value()) == add.resultType) ||
                          (node.opcode == wir::lowered::Opcode::Constant && semanticUses[id.value()] == 1)))
                    {
                        safe = false;
                        return;
                    }

                    begin = std::min(begin, found->second);
                    leaves.push_back(id);
                };
                sums.insert(add.result.value());
                visit(visit, add.operands[0], index);
                visit(visit, add.operands[1], index);
                if (!safe || sums.size() < 2)
                    continue;
                std::unordered_set<std::uint32_t> leafIds;
                for (const auto leaf : leaves)
                    leafIds.insert(leaf.value());
                for (auto cursor = begin; cursor < index && safe; ++cursor)
                {
                    const auto& step = instructions[cursor];
                    if (step.result && (sums.contains(step.result.value()) || leafIds.contains(step.result.value())))
                        continue;
                    if ((step.opcode == wir::lowered::Opcode::DropValue ||
                         step.opcode == wir::lowered::Opcode::Release) &&
                        step.operands.size() == 1 && sums.contains(step.operands[0].value()))
                        continue;

                    safe = false;
                }

                if (safe)
                {
                    sums.erase(add.result.value());
                    plan.flattenedConcatOperands.emplace(add.result.value(), std::move(leaves));
                    plan.elidedConcatValues.insert(sums.begin(), sums.end());
                }
            }

            // Give the STL/Text rvalue overloads an independently owned last-use
            // buffer. Never consume a load alias optimized into a borrow, a
            // parameter, phi value, cross-block definition or coroutine value.
            for (std::size_t index = 0; index < instructions.size(); ++index)
            {
                const auto& add = instructions[index];
                const auto* type = module.types.tryGet(add.resultType);
                if (!type || (type->kind != wir::TypeKind::String && type->kind != wir::TypeKind::Text) ||
                    !concat(add, add.resultType) || plan.elidedConcatValues.contains(add.result.value()) ||
                    plan.flattenedConcatOperands.contains(add.result.value()))
                    continue;
                for (const auto operand : add.operands)
                {
                    const auto found = definitions.find(operand.value());
                    if (found == definitions.end() || found->second >= index || semanticUses[operand.value()] != 1 ||
                        plan.borrowedStringCopies.contains(operand.value()) ||
                        plan.elidedConcatValues.contains(operand.value()))
                        continue;
                    const auto& definition = instructions[found->second];
                    if (definition.resultType == add.resultType &&
                        definition.resultOwnership == wir::typed::ValueOwnership::Owned)
                        plan.consumedConcatOperands.insert(operand.value());
                }
            }
        }
        return plan;
    }
} // namespace wio::codegen
