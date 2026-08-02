#include "wio/sema/type.h"

#include "compiler.h"
#include "wio/codegen/mangler.h"
#include "wio/sema/symbol.h"
#include "wio/sema/scope.h"

#include <functional>
#include <unordered_set>

// NOLINTBEGIN(cppcoreguidelines-pro-type-static-cast-downcast)
namespace wio::sema
{
    namespace
    {
        bool acceptsNull(const Type* type)
        {
            while (type && type->kind() == TypeKind::Alias)
                type = static_cast<const AliasType*>(type)->aliasedType.Get();

            if (!type)
                return false;

            if (type->kind() == TypeKind::Null ||
                type->kind() == TypeKind::Function ||
                type->kind() == TypeKind::Reference)
            {
                return true;
            }

            if (type->kind() == TypeKind::Primitive)
            {
                const std::string& name = static_cast<const PrimitiveType*>(type)->name;
                return name == "any" || name == "opaque";
            }

            if (type->kind() == TypeKind::Struct)
            {
                const auto* structType = static_cast<const StructType*>(type);
                return structType->isObject || structType->isInterface;
            }

            return false;
        }

        bool bindNullToTarget(const Ref<Type>& target, const Ref<Type>& candidate)
        {
            if (!target || !candidate || candidate->kind() != TypeKind::Null || !acceptsNull(target.Get()))
                return false;

            candidate.AsFast<NullType>()->transformedType = target;
            return true;
        }
    }

    // =============================================================
    // Type Helper Methods
    // =============================================================

    bool Type::matchTypes(const Ref<Type>& lhs, const Ref<Type>& rhs)
    {
        if (!lhs || !rhs)
            return false;

        if (rhs->kind() == TypeKind::Null)
            return bindNullToTarget(lhs, rhs);

        if (lhs->kind() == TypeKind::Null)
            return rhs->kind() == TypeKind::Null;

        if (lhs->kind() == TypeKind::Array)
        {
            if (rhs->kind() == TypeKind::Array)
            {
                const Ref<ArrayType> initializerArrayType = rhs.AsFast<ArrayType>();
                Ref<ArrayType> lhsArrayType = lhs.AsFast<ArrayType>();
            
                if (lhsArrayType->size == 0 && initializerArrayType->size != 0)
                    lhsArrayType->size = initializerArrayType->size;
            }
        
        }
        if (lhs->kind() == TypeKind::Dictionary)
        {
            if (rhs->kind() == TypeKind::Dictionary)
            {
                auto lDict = lhs.AsFast<DictionaryType>();
                auto rDict = rhs.AsFast<DictionaryType>();

                if (rDict->keyType->isUnknown() && !lDict->keyType->isUnknown())
                    rDict->keyType = lDict->keyType;
            
                if (rDict->valueType->isUnknown() && !lDict->valueType->isUnknown())
                    rDict->valueType = lDict->valueType;
            }

        }

        return lhs->isCompatibleWith(rhs);
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

        while (t1 && t1->kind() == TypeKind::Alias)
            t1 = static_cast<const AliasType*>(t1)->aliasedType.Get();

        while (t2 && t2->kind() == TypeKind::Alias)
            t2 = static_cast<const AliasType*>(t2)->aliasedType.Get();

        if (!t1 || !t2)
            return false;

        TypeKind kind1 = t1->kind();
        TypeKind kind2 = t2->kind();

        auto isAnyPrimitive = [](const Type* type) -> bool
        {
            return type &&
                   type->kind() == TypeKind::Primitive &&
                   static_cast<const PrimitiveType*>(type)->name == "any";
        };

        auto isAnyStorableType = [](const Type* type) -> bool
        {
            if (!type)
                return false;

            switch (type->kind())
            {
            case TypeKind::Primitive:
            {
                const auto* primitive = static_cast<const PrimitiveType*>(type);
                return primitive->name != "void" &&
                       primitive->name != "<unknown>";
            }
            case TypeKind::Array:
            case TypeKind::Dictionary:
            case TypeKind::Struct:
                return true;
            default:
                return false;
            }
        };

        if (isAnyPrimitive(t1))
        {
            if (kind2 == TypeKind::Null)
            {
                const_cast<NullType*>(static_cast<const NullType*>(t2))->transformedType =
                    Ref<Type>(const_cast<Type*>(t1));
                return true;
            }

            return isAnyStorableType(t2);
        }

        if (kind1 != kind2)
        {
            if (kind2 == TypeKind::Null)
            {
                if (!acceptsNull(t1))
                    return false;

                const_cast<NullType*>(static_cast<const NullType*>(t2))->transformedType =
                    Ref<Type>(const_cast<Type*>(t1));
                return true;
            }

            const bool isPackViewStoragePair =
                (kind1 == TypeKind::ValuePackView && kind2 == TypeKind::PackStorage) ||
                (kind1 == TypeKind::PackStorage && kind2 == TypeKind::ValuePackView);
            if (isPackViewStoragePair)
            {
                if (kind1 == TypeKind::ValuePackView)
                {
                    auto* p1 = static_cast<const ValuePackViewType*>(t1);
                    auto* p2 = static_cast<const PackStorageType*>(t2);
                    if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
                    {
                        if (p1->elementTypes.size() != p2->elementTypes.size())
                            return false;
                        for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                        {
                            if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                                return false;
                        }
                        return true;
                    }
                    return p1->packName == p2->packName;
                }

                auto* p1 = static_cast<const PackStorageType*>(t1);
                auto* p2 = static_cast<const ValuePackViewType*>(t2);
                if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
                {
                    if (p1->elementTypes.size() != p2->elementTypes.size())
                        return false;
                    for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                    {
                        if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                            return false;
                    }
                    return true;
                }
                return p1->packName == p2->packName;
            }

            return false;
        }

        switch (kind1)
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

        case TypeKind::GenericParameter:
        {
            auto* g1 = static_cast<const GenericParameterType*>(t1);
            auto* g2 = static_cast<const GenericParameterType*>(t2);
            return g1->name == g2->name;
        }
        case TypeKind::GenericParameterPack:
        {
            auto* g1 = static_cast<const GenericParameterPackType*>(t1);
            auto* g2 = static_cast<const GenericParameterPackType*>(t2);
            return g1->name == g2->name;
        }
        case TypeKind::ValuePackView:
        {
            auto* p1 = static_cast<const ValuePackViewType*>(t1);
            if (kind2 == TypeKind::PackStorage)
            {
                auto* p2 = static_cast<const PackStorageType*>(t2);
                if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
                {
                    if (p1->elementTypes.size() != p2->elementTypes.size())
                        return false;
                    for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                    {
                        if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                            return false;
                    }
                    return true;
                }
                return p1->packName == p2->packName;
            }

            auto* p2 = static_cast<const ValuePackViewType*>(t2);
            if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
            {
                if (p1->elementTypes.size() != p2->elementTypes.size())
                    return false;
                for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                {
                    if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                        return false;
                }
                return true;
            }
            return p1->packName == p2->packName;
        }
        case TypeKind::TypePackView:
        {
            auto* p1 = static_cast<const TypePackViewType*>(t1);
            auto* p2 = static_cast<const TypePackViewType*>(t2);
            if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
            {
                if (p1->elementTypes.size() != p2->elementTypes.size())
                    return false;
                for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                {
                    if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                        return false;
                }
                return true;
            }
            return p1->packName == p2->packName;
        }
        case TypeKind::PackStorage:
        {
            auto* p1 = static_cast<const PackStorageType*>(t1);
            if (kind2 == TypeKind::ValuePackView)
            {
                auto* p2 = static_cast<const ValuePackViewType*>(t2);
                if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
                {
                    if (p1->elementTypes.size() != p2->elementTypes.size())
                        return false;
                    for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                    {
                        if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                            return false;
                    }
                    return true;
                }
                return p1->packName == p2->packName;
            }

            auto* p2 = static_cast<const PackStorageType*>(t2);
            if (!p1->elementTypes.empty() || !p2->elementTypes.empty())
            {
                if (p1->elementTypes.size() != p2->elementTypes.size())
                    return false;
                for (size_t i = 0; i < p1->elementTypes.size(); ++i)
                {
                    if (!p1->elementTypes[i]->isCompatibleWith(p2->elementTypes[i]))
                        return false;
                }
                return true;
            }
            return p1->packName == p2->packName;
        }
        
        case TypeKind::Reference:
        {
            auto* r1 = static_cast<const ReferenceType*>(t1);
            auto* r2 = static_cast<const ReferenceType*>(t2);

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

            if (a1->arrayKind == ArrayType::ArrayKind::Dynamic)
                return a1->elementType->isCompatibleWith(a2->elementType);

            if (a2->arrayKind == ArrayType::ArrayKind::Dynamic)
                return false;

            if (a2->size > a1->size) return false; // lhs should be bigger
            return a1->elementType->isCompatibleWith(a2->elementType);
        }

        case TypeKind::Dictionary:
        {
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

            if (f1->hasParameterPack != f2->hasParameterPack)
                return false;

            if (f1->paramTypes.size() != f2->paramTypes.size())
                return false;
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
            if (s1->name != s2->name || s1->scopePath != s2->scopePath)
                return false;

            if (s1->genericArguments.size() != s2->genericArguments.size())
                return false;

            for (size_t i = 0; i < s1->genericArguments.size(); ++i)
            {
                if (!s1->genericArguments[i]->isCompatibleWith(s2->genericArguments[i]))
                    return false;
            }

            return true;
        }

        case TypeKind::Alias:
            return t1 == t2;
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

    bool Type::isPoisoned() const
    {
        std::unordered_set<const Type*> visited;
        std::function<bool(const Type*)> containsUnknown = [&](const Type* type) -> bool
        {
            if (!type)
                return true;
            if (type->isUnknown())
                return true;
            if (!visited.insert(type).second)
                return false;

            auto anyPoisoned = [&](const std::vector<Ref<Type>>& types)
            {
                return std::ranges::any_of(types, [&](const Ref<Type>& candidate)
                {
                    return containsUnknown(candidate.Get());
                });
            };

            switch (type->kind())
            {
            case TypeKind::Null:
            {
                const auto* nullType = static_cast<const NullType*>(type);
                return nullType->transformedType && containsUnknown(nullType->transformedType.Get());
            }
            case TypeKind::Reference:
                return containsUnknown(static_cast<const ReferenceType*>(type)->referredType.Get());
            case TypeKind::Array:
                return containsUnknown(static_cast<const ArrayType*>(type)->elementType.Get());
            case TypeKind::Dictionary:
            {
                const auto* dictionaryType = static_cast<const DictionaryType*>(type);
                return containsUnknown(dictionaryType->keyType.Get()) ||
                       containsUnknown(dictionaryType->valueType.Get());
            }
            case TypeKind::Function:
            {
                const auto* functionType = static_cast<const FunctionType*>(type);
                return containsUnknown(functionType->returnType.Get()) || anyPoisoned(functionType->paramTypes);
            }
            case TypeKind::Struct:
                return anyPoisoned(static_cast<const StructType*>(type)->genericArguments);
            case TypeKind::Alias:
                return containsUnknown(static_cast<const AliasType*>(type)->aliasedType.Get());
            case TypeKind::ValuePackView:
                return anyPoisoned(static_cast<const ValuePackViewType*>(type)->elementTypes);
            case TypeKind::TypePackView:
                return anyPoisoned(static_cast<const TypePackViewType*>(type)->elementTypes);
            case TypeKind::PackStorage:
                return anyPoisoned(static_cast<const PackStorageType*>(type)->elementTypes);
            case TypeKind::Primitive:
            case TypeKind::GenericParameter:
            case TypeKind::GenericParameterPack:
                return false;
            }

            return false;
        };

        return containsUnknown(this);
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

    GenericParameterType::GenericParameterType(std::string name)
        : name(std::move(name))
    {
    }

    TypeKind GenericParameterType::kind() const
    {
        return TypeKind::GenericParameter;
    }

    std::string GenericParameterType::toString() const
    {
        return name;
    }

    std::string GenericParameterType::toCppString() const
    {
        return name;
    }

    GenericParameterPackType::GenericParameterPackType(std::string name)
        : name(std::move(name))
    {
    }

    TypeKind GenericParameterPackType::kind() const
    {
        return TypeKind::GenericParameterPack;
    }

    std::string GenericParameterPackType::toString() const
    {
        return name + "...";
    }

    std::string GenericParameterPackType::toCppString() const
    {
        return name + "...";
    }

    ValuePackViewType::ValuePackViewType(std::string packName, std::vector<Ref<Type>> elementTypes)
        : packName(std::move(packName)), elementTypes(std::move(elementTypes))
    {
    }

    TypeKind ValuePackViewType::kind() const
    {
        return TypeKind::ValuePackView;
    }

    std::string ValuePackViewType::toString() const
    {
        if (!elementTypes.empty())
        {
            std::string result = "pack-values<";
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toString() : "<unknown>";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }
        return "pack-values<" + packName + "...>";
    }

    std::string ValuePackViewType::toCppString() const
    {
        std::string result = "wio::meta::ValuePackView<";
        if (!elementTypes.empty())
        {
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toCppString() : "void";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
        }
        else
        {
            result += packName + "...";
        }
        result += ">";
        return result;
    }

    TypePackViewType::TypePackViewType(std::string packName, std::vector<Ref<Type>> elementTypes)
        : packName(std::move(packName)), elementTypes(std::move(elementTypes))
    {
    }

    TypeKind TypePackViewType::kind() const
    {
        return TypeKind::TypePackView;
    }

    std::string TypePackViewType::toString() const
    {
        if (!elementTypes.empty())
        {
            std::string result = "type-pack<";
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toString() : "<unknown>";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }
        return "type-pack<" + packName + "...>";
    }

    std::string TypePackViewType::toCppString() const
    {
        std::string result = "wio::meta::TypePackView<";
        if (!elementTypes.empty())
        {
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toCppString() : "void";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
        }
        else
        {
            result += packName + "...";
        }
        result += ">";
        return result;
    }

    PackStorageType::PackStorageType(std::string packName, std::vector<Ref<Type>> elementTypes)
        : packName(std::move(packName)), elementTypes(std::move(elementTypes))
    {
    }

    TypeKind PackStorageType::kind() const
    {
        return TypeKind::PackStorage;
    }

    std::string PackStorageType::toString() const
    {
        if (!elementTypes.empty())
        {
            std::string result = "pack-storage<";
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toString() : "<unknown>";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }
        return "pack-storage<" + packName + "...>";
    }

    std::string PackStorageType::toCppString() const
    {
        std::string result = "wio::meta::PackStorage<";
        if (!elementTypes.empty())
        {
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                result += elementTypes[i] ? elementTypes[i]->toCppString() : "void";
                if (i + 1 < elementTypes.size())
                    result += ", ";
            }
        }
        else
        {
            result += packName + "...";
        }
        result += ">";
        return result;
    }

    FunctionType::FunctionType(std::vector<Ref<Type>> paramTypes, Ref<Type> returnType, bool hasParameterPack)
        : paramTypes(std::move(paramTypes)), returnType(std::move(returnType)), hasParameterPack(hasParameterPack)
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
        std::stringstream ss;
        ss << "std::function<";
        ss << (returnType ? returnType->toCppString() : "void");
        ss << "(";
        for (size_t i = 0; i < paramTypes.size(); ++i)
        {
            ss << (paramTypes[i] ? paramTypes[i]->toCppString() : "void");
            if (i + 1 < paramTypes.size())
                ss << ", ";
        }
        ss << ")>";
        return ss.str();
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
                std::string objectType = codegen::Mangler::mangleStruct(sType->name, sType->scopePath);
                if (!sType->genericArguments.empty())
                {
                    objectType += "<";
                    for (size_t i = 0; i < sType->genericArguments.size(); ++i)
                    {
                        objectType += sType->genericArguments[i]
                            ? sType->genericArguments[i]->toCppString()
                            : "void";
                        if (i + 1 < sType->genericArguments.size())
                            objectType += ", ";
                    }
                    objectType += ">";
                }

                return std::string("wio::runtime::") +
                       (isMutable ? "BorrowedObjectRef<" : "BorrowedObjectView<") +
                       objectType + ">";
            }
        }
        
        if (isMutable) return baseTypeStr + "*";
        if (!baseTypeStr.empty() && baseTypeStr.back() == '*')
            return baseTypeStr + " const*";
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
        if (genericArguments.empty())
            return name;

        std::string result = name + "<";
        for (size_t i = 0; i < genericArguments.size(); ++i)
        {
            result += genericArguments[i] ? genericArguments[i]->toString() : "<unknown>";
            if (i + 1 < genericArguments.size())
                result += ", ";
        }
        result += ">";
        return result;
    }

    std::string StructType::toCppString() const
    {
        std::string mangled = isInterface ? codegen::Mangler::mangleInterface(name, scopePath) 
                                          : codegen::Mangler::mangleStruct(name, scopePath);

        if (!genericArguments.empty())
        {
            mangled += "<";
            for (size_t i = 0; i < genericArguments.size(); ++i)
            {
                mangled += genericArguments[i] ? genericArguments[i]->toCppString() : "void";
                if (i + 1 < genericArguments.size())
                    mangled += ", ";
            }
            mangled += ">";
        }
        
        if (isObject || isInterface) 
        {
            return "wio::runtime::Ref<" + mangled + ">";
        }
        return mangled;
    }

    std::string getGenericSpecializationKey(const std::vector<Ref<Type>>& types)
    {
        std::string key;
        for (size_t i = 0; i < types.size(); ++i)
        {
            if (i > 0)
                key += "|";
            key += types[i] ? types[i]->toCppString() : "<unknown>";
        }
        return key;
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
