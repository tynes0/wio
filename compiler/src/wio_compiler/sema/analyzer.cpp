#include "wio/sema/analyzer.h"

#include <cctype>
#include <functional>
#include <optional>
#include <ranges>
#include <unordered_set>

#include "wio/codegen/mangler.h"
#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/common/utility.h"
#include "wio/sema/intrinsic_member_resolver.h"

#include "compiler.h"
namespace wio::sema
{
    namespace
    {
        const Token* getFirstAttributeArg(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr);
        std::vector<Attribute> getModuleLifecycleAttributes(const std::vector<NodePtr<AttributeStatement>>& attributes);

        const std::unordered_set<std::string>& getCppReservedIdentifiers()
        {
            static const std::unordered_set<std::string> keywords = {
                "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break",
                "case", "catch", "char", "char8_t", "char16_t", "char32_t", "class", "compl", "concept",
                "const", "consteval", "constexpr", "constinit", "const_cast", "continue", "co_await",
                "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast",
                "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
                "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
                "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr",
                "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static",
                "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local", "throw",
                "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
                "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
            };

            return keywords;
        }

        bool isValidCppIdentifier(std::string_view identifier)
        {
            if (identifier.empty())
                return false;

            const unsigned char first = static_cast<unsigned char>(identifier.front());
            if (!(std::isalpha(first) || identifier.front() == '_'))
                return false;

            for (char ch : identifier)
            {
                const unsigned char uch = static_cast<unsigned char>(ch);
                if (!(std::isalnum(uch) || ch == '_'))
                    return false;
            }

            return !getCppReservedIdentifiers().contains(std::string(identifier));
        }

        bool isValidCppSymbolPath(std::string_view symbolPath, bool allowQualified)
        {
            if (symbolPath.empty())
                return false;

            if (!allowQualified && symbolPath.find("::") != std::string_view::npos)
                return false;

            size_t start = 0;
            while (start <= symbolPath.size())
            {
                const size_t separator = symbolPath.find("::", start);
                const size_t count = separator == std::string_view::npos ? symbolPath.size() - start : separator - start;
                const std::string_view segment = symbolPath.substr(start, count);
                if (!isValidCppIdentifier(segment))
                    return false;

                if (separator == std::string_view::npos)
                    break;

                start = separator + 2;
            }

            return true;
        }

        std::string getModuleLifecycleExportSymbol(Attribute lifecycleAttribute)
        {
            switch (lifecycleAttribute)
            {
            case Attribute::ModuleApiVersion: return "WioModuleApiVersion";
            case Attribute::ModuleLoad: return "WioModuleLoad";
            case Attribute::ModuleUpdate: return "WioModuleUpdate";
            case Attribute::ModuleUnload: return "WioModuleUnload";
            case Attribute::ModuleSaveState: return "WioModuleSaveState";
            case Attribute::ModuleRestoreState: return "WioModuleRestoreState";
            default: return {};
            }
        }

        std::string getDeclaredExportSymbolName(const FunctionDeclaration& node, bool hasModuleLifecycle)
        {
            if (hasModuleLifecycle)
            {
                std::vector<Attribute> lifecycleAttributes = getModuleLifecycleAttributes(node.attributes);
                if (!lifecycleAttributes.empty())
                    return getModuleLifecycleExportSymbol(lifecycleAttributes.front());
            }

            if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                return cppNameArg->value;

            return node.name ? node.name->token.value : "";
        }

        std::string formatInstantiatedExportSymbolName(const std::string& baseName, const std::vector<Ref<Type>>& instantiationTypes)
        {
            std::string result = baseName;
            for (const auto& instantiationType : instantiationTypes)
            {
                result += "__";
                std::string fragment = codegen::Mangler::mangleType(instantiationType);
                std::ranges::replace(fragment, ':', '_');
                result += fragment;
            }
            return result;
        }

        std::vector<std::string> splitModulePath(std::string_view modulePath)
        {
            std::vector<std::string> parts;
            size_t start = 0;

            while (start <= modulePath.size())
            {
                size_t separator = modulePath.find('/', start);
                size_t count = separator == std::string_view::npos ? modulePath.size() - start : separator - start;

                if (count > 0)
                    parts.emplace_back(modulePath.substr(start, count));

                if (separator == std::string_view::npos)
                    break;

                start = separator + 1;
            }

            return parts;
        }

        Ref<Type> unwrapAliasType(Ref<Type> type)
        {
            while (type && type->kind() == TypeKind::Alias)
                type = type.AsFast<AliasType>()->aliasedType;

            return type;
        }

        bool canMutateIntrinsicReceiver(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (auto receiverSymbol = expression->referencedSymbol.Lock(); receiverSymbol)
            {
                if (receiverSymbol->flags.get_isMutable())
                    return true;
            }

            Ref<Type> receiverType = unwrapAliasType(expression->refType.Lock());
            while (receiverType && receiverType->kind() == TypeKind::Reference)
            {
                auto referenceType = receiverType.AsFast<ReferenceType>();
                if (!referenceType->isMutable)
                    return false;

                receiverType = unwrapAliasType(referenceType->referredType);
            }

            return false;
        }

        bool isUnsupportedStaticArrayMember(const Ref<Type>& type, std::string_view memberName)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != TypeKind::Array)
                return false;

            auto arrayType = resolvedType.AsFast<ArrayType>();
            if (arrayType->arrayKind != ArrayType::ArrayKind::Static)
                return false;

            return isDynamicArrayOnlyIntrinsicMemberName(memberName);
        }

        bool isStringType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "string";
        }

        bool isIntrinsicReceiverType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return false;

            return resolvedType->kind() == TypeKind::Array ||
                   resolvedType->kind() == TypeKind::Dictionary ||
                   isStringType(resolvedType);
        }

        bool shouldAutoReadReferenceType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->kind() != TypeKind::Reference)
                return false;

            while (current && current->kind() == TypeKind::Reference)
            {
                auto refType = current.AsFast<ReferenceType>();
                current = unwrapAliasType(refType->referredType);

                if (!current)
                    return false;

                if (current->kind() == TypeKind::Struct)
                {
                    auto structType = current.AsFast<StructType>();
                    if (structType->isObject || structType->isInterface)
                        return false;
                }
            }

            return true;
        }

        Ref<Type> getAutoReadableType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!shouldAutoReadReferenceType(type))
                return current;

            while (current && current->kind() == TypeKind::Reference)
                current = unwrapAliasType(current.AsFast<ReferenceType>()->referredType);

            return current;
        }

        bool isIntegralLikeType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "isize" || name == "usize" || name == "byte" ||
                   name == "char" || name == "uchar";
        }

        bool isZeroIntegerLiteralExpression(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (const auto* literal = expression->as<IntegerLiteral>())
            {
                IntegerResult result = common::getInteger(literal->token.value);
                if (!result.isValid)
                    return false;

                switch (result.type)
                {
                case IntegerType::i8: return result.value.v_i8 == 0;
                case IntegerType::i16: return result.value.v_i16 == 0;
                case IntegerType::i32: return result.value.v_i32 == 0;
                case IntegerType::i64: return result.value.v_i64 == 0;
                case IntegerType::u8: return result.value.v_u8 == 0;
                case IntegerType::u16: return result.value.v_u16 == 0;
                case IntegerType::u32: return result.value.v_u32 == 0;
                case IntegerType::u64: return result.value.v_u64 == 0;
                case IntegerType::isize: return result.value.v_isize == 0;
                case IntegerType::usize: return result.value.v_usize == 0;
                case IntegerType::Unknown: return false;
                }
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                if (unary->op.type == TokenType::opMinus || unary->op.type == TokenType::opPlus)
                    return isZeroIntegerLiteralExpression(unary->operand);
            }

            return false;
        }

        bool isTypeDerivedFrom(const Ref<Type>& derived, const Ref<Type>& base);

        std::unordered_map<std::string, Ref<Type>> buildGenericTypeBindings(const std::vector<std::string>& parameterNames,
                                                                            const std::vector<Ref<Type>>& typeArguments)
        {
            std::unordered_map<std::string, Ref<Type>> bindings;
            const size_t bindingCount = std::min(parameterNames.size(), typeArguments.size());
            bindings.reserve(bindingCount);

            for (size_t i = 0; i < bindingCount; ++i)
                bindings.emplace(parameterNames[i], typeArguments[i]);

            return bindings;
        }

        bool containsGenericParameterType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
                return true;
            case TypeKind::Reference:
                return containsGenericParameterType(current.AsFast<ReferenceType>()->referredType);
            case TypeKind::Array:
                return containsGenericParameterType(current.AsFast<ArrayType>()->elementType);
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterType(dictType->keyType) ||
                       containsGenericParameterType(dictType->valueType);
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsGenericParameterType(funcType->returnType))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [](const Ref<Type>& paramType)
                {
                    return containsGenericParameterType(paramType);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                if (!structType->genericParameterNames.empty() && structType->genericArguments.empty())
                    return true;

                return std::ranges::any_of(structType->genericArguments, [](const Ref<Type>& genericArgument)
                {
                    return containsGenericParameterType(genericArgument);
                });
            }
            case TypeKind::Alias:
                return containsGenericParameterType(current.AsFast<AliasType>()->aliasedType);
            default:
                return false;
            }
        }

        bool containsNamedGenericParameterType(const Ref<Type>& type, const std::vector<std::string>& genericParameterNames)
        {
            if (genericParameterNames.empty())
                return false;

            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                return std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end();
            }
            case TypeKind::Reference:
                return containsNamedGenericParameterType(current.AsFast<ReferenceType>()->referredType, genericParameterNames);
            case TypeKind::Array:
                return containsNamedGenericParameterType(current.AsFast<ArrayType>()->elementType, genericParameterNames);
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsNamedGenericParameterType(dictType->keyType, genericParameterNames) ||
                       containsNamedGenericParameterType(dictType->valueType, genericParameterNames);
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsNamedGenericParameterType(funcType->returnType, genericParameterNames))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [&](const Ref<Type>& paramType)
                {
                    return containsNamedGenericParameterType(paramType, genericParameterNames);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                return std::ranges::any_of(structType->genericArguments, [&](const Ref<Type>& genericArgument)
                {
                    return containsNamedGenericParameterType(genericArgument, genericParameterNames);
                });
            }
            case TypeKind::Alias:
                return containsNamedGenericParameterType(current.AsFast<AliasType>()->aliasedType, genericParameterNames);
            default:
                return false;
            }
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings);

        Ref<Type> instantiateGenericStructType(const Ref<StructType>& structType,
                                               const std::vector<Ref<Type>>& explicitTypeArguments)
        {
            if (!structType)
                return nullptr;

            if (structType->genericParameterNames.empty())
                return structType;

            auto bindings = buildGenericTypeBindings(structType->genericParameterNames, explicitTypeArguments);

            auto instantiatedScope = structType->structScope.Lock();
            auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                structType->name,
                instantiatedScope,
                structType->isObject,
                structType->isInterface
            ).AsFast<StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericArguments = explicitTypeArguments;
            instantiatedType->fieldNames = structType->fieldNames;

            instantiatedType->fieldTypes.reserve(structType->fieldTypes.size());
            for (const auto& fieldType : structType->fieldTypes)
                instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, bindings));

            instantiatedType->baseTypes.reserve(structType->baseTypes.size());
            for (const auto& baseType : structType->baseTypes)
                instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, bindings));

            return instantiatedType;
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            auto& ctx = Compiler::get().getTypeContext();
            
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (auto it = bindings.find(genericParam->name); it != bindings.end())
                    return it->second;
                return current;
            }
            case TypeKind::Reference:
            {
                auto refType = current.AsFast<ReferenceType>();
                return ctx.getOrCreateReferenceType(
                    instantiateGenericType(refType->referredType, bindings),
                    refType->isMutable
                );
            }
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return ctx.getOrCreateArrayType(
                    instantiateGenericType(arrayType->elementType, bindings),
                    arrayType->arrayKind,
                    arrayType->size
                );
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return ctx.getOrCreateDictionaryType(
                    instantiateGenericType(dictType->keyType, bindings),
                    instantiateGenericType(dictType->valueType, bindings),
                    dictType->isOrdered
                );
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                std::vector<Ref<Type>> instantiatedParams;
                instantiatedParams.reserve(funcType->paramTypes.size());
                for (const auto& paramType : funcType->paramTypes)
                    instantiatedParams.push_back(instantiateGenericType(paramType, bindings));

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(funcType->returnType, bindings),
                    instantiatedParams
                );
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                std::vector<Ref<Type>> instantiatedArguments;

                if (!structType->genericArguments.empty())
                {
                    instantiatedArguments.reserve(structType->genericArguments.size());
                    for (const auto& genericArgument : structType->genericArguments)
                        instantiatedArguments.push_back(instantiateGenericType(genericArgument, bindings));

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                if (!structType->genericParameterNames.empty())
                {
                    instantiatedArguments.reserve(structType->genericParameterNames.size());
                    for (const auto& genericParameterName : structType->genericParameterNames)
                    {
                        auto bindingIt = bindings.find(genericParameterName);
                        if (bindingIt == bindings.end())
                            return current;

                        instantiatedArguments.push_back(bindingIt->second);
                    }

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                return current;
            }
            case TypeKind::Alias:
                return instantiateGenericType(current.AsFast<AliasType>()->aliasedType, bindings);
            default:
                return current;
            }
        }

        bool deduceGenericBindings(const Ref<Type>& expected,
                                   const Ref<Type>& actual,
                                   std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            Ref<Type> resolvedExpected = unwrapAliasType(expected);
            Ref<Type> resolvedActual = unwrapAliasType(actual);

            auto findMatchingBaseTypeInstance = [&](auto&& self, const Ref<Type>& candidateType, const Ref<Type>& targetType) -> Ref<Type>
            {
                Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                Ref<Type> resolvedTarget = unwrapAliasType(targetType);

                if (!resolvedCandidate || !resolvedTarget ||
                    resolvedCandidate->kind() != TypeKind::Struct ||
                    resolvedTarget->kind() != TypeKind::Struct)
                {
                    return nullptr;
                }

                auto candidateStruct = resolvedCandidate.AsFast<StructType>();
                auto targetStruct = resolvedTarget.AsFast<StructType>();

                for (const auto& baseType : candidateStruct->baseTypes)
                {
                    Ref<Type> resolvedBaseType = unwrapAliasType(baseType);
                    if (!resolvedBaseType || resolvedBaseType->kind() != TypeKind::Struct)
                        continue;

                    auto baseStruct = resolvedBaseType.AsFast<StructType>();
                    if (baseStruct->name == targetStruct->name &&
                        baseStruct->scopePath == targetStruct->scopePath)
                    {
                        return resolvedBaseType;
                    }

                    if (auto nestedMatch = self(self, resolvedBaseType, resolvedTarget))
                        return nestedMatch;
                }

                return nullptr;
            };

            if (!resolvedExpected || !resolvedActual || resolvedActual->isUnknown())
                return false;

            if (resolvedExpected->kind() == TypeKind::GenericParameter)
            {
                if (resolvedActual->kind() == TypeKind::Null)
                    return false;

                auto genericParam = resolvedExpected.AsFast<GenericParameterType>();
                if (auto it = bindings.find(genericParam->name); it != bindings.end())
                    return it->second->isCompatibleWith(resolvedActual) &&
                           resolvedActual->isCompatibleWith(it->second);

                bindings.emplace(genericParam->name, resolvedActual);
                return true;
            }

            if (resolvedExpected->kind() == TypeKind::Reference &&
                resolvedActual->kind() == TypeKind::Reference)
            {
                auto expectedRef = resolvedExpected.AsFast<ReferenceType>();
                auto actualRef = resolvedActual.AsFast<ReferenceType>();

                if (expectedRef->isMutable && !actualRef->isMutable)
                    return false;

                if (containsGenericParameterType(expectedRef->referredType))
                    return deduceGenericBindings(expectedRef->referredType, actualRef->referredType, bindings);

                return expectedRef->referredType->isCompatibleWith(actualRef->referredType) ||
                       isTypeDerivedFrom(actualRef->referredType, expectedRef->referredType);
            }

            if (resolvedExpected->kind() == TypeKind::Reference &&
                resolvedActual->kind() == TypeKind::Struct)
            {
                auto expectedRef = resolvedExpected.AsFast<ReferenceType>();
                auto expectedTarget = unwrapAliasType(expectedRef->referredType);

                if (!expectedRef->isMutable &&
                    expectedTarget && expectedTarget->kind() == TypeKind::Struct)
                {
                    auto expectedStruct = expectedTarget.AsFast<StructType>();
                    auto actualStruct = resolvedActual.AsFast<StructType>();

                    if ((expectedStruct->isObject || expectedStruct->isInterface) &&
                        (actualStruct->isObject || actualStruct->isInterface))
                    {
                        if (auto matchingBaseType = findMatchingBaseTypeInstance(findMatchingBaseTypeInstance, resolvedActual, expectedTarget))
                        {
                            if (containsGenericParameterType(expectedTarget))
                                return deduceGenericBindings(expectedTarget, matchingBaseType, bindings);

                            return expectedTarget->isCompatibleWith(matchingBaseType) ||
                                   isTypeDerivedFrom(matchingBaseType, expectedTarget);
                        }

                        if (containsGenericParameterType(expectedTarget))
                            return deduceGenericBindings(expectedTarget, resolvedActual, bindings);

                        return expectedTarget->isCompatibleWith(resolvedActual) ||
                               isTypeDerivedFrom(resolvedActual, expectedTarget);
                    }
                }
            }

            if (resolvedExpected->kind() == TypeKind::Array &&
                resolvedActual->kind() == TypeKind::Array)
            {
                auto expectedArray = resolvedExpected.AsFast<ArrayType>();
                auto actualArray = resolvedActual.AsFast<ArrayType>();

                if (expectedArray->arrayKind == ArrayType::ArrayKind::Dynamic)
                    return deduceGenericBindings(expectedArray->elementType, actualArray->elementType, bindings);

                if (actualArray->arrayKind == ArrayType::ArrayKind::Dynamic)
                    return false;

                if (actualArray->size > expectedArray->size)
                    return false;

                return deduceGenericBindings(expectedArray->elementType, actualArray->elementType, bindings);
            }

            if (resolvedExpected->kind() == TypeKind::Dictionary &&
                resolvedActual->kind() == TypeKind::Dictionary)
            {
                auto expectedDict = resolvedExpected.AsFast<DictionaryType>();
                auto actualDict = resolvedActual.AsFast<DictionaryType>();

                if (expectedDict->isOrdered != actualDict->isOrdered)
                    return false;

                return deduceGenericBindings(expectedDict->keyType, actualDict->keyType, bindings) &&
                       deduceGenericBindings(expectedDict->valueType, actualDict->valueType, bindings);
            }

            if (resolvedExpected->kind() == TypeKind::Function &&
                resolvedActual->kind() == TypeKind::Function)
            {
                auto expectedFunc = resolvedExpected.AsFast<FunctionType>();
                auto actualFunc = resolvedActual.AsFast<FunctionType>();

                if (expectedFunc->paramTypes.size() != actualFunc->paramTypes.size())
                    return false;

                if (!deduceGenericBindings(expectedFunc->returnType, actualFunc->returnType, bindings))
                    return false;

                for (size_t i = 0; i < expectedFunc->paramTypes.size(); ++i)
                {
                    if (!deduceGenericBindings(expectedFunc->paramTypes[i], actualFunc->paramTypes[i], bindings))
                        return false;
                }

                return true;
            }

            if (resolvedExpected->kind() == TypeKind::Struct &&
                resolvedActual->kind() == TypeKind::Struct)
            {
                auto expectedStruct = resolvedExpected.AsFast<StructType>();
                auto actualStruct = resolvedActual.AsFast<StructType>();

                if (expectedStruct->name != actualStruct->name ||
                    expectedStruct->scopePath != actualStruct->scopePath ||
                    expectedStruct->genericArguments.size() != actualStruct->genericArguments.size())
                {
                    return false;
                }

                for (size_t i = 0; i < expectedStruct->genericArguments.size(); ++i)
                {
                    if (!deduceGenericBindings(expectedStruct->genericArguments[i], actualStruct->genericArguments[i], bindings))
                        return false;
                }

                return true;
            }

            return resolvedExpected->isCompatibleWith(resolvedActual) ||
                   (resolvedExpected->isNumeric() && resolvedActual->isNumeric());
        }

        Ref<Type> resolvePrimitiveType(const std::string& name)
        {
            auto& ctx = Compiler::get().getTypeContext();
        
            if (name == "i8") return ctx.getI8();
            if (name == "i16") return ctx.getI16();
            if (name == "i32") return ctx.getI32();
            if (name == "i64") return ctx.getI64();
            if (name == "u8") return ctx.getU8();
            if (name == "u16") return ctx.getU16();
            if (name == "u32") return ctx.getU32();
            if (name == "u64") return ctx.getU64();
            if (name == "isize") return ctx.getISize();
            if (name == "usize") return ctx.getUSize();
            if (name == "f32") return ctx.getF32();
            if (name == "f64") return ctx.getF64();
            if (name == "bool") return ctx.getBool();
            if (name == "char") return ctx.getChar();
            if (name == "string") return ctx.getString();
            if (name == "void") return ctx.getVoid();

            if (name == "object") return ctx.getObject();

            return nullptr; 
        }

        bool isTypeDerivedFrom(const Ref<Type>& derived, const Ref<Type>& base)
        {
            if (!derived || !base) return false;
            if (derived->isCompatibleWith(base)) return true;
            
            if (derived->kind() == sema::TypeKind::Struct)
            {
                auto sType = derived.AsFast<sema::StructType>();
                for (auto& bType : sType->baseTypes)
                {
                    if (isTypeDerivedFrom(bType, base)) return true;
                }
            }
            return false;
        }

        bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            return std::ranges::any_of(attributes, [targetAttr](const auto& attr) { return attr->attribute == targetAttr; });
        }

        std::vector<const AttributeStatement*> getAttributeStatements(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            std::vector<const AttributeStatement*> matches;
            for (const auto& attr : attributes)
            {
                if (attr && attr->attribute == targetAttr)
                    matches.push_back(attr.Get());
            }

            return matches;
        }

        std::vector<Token> getAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            std::vector<Token> allArgs;
            for (const auto& attr : attributes) {
                if (attr->attribute == targetAttr)
                {
                    allArgs.insert(allArgs.end(), attr->args.begin(), attr->args.end());
                }
            }
            return allArgs;
        }

        struct AttributeTypeArgument
        {
            Token token;
            NodePtr<TypeSpecifier> typeSpecifier;
        };

        std::vector<AttributeTypeArgument> getAttributeTypeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            std::vector<AttributeTypeArgument> allArgs;
            for (const auto& attr : attributes)
            {
                if (attr->attribute != targetAttr)
                    continue;

                for (size_t i = 0; i < attr->args.size(); ++i)
                {
                    NodePtr<TypeSpecifier> typeSpecifier = nullptr;
                    if (i < attr->typeArgs.size())
                        typeSpecifier = attr->typeArgs[i];

                    allArgs.push_back(AttributeTypeArgument{
                        .token = attr->args[i],
                        .typeSpecifier = typeSpecifier
                    });
                }
            }
            return allArgs;
        }

        const Token* getFirstAttributeArg(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr)
        {
            for (const auto& attr : attributes)
            {
                if (attr->attribute == targetAttr && !attr->args.empty())
                    return &attr->args.front();
            }

            return nullptr;
        }

        Ref<Symbol> resolveQualifiedSymbol(const Ref<Scope>& startScope, std::string_view qualifiedName)
        {
            if (!startScope || qualifiedName.empty())
                return nullptr;

            size_t segmentStart = 0;
            Ref<Scope> scope = startScope;
            Ref<Symbol> resolvedSymbol = nullptr;

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

        Ref<Symbol> resolveAttributeSymbol(const Ref<Scope>& startScope, const Token& token)
        {
            if (token.type != TokenType::identifier)
                return nullptr;

            return resolveQualifiedSymbol(startScope, token.value);
        }

        bool isCAbiSafeExportType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return false;

            if (current->kind() != TypeKind::Primitive)
                return false;

            const std::string typeName = current->toString();
            return typeName != "string" && typeName != "object";
        }

        bool isExactType(const Ref<Type>& actual, const Ref<Type>& expected)
        {
            Ref<Type> lhs = actual;
            Ref<Type> rhs = expected;

            while (lhs && lhs->kind() == TypeKind::Alias)
                lhs = lhs.AsFast<AliasType>()->aliasedType;

            while (rhs && rhs->kind() == TypeKind::Alias)
                rhs = rhs.AsFast<AliasType>()->aliasedType;

            return lhs && rhs && lhs->isCompatibleWith(rhs) && rhs->isCompatibleWith(lhs);
        }

        bool isModuleLifecycleAttribute(Attribute attribute)
        {
            return attribute == Attribute::ModuleApiVersion ||
                   attribute == Attribute::ModuleLoad ||
                   attribute == Attribute::ModuleUpdate ||
                   attribute == Attribute::ModuleUnload ||
                   attribute == Attribute::ModuleSaveState ||
                   attribute == Attribute::ModuleRestoreState;
        }

        std::vector<Attribute> getModuleLifecycleAttributes(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<Attribute> lifecycleAttributes;
            for (const auto& attr : attributes)
            {
                if (attr && isModuleLifecycleAttribute(attr->attribute))
                    lifecycleAttributes.push_back(attr->attribute);
            }

            return lifecycleAttributes;
        }

        const char* getModuleLifecycleAttributeName(Attribute attribute)
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (attribute)
            {
            case Attribute::ModuleApiVersion: return "@ModuleApiVersion";
            case Attribute::ModuleLoad: return "@ModuleLoad";
            case Attribute::ModuleUpdate: return "@ModuleUpdate";
            case Attribute::ModuleUnload: return "@ModuleUnload";
            case Attribute::ModuleSaveState: return "@ModuleSaveState";
            case Attribute::ModuleRestoreState: return "@ModuleRestoreState";
            default: return "@UnknownModuleLifecycle";
            }
        }
    }
    
    SemanticAnalyzer::SemanticAnalyzer() = default;

    SemanticAnalyzer::~SemanticAnalyzer()
    {
        for (auto& sym : symbols_)
        {
            if (sym)
            {
                sym->innerScope = nullptr;
                sym->overloads.clear();
            }
        }
        
        for (auto& scope : scopes_)
        {
            if (scope)
            {
                scope->clear();
            }
        }
    }

    void SemanticAnalyzer::analyze(const Ref<Program>& program)
    {
        scopes_.clear();
        symbols_.clear();
        functionDeclarationsBySymbol_.clear();
        exportedCppSymbolLocations_.clear();
        currentScope_ = nullptr;
        currentExpectedExpressionType_ = nullptr;
        currentNamespacePath_.clear();
        seenModuleApiVersion_ = false;
        seenModuleLoad_ = false;
        seenModuleUpdate_ = false;
        seenModuleUnload_ = false;
        seenModuleSaveState_ = false;
        seenModuleRestoreState_ = false;
        
        program->accept(*this);
    }

    void SemanticAnalyzer::enterScope(ScopeKind kind)
    {
        auto scope = Ref<Scope>::Create(currentScope_, kind);
        currentScope_ = scope;
        scopes_.push_back(std::move(scope));
    }

    void SemanticAnalyzer::exitScope()
    {
        if (currentScope_)
            currentScope_ = currentScope_->getParent().Lock();
    }

    std::string SemanticAnalyzer::getCurrentNamespacePath() const
    {
        std::string namespacePath;

        for (size_t i = 0; i < currentNamespacePath_.size(); ++i)
        {
            if (i > 0)
                namespacePath += "_";

            namespacePath += currentNamespacePath_[i];
        }

        return namespacePath;
    }

    Ref<Symbol> SemanticAnalyzer::createSymbol(std::string name, Ref<Type> type, SymbolKind kind, common::Location loc, SymbolFlags flags)
    {
        auto symbol = Ref<Symbol>::Create(std::move(name), std::move(type), kind, flags, loc);
        symbol->scopePath = getCurrentNamespacePath();
        symbols_.push_back(symbol);
        return symbol;
    }

    void SemanticAnalyzer::visit(Program& node)
    {
        enterScope(ScopeKind::Global);

        isDeclarationPass_ = true;
        for (auto& stmt : node.statements)
            stmt->accept(*this);

        isDeclarationPass_ = false;
        isStructResolutionPass_ = true;
        for (auto& stmt : node.statements)
        {
            if (stmt->is<ComponentDeclaration>() || 
                stmt->is<ObjectDeclaration>() ||
                stmt->is<EnumDeclaration>() ||
                stmt->is<FlagsetDeclaration>() ||
                stmt->is<FlagDeclaration>() ||
                stmt->is<RealmDeclaration>())
            {
                stmt->accept(*this);
            }
        }
        
        isStructResolutionPass_ = false;
        for (auto& stmt : node.statements)
            stmt->accept(*this);

        auto entrySym = currentScope_->resolveLocally("Entry");
        if (Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!entrySym || (entrySym->kind != SymbolKind::Function && entrySym->kind != SymbolKind::FunctionGroup)))
        {
            WIO_LOG_ADD_ERROR(common::Location::invalid(), "No 'Entry' function found! An executable Wio program must define an 'Entry' function.");
        }
        exitScope();
    }

    void SemanticAnalyzer::visit(BlockStatement& node)
    {
        if (isDeclarationPass_) return;
        enterScope(ScopeKind::Block);

        for (auto& stmt : node.statements)
        {
            stmt->accept(*this);
        }

        exitScope();
    }

    void SemanticAnalyzer::visit(TypeSpecifier& node)
    {
        auto formatAppliedTypeName = [](const std::string& baseName, const std::vector<Ref<Type>>& typeArguments) -> std::string
        {
            if (typeArguments.empty())
                return baseName;

            std::string result = baseName + "<";
            for (size_t i = 0; i < typeArguments.size(); ++i)
            {
                result += typeArguments[i] ? typeArguments[i]->toString() : "<unknown>";
                if (i + 1 < typeArguments.size())
                    result += ", ";
            }
            result += ">";
            return result;
        };

        if (node.name.type == TokenType::kwFn)
        {
            node.generics[0]->accept(*this);
            auto retType = node.generics[0]->refType.Lock();
            
            std::vector<Ref<Type>> paramTypes;
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                paramTypes.push_back(node.generics[i]->refType.Lock());
            }
            
            node.refType = Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes);
            return;
        }

        if (node.name.value == "Dict" || node.name.value == "Tree")
        {
            bool isOrdered = (node.name.value == "Tree");
            
            if (node.generics.size() != 2)
            {
                WIO_LOG_ADD_ERROR(node.location(), "'{}' requires exactly 2 generic arguments (Key, Value).", node.name.value);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.generics[0]->accept(*this);
            node.generics[1]->accept(*this);

            node.refType = Compiler::get().getTypeContext().getOrCreateDictionaryType(
                node.generics[0]->refType.Lock(),
                node.generics[1]->refType.Lock(),
                isOrdered
            );
            return;
        }
        
        Ref<Type> type = resolvePrimitiveType(node.name.value);

        if (!type &&
            node.name.type == TokenType::identifier &&
            node.name.value.find("::") == std::string::npos)
        {
            for (auto& genericTypeParameterScope : std::ranges::reverse_view(genericTypeParameterScopes_))
            {
                if (auto genericTypeIt = genericTypeParameterScope.find(node.name.value); genericTypeIt != genericTypeParameterScope.end())
                {
                    type = genericTypeIt->second;
                    break;
                }
            }
        }

        if (!type)
        {
            if (node.name.type == TokenType::StaticArray)
            {
                node.generics[0]->accept(*this);
                type = Compiler::get().getTypeContext().getOrCreateArrayType(node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Static, node.size);
            }
            else if (node.name.type == TokenType::DynamicArray)
            {
                node.generics[0]->accept(*this);
                type = Compiler::get().getTypeContext().getOrCreateArrayType(node.generics[0]->refType.Lock(), ArrayType::ArrayKind::Dynamic);
            }
            else if (node.name.type == TokenType::kwRef || node.name.type == TokenType::kwView)
            {
                node.generics[0]->accept(*this);
                bool isMut = (node.name.type == TokenType::kwRef); 
                type = Compiler::get().getTypeContext().getOrCreateReferenceType(node.generics[0]->refType.Lock(), isMut);
            }
            else if (node.name.type == TokenType::identifier)
            {
                if (auto sym = resolveQualifiedSymbol(currentScope_, node.name.value))
                {
                    std::vector<Ref<Type>> explicitTypeArguments;
                    explicitTypeArguments.reserve(node.generics.size());
                    for (auto& genericArgument : node.generics)
                    {
                        genericArgument->accept(*this);
                        explicitTypeArguments.push_back(genericArgument->refType.Lock());
                    }

                    if (sym->kind == SymbolKind::Struct) 
                    {
                        auto structType = sym->type.AsFast<StructType>();
                        if (!structType->genericParameterNames.empty())
                        {
                            if (explicitTypeArguments.size() != structType->genericParameterNames.size())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Generic type '{}' expects {} generic arguments, but got {}.",
                                    node.name.value,
                                    structType->genericParameterNames.size(),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                type = instantiateGenericStructType(structType, explicitTypeArguments);
                            }
                        }
                        else
                        {
                            if (!explicitTypeArguments.empty())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type '{}' does not accept generic arguments.",
                                    node.name.value
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                type = sym->type;
                            }
                        }
                    }
                    else if (sym->kind == SymbolKind::TypeAlias)
                    {
                        if (!sym->genericParameterNames.empty())
                        {
                            if (explicitTypeArguments.size() != sym->genericParameterNames.size())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type alias '{}' expects {} generic arguments, but got {}.",
                                    node.name.value,
                                    sym->genericParameterNames.size(),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                std::unordered_map<std::string, Ref<Type>> bindings;
                                bindings.reserve(sym->genericParameterNames.size());
                                for (size_t i = 0; i < sym->genericParameterNames.size(); ++i)
                                    bindings.emplace(sym->genericParameterNames[i], explicitTypeArguments[i]);

                                Ref<Type> instantiatedAliasTarget = instantiateGenericType(sym->aliasTargetType, bindings);
                                type = Compiler::get().getTypeContext().getOrCreateAliasType(
                                    formatAppliedTypeName(node.name.value, explicitTypeArguments),
                                    instantiatedAliasTarget
                                );
                            }
                        }
                        else
                        {
                            if (!explicitTypeArguments.empty())
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type alias '{}' does not accept generic arguments.",
                                    node.name.value
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                type = sym->type;
                            }
                        }
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "'{}' is not a type.", node.name.value);
                    }
                }
            }
        }

        if (!type)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Unknown type: '{}'", node.name.value);
            type = Compiler::get().getTypeContext().getUnknown();
        }

        node.refType = type;
    }

    void SemanticAnalyzer::visit(BinaryExpression& node)
    {
        node.left->accept(*this);
        node.right->accept(*this);

        if (node.op.type == TokenType::kwIn && node.right->is<RangeExpression>())
        {
            auto leftType = node.left->refType.Lock();
            if (leftType && !leftType->isNumeric())
            {
                WIO_LOG_ADD_ERROR(node.location(), "The left operand of 'in' operator must be numeric when used with a range.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
            return; 
        }
        if (node.op.type == TokenType::kwIs)
        {
            auto typeSym = node.right->referencedSymbol.Lock();
            if (!typeSym || typeSym->kind != SymbolKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.right->location(), "The right side of the 'is' operator must be an object or interface type.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
            return;
        }

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();
        Ref<Type> readableLhsType = getAutoReadableType(lhsType);
        Ref<Type> readableRhsType = getAutoReadableType(rhsType);

        if (!lhsType || !rhsType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        bool isCompatible = lhsType->isCompatibleWith(rhsType);
        if (!isCompatible && readableLhsType && readableRhsType &&
            (shouldAutoReadReferenceType(lhsType) || shouldAutoReadReferenceType(rhsType)))
        {
            isCompatible = readableLhsType->isCompatibleWith(readableRhsType);
        }

        if (!isCompatible)
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Type mismatch in binary operation '{}': Cannot operate on '{}' and '{}'.",
                node.op.value,
                lhsType->toString(),
                rhsType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.isComparison() ||
            node.op.type == TokenType::opLogicalAnd ||
            node.op.type == TokenType::opLogicalOr ||
            node.op.type == TokenType::kwAnd ||
            node.op.type == TokenType::kwOr ||
            node.op.type == TokenType::kwIn)
        {
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else
        {
            // Todo: In arithmetic operations (for now), the result type is the same as the type of the left operand.
            node.refType = readableLhsType ? readableLhsType : lhsType;
        }
    }
    
    void SemanticAnalyzer::visit(UnaryExpression& node)
    {
        node.operand->accept(*this);
        Ref<Type> opType = node.operand->refType.Lock();

        if (!opType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.type == TokenType::kwNot || node.op.type == TokenType::opLogicalNot)
        {
            if (opType != Compiler::get().getTypeContext().getBool())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Logical NOT (!) operator requires boolean operand.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
        }
        else if (node.op.type == TokenType::opMinus)
        {
            if (!opType->isNumeric())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Unary minus (-) operator requires numeric operand.");
            }
            node.refType = opType;
        }
        else
        {
            // Others (bitwise not vs.) should return the same type for now.
            node.refType = opType;
        }
    }

    void SemanticAnalyzer::visit(AssignmentExpression& node)
    {
        node.left->accept(*this);
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        currentExpectedExpressionType_ = getAutoReadableType(node.left->refType.Lock());
        node.right->accept(*this);
        currentExpectedExpressionType_ = previousExpectedExpressionType;

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        bool isCompatible = false;
        bool isAutoDeref = false;

        if (lhsType && rhsType)
        {
            isCompatible = lhsType->isCompatibleWith(rhsType);

            if (!isCompatible && lhsType->kind() == TypeKind::Reference)
            {
                Ref<Type> currentRef = lhsType;
                bool canMutate = true;

                while (currentRef && currentRef->kind() == TypeKind::Reference)
                {
                    auto rType = currentRef.AsFast<ReferenceType>();
                    
                    if (!rType->isMutable) canMutate = false;
                    
                    if (rType->referredType->isCompatibleWith(rhsType) || 
                       (rType->referredType->isNumeric() && rhsType->isNumeric())) // YENİ: Deref atamada sayısal esneklik
                    {
                        isAutoDeref = true;
                        if (!canMutate) WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify data through a read-only reference (view).");
                        else isCompatible = true;
                        break;
                    }
                    currentRef = rType->referredType;
                }
            }
        }

        if (lhsType && rhsType && !isCompatible)
        {
            if (lhsType->isNumeric() && rhsType->isNumeric())
            {
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                    rhsType->toString(),
                    lhsType->toString()
                );
            }
        }

        if (!isAutoDeref)
        {
            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
            {
                if (!referSym->flags.get_isMutable() && !referSym->flags.get_isReadOnly())
                {
                    WIO_LOG_ADD_ERROR(node.op.loc, "Cannot assign to immutable variable '{0}'. Hint: Declare it as 'mut {0}'.", referSym->name);
                }
                if (referSym->flags.get_isReadOnly())
                {
                    bool isInsideObject = false;
                    auto currentSearch = currentScope_;
                    while (currentSearch)
                    {
                        if (currentSearch->resolveLocally(referSym->name)) { isInsideObject = true; break; }
                        currentSearch = currentSearch->getParent().Lock();
                    }

                    if (!isInsideObject)
                        WIO_LOG_ADD_ERROR(node.op.loc, "Cannot modify @Readonly member '{0}' from outside its object.", referSym->name);
                }
            }
        }

        node.refType = Compiler::get().getTypeContext().getVoid(); 
    }
    
    void SemanticAnalyzer::visit(IntegerLiteral& node)
    {
        IntegerResult result = common::getInteger(node.token.value);
        
        if (!result.isValid)
            WIO_LOG_ADD_ERROR(node.location(), "Invalid integer literal or value out of bounds: '{}'", node.token.value);
        
        Ref<Type> type = Type::getTypeFromIntegerResult(result);
        node.refType = type;
    }
    
    void SemanticAnalyzer::visit(FloatLiteral& node)
    {
        FloatResult result = common::getFloat(node.token.value);

        if (!result.isValid)
            WIO_LOG_ADD_ERROR(node.location(), "Invalid float literal or value out of bounds: '{}'", node.token.value);
        
        Ref<Type> type = Type::getTypeFromFloatResult(result);
        node.refType = type;
    }
    
    void SemanticAnalyzer::visit(StringLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getString();
    }
    
    void SemanticAnalyzer::visit(InterpolatedStringLiteral& node)
    {
        for(auto& part : node.parts)
        {
            part->accept(*this);
        }
        node.refType = Compiler::get().getTypeContext().getString();
    }
    
    void SemanticAnalyzer::visit(BoolLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getBool();
    }
    
    void SemanticAnalyzer::visit(CharLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getChar();
    }
    
    void SemanticAnalyzer::visit(ByteLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getU8();
    }
    
    void SemanticAnalyzer::visit(DurationLiteral& node)
    {
        node.refType = Compiler::get().getTypeContext().getF32();
    }
    
    void SemanticAnalyzer::visit(ArrayLiteral& node)
    {
        if (node.elements.empty())
        {
            if (currentExpectedExpressionType_)
            {
                Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
                if (expectedType && expectedType->kind() == TypeKind::Array)
                {
                    node.refType = expectedType;
                    return;
                }
            }

            // Fall back to an empty unknown array when no surrounding context can
            // provide the intended element type.
            node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(
                Compiler::get().getTypeContext().getUnknown(),
                ArrayType::ArrayKind::Static,
                0
            );
            return;
        }

        node.elements[0]->accept(*this);
        Ref<Type> baseType = node.elements[0]->refType.Lock();

        for (size_t i = 1; i < node.elements.size(); ++i)
        {
            node.elements[i]->accept(*this);
            if (auto lockedType = node.elements[i]->refType.Lock(); lockedType)
            {
                if (!baseType->isCompatibleWith(lockedType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.elements[i]->location(),
                        "Array elements must be of the same type. Expected '{}', but found '{}'.",
                        baseType->toString(),
                        lockedType->toString()
                    );
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.elements[i]->location(), "Undefined element type in array.");
            }
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(baseType, ArrayType::ArrayKind::Literal, node.elements.size());
    }
    
    void SemanticAnalyzer::visit(DictionaryLiteral& node)
    {
        if (node.pairs.empty())
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.pairs[0].first->accept(*this);
        node.pairs[0].second->accept(*this);

        auto keyType = node.pairs[0].first->refType.Lock();
        auto valType = node.pairs[0].second->refType.Lock();

        for (size_t i = 1; i < node.pairs.size(); ++i)
        {
            node.pairs[i].first->accept(*this);
            node.pairs[i].second->accept(*this);
            
            auto k = node.pairs[i].first->refType.Lock();
            auto v = node.pairs[i].second->refType.Lock();

            if (!keyType->isCompatibleWith(k) || !valType->isCompatibleWith(v))
            {
                WIO_LOG_ADD_ERROR(node.pairs[i].first->location(), "All keys and values in a dictionary literal must have the same type.");
            }
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateDictionaryType(keyType, valType, node.isOrdered);
    }
    
    void SemanticAnalyzer::visit(Identifier& node)
    {
        Ref<Symbol> sym = currentScope_->resolve(node.token.value);

        if (!sym)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Undefined symbol: '{}'", node.token.value);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = sym;
        node.refType = sym->type;
    }
    
    void SemanticAnalyzer::visit(NullExpression& node)
    {
        node.refType = Compiler::get().getTypeContext().getNull();
    }

    void SemanticAnalyzer::visit(ArrayAccessExpression& node)
    {
        node.object->accept(*this);
        Ref<Type> objType = node.object->refType.Lock();
        Ref<Type> resolvedObjType = unwrapAliasType(objType);

        node.index->accept(*this);
        Ref<Type> idxType = node.index->refType.Lock();

        if (!resolvedObjType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (resolvedObjType->kind() != TypeKind::Array && !isStringType(resolvedObjType))
        {
            WIO_LOG_ADD_ERROR(node.object->location(), "Type '{}' is not an array or string and cannot be indexed.", objType->toString());
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        if (!idxType->isNumeric())
        {
            WIO_LOG_ADD_ERROR(node.index->location(), "Array index must be an integer.");
        }
        
        if (resolvedObjType->kind() == TypeKind::Array)
        {
            auto arrType = resolvedObjType.AsFast<ArrayType>();
            node.refType = arrType->elementType;
            return;
        }

        node.refType = Compiler::get().getTypeContext().getChar();
    }
    
    void SemanticAnalyzer::visit(MemberAccessExpression& node)
    {
        node.object->accept(*this);
        node.intrinsicMember = IntrinsicMember::None;
        node.intrinsicOverloadMembers.clear();
        node.intrinsicOverloadTypes.clear();

        Ref<Symbol> leftSymbol = node.object->referencedSymbol.Lock();
        Ref<Type> leftType = nullptr;
        Ref<Symbol> foundMember = nullptr;
        
        if (auto lockedType = node.object->refType.Lock(); lockedType)
        {
            bool isNamespace = (leftSymbol && leftSymbol->kind == SymbolKind::Namespace);

            if (!isNamespace && (!lockedType || lockedType->isUnknown()))
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            leftType = lockedType;
        }

        std::function<Ref<Symbol>(Ref<Type>, const std::string&)> findMemberInHierarchy = 
            [&](const Ref<Type>& t, const std::string& name) -> Ref<Symbol> {
                if (!t || t->kind() != TypeKind::Struct)
                    return nullptr;
                auto sType = t.AsFast<StructType>();
            
                if (auto lockedScope = sType->structScope.Lock(); lockedScope)
                {
                    if (auto found = lockedScope->resolveLocally(name); found)
                        return found;
                }
                for (auto& base : sType->baseTypes)
                {
                    if (auto found = findMemberInHierarchy(base, name); found)
                        return found;
                }
                return nullptr;
        };

        Ref<Type> actualStructType = nullptr;
        auto resolveIntrinsicMemberOnType = [&](const Ref<Type>& candidateType) -> bool
        {
            auto overloads = resolveIntrinsicMemberOverloads(Compiler::get().getTypeContext(), candidateType, node.member->token.value);
            if (!overloads.empty())
            {
                if (overloads.size() == 1)
                {
                    const auto& resolution = overloads.front();
                    node.intrinsicMember = resolution.member;
                    node.refType = resolution.memberType;

                    if (resolution.requiresMutableReceiver && !canMutateIntrinsicReceiver(node.object))
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Container member '{}' requires a mutable receiver.", node.member->token.value);
                    }

                    if (auto memberId = node.member.Get(); memberId)
                        memberId->refType = resolution.memberType;
                }
                else
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    node.intrinsicMember = IntrinsicMember::None;
                    node.intrinsicOverloadMembers.reserve(overloads.size());
                    node.intrinsicOverloadTypes.reserve(overloads.size());

                    for (const auto& overload : overloads)
                    {
                        node.intrinsicOverloadMembers.push_back(overload.member);
                        node.intrinsicOverloadTypes.emplace_back(overload.memberType);
                    }

                    if (auto memberId = node.member.Get(); memberId)
                        memberId->refType = Compiler::get().getTypeContext().getUnknown();
                }

                return true;
            }

            if (isUnsupportedStaticArrayMember(candidateType, node.member->token.value))
            {
                WIO_LOG_ADD_ERROR(
                    node.member->location(),
                    "Static arrays do not support member '{}'. Use a dynamic array instead.",
                    node.member->token.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            return false;
        };
        
        if (leftSymbol && leftSymbol->kind == SymbolKind::Namespace)
        {
            if (!leftSymbol->innerScope)
            {
                WIO_LOG_ADD_ERROR(node.location(), "The namespace contents are inaccessible. (No Scope): {}", leftSymbol->name);
                return;
            }
            foundMember = leftSymbol->innerScope->resolve(node.member->token.value);
        }
        else 
        {
            Ref<Type> baseType = leftType;
            while (baseType && baseType->kind() == TypeKind::Alias)
                baseType = baseType.AsFast<AliasType>()->aliasedType;

            if (!baseType)
            {
                WIO_LOG_ADD_ERROR(node.member->location(), "Cannot access member '{}'. The left-hand side has no resolved type.", node.member->token.value);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
    
            if (baseType->kind() == TypeKind::Struct)
            {
                actualStructType = baseType;
                foundMember = findMemberInHierarchy(actualStructType, node.member->token.value);
            }
            else if (isIntrinsicReceiverType(baseType))
            {
                if (resolveIntrinsicMemberOnType(baseType))
                    return;
            }
            else if (baseType->kind() == TypeKind::Reference)
            {
                Ref<Type> referredType = baseType.AsFast<ReferenceType>()->referredType;

                while (referredType)
                {
                    while (referredType && referredType->kind() == TypeKind::Alias)
                        referredType = referredType.AsFast<AliasType>()->aliasedType;

                    if (referredType && referredType->kind() == TypeKind::Reference)
                    {
                        referredType = referredType.AsFast<ReferenceType>()->referredType;
                        continue;
                    }

                    break;
                }
    
                if (referredType && referredType->kind() == TypeKind::Struct)
                {
                    actualStructType = referredType;
                    foundMember = findMemberInHierarchy(actualStructType, node.member->token.value);
                }
                else if (referredType && isIntrinsicReceiverType(referredType))
                {
                    if (resolveIntrinsicMemberOnType(referredType))
                        return;
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.member->location(), 
                        "Cannot access member '{}'. Reference points to '{}', which is not a struct, array, dictionary, or string.", 
                        node.member->token.value, 
                        referredType ? referredType->toString() : "Unknown");
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }
        }
    
        if (!foundMember)
        {
            std::string ownerName = leftType ? leftType->toString() : (leftSymbol ? leftSymbol->name : "<unknown>");
            WIO_LOG_ADD_ERROR(node.member->location(), "Member not found: '{}' in '{}'", node.member->token.value, ownerName);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        bool isInsideHierarchy = false;
        bool isInsideSameObject = false;

        if (currentStructType_ && actualStructType)
        {
            if (currentStructType_ == actualStructType || 
                isTypeDerivedFrom(currentStructType_, actualStructType) || 
                isTypeDerivedFrom(actualStructType, currentStructType_))
            {
                isInsideHierarchy = true;
                isInsideSameObject = true;
            }
        }

        if (foundMember->flags.get_isPrivate() && !isInsideSameObject)
            WIO_LOG_ADD_ERROR(node.location(), "Cannot access private member '{}' from outside the object.", foundMember->name);

        if (foundMember->flags.get_isProtected() && !isInsideHierarchy)
            WIO_LOG_ADD_ERROR(node.location(), "Cannot access protected member '{}' from outside the object hierarchy.", foundMember->name);

        Ref<Type> memberType = foundMember->type;
        if (auto instantiatedStructType = actualStructType ? actualStructType.AsFast<StructType>() : nullptr;
            instantiatedStructType && !instantiatedStructType->genericParameterNames.empty() && !instantiatedStructType->genericArguments.empty())
        {
            auto bindings = buildGenericTypeBindings(instantiatedStructType->genericParameterNames, instantiatedStructType->genericArguments);
            memberType = instantiateGenericType(memberType, bindings);
        }

        node.referencedSymbol = foundMember;
        node.refType = memberType;
        
        if (auto memberId = node.member.Get(); memberId)
        {
            memberId->referencedSymbol = foundMember;
            memberId->refType = memberType;
        }
    }

    void SemanticAnalyzer::visit(FunctionCallExpression& node)
    {
        node.callee->accept(*this);
        Ref<Symbol> calleeSym = node.callee->referencedSymbol.Lock();
        std::vector<Ref<Type>> explicitTypeArguments;
        explicitTypeArguments.reserve(node.explicitTypeArguments.size());
        for (auto& explicitTypeArgument : node.explicitTypeArguments)
        {
            explicitTypeArgument->accept(*this);
            explicitTypeArguments.push_back(explicitTypeArgument->refType.Lock());
        }

        bool isConstructorCall = false;
        Ref<Type> structReturnType = nullptr;
        Ref<StructType> constructorStructType = nullptr;
        std::vector<std::string> constructorGenericParameterNames;
        std::unordered_map<std::string, Ref<Type>> constructorGenericBindings;
        bool useExplicitFunctionTypeArguments = false;

        if (calleeSym && calleeSym->kind == SymbolKind::Struct)
        {
            isConstructorCall = true;
            auto structType = calleeSym->type.AsFast<StructType>();
            constructorStructType = structType;
            constructorGenericParameterNames = structType->genericParameterNames;
            structReturnType = structType;

            if (!structType->genericParameterNames.empty())
            {
                if (!explicitTypeArguments.empty() && explicitTypeArguments.size() != structType->genericParameterNames.size())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic type '{}' expects {} generic arguments, but got {}.",
                        structType->name,
                        structType->genericParameterNames.size(),
                        explicitTypeArguments.size()
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                if (!explicitTypeArguments.empty())
                {
                    constructorGenericBindings = buildGenericTypeBindings(structType->genericParameterNames, explicitTypeArguments);
                    structReturnType = instantiateGenericStructType(structType, explicitTypeArguments);
                    node.callee->refType = structReturnType;
                }
                else if (currentExpectedExpressionType_)
                {
                    Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
                    if (expectedType && expectedType->kind() == TypeKind::Struct)
                    {
                        auto expectedStructType = expectedType.AsFast<StructType>();
                        if (expectedStructType &&
                            expectedStructType->name == structType->name &&
                            expectedStructType->scopePath == structType->scopePath &&
                            expectedStructType->genericArguments.size() == structType->genericParameterNames.size())
                        {
                            bool hasConcreteExpectedArguments = true;
                            for (const auto& genericArgument : expectedStructType->genericArguments)
                            {
                                if (!genericArgument || genericArgument->isUnknown() || containsGenericParameterType(genericArgument))
                                {
                                    hasConcreteExpectedArguments = false;
                                    break;
                                }
                            }

                            if (hasConcreteExpectedArguments)
                            {
                                constructorGenericBindings = buildGenericTypeBindings(
                                    structType->genericParameterNames,
                                    expectedStructType->genericArguments
                                );
                                structReturnType = instantiateGenericStructType(structType, expectedStructType->genericArguments);
                                node.callee->refType = structReturnType;
                            }
                        }
                    }
                }
            }
            else if (!explicitTypeArguments.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Type '{}' does not accept generic arguments.", structType->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (auto lockedScope = structType->structScope.Lock())
            {
                calleeSym = lockedScope->resolveLocally("OnConstruct");
            }
                
            if (!calleeSym)
            {
                WIO_LOG_ADD_ERROR(node.location(), "No constructor found for type '{}'.", structType->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        useExplicitFunctionTypeArguments = !isConstructorCall && !explicitTypeArguments.empty();

        std::vector<Ref<Type>> argTypes;
        for (auto& arg : node.arguments)
        {
            arg->accept(*this);
            argTypes.push_back(arg->refType.Lock());
        }

        auto isSafeRefCast = [&](const Ref<Type>& dest, const Ref<Type>& src) -> bool
        {
            if (dest && src && dest->kind() == TypeKind::Reference && src->kind() == TypeKind::Reference)
            {
                auto dRef = dest.AsFast<ReferenceType>();
                auto sRef = src.AsFast<ReferenceType>();
                
                bool isCompatible = isTypeDerivedFrom(sRef->referredType, dRef->referredType);

                if (!dRef->isMutable && isCompatible) return true;
                if (dRef->isMutable && sRef->isMutable && isCompatible) return true;
            }
            return false;
        };

        auto isImplicitObjectViewBridge = [&](const Ref<Type>& dest, const Ref<Type>& src) -> bool
        {
            Ref<Type> resolvedDest = unwrapAliasType(dest);
            Ref<Type> resolvedSrc = unwrapAliasType(src);

            if (!resolvedDest || !resolvedSrc || resolvedDest->kind() != TypeKind::Reference)
                return false;

            auto expectedRef = resolvedDest.AsFast<ReferenceType>();
            if (expectedRef->isMutable || resolvedSrc->kind() != TypeKind::Struct)
                return false;

            auto expectedTarget = unwrapAliasType(expectedRef->referredType);
            if (!expectedTarget || expectedTarget->kind() != TypeKind::Struct)
                return false;

            auto expectedStruct = expectedTarget.AsFast<StructType>();
            auto actualStruct = resolvedSrc.AsFast<StructType>();
            if ((!expectedStruct->isObject && !expectedStruct->isInterface) ||
                (!actualStruct->isObject && !actualStruct->isInterface))
            {
                return false;
            }

            return expectedTarget->isCompatibleWith(resolvedSrc) ||
                   isTypeDerivedFrom(resolvedSrc, expectedTarget);
        };

        std::vector<Ref<Symbol>> candidateSymbols;
        bool requiresOverloadResolution = false;
        if (calleeSym && calleeSym->kind == SymbolKind::FunctionGroup)
        {
            candidateSymbols = calleeSym->overloads;
            requiresOverloadResolution = true;
        }
        else if (calleeSym && calleeSym->kind == SymbolKind::Function)
        {
            candidateSymbols.push_back(calleeSym);
            Ref<Type> resolvedCalleeType = node.callee->refType.Lock();
            if (!resolvedCalleeType || resolvedCalleeType->kind() != TypeKind::Function)
                resolvedCalleeType = calleeSym->type;
            if (!constructorGenericBindings.empty())
                resolvedCalleeType = instantiateGenericType(resolvedCalleeType, constructorGenericBindings);

            requiresOverloadResolution = (!calleeSym->genericParameterNames.empty() && containsGenericParameterType(resolvedCalleeType)) ||
                                         useExplicitFunctionTypeArguments ||
                                         (isConstructorCall && !constructorGenericParameterNames.empty()) ||
                                         !constructorGenericBindings.empty();
        }

        if (requiresOverloadResolution)
        {
            struct ResolvedFunctionCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<Type> functionType = nullptr;
                int score = -1;
                std::unordered_map<std::string, Ref<Type>> genericBindings;
            };

            auto formatInstantiationSignature = [&](const std::vector<Ref<Type>>& types) -> std::string
            {
                std::string signature = "<";
                for (size_t i = 0; i < types.size(); ++i)
                {
                    signature += types[i] ? types[i]->toString() : "<unknown>";
                    if (i + 1 < types.size())
                        signature += ", ";
                }
                signature += ">";
                return signature;
            };

            auto isAllowedInstantiateBinding = [&](const Ref<Symbol>& overload,
                                                   const std::vector<std::string>& activeGenericParameterNames,
                                                   const std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!overload || activeGenericParameterNames.empty())
                    return true;

                auto foundDeclaration = functionDeclarationsBySymbol_.find(overload.Get());
                if (foundDeclaration == functionDeclarationsBySymbol_.end() || !foundDeclaration->second)
                    return true;

                const auto* functionDeclaration = foundDeclaration->second;
                if (!hasAttribute(functionDeclaration->attributes, Attribute::Native) &&
                    !hasAttribute(functionDeclaration->attributes, Attribute::Export))
                {
                    return true;
                }

                auto instantiateAttributes = getAttributeStatements(functionDeclaration->attributes, Attribute::Instantiate);
                if (instantiateAttributes.empty())
                    return false;

                std::vector<Ref<Type>> resolvedBindingTypes;
                resolvedBindingTypes.reserve(activeGenericParameterNames.size());
                for (const auto& genericParameterName : activeGenericParameterNames)
                {
                    auto foundBinding = bindings.find(genericParameterName);
                    if (foundBinding == bindings.end() || !foundBinding->second)
                        return false;

                    resolvedBindingTypes.push_back(foundBinding->second);
                }

                if (std::ranges::any_of(resolvedBindingTypes, [](const Ref<Type>& bindingType)
                {
                    return containsGenericParameterType(bindingType);
                }))
                {
                    return true;
                }

                for (const auto* instantiateAttribute : instantiateAttributes)
                {
                    if (!instantiateAttribute || instantiateAttribute->typeArgs.size() != resolvedBindingTypes.size())
                        continue;

                    bool matches = true;
                    for (size_t i = 0; i < instantiateAttribute->typeArgs.size(); ++i)
                    {
                        auto typeSpecifier = instantiateAttribute->typeArgs[i];
                        if (!typeSpecifier)
                        {
                            matches = false;
                            break;
                        }

                        typeSpecifier->accept(*this);
                        Ref<Type> declaredInstantiationType = typeSpecifier->refType.Lock();
                        if (!declaredInstantiationType || !isExactType(declaredInstantiationType, resolvedBindingTypes[i]))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (matches)
                        return true;
                }

                return false;
            };

            auto scoreResolvedCall = [&](const Ref<FunctionType>& functionType, bool isGenericCandidate, size_t genericParameterCount) -> std::optional<int>
            {
                if (!functionType || functionType->paramTypes.size() != argTypes.size())
                    return std::nullopt;

                int currentScore = 0;
                for (size_t i = 0; i < argTypes.size(); ++i)
                {
                    auto dest = functionType->paramTypes[i];
                    const auto& src = argTypes[i];

                    if (dest->isCompatibleWith(src) || (dest->isNumeric() && src->isNumeric()))
                    {
                        if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive && 
                            dest.AsFast<PrimitiveType>()->name == src.AsFast<PrimitiveType>()->name)
                        {
                            currentScore += 1000; 
                        }
                        else if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive)
                        {
                            currentScore += 100;
                            auto destName = dest.AsFast<PrimitiveType>()->name;
                            auto srcName = src.AsFast<PrimitiveType>()->name;
                            bool destIsUn = destName.starts_with('u');
                            bool srcIsUn = srcName.starts_with('u');
                            bool destIsInt = destName.starts_with('i');
                            bool srcIsInt = srcName.starts_with('i');
                            bool destIsFlt = destName.starts_with('f');
                            bool srcIsFlt = srcName.starts_with('f');

                            if ((destIsUn && srcIsUn) || (destIsInt && srcIsInt) || (destIsFlt && srcIsFlt)) currentScore += 50;

                            auto getSize = [](const std::string& s) -> int
                            {
                                if (s.ends_with("8")) return 1;
                                if (s.ends_with("16")) return 2;
                                if (s.ends_with("32")) return 4;
                                if (s.ends_with("64") || s == "isize" || s == "usize") return 8;
                                return 4;
                            };

                            int sizeDiff = getSize(destName) - getSize(srcName);
                            if (sizeDiff >= 0) currentScore += (10 - sizeDiff); 
                        }
                        else
                        {
                            currentScore += 10;
                        }
                    }
                    else if (isImplicitObjectViewBridge(dest, src))
                    {
                        currentScore += 9;
                    }
                    else if (isSafeRefCast(dest, src)) 
                    {
                        currentScore += 5; 
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }

                if (isGenericCandidate)
                    currentScore -= static_cast<int>(std::max<size_t>(1, genericParameterCount));

                return currentScore;
            };

            bool rejectedByInstantiationWhitelist = false;
            std::string rejectedInstantiationFunctionName;
            std::string rejectedInstantiationSignature;

            auto tryResolveFunctionCandidate = [&](const Ref<Symbol>& overload) -> std::optional<ResolvedFunctionCandidate>
            {
                if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                    return std::nullopt;

                Ref<Type> resolvedFunctionType = overload->type;
                if (overload == calleeSym)
                {
                    if (auto calleeResolvedType = node.callee->refType.Lock();
                        calleeResolvedType && calleeResolvedType->kind() == TypeKind::Function)
                    {
                        resolvedFunctionType = calleeResolvedType;
                    }
                }
                if (!constructorGenericBindings.empty())
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, constructorGenericBindings);

                auto declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                const std::vector<std::string>& activeGenericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                bool isGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                if (useExplicitFunctionTypeArguments && !isGenericCandidate)
                    return std::nullopt;

                std::unordered_map<std::string, Ref<Type>> bindings;

                if (isGenericCandidate)
                {
                    if (useExplicitFunctionTypeArguments)
                    {
                        if (activeGenericParameterNames.size() != explicitTypeArguments.size())
                            return std::nullopt;

                        for (size_t i = 0; i < explicitTypeArguments.size(); ++i)
                        {
                            if (!explicitTypeArguments[i] || explicitTypeArguments[i]->isUnknown())
                                return std::nullopt;

                            bindings.emplace(activeGenericParameterNames[i], explicitTypeArguments[i]);
                        }
                    }

                    if (bindings.empty())
                    {
                        for (size_t i = 0; i < argTypes.size(); ++i)
                        {
                            if (!deduceGenericBindings(declaredFunctionType->paramTypes[i], argTypes[i], bindings))
                                return std::nullopt;
                        }
                    }

                    for (const auto& genericParameterName : activeGenericParameterNames)
                    {
                        if (!bindings.contains(genericParameterName) ||
                            !bindings.at(genericParameterName) ||
                            bindings.at(genericParameterName)->isUnknown())
                        {
                            return std::nullopt;
                        }
                    }

                    if (!isAllowedInstantiateBinding(overload, activeGenericParameterNames, bindings))
                    {
                        std::vector<Ref<Type>> resolvedBindingTypes;
                        resolvedBindingTypes.reserve(activeGenericParameterNames.size());
                        for (const auto& genericParameterName : activeGenericParameterNames)
                        {
                            auto foundBinding = bindings.find(genericParameterName);
                            if (foundBinding != bindings.end())
                                resolvedBindingTypes.push_back(foundBinding->second);
                        }

                        rejectedByInstantiationWhitelist = true;
                        rejectedInstantiationFunctionName = overload->name;
                        rejectedInstantiationSignature = formatInstantiationSignature(resolvedBindingTypes);
                        return std::nullopt;
                    }

                    std::vector<Ref<Type>> instantiatedParamTypes;
                    instantiatedParamTypes.reserve(declaredFunctionType->paramTypes.size());
                    for (const auto& paramType : declaredFunctionType->paramTypes)
                    {
                        Ref<Type> instantiatedType = instantiateGenericType(paramType, bindings);
                        if (!instantiatedType)
                            return std::nullopt;

                        instantiatedParamTypes.push_back(instantiatedType);
                    }

                    Ref<Type> instantiatedReturnType = instantiateGenericType(declaredFunctionType->returnType, bindings);
                    if (!instantiatedReturnType)
                        return std::nullopt;

                    resolvedFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                        instantiatedReturnType,
                        instantiatedParamTypes
                    );
                }

                if (auto score = scoreResolvedCall(
                        resolvedFunctionType.AsFast<FunctionType>(),
                        isGenericCandidate,
                        activeGenericParameterNames.size()
                    ); score.has_value())
                    return ResolvedFunctionCandidate{
                        .symbol = overload,
                        .functionType = resolvedFunctionType,
                        .score = *score,
                        .genericBindings = bindings
                    };

                return std::nullopt;
            };

            std::optional<ResolvedFunctionCandidate> bestMatch;
            bool isAmbiguous = false;

            for (const auto& overload : candidateSymbols)
            {
                auto resolvedCandidate = tryResolveFunctionCandidate(overload);
                if (!resolvedCandidate.has_value())
                    continue;

                if (!bestMatch.has_value() || resolvedCandidate->score > bestMatch->score)
                {
                    bestMatch = resolvedCandidate;
                    isAmbiguous = false;
                }
                else if (resolvedCandidate->score == bestMatch->score)
                {
                    isAmbiguous = true;
                }
            }

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Ambiguous function call to '{}'.", calleeSym->name);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
            if (!bestMatch.has_value())
            {
                if (rejectedByInstantiationWhitelist)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic interop function '{}' does not declare @Instantiate{}.",
                        rejectedInstantiationFunctionName,
                        rejectedInstantiationSignature
                    );
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "No matching function/constructor overload found.");
                }
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!isConstructorCall)
            {
                node.callee->referencedSymbol = bestMatch->symbol;
                node.callee->refType = bestMatch->functionType;
            }
            else if (constructorStructType && !constructorGenericParameterNames.empty() && !constructorGenericBindings.empty())
            {
                node.callee->refType = structReturnType;
            }
            else if (constructorStructType && !constructorGenericParameterNames.empty())
            {
                std::vector<Ref<Type>> deducedGenericArguments;
                deducedGenericArguments.reserve(constructorGenericParameterNames.size());
                for (const auto& genericParameterName : constructorGenericParameterNames)
                {
                    auto foundBinding = bestMatch->genericBindings.find(genericParameterName);
                    if (foundBinding == bestMatch->genericBindings.end() || !foundBinding->second)
                    {
                        WIO_LOG_ADD_ERROR(node.location(),
                            "Cannot infer generic arguments for constructor '{}'. Use explicit type arguments.",
                            constructorStructType->name);
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return;
                    }

                    deducedGenericArguments.push_back(foundBinding->second);
                }

                constructorGenericBindings = bestMatch->genericBindings;
                structReturnType = instantiateGenericStructType(constructorStructType, deducedGenericArguments);
                node.callee->refType = structReturnType;
            }
            
            node.refType = isConstructorCall ? structReturnType : bestMatch->functionType.AsFast<FunctionType>()->returnType;
            return; 
        }

        Ref<Type> calleeType = node.callee->refType.Lock();
        if (auto* memberAccess = node.callee->as<MemberAccessExpression>();
            memberAccess && !memberAccess->intrinsicOverloadMembers.empty())
        {
            auto scoreIntrinsicOverload = [&](const Ref<FunctionType>& functionType) -> std::optional<int>
            {
                if (!functionType || functionType->paramTypes.size() != argTypes.size())
                    return std::nullopt;

                int currentScore = 0;
                for (size_t i = 0; i < argTypes.size(); ++i)
                {
                    auto dest = functionType->paramTypes[i];
                    const auto& src = argTypes[i];

                    if (dest->isCompatibleWith(src) || (dest->isNumeric() && src->isNumeric()))
                    {
                        if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive &&
                            dest.AsFast<PrimitiveType>()->name == src.AsFast<PrimitiveType>()->name)
                        {
                            currentScore += 1000;
                        }
                        else if (dest->kind() == TypeKind::Primitive && src->kind() == TypeKind::Primitive)
                        {
                            currentScore += 100;
                        }
                        else
                        {
                            currentScore += 10;
                        }
                    }
                    else if (isImplicitObjectViewBridge(dest, src))
                    {
                        currentScore += 9;
                    }
                    else if (isSafeRefCast(dest, src))
                    {
                        currentScore += 5;
                    }
                    else
                    {
                        return std::nullopt;
                    }
                }

                return currentScore;
            };

            std::optional<size_t> bestIndex;
            std::optional<int> bestScore;
            bool isAmbiguous = false;

            for (size_t i = 0; i < memberAccess->intrinsicOverloadTypes.size(); ++i)
            {
                Ref<Type> overloadType = memberAccess->intrinsicOverloadTypes[i].Lock();
                if (!overloadType || overloadType->kind() != TypeKind::Function)
                    continue;

                auto score = scoreIntrinsicOverload(overloadType.AsFast<FunctionType>());
                if (!score.has_value())
                    continue;

                if (!bestIndex.has_value() || *score > *bestScore)
                {
                    bestIndex = i;
                    bestScore = *score;
                    isAmbiguous = false;
                }
                else if (*score == *bestScore)
                {
                    isAmbiguous = true;
                }
            }

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Ambiguous function call to intrinsic member '{}'.", memberAccess->member->token.value);
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!bestIndex.has_value())
            {
                WIO_LOG_ADD_ERROR(node.location(), "No matching function/constructor overload found.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            memberAccess->intrinsicMember = memberAccess->intrinsicOverloadMembers[*bestIndex];
            memberAccess->refType = memberAccess->intrinsicOverloadTypes[*bestIndex].Lock();
            node.callee->refType = memberAccess->refType;
            calleeType = memberAccess->refType.Lock();

            if (isMutatingIntrinsicMember(memberAccess->intrinsicMember) && !canMutateIntrinsicReceiver(memberAccess->object))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Container member '{}' requires a mutable receiver.", memberAccess->member->token.value);
            }

            if (!calleeType || calleeType->kind() != TypeKind::Function)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Called expression is undefined.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = calleeType.AsFast<FunctionType>()->returnType;
            return;
        }

        if (!calleeSym && (!calleeType || calleeType->kind() != TypeKind::Function))
        {
            WIO_LOG_ADD_ERROR(node.location(), "Called expression is undefined.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (!calleeType || calleeType->kind() != TypeKind::Function)
            calleeType = calleeSym->type;
        if (!constructorGenericBindings.empty())
            calleeType = instantiateGenericType(calleeType, constructorGenericBindings);

        if (!calleeType || calleeType->kind() != TypeKind::Function)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Called expression is not a function or struct.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto funcType = calleeType.AsFast<FunctionType>();
        if (argTypes.size() != funcType->paramTypes.size())
            WIO_LOG_ADD_ERROR(node.location(), "Function expects '{}' arguments, but got '{}'.", funcType->paramTypes.size(), argTypes.size());
        
        size_t argCount = std::min(argTypes.size(), funcType->paramTypes.size());
        for (size_t i = 0; i < argCount; ++i)
        {
            auto expectedType = funcType->paramTypes[i];
            const auto& actualType = argTypes[i];

            if (!expectedType->isCompatibleWith(actualType) && 
                !isImplicitObjectViewBridge(expectedType, actualType) &&
                !isSafeRefCast(expectedType, actualType) &&
                !(expectedType->isNumeric() && actualType->isNumeric()))
            {
                std::string extraHint;
                
                if (expectedType->kind() == TypeKind::Reference && actualType->kind() == TypeKind::Reference)
                {
                    auto expRef = expectedType.AsFast<ReferenceType>();
                    auto actRef = actualType.AsFast<ReferenceType>();
                    
                    if (expRef->isMutable && !actRef->isMutable)
                    {
                        extraHint = " Hint: The function expects a mutable reference ('ref'), but a read-only reference ('view') or immutable variable ('let') was provided.";
                    }
                    else if (!isTypeDerivedFrom(actRef->referredType, expRef->referredType))
                    {
                        extraHint = " Hint: Type '" + actRef->referredType->toString() + "' does not inherit from '" + expRef->referredType->toString() + "'.";
                    }
                }

                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), 
                    "Argument mismatch at index {}: Expected '{}', but got '{}'.{}", 
                    i, 
                    expectedType->toString(), 
                    actualType->toString(),
                    extraHint);
            }
        }

        node.refType = isConstructorCall ? structReturnType : funcType->returnType;
    }

    void SemanticAnalyzer::visit(LambdaExpression& node)
    {
        enterScope(ScopeKind::Function);

        std::vector<Ref<Type>> paramTypes;
        for (auto& param : node.parameters)
        {
            Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
            if (param.type)
            {
                param.type->accept(*this);
                pType = param.type->refType.Lock();
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Lambda parameters must have explicit types.");
            }
            paramTypes.push_back(pType);

            Ref<Symbol> paramSym = createSymbol(param.name->token.value, pType, SymbolKind::Variable, param.name->location());
            currentScope_->define(param.name->token.value, paramSym);
            param.name->referencedSymbol = paramSym;
            param.name->refType = pType;
        }

        Ref<Type> retType = Compiler::get().getTypeContext().getVoid();
        if (node.returnType)
        {
            node.returnType->accept(*this);
            retType = node.returnType->refType.Lock();
        }

        if (node.body)
        {
            node.body->accept(*this);
            
            if (!node.returnType)
            {
                if (node.body->is<ExpressionStatement>())
                {
                    retType = node.body->as<ExpressionStatement>()->expression->refType.Lock();
                } 
                else if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    for (auto& stmt : block->statements)
                    {
                        if (stmt->is<ReturnStatement>())
                        {
                            auto retStmt = stmt->as<ReturnStatement>();
                            if (retStmt->value)
                            {
                                retType = retStmt->value->refType.Lock();
                            }
                            break;
                        }
                    }
                }
            }
        }

        exitScope();

        node.refType = Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes);
    }
    
    void SemanticAnalyzer::visit(RefExpression& node)
    {
        node.operand->accept(*this);
        
        bool isMut; 

        if (auto lockedSym = node.operand->referencedSymbol.Lock(); lockedSym)
            isMut = lockedSym->flags.get_isMutable();
        else
            isMut = false; 

        node.isMut = isMut;
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(node.operand->refType.Lock(), isMut);
    }

    void SemanticAnalyzer::visit(FitExpression& node)
    {
        node.operand->accept(*this);
        node.targetType->accept(*this);

        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();

        if (srcType && destType)
        {
            if (srcType->isNumeric() && destType->isNumeric())
            {
                node.refType = destType;
                return;
            }
            
            bool isSrcObject = (srcType->kind() == TypeKind::Reference || srcType->kind() == TypeKind::Struct);
            bool isDestObject = (destType->kind() == TypeKind::Reference || destType->kind() == TypeKind::Struct);

            if (isSrcObject && isDestObject) 
            {
                node.refType = destType;
                return;
            }

            WIO_LOG_ADD_ERROR(node.location(), "The 'fit' operator can only be used with numeric types or objects/interfaces.");
        }

        node.refType = destType;
    }

    void SemanticAnalyzer::visit(SelfExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;
        
        if (!currentStructType_) {
            WIO_LOG_ADD_ERROR(node.location(), "'self' can only be used inside a component or object method.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(currentStructType_, true);
    }

    void SemanticAnalyzer::visit(SuperExpression& node)
    {
        if (isDeclarationPass_ || isStructResolutionPass_) return;
        
        if (!currentStructType_ || !currentBaseStructType_)
        {
            WIO_LOG_ADD_ERROR(node.location(), "'super' can only be used inside an object method that has a base class.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(currentBaseStructType_, true);
    }

    void SemanticAnalyzer::visit(RangeExpression& node)
    {
        node.start->accept(*this);
        node.end->accept(*this);
        
        auto sType = node.start->refType.Lock();
        auto eType = node.end->refType.Lock();
        
        if (sType && eType && (!sType->isNumeric() || !eType->isNumeric()))
        {
            WIO_LOG_ADD_ERROR(node.location(), "Range bounds must be numeric types.");
        }
        node.refType = Compiler::get().getTypeContext().getUnknown(); 
    }

    void SemanticAnalyzer::visit(MatchExpression& node)
    {
        node.value->accept(*this);
        
        Ref<Type> commonReturnType = nullptr;
        bool isVoidMatch = false;

        for (auto& matchCase : node.cases)
        {
            for (auto& val : matchCase.matchValues)
            {
                val->accept(*this);
            }
            matchCase.body->accept(*this);

            if (matchCase.body->is<BlockStatement>())
            {
                isVoidMatch = true;
            }
            else if (matchCase.body->is<ExpressionStatement>())
            {
                auto exprStmt = matchCase.body->as<ExpressionStatement>();
                auto bodyType = exprStmt->expression->refType.Lock();
                if (!commonReturnType)
                    commonReturnType = bodyType;
            }
        }
        
        if (isVoidMatch)
            node.refType = Compiler::get().getTypeContext().getVoid();
        else
            node.refType = commonReturnType ? commonReturnType : Compiler::get().getTypeContext().getVoid();
    }
    
    void SemanticAnalyzer::visit(ExpressionStatement& node) 
    {
        if (isDeclarationPass_) return;
        node.expression->accept(*this);
    }

    void SemanticAnalyzer::visit(AttributeStatement& node)
    {
        WIO_UNUSED(node);
    }
    
    void SemanticAnalyzer::visit(VariableDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            if (currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block)
                return;
    
            Ref<Type> declaredType = nullptr;
            if (node.type)
            {
                node.type->accept(*this);
                declaredType = node.type->refType.Lock();
            }
            
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (currentScope_->getKind() == ScopeKind::Global) flags.set_isGlobal(true);
    
            Ref<Symbol> sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
            currentScope_->define(node.name->token.value, sym);
            node.name->referencedSymbol = sym;
            
            return;
        }
    
        Ref<Symbol> sym = node.name->referencedSymbol.Lock();
        
        if (!sym)
        {
            Ref<Type> declaredType = nullptr;
            if (node.type)
            {
                node.type->accept(*this);
                declaredType = node.type->refType.Lock();
            }
            
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            
            sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
            currentScope_->define(node.name->token.value, sym);
            node.name->referencedSymbol = sym;
        }
    
        if (node.initializer)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = sym->type;
            node.initializer->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            Ref<Type> initType = node.initializer->refType.Lock();
    
            if (!sym->type || sym->type->isUnknown()) 
            {
                sym->type = initType;
                node.name->refType = initType;
            }
            else if (!sym->type->isCompatibleWith(initType)) 
            {
                if (sym->type->isNumeric() && initType && initType->isNumeric())
                {
                    // No Problem!
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Type mismatch for '{}'.", node.name->token.value);
                }
            }
        }
    }

    void SemanticAnalyzer::visit(TypeAliasDeclaration& node)
    {
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (const auto& genericParameter : node.genericParameters)
            {
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this type alias.", parameterName);
                    continue;
                }

                Ref<Type> parameterType = Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (!isDeclarationPass_)
        {
            if (!node.name->referencedSymbol.Lock())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Local type aliases are not supported yet. Declare type aliases at global or realm scope.");
            }
            return;
        }

        auto genericScope = buildGenericTypeParameterScope();
        genericTypeParameterScopes_.push_back(genericScope);

        node.aliasedType->accept(*this);
        Ref<Type> aliasedType = node.aliasedType->refType.Lock();
        Ref<Type> aliasType = Compiler::get().getTypeContext().getOrCreateAliasType(node.name->token.value, aliasedType);
        Ref<Symbol> aliasSym = createSymbol(node.name->token.value, aliasType, SymbolKind::TypeAlias, node.location());
        aliasSym->aliasTargetType = aliasedType;
        aliasSym->genericParameterNames.reserve(node.genericParameters.size());
        for (const auto& genericParameter : node.genericParameters)
        {
            if (genericParameter)
                aliasSym->genericParameterNames.push_back(genericParameter->token.value);
        }

        currentScope_->define(node.name->token.value, aliasSym);
        node.name->referencedSymbol = aliasSym;
        node.name->refType = aliasType;

        genericTypeParameterScopes_.pop_back();
    }
    
    void SemanticAnalyzer::visit(FunctionDeclaration& node)
    {
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (const auto& genericParameter : node.genericParameters)
            {
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this function.", parameterName);
                    continue;
                }

                Ref<Type> parameterType = Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            Ref<Type> returnType = Compiler::get().getTypeContext().getVoid();
            if (node.returnType)
            {
                node.returnType->accept(*this);
                returnType = node.returnType->refType.Lock();
            }

            std::vector<Ref<Type>> paramTypes;
            for (auto& param : node.parameters)
            {
                Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
                if (param.type)
                {
                    param.type->accept(*this);
                    pType = param.type->refType.Lock();
                }
                paramTypes.push_back(pType);
            }

            auto funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(returnType, paramTypes);
            
            Ref<Symbol> funcSym = createSymbol(node.name->token.value, funcType, SymbolKind::Function, node.location());
            funcSym->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    funcSym->genericParameterNames.push_back(genericParameter->token.value);
            }
            currentScope_->define(node.name->token.value, funcSym);
            functionDeclarationsBySymbol_[funcSym.Get()] = &node;

            node.name->refType = funcType;
            node.name->referencedSymbol = funcSym;
            genericTypeParameterScopes_.pop_back();
            return;
        }

        auto funcSym = node.name->referencedSymbol.Lock();
        auto funcType = funcSym->type.AsFast<FunctionType>();

        bool isNative = hasAttribute(node.attributes, Attribute::Native);
        bool isExported = hasAttribute(node.attributes, Attribute::Export);
        bool isCommand = hasAttribute(node.attributes, Attribute::Command);
        bool isEvent = hasAttribute(node.attributes, Attribute::Event);
        std::vector<Attribute> moduleLifecycleAttributes = getModuleLifecycleAttributes(node.attributes);
        bool hasModuleLifecycle = !moduleLifecycleAttributes.empty();
        bool isGenericFunction = !node.genericParameters.empty();
        bool isStructMethod = currentScope_ && currentScope_->getKind() == ScopeKind::Struct;
        auto instantiateAttributes = getAttributeStatements(node.attributes, Attribute::Instantiate);
        bool hasInstantiate = !instantiateAttributes.empty();
        if (isCommand && isEvent)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Command and @Event cannot be combined on the same function.");
        }

        if (isNative && isExported)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Export cannot be combined with @Native.");
        }

        if (hasInstantiate && !isGenericFunction)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Instantiate can only be used on generic functions.");
        }

        if (isGenericFunction)
        {
            if (isStructMethod)
            {
                auto currentStruct = currentStructType_ ? currentStructType_.AsFast<StructType>() : nullptr;
                if (!currentStruct || !currentStruct->isObject)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic methods are currently supported only on object methods.");
                }
                else if (node.name->token.value == "OnConstruct" || node.name->token.value == "OnDestruct")
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic constructors and destructors are not supported yet.");
                }
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as a generic function.");
            }

            if (hasInstantiate && isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Instantiate is currently supported only on top-level generic functions.");
            }

            if (hasInstantiate && !isNative && !isExported)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Instantiate is currently supported only together with @Native or @Export.");
            }

            if ((isNative || isExported) && !hasInstantiate)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic {} functions require at least one @Instantiate(...) declaration.",
                    isNative ? "@Native" : "@Export"
                );
            }

            if (isCommand || isEvent || hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Generic functions cannot currently use C ABI or module export attributes.");
            }
        }
        else if (hasInstantiate)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Instantiate can only be used on generic functions.");
        }

        if ((isCommand || isEvent) && !isExported)
        {
            WIO_LOG_ADD_ERROR(node.location(), "{} requires @Export on the same function.", isCommand ? "@Command" : "@Event");
        }

        if (isCommand)
        {
            auto commandArgs = getAttributeArgs(node.attributes, Attribute::Command);
            if (commandArgs.size() > 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Command accepts at most one command name argument.");
            }
            else if (!commandArgs.empty() &&
                     commandArgs.front().type != TokenType::identifier &&
                     commandArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Command expects an identifier path or a string literal command name.");
            }
        }

        if (isEvent)
        {
            auto eventArgs = getAttributeArgs(node.attributes, Attribute::Event);
            if (eventArgs.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event expects exactly one event name argument.");
            }
            else if (eventArgs.front().type != TokenType::identifier &&
                     eventArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event expects an identifier path or a string literal event name.");
            }

            if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Event hooks must return void.");
            }
        }

        if (isNative)
        {
            if (node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Native functions cannot define a Wio body. Declare them with ';' only.");
            }

            if (currentScope_ && currentScope_->getKind() == ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Native is currently supported only for top-level functions.");
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as @Native.");
            }
        }

        if (isExported && !isGenericFunction)
        {
            if (isNative)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export cannot be combined with @Native.");
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export functions must define a Wio body.");
            }

            if (currentScope_ && currentScope_->getKind() == ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@Export is currently supported only for top-level functions.");
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry cannot be declared as @Export.");
            }

            for (size_t i = 0; i < funcType->paramTypes.size(); ++i)
            {
                if (!isCAbiSafeExportType(funcType->paramTypes[i]))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export currently supports only primitive parameter types. Parameter {} in '{}' uses '{}'.",
                        i,
                        node.name->token.value,
                        funcType->paramTypes[i] ? funcType->paramTypes[i]->toString() : "<unknown>"
                    );
                }
            }

            if (!isCAbiSafeExportType(funcType->returnType))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Export currently supports only primitive or void return types. '{}' returns '{}'.",
                    node.name->token.value,
                    funcType->returnType ? funcType->returnType->toString() : "<unknown>"
                );
            }
        }

        if (moduleLifecycleAttributes.size() > 1)
        {
            WIO_LOG_ADD_ERROR(node.location(), "A function can declare only one module lifecycle attribute.");
        }

        if (hasModuleLifecycle)
        {
            Attribute lifecycleAttribute = moduleLifecycleAttributes.front();
            bool* seenLifecycleFlag = nullptr;
            
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (lifecycleAttribute)
            {
            case Attribute::ModuleApiVersion: seenLifecycleFlag = &seenModuleApiVersion_; break;
            case Attribute::ModuleLoad: seenLifecycleFlag = &seenModuleLoad_; break;
            case Attribute::ModuleUpdate: seenLifecycleFlag = &seenModuleUpdate_; break;
            case Attribute::ModuleUnload: seenLifecycleFlag = &seenModuleUnload_; break;
            case Attribute::ModuleSaveState: seenLifecycleFlag = &seenModuleSaveState_; break;
            case Attribute::ModuleRestoreState: seenLifecycleFlag = &seenModuleRestoreState_; break;
            default: break;
            }

            if (seenLifecycleFlag && *seenLifecycleFlag)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Only one {} function may be declared per compilation unit.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }
            else if (seenLifecycleFlag)
            {
                *seenLifecycleFlag = true;
            }

            if (isNative)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} cannot be combined with @Native.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (isExported)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} already defines a fixed exported symbol and cannot be combined with @Export.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} functions must define a Wio body.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (currentScope_ && currentScope_->getKind() == ScopeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} is currently supported only for top-level functions.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Entry cannot be declared as {}.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            if (hasAttribute(node.attributes, Attribute::CppName))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "{} uses a fixed exported symbol and cannot be combined with @CppName.",
                    getModuleLifecycleAttributeName(lifecycleAttribute)
                );
            }

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (lifecycleAttribute)
            {
            case Attribute::ModuleApiVersion:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleApiVersion must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getU32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleApiVersion must return u32.");
                }
                break;
            case Attribute::ModuleLoad:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleLoad must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleLoad must return i32.");
                }
                break;
            case Attribute::ModuleUpdate:
                if (node.parameters.size() != 1 ||
                    !isExactType(funcType->paramTypes[0], Compiler::get().getTypeContext().getF32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUpdate must declare exactly one f32 parameter.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUpdate must return void.");
                }
                break;
            case Attribute::ModuleUnload:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUnload must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleUnload must return void.");
                }
                break;
            case Attribute::ModuleSaveState:
                if (!node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleSaveState must not declare parameters.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleSaveState must return i32.");
                }
                break;
            case Attribute::ModuleRestoreState:
                if (node.parameters.size() != 1 ||
                    !isExactType(funcType->paramTypes[0], Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleRestoreState must declare exactly one i32 parameter.");
                }
                if (!isExactType(funcType->returnType, Compiler::get().getTypeContext().getI32()))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@ModuleRestoreState must return i32.");
                }
                break;
            default:
                break;
            }
        }

        if (hasAttribute(node.attributes, Attribute::CppHeader))
        {
            auto headerArgs = getAttributeArgs(node.attributes, Attribute::CppHeader);
            if (!isNative)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader can only be used together with @Native.");
            }
            else if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader expects exactly one string literal argument.");
            }
        }

        if (hasAttribute(node.attributes, Attribute::CppName))
        {
            auto cppNameArgs = getAttributeArgs(node.attributes, Attribute::CppName);
            if (!isNative && !isExported && !hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName can only be used together with @Native or @Export.");
            }
            else if (hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(node.location(), "{} uses a fixed exported symbol and cannot be combined with @CppName.", getModuleLifecycleAttributeName(moduleLifecycleAttributes.front()));
            }
            else if (cppNameArgs.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName expects exactly one target symbol argument.");
            }
            else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
            {
                if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName expects an identifier path like foo::bar or a string literal.");
                }
                else if (!isValidCppSymbolPath(cppNameArg->value, isNative))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        isNative
                            ? "@CppName for @Native must be a valid C++ identifier path like foo::bar."
                            : "@CppName for @Export must be a plain C/C++ symbol name like FooBar."
                    );
                }
            }
        }

        Ref<Type> prevRetType = currentFunctionReturnType_;
        currentFunctionReturnType_ = funcType->returnType;

        auto genericScope = buildGenericTypeParameterScope();
        genericTypeParameterScopes_.push_back(genericScope);

        if (hasInstantiate)
        {
            std::unordered_set<std::string> seenInstantiationSignatures;
            for (const auto* instantiateAttribute : instantiateAttributes)
            {
                if (!instantiateAttribute)
                    continue;

                if (instantiateAttribute->typeArgs.size() != node.genericParameters.size())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Instantiate expects exactly {} type arguments for '{}'.",
                        node.genericParameters.size(),
                        node.name->token.value
                    );
                    continue;
                }

                std::vector<Ref<Type>> instantiationTypes;
                instantiationTypes.reserve(instantiateAttribute->typeArgs.size());
                bool isValidInstantiation = true;
                std::string instantiationSignature;

                for (const auto& typeSpecifier : instantiateAttribute->typeArgs)
                {
                    if (!typeSpecifier)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Instantiate expects concrete type arguments, not raw tokens.");
                        isValidInstantiation = false;
                        break;
                    }

                    typeSpecifier->accept(*this);
                    Ref<Type> instantiationType = typeSpecifier->refType.Lock();
                    if (!instantiationType || instantiationType->isUnknown())
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Instantiate contains an unresolved type argument.");
                        isValidInstantiation = false;
                        break;
                    }

                    if (containsGenericParameterType(instantiationType))
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Instantiate must use fully concrete type arguments.");
                        isValidInstantiation = false;
                        break;
                    }

                    instantiationTypes.push_back(instantiationType);
                    if (!instantiationSignature.empty())
                        instantiationSignature += "|";
                    instantiationSignature += instantiationType->toString();
                }

                if (!isValidInstantiation)
                    continue;

                if (!seenInstantiationSignatures.insert(instantiationSignature).second)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Duplicate @Instantiate declaration for '{}'.", instantiationSignature);
                    continue;
                }

                if (isExported)
                {
                    auto instantiationBindings = buildGenericTypeBindings(funcSym->genericParameterNames, instantiationTypes);
                    Ref<Type> instantiatedFunctionTypeRef = instantiateGenericType(funcType, instantiationBindings);
                    auto instantiatedFunctionType = instantiatedFunctionTypeRef ? instantiatedFunctionTypeRef.AsFast<FunctionType>() : nullptr;

                    if (!instantiatedFunctionType)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Instantiate failed to produce a concrete exported signature.");
                        continue;
                    }

                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        if (!isCAbiSafeExportType(instantiatedFunctionType->paramTypes[i]))
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "@Export instantiated with '{}' produces a non-C-ABI-safe parameter {} of type '{}'.",
                                instantiationSignature,
                                i,
                                instantiatedFunctionType->paramTypes[i] ? instantiatedFunctionType->paramTypes[i]->toString() : "<unknown>"
                            );
                        }
                    }

                    if (!isCAbiSafeExportType(instantiatedFunctionType->returnType))
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Export instantiated with '{}' produces a non-C-ABI-safe return type '{}'.",
                            instantiationSignature,
                            instantiatedFunctionType->returnType ? instantiatedFunctionType->returnType->toString() : "<unknown>"
                        );
                    }
                }
            }
        }

        std::unordered_set<std::string> localExportSymbols;
        auto registerExportedSymbol = [&](const std::string& symbolName)
        {
            if (symbolName.empty() || !localExportSymbols.insert(symbolName).second)
                return;

            auto [it, inserted] = exportedCppSymbolLocations_.emplace(symbolName, node.location());
            if (!inserted)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Exported C++ symbol '{}' is already declared by another @Export or module lifecycle function.",
                    symbolName
                );
            }
        };

        if (hasModuleLifecycle || isExported)
        {
            const std::string baseExportSymbolName = getDeclaredExportSymbolName(node, hasModuleLifecycle);
            if (!node.genericParameters.empty() && isExported)
            {
                for (const auto* instantiateAttribute : instantiateAttributes)
                {
                    if (!instantiateAttribute || instantiateAttribute->typeArgs.size() != node.genericParameters.size())
                        continue;

                    std::vector<Ref<Type>> instantiationTypes;
                    bool isValidInstantiation = true;
                    instantiationTypes.reserve(instantiateAttribute->typeArgs.size());

                    for (const auto& typeSpecifier : instantiateAttribute->typeArgs)
                    {
                        if (!typeSpecifier)
                        {
                            isValidInstantiation = false;
                            break;
                        }

                        Ref<Type> instantiationType = typeSpecifier->refType.Lock();
                        if (!instantiationType || instantiationType->isUnknown() || containsGenericParameterType(instantiationType))
                        {
                            isValidInstantiation = false;
                            break;
                        }

                        instantiationTypes.push_back(instantiationType);
                    }

                    if (!isValidInstantiation)
                        continue;

                    registerExportedSymbol(formatInstantiatedExportSymbolName(baseExportSymbolName, instantiationTypes));
                }
            }
            else
            {
                registerExportedSymbol(baseExportSymbolName);
            }
        }
        
        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            
            Ref<Symbol> pSym = createSymbol(param.name->token.value, funcType->paramTypes[i], SymbolKind::Parameter, param.name->location());
            currentScope_->define(param.name->token.value, pSym);
            
            param.name->referencedSymbol = pSym;
            param.name->refType = funcType->paramTypes[i];
        }

        if (node.whenCondition)
        {
            node.whenCondition->accept(*this);
            
            if (node.whenFallback)
            {
                node.whenFallback->accept(*this);
            }
            else if (funcType->returnType != Compiler::get().getTypeContext().getVoid())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Functions with a return value must provide an 'else' fallback for 'when' guards.");
            }
        }

        if (node.body)
            node.body->accept(*this);

        exitScope();
        genericTypeParameterScopes_.pop_back();
        currentFunctionReturnType_ = prevRetType;
    }

    void SemanticAnalyzer::visit(RealmDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            Ref<Symbol> realmSym = currentScope_->resolveLocally(node.name->token.value);
            if (realmSym)
            {
                if (realmSym->kind != SymbolKind::Namespace)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and is not a realm.", node.name->token.value);
                    return;
                }

                if (!realmSym->innerScope)
                {
                    auto realmScope = Ref<Scope>::Create(currentScope_, ScopeKind::Global);
                    scopes_.push_back(realmScope);
                    realmSym->innerScope = realmScope;
                }
            }
            else
            {
                auto realmScope = Ref<Scope>::Create(currentScope_, ScopeKind::Global);
                scopes_.push_back(realmScope);

                realmSym = createSymbol(node.name->token.value, Compiler::get().getTypeContext().getUnknown(), SymbolKind::Namespace, node.location());
                realmSym->innerScope = realmScope;
                currentScope_->define(node.name->token.value, realmSym);
            }

            node.name->referencedSymbol = realmSym;
            node.name->refType = realmSym->type;
        }

        auto realmSym = node.name->referencedSymbol.Lock();
        if (!realmSym || !realmSym->innerScope)
            return;

        auto prevScope = currentScope_;
        currentScope_ = realmSym->innerScope;
        currentNamespacePath_.push_back(node.name->token.value);

        for (auto& statement : node.statements)
            statement->accept(*this);

        currentNamespacePath_.pop_back();
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(InterfaceDeclaration& node)
    {
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (const auto& genericParameter : node.genericParameters)
            {
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this interface.", parameterName);
                    continue;
                }

                Ref<Type> parameterType = Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            auto interfaceScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(interfaceScope);
            
            Ref<Type> interfaceType = Ref<StructType>::Create(node.name->token.value, interfaceScope, false, true);
            interfaceType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            interfaceType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    interfaceType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
            }
            Ref<Symbol> interfaceSym = createSymbol(node.name->token.value, interfaceType, SymbolKind::Struct, node.location());
            interfaceSym->innerScope = interfaceScope;
            interfaceSym->flags.set_isInterface(true);
            interfaceSym->genericParameterNames = interfaceType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, interfaceSym);
            
            node.name->refType = interfaceType;
            node.name->referencedSymbol = interfaceSym;

            auto prevScope = currentScope_;
            currentScope_ = interfaceScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& method : node.methods) method->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());

        for (auto& method : node.methods)
            method->accept(*this); 

        genericTypeParameterScopes_.pop_back();
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ComponentDeclaration& node)
    {
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (const auto& genericParameter : node.genericParameters)
            {
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this component.", parameterName);
                    continue;
                }

                Ref<Type> parameterType = Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
            }
            Ref<Symbol> compSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            compSym->innerScope = structScope;
            compSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, compSym);
            
            node.name->refType = structType;
            node.name->referencedSymbol = compSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& member : node.members) member.declaration->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();
        
        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            Ref<Type> generatedSelfType = structType;
            if (!structType->genericParameterNames.empty())
            {
                std::vector<Ref<Type>> genericSelfArguments;
                genericSelfArguments.reserve(structType->genericParameterNames.size());
                for (const auto& genericParameterName : structType->genericParameterNames)
                {
                    if (auto genericIt = genericScope.find(genericParameterName); genericIt != genericScope.end())
                        genericSelfArguments.push_back(genericIt->second);
                }

                if (genericSelfArguments.size() == structType->genericParameterNames.size())
                    generatedSelfType = instantiateGenericStructType(structType, genericSelfArguments);
            }

            bool hasCustomCtor = false;
            bool hasEmptyCtor = false;
            bool hasCopyCtor = false;
            bool hasMemberCtor = false;
            
            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
            auto bases = getAttributeArgs(node.attributes, Attribute::From);
            if (!bases.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Components must be POD (Plain Old Data) and cannot inherit from objects or interfaces.");
            }

            AccessModifier defaultAccess = AccessModifier::Public; 
            std::vector<Token> defaultArgs = getAttributeArgs(node.attributes, Attribute::Default);
            if (!defaultArgs.empty()) {
                if (defaultArgs[0].type == TokenType::kwPrivate) defaultAccess = AccessModifier::Private;
                else if (defaultArgs[0].type == TokenType::kwProtected) defaultAccess = AccessModifier::Protected;
            }

            structType->fieldNames.clear();
            structType->fieldTypes.clear();

            // PASS 1: Variables
            std::vector<Ref<Type>> memberTypes;
            for (auto& member : node.members)
            {
                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    auto memberSym = varDecl->name->referencedSymbol.Lock();
                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);
                    
                    if (memberSym && memberSym->type)
                    {
                        memberTypes.push_back(memberSym->type);
                        structType->fieldNames.push_back(varDecl->name->token.value);
                        structType->fieldTypes.push_back(memberSym->type);
                    }
                    
                    if (member.access == AccessModifier::None) member.access = defaultAccess;
                    if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            // PASS 2: Functions
            for (auto& member : node.members)
            {
                if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    auto memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;
                    
                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                        size_t pCount = funcDecl->parameters.size();
                        
                        if (pCount == 0) hasEmptyCtor = true;
                        else if (pCount == 1) 
                        {
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                if (fType->paramTypes[0]->kind() == TypeKind::Reference && 
                                    fType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType) {
                                    hasCopyCtor = true;
                                }
                            }
                        }
                        
                        if (pCount == memberTypes.size() && !(pCount == 1 && hasCopyCtor)) 
                        {
                            bool typesMatch = true;
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                for (size_t i = 0; i < pCount; ++i) {
                                    if (!fType->paramTypes[i]->isCompatibleWith(memberTypes[i])) {
                                        typesMatch = false; break;
                                    }
                                }
                            }
                            if (typesMatch) hasMemberCtor = true;
                        }
                    }

                    if (memberSym)
                    {
                        if (member.access == AccessModifier::None) member.access = defaultAccess;
                        if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                        else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                        else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                    }
                }
            }

            // PASS 3: Generate Constructors
            if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                if (!hasEmptyCtor) {
                    auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
                    Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", defaultSym);
                }

                if (!hasMemberCtor && !memberTypes.empty()) {
                    auto memberCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                if (!hasCopyCtor) {
                    auto copyParamType = Compiler::get().getTypeContext().getOrCreateReferenceType(generatedSelfType, false);
                    auto copyCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, { copyParamType });
                    Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", copySym);
                }
            }
            
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
        
        for (auto& member : node.members)
            if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);

        genericTypeParameterScopes_.pop_back();
        currentStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ObjectDeclaration& node)
    {
        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (const auto& genericParameter : node.genericParameters)
            {
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this object.", parameterName);
                    continue;
                }

                Ref<Type> parameterType = Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope, true);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
            }
            Ref<Symbol> objSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            objSym->innerScope = structScope;
            objSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, objSym);
            
            node.name->refType = structType;
            node.name->referencedSymbol = objSym;

            auto prevScope = currentScope_;
            currentScope_ = structScope;
            genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());
            for (auto& member : node.members) member.declaration->accept(*this);
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto sym = node.name->referencedSymbol.Lock();
        auto structType = sym->type.AsFast<StructType>();
        
        if (isStructResolutionPass_)
        {
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            Ref<Type> generatedSelfType = structType;
            if (!structType->genericParameterNames.empty())
            {
                std::vector<Ref<Type>> genericSelfArguments;
                genericSelfArguments.reserve(structType->genericParameterNames.size());
                for (const auto& genericParameterName : structType->genericParameterNames)
                {
                    if (auto genericIt = genericScope.find(genericParameterName); genericIt != genericScope.end())
                        genericSelfArguments.push_back(genericIt->second);
                }

                if (genericSelfArguments.size() == structType->genericParameterNames.size())
                    generatedSelfType = instantiateGenericStructType(structType, genericSelfArguments);
            }

            bool hasCustomCtor = false;
            bool hasEmptyCtor = false;
            bool hasCopyCtor = false;
            bool hasMemberCtor = false;
            
            bool hasNoDefaultCtor = hasAttribute(node.attributes, Attribute::NoDefaultCtor);
            bool forceGenerateCtors = hasAttribute(node.attributes, Attribute::GenerateCtors);
            auto baseArgs = getAttributeTypeArgs(node.attributes, Attribute::From);

            AccessModifier defaultAccess = AccessModifier::Private;
            std::vector<Token> defaultArgs = getAttributeArgs(node.attributes, Attribute::Default);
            if (!defaultArgs.empty())
            {
                if (defaultArgs[0].type == TokenType::kwPublic)
                    defaultAccess = AccessModifier::Public;
                else if (defaultArgs[0].type == TokenType::kwProtected)
                    defaultAccess = AccessModifier::Protected;
            }

            auto resolveBaseType = [&](const AttributeTypeArgument& baseArg) -> Ref<StructType>
            {
                Ref<Type> resolvedType = nullptr;

                if (baseArg.typeSpecifier)
                {
                    baseArg.typeSpecifier->accept(*this);
                    resolvedType = baseArg.typeSpecifier->refType.Lock();
                }
                else if (auto baseSym = resolveAttributeSymbol(prevScope, baseArg.token))
                {
                    resolvedType = baseSym->type;
                }

                while (resolvedType && resolvedType->kind() == TypeKind::Alias)
                    resolvedType = resolvedType.AsFast<AliasType>()->aliasedType;

                if (!resolvedType || resolvedType->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from '{}'.", node.name->token.value, baseArg.token.value);
                    return nullptr;
                }

                return resolvedType.AsFast<StructType>();
            };

            std::vector<Ref<StructType>> resolvedBaseTypes;
            resolvedBaseTypes.reserve(baseArgs.size());
            int structBaseCount = 0;
            for (const auto& baseArg : baseArgs)
            {
                if (auto baseType = resolveBaseType(baseArg))
                {
                    structType->baseTypes.emplace_back(baseType);
                    resolvedBaseTypes.push_back(baseType);
                    
                    if (!baseType->isInterface)
                    {
                        structBaseCount++;
                        if (structBaseCount > 1)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from multiple objects/components. Single class inheritance only!", node.name->token.value);
                        }
                        else
                        {
                            bool hasDefaultCtor = false;
                            
                            if (auto baseScope = baseType->structScope.Lock();
                                baseScope && baseScope->resolveLocally("OnConstruct"))
                            {
                                auto ctorSym = baseScope->resolveLocally("OnConstruct");
                                if (ctorSym->kind == SymbolKind::FunctionGroup) 
                                {
                                    for (auto& overload : ctorSym->overloads) {
                                        if (overload->type && overload->type.AsFast<FunctionType>()->paramTypes.empty()) {
                                            hasDefaultCtor = true; break;
                                        }
                                    }
                                } 
                                else if (ctorSym->type && ctorSym->type.AsFast<FunctionType>()->paramTypes.empty()) 
                                {
                                    hasDefaultCtor = true;
                                }
                            }
                            else 
                            {
                                hasDefaultCtor = true;
                            }
                            if (!hasDefaultCtor)
                            {
                                WIO_LOG_ADD_ERROR(node.location(), 
                                    "Base object '{}' lacks a default (parameterless) constructor. Derived object '{}' cannot be instantiated safely. Hint: Add '@GenerateCtors' or an explicit 'OnConstruct() {{}}' to the base object.", 
                                    baseArg.token.value, node.name->token.value);
                            }
                        }
                    }
                }
            }

            if (structBaseCount == 0)
                structType->baseTypes.push_back(Compiler::get().getTypeContext().getObject());

            structType->fieldNames.clear();
            structType->fieldTypes.clear();

            // PASS 1: Variables
            std::vector<Ref<Type>> memberTypes;
            for (auto& member : node.members)
            {
                if (member.declaration->is<VariableDeclaration>())
                {
                    member.declaration->accept(*this);
                    auto varDecl = member.declaration->as<VariableDeclaration>();
                    auto memberSym = varDecl->name->referencedSymbol.Lock();
                    
                    if (hasAttribute(varDecl->attributes, Attribute::ReadOnly)) memberSym->flags.set_isReadOnly(true);
                    
                    if (memberSym && memberSym->type)
                    {
                        memberTypes.push_back(memberSym->type);
                        structType->fieldNames.push_back(varDecl->name->token.value);
                        structType->fieldTypes.push_back(memberSym->type);
                    }

                    if (member.access == AccessModifier::None) member.access = defaultAccess;
                    if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                    else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                    else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                }
            }

            // PASS 2: Functions
            for (auto& member : node.members)
            {
                if (member.declaration->is<FunctionDeclaration>())
                {
                    auto funcDecl = member.declaration->as<FunctionDeclaration>();
                    auto memberSym = funcDecl->name->referencedSymbol.Lock();
                    std::string funcName = funcDecl->name->token.value;
                    
                    if (funcName == "OnConstruct")
                    {
                        hasCustomCtor = true;
                        size_t pCount = funcDecl->parameters.size();
                        
                        if (pCount == 0) hasEmptyCtor = true;
                        else if (pCount == 1) 
                        {
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                if (fType->paramTypes[0]->kind() == TypeKind::Reference && 
                                    fType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType) {
                                    hasCopyCtor = true;
                                }
                            }
                        }
                        
                        if (pCount == memberTypes.size() && !(pCount == 1 && hasCopyCtor)) 
                        {
                            bool typesMatch = true;
                            if (memberSym && memberSym->type) {
                                auto fType = memberSym->type.AsFast<FunctionType>();
                                for (size_t i = 0; i < pCount; ++i) {
                                    if (!fType->paramTypes[i]->isCompatibleWith(memberTypes[i])) {
                                        typesMatch = false; break;
                                    }
                                }
                            }
                            if (typesMatch) hasMemberCtor = true;
                        }
                    }
                    else if (funcName != "OnDestruct") 
                    {
                        bool isOverride = false;
                        for (const auto& baseType : resolvedBaseTypes)
                        {
                            if (baseType)
                            {
                                if (auto lockedScope = baseType->structScope.Lock(); lockedScope)
                                {
                                    if (lockedScope->resolveLocally(funcName))
                                    {
                                        isOverride = true; 
                                        break;
                                    }
                                }
                            }
                        } 
                        if (isOverride)
                        {
                            memberSym->flags.set_isOverride(true);
                        }
                    }

                    if (memberSym)
                    {
                        if (member.access == AccessModifier::None) member.access = defaultAccess;
                        if (member.access == AccessModifier::Public) memberSym->flags.set_isPublic(true);
                        else if (member.access == AccessModifier::Private) memberSym->flags.set_isPrivate(true);
                        else if (member.access == AccessModifier::Protected) memberSym->flags.set_isProtected(true);
                    }
                }
            }

            // PASS 3: Generate Constructors
            if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors) 
            {
                auto voidType = Compiler::get().getTypeContext().getVoid();

                if (!hasEmptyCtor) {
                    auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
                    Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", defaultSym);
                }

                if (!hasMemberCtor && !memberTypes.empty()) {
                    auto memberCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, memberTypes);
                    Ref<Symbol> memberSym = createSymbol("OnConstruct", memberCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", memberSym);
                }

                if (!hasCopyCtor) {
                    auto copyParamType = Compiler::get().getTypeContext().getOrCreateReferenceType(generatedSelfType, false);
                    auto copyCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, { copyParamType });
                    Ref<Symbol> copySym = createSymbol("OnConstruct", copyCtorType, SymbolKind::Function, node.location());
                    currentScope_->define("OnConstruct", copySym);
                }
            }
            
            genericTypeParameterScopes_.pop_back();
            currentScope_ = prevScope;
            return;
        }

        auto prevScope = currentScope_;
        currentScope_ = sym->innerScope;
        currentStructType_ = structType;
        currentBaseStructType_ = nullptr;
        genericTypeParameterScopes_.push_back(buildGenericTypeParameterScope());

        for (const auto& baseType : structType->baseTypes)
        {
            if (!baseType || baseType->kind() != TypeKind::Struct)
                continue;

            auto structBase = baseType.AsFast<StructType>();
            if (!structBase->isInterface && !(structBase->name == "object" && structBase->scopePath.empty()))
            {
                currentBaseStructType_ = structBase;
                break;
            }
        }
        
        for (auto& member : node.members)
            if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);

        genericTypeParameterScopes_.pop_back();
        currentStructType_ = nullptr;
        currentBaseStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(FlagDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> flagType = Ref<StructType>::Create(node.name->token.value, structScope);
            flagType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            Ref<Symbol> flagSym = createSymbol(node.name->token.value, flagType, SymbolKind::Struct, node.location());
            
            flagSym->innerScope = structScope;
            flagSym->flags.set_isFlag(true); 
            
            currentScope_->define(node.name->token.value, flagSym);
            node.name->refType = flagType;
            node.name->referencedSymbol = flagSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto voidType = Compiler::get().getTypeContext().getVoid();
            auto defaultCtorType = Compiler::get().getTypeContext().getOrCreateFunctionType(voidType, {});
            Ref<Symbol> defaultSym = createSymbol("OnConstruct", defaultCtorType, SymbolKind::Function, node.location());
            currentScope_->define("OnConstruct", defaultSym);
            
            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(EnumDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            auto enumScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(enumScope);
            
            Ref<Type> enumType = Ref<StructType>::Create(node.name->token.value, enumScope);
            enumType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            Ref<Symbol> enumSym = createSymbol(node.name->token.value, enumType, SymbolKind::Struct, node.location());
            
            enumSym->innerScope = enumScope;
            enumSym->flags.set_isEnum(true);
            
            currentScope_->define(node.name->token.value, enumSym);
            node.name->refType = enumType;
            node.name->referencedSymbol = enumSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto targetType = Compiler::get().getTypeContext().getI32();
           
            if (auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for enum.");
                }
            }
            
            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);
                
                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;
                
                if (member.value)
                    member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }

    void SemanticAnalyzer::visit(FlagsetDeclaration& node)
    {
        if (isDeclarationPass_) {
            auto flagsetScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(flagsetScope);
            
            Ref<Type> flagsetType = Ref<StructType>::Create(node.name->token.value, flagsetScope);
            flagsetType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            Ref<Symbol> flagsetSym = createSymbol(node.name->token.value, flagsetType, SymbolKind::Struct, node.location());
            
            flagsetSym->innerScope = flagsetScope;
            flagsetSym->flags.set_isFlagset(true);
            
            currentScope_->define(node.name->token.value, flagsetSym);
            node.name->refType = flagsetType;
            node.name->referencedSymbol = flagsetSym;
        }
        else if (isStructResolutionPass_)
        {
            auto sym = node.name->referencedSymbol.Lock();
            auto prevScope = currentScope_;
            currentScope_ = sym->innerScope;
            
            auto targetType = Compiler::get().getTypeContext().getU32();
           
            if (auto typeArgs = getAttributeArgs(node.attributes, Attribute::Type); !typeArgs.empty())
            {
                auto& ctx = Compiler::get().getTypeContext();
                // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
                switch (typeArgs[0].type){
                case TokenType::kwI8:  targetType = ctx.getI8(); break;
                case TokenType::kwU8:  targetType = ctx.getU8(); break;
                case TokenType::kwI16: targetType = ctx.getI16(); break;
                case TokenType::kwU16: targetType = ctx.getU16(); break;
                case TokenType::kwI32: targetType = ctx.getI32(); break;
                case TokenType::kwU32: targetType = ctx.getU32(); break;
                case TokenType::kwI64: targetType = ctx.getI64(); break;
                case TokenType::kwU64: targetType = ctx.getU64(); break;
                default: WIO_LOG_ADD_ERROR(node.location(), "Invalid underlying type for flagset.");
                }
            }
            
            for (auto& member : node.members)
            {
                Ref<Symbol> memberSym = createSymbol(member.name->token.value, sym->type, SymbolKind::Variable, member.name->location());
                memberSym->flags.set_isReadOnly(true);
                
                currentScope_->define(member.name->token.value, memberSym);
                member.name->referencedSymbol = memberSym;
                member.name->refType = targetType;
                
                if (member.value) member.value->accept(*this);
            }
            currentScope_ = prevScope;
        }
    }
    
    void SemanticAnalyzer::visit(IfStatement& node)
    {
        node.condition->accept(*this);

        auto ifScope = Ref<Scope>::Create(currentScope_, ScopeKind::Block);
        auto prevScope = currentScope_;
        currentScope_ = ifScope;

        if (node.matchVar.isValid())
        {
            if (node.condition->is<BinaryExpression>())
            {
                auto binExpr = node.condition->as<BinaryExpression>();
                auto typeSym = binExpr->right->referencedSymbol.Lock();
                if (binExpr->op.type == TokenType::kwIs && typeSym && typeSym->kind == SymbolKind::Struct)
                {
                    auto refType = Compiler::get().getTypeContext().getOrCreateReferenceType(typeSym->type, false);
                    auto varSym = createSymbol(node.matchVar.value, refType, SymbolKind::Variable, node.matchVar.loc);
                    currentScope_->define(node.matchVar.value, varSym);
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.matchVar.loc, "Pattern matching 'fit' can only be used with the 'is' operator (e.g., target is Boss fit t).");
                }
            }
        }

        if (node.thenBranch) node.thenBranch->accept(*this);
        
        currentScope_ = prevScope;
        if (node.elseBranch) node.elseBranch->accept(*this);
    }
    
    void SemanticAnalyzer::visit(WhileStatement& node)
    {
        if (isDeclarationPass_) return;
        node.condition->accept(*this);

        if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
        {
            // Todo: Check null (i.g. it's not works well)
            if (!condType->isNumeric() && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null) 
            {
                WIO_LOG_ADD_ERROR(
                    node.condition->location(),
                    "While condition must be a boolean, numeric, or reference type. Got: {}",
                    condType->toString()
                );
            }
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;
    }

    void SemanticAnalyzer::visit(ForInStatement& node)
    {
        if (isDeclarationPass_) return;

        node.iterable->accept(*this);
        if (node.step)
            node.step->accept(*this);

        Ref<Type> stepType = node.step ? getAutoReadableType(node.step->refType.Lock()) : nullptr;

        Ref<Type> iterableType = node.iterable->refType.Lock();
        while (iterableType && iterableType->kind() == TypeKind::Alias)
            iterableType = iterableType.AsFast<AliasType>()->aliasedType;
        while (iterableType && iterableType->kind() == TypeKind::Reference)
            iterableType = iterableType.AsFast<ReferenceType>()->referredType;
        while (iterableType && iterableType->kind() == TypeKind::Alias)
            iterableType = iterableType.AsFast<AliasType>()->aliasedType;

        enterScope(ScopeKind::Block);

        auto getBindingMode = [&](size_t index) -> ForBindingMode
        {
            if (index < node.bindingModes.size())
                return node.bindingModes[index];

            return ForBindingMode::ValueImmutable;
        };

        auto createLoopBindingType = [&](const Ref<Type>& valueType, ForBindingMode bindingMode) -> Ref<Type>
        {
            switch (bindingMode)
            {
            case ForBindingMode::ValueMutable:
            case ForBindingMode::ValueImmutable:
                return valueType;
            case ForBindingMode::ReferenceMutable:
                return Compiler::get().getTypeContext().getOrCreateReferenceType(valueType, true);
            case ForBindingMode::ReferenceView:
                return Compiler::get().getTypeContext().getOrCreateReferenceType(valueType, false);
            }

            return valueType;
        };

        auto createBindingSymbol = [&](const NodePtr<Identifier>& binding, const Ref<Type>& bindingType, ForBindingMode bindingMode)
        {
            SymbolFlags flags = SymbolFlags::createAllFalse();
            if (bindingMode == ForBindingMode::ValueMutable || bindingMode == ForBindingMode::ReferenceMutable)
                flags.set_isMutable(true);

            Ref<Symbol> bindingSym = createSymbol(binding->token.value, bindingType, SymbolKind::Variable, binding->location(), flags);
            currentScope_->define(binding->token.value, bindingSym);
            binding->referencedSymbol = bindingSym;
            binding->refType = bindingType;
        };

        node.bindingAccessors.clear();

        if (node.iterable->is<RangeExpression>())
        {
            auto rangeExpr = node.iterable->as<RangeExpression>();
            Ref<Type> startType = getAutoReadableType(rangeExpr->start->refType.Lock());
            Ref<Type> endType = getAutoReadableType(rangeExpr->end->refType.Lock());

            if (node.bindings.size() != 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration requires exactly 1 binding.");
            }
            else if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration does not support 'ref' or 'view' bindings.");
            }
            else if (!isIntegralLikeType(startType) || !isIntegralLikeType(endType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration currently requires integer bounds.");
            }
            else if (node.step && !isIntegralLikeType(stepType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range step expressions must be integer values.");
            }
            else if (node.step && isZeroIntegerLiteralExpression(node.step))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range step cannot be zero.");
            }
            else
            {
                Ref<Type> rangeValueType = startType;
                if (!rangeValueType || !rangeValueType->isCompatibleWith(endType))
                    rangeValueType = endType;

                createBindingSymbol(node.bindings[0], createLoopBindingType(rangeValueType, getBindingMode(0)), getBindingMode(0));
            }
        }
        else if (iterableType && iterableType->kind() == TypeKind::Array)
        {
            auto arrayType = iterableType.AsFast<ArrayType>();
            Ref<Type> elementType = arrayType->elementType;
            Ref<Type> indexType = Compiler::get().getTypeContext().getUSize();

            if (node.step && !isIntegralLikeType(stepType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array step expressions must be integer values.");
            }
            else if (node.step && isZeroIntegerLiteralExpression(node.step))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array step cannot be zero.");
            }

            if (node.bindings.size() == 1)
            {
                createBindingSymbol(node.bindings[0], createLoopBindingType(elementType, getBindingMode(0)), getBindingMode(0));
            }
            else if (elementType && elementType->kind() == TypeKind::Struct)
            {
                auto structType = elementType.AsFast<StructType>();
                if (structType->isObject || structType->isInterface)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Destructuring in array loops currently supports component-like POD structs only.");
                }
                else if (node.bindings.size() == structType->fieldNames.size())
                {
                    for (size_t i = 0; i < node.bindings.size(); ++i)
                    {
                        node.bindingAccessors.push_back(structType->fieldNames[i]);
                        createBindingSymbol(
                            node.bindings[i],
                            createLoopBindingType(structType->fieldTypes[i], getBindingMode(i)),
                            getBindingMode(i)
                        );
                    }
                }
                else if (node.bindings.size() == structType->fieldNames.size() + 1)
                {
                    if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                    }
                    else
                    {
                        node.bindingAccessors.emplace_back("__index__");
                        createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));

                        for (size_t i = 1; i < node.bindings.size(); ++i)
                        {
                            node.bindingAccessors.push_back(structType->fieldNames[i - 1]);
                            createBindingSymbol(
                                node.bindings[i],
                                createLoopBindingType(structType->fieldTypes[i - 1], getBindingMode(i)),
                                getBindingMode(i)
                            );
                        }
                    }
                }
                else if (node.bindings.size() == 2)
                {
                    if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                    }
                    else
                    {
                        node.bindingAccessors = { "__index__", "__value__" };
                        createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));
                        createBindingSymbol(node.bindings[1], createLoopBindingType(elementType, getBindingMode(1)), getBindingMode(1));
                    }
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Component binding expects {} names, or {} with an index binding, but {} were provided.",
                        structType->fieldNames.size(), structType->fieldNames.size() + 1, node.bindings.size());
                }
            }
            else if (node.bindings.size() == 2)
            {
                if (getBindingMode(0) == ForBindingMode::ReferenceMutable || getBindingMode(0) == ForBindingMode::ReferenceView)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Array index bindings do not support 'ref' or 'view'.");
                }
                else
                {
                    node.bindingAccessors = { "__index__", "__value__" };
                    createBindingSymbol(node.bindings[0], indexType, getBindingMode(0));
                    createBindingSymbol(node.bindings[1], createLoopBindingType(elementType, getBindingMode(1)), getBindingMode(1));
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Array iteration supports either a single value binding or 'index | value'.");
            }
        }
        else if (iterableType && iterableType->kind() == TypeKind::Dictionary)
        {
            if (node.step)
                WIO_LOG_ADD_ERROR(node.location(), "Step clauses are currently supported only for range iteration.");

            auto dictType = iterableType.AsFast<DictionaryType>();
            if (node.bindings.size() != 2)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Dictionary iteration requires exactly 2 bindings: key | value.");
            }
            else
            {
                const ForBindingMode keyMode = getBindingMode(0);
                const ForBindingMode valueMode = getBindingMode(1);

                if (keyMode == ForBindingMode::ReferenceMutable)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Dictionary keys are immutable. Use a value binding or 'view' for the key.");
                }
                else
                {
                    node.bindingAccessors = { "first", "second" };
                    createBindingSymbol(node.bindings[0], createLoopBindingType(dictType->keyType, keyMode), keyMode);
                    createBindingSymbol(node.bindings[1], createLoopBindingType(dictType->valueType, valueMode), valueMode);
                }
            }
        }
        else
        {
            const std::string actualType = iterableType ? iterableType->toString() : "<unknown>";
            WIO_LOG_ADD_ERROR(node.location(), "For-in loops currently require an array or dictionary type. Got '{}'.", actualType);
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;

        exitScope();
    }

    void SemanticAnalyzer::visit(CForStatement& node)
    {
        if (isDeclarationPass_) return;

        enterScope(ScopeKind::Block);

        if (node.initializer)
            node.initializer->accept(*this);

        if (node.condition)
        {
            node.condition->accept(*this);

            if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
            {
                if (!condType->isNumeric() && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.condition->location(),
                        "For-loop condition must be a boolean, numeric, or reference type. Got: {}",
                        condType->toString()
                    );
                }
            }
        }

        if (node.increment)
            node.increment->accept(*this);

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;

        exitScope();
    }

    void SemanticAnalyzer::visit(BreakStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'break' statement can only be used inside a loop.");
    }

    void SemanticAnalyzer::visit(ContinueStatement& node)
    {
        if (isDeclarationPass_) return;
        if (loopDepth_ == 0)
            WIO_LOG_ADD_ERROR(node.location(), "'continue' statement can only be used inside a loop.");
    }
    
    void SemanticAnalyzer::visit(ReturnStatement& node)
    {
        if (isDeclarationPass_) return;
        Ref<Type> actualType = Compiler::get().getTypeContext().getVoid();

        if (node.value)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = currentFunctionReturnType_;
            node.value->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            actualType = node.value->refType.Lock();
        }

        if (currentFunctionReturnType_)
        {
            if (!actualType->isCompatibleWith(currentFunctionReturnType_))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Return type mismatch! Expected '{}', but got '{}'.",
                    currentFunctionReturnType_->toString(),
                    actualType->toString()
                );
            }
        }
        else
        {
            WIO_LOG_ADD_ERROR(node.location(), "Return statement found outside of a function.");
        }
    }

    void SemanticAnalyzer::visit(UseStatement& node)
    {
        if (!isDeclarationPass_) return;
        if (node.isCppHeader) return;
        
        auto getOrCreateNamespace = [&](const Ref<Scope>& targetScope, const std::string& name) -> Ref<Symbol>
        {
            if (auto existing = targetScope->resolve(name))
            {
                if (existing->kind == SymbolKind::Namespace)
                    return existing;
                
                WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and is not a namespace.", name);
                return nullptr;
            }
            
            auto nsScope = Ref<Scope>::Create(targetScope, ScopeKind::Global); 
            
            auto nsSymbol = createSymbol(name, Compiler::get().getTypeContext().getUnknown(), SymbolKind::Namespace, node.location());
            nsSymbol->innerScope = nsScope;
            
            targetScope->define(name, nsSymbol);
            return nsSymbol;
        };

        auto resolveImportedNamespace = [&]() -> Ref<Symbol>
        {
            std::vector<std::string> namespaceParts;
            if (node.isStdLib)
                namespaceParts.emplace_back("std");

            auto moduleParts = splitModulePath(node.modulePath);
            namespaceParts.insert(namespaceParts.end(), moduleParts.begin(), moduleParts.end());

            if (namespaceParts.empty())
                return nullptr;

            Ref<Symbol> resolvedNamespace = currentScope_->resolve(namespaceParts.front());
            if (!resolvedNamespace || resolvedNamespace->kind != SymbolKind::Namespace)
                return nullptr;

            for (size_t i = 1; i < namespaceParts.size(); ++i)
            {
                if (!resolvedNamespace->innerScope)
                    return nullptr;

                resolvedNamespace = resolvedNamespace->innerScope->resolve(namespaceParts[i]);
                if (!resolvedNamespace || resolvedNamespace->kind != SymbolKind::Namespace)
                    return nullptr;
            }

            return resolvedNamespace;
        };

        if (node.aliasName.empty())
            return;

        if (auto importedNamespace = resolveImportedNamespace())
        {
            if (auto existingAlias = currentScope_->resolveLocally(node.aliasName))
            {
                if (existingAlias == importedNamespace)
                    return;

                WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and cannot be used as an import alias.", node.aliasName);
                return;
            }

            currentScope_->define(node.aliasName, importedNamespace);
            return;
        }

        if (node.isStdLib)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Standard library module 'std::{}' could not be resolved after merge.", node.modulePath);
            return;
        }

        std::vector<Ref<Symbol>> importedSymbols;
        importedSymbols.reserve(node.importedSymbols.size());

        for (const auto& importedName : node.importedSymbols)
        {
            auto importedSymbol = currentScope_->resolveLocally(importedName);
            if (!importedSymbol)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Imported symbol '{}' from module '{}' could not be resolved after merge.", importedName, node.modulePath);
                continue;
            }

            importedSymbols.push_back(importedSymbol);
        }

        Ref<Symbol> aliasNamespace = getOrCreateNamespace(currentScope_, node.aliasName);
        if (!aliasNamespace || !aliasNamespace->innerScope)
            return;

        for (const auto& importedSymbol : importedSymbols)
        {
            if (!importedSymbol)
                continue;

            if (aliasNamespace->innerScope->resolveLocally(importedSymbol->name))
                continue;

            aliasNamespace->innerScope->define(importedSymbol->name, importedSymbol);
        }
    }
}
