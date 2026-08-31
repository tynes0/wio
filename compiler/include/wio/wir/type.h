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

    struct Type
    {
        TypeKind kind = TypeKind::Invalid;
        std::string name;
        std::vector<TypeId> arguments;
        bool isMutable = false;
        std::optional<std::size_t> staticExtent;
        NominalKind nominalKind = NominalKind::None;
        NominalRepresentation nominalRepresentation = NominalRepresentation::Wio;

        auto operator<=>(const Type&) const = default;
    };

    class TypeTable final
    {
    public:
        TypeTable();

        [[nodiscard]] TypeId intern(Type type);
        [[nodiscard]] const Type* tryGet(TypeId id) const;
        [[nodiscard]] const Type& get(TypeId id) const;
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
}
