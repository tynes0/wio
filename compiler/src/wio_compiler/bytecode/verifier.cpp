#include "wio/bytecode/verifier.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace wio::bytecode
{
    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code, std::string message, const std::uint32_t function = InvalidIndex,
                          const std::uint32_t block = InvalidIndex, const std::uint32_t instruction = InvalidIndex)
        { result.diagnostics_.push_back({std::move(code), std::move(message), function, block, instruction}); };
        auto validString = [&](const StringId id) { return id == InvalidIndex || id < module.strings.size(); };
        auto validType = [&](const std::uint32_t id) { return id == InvalidIndex || id < module.types.size(); };
        auto validFunction = [&](const std::uint32_t id)
        { return id == InvalidIndex || (id < module.functions.size() && module.functions[id].id == id); };
        const auto nominalType = [&](std::uint32_t id)
        {
            for (std::uint32_t depth = 0; id < module.types.size() && depth < 32; ++depth)
            {
                const Type& type = module.types[id];
                if (type.kind == 28)
                    return id;
                if ((type.kind != 29 && type.kind != 30) || type.arguments.empty())
                    break;
                id = type.arguments.front();
            }
            return InvalidIndex;
        };
        const auto mayOmitResult = [](const Opcode opcode)
        {
            return opcode == Opcode::Call || opcode == Opcode::NativeInvoke || opcode == Opcode::IndirectCall ||
                   opcode == Opcode::ExtensionCall || opcode == Opcode::MethodCall || opcode == Opcode::VirtualCall ||
                   opcode == Opcode::InterfaceCall || opcode == Opcode::IntrinsicCall;
        };
        const auto integerType = [&](const std::uint32_t id)
        {
            if (id >= module.types.size())
                return false;
            const std::uint8_t kind = module.types[id].kind;
            return (kind >= 3 && kind <= 12) || kind == 15 || kind == 16;
        };

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
            for (const std::uint32_t base : type.baseTypes)
            {
                if (!validType(base))
                    report("WBC1018", "Nominal base references an invalid type id");
            }
            for (const Type::Field& field : type.fields)
            {
                if (!validString(field.name) || !validType(field.type))
                    report("WBC1019", "Field layout references invalid string or type metadata");
            }
            for (const Type::Method& method : type.methods)
            {
                if (!validString(method.name) || !validType(method.returnType) || !validFunction(method.function))
                    report("WBC1020", "Method layout references invalid string, type, or function metadata");
                for (const std::uint32_t parameter : method.parameterTypes)
                {
                    if (!validType(parameter))
                        report("WBC1021", "Method parameter references an invalid type id");
                }
            }
            for (const Type::DispatchEntry& dispatch : type.dispatchEntries)
            {
                if (dispatch.contractType >= module.types.size() || dispatch.implementation >= module.functions.size())
                {
                    report("WBC1022", "Dispatch entry on type #" + std::to_string(i) +
                                          " references invalid contract #" + std::to_string(dispatch.contractType) +
                                          " or implementation #" + std::to_string(dispatch.implementation));
                    continue;
                }
                const Type& contract = module.types[dispatch.contractType];
                const auto method = std::find_if(contract.methods.begin(), contract.methods.end(),
                                                 [&](const auto& item) { return item.slot == dispatch.slot; });
                if (contract.kind != 28 || (contract.nominalKind != 2 && contract.nominalKind != 3) ||
                    method == contract.methods.end())
                    report("WBC1052", "Dispatch entry does not match an object/interface contract method slot");
            }
            for (const std::uint32_t castType : type.castTypes)
            {
                if (castType >= module.types.size() || module.types[castType].kind != 28)
                    report("WBC1054", "Object cast table references a non-nominal or invalid type");
            }
            if (!validFunction(type.destructor) || !validFunction(type.defaultConstructor) ||
                !validFunction(type.fieldInitializer) || !validType(type.enumUnderlyingType))
                report("WBC1023", "Type lifecycle or enum metadata contains an invalid id");
            for (const Type::EnumCase& enumCase : type.enumCases)
            {
                if (!validString(enumCase.name))
                    report("WBC1024", "Enum case references an invalid string id");
            }
            if (type.hasNativeBinding &&
                (!validString(type.nativeBinding.cppName) || !validString(type.nativeBinding.header)))
                report("WBC1025", "Native type binding references an invalid string id");
        }

        std::unordered_set<std::uint32_t> globalIds;
        for (std::size_t index = 0; index < module.globals.size(); ++index)
        {
            const Global& global = module.globals[index];
            if (global.id != index || !globalIds.insert(global.id).second)
                report("WBC1007", "Global ids must be valid and unique");
            if (!validString(global.name) || !validType(global.type))
                report("WBC1008", "Global references invalid string or type metadata");
        }

        std::unordered_set<std::uint32_t> functionIds;
        for (std::size_t functionIndex = 0; functionIndex < module.functions.size(); ++functionIndex)
        {
            const Function& function = module.functions[functionIndex];
            if (function.id != functionIndex || !functionIds.insert(function.id).second)
                report("WBC1009", "Function ids must be valid and unique", function.id);
            if (!validString(function.name) || !validString(function.specializationKey) ||
                !validType(function.returnType) || !validType(function.callableType) || !validType(function.ownerType))
                report("WBC1010", "Function references invalid string or type metadata", function.id);
            if (function.captureParameterCount != function.captures.size())
                report("WBC1026", "Function capture count does not match its capture table", function.id);
            for (const Function::Capture& capture : function.captures)
            {
                if (!validString(capture.name) || !validType(capture.type) || capture.kind > 2)
                    report("WBC1027", "Closure capture references invalid string or type metadata", function.id);
            }
            if (function.hasNativeBinding)
            {
                const Function::NativeBinding& binding = function.nativeBinding;
                if (!validString(binding.symbol) || !validString(binding.header) || !validString(binding.stableKey) ||
                    !validString(binding.thunkSymbol) || !validType(binding.result.type))
                    report("WBC1028", "Native binding references invalid string or type metadata", function.id);
                for (const Function::NativeAbiValue& parameter : binding.parameters)
                {
                    if (!validType(parameter.type))
                        report("WBC1029", "Native parameter references an invalid type id", function.id);
                }
                for (const std::uint32_t argument : binding.templateArguments)
                {
                    if (!validType(argument))
                        report("WBC1030", "Native template argument references an invalid type id", function.id);
                }
            }

            std::unordered_set<std::uint32_t> blockIds;
            std::unordered_map<std::uint32_t, const Block*> blocksById;
            for (const Block& block : function.blocks)
            {
                if (block.id == InvalidIndex || !blockIds.insert(block.id).second)
                    report("WBC1011", "Block ids must be valid and unique within a function", function.id, block.id);
                else
                    blocksById.emplace(block.id, &block);
            }
            const bool external = (function.flags & 0x0002u) != 0;
            if ((external && !function.blocks.empty()) || (!external && function.blocks.empty()))
                report("WBC1034", "External functions must have no code and defined functions must have code",
                       function.id);

            std::unordered_map<std::uint32_t, std::uint32_t> valueTypes;
            auto defineValue = [&](const Parameter& parameter, const std::uint32_t block)
            {
                if (parameter.value == InvalidIndex || !validType(parameter.type) ||
                    !valueTypes.emplace(parameter.value, parameter.type).second)
                    report("WBC1035", "Function value ids must be valid, typed, and unique", function.id, block);
                if (!validString(parameter.name))
                    report("WBC1036", "Function parameter references an invalid name", function.id, block);
            };
            for (const Parameter& parameter : function.parameters)
                defineValue(parameter, InvalidIndex);
            for (const Block& block : function.blocks)
            {
                for (const Parameter& parameter : block.parameters)
                    defineValue(parameter, block.id);
                for (const Instruction& instruction : block.instructions)
                {
                    if (!producesValue(instruction.opcode) || instruction.result == InvalidIndex)
                        continue;
                    if (instruction.result == InvalidIndex || !validType(instruction.resultType) ||
                        !valueTypes.emplace(instruction.result, instruction.resultType).second)
                        report("WBC1037", "Value-producing instruction requires a unique typed result", function.id,
                               block.id);
                }
            }
            if (function.hasCoroutine)
            {
                const bool async = (function.flags & 0x0001u) != 0;
                const Type* taskType =
                    function.returnType < module.types.size() ? &module.types[function.returnType] : nullptr;
                if (!async || !taskType || taskType->kind != 34 || taskType->arguments.size() != 1 ||
                    function.coroutine.resultType >= module.types.size() ||
                    taskType->arguments.front() != function.coroutine.resultType)
                    report("WBC1031", "Coroutine layout does not match its async task return type", function.id);
                std::unordered_set<std::uint32_t> frameValues;
                for (std::size_t index = 0; index < function.coroutine.frameSlots.size(); ++index)
                {
                    const Function::CoroutineFrameSlot& slot = function.coroutine.frameSlots[index];
                    const auto value = valueTypes.find(slot.value);
                    if (slot.slot != index || slot.type >= module.types.size() || value == valueTypes.end() ||
                        value->second != slot.type || !frameValues.insert(slot.value).second)
                        report("WBC1032", "Coroutine frame slot is not dense or does not match a live typed value",
                               function.id);
                }
                for (std::size_t index = 0; index < function.coroutine.states.size(); ++index)
                {
                    const Function::CoroutineState& state = function.coroutine.states[index];
                    if (!blockIds.contains(state.suspendBlock) || !blockIds.contains(state.resumeBlock) ||
                        state.index != index || state.resultType >= module.types.size() ||
                        (state.awaitedTask != InvalidIndex && !valueTypes.contains(state.awaitedTask)) ||
                        (state.resumedValue != InvalidIndex && !valueTypes.contains(state.resumedValue)))
                        report("WBC1033", "Coroutine state references invalid block or type metadata", function.id);
                }
            }
            else if ((function.flags & 0x0001u) != 0)
                report("WBC1058", "Async bytecode function is missing its coroutine layout", function.id);
            for (const Block& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    report("WBC1038", "Defined bytecode block cannot be empty", function.id, block.id);
                    continue;
                }
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    const std::uint32_t instructionIndex = static_cast<std::uint32_t>(index);
                    if (!isKnownOpcode(instruction.opcode))
                        report("WBC1012", "Instruction contains an unknown opcode", function.id, block.id,
                               instructionIndex);
                    if (!validType(instruction.resultType) || !validType(instruction.targetType))
                        report("WBC1013", "Instruction references an invalid type id", function.id, block.id,
                               instructionIndex);
                    if (instruction.constant >= module.constants.size())
                        report("WBC1014", "Instruction references an invalid constant id", function.id, block.id,
                               instructionIndex);
                    if (!validString(instruction.selector) || !validString(instruction.specializationKey))
                        report("WBC1015", "Instruction references an invalid string id", function.id, block.id,
                               instructionIndex);
                    if (isTerminator(instruction.opcode) != (index + 1 == block.instructions.size()))
                        report("WBC1039", "Every block must end in exactly one final terminator", function.id, block.id,
                               instructionIndex);
                    const bool voidResultAnnotation =
                        instruction.result == InvalidIndex && instruction.resultType != InvalidIndex &&
                        instruction.resultType < module.types.size() && module.types[instruction.resultType].kind == 1;
                    const bool omittedCallResult = instruction.result == InvalidIndex &&
                                                   mayOmitResult(instruction.opcode) &&
                                                   (instruction.resultType == InvalidIndex || voidResultAnnotation);
                    if ((!producesValue(instruction.opcode) && instruction.result != InvalidIndex) ||
                        (producesValue(instruction.opcode) && instruction.result == InvalidIndex && !omittedCallResult))
                        report("WBC1040", "Instruction result shape does not match its opcode", function.id, block.id,
                               instructionIndex);
                    if (instruction.result == InvalidIndex && instruction.resultType != InvalidIndex &&
                        !voidResultAnnotation)
                        report("WBC1041", "Non-value instruction cannot carry a result type", function.id, block.id,
                               instructionIndex);
                    if (!instruction.expandedOperands.empty() &&
                        instruction.expandedOperands.size() != instruction.operands.size())
                        report("WBC1042", "Pack expansion metadata must align with instruction operands", function.id,
                               block.id, instructionIndex);
                    if (instruction.callee != InvalidIndex && !validFunction(instruction.callee))
                        report("WBC1043", "Instruction references an invalid callee", function.id, block.id,
                               instructionIndex);
                    if (instruction.global != InvalidIndex &&
                        (instruction.global >= module.globals.size() ||
                         module.globals[instruction.global].id != instruction.global))
                        report("WBC1044", "Instruction references an invalid global", function.id, block.id,
                               instructionIndex);
                    for (const std::uint32_t operand : instruction.operands)
                    {
                        if (!valueTypes.contains(operand))
                            report("WBC1045", "Instruction references an undefined function value", function.id,
                                   block.id, instructionIndex);
                    }
                    for (const StringId segment : instruction.stringSegments)
                    {
                        if (!validString(segment))
                            report("WBC1016", "Interpolation segment references an invalid string id", function.id,
                                   block.id, instructionIndex);
                    }
                    for (const BranchTarget& target : instruction.targets)
                    {
                        if (!blockIds.contains(target.block))
                            report("WBC1017", "Branch target references a block outside the function", function.id,
                                   block.id, instructionIndex);
                        else
                        {
                            const Block& targetBlock = *blocksById.at(target.block);
                            if (target.arguments.size() != targetBlock.parameters.size())
                                report("WBC1046", "Branch argument count does not match target parameters", function.id,
                                       block.id, instructionIndex);
                            const std::size_t comparable =
                                (std::min)(target.arguments.size(), targetBlock.parameters.size());
                            for (std::size_t argumentIndex = 0; argumentIndex < comparable; ++argumentIndex)
                            {
                                const auto value = valueTypes.find(target.arguments[argumentIndex]);
                                if (value == valueTypes.end() ||
                                    value->second != targetBlock.parameters[argumentIndex].type)
                                    report("WBC1047", "Branch argument type does not match target parameter",
                                           function.id, block.id, instructionIndex);
                            }
                        }
                    }
                    const bool validBranchShape =
                        (instruction.opcode != Opcode::Jump || instruction.targets.size() == 1) &&
                        (instruction.opcode != Opcode::CondJump ||
                         (instruction.targets.size() == 2 && instruction.operands.size() == 1)) &&
                        (instruction.opcode != Opcode::Return || instruction.operands.size() <= 1) &&
                        (instruction.opcode != Opcode::Unreachable ||
                         (instruction.operands.empty() && instruction.targets.empty()));
                    if (!validBranchShape)
                        report("WBC1048", "Control-flow instruction has an invalid operand or target shape",
                               function.id, block.id, instructionIndex);

                    const auto valueType = [&](const std::uint32_t value)
                    {
                        const auto found = valueTypes.find(value);
                        return found == valueTypes.end() ? InvalidIndex : found->second;
                    };
                    if (instruction.opcode == Opcode::GenericConstant)
                    {
                        report("WBC1063", "Executable bytecode cannot contain an unresolved generic constant",
                               function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::EnumConstant)
                    {
                        const Type* target = instruction.targetType < module.types.size()
                                                 ? &module.types[instruction.targetType]
                                                 : nullptr;
                        const std::string_view selector = module.string(instruction.selector);
                        const bool knownCase =
                            target && std::any_of(target->enumCases.begin(), target->enumCases.end(),
                                                  [&](const Type::EnumCase& candidate)
                                                  { return module.string(candidate.name) == selector; });
                        if (!target || target->kind != 28 || (target->nominalKind != 4 && target->nominalKind != 5) ||
                            instruction.resultType != instruction.targetType || !instruction.operands.empty() ||
                            !knownCase)
                            report("WBC1064", "Enum constant does not match a declared enum/flagset case", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::AnyBox)
                    {
                        const bool valid =
                            instruction.operands.size() == 1 && instruction.resultType < module.types.size() &&
                            module.types[instruction.resultType].kind == 19 &&
                            instruction.targetType == valueType(instruction.operands.front()) &&
                            instruction.signatureTypes == std::vector<std::uint32_t>{instruction.targetType};
                        if (!valid)
                            report("WBC1065", "Any box does not preserve its concrete source type", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::AnyCheckedCast || instruction.opcode == Opcode::AnyTypeTest)
                    {
                        const std::uint32_t source =
                            instruction.operands.size() == 1 ? valueType(instruction.operands.front()) : InvalidIndex;
                        const bool validResult = instruction.opcode == Opcode::AnyTypeTest
                                                     ? instruction.resultType < module.types.size() &&
                                                           module.types[instruction.resultType].kind == 2
                                                     : instruction.resultType == instruction.targetType;
                        if (source >= module.types.size() || module.types[source].kind != 19 ||
                            instruction.targetType >= module.types.size() || !validResult)
                            report("WBC1066", "Any test/cast has invalid source, target, or result metadata",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::NullableWrap)
                    {
                        const Type* resultType = instruction.resultType < module.types.size()
                                                     ? &module.types[instruction.resultType]
                                                     : nullptr;
                        const bool valid = instruction.operands.size() == 1 && resultType && resultType->kind == 30 &&
                                           resultType->arguments.size() == 1 &&
                                           resultType->arguments.front() == valueType(instruction.operands.front()) &&
                                           instruction.targetType == instruction.resultType;
                        if (!valid)
                            report("WBC1067", "Nullable wrap does not match its payload type", function.id, block.id,
                                   instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::NullableUnwrap)
                    {
                        const std::uint32_t source =
                            instruction.operands.size() == 1 ? valueType(instruction.operands.front()) : InvalidIndex;
                        const Type* nullable = source < module.types.size() ? &module.types[source] : nullptr;
                        if (!nullable || nullable->kind != 30 || nullable->arguments.size() != 1 ||
                            nullable->arguments.front() != instruction.resultType ||
                            instruction.targetType != instruction.resultType)
                            report("WBC1068", "Nullable unwrap does not match its payload result type", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::VariantTest || instruction.opcode == Opcode::VariantPayload)
                    {
                        const std::uint32_t source =
                            instruction.operands.size() == 1 ? valueType(instruction.operands.front()) : InvalidIndex;
                        const Type* variant = source < module.types.size() ? &module.types[source] : nullptr;
                        const std::string_view selector = module.string(instruction.selector);
                        const bool optionSelector = selector == "Some" || selector == "None";
                        const bool resultSelector = selector == "Ok" || selector == "Err";
                        bool valid = variant && variant->kind == 28 && variant->fields.size() >= 2 &&
                                     ((variant->nominalValueModel == 3 && optionSelector) ||
                                      (variant->nominalValueModel == 4 && resultSelector));
                        if (valid && instruction.opcode == Opcode::VariantTest)
                            valid = instruction.resultType < module.types.size() &&
                                    module.types[instruction.resultType].kind == 2;
                        else if (valid)
                        {
                            const std::size_t field = selector == "Err" ? 2 : 1;
                            valid =
                                field < variant->fields.size() && instruction.resultType == variant->fields[field].type;
                        }
                        if (!valid)
                            report("WBC1069", "Variant operation does not match its option/result layout", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::IteratorCreate)
                    {
                        const Type* iterator = instruction.resultType < module.types.size()
                                                   ? &module.types[instruction.resultType]
                                                   : nullptr;
                        const std::string_view selector = module.string(instruction.selector);
                        const bool range = selector == "range.inclusive" || selector == "range.exclusive";
                        const bool container = selector == "array" || selector == "dictionary";
                        bool valid = iterator && iterator->kind == 35 && iterator->arguments.size() == 1 &&
                                     (range || container) &&
                                     instruction.signatureTypes.size() == instruction.operands.size();
                        for (std::size_t operand = 0; valid && operand < instruction.operands.size(); ++operand)
                            valid = valueType(instruction.operands[operand]) == instruction.signatureTypes[operand];
                        if (valid && range)
                            valid = instruction.operands.size() == 3 && integerType(iterator->arguments.front()) &&
                                    std::all_of(instruction.operands.begin(), instruction.operands.end(),
                                                [&](const std::uint32_t value)
                                                { return valueType(value) == iterator->arguments.front(); });
                        if (valid && container)
                        {
                            const std::uint32_t source =
                                instruction.operands.empty() ? InvalidIndex : valueType(instruction.operands.front());
                            const std::uint32_t directSource = source < module.types.size() &&
                                                                       module.types[source].kind == 29 &&
                                                                       module.types[source].arguments.size() == 1
                                                                   ? module.types[source].arguments.front()
                                                                   : source;
                            const std::uint8_t expected = selector == "array" ? 31 : 32;
                            const bool validShape = selector == "array" ? instruction.operands.size() == 1 ||
                                                                              instruction.operands.size() == 2
                                                                        : instruction.operands.size() == 1;
                            valid = validShape && directSource < module.types.size() &&
                                    module.types[directSource].kind == expected &&
                                    directSource == iterator->arguments.front();
                            if (valid && instruction.operands.size() == 2)
                                valid = integerType(valueType(instruction.operands[1]));
                        }
                        if (!valid)
                            report("WBC1070", "Iterator creation has invalid source or signature metadata", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::IteratorHasNext ||
                             instruction.opcode == Opcode::IteratorValue ||
                             instruction.opcode == Opcode::IteratorAdvance)
                    {
                        const std::uint32_t source =
                            instruction.operands.size() == 1 ? valueType(instruction.operands.front()) : InvalidIndex;
                        const Type* iterator = source < module.types.size() ? &module.types[source] : nullptr;
                        bool valid = iterator && iterator->kind == 35;
                        if (instruction.opcode == Opcode::IteratorHasNext)
                            valid = valid && instruction.resultType < module.types.size() &&
                                    module.types[instruction.resultType].kind == 2;
                        else if (instruction.opcode == Opcode::IteratorAdvance)
                            valid = valid && instruction.result == InvalidIndex;
                        else
                            valid = valid && instruction.result != InvalidIndex &&
                                    instruction.selector != InvalidIndex &&
                                    !module.string(instruction.selector).empty();
                        if (!valid)
                            report("WBC1071", "Iterator operation has invalid cursor or result metadata", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::ResultIsError || instruction.opcode == Opcode::ResultValue ||
                             instruction.opcode == Opcode::ResultUnwrap ||
                             instruction.opcode == Opcode::ResultPropagate)
                    {
                        const std::uint32_t source =
                            instruction.operands.size() == 1 ? valueType(instruction.operands.front()) : InvalidIndex;
                        const Type* resultType = source < module.types.size() ? &module.types[source] : nullptr;
                        bool valid = resultType && resultType->kind == 28 && resultType->nominalValueModel == 4 &&
                                     !resultType->arguments.empty() && resultType->fields.size() >= 3;
                        if (valid && instruction.opcode == Opcode::ResultIsError)
                            valid = instruction.resultType < module.types.size() &&
                                    module.types[instruction.resultType].kind == 2;
                        else if (valid && (instruction.opcode == Opcode::ResultValue ||
                                           instruction.opcode == Opcode::ResultUnwrap))
                            valid = instruction.resultType == resultType->arguments.front();
                        else if (valid)
                        {
                            const std::uint32_t functionResult =
                                function.hasCoroutine ? function.coroutine.resultType : function.returnType;
                            const Type* target = instruction.targetType < module.types.size()
                                                     ? &module.types[instruction.targetType]
                                                     : nullptr;
                            valid = instruction.result == InvalidIndex && instruction.targetType == functionResult &&
                                    target && target->kind == 28 && target->nominalValueModel == 4;
                        }
                        if (!valid)
                            report("WBC1072", "Result operation does not match its canonical result layout",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::FunctionReference)
                    {
                        const bool validCallee =
                            instruction.callee != InvalidIndex && validFunction(instruction.callee);
                        const Function* callee = validCallee ? &module.functions[instruction.callee] : nullptr;
                        const bool validCallable = instruction.resultType < module.types.size() &&
                                                   module.types[instruction.resultType].kind == 33;
                        if (!callee || !validCallable || !instruction.operands.empty() ||
                            (!callee->genericParameters.empty() ? instruction.specializationKey == InvalidIndex
                                                                : callee->callableType != instruction.resultType))
                            report("WBC1049", "Function reference does not match a known callable declaration",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::ClosureCreate)
                    {
                        const bool validCallee =
                            instruction.callee != InvalidIndex && validFunction(instruction.callee);
                        const Function* callee = validCallee ? &module.functions[instruction.callee] : nullptr;
                        bool valid = callee && instruction.resultType < module.types.size() &&
                                     module.types[instruction.resultType].kind == 33 &&
                                     callee->callableType == instruction.resultType &&
                                     instruction.operands.size() == instruction.signatureTypes.size() &&
                                     instruction.operands.size() == instruction.captureKinds.size() &&
                                     instruction.operands.size() == callee->captureParameterCount &&
                                     instruction.operands.size() == callee->captures.size();
                        if (valid)
                        {
                            for (std::size_t captureIndex = 0; captureIndex < instruction.operands.size();
                                 ++captureIndex)
                            {
                                valid =
                                    valid && instruction.captureKinds[captureIndex] <= 2 &&
                                    instruction.captureKinds[captureIndex] == callee->captures[captureIndex].kind &&
                                    instruction.signatureTypes[captureIndex] == callee->captures[captureIndex].type &&
                                    valueType(instruction.operands[captureIndex]) ==
                                        instruction.signatureTypes[captureIndex];
                            }
                        }
                        if (!valid)
                            report("WBC1050", "Closure creation does not match its callable capture layout",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::IndirectCall)
                    {
                        const std::uint32_t callableType =
                            instruction.operands.empty() ? InvalidIndex : valueType(instruction.operands.front());
                        const Type* callable =
                            callableType < module.types.size() ? &module.types[callableType] : nullptr;
                        bool valid = callable && callable->kind == 33 && !callable->arguments.empty() &&
                                     callable->arguments.size() == instruction.operands.size() &&
                                     instruction.signatureTypes.size() == instruction.operands.size() &&
                                     instruction.signatureTypes.front() == callableType;
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size();
                                 ++argumentIndex)
                            {
                                valid =
                                    valid &&
                                    valueType(instruction.operands[argumentIndex]) ==
                                        instruction.signatureTypes[argumentIndex] &&
                                    instruction.signatureTypes[argumentIndex] == callable->arguments[argumentIndex - 1];
                            }
                            const std::uint32_t returnType = callable->arguments.back();
                            valid =
                                valid && returnType < module.types.size() &&
                                (module.types[returnType].kind == 1
                                     ? instruction.result == InvalidIndex && (instruction.resultType == InvalidIndex ||
                                                                              instruction.resultType == returnType)
                                     : instruction.result != InvalidIndex && instruction.resultType == returnType);
                        }
                        if (!valid)
                            report("WBC1051", "Indirect call does not match its function value signature", function.id,
                                   block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::VirtualCall || instruction.opcode == Opcode::InterfaceCall)
                    {
                        const std::uint32_t target = nominalType(instruction.targetType);
                        const Type* contract = target < module.types.size() ? &module.types[target] : nullptr;
                        const std::uint8_t expectedKind = instruction.opcode == Opcode::VirtualCall ? 2 : 3;
                        const Type::Method* method = nullptr;
                        if (contract)
                        {
                            const auto found =
                                std::find_if(contract->methods.begin(), contract->methods.end(), [&](const auto& item)
                                             { return item.slot == instruction.projectionIndex; });
                            if (found != contract->methods.end())
                                method = &*found;
                        }
                        const Function* callee = instruction.callee < module.functions.size()
                                                     ? &module.functions[instruction.callee]
                                                     : nullptr;
                        const std::uint32_t receiver = instruction.operands.empty()
                                                           ? InvalidIndex
                                                           : nominalType(valueType(instruction.operands.front()));
                        const bool receiverCompatible =
                            receiver < module.types.size() &&
                            (receiver == target ||
                             std::find(module.types[receiver].castTypes.begin(), module.types[receiver].castTypes.end(),
                                       target) != module.types[receiver].castTypes.end());
                        const bool declarationMatches =
                            method && callee &&
                            (method->function == instruction.callee || callee->genericOrigin == method->function);
                        const bool signatureMatches =
                            instruction.signatureTypes.empty() ||
                            (instruction.signatureTypes.size() == instruction.operands.size() &&
                             std::all_of(instruction.signatureTypes.begin(), instruction.signatureTypes.end(),
                                         [&](const auto type) { return type < module.types.size(); }));
                        if (!contract || contract->kind != 28 || contract->nominalKind != expectedKind || !method ||
                            !receiverCompatible || !declarationMatches || !signatureMatches)
                            report("WBC1055",
                                   "Dynamic call does not match its receiver, contract, slot, or declaration",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::Upcast || instruction.opcode == Opcode::CheckedCast ||
                             instruction.opcode == Opcode::TypeTest)
                    {
                        const std::uint32_t source = instruction.operands.size() == 1
                                                         ? nominalType(valueType(instruction.operands.front()))
                                                         : InvalidIndex;
                        const std::uint32_t target = nominalType(instruction.targetType);
                        const bool targetKnown = target < module.types.size() && module.types[target].kind == 28;
                        const bool staticallyCompatible =
                            instruction.opcode != Opcode::Upcast ||
                            (source < module.types.size() && targetKnown &&
                             (source == target ||
                              std::find(module.types[source].castTypes.begin(), module.types[source].castTypes.end(),
                                        target) != module.types[source].castTypes.end()));
                        if (source >= module.types.size() || !targetKnown || !staticallyCompatible)
                            report("WBC1056", "Object cast or type-test has invalid source or target metadata",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::IdentityEqual)
                    {
                        const bool valid = instruction.operands.size() == 2 &&
                                           nominalType(valueType(instruction.operands[0])) < module.types.size() &&
                                           nominalType(valueType(instruction.operands[1])) < module.types.size() &&
                                           (instruction.binaryOperator == 5 || instruction.binaryOperator == 6);
                        if (!valid)
                            report("WBC1057", "Object identity comparison has invalid operand or operator metadata",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::CancellationCheck)
                    {
                        const bool followedBySuspend =
                            index + 1 < block.instructions.size() &&
                            block.instructions[index + 1].opcode == Opcode::CoroutineSuspend &&
                            block.instructions[index + 1].projectionIndex == instruction.projectionIndex;
                        if (!function.hasCoroutine || !instruction.operands.empty() || !followedBySuspend)
                            report("WBC1059", "Cancellation check must precede its matching coroutine suspension",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::CoroutineSuspend)
                    {
                        const Function::CoroutineState* state =
                            function.hasCoroutine && instruction.projectionIndex < function.coroutine.states.size()
                                ? &function.coroutine.states[instruction.projectionIndex]
                                : nullptr;
                        const bool precededByCheck =
                            index > 0 && block.instructions[index - 1].opcode == Opcode::CancellationCheck &&
                            block.instructions[index - 1].projectionIndex == instruction.projectionIndex;
                        bool operationValid = false;
                        if (state && instruction.asyncOperation == 1 && instruction.operands.size() == 1)
                        {
                            const std::uint32_t taskId = valueType(instruction.operands.front());
                            const Type* task = taskId < module.types.size() ? &module.types[taskId] : nullptr;
                            operationValid = task && task->kind == 34 && task->arguments.size() == 1 &&
                                             task->arguments.front() == state->resultType &&
                                             state->awaitedTask == instruction.operands.front();
                        }
                        else if (state && instruction.asyncOperation == 2 && instruction.operands.empty())
                        {
                            operationValid = instruction.asyncExecutor >= 1 && instruction.asyncExecutor <= 4 &&
                                             state->awaitedTask == InvalidIndex &&
                                             state->resultType < module.types.size() &&
                                             module.types[state->resultType].kind == 1;
                        }
                        if (!state || state->suspendBlock != block.id || instruction.targets.size() != 1 ||
                            instruction.targets.front().block != state->resumeBlock ||
                            state->executor != instruction.asyncExecutor || !state->cancellationPoint ||
                            !precededByCheck || !operationValid)
                            report("WBC1060", "Coroutine suspension does not match its canonical state and resume edge",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::CoroutineResume)
                    {
                        const Function::CoroutineState* state =
                            function.hasCoroutine && instruction.projectionIndex < function.coroutine.states.size()
                                ? &function.coroutine.states[instruction.projectionIndex]
                                : nullptr;
                        if (!state || state->resumeBlock != block.id || !instruction.operands.empty() ||
                            instruction.result != state->resumedValue || instruction.resultType != state->resultType)
                            report("WBC1061", "Coroutine resume does not materialize its canonical state payload",
                                   function.id, block.id, instructionIndex);
                    }
                    else if (instruction.opcode == Opcode::CoroutineComplete)
                    {
                        const std::uint32_t resultType =
                            function.hasCoroutine ? function.coroutine.resultType : InvalidIndex;
                        const bool returnsVoid = resultType < module.types.size() && module.types[resultType].kind == 1;
                        const bool valid = function.hasCoroutine && resultType < module.types.size() &&
                                           (returnsVoid ? instruction.operands.empty()
                                                        : instruction.operands.size() == 1 &&
                                                              valueType(instruction.operands.front()) == resultType);
                        if (!valid)
                            report("WBC1062", "Coroutine completion does not match its async payload type", function.id,
                                   block.id, instructionIndex);
                    }
                }
            }
        }

        for (const Import& import : module.contract.imports)
        {
            if (import.stableId == 0 || !validString(import.logicalName) || !validString(import.sourcePath) ||
                !validString(import.alias))
                report("WBC1100", "Module import contains invalid identity or string metadata");
            for (const StringId symbol : import.importedSymbols)
            {
                if (!validString(symbol))
                    report("WBC1101", "Module import symbol references an invalid string id");
            }
        }
        for (const Export& exportRecord : module.contract.exports)
        {
            if (exportRecord.stableId == 0 || !validString(exportRecord.stableKey) ||
                !validString(exportRecord.logicalName) || !validString(exportRecord.symbolName) ||
                !validString(exportRecord.roleName) || !validFunction(exportRecord.function) ||
                !validType(exportRecord.type) || !validType(exportRecord.returnType))
                report("WBC1102", "Module export contains invalid identity, symbol, or target metadata");
            for (const std::uint32_t type : exportRecord.parameterTypes)
            {
                if (!validType(type))
                    report("WBC1103", "Module export parameter references an invalid type id");
            }
        }
        for (const AttributeApplication& attribute : module.contract.attributes)
        {
            if (attribute.stableId == 0 || !validString(attribute.canonicalName) ||
                !validString(attribute.originParent) || !validString(attribute.selector) ||
                !validType(attribute.targetType) || !validFunction(attribute.targetFunction))
                report("WBC1104", "Attribute application contains invalid identity or target metadata");
            for (const AttributeArgument& argument : attribute.arguments)
            {
                if (!validString(argument.name) || !validString(argument.sourceText) || !validType(argument.type))
                    report("WBC1105", "Attribute argument contains invalid string or type metadata");
            }
            for (const AttributeProcessor& processor : attribute.processors)
            {
                if (processor.stableId == 0 || !validString(processor.canonicalTypeName) ||
                    !validString(processor.hookName) || !validString(processor.hookMode) ||
                    !validType(processor.processorType) || !validFunction(processor.hookFunction) ||
                    !validType(processor.valueType))
                    report("WBC1106", "Attribute processor contains invalid identity or hook metadata");
            }
        }
        for (const Reflection& reflection : module.contract.reflection)
        {
            if (reflection.stableTypeId == 0 || !validString(reflection.logicalName) || !validType(reflection.type))
                report("WBC1107", "Reflection descriptor contains invalid identity or type metadata");
            for (const StringId name : reflection.genericParameterNames)
            {
                if (!validString(name))
                    report("WBC1108", "Reflection generic parameter references an invalid string id");
            }
            for (const ReflectedField& field : reflection.fields)
            {
                if (field.stableId == 0 || !validString(field.name) || !validType(field.type))
                    report("WBC1109", "Reflected field contains invalid identity or type metadata");
            }
            for (const ReflectedMethod& method : reflection.methods)
            {
                if (method.stableId == 0 || !validString(method.name) || !validFunction(method.function) ||
                    !validType(method.returnType))
                    report("WBC1110", "Reflected method contains invalid identity or callable metadata");
            }
            for (const ReflectedCase& enumCase : reflection.cases)
            {
                if (enumCase.stableId == 0 || !validString(enumCase.name))
                    report("WBC1111", "Reflected case contains invalid identity or string metadata");
            }
        }
        for (const System& system : module.contract.systems)
        {
            if (system.stableId == 0 || !validString(system.logicalName) || !validType(system.type) ||
                !validFunction(system.start) || !validFunction(system.update) || !validFunction(system.close))
                report("WBC1112", "System descriptor contains invalid lifecycle metadata");
        }
        if (module.contract.hasApplication)
        {
            const Application& application = module.contract.application;
            if (application.stableId == 0 || !validString(application.logicalName) || !validType(application.type) ||
                !validFunction(application.construct) || !validFunction(application.entry) ||
                !validFunction(application.start) || !validFunction(application.update) ||
                !validFunction(application.close) || !validFunction(application.exit))
                report("WBC1113", "Application descriptor contains invalid lifecycle metadata");
            for (const Stage& stage : application.stages)
            {
                if (stage.stableId == 0 || !validString(stage.name) || !validString(stage.after))
                    report("WBC1114", "Application stage contains invalid identity or string metadata");
                for (const StageRun& run : stage.runs)
                {
                    if (!validString(run.targetName) || !validString(run.methodName) || !validType(run.targetType) ||
                        !validFunction(run.function))
                        report("WBC1115", "Application stage run contains invalid target metadata");
                    for (const ResourceBinding& resource : run.resources)
                    {
                        if (!validString(resource.name) || !validType(resource.type))
                            report("WBC1116", "Application resource contains invalid string or type metadata");
                    }
                }
            }
        }
        const Lifecycle& lifecycle = module.contract.lifecycle;
        if (!validFunction(lifecycle.apiVersion) || !validFunction(lifecycle.load) ||
            !validFunction(lifecycle.update) || !validFunction(lifecycle.unload) ||
            !validFunction(lifecycle.saveState) || !validFunction(lifecycle.restoreState))
            report("WBC1117", "Module lifecycle references an invalid function id");
        if (module.abiDescriptorVersion == 0 || module.contract.callTableStableId == 0 ||
            module.contract.callTableEntries.size() != module.contract.exports.size())
            report("WBC1118", "SDK call table identity or export cardinality is invalid");
        return result;
    }
} // namespace wio::bytecode
