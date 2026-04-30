#include "wio/sema/type_context.h"

namespace wio::sema
{
    TypeContext::TypeContext()
    {
        t_void   = makeType<PrimitiveType>("void");
        t_bool   = makeType<PrimitiveType>("bool");
        t_char   = makeType<PrimitiveType>("char");
        t_string = makeType<PrimitiveType>("string");

        t_i8  = makeType<PrimitiveType>("i8");
        t_i16 = makeType<PrimitiveType>("i16");
        t_i32 = makeType<PrimitiveType>("i32");
        t_i64 = makeType<PrimitiveType>("i64");
            
        t_u8  = makeType<PrimitiveType>("u8");
        t_u16 = makeType<PrimitiveType>("u16");
        t_u32 = makeType<PrimitiveType>("u32");
        t_u64 = makeType<PrimitiveType>("u64");

        t_isize = makeType<PrimitiveType>("isize");
        t_usize = makeType<PrimitiveType>("usize");
        
        t_f32 = makeType<PrimitiveType>("f32");
        t_f64 = makeType<PrimitiveType>("f64");

        t_unknown = makeType<PrimitiveType>("<unknown>");
        
        t_null = makeType<NullType>(t_void);
        t_object = makeType<StructType>("object", nullptr, true, false);
    }

    Ref<Type> TypeContext::getOrCreateReferenceType(Ref<Type> referredType, bool isMutable)
    {
        return makeType<ReferenceType>(std::move(referredType), isMutable);
    }

    Ref<Type> TypeContext::getOrCreateArrayType(Ref<Type> elementType, ArrayType::ArrayKind arrayKind, size_t size)
    {
        return makeType<ArrayType>(std::move(elementType), arrayKind, size);
    }
    
    Ref<Type> TypeContext::getOrCreateDictionaryType(Ref<Type> keyType, Ref<Type> valueType, bool isOrdered)
    {
        return makeType<DictionaryType>(std::move(keyType), std::move(valueType), isOrdered);
    }

    Ref<Type> TypeContext::getOrCreateTreeType(Ref<Type> keyType, Ref<Type> valueType)
    {
        return makeType<DictionaryType>(std::move(keyType), std::move(valueType), true);
    }

    Ref<Type> TypeContext::getOrCreateFunctionType(Ref<Type> returnType, std::vector<Ref<Type>> paramTypes, bool hasParameterPack)
    {
        return makeType<FunctionType>(std::move(paramTypes), std::move(returnType), hasParameterPack);
    }

    Ref<Type> TypeContext::getOrCreateStructType(const std::string& name,
                                                 const Ref<Scope>& structScope,
                                                 bool isObject,
                                                 bool isInterface)
    {
        return makeType<StructType>(name, structScope, isObject, isInterface);
    }

    Ref<Type> TypeContext::getOrCreateAliasType(const std::string& name, Ref<Type> aliasedType)
    {
        return makeType<AliasType>(name, std::move(aliasedType));
    }

    Ref<Type> TypeContext::getOrCreateGenericParameterType(const std::string& name)
    {
        return makeType<GenericParameterType>(name);
    }

    Ref<Type> TypeContext::getOrCreateGenericParameterPackType(const std::string& name)
    {
        return makeType<GenericParameterPackType>(name);
    }

    Ref<Type> TypeContext::getOrCreateValuePackViewType(const std::string& packName, std::vector<Ref<Type>> elementTypes)
    {
        return makeType<ValuePackViewType>(packName, std::move(elementTypes));
    }

    Ref<Type> TypeContext::getOrCreateTypePackViewType(const std::string& packName, std::vector<Ref<Type>> elementTypes)
    {
        return makeType<TypePackViewType>(packName, std::move(elementTypes));
    }

    Ref<Type> TypeContext::getOrCreatePackStorageType(const std::string& packName, std::vector<Ref<Type>> elementTypes)
    {
        return makeType<PackStorageType>(packName, std::move(elementTypes));
    }
}
