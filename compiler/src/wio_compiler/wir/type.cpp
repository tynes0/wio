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
        stringType_ = intern(Type{.kind = TypeKind::String});
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
                existing.nominalRepresentation == type.nominalRepresentation)
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
}
