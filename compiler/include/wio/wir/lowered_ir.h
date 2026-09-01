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
        FunctionReference,
        ClosureCreate,
        IndirectCall,
        ExtensionCall,
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
        DictionaryCreate,
        DictionaryGet,
        DictionaryPlace,
        Interpolate,
        EnumConstant,
        IntrinsicCall,
        AnyBox,
        AnyCheckedCast,
        AnyTypeTest,
        NullableWrap,
        LocalPlace,
        PlaceInit,
        Load,
        Store,
        FieldPlace,
        ArrayPlace,
        Borrow,
        ConstructComponent,
        ConstructObject,
        Retain,
        CopyValue,
        MoveValue,
        Replace,
        Release,
        DropValue,
        ReleasePlace,
        DropPlace,
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
        typed::ValueOwnership ownership = typed::ValueOwnership::Trivial;
        typed::BorrowLifetime borrowLifetime = typed::BorrowLifetime::None;
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
        std::string selector;
        std::uint32_t projectionIndex = 0;
        std::vector<TypeId> signatureTypes;
        std::vector<TypeId> genericArguments;
        std::vector<CaptureKind> captureKinds;
        std::vector<std::string> stringSegments;
        std::string specializationKey;
        IntrinsicFamily intrinsicFamily = IntrinsicFamily::None;
        TypeId targetType;
        typed::ValueOwnership resultOwnership = typed::ValueOwnership::Trivial;
        typed::BorrowLifetime borrowLifetime = typed::BorrowLifetime::None;
        ValueId borrowOrigin;
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
        TypeId callableType;
        TypeId ownerType;
        std::uint32_t methodSlot = 0;
        std::uint32_t captureParameterCount = 0;
        std::vector<CaptureLayout> captures;
        std::vector<TypeId> genericParameters;
        std::vector<BasicBlock> blocks;
        SourceSpan source;
        bool isAsync = false;
        bool isExternal = false;
        bool isMethod = false;
        bool isAbstract = false;
        bool isExtension = false;
        bool isClosureBody = false;
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
