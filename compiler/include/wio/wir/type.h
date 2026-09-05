#pragma once

#include "wio/wir/id.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wio::wir
{
    enum class TypeKind : std::uint8_t
    {
        Invalid,
        Void,
        Bool,
        I8,
        I16,
        I32,
        I64,
        ISize,
        U8,
        U16,
        U32,
        U64,
        USize,
        F32,
        F64,
        Byte,
        Char,
        String,
        Text,
        Any,
        Opaque,
        GenericParameter,
        ConstGenericParameter,
        ConstValue,
        GenericParameterPack,
        ValuePack,
        TypePack,
        PackStorage,
        Named,
        Reference,
        Nullable,
        Array,
        Dictionary,
        Function,
        AsyncTask,
        // Compiler-internal cursor used by backend-neutral for-in lowering.
        // It never appears in source signatures or native ABI metadata.
        Iterator
    };

    enum class NominalKind : std::uint8_t
    {
        None,
        Component,
        Object,
        Interface,
        Enum,
        Flagset
    };

    enum class NominalRepresentation : std::uint8_t
    {
        Wio,
        NativePod
    };

    enum class NominalValueModel : std::uint8_t
    {
        Regular,
        Tuple,
        Span,
        Option,
        Result
    };

    enum class IntrinsicFamily : std::uint8_t
    {
        None,
        Array,
        Dictionary,
        String,
        Text,
        Enum,
        Flagset,
        Nullable,
        Any,
        Option,
        Result,
        Tuple,
        Span
    };

    enum class FieldVisibility : std::uint8_t
    {
        Private,
        Protected,
        Public
    };

    enum class CaptureKind : std::uint8_t
    {
        Value,
        Reference,
        RetainedSelf
    };

    enum class AsyncExecutorKind : std::uint8_t
    {
        Inherit,
        Main,
        Worker,
        Blocking,
        Io
    };

    enum class AsyncOperation : std::uint8_t
    {
        None,
        AwaitTask,
        SwitchExecutor,
        Start,
        Spawn,
        SpawnWorker,
        SpawnBlocking,
        SpawnIo,
        Join,
        Cancel,
        CancelAfter,
        Detach,
        Yield,
        Sleep,
        Wait
    };

    enum class CoroutineFrameSlotKind : std::uint8_t
    {
        Parameter,
        Local,
        AwaitedTask,
        Temporary
    };

    // A backend-neutral ownership contract. ReferenceCounted values use the
    // same intrusive strong/weak protocol in generated C++ and in the VM.
    enum class OwnershipModel : std::uint8_t
    {
        Trivial,
        OwnedValue,
        ReferenceCounted,
        Borrowed,
        Generic
    };

    enum class CleanupKind : std::uint8_t
    {
        None,
        DestroyValue,
        ReleaseReference
    };

    struct CoroutineFrameSlot
    {
        std::uint32_t slot = 0;
        ValueId value;
        TypeId type;
        CoroutineFrameSlotKind kind = CoroutineFrameSlotKind::Temporary;
        OwnershipModel ownership = OwnershipModel::Trivial;
        CleanupKind cleanup = CleanupKind::None;

        auto operator<=>(const CoroutineFrameSlot&) const = default;
    };

    struct CoroutineState
    {
        std::uint32_t index = 0;
        BlockId suspendBlock;
        BlockId resumeBlock;
        ValueId awaitedTask;
        ValueId resumedValue;
        TypeId resultType;
        AsyncExecutorKind executor = AsyncExecutorKind::Inherit;
        bool cancellationPoint = true;

        auto operator<=>(const CoroutineState&) const = default;
    };

    struct CoroutineLayout
    {
        TypeId resultType;
        std::vector<CoroutineFrameSlot> frameSlots;
        std::vector<CoroutineState> states;
        bool cooperativeCancellation = true;
        bool maySwitchThreads = false;

        auto operator<=>(const CoroutineLayout&) const = default;
    };

    enum class NativeSymbolLanguage : std::uint8_t
    {
        Cpp,
        C
    };

    enum class NativeCallingConvention : std::uint8_t
    {
        PlatformDefault,
        Cdecl,
        StdCall,
        FastCall
    };

    enum class NativeExceptionBoundary : std::uint8_t
    {
        None,
        TranslateToWioFailure
    };

    enum class NativePassingMode : std::uint8_t
    {
        Value,
        Borrow,
        BorrowMut,
        Consume,
        ReturnOwned
    };

    enum class NativeMarshallingKind : std::uint8_t
    {
        Void,
        Scalar,
        Utf8String,
        UnicodeText,
        NativePod,
        OpaqueHandle,
        ObjectHandle,
        Callback,
        RuntimeValue,
        Generic
    };

    enum class NativeCallbackLifetime : std::uint8_t
    {
        Call,
        Retained
    };

    enum class NativeCallbackThread : std::uint8_t
    {
        Caller,
        Any
    };

    enum class NativeThunkKind : std::uint8_t
    {
        Direct,
        Adapter,
        TemplateSpecialization
    };

    enum class NativeReceiverKind : std::uint8_t
    {
        None,
        ConstReference,
        MutableReference
    };

    struct NativeTypeBinding
    {
        std::string cppName;
        std::string header;
        bool standardLayout = true;
        bool triviallyCopyable = true;

        auto operator<=>(const NativeTypeBinding&) const = default;
    };

    struct NativeAbiValue
    {
        TypeId type;
        NativePassingMode passing = NativePassingMode::Value;
        NativeMarshallingKind marshalling = NativeMarshallingKind::Scalar;
        NativeCallbackLifetime callbackLifetime = NativeCallbackLifetime::Call;
        NativeCallbackThread callbackThread = NativeCallbackThread::Caller;
        bool nullable = false;

        auto operator<=>(const NativeAbiValue&) const = default;
    };

    // Canonical backend-neutral description of a C/C++ boundary. Both the
    // native backend and the VM bridge consume this contract; neither backend
    // is allowed to infer ref/view, callback, ownership, or failure behavior.
    struct NativeBinding
    {
        std::string symbol;
        std::string header;
        std::string stableKey;
        std::string thunkSymbol;
        NativeSymbolLanguage language = NativeSymbolLanguage::Cpp;
        NativeCallingConvention callingConvention = NativeCallingConvention::PlatformDefault;
        NativeExceptionBoundary exceptionBoundary = NativeExceptionBoundary::TranslateToWioFailure;
        NativeThunkKind thunkKind = NativeThunkKind::Direct;
        NativeReceiverKind receiver = NativeReceiverKind::None;
        std::vector<NativeAbiValue> parameters;
        NativeAbiValue result;
        bool requiresAdapter = false;

        auto operator<=>(const NativeBinding&) const = default;
    };

    struct CaptureLayout
    {
        std::string name;
        TypeId type;
        CaptureKind kind = CaptureKind::Value;

        auto operator<=>(const CaptureLayout&) const = default;
    };

    struct FieldLayout
    {
        std::string name;
        TypeId type;
        bool isMutable = true;
        FieldVisibility visibility = FieldVisibility::Private;

        auto operator<=>(const FieldLayout&) const = default;
    };

    struct MethodLayout
    {
        std::string name;
        std::vector<TypeId> parameterTypes;
        TypeId returnType;
        FunctionId function;
        std::uint32_t slot = 0;
        bool receiverMutable = true;
        bool isAbstract = false;

        auto operator<=>(const MethodLayout&) const = default;
    };

    struct Type
    {
        TypeKind kind = TypeKind::Invalid;
        std::string name;
        std::vector<TypeId> arguments;
        bool isMutable = false;
        std::optional<std::size_t> staticExtent;
        NominalKind nominalKind = NominalKind::None;
        NominalRepresentation nominalRepresentation = NominalRepresentation::Wio;
        NominalValueModel nominalValueModel = NominalValueModel::Regular;
        std::vector<TypeId> baseTypes;
        std::vector<FieldLayout> fields;
        std::vector<MethodLayout> methods;
        bool hasConstructor = false;
        bool hasDestructor = false;
        OwnershipModel ownership = OwnershipModel::Trivial;
        CleanupKind cleanup = CleanupKind::None;
        std::optional<NativeTypeBinding> nativeBinding;

        auto operator<=>(const Type&) const = default;
    };

    class TypeTable final
    {
    public:
        TypeTable();

        [[nodiscard]] TypeId intern(Type type);
        [[nodiscard]] TypeId internNominal(Type type);
        [[nodiscard]] const Type* tryGet(TypeId id) const;
        [[nodiscard]] const Type& get(TypeId id) const;
        [[nodiscard]] Type& getMutable(TypeId id);
        [[nodiscard]] std::size_t size() const { return types_.size(); }
        [[nodiscard]] const std::vector<Type>& types() const { return types_; }

        [[nodiscard]] TypeId voidType() const { return voidType_; }
        [[nodiscard]] TypeId boolType() const { return boolType_; }
        [[nodiscard]] TypeId i32Type() const { return i32Type_; }
        [[nodiscard]] TypeId stringType() const { return stringType_; }

    private:
        std::vector<Type> types_;
        TypeId voidType_;
        TypeId boolType_;
        TypeId i32Type_;
        TypeId stringType_;
    };

    [[nodiscard]] std::string_view typeKindName(TypeKind kind);
    [[nodiscard]] std::string_view nominalKindName(NominalKind kind);
    [[nodiscard]] std::string_view nominalRepresentationName(NominalRepresentation representation);
    [[nodiscard]] std::string_view nominalValueModelName(NominalValueModel model);
    [[nodiscard]] std::string_view intrinsicFamilyName(IntrinsicFamily family);
    [[nodiscard]] std::string_view fieldVisibilityName(FieldVisibility visibility);
    [[nodiscard]] std::string_view captureKindName(CaptureKind kind);
    [[nodiscard]] std::string_view asyncExecutorKindName(AsyncExecutorKind executor);
    [[nodiscard]] std::string_view asyncOperationName(AsyncOperation operation);
    [[nodiscard]] std::string_view coroutineFrameSlotKindName(CoroutineFrameSlotKind kind);
    [[nodiscard]] std::string_view ownershipModelName(OwnershipModel ownership);
    [[nodiscard]] std::string_view cleanupKindName(CleanupKind cleanup);
    [[nodiscard]] std::string_view nativeSymbolLanguageName(NativeSymbolLanguage language);
    [[nodiscard]] std::string_view nativeCallingConventionName(NativeCallingConvention convention);
    [[nodiscard]] std::string_view nativeExceptionBoundaryName(NativeExceptionBoundary boundary);
    [[nodiscard]] std::string_view nativePassingModeName(NativePassingMode mode);
    [[nodiscard]] std::string_view nativeMarshallingKindName(NativeMarshallingKind kind);
    [[nodiscard]] std::string_view nativeCallbackLifetimeName(NativeCallbackLifetime lifetime);
    [[nodiscard]] std::string_view nativeCallbackThreadName(NativeCallbackThread thread);
    [[nodiscard]] std::string_view nativeThunkKindName(NativeThunkKind kind);
    [[nodiscard]] std::string_view nativeReceiverKindName(NativeReceiverKind receiver);
    [[nodiscard]] bool requiresCleanup(const Type& type);
}
