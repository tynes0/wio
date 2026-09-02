#include "wio/wir/type.h"

#include <stdexcept>
#include <utility>

namespace wio::wir
{
    TypeTable::TypeTable()
    {
        voidType_ = intern(Type{.kind = TypeKind::Void});
        boolType_ = intern(Type{.kind = TypeKind::Bool});
        i32Type_ = intern(Type{.kind = TypeKind::I32});
        stringType_ = intern(Type{
            .kind = TypeKind::String,
            .ownership = OwnershipModel::OwnedValue,
            .cleanup = CleanupKind::DestroyValue
        });
    }

    TypeId TypeTable::intern(Type type)
    {
        for (std::size_t index = 0; index < types_.size(); ++index)
        {
            if (types_[index] == type)
                return TypeId{static_cast<TypeId::ValueType>(index)};
        }

        const auto id = TypeId{static_cast<TypeId::ValueType>(types_.size())};
        types_.push_back(std::move(type));
        return id;
    }

    TypeId TypeTable::internNominal(Type type)
    {
        if (type.kind != TypeKind::Named)
            return intern(std::move(type));
        for (std::size_t index = 0; index < types_.size(); ++index)
        {
            const Type& existing = types_[index];
            if (existing.kind == TypeKind::Named && existing.name == type.name &&
                existing.arguments == type.arguments && existing.nominalKind == type.nominalKind &&
                existing.nominalRepresentation == type.nominalRepresentation &&
                existing.nominalValueModel == type.nominalValueModel)
            {
                return TypeId{static_cast<TypeId::ValueType>(index)};
            }
        }
        const TypeId id{static_cast<TypeId::ValueType>(types_.size())};
        types_.push_back(std::move(type));
        return id;
    }

    const Type* TypeTable::tryGet(const TypeId id) const
    {
        if (!id || id.value() >= types_.size())
            return nullptr;
        return &types_[id.value()];
    }

    const Type& TypeTable::get(const TypeId id) const
    {
        const Type* type = tryGet(id);
        if (!type)
            throw std::out_of_range("WIR type id is invalid");
        return *type;
    }

    Type& TypeTable::getMutable(const TypeId id)
    {
        if (!id || id.value() >= types_.size())
            throw std::out_of_range("WIR type id is invalid");
        return types_[id.value()];
    }

    std::string_view typeKindName(const TypeKind kind)
    {
        switch (kind)
        {
        case TypeKind::Invalid: return "invalid";
        case TypeKind::Void: return "void";
        case TypeKind::Bool: return "bool";
        case TypeKind::I8: return "i8";
        case TypeKind::I16: return "i16";
        case TypeKind::I32: return "i32";
        case TypeKind::I64: return "i64";
        case TypeKind::ISize: return "isize";
        case TypeKind::U8: return "u8";
        case TypeKind::U16: return "u16";
        case TypeKind::U32: return "u32";
        case TypeKind::U64: return "u64";
        case TypeKind::USize: return "usize";
        case TypeKind::F32: return "f32";
        case TypeKind::F64: return "f64";
        case TypeKind::Byte: return "byte";
        case TypeKind::Char: return "char";
        case TypeKind::String: return "string";
        case TypeKind::Text: return "text";
        case TypeKind::Any: return "any";
        case TypeKind::Opaque: return "opaque";
        case TypeKind::GenericParameter: return "generic-parameter";
        case TypeKind::Named: return "named";
        case TypeKind::Reference: return "reference";
        case TypeKind::Nullable: return "nullable";
        case TypeKind::Array: return "array";
        case TypeKind::Dictionary: return "dictionary";
        case TypeKind::Function: return "function";
        case TypeKind::AsyncTask: return "async-task";
        }
        return "invalid";
    }

    std::string_view nominalKindName(const NominalKind kind)
    {
        switch (kind)
        {
        case NominalKind::None: return "none";
        case NominalKind::Component: return "component";
        case NominalKind::Object: return "object";
        case NominalKind::Interface: return "interface";
        case NominalKind::Enum: return "enum";
        case NominalKind::Flagset: return "flagset";
        }
        return "none";
    }

    std::string_view nominalRepresentationName(const NominalRepresentation representation)
    {
        switch (representation)
        {
        case NominalRepresentation::Wio: return "wio";
        case NominalRepresentation::NativePod: return "native-pod";
        }
        return "wio";
    }

    std::string_view nominalValueModelName(const NominalValueModel model)
    {
        switch (model)
        {
        case NominalValueModel::Regular: return "regular";
        case NominalValueModel::Tuple: return "tuple";
        case NominalValueModel::Span: return "span";
        case NominalValueModel::Option: return "option";
        case NominalValueModel::Result: return "result";
        }
        return "regular";
    }

    std::string_view intrinsicFamilyName(const IntrinsicFamily family)
    {
        switch (family)
        {
        case IntrinsicFamily::None: return "none";
        case IntrinsicFamily::Array: return "array";
        case IntrinsicFamily::Dictionary: return "dictionary";
        case IntrinsicFamily::String: return "string";
        case IntrinsicFamily::Text: return "text";
        case IntrinsicFamily::Enum: return "enum";
        case IntrinsicFamily::Flagset: return "flagset";
        case IntrinsicFamily::Nullable: return "nullable";
        case IntrinsicFamily::Any: return "any";
        case IntrinsicFamily::Option: return "option";
        case IntrinsicFamily::Result: return "result";
        case IntrinsicFamily::Tuple: return "tuple";
        case IntrinsicFamily::Span: return "span";
        }
        return "none";
    }

    std::string_view fieldVisibilityName(const FieldVisibility visibility)
    {
        switch (visibility)
        {
        case FieldVisibility::Private: return "private";
        case FieldVisibility::Protected: return "protected";
        case FieldVisibility::Public: return "public";
        }
        return "private";
    }

    std::string_view captureKindName(const CaptureKind kind)
    {
        switch (kind)
        {
        case CaptureKind::Value: return "value";
        case CaptureKind::Reference: return "reference";
        case CaptureKind::RetainedSelf: return "retained-self";
        }
        return "value";
    }

    std::string_view asyncExecutorKindName(const AsyncExecutorKind executor)
    {
        switch (executor)
        {
        case AsyncExecutorKind::Inherit: return "inherit";
        case AsyncExecutorKind::Main: return "main";
        case AsyncExecutorKind::Worker: return "worker";
        case AsyncExecutorKind::Blocking: return "blocking";
        case AsyncExecutorKind::Io: return "io";
        }
        return "inherit";
    }

    std::string_view asyncOperationName(const AsyncOperation operation)
    {
        switch (operation)
        {
        case AsyncOperation::None: return "none";
        case AsyncOperation::AwaitTask: return "await-task";
        case AsyncOperation::SwitchExecutor: return "switch-executor";
        case AsyncOperation::Start: return "start";
        case AsyncOperation::Spawn: return "spawn";
        case AsyncOperation::SpawnWorker: return "spawn-worker";
        case AsyncOperation::SpawnBlocking: return "spawn-blocking";
        case AsyncOperation::SpawnIo: return "spawn-io";
        case AsyncOperation::Join: return "join";
        case AsyncOperation::Cancel: return "cancel";
        case AsyncOperation::CancelAfter: return "cancel-after";
        case AsyncOperation::Detach: return "detach";
        case AsyncOperation::Yield: return "yield";
        case AsyncOperation::Sleep: return "sleep";
        case AsyncOperation::Wait: return "wait";
        }
        return "none";
    }

    std::string_view coroutineFrameSlotKindName(const CoroutineFrameSlotKind kind)
    {
        switch (kind)
        {
        case CoroutineFrameSlotKind::Parameter: return "parameter";
        case CoroutineFrameSlotKind::Local: return "local";
        case CoroutineFrameSlotKind::AwaitedTask: return "awaited-task";
        case CoroutineFrameSlotKind::Temporary: return "temporary";
        }
        return "temporary";
    }

    std::string_view ownershipModelName(const OwnershipModel ownership)
    {
        switch (ownership)
        {
        case OwnershipModel::Trivial: return "trivial";
        case OwnershipModel::OwnedValue: return "owned-value";
        case OwnershipModel::ReferenceCounted: return "reference-counted";
        case OwnershipModel::Borrowed: return "borrowed";
        case OwnershipModel::Generic: return "generic";
        }
        return "trivial";
    }

    std::string_view cleanupKindName(const CleanupKind cleanup)
    {
        switch (cleanup)
        {
        case CleanupKind::None: return "none";
        case CleanupKind::DestroyValue: return "destroy-value";
        case CleanupKind::ReleaseReference: return "release-reference";
        }
        return "none";
    }

    std::string_view nativeSymbolLanguageName(const NativeSymbolLanguage language)
    {
        return language == NativeSymbolLanguage::C ? "c" : "cpp";
    }

    std::string_view nativeCallingConventionName(const NativeCallingConvention convention)
    {
        switch (convention)
        {
        case NativeCallingConvention::PlatformDefault: return "default";
        case NativeCallingConvention::Cdecl: return "cdecl";
        case NativeCallingConvention::StdCall: return "stdcall";
        case NativeCallingConvention::FastCall: return "fastcall";
        }
        return "default";
    }

    std::string_view nativeExceptionBoundaryName(const NativeExceptionBoundary boundary)
    {
        return boundary == NativeExceptionBoundary::None ? "none" : "translate-to-wio-failure";
    }

    std::string_view nativePassingModeName(const NativePassingMode mode)
    {
        switch (mode)
        {
        case NativePassingMode::Value: return "value";
        case NativePassingMode::Borrow: return "borrow";
        case NativePassingMode::BorrowMut: return "borrow-mut";
        case NativePassingMode::Consume: return "consume";
        case NativePassingMode::ReturnOwned: return "return-owned";
        }
        return "value";
    }

    std::string_view nativeMarshallingKindName(const NativeMarshallingKind kind)
    {
        switch (kind)
        {
        case NativeMarshallingKind::Void: return "void";
        case NativeMarshallingKind::Scalar: return "scalar";
        case NativeMarshallingKind::Utf8String: return "utf8-string";
        case NativeMarshallingKind::UnicodeText: return "unicode-text";
        case NativeMarshallingKind::NativePod: return "native-pod";
        case NativeMarshallingKind::OpaqueHandle: return "opaque-handle";
        case NativeMarshallingKind::ObjectHandle: return "object-handle";
        case NativeMarshallingKind::Callback: return "callback";
        case NativeMarshallingKind::RuntimeValue: return "runtime-value";
        case NativeMarshallingKind::Generic: return "generic";
        }
        return "scalar";
    }

    std::string_view nativeCallbackLifetimeName(const NativeCallbackLifetime lifetime)
    {
        return lifetime == NativeCallbackLifetime::Retained ? "retained" : "call";
    }

    std::string_view nativeCallbackThreadName(const NativeCallbackThread thread)
    {
        return thread == NativeCallbackThread::Any ? "any" : "caller";
    }

    std::string_view nativeThunkKindName(const NativeThunkKind kind)
    {
        switch (kind)
        {
        case NativeThunkKind::Direct: return "direct";
        case NativeThunkKind::Adapter: return "adapter";
        case NativeThunkKind::TemplateSpecialization: return "template-specialization";
        }
        return "direct";
    }

    std::string_view nativeReceiverKindName(const NativeReceiverKind receiver)
    {
        switch (receiver)
        {
        case NativeReceiverKind::None: return "none";
        case NativeReceiverKind::ConstReference: return "const-reference";
        case NativeReceiverKind::MutableReference: return "mutable-reference";
        }
        return "none";
    }

    bool requiresCleanup(const Type& type)
    {
        return type.cleanup != CleanupKind::None;
    }
}
