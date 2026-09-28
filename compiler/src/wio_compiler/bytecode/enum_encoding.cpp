#include "wio/bytecode/enum_encoding.h"

#include <exception>

namespace wio::bytecode
{
#define WIO_PIN_CASE(name, code)                                                                                       \
    case Enum::name:                                                                                                   \
        return code

    std::uint8_t encodeEnum(const wir::TypeKind value) noexcept
    {
        using Enum = wir::TypeKind;
        switch (value)
        {
            WIO_PIN_CASE(Invalid, 0);
            WIO_PIN_CASE(Void, 1);
            WIO_PIN_CASE(Bool, 2);
            WIO_PIN_CASE(I8, 3);
            WIO_PIN_CASE(I16, 4);
            WIO_PIN_CASE(I32, 5);
            WIO_PIN_CASE(I64, 6);
            WIO_PIN_CASE(ISize, 7);
            WIO_PIN_CASE(U8, 8);
            WIO_PIN_CASE(U16, 9);
            WIO_PIN_CASE(U32, 10);
            WIO_PIN_CASE(U64, 11);
            WIO_PIN_CASE(USize, 12);
            WIO_PIN_CASE(F32, 13);
            WIO_PIN_CASE(F64, 14);
            WIO_PIN_CASE(Byte, 15);
            WIO_PIN_CASE(Char, 16);
            WIO_PIN_CASE(String, 17);
            WIO_PIN_CASE(Text, 18);
            WIO_PIN_CASE(Any, 19);
            WIO_PIN_CASE(Opaque, 20);
            WIO_PIN_CASE(GenericParameter, 21);
            WIO_PIN_CASE(ConstGenericParameter, 22);
            WIO_PIN_CASE(ConstValue, 23);
            WIO_PIN_CASE(GenericParameterPack, 24);
            WIO_PIN_CASE(ValuePack, 25);
            WIO_PIN_CASE(TypePack, 26);
            WIO_PIN_CASE(PackStorage, 27);
            WIO_PIN_CASE(Named, 28);
            WIO_PIN_CASE(Reference, 29);
            WIO_PIN_CASE(Nullable, 30);
            WIO_PIN_CASE(Array, 31);
            WIO_PIN_CASE(Dictionary, 32);
            WIO_PIN_CASE(Function, 33);
            WIO_PIN_CASE(AsyncTask, 34);
            WIO_PIN_CASE(Iterator, 35);
        }
        std::terminate();
    }

#define WIO_DEFINE_SIMPLE_ENCODER(type, ns, cases)                                                                     \
    std::uint8_t encodeEnum(const ns::type value) noexcept                                                             \
    {                                                                                                                  \
        using Enum = ns::type;                                                                                         \
        switch (value)                                                                                                 \
        {                                                                                                              \
            cases                                                                                                      \
        }                                                                                                              \
        std::terminate();                                                                                              \
    }

#define WIO_NOMINAL_KIND_CASES                                                                                         \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(Component, 1);                                                                                        \
    WIO_PIN_CASE(Object, 2);                                                                                           \
    WIO_PIN_CASE(Interface, 3);                                                                                        \
    WIO_PIN_CASE(Enum, 4);                                                                                             \
    WIO_PIN_CASE(Flagset, 5);
    WIO_DEFINE_SIMPLE_ENCODER(NominalKind, wir, WIO_NOMINAL_KIND_CASES)
#define WIO_NOMINAL_REP_CASES                                                                                          \
    WIO_PIN_CASE(Wio, 0);                                                                                              \
    WIO_PIN_CASE(NativePod, 1);
    WIO_DEFINE_SIMPLE_ENCODER(NominalRepresentation, wir, WIO_NOMINAL_REP_CASES)
#define WIO_NOMINAL_VALUE_CASES                                                                                        \
    WIO_PIN_CASE(Regular, 0);                                                                                          \
    WIO_PIN_CASE(Tuple, 1);                                                                                            \
    WIO_PIN_CASE(Span, 2);                                                                                             \
    WIO_PIN_CASE(Option, 3);                                                                                           \
    WIO_PIN_CASE(Result, 4);
    WIO_DEFINE_SIMPLE_ENCODER(NominalValueModel, wir, WIO_NOMINAL_VALUE_CASES)
#define WIO_INTRINSIC_CASES                                                                                            \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(Array, 1);                                                                                            \
    WIO_PIN_CASE(Dictionary, 2);                                                                                       \
    WIO_PIN_CASE(String, 3);                                                                                           \
    WIO_PIN_CASE(Text, 4);                                                                                             \
    WIO_PIN_CASE(Enum, 5);                                                                                             \
    WIO_PIN_CASE(Flagset, 6);                                                                                          \
    WIO_PIN_CASE(Nullable, 7);                                                                                         \
    WIO_PIN_CASE(Any, 8);                                                                                              \
    WIO_PIN_CASE(Option, 9);                                                                                           \
    WIO_PIN_CASE(Result, 10);                                                                                          \
    WIO_PIN_CASE(Tuple, 11);                                                                                           \
    WIO_PIN_CASE(Span, 12);                                                                                            \
    WIO_PIN_CASE(Pack, 13);                                                                                            \
    WIO_PIN_CASE(AsyncTask, 14);
    WIO_DEFINE_SIMPLE_ENCODER(IntrinsicFamily, wir, WIO_INTRINSIC_CASES)
#define WIO_VISIBILITY_CASES                                                                                           \
    WIO_PIN_CASE(Private, 0);                                                                                          \
    WIO_PIN_CASE(Protected, 1);                                                                                        \
    WIO_PIN_CASE(Public, 2);
    WIO_DEFINE_SIMPLE_ENCODER(FieldVisibility, wir, WIO_VISIBILITY_CASES)
#define WIO_CAPTURE_CASES                                                                                              \
    WIO_PIN_CASE(Value, 0);                                                                                            \
    WIO_PIN_CASE(Reference, 1);                                                                                        \
    WIO_PIN_CASE(RetainedSelf, 2);
    WIO_DEFINE_SIMPLE_ENCODER(CaptureKind, wir, WIO_CAPTURE_CASES)
#define WIO_EXECUTOR_CASES                                                                                             \
    WIO_PIN_CASE(Inherit, 0);                                                                                          \
    WIO_PIN_CASE(Main, 1);                                                                                             \
    WIO_PIN_CASE(Worker, 2);                                                                                           \
    WIO_PIN_CASE(Blocking, 3);                                                                                         \
    WIO_PIN_CASE(Io, 4);
    WIO_DEFINE_SIMPLE_ENCODER(AsyncExecutorKind, wir, WIO_EXECUTOR_CASES)
#define WIO_ASYNC_OPERATION_CASES                                                                                      \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(AwaitTask, 1);                                                                                        \
    WIO_PIN_CASE(SwitchExecutor, 2);                                                                                   \
    WIO_PIN_CASE(Start, 3);                                                                                            \
    WIO_PIN_CASE(Spawn, 4);                                                                                            \
    WIO_PIN_CASE(SpawnWorker, 5);                                                                                      \
    WIO_PIN_CASE(SpawnBlocking, 6);                                                                                    \
    WIO_PIN_CASE(SpawnIo, 7);                                                                                          \
    WIO_PIN_CASE(Join, 8);                                                                                             \
    WIO_PIN_CASE(Cancel, 9);                                                                                           \
    WIO_PIN_CASE(CancelAfter, 10);                                                                                     \
    WIO_PIN_CASE(Detach, 11);                                                                                          \
    WIO_PIN_CASE(Yield, 12);                                                                                           \
    WIO_PIN_CASE(Sleep, 13);                                                                                           \
    WIO_PIN_CASE(Wait, 14);
    WIO_DEFINE_SIMPLE_ENCODER(AsyncOperation, wir, WIO_ASYNC_OPERATION_CASES)
#define WIO_FRAME_SLOT_CASES                                                                                           \
    WIO_PIN_CASE(Parameter, 0);                                                                                        \
    WIO_PIN_CASE(Local, 1);                                                                                            \
    WIO_PIN_CASE(AwaitedTask, 2);                                                                                      \
    WIO_PIN_CASE(Temporary, 3);
    WIO_DEFINE_SIMPLE_ENCODER(CoroutineFrameSlotKind, wir, WIO_FRAME_SLOT_CASES)
#define WIO_OWNERSHIP_CASES                                                                                            \
    WIO_PIN_CASE(Trivial, 0);                                                                                          \
    WIO_PIN_CASE(OwnedValue, 1);                                                                                       \
    WIO_PIN_CASE(ReferenceCounted, 2);                                                                                 \
    WIO_PIN_CASE(Borrowed, 3);                                                                                         \
    WIO_PIN_CASE(Generic, 4);
    WIO_DEFINE_SIMPLE_ENCODER(OwnershipModel, wir, WIO_OWNERSHIP_CASES)
#define WIO_CLEANUP_CASES                                                                                              \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(DestroyValue, 1);                                                                                     \
    WIO_PIN_CASE(ReleaseReference, 2);
    WIO_DEFINE_SIMPLE_ENCODER(CleanupKind, wir, WIO_CLEANUP_CASES)
#define WIO_NATIVE_LANGUAGE_CASES                                                                                      \
    WIO_PIN_CASE(Cpp, 0);                                                                                              \
    WIO_PIN_CASE(C, 1);
    WIO_DEFINE_SIMPLE_ENCODER(NativeSymbolLanguage, wir, WIO_NATIVE_LANGUAGE_CASES)
#define WIO_CALLING_CASES                                                                                              \
    WIO_PIN_CASE(PlatformDefault, 0);                                                                                  \
    WIO_PIN_CASE(Cdecl, 1);                                                                                            \
    WIO_PIN_CASE(StdCall, 2);                                                                                          \
    WIO_PIN_CASE(FastCall, 3);
    WIO_DEFINE_SIMPLE_ENCODER(NativeCallingConvention, wir, WIO_CALLING_CASES)
#define WIO_EXCEPTION_CASES                                                                                            \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(TranslateToWioFailure, 1);
    WIO_DEFINE_SIMPLE_ENCODER(NativeExceptionBoundary, wir, WIO_EXCEPTION_CASES)
#define WIO_PASSING_CASES                                                                                              \
    WIO_PIN_CASE(Value, 0);                                                                                            \
    WIO_PIN_CASE(Borrow, 1);                                                                                           \
    WIO_PIN_CASE(BorrowMut, 2);                                                                                        \
    WIO_PIN_CASE(Consume, 3);                                                                                          \
    WIO_PIN_CASE(ReturnOwned, 4);
    WIO_DEFINE_SIMPLE_ENCODER(NativePassingMode, wir, WIO_PASSING_CASES)
#define WIO_MARSHALLING_CASES                                                                                          \
    WIO_PIN_CASE(Void, 0);                                                                                             \
    WIO_PIN_CASE(Scalar, 1);                                                                                           \
    WIO_PIN_CASE(Utf8String, 2);                                                                                       \
    WIO_PIN_CASE(UnicodeText, 3);                                                                                      \
    WIO_PIN_CASE(NativePod, 4);                                                                                        \
    WIO_PIN_CASE(OpaqueHandle, 5);                                                                                     \
    WIO_PIN_CASE(ObjectHandle, 6);                                                                                     \
    WIO_PIN_CASE(Callback, 7);                                                                                         \
    WIO_PIN_CASE(RuntimeValue, 8);                                                                                     \
    WIO_PIN_CASE(Generic, 9);
    WIO_DEFINE_SIMPLE_ENCODER(NativeMarshallingKind, wir, WIO_MARSHALLING_CASES)
#define WIO_CALLBACK_LIFETIME_CASES                                                                                    \
    WIO_PIN_CASE(Call, 0);                                                                                             \
    WIO_PIN_CASE(Retained, 1);
    WIO_DEFINE_SIMPLE_ENCODER(NativeCallbackLifetime, wir, WIO_CALLBACK_LIFETIME_CASES)
#define WIO_CALLBACK_THREAD_CASES                                                                                      \
    WIO_PIN_CASE(Caller, 0);                                                                                           \
    WIO_PIN_CASE(Any, 1);
    WIO_DEFINE_SIMPLE_ENCODER(NativeCallbackThread, wir, WIO_CALLBACK_THREAD_CASES)
#define WIO_THUNK_CASES                                                                                                \
    WIO_PIN_CASE(Direct, 0);                                                                                           \
    WIO_PIN_CASE(Adapter, 1);                                                                                          \
    WIO_PIN_CASE(TemplateSpecialization, 2);
    WIO_DEFINE_SIMPLE_ENCODER(NativeThunkKind, wir, WIO_THUNK_CASES)
#define WIO_RECEIVER_CASES                                                                                             \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(ConstReference, 1);                                                                                   \
    WIO_PIN_CASE(MutableReference, 2);
    WIO_DEFINE_SIMPLE_ENCODER(NativeReceiverKind, wir, WIO_RECEIVER_CASES)
#define WIO_MODULE_KIND_CASES                                                                                          \
    WIO_PIN_CASE(Program, 0);                                                                                          \
    WIO_PIN_CASE(WioLibrary, 1);                                                                                       \
    WIO_PIN_CASE(NativeLibrary, 2);
    WIO_DEFINE_SIMPLE_ENCODER(ModuleKind, wir, WIO_MODULE_KIND_CASES)
#define WIO_IMPORT_KIND_CASES                                                                                          \
    WIO_PIN_CASE(WioModule, 0);                                                                                        \
    WIO_PIN_CASE(StandardModule, 1);                                                                                   \
    WIO_PIN_CASE(NativeHeader, 2);                                                                                     \
    WIO_PIN_CASE(NativeLibrary, 3);
    WIO_DEFINE_SIMPLE_ENCODER(ModuleImportKind, wir, WIO_IMPORT_KIND_CASES)
#define WIO_EXPORT_KIND_CASES                                                                                          \
    WIO_PIN_CASE(Function, 0);                                                                                         \
    WIO_PIN_CASE(GenericFunctionSpecialization, 1);                                                                    \
    WIO_PIN_CASE(ObjectType, 2);                                                                                       \
    WIO_PIN_CASE(ComponentType, 3);
    WIO_DEFINE_SIMPLE_ENCODER(ModuleExportKind, wir, WIO_EXPORT_KIND_CASES)
#define WIO_EXPORT_ROLE_CASES                                                                                          \
    WIO_PIN_CASE(Ordinary, 0);                                                                                         \
    WIO_PIN_CASE(Command, 1);                                                                                          \
    WIO_PIN_CASE(EventHook, 2);
    WIO_DEFINE_SIMPLE_ENCODER(ModuleExportRole, wir, WIO_EXPORT_ROLE_CASES)
#define WIO_TARGET_KIND_CASES                                                                                          \
    WIO_PIN_CASE(Module, 0);                                                                                           \
    WIO_PIN_CASE(Function, 1);                                                                                         \
    WIO_PIN_CASE(Type, 2);                                                                                             \
    WIO_PIN_CASE(Field, 3);                                                                                            \
    WIO_PIN_CASE(Method, 4);                                                                                           \
    WIO_PIN_CASE(Parameter, 5);                                                                                        \
    WIO_PIN_CASE(EnumCase, 6);                                                                                         \
    WIO_PIN_CASE(Application, 7);                                                                                      \
    WIO_PIN_CASE(System, 8);                                                                                           \
    WIO_PIN_CASE(Handler, 9);
    WIO_DEFINE_SIMPLE_ENCODER(MetadataTargetKind, wir, WIO_TARGET_KIND_CASES)
#define WIO_ORIGIN_CASES                                                                                               \
    WIO_PIN_CASE(Direct, 0);                                                                                           \
    WIO_PIN_CASE(Inherited, 1);                                                                                        \
    WIO_PIN_CASE(Scoped, 2);                                                                                           \
    WIO_PIN_CASE(Composed, 3);                                                                                         \
    WIO_PIN_CASE(Generated, 4);                                                                                        \
    WIO_PIN_CASE(Compiler, 5);
    WIO_DEFINE_SIMPLE_ENCODER(AttributeOriginKind, wir, WIO_ORIGIN_CASES)
#define WIO_PHASE_CASES                                                                                                \
    WIO_PIN_CASE(Validation, 0);                                                                                       \
    WIO_PIN_CASE(Derive, 1);                                                                                           \
    WIO_PIN_CASE(Pre, 2);                                                                                              \
    WIO_PIN_CASE(Post, 3);                                                                                             \
    WIO_PIN_CASE(Finally, 4);                                                                                          \
    WIO_PIN_CASE(Around, 5);                                                                                           \
    WIO_PIN_CASE(Unknown, 6);
    WIO_DEFINE_SIMPLE_ENCODER(AttributeProcessorPhase, wir, WIO_PHASE_CASES)
#define WIO_STAGE_KIND_CASES                                                                                           \
    WIO_PIN_CASE(Variable, 0);                                                                                         \
    WIO_PIN_CASE(Fixed, 1);
    WIO_DEFINE_SIMPLE_ENCODER(ApplicationStageKind, wir, WIO_STAGE_KIND_CASES)
#define WIO_AFFINITY_CASES                                                                                             \
    WIO_PIN_CASE(Inherit, 0);                                                                                          \
    WIO_PIN_CASE(Main, 1);                                                                                             \
    WIO_PIN_CASE(Worker, 2);
    WIO_DEFINE_SIMPLE_ENCODER(ApplicationAffinity, wir, WIO_AFFINITY_CASES)
#define WIO_RESOURCE_CASES                                                                                             \
    WIO_PIN_CASE(Read, 0);                                                                                             \
    WIO_PIN_CASE(Write, 1);
    WIO_DEFINE_SIMPLE_ENCODER(ResourceAccess, wir, WIO_RESOURCE_CASES)

#define WIO_UNARY_CASES                                                                                                \
    WIO_PIN_CASE(Negate, 0);                                                                                           \
    WIO_PIN_CASE(LogicalNot, 1);                                                                                       \
    WIO_PIN_CASE(BitwiseNot, 2);
    WIO_DEFINE_SIMPLE_ENCODER(UnaryOperator, wir::typed, WIO_UNARY_CASES)
#define WIO_BINARY_CASES                                                                                               \
    WIO_PIN_CASE(Add, 0);                                                                                              \
    WIO_PIN_CASE(Subtract, 1);                                                                                         \
    WIO_PIN_CASE(Multiply, 2);                                                                                         \
    WIO_PIN_CASE(Divide, 3);                                                                                           \
    WIO_PIN_CASE(Remainder, 4);                                                                                        \
    WIO_PIN_CASE(Equal, 5);                                                                                            \
    WIO_PIN_CASE(NotEqual, 6);                                                                                         \
    WIO_PIN_CASE(Less, 7);                                                                                             \
    WIO_PIN_CASE(LessEqual, 8);                                                                                        \
    WIO_PIN_CASE(Greater, 9);                                                                                          \
    WIO_PIN_CASE(GreaterEqual, 10);                                                                                    \
    WIO_PIN_CASE(BitwiseAnd, 11);                                                                                      \
    WIO_PIN_CASE(BitwiseOr, 12);                                                                                       \
    WIO_PIN_CASE(BitwiseXor, 13);                                                                                      \
    WIO_PIN_CASE(ShiftLeft, 14);                                                                                       \
    WIO_PIN_CASE(ShiftRight, 15);
    WIO_DEFINE_SIMPLE_ENCODER(BinaryOperator, wir::typed, WIO_BINARY_CASES)
#define WIO_CONVERSION_CASES                                                                                           \
    WIO_PIN_CASE(NumericWiden, 0);                                                                                     \
    WIO_PIN_CASE(NumericFit, 1);
    WIO_DEFINE_SIMPLE_ENCODER(ConversionKind, wir::typed, WIO_CONVERSION_CASES)
#define WIO_VALUE_OWNERSHIP_CASES                                                                                      \
    WIO_PIN_CASE(Trivial, 0);                                                                                          \
    WIO_PIN_CASE(Borrowed, 1);                                                                                         \
    WIO_PIN_CASE(Owned, 2);
    WIO_DEFINE_SIMPLE_ENCODER(ValueOwnership, wir::typed, WIO_VALUE_OWNERSHIP_CASES)
#define WIO_BORROW_LIFETIME_CASES                                                                                      \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(Caller, 1);                                                                                           \
    WIO_PIN_CASE(Lexical, 2);
    WIO_DEFINE_SIMPLE_ENCODER(BorrowLifetime, wir::typed, WIO_BORROW_LIFETIME_CASES)
#define WIO_STORAGE_CASES                                                                                              \
    WIO_PIN_CASE(Unspecified, 0);                                                                                      \
    WIO_PIN_CASE(Stack, 1);                                                                                            \
    WIO_PIN_CASE(Heap, 2);                                                                                             \
    WIO_PIN_CASE(CoroutineFrame, 3);
    WIO_DEFINE_SIMPLE_ENCODER(StorageClass, wir::lowered, WIO_STORAGE_CASES)
#define WIO_ESCAPE_CASES                                                                                               \
    WIO_PIN_CASE(None, 0);                                                                                             \
    WIO_PIN_CASE(Local, 1);                                                                                            \
    WIO_PIN_CASE(Call, 2);                                                                                             \
    WIO_PIN_CASE(Store, 3);                                                                                            \
    WIO_PIN_CASE(Return, 4);                                                                                           \
    WIO_PIN_CASE(Coroutine, 5);
    WIO_DEFINE_SIMPLE_ENCODER(EscapeClass, wir::lowered, WIO_ESCAPE_CASES)
#define WIO_BOUNDS_CASES                                                                                               \
    WIO_PIN_CASE(NotApplicable, 0);                                                                                    \
    WIO_PIN_CASE(Required, 1);                                                                                         \
    WIO_PIN_CASE(EliminatedStatic, 2);                                                                                 \
    WIO_PIN_CASE(EliminatedProven, 3);
    WIO_DEFINE_SIMPLE_ENCODER(BoundsCheckMode, wir::lowered, WIO_BOUNDS_CASES)

#undef WIO_DEFINE_SIMPLE_ENCODER
#undef WIO_PIN_CASE
} // namespace wio::bytecode
