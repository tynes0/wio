#pragma once

#include "wio/wir/id.h"
#include "wio/wir/module.h"
#include "wio/wir/source_span.h"
#include "wio/wir/type.h"
#include "wio/wir/typed_ir.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wio::wir::lowered
{
    // Backend storage decision produced by canonical escape analysis. This is
    // an implementation contract, not a change to the language ownership
    // model: heap values still use their declared retain/release protocol.
    enum class StorageClass : std::uint8_t
    {
        Unspecified,
        Stack,
        Heap,
        CoroutineFrame
    };

    enum class EscapeClass : std::uint8_t
    {
        None,
        Local,
        Call,
        Store,
        Return,
        Coroutine
    };

    enum class BoundsCheckMode : std::uint8_t
    {
        NotApplicable,
        Required,
        EliminatedStatic,
        EliminatedProven
    };

    enum class Opcode : std::uint8_t
    {
        Constant,
        GenericConstant,
        DefaultValue,
        Unary,
        Binary,
        RangeContains,
        Convert,
        Call,
        NativeInvoke,
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
        IteratorCreate,
        IteratorHasNext,
        IteratorValue,
        IteratorAdvance,
        ResultIsError,
        ResultValue,
        ResultUnwrap,
        ResultPropagate,
        CancellationCheck,
        CoroutineSuspend,
        CoroutineResume,
        CoroutineComplete,
        GlobalPlace,
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
        GlobalId global;
        typed::Literal literal;
        typed::UnaryOperator unaryOperator = typed::UnaryOperator::Negate;
        typed::BinaryOperator binaryOperator = typed::BinaryOperator::Add;
        typed::ConversionKind conversionKind = typed::ConversionKind::NumericWiden;
        std::string selector;
        std::uint32_t projectionIndex = 0;
        std::vector<TypeId> signatureTypes;
        std::vector<TypeId> genericArguments;
        std::vector<CaptureKind> captureKinds;
        std::vector<bool> expandedOperands;
        std::vector<std::string> stringSegments;
        std::string specializationKey;
        IntrinsicFamily intrinsicFamily = IntrinsicFamily::None;
        AsyncOperation asyncOperation = AsyncOperation::None;
        AsyncExecutorKind asyncExecutor = AsyncExecutorKind::Inherit;
        TypeId targetType;
        typed::ValueOwnership resultOwnership = typed::ValueOwnership::Trivial;
        typed::BorrowLifetime borrowLifetime = typed::BorrowLifetime::None;
        ValueId borrowOrigin;
        StorageClass storageClass = StorageClass::Unspecified;
        EscapeClass escapeClass = EscapeClass::None;
        BoundsCheckMode boundsCheck = BoundsCheckMode::NotApplicable;
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
        FunctionId genericOrigin;
        std::vector<TypeId> specializationArguments;
        std::string specializationKey;
        std::vector<BasicBlock> blocks;
        SourceSpan source;
        bool isAsync = false;
        bool isExternal = false;
        bool isMethod = false;
        bool isAbstract = false;
        bool isExtension = false;
        bool isClosureBody = false;
        std::optional<NativeBinding> nativeBinding;
        std::optional<CoroutineLayout> coroutine;
    };

    struct Global
    {
        GlobalId id;
        std::string name;
        TypeId type;
        FunctionId initializer;
        SourceSpan source;
        bool isMutable = false;
        bool isConst = false;
    };

    struct Module
    {
        std::string name;
        ModuleContract contract;
        TypeTable types;
        std::vector<Global> globals;
        std::vector<Function> functions;
    };

    [[nodiscard]] bool isTerminator(Opcode opcode);
    [[nodiscard]] bool producesValue(Opcode opcode);
    [[nodiscard]] std::string_view opcodeName(Opcode opcode);
    [[nodiscard]] std::string_view storageClassName(StorageClass storageClass);
    [[nodiscard]] std::string_view escapeClassName(EscapeClass escapeClass);
    [[nodiscard]] std::string_view boundsCheckModeName(BoundsCheckMode mode);
}
