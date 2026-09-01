#pragma once

#include "wio/wir/id.h"
#include "wio/wir/source_span.h"
#include "wio/wir/type.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wio::wir::typed
{
    struct NullLiteral final
    {
        auto operator<=>(const NullLiteral&) const = default;
    };

    enum class UnaryOperator : std::uint8_t
    {
        Negate,
        LogicalNot,
        BitwiseNot
    };

    enum class BinaryOperator : std::uint8_t
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Remainder,
        Equal,
        NotEqual,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        BitwiseAnd,
        BitwiseOr,
        BitwiseXor,
        ShiftLeft,
        ShiftRight
    };

    enum class ConversionKind : std::uint8_t
    {
        NumericWiden,
        NumericFit
    };

    enum class Opcode : std::uint8_t
    {
        Constant,
        Unary,
        Binary,
        Convert,
        Call,
        MethodCall,
        VirtualCall,
        InterfaceCall,
        Upcast,
        CheckedCast,
        TypeTest,
        IdentityEqual,
        VariantTest,
        VariantPayload,
        ArrayLength,
        ArrayElement,
        ArrayCreate,
        ArrayGet,
        LocalPlace,
        PlaceInit,
        Load,
        Store,
        FieldPlace,
        ArrayPlace,
        Borrow,
        ConstructComponent,
        ConstructObject,
        Drop,
        // SSA value selection. All operands are already evaluated, so builders
        // may only use this for side-effect-free alternatives.
        Select,
        Return,
        Branch,
        CondBranch,
        Unreachable
    };

    using Literal = std::variant<
        std::monostate,
        NullLiteral,
        bool,
        std::int64_t,
        std::uint64_t,
        double,
        std::string>;

    struct Parameter
    {
        ValueId id;
        std::string name;
        TypeId type;
        SourceSpan source;
    };

    struct Instruction
    {
        Opcode opcode = Opcode::Unreachable;
        ValueId result;
        TypeId resultType;
        std::vector<ValueId> operands;
        std::vector<BlockId> targets;
        FunctionId callee;
        Literal literal;
        UnaryOperator unaryOperator = UnaryOperator::Negate;
        BinaryOperator binaryOperator = BinaryOperator::Add;
        ConversionKind conversionKind = ConversionKind::NumericWiden;
        std::string selector;
        std::uint32_t projectionIndex = 0;
        std::vector<TypeId> signatureTypes;
        TypeId targetType;
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
        TypeId ownerType;
        std::uint32_t methodSlot = 0;
        std::vector<BasicBlock> blocks;
        SourceSpan source;
        bool isAsync = false;
        bool isExternal = false;
        bool isMethod = false;
        bool isAbstract = false;
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
    [[nodiscard]] std::string_view unaryOperatorName(UnaryOperator op);
    [[nodiscard]] std::string_view binaryOperatorName(BinaryOperator op);
    [[nodiscard]] std::string_view conversionKindName(ConversionKind kind);
}
