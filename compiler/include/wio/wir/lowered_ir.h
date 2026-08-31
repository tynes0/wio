#pragma once

#include "wio/wir/id.h"
#include "wio/wir/source_span.h"
#include "wio/wir/type.h"
#include "wio/wir/typed_ir.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::wir::lowered
{
    enum class Opcode : std::uint8_t
    {
        Constant,
        Unary,
        Binary,
        Convert,
        Call,
        Return,
        Jump,
        CondJump,
        Unreachable
    };

    struct Parameter
    {
        ValueId id;
        std::string name;
        TypeId type;
        SourceSpan source;
    };

    struct BranchTarget
    {
        BlockId block;
        std::vector<ValueId> arguments;
    };

    struct Instruction
    {
        Opcode opcode = Opcode::Unreachable;
        ValueId result;
        TypeId resultType;
        std::vector<ValueId> operands;
        std::vector<BranchTarget> targets;
        FunctionId callee;
        typed::Literal literal;
        typed::UnaryOperator unaryOperator = typed::UnaryOperator::Negate;
        typed::BinaryOperator binaryOperator = typed::BinaryOperator::Add;
        typed::ConversionKind conversionKind = typed::ConversionKind::NumericWiden;
        SourceSpan source;
    };

    struct BasicBlock
    {
        BlockId id;
        std::string name;
        std::vector<Parameter> parameters;
        std::vector<Instruction> instructions;
        SourceSpan source;
    };

    struct Function
    {
        FunctionId id;
        std::string name;
        std::vector<Parameter> parameters;
        TypeId returnType;
        std::vector<BasicBlock> blocks;
        SourceSpan source;
        bool isAsync = false;
        bool isExternal = false;
    };

    struct Module
    {
        std::string name;
        TypeTable types;
        std::vector<Function> functions;
    };

    [[nodiscard]] bool isTerminator(Opcode opcode);
    [[nodiscard]] bool producesValue(Opcode opcode);
    [[nodiscard]] std::string_view opcodeName(Opcode opcode);
}
