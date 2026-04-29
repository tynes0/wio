#include "wio/codegen/cpp_generator.h"

#include "wio/common/filesystem/filesystem.h"
#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/symbol.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>

#define EMIT_TABS() do { for (int _____I_____ = 0; _____I_____ < indentationLevel_; ++_____I_____) buffer_ << "    "; } while(false)

namespace wio::codegen
{
    namespace 
    {
        bool isCppReservedIdentifier(std::string_view identifier)
        {
            static const std::unordered_set<std::string_view> reservedIdentifiers = {
                "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
                "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
                "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
                "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
                "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
                "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
                "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
                "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
                "protected", "public", "register", "reinterpret_cast", "requires", "return",
                "short", "signed", "sizeof", "static", "static_assert", "static_cast",
                "struct", "switch", "template", "this", "thread_local", "throw", "true",
                "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
                "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
            };

            return reservedIdentifiers.contains(identifier);
        }

        std::string sanitizeCppIdentifier(std::string_view identifier)
        {
            if (identifier.empty())
                return "_wio_empty";

            if (isCppReservedIdentifier(identifier))
                return "_wio_" + std::string(identifier);

            return std::string(identifier);
        }

        Ref<sema::Type> unwrapAliasTypeForCodegen(Ref<sema::Type> type)
        {
            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;

            return type;
        }

        std::string getBackendTypeEquivalenceKey(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            if (!resolved)
                return "<null>";

            if (resolved->kind() != sema::TypeKind::Primitive)
                return resolved->toCppString();

            const std::string& name = resolved.AsFast<sema::PrimitiveType>()->name;
            if (name == "i8") return "int8_t";
            if (name == "i16") return "int16_t";
            if (name == "i32") return "int32_t";
            if (name == "i64") return "int64_t";
            if (name == "u8") return "uint8_t";
            if (name == "u16") return "uint16_t";
            if (name == "u32") return "uint32_t";
            if (name == "u64") return "uint64_t";
            if (name == "isize") return sizeof(std::intptr_t) == sizeof(std::int64_t) ? "int64_t" : "int32_t";
            if (name == "usize") return sizeof(std::size_t) == sizeof(std::uint64_t) ? "uint64_t" : "uint32_t";
            if (name == "f32") return "float";
            if (name == "f64") return "double";
            if (name == "char") return "char";
            if (name == "uchar" || name == "byte") return "unsigned char";
            if (name == "bool") return "bool";
            if (name == "string") return "wio::String";
            return resolved->toCppString();
        }

        std::string getBackendInstantiationEquivalenceKey(const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string key;
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                if (i > 0)
                    key += "|";
                key += getBackendTypeEquivalenceKey(instantiationTypes[i]);
            }
            return key;
        }

        std::string toCppType(const Ref<sema::Type>& type)
        {
            if (!type) return "void"; // Fallback

            auto appendGenericArguments = [&](std::string baseName, const Ref<sema::StructType>& structType)
            {
                if (!structType || structType->genericArguments.empty())
                    return baseName;

                baseName += "<";
                for (size_t i = 0; i < structType->genericArguments.size(); ++i)
                {
                    baseName += toCppType(structType->genericArguments[i]);
                    if (i + 1 < structType->genericArguments.size())
                        baseName += ", ";
                }
                baseName += ">";
                return baseName;
            };

            if (type->kind() == sema::TypeKind::Function)
            {
                auto funcType = type.AsFast<sema::FunctionType>();
                std::string result = "std::function<" + toCppType(funcType->returnType) + "(";
                for (size_t i = 0; i < funcType->paramTypes.size(); ++i) {
                    result += toCppType(funcType->paramTypes[i]);
                    if (i < funcType->paramTypes.size() - 1) result += ", ";
                }
                result += ")>";
                return result;
            }
            
            if (type->kind() == sema::TypeKind::Reference)
            {
                auto refType = type.AsFast<sema::ReferenceType>();
                if (refType->referredType && refType->referredType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType->referredType.AsFast<sema::StructType>();
                    if (sType->isInterface)
                        return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*";
                }
            }
            else if (type->kind() == sema::TypeKind::Struct)
            {
                auto sType = type.AsFast<sema::StructType>();
                if (sType->isInterface)
                    return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*"; 
            }

            return type->toCppString();
        }

        Ref<sema::Type> unwrapAliasType(Ref<sema::Type> type)
        {
            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;
            return type;
        }

        std::unordered_map<std::string, Ref<sema::Type>> buildGenericTypeBindings(
            const std::vector<std::string>& parameterNames,
            const std::vector<Ref<sema::Type>>& typeArguments)
        {
            std::unordered_map<std::string, Ref<sema::Type>> bindings;
            const size_t bindingCount = std::min(parameterNames.size(), typeArguments.size());
            bindings.reserve(bindingCount);

            for (size_t i = 0; i < bindingCount; ++i)
                bindings.emplace(parameterNames[i], typeArguments[i]);

            return bindings;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings);

        size_t getRequiredParameterCount(const FunctionDeclaration& node)
        {
            size_t requiredCount = node.parameters.size();
            while (requiredCount > 0 && node.parameters[requiredCount - 1].defaultValue)
                --requiredCount;

            return requiredCount;
        }

        bool hasDefaultParameters(const FunctionDeclaration& node)
        {
            return getRequiredParameterCount(node) != node.parameters.size();
        }

        std::vector<Ref<sema::Type>> getLeadingParameterTypes(const Ref<sema::FunctionType>& functionType, size_t arity)
        {
            std::vector<Ref<sema::Type>> parameterTypes;
            if (!functionType)
                return parameterTypes;

            const size_t cappedArity = std::min(arity, functionType->paramTypes.size());
            parameterTypes.reserve(cappedArity);
            for (size_t i = 0; i < cappedArity; ++i)
                parameterTypes.push_back(functionType->paramTypes[i]);

            return parameterTypes;
        }

        bool containsGenericParameterTypeForCodegen(const Ref<sema::Type>& type)
        {
            if (!type)
                return false;

            Ref<sema::Type> resolvedType = unwrapAliasTypeForCodegen(type);
            if (!resolvedType)
                return false;

            switch (resolvedType->kind())
            {
            case sema::TypeKind::GenericParameter:
                return true;
            case sema::TypeKind::Reference:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::ReferenceType>()->referredType);
            case sema::TypeKind::Array:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::ArrayType>()->elementType);
            case sema::TypeKind::Dictionary:
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                return containsGenericParameterTypeForCodegen(dictionaryType->keyType) ||
                       containsGenericParameterTypeForCodegen(dictionaryType->valueType);
            }
            case sema::TypeKind::Function:
            {
                auto functionType = resolvedType.AsFast<sema::FunctionType>();
                if (containsGenericParameterTypeForCodegen(functionType->returnType))
                    return true;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (containsGenericParameterTypeForCodegen(parameterType))
                        return true;
                }
                return false;
            }
            case sema::TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                {
                    if (containsGenericParameterTypeForCodegen(genericArgument))
                        return true;
                }
                return false;
            }
            default:
                return false;
            }
        }

        Ref<sema::Type> instantiateGenericStructType(const Ref<sema::StructType>& structType,
                                                     const std::vector<Ref<sema::Type>>& explicitTypeArguments)
        {
            if (!structType)
                return nullptr;

            if (structType->genericParameterNames.empty())
                return structType;

            auto bindingMap = buildGenericTypeBindings(structType->genericParameterNames, explicitTypeArguments);
            auto instantiatedScope = structType->structScope.Lock();
            auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                structType->name,
                instantiatedScope,
                structType->isObject,
                structType->isInterface
            ).AsFast<sema::StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericArguments = explicitTypeArguments;
            instantiatedType->fieldNames = structType->fieldNames;

            instantiatedType->fieldTypes.reserve(structType->fieldTypes.size());
            for (const auto& fieldType : structType->fieldTypes)
                instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, bindingMap));

            instantiatedType->baseTypes.reserve(structType->baseTypes.size());
            for (const auto& baseType : structType->baseTypes)
                instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, bindingMap));

            return instantiatedType;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings)
        {
            Ref<sema::Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            auto& ctx = Compiler::get().getTypeContext();
            
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case sema::TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<sema::GenericParameterType>();
                if (auto it = bindings.find(genericParam->name); it != bindings.end())
                    return it->second;
                return current;
            }
            case sema::TypeKind::Reference:
            {
                auto refType = current.AsFast<sema::ReferenceType>();
                return ctx.getOrCreateReferenceType(
                    instantiateGenericType(refType->referredType, bindings),
                    refType->isMutable
                );
            }
            case sema::TypeKind::Array:
            {
                auto arrayType = current.AsFast<sema::ArrayType>();
                return ctx.getOrCreateArrayType(
                    instantiateGenericType(arrayType->elementType, bindings),
                    arrayType->arrayKind,
                    arrayType->size
                );
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<sema::DictionaryType>();
                return ctx.getOrCreateDictionaryType(
                    instantiateGenericType(dictType->keyType, bindings),
                    instantiateGenericType(dictType->valueType, bindings),
                    dictType->isOrdered
                );
            }
            case sema::TypeKind::Function:
            {
                auto functionType = current.AsFast<sema::FunctionType>();
                std::vector<Ref<sema::Type>> instantiatedParamTypes;
                instantiatedParamTypes.reserve(functionType->paramTypes.size());
                for (const auto& paramType : functionType->paramTypes)
                    instantiatedParamTypes.push_back(instantiateGenericType(paramType, bindings));

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(functionType->returnType, bindings),
                    instantiatedParamTypes
                );
            }
            case sema::TypeKind::Struct:
            {
                auto structType = current.AsFast<sema::StructType>();
                if (!structType->genericArguments.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedArguments;
                    instantiatedArguments.reserve(structType->genericArguments.size());
                    for (const auto& genericArgument : structType->genericArguments)
                        instantiatedArguments.push_back(instantiateGenericType(genericArgument, bindings));

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                if (!structType->genericParameterNames.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedArguments;
                    instantiatedArguments.reserve(structType->genericParameterNames.size());
                    for (const auto& genericParameterName : structType->genericParameterNames)
                    {
                        if (auto it = bindings.find(genericParameterName); it != bindings.end())
                            instantiatedArguments.push_back(it->second);
                        else
                            return current;
                    }

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                return current;
            }
            default:
                return current;
            }
        }

        std::vector<std::vector<Ref<sema::Type>>> getInstantiateTypeLists(const FunctionDeclaration& node)
        {
            if (auto functionSymbol = node.name ? node.name->referencedSymbol.Lock() : nullptr)
                return functionSymbol->resolvedGenericInstantiations;

            return {};
        }

        std::string formatInstantiatedLogicalName(const std::string& baseName, const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = baseName + "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                result += instantiationTypes[i] ? instantiationTypes[i]->toString() : "unknown";
                if (i + 1 < instantiationTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }

        std::string formatInstantiatedExportSymbolName(const std::string& baseName, const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = baseName;
            for (const auto& instantiationType : instantiationTypes)
            {
                result += "__";
                std::string fragment = Mangler::mangleType(instantiationType);
                std::ranges::replace(fragment, ':', '_');
                result += fragment;
            }
            return result;
        }

        std::string formatTemplateArgumentList(const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            std::string result = "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                result += toCppType(instantiationTypes[i]);
                if (i + 1 < instantiationTypes.size())
                    result += ", ";
            }
            result += ">";
            return result;
        }

        bool shouldAutoReadReferenceType(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> current = unwrapAliasType(type);
            if (!current || current->kind() != sema::TypeKind::Reference)
                return false;

            while (current && current->kind() == sema::TypeKind::Reference)
            {
                auto refType = current.AsFast<sema::ReferenceType>();
                current = unwrapAliasType(refType->referredType);

                if (!current)
                    return false;

                if (current->kind() == sema::TypeKind::Struct)
                {
                    auto structType = current.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        return false;
                }
            }

            return true;
        }

        std::size_t getAutoReadableReferenceDepth(const Ref<sema::Type>& type)
        {
            if (!shouldAutoReadReferenceType(type))
                return 0;

            std::size_t depth = 0;
            Ref<sema::Type> current = unwrapAliasType(type);
            while (current && current->kind() == sema::TypeKind::Reference)
            {
                ++depth;
                current = unwrapAliasType(current.AsFast<sema::ReferenceType>()->referredType);
            }

            return depth;
        }

        std::string_view getIntrinsicHelperName(const IntrinsicMember member)
        {
            switch (member)
            {
            case IntrinsicMember::ArrayCount:
            case IntrinsicMember::DictCount:
            case IntrinsicMember::StringCount:
                return "Count";
            case IntrinsicMember::ArrayEmpty:
            case IntrinsicMember::DictEmpty:
            case IntrinsicMember::StringEmpty:
                return "Empty";
            case IntrinsicMember::ArrayCapacity:
                return "ArrayCapacity";
            case IntrinsicMember::ArrayContains:
                return "ArrayContains";
            case IntrinsicMember::ArrayIndexOf:
                return "ArrayIndexOf";
            case IntrinsicMember::ArrayLastIndexOf:
                return "ArrayLastIndexOf";
            case IntrinsicMember::ArrayFirst:
                return "ArrayFirst";
            case IntrinsicMember::ArrayLast:
                return "ArrayLast";
            case IntrinsicMember::ArrayGetOr:
                return "ArrayGetOr";
            case IntrinsicMember::ArrayClone:
                return "ArrayClone";
            case IntrinsicMember::ArraySlice:
                return "ArraySlice";
            case IntrinsicMember::ArrayTake:
                return "ArrayTake";
            case IntrinsicMember::ArraySkip:
                return "ArraySkip";
            case IntrinsicMember::ArrayConcat:
                return "ArrayConcat";
            case IntrinsicMember::ArrayReversed:
                return "ArrayReversed";
            case IntrinsicMember::ArrayJoin:
                return "ArrayJoin";
            case IntrinsicMember::ArrayPush:
                return "ArrayPush";
            case IntrinsicMember::ArrayPushFront:
                return "ArrayPushFront";
            case IntrinsicMember::ArrayPop:
                return "ArrayPop";
            case IntrinsicMember::ArrayPopFront:
                return "ArrayPopFront";
            case IntrinsicMember::ArrayInsert:
                return "ArrayInsert";
            case IntrinsicMember::ArrayClear:
                return "ArrayClear";
            case IntrinsicMember::ArrayRemoveAt:
                return "ArrayRemoveAt";
            case IntrinsicMember::ArrayRemove:
                return "ArrayRemove";
            case IntrinsicMember::ArrayExtend:
                return "ArrayExtend";
            case IntrinsicMember::ArrayReserve:
                return "ArrayReserve";
            case IntrinsicMember::ArrayShrinkToFit:
                return "ArrayShrinkToFit";
            case IntrinsicMember::ArrayFill:
                return "ArrayFill";
            case IntrinsicMember::ArrayReverse:
                return "ArrayReverse";
            case IntrinsicMember::ArraySort:
                return "ArraySort";
            case IntrinsicMember::ArraySorted:
                return "ArraySorted";
            case IntrinsicMember::DictContainsKey:
                return "DictContainsKey";
            case IntrinsicMember::DictContainsValue:
                return "DictContainsValue";
            case IntrinsicMember::DictGet:
                return "DictGet";
            case IntrinsicMember::DictGetOr:
                return "DictGetOr";
            case IntrinsicMember::DictTryGet:
                return "DictTryGet";
            case IntrinsicMember::DictSet:
                return "DictSet";
            case IntrinsicMember::DictGetOrAdd:
                return "DictGetOrAdd";
            case IntrinsicMember::DictKeys:
                return "DictKeys";
            case IntrinsicMember::DictValues:
                return "DictValues";
            case IntrinsicMember::DictClone:
                return "DictClone";
            case IntrinsicMember::DictMerge:
                return "DictMerge";
            case IntrinsicMember::DictExtend:
                return "DictExtend";
            case IntrinsicMember::DictClear:
                return "DictClear";
            case IntrinsicMember::DictRemove:
                return "DictRemove";
            case IntrinsicMember::TreeFirstKey:
                return "TreeFirstKey";
            case IntrinsicMember::TreeFirstValue:
                return "TreeFirstValue";
            case IntrinsicMember::TreeLastKey:
                return "TreeLastKey";
            case IntrinsicMember::TreeLastValue:
                return "TreeLastValue";
            case IntrinsicMember::TreeFloorKeyOr:
                return "TreeFloorKeyOr";
            case IntrinsicMember::TreeCeilKeyOr:
                return "TreeCeilKeyOr";
            case IntrinsicMember::StringContains:
                return "StringContains";
            case IntrinsicMember::StringContainsChar:
                return "StringContainsChar";
            case IntrinsicMember::StringStartsWith:
                return "StringStartsWith";
            case IntrinsicMember::StringEndsWith:
                return "StringEndsWith";
            case IntrinsicMember::StringIndexOf:
                return "StringIndexOf";
            case IntrinsicMember::StringLastIndexOf:
                return "StringLastIndexOf";
            case IntrinsicMember::StringIndexOfChar:
                return "StringIndexOfChar";
            case IntrinsicMember::StringLastIndexOfChar:
                return "StringLastIndexOfChar";
            case IntrinsicMember::StringFirst:
                return "StringFirst";
            case IntrinsicMember::StringLast:
                return "StringLast";
            case IntrinsicMember::StringGetOr:
                return "StringGetOr";
            case IntrinsicMember::StringSlice:
                return "StringSlice";
            case IntrinsicMember::StringSliceFrom:
                return "StringSliceFrom";
            case IntrinsicMember::StringTake:
                return "StringTake";
            case IntrinsicMember::StringSkip:
                return "StringSkip";
            case IntrinsicMember::StringLeft:
                return "StringLeft";
            case IntrinsicMember::StringRight:
                return "StringRight";
            case IntrinsicMember::StringTrim:
                return "StringTrim";
            case IntrinsicMember::StringTrimStart:
                return "StringTrimStart";
            case IntrinsicMember::StringTrimEnd:
                return "StringTrimEnd";
            case IntrinsicMember::StringToLower:
                return "StringToLower";
            case IntrinsicMember::StringToUpper:
                return "StringToUpper";
            case IntrinsicMember::StringReplace:
                return "StringReplace";
            case IntrinsicMember::StringReplaceFirst:
                return "StringReplaceFirst";
            case IntrinsicMember::StringRepeat:
                return "StringRepeat";
            case IntrinsicMember::StringSplit:
                return "StringSplit";
            case IntrinsicMember::StringLines:
                return "StringLines";
            case IntrinsicMember::StringPadLeft:
                return "StringPadLeft";
            case IntrinsicMember::StringPadRight:
                return "StringPadRight";
            case IntrinsicMember::StringReversed:
                return "StringReversed";
            case IntrinsicMember::StringAppend:
                return "StringAppend";
            case IntrinsicMember::StringPush:
                return "StringPush";
            case IntrinsicMember::StringInsert:
                return "StringInsert";
            case IntrinsicMember::StringErase:
                return "StringErase";
            case IntrinsicMember::StringClear:
                return "StringClear";
            case IntrinsicMember::StringReverse:
                return "StringReverse";
            case IntrinsicMember::StringReplaceInPlace:
                return "StringReplaceInPlace";
            case IntrinsicMember::StringTrimInPlace:
                return "StringTrimInPlace";
            case IntrinsicMember::StringToLowerInPlace:
                return "StringToLowerInPlace";
            case IntrinsicMember::StringToUpperInPlace:
                return "StringToUpperInPlace";
            case IntrinsicMember::None:
                return {};
            }

            return {};
        }

        std::vector<std::string> getBaseInterfaces(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<std::string> bases;
            for (const auto& attr : attributes)
                if (attr->attribute == Attribute::From)
                    for (const auto& arg : attr->args)
                        if (arg.type == TokenType::identifier)
                            bases.push_back(arg.value);
            return bases;
        }

        bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            return std::ranges::any_of(attributes, [targetAttr](const auto& attr) { return attr->attribute == targetAttr; });
        }

        std::vector<Token> getAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            for (const auto& attr : attributes) {
                if (attr->attribute == targetAttr) return attr->args;
            }
            return {};
        }

        std::optional<Token> getSingleAttributeArg(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            for (const auto& attr : attributes)
            {
                if (attr->attribute == targetAttr && attr->args.size() == 1)
                    return attr->args.front();
            }

            return std::nullopt;
        }

        bool isNativeFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Native);
        }

        bool isExportedFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        bool isCommandFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Command);
        }

        bool isEventFunction(const FunctionDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Event);
        }

        bool isExportedComponent(const ComponentDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        bool isExportedObject(const ObjectDeclaration& node)
        {
            return hasAttribute(node.attributes, Attribute::Export);
        }

        std::optional<Attribute> getModuleLifecycleAttribute(const FunctionDeclaration& node)
        {
            for (const auto& attr : node.attributes)
            {
                if (!attr)
                    continue;

                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (attr->attribute)
                {
                case Attribute::ModuleApiVersion:
                case Attribute::ModuleLoad:
                case Attribute::ModuleUpdate:
                case Attribute::ModuleUnload:
                case Attribute::ModuleSaveState:
                case Attribute::ModuleRestoreState:
                    return attr->attribute;
                default:
                    break;
                }
            }

            return std::nullopt;
        }

        std::string getNativeCppSymbolName(const FunctionDeclaration& node)
        {
            if (auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName); cppNameArg.has_value())
                return cppNameArg->value;

            return node.name ? node.name->token.value : "";
        }

        std::string getExportedCppSymbolName(const FunctionDeclaration& node)
        {
            if (std::optional<Attribute> lifecycleAttribute = getModuleLifecycleAttribute(node); lifecycleAttribute.has_value())
            {
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (*lifecycleAttribute)
                {
                case Attribute::ModuleApiVersion: return "WioModuleApiVersion";
                case Attribute::ModuleLoad: return "WioModuleLoad";
                case Attribute::ModuleUpdate: return "WioModuleUpdate";
                case Attribute::ModuleUnload: return "WioModuleUnload";
                case Attribute::ModuleSaveState: return "WioModuleSaveState";
                case Attribute::ModuleRestoreState: return "WioModuleRestoreState";
                default: break;
                }
            }

            if (auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName); cppNameArg.has_value())
                return cppNameArg->value;

            return node.name ? node.name->token.value : "";
        }

        struct ModuleLifecycleFunctions
        {
            const FunctionDeclaration* apiVersion = nullptr;
            const FunctionDeclaration* load = nullptr;
            const FunctionDeclaration* update = nullptr;
            const FunctionDeclaration* unload = nullptr;
            const FunctionDeclaration* saveState = nullptr;
            const FunctionDeclaration* restoreState = nullptr;

            bool hasAny() const
            {
                return apiVersion || load || update || unload || saveState || restoreState;
            }
        };

        void setLifecycleFunctionIfEmpty(const FunctionDeclaration*& slot, const FunctionDeclaration* declaration)
        {
            if (slot == nullptr)
                slot = declaration;
        }

        struct ExportedFunctionInfo
        {
            enum class FieldAccessorKind : uint8_t
            {
                Value,
                ObjectHandle,
                ComponentHandle
            };

            const FunctionDeclaration* declaration = nullptr;
            Ref<sema::FunctionType> functionType = nullptr;
            std::string logicalName;
            std::string symbolName;
            std::string internalSymbol;
            std::vector<Ref<sema::Type>> templateArguments;
            std::optional<std::string> commandName;
            std::optional<std::string> eventName;
            enum class SyntheticKind : uint8_t
            {
                None,
                TypeConstruct,
                TypeDestroy,
                TypeFieldGet,
                TypeFieldSet,
                TypeMethod
            } syntheticKind = SyntheticKind::None;
            std::string ownerCppTypeName;
            std::string memberCppName;
            std::string memberCppTypeName;
            bool ownerIsObject = false;
            FieldAccessorKind fieldAccessorKind = FieldAccessorKind::Value;
        };

        struct ExportedFieldInfo
        {
            std::string fieldName;
            Ref<sema::Type> fieldType = nullptr;
            bool isReadOnly = false;
            AccessModifier accessModifier = AccessModifier::Public;
            std::string memberCppName;
            std::string memberCppTypeName;
            ExportedFunctionInfo::FieldAccessorKind accessorKind = ExportedFunctionInfo::FieldAccessorKind::Value;
            std::optional<std::string> dynamicGetterSymbol;
            std::optional<std::string> dynamicSetterSymbol;
            size_t getterExportIndex = 0;
            std::optional<size_t> setterExportIndex;
        };

        struct ExportedMethodInfo
        {
            std::string methodName;
            size_t exportIndex = 0;
        };

        struct ExportedConstructorInfo
        {
            size_t exportIndex = 0;
        };

        struct ExportedTypeInfo
        {
            std::string logicalName;
            std::string symbolName;
            std::string cppTypeName;
            bool isObject = false;
            std::optional<size_t> createExportIndex;
            size_t destroyExportIndex = 0;
            std::vector<ExportedConstructorInfo> constructors;
            std::vector<ExportedFieldInfo> fields;
            std::vector<ExportedMethodInfo> methods;
        };

        Ref<sema::StructType> getStructTypeFromSymbol(const Ref<sema::Symbol>& symbol);
        std::string mangleStructTypeName(const Ref<sema::StructType>& type);

        std::string getAbiTypeEnumName(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return "WIO_ABI_UNKNOWN";

            if (resolvedType->isVoid())
                return "WIO_ABI_VOID";

            if (resolvedType->kind() != sema::TypeKind::Primitive)
                return "WIO_ABI_UNKNOWN";

            const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
            if (primitiveName == "bool") return "WIO_ABI_BOOL";
            if (primitiveName == "char") return "WIO_ABI_CHAR";
            if (primitiveName == "uchar") return "WIO_ABI_UCHAR";
            if (primitiveName == "byte") return "WIO_ABI_BYTE";
            if (primitiveName == "i8") return "WIO_ABI_I8";
            if (primitiveName == "i16") return "WIO_ABI_I16";
            if (primitiveName == "i32") return "WIO_ABI_I32";
            if (primitiveName == "i64") return "WIO_ABI_I64";
            if (primitiveName == "u8") return "WIO_ABI_U8";
            if (primitiveName == "u16") return "WIO_ABI_U16";
            if (primitiveName == "u32") return "WIO_ABI_U32";
            if (primitiveName == "u64") return "WIO_ABI_U64";
            if (primitiveName == "isize") return "WIO_ABI_ISIZE";
            if (primitiveName == "usize") return "WIO_ABI_USIZE";
            if (primitiveName == "f32") return "WIO_ABI_F32";
            if (primitiveName == "f64") return "WIO_ABI_F64";
            return "WIO_ABI_UNKNOWN";
        }

        std::string getAbiValueFieldName(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != sema::TypeKind::Primitive)
                return {};

            const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
            if (primitiveName == "bool") return "v_bool";
            if (primitiveName == "char") return "v_char";
            if (primitiveName == "uchar") return "v_uchar";
            if (primitiveName == "byte") return "v_byte";
            if (primitiveName == "i8") return "v_i8";
            if (primitiveName == "i16") return "v_i16";
            if (primitiveName == "i32") return "v_i32";
            if (primitiveName == "i64") return "v_i64";
            if (primitiveName == "u8") return "v_u8";
            if (primitiveName == "u16") return "v_u16";
            if (primitiveName == "u32") return "v_u32";
            if (primitiveName == "u64") return "v_u64";
            if (primitiveName == "isize") return "v_isize";
            if (primitiveName == "usize") return "v_usize";
            if (primitiveName == "f32") return "v_f32";
            if (primitiveName == "f64") return "v_f64";
            return {};
        }

        void collectModuleLifecycleFunctions(const std::vector<NodePtr<Statement>>& statements, ModuleLifecycleFunctions& lifecycleFunctions)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectModuleLifecycleFunctions(realmDecl->statements, lifecycleFunctions);
                    continue;
                }

                const auto* fnDecl = statement->as<FunctionDeclaration>();
                if (!fnDecl)
                    continue;

                if (std::optional<Attribute> lifecycleAttribute = getModuleLifecycleAttribute(*fnDecl); lifecycleAttribute.has_value())
                {
                    // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                    switch (*lifecycleAttribute)
                    {
                    case Attribute::ModuleApiVersion: setLifecycleFunctionIfEmpty(lifecycleFunctions.apiVersion, fnDecl); break;
                    case Attribute::ModuleLoad: setLifecycleFunctionIfEmpty(lifecycleFunctions.load, fnDecl); break;
                    case Attribute::ModuleUpdate: setLifecycleFunctionIfEmpty(lifecycleFunctions.update, fnDecl); break;
                    case Attribute::ModuleUnload: setLifecycleFunctionIfEmpty(lifecycleFunctions.unload, fnDecl); break;
                    case Attribute::ModuleSaveState: setLifecycleFunctionIfEmpty(lifecycleFunctions.saveState, fnDecl); break;
                    case Attribute::ModuleRestoreState: setLifecycleFunctionIfEmpty(lifecycleFunctions.restoreState, fnDecl); break;
                    default: break;
                    }
                }
            }
        }

        void collectExportedFunctions(const std::vector<NodePtr<Statement>>& statements, std::vector<ExportedFunctionInfo>& exportedFunctions)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectExportedFunctions(realmDecl->statements, exportedFunctions);
                    continue;
                }

                const auto* fnDecl = statement->as<FunctionDeclaration>();
                if (!fnDecl || !isExportedFunction(*fnDecl))
                    continue;

                auto exportSymbol = fnDecl->name ? fnDecl->name->referencedSymbol.Lock() : nullptr;
                auto declaredFunctionType = exportSymbol && exportSymbol->type
                    ? exportSymbol->type.AsFast<sema::FunctionType>()
                    : nullptr;
                const std::string baseLogicalName = fnDecl->name ? fnDecl->name->token.value : "";
                const std::string baseSymbolName = getExportedCppSymbolName(*fnDecl);
                const std::string internalSymbol = exportSymbol
                    ? Mangler::mangleFunction(fnDecl->name->token.value, declaredFunctionType->paramTypes, exportSymbol->scopePath)
                    : "";

                auto appendCommandAndEventMetadata = [&](ExportedFunctionInfo& info)
                {
                    if (isCommandFunction(*fnDecl))
                    {
                        if (auto commandArg = getSingleAttributeArg(fnDecl->attributes, Attribute::Command); commandArg.has_value())
                            info.commandName = commandArg->value;
                        else
                            info.commandName = info.logicalName;
                    }

                    if (isEventFunction(*fnDecl))
                    {
                        if (auto eventArg = getSingleAttributeArg(fnDecl->attributes, Attribute::Event); eventArg.has_value())
                            info.eventName = eventArg->value;
                    }
                };

                auto instantiations = getInstantiateTypeLists(*fnDecl);
                if (!fnDecl->genericParameters.empty() && !instantiations.empty())
                {
                    for (const auto& instantiationTypes : instantiations)
                    {
                        ExportedFunctionInfo info;
                        info.declaration = fnDecl;
                        info.logicalName = formatInstantiatedLogicalName(baseLogicalName, instantiationTypes);
                        info.symbolName = formatInstantiatedExportSymbolName(baseSymbolName, instantiationTypes);
                        info.internalSymbol = internalSymbol;
                        info.templateArguments = instantiationTypes;

                        if (exportSymbol)
                        {
                            auto bindings = buildGenericTypeBindings(exportSymbol->genericParameterNames, instantiationTypes);
                            info.functionType = instantiateGenericType(exportSymbol->type, bindings).AsFast<sema::FunctionType>();
                        }

                        appendCommandAndEventMetadata(info);
                        exportedFunctions.push_back(std::move(info));
                    }
                    continue;
                }

                ExportedFunctionInfo info;
                info.declaration = fnDecl;
                info.functionType = declaredFunctionType;
                info.logicalName = baseLogicalName;
                info.symbolName = baseSymbolName;
                info.internalSymbol = internalSymbol;
                appendCommandAndEventMetadata(info);
                exportedFunctions.push_back(std::move(info));
            }
        }

        void indexStructDeclarations(
            const std::vector<NodePtr<Statement>>& statements,
            std::unordered_map<const sema::StructType*, const ObjectDeclaration*>& objectDeclarations,
            std::unordered_map<const sema::StructType*, const ComponentDeclaration*>& componentDeclarations)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    indexStructDeclarations(realmDecl->statements, objectDeclarations, componentDeclarations);
                    continue;
                }

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (auto componentType = getStructTypeFromSymbol(componentDecl->name ? componentDecl->name->referencedSymbol.Lock() : nullptr); componentType)
                        componentDeclarations.try_emplace(componentType.Get(), componentDecl);
                    continue;
                }

                if (const auto* objectDecl = statement->as<ObjectDeclaration>())
                {
                    if (auto objectType = getStructTypeFromSymbol(objectDecl->name ? objectDecl->name->referencedSymbol.Lock() : nullptr); objectType)
                        objectDeclarations.try_emplace(objectType.Get(), objectDecl);
                }
            }
        }

        void collectExportedTypes(const std::vector<NodePtr<Statement>>& statements,
                                  std::vector<ExportedFunctionInfo>& exportedFunctions,
                                  std::vector<ExportedTypeInfo>& exportedTypes,
                                  const std::unordered_map<const sema::StructType*, const ObjectDeclaration*>& objectDeclarations,
                                  const std::unordered_map<const sema::StructType*, const ComponentDeclaration*>& componentDeclarations)
        {
            auto& typeContext = Compiler::get().getTypeContext();
            auto isAbiSafeType = [&](const Ref<sema::Type>& type)
            {
                return getAbiTypeEnumName(type) != "WIO_ABI_UNKNOWN";
            };

            auto isExactType = [&](const Ref<sema::Type>& lhs, const Ref<sema::Type>& rhs)
            {
                Ref<sema::Type> left = unwrapAliasType(lhs);
                Ref<sema::Type> right = unwrapAliasType(rhs);
                return left && right && left->isCompatibleWith(right) && right->isCompatibleWith(left);
            };

            auto isCopyConstructorSignature = [&](const Ref<sema::StructType>& selfType,
                                                  const Ref<sema::FunctionType>& functionType)
            {
                if (!selfType || !functionType || functionType->paramTypes.size() != 1)
                    return false;

                Ref<sema::Type> parameterType = unwrapAliasType(functionType->paramTypes[0]);
                if (!parameterType || parameterType->kind() != sema::TypeKind::Reference)
                    return false;

                auto referenceType = parameterType.AsFast<sema::ReferenceType>();
                return isExactType(referenceType->referredType, selfType);
            };

            auto formatConstructorLogicalName = [&](const std::string& typeLogicalName,
                                                    const std::vector<Ref<sema::Type>>& parameterTypes)
            {
                if (parameterTypes.empty())
                    return typeLogicalName + ".__create";

                std::string result = typeLogicalName + ".__create(";
                for (size_t i = 0; i < parameterTypes.size(); ++i)
                {
                    result += parameterTypes[i] ? parameterTypes[i]->toString() : "unknown";
                    if (i + 1 < parameterTypes.size())
                        result += ", ";
                }
                result += ")";
                return result;
            };

            auto formatConstructorSymbolName = [&](const std::string& typeSymbolName,
                                                   const std::vector<Ref<sema::Type>>& parameterTypes)
            {
                std::string result = "WioCreateType__" + typeSymbolName;
                if (parameterTypes.empty())
                    return result;

                for (const auto& parameterType : parameterTypes)
                {
                    result += "__";
                    std::string fragment = Mangler::mangleType(parameterType);
                    std::ranges::replace(fragment, ':', '_');
                    result += fragment;
                }

                return result;
            };

            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectExportedTypes(realmDecl->statements, exportedFunctions, exportedTypes, objectDeclarations, componentDeclarations);
                    continue;
                }

                auto appendExportedField = [&](const VariableDeclaration& variableDecl,
                                               ExportedTypeInfo& typeInfo,
                                               bool isObjectType,
                                               std::unordered_set<std::string>& seenFieldNames)
                {
                    const std::string fieldName = variableDecl.name ? variableDecl.name->token.value : "";
                    if (fieldName.empty() || !seenFieldNames.insert(fieldName).second)
                        return;

                    auto variableSymbol = variableDecl.name ? variableDecl.name->referencedSymbol.Lock() : nullptr;
                    if (!variableSymbol || !variableSymbol->flags.get_isPublic())
                        return;

                    Ref<sema::Type> fieldType = variableSymbol->type ? variableSymbol->type : variableDecl.name->refType.Lock();
                    if (!fieldType)
                        return;

                    auto resolvedFieldType = unwrapAliasType(fieldType);
                    ExportedFunctionInfo::FieldAccessorKind accessorKind = ExportedFunctionInfo::FieldAccessorKind::Value;
                    Ref<sema::Type> accessorBridgeType = fieldType;
                    std::string fieldBridgeCppTypeName = toCppType(fieldType);

                    if (resolvedFieldType && resolvedFieldType->kind() == sema::TypeKind::Struct)
                    {
                        if (auto structType = resolvedFieldType.AsFast<sema::StructType>(); structType)
                        {
                            if (objectDeclarations.contains(structType.Get()))
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ObjectHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                            else if (componentDeclarations.contains(structType.Get()))
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ComponentHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                        }
                    }

                    ExportedFieldInfo fieldInfo;
                    fieldInfo.fieldName = fieldName;
                    fieldInfo.fieldType = fieldType;
                    fieldInfo.isReadOnly = variableSymbol && variableSymbol->flags.get_isReadOnly();
                    fieldInfo.memberCppName = sanitizeCppIdentifier(fieldInfo.fieldName);
                    fieldInfo.memberCppTypeName = fieldBridgeCppTypeName;
                    fieldInfo.accessorKind = accessorKind;
                    if (variableSymbol->flags.get_isProtected())
                        fieldInfo.accessModifier = AccessModifier::Protected;
                    else if (variableSymbol->flags.get_isPrivate())
                        fieldInfo.accessModifier = AccessModifier::Private;
                    else
                        fieldInfo.accessModifier = AccessModifier::Public;

                    const bool needsDynamicBridge = resolvedFieldType &&
                        (resolvedFieldType->kind() == sema::TypeKind::Array ||
                         resolvedFieldType->kind() == sema::TypeKind::Dictionary ||
                         resolvedFieldType->kind() == sema::TypeKind::Function);
                    if (needsDynamicBridge)
                    {
                        fieldInfo.dynamicGetterSymbol = common::formatString("WioDynamicGetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                        if (!fieldInfo.isReadOnly)
                            fieldInfo.dynamicSetterSymbol = common::formatString("WioDynamicSetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                    }

                    ExportedFunctionInfo getterExport;
                    getterExport.functionType = typeContext.getOrCreateFunctionType(accessorBridgeType, { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                    getterExport.logicalName = common::formatString("{}.{}.get", typeInfo.logicalName, fieldInfo.fieldName);
                    getterExport.symbolName = common::formatString("WioGetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                    getterExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeFieldGet;
                    getterExport.ownerCppTypeName = typeInfo.cppTypeName;
                    getterExport.memberCppName = fieldInfo.memberCppName;
                    getterExport.memberCppTypeName = fieldInfo.memberCppTypeName;
                    getterExport.ownerIsObject = isObjectType;
                    getterExport.fieldAccessorKind = fieldInfo.accessorKind;
                    fieldInfo.getterExportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(getterExport));

                    if (!fieldInfo.isReadOnly)
                    {
                        ExportedFunctionInfo setterExport;
                        setterExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize(), accessorBridgeType }).AsFast<sema::FunctionType>();
                        setterExport.logicalName = common::formatString("{}.{}.set", typeInfo.logicalName, fieldInfo.fieldName);
                        setterExport.symbolName = common::formatString("WioSetField__{}__{}", typeInfo.symbolName, fieldInfo.fieldName);
                        setterExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeFieldSet;
                        setterExport.ownerCppTypeName = typeInfo.cppTypeName;
                        setterExport.memberCppName = fieldInfo.memberCppName;
                        setterExport.memberCppTypeName = fieldInfo.memberCppTypeName;
                        setterExport.ownerIsObject = isObjectType;
                        setterExport.fieldAccessorKind = fieldInfo.accessorKind;
                        fieldInfo.setterExportIndex = exportedFunctions.size();
                        exportedFunctions.push_back(std::move(setterExport));
                    }

                    typeInfo.fields.push_back(std::move(fieldInfo));
                };

                auto appendExportedMethod = [&](const FunctionDeclaration& functionDecl,
                                                ExportedTypeInfo& typeInfo,
                                                std::unordered_set<std::string>& seenMethodKeys)
                {
                    if (!functionDecl.name || !functionDecl.genericParameters.empty())
                        return;

                    const std::string functionName = functionDecl.name->token.value;
                    if (functionName == "OnConstruct" || functionName == "OnDestruct")
                        return;

                    auto functionSymbol = functionDecl.name->referencedSymbol.Lock();
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                    if (!functionSymbol || !functionSymbol->flags.get_isPublic() || !functionType)
                        return;

                    const std::string methodKey = functionName + "|" + Mangler::mangleFunction(functionName, functionType->paramTypes);
                    if (!seenMethodKeys.insert(methodKey).second)
                        return;

                    if (getAbiTypeEnumName(functionType->returnType) == "WIO_ABI_UNKNOWN")
                        return;

                    bool allParametersAbiSafe = true;
                    std::vector<Ref<sema::Type>> exportedParameterTypes;
                    exportedParameterTypes.reserve(functionType->paramTypes.size() + 1);
                    exportedParameterTypes.push_back(typeContext.getUSize());

                    for (const auto& parameterType : functionType->paramTypes)
                    {
                        if (getAbiTypeEnumName(parameterType) == "WIO_ABI_UNKNOWN")
                        {
                            allParametersAbiSafe = false;
                            break;
                        }

                        exportedParameterTypes.push_back(parameterType);
                    }

                    if (!allParametersAbiSafe)
                        return;

                    ExportedFunctionInfo methodExport;
                    methodExport.functionType = typeContext.getOrCreateFunctionType(functionType->returnType, exportedParameterTypes).AsFast<sema::FunctionType>();
                    methodExport.logicalName = typeInfo.logicalName + "." + functionName;
                    methodExport.symbolName = common::formatString("WioMethod__{}__{}__{}", typeInfo.symbolName, functionName, Mangler::mangleFunction(functionName, functionType->paramTypes));
                    methodExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeMethod;
                    methodExport.ownerCppTypeName = typeInfo.cppTypeName;
                    methodExport.memberCppName = Mangler::mangleFunction(functionName, functionType->paramTypes);
                    methodExport.ownerIsObject = true;

                    ExportedMethodInfo methodInfo;
                    methodInfo.methodName = functionName;
                    methodInfo.exportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(methodExport));
                    typeInfo.methods.push_back(std::move(methodInfo));
                };

                auto appendConstructorExport = [&](ExportedTypeInfo& typeInfo,
                                                   const std::vector<Ref<sema::Type>>& parameterTypes,
                                                   bool isObjectType,
                                                   std::unordered_set<std::string>& seenConstructorSignatures)
                {
                    for (const auto& parameterType : parameterTypes)
                    {
                        if (!isAbiSafeType(parameterType))
                            return;
                    }

                    std::string signatureKey = formatConstructorSymbolName(typeInfo.symbolName, parameterTypes);
                    if (!seenConstructorSignatures.insert(signatureKey).second)
                        return;

                    ExportedFunctionInfo constructorExport;
                    constructorExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getUSize(), parameterTypes).AsFast<sema::FunctionType>();
                    constructorExport.logicalName = formatConstructorLogicalName(typeInfo.logicalName, parameterTypes);
                    constructorExport.symbolName = std::move(signatureKey);
                    constructorExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeConstruct;
                    constructorExport.ownerCppTypeName = typeInfo.cppTypeName;
                    constructorExport.ownerIsObject = isObjectType;

                    const size_t exportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(constructorExport));
                    typeInfo.constructors.push_back({ exportIndex });

                    if (parameterTypes.empty() && !typeInfo.createExportIndex.has_value())
                        typeInfo.createExportIndex = exportIndex;
                };

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (!isExportedComponent(*componentDecl) || !componentDecl->name || !componentDecl->genericParameters.empty())
                        continue;

                    auto componentSymbol = componentDecl->name->referencedSymbol.Lock();
                    auto componentType = getStructTypeFromSymbol(componentSymbol);
                    if (!componentType)
                        continue;

                    ExportedTypeInfo typeInfo;
                    typeInfo.logicalName = componentType->scopePath.empty()
                        ? componentType->name
                        : common::formatString("{}::{}", componentType->scopePath, componentType->name);
                    typeInfo.symbolName = Mangler::mangleStruct(componentType->name, componentType->scopePath);
                    typeInfo.cppTypeName = mangleStructTypeName(componentType);
                    typeInfo.isObject = false;

                    ExportedFunctionInfo destroyExport;
                    destroyExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                    destroyExport.logicalName = typeInfo.logicalName + ".__destroy";
                    destroyExport.symbolName = "WioDestroyType__" + typeInfo.symbolName;
                    destroyExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeDestroy;
                    destroyExport.ownerCppTypeName = typeInfo.cppTypeName;
                    typeInfo.destroyExportIndex = exportedFunctions.size();
                    exportedFunctions.push_back(std::move(destroyExport));

                    std::vector<Ref<sema::Type>> memberTypes;
                    memberTypes.reserve(componentDecl->members.size());
                    bool hasCustomCtor = false;
                    bool hasEmptyCtor = false;
                    bool hasMemberCtor = false;
                    const bool hasNoDefaultCtor = hasAttribute(componentDecl->attributes, Attribute::NoDefaultCtor);
                    const bool forceGenerateCtors = hasAttribute(componentDecl->attributes, Attribute::GenerateCtors);
                    std::unordered_set<std::string> seenConstructorSignatures;

                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                            continue;

                        auto* variableDecl = member.declaration->as<VariableDeclaration>();
                        auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                        if (Ref<sema::Type> memberType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock(); memberType)
                            memberTypes.push_back(memberType);
                    }

                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                            continue;

                        auto* functionDecl = member.declaration->as<FunctionDeclaration>();
                        if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                            continue;

                        hasCustomCtor = true;
                        auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                        auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                        if (!functionType)
                            continue;

                        const bool isCopyCtor = isCopyConstructorSignature(componentType, functionType);
                        if (functionType->paramTypes.empty())
                            hasEmptyCtor = true;
                        
                        if (!isCopyCtor && functionType->paramTypes.size() == memberTypes.size())
                        {
                            bool isMemberCtor = true;
                            for (size_t i = 0; i < memberTypes.size(); ++i)
                            {
                                if (!isExactType(functionType->paramTypes[i], memberTypes[i]))
                                {
                                    isMemberCtor = false;
                                    break;
                                }
                            }

                            if (isMemberCtor)
                                hasMemberCtor = true;
                        }

                        if (!isCopyCtor && member.access == AccessModifier::Public)
                            appendConstructorExport(typeInfo, functionType->paramTypes, /*isObjectType=*/false, seenConstructorSignatures);
                    }

                    if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                    {
                        if (!hasEmptyCtor)
                            appendConstructorExport(typeInfo, {}, /*isObjectType=*/false, seenConstructorSignatures);

                        if (!hasMemberCtor && !memberTypes.empty())
                            appendConstructorExport(typeInfo, memberTypes, /*isObjectType=*/false, seenConstructorSignatures);
                    }

                    std::unordered_set<std::string> seenFieldNames;
                    for (const auto& member : componentDecl->members)
                    {
                        if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                            continue;

                        appendExportedField(*member.declaration->as<VariableDeclaration>(), typeInfo, false, seenFieldNames);
                    }

                    exportedTypes.push_back(std::move(typeInfo));
                    continue;
                }

                const auto* objectDecl = statement->as<ObjectDeclaration>();
                if (!objectDecl || !isExportedObject(*objectDecl) || !objectDecl->name || !objectDecl->genericParameters.empty())
                    continue;

                auto objectSymbol = objectDecl->name->referencedSymbol.Lock();
                auto objectType = getStructTypeFromSymbol(objectSymbol);
                if (!objectType)
                    continue;

                ExportedTypeInfo typeInfo;
                typeInfo.logicalName = objectType->scopePath.empty()
                    ? objectType->name
                    : common::formatString("{}::{}", objectType->scopePath, objectType->name);
                
                typeInfo.symbolName = Mangler::mangleStruct(objectType->name, objectType->scopePath);
                typeInfo.cppTypeName = mangleStructTypeName(objectType);
                typeInfo.isObject = true;

                ExportedFunctionInfo destroyExport;
                destroyExport.functionType = typeContext.getOrCreateFunctionType(typeContext.getVoid(), { typeContext.getUSize() }).AsFast<sema::FunctionType>();
                destroyExport.logicalName = typeInfo.logicalName + ".__destroy";
                destroyExport.symbolName = "WioDestroyType__" + typeInfo.symbolName;
                destroyExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeDestroy;
                destroyExport.ownerCppTypeName = typeInfo.cppTypeName;
                destroyExport.ownerIsObject = true;
                typeInfo.destroyExportIndex = exportedFunctions.size();
                exportedFunctions.push_back(std::move(destroyExport));

                std::vector<Ref<sema::Type>> memberTypes;
                memberTypes.reserve(objectDecl->members.size());
                bool hasCustomCtor = false;
                bool hasEmptyCtor = false;
                bool hasMemberCtor = false;
                const bool hasNoDefaultCtor = hasAttribute(objectDecl->attributes, Attribute::NoDefaultCtor);
                const bool forceGenerateCtors = hasAttribute(objectDecl->attributes, Attribute::GenerateCtors);
                std::unordered_set<std::string> seenConstructorSignatures;

                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration || !member.declaration->is<VariableDeclaration>())
                        continue;

                    auto* variableDecl = member.declaration->as<VariableDeclaration>();
                    auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                    if (Ref<sema::Type> memberType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock(); memberType)
                        memberTypes.push_back(memberType);
                }

                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto* functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    hasCustomCtor = true;
                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<sema::FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor = isCopyConstructorSignature(objectType, functionType);
                    if (functionType->paramTypes.empty())
                        hasEmptyCtor = true;

                    if (!isCopyCtor && functionType->paramTypes.size() == memberTypes.size())
                    {
                        bool isMemberCtor = true;
                        for (size_t i = 0; i < memberTypes.size(); ++i)
                        {
                            if (!isExactType(functionType->paramTypes[i], memberTypes[i]))
                            {
                                isMemberCtor = false;
                                break;
                            }
                        }

                        if (isMemberCtor)
                            hasMemberCtor = true;
                    }

                    if (!isCopyCtor && member.access == AccessModifier::Public)
                        appendConstructorExport(typeInfo, functionType->paramTypes, /*isObjectType=*/true, seenConstructorSignatures);
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        appendConstructorExport(typeInfo, {}, /*isObjectType=*/true, seenConstructorSignatures);

                    if (!hasMemberCtor && !memberTypes.empty())
                        appendConstructorExport(typeInfo, memberTypes, /*isObjectType=*/true, seenConstructorSignatures);
                }

                std::unordered_set<std::string> seenFieldNames;
                std::unordered_set<std::string> seenMethodKeys;
                for (const auto& member : objectDecl->members)
                {
                    if (!member.declaration)
                        continue;

                    if (member.declaration->is<VariableDeclaration>())
                    {
                        appendExportedField(*member.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                        continue;
                    }

                    if (!member.declaration->is<FunctionDeclaration>())
                        continue;

                    appendExportedMethod(*member.declaration->as<FunctionDeclaration>(), typeInfo, seenMethodKeys);
                }

                std::function<void(const Ref<sema::StructType>&)> appendInheritedMembers = [&](const Ref<sema::StructType>& derivedType)
                {
                    if (!derivedType)
                        return;

                    for (const auto& baseType : derivedType->baseTypes)
                    {
                        auto resolvedBaseType = unwrapAliasType(baseType);
                        if (!resolvedBaseType || resolvedBaseType->kind() != sema::TypeKind::Struct)
                            continue;

                        auto baseStruct = resolvedBaseType.AsFast<sema::StructType>();
                        if (!baseStruct || baseStruct->isInterface || (baseStruct->name == "object" && baseStruct->scopePath.empty()))
                            continue;

                        if (auto objectIt = objectDeclarations.find(baseStruct.Get()); objectIt != objectDeclarations.end())
                        {
                            if (const auto* baseObjectDecl = objectIt->second; baseObjectDecl)
                            {
                                for (const auto& baseMember : baseObjectDecl->members)
                                {
                                    if (!baseMember.declaration)
                                        continue;

                                    if (baseMember.declaration->is<VariableDeclaration>())
                                    {
                                        appendExportedField(*baseMember.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                                        continue;
                                    }

                                    if (baseMember.declaration->is<FunctionDeclaration>())
                                        appendExportedMethod(*baseMember.declaration->as<FunctionDeclaration>(), typeInfo, seenMethodKeys);
                                }
                            }

                            appendInheritedMembers(baseStruct);
                            continue;
                        }

                        if (auto componentIt = componentDeclarations.find(baseStruct.Get()); componentIt != componentDeclarations.end())
                        {
                            const auto* baseComponentDecl = componentIt->second;
                            if (!baseComponentDecl)
                                continue;

                            for (const auto& baseMember : baseComponentDecl->members)
                            {
                                if (!baseMember.declaration || !baseMember.declaration->is<VariableDeclaration>())
                                    continue;

                                appendExportedField(*baseMember.declaration->as<VariableDeclaration>(), typeInfo, true, seenFieldNames);
                            }
                        }
                    }
                };

                appendInheritedMembers(objectType);

                exportedTypes.push_back(std::move(typeInfo));
            }
        }

        void collectCppHeaders(const std::vector<NodePtr<Statement>>& statements, std::unordered_set<std::string>& seenHeaders, std::vector<std::string>& orderedHeaders)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;

                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    collectCppHeaders(realmDecl->statements, seenHeaders, orderedHeaders);
                    continue;
                }

                if (const auto* useStmt = statement->as<UseStatement>())
                {
                    if (useStmt->isCppHeader)
                    {
                        if (seenHeaders.insert(useStmt->modulePath).second)
                            orderedHeaders.push_back(useStmt->modulePath);
                        continue;
                    }
                }

                const auto* fnDecl = statement->as<FunctionDeclaration>();
                if (!fnDecl || !isNativeFunction(*fnDecl))
                    continue;

                auto headerArg = getSingleAttributeArg(fnDecl->attributes, Attribute::CppHeader);
                if (!headerArg.has_value() || headerArg->type != TokenType::stringLiteral)
                    continue;

                if (seenHeaders.insert(headerArg->value).second)
                    orderedHeaders.push_back(headerArg->value);
            }
        }

        std::string mangleStructTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};
            std::string name = Mangler::mangleStruct(type->name, type->scopePath);
            if (!type->genericArguments.empty())
            {
                name += "<";
                for (size_t i = 0; i < type->genericArguments.size(); ++i)
                {
                    name += toCppType(type->genericArguments[i]);
                    if (i + 1 < type->genericArguments.size())
                        name += ", ";
                }
                name += ">";
            }

            return name;
        }

        std::string mangleInterfaceTypeName(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};
            std::string name = Mangler::mangleInterface(type->name, type->scopePath);
            if (!type->genericArguments.empty())
            {
                name += "<";
                for (size_t i = 0; i < type->genericArguments.size(); ++i)
                {
                    name += toCppType(type->genericArguments[i]);
                    if (i + 1 < type->genericArguments.size())
                        name += ", ";
                }
                name += ">";
            }

            return name;
        }

        std::string mangleNamedType(const Ref<sema::StructType>& type)
        {
            if (!type)
                return {};

            return type->isInterface ? mangleInterfaceTypeName(type) : mangleStructTypeName(type);
        }

        Ref<sema::StructType> getStructTypeFromSymbol(const Ref<sema::Symbol>& symbol)
        {
            if (!symbol || symbol->kind != sema::SymbolKind::Struct || !symbol->type || symbol->type->kind() != sema::TypeKind::Struct)
                return nullptr;

            return symbol->type.AsFast<sema::StructType>();
        }

        Ref<sema::Symbol> resolveQualifiedSymbol(const Ref<sema::Scope>& startScope, std::string_view qualifiedName)
        {
            if (!startScope || qualifiedName.empty())
                return nullptr;

            size_t segmentStart = 0;
            Ref<sema::Scope> scope = startScope;
            Ref<sema::Symbol> resolvedSymbol = nullptr;

            while (segmentStart < qualifiedName.size())
            {
                size_t separator = qualifiedName.find("::", segmentStart);
                std::string segment = separator == std::string_view::npos
                    ? std::string(qualifiedName.substr(segmentStart))
                    : std::string(qualifiedName.substr(segmentStart, separator - segmentStart));

                if (segment.empty())
                    return nullptr;

                resolvedSymbol = scope->resolve(segment);
                if (!resolvedSymbol)
                    return nullptr;

                if (separator == std::string_view::npos)
                    return resolvedSymbol;

                if (!resolvedSymbol->innerScope)
                    return nullptr;

                scope = resolvedSymbol->innerScope;
                segmentStart = separator + 2;
            }

            return resolvedSymbol;
        }

        std::string mangleNamedType(const Ref<sema::Symbol>& symbol)
        {
            return mangleNamedType(getStructTypeFromSymbol(symbol));
        }
    }
    
    CppGenerator::CppGenerator() = default;

    std::string CppGenerator::generate(const Ref<Program>& program)
    {
        buffer_.str("");
        indentationLevel_ = 0;

        generateHeader();

        std::unordered_set<std::string> seenHeaders;
        std::vector<std::string> nativeHeaders;
        collectCppHeaders(program->statements, seenHeaders, nativeHeaders);
        for (const auto& header : nativeHeaders)
        {
            emitHeaderLine("#include \"" + common::wioStringToEscapedCppString(header) + "\"");
        }
        if (!nativeHeaders.empty())
            emitHeaderLine("");

        program->accept(*this);
        emitModuleApiTable(program);

        return header_.str() + buffer_.str();
    }

    void CppGenerator::generateHeader()
    {
        header_.str("");
        emitHeaderLine("// Generated by the Wio compiler.");
        emitHeaderLine("//");
        emitHeaderLine("// This file is an implementation artifact that can be inspected for");
        emitHeaderLine("// debugging, backend portability work, or native interop troubleshooting.");
        emitHeaderLine("// Source-level diagnostics are remapped back to .wio files through #line");
        emitHeaderLine("// directives, so backend failures should still point to Wio locations.");
        emitHeaderLine();
        emitHeaderLine("#include <cstdint>");
        emitHeaderLine("#include <cstddef>");
        emitHeaderLine("#include <algorithm>");
        emitHeaderLine("#include <limits>");
        emitHeaderLine("#include <string>");
        emitHeaderLine("#include <utility>");
        emitHeaderLine("#include <vector>");
        emitHeaderLine("#include <array>");
        emitHeaderLine("#include <format>");
        emitHeaderLine("#include <iostream>");
        emitHeaderLine("#include <functional>");
        emitHeaderLine("#include <map>");
        emitHeaderLine("#include <stdexcept>");
        emitHeaderLine("#include <unordered_map>");
        emitHeaderLine("#include <exception.h>");
        emitHeaderLine("#include <fit.h>");
        emitHeaderLine("#include <intrinsics.h>");
        emitHeaderLine("#include <module_api.h>");
        emitHeaderLine("#include <ref.h>");
        emitHeaderLine();
        emitHeaderLine("#if defined(_WIN32)");
        emitHeaderLine("#define WIO_EXPORT __declspec(dllexport)");
        emitHeaderLine("#else");
        emitHeaderLine("#define WIO_EXPORT __attribute__((visibility(\"default\")))");
        emitHeaderLine("#endif");
        emitHeaderLine();
        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("using String = std::string;");
        emitHeaderLine("using WString = std::wstring;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine();
        emitHeaderLine("namespace wio");
        emitHeaderLine("{");
        indent();
        emitHeaderLine("template <typename T>");
        emitHeaderLine("using DArray = std::vector<T>;");
        emitHeaderLine();
        emitHeaderLine("template <typename T, size_t N>");
        emitHeaderLine("using SArray = std::array<T, N>;");
        emitHeaderLine();
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Dict = std::unordered_map<K, V>;");
        emitHeaderLine();
        emitHeaderLine("template <typename K, typename V>");
        emitHeaderLine("using Tree = std::map<K, V>;");
        dedent();
        emitHeaderLine("}");
        emitHeaderLine();

    }

    void CppGenerator::emitModuleApiTable(const Ref<Program>& program)
    {
        if (!program)
            return;

        emitGeneratedDirective();

        ModuleLifecycleFunctions lifecycleFunctions;
        collectModuleLifecycleFunctions(program->statements, lifecycleFunctions);
        std::vector<ExportedFunctionInfo> exportedFunctions;
        collectExportedFunctions(program->statements, exportedFunctions);
        std::unordered_map<const sema::StructType*, const ObjectDeclaration*> objectDeclarations;
        std::unordered_map<const sema::StructType*, const ComponentDeclaration*> componentDeclarations;
        indexStructDeclarations(program->statements, objectDeclarations, componentDeclarations);
        std::vector<ExportedTypeInfo> exportedTypes;
        collectExportedTypes(program->statements, exportedFunctions, exportedTypes, objectDeclarations, componentDeclarations);

        if (!lifecycleFunctions.hasAny() && exportedFunctions.empty())
            return;

        std::uint32_t capabilities = 0;
        if (lifecycleFunctions.apiVersion) capabilities |= (1u << 0);
        if (lifecycleFunctions.load) capabilities |= (1u << 1);
        if (lifecycleFunctions.update) capabilities |= (1u << 2);
        if (lifecycleFunctions.unload) capabilities |= (1u << 3);
        if (lifecycleFunctions.saveState) capabilities |= (1u << 4);
        if (lifecycleFunctions.restoreState) capabilities |= (1u << 5);
        std::uint32_t stateSchemaVersion = (lifecycleFunctions.saveState || lifecycleFunctions.restoreState) ? 1u : 0u;

        auto isInvokeCompatibleExport = [&](const ExportedFunctionInfo& exportInfo) -> bool
        {
            auto exportFunctionType = exportInfo.functionType;
            if (!exportFunctionType)
                return false;

            if (getAbiTypeEnumName(exportFunctionType->returnType) == "WIO_ABI_UNKNOWN")
                return false;

            for (const auto& parameterType : exportFunctionType->paramTypes)
            {
                if (getAbiTypeEnumName(parameterType) == "WIO_ABI_UNKNOWN")
                    return false;
            }

            return true;
        };

        auto emitSyntheticRawExport = [&](const ExportedFunctionInfo& exportInfo)
        {
            auto exportFunctionType = exportInfo.functionType;
            if (!exportFunctionType || exportInfo.syntheticKind == ExportedFunctionInfo::SyntheticKind::None)
                return;

            
            emitLine(common::formatString(
                "extern \"C\" WIO_EXPORT {} {}(",
                toCppType(exportFunctionType->returnType),
                exportInfo.symbolName
            ));
            for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
            {
                EMIT_TABS();
                emit(common::formatString(
                    "    {} _wio_arg{}",
                    toCppType(exportFunctionType->paramTypes[paramIndex]),
                    paramIndex
                ));
                if (paramIndex + 1 < exportFunctionType->paramTypes.size())
                    emit(",");
                emit("\n");
            }
            emitLine(")");
            emitLine("{");
            indent();

            switch (exportInfo.syntheticKind)
            {
            case ExportedFunctionInfo::SyntheticKind::TypeConstruct:
            {
                std::string constructorArguments;
                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    if (paramIndex > 0)
                        constructorArguments += ", ";
                    constructorArguments += "_wio_arg" + std::to_string(paramIndex);
                }

                if (exportInfo.ownerIsObject)
                {
                    emitLine(common::formatString(
                        "auto* instance = wio::runtime::Ref<{}>::Create({}).Detach();",
                        exportInfo.ownerCppTypeName,
                        constructorArguments
                    ));
                }
                else
                {
                    emitLine(common::formatString(
                        "auto* instance = new {}({});",
                        exportInfo.ownerCppTypeName,
                        constructorArguments
                    ));
                }
                emitLine("return reinterpret_cast<std::uintptr_t>(instance);");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeDestroy:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.ownerIsObject)
                    emitLine("wio::runtime::RefDeleter<" + exportInfo.ownerCppTypeName + ">::Execute(instance);");
                else
                    emitLine("delete instance;");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeFieldGet:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                    emitLine("return instance->" + exportInfo.memberCppName + ";");
                else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    emitLine("return reinterpret_cast<std::uintptr_t>(instance->" + exportInfo.memberCppName + ".Get());");
                else
                    emitLine("return reinterpret_cast<std::uintptr_t>(&instance->" + exportInfo.memberCppName + ");");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeFieldSet:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                    emitLine("instance->" + exportInfo.memberCppName + " = _wio_arg1;");
                else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    emitLine("instance->" + exportInfo.memberCppName + " = wio::runtime::Ref<" + exportInfo.memberCppTypeName + ">(reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(_wio_arg1));");
                else
                    emitLine("instance->" + exportInfo.memberCppName + " = *reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(_wio_arg1);");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::TypeMethod:
            {
                emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(_wio_arg0);");
                std::string callExpression = "instance->" + exportInfo.memberCppName + "(";
                for (size_t paramIndex = 1; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    if (paramIndex > 1)
                        callExpression += ", ";
                    callExpression += "_wio_arg" + std::to_string(paramIndex);
                }
                callExpression += ")";

                if (exportFunctionType->returnType->isVoid())
                    emitLine(callExpression + ";");
                else
                    emitLine("return " + callExpression + ";");
                break;
            }
            case ExportedFunctionInfo::SyntheticKind::None:
                break;
            }

            dedent();
            emitLine("}");
        };

        emitLine();
        for (size_t i = 0; i < exportedFunctions.size(); ++i)
        {
            const auto& exportInfo = exportedFunctions[i];
            auto exportFunctionType = exportInfo.functionType;

            emitSyntheticRawExport(exportInfo);

            if (isInvokeCompatibleExport(exportInfo))
            {
                emitLine("static std::int32_t WIO_INVOKE_EXPORT_" + std::to_string(i) + "(const WioValue* args, std::uint32_t argCount, WioValue* outResult)");
                emitLine("{");
                indent();
                emitLine("if (argCount != " + std::to_string(exportFunctionType->paramTypes.size()) + "u) return WIO_INVOKE_BAD_ARGUMENTS;");
                if (!exportFunctionType->paramTypes.empty())
                    emitLine("if (args == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");

                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    emitLine(
                        "if (args[" + std::to_string(paramIndex) + "].type != " +
                        getAbiTypeEnumName(exportFunctionType->paramTypes[paramIndex]) +
                        ") return WIO_INVOKE_TYPE_MISMATCH;"
                    );
                }

                if (!exportFunctionType->returnType->isVoid())
                    emitLine("if (outResult == nullptr) return WIO_INVOKE_RESULT_REQUIRED;");

                switch (exportInfo.syntheticKind)
                {
                case ExportedFunctionInfo::SyntheticKind::None:
                {
                    std::string callExpression = exportInfo.internalSymbol;
                    if (!exportInfo.templateArguments.empty())
                        callExpression += formatTemplateArgumentList(exportInfo.templateArguments);
                    callExpression += "(";
                    for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 0)
                            callExpression += ", ";

                        callExpression += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }
                    callExpression += ")";

                    if (exportFunctionType->returnType->isVoid())
                    {
                        emitLine(callExpression + ";");
                        emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    }
                    else
                    {
                        emitLine("auto result = " + callExpression + ";");
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                    }
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeConstruct:
                {
                    std::string constructorArguments;
                    for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 0)
                            constructorArguments += ", ";

                        constructorArguments += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }

                    if (exportInfo.ownerIsObject)
                        emitLine("auto* instance = wio::runtime::Ref<" + exportInfo.ownerCppTypeName + ">::Create(" + constructorArguments + ").Detach();");
                    else
                        emitLine("auto* instance = new " + exportInfo.ownerCppTypeName + "(" + constructorArguments + ");");
                    emitLine("outResult->type = WIO_ABI_USIZE;");
                    emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(instance);");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeDestroy:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.ownerIsObject)
                        emitLine("wio::runtime::RefDeleter<" + exportInfo.ownerCppTypeName + ">::Execute(instance);");
                    else
                        emitLine("delete instance;");
                    emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeFieldGet:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                    {
                        emitLine("auto result = instance->" + exportInfo.memberCppName + ";");
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                    }
                    else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                    {
                        emitLine("outResult->type = WIO_ABI_USIZE;");
                        emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(instance->" + exportInfo.memberCppName + ".Get());");
                    }
                    else
                    {
                        emitLine("outResult->type = WIO_ABI_USIZE;");
                        emitLine("outResult->value.v_usize = reinterpret_cast<std::uintptr_t>(&instance->" + exportInfo.memberCppName + ");");
                    }
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeFieldSet:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                    if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::Value)
                        emitLine("instance->" + exportInfo.memberCppName + " = args[1].value." + getAbiValueFieldName(exportFunctionType->paramTypes[1]) + ";");
                    else if (exportInfo.fieldAccessorKind == ExportedFunctionInfo::FieldAccessorKind::ObjectHandle)
                        emitLine("instance->" + exportInfo.memberCppName + " = wio::runtime::Ref<" + exportInfo.memberCppTypeName + ">(reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(args[1].value.v_usize));");
                    else
                        emitLine("instance->" + exportInfo.memberCppName + " = *reinterpret_cast<" + exportInfo.memberCppTypeName + "*>(args[1].value.v_usize);");
                    emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    break;
                }
                case ExportedFunctionInfo::SyntheticKind::TypeMethod:
                {
                    emitLine("auto* instance = reinterpret_cast<" + exportInfo.ownerCppTypeName + "*>(args[0].value.v_usize);");
                    emitLine("if (instance == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");

                    std::string callExpression = "instance->" + exportInfo.memberCppName + "(";
                    for (size_t paramIndex = 1; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                    {
                        if (paramIndex > 1)
                            callExpression += ", ";

                        callExpression += "args[" + std::to_string(paramIndex) + "].value." + getAbiValueFieldName(exportFunctionType->paramTypes[paramIndex]);
                    }
                    callExpression += ")";

                    if (exportFunctionType->returnType->isVoid())
                    {
                        emitLine(callExpression + ";");
                        emitLine("if (outResult != nullptr) outResult->type = WIO_ABI_VOID;");
                    }
                    else
                    {
                        emitLine("auto result = " + callExpression + ";");
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                    }
                    break;
                }
                }

                emitLine("return WIO_INVOKE_OK;");
                dedent();
                emitLine("}");
            }

            if (!exportFunctionType->paramTypes.empty())
            {
                emitLine("static const WioAbiType WIO_EXPORT_PARAM_TYPES_" + std::to_string(i) + "[] =");
                emitLine("{");
                indent();
                for (size_t paramIndex = 0; paramIndex < exportFunctionType->paramTypes.size(); ++paramIndex)
                {
                    std::string suffix = (paramIndex + 1 < exportFunctionType->paramTypes.size()) ? "," : "";
                    emitLine(getAbiTypeEnumName(exportFunctionType->paramTypes[paramIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }
        }

        if (!exportedFunctions.empty())
        {
            emitLine("static const WioModuleExport WIO_MODULE_EXPORTS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < exportedFunctions.size(); ++i)
            {
                const auto& exportInfo = exportedFunctions[i];
                auto exportFunctionType = exportInfo.functionType;
                const std::string invokeExpr = isInvokeCompatibleExport(exportInfo)
                    ? ("&WIO_INVOKE_EXPORT_" + std::to_string(i))
                    : "nullptr";
                std::string paramTypesExpr = exportFunctionType->paramTypes.empty()
                    ? "nullptr"
                    : ("WIO_EXPORT_PARAM_TYPES_" + std::to_string(i));
                std::string suffix = (i + 1 < exportedFunctions.size()) ? "," : "";

                emitLine(common::formatString(
                    R"({{ "{}", "{}", {}, {}u, {}, {}, reinterpret_cast<const void*>(&{}) }}{})",
                    common::wioStringToEscapedCppString(exportInfo.logicalName),
                    common::wioStringToEscapedCppString(exportInfo.symbolName),
                    getAbiTypeEnumName(exportFunctionType->returnType),
                    exportFunctionType->paramTypes.size(),
                    paramTypesExpr,
                    invokeExpr,
                    exportInfo.symbolName,
                    suffix
                ));
            }
            dedent();
            emitLine("};");
        }

        std::vector<size_t> commandExportIndices;
        std::vector<size_t> eventExportIndices;
        for (size_t i = 0; i < exportedFunctions.size(); ++i)
        {
            if (exportedFunctions[i].commandName.has_value())
                commandExportIndices.push_back(i);
            if (exportedFunctions[i].eventName.has_value())
                eventExportIndices.push_back(i);
        }

        if (!commandExportIndices.empty())
        {
            emitLine("static const WioModuleCommand WIO_MODULE_COMMANDS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < commandExportIndices.size(); ++i)
            {
                size_t exportIndex = commandExportIndices[i];
                std::string suffix = (i + 1 < commandExportIndices.size()) ? "," : "";
                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].commandName.value()) +
                    "\", &WIO_MODULE_EXPORTS[" + std::to_string(exportIndex) + "] }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        if (!eventExportIndices.empty())
        {
            emitLine("static const WioModuleEventHook WIO_MODULE_EVENT_HOOKS[] =");
            emitLine("{");
            indent();
            for (size_t i = 0; i < eventExportIndices.size(); ++i)
            {
                size_t exportIndex = eventExportIndices[i];
                std::string suffix = (i + 1 < eventExportIndices.size()) ? "," : "";
                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].logicalName) +
                    "\", \"" + common::wioStringToEscapedCppString(exportedFunctions[exportIndex].eventName.value()) +
                    "\", &WIO_MODULE_EXPORTS[" + std::to_string(exportIndex) + "] }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        struct TypeDescriptorEmissionInfo
        {
            std::string displayName;
            std::string logicalTypeName;
            std::string kindExpr = "WIO_MODULE_TYPE_DESC_UNKNOWN";
            std::string abiExpr = "WIO_ABI_UNKNOWN";
            std::uint64_t staticArraySize = 0u;
            std::optional<size_t> elementIndex;
            std::optional<size_t> keyIndex;
            std::optional<size_t> valueIndex;
            std::optional<size_t> returnIndex;
            std::vector<size_t> parameterIndices;
        };

        std::vector<TypeDescriptorEmissionInfo> emittedTypeDescriptors;
        std::unordered_map<std::string, size_t> emittedTypeDescriptorIndices;

        auto getLogicalTypeName = [](const Ref<sema::StructType>& structType) -> std::string
        {
            if (!structType)
                return {};

            return structType->scopePath.empty()
                ? structType->name
                : common::formatString("{}::{}", structType->scopePath, structType->name);
        };

        std::function<size_t(const Ref<sema::Type>&)> ensureTypeDescriptor = [&](const Ref<sema::Type>& type) -> size_t
        {
            const std::string key = type ? type->toString() : std::string("<unknown>");
            if (auto it = emittedTypeDescriptorIndices.find(key); it != emittedTypeDescriptorIndices.end())
                return it->second;

            TypeDescriptorEmissionInfo info;
            info.displayName = key;

            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
            {
                const size_t index = emittedTypeDescriptors.size();
                emittedTypeDescriptorIndices.emplace(key, index);
                emittedTypeDescriptors.push_back(std::move(info));
                return index;
            }

            switch (resolvedType->kind())
            {
            case sema::TypeKind::Primitive:
            {
                const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (primitiveName == "string")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_STRING";
                else
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_PRIMITIVE";
                info.abiExpr = getAbiTypeEnumName(resolvedType);
                break;
            }
            case sema::TypeKind::Array:
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                info.kindExpr = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? "WIO_MODULE_TYPE_DESC_STATIC_ARRAY"
                    : "WIO_MODULE_TYPE_DESC_DYNAMIC_ARRAY";
                info.staticArraySize = static_cast<std::uint64_t>(arrayType->size);
                info.elementIndex = ensureTypeDescriptor(arrayType->elementType);
                break;
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictType = resolvedType.AsFast<sema::DictionaryType>();
                info.kindExpr = dictType->isOrdered ? "WIO_MODULE_TYPE_DESC_TREE" : "WIO_MODULE_TYPE_DESC_DICT";
                info.keyIndex = ensureTypeDescriptor(dictType->keyType);
                info.valueIndex = ensureTypeDescriptor(dictType->valueType);
                break;
            }
            case sema::TypeKind::Function:
            {
                auto functionType = resolvedType.AsFast<sema::FunctionType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_FUNCTION";
                info.returnIndex = ensureTypeDescriptor(functionType->returnType);
                info.parameterIndices.reserve(functionType->paramTypes.size());
                for (const auto& parameterType : functionType->paramTypes)
                    info.parameterIndices.push_back(ensureTypeDescriptor(parameterType));
                break;
            }
            case sema::TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                info.logicalTypeName = getLogicalTypeName(structType);
                if (objectDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OBJECT";
                else if (componentDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_COMPONENT";
                else
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPAQUE";
                break;
            }
            default:
                info.kindExpr = "WIO_MODULE_TYPE_DESC_UNKNOWN";
                break;
            }

            const size_t index = emittedTypeDescriptors.size();
            emittedTypeDescriptorIndices.emplace(key, index);
            emittedTypeDescriptors.push_back(std::move(info));
            return index;
        };

        for (const auto& exportedType : exportedTypes)
        {
            for (const auto& field : exportedType.fields)
                ensureTypeDescriptor(field.fieldType);
        }

            if (!emittedTypeDescriptors.empty())
            {
            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
                emitLine("extern const WioModuleTypeDescriptor WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) + ";");

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                if (descriptor.parameterIndices.empty())
                    continue;

                emitLine("static const WioModuleTypeDescriptor* WIO_TYPE_DESCRIPTOR_PARAMS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t parameterIndex = 0; parameterIndex < descriptor.parameterIndices.size(); ++parameterIndex)
                {
                    const std::string suffix = (parameterIndex + 1 < descriptor.parameterIndices.size()) ? "," : "";
                    emitLine("&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptor.parameterIndices[parameterIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                const std::string logicalTypeExpr = descriptor.logicalTypeName.empty()
                    ? "nullptr"
                    : ("\"" + common::wioStringToEscapedCppString(descriptor.logicalTypeName) + "\"");
                const std::string elementExpr = descriptor.elementIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.elementIndex))
                    : "nullptr";
                const std::string keyExpr = descriptor.keyIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.keyIndex))
                    : "nullptr";
                const std::string valueExpr = descriptor.valueIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.valueIndex))
                    : "nullptr";
                const std::string returnExpr = descriptor.returnIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.returnIndex))
                    : "nullptr";
                const std::string paramExpr = descriptor.parameterIndices.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_PARAMS_" + std::to_string(descriptorIndex));

                emitLine(
                    "const WioModuleTypeDescriptor WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                    " = { \"" + common::wioStringToEscapedCppString(descriptor.displayName) +
                    "\", " + logicalTypeExpr +
                    ", " + descriptor.kindExpr +
                    ", " + descriptor.abiExpr +
                    ", " + std::to_string(descriptor.staticArraySize) + "ull, " +
                    elementExpr + ", " + keyExpr + ", " + valueExpr + ", " + returnExpr +
                    ", " + std::to_string(descriptor.parameterIndices.size()) + "u, " + paramExpr + " };"
                );
                }
            }

            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
                const auto& exportedType = exportedTypes[typeIndex];
                for (const auto& field : exportedType.fields)
                {
                    if (!field.dynamicGetterSymbol.has_value())
                        continue;

                    const size_t descriptorIndex = ensureTypeDescriptor(field.fieldType);
                    emitLine(
                        "static WioErasedValue* " + *field.dynamicGetterSymbol + "(std::uintptr_t handle)"
                    );
                    emitLine("{");
                    indent();
                    emitLine("auto* instance = reinterpret_cast<" + exportedType.cppTypeName + "*>(handle);");
                    emitLine("if (instance == nullptr) return nullptr;");
                    emitLine(
                        "return new WioErasedValueModel<" + field.memberCppTypeName + ">(" +
                        "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                        ", instance->" + field.memberCppName + ");"
                    );
                    dedent();
                    emitLine("}");
                    emitLine();

                    if (field.dynamicSetterSymbol.has_value())
                    {
                        emitLine(
                            "static std::int32_t " + *field.dynamicSetterSymbol +
                            "(std::uintptr_t handle, const WioErasedValue* value)"
                        );
                        emitLine("{");
                        indent();
                        emitLine("auto* instance = reinterpret_cast<" + exportedType.cppTypeName + "*>(handle);");
                        emitLine("if (instance == nullptr || value == nullptr) return WIO_INVOKE_BAD_ARGUMENTS;");
                        emitLine(
                            "auto* typedValue = dynamic_cast<const WioErasedValueModel<" + field.memberCppTypeName + ">*>(value);"
                        );
                        emitLine("if (typedValue == nullptr) return WIO_INVOKE_TYPE_MISMATCH;");
                        emitLine("instance->" + field.memberCppName + " = typedValue->value;");
                        emitLine("return WIO_INVOKE_OK;");
                        dedent();
                        emitLine("}");
                        emitLine();
                    }
                }
            }

            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
            const auto& exportedType = exportedTypes[typeIndex];

            if (!exportedType.constructors.empty())
            {
                emitLine("static const WioModuleConstructor WIO_MODULE_TYPE_CONSTRUCTORS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t constructorIndex = 0; constructorIndex < exportedType.constructors.size(); ++constructorIndex)
                {
                    const auto& constructor = exportedType.constructors[constructorIndex];
                    const std::string suffix = (constructorIndex + 1 < exportedType.constructors.size()) ? "," : "";
                    emitLine(
                        "{ &WIO_MODULE_EXPORTS[" + std::to_string(constructor.exportIndex) + "] }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }

            if (!exportedType.fields.empty())
            {
                emitLine("static const WioModuleField WIO_MODULE_TYPE_FIELDS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t fieldIndex = 0; fieldIndex < exportedType.fields.size(); ++fieldIndex)
                {
                    const auto& field = exportedType.fields[fieldIndex];
                    const std::uint32_t flags = 1u | (field.isReadOnly ? 4u : 2u);
                    const std::string suffix = (fieldIndex + 1 < exportedType.fields.size()) ? "," : "";
                    const std::string accessModifierExpr =
                        field.accessModifier == AccessModifier::Protected ? "WIO_MODULE_ACCESS_PROTECTED" :
                        field.accessModifier == AccessModifier::Private ? "WIO_MODULE_ACCESS_PRIVATE" :
                        "WIO_MODULE_ACCESS_PUBLIC";
                    const std::string setterExportExpr = field.setterExportIndex.has_value()
                        ? ("&WIO_MODULE_EXPORTS[" + std::to_string(*field.setterExportIndex) + "]")
                        : "nullptr";
                    const std::string dynamicGetterExpr = field.dynamicGetterSymbol.has_value()
                        ? ("&" + *field.dynamicGetterSymbol)
                        : "nullptr";
                    const std::string dynamicSetterExpr = field.dynamicSetterSymbol.has_value()
                        ? ("&" + *field.dynamicSetterSymbol)
                        : "nullptr";
                    const std::string typeDescriptorExpr = field.fieldType
                        ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(ensureTypeDescriptor(field.fieldType)))
                        : "nullptr";

                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(field.fieldName) +
                        "\", " + getAbiTypeEnumName(field.fieldType) +
                        ", " + typeDescriptorExpr +
                        ", " + std::to_string(flags) + "u, " + accessModifierExpr +
                        ", &WIO_MODULE_EXPORTS[" + std::to_string(field.getterExportIndex) +
                        "], " + setterExportExpr + ", " + dynamicGetterExpr + ", " + dynamicSetterExpr + " }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }

            if (!exportedType.methods.empty())
            {
                emitLine("static const WioModuleMethod WIO_MODULE_TYPE_METHODS_" + std::to_string(typeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t methodIndex = 0; methodIndex < exportedType.methods.size(); ++methodIndex)
                {
                    const auto& method = exportedType.methods[methodIndex];
                    const std::string suffix = (methodIndex + 1 < exportedType.methods.size()) ? "," : "";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(method.methodName) +
                        "\", &WIO_MODULE_EXPORTS[" + std::to_string(method.exportIndex) + "] }" + suffix
                    );
                }
                dedent();
                emitLine("};");
            }
        }

        if (!exportedTypes.empty())
        {
            emitLine("static const WioModuleType WIO_MODULE_TYPES[] =");
            emitLine("{");
            indent();
            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
                const auto& exportedType = exportedTypes[typeIndex];
                const std::string suffix = (typeIndex + 1 < exportedTypes.size()) ? "," : "";
                const std::string typeKind = exportedType.isObject ? "WIO_MODULE_TYPE_OBJECT" : "WIO_MODULE_TYPE_COMPONENT";
                const std::string createExportExpr = exportedType.createExportIndex.has_value()
                    ? ("&WIO_MODULE_EXPORTS[" + std::to_string(*exportedType.createExportIndex) + "]")
                    : "nullptr";
                const std::string constructorArrayExpr = exportedType.constructors.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_CONSTRUCTORS_" + std::to_string(typeIndex));
                const std::string fieldArrayExpr = exportedType.fields.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_FIELDS_" + std::to_string(typeIndex));
                const std::string methodArrayExpr = exportedType.methods.empty()
                    ? "nullptr"
                    : ("WIO_MODULE_TYPE_METHODS_" + std::to_string(typeIndex));

                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedType.logicalName) +
                    "\", \"" + common::wioStringToEscapedCppString(exportedType.symbolName) +
                    "\", " + typeKind +
                    ", " + createExportExpr +
                    ", &WIO_MODULE_EXPORTS[" + std::to_string(exportedType.destroyExportIndex) +
                    "], " + std::to_string(exportedType.constructors.size()) + "u, " + constructorArrayExpr +
                    ", " + std::to_string(exportedType.fields.size()) + "u, " + fieldArrayExpr +
                    ", " + std::to_string(exportedType.methods.size()) + "u, " + methodArrayExpr + " }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        emitLine("extern \"C\" WIO_EXPORT const WioModuleApi* WioModuleGetApi()");
        emitLine("{");
        indent();

        emitLine("static const WioModuleApi API =");
        emitLine("{");
        indent();
        emitLine("WIO_MODULE_API_DESCRIPTOR_VERSION,");
        emitLine(std::to_string(capabilities) + "u,");
        emitLine(std::to_string(stateSchemaVersion) + "u,");
        emitLine("0u,");
        emitLine(lifecycleFunctions.apiVersion ? "&WioModuleApiVersion," : "nullptr,");
        emitLine(lifecycleFunctions.load ? "&WioModuleLoad," : "nullptr,");
        emitLine(lifecycleFunctions.update ? "&WioModuleUpdate," : "nullptr,");
        emitLine(lifecycleFunctions.saveState ? "&WioModuleSaveState," : "nullptr,");
        emitLine(lifecycleFunctions.restoreState ? "&WioModuleRestoreState," : "nullptr,");
        emitLine(lifecycleFunctions.unload ? "&WioModuleUnload," : "nullptr,");
        emitLine(std::to_string(exportedFunctions.size()) + "u,");
        emitLine(exportedFunctions.empty() ? "nullptr," : "WIO_MODULE_EXPORTS,");
        emitLine(std::to_string(commandExportIndices.size()) + "u,");
        emitLine(commandExportIndices.empty() ? "nullptr," : "WIO_MODULE_COMMANDS,");
        emitLine(std::to_string(eventExportIndices.size()) + "u,");
        emitLine(eventExportIndices.empty() ? "nullptr," : "WIO_MODULE_EVENT_HOOKS,");
        emitLine(std::to_string(exportedTypes.size()) + "u,");
        emitLine(exportedTypes.empty() ? "nullptr" : "WIO_MODULE_TYPES");
        dedent();
        emitLine("};");
        emitLine("return &API;");
        dedent();
        emitLine("}");
    }

    void CppGenerator::emit(const std::string& str)
    {
        buffer_ << str;
    }

    void CppGenerator::emitLine(const std::string& str)
    {
        EMIT_TABS();
        buffer_ << str << "\n";
    }

    void CppGenerator::emitHeader(const std::string& str)
    {
        header_ << str << "\n";
    }

    void CppGenerator::emitHeaderLine(const std::string& str)
    {
        for (int i = 0; i < indentationLevel_; ++i) header_ << "    ";
        header_ << str << "\n";
    }

    void CppGenerator::emitSourceDirective(const common::Location& loc)
    {
        if (!loc.hasSourceContext())
            return;

        emitLine("#line " + std::to_string(loc.line) + " \"" + common::wioStringToEscapedCppString(loc.file) + "\"");
    }

    void CppGenerator::emitGeneratedDirective()
    {
        emitLine("#line 1 \"<wio-generated>\"");
    }

    void CppGenerator::emitMain(FunctionDeclaration& node)
    {
        auto lockedRefType = node.returnType->refType.Lock();
        
        if (lockedRefType->toString() != "i32" && lockedRefType->toString() != "void")
        {
            throw InvalidEntryReturnType("Entry return type must be i32 or void.", node.location());
        }
        if(!node.body)
        {
            throw MissedEntryBody("The Entry function must have a body.", node.location());
        }

        emitGeneratedDirective();
        emitLine();
        
        std::string paramName;
        bool hasArgs = false;

        if (node.parameters.empty())
        {
            emitLine("int main() {");
        }
        else
        {
            if (node.parameters.size() > 1)
                throw InvalidEntryParameter("The `Entry` function should have only one parameter. (string[])", node.location());

            Parameter& param = node.parameters.front();
            auto lockedParamRefType = param.name->refType.Lock();
            if (lockedParamRefType->toString() != "string[]")
            {
                throw InvalidEntryParameter("The `Entry` function's parameter type must be `string[]`", node.location());
            }

            paramName = sanitizeCppIdentifier(param.name->token.value);
            hasArgs = true;
            
            emitLine("int main(int argc, char** argv) {");
        }
        indent();

        if (hasArgs)
        {
            emitLine("wio::DArray<wio::String> " + paramName + ";");
            emitLine(paramName + ".reserve(argc);");
            emitLine("for (int i = 0; i < argc; ++i) {");
            indent();
            emitLine(paramName + ".push_back(wio::String(argv[i]));");
            dedent();
            emitLine("}");
            emitLine("");
        }

        emitLine("try {");
        indent();
        
        if (node.body->is<BlockStatement>())
        {
            auto block = node.body->as<BlockStatement>();
            for (auto& stmt : block->statements)
                stmt->accept(*this);
        }
        else
        {
            node.body->accept(*this);
        }
        
        if (lockedRefType->toString() == "void")
            emitLine("return 0;");

        dedent();
        emitLine("}");
        emitLine("catch (const wio::runtime::RuntimeException& ex)"); 
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: " << ex.what() << '\n';)"); 
        emitLine("return 1;");
        dedent();
        emitLine("}"); 
        
        emitLine("return 0;"); 
        dedent();
        emitLine("}");
    }
    
    void CppGenerator::indent() { indentationLevel_++; }
    void CppGenerator::dedent() { indentationLevel_--; }

    void CppGenerator::emitStatements(const std::vector<NodePtr<Statement>>& statements)
    {
        auto emitPhase = [&](auto&& self, const std::vector<NodePtr<Statement>>& group, const auto& emitter) -> void
        {
            for (const auto& stmt : group)
            {
                if (stmt->is<RealmDeclaration>())
                {
                    self(self, stmt->as<RealmDeclaration>()->statements, emitter);
                    continue;
                }

                emitter(stmt);
            }
        };

        auto emitTemplateForwardDeclarationPrefix = [&](const std::vector<NodePtr<Identifier>>& genericParameters)
        {
            if (genericParameters.empty())
                return;

            std::string templateLine = "template <";
            for (size_t i = 0; i < genericParameters.size(); ++i)
            {
                templateLine += "typename " + genericParameters[i]->token.value;
                if (i + 1 < genericParameters.size())
                    templateLine += ", ";
            }
            templateLine += ">";
            emitLine(templateLine);
        };

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<UseStatement>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<EnumDeclaration>() || stmt->template is<FlagsetDeclaration>() || stmt->template is<FlagDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<ComponentDeclaration>())
            {
                auto declaration = stmt->template as<ComponentDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                emitLine(std::format("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
            }
            else if (stmt->template is<ObjectDeclaration>())
            {
                auto declaration = stmt->template as<ObjectDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                emitLine(std::format("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
            }
            else if (stmt->template is<InterfaceDeclaration>())
            {
                auto declaration = stmt->template as<InterfaceDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                emitLine(std::format("struct {};", Mangler::mangleInterface(declaration->name->token.value, sym ? sym->scopePath : "")));
            }
        });

        isEmittingPrototypes_ = true;
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
        });
        isEmittingPrototypes_ = false;

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<InterfaceDeclaration>() || stmt->template is<ComponentDeclaration>() || stmt->template is<ObjectDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<VariableDeclaration>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
        });
    }
    
    void CppGenerator::visit(Program& node)
    {
        emitStatements(node.statements);
    }

    void CppGenerator::visit(TypeSpecifier& node)
    {
        if (node.name.type == TokenType::kwFn)
        {//
            emit("std::function<");
            node.generics[0]->accept(*this);
            emit("(");
            
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                if (i < node.generics.size() - 1) emit(", ");
            }
            
            emit(")>");
            return;
        }

        if (auto resolvedType = node.refType.Lock())
        {
            emit(toCppType(resolvedType));
            return;
        }

        emit(node.name.value);
    }

    void CppGenerator::visit(BinaryExpression& node)
    {
        auto emitReadableExpression = [&](const NodePtr<Expression>& expression)
        {
            const std::size_t derefCount = getAutoReadableReferenceDepth(expression ? expression->refType.Lock() : nullptr);
            for (std::size_t i = 0; i < derefCount; ++i)
                emit("*(");

            expression->accept(*this);

            for (std::size_t i = 0; i < derefCount; ++i)
                emit(")");
        };

        if (node.op.type == TokenType::kwIn)
        {
            if (node.right->is<RangeExpression>())
            {
                auto range = node.right->as<RangeExpression>();
                // C++ Output: [&](){ auto _val = (x); return _val >= (1) && _val <= (5); }()
                emit("[&](){ auto _val = (");
                emitReadableExpression(node.left);
                emit("); return _val >= (");
                range->start->accept(*this);
                emit(range->isInclusive ? ") && _val <= (" : ") && _val < (");
                range->end->accept(*this);
                emit("); }() ");
                return;
            }
        }

        if (node.op.type == TokenType::kwIs)
        {
            node.left->accept(*this);
            
            bool isFatPointer = false;
            if (const auto& lhsType = node.left->refType.Lock())
            {
                auto baseType = lhsType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                    
                if (baseType->kind() == sema::TypeKind::Struct &&
                    baseType.AsFast<sema::StructType>()->isInterface)
                {
                    isFatPointer = true;
                }
                else if (baseType->kind() == sema::TypeKind::Reference)
                {
                    auto rType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    if (rType && rType->kind() == sema::TypeKind::Struct &&
                        rType.AsFast<sema::StructType>()->isInterface)
                        isFatPointer = true;
                }
            }

            emit(isFatPointer ? "._WF_IsA(" : "->_WF_IsA(");

            auto rightSym = node.right->referencedSymbol.Lock();
            if (rightSym && rightSym->kind == sema::SymbolKind::Struct)
            {
                auto sType = rightSym->type.AsFast<sema::StructType>();
                if (sType->isInterface) 
                    emit(mangleInterfaceTypeName(sType) + "::TYPE_ID");
                else 
                    emit(mangleStructTypeName(sType) + "::TYPE_ID");
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "{} not a type!", node.right->kindName());
                emit("0 /* Error: Not a type! */");
            }
            emit(")");
            return;//
        }
        
        emit("(");
        emitReadableExpression(node.left);
        
        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwAnd)
            opStr = "&&";
        else if (node.op.type == TokenType::kwOr)
            opStr = "||";
        
        emit(" " + opStr + " ");
        emitReadableExpression(node.right);
        emit(")");
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwNot) opStr = "!";
        
        if (node.opType == UnaryExpression::UnaryOperatorType::Prefix)
        {
            emit(opStr);
            node.operand->accept(*this);
        }
        else
        {
            node.operand->accept(*this);
            emit(opStr);
        }
    }

    void CppGenerator::visit(AssignmentExpression& node)
    {
        int derefCount = 0;

        auto lhsType = node.left->refType.Lock();
        auto rhsType = node.right->refType.Lock();

        if (lhsType && rhsType && !lhsType->isCompatibleWith(rhsType))
        {
            Ref<sema::Type> current = lhsType;
        
            while (current && current->kind() == sema::TypeKind::Reference)
            {
                auto rType = current.AsFast<sema::ReferenceType>();
                derefCount++;
            
                if (rType->referredType->isCompatibleWith(rhsType)) {
                    break;
                }
                current = rType->referredType;
            }
        }

        for (int i = 0; i < derefCount; ++i) emit("*(");
    
        node.left->accept(*this);
    
        for (int i = 0; i < derefCount; ++i) emit(")");

        emit(" " + node.op.value + " "); // =, +=, -= ...
    
        node.right->accept(*this);
    }

    void CppGenerator::visit(IntegerLiteral& node)
    {
        std::string valStr = common::stripIntegerLiteralTypeSuffix(node.token.value);

        auto type = node.refType.Lock();
        std::string tName = type ? type->toString() : "i32";

        if (tName == "u32")
            emit(valStr + "u");
        else if (tName == "i64" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<int64_t>::min)()");
        else if (tName == "isize" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<ptrdiff_t>::min)()");
        else if (tName == "i64")
            emit(valStr + "ll");
        else if (tName == "u64" || tName == "usize")
            emit(valStr + "ull");
        else if (tName == "i8" || tName == "u8" || tName == "i16" || tName == "u16")
            emit("static_cast<" + type->toCppString() + ">(" + valStr + ")");
        else
            emit(valStr);
    }

    void CppGenerator::visit(FloatLiteral& node)
    {
        std::string valStr = common::stripFloatLiteralTypeSuffix(node.token.value);

        auto type = node.refType.Lock();
        if (type && type->toString() == "f64")
            emit(valStr); 
        else
            emit(valStr + "f"); 
    }

        void CppGenerator::visit(StringLiteral& node)
    {
        emit("\"" + common::wioStringToEscapedCppString(node.token.value) + "\"");
    }
    
    void CppGenerator::visit(InterpolatedStringLiteral& node)
    {
        if (node.parts.empty())
        {
            emit("\"\"");
            return;
        }

        std::string formatString;
        std::vector<Ref<Expression>> arguments;

        for (auto& part : node.parts)
        {
            if (auto strLiteral = part.As<StringLiteral>()) 
            {
                formatString += common::wioStringToEscapedCppString(strLiteral->token.value);
            }
            else 
            {
                formatString += "{}";
                arguments.push_back(part);
            }
        }

        emit("std::format(\"" + formatString + "\"");

        for (auto& arg : arguments)
        {
            emit(", ");
        
            int derefCount = 0;
            Ref<sema::Type> currentType = arg->refType.Lock();
        
            while (currentType && currentType->kind() == sema::TypeKind::Reference)
            {
                derefCount++;
                currentType = currentType.AsFast<sema::ReferenceType>()->referredType;
            }

            for (int i = 0; i < derefCount; ++i) emit("*(");
        
            arg->accept(*this);
        
            for (int i = 0; i < derefCount; ++i) emit(")");
        }
    
        emit(")");
    }

    void CppGenerator::visit(BoolLiteral& node)
    {
        emit(node.token.value);
    }
    
    void CppGenerator::visit(CharLiteral& node)
    {
        emit("\'" + common::wioStringToEscapedCppString(node.token.value) + "\'");
    }
    
    void CppGenerator::visit(ByteLiteral& node)
    {
        emit("static_cast<uint8_t>(" + node.token.value + ")");
    }
    
    void CppGenerator::visit(DurationLiteral& node)
    {
        std::string valStr = node.token.value; 
        double multiplier = 1.0;
        std::string numPart;
    
        if (valStr.ends_with("ms"))      { multiplier = 0.001;       numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("us")) { multiplier = 0.000001;    numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("ns")) { multiplier = 0.000000001; numPart = valStr.substr(0, valStr.size() - 2); }
        else if (valStr.ends_with("s"))  { multiplier = 1.0;         numPart = valStr.substr(0, valStr.size() - 1); }
        else if (valStr.ends_with("m"))  { multiplier = 60.0;        numPart = valStr.substr(0, valStr.size() - 1); }
        else if (valStr.ends_with("h"))  { multiplier = 3600.0;      numPart = valStr.substr(0, valStr.size() - 1); }
        else { numPart = valStr; }

        double value = std::stod(numPart) * multiplier;
        emit(std::to_string(value) + "f");
    }
    
    void CppGenerator::visit(ArrayLiteral& node)
    {
        const bool nested = !node.elements.empty() && node.elements[0].As<ArrayLiteral>();
        
        if (nested)
            emit("{");
        
        emit("{");
        for (size_t i = 0; i < node.elements.size(); ++i)
        {
            node.elements[i]->accept(*this);
            if (i < node.elements.size() - 1) emit(", ");
        }
        emit("}");

        if (nested)
            emit("}");
    }
    
    void CppGenerator::visit(DictionaryLiteral& node)
    {
        emit("{");
        for (size_t i = 0; i < node.pairs.size(); ++i)
        {
            emit("{ ");
            node.pairs[i].first->accept(*this);
            emit(", ");
            node.pairs[i].second->accept(*this);
            emit(" }");
            
            if (i < node.pairs.size() - 1) emit(", ");
        }
        emit("}");
    }

    void CppGenerator::visit(Identifier& node)
    {
        if (auto sym = node.referencedSymbol.Lock())
        {
            if (sym->kind == sema::SymbolKind::Function)
            {
                auto funcType = sym->type.AsFast<sema::FunctionType>();
                emit(Mangler::mangleFunction(sym->name, funcType->paramTypes, sym->scopePath));
                return;
            }

            if (sym->kind == sema::SymbolKind::FunctionGroup)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                {
                    if (!sym->overloads.empty())
                        selectedType = sym->overloads.front()->type;
                }

                std::string scopePath = sym->scopePath;
                if (scopePath.empty() && !sym->overloads.empty())
                    scopePath = sym->overloads.front()->scopePath;

                if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                {
                    auto funcType = selectedType.AsFast<sema::FunctionType>();
                    emit(Mangler::mangleFunction(sym->name, funcType->paramTypes, scopePath));
                    return;
                }
            }
            
            if (sym->kind == sema::SymbolKind::Variable && sym->flags.get_isGlobal())
            {
                emit(Mangler::mangleGlobalVar(sym->name, sym->scopePath));
                return;
            }

            if (sym->kind == sema::SymbolKind::Struct)
            {
                emit(Mangler::mangleStruct(sym->name, sym->scopePath));
                return;
            }
        }

        emit(sanitizeCppIdentifier(node.token.value));
    }

    void CppGenerator::visit(NullExpression& node)
    {
        auto lockedRefType = node.refType.Lock();
        if (lockedRefType && lockedRefType->kind() == sema::TypeKind::Null)
        {
            emit(lockedRefType.AsFast<sema::NullType>()->transformedType->toCppString());
            emit("{}");
        }
        else
        {
            emit("nullptr");
        }
    }

    void CppGenerator::visit(ArrayAccessExpression& node)
    {
        // Wio: arr[0] -> C++: arr[0]
        node.object->accept(*this);
        emit("[");
        node.index->accept(*this);
        emit("]");
    }

    bool CppGenerator::emitIntrinsicMemberAccess(MemberAccessExpression& node)
    {
        if (node.intrinsicMember == IntrinsicMember::None)
            return false;

        auto functionType = node.refType.Lock();
        if (!functionType || functionType->kind() != sema::TypeKind::Function)
            return false;

        std::size_t referenceDepth = 0;
        Ref<sema::Type> receiverType = node.object->refType.Lock();
        while (receiverType && receiverType->kind() == sema::TypeKind::Alias)
            receiverType = receiverType.AsFast<sema::AliasType>()->aliasedType;
        while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
        {
            ++referenceDepth;
            receiverType = receiverType.AsFast<sema::ReferenceType>()->referredType;
            while (receiverType && receiverType->kind() == sema::TypeKind::Alias)
                receiverType = receiverType.AsFast<sema::AliasType>()->aliasedType;
        }

        auto emitReceiver = [&]()
        {
            if (referenceDepth == 0)
            {
                node.object->accept(*this);
                return;
            }

            emit("(");
            for (std::size_t i = 0; i < referenceDepth; ++i)
                emit("*");
            emit("(");
            node.object->accept(*this);
            emit("))");
        };

        auto emitArgumentName = [&](std::size_t index)
        {
            emit("_wio_arg" + std::to_string(index));
        };

        auto intrinsicFunctionType = functionType.AsFast<sema::FunctionType>();
        const std::string_view helperName = getIntrinsicHelperName(node.intrinsicMember);
        if (helperName.empty())
            return false;

        emit("([&](");
        for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
        {
            emit(toCppType(intrinsicFunctionType->paramTypes[i]));
            emit(" _wio_arg" + std::to_string(i));
            if (i + 1 < intrinsicFunctionType->paramTypes.size())
                emit(", ");
        }
        emit(") -> ");
        emit(toCppType(intrinsicFunctionType->returnType));
        emit(" { ");

        if (intrinsicFunctionType->returnType && !intrinsicFunctionType->returnType->isVoid())
            emit("return ");

        emit("wio::intrinsics::");
        emit(std::string(helperName));
        emit("(");
        emitReceiver();

        for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
        {
            emit(", ");
            emitArgumentName(i);
        }
        emit(")");
        emit("; })");
        return true;
    }

    void CppGenerator::visit(MemberAccessExpression& node)
    {
        if (emitIntrinsicMemberAccess(node))
            return;

        auto getSelectedMemberSymbol = [&]() -> Ref<sema::Symbol>
        {
            if (auto selectedMember = node.referencedSymbol.Lock())
                return selectedMember;

            return node.member->referencedSymbol.Lock();
        };

        auto emitMemberName = [&]()
        {
            if (auto memberSym = getSelectedMemberSymbol();
                memberSym && memberSym->kind == sema::SymbolKind::Function)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = node.member->refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = memberSym->type;

                auto funcType = selectedType ? selectedType.AsFast<sema::FunctionType>() : nullptr;
                if (!funcType)
                    funcType = memberSym->type.AsFast<sema::FunctionType>();

                if (funcType &&
                    memberSym->type && memberSym->type->kind() == sema::TypeKind::Function)
                {
                    auto declarationFunctionType = memberSym->type.AsFast<sema::FunctionType>();
                    if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                    {
                        funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            declarationFunctionType->returnType,
                            getLeadingParameterTypes(declarationFunctionType, funcType->paramTypes.size())
                        ).AsFast<sema::FunctionType>();
                    }
                }

                emit(Mangler::mangleFunction(memberSym->name, funcType->paramTypes));
                return;
            }

            if (auto memberSym = getSelectedMemberSymbol();
                memberSym && memberSym->kind == sema::SymbolKind::FunctionGroup)
            {
                auto selectedType = node.refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                    selectedType = node.member->refType.Lock();
                if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                {
                    if (!memberSym->overloads.empty())
                        selectedType = memberSym->overloads.front()->type;
                }

                std::string scopePath = memberSym->scopePath;
                if (scopePath.empty() && !memberSym->overloads.empty())
                    scopePath = memberSym->overloads.front()->scopePath;

                if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                {
                    auto funcType = selectedType.AsFast<sema::FunctionType>();
                    if (!memberSym->overloads.empty() &&
                        memberSym->overloads.front()->type &&
                        memberSym->overloads.front()->type->kind() == sema::TypeKind::Function)
                    {
                        auto declarationFunctionType = memberSym->overloads.front()->type.AsFast<sema::FunctionType>();
                        if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                        {
                            funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                declarationFunctionType->returnType,
                                getLeadingParameterTypes(declarationFunctionType, funcType->paramTypes.size())
                            ).AsFast<sema::FunctionType>();
                        }
                    }
                    emit(Mangler::mangleFunction(memberSym->name, funcType->paramTypes));
                    return;
                }
            }

            if (auto memberSym = node.member->referencedSymbol.Lock();
                memberSym && memberSym->kind == sema::SymbolKind::Struct)
            {
                emit(Mangler::mangleStruct(memberSym->name, memberSym->scopePath));
                return;
            }

            node.member->accept(*this);
        };

        if (auto objSym = node.object->referencedSymbol.Lock();
            objSym && objSym->kind == sema::SymbolKind::Namespace)
        {
            if (auto memberSym = getSelectedMemberSymbol())
            {
                if (memberSym->kind == sema::SymbolKind::Function)
                {
                    auto funcType = node.refType.Lock();
                    if (!funcType || funcType->kind() != sema::TypeKind::Function)
                        funcType = node.member->refType.Lock();
                    if (!funcType || funcType->kind() != sema::TypeKind::Function)
                        funcType = memberSym->type;

                    if (funcType && funcType->kind() == sema::TypeKind::Function)
                    {
                        auto mangledFunctionType = funcType.AsFast<sema::FunctionType>();
                        if (memberSym->type &&
                            memberSym->type->kind() == sema::TypeKind::Function)
                        {
                            auto declarationFunctionType = memberSym->type.AsFast<sema::FunctionType>();
                            if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                            {
                                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                    declarationFunctionType->returnType,
                                    getLeadingParameterTypes(declarationFunctionType, mangledFunctionType->paramTypes.size())
                                ).AsFast<sema::FunctionType>();
                            }
                        }

                        emit(Mangler::mangleFunction(memberSym->name, mangledFunctionType->paramTypes, memberSym->scopePath));
                        return;
                    }
                }
                else if (memberSym->kind == sema::SymbolKind::FunctionGroup)
                {
                    auto selectedType = node.refType.Lock();
                    if (!selectedType || selectedType->kind() != sema::TypeKind::Function)
                        selectedType = node.member->refType.Lock();
                    if ((!selectedType || selectedType->kind() != sema::TypeKind::Function) && !memberSym->overloads.empty())
                        selectedType = memberSym->overloads.front()->type;

                    std::string scopePath = memberSym->scopePath;
                    if (scopePath.empty() && !memberSym->overloads.empty())
                        scopePath = memberSym->overloads.front()->scopePath;

                    if (selectedType && selectedType->kind() == sema::TypeKind::Function)
                    {
                        auto mangledFunctionType = selectedType.AsFast<sema::FunctionType>();
                        if (!memberSym->overloads.empty() &&
                            memberSym->overloads.front()->type &&
                            memberSym->overloads.front()->type->kind() == sema::TypeKind::Function)
                        {
                            auto declarationFunctionType = memberSym->overloads.front()->type.AsFast<sema::FunctionType>();
                            if (containsGenericParameterTypeForCodegen(declarationFunctionType))
                            {
                                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                                    declarationFunctionType->returnType,
                                    getLeadingParameterTypes(declarationFunctionType, mangledFunctionType->paramTypes.size())
                                ).AsFast<sema::FunctionType>();
                            }
                        }

                        emit(Mangler::mangleFunction(memberSym->name, mangledFunctionType->paramTypes, scopePath));
                        return;
                    }
                }
                else if (memberSym->kind == sema::SymbolKind::Struct)
                {
                    emit(Mangler::mangleStruct(memberSym->name, memberSym->scopePath));
                    return;
                }
            }

            node.member->accept(*this);
            return;
        }

        if (node.object->is<SuperExpression>()) 
        {
            auto lockedType = node.object->refType.Lock();
            if (lockedType && lockedType->kind() == sema::TypeKind::Reference) 
            {
                auto refType = lockedType.AsFast<sema::ReferenceType>();
                auto baseStruct = refType->referredType.AsFast<sema::StructType>();
                
                emit(mangleStructTypeName(baseStruct) + "::");
                emitMemberName();
                return;
            }
        }
        
        std::string op = ".";
        std::size_t referenceDepth = 0;
        Ref<sema::Type> terminalType = nullptr;
    
        if (auto objSym = node.object->referencedSymbol.Lock())
        {
            if (objSym->kind == sema::SymbolKind::Namespace || 
                objSym->flags.get_isEnum() || 
                objSym->flags.get_isFlagset())
            {
                op = "::";
            }
        }
    
        if (op == ".") 
        {
            if (auto objType = node.object->refType.Lock())
            {
                auto baseType = objType;
                while (baseType && baseType->kind() == sema::TypeKind::Alias)
                    baseType = baseType.AsFast<sema::AliasType>()->aliasedType;

                while (baseType && baseType->kind() == sema::TypeKind::Reference)
                {
                    referenceDepth++;
                    baseType = baseType.AsFast<sema::ReferenceType>()->referredType;
                    while (baseType && baseType->kind() == sema::TypeKind::Alias)
                        baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                }

                terminalType = baseType;

                if (referenceDepth > 0)
                {
                    op = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        op = "->";
                }
            }
        }

        auto emitObjectWithReferenceDepth = [&](std::size_t extraReferenceDepth)
        {
            if (op == "::")
            {
                node.object->accept(*this);
                return;
            }

            if (extraReferenceDepth <= 1)
            {
                node.object->accept(*this);
                return;
            }

            emit("(");
            for (std::size_t i = 0; i < extraReferenceDepth - 1; ++i)
                emit("*");
            emit("(");
            node.object->accept(*this);
            emit("))");
        };

        emitObjectWithReferenceDepth(referenceDepth);

        emit(op);

        emitMemberName();
    }

    void CppGenerator::visit(FunctionCallExpression& node)
    {
        auto calleeType = node.callee->refType.Lock();
        auto calleeSym = node.callee->referencedSymbol.Lock();
        
        if (calleeType && calleeType->kind() == sema::TypeKind::Struct)
        {
            auto structType = calleeType.AsFast<sema::StructType>();
            if (structType->isObject)
            {
                emit("wio::runtime::Ref<" + mangleStructTypeName(structType) + ">::Create(");
            }
            else
            {
                emit(mangleStructTypeName(structType) + "(");
            }
            
            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                node.arguments[i]->accept(*this);
                if (i < node.arguments.size() - 1)
                    emit(", ");
            }
            emit(")");
            return;
        }

        Ref<sema::FunctionType> functionType = nullptr;
        if (calleeType && calleeType->kind() == sema::TypeKind::Function)
        {
            functionType = calleeType.AsFast<sema::FunctionType>();
        }
        else if (calleeSym && calleeSym->type && calleeSym->type->kind() == sema::TypeKind::Function)
        {
            functionType = calleeSym->type.AsFast<sema::FunctionType>();
        }

        auto emitArgumentWithImplicitCast = [&](const Ref<Expression>& argument, const Ref<sema::Type>& expectedType)
        {
            auto actualType = argument ? argument->refType.Lock() : nullptr;
            auto resolvedExpected = unwrapAliasType(expectedType);
            auto resolvedActual = unwrapAliasType(actualType);

            if (resolvedExpected &&
                resolvedExpected->kind() == sema::TypeKind::Function &&
                argument &&
                argument->is<LambdaExpression>())
            {
                emit(toCppType(expectedType));
                emit("(");
                argument->accept(*this);
                emit(")");
                return;
            }

            if (resolvedExpected && resolvedActual &&
                resolvedExpected->kind() == sema::TypeKind::Reference &&
                !resolvedExpected->isCompatibleWith(resolvedActual))
            {
                auto expectedRef = resolvedExpected.AsFast<sema::ReferenceType>();
                auto expectedTarget = unwrapAliasType(expectedRef->referredType);

                if (expectedTarget && expectedTarget->kind() == sema::TypeKind::Struct)
                {
                    auto expectedStruct = expectedTarget.AsFast<sema::StructType>();
                    if (expectedStruct->isInterface)
                    {
                        bool canCastObjectHandle = resolvedActual->kind() == sema::TypeKind::Struct;
                        if (!canCastObjectHandle && resolvedActual->kind() == sema::TypeKind::Reference)
                            canCastObjectHandle = true;

                        if (canCastObjectHandle)
                        {
                            emit("static_cast<" + toCppType(expectedType) + ">((");
                            argument->accept(*this);
                            emit(")->_WF_CastTo(" + mangleInterfaceTypeName(expectedStruct) + "::TYPE_ID))");
                            return;
                        }
                    }
                }
            }

            argument->accept(*this);
        };

        bool shouldEmitDirectFunctionCallee = false;
        if (calleeSym &&
            (calleeSym->kind == sema::SymbolKind::Function || calleeSym->kind == sema::SymbolKind::FunctionGroup) &&
            functionType)
        {
            if (node.callee->is<Identifier>())
            {
                shouldEmitDirectFunctionCallee = true;
            }
            else if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
            {
                if (auto objectSym = memberAccess->object->referencedSymbol.Lock();
                    objectSym && objectSym->kind == sema::SymbolKind::Namespace)
                {
                    shouldEmitDirectFunctionCallee = true;
                }
            }
        }

        if (shouldEmitDirectFunctionCallee)
        {
            std::string scopePath = calleeSym->scopePath;
            if (scopePath.empty() && calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                scopePath = calleeSym->overloads.front()->scopePath;

            Ref<sema::FunctionType> mangledFunctionType = functionType;
            if (!calleeSym->genericParameterNames.empty())
            {
                Ref<sema::Type> declarationType = calleeSym->type;
                if ((!declarationType || declarationType->kind() != sema::TypeKind::Function) &&
                    calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                {
                    declarationType = calleeSym->overloads.front()->type;
                }

                if (declarationType && declarationType->kind() == sema::TypeKind::Function)
                    mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                        declarationType.AsFast<sema::FunctionType>()->returnType,
                        getLeadingParameterTypes(declarationType.AsFast<sema::FunctionType>(), node.arguments.size())
                    ).AsFast<sema::FunctionType>();
            }

            emit(Mangler::mangleFunction(calleeSym->name, mangledFunctionType->paramTypes, scopePath));
        }
        else
        {
            node.callee->accept(*this);
        }
        if (!node.explicitTypeArguments.empty())
        {
            emit("<");
            for (size_t i = 0; i < node.explicitTypeArguments.size(); ++i)
            {
                node.explicitTypeArguments[i]->accept(*this);
                if (i < node.explicitTypeArguments.size() - 1)
                    emit(", ");
            }
            emit(">");
        }
        emit("(");
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            Ref<sema::Type> expectedType = nullptr;
            if (functionType && i < functionType->paramTypes.size())
                expectedType = functionType->paramTypes[i];

            emitArgumentWithImplicitCast(node.arguments[i], expectedType);
            if (i < node.arguments.size() - 1)
                emit(", ");
        }
        emit(")");
    }

    void CppGenerator::visit(LambdaExpression& node)
    {
        auto functionType = node.refType.Lock().AsFast<sema::FunctionType>();
        emit("[&](");
        
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            Ref<sema::Type> parameterType = nullptr;
            if (functionType && i < functionType->paramTypes.size())
                parameterType = functionType->paramTypes[i];
            else if (node.parameters[i].type)
                parameterType = node.parameters[i].type->refType.Lock();
            else if (node.parameters[i].name)
                parameterType = node.parameters[i].name->refType.Lock();

            if (parameterType && !parameterType->isUnknown())
                emit(toCppType(parameterType));
            else
                emit("auto"); 
            emit(" ");
            emit(node.parameters[i].name->token.value);
            if (i < node.parameters.size() - 1)
                emit(", ");
        }
        emit(")");

        if (functionType && functionType->returnType && !functionType->returnType->isUnknown())
        {
            emit(" -> ");
            emit(toCppType(functionType->returnType));
        }

        emit(" {\n");
        indent();

        if (node.body->is<ExpressionStatement>())
        {
            EMIT_TABS();
            emit("return ");
            node.body->as<ExpressionStatement>()->expression->accept(*this);
            emit(";\n");
        }
        else if (node.body->is<BlockStatement>())
        {
            auto block = node.body->as<BlockStatement>();
            for (auto& stmt : block->statements)
                stmt->accept(*this);
        }

        dedent();
        EMIT_TABS(); emit("}");
    }

    void CppGenerator::visit(RefExpression& node)
    {
        auto lockedType = node.operand->refType.Lock();
        bool isSmartPtrOrInterface = false;

        if (lockedType)
        {
            auto baseType = lockedType;
            while (baseType && baseType->kind() == sema::TypeKind::Alias)
                baseType = baseType.AsFast<sema::AliasType>()->aliasedType;
                
            if (baseType->kind() == sema::TypeKind::Struct)
            {
                auto sType = baseType.AsFast<sema::StructType>();
                if (sType->isObject || sType->isInterface) isSmartPtrOrInterface = true;
            }
            else if (baseType->kind() == sema::TypeKind::Reference)
            {
                auto refType = baseType.AsFast<sema::ReferenceType>()->referredType;
                if (refType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType.AsFast<sema::StructType>();
                    if (sType->isObject || sType->isInterface) isSmartPtrOrInterface = true;
                }
            }
        }
        
        if (!isSmartPtrOrInterface) emit("&");
        node.operand->accept(*this);
    }
    
    void CppGenerator::visit(FitExpression& node)
    {
        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();


        if (srcType->isNumeric() && destType->isNumeric())
        {
            std::string cppDestType = destType->toCppString();
            emit("wio::FitNumeric<" + cppDestType + ">(");
            node.operand->accept(*this);
            emit(")");
        }
        else
        {
            std::string destCpp = toCppType(destType); 
            std::string typeIdStr;
            Ref<sema::StructType> sType;
            
            if (destType->kind() == sema::TypeKind::Reference) 
                sType = destType.AsFast<sema::ReferenceType>()->referredType.AsFast<sema::StructType>();
            else if (destType->kind() == sema::TypeKind::Struct) 
                sType = destType.AsFast<sema::StructType>();

            if (sType)
            {
                if (sType->isInterface)
                    typeIdStr = mangleInterfaceTypeName(sType) + "::TYPE_ID";
                else
                    typeIdStr = mangleStructTypeName(sType) + "::TYPE_ID";
            }

            if (sType && sType->isInterface)
            {
                emit(common::formatString("static_cast<{}>((", destCpp));
                node.operand->accept(*this);
                emit(common::formatString(")->_WF_CastTo({}))", typeIdStr));
            }
            else
            {
                std::string objectTypeName = mangleStructTypeName(sType);
                emit(common::formatString("wio::runtime::Ref<{}>(static_cast<{}*>((", objectTypeName, objectTypeName));
                node.operand->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})))", typeIdStr));
            }
        }
    }

    void CppGenerator::visit(SelfExpression& node)
    {
        WIO_UNUSED(node);
        emit("this");
    }

    void CppGenerator::visit(SuperExpression& node)
    {
        auto lockedType = node.refType.Lock();
        
        if (lockedType && lockedType->kind() == sema::TypeKind::Reference)
        {
            auto refType = lockedType.AsFast<sema::ReferenceType>();
            auto baseStruct = refType->referredType.AsFast<sema::StructType>();
            
            std::string mangledBase = mangleStructTypeName(baseStruct);
            
            emit("static_cast<" + mangledBase + "*>(this)");
        }
        else
        {
            emit("this");
        }
    }

    void CppGenerator::visit(RangeExpression& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(MatchExpression& node)
    {
        bool producesValue = false;
        std::string matchReturnType = "void";
        if (auto t = node.refType.Lock(); t)
        {
            producesValue = !t->isVoid() && !t->isUnknown();
            if (producesValue)
                matchReturnType = toCppType(t);
        }

        emit("[&]()");
        if (producesValue)
            emit(" -> " + matchReturnType);
        emit(" {\n");
        indent();
        EMIT_TABS();
        emit("auto _match_val = (");
        node.value->accept(*this);
        emit(");\n");
        
        bool first = true;
        for (auto& matchCase : node.cases)
        {
            EMIT_TABS();
            if (matchCase.matchValues.empty()) // assumed
            {
                emit("else {\n");
            }
            else
            {
                if (!first) emit("else ");
                emit("if (");
                
                for (size_t i = 0; i < matchCase.matchValues.size(); ++i)
                {
                    auto& mVal = matchCase.matchValues[i];
                    if (mVal->is<RangeExpression>())
                    {
                        auto r = mVal->as<RangeExpression>();
                        emit("(_match_val >= ");
                        r->start->accept(*this);
                        emit(r->isInclusive ? " && _match_val <= " : " && _match_val < ");
                        r->end->accept(*this);
                        emit(")");
                    }
                    else
                    {
                        emit("_match_val == ");
                        mVal->accept(*this);
                    }
                    
                    if (i < matchCase.matchValues.size() - 1)
                        emit(" || ");
                }
                emit(") {\n");
            }
            first = false;
            
            indent();
            
            if (producesValue && matchCase.body->is<ExpressionStatement>())
            {
                EMIT_TABS();
                emit("return ");
                matchCase.body->as<ExpressionStatement>()->expression->accept(*this);
                emit(";\n");
            }
            else
            {
                matchCase.body->accept(*this);
            }
            
            dedent();
            EMIT_TABS();
            emit("}\n");
        }
        
        dedent();
        EMIT_TABS();
        emit("}()");
    }

    void CppGenerator::visit(ExpressionStatement& node)
    {
        emitSourceDirective(node.location());
        EMIT_TABS();
        node.expression->accept(*this);
        buffer_ << ";\n";
    }

    void CppGenerator::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(VariableDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto sym = node.name->referencedSymbol.Lock();

        Ref<sema::Type> varType = (sym && sym->type) ? sym->type : node.name->refType.Lock();
        std::string typeStr = toCppType(varType);
        std::string prefix;
        std::string suffix;

        if (node.mutability == Mutability::Const)
        {
            prefix = "constexpr ";
        }
        else if (node.mutability == Mutability::Immutable)
        {
            bool isStruct = (varType && varType->kind() == sema::TypeKind::Struct);
            if (!isStruct)
            {
                if (!typeStr.empty() && typeStr.back() == '*')
                    suffix = " const"; 
                else
                    prefix = "const ";
            }
        }
        EMIT_TABS();

        buffer_ << prefix << typeStr << suffix << " ";

        std::string varName = sanitizeCppIdentifier(node.name->token.value);

        buffer_ << ((sym && sym->flags.get_isGlobal()) ? Mangler::mangleGlobalVar(varName, sym->scopePath) : varName);
        
        if (node.initializer)
        {
            buffer_ << " = ";
            node.initializer->accept(*this);
        }

        buffer_ << ";\n";
    }

    void CppGenerator::visit(TypeAliasDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto sym = node.name->referencedSymbol.Lock();
        const std::string aliasName = sym ? Mangler::mangleStruct(sym->name, sym->scopePath) : node.name->token.value;

        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit("typename " + node.genericParameters[i]->token.value);
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }

        EMIT_TABS();
        emit("using " + aliasName + " = ");
        if (node.aliasedType)
            node.aliasedType->accept(*this);
        else
            emit("void");
        emitLine(";");
    }

    void CppGenerator::visit(FunctionDeclaration& node)
    {
        auto sym = node.name->referencedSymbol.Lock();
        auto funcType = sym->type.AsFast<sema::FunctionType>();
        auto instantiationTypeLists = getInstantiateTypeLists(node);

        std::string returnType = funcType->returnType ? toCppType(funcType->returnType) : "void";
        std::string funcName = node.name->token.value;
        bool isNative = isNativeFunction(node);
        bool isExported = isExportedFunction(node);
        bool hasModuleLifecycleExport = getModuleLifecycleAttribute(node).has_value();
        bool emitsExportWrapper = isExported || hasModuleLifecycleExport;

        if (funcName == "Entry" &&
            node.genericParameters.empty() &&
            Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!sym || sym->scopePath.empty()))
        { 
            if (!isEmittingPrototypes_)
                emitMain(node);
            return;
        }

        emitSourceDirective(node.location());
        emitLine();

        auto instantiateFunctionTypeForCodegen = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            auto bindings = buildGenericTypeBindings(sym->genericParameterNames, instantiationTypes);
            return instantiateGenericType(funcType, bindings).AsFast<sema::FunctionType>();
        };

        auto emitTemplateSpecializationArguments = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            emit(formatTemplateArgumentList(instantiationTypes));
        };

        auto emitGenericParameterArgumentList = [&]()
        {
            if (node.genericParameters.empty())
                return;

            emit("<");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit(node.genericParameters[i]->token.value);
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emit(">");
        };

        auto emitExplicitInstantiationDeclaration = [&](const std::vector<Ref<sema::Type>>& instantiationTypes)
        {
            auto instantiatedFunctionType = instantiateFunctionTypeForCodegen(instantiationTypes);
            if (!instantiatedFunctionType)
                return;

            EMIT_TABS();
            emit("template " + toCppType(instantiatedFunctionType->returnType) + " ");
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : ""));
            emitTemplateSpecializationArguments(instantiationTypes);
            emit("(");
            for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
            {
                emit(toCppType(instantiatedFunctionType->paramTypes[i]));
                if (i + 1 < instantiatedFunctionType->paramTypes.size())
                    emit(", ");
            }
            emitLine(");");
        };

        auto emitWrapperParameters = [&](const std::vector<Ref<sema::Type>>& parameterTypes)
        {
            for (size_t i = 0; i < parameterTypes.size(); ++i)
            {
                emit(common::formatString(
                    "{} {}",
                    toCppType(parameterTypes[i]),
                    sanitizeCppIdentifier(node.parameters[i].name->token.value)
                ));
                if (i + 1 < parameterTypes.size())
                    emit(", ");
            }
        };

        auto emitForwardingCallArguments = [&](size_t providedArgumentCount)
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                if (i > 0)
                    emit(", ");

                if (i < providedArgumentCount)
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                }
                else if (node.parameters[i].defaultValue)
                {
                    node.parameters[i].defaultValue->accept(*this);
                }
            }
        };

        auto emitDefaultArgumentWrappers = [&]()
        {
            if ((!node.body && !isNative) || !hasDefaultParameters(node))
                return;

            const size_t requiredParameterCount = getRequiredParameterCount(node);
            const size_t totalParameterCount = node.parameters.size();

            if (funcName == "OnConstruct")
            {
                for (size_t wrapperArity = requiredParameterCount; wrapperArity < totalParameterCount; ++wrapperArity)
                {
                    EMIT_TABS();
                    emit(currentClassName_ + "(");
                    emitWrapperParameters(getLeadingParameterTypes(funcType, wrapperArity));
                    emit(") : " + currentClassName_ + "(");
                    emitForwardingCallArguments(wrapperArity);
                    emitLine(") {}");
                }
                return;
            }

            if (funcName == "OnDestruct")
                return;

            const std::string scopePath = sym ? sym->scopePath : "";
            const std::string wrapperFullSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes, scopePath);
            const std::string wrapperMethodFullSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes);

            for (size_t wrapperArity = requiredParameterCount; wrapperArity < totalParameterCount; ++wrapperArity)
            {
                const auto wrapperParameterTypes = getLeadingParameterTypes(funcType, wrapperArity);
                const std::string wrapperSymbol = currentClassName_.empty()
                    ? Mangler::mangleFunction(funcName, wrapperParameterTypes, scopePath)
                    : Mangler::mangleFunction(funcName, wrapperParameterTypes);

                if (!node.genericParameters.empty())
                {
                    EMIT_TABS();
                    emit("template <");
                    for (size_t i = 0; i < node.genericParameters.size(); ++i)
                    {
                        emit("typename " + node.genericParameters[i]->token.value);
                        if (i + 1 < node.genericParameters.size())
                            emit(", ");
                    }
                    emitLine(">");
                }

                EMIT_TABS();
                emit(returnType + " " + wrapperSymbol + "(");
                emitWrapperParameters(wrapperParameterTypes);
                emit(")");

                if (isEmittingPrototypes_)
                {
                    emitLine(";\n");
                    continue;
                }

                emitLine();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");

                emit(currentClassName_.empty() ? wrapperFullSymbol : wrapperMethodFullSymbol);
                emitGenericParameterArgumentList();
                emit("(");
                emitForwardingCallArguments(wrapperArity);
                emit(");");
                emit("\n");
                dedent();
                emitLine("}");
            }
        };

        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit("typename " + node.genericParameters[i]->token.value);
                if (i < node.genericParameters.size() - 1)
                    emit(", ");
            }
            emitLine(">");
        }

        EMIT_TABS();

        if (!currentClassName_.empty())
        {
            if (funcName == "OnConstruct")
            {
                emit(currentClassName_ + "(");
            } else if (funcName == "OnDestruct")
            {
                emit("~" + currentClassName_ + "() ");
            }
            else
            {
                if (node.genericParameters.empty())
                    emit("virtual ");
                emit(returnType + " "); 
                emit(Mangler::mangleFunction(funcName, funcType->paramTypes) + "(");
            }
        }
        else 
        {
            emit(returnType + " ");
            emit(Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "") + "(");
        }

        if (funcName != "OnDestruct")
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                auto& param = node.parameters[i];
                emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), sanitizeCppIdentifier(param.name->token.value)));
                if (i < node.parameters.size() - 1) emit(", ");
            }
            emit(")");

            if (!currentClassName_.empty())
            {
                if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
            }
        }

        if (isEmittingPrototypes_)
        {
            emitLine(";\n");
            emitDefaultArgumentWrappers();
            return;
        }

        if (isNative)
        {
            std::string nativeSymbol = getNativeCppSymbolName(node);

            emitLine();
            emitLine("{");
            indent();
            EMIT_TABS();

            if (funcType->returnType && !funcType->returnType->isVoid())
                emit("return ");

            emit(nativeSymbol + "(");
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                if (i < node.parameters.size() - 1)
                    emit(", ");
            }
            emit(");");
            emit("\n");

            dedent();
            emitLine("}");
        }
        else if (node.body)
        {
            emitLine();
            
            if (node.whenCondition) 
            {
                emitLine("{");
                indent();
                
                EMIT_TABS();
                emit("if (!(");
                node.whenCondition->accept(*this);
                emit(")) return");
                if (node.whenFallback)
                {
                    emit(" ");
                    node.whenFallback->accept(*this);
                }
                emit(";\n\n");
                
                if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    for (auto& stmt : block->statements)
                        stmt->accept(*this);
                }
                else
                {
                    node.body->accept(*this);
                }
                
                dedent();
                emitLine("}");
            }
            else 
            {
                node.body->accept(*this);
            }
        }
        else
        {
            emitLine(";\n");
        }

        emitDefaultArgumentWrappers();

        if (emitsExportWrapper && !isEmittingPrototypes_ && currentClassName_.empty() && node.body)
        {
            emitGeneratedDirective();
            std::string internalSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "");
            if (!node.genericParameters.empty() && !instantiationTypeLists.empty())
            {
                const std::string exportBaseSymbol = getExportedCppSymbolName(node);
                for (const auto& instantiationTypes : instantiationTypeLists)
                {
                    auto instantiatedFunctionType = instantiateFunctionTypeForCodegen(instantiationTypes);
                    if (!instantiatedFunctionType)
                        continue;

                    emitLine();
                    EMIT_TABS();
                    emit("extern \"C\" WIO_EXPORT " + toCppType(instantiatedFunctionType->returnType) + " " +
                         formatInstantiatedExportSymbolName(exportBaseSymbol, instantiationTypes) + "(");
                    for (size_t i = 0; i < node.parameters.size(); ++i)
                    {
                        emit(common::formatString(
                            "{} {}",
                            toCppType(instantiatedFunctionType->paramTypes[i]),
                            sanitizeCppIdentifier(node.parameters[i].name->token.value)
                        ));
                        if (i < node.parameters.size() - 1) emit(", ");
                    }
                    emit(")");
                    emit("\n");
                    emitLine("{");
                    indent();
                    EMIT_TABS();

                    if (instantiatedFunctionType->returnType && !instantiatedFunctionType->returnType->isVoid())
                        emit("return ");

                    emit(internalSymbol);
                    emitTemplateSpecializationArguments(instantiationTypes);
                    emit("(");
                    for (size_t i = 0; i < node.parameters.size(); ++i)
                    {
                        emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                        if (i < node.parameters.size() - 1) emit(", ");
                    }
                    emit(");");
                    emit("\n");
                    dedent();
                    emitLine("}");
                }
            }
            else
            {
                std::string exportedSymbol = getExportedCppSymbolName(node);

                emitLine();
                EMIT_TABS();
                emit("extern \"C\" WIO_EXPORT " + returnType + " " + exportedSymbol + "(");
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& param = node.parameters[i];
                    emit(common::formatString("{} {}", toCppType(param.name->refType.Lock()), sanitizeCppIdentifier(param.name->token.value)));
                    if (i < node.parameters.size() - 1) emit(", ");
                }
                emit(")");
                emit("\n");
                emitLine("{");
                indent();
                EMIT_TABS();

                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");

                emit(internalSymbol + "(");
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value));
                    if (i < node.parameters.size() - 1) emit(", ");
                }
                emit(");");
                emit("\n");
                dedent();
                emitLine("}");
            }
        }

        if (!isEmittingPrototypes_ && currentClassName_.empty() && !node.genericParameters.empty() && !instantiationTypeLists.empty() && (isNative || isExported))
        {
            emitGeneratedDirective();
            emitLine();
            std::unordered_set<std::string> emittedBackendInstantiations;
            for (const auto& instantiationTypes : instantiationTypeLists)
            {
                if (!emittedBackendInstantiations.insert(getBackendInstantiationEquivalenceKey(instantiationTypes)).second)
                    continue;

                emitExplicitInstantiationDeclaration(instantiationTypes);
            }
        }
    }

    void CppGenerator::visit(RealmDeclaration& node)
    {
        emitStatements(node.statements);
    }

    void CppGenerator::visit(InterfaceDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto interfaceType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit("typename " + node.genericParameters[i]->token.value);
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }
        std::string interfaceName = mangleInterfaceTypeName(interfaceType);
        emitLine(common::formatString("struct {}", interfaceName));
        emitLine("{");
        indent();
    
        uint64_t typeId = common::fnv1a(interfaceName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual ~{}() = default;\n", interfaceName));
    
        for (auto& method : node.methods)
        {
            EMIT_TABS();
            auto sym = method->name->referencedSymbol.Lock();
            auto funcType = sym->type.AsFast<sema::FunctionType>();
            std::string retType = funcType->returnType ? toCppType(funcType->returnType) : "void";
            std::string mangledName = Mangler::mangleFunction(method->name->token.value, funcType->paramTypes);
        
            emit(common::formatString("virtual {} {}(", retType, mangledName));
            for (size_t i = 0; i < method->parameters.size(); ++i) {
                emit(common::formatString("{} {}", toCppType(method->parameters[i].name->refType.Lock()), sanitizeCppIdentifier(method->parameters[i].name->token.value)));
                if (i < method->parameters.size() - 1) emit(", ");
            }
            emit(") = 0;\n");
        }
    
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(ComponentDeclaration& node)
        {
        emitSourceDirective(node.location());
        auto componentSym = node.name->referencedSymbol.Lock();
        auto componentType = getStructTypeFromSymbol(componentSym);
        auto enclosingScope = componentSym && componentSym->innerScope ? componentSym->innerScope->getParent().Lock() : nullptr;

        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit("typename " + node.genericParameters[i]->token.value);
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }

        std::string structName = mangleStructTypeName(componentType);
        emit("struct " + structName);
        
        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
        
        auto bases = getBaseInterfaces(node.attributes);
        if (!bases.empty())
        {
            emit(" : ");
            for (size_t i = 0; i < bases.size(); ++i)
            {
                auto baseSym = enclosingScope ? resolveQualifiedSymbol(enclosingScope, bases[i]) : nullptr;
                std::string baseName = mangleNamedType(baseSym);
                if (baseName.empty())
                    baseName = Mangler::mangleInterface(bases[i]);

                emit("public " + baseName);
                if (i < bases.size() - 1) emit(", ");
            }
        }
        emitLine("\n{");
        indent();
    
        auto trustArgs = getAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = enclosingScope ? resolveQualifiedSymbol(enclosingScope, t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }
    
        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                auto sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), sanitizeCppIdentifier(vDecl->name->token.value));
            }
        }
    
        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;
    
        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct") 
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();
                    
                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1) 
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }
                    
                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor)) 
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }
    
            if (member.access != currentAccess && member.access != AccessModifier::None)
            {
                dedent();
                if (member.access == AccessModifier::Public) emitLine("public:");
                else if (member.access == AccessModifier::Private) emitLine("private:");
                else if (member.access == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = member.access;
            }
            member.declaration->accept(*this);
        }
    
        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);

        auto emitValueInitDefaultCtor = [&]()
        {
            EMIT_TABS();
            emit(currentClassName_ + "()");
            if (!memberVars.empty())
            {
                emit(" : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "()");
                    if (i < memberVars.size() - 1) emit(", ");
                }
            }
            emit(" {}\n");
        };

        // Keep components embeddable inside generated C++ objects even when the Wio
        // surface only exposes member constructors.
        if (!hasEmptyCtor && !hasNoDefaultCtor)
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
                currentAccess = AccessModifier::Public;
            }

            emitValueInitDefaultCtor();
        }
    
        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }
                
            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");
    
            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }
    
        currentClassName_ = "";
        dedent();
        emitLine("};\n");
    }
    
    void CppGenerator::visit(ObjectDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto symb = node.name->referencedSymbol.Lock();
        auto objectType = getStructTypeFromSymbol(symb);
        if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                emit("typename " + node.genericParameters[i]->token.value);
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }
        std::string structName = mangleStructTypeName(objectType);
        emit("struct " + structName); 
        
        if (hasAttribute(node.attributes, Attribute::Final)) emit(" final");
        
        auto globalScope = symb->innerScope->getParent().Lock();

        std::vector<Ref<sema::StructType>> bases;
        if (objectType)
        {
            for (const auto& baseType : objectType->baseTypes)
            {
                auto resolvedBaseType = unwrapAliasType(baseType);
                if (!resolvedBaseType || resolvedBaseType->kind() != sema::TypeKind::Struct)
                    continue;

                auto baseStruct = resolvedBaseType.AsFast<sema::StructType>();
                if (baseStruct->name == "object" && baseStruct->scopePath.empty())
                    continue;

                bases.push_back(baseStruct);
            }
        }
    
        bool hasBaseObject = false;
        for (const auto& baseType : bases)
        {
            if (baseType && !baseType->isInterface)
            {
                hasBaseObject = true;
                break;
            }
        }
    
        std::string baseList;
    
        if (!hasBaseObject)
        {
            emit(" : public wio::runtime::RefCountedObject");
        }
        
        for (const auto& base : bases)
        {
            if (!baseList.empty()) baseList += ", ";
            baseList += "public " + mangleNamedType(base);
        }
    
        if (!baseList.empty())
        {
            if (hasBaseObject) emitLine(" : " + baseList);
            else emitLine(", " + baseList);
        }
        emitLine("{");
        indent();
    
        uint64_t typeId = common::fnv1a(structName.c_str());
        emitLine(common::formatString("static constexpr uint64_t TYPE_ID = {}ull;", typeId));
        emitLine(common::formatString("virtual uint64_t _WF_GetTypeID() const {{ return {}ull; }}", typeId));
        
        emitLine("virtual bool _WF_IsA(uint64_t id) const override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return true;", typeId));
        for (const auto& base : bases) {
            if (base && base->isInterface) {
                emitLine(common::formatString("if (id == {}::TYPE_ID) return true;", mangleNamedType(base)));
            } else {
                emitLine(common::formatString("if ({}::_WF_IsA(id)) return true;", mangleNamedType(base)));
            }
        }
        emitLine("return false;");
        dedent();
        emitLine("}\n");
    
        emitLine("virtual void* _WF_CastTo(uint64_t id) override {");
        indent();
        emitLine(common::formatString("if (id == {}ull) return this;", typeId));
        for (const auto& base : bases)
        {
            if (base && base->isInterface)
            {
                std::string intf = mangleNamedType(base);
                emitLine(common::formatString("if (id == {}::TYPE_ID) return static_cast<{}*>(this);", intf, intf));
            }
            else
            {
                emitLine(common::formatString("if (void* base_cast = {}::_WF_CastTo(id)) return base_cast;", mangleNamedType(base)));
            }
        }
        emitLine("return nullptr;");
        dedent();
        emitLine("}\n");
    
        std::string objectRefFriend = structName;
        if (!node.genericParameters.empty())
        {
            objectRefFriend += "<";
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                objectRefFriend += node.genericParameters[i]->token.value;
                if (i + 1 < node.genericParameters.size())
                    objectRefFriend += ", ";
            }
            objectRefFriend += ">";
        }
        emitLine("friend class wio::runtime::Ref<" + objectRefFriend + ">;");
        auto trustArgs = getAttributeArgs(node.attributes, Attribute::Trust);
        for (const auto& t : trustArgs)
        {
            if (t.type == TokenType::identifier)
            {
                auto trustSym = globalScope ? resolveQualifiedSymbol(globalScope, t.value) : nullptr;
                std::string trustName = mangleNamedType(trustSym);
                if (trustName.empty())
                    trustName = Mangler::mangleStruct(t.value);

                emitLine("friend struct " + trustName + ";");
            }
        }
    
        currentClassName_ = structName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                const auto& sym = vDecl->name->referencedSymbol.Lock();
                Ref<sema::Type> varType = (sym && sym->type) ? sym->type : vDecl->name->refType.Lock();
                memberVars.emplace_back(toCppType(varType), sanitizeCppIdentifier(vDecl->name->token.value));
            }
        }
    
        bool hasCustomCtor = false;
        bool hasEmptyCtor = false;
        bool hasCopyCtor = false;
        bool hasMemberCtor = false;
    
        for (auto& member : node.members)
        {
            if (member.declaration->is<FunctionDeclaration>())
            {
                auto funcDecl = member.declaration->as<FunctionDeclaration>();
                if (funcDecl->name->token.value == "OnConstruct") 
                {
                    hasCustomCtor = true;
                    size_t pCount = funcDecl->parameters.size();
                    
                    if (pCount == 0) hasEmptyCtor = true;
                    else if (pCount == 1) 
                    {
                        std::string pType = toCppType(funcDecl->parameters[0].name->refType.Lock());
                        if (pType.find(currentClassName_) != std::string::npos) hasCopyCtor = true;
                    }
                    
                    if (pCount == memberVars.size() && !(pCount == 1 && hasCopyCtor)) 
                    {
                        bool typesMatch = true;
                        for (size_t i = 0; i < pCount; ++i) {
                            if (toCppType(funcDecl->parameters[i].name->refType.Lock()) != memberVars[i].first) {
                                typesMatch = false; break;
                            }
                        }
                        if (typesMatch) hasMemberCtor = true;
                    }
                }
            }
    
            AccessModifier targetAccess = (member.access == AccessModifier::None) ? AccessModifier::Private : member.access;
    
            if (targetAccess != currentAccess)
            {
                dedent();
                if (targetAccess == AccessModifier::Public) emitLine("public:");
                else if (targetAccess == AccessModifier::Private) emitLine("private:");
                else if (targetAccess == AccessModifier::Protected) emitLine("protected:");
                indent();
                currentAccess = targetAccess;
            }
            member.declaration->accept(*this);
        }
    
        bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
        bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
    
        if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
        {
            if (currentAccess != AccessModifier::Public)
            {
                dedent();
                emitLine("public:");
                indent();
            }
    
            if (!hasEmptyCtor)
                emitLine(currentClassName_ + "() = default;");
                
            if (!hasCopyCtor)
                emitLine(currentClassName_ + "(const " + currentClassName_ + "&) = default;");
    
            if (!memberVars.empty() && !hasMemberCtor)
            {
                EMIT_TABS();
                emit(currentClassName_ + "(");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].first + " _" + memberVars[i].second);
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(") : ");
                for (size_t i = 0; i < memberVars.size(); ++i)
                {
                    emit(memberVars[i].second + "(_" + memberVars[i].second + ")");
                    if (i < memberVars.size() - 1) emit(", ");
                }
                emit(" {}\n");
            }
        }
    
        currentClassName_ = "";
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto flagType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string structName = mangleStructTypeName(flagType);
        emitLine(common::formatString("struct {0} {{ explicit {0}() = default; };\n", structName));
    }

    void CppGenerator::visit(EnumDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto enumType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(enumType);
        std::string underType = "int32_t";
        
        auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty()) {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        emitLine("enum " + enumName + " : " + underType + "\n{");
        indent();

        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }
            
            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }
        
        dedent();
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagsetDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto flagsetType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(flagsetType);
        std::string underType = "uint32_t";
        
        auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type);
        if (!typeArgs.empty())
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (typeArgs[0].type)
            {
                case TokenType::kwI8: underType = "int8_t"; break;
                case TokenType::kwU8: underType = "uint8_t"; break;
                case TokenType::kwI16: underType = "int16_t"; break;
                case TokenType::kwU16: underType = "uint16_t"; break;
                case TokenType::kwI32: underType = "int32_t"; break;
                case TokenType::kwU32: underType = "uint32_t"; break;
                case TokenType::kwI64: underType = "int64_t"; break;
                case TokenType::kwU64: underType = "uint64_t"; break;
                default: break;
            }
        }

        emitLine("enum class " + enumName + " : " + underType + "\n{");
        indent();
        
        for (size_t i = 0; i < node.members.size(); ++i)
        {
            EMIT_TABS();
            emit(node.members[i].name->token.value);
            if (node.members[i].value)
            {
                emit(" = ");
                node.members[i].value->accept(*this);
            }
            
            if (i < node.members.size() - 1)
                emit(",");
            emit("\n");
        }
        
        dedent();
        emitLine("};");

        emitLine(common::formatString("inline constexpr {0} operator|({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) | static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator&({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) & static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator^({0} a, {0} b) {{ return static_cast<{0}>(static_cast<{1}>(a) ^ static_cast<{1}>(b)); }}", enumName, underType));
        emitLine(common::formatString("inline constexpr {0} operator~({0} a) {{ return static_cast<{0}>(~static_cast<{1}>(a)); }}", enumName, underType));
    }

    void CppGenerator::visit(BlockStatement& node)
    {
        emitLine("{");
        indent();
        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }
        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(IfStatement& node)
    {
        emitSourceDirective(node.location());
        if (node.matchVar.isValid() && node.condition->is<BinaryExpression>())
        {
            auto binExpr = node.condition->as<BinaryExpression>();
            auto typeSym = binExpr->right->referencedSymbol.Lock();
            auto sType = typeSym->type.AsFast<sema::StructType>();
            
            std::string destCpp = toCppType(typeSym->type);
            std::string typeIdStr = sType->isInterface
                ? (mangleInterfaceTypeName(sType) + "::TYPE_ID")
                : (mangleStructTypeName(sType) + "::TYPE_ID");

            EMIT_TABS();
            if (sType->isInterface)
            {
                emit(common::formatString("if ({} {} = static_cast<{}>((", destCpp, node.matchVar.value, destCpp));
                binExpr->left->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})); {})", typeIdStr, node.matchVar.value));
            }
            else
            {
                std::string objectTypeName = mangleStructTypeName(sType);
                emit(common::formatString("if ({}* _raw_{} = static_cast<{}*>((", objectTypeName, node.matchVar.value, objectTypeName));
                binExpr->left->accept(*this);
                emit(common::formatString(")->_WF_CastTo({})))", typeIdStr));
            }
            emit("\n");
            emitLine("{");
            indent();
            
            if (!sType->isInterface)
            {
                emitLine(common::formatString("wio::runtime::Ref<{}> {}(_raw_{});", mangleStructTypeName(sType), node.matchVar.value, node.matchVar.value));
            }
            
            if (node.thenBranch) node.thenBranch->accept(*this);
            
            dedent();
            emitLine("}");
            
            if (node.elseBranch)
            {
                emit("else ");
                node.elseBranch->accept(*this);
            }
            return;
        }

        EMIT_TABS();
        emit("if (");
        node.condition->accept(*this);
        emit(")\n");
        
        if (node.thenBranch) {
            if (!node.thenBranch->is<BlockStatement>()) { emitLine("{"); indent(); }
            node.thenBranch->accept(*this);
            if (!node.thenBranch->is<BlockStatement>()) { dedent(); emitLine("}"); }
        }
        
        if (node.elseBranch)
        {
            emit("else ");
            node.elseBranch->accept(*this);
        }
    }
    
    void CppGenerator::visit(WhileStatement& node)
    {
        emitSourceDirective(node.location());
        EMIT_TABS();
        buffer_ << "while (";
        node.condition->accept(*this);
        buffer_ << ")\n";
        
        node.body->accept(*this);
    }

    void CppGenerator::visit(ForInStatement& node)
    {
        emitSourceDirective(node.location());
        auto emitLoopBodyStatements = [&](const NodePtr<Statement>& body)
        {
            if (!body)
                return;

            if (body->is<BlockStatement>())
            {
                auto block = body->as<BlockStatement>();
                for (auto& stmt : block->statements)
                    stmt->accept(*this);
                return;
            }

            body->accept(*this);
        };

        auto unwrapAliasType = [](Ref<sema::Type> type)
        {
            while (type && type->kind() == sema::TypeKind::Alias)
                type = type.AsFast<sema::AliasType>()->aliasedType;
            return type;
        };

        auto buildReferenceInitializer = [&](const Ref<sema::Type>& bindingType, const std::string& sourceExpr)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(bindingType);
            if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                return sourceExpr;

            auto refType = resolvedType.AsFast<sema::ReferenceType>();
            Ref<sema::Type> referredType = unwrapAliasType(refType->referredType);

            if (referredType && referredType->kind() == sema::TypeKind::Struct)
            {
                auto structType = referredType.AsFast<sema::StructType>();
                if (structType->isObject || structType->isInterface)
                    return sourceExpr;
            }

            return "&(" + sourceExpr + ")";
        };

        auto getBindingMode = [&](size_t index) -> ForBindingMode
        {
            if (index < node.bindingModes.size())
                return node.bindingModes[index];

            return ForBindingMode::ValueImmutable;
        };

        if (node.iterable->is<RangeExpression>())
        {
            const auto* range = node.iterable->as<RangeExpression>();
            const auto& binding = node.bindings.front();
            const ForBindingMode bindingMode = getBindingMode(0);
            auto bindingSym = binding->referencedSymbol.Lock();
            Ref<sema::Type> bindingType = (bindingSym && bindingSym->type) ? bindingSym->type : binding->refType.Lock();

            const std::string loopVarName = bindingMode == ForBindingMode::ValueImmutable
                ? "_wio_range_value"
                : binding->token.value;

            emitLine("{");
            indent();

            EMIT_TABS();
            emit("auto _wio_range_start = (");
            range->start->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_range_end = (");
            range->end->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_range_step = static_cast<" + toCppType(bindingType) + ">(");
            if (node.step)
                node.step->accept(*this);
            else
                emit("1");
            emit(");\n");

            emitLine("if (_wio_range_step == 0) throw std::runtime_error(\"Range step cannot be zero.\");");

            emitLine("if (_wio_range_step > 0)");
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("for (" + toCppType(bindingType) + " " + loopVarName + " = _wio_range_start; ");
            emit(loopVarName);
            emit(range->isInclusive ? " <= " : " < ");
            emit("_wio_range_end; " + loopVarName + " = static_cast<" + toCppType(bindingType) + ">(" + loopVarName + " + _wio_range_step))\n");

            emitLine("{");
            indent();

            if (bindingMode == ForBindingMode::ValueImmutable)
                emitLine("const auto& " + binding->token.value + " = " + loopVarName + ";");

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");

            emitLine("else");
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("for (" + toCppType(bindingType) + " " + loopVarName + " = _wio_range_start; ");
            emit(loopVarName);
            emit(range->isInclusive ? " >= " : " > ");
            emit("_wio_range_end; " + loopVarName + " = static_cast<" + toCppType(bindingType) + ">(" + loopVarName + " + _wio_range_step))\n");

            emitLine("{");
            indent();

            if (bindingMode == ForBindingMode::ValueImmutable)
                emitLine("const auto& " + binding->token.value + " = " + loopVarName + ";");

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");

            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
        }

        auto emitBoundDeclaration = [&](size_t index, const std::string& sourceExpr)
        {
            const auto& binding = node.bindings[index];
            const ForBindingMode bindingMode = getBindingMode(index);
            auto bindingSym = binding->referencedSymbol.Lock();
            Ref<sema::Type> bindingType = (bindingSym && bindingSym->type) ? bindingSym->type : binding->refType.Lock();

            switch (bindingMode)
            {
            case ForBindingMode::ValueMutable:
                emitLine("auto " + binding->token.value + " = " + sourceExpr + ";");
                return;
            case ForBindingMode::ValueImmutable:
                emitLine("const auto& " + binding->token.value + " = " + sourceExpr + ";");
                return;
            case ForBindingMode::ReferenceMutable:
            case ForBindingMode::ReferenceView:
                  emitLine(toCppType(bindingType) + " " + binding->token.value + " = " + buildReferenceInitializer(bindingType, sourceExpr) + ";");
                  return;
              }
          };

        auto isArrayIterable = [&]() -> bool
        {
            Ref<sema::Type> iterableType = node.iterable ? node.iterable->refType.Lock() : nullptr;
            iterableType = unwrapAliasType(iterableType);

            while (iterableType && iterableType->kind() == sema::TypeKind::Reference)
                iterableType = unwrapAliasType(iterableType.AsFast<sema::ReferenceType>()->referredType);

            return iterableType && iterableType->kind() == sema::TypeKind::Array;
        };

        const bool explicitIndexBinding = !node.bindingAccessors.empty() && node.bindingAccessors.front() == "__index__";
        if (isArrayIterable() && (explicitIndexBinding || node.step != nullptr))
        {
            emitLine("{");
            indent();

            EMIT_TABS();
            emit("auto&& _wio_range = (");
            node.iterable->accept(*this);
            emit(");\n");

            EMIT_TABS();
            emit("auto _wio_array_step = static_cast<std::int64_t>(");
            if (node.step)
                node.step->accept(*this);
            else
                emit("1");
            emit(");\n");

            emitLine("if (_wio_array_step <= 0) throw std::runtime_error(\"Array step must be positive.\");");

            EMIT_TABS();
            emit("for (std::size_t _wio_index = 0; _wio_index < _wio_range.size(); _wio_index += static_cast<std::size_t>(_wio_array_step))\n");
            emitLine("{");
            indent();

            if (explicitIndexBinding)
            {
                const auto& indexBinding = node.bindings.front();
                const ForBindingMode indexBindingMode = getBindingMode(0);
                auto indexBindingSym = indexBinding->referencedSymbol.Lock();
                Ref<sema::Type> indexBindingType = (indexBindingSym && indexBindingSym->type) ? indexBindingSym->type : indexBinding->refType.Lock();
                std::string indexCastExpr = "static_cast<" + toCppType(indexBindingType) + ">(_wio_index)";

                switch (indexBindingMode)
                {
                case ForBindingMode::ValueMutable:
                    emitLine("auto " + indexBinding->token.value + " = " + indexCastExpr + ";");
                    break;
                case ForBindingMode::ValueImmutable:
                    emitLine("const auto " + indexBinding->token.value + " = " + indexCastExpr + ";");
                    break;
                case ForBindingMode::ReferenceMutable:
                case ForBindingMode::ReferenceView:
                    break;
                }
            }

            const std::string itemExpr = "_wio_range[_wio_index]";
            if (node.bindingAccessors.empty())
            {
                emitBoundDeclaration(0, itemExpr);
            }
            else
            {
                const size_t bindingStartIndex = explicitIndexBinding ? 1 : 0;
                if (explicitIndexBinding && node.bindingAccessors.size() >= 2 && node.bindingAccessors[1] == "__value__")
                {
                    emitBoundDeclaration(1, itemExpr);
                }
                else
                {
                    for (size_t i = bindingStartIndex; i < node.bindings.size() && i < node.bindingAccessors.size(); ++i)
                    {
                        if (node.bindingAccessors[i] == "__index__")
                            continue;

                        if (node.bindingAccessors[i] == "__value__")
                            emitBoundDeclaration(i, itemExpr);
                        else
                            emitBoundDeclaration(i, itemExpr + "." + node.bindingAccessors[i]);
                    }
                }
            }

            emitLoopBodyStatements(node.body);
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
        }

        const bool hasMutableReferenceBinding = std::ranges::any_of(node.bindingModes, [](ForBindingMode mode)
        {
            return mode == ForBindingMode::ReferenceMutable;
        });

        const bool needsPrelude = node.bindings.size() > 1 ||
            getBindingMode(0) == ForBindingMode::ReferenceMutable ||
            getBindingMode(0) == ForBindingMode::ReferenceView;

        std::string rangeVarName = needsPrelude ? "_wio_loop_item" : node.bindings.front()->token.value;
        std::string rangeDecl = "const auto& " + rangeVarName;
        if (!needsPrelude)
        {
            switch (getBindingMode(0))
            {
            case ForBindingMode::ValueMutable:
                rangeDecl = "auto " + rangeVarName;
                break;
            case ForBindingMode::ValueImmutable:
                rangeDecl = "const auto& " + rangeVarName;
                break;
            case ForBindingMode::ReferenceMutable:
                rangeDecl = "auto& " + rangeVarName;
                break;
            case ForBindingMode::ReferenceView:
                rangeDecl = "const auto& " + rangeVarName;
                break;
            }
        }
        else if (hasMutableReferenceBinding)
        {
            rangeDecl = "auto& " + rangeVarName;
        }

        EMIT_TABS();
        buffer_ << "for (" << rangeDecl << " : ";
        node.iterable->accept(*this);
        buffer_ << ")\n";

        emitLine("{");
        indent();

        if (needsPrelude)
        {
            if (node.bindingAccessors.empty())
            {
                emitBoundDeclaration(0, rangeVarName);
            }
            else
            {
                for (size_t i = 0; i < node.bindings.size() && i < node.bindingAccessors.size(); ++i)
                {
                    emitBoundDeclaration(i, rangeVarName + "." + node.bindingAccessors[i]);
                }
            }
        }

        emitLoopBodyStatements(node.body);
        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(CForStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("{");
        indent();

        if (node.initializer)
            node.initializer->accept(*this);

        EMIT_TABS();
        emit("for (; ");
        if (node.condition)
            node.condition->accept(*this);
        else
            emit("true");
        emit("; ");
        if (node.increment)
            node.increment->accept(*this);
        emit(")\n");

        if (node.body)
            node.body->accept(*this);

        dedent();
        emitLine("}");
    }

    void CppGenerator::visit(BreakStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("break;");
    }

    void CppGenerator::visit(ContinueStatement& node)
    {
        emitSourceDirective(node.location());
        emitLine("continue;");
    }

    void CppGenerator::visit(ReturnStatement& node)
    {
        emitSourceDirective(node.location());
        EMIT_TABS();
        
        buffer_ << "return";
        if (node.value)
        {
            buffer_ << " ";
            node.value->accept(*this);
        }
        buffer_ << ";\n";
    }

    void CppGenerator::visit(UseStatement& node)
    {
        WIO_UNUSED(node);
    }
}
