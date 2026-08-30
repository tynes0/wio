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
}
