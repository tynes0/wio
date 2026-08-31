#include "wio/wir/typed_ir.h"

namespace wio::wir::typed
{
    bool isTerminator(const Opcode opcode)
    {
        return opcode == Opcode::Return || opcode == Opcode::Branch ||
               opcode == Opcode::CondBranch || opcode == Opcode::Unreachable;
    }

    bool producesValue(const Opcode opcode)
    {
        return opcode == Opcode::Constant || opcode == Opcode::Unary ||
               opcode == Opcode::Binary || opcode == Opcode::Convert ||
               opcode == Opcode::Call || opcode == Opcode::Select;
    }

    std::string_view opcodeName(const Opcode opcode)
    {
        switch (opcode)
        {
        case Opcode::Constant: return "const";
        case Opcode::Unary: return "unary";
        case Opcode::Binary: return "binary";
        case Opcode::Convert: return "convert";
        case Opcode::Call: return "call";
        case Opcode::Select: return "select";
        case Opcode::Return: return "return";
        case Opcode::Branch: return "branch";
        case Opcode::CondBranch: return "cond-branch";
        case Opcode::Unreachable: return "unreachable";
        }
        return "unknown";
    }

    std::string_view unaryOperatorName(const UnaryOperator op)
    {
        switch (op)
        {
        case UnaryOperator::Negate: return "neg";
        case UnaryOperator::LogicalNot: return "logical-not";
        case UnaryOperator::BitwiseNot: return "bitwise-not";
        }
        return "unknown";
    }

    std::string_view binaryOperatorName(const BinaryOperator op)
    {
        switch (op)
        {
        case BinaryOperator::Add: return "add";
        case BinaryOperator::Subtract: return "sub";
        case BinaryOperator::Multiply: return "mul";
        case BinaryOperator::Divide: return "div";
        case BinaryOperator::Remainder: return "rem";
        case BinaryOperator::Equal: return "eq";
        case BinaryOperator::NotEqual: return "ne";
        case BinaryOperator::Less: return "lt";
        case BinaryOperator::LessEqual: return "le";
        case BinaryOperator::Greater: return "gt";
        case BinaryOperator::GreaterEqual: return "ge";
        case BinaryOperator::BitwiseAnd: return "bitwise-and";
        case BinaryOperator::BitwiseOr: return "bitwise-or";
        case BinaryOperator::BitwiseXor: return "bitwise-xor";
        case BinaryOperator::ShiftLeft: return "shift-left";
        case BinaryOperator::ShiftRight: return "shift-right";
        }
        return "unknown";
    }

    std::string_view conversionKindName(const ConversionKind kind)
    {
        switch (kind)
        {
        case ConversionKind::NumericWiden: return "numeric-widen";
        case ConversionKind::NumericFit: return "numeric-fit";
        }
        return "unknown";
    }
}
