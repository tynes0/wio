#include "wio/sema/type.h"

#include "compiler.h"
#include "wio/codegen/mangler.h"
#include "wio/sema/symbol.h"
#include "wio/sema/scope.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
namespace wio::sema
{
    // =============================================================
    // Type Helper Methods
    // =============================================================

    bool Type::matchTypes(const Ref<Type>& lhs, const Ref<Type>& rhs)
    {
        if (!lhs || !rhs)
            return false;

        if (lhs->kind() == TypeKind::Primitive)
        {
            if (rhs->kind() == TypeKind::Null)
                rhs.AsFast<NullType>()->transformedType = lhs;
            
            return true;
        }
        if (lhs->kind() == TypeKind::Null)
        {
            return true;
        }
        if (lhs->kind() == TypeKind::Function)
        {
            if (rhs->kind() == TypeKind::Null)
            {
                rhs.AsFast<NullType>()->transformedType = lhs;
                return true;
            }
            if (rhs->kind() == TypeKind::Function)
            {
                return lhs->isCompatibleWith(rhs);
            }
            return false;
        }
        if (lhs->kind() == TypeKind::Reference)
        {
            if (rhs->kind() == TypeKind::Null)
                rhs.AsFast<NullType>()->transformedType = lhs;
        
            return false; // Reference types should not be mixed with each other.
        }
        if (lhs->kind() == TypeKind::Array)
        {
            if (rhs->kind() == TypeKind::Null)
            {
                rhs.AsFast<NullType>()->transformedType = lhs;
            }
            else if (rhs->kind() == TypeKind::Array)
            {
                const Ref<ArrayType> initializerArrayType = rhs.AsFast<ArrayType>();
                Ref<ArrayType> lhsArrayType = lhs.AsFast<ArrayType>();
            
                if (lhsArrayType->size == 0 && initializerArrayType->size != 0)
                    lhsArrayType->size = initializerArrayType->size;
            }
        
            return true;
        }
        if (lhs->kind() == TypeKind::Dictionary)
        {
            if (rhs->kind() == TypeKind::Null)
            {
                rhs.AsFast<NullType>()->transformedType = lhs;
            }
            else if (rhs->kind() == TypeKind::Dictionary)
            {
                auto lDict = lhs.AsFast<DictionaryType>();
                auto rDict = rhs.AsFast<DictionaryType>();

                if (rDict->keyType->isUnknown() && !lDict->keyType->isUnknown())
                    rDict->keyType = lDict->keyType;
            
                if (rDict->valueType->isUnknown() && !lDict->valueType->isUnknown())
                    rDict->valueType = lDict->valueType;
            }

            return true;
        }
        if (lhs->kind() == TypeKind::Struct)
        {
            if (rhs->kind() == TypeKind::Null)
            {
                rhs.AsFast<NullType>()->transformedType = lhs;
                return true;
            }
            if (rhs->kind() == TypeKind::Struct)
            {
                auto lStruct = lhs.AsFast<StructType>();
                auto rStruct = rhs.AsFast<StructType>();
            
                return lStruct->name == rStruct->name && lStruct->scopePath == rStruct->scopePath;
            }
        
            return false;
        }
        if (lhs->kind() == TypeKind::Alias)
        {
            return true;
        }

        return false; // Unexpected TypeKind (Shouldn't be possible. I think)
    }

    bool Type::isNumeric() const
    {
        if (kind() != TypeKind::Primitive) return false;
        const auto* p = static_cast<const PrimitiveType*>(this);
        
        return p->name == "i8" || p->name == "i16" || p->name == "i32" || p->name == "i64" ||
               p->name == "u8" || p->name == "u16" || p->name == "u32" || p->name == "u64" ||
               p->name == "isize" || p->name == "usize" ||
               p->name == "f32" || p->name == "f64";
    }

    bool Type::isCompatibleWith(const Ref<Type>& other) const
    {
        if (!other) return false;
        if (this == other.Get()) return true;

        const Type* t1 = this;
        const Type* t2 = other.Get();
        
        if (kind() != other->kind())
        {
            if (kind() == TypeKind::Alias)
            {
                t1 = static_cast<const AliasType*>(this)->aliasedType.Get();
            }
            else if (other->kind() == TypeKind::Alias)
            {
                t2 = other.AsFast<AliasType>()->aliasedType.Get();
            }
            else if (other->kind() == TypeKind::Null)
            {
                return true; // Null compatible with all types 
            }
            else
            {
                return false;
            }
        }


        switch (kind())
        {
        case TypeKind::Primitive:
            {
                auto* p1 = static_cast<const PrimitiveType*>(t1);
                auto* p2 = static_cast<const PrimitiveType*>(t2);
                if (p1->name == p2->name) return true;

                if (t1->isNumeric() && t2->isNumeric())
                {
                    bool destIsFloat = (p1->name == "f32" || p1->name == "f64");
                    bool srcIsFloat = (p2->name == "f32" || p2->name == "f64");

                    if (destIsFloat && srcIsFloat)
                        return p1->name == "f64" && p2->name == "f32";

                    if (destIsFloat && !srcIsFloat)
                        return true;

                    if (!destIsFloat && !srcIsFloat)
                    {
                        auto getSize = [](const std::string& s) -> int {
                            if (s.ends_with("8")) return 1;
                            if (s.ends_with("16")) return 2;
                            if (s.ends_with("32")) return 4;
                            if (s.ends_with("64") || s == "isize" || s == "usize") return 8;
                            return 0;
                        };
                        
                        return getSize(p1->name) >= getSize(p2->name);
                    }
                }
                return false;
            }

        case TypeKind::Null:
        {
           return true;
        }
        
        case TypeKind::Reference:
        {
            if (other->kind() != TypeKind::Reference) return false;

            auto* r1 = static_cast<const ReferenceType*>(this);
            auto* r2 = static_cast<const ReferenceType*>(other.Get());

            if (r1->isMutable && !r2->isMutable)
            {
                return false;
            }

            return r1->referredType->isCompatibleWith(r2->referredType);
        }

        case TypeKind::Array:
        {
            auto* a1 = static_cast<const ArrayType*>(t1);
            auto* a2 = static_cast<const ArrayType*>(t2);
            if (a2->size > a1->size) return false; // lhs should be bigger
            return a1->elementType->isCompatibleWith(a2->elementType);
        }

        case TypeKind::Dictionary:
        {
            if (other->kind() != TypeKind::Dictionary) return false;
        
            auto* d1 = static_cast<const DictionaryType*>(t1);
            auto* d2 = static_cast<const DictionaryType*>(t2);
        
            return d1->isOrdered == d2->isOrdered &&
                   d1->keyType->isCompatibleWith(d2->keyType) &&
                   d1->valueType->isCompatibleWith(d2->valueType);
        }

        case TypeKind::Function:
        {
            auto* f1 = static_cast<const FunctionType*>(t1);
            auto* f2 = static_cast<const FunctionType*>(t2);
            
            if (f1->paramTypes.size() != f2->paramTypes.size()) return false;
            if (!f1->returnType->isCompatibleWith(f2->returnType)) return false;
            
            for (size_t i = 0; i < f1->paramTypes.size(); ++i)
            {
                if (!f1->paramTypes[i]->isCompatibleWith(f2->paramTypes[i])) return false;
            }
            return true;
        }

        case TypeKind::Struct:
        {
            auto* s1 = static_cast<const StructType*>(t1);
            auto* s2 = static_cast<const StructType*>(t2);
            return s1->name == s2->name && s1->scopePath == s2->scopePath;
        }
        
        case TypeKind::Alias:
            auto* a1 = static_cast<const AliasType*>(t1);
            auto* a2 = static_cast<const AliasType*>(t2);
            return a1->aliasedType->isCompatibleWith(a2->aliasedType);
        }
        
        return false;
    }

    bool Type::isVoid() const
    {
        return this == Compiler::get().getTypeContext().getVoid().Get();
    }

    bool Type::isUnknown() const
    {
        return this == Compiler::get().getTypeContext().getUnknown().Get();
    }

    Ref<Type> Type::getTypeFromIntegerResult(const IntegerResult& result)
    {
        if (result.type == IntegerType::i8) return Compiler::get().getTypeContext().getI8();
        if (result.type == IntegerType::i16) return Compiler::get().getTypeContext().getI16();
        if (result.type == IntegerType::i32) return Compiler::get().getTypeContext().getI32();
        if (result.type == IntegerType::i64) return Compiler::get().getTypeContext().getI64();
        if (result.type == IntegerType::u8) return Compiler::get().getTypeContext().getU8();
        if (result.type == IntegerType::u16) return Compiler::get().getTypeContext().getU16();
        if (result.type == IntegerType::u32) return Compiler::get().getTypeContext().getU32();
        if (result.type == IntegerType::u64) return Compiler::get().getTypeContext().getU64();
        if (result.type == IntegerType::isize) return Compiler::get().getTypeContext().getISize();
        if (result.type == IntegerType::usize) return Compiler::get().getTypeContext().getUSize();
        return Compiler::get().getTypeContext().getI32();
    }

    Ref<Type> Type::getTypeFromFloatResult(const FloatResult& result)
    {
        if (result.type == FloatType::f32) return Compiler::get().getTypeContext().getF32();
        if (result.type == FloatType::f64) return Compiler::get().getTypeContext().getF64();
        return Compiler::get().getTypeContext().getF32();
    }

    PrimitiveType::PrimitiveType(std::string name)
        : name(std::move(name))
    {
    }

    TypeKind PrimitiveType::kind() const
    {
        return TypeKind::Primitive;
    }

    std::string PrimitiveType::toString() const
    {
        return name;
    }

    std::string PrimitiveType::toCppString() const
    {
        return common::wioPrimitiveTypeToCppType(name); 
    }

    NullType::NullType(Ref<Type> transformedType)
        : transformedType(std::move(transformedType))
    {
    }

    TypeKind NullType::kind() const
    {
        return TypeKind::Null;
    }

    std::string NullType::toString() const
    {
        return "null";
    }

    std::string NullType::toCppString() const
    {
        return "nullptr";
    }

    FunctionType::FunctionType(std::vector<Ref<Type>> paramTypes, Ref<Type> returnType)
        : paramTypes(std::move(paramTypes)), returnType(std::move(returnType))
    {
    }

    TypeKind FunctionType::kind() const
    {
        return TypeKind::Function;
    }

    std::string FunctionType::toString() const
    {
        std::stringstream ss;
        ss << "fn(";
        for (size_t i = 0; i < paramTypes.size(); ++i)
        {
            ss << paramTypes[i]->toString();
            
            if (i < paramTypes.size() - 1)
                ss << ", ";
        }

        ss << ")";
        ss << " -> " << returnType->toString();
        
        return ss.str();
    }

    std::string FunctionType::toCppString() const
    {
        return ""; // TODO: Impl
    }

    ReferenceType::ReferenceType(Ref<Type> referredType, bool isMutable)
        : referredType(std::move(referredType)), isMutable(isMutable)
    {
    }

    TypeKind ReferenceType::kind() const
    {
        return TypeKind::Reference;
    }

    std::string ReferenceType::toString() const
    {
        return std::string("ref ") + (isMutable ? "mut " : "") + referredType->toString();
    }

    std::string ReferenceType::toCppString() const
    {
        std::string baseTypeStr = referredType->toCppString();
        
        if (referredType->kind() == sema::TypeKind::Struct)
        {
            auto sType = referredType.AsFast<sema::StructType>();
            // YENİ: Interface'ler Fat Pointer olduğu için her yere doğrudan KOPYA (Value) gider.
            // Bu, gecici (rvalue) nesne atama hatasını (cannot bind non-const lvalue) kökünden çözer!
            if (sType->isInterface) 
            {
                return baseTypeStr; 
            }
            
            if (sType->isObject) 
            {
                if (isMutable) return baseTypeStr + "&";
                else return "const " + baseTypeStr + "&";
            }
        }
        
        if (isMutable) return baseTypeStr + "*";
        else return "const " + baseTypeStr + "*";
    }

    ArrayType::ArrayType(Ref<Type> elementType, ArrayKind arrayKind, size_t size)
        : elementType(std::move(elementType)), arrayKind(arrayKind), size(size)
    {
    }

    TypeKind ArrayType::kind() const
    {
        return TypeKind::Array; 
    }

    std::string ArrayType::toString() const
    {
        if (arrayKind == ArrayKind::Static || arrayKind == ArrayKind::Literal)
            return "[" + elementType->toString() + "; " + std::to_string(size) + "]";
        return elementType->toString() + "[]";
    
    }

    std::string ArrayType::toCppString() const
    {
        if (arrayKind == ArrayKind::Static)
            return "wio::SArray<" + elementType->toCppString() + ", " + std::to_string(size) + ">";
        return "wio::DArray<" + elementType->toCppString() + ">";
    }

    DictionaryType::DictionaryType(Ref<Type> keyType, Ref<Type> valueType, bool isOrdered)
        : keyType(std::move(keyType)), valueType(std::move(valueType)), isOrdered(isOrdered)
    {
    }

    TypeKind DictionaryType::kind() const
    {
        return TypeKind::Dictionary;
    }

    std::string DictionaryType::toString() const
    {
        return (isOrdered ? "Tree<" : "Dict<") + keyType->toString() + ", " + valueType->toString() + ">";
    }

    std::string DictionaryType::toCppString() const
    {
        return "wio::" + std::string(isOrdered ? "Tree<" : "Dict<") + keyType->toCppString() + ", " + valueType->toCppString() + ">";
    }

    StructType::StructType(std::string name, WeakRef<Scope> structScope, bool isObject, bool isInterface)
        : name(std::move(name)), structScope(std::move(structScope)), isObject(isObject), isInterface(isInterface)
    {
    }

    TypeKind StructType::kind() const
    {
        return TypeKind::Struct;
    }

    std::string StructType::toString() const
    {
        return name;
    }

    std::string StructType::toCppString() const
    {
        std::string mangled = isInterface ? codegen::Mangler::mangleInterface(name, scopePath) 
                                          : codegen::Mangler::mangleStruct(name, scopePath);
        
        if (isObject || isInterface) 
        {
            return "wio::runtime::Ref<" + mangled + ">";
        }
        return mangled;
    }

    AliasType::AliasType(std::string name, Ref<Type> aliasedType)
        : name(std::move(name)), aliasedType(std::move(aliasedType))
    {
    }

    TypeKind AliasType::kind() const
    {
        return TypeKind::Alias;
    }

    std::string AliasType::toString() const
    {
        return name;
    }

    std::string AliasType::toCppString() const
    {
        return aliasedType->toCppString();
    }
}
// NOLINTEND(cppcoreguidelines-pro-type-static-cast-downcast)
