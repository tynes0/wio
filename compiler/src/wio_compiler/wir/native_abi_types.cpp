#include "wio/wir/native_abi_types.h"

namespace wio::wir
{
    NativeMarshallingKind nativeAbiMarshalling(const TypeTable& types, TypeId id)
    {
        const auto& t = types.get(id);

        if ((t.kind == TypeKind::Reference || t.kind == TypeKind::Nullable) && t.arguments.size() == 1)
            return nativeAbiMarshalling(types, t.arguments.front());

        switch (t.kind) // NOLINT(clang-diagnostic-switch-enum)
        {
        case TypeKind::Void:
            return NativeMarshallingKind::Void;
        case TypeKind::String:
            return NativeMarshallingKind::Utf8String;
        case TypeKind::Text:
            return NativeMarshallingKind::UnicodeText;
        case TypeKind::Opaque:
            return NativeMarshallingKind::OpaqueHandle;
        case TypeKind::Function:
            return NativeMarshallingKind::Callback;
        case TypeKind::GenericParameter:
        case TypeKind::ConstGenericParameter:
        case TypeKind::GenericParameterPack:
        case TypeKind::ValuePack:
        case TypeKind::TypePack:
        case TypeKind::PackStorage:
            return NativeMarshallingKind::Generic;
        case TypeKind::Named:
            if (t.nominalKind == NominalKind::Enum || t.nominalKind == NominalKind::Flagset)
                return NativeMarshallingKind::Scalar;
            if (t.nominalRepresentation == NominalRepresentation::NativePod)
                return NativeMarshallingKind::NativePod;
            if (t.nominalKind == NominalKind::Object || t.nominalKind == NominalKind::Interface)
                return NativeMarshallingKind::ObjectHandle;
            return NativeMarshallingKind::RuntimeValue;
        case TypeKind::Any:
        case TypeKind::Array:
        case TypeKind::Dictionary:
        case TypeKind::AsyncTask:
            return NativeMarshallingKind::RuntimeValue;
        default:
            return NativeMarshallingKind::Scalar;
        }
    }

    std::string nativeAbiTypeKey(const TypeTable& types, TypeId id)
    {
        const auto& t = types.get(id);
        std::string key = std::string(typeKindName(t.kind)) + ":" + t.name + (t.isMutable ? ":mut" : ":view");
        if (t.staticExtent)
            key += ":" + std::to_string(*t.staticExtent);
        key += '<';
        for (auto argument : t.arguments)
            key += nativeAbiTypeKey(types, argument) + ';';
        return key + '>';
    }

    void refreshNativeAbiValue(const TypeTable& types, NativeAbiValue& value, bool result)
    {
        const auto& t = types.get(value.type);
        value.marshalling = nativeAbiMarshalling(types, value.type);
        value.nullable = t.kind == TypeKind::Nullable;
        value.passing = t.kind == TypeKind::Reference
                            ? (t.isMutable ? NativePassingMode::BorrowMut : NativePassingMode::Borrow)
                        : result && t.cleanup != CleanupKind::None ? NativePassingMode::ReturnOwned
                                                                   : NativePassingMode::Value;
    }

    TypeId nativeAbiObjectType(const TypeTable& types, TypeId id)
    {
        for (unsigned depth = 0; depth < 64; ++depth)
        {
            const auto* type = types.tryGet(id);
            if (!type)
                return {};
            if ((type->kind == TypeKind::Reference || type->kind == TypeKind::Nullable) && type->arguments.size() == 1)
            {
                id = type->arguments.front();
                continue;
            }
            return type->kind == TypeKind::Named &&
                           (type->nominalKind == NominalKind::Object || type->nominalKind == NominalKind::Interface)
                       ? id
                       : TypeId{};
        }
        return {};
    }
} // namespace wio::wir
