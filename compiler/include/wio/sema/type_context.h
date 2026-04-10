#pragma once

#include <vector>
#include <memory>
#include <string>

#include "type.h"
#include "wio/common/smart_ptr.h" 

namespace wio::sema
{
    class TypeContext
    {
    public:
        TypeContext();

        TypeContext(const TypeContext&) = delete;
        TypeContext& operator=(const TypeContext&) = delete;
        TypeContext(TypeContext&&) = default;
        TypeContext& operator=(TypeContext&&) = default;
        ~TypeContext() = default;

        Ref<Type> getVoid()   const { return t_void; }
        Ref<Type> getBool()   const { return t_bool; }
        Ref<Type> getChar()   const { return t_char; }
        Ref<Type> getString() const { return t_string; }
        Ref<Type> getUnknown() const { return t_unknown; }

        Ref<Type> getI8()  const { return t_i8; }
        Ref<Type> getI16() const { return t_i16; }
        Ref<Type> getI32() const { return t_i32; }
        Ref<Type> getI64() const { return t_i64; }

        Ref<Type> getU8()  const { return t_u8; }
        Ref<Type> getU16() const { return t_u16; }
        Ref<Type> getU32() const { return t_u32; }
        Ref<Type> getU64() const { return t_u64; }

        Ref<Type> getISize() const { return t_isize; }
        Ref<Type> getUSize() const { return t_usize; }

        Ref<Type> getF32() const { return t_f32; }
        Ref<Type> getF64() const { return t_f64; }

        Ref<Type> getNull() const { return t_null; }
        Ref<Type> getObject() const { return t_object; }
        
        Ref<Type> getOrCreateReferenceType(Ref<Type> referredType, bool isMutable);
        Ref<Type> getOrCreateArrayType(Ref<Type> elementType, ArrayType::ArrayKind arrayKind, size_t size = 0);
        Ref<Type> getOrCreateFunctionType(Ref<Type> returnType, std::vector<Ref<Type>> paramTypes);
        Ref<Type> getOrCreateDictionaryType(Ref<Type> keyType, Ref<Type> valueType, bool isOrdered = false);
        Ref<Type> getOrCreateTreeType(Ref<Type> keyType, Ref<Type> valueType);
        Ref<Type> getOrCreateStructType(const std::string& name, const Ref<Scope>& structScope);
        Ref<Type> getOrCreateAliasType(const std::string& name, Ref<Type> aliasedType);

    private:
        std::vector<Ref<Type>> ownedTypes_;

        Ref<Type> t_void = nullptr;
        Ref<Type> t_bool = nullptr;
        Ref<Type> t_char = nullptr;
        Ref<Type> t_string = nullptr;
        Ref<Type> t_unknown = nullptr;

        Ref<Type> t_i8 = nullptr;
        Ref<Type> t_i16 = nullptr;
        Ref<Type> t_i32 = nullptr;
        Ref<Type> t_i64 = nullptr;

        Ref<Type> t_u8 = nullptr;
        Ref<Type> t_u16 = nullptr;
        Ref<Type> t_u32 = nullptr;
        Ref<Type> t_u64 = nullptr;
        
        Ref<Type> t_isize = nullptr;
        Ref<Type> t_usize = nullptr;

        Ref<Type> t_f32 = nullptr;
        Ref<Type> t_f64 = nullptr;

        Ref<Type> t_null = nullptr;
        Ref<Type> t_object = nullptr;

        template <typename T, typename... Args>
        Ref<T> makeType(Args&&... args)
        {
            auto uPtr = Ref<T>::Create(std::forward<Args>(args)...);
            ownedTypes_.push_back(uPtr);
            return uPtr;
        }
    };
}
