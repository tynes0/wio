#include "wio/codegen/wir_cpp_backend.h"

#include "wio/wir/lowered_ir_verifier.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
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
                if (!result_.succeeded())
                    return;

                emitPreamble();
                emitNominalDeclarations();
                emitFunctionDeclarations();
                emitGlobals();
                emitFunctions();
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

            void diagnose(std::string code, std::string message, const SourceSpan& source = {},
                          const FunctionId function = {}, const BlockId block = {})
            {
                WirCppResultSink::addDiagnostic(result_, WirCppDiagnostic{
                    .code = std::move(code), .message = std::move(message), .source = source,
                    .function = function, .block = block
                });
            }

            void validateTypes()
            {
                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const Type& type = module_.types.types()[index];
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
                        if (type.nominalRepresentation == NominalRepresentation::NativePod &&
                            (!type.nativeBinding || type.nativeBinding->cppName.empty()))
                            diagnose("WCPP1105", "native POD type requires a C++ type binding");
                        if (type.nominalRepresentation == NominalRepresentation::Wio &&
                            (type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface) &&
                            !type.baseTypes.empty())
                            diagnose("WCPP1106", "object/interface inheritance layout is not implemented by this backend");
                        break;
                    default: break;
                    }
                }
            }

            bool supportedOpcode(const lowered::Opcode opcode) const
            {
                switch (opcode)
                {
                case lowered::Opcode::VariantTest:
                case lowered::Opcode::VariantPayload:
                case lowered::Opcode::EnumConstant:
                case lowered::Opcode::IntrinsicCall:
                case lowered::Opcode::IteratorCreate:
                case lowered::Opcode::IteratorHasNext:
                case lowered::Opcode::IteratorValue:
                case lowered::Opcode::IteratorAdvance:
                case lowered::Opcode::ResultIsError:
                case lowered::Opcode::ResultValue:
                case lowered::Opcode::ResultUnwrap:
                case lowered::Opcode::ResultPropagate:
                case lowered::Opcode::CancellationCheck:
                case lowered::Opcode::CoroutineSuspend:
                case lowered::Opcode::CoroutineResume:
                case lowered::Opcode::CoroutineComplete:
                case lowered::Opcode::VirtualCall:
                case lowered::Opcode::InterfaceCall:
                case lowered::Opcode::Upcast:
                case lowered::Opcode::CheckedCast:
                case lowered::Opcode::TypeTest:
                    return false;
                default:
                    return true;
                }
            }

            void validateInstructions()
            {
                for (const lowered::Function& function : module_.functions)
                {
                    const auto functionValueTypes = valueTypesFor(function);
                    if (function.isAsync || function.coroutine)
                        diagnose("WCPP1200", "coroutine state-machine emission is not enabled yet", function.source,
                            function.id);
                    for (const lowered::BasicBlock& block : function.blocks)
                    {
                        for (const lowered::Instruction& instruction : block.instructions)
                        {
                            if (!supportedOpcode(instruction.opcode))
                                diagnose("WCPP1201", "opcode is not implemented by the WIR C++ backend: " +
                                    std::string(lowered::opcodeName(instruction.opcode)), instruction.source,
                                    function.id, block.id);
                            if (instruction.opcode == lowered::Opcode::NativeInvoke)
                            {
                                const auto callee = functions_.find(instruction.callee.value());
                                if (callee == functions_.end() || !callee->second->nativeBinding ||
                                    callee->second->nativeBinding->requiresAdapter)
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
                                bool aggregateCompatible = target && target->fields.size() == instruction.operands.size();
                                for (std::size_t index = 0; aggregateCompatible && index < instruction.operands.size(); ++index)
                                {
                                    const auto value = functionValueTypes.find(instruction.operands[index].value());
                                    aggregateCompatible = value != functionValueTypes.end() &&
                                        value->second == target->fields[index].type;
                                }
                                if (!aggregateCompatible)
                                    diagnose("WCPP1203", "constructor requires a callable constructor adapter; only canonical field-wise construction is enabled",
                                        instruction.source, function.id, block.id);
                            }
                            if ((instruction.opcode == lowered::Opcode::AnyBox ||
                                 instruction.opcode == lowered::Opcode::AnyCheckedCast ||
                                 instruction.opcode == lowered::Opcode::AnyTypeTest) && instruction.targetType)
                            {
                                const Type* target = module_.types.tryGet(instruction.targetType);
                                if (target && target->kind == TypeKind::Named && target->nominalKind == NominalKind::Interface)
                                    diagnose("WCPP1204", "interface values in any require completed interface adapter layout",
                                        instruction.source, function.id, block.id);
                            }
                        }
                    }
                }
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
                        return type->nativeBinding->cppName;
                    if (type->nominalKind == NominalKind::Object || type->nominalKind == NominalKind::Interface)
                        return "wio::runtime::Ref<" + objectName(id) + ">";
                    if (type->nominalKind == NominalKind::Enum || type->nominalKind == NominalKind::Flagset)
                        return "std::int64_t";
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

            void emitPreamble()
            {
                output_ << "// Generated by Wio's canonical Lowered WIR C++ backend.\n"
                    "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <functional>\n"
                    "#include <map>\n#include <memory>\n#include <optional>\n#include <sstream>\n"
                    "#include <stdexcept>\n#include <string>\n#include <tuple>\n#include <type_traits>\n"
                    "#include <unordered_map>\n#include <utility>\n#include <vector>\n"
                    "#include <any.h>\n#include <intrinsics.h>\n#include <ref.h>\n#include <std_async.h>\n#include <text.h>\n";
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
template<class T> class Place {
public:
    static Place local() {
        Place place;
        place.owner_ = std::make_shared<std::optional<T>>();
        return place;
    }
    static Place borrow(T& value) { Place place; place.pointer_ = &value; return place; }
    T& read() {
        if (pointer_) return *pointer_;
        if (!owner_ || !owner_->has_value()) throw std::runtime_error("read from uninitialized Wio place");
        return owner_->value();
    }
    const T& read() const { return const_cast<Place*>(this)->read(); }
    void write(T value) {
        if (pointer_) *pointer_ = std::move(value);
        else { if (!owner_) owner_ = std::make_shared<std::optional<T>>(); *owner_ = std::move(value); }
    }
    void clear() { if (pointer_) *pointer_ = T{}; else if (owner_) owner_->reset(); }
private:
    std::shared_ptr<std::optional<T>> owner_;
    T* pointer_ = nullptr;
};
template<class T> T& value_base(T& value) { return value; }
template<class T> T& value_base(wio::runtime::Ref<T>& value) { return *value; }
template<class T> T& value_base(Place<T>& value) { return value_base(value.read()); }
template<class T, class U> wio::runtime::Ref<T> checked_ref_cast(const wio::runtime::Ref<U>& value) {
    if (auto* casted = dynamic_cast<T*>(value.Get())) return wio::runtime::Ref<T>(casted);
    return {};
}
template<class T> std::string stringify(const T& value) { std::ostringstream stream; stream << value; return stream.str(); }
inline std::string stringify(const bool value) { return value ? "true" : "false"; }
inline std::string stringify(const std::string& value) { return value; }
template<class T> struct Iterator { std::size_t index = 0; T* container = nullptr; };
}

)CPP";
            }

            void emitNominalDeclarations()
            {
                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (type.kind == TypeKind::Named && type.nominalRepresentation == NominalRepresentation::Wio &&
                        (type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface))
                        output_ << "struct " << objectName(id) << ";\n";
                }
                output_ << '\n';

                for (std::size_t index = 0; index < module_.types.size(); ++index)
                {
                    const TypeId id{static_cast<TypeId::ValueType>(index)};
                    const Type& type = module_.types.types()[index];
                    if (type.kind != TypeKind::Named || type.nominalRepresentation != NominalRepresentation::Wio ||
                        type.nominalKind == NominalKind::Enum || type.nominalKind == NominalKind::Flagset)
                        continue;
                    const bool object = type.nominalKind == NominalKind::Object || type.nominalKind == NominalKind::Interface;
                    output_ << "struct " << (object ? objectName(id) : typeName(id));
                    if (object) output_ << " : wio::runtime::RefCountedObject";
                    output_ << " {\n";
                    for (std::size_t fieldIndex = 0; fieldIndex < type.fields.size(); ++fieldIndex)
                        output_ << "    " << cppType(type.fields[fieldIndex].type) << " _f" << fieldIndex << "{}; // " <<
                            safeIdentifier(type.fields[fieldIndex].name) << "\n";
                    if (object && !type.fields.empty())
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
                    output_ << functionSignature(function, true) << '\n';
                output_ << '\n';
            }

            void collectValueTypes(const lowered::Function& function)
            {
                valueTypes_.clear();
                for (const lowered::Parameter& parameter : function.parameters)
                    valueTypes_[parameter.id.value()] = parameter.type;
                for (const lowered::BasicBlock& block : function.blocks)
                {
                    for (const lowered::Parameter& parameter : block.parameters)
                        valueTypes_[parameter.id.value()] = parameter.type;
                    for (const lowered::Instruction& instruction : block.instructions)
                        if (instruction.result) valueTypes_[instruction.result.value()] = instruction.resultType;
                }
            }

            std::string operand(const ValueId id) const { return "(*" + valueName(id) + ")"; }
            std::string movedOperand(const ValueId id) const { return "std::move(*" + valueName(id) + ")"; }

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

            std::string nativeArgument(const lowered::Function& callee, const std::size_t index,
                                       const ValueId operandId) const
            {
                if (callee.nativeBinding && index < callee.nativeBinding->parameters.size())
                {
                    const NativePassingMode passing = callee.nativeBinding->parameters[index].passing;
                    if (passing == NativePassingMode::Borrow || passing == NativePassingMode::BorrowMut)
                        return operand(operandId) + ".read()";
                    if (passing == NativePassingMode::Consume)
                        return movedOperand(operandId);
                }
                return operand(operandId);
            }

            std::string nativeCall(const lowered::Function& callee, const lowered::Instruction& instruction) const
            {
                std::string arguments;
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index) arguments += ", ";
                    arguments += nativeArgument(callee, index, instruction.operands[index]);
                }
                const std::string call = callee.nativeBinding->symbol + "(" + arguments + ")";
                if (callee.nativeBinding->exceptionBoundary == NativeExceptionBoundary::None)
                    return call;
                return "([&]() -> " + cppType(callee.returnType) + " { try { return " + call +
                    "; } catch (const std::exception& _error) { throw wio::runtime::RuntimeException(" +
                    cppString("native call '" + callee.nativeBinding->symbol + "' failed: ") +
                    " + std::string(_error.what())); } catch (...) { throw wio::runtime::RuntimeException(" +
                    cppString("native call '" + callee.nativeBinding->symbol + "' failed with an unknown exception") +
                    "); } }())";
            }

            std::string callArguments(const lowered::Instruction& instruction, const std::size_t begin = 0) const
            {
                std::string arguments;
                for (std::size_t index = begin; index < instruction.operands.size(); ++index)
                {
                    if (index != begin) arguments += ", ";
                    arguments += operand(instruction.operands[index]);
                }
                return arguments;
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
                    assignResult(instruction, functionName(instruction.callee) + "(" + callArguments(instruction) + ")");
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
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        if (index) capturedArguments += ", ";
                        capturedArguments += "_c" + std::to_string(index);
                    }
                    if (!capturedArguments.empty() && !arguments.empty()) capturedArguments += ", ";
                    assignResult(instruction, captures + "(" + parameters + ") { return " + functionName(instruction.callee) +
                        "(" + capturedArguments + arguments + "); }");
                    break;
                }
                case lowered::Opcode::IndirectCall:
                    assignResult(instruction, operand(instruction.operands[0]) + "(" + callArguments(instruction, 1) + ")");
                    break;
                case lowered::Opcode::VirtualCall:
                case lowered::Opcode::InterfaceCall:
                    assignResult(instruction, functionName(instruction.callee) + "(" + callArguments(instruction) + ")");
                    break;
                case lowered::Opcode::Upcast:
                    assignResult(instruction, operand(instruction.operands[0]));
                    break;
                case lowered::Opcode::CheckedCast:
                    assignResult(instruction, "wio::wir_backend::checked_ref_cast<" + objectName(instruction.targetType) + ">(" +
                        operand(instruction.operands[0]) + ")");
                    break;
                case lowered::Opcode::TypeTest:
                    assignResult(instruction, "static_cast<bool>(wio::wir_backend::checked_ref_cast<" + objectName(instruction.targetType) +
                        ">(" + operand(instruction.operands[0]) + "))");
                    break;
                case lowered::Opcode::IdentityEqual:
                    assignResult(instruction, "(" + operand(instruction.operands[0]) + ".Get() == " +
                        operand(instruction.operands[1]) + ".Get())");
                    break;
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
                case lowered::Opcode::AnyBox:
                {
                    const Type& boxed = module_.types.get(instruction.targetType);
                    if (boxed.kind == TypeKind::Named && boxed.nominalKind == NominalKind::Object)
                        assignResult(instruction, "wio::runtime::Any::FromObject(" + operand(instruction.operands[0]) + ")");
                    else if (boxed.kind == TypeKind::Opaque)
                        assignResult(instruction, "wio::runtime::Any::FromOpaque(" + operand(instruction.operands[0]) + ")");
                    else
                        assignResult(instruction, "wio::runtime::Any::Box(" + operand(instruction.operands[0]) + ")");
                    break;
                }
                case lowered::Opcode::AnyCheckedCast:
                {
                    const Type& target = module_.types.get(instruction.targetType);
                    if (target.kind == TypeKind::Named && target.nominalKind == NominalKind::Object)
                        assignResult(instruction, operand(instruction.operands[0]) + ".AsObject<" + objectName(instruction.targetType) + ">()");
                    else if (target.kind == TypeKind::Opaque)
                        assignResult(instruction, operand(instruction.operands[0]) + ".AsOpaque()");
                    else
                        assignResult(instruction, operand(instruction.operands[0]) + ".AsBoxed<" + cppType(instruction.targetType) + ">()");
                    break;
                }
                case lowered::Opcode::AnyTypeTest:
                {
                    const Type& target = module_.types.get(instruction.targetType);
                    if (target.kind == TypeKind::Named && target.nominalKind == NominalKind::Object)
                        assignResult(instruction, operand(instruction.operands[0]) + ".IsObject<" + objectName(instruction.targetType) + ">()");
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
                case lowered::Opcode::Borrow:
                    assignResult(instruction, operand(instruction.operands[0]) + ".read()");
                    break;
                case lowered::Opcode::FieldPlace:
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" +
                        "wio::wir_backend::value_base(" + operand(instruction.operands[0]) + ")._f" +
                        std::to_string(instruction.projectionIndex) + ")");
                    break;
                case lowered::Opcode::ArrayPlace:
                    assignResult(instruction, "wio::wir_backend::Place<" + placeValueType(instruction.resultType) + ">::borrow(" +
                        "wio::wir_backend::value_base(" + operand(instruction.operands[0]) + ")[" +
                        operand(instruction.operands[1]) + "])");
                    break;
                case lowered::Opcode::ConstructComponent:
                    assignResult(instruction, cppType(instruction.resultType) + "{" + callArguments(instruction) + "}");
                    break;
                case lowered::Opcode::ConstructObject:
                    assignResult(instruction, cppType(instruction.resultType) + "::Create(" + callArguments(instruction) + ")");
                    break;
                case lowered::Opcode::Retain:
                case lowered::Opcode::CopyValue:
                    assignResult(instruction, operand(instruction.operands[0]));
                    break;
                case lowered::Opcode::MoveValue:
                    assignResult(instruction, movedOperand(instruction.operands[0]));
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
                    if (function.isExternal)
                    {
                        if (function.nativeBinding)
                        {
                            output_ << functionSignature(function, false) << " {\n    ";
                            // Wrapper parameters are ordinary variables rather than SSA
                            // optionals, so emit the ABI adaptation directly here.
                            std::string arguments;
                            for (std::size_t index = 0; index < function.parameters.size(); ++index)
                            {
                                if (index) arguments += ", ";
                                const NativePassingMode passing = index < function.nativeBinding->parameters.size()
                                    ? function.nativeBinding->parameters[index].passing : NativePassingMode::Value;
                                arguments += (passing == NativePassingMode::Borrow || passing == NativePassingMode::BorrowMut)
                                    ? "_p" + std::to_string(index) + ".read()"
                                    : passing == NativePassingMode::Consume
                                        ? "std::move(_p" + std::to_string(index) + ")"
                                        : "_p" + std::to_string(index);
                            }
                            const std::string call = function.nativeBinding->symbol + "(" + arguments + ")";
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
                    std::vector<std::pair<std::uint32_t, TypeId>> values(valueTypes_.begin(), valueTypes_.end());
                    std::ranges::sort(values, {}, &std::pair<std::uint32_t, TypeId>::first);
                    for (const auto& [id, type] : values)
                        output_ << "    std::optional<" << cppType(type) << "> _v" << id << ";\n";
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
                const lowered::Function* entry = entryFunction();
                if (!entry || !entry->parameters.empty()) return;
                const Type* resultType = module_.types.tryGet(entry->returnType);
                if (!resultType) return;
                output_ << "int main() {\n";
                if (resultType->kind == TypeKind::Void)
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
