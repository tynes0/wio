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
        GenericParameter,
        Named,
        Reference,
        Nullable,
        Array,
        Dictionary,
        Function,
        AsyncTask
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
}
