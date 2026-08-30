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
               opcode == Opcode::Binary || opcode == Opcode::Call;
    }

    std::string_view opcodeName(const Opcode opcode)
    {
        switch (opcode)
        {
        case Opcode::Constant: return "const";
        case Opcode::Unary: return "unary";
        case Opcode::Binary: return "binary";
        case Opcode::Call: return "call";
        case Opcode::Return: return "return";
        case Opcode::Jump: return "jump";
        case Opcode::CondJump: return "cond-jump";
        case Opcode::Unreachable: return "unreachable";
        }
        return "unknown";
    }
}
