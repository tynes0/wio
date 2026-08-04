#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <frenum.h>

#include "scope.h"
#include "wio/common/smart_ptr.h"
#include "wio/common/utility.h"

namespace wio
{
    namespace sema
    {
        enum class TypeKind : uint8_t {
            Primitive,  // i32, f32, bool, void
            Null,       // null literal
            Nullable,   // T?
            GenericParameter, // T
            ConstGenericParameter, // const N: usize
            ConstValue, // 4 used as a generic argument
            GenericParameterPack, // Args...
            ValuePackView, // args.array
            TypePackView, // Args.array
            PackStorage, // pack values: Args...;
            Reference,  // ref
            Array,      // [T]
            Dictionary, // Dict<T,T> or Tree<T,T>
            Function,   // fn(int) -> string
            Struct,     // struct A {}
            Alias       // type ID = u32;
        };

        // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
        struct Type : RefCountedObject
        {
            ~Type() override = default;
            virtual TypeKind kind() const = 0;
            virtual std::string toString() const = 0;
            virtual std::string toCppString() const = 0;
            
            static bool matchTypes(const Ref<Type>& lhs, const Ref<Type>& rhs);

            bool isNumeric() const;
            bool isCompatibleWith(const Ref<Type>& other) const;
            bool isVoid() const;
            bool isUnknown() const;
            bool isPoisoned() const;

            static Ref<Type> getTypeFromIntegerResult(const IntegerResult& result);
            static Ref<Type> getTypeFromFloatResult(const FloatResult& result);
        };

        struct PrimitiveType : Type
        {
            std::string name;

            PrimitiveType(std::string name);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct NullType : Type
        {
            Ref<Type> transformedType;
            
            NullType(Ref<Type> transformedType);
            
            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
            
        };

        struct NullableType : Type
        {
            Ref<Type> valueType;

            explicit NullableType(Ref<Type> valueType);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct GenericParameterType : Type
        {
            std::string name;

            explicit GenericParameterType(std::string name);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct ConstGenericParameterType : Type
        {
            std::string name;
            Ref<Type> valueType;

            ConstGenericParameterType(std::string name, Ref<Type> valueType);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct ConstValueType : Type
        {
            std::string value;
            Ref<Type> valueType;

            ConstValueType(std::string value, Ref<Type> valueType);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct GenericParameterPackType : Type
        {
            std::string name;

            explicit GenericParameterPackType(std::string name);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct ValuePackViewType : Type
        {
            std::string packName;
            std::vector<Ref<Type>> elementTypes;

            ValuePackViewType(std::string packName, std::vector<Ref<Type>> elementTypes = {});

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct TypePackViewType : Type
        {
            std::string packName;
            std::vector<Ref<Type>> elementTypes;

            TypePackViewType(std::string packName, std::vector<Ref<Type>> elementTypes = {});

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct PackStorageType : Type
        {
            std::string packName;
            std::vector<Ref<Type>> elementTypes;

            PackStorageType(std::string packName, std::vector<Ref<Type>> elementTypes = {});

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };
    
        struct FunctionType : Type
        {
            std::vector<Ref<Type>> paramTypes;
            Ref<Type> returnType;
            bool hasParameterPack = false;

            FunctionType(std::vector<Ref<Type>> paramTypes, Ref<Type> returnType, bool hasParameterPack = false);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct ReferenceType : Type
        {
            Ref<Type> referredType;
            bool isMutable;

            ReferenceType(Ref<Type> referredType, bool isMutable);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct ArrayType : Type
        {
            enum class ArrayKind : uint8_t { Static, Dynamic, Literal };
            
            Ref<Type> elementType;
            ArrayKind arrayKind;
            size_t size;
            Ref<Type> extentType;

            ArrayType(Ref<Type> elementType, ArrayKind arrayKind, size_t size = 0, Ref<Type> extentType = nullptr);
            
            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct DictionaryType : Type
        {
            Ref<Type> keyType;
            Ref<Type> valueType;
            bool isOrdered; // false = Dict (Hash Map), true = Tree (Ordered Map)

            DictionaryType(Ref<Type> keyType, Ref<Type> valueType, bool isOrdered);
            
            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        struct StructType : Type
        {
            std::string name;
            std::string scopePath;
            WeakRef<Scope> structScope;
            std::vector<std::string> genericParameterNames;
            std::vector<Ref<Type>> genericParameterTypes;
            // Aligned with genericParameterNames. Defaults may refer to an
            // earlier generic parameter and are instantiated at the use site.
            std::vector<Ref<Type>> genericParameterDefaults;
            std::vector<Ref<Type>> genericArguments;
            std::unordered_map<std::string, WeakRef<StructType>> explicitSpecializations;
            std::vector<WeakRef<StructType>> partialSpecializations;
            WeakRef<StructType> genericPrimaryType;
            bool isExplicitSpecialization = false;
            bool isPartialSpecialization = false;
            bool hasGenericParameterPack = false;
            std::vector<Ref<Type>> baseTypes;
            std::vector<std::string> fieldNames;
            std::vector<Ref<Type>> fieldTypes;
            std::vector<std::string> trustedTypeKeys;
            bool isObject;
            bool isInterface;
            bool isEnum = false;
            bool isFlagset = false;
            Ref<Type> enumUnderlyingType = nullptr;
            bool isFinal = false;
            bool isNativePodComponent = false;
            std::string nativeCppName;
            std::string nativeCppHeader;

            StructType(std::string name, WeakRef<Scope> structScope, bool isObject = false, bool isInterface = false);
            
            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };

        std::string getGenericSpecializationKey(const std::vector<Ref<Type>>& types);

        struct AliasType : Type
        {
            std::string name;
            Ref<Type> aliasedType;

            AliasType(std::string name, Ref<Type> aliasedType);

            TypeKind kind() const override;
            std::string toString() const override;
            std::string toCppString() const override;
        };
    }
}

MakeFrenumWithNamespace(wio::sema, TypeKind, Primitive, Null, Nullable, GenericParameter, ConstGenericParameter, ConstValue, GenericParameterPack, ValuePackView, TypePackView, PackStorage, Reference, Array, Dictionary, Function, Struct, Alias)
