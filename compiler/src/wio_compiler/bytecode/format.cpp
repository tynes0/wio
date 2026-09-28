#include "wio/bytecode/format.h"

namespace wio::bytecode
{
    std::string_view sectionKindName(const SectionKind kind) noexcept
    {
        switch (kind)
        {
        case SectionKind::Manifest: return "manifest";
        case SectionKind::Strings: return "strings";
        case SectionKind::Constants: return "constants";
        case SectionKind::Types: return "types";
        case SectionKind::Globals: return "globals";
        case SectionKind::Functions: return "functions";
        case SectionKind::Code: return "code";
        case SectionKind::ModuleContract: return "module-contract";
        case SectionKind::Debug: return "debug";
        }
        return "unknown";
    }

    std::string_view opcodeName(const Opcode opcode) noexcept
    {
        switch (opcode)
        {
#define WIO_BYTECODE_OPCODE_CASE(name) case Opcode::name: return #name
            WIO_BYTECODE_OPCODE_CASE(Constant);
            WIO_BYTECODE_OPCODE_CASE(GenericConstant);
            WIO_BYTECODE_OPCODE_CASE(DefaultValue);
            WIO_BYTECODE_OPCODE_CASE(Unary);
            WIO_BYTECODE_OPCODE_CASE(Binary);
            WIO_BYTECODE_OPCODE_CASE(RangeContains);
            WIO_BYTECODE_OPCODE_CASE(Convert);
            WIO_BYTECODE_OPCODE_CASE(Call);
            WIO_BYTECODE_OPCODE_CASE(NativeInvoke);
            WIO_BYTECODE_OPCODE_CASE(FunctionReference);
            WIO_BYTECODE_OPCODE_CASE(ClosureCreate);
            WIO_BYTECODE_OPCODE_CASE(IndirectCall);
            WIO_BYTECODE_OPCODE_CASE(ExtensionCall);
            WIO_BYTECODE_OPCODE_CASE(MethodCall);
            WIO_BYTECODE_OPCODE_CASE(VirtualCall);
            WIO_BYTECODE_OPCODE_CASE(InterfaceCall);
            WIO_BYTECODE_OPCODE_CASE(Upcast);
            WIO_BYTECODE_OPCODE_CASE(CheckedCast);
            WIO_BYTECODE_OPCODE_CASE(TypeTest);
            WIO_BYTECODE_OPCODE_CASE(IdentityEqual);
            WIO_BYTECODE_OPCODE_CASE(VariantTest);
            WIO_BYTECODE_OPCODE_CASE(VariantPayload);
            WIO_BYTECODE_OPCODE_CASE(ArrayLength);
            WIO_BYTECODE_OPCODE_CASE(ArrayElement);
            WIO_BYTECODE_OPCODE_CASE(ArrayCreate);
            WIO_BYTECODE_OPCODE_CASE(ArrayGet);
            WIO_BYTECODE_OPCODE_CASE(DictionaryCreate);
            WIO_BYTECODE_OPCODE_CASE(DictionaryGet);
            WIO_BYTECODE_OPCODE_CASE(DictionaryPlace);
            WIO_BYTECODE_OPCODE_CASE(Interpolate);
            WIO_BYTECODE_OPCODE_CASE(EnumConstant);
            WIO_BYTECODE_OPCODE_CASE(IntrinsicCall);
            WIO_BYTECODE_OPCODE_CASE(AnyBox);
            WIO_BYTECODE_OPCODE_CASE(AnyCheckedCast);
            WIO_BYTECODE_OPCODE_CASE(AnyTypeTest);
            WIO_BYTECODE_OPCODE_CASE(NullableWrap);
            WIO_BYTECODE_OPCODE_CASE(NullableUnwrap);
            WIO_BYTECODE_OPCODE_CASE(IteratorCreate);
            WIO_BYTECODE_OPCODE_CASE(IteratorHasNext);
            WIO_BYTECODE_OPCODE_CASE(IteratorValue);
            WIO_BYTECODE_OPCODE_CASE(IteratorAdvance);
            WIO_BYTECODE_OPCODE_CASE(ResultIsError);
            WIO_BYTECODE_OPCODE_CASE(ResultValue);
            WIO_BYTECODE_OPCODE_CASE(ResultUnwrap);
            WIO_BYTECODE_OPCODE_CASE(ResultPropagate);
            WIO_BYTECODE_OPCODE_CASE(CancellationCheck);
            WIO_BYTECODE_OPCODE_CASE(CoroutineSuspend);
            WIO_BYTECODE_OPCODE_CASE(CoroutineResume);
            WIO_BYTECODE_OPCODE_CASE(CoroutineComplete);
            WIO_BYTECODE_OPCODE_CASE(GlobalPlace);
            WIO_BYTECODE_OPCODE_CASE(LocalPlace);
            WIO_BYTECODE_OPCODE_CASE(PlaceInit);
            WIO_BYTECODE_OPCODE_CASE(Load);
            WIO_BYTECODE_OPCODE_CASE(Store);
            WIO_BYTECODE_OPCODE_CASE(FieldPlace);
            WIO_BYTECODE_OPCODE_CASE(ArrayPlace);
            WIO_BYTECODE_OPCODE_CASE(Borrow);
            WIO_BYTECODE_OPCODE_CASE(ConstructComponent);
            WIO_BYTECODE_OPCODE_CASE(ConstructObject);
            WIO_BYTECODE_OPCODE_CASE(Retain);
            WIO_BYTECODE_OPCODE_CASE(CopyValue);
            WIO_BYTECODE_OPCODE_CASE(MoveValue);
            WIO_BYTECODE_OPCODE_CASE(Replace);
            WIO_BYTECODE_OPCODE_CASE(Release);
            WIO_BYTECODE_OPCODE_CASE(DropValue);
            WIO_BYTECODE_OPCODE_CASE(ReleasePlace);
            WIO_BYTECODE_OPCODE_CASE(DropPlace);
            WIO_BYTECODE_OPCODE_CASE(Return);
            WIO_BYTECODE_OPCODE_CASE(Jump);
            WIO_BYTECODE_OPCODE_CASE(CondJump);
            WIO_BYTECODE_OPCODE_CASE(Unreachable);
#undef WIO_BYTECODE_OPCODE_CASE
        }
        return "unknown";
    }

    bool isKnownOpcode(const Opcode opcode) noexcept
    {
        return opcodeName(opcode) != "unknown";
    }
} // namespace wio::bytecode
