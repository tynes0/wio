#include "wio/codegen/wir_cpp_backend.h"
#include "wio/codegen/wir_intrinsics.h"
#include "wio/codegen/wir_module_emitter.h"
#include "wio/codegen/wir_reflection_emitter.h"

#include "wio/wir/lowered_ir_verifier.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace wio::codegen
{
    struct WirCppResultSink final
    {
        static void setCode(WirCppGenerationResult& result, std::string code)
        {
            result.code_ = std::move(code);
        }

        static void addDiagnostic(WirCppGenerationResult& result, WirCppDiagnostic diagnostic)
        {
            result.diagnostics_.push_back(std::move(diagnostic));
        }
    };

    namespace
    {
        using namespace wir;
        namespace lowered = wir::lowered;

        std::string cppString(const std::string_view value)
        {
            std::ostringstream output;
            output << '"';
            for (const unsigned char character : value)
            {
                switch (character)
                {
                case '\\': output << "\\\\"; break;
                case '"': output << "\\\""; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;
                default:
                    if (character < 0x20)
                        output << "\\x" << std::hex << std::setw(2) << std::setfill('0') <<
                            static_cast<unsigned>(character) << std::dec;
                    else
                        output << static_cast<char>(character);
                    break;
                }
            }
            output << '"';
            return output.str();
        }

        std::string safeIdentifier(const std::string_view value)
        {
            std::string result;
            result.reserve(value.size() + 1);
            for (const unsigned char character : value)
                result += std::isalnum(character) || character == '_' ? static_cast<char>(character) : '_';
            if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())))
                result.insert(result.begin(), '_');
            return result;
        }

        std::string typeName(const TypeId id) { return "_wio_t" + std::to_string(id.value()); }
        std::string objectName(const TypeId id) { return "_wio_obj" + std::to_string(id.value()); }
        std::string functionName(const FunctionId id) { return "_wio_f" + std::to_string(id.value()); }
        std::string globalName(const GlobalId id) { return "_wio_g" + std::to_string(id.value()); }
        std::string valueName(const ValueId id) { return "_v" + std::to_string(id.value()); }

        class Emitter final
        {
        public:
            Emitter(const lowered::Module& module, WirCppGenerationResult& result,
                    const WirCppBackendOptions& options)
                : module_(module), result_(result), options_(options)
            {
                for (const lowered::Function& function : module.functions)
                    functions_.emplace(function.id.value(), &function);
                for (const lowered::Global& global : module.globals)
                    globals_.emplace(global.id.value(), &global);
            }

            void run()
            {
                const lowered::VerificationResult verification = lowered::Verifier{}.verify(module_);
                for (const lowered::VerificationDiagnostic& diagnostic : verification.diagnostics())
                {
                    diagnose("WCPP1000", "invalid Lowered WIR: " + diagnostic.code + ": " +
                        diagnostic.message, diagnostic.source, diagnostic.function, diagnostic.block);
                }
                if (!verification.succeeded())
                    return;

                validateTypes();
                validateInstructions();
                validateWireBoundaries();
                if (!result_.succeeded())
                    return;

                emitPreamble();
                emitNominalDeclarations();
                output_ << WirReflectionEmitter::traits(module_,[this](TypeId id){return cppType(id);});
                emitFunctionDeclarations();
                emitHierarchyBodies();
                emitGlobals();
                emitFunctions();
                output_ << WirModuleEmitter::emit(module_, [this](TypeId id) { return cppType(id); });
                emitMain();
                WirCppResultSink::setCode(result_, output_.str());
            }

        private:
            const lowered::Module& module_;
            WirCppGenerationResult& result_;
            const WirCppBackendOptions& options_;
            std::ostringstream output_;
            std::unordered_map<std::uint32_t, const lowered::Function*> functions_;
            std::unordered_map<std::uint32_t, const lowered::Global*> globals_;
            const lowered::Function* currentFunction_ = nullptr;
            const lowered::BasicBlock* currentBlock_ = nullptr;
            std::unordered_map<std::uint32_t, TypeId> valueTypes_;
            std::set<std::uint32_t> borrowedLoads_;
            std::set<std::uint32_t> objectBorrowedLoads_;
            std::set<std::uint32_t> ownedValues_;

            void diagnose(std::string code, std::string message, const SourceSpan& source = {},
                          const FunctionId function = {}, const BlockId block = {})
            {
                WirCppResultSink::addDiagnostic(result_, WirCppDiagnostic{
                    .code = std::move(code), .message = std::move(message), .source = source,
                    .function = function, .block = block
                });
            }

            bool typeIsOpenImpl(const TypeId id, std::set<TypeId::ValueType>& visiting) const
            {
                const Type* type = module_.types.tryGet(id);
                if (!type) return true;
                if (type->kind == TypeKind::GenericParameter ||
                    type->kind == TypeKind::ConstGenericParameter ||
                    type->kind == TypeKind::GenericParameterPack ||
                    type->kind == TypeKind::ValuePack ||
                    type->kind == TypeKind::TypePack)
                    return true;
                if (!visiting.insert(id.value()).second)
                    return false;
                const auto open = [&](const TypeId nested)
                {
                    return nested && typeIsOpenImpl(nested, visiting);
                };
                const bool result = open(type->extentParameter) || std::ranges::any_of(type->arguments, open) ||
                    std::ranges::any_of(type->baseTypes, open) ||
                    std::ranges::any_of(type->fields, [&](const FieldLayout& field) { return open(field.type); });
                visiting.erase(id.value());
                return result;
            }

            bool typeIsOpen(const TypeId id) const
            {
                std::set<TypeId::ValueType> visiting;
                return typeIsOpenImpl(id, visiting);
            }

            bool functionIsOpen(const lowered::Function& function) const
            {
                if (!function.genericParameters.empty() || typeIsOpen(function.returnType) ||
                    (function.ownerType && typeIsOpen(function.ownerType)))
                    return true;
                return std::ranges::any_of(function.parameters,
                    [&](const lowered::Parameter& parameter) { return typeIsOpen(parameter.type); });
            }

            bool isSyntheticRootObject(const TypeId id) const
            {
                const Type* type = module_.types.tryGet(id);
                return type && type->kind == TypeKind::Named &&
                    type->nominalRepresentation == NominalRepresentation::Wio &&
                    type->nominalKind == NominalKind::Object && type->name == "object" &&
                    type->baseTypes.empty() && type->fields.empty();
            }

            void validateTypes()
            {
                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    // Open declarations remain useful canonical metadata, but this
                    // backend emits only already-specialized concrete layouts.
                    if (typeIsOpen(id))
                        continue;
                    switch (type.kind)
                    {
                    case TypeKind::Invalid:
                    case TypeKind::GenericParameter:
                    case TypeKind::ConstGenericParameter:
                    case TypeKind::GenericParameterPack:
                    case TypeKind::ValuePack:
                    case TypeKind::TypePack:
                        diagnose("WCPP1100", "unresolved type reached the C++ backend: " +
                            std::string(typeKindName(type.kind)));
                        break;
                    case TypeKind::ConstValue:
                        if (type.name.empty())
                            diagnose("WCPP1101", "const-value type has no canonical value");
                        break;
                    case TypeKind::Reference:
                    case TypeKind::Nullable:
                    case TypeKind::Array:
                    case TypeKind::AsyncTask:
                    case TypeKind::Iterator:
                        if (type.arguments.size() != 1)
                            diagnose("WCPP1102", "type " + std::to_string(index) + " requires one argument");
                        break;
                    case TypeKind::Dictionary:
                        if (type.arguments.size() != 2)
                            diagnose("WCPP1103", "dictionary type requires key and value arguments");
                        break;
                    case TypeKind::Function:
                        if (type.arguments.empty())
                            diagnose("WCPP1104", "function type requires a return type");
                        break;
                    case TypeKind::Named:
                        if (type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface)
                        {
                            if (type.castTypes.empty()) diagnose("WCPP1106", "object/interface requires canonical hierarchy lowering");
                            for (const auto& entry : type.dispatchEntries)
                            {
                                if (!entry.implementation) continue;
                                const auto function = functions_.find(entry.implementation.value());
                                const auto& method = dispatchMethod(entry);
                                bool valid = function != functions_.end() && !functionIsOpen(*function->second) &&
                                    !function->second->isAbstract && function->second->isMethod &&
                                    function->second->returnType == method.returnType &&
                                    function->second->parameters.size() == method.parameterTypes.size() + 1 &&
                                    std::ranges::find(type.castTypes, function->second->ownerType) != type.castTypes.end();
                                for (std::size_t p = 0; valid && p < method.parameterTypes.size(); ++p)
                                    valid = function->second->parameters[p + 1].type == method.parameterTypes[p];
                                if (!valid) diagnose("WCPP1212", "dispatch requires a concrete canonical implementation with a matching signature");
                            }
                            for (FunctionId lifecycle : {type.defaultConstructor, type.destructor})
                                if (lifecycle)
                                {
                                    const auto function = functions_.find(lifecycle.value());
                                    if (function == functions_.end() || functionIsOpen(*function->second) ||
                                        function->second->ownerType != id || function->second->parameters.size() != 1)
                                        diagnose("WCPP1212", "object lifecycle requires a concrete receiver-only body");
                                }
                        }
                        if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                            (!type.nativeBinding || type.nativeBinding->cppName.empty()))
                            diagnose("WCPP1105", "native POD type requires a C++ type binding");
                        if (type.nominalKind == NominalKind::Enum || type.nominalKind == NominalKind::Flagset)
                        {
                            const Type* underlying = module_.types.tryGet(type.enumUnderlyingType);
                            if (!underlying || !integerKind(underlying->kind) || type.enumCases.empty())
                                diagnose("WCPP1107", "enum/flagset requires canonical underlying-type and case layout metadata");
                        }
                        break;
                    default: break;
                    }
                }
                for (const lowered::Global& global : module_.globals)
                    if (typeIsOpen(global.type))
                        diagnose("WCPP1108", "global value requires a concrete specialized type", global.source);
            }

            void validateWireBoundaries() {
                if (module_.contract.application) {
                    const auto& app = *module_.contract.application;
                    const auto& fields = module_.types.get(app.type).fields;
                    if (std::ranges::none_of(fields, [](const auto& f) { return f.name == "__exitRequested"; }) ||
                        std::ranges::none_of(fields, [](const auto& f) { return f.name == "__exitCode"; }))
                        diagnose("WCPP1214", "application requires canonical exit-state fields");
                    for (const auto& stage : app.stages)
                        if (stage.affinity == ApplicationAffinity::Worker)
                            diagnose("WCPP1214",
                                     "worker application stages require a thread-transfer lowering contract");
                }
                for (const auto& attribute : module_.contract.attributes)
                    if (attribute.targetFunction)
                        for (const auto& processor : attribute.processors)
                            if (processor.phase == AttributeProcessorPhase::Pre ||
                                processor.phase == AttributeProcessorPhase::Post ||
                                processor.phase == AttributeProcessorPhase::Finally ||
                                processor.phase == AttributeProcessorPhase::Around)
                                diagnose(
                                    "WCPP1215",
                                    "behavioral attribute weaving is not yet available in the Lowered-WIR C++ backend",
                                    {}, attribute.targetFunction);
                for (const auto& function : module_.functions) {
                    if (functionIsOpen(function))
                        continue;
                    const bool exported = module_.contract.kind == ModuleKind::WioLibrary &&
                                          std::ranges::any_of(module_.contract.exports,
                                                              [&](const auto& e) { return e.function == function.id; });
                    if (!exported && !function.nativeBinding)
                        continue;
                    const auto& result = module_.types.get(function.returnType);
                    const bool async = result.kind == TypeKind::AsyncTask;
                    bool unsafe = result.kind == TypeKind::Reference ||
                        (async && module_.types.get(result.arguments.front()).kind == TypeKind::Reference);
                    for (const auto& parameter : function.parameters)
                        unsafe |= async && module_.types.get(parameter.type).kind == TypeKind::Reference;
                    if (unsafe)
                        diagnose("WCPP1213", "wire ABI requires owned results and forbids ref/view parameters on asynchronous calls",
                            {}, function.id);
                }
            }

            bool runtimeAsyncType(const TypeId id) const
            {
                const Type& type = module_.types.get(id);
                if (type.kind == TypeKind::AsyncTask || type.kind == TypeKind::Function)
                    return std::ranges::all_of(type.arguments, [&](TypeId argument) { return runtimeAsyncType(argument); });
                return type.kind == TypeKind::Void || type.kind == TypeKind::Bool ||
                    integerKind(type.kind) || type.kind == TypeKind::F32 || type.kind == TypeKind::F64 ||
                    type.kind == TypeKind::String || type.kind == TypeKind::Text;
            }

            bool runtimeAsyncBinding(const lowered::Function& function) const
            {
                // The in-process runtime uses exactly the same AsyncTask<T>
                // representation. This is not a general foreign/SDK adapter.
                if (!function.nativeBinding || function.nativeBinding->receiver != NativeReceiverKind::None ||
                    !function.nativeBinding->symbol.starts_with("wio::runtime::") ||
                    !runtimeAsyncType(function.returnType)) return false;
                const bool taskSignature = module_.types.get(function.returnType).kind == TypeKind::AsyncTask ||
                    std::ranges::any_of(function.parameters, [&](const lowered::Parameter& parameter)
                        { return module_.types.get(parameter.type).kind == TypeKind::AsyncTask; });
                if (!taskSignature) return false;
                return std::ranges::all_of(function.parameters, [&](const lowered::Parameter& parameter)
                    { return runtimeAsyncType(parameter.type); });
            }

            bool supportedOpcode(const lowered::Opcode opcode) const
            {
                switch (opcode)
                {
                case lowered::Opcode::GenericConstant:
                    return false;
                default:
                    return true;
                }
            }

            void validateInstructions()
            {
                for (const lowered::Function& function : module_.functions)
                {
                    if (functionIsOpen(function))
                        continue;
                    const auto functionValueTypes = valueTypesFor(function);
                    for (const lowered::BasicBlock& block : function.blocks)
                    {
                        for (const lowered::Instruction& instruction : block.instructions)
                        {
                            if (!supportedOpcode(instruction.opcode))
                                diagnose("WCPP1201", "opcode is not implemented by the WIR C++ backend: " +
                                    std::string(lowered::opcodeName(instruction.opcode)), instruction.source,
                                    function.id, block.id);
                            if (instruction.opcode == lowered::Opcode::IteratorCreate)
                            {
                                const Type* iterator = module_.types.tryGet(instruction.resultType);
                                const Type* source = iterator && iterator->arguments.size() == 1
                                    ? module_.types.tryGet(iterator->arguments.front()) : nullptr;
                                const bool range = instruction.selector == "range.inclusive" ||
                                    instruction.selector == "range.exclusive";
                                const bool valid = source && (range
                                    ? integerKind(source->kind) && instruction.operands.size() == 3
                                    : (instruction.selector == "array" && source->kind == TypeKind::Array &&
                                        (instruction.operands.size() == 1 || instruction.operands.size() == 2)) ||
                                      (instruction.selector == "dictionary" && source->kind == TypeKind::Dictionary &&
                                        instruction.operands.size() == 1));
                                if (!valid)
                                    diagnose("WCPP1210", "iterator source or operand count is not supported",
                                        instruction.source, function.id, block.id);
                            }
                            if (instruction.opcode == lowered::Opcode::IteratorValue)
                            {
                                const auto value = instruction.operands.empty() ? functionValueTypes.end() :
                                    functionValueTypes.find(instruction.operands.front().value());
                                const Type* iterator = value == functionValueTypes.end() ? nullptr :
                                    module_.types.tryGet(value->second);
                                const Type* source = iterator && iterator->arguments.size() == 1 ?
                                    module_.types.tryGet(iterator->arguments.front()) : nullptr;
                                bool valid = source && instruction.selector == "__value__" &&
                                    (integerKind(source->kind) || source->kind == TypeKind::Array);
                                if (source && source->kind == TypeKind::Dictionary)
                                    valid = instruction.selector == "first" || instruction.selector == "second";
                                if (source && source->kind == TypeKind::Array)
                                {
                                    valid |= instruction.selector == "__index__";
                                    const Type* item = source->arguments.empty() ? nullptr :
                                        module_.types.tryGet(source->arguments.front());
                                    valid |= item && std::ranges::any_of(item->fields, [&](const FieldLayout& field)
                                        { return field.name == instruction.selector; });
                                }
                                if (!valid)
                                    diagnose("WCPP1210", "iterator projection is not supported: " + instruction.selector,
                                        instruction.source, function.id, block.id);
                            }
                            if (instruction.callee)
                            {
                                const auto callee = functions_.find(instruction.callee.value());
                                if (callee != functions_.end() && functionIsOpen(*callee->second) &&
                                    !(callee->second->isAbstract && instruction.opcode == lowered::Opcode::InterfaceCall))
                                    diagnose("WCPP1209", "generic call requires a concrete specialized function body",
                                        instruction.source, function.id, block.id);
                            }
                            if (instruction.opcode == lowered::Opcode::IntrinsicCall &&
                                instruction.intrinsicFamily != IntrinsicFamily::Enum &&
                                instruction.intrinsicFamily != IntrinsicFamily::Flagset &&
                                !wirIntrinsicHelper(instruction.intrinsicFamily, instruction.selector))
                            {
                                diagnose("WCPP1205", "intrinsic family is not implemented by this C++ backend slice: " +
                                    std::string(intrinsicFamilyName(instruction.intrinsicFamily)), instruction.source,
                                    function.id, block.id);
                            }
                            if (instruction.opcode == lowered::Opcode::IntrinsicCall &&
                                !supportedEnumIntrinsic(instruction))
                            {
                                diagnose("WCPP1206", "enum/flagset intrinsic is not recognized: " +
                                    instruction.selector, instruction.source, function.id, block.id);
                            }
                            if (instruction.opcode == lowered::Opcode::EnumConstant &&
                                !findEnumCase(instruction.targetType, instruction.selector))
                            {
                                diagnose("WCPP1207", "enum/flagset constant does not exist in canonical layout: " +
                                    instruction.selector, instruction.source, function.id, block.id);
                            }
                            if (instruction.opcode == lowered::Opcode::VariantTest ||
                                instruction.opcode == lowered::Opcode::VariantPayload)
                            {
                                const auto source = instruction.operands.empty()
                                    ? functionValueTypes.end()
                                    : functionValueTypes.find(instruction.operands.front().value());
                                const Type* variant = source == functionValueTypes.end()
                                    ? nullptr
                                    : module_.types.tryGet(source->second);
                                if (!variant || (variant->nominalValueModel != NominalValueModel::Option &&
                                    variant->nominalValueModel != NominalValueModel::Result))
                                {
                                    diagnose("WCPP1208", "payload variants currently require canonical Option/Result layout",
                                        instruction.source, function.id, block.id);
                                }
                            }
                            if (instruction.opcode == lowered::Opcode::NativeInvoke)
                            {
                                const auto callee = functions_.find(instruction.callee.value());
                                if (callee == functions_.end() || !callee->second->nativeBinding ||
                                    functionIsOpen(*callee->second))
                                {
                                    diagnose("WCPP1202",
                                        "native invocation requires an adapter thunk not implemented by this backend",
                                        instruction.source, function.id, block.id);
                                }
                            }
                            if (instruction.opcode == lowered::Opcode::ConstructComponent ||
                                instruction.opcode == lowered::Opcode::ConstructObject)
                            {
                                const Type* target = module_.types.tryGet(instruction.resultType);
                                if (target && instruction.opcode == lowered::Opcode::ConstructObject &&
                                    std::ranges::any_of(target->dispatchEntries, [](const DispatchEntry& entry) { return !entry.implementation; }))
                                    diagnose("WCPP1211", "cannot construct an object with abstract method contracts", instruction.source, function.id, block.id);
                                if (instruction.callee)
                                {
                                    const auto body = functions_.find(instruction.callee.value());
                                    bool valid =
                                        body != functions_.end() && body->second->ownerType == instruction.resultType &&
                                        body->second->parameters.size() == instruction.signatureTypes.size() + 1 &&
                                        module_.types.get(body->second->returnType).kind == TypeKind::Void;
                                    for (std::size_t p = 0; valid && p < instruction.signatureTypes.size(); ++p)
                                        valid = body->second->parameters[p + 1].type == instruction.signatureTypes[p];
                                    if (!valid)
                                        diagnose("WCPP1203",
                                                 "constructor identity does not match its canonical argument signature",
                                                 instruction.source, function.id, block.id);
                                }
                                bool aggregateCompatible =
                                    valueModelConstructorCompatible(target, instruction, functionValueTypes);
                                if (target && target->nominalValueModel == NominalValueModel::Regular)
                                    aggregateCompatible = target->fields.size() == instruction.operands.size() ||
                                                          (instruction.operands.empty() && !target->hasConstructor);
                                if (module_.contract.application && instruction.operands.empty() &&
                                    instruction.resultType == module_.contract.application->type)
                                    aggregateCompatible = true;
                                for (std::size_t index = 0; aggregateCompatible && index < instruction.operands.size();
                                     ++index) {
                                    if (target->nominalValueModel != NominalValueModel::Regular)
                                        break;
                                    const auto value = functionValueTypes.find(instruction.operands[index].value());
                                    aggregateCompatible = value != functionValueTypes.end() &&
                                                          value->second == target->fields[index].type;
                                }
                                if (!aggregateCompatible && !instruction.callee)
                                    diagnose("WCPP1203",
                                             "constructor requires a callable constructor adapter; only canonical "
                                             "field-wise construction is enabled",
                                             instruction.source, function.id, block.id);
                            }
                        }
                    }
                }
            }

            static bool integerKind(const TypeKind kind)
            {
                return kind == TypeKind::I8 || kind == TypeKind::I16 || kind == TypeKind::I32 ||
                    kind == TypeKind::I64 || kind == TypeKind::ISize || kind == TypeKind::U8 ||
                    kind == TypeKind::U16 || kind == TypeKind::U32 || kind == TypeKind::U64 ||
                    kind == TypeKind::USize || kind == TypeKind::Byte;
            }

            bool supportedEnumIntrinsic(const lowered::Instruction& instruction) const
            {
                if (instruction.intrinsicFamily == IntrinsicFamily::Enum)
                    return instruction.selector == "Name" || instruction.selector == "Value" ||
                        instruction.selector == "IsValid";
                if (instruction.intrinsicFamily == IntrinsicFamily::Flagset)
                    return instruction.selector == "Name" || instruction.selector == "Has" ||
                        instruction.selector == "HasAny" || instruction.selector == "With" ||
                        instruction.selector == "Without" || instruction.selector == "Toggle" ||
                        instruction.selector == "Clear";
                return true;
            }

            bool valueModelConstructorCompatible(
                const Type* target,
                const lowered::Instruction& instruction,
                const std::unordered_map<std::uint32_t, TypeId>& valueTypes) const
            {
                if (!target) return false;
                if (target->nominalValueModel == NominalValueModel::Option)
                {
                    if (target->fields.size() < 2 || target->arguments.empty()) return false;
                    if (instruction.operands.empty()) return true;
                    const auto operandType = valueTypes.find(instruction.operands.front().value());
                    return instruction.operands.size() == 1 && operandType != valueTypes.end() &&
                        operandType->second == target->arguments.front();
                }
                if (target->nominalValueModel == NominalValueModel::Result)
                {
                    if (target->fields.size() < 3 || target->arguments.empty() || instruction.operands.size() != 1)
                        return false;
                    const auto operandType = valueTypes.find(instruction.operands.front().value());
                    return operandType != valueTypes.end() &&
                        (operandType->second == target->arguments.front() || operandType->second == target->fields[2].type);
                }
                return true;
            }

            std::unordered_map<std::uint32_t, TypeId> valueTypesFor(const lowered::Function& function) const
            {
                std::unordered_map<std::uint32_t, TypeId> result;
                for (const lowered::Parameter& parameter : function.parameters) result[parameter.id.value()] = parameter.type;
                for (const lowered::BasicBlock& block : function.blocks)
                {
                    for (const lowered::Parameter& parameter : block.parameters) result[parameter.id.value()] = parameter.type;
                    for (const lowered::Instruction& instruction : block.instructions)
                        if (instruction.result) result[instruction.result.value()] = instruction.resultType;
                }
                return result;
            }

            std::string cppType(const TypeId id) const
            {
                const Type* type = module_.types.tryGet(id);
                if (!type) return "void";
                switch (type->kind)
                {
                case TypeKind::Void: return "void";
                case TypeKind::Bool: return "bool";
                case TypeKind::I8: return "std::int8_t";
                case TypeKind::I16: return "std::int16_t";
                case TypeKind::I32: return "std::int32_t";
                case TypeKind::I64: return "std::int64_t";
                case TypeKind::ISize: return "std::ptrdiff_t";
                case TypeKind::U8:
                case TypeKind::Byte: return "std::uint8_t";
                case TypeKind::U16: return "std::uint16_t";
                case TypeKind::U32:
                    return "std::uint32_t";
                case TypeKind::Char: return "char";
                case TypeKind::U64: return "std::uint64_t";
                case TypeKind::USize: return "std::size_t";
                case TypeKind::F32: return "float";
                case TypeKind::F64: return "double";
                case TypeKind::String: return "std::string";
                case TypeKind::Text: return "wio::runtime::Text";
                case TypeKind::Any: return "wio::runtime::Any";
                case TypeKind::Opaque: return "void*";
                case TypeKind::ConstValue: return "std::integral_constant<std::int64_t, " + type->name + ">";
                case TypeKind::PackStorage:
                {
                    std::string value = "std::tuple<";
                    for (std::size_t i = 0; i < type->arguments.size(); ++i)
                    {
                        if (i) value += ", ";
                        value += cppType(type->arguments[i]);
                    }
                    return value + ">";
                }
                case TypeKind::Named:
                    if (type->nominalRepresentation == NominalRepresentation::NativePod)
                    {
                        std::string name = type->nativeBinding->cppName;
                        if (!type->arguments.empty() && name.find('<') == std::string::npos) {
                            name += '<';
                            for (std::size_t i = 0; i < type->arguments.size(); ++i) {
                                if (i) name += ", ";
                                const auto argument = type->arguments[i];
                                const auto& layout = module_.types.get(argument);
                                name += layout.kind == TypeKind::ConstValue ? layout.name : cppType(argument);
                            }
                            name += '>';
                        }
                        return name;
                    }
                    if (type->nominalKind == NominalKind::Object || type->nominalKind == NominalKind::Interface)
                        return "wio::runtime::Ref<" + objectName(id) + ">";
                    if (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset)
                        return typeName(id);
                    return typeName(id);
                case TypeKind::Reference:
                    return "wio::wir_backend::Place<" + cppType(type->arguments.front()) + ">";
                case TypeKind::Nullable:
                    return "std::optional<" + cppType(type->arguments.front()) + ">";
                case TypeKind::Array:
                    if (type->staticExtent)
                        return "std::array<" + cppType(type->arguments.front()) + ", " +
                            std::to_string(*type->staticExtent) + ">";
                    return "std::vector<" + cppType(type->arguments.front()) + ">";
                case TypeKind::Dictionary:
                    return (type->name == "ordered" ? "std::map<" : "std::unordered_map<") +
                        cppType(type->arguments[0]) + ", " + cppType(type->arguments[1]) + ">";
                case TypeKind::Function:
                {
                    std::string value = "std::function<" + cppType(type->arguments.back()) + "(";
                    for (std::size_t i = 0; i + 1 < type->arguments.size(); ++i)
                    {
                        if (i) value += ", ";
                        value += cppType(type->arguments[i]);
                    }
                    return value + ")>";
                }
                case TypeKind::AsyncTask:
                    return "wio::runtime::AsyncTask<" + cppType(type->arguments.front()) + ">";
                case TypeKind::Iterator:
                    return "wio::wir_backend::Iterator<" + cppType(type->arguments.front()) + ">";
                default: return "void";
                }
            }

            std::string placeValueType(const TypeId id) const
            {
                const Type* type = module_.types.tryGet(id);
                return type && type->kind == TypeKind::Reference && type->arguments.size() == 1
                    ? cppType(type->arguments.front())
                    : cppType(id);
            }

            std::string enumRawLiteral(const Type& type, const std::uint64_t rawValue) const
            {
                const Type* underlying = module_.types.tryGet(type.enumUnderlyingType);
                const bool signedValue = underlying &&
                    (underlying->kind == TypeKind::I8 || underlying->kind == TypeKind::I16 ||
                     underlying->kind == TypeKind::I32 || underlying->kind == TypeKind::I64 ||
                     underlying->kind == TypeKind::ISize);
                if (!signedValue) return std::to_string(rawValue) + "ULL";
                const std::int64_t value = static_cast<std::int64_t>(rawValue);
                if (value == std::numeric_limits<std::int64_t>::min())
                    return "(-9223372036854775807LL - 1LL)";
                return std::to_string(value) + "LL";
            }

            std::string enumCaseExpression(const TypeId id, const EnumCaseLayout& enumCase) const
            {
                const Type& type = module_.types.get(id);
                return "static_cast<" + cppType(id) + ">(static_cast<" +
                    cppType(type.enumUnderlyingType) + ">(" + enumRawLiteral(type, enumCase.rawValue) + "))";
            }

            const EnumCaseLayout* findEnumCase(const TypeId id, const std::string_view name) const
            {
                const Type* type = module_.types.tryGet(id);
                if (!type) return nullptr;
                const auto found = std::ranges::find(type->enumCases, name, &EnumCaseLayout::name);
                return found == type->enumCases.end() ? nullptr : &*found;
            }

            void emitPreamble()
            {
                output_ << "#include <wir_application.h>\n#include <wio_native_reflection.h>\n#include <type_reflection.h>\n";
                output_ << "// Generated by Wio's canonical Lowered WIR C++ backend.\n"
                    "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <functional>\n#include <limits>\n"
                    "#include <map>\n#include <memory>\n#include <optional>\n#include <sstream>\n"
                    "#include <stdexcept>\n#include <string>\n#include <tuple>\n#include <type_traits>\n"
                    "#include <unordered_map>\n#include <utility>\n#include <vector>\n"
                    "#include <any.h>\n#include <intrinsics.h>\n#include <ref.h>\n#include <std_async.h>\n#include <text.h>\n#include <wir_iterator.h>\n#include <wir_async.h>\n";
                output_ << "#include <wir_abi.h>\n#include <wir_native.h>\n#include <module_api.h>\n#include <wio_module_contract.h>\n";
                std::set<std::string> headers;
                for (const Type& type : module_.types.types())
                    if (type.nativeBinding && !type.nativeBinding->header.empty()) headers.insert(type.nativeBinding->header);
                for (const lowered::Function& function : module_.functions)
                    if (function.nativeBinding && !function.nativeBinding->header.empty()) headers.insert(function.nativeBinding->header);
                for (const ModuleImport& import : module_.contract.imports)
                    if (import.kind == ModuleImportKind::NativeHeader && !import.sourcePath.empty()) headers.insert(import.sourcePath);
                for (const std::string& header : headers)
                    output_ << "#include \"" << header << "\"\n";

                output_ << R"CPP(
namespace wio::wir_backend {
struct SkipConstructor {};
template<class T> class Place {
public:
    static Place local() {
        Place place;
        place.owner_ = std::make_shared<std::optional<T>>();
        return place;
    }
    static Place borrow(T& value) { Place place; place.pointer_ = &value; return place; }
    static Place borrowView(const T& value) { Place place; place.pointer_ = const_cast<T*>(&value); place.readOnly_ = true; return place; }
    T& read() {
        if (pointer_) return *pointer_;
        if (!owner_ || !owner_->has_value()) throw std::runtime_error("read from uninitialized Wio place");
        return owner_->value();
    }
    const T& read() const { return const_cast<Place*>(this)->read(); }
    T take() {
        if (readOnly_) throw std::runtime_error("move through a Wio view");
        T value = std::move(read());
        clear();
        return value;
    }
    void write(T value) {
        if (readOnly_) throw std::runtime_error("write through a Wio view");
        if (pointer_) *pointer_ = std::move(value);
        else { if (!owner_) owner_ = std::make_shared<std::optional<T>>(); *owner_ = std::move(value); }
    }
    void clear() { if (readOnly_) throw std::runtime_error("clear through a Wio view"); if (pointer_) *pointer_ = T{}; else if (owner_) owner_->reset(); }
private:
    std::shared_ptr<std::optional<T>> owner_;
    T* pointer_ = nullptr;
    bool readOnly_ = false;
};
template<class T> T& value_base(T& value) { return value; }
template<class T> T& value_base(wio::runtime::Ref<T>& value) { return *value; }
template<class T> decltype(auto) value_base(Place<T>& value) { return value_base(value.read()); }
template<class T, class U> wio::runtime::Ref<T> checked_ref_cast(const wio::runtime::Ref<U>& value) {
    if (auto* casted = dynamic_cast<T*>(value.Get())) return wio::runtime::Ref<T>(casted);
    return {};
}
// Object places borrow handles (or raw self), never manufacture an owning
// self-reference. Local storage still owns exactly one ordinary Ref<T>.
template<class T> class Place<wio::runtime::Ref<T>> {
    using Handle = wio::runtime::Ref<T>;
    std::shared_ptr<std::optional<Handle>> owner_;
    Handle* handle_ = nullptr;
    T* raw_ = nullptr;
    bool readOnly_ = false;
public:
    static Place local() { Place p; p.owner_ = std::make_shared<std::optional<Handle>>(); return p; }
    static Place borrow(Handle& h) { Place p; p.handle_ = &h; return p; }
    static Place borrowView(const Handle& h) { auto p = borrow(const_cast<Handle&>(h)); p.readOnly_ = true; return p; }
    static Place raw(T* value) { Place p; p.raw_ = value; return p; }
    T* Get() const {
        if (handle_) return handle_->Get();
        if (owner_) { if (!*owner_) throw std::runtime_error("read from uninitialized Wio object place"); return owner_->value().Get(); }
        return raw_;
    }
    Handle read() const { return Handle(Get()); }
    void write(Handle value) {
        if (readOnly_) throw std::runtime_error("write through a Wio view");
        if (handle_) *handle_ = std::move(value);
        else if (owner_) *owner_ = std::move(value);
        else throw std::runtime_error("rebind of raw Wio self");
    }
    void clear() { if (readOnly_) throw std::runtime_error("clear through a Wio view"); if (handle_) handle_->Reset(); else if (owner_) owner_->reset(); }
    Handle take() { auto value = read(); clear(); return value; }
};
template<class T> T& value_base(Place<wio::runtime::Ref<T>>& value) {
    if (!value.Get()) throw std::runtime_error("null Wio object access");
    return *value.Get();
}
template<class T> T* object_ptr(const wio::runtime::Ref<T>& value) { return value.Get(); }
template<class T> T* object_ptr(const Place<wio::runtime::Ref<T>>& value) { return value.Get(); }
template<class T> struct ObjectBorrow {
    T* pointer;
    T* Get() const { return pointer; }
    T* operator->() const { return pointer; }
    operator wio::runtime::Ref<T>() const { return wio::runtime::Ref<T>(pointer); }
};
template<class T> T* object_ptr(const ObjectBorrow<T>& value) { return value.Get(); }
template<class T> T& value_base(ObjectBorrow<T>& value) { if (!value.Get()) throw std::runtime_error("null Wio object access"); return *value.Get(); }
template<class T> auto object_ptr(const std::optional<T>& value) { return value ? object_ptr(*value) : nullptr; }
template<class T, class U> T* object_cast(const U& value) { return dynamic_cast<T*>(object_ptr(value)); }
template<class T, class U> T* require_object_cast(const U& value) {
    auto* result = object_cast<T>(value);
    if (!result) throw std::runtime_error("Wio object fit target mismatch");
    return result;
}
template<class T> wio::runtime::RefCountedObject* object_identity(const T& value) { return object_ptr(value); }
template<class T> std::string stringify(const T& value) { std::ostringstream stream; stream << value; return stream.str(); }
inline std::string stringify(const bool value) { return value ? "true" : "false"; }
inline std::string stringify(const std::string& value) { return value; }
}

)CPP";
            }

            std::vector<TypeId> nominalOrder() const
            {
                std::vector<TypeId> order;
                std::set<TypeId> seen;
                std::function<void(TypeId)> visit = [&](TypeId id)
                {
                    if (!seen.insert(id).second || typeIsOpen(id)) return;
                    const auto& type = module_.types.get(id);
                    if (type.kind != TypeKind::Named || type.nominalRepresentation != NominalRepresentation::Wio ||
                        type.nominalKind == NominalKind::Enum || type.nominalKind == NominalKind::Flagset) return;
                    for (TypeId base : type.baseTypes) visit(base);
                    for (const auto& field : type.fields)
                        if (module_.types.get(field.type).nominalKind == NominalKind::Component) visit(field.type);
                    order.push_back(id);
                };
                for (std::size_t i = 0; i < module_.types.size(); ++i) visit(TypeId{static_cast<TypeId::ValueType>(i)});
                return order;
            }

            static std::string dispatchName(TypeId contract, std::uint32_t slot)
            { return "_dispatch_" + std::to_string(contract.value()) + "_" + std::to_string(slot); }

            TypeId nominalType(TypeId id) const
            {
                while (module_.types.get(id).kind == TypeKind::Reference || module_.types.get(id).kind == TypeKind::Nullable)
                    id = module_.types.get(id).arguments.front();
                return id;
            }

            const MethodLayout& dispatchMethod(const DispatchEntry& entry) const
            {
                const auto& methods = module_.types.get(entry.contractType).methods;
                return *std::ranges::find(methods, entry.slot, &MethodLayout::slot);
            }

            std::string dispatchSignature(const DispatchEntry& entry, const std::string& owner = {}) const
            {
                const auto& method = dispatchMethod(entry);
                std::string result = cppType(method.returnType) + " " + owner + dispatchName(entry.contractType, entry.slot) + "(";
                for (std::size_t i = 0; i < method.parameterTypes.size(); ++i)
                {
                    if (i) result += ", ";
                    result += cppType(method.parameterTypes[i]) + " _a" + std::to_string(i);
                }
                return result + ")";
            }

            void emitHierarchyBodies()
            {
                for (TypeId id : nominalOrder())
                {
                    const auto& type = module_.types.get(id);
                    for (const auto& entry : type.dispatchEntries)
                    {
                        if (!entry.implementation) continue;
                        const auto& function = *functions_.at(entry.implementation.value());
                        output_ << dispatchSignature(entry, objectName(id) + "::") << " { return " << functionName(function.id) << "("
                            << cppType(function.parameters.front().type) << "::raw(static_cast<" << objectName(function.ownerType) << "*>(this))";
                        for (std::size_t i = 0; i < dispatchMethod(entry).parameterTypes.size(); ++i)
                            output_ << ", std::move(_a" << i << ")";
                        output_ << "); }\n";
                    }
                    if (type.destructor)
                    {
                        const auto& function = *functions_.at(type.destructor.value());
                        output_ << objectName(id) << "::~" << objectName(id) << "() { " << functionName(function.id) << "("
                            << cppType(function.parameters.front().type) << "::raw(this)); }\n";
                    }
                    if (type.defaultConstructor)
                    {
                        const auto& function = *functions_.at(type.defaultConstructor.value());
                        output_ << objectName(id) << "::" << objectName(id) << "() { " << functionName(function.id) << "("
                            << cppType(function.parameters.front().type) << "::raw(this)); }\n";
                    }
                }
            }

            void emitNominalDeclarations()
            {
                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (typeIsOpen(id) || type.kind != TypeKind::Named ||
                        type.nominalRepresentation != NominalRepresentation::NativePod ||
                        type.nominalKind != NominalKind::Component) continue;
                    const auto name = cppType(id);
                    output_ << "static_assert(std::is_standard_layout_v<" << name << "> && std::is_trivially_copyable_v<" << name
                        << ">, \"Wio native component requires a standard-layout trivially-copyable C++ type\");\n";
                    for (const auto& field : type.fields)
                        output_ << "static_assert(std::is_same_v<decltype(std::declval<" << name << ">()." << field.name << "), "
                            << cppType(field.type) << ">, \"Wio native component field type mismatch\");\n";
                }
                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (typeIsOpen(id) || type.kind != TypeKind::Named ||
                        type.nominalRepresentation != NominalRepresentation::Wio ||
                        (type.nominalKind != NominalKind::Enum && type.nominalKind != NominalKind::Flagset))
                        continue;
                    output_ << "enum class " << typeName(id) << " : " << cppType(type.enumUnderlyingType) << " {\n";
                    for (std::size_t caseIndex = 0; caseIndex < type.enumCases.size(); ++caseIndex)
                    {
                        const EnumCaseLayout& enumCase = type.enumCases[caseIndex];
                        output_ << "    " << safeIdentifier(enumCase.name) << " = static_cast<" <<
                            cppType(type.enumUnderlyingType) << ">(" << enumRawLiteral(type, enumCase.rawValue) << ")" <<
                            (caseIndex + 1 == type.enumCases.size() ? "\n" : ",\n");
                    }
                    output_ << "};\n";
                    if (type.nominalKind == NominalKind::Flagset)
                    {
                        const std::string name = typeName(id);
                        const std::string underlying = cppType(type.enumUnderlyingType);
                        output_ << "inline " << name << " operator|(" << name << " lhs, " << name << " rhs) noexcept { return static_cast<" <<
                            name << ">(static_cast<" << underlying << ">(lhs) | static_cast<" << underlying << ">(rhs)); }\n";
                        output_ << "inline " << name << " operator&(" << name << " lhs, " << name << " rhs) noexcept { return static_cast<" <<
                            name << ">(static_cast<" << underlying << ">(lhs) & static_cast<" << underlying << ">(rhs)); }\n";
                        output_ << "inline " << name << " operator^(" << name << " lhs, " << name << " rhs) noexcept { return static_cast<" <<
                            name << ">(static_cast<" << underlying << ">(lhs) ^ static_cast<" << underlying << ">(rhs)); }\n";
                        output_ << "inline " << name << " operator~(" << name << " value) noexcept { return static_cast<" <<
                            name << ">(~static_cast<" << underlying << ">(value)); }\n";
                    }
                }
                output_ << '\n';

                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (!typeIsOpen(id) && type.kind == TypeKind::Named &&
                        type.nominalRepresentation == NominalRepresentation::Wio &&
                        (type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface))
                        output_ << "struct " << objectName(id) << ";\n";
                }
                output_ << '\n';

                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (typeIsOpen(id) || type.kind != TypeKind::Named ||
                        (type.nominalKind != NominalKind::Enum && type.nominalKind != NominalKind::Flagset))
                        continue;
                    const std::string valueType = cppType(id);
                    const std::string underlying = cppType(type.enumUnderlyingType);
                    output_ << "static bool _wio_enum_valid" << id.value() << "(" << valueType << " value) noexcept {\n";
                    for (const EnumCaseLayout& enumCase : type.enumCases)
                        output_ << "    if (value == " << enumCaseExpression(id, enumCase) << ") return true;\n";
                    output_ << "    return false;\n}\n";
                    output_ << "static std::string _wio_enum_name" << id.value() << "(" << valueType << " value) {\n";
                    for (const EnumCaseLayout& enumCase : type.enumCases)
                        output_ << "    if (value == " << enumCaseExpression(id, enumCase) << ") return " <<
                            cppString(enumCase.name) << ";\n";
                    if (type.nominalKind == NominalKind::Flagset)
                    {
                        output_ << "    const " << underlying << " raw = static_cast<" << underlying << ">(value);\n"
                            "    " << underlying << " remaining = raw;\n    std::string result;\n";
                        for (const EnumCaseLayout& enumCase : type.enumCases)
                        {
                            if (enumCase.rawValue == 0) continue;
                            output_ << "    { const " << underlying << " member = static_cast<" << underlying << ">(" <<
                                enumRawLiteral(type, enumCase.rawValue) << "); if ((raw & member) == member) { if (!result.empty()) result += \"|\"; result += " <<
                                cppString(enumCase.name) << "; remaining = static_cast<" << underlying << ">(remaining & ~member); } }\n";
                        }
                        output_ << "    if (remaining != 0) { if (!result.empty()) result += \"|\"; result += \"<unknown>\"; }\n"
                            "    if (result.empty()) return raw == 0 ? \"0\" : \"<unknown>\";\n    return result;\n";
                    }
                    else
                    {
                        output_ << "    return \"<unknown>\";\n";
                    }
                    output_ << "}\n";
                }
                output_ << '\n';

                for (const TypeId id : nominalOrder())
                {
                    const Type& type = module_.types.get(id);
                    const bool object = type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface;
                    output_ << "struct " << (object ? objectName(id) : typeName(id));
                    if (object)
                    {
                        output_ << " : virtual wio::runtime::RefCountedObject";
                        for (TypeId base : type.baseTypes)
                            output_ << ", " << (module_.types.get(base).nominalKind == NominalKind::Interface ? "virtual " : "") << objectName(base);
                    }
                    output_ << " {\n";
                    for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
                        output_ << "    " << cppType(type.fields[fieldIndex].type) << " _f" << fieldIndex << "{}; // " <<
                            safeIdentifier(type.fields[fieldIndex].name) << "\n";
                    if (object)
                    {
                        output_ << "    static constexpr std::uint64_t TYPE_ID = " << id.value() << ";\n";
                        output_ << "    bool _WF_IsA(std::uint64_t id) const override { return ";
                        for (TypeId cast : type.castTypes) output_ << "id == " << cast.value() << " || ";
                        output_ << "false; }\n    void* _WF_CastTo(std::uint64_t id) override {\n";
                        for (TypeId cast : type.castTypes)
                            output_ << "        if (id == " << cast.value() << ") return static_cast<" << objectName(cast) << "*>(this);\n";
                        output_ << "        return nullptr;\n    }\n";
                        for (const DispatchEntry& entry : type.dispatchEntries)
                            output_ << "    virtual " << dispatchSignature(entry) << (entry.implementation ? ";\n" : " = 0;\n");
                        if (type.destructor) output_ << "    ~" << objectName(id) << "() override;\n";
                        output_ << "    " << objectName(id) << (type.defaultConstructor ? "();\n" : "() = default;\n");
                        output_ << "    explicit " << objectName(id) << "(wio::wir_backend::SkipConstructor) {}\n";
                        if (type.nominalValueModel == NominalValueModel::Option && type.fields.size() >= 2 &&
                            !type.arguments.empty())
                        {
                            output_ << "    explicit " << objectName(id) << '(' << cppType(type.arguments.front()) <<
                                " value) : _f0(true), _f1(std::move(value)) {}\n";
                        }
                        else if (type.nominalValueModel == NominalValueModel::Result && type.fields.size() >= 3 &&
                            !type.arguments.empty())
                        {
                            output_ << "    explicit " << objectName(id) << '(' << cppType(type.arguments.front()) <<
                                " value) : _f0(true), _f1(std::move(value)) {}\n";
                            output_ << "    explicit " << objectName(id) << '(' << cppType(type.fields[2].type) <<
                                " error) : _f0(false), _f2(std::move(error)) {}\n";
                        }
                        else if (!type.fields.empty())
                        {
                            output_ << "    " << objectName(id) << '(';
                            for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
                            {
                                if (fieldIndex) output_ << ", ";
                                output_ << cppType(type.fields[fieldIndex].type) << " v" << fieldIndex;
                            }
                            output_ << ") : ";
                            for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
                            {
                                if (fieldIndex) output_ << ", ";
                                output_ << "_f" << fieldIndex << "(std::move(v" << fieldIndex << "))";
                            }
                            output_ << " {}\n";
                        }
                    }
                    output_ << "};\n";
                }
                output_ << '\n';
            }

            std::string functionSignature(const lowered::Function& function, const bool declaration) const
            {
                std::ostringstream signature;
                signature << (function.isExternal && !function.nativeBinding ? "extern " : "") << cppType(function.returnType) << ' ' <<
                    functionName(function.id) << '(';
                for (std::size_t index = 0; index < function.parameters.size(); ++index)
                {
                    if (index) signature << ", ";
                    signature << cppType(function.parameters[index].type) << " _p" << index;
                }
                signature << ')';
                if (declaration) signature << ';';
                return signature.str();
            }

            void emitGlobals()
            {
                for (const lowered::Global& global : module_.globals)
                {
                    output_ << (global.isConst ? "const " : "") << cppType(global.type) << ' ' << globalName(global.id);
                    if (global.initializer) output_ << " = " << functionName(global.initializer) << "()";
                    else output_ << "{}";
                    output_ << ";\n";
                }
                if (!module_.globals.empty()) output_ << '\n';
            }

            void emitFunctionDeclarations()
            {
                for (const lowered::Function& function : module_.functions)
                    if (!functionIsOpen(function))
                        output_ << functionSignature(function, true) << '\n';
                output_ << '\n';
            }

            void collectValueTypes(const lowered::Function& function)
            {
                valueTypes_.clear();
                borrowedLoads_.clear();
                objectBorrowedLoads_.clear();
                ownedValues_.clear();
                for (const lowered::Parameter& parameter : function.parameters)
                {
                    valueTypes_[parameter.id.value()] = parameter.type;
                    if (parameter.ownership == typed::ValueOwnership::Owned) ownedValues_.insert(parameter.id.value());
                }
                for (const lowered::BasicBlock& block : function.blocks)
                {
                    for (const lowered::Parameter& parameter : block.parameters)
                    {
                        valueTypes_[parameter.id.value()] = parameter.type;
                        if (parameter.ownership == typed::ValueOwnership::Owned) ownedValues_.insert(parameter.id.value());
                    }
                    for (const lowered::Instruction& instruction : block.instructions)
                    {
                        if (instruction.result) valueTypes_[instruction.result.value()] = instruction.resultType;
                        if (instruction.resultOwnership == typed::ValueOwnership::Owned) ownedValues_.insert(instruction.result.value());
                        if (instruction.opcode == lowered::Opcode::Load &&
                            instruction.resultOwnership == typed::ValueOwnership::Borrowed &&
                            (module_.types.get(instruction.resultType).nominalKind == NominalKind::Object ||
                             module_.types.get(instruction.resultType).nominalKind == NominalKind::Interface))
                            objectBorrowedLoads_.insert(instruction.result.value());
                        if (instruction.opcode == lowered::Opcode::Load &&
                            instruction.resultOwnership == typed::ValueOwnership::Borrowed &&
                            module_.types.get(instruction.resultType).nominalKind != NominalKind::Object &&
                            module_.types.get(instruction.resultType).nominalKind != NominalKind::Interface)
                            borrowedLoads_.insert(instruction.result.value());
                    }
                }
            }

            std::string operand(const ValueId id) const
            {
                return borrowedLoads_.contains(id.value()) ? "(" + valueName(id) + "->get())" : "(*" + valueName(id) + ")";
            }
            std::string movedOperand(const ValueId id) const { return "std::move(" + operand(id) + ")"; }

            void emitSource(const SourceSpan& source)
            {
                if (options_.emitLineDirectives && source.hasSourceContext())
                    output_ << "#line " << source.begin.line << ' ' << cppString(source.begin.file) << "\n";
            }

            std::string literal(const lowered::Instruction& instruction) const
            {
                if (std::holds_alternative<typed::NullLiteral>(instruction.literal) ||
                    std::holds_alternative<std::monostate>(instruction.literal)) return "{}";
                if (const auto* value = std::get_if<bool>(&instruction.literal)) return *value ? "true" : "false";
                if (const auto* value = std::get_if<std::int64_t>(&instruction.literal)) return std::to_string(*value);
                if (const auto* value = std::get_if<std::uint64_t>(&instruction.literal)) return std::to_string(*value) + "ULL";
                if (const auto* value = std::get_if<double>(&instruction.literal))
                {
                    std::ostringstream text;
                    text << std::setprecision(17) << *value;
                    return text.str();
                }
                if (const auto* value = std::get_if<std::string>(&instruction.literal))
                {
                    const Type* resultType = module_.types.tryGet(instruction.resultType);
                    return resultType && resultType->kind == TypeKind::Text
                        ? "wio::runtime::Text::FromUtf8(" + cppString(*value) + ")"
                        : cppString(*value);
                }
                return "{}";
            }

            static std::string binaryOperator(const typed::BinaryOperator operation)
            {
                switch (operation)
                {
                case typed::BinaryOperator::Add: return "+";
                case typed::BinaryOperator::Subtract: return "-";
                case typed::BinaryOperator::Multiply: return "*";
                case typed::BinaryOperator::Divide: return "/";
                case typed::BinaryOperator::Remainder: return "%";
                case typed::BinaryOperator::Equal: return "==";
                case typed::BinaryOperator::NotEqual: return "!=";
                case typed::BinaryOperator::Less: return "<";
                case typed::BinaryOperator::LessEqual: return "<=";
                case typed::BinaryOperator::Greater: return ">";
                case typed::BinaryOperator::GreaterEqual: return ">=";
                case typed::BinaryOperator::BitwiseAnd: return "&";
                case typed::BinaryOperator::BitwiseOr: return "|";
                case typed::BinaryOperator::BitwiseXor: return "^";
                case typed::BinaryOperator::ShiftLeft: return "<<";
                case typed::BinaryOperator::ShiftRight: return ">>";
                }
                return "+";
            }

            bool integerType(const TypeId id) const
            {
                const Type* type = module_.types.tryGet(id);
                return type && (type->kind == TypeKind::I8 || type->kind == TypeKind::I16 ||
                    type->kind == TypeKind::I32 || type->kind == TypeKind::I64 || type->kind == TypeKind::ISize ||
                    type->kind == TypeKind::U8 || type->kind == TypeKind::U16 || type->kind == TypeKind::U32 ||
                    type->kind == TypeKind::U64 || type->kind == TypeKind::USize || type->kind == TypeKind::Byte);
            }

            std::string nativeCall(const lowered::Function& callee, const lowered::Instruction& instruction) const
            {
                return functionName(callee.id) + "(" + callArguments(instruction) + ")";
            }

            std::string callArguments(const lowered::Instruction& instruction, const std::size_t begin = 0, const bool consume = false) const
            {
                std::string arguments;
                for (std::size_t index = begin; index < instruction.operands.size(); ++index)
                {
                    if (index != begin) arguments += ", ";
                    arguments += consume && ownedValues_.contains(instruction.operands[index].value())
                        ? movedOperand(instruction.operands[index]) : operand(instruction.operands[index]);
                }
                return arguments;
            }

            TypeId valueType(const ValueId value) const
            {
                const auto found = valueTypes_.find(value.value());
                return found == valueTypes_.end() ? TypeId{} : found->second;
            }

            std::string intrinsicExpression(const lowered::Instruction& instruction) const
            {
                if (const auto helper = wirIntrinsicHelper(instruction.intrinsicFamily, instruction.selector))
                {
                    std::vector<std::string> arguments;
                    for (const ValueId id : instruction.operands)
                    {
                        const Type& type = module_.types.get(valueType(id));
                        arguments.push_back(operand(id) + (type.kind == TypeKind::Reference ? ".read()" : ""));
                    }
                    const Type* result = module_.types.tryGet(instruction.resultType);
                    if (result && result->nominalValueModel == NominalValueModel::Option &&
                        (instruction.selector == "Get" || instruction.selector == "At"))
                    {
                        const std::string option = cppType(instruction.resultType);
                        const std::string& receiver = arguments[0];
                        const std::string& key = arguments[1];
                        if (instruction.intrinsicFamily == IntrinsicFamily::Dictionary)
                            return "([&]() -> " + option + " { auto _it = " + receiver + ".find(" + key +
                                "); if (_it == " + receiver + ".end()) return " + option + "::Create(); return " +
                                option + "::Create(_it->second); }())";
                        return "([&]() -> " + option + " { if (" + key + " >= " + receiver +
                            ".size()) return " + option + "::Create(); return " + option +
                            "::Create(wio::intrinsics::Index(" + receiver + ", " + key + ")); }())";
                    }
                    std::string call = "wio::intrinsics::" + *helper + "(";
                    for (std::size_t index = 0; index < arguments.size(); ++index)
                    {
                        if (index) call += ", ";
                        call += arguments[index];
                    }
                    return call + ")";
                }
                const Type& target = module_.types.get(instruction.targetType);
                const std::string receiver = operand(instruction.operands.front());
                const std::string underlying = cppType(target.enumUnderlyingType);
                if (instruction.selector == "Name")
                    return "_wio_enum_name" + std::to_string(instruction.targetType.value()) + "(" + receiver + ")";
                if (instruction.selector == "Value")
                    return "static_cast<" + underlying + ">(" + receiver + ")";
                if (instruction.selector == "IsValid")
                    return "_wio_enum_valid" + std::to_string(instruction.targetType.value()) + "(" + receiver + ")";
                if (instruction.selector == "Clear")
                    return "static_cast<" + cppType(instruction.targetType) + ">(static_cast<" + underlying + ">(0))";

                const std::string mask = operand(instruction.operands[1]);
                const std::string rawReceiver = "static_cast<" + underlying + ">(" + receiver + ")";
                const std::string rawMask = "static_cast<" + underlying + ">(" + mask + ")";
                if (instruction.selector == "Has")
                    return "((" + rawReceiver + " & " + rawMask + ") == " + rawMask + ")";
                if (instruction.selector == "HasAny")
                    return "((" + rawReceiver + " & " + rawMask + ") != 0)";
                const std::string operation = instruction.selector == "With" ? " | " :
                    instruction.selector == "Toggle" ? " ^ " : " & ~";
                return "static_cast<" + cppType(instruction.targetType) + ">(" + rawReceiver + operation + rawMask + ")";
            }

            std::string resultPayload(const lowered::Instruction& instruction, const bool checked) const
            {
                const std::string source = operand(instruction.operands.front());
                if (!checked) return source + "->_f1";
                return "([&]() -> " + cppType(instruction.resultType) + " { if (!" + source +
                    "->_f0) throw wio::runtime::RuntimeException(\"Result does not contain a success value.\"); return " +
                    source + "->_f1; }())";
            }

            void assignResult(const lowered::Instruction& instruction, const std::string& expression)
            {
                if (instruction.result) output_ << "                " << valueName(instruction.result) << " = " << expression << ";\n";
                else output_ << "                " << expression << ";\n";
            }

            void emitBranchAssignments(const lowered::BranchTarget& target)
            {
                const lowered::BasicBlock* destination = nullptr;
                for (const lowered::BasicBlock& block : currentFunction_->blocks)
                    if (block.id == target.block) destination = &block;
                for (std::size_t index = 0; destination && index < target.arguments.size(); ++index)
                    output_ << "                " << valueName(destination->parameters[index].id) << " = " <<
                        (destination->parameters[index].ownership == typed::ValueOwnership::Owned
                            ? movedOperand(target.arguments[index]) : operand(target.arguments[index])) << ";\n";
                output_ << "                _block = " << target.block.value() << "; continue;\n";
            }

            void emitInstruction(const lowered::Instruction& instruction)
            {
                emitSource(instruction.source);
                if (options_.emitComments)
                    output_ << "                // " << lowered::opcodeName(instruction.opcode) << "\n";
                switch (instruction.opcode)
                {
                case lowered::Opcode::Constant: assignResult(instruction, literal(instruction)); break;
                case lowered::Opcode::Unary:
                    if (instruction.unaryOperator == typed::UnaryOperator::Negate && integerType(instruction.resultType))
                        assignResult(instruction, "wio::intrinsics::WrappingNeg<" + cppType(instruction.resultType) + ">(" +
                            operand(instruction.operands[0]) + ")");
                    else
                        assignResult(instruction, (instruction.unaryOperator == typed::UnaryOperator::Negate ? "-" :
                            instruction.unaryOperator == typed::UnaryOperator::LogicalNot ? "!" : "~") + operand(instruction.operands[0]));
                    break;
                case lowered::Opcode::Binary:
                    if (integerType(instruction.resultType) &&
                        (instruction.binaryOperator == typed::BinaryOperator::Add ||
                         instruction.binaryOperator == typed::BinaryOperator::Subtract ||
                         instruction.binaryOperator == typed::BinaryOperator::Multiply ||
                         instruction.binaryOperator == typed::BinaryOperator::Divide ||
                         instruction.binaryOperator == typed::BinaryOperator::Remainder))
                    {
                        const char* helper = instruction.binaryOperator == typed::BinaryOperator::Add ? "WrappingAdd" :
                            instruction.binaryOperator == typed::BinaryOperator::Subtract ? "WrappingSub" :
                            instruction.binaryOperator == typed::BinaryOperator::Multiply ? "WrappingMul" :
                            instruction.binaryOperator == typed::BinaryOperator::Divide ? "IntegerDivide" : "IntegerRemainder";
                        assignResult(instruction, "wio::intrinsics::" + std::string(helper) + "<" + cppType(instruction.resultType) +
                            ">(" + operand(instruction.operands[0]) + ", " + operand(instruction.operands[1]) + ")");
                    }
                    else
                        assignResult(instruction, "(" + operand(instruction.operands[0]) + " " +
                            binaryOperator(instruction.binaryOperator) + " " + operand(instruction.operands[1]) + ")");
                    break;
                case lowered::Opcode::RangeContains:
                    assignResult(instruction, "(" + operand(instruction.operands[1]) + " <= " + operand(instruction.operands[0]) +
                        " && " + operand(instruction.operands[0]) + (instruction.selector == "inclusive" ? " <= " : " < ") +
                        operand(instruction.operands[2]) + ")");
                    break;
                case lowered::Opcode::Convert:
                    assignResult(instruction, "static_cast<" + cppType(instruction.resultType) + ">(" + operand(instruction.operands[0]) + ")");
                    break;
                case lowered::Opcode::Call:
                case lowered::Opcode::ExtensionCall:
                case lowered::Opcode::MethodCall:
                    assignResult(instruction, functionName(instruction.callee) + "(" + callArguments(instruction, 0, true) + ")");
                    break;
                case lowered::Opcode::NativeInvoke:
                {
                    const auto found = functions_.find(instruction.callee.value());
                    assignResult(instruction, nativeCall(*found->second, instruction));
                    break;
                }
                case lowered::Opcode::FunctionReference:
                    assignResult(instruction, "&" + functionName(instruction.callee));
                    break;
                case lowered::Opcode::ClosureCreate:
                {
                    std::string captures = "[";
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        if (index) captures += ", ";
                        captures += "_c" + std::to_string(index) + " = " + operand(instruction.operands[index]);
                    }
                    captures += "]";
                    const Type& callable = module_.types.get(instruction.resultType);
                    std::string parameters;
                    std::string arguments;
                    for (std::size_t index = 0; index + 1 < callable.arguments.size(); ++index)
                    {
                        if (index) { parameters += ", "; arguments += ", "; }
                        parameters += cppType(callable.arguments[index]) + " _a" + std::to_string(index);
                        arguments += "_a" + std::to_string(index);
                    }
                    std::string capturedArguments;
                    const lowered::Function& body = *functions_.at(instruction.callee.value());
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        if (index) capturedArguments += ", ";
                        const TypeId parameterType = body.parameters[index].type;
                        const std::string capture = "_c" + std::to_string(index);
                        capturedArguments += module_.types.get(parameterType).kind == TypeKind::Reference
                            ? cppType(parameterType) + "::borrow(" + capture + ")" : capture;
                    }
                    if (!capturedArguments.empty() && !arguments.empty()) capturedArguments += ", ";
                    assignResult(instruction, captures + "(" + parameters + ") mutable { return " + functionName(instruction.callee) +
                        "(" + capturedArguments + arguments + "); }");
                    break;
                }
                case lowered::Opcode::IndirectCall:
                    assignResult(instruction, operand(instruction.operands[0]) + "(" + callArguments(instruction, 1) + ")");
                    break;
                case lowered::Opcode::VirtualCall:
                case lowered::Opcode::InterfaceCall:
                    assignResult(instruction, "wio::wir_backend::value_base(" + operand(instruction.operands.front()) + ")." +
                        dispatchName(instruction.targetType, instruction.projectionIndex) + "(" + callArguments(instruction, 1, true) + ")");
                    break;
                case lowered::Opcode::Upcast:
                case lowered::Opcode::CheckedCast:
                {
                    const bool borrowed = module_.types.get(instruction.resultType).kind == TypeKind::Reference;
                    const std::string pointer = "wio::wir_backend::require_object_cast<" + objectName(nominalType(instruction.targetType)) + ">(" + operand(instruction.operands[0]) + ")";
                    assignResult(instruction, cppType(instruction.resultType) + (borrowed ? "::raw(" : "(") + pointer + ")");
                    break;
                }
                case lowered::Opcode::TypeTest:
                    assignResult(instruction, "(wio::wir_backend::object_cast<" + objectName(nominalType(instruction.targetType)) +
                        ">(" + operand(instruction.operands[0]) + ") != nullptr)");
                    break;
                case lowered::Opcode::IdentityEqual:
                    assignResult(instruction, "(wio::wir_backend::object_identity(" + operand(instruction.operands[0]) +
                        (instruction.binaryOperator == typed::BinaryOperator::NotEqual ? ") != " : ") == ") +
                        "wio::wir_backend::object_identity(" + operand(instruction.operands[1]) + "))");
                    break;
                case lowered::Opcode::VariantTest:
                {
                    const std::string present = operand(instruction.operands.front()) + "->_f0";
                    const bool positive = instruction.selector == "Some" || instruction.selector == "Ok";
                    assignResult(instruction, positive ? present : "(!" + present + ")");
                    break;
                }
                case lowered::Opcode::VariantPayload:
                {
                    const Type& variant = module_.types.get(valueType(instruction.operands.front()));
                    const bool error = variant.nominalValueModel == NominalValueModel::Result &&
                        instruction.selector == "Err";
                    assignResult(instruction, operand(instruction.operands.front()) + (error ? "->_f2" : "->_f1"));
                    break;
                }
                case lowered::Opcode::ArrayLength:
                    assignResult(instruction, "static_cast<" + cppType(instruction.resultType) + ">(" + operand(instruction.operands[0]) + ".size())");
                    break;
                case lowered::Opcode::ArrayElement:
                case lowered::Opcode::ArrayGet:
                    assignResult(instruction, operand(instruction.operands[0]) +
                        (instruction.boundsCheck == lowered::BoundsCheckMode::Required ? ".at(" : "[") +
                        operand(instruction.operands[1]) +
                        (instruction.boundsCheck == lowered::BoundsCheckMode::Required ? ")" : "]"));
                    break;
                case lowered::Opcode::ArrayCreate:
                case lowered::Opcode::DefaultValue:
                    assignResult(instruction, cppType(instruction.resultType) + "{" + callArguments(instruction) + "}");
                    break;
                case lowered::Opcode::DictionaryCreate:
                {
                    std::string expression = "([&]() { " + cppType(instruction.resultType) + " _dictionary; ";
                    for (std::size_t index = 0; index + 1 < instruction.operands.size(); index += 2)
                        expression += "_dictionary.emplace(" + operand(instruction.operands[index]) + ", " +
                            operand(instruction.operands[index + 1]) + "); ";
                    expression += "return _dictionary; }())";
                    assignResult(instruction, expression);
                    break;
                }
                case lowered::Opcode::DictionaryGet:
                    assignResult(instruction, operand(instruction.operands[0]) + ".at(" + operand(instruction.operands[1]) + ")");
                    break;
                case lowered::Opcode::DictionaryPlace:
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" +
                        "wio::wir_backend::value_base(" + operand(instruction.operands[0]) + ").at(" +
                        operand(instruction.operands[1]) + "))");
                    break;
                case lowered::Opcode::Interpolate:
                {
                    std::string expression = cppString(instruction.stringSegments.empty() ? "" : instruction.stringSegments.front());
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        expression += " + wio::wir_backend::stringify(" + operand(instruction.operands[index]) + ")";
                        if (index + 1 < instruction.stringSegments.size()) expression += " + " + cppString(instruction.stringSegments[index + 1]);
                    }
                    const Type* resultType = module_.types.tryGet(instruction.resultType);
                    assignResult(instruction, resultType && resultType->kind == TypeKind::Text
                        ? "wio::runtime::Text::FromUtf8(" + expression + ")"
                        : expression);
                    break;
                }
                case lowered::Opcode::EnumConstant:
                {
                    const EnumCaseLayout* enumCase = findEnumCase(instruction.targetType, instruction.selector);
                    assignResult(instruction, enumCaseExpression(instruction.targetType, *enumCase));
                    break;
                }
                case lowered::Opcode::IntrinsicCall:
                    if (instruction.result) assignResult(instruction, intrinsicExpression(instruction));
                    else output_ << "                " << intrinsicExpression(instruction) << ";\n";
                    break;
                case lowered::Opcode::IteratorCreate:
                {
                    const bool range = instruction.selector.starts_with("range.");
                    std::string args;
                    for (const ValueId id : instruction.operands)
                    {
                        if (!args.empty()) args += ", ";
                        args += operand(id);
                    }
                    if (range) args += instruction.selector == "range.inclusive" ? ", true" : ", false";
                    assignResult(instruction, cppType(instruction.resultType) + (range ? "::range(" : "::container(") + args + ")");
                    break;
                }
                case lowered::Opcode::IteratorHasNext:
                    assignResult(instruction, operand(instruction.operands.front()) + ".hasNext()");
                    break;
                case lowered::Opcode::IteratorAdvance:
                    output_ << "                " << operand(instruction.operands.front()) << ".advance();\n";
                    break;
                case lowered::Opcode::IteratorValue:
                {
                    const Type& iterator = module_.types.get(valueType(instruction.operands.front()));
                    const Type& source = module_.types.get(iterator.arguments.front());
                    std::string expression = operand(instruction.operands.front()) +
                        (instruction.selector == "__index__" ? ".index()" : ".value()");
                    if (instruction.selector != "__value__" && instruction.selector != "__index__")
                    {
                        if (source.kind == TypeKind::Dictionary)
                            expression += "." + instruction.selector;
                        else
                        {
                            const Type& item = module_.types.get(source.arguments.front());
                            const auto field = std::ranges::find(item.fields, instruction.selector, &FieldLayout::name);
                            expression = "wio::wir_backend::value_base(" + expression + ")._f" +
                                std::to_string(std::distance(item.fields.begin(), field));
                        }
                    }
                    const Type& result = module_.types.get(instruction.resultType);
                    if (result.kind == TypeKind::Reference)
                        expression = cppType(instruction.resultType) + (result.isMutable ? "::borrow(" : "::borrowView(") + expression + ")";
                    assignResult(instruction, expression);
                    break;
                }
                case lowered::Opcode::AnyBox:
                {
                    const Type& boxed = module_.types.get(nominalType(instruction.targetType));
                    if (boxed.nominalKind == NominalKind::Object || boxed.nominalKind == NominalKind::Interface)
                        assignResult(instruction, "wio::runtime::Any::FromObject(" + cppType(nominalType(instruction.targetType)) + "(wio::wir_backend::object_ptr(" + operand(instruction.operands[0]) + ")))");
                    else if (boxed.kind == TypeKind::Opaque)
                        assignResult(instruction, "wio::runtime::Any::FromOpaque(" + operand(instruction.operands[0]) + ")");
                    else
                        assignResult(instruction, "wio::runtime::Any::Box(" + operand(instruction.operands[0]) + ")");
                    break;
                }
                case lowered::Opcode::AnyCheckedCast:
                {
                    const Type& target = module_.types.get(nominalType(instruction.targetType));
                    if (target.nominalKind == NominalKind::Object || target.nominalKind == NominalKind::Interface)
                    {
                        const std::string cast = operand(instruction.operands[0]) + ".CastObject<" + objectName(nominalType(instruction.targetType)) + ">()";
                        assignResult(instruction, module_.types.get(instruction.resultType).kind == TypeKind::Reference
                            ? cppType(instruction.resultType) + "::raw(" + cast + ".Get())" : cast);
                    }
                    else if (target.kind == TypeKind::Opaque)
                        assignResult(instruction, operand(instruction.operands[0]) + ".AsOpaque()");
                    else
                        assignResult(instruction, operand(instruction.operands[0]) + ".AsBoxed<" + cppType(instruction.targetType) + ">()");
                    break;
                }
                case lowered::Opcode::AnyTypeTest:
                {
                    const Type& target = module_.types.get(nominalType(instruction.targetType));
                    if (target.nominalKind == NominalKind::Object || target.nominalKind == NominalKind::Interface)
                        assignResult(instruction, operand(instruction.operands[0]) + ".CanCastObject<" + objectName(nominalType(instruction.targetType)) + ">()");
                    else if (target.kind == TypeKind::Opaque)
                        assignResult(instruction, operand(instruction.operands[0]) + ".IsOpaque()");
                    else
                        assignResult(instruction, operand(instruction.operands[0]) + ".IsBoxed<" + cppType(instruction.targetType) + ">()");
                    break;
                }
                case lowered::Opcode::NullableWrap:
                    assignResult(instruction, instruction.operands.empty() ? cppType(instruction.resultType) + "{}" :
                        cppType(instruction.resultType) + "{" + operand(instruction.operands[0]) + "}");
                    break;
                case lowered::Opcode::ResultIsError:
                    assignResult(instruction, "(!" + operand(instruction.operands.front()) + "->_f0)");
                    break;
                case lowered::Opcode::ResultValue:
                    assignResult(instruction, resultPayload(instruction, false));
                    break;
                case lowered::Opcode::ResultUnwrap:
                    assignResult(instruction, resultPayload(instruction, true));
                    break;
                case lowered::Opcode::ResultPropagate:
                    output_ << "                return " << cppType(instruction.targetType) << "::Create(" <<
                        operand(instruction.operands.front()) << "->_f2);\n";
                    break;
                case lowered::Opcode::GlobalPlace:
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" + globalName(instruction.global) + ")");
                    break;
                case lowered::Opcode::LocalPlace:
                    assignResult(instruction, cppType(instruction.resultType) + "::local()");
                    break;
                case lowered::Opcode::PlaceInit:
                case lowered::Opcode::Store:
                    output_ << "                " << operand(instruction.operands[0]) << ".write(" <<
                        movedOperand(instruction.operands[1]) << ");\n";
                    break;
                case lowered::Opcode::Load:
                    assignResult(instruction, objectBorrowedLoads_.contains(instruction.result.value())
                        ? "wio::wir_backend::ObjectBorrow<" + objectName(instruction.resultType) + ">{" + operand(instruction.operands[0]) + ".Get()}"
                        : borrowedLoads_.contains(instruction.result.value())
                        ? "std::ref(" + operand(instruction.operands[0]) + ".read())"
                        : operand(instruction.operands[0]) + ".read()");
                    break;
                case lowered::Opcode::Borrow:
                    assignResult(instruction, operand(instruction.operands[0]));
                    break;
                case lowered::Opcode::FieldPlace:
                {
                    const TypeId owner = instruction.targetType ? instruction.targetType : nominalType(valueType(instruction.operands[0]));
                    const Type& layout = module_.types.get(owner);
                    const bool object = layout.nominalKind == NominalKind::Object || layout.nominalKind == NominalKind::Interface;
                    const std::string base = "wio::wir_backend::value_base(" + operand(instruction.operands[0]) + ")";
                    const std::string field = layout.nominalRepresentation == NominalRepresentation::NativePod
                        ? instruction.selector : "_f" + std::to_string(instruction.projectionIndex);
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" +
                        (object ? "static_cast<" + objectName(owner) + "&>(" + base + ")" : base) + "." + field + ")");
                    break;
                }
                case lowered::Opcode::ArrayPlace:
                {
                    const std::string base = "wio::wir_backend::value_base(" + operand(instruction.operands[0]) + ")";
                    const std::string index = operand(instruction.operands[1]);
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" +
                        (instruction.boundsCheck == lowered::BoundsCheckMode::Required
                            ? "wio::intrinsics::Index(" + base + ", " + index + ")"
                            : base + "[" + index + "]") + ")");
                    break;
                }
                case lowered::Opcode::ConstructComponent:
                    if (instruction.callee)
                    {
                        const auto& constructor = *functions_.at(instruction.callee.value());
                        std::string expression = "([&]() { " + cppType(instruction.resultType) + " instance{}; " +
                            functionName(instruction.callee) + "(" + cppType(constructor.parameters.front().type) + "::borrow(instance)";
                        if (!instruction.operands.empty()) expression += ", " + callArguments(instruction, 0, true);
                        assignResult(instruction, expression + "); return instance; }())");
                    }
                    else assignResult(instruction, cppType(instruction.resultType) + "{" + callArguments(instruction, 0, true) + "}");
                    break;
                case lowered::Opcode::ConstructObject:
                    if (instruction.callee)
                    {
                        const auto& constructor = *functions_.at(instruction.callee.value());
                        std::string expression = "([&]() { auto instance = " + cppType(instruction.resultType) + "::Create(wio::wir_backend::SkipConstructor{}); " +
                            functionName(instruction.callee) + "(" + cppType(constructor.parameters.front().type) + "::borrow(instance)";
                        if (!instruction.operands.empty()) expression += ", " + callArguments(instruction, 0, true);
                        assignResult(instruction, expression + "); return instance; }())");
                    }
                    else assignResult(instruction, cppType(instruction.resultType) + "::Create(" + callArguments(instruction, 0, true) + ")");
                    break;
                case lowered::Opcode::Retain:
                case lowered::Opcode::CopyValue:
                    assignResult(instruction, operand(instruction.operands[0]));
                    break;
                case lowered::Opcode::MoveValue:
                    assignResult(instruction, module_.types.get(valueType(instruction.operands[0])).kind == TypeKind::Reference
                        ? operand(instruction.operands[0]) + ".take()" : movedOperand(instruction.operands[0]));
                    break;
                case lowered::Opcode::Replace:
                    output_ << "                " << operand(instruction.operands[0]) << ".write(" << movedOperand(instruction.operands[1]) << ");\n";
                    break;
                case lowered::Opcode::Release:
                case lowered::Opcode::DropValue:
                    output_ << "                " << valueName(instruction.operands[0]) << ".reset();\n";
                    break;
                case lowered::Opcode::ReleasePlace:
                case lowered::Opcode::DropPlace:
                    output_ << "                " << operand(instruction.operands[0]) << ".clear();\n";
                    break;
                case lowered::Opcode::Return:
                    if (instruction.operands.empty()) output_ << "                return;\n";
                    else output_ << "                return " << movedOperand(instruction.operands[0]) << ";\n";
                    break;
                case lowered::Opcode::CancellationCheck:
                    output_ << "                co_await wio::wir_backend::CancellationCheckpoint{};\n";
                    break;
                case lowered::Opcode::CoroutineSuspend:
                {
                    const auto& state = currentFunction_->coroutine->states.at(instruction.projectionIndex);
                    if (instruction.asyncOperation == AsyncOperation::SwitchExecutor)
                    {
                        const std::string awaiter = instruction.asyncExecutor == AsyncExecutorKind::Main
                            ? "wio::runtime::AsyncMainAwaiter{}"
                            : "wio::wir_backend::SwitchExecutor{wio::wir_backend::Executor::" +
                                std::string(instruction.asyncExecutor == AsyncExecutorKind::Blocking ? "Blocking" :
                                    instruction.asyncExecutor == AsyncExecutorKind::Io ? "Io" : "Worker") + "}";
                        output_ << "                co_await " << awaiter << ";\n";
                    }
                    else
                    {
                        output_ << "                ";
                        if (state.resumedValue) output_ << "_resume" << state.index << ".emplace(";
                        output_ << "co_await " << operand(instruction.operands.front());
                        if (state.resumedValue) output_ << ')';
                        output_ << ";\n";
                    }
                    output_ << "                co_await wio::wir_backend::CancellationCheckpoint{};\n";
                    emitBranchAssignments(instruction.targets.front());
                    break;
                }
                case lowered::Opcode::CoroutineResume:
                    assignResult(instruction, "std::move(*_resume" + std::to_string(instruction.projectionIndex) + ")");
                    output_ << "                _resume" << instruction.projectionIndex << ".reset();\n";
                    break;
                case lowered::Opcode::CoroutineComplete:
                    output_ << "                co_return";
                    if (!instruction.operands.empty()) output_ << ' ' << movedOperand(instruction.operands.front());
                    output_ << ";\n";
                    break;
                case lowered::Opcode::Jump:
                    emitBranchAssignments(instruction.targets.front());
                    break;
                case lowered::Opcode::CondJump:
                    output_ << "                if (" << operand(instruction.operands[0]) << ") {\n";
                    emitBranchAssignments(instruction.targets[0]);
                    output_ << "                } else {\n";
                    emitBranchAssignments(instruction.targets[1]);
                    output_ << "                }\n";
                    break;
                case lowered::Opcode::Unreachable:
                    output_ << "                throw std::logic_error(\"unreachable Wio block executed\");\n";
                    break;
                default: break;
                }
            }

            void emitFunctions()
            {
                for (const lowered::Function& function : module_.functions)
                {
                    if (functionIsOpen(function))
                        continue;
                    if (function.isExternal)
                    {
                        if (function.nativeBinding)
                        {
                            output_ << functionSignature(function, false) << " {\n    ";
                            if (!runtimeAsyncBinding(function) && std::ranges::any_of(function.parameters,
                                [&](const auto& p) { return module_.types.get(p.type).kind == TypeKind::Function; }))
                                output_ << "wio::wir_backend::NativeCallScope _nativeScope;\n    ";
                            // Wrapper parameters are ordinary variables rather than SSA
                            // optionals, so emit the ABI adaptation directly here.
                            std::string arguments;
                            for (std::size_t index = 0; index < function.parameters.size(); ++index)
                            {
                                if (index) arguments += ", ";
                                const NativePassingMode passing = index < function.nativeBinding->parameters.size()
                                    ? function.nativeBinding->parameters[index].passing : NativePassingMode::Value;
                                std::string argument = (passing == NativePassingMode::Borrow || passing == NativePassingMode::BorrowMut)
                                    ? std::string(passing == NativePassingMode::Borrow ? "std::as_const(" : "") + "_p" + std::to_string(index) + ".read()" + (passing == NativePassingMode::Borrow ? ")" : "")
                                    : passing == NativePassingMode::Consume
                                        ? "std::move(_p" + std::to_string(index) + ")"
                                        : "_p" + std::to_string(index);
                                if (module_.types.get(function.parameters[index].type).kind == TypeKind::Function && !runtimeAsyncBinding(function)) {
                                    const auto& binding = function.nativeBinding->parameters[index];
                                    argument = "_nativeScope.callback(" + argument + ", " +
                                        (binding.callbackLifetime == NativeCallbackLifetime::Retained ? "true" : "false") + ", " +
                                        (binding.callbackThread == NativeCallbackThread::Any ? "true" : "false") + ")";
                                }
                                arguments += argument;
                            }
                            std::string symbol = function.nativeBinding->symbol;
                            if (!function.nativeBinding->templateArguments.empty())
                            {
                                symbol += '<';
                                for (std::size_t i = 0; i < function.nativeBinding->templateArguments.size(); ++i)
                                {
                                    if (i) symbol += ", ";
                                    const auto id = function.nativeBinding->templateArguments[i];
                                    const auto& argument = module_.types.get(id);
                                    symbol += argument.kind == TypeKind::ConstValue ? argument.name : cppType(id);
                                }
                                symbol += '>';
                            }
                            std::string call = symbol + "(" + arguments + ")";
                            const auto& returnType = module_.types.get(function.returnType);
                            if (returnType.kind == TypeKind::Reference)
                                call = cppType(function.returnType) + (returnType.isMutable ? "::borrow(" : "::borrowView(") + call + ")";
                            if (function.nativeBinding->exceptionBoundary != NativeExceptionBoundary::None)
                                output_ << "try { ";
                            output_ << "return " << call << ';';
                            if (function.nativeBinding->exceptionBoundary != NativeExceptionBoundary::None)
                            {
                                output_ << " } catch (const std::exception& _error) { throw wio::runtime::RuntimeException("
                                    << cppString("native call '" + function.nativeBinding->symbol + "' failed: ")
                                    << " + std::string(_error.what())); } catch (...) { throw wio::runtime::RuntimeException("
                                    << cppString("native call '" + function.nativeBinding->symbol + "' failed with an unknown exception")
                                    << "); }";
                            }
                            output_ << "\n}\n\n";
                        }
                        continue;
                    }
                    currentFunction_ = &function;
                    collectValueTypes(function);
                    output_ << functionSignature(function, false) << " {\n";
                    if (function.coroutine && function.coroutine->retainedReceiver)
                    {
                        output_ << "    auto _selfGuard = " << cppType(function.ownerType) << "(_p0.Get());\n";
                        output_ << "    _p0 = " << cppType(function.parameters.front().type) << "::raw(_selfGuard.Get());\n";
                    }
                    if (function.coroutine)
                        for (const auto& state : function.coroutine->states)
                            if (state.resumedValue)
                                output_ << "    std::optional<" << cppType(state.resultType) << "> _resume" << state.index << ";\n";
                    std::vector<std::pair<std::uint32_t, TypeId>> values(valueTypes_.begin(), valueTypes_.end());
                    std::ranges::sort(values, {}, &std::pair<std::uint32_t, TypeId>::first);
                    for (const auto& [id, type] : values)
                        output_ << "    std::optional<" << (objectBorrowedLoads_.contains(id)
                            ? "wio::wir_backend::ObjectBorrow<" + objectName(type) + ">"
                            : (borrowedLoads_.contains(id) ? "std::reference_wrapper<" : "") + cppType(type) + (borrowedLoads_.contains(id) ? ">" : ""))
                            << "> _v" << id << ";\n";
                    for (std::size_t index = 0; index < function.parameters.size(); ++index)
                        output_ << "    " << valueName(function.parameters[index].id) << " = std::move(_p" << index << ");\n";
                    output_ << "    std::uint32_t _block = " << function.blocks.front().id.value() << ";\n"
                        "    for (;;) {\n        switch (_block) {\n";
                    for (const lowered::BasicBlock& block : function.blocks)
                    {
                        currentBlock_ = &block;
                        output_ << "        case " << block.id.value() << ": {\n";
                        for (const lowered::Instruction& instruction : block.instructions) emitInstruction(instruction);
                        output_ << "                throw std::logic_error(\"Wio block has no terminator\");\n"
                            "        }\n";
                    }
                    output_ << "        default: throw std::logic_error(\"invalid Wio block id\");\n"
                        "        }\n    }\n}\n\n";
                }
                currentFunction_ = nullptr;
                currentBlock_ = nullptr;
            }

            const lowered::Function* entryFunction() const
            {
                if (module_.contract.application && module_.contract.application->entry)
                {
                    const auto found = functions_.find(module_.contract.application->entry.value());
                    if (found != functions_.end()) return found->second;
                }
                const auto found = std::ranges::find_if(module_.functions, [](const lowered::Function& function)
                {
                    return function.name == "Entry" || function.name == "main";
                });
                return found == module_.functions.end() ? nullptr : &*found;
            }

            void emitMain()
            {
                if (!options_.emitMain || module_.contract.kind != ModuleKind::Program) return;
                if (module_.contract.application) {
                    output_ << "int main() { return _wio_app_host::run(); }\n";
                    return;
                }
                const lowered::Function* entry = entryFunction();
                if (!entry || !entry->parameters.empty()) return;
                const Type* resultType = module_.types.tryGet(entry->returnType);
                if (!resultType) return;
                output_ << "int main() {\n";
                if (resultType->kind == TypeKind::AsyncTask)
                {
                    output_ << "    wio::runtime::BindAsyncMainExecutor();\n    ";
                    if (module_.types.get(resultType->arguments.front()).kind == TypeKind::I32) output_ << "return ";
                    output_ << "wio::wir_backend::RunEntry(" << functionName(entry->id) << "());\n";
                    if (module_.types.get(resultType->arguments.front()).kind != TypeKind::I32) output_ << "    return 0;\n";
                }
                else if (resultType->kind == TypeKind::Void)
                    output_ << "    " << functionName(entry->id) << "();\n    return 0;\n";
                else if (resultType->kind == TypeKind::I32)
                    output_ << "    return " << functionName(entry->id) << "();\n";
                else
                    output_ << "    (void)" << functionName(entry->id) << "();\n    return 0;\n";
                output_ << "}\n";
            }
        };
    }

    bool WirCppGenerationResult::succeeded() const
    {
        return std::ranges::none_of(diagnostics_, [](const WirCppDiagnostic& diagnostic)
        {
            return diagnostic.severity == WirCppDiagnosticSeverity::Error;
        });
    }

    WirCppGenerationResult WirCppBackend::generate(
        const wir::lowered::Module& module,
        const WirCppBackendOptions& options) const
    {
        WirCppGenerationResult result;
        Emitter{module, result, options}.run();
        return result;
    }
}
