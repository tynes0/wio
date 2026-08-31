#include "wio/wir/typed_ir_printer.h"

#include <iomanip>
#include <sstream>
#include <type_traits>

namespace wio::wir::typed
{
    namespace
    {
        std::string typeRef(const TypeId id)
        {
            return id ? "!t" + std::to_string(id.value()) : "!invalid";
        }

        std::string valueRef(const ValueId id)
        {
            return id ? "%v" + std::to_string(id.value()) : "%invalid";
        }

        std::string blockRef(const BlockId id)
        {
            return id ? "^b" + std::to_string(id.value()) : "^invalid";
        }

        std::string functionRef(const FunctionId id)
        {
            return id ? "@f" + std::to_string(id.value()) : "@invalid";
        }

        std::string printLiteral(const Literal& literal)
        {
            return std::visit([](const auto& value) -> std::string
            {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::monostate>)
                    return "<none>";
                else if constexpr (std::is_same_v<Value, NullLiteral>)
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
            return stream.str();
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
                stream << unaryOperatorName(instruction.unaryOperator) << " " << valueRef(instruction.operands.at(0));
                break;
            case Opcode::Binary:
                stream << binaryOperatorName(instruction.binaryOperator) << " "
                       << valueRef(instruction.operands.at(0)) << ", " << valueRef(instruction.operands.at(1));
                break;
            case Opcode::Convert:
                stream << conversionKindName(instruction.conversionKind) << " "
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
            case Opcode::Select:
                stream << "select " << valueRef(instruction.operands.at(0)) << ", "
                       << valueRef(instruction.operands.at(1)) << ", " << valueRef(instruction.operands.at(2));
                break;
            case Opcode::Return:
                stream << "return";
                if (!instruction.operands.empty())
                    stream << " " << valueRef(instruction.operands.front());
                break;
            case Opcode::Branch:
                stream << "branch " << blockRef(instruction.targets.at(0));
                if (!instruction.operands.empty())
                {
                    stream << "(";
                    for (std::size_t index = 0; index < instruction.operands.size(); ++index)
                    {
                        if (index > 0)
                            stream << ", ";
                        stream << valueRef(instruction.operands[index]);
                    }
                    stream << ")";
                }
                break;
            case Opcode::CondBranch:
                stream << "cond-branch " << valueRef(instruction.operands.at(0)) << ", "
                       << blockRef(instruction.targets.at(0)) << ", " << blockRef(instruction.targets.at(1));
                break;
            case Opcode::Unreachable:
                stream << "unreachable";
                break;
            }
            stream << '\n';
        }
    }

    std::string Printer::print(const Module& module) const
    {
        std::ostringstream stream;
        stream << "typed-wir module " << std::quoted(module.name) << " {\n";

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
            stream << "func " << functionRef(function.id) << " " << std::quoted(function.name) << "(";
            printParameters(stream, function.parameters);
            stream << ") -> " << typeRef(function.returnType);
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
