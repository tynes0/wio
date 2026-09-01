#include "wio/wir/lowered_ir_printer.h"

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace wio::wir::lowered
{
    namespace
    {
        std::string typeRef(const TypeId id) { return id ? "!t" + std::to_string(id.value()) : "!invalid"; }
        std::string valueRef(const ValueId id) { return id ? "%v" + std::to_string(id.value()) : "%invalid"; }
        std::string blockRef(const BlockId id) { return id ? "^b" + std::to_string(id.value()) : "^invalid"; }
        std::string functionRef(const FunctionId id) { return id ? "@f" + std::to_string(id.value()) : "@invalid"; }

        std::string printLiteral(const typed::Literal& literal)
        {
            return std::visit([](const auto& value) -> std::string
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::monostate>)
                    return "<none>";
                else if constexpr (std::is_same_v<Value, typed::NullLiteral>)
                    return "null";
                else if constexpr (std::is_same_v<Value, bool>)
                    return value ? "true" : "false";
                else if constexpr (std::is_same_v<Value, std::string>)
                {
                    std::ostringstream stream;
                    stream << std::quoted(value);
                    return stream.str();
                }
                else
                    return std::to_string(value);
            }, literal);
        }

        void printParameters(std::ostringstream& stream, const std::vector<Parameter>& parameters)
        {
            for (std::size_t index = 0; index < parameters.size(); ++index)
            {
                if (index > 0)
                    stream << ", ";
                const Parameter& parameter = parameters[index];
                stream << valueRef(parameter.id);
                if (!parameter.name.empty())
                    stream << " " << std::quoted(parameter.name);
                stream << ": " << typeRef(parameter.type);
            }
        }

        std::string printType(const Type& type)
        {
            std::ostringstream stream;
            stream << typeKindName(type.kind);
            if (!type.name.empty())
                stream << " " << std::quoted(type.name);
            if (!type.arguments.empty())
            {
                stream << "<";
                for (std::size_t index = 0; index < type.arguments.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(type.arguments[index]);
                }
                stream << ">";
            }
            if (type.isMutable)
                stream << " mutable";
            if (type.staticExtent.has_value())
                stream << " extent=" << *type.staticExtent;
            if (type.nominalKind != NominalKind::None)
                stream << " nominal=" << nominalKindName(type.nominalKind);
            if (type.nominalRepresentation != NominalRepresentation::Wio)
                stream << " representation=" << nominalRepresentationName(type.nominalRepresentation);
            if (!type.baseTypes.empty())
            {
                stream << " bases=[";
                for (std::size_t index = 0; index < type.baseTypes.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(type.baseTypes[index]);
                }
                stream << "]";
            }
            if (!type.fields.empty())
            {
                stream << " fields={";
                for (std::size_t index = 0; index < type.fields.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const FieldLayout& field = type.fields[index];
                    stream << std::quoted(field.name) << ":" << typeRef(field.type) << " "
                           << fieldVisibilityName(field.visibility);
                    if (field.isMutable)
                        stream << " mutable";
                }
                stream << "}";
            }
            if (!type.methods.empty())
            {
                stream << " methods={";
                for (std::size_t index = 0; index < type.methods.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const MethodLayout& method = type.methods[index];
                    stream << std::quoted(method.name) << "#" << method.slot << "=" << functionRef(method.function);
                    if (method.isAbstract)
                        stream << " abstract";
                }
                stream << "}";
            }
            if (type.hasConstructor)
                stream << " has-constructor";
            if (type.hasDestructor)
                stream << " has-destructor";
            return stream.str();
        }

        void printTarget(std::ostringstream& stream, const BranchTarget& target)
        {
            stream << blockRef(target.block);
            if (!target.arguments.empty())
            {
                stream << "(";
                for (std::size_t index = 0; index < target.arguments.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(target.arguments[index]);
                }
                stream << ")";
            }
        }

        void printInstruction(std::ostringstream& stream, const Instruction& instruction)
        {
            stream << "    ";
            if (instruction.result)
                stream << valueRef(instruction.result) << ": " << typeRef(instruction.resultType) << " = ";

            switch (instruction.opcode)
            {
            case Opcode::Constant:
                stream << "const " << printLiteral(instruction.literal);
                break;
            case Opcode::Unary:
                stream << typed::unaryOperatorName(instruction.unaryOperator) << " " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Binary:
                stream << typed::binaryOperatorName(instruction.binaryOperator) << " "
                       << valueRef(instruction.operands.at(0)) << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Convert:
                stream << typed::conversionKindName(instruction.conversionKind) << " "
                       << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Call:
                stream << "call " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::FunctionReference:
                stream << "function-ref " << functionRef(instruction.callee);
                break;
            case Opcode::ClosureCreate:
                stream << "closure-create " << functionRef(instruction.callee) << " captures(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << captureKindName(instruction.captureKinds.at(index)) << " "
                           << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::IndirectCall:
                stream << "indirect-call " << valueRef(instruction.operands.at(0)) << "(";
                for (std::size_t index = 1; index < instruction.operands.size(); ++index)
                {
                    if (index > 1)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::ExtensionCall:
                stream << "extension-call " << typeRef(instruction.targetType) << "::"
                       << std::quoted(instruction.selector) << " " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::MethodCall:
            case Opcode::VirtualCall:
            case Opcode::InterfaceCall:
                stream << opcodeName(instruction.opcode) << " " << typeRef(instruction.targetType)
                       << "::" << std::quoted(instruction.selector) << "#" << instruction.projectionIndex
                       << " " << functionRef(instruction.callee) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::Upcast:
            case Opcode::CheckedCast:
            case Opcode::TypeTest:
                stream << opcodeName(instruction.opcode) << " " << valueRef(instruction.operands.at(0))
                       << " to " << typeRef(instruction.targetType);
                break;
            case Opcode::IdentityEqual:
                stream << "identity-" << typed::binaryOperatorName(instruction.binaryOperator) << " "
                       << valueRef(instruction.operands.at(0)) << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::VariantTest:
                stream << "variant-test " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector);
                break;
            case Opcode::VariantPayload:
                stream << "variant-payload " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector) << ", " << instruction.projectionIndex;
                break;
            case Opcode::ArrayLength:
                stream << "array-length " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::ArrayElement:
                stream << "array-element " << valueRef(instruction.operands.at(0)) << ", "
                       << instruction.projectionIndex;
                break;
            case Opcode::ArrayCreate:
                stream << "array-create [";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << "]";
                break;
            case Opcode::ArrayGet:
                stream << "array-get " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::LocalPlace:
                stream << "local-place " << std::quoted(instruction.selector);
                break;
            case Opcode::PlaceInit:
                stream << "place-init " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Load:
                stream << "load " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Store:
                stream << "store " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::FieldPlace:
                stream << "field-place " << valueRef(instruction.operands.at(0)) << ", "
                       << std::quoted(instruction.selector);
                break;
            case Opcode::ArrayPlace:
                stream << "array-place " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Borrow:
                stream << "borrow " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::ConstructComponent:
            case Opcode::ConstructObject:
                stream << opcodeName(instruction.opcode) << " " << std::quoted(instruction.selector) << "(";
                for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << valueRef(instruction.operands[index]);
                }
                stream << ")";
                break;
            case Opcode::Drop:
                stream << "drop " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Return:
                stream << "return";
                if (!instruction.operands.empty())
                    stream << " " << valueRef(instruction.operands.front());
                break;
            case Opcode::Jump:
                stream << "jump ";
                printTarget(stream, instruction.targets.at(0));
                break;
            case Opcode::CondJump:
                stream << "cond-jump " << valueRef(instruction.operands.at(0)) << ", ";
                printTarget(stream, instruction.targets.at(0));
                stream << ", ";
                printTarget(stream, instruction.targets.at(1));
                break;
            case Opcode::Unreachable:
                stream << "unreachable";
                break;
            }
            if (!instruction.genericArguments.empty())
            {
                stream << " generic<";
                for (std::size_t index = 0; index < instruction.genericArguments.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    stream << typeRef(instruction.genericArguments[index]);
                }
                stream << ">";
            }
            if (!instruction.specializationKey.empty())
                stream << " specialization=" << std::quoted(instruction.specializationKey);
            stream << '\n';
        }
    }

    std::string Printer::print(const Module& module) const
    {
        std::ostringstream stream;
        stream << "lowered-wir module " << std::quoted(module.name) << " {\n";
        for (std::size_t index = 0; index < module.types.size(); ++index)
        {
            const TypeId id{static_cast<TypeId::ValueType>(index)};
            stream << "  " << typeRef(id) << " = " << printType(module.types.get(id)) << '\n';
        }

        for (const Function& function : module.functions)
        {
            stream << "\n  ";
            if (function.isExternal)
                stream << "external ";
            if (function.isAsync)
                stream << "async ";
            if (function.isAbstract)
                stream << "abstract ";
            if (function.isExtension)
                stream << "extension ";
            if (function.isClosureBody)
                stream << "closure-body ";
            stream << "func " << functionRef(function.id) << " " << std::quoted(function.name) << "(";
            printParameters(stream, function.parameters);
            stream << ") -> " << typeRef(function.returnType) << " callable=" << typeRef(function.callableType);
            if (function.isMethod)
                stream << " owner=" << typeRef(function.ownerType) << " slot=" << function.methodSlot;
            if (!function.captures.empty())
            {
                stream << " captures={";
                for (std::size_t index = 0; index < function.captures.size(); ++index)
                {
                    if (index > 0)
                        stream << ", ";
                    const CaptureLayout& capture = function.captures[index];
                    stream << std::quoted(capture.name) << ":" << typeRef(capture.type)
                           << " " << captureKindName(capture.kind);
                }
                stream << "}";
            }
            if (function.isExternal)
            {
                stream << '\n';
                continue;
            }
            stream << " {\n";

            for (const BasicBlock& block : function.blocks)
            {
                stream << "  " << blockRef(block.id);
                if (!block.name.empty())
                    stream << " " << std::quoted(block.name);
                if (!block.parameters.empty())
                {
                    stream << "(";
                    printParameters(stream, block.parameters);
                    stream << ")";
                }
                stream << ":\n";
                for (const Instruction& instruction : block.instructions)
                    printInstruction(stream, instruction);
            }
            stream << "  }\n";
        }
        stream << "}\n";
        return stream.str();
    }
}
