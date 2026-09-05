#include "wio/wir/lowered_ir.h"

namespace wio::wir::lowered
{
    bool isTerminator(const Opcode opcode)
    {
        return opcode == Opcode::Return || opcode == Opcode::Jump ||
               opcode == Opcode::CondJump || opcode == Opcode::CoroutineSuspend ||
               opcode == Opcode::CoroutineComplete || opcode == Opcode::ResultPropagate ||
               opcode == Opcode::Unreachable;
    }

    bool producesValue(const Opcode opcode)
    {
        return opcode == Opcode::Constant || opcode == Opcode::Unary ||
               opcode == Opcode::Binary || opcode == Opcode::RangeContains || opcode == Opcode::Convert || opcode == Opcode::Call ||
               opcode == Opcode::NativeInvoke ||
               opcode == Opcode::FunctionReference || opcode == Opcode::ClosureCreate ||
               opcode == Opcode::IndirectCall || opcode == Opcode::ExtensionCall ||
               opcode == Opcode::MethodCall || opcode == Opcode::VirtualCall ||
               opcode == Opcode::InterfaceCall || opcode == Opcode::Upcast ||
               opcode == Opcode::CheckedCast || opcode == Opcode::TypeTest ||
               opcode == Opcode::IdentityEqual ||
               opcode == Opcode::VariantTest || opcode == Opcode::VariantPayload ||
               opcode == Opcode::ArrayLength || opcode == Opcode::ArrayElement ||
               opcode == Opcode::ArrayCreate || opcode == Opcode::ArrayGet ||
               opcode == Opcode::DictionaryCreate || opcode == Opcode::DictionaryGet ||
               opcode == Opcode::DictionaryPlace || opcode == Opcode::Interpolate ||
               opcode == Opcode::EnumConstant || opcode == Opcode::IntrinsicCall ||
               opcode == Opcode::AnyBox || opcode == Opcode::AnyCheckedCast ||
               opcode == Opcode::AnyTypeTest || opcode == Opcode::NullableWrap ||
               opcode == Opcode::IteratorCreate || opcode == Opcode::IteratorHasNext ||
               opcode == Opcode::IteratorValue ||
               opcode == Opcode::ResultIsError || opcode == Opcode::ResultValue ||
               opcode == Opcode::ResultUnwrap ||
               opcode == Opcode::CoroutineResume ||
               opcode == Opcode::GlobalPlace || opcode == Opcode::LocalPlace || opcode == Opcode::Load ||
               opcode == Opcode::FieldPlace || opcode == Opcode::ArrayPlace ||
               opcode == Opcode::Borrow || opcode == Opcode::ConstructComponent ||
               opcode == Opcode::ConstructObject || opcode == Opcode::Retain ||
               opcode == Opcode::CopyValue || opcode == Opcode::MoveValue;
    }

    std::string_view opcodeName(const Opcode opcode)
    {
        switch (opcode)
        {
        case Opcode::Constant: return "const";
        case Opcode::Unary: return "unary";
        case Opcode::Binary: return "binary";
        case Opcode::RangeContains: return "range-contains";
        case Opcode::Convert: return "convert";
        case Opcode::Call: return "call";
        case Opcode::NativeInvoke: return "native-invoke";
        case Opcode::FunctionReference: return "function-ref";
        case Opcode::ClosureCreate: return "closure-create";
        case Opcode::IndirectCall: return "indirect-call";
        case Opcode::ExtensionCall: return "extension-call";
        case Opcode::MethodCall: return "method-call";
        case Opcode::VirtualCall: return "virtual-call";
        case Opcode::InterfaceCall: return "interface-call";
        case Opcode::Upcast: return "upcast";
        case Opcode::CheckedCast: return "checked-cast";
        case Opcode::TypeTest: return "type-test";
        case Opcode::IdentityEqual: return "identity-equal";
        case Opcode::VariantTest: return "variant-test";
        case Opcode::VariantPayload: return "variant-payload";
        case Opcode::ArrayLength: return "array-length";
        case Opcode::ArrayElement: return "array-element";
        case Opcode::ArrayCreate: return "array-create";
        case Opcode::ArrayGet: return "array-get";
        case Opcode::DictionaryCreate: return "dictionary-create";
        case Opcode::DictionaryGet: return "dictionary-get";
        case Opcode::DictionaryPlace: return "dictionary-place";
        case Opcode::Interpolate: return "interpolate";
        case Opcode::EnumConstant: return "enum-constant";
        case Opcode::IntrinsicCall: return "intrinsic-call";
        case Opcode::AnyBox: return "any-box";
        case Opcode::AnyCheckedCast: return "any-checked-cast";
        case Opcode::AnyTypeTest: return "any-type-test";
        case Opcode::NullableWrap: return "nullable-wrap";
        case Opcode::IteratorCreate: return "iterator-create";
        case Opcode::IteratorHasNext: return "iterator-has-next";
        case Opcode::IteratorValue: return "iterator-value";
        case Opcode::IteratorAdvance: return "iterator-advance";
        case Opcode::ResultIsError: return "result-is-error";
        case Opcode::ResultValue: return "result-value";
        case Opcode::ResultUnwrap: return "result-unwrap";
        case Opcode::ResultPropagate: return "result-propagate";
        case Opcode::CancellationCheck: return "cancellation-check";
        case Opcode::CoroutineSuspend: return "coroutine-suspend";
        case Opcode::CoroutineResume: return "coroutine-resume";
        case Opcode::CoroutineComplete: return "coroutine-complete";
        case Opcode::GlobalPlace: return "global-place";
        case Opcode::LocalPlace: return "local-place";
        case Opcode::PlaceInit: return "place-init";
        case Opcode::Load: return "load";
        case Opcode::Store: return "store";
        case Opcode::FieldPlace: return "field-place";
        case Opcode::ArrayPlace: return "array-place";
        case Opcode::Borrow: return "borrow";
        case Opcode::ConstructComponent: return "construct-component";
        case Opcode::ConstructObject: return "construct-object";
        case Opcode::Retain: return "retain";
        case Opcode::CopyValue: return "copy-value";
        case Opcode::MoveValue: return "move-value";
        case Opcode::Replace: return "replace";
        case Opcode::Release: return "release";
        case Opcode::DropValue: return "drop-value";
        case Opcode::ReleasePlace: return "release-place";
        case Opcode::DropPlace: return "drop-place";
        case Opcode::Return: return "return";
        case Opcode::Jump: return "jump";
        case Opcode::CondJump: return "cond-jump";
        case Opcode::Unreachable: return "unreachable";
        }
        return "unknown";
    }
}
