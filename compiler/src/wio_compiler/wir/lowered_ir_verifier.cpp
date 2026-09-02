#include "wio/wir/lowered_ir_verifier.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace wio::wir::lowered
{
    namespace
    {
        using FunctionMap = std::unordered_map<FunctionId::ValueType, const Function*>;
        using BlockMap = std::unordered_map<BlockId::ValueType, const BasicBlock*>;
        using ValueTypeMap = std::unordered_map<ValueId::ValueType, TypeId>;

        bool isComparison(const typed::BinaryOperator op)
        {
            return op == typed::BinaryOperator::Equal || op == typed::BinaryOperator::NotEqual ||
                   op == typed::BinaryOperator::Less || op == typed::BinaryOperator::LessEqual ||
                   op == typed::BinaryOperator::Greater || op == typed::BinaryOperator::GreaterEqual;
        }

        bool isNumeric(const TypeKind kind)
        {
            return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                   kind == TypeKind::I64 || kind == TypeKind::ISize ||
                   kind == TypeKind::U8 || kind == TypeKind::U16 || kind == TypeKind::U32 ||
                   kind == TypeKind::U64 || kind == TypeKind::USize ||
                   kind == TypeKind::F32 || kind == TypeKind::F64;
        }

        bool isInteger(const TypeKind kind)
        {
            return isNumeric(kind) && kind != TypeKind::F32 && kind != TypeKind::F64;
        }

        TypeId coroutineResultType(const TypeTable& types, const Function& function)
        {
            if (!function.isAsync)
                return function.returnType;
            const Type* task = types.tryGet(function.returnType);
            return task && task->kind == TypeKind::AsyncTask && task->arguments.size() == 1
                ? task->arguments.front()
                : TypeId{};
        }

        const FieldLayout* findFieldLayout(
            const TypeTable& types,
            const Type& owner,
            const std::string_view name,
            std::unordered_set<TypeId::ValueType>& visited)
        {
            const auto field = std::ranges::find_if(
                owner.fields,
                [&](const FieldLayout& layout) { return layout.name == name; });
            if (field != owner.fields.end())
                return &*field;
            for (const TypeId baseTypeId : owner.baseTypes)
            {
                if (!baseTypeId || !visited.insert(baseTypeId.value()).second)
                    continue;
                const Type* baseType = types.tryGet(baseTypeId);
                if (baseType)
                {
                    if (const FieldLayout* inherited = findFieldLayout(types, *baseType, name, visited))
                        return inherited;
                }
            }
            return nullptr;
        }

        const Type* underlyingObjectType(const TypeTable& types, TypeId typeId, TypeId* nominalId = nullptr)
        {
            const Type* type = types.tryGet(typeId);
            if (type && type->kind == TypeKind::Reference && type->arguments.size() == 1)
            {
                typeId = type->arguments.front();
                type = types.tryGet(typeId);
            }
            if (!type || type->kind != TypeKind::Named ||
                (type->nominalKind != NominalKind::Object && type->nominalKind != NominalKind::Interface))
                return nullptr;
            if (nominalId)
                *nominalId = typeId;
            return type;
        }

        bool nominalDerivesFrom(
            const TypeTable& types,
            const TypeId source,
            const TypeId destination,
            std::unordered_set<TypeId::ValueType>& visited)
        {
            if (source == destination)
                return true;
            if (!source || !visited.insert(source.value()).second)
                return false;
            const Type* type = types.tryGet(source);
            return type && std::ranges::any_of(type->baseTypes, [&](const TypeId base)
                { return nominalDerivesFrom(types, base, destination, visited); });
        }

        bool validNativeAbiValue(const TypeTable& types, const NativeAbiValue& value)
        {
            const Type* type = types.tryGet(value.type);
            if (!type || value.nullable != (type->kind == TypeKind::Nullable))
                return false;
            if (value.passing == NativePassingMode::Borrow || value.passing == NativePassingMode::BorrowMut)
            {
                if (type->kind != TypeKind::Reference || type->arguments.size() != 1 ||
                    ((value.passing == NativePassingMode::BorrowMut) != type->isMutable))
                    return false;
            }
            const Type* base = type;
            while (base && (base->kind == TypeKind::Reference || base->kind == TypeKind::Nullable) &&
                   base->arguments.size() == 1)
                base = types.tryGet(base->arguments.front());
            if (!base)
                return false;
            switch (value.marshalling)
            {
            case NativeMarshallingKind::Void: return base->kind == TypeKind::Void;
            case NativeMarshallingKind::Utf8String: return base->kind == TypeKind::String;
            case NativeMarshallingKind::UnicodeText: return base->kind == TypeKind::Text;
            case NativeMarshallingKind::NativePod:
                return base->nominalRepresentation == NominalRepresentation::NativePod;
            case NativeMarshallingKind::OpaqueHandle: return base->kind == TypeKind::Opaque;
            case NativeMarshallingKind::ObjectHandle:
                return base->kind == TypeKind::Named &&
                    (base->nominalKind == NominalKind::Object || base->nominalKind == NominalKind::Interface);
            case NativeMarshallingKind::Callback: return base->kind == TypeKind::Function;
            case NativeMarshallingKind::Generic: return base->kind == TypeKind::GenericParameter;
            case NativeMarshallingKind::Scalar:
                if (base->kind == TypeKind::Named)
                    return base->nominalKind == NominalKind::Enum || base->nominalKind == NominalKind::Flagset;
                return base->kind != TypeKind::Void && base->kind != TypeKind::String &&
                    base->kind != TypeKind::Text && base->kind != TypeKind::Any &&
                    base->kind != TypeKind::Opaque && base->kind != TypeKind::Function &&
                    base->kind != TypeKind::Array && base->kind != TypeKind::Dictionary &&
                    base->kind != TypeKind::AsyncTask;
            case NativeMarshallingKind::RuntimeValue:
                return base->kind == TypeKind::Any || base->kind == TypeKind::Array ||
                    base->kind == TypeKind::Dictionary || base->kind == TypeKind::AsyncTask ||
                    base->kind == TypeKind::Named;
            }
            return false;
        }
    }

    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code,
                          std::string message,
                          const SourceSpan& source = {},
                          const FunctionId function = {},
                          const BlockId block = {})
        {
            result.diagnostics_.push_back(VerificationDiagnostic{
                .code = std::move(code),
                .message = std::move(message),
                .source = source,
                .function = function,
                .block = block
            });
        };

        if (module.name.empty())
            report("LIR1000", "Lowered WIR module name cannot be empty.");
        for (const Type& type : module.types.types())
        {
            if (type.kind == TypeKind::Invalid)
                report("LIR1001", "Lowered WIR type table contains an invalid type.");
            for (const TypeId argument : type.arguments)
            {
                if (!module.types.tryGet(argument))
                    report("LIR1002", "Lowered WIR type references an unknown type id.");
            }
            if (type.kind != TypeKind::Named && type.nominalKind != NominalKind::None)
                report("LIR1003", "Only named Lowered WIR types may carry a nominal kind.");
            if (type.kind != TypeKind::Named && type.nominalValueModel != NominalValueModel::Regular)
                report("LIR1009", "Only named Lowered WIR types may carry a nominal value model.");
            if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                (type.kind != TypeKind::Named || type.nominalKind != NominalKind::Component))
            {
                report("LIR1004", "Native POD representation requires a named component type.");
            }
            if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                (!type.nativeBinding || type.nativeBinding->cppName.empty()))
                report("LIR1010", "Native POD types require canonical C++ type binding metadata.");
            if (type.nativeBinding && type.nominalRepresentation != NominalRepresentation::NativePod)
                report("LIR1011", "Only Native POD types may carry native type binding metadata.");
            if (type.kind != TypeKind::Named &&
                (!type.baseTypes.empty() || !type.fields.empty() || !type.methods.empty() ||
                 type.hasConstructor || type.hasDestructor))
            {
                report("LIR1005", "Only named Lowered WIR types may carry layout or lifecycle metadata.");
            }
            std::unordered_set<std::string> fieldNames;
            for (const FieldLayout& field : type.fields)
            {
                if (field.name.empty() || !module.types.tryGet(field.type) || !fieldNames.insert(field.name).second)
                    report("LIR1006", "Lowered WIR field layouts require unique names and valid field types.");
            }
            for (const TypeId baseTypeId : type.baseTypes)
            {
                const Type* baseType = module.types.tryGet(baseTypeId);
                if (!baseType || baseType->kind != TypeKind::Named)
                    report("LIR1007", "Lowered WIR nominal base layouts must reference named types.");
            }
            std::unordered_set<std::uint32_t> methodSlots;
            for (const MethodLayout& method : type.methods)
            {
                const bool signatureValid = module.types.tryGet(method.returnType) &&
                    std::ranges::all_of(method.parameterTypes, [&](const TypeId parameter)
                        { return module.types.tryGet(parameter) != nullptr; });
                if (method.name.empty() || !method.function || !signatureValid ||
                    !methodSlots.insert(method.slot).second)
                    report("LIR1008", "Lowered WIR method layouts require valid signatures and unique slots.");
            }
        }

        FunctionMap functions;
        for (const Function& function : module.functions)
        {
            if (!function.id)
                report("LIR1100", "Lowered WIR function has an invalid id.", function.source);
            else if (!functions.emplace(function.id.value(), &function).second)
                report("LIR1101", "Lowered WIR function id is duplicated.", function.source, function.id);
            if (function.name.empty())
                report("LIR1102", "Lowered WIR function name cannot be empty.", function.source, function.id);
            if (!module.types.tryGet(function.returnType))
                report("LIR1103", "Lowered WIR function return type is invalid.", function.source, function.id);
            const Type* asyncTaskType = module.types.tryGet(function.returnType);
            const bool validAsyncResult = asyncTaskType && asyncTaskType->kind == TypeKind::AsyncTask &&
                asyncTaskType->arguments.size() == 1;
            if (function.isAsync && (!validAsyncResult || !function.coroutine ||
                function.coroutine->resultType != asyncTaskType->arguments.front()))
                report("LIR1114", "Async Lowered WIR function requires coroutine<T> return and matching coroutine layout.", function.source, function.id);
            if (!function.isAsync && function.coroutine)
                report("LIR1115", "Non-async Lowered WIR function cannot carry coroutine layout.", function.source, function.id);
            const Type* callableType = module.types.tryGet(function.callableType);
            if (!callableType || callableType->kind != TypeKind::Function || callableType->arguments.empty() ||
                callableType->arguments.back() != function.returnType)
                report("LIR1110", "Lowered WIR function requires a callable type ending in its return type.", function.source, function.id);
            else
            {
                const std::size_t hiddenParameterCount = function.captureParameterCount + (function.isMethod ? 1u : 0u);
                bool signatureMatches = function.parameters.size() >= hiddenParameterCount &&
                    callableType->arguments.size() == function.parameters.size() - hiddenParameterCount + 1;
                for (std::size_t parameterIndex = hiddenParameterCount;
                     signatureMatches && parameterIndex < function.parameters.size(); ++parameterIndex)
                {
                    signatureMatches = function.parameters[parameterIndex].type ==
                        callableType->arguments[parameterIndex - hiddenParameterCount];
                }
                if (!signatureMatches)
                    report("LIR1112", "Lowered WIR visible parameters must match the callable type after hidden receiver/capture parameters.", function.source, function.id);
            }
            if (function.captureParameterCount != function.captures.size() ||
                function.captureParameterCount > function.parameters.size() ||
                (!function.isClosureBody && !function.captures.empty()))
                report("LIR1111", "Lowered WIR closure capture metadata is inconsistent.", function.source, function.id);
            if (function.isMethod)
            {
                const Type* owner = module.types.tryGet(function.ownerType);
                const Type* receiver = !function.parameters.empty()
                    ? module.types.tryGet(function.parameters.front().type)
                    : nullptr;
                if (!owner || owner->kind != TypeKind::Named || !receiver ||
                    receiver->kind != TypeKind::Reference || receiver->arguments.size() != 1 ||
                    receiver->arguments.front() != function.ownerType)
                    report("LIR1106", "Lowered WIR methods require a named owner and leading self parameter.", function.source, function.id);
            }
            if (function.nativeBinding)
            {
                const NativeBinding& binding = *function.nativeBinding;
                const std::size_t hiddenParameterCount = function.captureParameterCount + (function.isMethod ? 1u : 0u);
                bool valid = function.isExternal && !function.isAbstract && !binding.symbol.empty() &&
                    !binding.stableKey.empty() && !binding.thunkSymbol.empty() &&
                    function.parameters.size() >= hiddenParameterCount &&
                    binding.parameters.size() == function.parameters.size() - hiddenParameterCount &&
                    binding.result.type == function.returnType && validNativeAbiValue(module.types, binding.result) &&
                    binding.requiresAdapter == (binding.thunkKind != NativeThunkKind::Direct) &&
                    (function.genericParameters.empty() || binding.thunkKind == NativeThunkKind::TemplateSpecialization);
                for (std::size_t index = 0; valid && index < binding.parameters.size(); ++index)
                    valid = binding.parameters[index].passing != NativePassingMode::ReturnOwned &&
                        validNativeAbiValue(module.types, binding.parameters[index]) &&
                        binding.parameters[index].type == function.parameters[index + hiddenParameterCount].type;
                if (!valid)
                    report("LIR1113", "Native Lowered WIR function binding has an invalid identity, ABI signature, or thunk contract.", function.source, function.id);
            }
        }

        if (module.contract.logicalName.empty() || module.contract.stableKey.empty() || module.contract.stableId == 0 ||
            module.contract.stableId != stableModuleHash(module.contract.stableKey))
            report("LIR1500", "Lowered WIR module contract requires a canonical stable identity.");
        if (module.contract.callTable.descriptorVersion != ModuleAbiDescriptorVersion ||
            module.contract.callTable.stableId == 0 ||
            module.contract.callTable.stableId != stableModuleHash(module.contract.stableKey +
                ":sdk-call-table:v" + std::to_string(ModuleAbiDescriptorVersion)) ||
            module.contract.callTable.entries.size() != module.contract.exports.size())
            report("LIR1501", "Lowered WIR SDK call table version, identity, and export cardinality must be canonical.");
        std::unordered_set<std::uint64_t> importIds;
        for (const ModuleImport& import : module.contract.imports)
            if (import.stableId == 0 || import.logicalName.empty() || !importIds.insert(import.stableId).second)
                report("LIR1504", "Lowered WIR module imports require unique stable identities and logical names.");
        std::unordered_set<std::uint64_t> exportIds;
        for (std::size_t index = 0; index < module.contract.exports.size(); ++index)
        {
            const ModuleExport& entry = module.contract.exports[index];
            const bool identityValid = entry.stableId != 0 && !entry.stableKey.empty() &&
                entry.stableId == stableModuleHash(entry.stableKey) && exportIds.insert(entry.stableId).second;
            const bool slotValid = entry.callTableSlot == index &&
                module.contract.callTable.entries[index] == entry.stableId;
            bool targetValid = false;
            if (entry.kind == ModuleExportKind::Function ||
                entry.kind == ModuleExportKind::GenericFunctionSpecialization)
                targetValid = entry.function && functions.contains(entry.function.value()) &&
                    module.types.tryGet(entry.returnType) != nullptr;
            else
            {
                const Type* type = module.types.tryGet(entry.type);
                targetValid = type && type->kind == TypeKind::Named;
            }
            const bool roleValid = entry.role == ModuleExportRole::Ordinary
                ? entry.roleName.empty()
                : !entry.roleName.empty();
            if (!identityValid || !slotValid || !roleValid || !targetValid)
                report("LIR1502", "Lowered WIR export descriptor has an invalid stable identity, slot, or target.");
        }
        const ModuleLifecycle& lifecycle = module.contract.lifecycle;
        const auto lifecycleKnown = [&](const FunctionId id)
            { return !id || functions.contains(id.value()); };
        if (!lifecycleKnown(lifecycle.apiVersion) || !lifecycleKnown(lifecycle.load) ||
            !lifecycleKnown(lifecycle.update) || !lifecycleKnown(lifecycle.unload) ||
            !lifecycleKnown(lifecycle.saveState) || !lifecycleKnown(lifecycle.restoreState) ||
            static_cast<bool>(lifecycle.saveState) != static_cast<bool>(lifecycle.restoreState))
            report("LIR1503", "Lowered WIR module lifecycle must reference known functions and pair save/restore state hooks.");
        std::unordered_set<std::uint64_t> reflectedTypes;
        for (const ReflectionDescriptor& descriptor : module.contract.reflection)
        {
            const Type* type = module.types.tryGet(descriptor.type);
            if (descriptor.stableTypeId == 0 || descriptor.logicalName.empty() || !type ||
                type->kind != TypeKind::Named || descriptor.nominalKind != type->nominalKind ||
                !reflectedTypes.insert(descriptor.stableTypeId).second)
                report("LIR1505", "Lowered WIR reflection descriptors require unique identities and matching nominal types.");
        }

        for (const Type& type : module.types.types())
        {
            for (const MethodLayout& method : type.methods)
            {
                const auto function = functions.find(method.function.value());
                if (function == functions.end() || !function->second->isMethod)
                    report("LIR1009", "Lowered WIR method layout references an unknown or non-method function.");
            }
        }

        for (const Function& function : module.functions)
        {
            ValueTypeMap values;
            std::unordered_map<ValueId::ValueType, Opcode> producerOpcodes;
            auto defineValue = [&](const Parameter& parameter, const BlockId block)
            {
                if (!parameter.id)
                {
                    report("LIR1200", "Lowered WIR value has an invalid id.", parameter.source, function.id, block);
                    return;
                }
                if (!module.types.tryGet(parameter.type))
                    report("LIR1201", "Lowered WIR value has an invalid type.", parameter.source, function.id, block);
                if (!values.emplace(parameter.id.value(), parameter.type).second)
                    report("LIR1202", "Lowered WIR value id is defined more than once.", parameter.source, function.id, block);
            };
            for (const Parameter& parameter : function.parameters)
                defineValue(parameter, {});

            if (function.isExternal)
            {
                if (!function.blocks.empty())
                    report("LIR1104", "External Lowered WIR functions cannot contain blocks.", function.source, function.id);
                continue;
            }
            if (function.blocks.empty())
            {
                report("LIR1105", "Defined Lowered WIR functions require at least one block.", function.source, function.id);
                continue;
            }

            BlockMap blocks;
            for (const BasicBlock& block : function.blocks)
            {
                if (!block.id)
                    report("LIR1300", "Lowered WIR block has an invalid id.", block.source, function.id);
                else if (!blocks.emplace(block.id.value(), &block).second)
                    report("LIR1301", "Lowered WIR block id is duplicated.", block.source, function.id, block.id);
                for (const Parameter& parameter : block.parameters)
                    defineValue(parameter, block.id);
                for (const Instruction& instruction : block.instructions)
                {
                    if (instruction.result)
                    {
                        defineValue(Parameter{
                            .id = instruction.result,
                            .type = instruction.resultType,
                            .ownership = instruction.resultOwnership,
                            .borrowLifetime = instruction.borrowLifetime,
                            .source = instruction.source
                        }, block.id);
                        producerOpcodes.emplace(instruction.result.value(), instruction.opcode);
                    }
                }
            }

            if (function.coroutine)
            {
                std::unordered_set<std::uint32_t> frameSlots;
                std::unordered_set<ValueId::ValueType> frameValues;
                for (const CoroutineFrameSlot& slot : function.coroutine->frameSlots)
                {
                    const Type* type = module.types.tryGet(slot.type);
                    if (!frameSlots.insert(slot.slot).second || slot.slot >= function.coroutine->frameSlots.size() ||
                        !slot.value || !values.contains(slot.value.value()) ||
                        !frameValues.insert(slot.value.value()).second || !type ||
                        type->ownership != slot.ownership || type->cleanup != slot.cleanup)
                    {
                        report("LIR1451", "Coroutine frame slots require unique dense ids, values, and matching ownership metadata.", function.source, function.id);
                    }
                }
                for (std::size_t stateIndex = 0; stateIndex < function.coroutine->states.size(); ++stateIndex)
                {
                    const CoroutineState& state = function.coroutine->states[stateIndex];
                    if (state.index != stateIndex || !blocks.contains(state.suspendBlock.value()) ||
                        !blocks.contains(state.resumeBlock.value()) ||
                        (state.awaitedTask && !values.contains(state.awaitedTask.value())) ||
                        (state.resumedValue && !values.contains(state.resumedValue.value())) ||
                        (state.resultType && !module.types.tryGet(state.resultType)))
                    {
                        report("LIR1452", "Coroutine states require dense ids and valid suspend/resume/value references.", function.source, function.id);
                    }
                }
            }

            auto valueType = [&](const ValueId value) -> TypeId
            {
                if (!value)
                    return {};
                const auto found = values.find(value.value());
                return found == values.end() ? TypeId{} : found->second;
            };

            std::unordered_set<ValueId::ValueType> initializedPlaces;
            for (const BasicBlock& block : function.blocks)
            {
                if (block.instructions.empty())
                {
                    report("LIR1302", "Lowered WIR block requires a terminator.", block.source, function.id, block.id);
                    continue;
                }

                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    const bool terminator = isTerminator(instruction.opcode);
                    if (terminator != (index + 1 == block.instructions.size()))
                    {
                        report(
                            terminator ? "LIR1303" : "LIR1304",
                            terminator ? "Lowered WIR terminator must be the final instruction in its block."
                                       : "Lowered WIR block must end with a terminator.",
                            instruction.source,
                            function.id,
                            block.id);
                    }

                    for (const ValueId operand : instruction.operands)
                    {
                        if (!valueType(operand))
                            report("LIR1400", "Lowered WIR instruction references an unknown value.", instruction.source, function.id, block.id);
                    }

                    bool callReturnsVoid = false;
                    const bool isCallInstruction = instruction.opcode == Opcode::Call ||
                        instruction.opcode == Opcode::NativeInvoke ||
                        instruction.opcode == Opcode::IndirectCall ||
                        instruction.opcode == Opcode::ExtensionCall ||
                        instruction.opcode == Opcode::MethodCall ||
                        instruction.opcode == Opcode::VirtualCall ||
                        instruction.opcode == Opcode::InterfaceCall ||
                        instruction.opcode == Opcode::IntrinsicCall;
                    if (isCallInstruction && instruction.callee)
                    {
                        const auto calleeIt = functions.find(instruction.callee.value());
                        if (calleeIt != functions.end())
                        {
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                        }
                    }
                    if (instruction.opcode == Opcode::IndirectCall && !instruction.operands.empty())
                    {
                        const Type* callable = module.types.tryGet(valueType(instruction.operands.front()));
                        const Type* returnType = callable && callable->kind == TypeKind::Function && !callable->arguments.empty()
                            ? module.types.tryGet(callable->arguments.back())
                            : nullptr;
                        callReturnsVoid = returnType && returnType->kind == TypeKind::Void;
                    }
                    if (instruction.opcode == Opcode::IntrinsicCall && !instruction.result)
                        callReturnsVoid = true;
                    const bool shouldHaveResult = producesValue(instruction.opcode) && !callReturnsVoid;
                    if (shouldHaveResult && (!instruction.result || !module.types.tryGet(instruction.resultType)))
                        report("LIR1401", "Value-producing Lowered WIR instruction requires a typed result.", instruction.source, function.id, block.id);
                    if (!shouldHaveResult && !isCallInstruction && instruction.result)
                        report("LIR1402", "Lowered WIR terminator cannot produce a value.", instruction.source, function.id, block.id);
                    const bool asyncMetadataCarrier = isCallInstruction ||
                        instruction.opcode == Opcode::CancellationCheck ||
                        instruction.opcode == Opcode::CoroutineSuspend ||
                        instruction.opcode == Opcode::CoroutineResume;
                    if (!asyncMetadataCarrier && instruction.opcode != Opcode::CoroutineComplete &&
                        (instruction.asyncOperation != AsyncOperation::None ||
                         instruction.asyncExecutor != AsyncExecutorKind::Inherit))
                    {
                        report("LIR1453", "Lowered WIR async metadata is attached to an unsupported instruction.", instruction.source, function.id, block.id);
                    }

                    if (instruction.opcode == Opcode::Unary)
                    {
                        if (instruction.operands.size() != 1 || valueType(instruction.operands.front()) != instruction.resultType)
                            report("LIR1403", "Lowered WIR unary operand must match its result type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Binary)
                    {
                        if (instruction.operands.size() != 2 || valueType(instruction.operands[0]) != valueType(instruction.operands[1]))
                            report("LIR1404", "Lowered WIR binary operands must have the same type.", instruction.source, function.id, block.id);
                        else if (isComparison(instruction.binaryOperator) && instruction.resultType != module.types.boolType())
                            report("LIR1405", "Lowered WIR comparison result must be bool.", instruction.source, function.id, block.id);
                        else if (!isComparison(instruction.binaryOperator) && instruction.resultType != valueType(instruction.operands[0]))
                            report("LIR1406", "Lowered WIR binary result must match its operand type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Convert)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* destinationType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || !destinationType ||
                            !isNumeric(sourceType->kind) || !isNumeric(destinationType->kind))
                        {
                            report("LIR1417", "Lowered WIR numeric conversion requires one numeric operand and a numeric result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantTest)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named ||
                            instruction.resultType != module.types.boolType() || instruction.selector.empty())
                        {
                            report("LIR1418", "Lowered WIR variant test requires one named value, a variant selector, and a bool result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::VariantPayload)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Named || instruction.selector.empty())
                        {
                            report("LIR1419", "Lowered WIR variant payload requires one named value and a variant selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayLength)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Array ||
                            !resultType || resultType->kind != TypeKind::USize)
                        {
                            report("LIR1420", "Lowered WIR array length requires one array value and a usize result.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayElement)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!sourceType || sourceType->kind != TypeKind::Array || sourceType->arguments.size() != 1 ||
                            sourceType->arguments.front() != instruction.resultType)
                        {
                            report("LIR1421", "Lowered WIR array element projection must return its array element type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayCreate)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        bool valid = resultType && resultType->kind == TypeKind::Array &&
                            resultType->arguments.size() == 1;
                        if (valid)
                        {
                            valid = std::ranges::all_of(
                                instruction.operands,
                                [&](const ValueId operand)
                                {
                                    return valueType(operand) == resultType->arguments.front();
                                });
                            valid = valid && (!resultType->staticExtent.has_value() ||
                                *resultType->staticExtent == instruction.operands.size());
                        }
                        if (!valid)
                            report("LIR1422", "Lowered WIR array creation requires element operands matching its array type and extent.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ArrayGet)
                    {
                        const Type* arrayType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        if (!arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) ||
                            arrayType->arguments.front() != instruction.resultType)
                        {
                            report("LIR1423", "Lowered WIR array get requires an array, an integer index, and its element result type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::DictionaryCreate)
                    {
                        const Type* dictionaryType = module.types.tryGet(instruction.resultType);
                        bool valid = dictionaryType && dictionaryType->kind == TypeKind::Dictionary &&
                            dictionaryType->arguments.size() == 2 && instruction.operands.size() % 2 == 0 &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            (instruction.selector == "ordered" || instruction.selector == "unordered");
                        for (std::size_t entryIndex = 0; valid && entryIndex < instruction.operands.size(); ++entryIndex)
                        {
                            const TypeId expected = dictionaryType->arguments[entryIndex % 2];
                            valid = valueType(instruction.operands[entryIndex]) == expected &&
                                instruction.signatureTypes[entryIndex] == expected;
                        }
                        if (!valid)
                            report("LIR1437", "Lowered WIR dictionary creation requires alternating key/value operands matching its concrete type.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::DictionaryGet)
                    {
                        const Type* dictionaryType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        if (!dictionaryType || dictionaryType->kind != TypeKind::Dictionary ||
                            dictionaryType->arguments.size() != 2 ||
                            valueType(instruction.operands[1]) != dictionaryType->arguments[0] ||
                            instruction.resultType != dictionaryType->arguments[1])
                        {
                            report("LIR1438", "Lowered WIR dictionary get requires matching dictionary, key, and mapped result types.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Interpolate)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const bool familyMatches = resultType &&
                            ((resultType->kind == TypeKind::String && instruction.intrinsicFamily == IntrinsicFamily::String) ||
                             (resultType->kind == TypeKind::Text && instruction.intrinsicFamily == IntrinsicFamily::Text));
                        bool signaturesMatch = instruction.signatureTypes.size() == instruction.operands.size();
                        for (std::size_t operandIndex = 0;
                             signaturesMatch && operandIndex < instruction.operands.size(); ++operandIndex)
                        {
                            signaturesMatch = instruction.signatureTypes[operandIndex] ==
                                valueType(instruction.operands[operandIndex]);
                        }
                        if (!familyMatches || instruction.stringSegments.size() != instruction.operands.size() + 1 ||
                            !signaturesMatch)
                        {
                            report("LIR1439", "Lowered WIR interpolation requires string/text identity, segments, and concrete signatures.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::EnumConstant)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!instruction.operands.empty() || instruction.selector.empty() ||
                            instruction.targetType != instruction.resultType || !resultType ||
                            resultType->kind != TypeKind::Named ||
                            (resultType->nominalKind != NominalKind::Enum && resultType->nominalKind != NominalKind::Flagset))
                        {
                            report("LIR1440", "Lowered WIR enum constant requires a named enum/flagset result and stable selector.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::IntrinsicCall)
                    {
                        bool signaturesMatch = instruction.signatureTypes.size() == instruction.operands.size();
                        for (std::size_t operandIndex = 0;
                             signaturesMatch && operandIndex < instruction.operands.size(); ++operandIndex)
                        {
                            signaturesMatch = instruction.signatureTypes[operandIndex] ==
                                valueType(instruction.operands[operandIndex]);
                        }
                        if (instruction.intrinsicFamily == IntrinsicFamily::None || instruction.selector.empty() ||
                            instruction.operands.empty() || instruction.signatureTypes.size() != instruction.operands.size() ||
                            !module.types.tryGet(instruction.targetType) || !signaturesMatch)
                        {
                            report("LIR1441", "Lowered WIR intrinsic call requires a family, selector, receiver, target, and concrete signature.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::AnyBox)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (instruction.operands.size() != 1 || !resultType || resultType->kind != TypeKind::Any ||
                            instruction.targetType != valueType(instruction.operands.front()) ||
                            instruction.signatureTypes != std::vector<TypeId>{instruction.targetType})
                        {
                            report("LIR1442", "Lowered WIR any box requires one operand and its concrete source type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::AnyCheckedCast || instruction.opcode == Opcode::AnyTypeTest)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const bool validResult = instruction.opcode == Opcode::AnyTypeTest
                            ? instruction.resultType == module.types.boolType()
                            : instruction.resultType == instruction.targetType;
                        if (!sourceType || sourceType->kind != TypeKind::Any ||
                            !module.types.tryGet(instruction.targetType) || !validResult)
                        {
                            report("LIR1443", "Lowered WIR any test/cast requires one any operand and a concrete target.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::NullableWrap)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (instruction.operands.size() != 1 || !resultType || resultType->kind != TypeKind::Nullable ||
                            resultType->arguments.size() != 1 || resultType->arguments.front() != valueType(instruction.operands.front()) ||
                            instruction.targetType != instruction.resultType)
                        {
                            report("LIR1444", "Lowered WIR nullable wrap requires one matching payload value.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::LocalPlace)
                    {
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        if (!instruction.operands.empty() || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || instruction.selector.empty())
                        {
                            report("LIR1424", "Lowered WIR local place requires a named reference result and no operands.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::PlaceInit || instruction.opcode == Opcode::Store ||
                             instruction.opcode == Opcode::Replace)
                    {
                        const Type* placeType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const bool mutabilityValid = instruction.opcode == Opcode::PlaceInit ||
                            (placeType && placeType->isMutable);
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            valueType(instruction.operands[1]) != placeType->arguments.front() || !mutabilityValid)
                        {
                            report(
                                instruction.opcode == Opcode::PlaceInit ? "LIR1425" : "LIR1426",
                                instruction.opcode == Opcode::PlaceInit
                                    ? "Lowered WIR place initialization requires a reference place and a matching value."
                                    : "Lowered WIR store requires a mutable reference place and a matching value.",
                                instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::PlaceInit && instruction.operands.size() == 2)
                        {
                            const ValueId place = instruction.operands.front();
                            const auto producer = producerOpcodes.find(place.value());
                            if (producer == producerOpcodes.end() || producer->second != Opcode::LocalPlace ||
                                !initializedPlaces.insert(place.value()).second)
                            {
                                report("LIR1431", "Lowered WIR place initialization must target an uninitialized local place exactly once.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::Load)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.resultType != placeType->arguments.front())
                        {
                            report("LIR1427", "Lowered WIR load requires a reference place and its referred result type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::FieldPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* baseValueType = baseType;
                        if (baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1)
                            baseValueType = module.types.tryGet(baseType->arguments.front());
                        const bool strengthensMutability = baseType && baseType->kind == TypeKind::Reference &&
                            placeType && placeType->isMutable && !baseType->isMutable;
                        if (!baseValueType || baseValueType->kind != TypeKind::Named || !placeType ||
                            placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            instruction.selector.empty() || strengthensMutability)
                        {
                            report("LIR1428", "Lowered WIR field place requires a named base, field selector, and non-strengthening reference result.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            std::unordered_set<TypeId::ValueType> visited;
                            const FieldLayout* field = findFieldLayout(module.types, *baseValueType, instruction.selector, visited);
                            if (!field || field->type != placeType->arguments.front() ||
                                (placeType->isMutable && !field->isMutable))
                            {
                                report("LIR1432", "Lowered WIR field place must match the nominal field layout and field mutability.", instruction.source, function.id, block.id);
                            }
                        }
                    }
                    else if (instruction.opcode == Opcode::ArrayPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* indexType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[1]))
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        const Type* arrayType = baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1
                            ? module.types.tryGet(baseType->arguments.front())
                            : nullptr;
                        if (!baseType || !arrayType || arrayType->kind != TypeKind::Array || arrayType->arguments.size() != 1 ||
                            !indexType || !isInteger(indexType->kind) || !placeType || placeType->kind != TypeKind::Reference ||
                            placeType->arguments.size() != 1 || placeType->arguments.front() != arrayType->arguments.front() ||
                            (placeType->isMutable && !baseType->isMutable))
                        {
                            report("LIR1429", "Lowered WIR array place requires an array reference, integer index, and non-strengthening element reference.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::DictionaryPlace)
                    {
                        const Type* baseType = instruction.operands.size() == 2
                            ? module.types.tryGet(valueType(instruction.operands[0]))
                            : nullptr;
                        const Type* dictionaryType = baseType && baseType->kind == TypeKind::Reference && baseType->arguments.size() == 1
                            ? module.types.tryGet(baseType->arguments.front())
                            : nullptr;
                        const Type* placeType = module.types.tryGet(instruction.resultType);
                        if (!baseType || !dictionaryType || dictionaryType->kind != TypeKind::Dictionary ||
                            dictionaryType->arguments.size() != 2 || valueType(instruction.operands[1]) != dictionaryType->arguments[0] ||
                            !placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            placeType->arguments.front() != dictionaryType->arguments[1] ||
                            (placeType->isMutable && !baseType->isMutable))
                        {
                            report("LIR1445", "Lowered WIR dictionary place requires a dictionary reference, matching key, and mapped reference.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Borrow)
                    {
                        const Type* sourceType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (!sourceType || sourceType->kind != TypeKind::Reference || sourceType->arguments.size() != 1 ||
                            !resultType || resultType->kind != TypeKind::Reference || resultType->arguments.size() != 1 ||
                            sourceType->arguments.front() != resultType->arguments.front() ||
                            (resultType->isMutable && !sourceType->isMutable))
                        {
                            report("LIR1430", "Lowered WIR borrow cannot change the referred type or strengthen mutability.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ConstructComponent || instruction.opcode == Opcode::ConstructObject)
                    {
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        const NominalKind expectedKind = instruction.opcode == Opcode::ConstructObject
                            ? NominalKind::Object
                            : NominalKind::Component;
                        if (!resultType || resultType->kind != TypeKind::Named || resultType->nominalKind != expectedKind ||
                            !resultType->hasConstructor || instruction.selector.empty() ||
                            instruction.signatureTypes.size() != instruction.operands.size() ||
                            !std::ranges::all_of(instruction.signatureTypes, [&](const TypeId type) { return module.types.tryGet(type) != nullptr; }) ||
                            !std::ranges::equal(
                                instruction.signatureTypes,
                                instruction.operands,
                                {},
                                [](const TypeId type) { return type; },
                                [&](const ValueId value) { return valueType(value); }))
                        {
                            report("LIR1433", "Lowered WIR construction requires a matching constructible component/object result and typed constructor signature.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Retain || instruction.opcode == Opcode::CopyValue)
                    {
                        const Type* copiedType = module.types.tryGet(instruction.resultType);
                        const bool expectsReferenceCounted = instruction.opcode == Opcode::Retain;
                        if (instruction.operands.size() != 1 ||
                            valueType(instruction.operands.front()) != instruction.resultType || !copiedType ||
                            !requiresCleanup(*copiedType) ||
                            (expectsReferenceCounted != (copiedType->cleanup == CleanupKind::ReleaseReference)) ||
                            instruction.resultOwnership != typed::ValueOwnership::Owned)
                        {
                            report("LIR1446", "Lowered WIR retain/copy-value must match the type cleanup protocol.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::MoveValue)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* movedType = module.types.tryGet(instruction.resultType);
                        if (!placeType || placeType->kind != TypeKind::Reference || placeType->arguments.size() != 1 ||
                            placeType->arguments.front() != instruction.resultType || !movedType ||
                            !requiresCleanup(*movedType) || instruction.resultOwnership != typed::ValueOwnership::Owned)
                        {
                            report("LIR1447", "Lowered WIR move-value must transfer one cleanup-bearing place.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Release || instruction.opcode == Opcode::DropValue)
                    {
                        const Type* releasedType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const bool expectsReferenceCounted = instruction.opcode == Opcode::Release;
                        if (!releasedType || !requiresCleanup(*releasedType) ||
                            (expectsReferenceCounted != (releasedType->cleanup == CleanupKind::ReleaseReference)))
                        {
                            report("LIR1448", "Lowered WIR release/drop-value must match the value cleanup protocol.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::ReleasePlace || instruction.opcode == Opcode::DropPlace)
                    {
                        const Type* placeType = instruction.operands.size() == 1
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        const Type* valueTypeInfo = placeType && placeType->kind == TypeKind::Reference && placeType->arguments.size() == 1
                            ? module.types.tryGet(placeType->arguments.front())
                            : nullptr;
                        const bool expectsReferenceCounted = instruction.opcode == Opcode::ReleasePlace;
                        if (!valueTypeInfo || !requiresCleanup(*valueTypeInfo) ||
                            (expectsReferenceCounted != (valueTypeInfo->cleanup == CleanupKind::ReleaseReference)))
                        {
                            report("LIR1434", "Lowered WIR release/drop-place must match the stored value cleanup protocol.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::FunctionReference)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        if (calleeIt == functions.end() || !resultType || resultType->kind != TypeKind::Function ||
                            !instruction.operands.empty() ||
                            (calleeIt->second->genericParameters.empty() && instruction.resultType != calleeIt->second->callableType))
                            report("LIR1439", "Lowered WIR function reference must pin a known callable declaration.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ClosureCreate)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* resultType = module.types.tryGet(instruction.resultType);
                        bool valid = calleeIt != functions.end() && calleeIt->second->isClosureBody && resultType &&
                            resultType->kind == TypeKind::Function && instruction.resultType == calleeIt->second->callableType &&
                            instruction.operands.size() == instruction.signatureTypes.size() &&
                            instruction.operands.size() == instruction.captureKinds.size() &&
                            instruction.operands.size() == calleeIt->second->captures.size();
                        if (valid)
                        {
                            for (std::size_t captureIndex = 0; captureIndex < instruction.operands.size(); ++captureIndex)
                                valid = valid && valueType(instruction.operands[captureIndex]) == instruction.signatureTypes[captureIndex] &&
                                    instruction.signatureTypes[captureIndex] == calleeIt->second->captures[captureIndex].type &&
                                    instruction.captureKinds[captureIndex] == calleeIt->second->captures[captureIndex].kind;
                        }
                        if (!valid)
                            report("LIR1440", "Lowered WIR closure creation must match its capture layout.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::IndirectCall)
                    {
                        const Type* callable = !instruction.operands.empty()
                            ? module.types.tryGet(valueType(instruction.operands.front()))
                            : nullptr;
                        bool valid = callable && callable->kind == TypeKind::Function && !callable->arguments.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            instruction.signatureTypes.front() == valueType(instruction.operands.front()) &&
                            callable->arguments.size() == instruction.operands.size();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                valid = valid && valueType(instruction.operands[argumentIndex]) == instruction.signatureTypes[argumentIndex] &&
                                    instruction.signatureTypes[argumentIndex] == callable->arguments[argumentIndex - 1];
                            const TypeId returnType = callable->arguments.back();
                            const Type* returnTypeInfo = module.types.tryGet(returnType);
                            valid = valid && returnTypeInfo &&
                                (returnTypeInfo->kind == TypeKind::Void ? !instruction.result : instruction.resultType == returnType);
                        }
                        if (!valid)
                            report("LIR1441", "Lowered WIR indirect call must match its function value signature.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::ExtensionCall)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const Type* target = module.types.tryGet(instruction.targetType);
                        bool valid = calleeIt != functions.end() && calleeIt->second->isExtension && target &&
                            target->kind == TypeKind::Named && !instruction.operands.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size() &&
                            !instruction.selector.empty() && !instruction.specializationKey.empty();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                valid = valid && valueType(instruction.operands[argumentIndex]) == instruction.signatureTypes[argumentIndex];
                            const Type* receiver = module.types.tryGet(instruction.signatureTypes.front());
                            valid = valid && (valueType(instruction.operands.front()) == instruction.signatureTypes.front() ||
                                (receiver && receiver->kind == TypeKind::Reference && receiver->arguments.size() == 1 &&
                                 receiver->arguments.front() == valueType(instruction.operands.front())));
                            const Type* returnType = module.types.tryGet(calleeIt->second->returnType);
                            valid = valid && returnType &&
                                (returnType->kind == TypeKind::Void
                                    ? !instruction.result
                                    : calleeIt->second->genericParameters.empty()
                                        ? instruction.resultType == calleeIt->second->returnType
                                        : module.types.tryGet(instruction.resultType) && !instruction.specializationKey.empty());
                        }
                        if (!valid)
                            report("LIR1442", "Lowered WIR extension call must pin its implementation and concrete signature.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::MethodCall ||
                             instruction.opcode == Opcode::VirtualCall ||
                             instruction.opcode == Opcode::InterfaceCall)
                    {
                        const Type* owner = module.types.tryGet(instruction.targetType);
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        const auto method = owner ? std::ranges::find_if(owner->methods, [&](const MethodLayout& layout)
                        {
                            return layout.slot == instruction.projectionIndex &&
                                layout.name == instruction.selector && layout.function == instruction.callee;
                        }) : std::vector<MethodLayout>::const_iterator{};
                        const bool dispatchKindValid = owner && owner->kind == TypeKind::Named &&
                            (instruction.opcode != Opcode::VirtualCall || owner->nominalKind == NominalKind::Object) &&
                            (instruction.opcode != Opcode::InterfaceCall || owner->nominalKind == NominalKind::Interface);
                        bool valid = dispatchKindValid && calleeIt != functions.end() &&
                            method != owner->methods.end() && !instruction.operands.empty() &&
                            instruction.signatureTypes.size() == instruction.operands.size();
                        if (valid)
                        {
                            for (std::size_t argumentIndex = 1; argumentIndex < instruction.operands.size(); ++argumentIndex)
                            {
                                valid = valid && valueType(instruction.operands[argumentIndex]) ==
                                    instruction.signatureTypes[argumentIndex];
                            }
                            TypeId receiverNominal;
                            std::unordered_set<TypeId::ValueType> visited;
                            valid = valid && underlyingObjectType(module.types, valueType(instruction.operands.front()), &receiverNominal) &&
                                nominalDerivesFrom(module.types, receiverNominal, instruction.targetType, visited);
                            const Function& callee = *calleeIt->second;
                            const Type* returnType = module.types.tryGet(callee.returnType);
                            const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                            valid = valid && returnType &&
                                (returnsVoid
                                    ? !instruction.result
                                    : callee.genericParameters.empty()
                                        ? instruction.resultType == callee.returnType
                                        : module.types.tryGet(instruction.resultType) && !instruction.specializationKey.empty());
                        }
                        if (!valid)
                            report("LIR1435", "Lowered WIR method dispatch must match its owner slot, receiver, signature, and callee.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Upcast || instruction.opcode == Opcode::CheckedCast)
                    {
                        TypeId sourceNominal;
                        TypeId targetNominal;
                        const bool hasObjectTypes = instruction.operands.size() == 1 &&
                            underlyingObjectType(module.types, valueType(instruction.operands.front()), &sourceNominal) &&
                            underlyingObjectType(module.types, instruction.targetType, &targetNominal);
                        bool valid = hasObjectTypes && instruction.resultType == instruction.targetType;
                        if (valid && instruction.opcode == Opcode::Upcast)
                        {
                            std::unordered_set<TypeId::ValueType> visited;
                            valid = nominalDerivesFrom(module.types, sourceNominal, targetNominal, visited);
                        }
                        if (!valid)
                            report("LIR1436", "Lowered WIR object cast requires compatible object/interface source and target types.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::TypeTest)
                    {
                        if (instruction.operands.size() != 1 ||
                            !underlyingObjectType(module.types, valueType(instruction.operands.front())) ||
                            !underlyingObjectType(module.types, instruction.targetType) ||
                            instruction.resultType != module.types.boolType())
                            report("LIR1437", "Lowered WIR type test requires an object/interface value, target type, and bool result.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::IdentityEqual)
                    {
                        if (instruction.operands.size() != 2 ||
                            !underlyingObjectType(module.types, valueType(instruction.operands[0])) ||
                            !underlyingObjectType(module.types, valueType(instruction.operands[1])) ||
                            instruction.resultType != module.types.boolType() ||
                            (instruction.binaryOperator != typed::BinaryOperator::Equal &&
                             instruction.binaryOperator != typed::BinaryOperator::NotEqual))
                            report("LIR1438", "Lowered WIR identity equality requires object/interface operands and bool result.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::Call || instruction.opcode == Opcode::NativeInvoke)
                    {
                        const auto calleeIt = instruction.callee ? functions.find(instruction.callee.value()) : functions.end();
                        if (calleeIt == functions.end())
                        {
                            report("LIR1407", "Lowered WIR call references an unknown function.", instruction.source, function.id, block.id);
                        }
                        else
                        {
                            const Function& callee = *calleeIt->second;
                            if ((instruction.opcode == Opcode::NativeInvoke) != callee.nativeBinding.has_value())
                                report("LIR1450", "Lowered native-invoke and ordinary call must match the callee ABI binding.", instruction.source, function.id, block.id);
                            const bool concreteSignature = instruction.signatureTypes.size() == instruction.operands.size() &&
                                std::ranges::all_of(instruction.signatureTypes, [&](const TypeId type)
                                    { return module.types.tryGet(type) != nullptr; });
                            if (callee.parameters.size() != instruction.operands.size() || !concreteSignature)
                                report("LIR1408", "Lowered WIR call argument count does not match its callee.", instruction.source, function.id, block.id);
                            else
                            {
                                for (std::size_t argumentIndex = 0; argumentIndex < instruction.operands.size(); ++argumentIndex)
                                {
                                    const TypeId operandType = valueType(instruction.operands[argumentIndex]);
                                    const Type* signatureType = module.types.tryGet(instruction.signatureTypes[argumentIndex]);
                                    const bool extensionReceiverMatch = callee.isExtension && argumentIndex == 0 && signatureType &&
                                        signatureType->kind == TypeKind::Reference && signatureType->arguments.size() == 1 &&
                                        signatureType->arguments.front() == operandType;
                                    if ((!extensionReceiverMatch && operandType != instruction.signatureTypes[argumentIndex]) ||
                                        (callee.genericParameters.empty() && instruction.signatureTypes[argumentIndex] != callee.parameters[argumentIndex].type))
                                        report("LIR1409", "Lowered WIR call argument type does not match its parameter.", instruction.source, function.id, block.id);
                                }
                            }
                            const Type* returnType = module.types.tryGet(callee.returnType);
                            const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                            const bool resultMatches = callee.genericParameters.empty()
                                ? instruction.resultType == callee.returnType
                                : module.types.tryGet(instruction.resultType) != nullptr && !instruction.specializationKey.empty();
                            if (!returnType || (returnsVoid ? instruction.result.isValid() : !resultMatches))
                                report("LIR1410", "Lowered WIR call result does not match its callee.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::CancellationCheck)
                    {
                        const bool followedBySuspend = index + 1 < block.instructions.size() &&
                            block.instructions[index + 1].opcode == Opcode::CoroutineSuspend &&
                            block.instructions[index + 1].projectionIndex == instruction.projectionIndex;
                        if (!function.isAsync || !instruction.operands.empty() || instruction.result || !followedBySuspend)
                            report("LIR1454", "Cancellation check must immediately precede its coroutine suspension point.", instruction.source, function.id, block.id);
                    }
                    else if (instruction.opcode == Opcode::CoroutineSuspend)
                    {
                        const CoroutineState* state = function.coroutine && instruction.projectionIndex < function.coroutine->states.size()
                            ? &function.coroutine->states[instruction.projectionIndex]
                            : nullptr;
                        const bool operationShape = instruction.asyncOperation == AsyncOperation::AwaitTask
                            ? instruction.operands.size() == 1 && [&]
                                {
                                    const Type* task = module.types.tryGet(valueType(instruction.operands.front()));
                                    return task && task->kind == TypeKind::AsyncTask && task->arguments.size() == 1;
                                }()
                            : instruction.asyncOperation == AsyncOperation::SwitchExecutor &&
                                instruction.operands.empty() && instruction.asyncExecutor != AsyncExecutorKind::Inherit;
                        const bool precededByCancellationCheck = index > 0 &&
                            block.instructions[index - 1].opcode == Opcode::CancellationCheck &&
                            block.instructions[index - 1].projectionIndex == instruction.projectionIndex;
                        if (!function.isAsync || !state || block.id != state->suspendBlock ||
                            instruction.targets.size() != 1 || instruction.targets.front().block != state->resumeBlock ||
                            !operationShape || !precededByCancellationCheck)
                        {
                            report("LIR1455", "Coroutine suspension must match one canonical state and resume target.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::CoroutineResume)
                    {
                        const CoroutineState* state = function.coroutine && instruction.projectionIndex < function.coroutine->states.size()
                            ? &function.coroutine->states[instruction.projectionIndex]
                            : nullptr;
                        if (!function.isAsync || !state || block.id != state->resumeBlock ||
                            !instruction.operands.empty() || instruction.result != state->resumedValue ||
                            instruction.resultType != state->resultType)
                        {
                            report("LIR1456", "Coroutine resume must materialize the payload declared by its canonical state.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::CoroutineComplete)
                    {
                        const TypeId resultType = coroutineResultType(module.types, function);
                        const Type* type = module.types.tryGet(resultType);
                        const bool returnsVoid = type && type->kind == TypeKind::Void;
                        if (!function.isAsync || !type ||
                            (returnsVoid ? !instruction.operands.empty()
                                         : instruction.operands.size() != 1 || valueType(instruction.operands.front()) != resultType))
                        {
                            report("LIR1457", "Coroutine completion value must match the async payload type.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Return)
                    {
                        if (function.isAsync)
                            report("LIR1458", "Async Lowered WIR must use coroutine-complete instead of return.", instruction.source, function.id, block.id);
                        const Type* returnType = module.types.tryGet(function.returnType);
                        const bool returnsVoid = returnType && returnType->kind == TypeKind::Void;
                        if (!returnType ||
                            (returnsVoid ? !instruction.operands.empty()
                                         : instruction.operands.size() != 1 || valueType(instruction.operands.front()) != function.returnType))
                        {
                            report("LIR1411", "Lowered WIR return value does not match its function.", instruction.source, function.id, block.id);
                        }
                    }
                    else if (instruction.opcode == Opcode::Jump || instruction.opcode == Opcode::CondJump)
                    {
                        const std::size_t expectedTargets = instruction.opcode == Opcode::Jump ? 1 : 2;
                        const std::size_t expectedOperands = instruction.opcode == Opcode::Jump ? 0 : 1;
                        if (instruction.targets.size() != expectedTargets || instruction.operands.size() != expectedOperands)
                        {
                            report("LIR1412", "Lowered WIR jump shape is invalid.", instruction.source, function.id, block.id);
                        }
                        if (instruction.opcode == Opcode::CondJump && !instruction.operands.empty() &&
                            valueType(instruction.operands.front()) != module.types.boolType())
                        {
                            report("LIR1413", "Lowered WIR conditional jump condition must be bool.", instruction.source, function.id, block.id);
                        }
                    }

                    for (const BranchTarget& target : instruction.targets)
                    {
                        const auto targetIt = target.block ? blocks.find(target.block.value()) : blocks.end();
                        if (targetIt == blocks.end())
                        {
                            report("LIR1414", "Lowered WIR jump references an unknown block.", instruction.source, function.id, block.id);
                            continue;
                        }
                        const BasicBlock& targetBlock = *targetIt->second;
                        if (target.arguments.size() != targetBlock.parameters.size())
                        {
                            report("LIR1415", "Lowered WIR jump argument count does not match target block parameters.", instruction.source, function.id, block.id);
                            continue;
                        }
                        for (std::size_t argumentIndex = 0; argumentIndex < target.arguments.size(); ++argumentIndex)
                        {
                            if (valueType(target.arguments[argumentIndex]) != targetBlock.parameters[argumentIndex].type)
                                report("LIR1416", "Lowered WIR jump argument type does not match target block parameter.", instruction.source, function.id, block.id);
                        }
                    }
                }
            }
        }
        return result;
    }
}
