#include "wio/wir/lowered_ir.h"

namespace wio::wir::lowered
{
    bool isTerminator(const Opcode opcode)
    {
        return opcode == Opcode::Return || opcode == Opcode::Jump ||
               opcode == Opcode::CondJump || opcode == Opcode::Unreachable;
    }

    bool producesValue(const Opcode opcode)
    {
        return opcode == Opcode::Constant || opcode == Opcode::Unary ||
               opcode == Opcode::Binary || opcode == Opcode::Convert || opcode == Opcode::Call ||
               opcode == Opcode::VariantTest || opcode == Opcode::VariantPayload ||
               opcode == Opcode::ArrayLength || opcode == Opcode::ArrayElement ||
               opcode == Opcode::ArrayCreate || opcode == Opcode::ArrayGet;
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
        case Opcode::VariantTest: return "variant-test";
        case Opcode::VariantPayload: return "variant-payload";
        case Opcode::ArrayLength: return "array-length";
        case Opcode::ArrayElement: return "array-element";
        case Opcode::ArrayCreate: return "array-create";
        case Opcode::ArrayGet: return "array-get";
        case Opcode::Return: return "return";
        case Opcode::Jump: return "jump";
        case Opcode::CondJump: return "cond-jump";
        case Opcode::Unreachable: return "unreachable";
        }
        return "unknown";
    }
}
