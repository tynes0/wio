#include "wio/bytecode/verifier.h"

#include <unordered_set>

namespace wio::bytecode
{
    VerificationResult Verifier::verify(const Module& module) const
    {
        VerificationResult result;
        auto report = [&](std::string code, std::string message, const std::uint32_t function = InvalidIndex,
                          const std::uint32_t block = InvalidIndex, const std::uint32_t instruction = InvalidIndex)
        {
            result.diagnostics_.push_back(
                {std::move(code), std::move(message), function, block, instruction});
        };
        auto validString = [&](const StringId id) { return id == InvalidIndex || id < module.strings.size(); };
        auto validType = [&](const std::uint32_t id) { return id == InvalidIndex || id < module.types.size(); };
        auto validFunction = [&](const std::uint32_t id)
        { return id == InvalidIndex || (id < module.functions.size() && module.functions[id].id == id); };

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
                if (!validType(dispatch.contractType) || !validFunction(dispatch.implementation))
                    report("WBC1022", "Dispatch entry references invalid type or function metadata");
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
                if (!validString(capture.name) || !validType(capture.type))
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
            for (const Block& block : function.blocks)
            {
                if (block.id == InvalidIndex || !blockIds.insert(block.id).second)
                    report("WBC1011", "Block ids must be valid and unique within a function", function.id, block.id);
            }
            if (function.hasCoroutine)
            {
                if (!validType(function.coroutine.resultType))
                    report("WBC1031", "Coroutine layout references an invalid result type", function.id);
                for (const Function::CoroutineFrameSlot& slot : function.coroutine.frameSlots)
                {
                    if (!validType(slot.type))
                        report("WBC1032", "Coroutine frame slot references an invalid type", function.id);
                }
                for (const Function::CoroutineState& state : function.coroutine.states)
                {
                    if (!blockIds.contains(state.suspendBlock) || !blockIds.contains(state.resumeBlock) ||
                        !validType(state.resultType))
                        report("WBC1033", "Coroutine state references invalid block or type metadata", function.id);
                }
            }
            for (const Block& block : function.blocks)
            {
                for (std::size_t index = 0; index < block.instructions.size(); ++index)
                {
                    const Instruction& instruction = block.instructions[index];
                    if (!isKnownOpcode(instruction.opcode))
                        report("WBC1012", "Instruction contains an unknown opcode", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (!validType(instruction.resultType) || !validType(instruction.targetType))
                        report("WBC1013", "Instruction references an invalid type id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (instruction.constant >= module.constants.size())
                        report("WBC1014", "Instruction references an invalid constant id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    if (!validString(instruction.selector) || !validString(instruction.specializationKey))
                        report("WBC1015", "Instruction references an invalid string id", function.id, block.id,
                               static_cast<std::uint32_t>(index));
                    for (const StringId segment : instruction.stringSegments)
                    {
                        if (!validString(segment))
                            report("WBC1016", "Interpolation segment references an invalid string id", function.id,
                                   block.id, static_cast<std::uint32_t>(index));
                    }
                    for (const BranchTarget& target : instruction.targets)
                    {
                        if (!blockIds.contains(target.block))
                            report("WBC1017", "Branch target references a block outside the function", function.id,
                                   block.id, static_cast<std::uint32_t>(index));
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
        if (!validFunction(lifecycle.apiVersion) || !validFunction(lifecycle.load) || !validFunction(lifecycle.update) ||
            !validFunction(lifecycle.unload) || !validFunction(lifecycle.saveState) ||
            !validFunction(lifecycle.restoreState))
            report("WBC1117", "Module lifecycle references an invalid function id");
        if (module.abiDescriptorVersion == 0 || module.contract.callTableStableId == 0 ||
            module.contract.callTableEntries.size() != module.contract.exports.size())
            report("WBC1118", "SDK call table identity or export cardinality is invalid");
        return result;
    }
} // namespace wio::bytecode
