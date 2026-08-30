#include "wio/codegen/cpp_generator.h"

#include "wio/ast/attribute_contract.h"
#include "wio/ast/attribute_queries.h"
#include "wio/ast/declaration_queries.h"
#include "wio/codegen/cpp_identifier.h"
#include "wio/common/filesystem/filesystem.h"
#include "wio/common/operator_overload.h"
#include "wio/common/utility.h"
#include "compiler.h"
#include "wio/common/logger.h"
#include "wio/sema/symbol.h"
#include "wio/sema/scope_lookup.h"
#include "wio/sema/constant_evaluator.h"
#include "wio/sema/generic_support.h"
#include "wio/sema/type_queries.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_set>
#include <utility>

#define EMIT_TABS() do { for (int _____I_____ = 0; _____I_____ < indentationLevel_; ++_____I_____) buffer_ << "    "; } while(false)

namespace wio::codegen
{
    namespace 
    {
        using attribute_queries::getFirstAttributeArgs;
        using attribute_queries::getSingleAttributeArg;
        using attribute_queries::hasAttribute;
        using cpp_identifier::isCppReservedIdentifier;
        using cpp_identifier::replaceCppIdentifier;
        using cpp_identifier::sanitizeCppIdentifier;
        using declaration_queries::getFixedParameterCount;
        using declaration_queries::getRequiredParameterCount;
        using declaration_queries::hasDefaultParameters;
        using sema::scope_lookup::resolveQualifiedSymbol;
        using sema::generic_support::GenericBindingSet;
        using sema::generic_support::PackElementBindingKind;
        using sema::generic_support::ParsedPackElementBinding;
        using sema::generic_support::SymbolicPackNameMode;
        using sema::generic_support::buildExtendedGenericBindings;
        using sema::generic_support::buildGenericTypeBindings;
        using sema::generic_support::getMinimumGenericArgumentCount;
        using sema::generic_support::makePackElementBindingName;
        using sema::generic_support::makePackTailElementBindingName;
        using sema::generic_support::tryEvaluatePackIndexBinding;
        using sema::generic_support::tryEvaluateStaticPackIndex;
        using sema::generic_support::tryGetSymbolicPackReferenceName;
        using sema::generic_support::tryParsePackElementBindingName;
        using sema::generic_support::tryResolveConcretePackElementIndex;
        using sema::type_queries::getAutoReadableReferenceDepth;
        using sema::type_queries::getStdValueStructType;
        using sema::type_queries::isSdkValueBridgeType;
        using sema::type_queries::isStdLibraryScopePath;
        using sema::type_queries::shouldAutoReadReferenceType;
        using sema::type_queries::unwrapAliasType;

        std::string formatCppTemplateParameter(
            const NodePtr<Identifier>& parameter,
            const bool isPack,
            std::string_view nameOverride = {})
        {
            if (!parameter)
                return "typename _wio_missing";

            const std::string backendName = sanitizeCppIdentifier(
                nameOverride.empty() ? parameter->token.value : nameOverride);

            if (parameter->isConstGenericParameter)
            {
                Ref<sema::Type> valueType = parameter->genericValueType
                    ? parameter->genericValueType->refType.Lock()
                    : nullptr;
                Ref<sema::Type> resolvedValueType = valueType;
                while (resolvedValueType && resolvedValueType->kind() == sema::TypeKind::Alias)
                    resolvedValueType = resolvedValueType.AsFast<sema::AliasType>()->aliasedType;
                if (resolvedValueType && resolvedValueType->kind() == sema::TypeKind::Primitive)
                {
                    const std::string& name = resolvedValueType.AsFast<sema::PrimitiveType>()->name;
                    if (name == "string" || name == "text")
                    {
                        return std::string(name == "text"
                            ? "wio::runtime::ConstText "
                            : "wio::runtime::ConstString ") +
                            backendName;
                    }
                }
                return (valueType ? valueType->toCppString() : "std::size_t") +
                       " " + backendName;
            }

            return std::string(isPack ? "typename... " : "typename ") +
                   backendName;
        }

        Ref<sema::Type> unwrapAliasTypeForCodegen(Ref<sema::Type> type)
        {
            return unwrapAliasType(std::move(type));
        }

        using sema::ConstVariableDeclarationMap;

        void collectVariableDeclarationsBySymbol(const Ref<Program>& program, ConstVariableDeclarationMap& variableDeclarationsBySymbol)
        {
            if (!program)
                return;

            auto collectFromStatementList = [&](const std::vector<NodePtr<Statement>>& statements, auto&& collectFromStatementListRef) -> void
            {
                for (const auto& statement : statements)
                {
                    if (!statement)
                        continue;

                    if (auto* variableDeclaration = statement->as<VariableDeclaration>())
                    {
                        if (variableDeclaration->name)
                        {
                            Ref<sema::Symbol> symbol = variableDeclaration->name->referencedSymbol.Lock();
                            if (symbol)
                                variableDeclarationsBySymbol[symbol.Get()] = variableDeclaration;
                        }
                        continue;
                    }

                    if (auto* realmDeclaration = statement->as<RealmDeclaration>())
                    {
                        collectFromStatementListRef(realmDeclaration->statements, collectFromStatementListRef);
                    }
                }
            };

            collectFromStatementList(program->statements, collectFromStatementList);
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
            if (name == "any") return "wio::runtime::Any";
            if (name == "opaque") return "void*";
            return resolved->toCppString();
        }

        bool isAnyTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            return resolved &&
                   resolved->kind() == sema::TypeKind::Primitive &&
                   resolved.AsFast<sema::PrimitiveType>()->name == "any";
        }

        bool isOpaqueTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolved = unwrapAliasTypeForCodegen(type);
            return resolved &&
                   resolved->kind() == sema::TypeKind::Primitive &&
                   resolved.AsFast<sema::PrimitiveType>()->name == "opaque";
        }

        bool isStructMemberFunctionSymbol(const Ref<sema::Symbol>& symbol)
        {
            if (!symbol)
                return false;

            if (symbol->kind == sema::SymbolKind::Function)
                return symbol->innerScope && symbol->innerScope->getKind() == sema::ScopeKind::Struct;

            if (symbol->kind != sema::SymbolKind::FunctionGroup)
                return false;

            if (symbol->innerScope && symbol->innerScope->getKind() == sema::ScopeKind::Struct)
                return true;

            for (const auto& overload : symbol->overloads)
            {
                if (isStructMemberFunctionSymbol(overload))
                    return true;
            }

            return false;
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

            Ref<sema::Type> current = unwrapAliasTypeForCodegen(type);
            if (!current)
                current = type;

            if (current->kind() == sema::TypeKind::Nullable)
                return toCppType(current.AsFast<sema::NullableType>()->valueType);

            auto buildReferenceCppType = [](const Ref<sema::ReferenceType>& refType) -> std::string
            {
                if (!refType || !refType->referredType)
                    return "void*";

                std::string referredCppType = toCppType(refType->referredType);
                Ref<sema::Type> resolvedReferredType = unwrapAliasTypeForCodegen(refType->referredType);

                if (resolvedReferredType && resolvedReferredType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedReferredType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                        return referredCppType;

                    if (structType->isObject)
                    {
                        std::string objectType = Mangler::mangleStruct(structType->name, structType->scopePath);
                        if (!structType->genericArguments.empty())
                        {
                            objectType += "<";
                            for (size_t i = 0; i < structType->genericArguments.size(); ++i)
                            {
                                objectType += toCppType(structType->genericArguments[i]);
                                if (i + 1 < structType->genericArguments.size())
                                    objectType += ", ";
                            }
                            objectType += ">";
                        }

                        return std::string("wio::runtime::") +
                               (refType->isMutable ? "BorrowedObjectRef<" : "BorrowedObjectView<") +
                               objectType + ">";
                    }
                }

                if (refType->isMutable)
                    return referredCppType + "*";

                if (!referredCppType.empty() && referredCppType.back() == '*')
                    return referredCppType + " const*";

                return "const " + referredCppType + "*";
            };

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

            enum class SyntheticPackElementKind : uint8_t
            {
                Absolute,
                FromEnd
            };

            struct SyntheticPackElement
            {
                std::string packName;
                std::size_t value = 0;
                SyntheticPackElementKind kind = SyntheticPackElementKind::Absolute;
            };

            auto tryParseSyntheticPackElementType = [](std::string_view name) -> std::optional<SyntheticPackElement>
            {
                const size_t openBracket = name.find('[');
                const size_t closeBracket = name.find(']');
                if (openBracket == std::string_view::npos || closeBracket == std::string_view::npos || closeBracket <= openBracket + 1)
                    return std::nullopt;

                const std::string packName(name.substr(0, openBracket));
                const std::string indexString(name.substr(openBracket + 1, closeBracket - openBracket - 1));
                if (packName.empty() || indexString.empty())
                    return std::nullopt;

                if (indexString == "last")
                    return SyntheticPackElement{packName, 1, SyntheticPackElementKind::FromEnd};

                if (indexString.starts_with("last-"))
                {
                    const std::string offsetText = indexString.substr(5);
                    if (offsetText.empty())
                        return std::nullopt;

                    try
                    {
                        return SyntheticPackElement{
                            packName,
                            static_cast<std::size_t>(std::stoull(offsetText)) + 1,
                            SyntheticPackElementKind::FromEnd
                        };
                    }
                    catch (...)
                    {
                        return std::nullopt;
                    }
                }

                try
                {
                    return SyntheticPackElement{
                        packName,
                        static_cast<std::size_t>(std::stoull(indexString)),
                        SyntheticPackElementKind::Absolute
                    };
                }
                catch (...)
                {
                    return std::nullopt;
                }
            };

            if (current->kind() == sema::TypeKind::GenericParameter)
            {
                const auto genericParameter = current.AsFast<sema::GenericParameterType>();
                if (auto syntheticPackElement = tryParseSyntheticPackElementType(genericParameter->name))
                {
                    if (syntheticPackElement->kind == SyntheticPackElementKind::FromEnd)
                    {
                        return common::formatString(
                            "typename wio::meta::TypePackView<{}...>::template At<(wio::meta::TypePackView<{}...>::size - {})>",
                            syntheticPackElement->packName,
                            syntheticPackElement->packName,
                            syntheticPackElement->value
                        );
                    }

                    return common::formatString(
                        "typename wio::meta::TypePackView<{}...>::template At<{}>",
                        syntheticPackElement->packName,
                        syntheticPackElement->value
                    );
                }
            }

            if (current->kind() == sema::TypeKind::GenericParameterPack)
                return current.AsFast<sema::GenericParameterPackType>()->name + "...";

            if (current->kind() == sema::TypeKind::Function)
            {
                auto funcType = current.AsFast<sema::FunctionType>();
                std::string result = "std::function<" + toCppType(funcType->returnType) + "(";
                for (size_t i = 0; i < funcType->paramTypes.size(); ++i) {
                    result += toCppType(funcType->paramTypes[i]);
                    if (i < funcType->paramTypes.size() - 1) result += ", ";
                }
                result += ")>";
                return result;
            }

            if (current->kind() == sema::TypeKind::AsyncTask)
            {
                auto taskType = current.AsFast<sema::AsyncTaskType>();
                return "wio::runtime::AsyncTask<" + toCppType(taskType->valueType) + ">";
            }
            
            if (current->kind() == sema::TypeKind::Reference)
            {
                auto refType = current.AsFast<sema::ReferenceType>();
                if (!refType)
                    return current->toCppString();

                if (refType->referredType && refType->referredType->kind() == sema::TypeKind::Struct)
                {
                    auto sType = refType->referredType.AsFast<sema::StructType>();
                    if (sType->isInterface)
                        return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*";
                }

                return buildReferenceCppType(refType);
            }
            else if (current->kind() == sema::TypeKind::Struct)
            {
                auto sType = current.AsFast<sema::StructType>();
                if (sType->isInterface)
                    return appendGenericArguments(Mangler::mangleInterface(sType->name, sType->scopePath), sType) + "*"; 
            }

            return current->toCppString();
        }

        Ref<sema::Type> instantiateGenericStructType(const Ref<sema::StructType>& structType,
                                                     const std::vector<Ref<sema::Type>>& explicitTypeArguments);

        Ref<sema::StructType> getNativePodComponentStructTypeForCodegen(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            if (current->kind() == sema::TypeKind::Reference)
                current = unwrapAliasType(current.AsFast<sema::ReferenceType>()->referredType);

            if (!current || current->kind() != sema::TypeKind::Struct)
                return nullptr;

            auto structType = current.AsFast<sema::StructType>();
            if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                return nullptr;

            if (!structType->genericArguments.empty())
            {
                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto baseSymbol = structScope->resolve(structType->name);
                        baseSymbol && baseSymbol->kind == sema::SymbolKind::Struct)
                    {
                        auto baseStruct = baseSymbol->type.AsFast<sema::StructType>();
                        if (baseStruct && baseStruct.Get() != structType.Get() && !baseStruct->genericParameterNames.empty())
                            structType = instantiateGenericStructType(baseStruct, structType->genericArguments).AsFast<sema::StructType>();
                    }
                }
            }

            return structType;
        }

        bool usesNativePodAliasModelForCodegen(const Ref<sema::StructType>& structType)
        {
            return static_cast<bool>(structType) &&
                   !structType->isObject &&
                   !structType->isInterface &&
                   structType->isNativePodComponent;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type, const GenericBindingSet& bindings);
        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings);

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
            case sema::TypeKind::ConstGenericParameter:
            case sema::TypeKind::GenericParameterPack:
            case sema::TypeKind::ValuePackView:
            case sema::TypeKind::TypePackView:
            case sema::TypeKind::PackStorage:
                return true;
            case sema::TypeKind::Reference:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::ReferenceType>()->referredType);
            case sema::TypeKind::Nullable:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::NullableType>()->valueType);
            case sema::TypeKind::AsyncTask:
                return containsGenericParameterTypeForCodegen(resolvedType.AsFast<sema::AsyncTaskType>()->valueType);
            case sema::TypeKind::Array:
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                return containsGenericParameterTypeForCodegen(arrayType->elementType) ||
                       (arrayType->extentType && containsGenericParameterTypeForCodegen(arrayType->extentType));
            }
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

            if (auto specializationIt = structType->explicitSpecializations.find(
                    sema::getGenericSpecializationKey(explicitTypeArguments));
                specializationIt != structType->explicitSpecializations.end())
            {
                if (auto specialization = specializationIt->second.Lock(); specialization)
                    return specialization;
            }

            const auto bindings = buildExtendedGenericBindings(
                structType->genericParameterNames,
                structType->hasGenericParameterPack,
                explicitTypeArguments
            );
            auto instantiatedScope = structType->structScope.Lock();
            auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                structType->name,
                instantiatedScope,
                structType->isObject,
                structType->isInterface
            ).AsFast<sema::StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericParameterTypes = structType->genericParameterTypes;
            instantiatedType->genericArguments = explicitTypeArguments;
            instantiatedType->genericPrimaryType = structType;
            instantiatedType->hasGenericParameterPack = structType->hasGenericParameterPack;
            instantiatedType->fieldNames = structType->fieldNames;
            instantiatedType->trustedTypeKeys = structType->trustedTypeKeys;
            instantiatedType->isFinal = structType->isFinal;
            instantiatedType->isNativePodComponent = structType->isNativePodComponent;
            instantiatedType->nativeCppName = structType->nativeCppName;
            instantiatedType->nativeCppHeader = structType->nativeCppHeader;

            instantiatedType->fieldTypes.clear();
            instantiatedType->fieldTypes.reserve(structType->fieldTypes.size());
            for (const auto& fieldType : structType->fieldTypes)
                instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, bindings));

            instantiatedType->baseTypes.clear();
            instantiatedType->baseTypes.reserve(structType->baseTypes.size());
            for (const auto& baseType : structType->baseTypes)
                instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, bindings));

            return instantiatedType;
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const std::unordered_map<std::string, Ref<sema::Type>>& bindings)
        {
            GenericBindingSet wrappedBindings;
            wrappedBindings.directBindings = bindings;
            return instantiateGenericType(type, wrappedBindings);
        }

        Ref<sema::Type> instantiateGenericType(const Ref<sema::Type>& type,
                                               const GenericBindingSet& bindings)
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
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto aliasIt = bindings.packAliases.find(parsedPackElement->packName); aliasIt != bindings.packAliases.end())
                    {
                        ParsedPackElementBinding reboundBinding = *parsedPackElement;
                        reboundBinding.packName = aliasIt->second;
                        return ctx.getOrCreateGenericParameterType(makePackElementBindingName(reboundBinding));
                    }

                    if (auto packIt = bindings.packBindings.find(parsedPackElement->packName);
                        packIt != bindings.packBindings.end() && !packIt->second.empty())
                    {
                        if (auto resolvedIndex = tryResolveConcretePackElementIndex(*parsedPackElement, packIt->second.size()))
                            return packIt->second[*resolvedIndex];
                    }
                }
                return current;
            }
            case sema::TypeKind::ConstGenericParameter:
            {
                auto genericParam = current.AsFast<sema::ConstGenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;
                return current;
            }
            case sema::TypeKind::ConstValue:
                return current;
            case sema::TypeKind::GenericParameterPack:
            {
                auto genericPack = current.AsFast<sema::GenericParameterPackType>();
                if (auto aliasIt = bindings.packAliases.find(genericPack->name); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(genericPack->name); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(genericPack->name, it->second);
                return current;
            }
            case sema::TypeKind::ValuePackView:
            {
                auto viewType = current.AsFast<sema::ValuePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(viewType->elementTypes.size());
                    for (const auto& elementType : viewType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreateValuePackViewType(viewType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(viewType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateValuePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(viewType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreateValuePackViewType(viewType->packName, it->second);
                return current;
            }
            case sema::TypeKind::TypePackView:
            {
                auto viewType = current.AsFast<sema::TypePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(viewType->elementTypes.size());
                    for (const auto& elementType : viewType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreateTypePackViewType(viewType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(viewType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(viewType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(viewType->packName, it->second);
                return current;
            }
            case sema::TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<sema::PackStorageType>();
                if (!storageType->elementTypes.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedElements;
                    instantiatedElements.reserve(storageType->elementTypes.size());
                    for (const auto& elementType : storageType->elementTypes)
                        instantiatedElements.push_back(instantiateGenericType(elementType, bindings));
                    return ctx.getOrCreatePackStorageType(storageType->packName, std::move(instantiatedElements));
                }

                if (auto aliasIt = bindings.packAliases.find(storageType->packName); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreatePackStorageType(aliasIt->second);
                if (auto it = bindings.packBindings.find(storageType->packName); it != bindings.packBindings.end())
                    return ctx.getOrCreatePackStorageType(storageType->packName, it->second);
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
            case sema::TypeKind::Nullable:
                return ctx.getOrCreateNullableType(
                    instantiateGenericType(current.AsFast<sema::NullableType>()->valueType, bindings)
                );
            case sema::TypeKind::Array:
            {
                auto arrayType = current.AsFast<sema::ArrayType>();
                Ref<sema::Type> instantiatedExtent = arrayType->extentType
                    ? instantiateGenericType(arrayType->extentType, bindings)
                    : nullptr;
                size_t concreteSize = arrayType->size;
                if (instantiatedExtent && instantiatedExtent->kind() == sema::TypeKind::ConstValue)
                {
                    const std::string rawValue = common::stripIntegerLiteralTypeSuffix(
                        instantiatedExtent.AsFast<sema::ConstValueType>()->value);
                    concreteSize = static_cast<size_t>(std::stoull(rawValue));
                }
                return ctx.getOrCreateArrayType(
                    instantiateGenericType(arrayType->elementType, bindings),
                    arrayType->arrayKind,
                    concreteSize,
                    instantiatedExtent
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
                instantiatedParamTypes.reserve(functionType->paramTypes.size() + 4);

                if (functionType->hasParameterPack && !functionType->paramTypes.empty())
                {
                    const size_t fixedParameterCount = functionType->paramTypes.size() - 1;
                    for (size_t i = 0; i < fixedParameterCount; ++i)
                        instantiatedParamTypes.push_back(instantiateGenericType(functionType->paramTypes[i], bindings));

                    auto trailingType = unwrapAliasType(functionType->paramTypes.back());
                    if (trailingType && trailingType->kind() == sema::TypeKind::GenericParameterPack)
                    {
                        const std::string& packName = trailingType.AsFast<sema::GenericParameterPackType>()->name;
                        if (auto aliasIt = bindings.packAliases.find(packName); aliasIt != bindings.packAliases.end())
                        {
                            instantiatedParamTypes.push_back(ctx.getOrCreateGenericParameterPackType(aliasIt->second));
                            return ctx.getOrCreateFunctionType(
                                instantiateGenericType(functionType->returnType, bindings),
                                instantiatedParamTypes,
                                true
                            );
                        }
                        if (auto it = bindings.packBindings.find(packName); it != bindings.packBindings.end())
                        {
                            if (it->second.empty())
                            {
                                return ctx.getOrCreateFunctionType(
                                    instantiateGenericType(functionType->returnType, bindings),
                                    instantiatedParamTypes,
                                    false
                                );
                            }
                            if (!it->second.empty())
                            {
                                for (const auto& packType : it->second)
                                    instantiatedParamTypes.push_back(packType);
                                return ctx.getOrCreateFunctionType(
                                    instantiateGenericType(functionType->returnType, bindings),
                                    instantiatedParamTypes,
                                    false
                                );
                            }
                        }
                    }

                    instantiatedParamTypes.push_back(instantiateGenericType(functionType->paramTypes.back(), bindings));
                }
                else
                {
                    for (const auto& paramType : functionType->paramTypes)
                        instantiatedParamTypes.push_back(instantiateGenericType(paramType, bindings));
                }

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(functionType->returnType, bindings),
                    instantiatedParamTypes,
                    functionType->hasParameterPack
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
                    {
                        auto instantiatedArgument = instantiateGenericType(genericArgument, bindings);
                        auto resolvedArgument = unwrapAliasType(instantiatedArgument);
                        if (structType->hasGenericParameterPack && resolvedArgument &&
                            resolvedArgument->kind() == sema::TypeKind::TypePackView)
                        {
                            const auto packView = resolvedArgument.AsFast<sema::TypePackViewType>();
                            instantiatedArguments.insert(
                                instantiatedArguments.end(),
                                packView->elementTypes.begin(),
                                packView->elementTypes.end()
                            );
                        }
                        else
                        {
                            instantiatedArguments.push_back(instantiatedArgument);
                        }
                    }

                    return instantiateGenericStructType(structType, instantiatedArguments);
                }

                if (!structType->genericParameterNames.empty())
                {
                    std::vector<Ref<sema::Type>> instantiatedArguments;
                    const size_t fixedCount = getMinimumGenericArgumentCount(
                        structType->genericParameterNames,
                        structType->hasGenericParameterPack
                    );
                    instantiatedArguments.reserve(fixedCount + 4);
                    for (size_t i = 0; i < fixedCount; ++i)
                    {
                        if (auto it = bindings.directBindings.find(structType->genericParameterNames[i]); it != bindings.directBindings.end())
                            instantiatedArguments.push_back(it->second);
                        else
                            return current;
                    }

                    if (structType->hasGenericParameterPack)
                    {
                        const std::string& packName = structType->genericParameterNames.back();
                        auto bindingIt = bindings.packBindings.find(packName);
                        if (bindingIt == bindings.packBindings.end())
                            return current;

                        for (const auto& boundType : bindingIt->second)
                            instantiatedArguments.push_back(boundType);
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

        bool shouldTreatReferenceAsHandleForAssignment(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                return false;

            auto referenceType = resolvedType.AsFast<sema::ReferenceType>();
            Ref<sema::Type> referredType = unwrapAliasType(referenceType->referredType);
            if (!referredType || referredType->kind() != sema::TypeKind::Struct)
                return false;

            auto structType = referredType.AsFast<sema::StructType>();
            return structType && (structType->isObject || structType->isInterface);
        }

        std::string_view getIntrinsicHelperName(const IntrinsicMember member)
        {
            switch (member)
            {
            case IntrinsicMember::TaskStart:
                return "TaskStart";
            case IntrinsicMember::TaskCancel:
                return "TaskCancel";
            case IntrinsicMember::TaskIsReady:
                return "TaskIsReady";
            case IntrinsicMember::TaskIsCancelled:
                return "TaskIsCancelled";
            case IntrinsicMember::TaskIsFaulted:
                return "TaskIsFaulted";
            case IntrinsicMember::TaskWaitFor:
                return "TaskWaitFor";
            case IntrinsicMember::TaskBlock:
                return "TaskBlock";
            case IntrinsicMember::TaskCancelAfter:
                return "TaskCancelAfter";
            case IntrinsicMember::TaskDetach:
                return "TaskDetach";
            case IntrinsicMember::ArrayCount:
            case IntrinsicMember::DictCount:
            case IntrinsicMember::StringCount:
                return "Count";
            case IntrinsicMember::ArrayEmpty:
            case IntrinsicMember::DictEmpty:
            case IntrinsicMember::StringEmpty:
                return "Empty";
            case IntrinsicMember::TextCount:
                return "TextCount";
            case IntrinsicMember::TextByteCount:
                return "TextByteCount";
            case IntrinsicMember::TextEmpty:
                return "TextEmpty";
            case IntrinsicMember::TextSlice:
                return "TextSlice";
            case IntrinsicMember::TextToString:
                return "TextToString";
            case IntrinsicMember::TextContains:
                return "TextContains";
            case IntrinsicMember::TextStartsWith:
                return "TextStartsWith";
            case IntrinsicMember::TextEndsWith:
                return "TextEndsWith";
            case IntrinsicMember::TextGraphemeCount:
                return "TextGraphemeCount";
            case IntrinsicMember::TextSliceGraphemes:
                return "TextSliceGraphemes";
            case IntrinsicMember::TextDisplayWidth:
                return "TextDisplayWidth";
            case IntrinsicMember::TextCaseFold:
                return "TextCaseFold";
            case IntrinsicMember::TextCodePoints:
                return "TextCodePoints";
            case IntrinsicMember::TextGraphemes:
                return "TextGraphemes";
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
            case IntrinsicMember::ArrayGet:
                return "Index";
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
            case IntrinsicMember::StringGet:
                return "Index";
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
            case IntrinsicMember::StringToI8:
                return "StringToI8";
            case IntrinsicMember::StringToI16:
                return "StringToI16";
            case IntrinsicMember::StringToI32:
                return "StringToI32";
            case IntrinsicMember::StringToI64:
                return "StringToI64";
            case IntrinsicMember::StringToU8:
                return "StringToU8";
            case IntrinsicMember::StringToU16:
                return "StringToU16";
            case IntrinsicMember::StringToU32:
                return "StringToU32";
            case IntrinsicMember::StringToU64:
                return "StringToU64";
            case IntrinsicMember::StringToISize:
                return "StringToISize";
            case IntrinsicMember::StringToUSize:
                return "StringToUSize";
            case IntrinsicMember::StringToF32:
                return "StringToF32";
            case IntrinsicMember::StringToF64:
                return "StringToF64";
            case IntrinsicMember::StringToBool:
                return "StringToBool";
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
            case IntrinsicMember::EnumName:
            case IntrinsicMember::FlagsetName:
                return "EnumName";
            case IntrinsicMember::EnumRawValue:
                return "EnumRawValue";
            case IntrinsicMember::EnumIsValid:
                return "EnumIsValid";
            case IntrinsicMember::FlagsetHasAll:
                return "FlagsetHasAll";
            case IntrinsicMember::FlagsetHasAny:
                return "FlagsetHasAny";
            case IntrinsicMember::FlagsetWith:
                return "FlagsetWith";
            case IntrinsicMember::FlagsetWithout:
                return "FlagsetWithout";
            case IntrinsicMember::FlagsetToggle:
                return "FlagsetToggle";
            case IntrinsicMember::FlagsetClear:
                return "FlagsetClear";
            case IntrinsicMember::TaskPoll:
            case IntrinsicMember::TaskWithin:
            case IntrinsicMember::None:
                return {};
            }

            return {};
        }

        std::vector<std::string> getBaseInterfaces(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            std::vector<std::string> bases;
            for (const auto& attr : attributes)
                if (attr && matchesBuiltinAttribute(*attr, Attribute::From))
                    for (const auto& arg : attr->args)
                        if (arg.type == TokenType::identifier)
                            bases.push_back(arg.value);
            return bases;
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

            if (node.isExtensionMethod && !node.extensionMemberName.empty())
                return node.extensionMemberName;

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
            bool valueRequiresBridgeCast = false;
        };

        struct ExportedFieldInfo
        {
            const VariableDeclaration* declaration = nullptr;
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
            const FunctionDeclaration* declaration = nullptr;
            std::string methodName;
            size_t exportIndex = 0;
        };

        struct ExportedConstructorInfo
        {
            size_t exportIndex = 0;
        };

        struct ExportedTypeInfo
        {
            const std::vector<NodePtr<AttributeStatement>>* attributes = nullptr;
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

            if (resolvedType->kind() == sema::TypeKind::Nullable)
                return getAbiTypeEnumName(resolvedType.AsFast<sema::NullableType>()->valueType);

            if (resolvedType->isVoid())
                return "WIO_ABI_VOID";

            if (resolvedType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                if (structType && (structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                    return getAbiTypeEnumName(structType->enumUnderlyingType);
            }

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
            if (!resolvedType)
                return {};

            if (resolvedType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                if (structType && (structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                    return getAbiValueFieldName(structType->enumUnderlyingType);
            }

            if (resolvedType->kind() != sema::TypeKind::Primitive)
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
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectModuleLifecycleFunctions(group->declarations, lifecycleFunctions);
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

        const FunctionDeclaration* findApplicationEntry(const std::vector<NodePtr<Statement>>& statements)
        {
            for (const auto& statement : statements)
            {
                if (!statement)
                    continue;
                if (const auto* realmDecl = statement->as<RealmDeclaration>())
                {
                    if (const auto* nested = findApplicationEntry(realmDecl->statements))
                        return nested;
                    continue;
                }
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    if (const auto* nested = findApplicationEntry(group->declarations))
                        return nested;
                    continue;
                }
                if (const auto* fnDecl = statement->as<FunctionDeclaration>();
                    fnDecl && fnDecl->isApplicationEntry)
                    return fnDecl;
            }
            return nullptr;
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
                if (const auto* group = statement->as<DeclarationGroup>())
                {
                    collectExportedFunctions(group->declarations, exportedFunctions);
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
                            auto bindings = buildExtendedGenericBindings(
                                exportSymbol->genericParameterNames,
                                exportSymbol->hasGenericParameterPack,
                                instantiationTypes
                            );
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
                    auto bridgeResolvedFieldType = resolvedFieldType;
                    if (bridgeResolvedFieldType && bridgeResolvedFieldType->kind() == sema::TypeKind::Nullable)
                    {
                        bridgeResolvedFieldType = unwrapAliasType(
                            bridgeResolvedFieldType.AsFast<sema::NullableType>()->valueType
                        );
                    }
                    ExportedFunctionInfo::FieldAccessorKind accessorKind = ExportedFunctionInfo::FieldAccessorKind::Value;
                    Ref<sema::Type> accessorBridgeType = fieldType;
                    std::string fieldBridgeCppTypeName = toCppType(fieldType);
                    bool valueRequiresBridgeCast = false;

                    if (bridgeResolvedFieldType && bridgeResolvedFieldType->kind() == sema::TypeKind::Struct)
                    {
                        if (auto structType = bridgeResolvedFieldType.AsFast<sema::StructType>(); structType)
                        {
                            if ((structType->isEnum || structType->isFlagset) && structType->enumUnderlyingType)
                            {
                                accessorBridgeType = structType->enumUnderlyingType;
                                valueRequiresBridgeCast = true;
                            }
                            else if (objectDeclarations.contains(structType.Get()) &&
                                     getStdValueStructType(structType, "ByteBuffer") == nullptr)
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ObjectHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                            else if (componentDeclarations.contains(structType.Get()) &&
                                     getStdValueStructType(structType, "ResultUnit") == nullptr &&
                                     getStdValueStructType(structType, "Span") == nullptr)
                            {
                                accessorKind = ExportedFunctionInfo::FieldAccessorKind::ComponentHandle;
                                accessorBridgeType = typeContext.getUSize();
                                fieldBridgeCppTypeName = mangleStructTypeName(structType);
                            }
                        }
                    }

                    ExportedFieldInfo fieldInfo;
                    fieldInfo.declaration = &variableDecl;
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

                    const bool isTextField = resolvedFieldType && resolvedFieldType->kind() == sema::TypeKind::Primitive &&
                        resolvedFieldType.AsFast<sema::PrimitiveType>()->name == "text";
                    auto optionFieldType = getStdValueStructType(resolvedFieldType, "Option");
                    const bool isOptionField = optionFieldType && optionFieldType->genericArguments.size() == 1 &&
                        isSdkValueBridgeType(optionFieldType->genericArguments.front());
                    auto resultFieldType = getStdValueStructType(resolvedFieldType, "Result");
                    const bool isResultField = resultFieldType && resultFieldType->genericArguments.size() == 1 &&
                        isSdkValueBridgeType(resultFieldType->genericArguments.front());
                    const bool isUnitField = getStdValueStructType(resolvedFieldType, "ResultUnit") != nullptr;
                    const bool isSpanField = getStdValueStructType(resolvedFieldType, "Span") != nullptr;
                    const bool isByteBufferField = getStdValueStructType(resolvedFieldType, "ByteBuffer") != nullptr;
                    auto queueFieldType = getStdValueStructType(resolvedFieldType, "Queue");
                    auto unorderedSetFieldType = getStdValueStructType(resolvedFieldType, "UnorderedSet");
                    auto orderedSetFieldType = getStdValueStructType(resolvedFieldType, "OrderedSet");
                    auto tupleFieldType = getStdValueStructType(resolvedFieldType, "Tuple");
                    const bool isTupleField = tupleFieldType && std::all_of(
                        tupleFieldType->genericArguments.begin(),
                        tupleFieldType->genericArguments.end(),
                        [](const Ref<sema::Type>& argument) { return isSdkValueBridgeType(argument); }
                    );
                    const bool isSequenceContainerField =
                        (queueFieldType && queueFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(queueFieldType->genericArguments.front())) ||
                        (unorderedSetFieldType && unorderedSetFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(unorderedSetFieldType->genericArguments.front())) ||
                        (orderedSetFieldType && orderedSetFieldType->genericArguments.size() == 1 && isSdkValueBridgeType(orderedSetFieldType->genericArguments.front()));
                    const bool needsDynamicBridge = resolvedFieldType &&
                        (isTextField || isOptionField || isResultField || isUnitField || isSpanField || isByteBufferField ||
                         isSequenceContainerField || isTupleField ||
                         resolvedFieldType->kind() == sema::TypeKind::Array ||
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
                    getterExport.valueRequiresBridgeCast = valueRequiresBridgeCast;
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
                        setterExport.valueRequiresBridgeCast = valueRequiresBridgeCast;
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
                    methodExport.declaration = &functionDecl;
                    methodExport.functionType = typeContext.getOrCreateFunctionType(functionType->returnType, exportedParameterTypes).AsFast<sema::FunctionType>();
                    methodExport.logicalName = typeInfo.logicalName + "." + functionName;
                    methodExport.symbolName = common::formatString("WioMethod__{}__{}__{}", typeInfo.symbolName, functionName, Mangler::mangleFunction(functionName, functionType->paramTypes));
                    methodExport.syntheticKind = ExportedFunctionInfo::SyntheticKind::TypeMethod;
                    methodExport.ownerCppTypeName = typeInfo.cppTypeName;
                    methodExport.memberCppName = Mangler::mangleFunction(functionName, functionType->paramTypes);
                    methodExport.ownerIsObject = true;

                    ExportedMethodInfo methodInfo;
                    methodInfo.declaration = &functionDecl;
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
                    typeInfo.attributes = &componentDecl->attributes;
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
                typeInfo.attributes = &objectDecl->attributes;
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

        void appendNativeHeader(std::string header,
                                std::unordered_set<std::string>& seenHeaders,
                                std::vector<std::string>& orderedHeaders)
        {
            if (!header.empty() && seenHeaders.insert(header).second)
                orderedHeaders.push_back(std::move(header));
        }

        void collectCppHeadersFromFunction(const FunctionDeclaration& declaration,
                                           std::unordered_set<std::string>& seenHeaders,
                                           std::vector<std::string>& orderedHeaders)
        {
            if (!isNativeFunction(declaration))
                return;

            auto headerArg = getSingleAttributeArg(declaration.attributes, Attribute::CppHeader);
            if (!headerArg.has_value() || headerArg->type != TokenType::stringLiteral)
                return;

            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
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
                        appendNativeHeader(useStmt->modulePath, seenHeaders, orderedHeaders);
                        continue;
                    }
                }

                if (const auto* fnDecl = statement->as<FunctionDeclaration>())
                {
                    collectCppHeadersFromFunction(*fnDecl, seenHeaders, orderedHeaders);
                    continue;
                }

                if (const auto* objectDecl = statement->as<ObjectDeclaration>())
                {
                    if (hasAttribute(objectDecl->attributes, Attribute::Native))
                    {
                        if (auto headerArg = getSingleAttributeArg(objectDecl->attributes, Attribute::CppHeader); headerArg.has_value() && headerArg->type == TokenType::stringLiteral)
                            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
                    }

                    for (const auto& member : objectDecl->members)
                    {
                        if (member.declaration && member.declaration->is<FunctionDeclaration>())
                            collectCppHeadersFromFunction(*member.declaration->as<FunctionDeclaration>(), seenHeaders, orderedHeaders);
                    }
                    continue;
                }

                if (const auto* componentDecl = statement->as<ComponentDeclaration>())
                {
                    if (hasAttribute(componentDecl->attributes, Attribute::Native))
                    {
                        if (auto headerArg = getSingleAttributeArg(componentDecl->attributes, Attribute::CppHeader); headerArg.has_value() && headerArg->type == TokenType::stringLiteral)
                            appendNativeHeader(headerArg->value, seenHeaders, orderedHeaders);
                    }

                    for (const auto& member : componentDecl->members)
                    {
                        if (member.declaration && member.declaration->is<FunctionDeclaration>())
                            collectCppHeadersFromFunction(*member.declaration->as<FunctionDeclaration>(), seenHeaders, orderedHeaders);
                    }
                    continue;
                }

                if (const auto* interfaceDecl = statement->as<InterfaceDeclaration>())
                {
                    for (const auto& method : interfaceDecl->methods)
                    {
                        if (method)
                            collectCppHeadersFromFunction(*method, seenHeaders, orderedHeaders);
                    }
                }
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

        std::optional<std::string> getSdkDynamicBridgeCppType(const Ref<sema::Type>& type)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "void" || name == "object" || name == "any" || name == "opaque")
                    return std::nullopt;
                if (name == "text")
                    return "std::string";
                if (name == "uchar") return "wio::sdk::WioUChar";
                if (name == "byte") return "wio::sdk::WioByte";
                if (name == "u8") return "wio::sdk::WioU8";
                if (name == "i64") return "wio::sdk::WioI64";
                if (name == "u64") return "wio::sdk::WioU64";
                if (name == "isize") return "wio::sdk::WioISize";
                if (name == "usize") return "wio::sdk::WioUSize";
                return toCppType(resolvedType);
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                auto elementType = arrayType ? getSdkDynamicBridgeCppType(arrayType->elementType) : std::nullopt;
                if (!arrayType || !elementType.has_value())
                    return std::nullopt;

                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    return "std::array<" + *elementType + ", " + std::to_string(arrayType->size) + ">";
                }
                return "std::vector<" + *elementType + ">";
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                auto keyType = dictionaryType ? getSdkDynamicBridgeCppType(dictionaryType->keyType) : std::nullopt;
                auto valueType = dictionaryType ? getSdkDynamicBridgeCppType(dictionaryType->valueType) : std::nullopt;
                if (!dictionaryType || !keyType.has_value() || !valueType.has_value())
                    return std::nullopt;

                return std::string(dictionaryType->isOrdered ? "std::map<" : "std::unordered_map<") +
                    *keyType + ", " + *valueType + ">";
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(optionType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;
                return "wio::sdk::WioOption<" + *valueType + ">";
            }

            if (getStdValueStructType(resolvedType, "ResultUnit"))
                return "wio::sdk::WioUnit";

            if (getStdValueStructType(resolvedType, "Span"))
                return "wio::sdk::WioSpanRange";

            if (getStdValueStructType(resolvedType, "ByteBuffer"))
                return "wio::sdk::WioByteBuffer";

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                std::string hostType = "wio::sdk::WioTuple<";
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto argumentType = getSdkDynamicBridgeCppType(tupleType->genericArguments[index]);
                    if (!argumentType.has_value())
                        return std::nullopt;
                    if (index > 0)
                        hostType += ", ";
                    hostType += *argumentType;
                }
                hostType += ">";
                return hostType;
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(resultType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;
                return "wio::sdk::WioResult<" + *valueType + ">";
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                auto valueType = getSdkDynamicBridgeCppType(sequenceType->genericArguments.front());
                if (!valueType.has_value())
                    return std::nullopt;

                const std::string wrapperName = queueType
                    ? "wio::sdk::WioQueue<"
                    : (unorderedSetType ? "wio::sdk::WioUnorderedSet<" : "wio::sdk::WioOrderedSet<");
                return wrapperName + *valueType + ">";
            }

            return std::nullopt;
        }

        std::optional<std::string> makeSdkDynamicToHostExpression(const std::string& expression,
                                                                  const Ref<sema::Type>& type,
                                                                  const std::size_t depth = 0)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "text")
                    return "(" + expression + ").Utf8()";
                if (name == "uchar") return "wio::sdk::WioUChar(" + expression + ")";
                if (name == "byte") return "wio::sdk::WioByte(" + expression + ")";
                if (name == "u8") return "wio::sdk::WioU8(" + expression + ")";
                if (name == "i64") return "wio::sdk::WioI64(" + expression + ")";
                if (name == "u64") return "wio::sdk::WioU64(" + expression + ")";
                if (name == "isize") return "wio::sdk::WioISize(" + expression + ")";
                if (name == "usize") return "wio::sdk::WioUSize(" + expression + ")";
                if (getSdkDynamicBridgeCppType(resolvedType).has_value())
                    return expression;
                return std::nullopt;
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!arrayType || !hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_array_" + std::to_string(depth);
                const std::string itemExpression = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? sourceName + "[_wio_index]"
                    : "_wio_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicToHostExpression(itemExpression, arrayType->elementType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    std::string initializers;
                    for (std::size_t index = 0; index < arrayType->size; ++index)
                    {
                        auto indexedValue = makeSdkDynamicToHostExpression(
                            sourceName + "[" + std::to_string(index) + "]",
                            arrayType->elementType,
                            depth + 1
                        );
                        if (!indexedValue.has_value())
                            return std::nullopt;
                        if (!initializers.empty())
                            initializers += ", ";
                        initializers += *indexedValue;
                    }

                    return common::formatString(
                        "([&]() -> {} {{ const auto& {} = {}; return {}{{{}}}; }}())",
                        *hostType,
                        sourceName,
                        expression,
                        *hostType,
                        initializers
                    );
                }

                const std::string itemName = "_wio_item_" + std::to_string(depth);
                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "_wio_output.reserve({}.size()); for (const auto& {} : {}) "
                    "_wio_output.push_back({}); return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    sourceName,
                    itemName,
                    sourceName,
                    *convertedItem
                );
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!dictionaryType || !hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_dictionary_" + std::to_string(depth);
                const std::string keyName = "_wio_key_" + std::to_string(depth);
                const std::string valueName = "_wio_value_" + std::to_string(depth);
                auto convertedKey = makeSdkDynamicToHostExpression(keyName, dictionaryType->keyType, depth + 1);
                auto convertedValue = makeSdkDynamicToHostExpression(valueName, dictionaryType->valueType, depth + 1);
                if (!convertedKey.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "for (const auto& [{}, {}] : {}) _wio_output.emplace({}, {}); "
                    "return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    keyName,
                    valueName,
                    sourceName,
                    *convertedKey,
                    *convertedValue
                );
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = optionType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(valueType);
                const std::string variableName = "_wio_option_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicToHostExpression(
                    variableName + "->_WF_Value()",
                    valueType,
                    depth + 1
                );
                if (!hostType.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> wio::sdk::WioOption<{}> {{ auto {} = {}; "
                    "if (!{} || !{}->_WF_IsSome()) return wio::sdk::WioOption<{}>::none(); "
                    "return wio::sdk::WioOption<{}>::some({}); }}())",
                    *hostType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    *hostType,
                    *hostType,
                    *convertedValue
                );
            }

            if (getStdValueStructType(resolvedType, "ResultUnit"))
                return "wio::sdk::WioUnit{}";

            if (getStdValueStructType(resolvedType, "Span"))
            {
                return "wio::sdk::WioSpanRange{static_cast<std::size_t>((" + expression +
                    ").start), static_cast<std::size_t>((" + expression + ").count)}";
            }

            if (getStdValueStructType(resolvedType, "ByteBuffer"))
            {
                const std::string sourceName = "_wio_byte_buffer_" + std::to_string(depth);
                return common::formatString(
                    "([&]() -> wio::sdk::WioByteBuffer {{ auto {} = {}; wio::sdk::WioByteBuffer _wio_output; "
                    "if (!{}) return _wio_output; _wio_output.reserve(static_cast<std::size_t>({}->_WF_Capacity())); "
                    "for (const auto _wio_byte : {}->_WF_ToArray()) "
                    "_wio_output.write(static_cast<std::byte>(_wio_byte)); "
                    "(void)_wio_output.seek(static_cast<std::size_t>({}->_WF_Position())); return _wio_output; }}())",
                    sourceName,
                    expression,
                    sourceName,
                    sourceName,
                    sourceName,
                    sourceName
                );
            }

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                if (!hostType.has_value())
                    return std::nullopt;

                const std::string sourceName = "_wio_tuple_" + std::to_string(depth);
                std::string values;
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto convertedValue = makeSdkDynamicToHostExpression(
                        sourceName + "->data.template Get<" + std::to_string(index) + ">()",
                        tupleType->genericArguments[index],
                        depth + 1
                    );
                    if (!convertedValue.has_value())
                        return std::nullopt;
                    if (!values.empty())
                        values += ", ";
                    values += *convertedValue;
                }

                return common::formatString(
                    "([&]() -> {} {{ auto {} = {}; if (!{}) throw std::runtime_error(\"Wio SDK tuple field is null.\"); "
                    "return {}{{{}}}; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    sourceName,
                    *hostType,
                    values
                );
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = resultType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(valueType);
                const std::string variableName = "_wio_result_" + std::to_string(depth);
                const std::string errorName = "_wio_error_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicToHostExpression(
                    variableName + "->_WF_Value()",
                    valueType,
                    depth + 1
                );
                if (!hostType.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                return common::formatString(
                    "([&]() -> wio::sdk::WioResult<{}> {{ auto {} = {}; "
                    "if (!{} || {}->_WF_IsError()) {{ auto {} = {}->_WF_ErrorValue(); "
                    "return wio::sdk::WioResult<{}>::error(wio::sdk::WioResultError{{"
                    "static_cast<wio::sdk::WioResultDomain>(static_cast<std::int32_t>({}.domain)), "
                    "{}.code, {}.nativeCode, {}.message}}); }} "
                    "return wio::sdk::WioResult<{}>::ok({}); }}())",
                    *hostType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    errorName,
                    variableName,
                    *hostType,
                    errorName,
                    errorName,
                    errorName,
                    errorName,
                    *hostType,
                    *convertedValue
                );
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = sequenceType->genericArguments.front();
                auto hostType = getSdkDynamicBridgeCppType(resolvedType);
                const std::string sourceName = "_wio_sequence_" + std::to_string(depth);
                const std::string itemName = "_wio_sequence_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicToHostExpression(itemName, valueType, depth + 1);
                if (!hostType.has_value() || !convertedItem.has_value())
                    return std::nullopt;

                const std::string appendExpression = queueType
                    ? "_wio_output.push(" + *convertedItem + ");"
                    : "(void)_wio_output.add(" + *convertedItem + ");";
                return common::formatString(
                    "([&]() -> {} {{ auto {} = {}; {} _wio_output; if (!{}) return _wio_output; "
                    "for (const auto& {} : {}->_WF_ToArray()) {{ {} }} return _wio_output; }}())",
                    *hostType,
                    sourceName,
                    expression,
                    *hostType,
                    sourceName,
                    itemName,
                    sourceName,
                    appendExpression
                );
            }

            return std::nullopt;
        }

        std::optional<std::string> makeSdkDynamicFromHostExpression(const std::string& expression,
                                                                    const Ref<sema::Type>& type,
                                                                    const std::size_t depth = 0)
        {
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return std::nullopt;

            if (resolvedType->kind() == sema::TypeKind::Primitive)
            {
                const std::string& name = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (name == "text")
                    return "wio::runtime::Text::FromUtf8(" + expression + ")";
                if (name == "uchar" || name == "byte" || name == "u8" ||
                    name == "i64" || name == "u64" || name == "isize" || name == "usize")
                {
                    return "static_cast<" + toCppType(resolvedType) + ">((" + expression + ").value())";
                }
                if (getSdkDynamicBridgeCppType(resolvedType).has_value())
                    return expression;
                return std::nullopt;
            }

            if (resolvedType->kind() == sema::TypeKind::Array)
            {
                auto arrayType = resolvedType.AsFast<sema::ArrayType>();
                if (!arrayType)
                    return std::nullopt;

                const std::string sourceName = "_wio_host_array_" + std::to_string(depth);
                const std::string itemName = "_wio_host_item_" + std::to_string(depth);
                const std::string itemExpression = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? sourceName + "[_wio_index]"
                    : itemName;
                auto convertedItem = makeSdkDynamicFromHostExpression(itemExpression, arrayType->elementType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                const std::string cppType = toCppType(resolvedType);
                if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static)
                {
                    return common::formatString(
                        "([&]() -> {} {{ const auto& {} = {}; {} _wio_output{{}}; "
                        "for (std::size_t _wio_index = 0; _wio_index < {}; ++_wio_index) "
                        "_wio_output[_wio_index] = {}; return _wio_output; }}())",
                        cppType,
                        sourceName,
                        expression,
                        cppType,
                        arrayType->size,
                        *convertedItem
                    );
                }

                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "_wio_output.reserve({}.size()); for (const auto& {} : {}) "
                    "_wio_output.push_back({}); return _wio_output; }}())",
                    cppType,
                    sourceName,
                    expression,
                    cppType,
                    sourceName,
                    itemName,
                    sourceName,
                    *convertedItem
                );
            }

            if (resolvedType->kind() == sema::TypeKind::Dictionary)
            {
                auto dictionaryType = resolvedType.AsFast<sema::DictionaryType>();
                if (!dictionaryType)
                    return std::nullopt;

                const std::string sourceName = "_wio_host_dictionary_" + std::to_string(depth);
                const std::string keyName = "_wio_host_key_" + std::to_string(depth);
                const std::string valueName = "_wio_host_value_" + std::to_string(depth);
                auto convertedKey = makeSdkDynamicFromHostExpression(keyName, dictionaryType->keyType, depth + 1);
                auto convertedValue = makeSdkDynamicFromHostExpression(valueName, dictionaryType->valueType, depth + 1);
                if (!convertedKey.has_value() || !convertedValue.has_value())
                    return std::nullopt;

                const std::string cppType = toCppType(resolvedType);
                return common::formatString(
                    "([&]() -> {} {{ const auto& {} = {}; {} _wio_output; "
                    "for (const auto& [{}, {}] : {}) _wio_output.emplace({}, {}); "
                    "return _wio_output; }}())",
                    cppType,
                    sourceName,
                    expression,
                    cppType,
                    keyName,
                    valueName,
                    sourceName,
                    *convertedKey,
                    *convertedValue
                );
            }

            if (auto optionType = getStdValueStructType(resolvedType, "Option");
                optionType && optionType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = optionType->genericArguments.front();
                const std::string variableName = "_wio_option_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicFromHostExpression(
                    variableName + ".value()",
                    valueType,
                    depth + 1
                );
                if (!convertedValue.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(optionType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "if ({}.is_none()) return wio::runtime::Ref<{}>::Create(); "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    variableName,
                    expression,
                    variableName,
                    cppStructType,
                    cppStructType,
                    *convertedValue
                );
            }

            if (auto unitType = getStdValueStructType(resolvedType, "ResultUnit"); unitType)
                return mangleStructTypeName(unitType) + "()";

            if (auto spanType = getStdValueStructType(resolvedType, "Span"); spanType)
            {
                return mangleStructTypeName(spanType) + "(static_cast<std::size_t>((" + expression +
                    ").start()), static_cast<std::size_t>((" + expression + ").count()))";
            }

            if (auto byteBufferType = getStdValueStructType(resolvedType, "ByteBuffer"); byteBufferType)
            {
                const std::string sourceName = "_wio_host_byte_buffer_" + std::to_string(depth);
                const std::string cppStructType = mangleStructTypeName(byteBufferType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; wio::DArray<uint8_t> _wio_bytes; "
                    "_wio_bytes.reserve({}.count()); for (const auto _wio_byte : {}.data()) "
                    "_wio_bytes.push_back(std::to_integer<uint8_t>(_wio_byte)); "
                    "auto _wio_output = wio::runtime::Ref<{}>::Create(_wio_bytes); "
                    "_wio_output->_WF_Reserve_usize(static_cast<std::size_t>({}.capacity())); "
                    "(void)_wio_output->_WF_Seek_usize(static_cast<std::size_t>({}.position())); return _wio_output; }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    sourceName,
                    sourceName,
                    cppStructType,
                    sourceName,
                    sourceName
                );
            }

            if (auto tupleType = getStdValueStructType(resolvedType, "Tuple"); tupleType)
            {
                const std::string sourceName = "_wio_host_tuple_" + std::to_string(depth);
                std::string values;
                for (std::size_t index = 0; index < tupleType->genericArguments.size(); ++index)
                {
                    auto convertedValue = makeSdkDynamicFromHostExpression(
                        "std::get<" + std::to_string(index) + ">(" + sourceName + ")",
                        tupleType->genericArguments[index],
                        depth + 1
                    );
                    if (!convertedValue.has_value())
                        return std::nullopt;
                    if (!values.empty())
                        values += ", ";
                    values += *convertedValue;
                }

                const std::string cppStructType = mangleStructTypeName(tupleType);
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    cppStructType,
                    values
                );
            }

            if (auto resultType = getStdValueStructType(resolvedType, "Result");
                resultType && resultType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = resultType->genericArguments.front();
                const std::string variableName = "_wio_result_" + std::to_string(depth);
                auto convertedValue = makeSdkDynamicFromHostExpression(
                    variableName + ".value()",
                    valueType,
                    depth + 1
                );
                if (!convertedValue.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(resultType);
                const std::string resultErrorType = Mangler::mangleStruct("ResultError", "std");
                const std::string resultDomainType = Mangler::mangleStruct("ResultDomain", "std");
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "if ({}.is_error()) {{ const auto& _wio_host_error = {}.error_value(); "
                    "{} _wio_error{{}}; "
                    "_wio_error.domain = static_cast<{}>(static_cast<std::int32_t>(_wio_host_error.domain)); "
                    "_wio_error.code = _wio_host_error.code; _wio_error.nativeCode = _wio_host_error.native_code; "
                    "_wio_error.message = _wio_host_error.message; "
                    "return wio::runtime::Ref<{}>::Create(_wio_error); }} "
                    "return wio::runtime::Ref<{}>::Create({}); }}())",
                    cppStructType,
                    variableName,
                    expression,
                    variableName,
                    variableName,
                    resultErrorType,
                    resultDomainType,
                    cppStructType,
                    cppStructType,
                    *convertedValue
                );
            }

            auto queueType = getStdValueStructType(resolvedType, "Queue");
            auto unorderedSetType = getStdValueStructType(resolvedType, "UnorderedSet");
            auto orderedSetType = getStdValueStructType(resolvedType, "OrderedSet");
            auto sequenceType = queueType ? queueType : (unorderedSetType ? unorderedSetType : orderedSetType);
            if (sequenceType && sequenceType->genericArguments.size() == 1)
            {
                const Ref<sema::Type>& valueType = sequenceType->genericArguments.front();
                const std::string sourceName = "_wio_host_sequence_" + std::to_string(depth);
                const std::string itemName = "_wio_host_sequence_item_" + std::to_string(depth);
                auto convertedItem = makeSdkDynamicFromHostExpression(itemName, valueType, depth + 1);
                if (!convertedItem.has_value())
                    return std::nullopt;

                const std::string cppStructType = mangleStructTypeName(sequenceType);
                const std::string valuesExpression = queueType
                    ? sourceName + ".to_array()"
                    : sourceName + ".values()";
                const std::string appendExpression = queueType
                    ? "_wio_output->_WF_Enqueue_T(" + *convertedItem + ");"
                    : "(void)_wio_output->_WF_Add_T(" + *convertedItem + ");";
                return common::formatString(
                    "([&]() -> wio::runtime::Ref<{}> {{ const auto& {} = {}; "
                    "auto _wio_output = wio::runtime::Ref<{}>::Create(); "
                    "for (const auto& {} : {}) {{ {} }} return _wio_output; }}())",
                    cppStructType,
                    sourceName,
                    expression,
                    cppStructType,
                    itemName,
                    valuesExpression,
                    appendExpression
                );
            }

            return std::nullopt;
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

        std::string mangleNamedType(const Ref<sema::Symbol>& symbol)
        {
            return mangleNamedType(getStructTypeFromSymbol(symbol));
        }
    }
    
    CppGenerator::CppGenerator() = default;

    bool CppGenerator::emitAnyBoxingIfNeeded(const NodePtr<Expression>& expression, const Ref<sema::Type>& expectedType)
    {
        if (!expression || !expectedType)
            return false;

        Ref<sema::Type> resolvedExpectedType = unwrapAliasTypeForCodegen(expectedType);
        if (resolvedExpectedType && resolvedExpectedType->kind() == sema::TypeKind::Reference)
            resolvedExpectedType = unwrapAliasTypeForCodegen(resolvedExpectedType.AsFast<sema::ReferenceType>()->referredType);

        if (!isAnyTypeForCodegen(resolvedExpectedType))
            return false;

        Ref<sema::Type> resolvedActualType = unwrapAliasTypeForCodegen(expression->refType.Lock());
        if (resolvedActualType && resolvedActualType->kind() == sema::TypeKind::Reference)
            resolvedActualType = unwrapAliasTypeForCodegen(resolvedActualType.AsFast<sema::ReferenceType>()->referredType);

        if (!resolvedActualType)
            return false;

        if (isAnyTypeForCodegen(resolvedActualType))
            return false;

        if (resolvedActualType->kind() == sema::TypeKind::Null)
        {
            emit("wio::runtime::Any()");
            return true;
        }

        if (isOpaqueTypeForCodegen(resolvedActualType))
        {
            emit("wio::runtime::Any::FromOpaque(");
            expression->accept(*this);
            emit(")");
            return true;
        }

        if (resolvedActualType->kind() == sema::TypeKind::Primitive)
        {
            auto primitiveType = resolvedActualType.AsFast<sema::PrimitiveType>();
            if (primitiveType && primitiveType->name == "string")
            {
                emit("wio::runtime::Any(");
                expression->accept(*this);
                emit(")");
                return true;
            }
        }

        if (resolvedActualType->kind() == sema::TypeKind::Struct)
        {
            auto structType = resolvedActualType.AsFast<sema::StructType>();
            if (!structType)
                return false;

            if (structType->isInterface)
            {
                emit("wio::runtime::Any::FromInterface<" + mangleInterfaceTypeName(structType) + ">(");
                expression->accept(*this);
                emit(")");
                return true;
            }

            if (structType->isObject)
            {
                emit("wio::runtime::Any::FromObject<" + mangleStructTypeName(structType) + ">(");
                expression->accept(*this);
                emit(")");
                return true;
            }
        }

        if (expression->is<ArrayLiteral>() || expression->is<DictionaryLiteral>())
        {
            emit("wio::runtime::Any::Box(");
            emit(toCppType(resolvedActualType));
            expression->accept(*this);
            emit(")");
            return true;
        }

        emit("wio::runtime::Any::Box(");
        expression->accept(*this);
        emit(")");
        return true;
    }

    void CppGenerator::emitReadableExpression(const NodePtr<Expression>& expression)
    {
        const std::size_t derefCount = getAutoReadableReferenceDepth(expression ? expression->refType.Lock() : nullptr);
        for (std::size_t i = 0; i < derefCount; ++i)
            emit("*(");

        if (expression)
            expression->accept(*this);

        for (std::size_t i = 0; i < derefCount; ++i)
            emit(")");
    }

    void CppGenerator::emitExpressionWithExpectedType(const NodePtr<Expression>& expression,
                                                      const Ref<sema::Type>& expectedType,
                                                      const bool allowAutoRef)
    {
        auto actualType = expression ? expression->refType.Lock() : nullptr;
        auto resolvedExpected = unwrapAliasTypeForCodegen(expectedType);
        auto resolvedActual = unwrapAliasTypeForCodegen(actualType);

        if (emitAnyBoxingIfNeeded(expression, expectedType))
            return;

        if (resolvedExpected &&
            resolvedExpected->kind() == sema::TypeKind::Function &&
            expression &&
            expression->is<LambdaExpression>())
        {
            emit(toCppType(expectedType));
            emit("(");
            expression->accept(*this);
            emit(")");
            return;
        }

        if (resolvedExpected &&
            resolvedExpected->kind() != sema::TypeKind::Reference &&
            shouldAutoReadReferenceType(actualType))
        {
            emitReadableExpression(expression);
            return;
        }

        if (resolvedExpected &&
            resolvedActual &&
            resolvedExpected->kind() == sema::TypeKind::Reference)
        {
            auto expectedRef = resolvedExpected.AsFast<sema::ReferenceType>();
            auto expectedTarget = unwrapAliasTypeForCodegen(expectedRef->referredType);

            // Keep the borrow wrapper visible at the call site. In particular,
            // C++ template argument deduction does not consider the converting
            // constructors from Ref<T> to BorrowedObject{Ref,View}<T>.
            if (expectedTarget && expectedTarget->kind() == sema::TypeKind::Struct)
            {
                auto expectedStruct = expectedTarget.AsFast<sema::StructType>();
                Ref<sema::Type> actualTarget = resolvedActual;
                if (actualTarget && actualTarget->kind() == sema::TypeKind::Reference)
                {
                    actualTarget = unwrapAliasTypeForCodegen(
                        actualTarget.AsFast<sema::ReferenceType>()->referredType
                    );
                }

                const bool actualIsObject =
                    actualTarget &&
                    actualTarget->kind() == sema::TypeKind::Struct &&
                    actualTarget.AsFast<sema::StructType>()->isObject;

                const bool actualCarriesReferenceLayer =
                    resolvedActual->kind() == sema::TypeKind::Reference;

                if (expectedStruct->isObject &&
                    actualIsObject &&
                    (!expectedRef->isMutable || actualCarriesReferenceLayer))
                {
                    emit(toCppType(expectedType));
                    emit("(");
                    expression->accept(*this);
                    emit(")");
                    return;
                }
            }

            if (allowAutoRef &&
                expectedTarget &&
                (expectedTarget->isCompatibleWith(resolvedActual) ||
                 (expectedTarget->isNumeric() && resolvedActual->isNumeric())))
            {
                bool isSmartPtrOrInterface = false;
                if (resolvedActual && resolvedActual->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedActual.AsFast<sema::StructType>();
                    isSmartPtrOrInterface = structType && (structType->isObject || structType->isInterface);
                }

                if (!isSmartPtrOrInterface)
                {
                    const bool materializeImmutableComponentView =
                        !expectedRef->isMutable &&
                        expectedTarget->kind() == sema::TypeKind::Struct &&
                        !expectedTarget.AsFast<sema::StructType>()->isObject &&
                        !expectedTarget.AsFast<sema::StructType>()->isInterface;

                    if (materializeImmutableComponentView)
                    {
                        // Binding the component expression to a const reference
                        // extends a temporary through the complete call expression.
                        // Emitting a raw &expression is invalid for extension calls
                        // such as Parse(...).Value().ToString().
                        emit("&static_cast<const ");
                        emit(toCppType(expectedTarget));
                        emit("&>(");
                        expression->accept(*this);
                        emit(")");
                        return;
                    }

                    emit("&");
                }
                expression->accept(*this);
                return;
            }

            if (!resolvedExpected->isCompatibleWith(resolvedActual))
            {
                if (expectedTarget && expectedTarget->kind() == sema::TypeKind::Struct)
                {
                    auto expectedStruct = expectedTarget.AsFast<sema::StructType>();
                    if (expectedStruct->isInterface)
                    {
                        bool canCastObjectHandle = resolvedActual && resolvedActual->kind() == sema::TypeKind::Struct;
                        if (!canCastObjectHandle && resolvedActual && resolvedActual->kind() == sema::TypeKind::Reference)
                            canCastObjectHandle = true;

                        if (canCastObjectHandle)
                        {
                            emit("static_cast<" + toCppType(expectedType) + ">((");
                            expression->accept(*this);
                            emit(")->_WF_CastTo(" + mangleInterfaceTypeName(expectedStruct) + "::TYPE_ID))");
                            return;
                        }
                    }
                }
            }
        }

        if (expression)
            expression->accept(*this);
    }

    Ref<sema::FunctionType> CppGenerator::getMangledCallableFunctionType(const Ref<sema::Symbol>& callableSymbol,
                                                                         const Ref<sema::FunctionType>& resolvedFunctionType,
                                                                         const size_t argumentCount) const
    {
        if (!callableSymbol)
            return resolvedFunctionType;

        Ref<sema::Type> declarationType = callableSymbol->type;
        if ((!declarationType || declarationType->kind() != sema::TypeKind::Function) &&
            callableSymbol->kind == sema::SymbolKind::FunctionGroup &&
            !callableSymbol->overloads.empty())
        {
            declarationType = callableSymbol->overloads.front()->type;
        }

        auto declarationFunctionType = declarationType ? declarationType.AsFast<sema::FunctionType>() : nullptr;
        if (!declarationFunctionType)
            return resolvedFunctionType;

        if (!callableSymbol->genericParameterNames.empty())
        {
            if (declarationFunctionType->hasParameterPack)
                return declarationFunctionType;

            return Compiler::get().getTypeContext().getOrCreateFunctionType(
                declarationFunctionType->returnType,
                getLeadingParameterTypes(declarationFunctionType, argumentCount)
            ).AsFast<sema::FunctionType>();
        }

        return declarationFunctionType;
    }

    std::string CppGenerator::generate(const Ref<Program>& program)
    {
        buffer_.str("");
        header_.str("");
        indentationLevel_ = 0;
        variableDeclarationsBySymbol_.clear();

        collectVariableDeclarationsBySymbol(program, variableDeclarationsBySymbol_);

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
        emitHeaderLine("#include <chrono>");
        emitHeaderLine("#include \"format.h\"");
        emitHeaderLine("#include <iostream>");
        emitHeaderLine("#include <functional>");
        emitHeaderLine("#include <map>");
        emitHeaderLine("#include <memory>");
        emitHeaderLine("#include <new>");
        emitHeaderLine("#include <stdexcept>");
        emitHeaderLine("#include <thread>");
        emitHeaderLine("#include <unordered_map>");
        emitHeaderLine();
        emitHeaderLine("#include <exception.h>");
        emitHeaderLine("#include <constant_value.h>");
        emitHeaderLine("#include <any.h>");
        emitHeaderLine("#include <enum_reflection.h>");
        emitHeaderLine("#include <type_reflection.h>");
        emitHeaderLine("#include <fit.h>");
        emitHeaderLine("#include <intrinsics.h>");
        emitHeaderLine("#include <meta.h>");
        emitHeaderLine("#include <module_api.h>");
        emitHeaderLine("#include <wio_values.h>");
        emitHeaderLine("#include <ref.h>");
        emitHeaderLine("#include <std_async.h>");
        emitHeaderLine("#include <text.h>");
        emitHeaderLine();
        emitHeaderLine("namespace wio::runtime");
        emitHeaderLine("{");
        emitHeaderLine("    std::vector<std::string> CollectEntryArguments(int argc, char* const argv[]);");
        emitHeaderLine("}");
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
        const FunctionDeclaration* applicationEntry = findApplicationEntry(program->statements);
        std::vector<ExportedFunctionInfo> exportedFunctions;
        collectExportedFunctions(program->statements, exportedFunctions);
        std::unordered_map<const sema::StructType*, const ObjectDeclaration*> objectDeclarations;
        std::unordered_map<const sema::StructType*, const ComponentDeclaration*> componentDeclarations;
        indexStructDeclarations(program->statements, objectDeclarations, componentDeclarations);
        std::vector<ExportedTypeInfo> exportedTypes;
        collectExportedTypes(program->statements, exportedFunctions, exportedTypes, objectDeclarations, componentDeclarations);

        if (!lifecycleFunctions.hasAny() && exportedFunctions.empty() && applicationEntry == nullptr)
            return;

        std::uint32_t capabilities = 0;
        if (lifecycleFunctions.apiVersion) capabilities |= (1u << 0);
        if (lifecycleFunctions.load) capabilities |= (1u << 1);
        if (lifecycleFunctions.update) capabilities |= (1u << 2);
        if (lifecycleFunctions.unload) capabilities |= (1u << 3);
        if (lifecycleFunctions.saveState) capabilities |= (1u << 4);
        if (lifecycleFunctions.restoreState) capabilities |= (1u << 5);
        capabilities |= (1u << 6); // product version
        capabilities |= (1u << 7); // type metadata v2
        capabilities |= (1u << 8); // Unicode text dynamic fields
        auto hasRetainedAttributes = [](const std::vector<NodePtr<AttributeStatement>>* attributes)
        {
            return attributes && std::ranges::any_of(*attributes, [](const auto& attribute)
            {
                return attribute && attribute->runtimeRetained;
            });
        };
        bool hasRetainedAttributeMetadata = false;
        for (const auto& exportedFunction : exportedFunctions)
            hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                (exportedFunction.declaration && hasRetainedAttributes(&exportedFunction.declaration->attributes));
        for (const auto& exportedType : exportedTypes)
        {
            hasRetainedAttributeMetadata = hasRetainedAttributeMetadata || hasRetainedAttributes(exportedType.attributes);
            for (const auto& field : exportedType.fields)
                hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                    (field.declaration && hasRetainedAttributes(&field.declaration->attributes));
            for (const auto& method : exportedType.methods)
                hasRetainedAttributeMetadata = hasRetainedAttributeMetadata ||
                    (method.declaration && hasRetainedAttributes(&method.declaration->attributes));
        }
        if (hasRetainedAttributeMetadata) capabilities |= (1u << 9); // retained attribute metadata v1
        if (applicationEntry) capabilities |= (1u << 10); // host-driven application ABI v1
        if (applicationEntry && !applicationEntry->applicationStages.empty())
            capabilities |= (1u << 12); // application schedule inspection v1
        auto asyncResultType = [](const ExportedFunctionInfo& exportInfo) -> Ref<sema::Type>
        {
            if (!exportInfo.declaration || !exportInfo.declaration->isAsync || !exportInfo.functionType)
                return nullptr;
            Ref<sema::Type> taskType = unwrapAliasType(exportInfo.functionType->returnType);
            return taskType && taskType->kind() == sema::TypeKind::AsyncTask
                ? taskType.AsFast<sema::AsyncTaskType>()->valueType
                : nullptr;
        };
        std::vector<size_t> asyncExportIndices;
        for (size_t exportIndex = 0; exportIndex < exportedFunctions.size(); ++exportIndex)
        {
            const auto& exportInfo = exportedFunctions[exportIndex];
            Ref<sema::Type> resultType = asyncResultType(exportInfo);
            if (!resultType || getAbiTypeEnumName(resultType) == "WIO_ABI_UNKNOWN")
                continue;
            const bool parametersAreStable = std::ranges::all_of(
                exportInfo.functionType->paramTypes,
                [&](const Ref<sema::Type>& parameterType)
                {
                    return getAbiTypeEnumName(parameterType) != "WIO_ABI_UNKNOWN";
                });
            if (parametersAreStable)
                asyncExportIndices.push_back(exportIndex);
        }
        if (!asyncExportIndices.empty()) capabilities |= (1u << 11); // async task host ABI v1
        std::uint32_t stateSchemaVersion = (lifecycleFunctions.saveState || lifecycleFunctions.restoreState) ? 1u : 0u;

        struct SdkAttributeTable
        {
            size_t count = 0;
            std::string expression = "nullptr";
        };
        auto emitSdkAttributeTable = [&](const std::string& tableName,
                                         const std::vector<NodePtr<AttributeStatement>>* attributes)
            -> SdkAttributeTable
        {
            std::vector<const AttributeStatement*> retained;
            if (attributes)
            {
                for (const auto& attribute : *attributes)
                    if (attribute && attribute->runtimeRetained)
                        retained.push_back(attribute.Get());
            }
            if (retained.empty())
                return {};

            auto phaseExpression = [](const std::string& phase)
            {
                if (phase == "validation") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_VALIDATION");
                if (phase == "pre") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_PRE");
                if (phase == "post") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_POST");
                if (phase == "finally") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_FINALLY");
                if (phase == "around") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_AROUND");
                if (phase == "derive") return std::string("WIO_MODULE_ATTRIBUTE_PHASE_DERIVE");
                return std::string("WIO_MODULE_ATTRIBUTE_PHASE_UNKNOWN");
            };
            for (size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto* attribute = retained[attributeIndex];
                if (attribute->processorBindings.empty())
                    continue;
                emitLine("static const WioModuleAttributeProcessorDescriptor " + tableName +
                         "_PROCESSORS_" + std::to_string(attributeIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t processorIndex = 0; processorIndex < attribute->processorBindings.size(); ++processorIndex)
                {
                    const auto& processor = attribute->processorBindings[processorIndex];
                    const std::string suffix = processorIndex + 1 < attribute->processorBindings.size() ? "," : "";
                    emitLine("{ \"" + common::wioStringToEscapedCppString(processor.canonicalTypeName) +
                             "\", " + phaseExpression(processor.phase) + ", \"" +
                             common::wioStringToEscapedCppString(processor.hookMode) + "\", " +
                             std::to_string(processorIndex) + "u }" + suffix);
                }
                dedent();
                emitLine("};");
            }

            emitLine("static const WioModuleAttributeDescriptor " + tableName + "[] =");
            emitLine("{");
            indent();
            for (size_t attributeIndex = 0; attributeIndex < retained.size(); ++attributeIndex)
            {
                const auto* attribute = retained[attributeIndex];
                std::string arguments;
                for (size_t argumentIndex = 0; argumentIndex < attribute->args.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0) arguments += ", ";
                    if (argumentIndex < attribute->argumentNames.size() && !attribute->argumentNames[argumentIndex].empty())
                        arguments += attribute->argumentNames[argumentIndex] + "=";
                    arguments += attribute->args[argumentIndex].value;
                }
                const std::string origin = attribute->origin == AttributeOrigin::Composed
                    ? "WIO_MODULE_ATTRIBUTE_COMPOSED"
                    : (attribute->origin == AttributeOrigin::Scoped
                        ? "WIO_MODULE_ATTRIBUTE_SCOPED"
                        : "WIO_MODULE_ATTRIBUTE_DIRECT");
                const std::string processorExpression = attribute->processorBindings.empty()
                    ? "nullptr"
                    : tableName + "_PROCESSORS_" + std::to_string(attributeIndex);
                const std::string stableName = attribute->canonicalName.empty()
                    ? attribute->qualifiedName
                    : attribute->canonicalName;
                const std::string suffix = attributeIndex + 1 < retained.size() ? "," : "";
                emitLine("{ \"" + common::wioStringToEscapedCppString(stableName) + "\", \"" +
                         common::wioStringToEscapedCppString(arguments) + "\", " + origin + ", " +
                         std::to_string(attribute->processorBindings.size()) + "u, " + processorExpression + " }" + suffix);
            }
            dedent();
            emitLine("};");
            return { retained.size(), tableName };
        };

        auto isInvokeCompatibleExport = [&](const ExportedFunctionInfo& exportInfo) -> bool
        {
            auto exportFunctionType = exportInfo.functionType;
            if (!exportFunctionType || (exportInfo.declaration && exportInfo.declaration->isAsync))
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
                {
                    if (exportInfo.valueRequiresBridgeCast)
                        emitLine("return static_cast<" + toCppType(exportFunctionType->returnType) + ">(instance->" + exportInfo.memberCppName + ");");
                    else
                        emitLine("return instance->" + exportInfo.memberCppName + ";");
                }
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
                {
                    if (exportInfo.valueRequiresBridgeCast)
                        emitLine("instance->" + exportInfo.memberCppName + " = static_cast<" + exportInfo.memberCppTypeName + ">(_wio_arg1);");
                    else
                        emitLine("instance->" + exportInfo.memberCppName + " = _wio_arg1;");
                }
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
                        emitLine("outResult->type = " + getAbiTypeEnumName(exportFunctionType->returnType) + ";");
                        if (exportInfo.valueRequiresBridgeCast)
                            emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = static_cast<" + toCppType(exportFunctionType->returnType) + ">(instance->" + exportInfo.memberCppName + ");");
                        else
                        {
                            emitLine("auto result = instance->" + exportInfo.memberCppName + ";");
                            emitLine("outResult->value." + getAbiValueFieldName(exportFunctionType->returnType) + " = result;");
                        }
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
                    {
                        if (exportInfo.valueRequiresBridgeCast)
                            emitLine("instance->" + exportInfo.memberCppName + " = static_cast<" + exportInfo.memberCppTypeName + ">(args[1].value." + getAbiValueFieldName(exportFunctionType->paramTypes[1]) + ");");
                        else
                            emitLine("instance->" + exportInfo.memberCppName + " = args[1].value." + getAbiValueFieldName(exportFunctionType->paramTypes[1]) + ";");
                    }
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

        for (const size_t exportIndex : asyncExportIndices)
        {
            const auto& exportInfo = exportedFunctions[exportIndex];
            const auto functionType = exportInfo.functionType;
            const Ref<sema::Type> resultType = asyncResultType(exportInfo);
            const std::string suffix = std::to_string(exportIndex);
            const std::string stateType = "WioAsyncExportState_" + suffix;
            const std::string resultCppType = toCppType(resultType);

            emitLine("struct " + stateType + " final");
            emitLine("{");
            indent();
            emitLine("std::atomic<std::uint64_t> references{1u};");
            emitLine(toCppType(functionType->returnType) + " task;");
            emitLine("mutable std::mutex errorMutex{};");
            emitLine("mutable std::string lastError{};");
            dedent();
            emitLine("};");
            emitLine();

            emitLine("static void WioAsyncRetain_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("if (auto* state = static_cast<" + stateType + "*>(opaque)) state->references.fetch_add(1u, std::memory_order_relaxed);");
            dedent();
            emitLine("}");
            emitLine("static void WioAsyncRelease_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state != nullptr && state->references.fetch_sub(1u, std::memory_order_acq_rel) == 1u) delete state;");
            dedent();
            emitLine("}");

            emitLine("static WioAsyncTaskStatus WioAsyncStatus_" + suffix + "(const void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const " + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || !state->task.IsReady()) return WIO_ASYNC_TASK_PENDING;");
            emitLine("if (state->task.IsCancelled()) return WIO_ASYNC_TASK_CANCELLED;");
            emitLine("if (state->task.IsFaulted()) return WIO_ASYNC_TASK_FAULTED;");
            emitLine("return WIO_ASYNC_TASK_READY;");
            dedent();
            emitLine("}");

            emitLine("static void WioAsyncCancel_" + suffix + "(void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("try { if (auto* state = static_cast<" + stateType + "*>(opaque)) state->task.Cancel(); }");
            emitLine("catch (...) { }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncWaitFor_" + suffix + "(void* opaque, std::uint64_t milliseconds) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("const auto initialStatus = WioAsyncStatus_" + suffix + "(state);");
            emitLine("if (initialStatus == WIO_ASYNC_TASK_CANCELLED) return WIO_ASYNC_CANCELLED;");
            emitLine("if (initialStatus == WIO_ASYNC_TASK_FAULTED) return WIO_ASYNC_FAULTED;");
            emitLine("if (!state->task.WaitFor(milliseconds)) return WIO_ASYNC_TIMED_OUT;");
            emitLine("const auto status = WioAsyncStatus_" + suffix + "(state);");
            emitLine("return status == WIO_ASYNC_TASK_CANCELLED ? WIO_ASYNC_CANCELLED : (status == WIO_ASYNC_TASK_FAULTED ? WIO_ASYNC_FAULTED : WIO_ASYNC_OK);");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncGetResult_" + suffix + "(void* opaque, WioValue* outResult) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || outResult == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("const auto status = WioAsyncStatus_" + suffix + "(state);");
            emitLine("if (status == WIO_ASYNC_TASK_PENDING) return WIO_ASYNC_NOT_READY;");
            emitLine("if (status == WIO_ASYNC_TASK_CANCELLED) return WIO_ASYNC_CANCELLED;");
            emitLine("if (status == WIO_ASYNC_TASK_FAULTED) return WIO_ASYNC_FAULTED;");
            emitLine("try");
            emitLine("{");
            indent();
            if (resultType->isVoid())
            {
                emitLine("state->task.Get();");
                emitLine("outResult->type = WIO_ABI_VOID;");
            }
            else
            {
                emitLine("auto result = state->task.Get();");
                emitLine("outResult->type = " + getAbiTypeEnumName(resultType) + ";");
                emitLine("outResult->value." + getAbiValueFieldName(resultType) + " = result;");
            }
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("std::lock_guard lock(state->errorMutex);");
            emitLine("state->lastError = error.what();");
            emitLine("return state->task.IsCancelled() ? WIO_ASYNC_CANCELLED : WIO_ASYNC_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static std::int32_t WioAsyncOnComplete_" + suffix + "(void* opaque, WioAsyncCompletionFn callback, void* userData, WioAsyncCompletionTarget target) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<" + stateType + "*>(opaque);");
            emitLine("if (state == nullptr || callback == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("if (target != WIO_ASYNC_COMPLETION_CURRENT_EXECUTOR && target != WIO_ASYNC_COMPLETION_MAIN_EXECUTOR) return WIO_ASYNC_BAD_ARGUMENTS;");
            emitLine("WioAsyncRetain_" + suffix + "(state);");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("state->task.SharedState()->AddCompletionCallback([state, callback, userData, target]");
            emitLine("{");
            indent();
            emitLine("auto deliver = [state, callback, userData]");
            emitLine("{");
            indent();
            emitLine("try { callback(userData, WioAsyncStatus_" + suffix + "(state)); } catch (...) { }");
            emitLine("WioAsyncRelease_" + suffix + "(state);");
            dedent();
            emitLine("};");
            emitLine("if (target == WIO_ASYNC_COMPLETION_MAIN_EXECUTOR)");
            emitLine("{");
            indent();
            emitLine("if (!wio::runtime::DefaultAsyncMainExecutor().Post(std::move(deliver))) deliver();");
            dedent();
            emitLine("}");
            emitLine("else deliver();");
            dedent();
            emitLine("});");
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { WioAsyncRelease_" + suffix + "(state); return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");

            emitLine("static const char* WioAsyncLastError_" + suffix + "(const void* opaque) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const " + stateType + "*>(opaque);");
            emitLine("if (state == nullptr) return \"async task state is null\";");
            emitLine("std::lock_guard lock(state->errorMutex);");
            emitLine("const std::string message = state->task.SharedState()->FailureMessage();");
            emitLine("if (!message.empty()) state->lastError = message;");
            emitLine("return state->lastError.c_str();");
            dedent();
            emitLine("}");

            emitLine("static const WioAsyncTaskOps WIO_ASYNC_OPS_" + suffix + " =");
            emitLine("{");
            indent();
            emitLine("&WioAsyncRetain_" + suffix + ", &WioAsyncRelease_" + suffix + ", &WioAsyncStatus_" + suffix + ",");
            emitLine("&WioAsyncCancel_" + suffix + ", &WioAsyncWaitFor_" + suffix + ", &WioAsyncGetResult_" + suffix + ",");
            emitLine("&WioAsyncOnComplete_" + suffix + ", &WioAsyncLastError_" + suffix);
            dedent();
            emitLine("};");

            emitLine("static std::int32_t WioAsyncInvoke_" + suffix + "(const WioValue* args, std::uint32_t argCount, WioAsyncTaskHandle* outTask) noexcept");
            emitLine("{");
            indent();
            emitLine("if (outTask == nullptr || argCount != " + std::to_string(functionType->paramTypes.size()) + "u) return WIO_ASYNC_BAD_ARGUMENTS;");
            if (!functionType->paramTypes.empty())
                emitLine("if (args == nullptr) return WIO_ASYNC_BAD_ARGUMENTS;");
            for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
            {
                emitLine("if (args[" + std::to_string(parameterIndex) + "].type != " +
                         getAbiTypeEnumName(functionType->paramTypes[parameterIndex]) + ") return WIO_ASYNC_TYPE_MISMATCH;");
            }
            emitLine("try");
            emitLine("{");
            indent();
            std::string callExpression = exportInfo.internalSymbol;
            if (!exportInfo.templateArguments.empty())
                callExpression += formatTemplateArgumentList(exportInfo.templateArguments);
            callExpression += "(";
            for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
            {
                if (parameterIndex > 0)
                    callExpression += ", ";
                callExpression += "args[" + std::to_string(parameterIndex) + "].value." +
                    getAbiValueFieldName(functionType->paramTypes[parameterIndex]);
            }
            callExpression += ")";
            emitLine("auto* state = new " + stateType + "{1u, " + callExpression + "};");
            emitLine("*outTask = { state, &WIO_ASYNC_OPS_" + suffix + ", " + getAbiTypeEnumName(resultType) + " };");
            emitLine("return WIO_ASYNC_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_ASYNC_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();
        }

        std::vector<SdkAttributeTable> exportAttributeTables;
        exportAttributeTables.reserve(exportedFunctions.size());
        for (size_t exportIndex = 0; exportIndex < exportedFunctions.size(); ++exportIndex)
        {
            const auto* declaration = exportedFunctions[exportIndex].declaration;
            exportAttributeTables.push_back(emitSdkAttributeTable(
                "WIO_MODULE_EXPORT_ATTRIBUTES_" + std::to_string(exportIndex),
                declaration ? &declaration->attributes : nullptr));
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
                const auto& attributeTable = exportAttributeTables[i];

                emitLine(common::formatString(
                    R"({{ "{}", "{}", {}, {}u, {}, {}, reinterpret_cast<const void*>(&{}), {}u, {} }}{})",
                    common::wioStringToEscapedCppString(exportInfo.logicalName),
                    common::wioStringToEscapedCppString(exportInfo.symbolName),
                    getAbiTypeEnumName(exportFunctionType->returnType),
                    exportFunctionType->paramTypes.size(),
                    paramTypesExpr,
                    invokeExpr,
                    exportInfo.symbolName,
                    attributeTable.count,
                    attributeTable.expression,
                    suffix
                ));
            }
            dedent();
            emitLine("};");
        }

        std::string asyncHostDescriptorExpression = "nullptr";
        if (!asyncExportIndices.empty())
        {
            emitLine("static const WioModuleAsyncExport WIO_MODULE_ASYNC_EXPORTS[] =");
            emitLine("{");
            indent();
            for (size_t asyncIndex = 0; asyncIndex < asyncExportIndices.size(); ++asyncIndex)
            {
                const size_t exportIndex = asyncExportIndices[asyncIndex];
                const auto& exportInfo = exportedFunctions[exportIndex];
                const Ref<sema::Type> resultType = asyncResultType(exportInfo);
                const std::string parameterTypes = exportInfo.functionType->paramTypes.empty()
                    ? "nullptr"
                    : "WIO_EXPORT_PARAM_TYPES_" + std::to_string(exportIndex);
                const std::string tableSuffix = asyncIndex + 1 < asyncExportIndices.size() ? "," : "";
                emitLine("{ \"" + common::wioStringToEscapedCppString(exportInfo.logicalName) + "\", " +
                         getAbiTypeEnumName(resultType) + ", " +
                         std::to_string(exportInfo.functionType->paramTypes.size()) + "u, " + parameterTypes +
                         ", &WioAsyncInvoke_" + std::to_string(exportIndex) + " }" + tableSuffix);
            }
            dedent();
            emitLine("};");
            emitLine("static void WioAsyncHostBindMain() noexcept { try { wio::runtime::BindAsyncMainExecutor(); } catch (...) { } }");
            emitLine("static std::uint64_t WioAsyncHostPumpMain() noexcept { try { return wio::runtime::DrainAsyncMainExecutor(); } catch (...) { return 0u; } }");
            emitLine("static std::uint64_t WioAsyncHostPendingMain() noexcept { return wio::runtime::AsyncMainPendingCount(); }");
            emitLine("static void WioAsyncHostRequestShutdown() noexcept { wio::runtime::ShutdownAsyncRuntime(); }");
            emitLine("static const WioAsyncHostDescriptor WIO_MODULE_ASYNC_HOST =");
            emitLine("{");
            indent();
            emitLine("0u, 0u, &WioAsyncHostBindMain, &WioAsyncHostPumpMain,");
            emitLine("&WioAsyncHostPendingMain, &WioAsyncHostRequestShutdown");
            dedent();
            emitLine("};");
            asyncHostDescriptorExpression = "&WIO_MODULE_ASYNC_HOST";
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
            struct EnumMemberInfo
            {
                std::string name;
                std::string valueExpr;
            };

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
            std::vector<size_t> genericArgumentIndices;
            std::vector<EnumMemberInfo> enumMembers;
            std::optional<size_t> constValueTypeIndex;
            std::optional<std::string> constValue;
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

        std::unordered_map<std::string, const EnumDeclaration*> enumDeclarationsByLogicalName;
        std::unordered_map<std::string, const FlagsetDeclaration*> flagsetDeclarationsByLogicalName;

        std::function<void(const std::vector<NodePtr<Statement>>&, const std::string&)> collectEnumLikeDeclarations =
            [&](const std::vector<NodePtr<Statement>>& nodes, const std::string& scopePath)
        {
            for (const auto& stmt : nodes)
            {
                if (!stmt)
                    continue;

                if (const auto* realmDecl = stmt->as<RealmDeclaration>())
                {
                    const std::string nextScopePath = scopePath.empty()
                        ? realmDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, realmDecl->name->token.value);
                    collectEnumLikeDeclarations(realmDecl->statements, nextScopePath);
                    continue;
                }

                if (const auto* enumDecl = stmt->as<EnumDeclaration>())
                {
                    const std::string logicalName = scopePath.empty()
                        ? enumDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, enumDecl->name->token.value);
                    enumDeclarationsByLogicalName.emplace(logicalName, enumDecl);
                    continue;
                }

                if (const auto* flagsetDecl = stmt->as<FlagsetDeclaration>())
                {
                    const std::string logicalName = scopePath.empty()
                        ? flagsetDecl->name->token.value
                        : common::formatString("{}::{}", scopePath, flagsetDecl->name->token.value);
                    flagsetDeclarationsByLogicalName.emplace(logicalName, flagsetDecl);
                }
            }
        };
        collectEnumLikeDeclarations(program->statements, "");

        auto getEnumUnderlyingAbiExpr = [](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                           std::string_view fallbackAbiExpr) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return std::string(fallbackAbiExpr);

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "WIO_ABI_I8";
            case TokenType::kwU8: return "WIO_ABI_U8";
            case TokenType::kwI16: return "WIO_ABI_I16";
            case TokenType::kwU16: return "WIO_ABI_U16";
            case TokenType::kwI32: return "WIO_ABI_I32";
            case TokenType::kwU32: return "WIO_ABI_U32";
            case TokenType::kwI64: return "WIO_ABI_I64";
            case TokenType::kwU64: return "WIO_ABI_U64";
            default: return std::string(fallbackAbiExpr);
            }
        };

        std::function<size_t(const Ref<sema::Type>&)> ensureTypeDescriptor = [&](const Ref<sema::Type>& type) -> size_t
        {
            const std::string displayName = type ? type->toString() : std::string("<unknown>");
            Ref<sema::Type> resolvedType = unwrapAliasType(type);
            std::string key = displayName;
            if (resolvedType && resolvedType->kind() == sema::TypeKind::ConstValue)
            {
                const auto constValueType = resolvedType.AsFast<sema::ConstValueType>();
                key = "const<" + (constValueType->valueType ? constValueType->valueType->toString() : std::string("<unknown>")) +
                    ">:" + displayName;
            }
            if (auto it = emittedTypeDescriptorIndices.find(key); it != emittedTypeDescriptorIndices.end())
                return it->second;

            TypeDescriptorEmissionInfo info;
            info.displayName = displayName;

            if (!resolvedType)
            {
                const size_t index = emittedTypeDescriptors.size();
                emittedTypeDescriptorIndices.emplace(key, index);
                emittedTypeDescriptors.push_back(std::move(info));
                return index;
            }

            switch (resolvedType->kind())
            {
            case sema::TypeKind::ConstValue:
            {
                const auto constValueType = resolvedType.AsFast<sema::ConstValueType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_CONST_VALUE";
                info.constValueTypeIndex = ensureTypeDescriptor(constValueType->valueType);
                info.constValue = constValueType->value;
                info.displayName = "const " + emittedTypeDescriptors[*info.constValueTypeIndex].displayName +
                    " = " + constValueType->toString();
                break;
            }
            case sema::TypeKind::Nullable:
            {
                auto nullableType = resolvedType.AsFast<sema::NullableType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_NULLABLE";
                info.abiExpr = getAbiTypeEnumName(nullableType->valueType);
                info.elementIndex = ensureTypeDescriptor(nullableType->valueType);
                info.displayName = emittedTypeDescriptors[*info.elementIndex].displayName + "?";
                break;
            }
            case sema::TypeKind::Primitive:
            {
                const std::string primitiveName = resolvedType.AsFast<sema::PrimitiveType>()->name;
                if (primitiveName == "string")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_STRING";
                else if (primitiveName == "text")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_TEXT";
                else if (primitiveName == "opaque")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPAQUE";
                else if (primitiveName == "any")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ANY";
                else
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_PRIMITIVE";
                info.abiExpr = getAbiTypeEnumName(resolvedType);
                break;
            }
            case sema::TypeKind::AsyncTask:
            {
                auto taskType = resolvedType.AsFast<sema::AsyncTaskType>();
                info.kindExpr = "WIO_MODULE_TYPE_DESC_ASYNC_TASK";
                info.elementIndex = ensureTypeDescriptor(taskType->valueType);
                info.displayName = "coroutine<" + emittedTypeDescriptors[*info.elementIndex].displayName + ">";
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
                const std::string elementName = emittedTypeDescriptors[*info.elementIndex].displayName;
                info.displayName = arrayType->arrayKind == sema::ArrayType::ArrayKind::Static
                    ? ("[" + elementName + "; " + std::to_string(info.staticArraySize) + "]")
                    : (elementName + "[]");
                break;
            }
            case sema::TypeKind::Dictionary:
            {
                auto dictType = resolvedType.AsFast<sema::DictionaryType>();
                info.kindExpr = dictType->isOrdered ? "WIO_MODULE_TYPE_DESC_TREE" : "WIO_MODULE_TYPE_DESC_DICT";
                info.keyIndex = ensureTypeDescriptor(dictType->keyType);
                info.valueIndex = ensureTypeDescriptor(dictType->valueType);
                info.displayName = std::string(dictType->isOrdered ? "Tree<" : "Dict<") +
                    emittedTypeDescriptors[*info.keyIndex].displayName + ", " +
                    emittedTypeDescriptors[*info.valueIndex].displayName + ">";
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
                info.displayName = "fn(";
                for (size_t parameterIndex = 0; parameterIndex < info.parameterIndices.size(); ++parameterIndex)
                {
                    if (parameterIndex > 0)
                        info.displayName += ", ";
                    info.displayName += emittedTypeDescriptors[info.parameterIndices[parameterIndex]].displayName;
                }
                info.displayName += ") -> " + emittedTypeDescriptors[*info.returnIndex].displayName;
                break;
            }
            case sema::TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<sema::StructType>();
                info.logicalTypeName = getLogicalTypeName(structType);
                const auto genericPrimaryType = structType->genericPrimaryType.Lock();
                const auto& genericParameterTypes = !structType->genericParameterTypes.empty()
                    ? structType->genericParameterTypes
                    : (genericPrimaryType ? genericPrimaryType->genericParameterTypes : structType->genericParameterTypes);
                info.genericArgumentIndices.reserve(structType->genericArguments.size());
                for (size_t genericIndex = 0; genericIndex < structType->genericArguments.size(); ++genericIndex)
                {
                    Ref<sema::Type> genericArgument = structType->genericArguments[genericIndex];
                    if (genericArgument && genericArgument->kind() == sema::TypeKind::ConstValue &&
                        genericIndex < genericParameterTypes.size())
                    {
                        auto parameterType = unwrapAliasType(genericParameterTypes[genericIndex]);
                        if (parameterType && parameterType->kind() == sema::TypeKind::ConstGenericParameter)
                        {
                            const auto value = genericArgument.AsFast<sema::ConstValueType>();
                            const auto parameter = parameterType.AsFast<sema::ConstGenericParameterType>();
                            genericArgument = Compiler::get().getTypeContext().getOrCreateConstValueType(
                                value->value,
                                parameter->valueType
                            );
                        }
                    }
                    info.genericArgumentIndices.push_back(ensureTypeDescriptor(genericArgument));
                }
                if (!info.logicalTypeName.empty())
                {
                    info.displayName = info.logicalTypeName;
                    if (!info.genericArgumentIndices.empty())
                    {
                        info.displayName += "<";
                        for (size_t argumentIndex = 0; argumentIndex < info.genericArgumentIndices.size(); ++argumentIndex)
                        {
                            if (argumentIndex > 0)
                                info.displayName += ", ";
                            info.displayName += emittedTypeDescriptors[info.genericArgumentIndices[argumentIndex]].displayName;
                        }
                        info.displayName += ">";
                    }
                }
                if (structType->isEnum)
                {
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ENUM";
                    const auto declarationIt = enumDeclarationsByLogicalName.find(info.logicalTypeName);
                    const auto* declaration = declarationIt != enumDeclarationsByLogicalName.end() ? declarationIt->second : nullptr;
                    info.abiExpr = structType->enumUnderlyingType
                        ? getAbiTypeEnumName(structType->enumUnderlyingType)
                        : (declaration ? getEnumUnderlyingAbiExpr(declaration->attributes, "WIO_ABI_I32") : "WIO_ABI_I32");

                    const std::string enumCppName = mangleStructTypeName(structType);
                    if (declaration)
                    {
                        info.enumMembers.reserve(declaration->members.size());
                        for (const auto& member : declaration->members)
                        {
                            info.enumMembers.push_back({
                                member.name->token.value,
                                "WioMakeAbiIntegerValue(" + info.abiExpr +
                                    ", static_cast<std::uint64_t>(static_cast<std::underlying_type_t<" + enumCppName + ">>(" +
                                    enumCppName + "::" + member.name->token.value + ")))"
                            });
                        }
                    }
                }
                else if (structType->isFlagset)
                {
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_FLAGSET";
                    const auto declarationIt = flagsetDeclarationsByLogicalName.find(info.logicalTypeName);
                    const auto* declaration = declarationIt != flagsetDeclarationsByLogicalName.end() ? declarationIt->second : nullptr;
                    info.abiExpr = structType->enumUnderlyingType
                        ? getAbiTypeEnumName(structType->enumUnderlyingType)
                        : (declaration ? getEnumUnderlyingAbiExpr(declaration->attributes, "WIO_ABI_U32") : "WIO_ABI_U32");

                    const std::string flagsetCppName = mangleStructTypeName(structType);
                    if (declaration)
                    {
                        info.enumMembers.reserve(declaration->members.size());
                        for (const auto& member : declaration->members)
                        {
                            info.enumMembers.push_back({
                                member.name->token.value,
                                "WioMakeAbiIntegerValue(" + info.abiExpr +
                                    ", static_cast<std::uint64_t>(static_cast<std::underlying_type_t<" + flagsetCppName + ">>(" +
                                    flagsetCppName + "::" + member.name->token.value + ")))"
                            });
                        }
                    }
                }
                else if (structType->isInterface)
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_INTERFACE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Option")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OPTION";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Result")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_RESULT";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "ResultUnit")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_UNIT";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Tuple")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_TUPLE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Queue")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_QUEUE";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "UnorderedSet")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_UNORDERED_SET";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "OrderedSet")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_ORDERED_SET";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "Span")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_SPAN";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "ByteBuffer")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_BYTE_BUFFER";
                else if (isStdLibraryScopePath(structType->scopePath) && structType->name == "box")
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_BOX";
                else if (objectDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_OBJECT";
                else if (componentDeclarations.contains(structType.Get()))
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_COMPONENT";
                else if (!structType->genericArguments.empty())
                    info.kindExpr = "WIO_MODULE_TYPE_DESC_GENERIC_INSTANCE";
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
                if (descriptor.genericArgumentIndices.empty())
                    continue;

                emitLine("static const WioModuleTypeDescriptor* WIO_TYPE_DESCRIPTOR_GENERIC_ARGS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t argumentIndex = 0; argumentIndex < descriptor.genericArgumentIndices.size(); ++argumentIndex)
                {
                    const std::string suffix = (argumentIndex + 1 < descriptor.genericArgumentIndices.size()) ? "," : "";
                    emitLine("&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptor.genericArgumentIndices[argumentIndex]) + suffix);
                }
                dedent();
                emitLine("};");
            }

            for (size_t descriptorIndex = 0; descriptorIndex < emittedTypeDescriptors.size(); ++descriptorIndex)
            {
                const auto& descriptor = emittedTypeDescriptors[descriptorIndex];
                if (descriptor.enumMembers.empty())
                    continue;

                emitLine("static const WioModuleEnumMemberDescriptor WIO_TYPE_DESCRIPTOR_ENUM_MEMBERS_" + std::to_string(descriptorIndex) + "[] =");
                emitLine("{");
                indent();
                for (size_t memberIndex = 0; memberIndex < descriptor.enumMembers.size(); ++memberIndex)
                {
                    const auto& member = descriptor.enumMembers[memberIndex];
                    const std::string suffix = (memberIndex + 1 < descriptor.enumMembers.size()) ? "," : "";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(member.name) + "\", " + member.valueExpr + " }" + suffix
                    );
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
                const std::string enumMembersExpr = descriptor.enumMembers.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_ENUM_MEMBERS_" + std::to_string(descriptorIndex));
                const std::string genericArgumentsExpr = descriptor.genericArgumentIndices.empty()
                    ? "nullptr"
                    : ("WIO_TYPE_DESCRIPTOR_GENERIC_ARGS_" + std::to_string(descriptorIndex));
                const std::string constValueTypeExpr = descriptor.constValueTypeIndex.has_value()
                    ? ("&WIO_TYPE_DESCRIPTOR_" + std::to_string(*descriptor.constValueTypeIndex))
                    : "nullptr";
                const std::string constValueExpr = !descriptor.constValue.has_value()
                    ? "nullptr"
                    : ("\"" + common::wioStringToEscapedCppString(*descriptor.constValue) + "\"");

                emitLine(
                    "const WioModuleTypeDescriptor WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                    " = { \"" + common::wioStringToEscapedCppString(descriptor.displayName) +
                    "\", " + logicalTypeExpr +
                    ", " + descriptor.kindExpr +
                    ", " + descriptor.abiExpr +
                    ", " + std::to_string(descriptor.staticArraySize) + "ull, " +
                    elementExpr + ", " + keyExpr + ", " + valueExpr + ", " + returnExpr +
                    ", " + std::to_string(descriptor.parameterIndices.size()) + "u, " + paramExpr +
                    ", " + std::to_string(descriptor.enumMembers.size()) + "u, " + enumMembersExpr +
                    ", WioStableTypeId(\"" + common::wioStringToEscapedCppString(descriptor.displayName) + "\")" +
                    ", " + std::to_string(descriptor.genericArgumentIndices.size()) + "u, " + genericArgumentsExpr +
                    ", " + constValueTypeExpr + ", " + constValueExpr + " };"
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
                    Ref<sema::Type> dynamicFieldType = unwrapAliasType(field.fieldType);
                    const bool isTextField = dynamicFieldType && dynamicFieldType->kind() == sema::TypeKind::Primitive &&
                        dynamicFieldType.AsFast<sema::PrimitiveType>()->name == "text";
                    const bool isOptionField = getStdValueStructType(dynamicFieldType, "Option") != nullptr;
                    const bool isResultField = getStdValueStructType(dynamicFieldType, "Result") != nullptr;
                    const bool isUnitField = getStdValueStructType(dynamicFieldType, "ResultUnit") != nullptr;
                    const bool isSpanField = getStdValueStructType(dynamicFieldType, "Span") != nullptr;
                    const bool isByteBufferField = getStdValueStructType(dynamicFieldType, "ByteBuffer") != nullptr;
                    const bool isTupleField = getStdValueStructType(dynamicFieldType, "Tuple") != nullptr;
                    const bool isSequenceContainerField =
                        getStdValueStructType(dynamicFieldType, "Queue") != nullptr ||
                        getStdValueStructType(dynamicFieldType, "UnorderedSet") != nullptr ||
                        getStdValueStructType(dynamicFieldType, "OrderedSet") != nullptr;
                    const bool isPortableBridgeField = isOptionField || isResultField || isUnitField || isSpanField ||
                        isByteBufferField || isTupleField || isSequenceContainerField ||
                        (dynamicFieldType && (dynamicFieldType->kind() == sema::TypeKind::Array ||
                                              dynamicFieldType->kind() == sema::TypeKind::Dictionary));
                    const auto portablePayloadType = isPortableBridgeField
                        ? getSdkDynamicBridgeCppType(dynamicFieldType)
                        : std::optional<std::string>{};
                    const auto portableGetterExpression = isPortableBridgeField
                        ? makeSdkDynamicToHostExpression("instance->" + field.memberCppName, dynamicFieldType)
                        : std::optional<std::string>{};
                    emitLine(
                        "static WioErasedValue* " + *field.dynamicGetterSymbol + "(std::uintptr_t handle)"
                    );
                    emitLine("{");
                    indent();
                    emitLine("auto* instance = reinterpret_cast<" + exportedType.cppTypeName + "*>(handle);");
                    emitLine("if (instance == nullptr) return nullptr;");
                    if (isTextField)
                    {
                        emitLine(
                            std::string("return new WioErasedValueModel<std::string>(") +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", instance->" + field.memberCppName + ".Utf8());"
                        );
                    }
                    else if (isPortableBridgeField && portablePayloadType.has_value() && portableGetterExpression.has_value())
                    {
                        emitLine(
                            "return new WioErasedValueModel<" + *portablePayloadType + ">(" +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", " + *portableGetterExpression + ");"
                        );
                    }
                    else
                    {
                        emitLine(
                            "return new WioErasedValueModel<" + field.memberCppTypeName + ">(" +
                            "&WIO_TYPE_DESCRIPTOR_" + std::to_string(descriptorIndex) +
                            ", instance->" + field.memberCppName + ");"
                        );
                    }
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
                        if (isTextField)
                            emitLine("auto* typedValue = dynamic_cast<const WioErasedValueModel<std::string>*>(value);");
                        else if (isPortableBridgeField && portablePayloadType.has_value())
                            emitLine("auto* typedValue = dynamic_cast<const WioErasedValueModel<" + *portablePayloadType + ">*>(value);");
                        else
                            emitLine(
                                "auto* typedValue = dynamic_cast<const WioErasedValueModel<" + field.memberCppTypeName + ">*>(value);"
                            );
                        emitLine("if (typedValue == nullptr) return WIO_INVOKE_TYPE_MISMATCH;");
                        if (isTextField)
                            emitLine("instance->" + field.memberCppName + " = wio::runtime::Text::FromUtf8(typedValue->value);");
                        else if (isPortableBridgeField)
                        {
                            const auto setterExpression = makeSdkDynamicFromHostExpression("typedValue->value", dynamicFieldType);
                            if (setterExpression.has_value())
                                emitLine("instance->" + field.memberCppName + " = " + *setterExpression + ";");
                            else
                                emitLine("return WIO_INVOKE_TYPE_MISMATCH;");
                        }
                        else
                            emitLine("instance->" + field.memberCppName + " = typedValue->value;");
                        emitLine("return WIO_INVOKE_OK;");
                        dedent();
                        emitLine("}");
                        emitLine();
                    }
                }
            }

            std::vector<SdkAttributeTable> typeAttributeTables;
            typeAttributeTables.reserve(exportedTypes.size());
            for (size_t typeIndex = 0; typeIndex < exportedTypes.size(); ++typeIndex)
            {
            const auto& exportedType = exportedTypes[typeIndex];
            const auto typeAttributeTable = emitSdkAttributeTable(
                "WIO_MODULE_TYPE_ATTRIBUTES_" + std::to_string(typeIndex),
                exportedType.attributes);
            typeAttributeTables.push_back(typeAttributeTable);
            std::vector<SdkAttributeTable> fieldAttributeTables;
            fieldAttributeTables.reserve(exportedType.fields.size());
            for (size_t fieldIndex = 0; fieldIndex < exportedType.fields.size(); ++fieldIndex)
            {
                const auto* declaration = exportedType.fields[fieldIndex].declaration;
                fieldAttributeTables.push_back(emitSdkAttributeTable(
                    "WIO_MODULE_TYPE_FIELD_ATTRIBUTES_" + std::to_string(typeIndex) + "_" + std::to_string(fieldIndex),
                    declaration ? &declaration->attributes : nullptr));
            }
            std::vector<SdkAttributeTable> methodAttributeTables;
            methodAttributeTables.reserve(exportedType.methods.size());
            for (size_t methodIndex = 0; methodIndex < exportedType.methods.size(); ++methodIndex)
            {
                const auto* declaration = exportedType.methods[methodIndex].declaration;
                methodAttributeTables.push_back(emitSdkAttributeTable(
                    "WIO_MODULE_TYPE_METHOD_ATTRIBUTES_" + std::to_string(typeIndex) + "_" + std::to_string(methodIndex),
                    declaration ? &declaration->attributes : nullptr));
            }

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
                    const auto& attributeTable = fieldAttributeTables[fieldIndex];

                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(field.fieldName) +
                        "\", " + getAbiTypeEnumName(field.fieldType) +
                        ", " + typeDescriptorExpr +
                        ", " + std::to_string(flags) + "u, " + accessModifierExpr +
                        ", &WIO_MODULE_EXPORTS[" + std::to_string(field.getterExportIndex) +
                        "], " + setterExportExpr + ", " + dynamicGetterExpr + ", " + dynamicSetterExpr +
                        ", " + std::to_string(attributeTable.count) + "u, " + attributeTable.expression + " }" + suffix
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
                    const auto& attributeTable = methodAttributeTables[methodIndex];
                    const std::string suffix = (methodIndex + 1 < exportedType.methods.size()) ? "," : "";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(method.methodName) +
                        "\", &WIO_MODULE_EXPORTS[" + std::to_string(method.exportIndex) + "], " +
                        std::to_string(attributeTable.count) + "u, " + attributeTable.expression + " }" + suffix
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
                const auto& typeAttributeTable = typeAttributeTables[typeIndex];

                emitLine(
                    "{ \"" + common::wioStringToEscapedCppString(exportedType.logicalName) +
                    "\", \"" + common::wioStringToEscapedCppString(exportedType.symbolName) +
                    "\", " + typeKind +
                    ", " + createExportExpr +
                    ", &WIO_MODULE_EXPORTS[" + std::to_string(exportedType.destroyExportIndex) +
                    "], " + std::to_string(exportedType.constructors.size()) + "u, " + constructorArrayExpr +
                    ", " + std::to_string(exportedType.fields.size()) + "u, " + fieldArrayExpr +
                    ", " + std::to_string(exportedType.methods.size()) + "u, " + methodArrayExpr +
                    ", " + std::to_string(typeAttributeTable.count) + "u, " + typeAttributeTable.expression + " }" + suffix
                );
            }
            dedent();
            emitLine("};");
        }

        std::string applicationDescriptorExpression = "nullptr";
        if (applicationEntry)
        {
            const std::string applicationName = applicationEntry->applicationName;
            const std::string cppType = Mangler::mangleStruct(applicationName);
            const std::string lifecyclePrefix = "_WF___extension_" + applicationName + "Lifecycle_";
            const std::string receiverSuffix = "_ref_mut_" + applicationName;
            const std::string startFunction = lifecyclePrefix + "Start" + receiverSuffix;
            const std::string updateFunction = lifecyclePrefix + "Update" + receiverSuffix + "_f64";
            const std::string closeFunction = lifecyclePrefix + "Close" + receiverSuffix;
            const std::string exitFunction = lifecyclePrefix + "Exit" + receiverSuffix + "_i32";

            std::string applicationStagesExpression = "nullptr";
            if (!applicationEntry->applicationStages.empty())
            {
                emitLine("static const WioApplicationStageDescriptor WIO_APPLICATION_STAGES[] =");
                emitLine("{");
                indent();
                for (size_t stageIndex = 0; stageIndex < applicationEntry->applicationStages.size(); ++stageIndex)
                {
                    const auto& stage = applicationEntry->applicationStages[stageIndex];
                    std::vector<std::string> flagExpressions;
                    if (stage.fixed) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_FIXED");
                    if (stage.mainThread) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_MAIN_THREAD");
                    if (stage.containsSystem) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_CONTAINS_SYSTEM");
                    if (stage.containsApplication) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_CONTAINS_APPLICATION");
                    if (stage.legacyExplicit) flagExpressions.emplace_back("WIO_APPLICATION_STAGE_LEGACY_EXPLICIT");
                    std::string flags = "0u";
                    if (!flagExpressions.empty())
                    {
                        flags = flagExpressions.front();
                        for (size_t flagIndex = 1; flagIndex < flagExpressions.size(); ++flagIndex)
                            flags += " | " + flagExpressions[flagIndex];
                    }
                    const std::string suffix = stageIndex + 1u == applicationEntry->applicationStages.size() ? "" : ",";
                    emitLine(
                        "{ \"" + common::wioStringToEscapedCppString(stage.name) + "\", \"" +
                        common::wioStringToEscapedCppString(stage.after) + "\", " +
                        std::to_string(stage.fixedHz) + ", " + std::to_string(stage.order) + "u, " + flags + " }" + suffix);
                }
                dedent();
                emitLine("};");
                emitLine();
                applicationStagesExpression = "WIO_APPLICATION_STAGES";
            }

            emitLine("struct WioGeneratedApplicationState final");
            emitLine("{");
            indent();
            emitLine(cppType + " application{};");
            emitLine("std::string lastError{};");
            emitLine("std::thread::id owner{};");
            emitLine("bool started = false;");
            emitLine("bool closed = false;");
            emitLine("bool faulted = false;");
            dedent();
            emitLine("};");
            emitLine();

            emitLine("static bool WioApplicationOnOwnerThread(const WioGeneratedApplicationState* state) noexcept");
            emitLine("{");
            indent();
            emitLine("return state != nullptr && state->owner == std::this_thread::get_id();");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationConstruct(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("if (storage == nullptr) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("auto* state = ::new (storage) WioGeneratedApplicationState{};");
            emitLine("state->owner = std::this_thread::get_id();");
            emitLine("return WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static void WioApplicationRollbackStart(WioGeneratedApplicationState* state) noexcept");
            emitLine("{");
            indent();
            emitLine("if (state == nullptr) return;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine(closeFunction + "(&state->application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& rollbackError)");
            emitLine("{");
            indent();
            emitLine("state->lastError += \"; start rollback failed: \";");
            emitLine("state->lastError += rollbackError.what();");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->lastError += \"; start rollback failed with a non-standard exception\"; }");
            emitLine("state->closed = true;");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationStart(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || state->closed || state->started) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::BindAsyncMainExecutor();");
            emitLine(startFunction + "(&state->application);");
            emitLine("state->started = true;");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return state->application.__exitRequested ? WIO_APPLICATION_EXIT_REQUESTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("WioApplicationRollbackStart(state);");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("state->lastError = \"unhandled non-standard exception\";");
            emitLine("state->faulted = true;");
            emitLine("WioApplicationRollbackStart(state);");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationUpdate(void* storage, double deltaSeconds) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || !state->started || state->closed) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("if (state->faulted) return WIO_APPLICATION_FAULTED;");
            emitLine("if (state->application.__exitRequested) return WIO_APPLICATION_EXIT_REQUESTED;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(updateFunction + "(&state->application, deltaSeconds);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return state->application.__exitRequested ? WIO_APPLICATION_EXIT_REQUESTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("state->faulted = true;");
            emitLine("state->lastError = \"unhandled non-standard exception\";");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationRequestExit(void* storage, std::int32_t exitCode) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || state->closed) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine(exitFunction + "(&state->application, exitCode);");
            emitLine("return WIO_APPLICATION_EXIT_REQUESTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->faulted = true; return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static std::int32_t WioApplicationClose(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr || !state->started) return WIO_APPLICATION_INVALID_STATE;");
            emitLine("if (state->closed) return WIO_APPLICATION_ALREADY_CLOSED;");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return WIO_APPLICATION_WRONG_THREAD;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(closeFunction + "(&state->application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("state->closed = true;");
            emitLine("return state->faulted ? WIO_APPLICATION_FAULTED : WIO_APPLICATION_OK;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("state->closed = true;");
            emitLine("state->faulted = true;");
            emitLine("state->lastError = error.what();");
            emitLine("return WIO_APPLICATION_FAULTED;");
            dedent();
            emitLine("}");
            emitLine("catch (...) { state->closed = true; state->faulted = true; return WIO_APPLICATION_FAULTED; }");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static void WioApplicationDestroy(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (state == nullptr) return;");
            emitLine("if (state->started && !state->closed && WioApplicationOnOwnerThread(state)) (void)WioApplicationClose(state);");
            emitLine("state->~WioGeneratedApplicationState();");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static bool WioApplicationExitRequested(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr && state->application.__exitRequested;");
            dedent();
            emitLine("}");
            emitLine("static std::int32_t WioApplicationExitCode(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr ? state->application.__exitCode : 1;");
            dedent();
            emitLine("}");
            emitLine("static std::uint64_t WioApplicationPumpMain(void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("auto* state = static_cast<WioGeneratedApplicationState*>(storage);");
            emitLine("if (!WioApplicationOnOwnerThread(state)) return 0u;");
            emitLine("try { return wio::runtime::DrainAsyncMainExecutor(); }");
            emitLine("catch (...) { state->faulted = true; return 0u; }");
            dedent();
            emitLine("}");
            emitLine("static const char* WioApplicationLastError(const void* storage) noexcept");
            emitLine("{");
            indent();
            emitLine("const auto* state = static_cast<const WioGeneratedApplicationState*>(storage);");
            emitLine("return state != nullptr ? state->lastError.c_str() : \"application state is null\";");
            dedent();
            emitLine("}");
            emitLine();

            emitLine("static const WioApplicationDescriptor WIO_MODULE_APPLICATION =");
            emitLine("{");
            indent();
            emitLine("\"" + common::wioStringToEscapedCppString(applicationName) + "\",");
            emitLine("sizeof(WioGeneratedApplicationState),");
            emitLine("alignof(WioGeneratedApplicationState),");
            emitLine("WIO_APPLICATION_MAIN_THREAD_AFFINE | WIO_APPLICATION_HOST_OWNS_STORAGE | WIO_APPLICATION_NON_BLOCKING_UPDATE,");
            emitLine("0u,");
            emitLine("&WioApplicationConstruct,");
            emitLine("&WioApplicationStart,");
            emitLine("&WioApplicationUpdate,");
            emitLine("&WioApplicationRequestExit,");
            emitLine("&WioApplicationClose,");
            emitLine("&WioApplicationDestroy,");
            emitLine("&WioApplicationExitRequested,");
            emitLine("&WioApplicationExitCode,");
            emitLine("&WioApplicationPumpMain,");
            emitLine("&WioApplicationLastError,");
            emitLine(std::to_string(applicationEntry->applicationStages.size()) + "u,");
            emitLine("0u,");
            emitLine(applicationStagesExpression);
            dedent();
            emitLine("};");
            applicationDescriptorExpression = "&WIO_MODULE_APPLICATION";
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
        emitLine(exportedTypes.empty() ? "nullptr," : "WIO_MODULE_TYPES,");
        emitLine("{ WIO_SDK_VERSION_MAJOR, WIO_SDK_VERSION_MINOR, WIO_SDK_VERSION_PATCH },");
        emitLine("sizeof(WioModuleApi),");
        emitLine(applicationDescriptorExpression + ",");
        emitLine(std::to_string(asyncExportIndices.size()) + "u,");
        emitLine(asyncExportIndices.empty() ? "nullptr," : "WIO_MODULE_ASYNC_EXPORTS,");
        emitLine(asyncHostDescriptorExpression);
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
        Ref<sema::Type> lockedRefType = nullptr;
        if (auto funcSym = node.name ? node.name->referencedSymbol.Lock() : nullptr)
        {
            if (auto funcType = funcSym->type.AsFast<sema::FunctionType>())
                lockedRefType = funcType->returnType;
        }

        if (!lockedRefType && node.returnType)
            lockedRefType = node.returnType->refType.Lock();

        if (!lockedRefType)
            lockedRefType = Compiler::get().getTypeContext().getVoid();

        if (node.isAsync)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(lockedRefType);
            if (resolvedTaskType && resolvedTaskType->kind() == sema::TypeKind::AsyncTask)
                lockedRefType = resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType;
        }
        
        if (lockedRefType->toString() != "i32" && lockedRefType->toString() != "void")
        {
            throw InvalidEntryReturnType("Entry return type must be i32 or void.", node.location());
        }
        if(!node.body)
        {
            throw MissedEntryBody("The Entry function must have a body.", node.location());
        }

        if (node.isApplicationEntry)
        {
            const std::string cppType = Mangler::mangleStruct(node.applicationName);
            const std::string lifecyclePrefix = "_WF___extension_" + node.applicationName + "Lifecycle_";
            const std::string receiverSuffix = "_ref_mut_" + node.applicationName;
            const std::string startFunction = lifecyclePrefix + "Start" + receiverSuffix;
            const std::string updateFunction = lifecyclePrefix + "Update" + receiverSuffix + "_f64";
            const std::string closeFunction = lifecyclePrefix + "Close" + receiverSuffix;

            emitGeneratedDirective();
            emitLine();
            emitLine("int main()");
            emitLine("{");
            indent();
            emitLine(cppType + " application{};");
            emitLine("bool applicationStarted = false;");
            emitLine("bool applicationClosed = false;");
            emitLine("auto closeApplication = [&]() noexcept");
            emitLine("{");
            indent();
            emitLine("if (!applicationStarted || applicationClosed) return;");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(closeFunction + "(&application);");
            emitLine("applicationClosed = true;");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("catch (...) { applicationClosed = true; }");
            dedent();
            emitLine("};");
            emitLine("try");
            emitLine("{");
            indent();
            emitLine("wio::runtime::BindAsyncMainExecutor();");
            emitLine("applicationStarted = true;");
            emitLine(startFunction + "(&application);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("auto previousFrame = std::chrono::steady_clock::now();");
            emitLine("while (!application.__exitRequested)");
            emitLine("{");
            indent();
            emitLine("const auto currentFrame = std::chrono::steady_clock::now();");
            emitLine("const double elapsed = std::chrono::duration<double>(currentFrame - previousFrame).count();");
            emitLine("previousFrame = currentFrame;");
            emitLine("const double deltaSeconds = std::clamp(elapsed, 0.0, 0.25);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine(updateFunction + "(&application, deltaSeconds);");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            dedent();
            emitLine("}");
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("wio::runtime::DrainAsyncMainExecutor();");
            emitLine("return application.__exitCode;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& error)");
            emitLine("{");
            indent();
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("std::cout << \"Runtime Error: \" << error.what() << '\\n';");
            emitLine("return 1;");
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine("closeApplication();");
            emitLine("wio::runtime::ShutdownAsyncRuntime();");
            emitLine("std::cout << \"Runtime Error: Unknown native exception\" << '\\n';");
            emitLine("return 1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            return;
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
            emitLine("auto _wio_entry_arguments = wio::runtime::CollectEntryArguments(argc, argv);");
            emitLine(paramName + ".reserve(_wio_entry_arguments.size());");
            emitLine("for (const auto& _wio_entry_argument : _wio_entry_arguments) {");
            indent();
            emitLine(paramName + ".push_back(wio::String(_wio_entry_argument));");
            dedent();
            emitLine("}");
            emitLine("");
        }

        emitLine("try {");
        indent();
        
        if (node.isAsync)
        {
            auto funcSym = node.name ? node.name->referencedSymbol.Lock() : nullptr;
            auto funcType = funcSym ? funcSym->type.AsFast<sema::FunctionType>() : nullptr;
            const std::string entrySymbol = Mangler::mangleFunction(
                node.name->token.value,
                funcType ? funcType->paramTypes : std::vector<Ref<sema::Type>>{},
                funcSym ? funcSym->scopePath : ""
            );
            EMIT_TABS();
            if (!lockedRefType->isVoid())
                emit("return ");
            emit("wio::runtime::BlockOn(" + entrySymbol + "(");
            if (hasArgs)
                emit(paramName);
            emitLine("));");
            if (lockedRefType->isVoid())
                emitLine("return 0;");
        }
        else if (node.body->is<BlockStatement>())
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
        emitLine("catch (const std::exception& ex)");
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: Unhandled native exception: " << ex.what() << '\n';)");
        emitLine("return 1;");
        dedent();
        emitLine("}");
        emitLine("catch (...)");
        emitLine("{");
        indent();
        emitLine(R"(std::cout << "Runtime Error: Unknown native exception" << '\n';)");
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

                if (stmt->is<DeclarationGroup>())
                {
                    self(self, stmt->as<DeclarationGroup>()->declarations, emitter);
                    continue;
                }

                if (stmt->is<UsingAttributeStatement>())
                {
                    auto usingAttribute = stmt->as<UsingAttributeStatement>();
                    if (usingAttribute->body)
                        self(self, usingAttribute->body->declarations, emitter);
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
                const bool isGenericParameterPack =
                    genericParameters.size() > 0 &&
                    i + 1 == genericParameters.size() &&
                    genericParameters[i] &&
                    genericParameters[i]->refType.Lock() &&
                    genericParameters[i]->refType.Lock()->kind() == sema::TypeKind::GenericParameterPack;
                templateLine += formatCppTemplateParameter(genericParameters[i], isGenericParameterPack);
                if (i + 1 < genericParameters.size())
                    templateLine += ", ";
            }
            templateLine += ">";
            emitLine(templateLine);
        };

        auto getEnumUnderlyingCppType = [&](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                            const std::string& fallbackType) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return fallbackType;

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "int8_t";
            case TokenType::kwU8: return "uint8_t";
            case TokenType::kwI16: return "int16_t";
            case TokenType::kwU16: return "uint16_t";
            case TokenType::kwI32: return "int32_t";
            case TokenType::kwU32: return "uint32_t";
            case TokenType::kwI64: return "int64_t";
            case TokenType::kwU64: return "uint64_t";
            default: return fallbackType;
            }
        };

        auto getEnumUnderlyingWioTypeName = [&](const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                std::string_view fallbackTypeName) -> std::string
        {
            auto typeArgs = getFirstAttributeArgs(attributes, Attribute::Type);
            if (typeArgs.empty())
                return std::string(fallbackTypeName);

            switch (typeArgs[0].type)
            {
            case TokenType::kwI8: return "i8";
            case TokenType::kwU8: return "u8";
            case TokenType::kwI16: return "i16";
            case TokenType::kwU16: return "u16";
            case TokenType::kwI32: return "i32";
            case TokenType::kwU32: return "u32";
            case TokenType::kwI64: return "i64";
            case TokenType::kwU64: return "u64";
            default: return std::string(fallbackTypeName);
            }
        };

        auto emitStringViewArray = [&](std::string_view name, const std::vector<std::string>& values)
        {
            emit("static constexpr std::array<std::string_view, " + std::to_string(values.size()) + "> " +
                 std::string(name) + " = {");
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                    emit(", ");
                emit("\"" + common::wioStringToEscapedCppString(values[i]) + "\"");
            }
            emitLine("};");
        };

        auto emitSimpleTypeReflectionSpecialization = [&](const std::string& cppTypeName,
                                                           const std::string& wioTypeName,
                                                           std::string_view kind)
        {
            emitLine("template <>");
            emitLine("struct wio::runtime::TypeReflection<" + cppTypeName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::string_view Name = \"" +
                     common::wioStringToEscapedCppString(wioTypeName) + "\";");
            emitLine("static constexpr wio::runtime::ReflectedTypeKind Kind = wio::runtime::ReflectedTypeKind::" +
                     std::string(kind) + ";");
            emitStringViewArray("FieldNames", {});
            emitStringViewArray("FieldTypes", {});
            emitStringViewArray("FieldAccess", {});
            emitStringViewArray("MethodNames", {});
            emitStringViewArray("MethodSignatures", {});
            emitStringViewArray("MethodAccess", {});
            emitStringViewArray("BaseTypes", {});
            dedent();
            emitLine("};");
        };

        auto emitEnumReflectionSpecialization = [&](const EnumDeclaration& declaration)
        {
            auto sym = declaration.name->referencedSymbol.Lock();
            std::string enumName = Mangler::mangleStruct(declaration.name->token.value, sym ? sym->scopePath : "");
            const std::string underlyingTypeName = getEnumUnderlyingWioTypeName(declaration.attributes, "i32");

            emitLine("template <>");
            emitLine("struct wio::runtime::EnumReflection<" + enumName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::size_t Count = " + std::to_string(declaration.members.size()) + "u;");
            emitLine("static constexpr std::size_t Size = sizeof(" + enumName + ");");
            emitLine("static constexpr std::string_view UnderlyingTypeName = \"" + underlyingTypeName + "\";");
            emitLine("static std::string Name(const " + enumName + " value)");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (const auto& member : declaration.members)
            {
                emitLine("case " + enumName + "::" + member.name->token.value + ": return \"" +
                         common::wioStringToEscapedCppString(member.name->token.value) + "\";");
            }
            emitLine("default: return \"<unknown>\";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static " + enumName + " Value(const std::size_t index)");
            emitLine("{");
            indent();
            emitLine("switch (index)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + std::to_string(i) + "u: return " + enumName + "::" + declaration.members[i].name->token.value + ";");
            }
            emitLine("default: return " + enumName + "::" + declaration.members.front().name->token.value + ";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static std::ptrdiff_t Index(const " + enumName + " value) noexcept");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + enumName + "::" + declaration.members[i].name->token.value + ": return " + std::to_string(i) + ";");
            }
            emitLine("default: return -1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            dedent();
            emitLine("};");
            emitSimpleTypeReflectionSpecialization(
                enumName,
                sym && !sym->scopePath.empty()
                    ? sym->scopePath + "::" + declaration.name->token.value
                    : declaration.name->token.value,
                "enum_type");
        };

        auto emitFlagsetReflectionSpecialization = [&](const FlagsetDeclaration& declaration)
        {
            auto sym = declaration.name->referencedSymbol.Lock();
            std::string flagsetName = Mangler::mangleStruct(declaration.name->token.value, sym ? sym->scopePath : "");
            const std::string underlyingTypeName = getEnumUnderlyingWioTypeName(declaration.attributes, "u32");

            emitLine("template <>");
            emitLine("struct wio::runtime::EnumReflection<" + flagsetName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::size_t Count = " + std::to_string(declaration.members.size()) + "u;");
            emitLine("static constexpr std::size_t Size = sizeof(" + flagsetName + ");");
            emitLine("static constexpr std::string_view UnderlyingTypeName = \"" + underlyingTypeName + "\";");
            emitLine("static std::string Name(const " + flagsetName + " value)");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (const auto& member : declaration.members)
            {
                emitLine("case " + flagsetName + "::" + member.name->token.value + ": return \"" +
                         common::wioStringToEscapedCppString(member.name->token.value) + "\";");
            }
            emitLine("default: break;");
            dedent();
            emitLine("}");
            emitLine("using Under = std::underlying_type_t<" + flagsetName + ">;");
            emitLine("const Under raw = static_cast<Under>(value);");
            emitLine("std::string result;");
            emitLine("bool first = true;");
            emitLine("Under remaining = raw;");
            for (const auto& member : declaration.members)
            {
                const std::string memberName = member.name->token.value;
                emitLine("{");
                indent();
                emitLine("const Under memberValue = static_cast<Under>(" + flagsetName + "::" + memberName + ");");
                emitLine("if (memberValue != 0 && (raw & memberValue) == memberValue)");
                emitLine("{");
                indent();
                emitLine("if (!first) result += \"|\";");
                emitLine("result += \"" + common::wioStringToEscapedCppString(memberName) + "\";");
                emitLine("first = false;");
                emitLine("remaining = static_cast<Under>(remaining & static_cast<Under>(~memberValue));");
                dedent();
                emitLine("}");
                dedent();
                emitLine("}");
            }
            emitLine("if (remaining != 0)");
            emitLine("{");
            indent();
            emitLine("if (!result.empty()) result += \"|\";");
            emitLine("result += \"<unknown>\";");
            dedent();
            emitLine("}");
            emitLine("if (result.empty()) return raw == 0 ? \"0\" : \"<unknown>\";");
            emitLine("return result;");
            dedent();
            emitLine("}");
            emitLine("static " + flagsetName + " Value(const std::size_t index)");
            emitLine("{");
            indent();
            emitLine("switch (index)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + std::to_string(i) + "u: return " + flagsetName + "::" + declaration.members[i].name->token.value + ";");
            }
            emitLine("default: return " + flagsetName + "::" + declaration.members.front().name->token.value + ";");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            emitLine("static std::ptrdiff_t Index(const " + flagsetName + " value) noexcept");
            emitLine("{");
            indent();
            emitLine("switch (value)");
            emitLine("{");
            indent();
            for (size_t i = 0; i < declaration.members.size(); ++i)
            {
                emitLine("case " + flagsetName + "::" + declaration.members[i].name->token.value + ": return " + std::to_string(i) + ";");
            }
            emitLine("default: return -1;");
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
            dedent();
            emitLine("};");
            emitSimpleTypeReflectionSpecialization(
                flagsetName,
                sym && !sym->scopePath.empty()
                    ? sym->scopePath + "::" + declaration.name->token.value
                    : declaration.name->token.value,
                "flagset_type");
        };

        auto accessName = [](const AccessModifier access, const bool objectDefault) -> std::string
        {
            if (access == AccessModifier::Public)
                return "public";
            if (access == AccessModifier::Protected)
                return "protected";
            if (access == AccessModifier::Private)
                return "private";
            return objectDefault ? "private" : "public";
        };

        auto emitStructuredTypeReflectionSpecialization = [&](const auto& declaration,
                                                               const std::vector<ComponentMember>* componentMembers,
                                                               const std::vector<ObjectMember>* objectMembers,
                                                               const std::vector<NodePtr<FunctionDeclaration>>* interfaceMethods,
                                                               std::string_view reflectedKind)
        {
            auto symbol = declaration.name->referencedSymbol.Lock();
            auto structType = getStructTypeFromSymbol(symbol);
            if (!structType || usesNativePodAliasModelForCodegen(structType))
                return;

            std::vector<std::string> fieldNames;
            std::vector<std::string> fieldTypes;
            std::vector<std::string> fieldAccess;
            std::vector<std::string> methodNames;
            std::vector<std::string> methodSignatures;
            std::vector<std::string> methodAccess;
            std::vector<std::string> methodBehaviorAttributeNames;
            std::vector<std::string> methodBehaviorProcessorTypes;
            std::vector<std::string> methodBehaviorPhases;
            std::vector<std::string> methodBehaviorHooks;
            std::vector<std::string> methodBehaviorModes;
            std::vector<size_t> methodBehaviorOffsets{0};
            std::vector<std::string> baseTypes;
            std::vector<std::string> typeAttributes;
            std::vector<std::string> typeAttributeNames;
            std::vector<std::string> typeAttributeRetentions;
            std::vector<std::string> typeAttributeOrigins;
            std::vector<std::uint64_t> typeAttributeStableIds;
            std::vector<std::string> typeAttributeArgumentNames;
            std::vector<std::string> typeAttributeArgumentTypes;
            std::vector<std::string> typeAttributeArgumentValues;
            std::vector<std::uint8_t> typeAttributeArgumentUsedDefaults;
            std::vector<size_t> typeAttributeArgumentOffsets{0};
            std::vector<std::string> fieldAttributeNames;
            std::vector<size_t> fieldAttributeOffsets{0};

            auto reflectedAttributeName = [](const NodePtr<AttributeStatement>& attribute)
            {
                if (!attribute || !attribute->runtimeRetained || attribute->qualifiedName.empty())
                    return std::string{};
                std::string result = attribute->qualifiedName;
                if (!attribute->args.empty())
                {
                    result += "(";
                    for (size_t index = 0; index < attribute->args.size(); ++index)
                    {
                        if (index > 0) result += ",";
                        result += attribute->args[index].value;
                    }
                    result += ")";
                }
                return result;
            };

            for (const auto& attribute : declaration.attributes)
            {
                auto name = reflectedAttributeName(attribute);
                if (name.empty())
                    continue;
                typeAttributes.push_back(std::move(name));
                typeAttributeNames.push_back(attribute->qualifiedName);
                typeAttributeRetentions.push_back("runtime");
                switch (attribute->origin)
                {
                case AttributeOrigin::Direct: typeAttributeOrigins.push_back("direct"); break;
                case AttributeOrigin::Inherited: typeAttributeOrigins.push_back("inherited"); break;
                case AttributeOrigin::Scoped: typeAttributeOrigins.push_back("scoped"); break;
                case AttributeOrigin::Composed: typeAttributeOrigins.push_back("composed"); break;
                case AttributeOrigin::Generated: typeAttributeOrigins.push_back("generated"); break;
                case AttributeOrigin::Compiler: typeAttributeOrigins.push_back("compiler"); break;
                }

                std::uint64_t stableId = 14695981039346656037ull;
                const std::string& stableAttributeName = attribute->canonicalName.empty()
                    ? attribute->qualifiedName
                    : attribute->canonicalName;
                for (const unsigned char byte : stableAttributeName)
                {
                    stableId ^= byte;
                    stableId *= 1099511628211ull;
                }
                typeAttributeStableIds.push_back(stableId);

                for (size_t argumentIndex = 0; argumentIndex < attribute->args.size(); ++argumentIndex)
                {
                    const Token& argument = attribute->args[argumentIndex];
                    typeAttributeArgumentNames.push_back(
                        argumentIndex < attribute->argumentNames.size()
                            ? attribute->argumentNames[argumentIndex]
                            : std::string{});
                    std::string argumentType = "unknown";
                    if (argument.type == TokenType::stringLiteral)
                        argumentType = argument.isUnicodeString ? "text" : "string";
                    else if (argument.type == TokenType::integerLiteral)
                        argumentType = "integer";
                    else if (argument.type == TokenType::floatLiteral)
                        argumentType = "float";
                    else if (argument.type == TokenType::byteLiteral)
                        argumentType = "byte";
                    else if (argument.type == TokenType::kwTrue || argument.type == TokenType::kwFalse)
                        argumentType = "bool";
                    else if (argument.type == TokenType::identifier)
                        argumentType = "symbol";
                    typeAttributeArgumentTypes.push_back(std::move(argumentType));
                    typeAttributeArgumentValues.push_back(argument.value);
                    typeAttributeArgumentUsedDefaults.push_back(
                        argumentIndex < attribute->argumentUsedDefaults.size() &&
                        attribute->argumentUsedDefaults[argumentIndex]
                            ? static_cast<std::uint8_t>(1)
                            : static_cast<std::uint8_t>(0));
                }
                typeAttributeArgumentOffsets.push_back(typeAttributeArgumentValues.size());
            }

            auto addMember = [&](const auto& member, const bool objectDefault)
            {
                if (member.declaration->template is<VariableDeclaration>())
                {
                    auto variable = member.declaration->template as<VariableDeclaration>();
                    if (variable->mutability == Mutability::Const)
                        return;
                    fieldNames.push_back(variable->name->token.value);
                    Ref<sema::Type> type = variable->type ? variable->type->refType.Lock() : variable->name->refType.Lock();
                    fieldTypes.push_back(type ? type->toString() : "<unknown>");
                    fieldAccess.push_back(accessName(member.access, objectDefault));
                    for (const auto& attribute : variable->attributes)
                    {
                        auto name = reflectedAttributeName(attribute);
                        if (!name.empty()) fieldAttributeNames.push_back(std::move(name));
                    }
                    fieldAttributeOffsets.push_back(fieldAttributeNames.size());
                    return;
                }

                if (member.declaration->template is<FunctionDeclaration>())
                {
                    auto function = member.declaration->template as<FunctionDeclaration>();
                    methodNames.push_back(function->name->token.value);
                    std::string signature = function->name->token.value + "(";
                    for (size_t i = 0; i < function->parameters.size(); ++i)
                    {
                        Ref<sema::Type> parameterType = function->parameters[i].name->refType.Lock();
                        signature += parameterType ? parameterType->toString() : "<unknown>";
                        if (i + 1 < function->parameters.size())
                            signature += ", ";
                    }
                    Ref<sema::Type> returnType = function->returnType ? function->returnType->refType.Lock() : nullptr;
                    signature += ") -> " + std::string(returnType ? returnType->toString() : "void");
                    methodSignatures.push_back(std::move(signature));
                    methodAccess.push_back(accessName(member.access, objectDefault));

                    std::vector<const AttributeStatement*> orderedAttributes;
                    orderedAttributes.reserve(function->attributes.size());
                    for (const auto& attribute : function->attributes)
                    {
                        if (attribute)
                            orderedAttributes.push_back(attribute.Get());
                    }
                    std::ranges::stable_sort(
                        orderedAttributes,
                        {},
                        &AttributeStatement::processorOrder);
                    for (const auto* attribute : orderedAttributes)
                    {
                        for (const auto& processor : attribute->processorBindings)
                        {
                            if ((processor.phase != "pre" && processor.phase != "post" &&
                                 processor.phase != "finally" && processor.phase != "around") ||
                                processor.cppTypeName.empty() || processor.hookCppName.empty())
                            {
                                continue;
                            }
                            methodBehaviorAttributeNames.push_back(
                                attribute->canonicalName.empty()
                                    ? attribute->qualifiedName
                                    : attribute->canonicalName);
                            methodBehaviorProcessorTypes.push_back(processor.canonicalTypeName);
                            methodBehaviorPhases.push_back(processor.phase);
                            methodBehaviorHooks.push_back(
                                processor.phase == "pre" ? "Before" :
                                processor.phase == "post" ? "After" :
                                processor.phase == "finally" ? "Finally" : "Around");
                            methodBehaviorModes.push_back(processor.hookMode);
                        }
                    }
                    methodBehaviorOffsets.push_back(methodBehaviorPhases.size());
                }
            };

            if (componentMembers)
            {
                for (const auto& member : *componentMembers)
                    addMember(member, false);
            }
            if (objectMembers)
            {
                for (const auto& member : *objectMembers)
                    addMember(member, true);
            }
            if (interfaceMethods)
            {
                for (const auto& function : *interfaceMethods)
                {
                    methodNames.push_back(function->name->token.value);
                    std::string signature = function->name->token.value + "(";
                    for (size_t i = 0; i < function->parameters.size(); ++i)
                    {
                        Ref<sema::Type> parameterType = function->parameters[i].name->refType.Lock();
                        signature += parameterType ? parameterType->toString() : "<unknown>";
                        if (i + 1 < function->parameters.size())
                            signature += ", ";
                    }
                    Ref<sema::Type> returnType = function->returnType ? function->returnType->refType.Lock() : nullptr;
                    signature += ") -> " + std::string(returnType ? returnType->toString() : "void");
                    methodSignatures.push_back(std::move(signature));
                    methodAccess.push_back("public");
                    methodBehaviorOffsets.push_back(methodBehaviorPhases.size());
                }
            }

            for (const auto& baseType : structType->baseTypes)
            {
                Ref<sema::Type> resolvedBase = unwrapAliasType(baseType);
                if (!resolvedBase)
                    continue;
                if (resolvedBase->kind() == sema::TypeKind::Struct)
                {
                    auto baseStruct = resolvedBase.AsFast<sema::StructType>();
                    if (baseStruct && baseStruct->name == "object" && baseStruct->scopePath.empty())
                        continue;
                }
                baseTypes.push_back(resolvedBase->toString());
            }

            std::string cppTypeName = reflectedKind == "interface_type"
                ? Mangler::mangleInterface(structType->name, structType->scopePath)
                : mangleStructTypeName(structType);
            std::vector<std::string> reflectionParameterNames;
            reflectionParameterNames.reserve(declaration.genericParameters.size());
            for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                reflectionParameterNames.push_back("_WIO_REFLECT_ARG_" + std::to_string(i));
            if (structType->isExplicitSpecialization)
            {
                cppTypeName = mangleStructTypeName(structType);
            }
            else if (!declaration.genericParameters.empty())
            {
                cppTypeName =
                    (reflectedKind == "interface_type"
                        ? Mangler::mangleInterface(structType->name, structType->scopePath)
                        : Mangler::mangleStruct(structType->name, structType->scopePath)) +
                    "<";
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        cppTypeName += ", ";
                    cppTypeName += reflectionParameterNames[i];
                    if (declaration.hasGenericParameterPack && i + 1 == declaration.genericParameters.size())
                        cppTypeName += "...";
                }
                cppTypeName += ">";
            }
            if (structType->isExplicitSpecialization && !declaration.genericParameters.empty())
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    replaceCppIdentifier(
                        cppTypeName,
                        declaration.genericParameters[i]->token.value,
                        reflectionParameterNames[i]);
                }
            }

            std::vector<std::string> genericMetadataParameterNames;
            std::vector<std::string> genericMetadataArgumentExpressions;
            Ref<sema::StructType> genericMetadataPrimary = structType->genericPrimaryType.Lock();
            if (structType->isExplicitSpecialization && genericMetadataPrimary)
                genericMetadataParameterNames = genericMetadataPrimary->genericParameterNames;
            else
                genericMetadataParameterNames = structType->genericParameterNames;

            auto reflectionParameterNameFor = [&](std::string_view sourceName) -> std::string
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (declaration.genericParameters[i]->token.value == sourceName)
                        return reflectionParameterNames[i];
                }
                return sanitizeCppIdentifier(sourceName);
            };
            std::function<std::string(const Ref<sema::Type>&)> genericArgumentExpression;
            genericArgumentExpression = [&](const Ref<sema::Type>& argument) -> std::string
            {
                if (!argument)
                    return "std::string(\"<unknown>\")";

                Ref<sema::Type> resolvedArgument = argument;
                while (resolvedArgument && resolvedArgument->kind() == sema::TypeKind::Alias)
                    resolvedArgument = resolvedArgument.AsFast<sema::AliasType>()->aliasedType;
                if (!resolvedArgument)
                    return "std::string(\"<unknown>\")";

                if (resolvedArgument->kind() == sema::TypeKind::GenericParameter)
                {
                    const auto parameter = resolvedArgument.AsFast<sema::GenericParameterType>();
                    return "wio::runtime::ReflectedTypeName<" +
                           reflectionParameterNameFor(parameter->name) + ">()";
                }
                if (resolvedArgument->kind() == sema::TypeKind::ConstGenericParameter)
                {
                    const auto parameter = resolvedArgument.AsFast<sema::ConstGenericParameterType>();
                    const std::string backendName = reflectionParameterNameFor(parameter->name);
                    Ref<sema::Type> valueType = unwrapAliasTypeForCodegen(parameter->valueType);
                    if (valueType && valueType->kind() == sema::TypeKind::Primitive)
                    {
                        const std::string& valueTypeName = valueType.AsFast<sema::PrimitiveType>()->name;
                        if (valueTypeName == "string")
                            return backendName + ".RuntimeValue()";
                        if (valueTypeName == "text")
                            return backendName + ".RuntimeValue().Utf8()";
                    }
                    return "std::to_string(" + backendName + ")";
                }
                if (resolvedArgument->kind() == sema::TypeKind::ConstValue)
                {
                    const auto value = resolvedArgument.AsFast<sema::ConstValueType>();
                    return "std::string(\"" +
                           common::wioStringToEscapedCppString(value->value) + "\")";
                }
                if (resolvedArgument->kind() == sema::TypeKind::Struct)
                {
                    const auto nested = resolvedArgument.AsFast<sema::StructType>();
                    const std::string nestedName = nested->scopePath.empty()
                        ? nested->name
                        : nested->scopePath + "::" + nested->name;
                    if (nested->genericArguments.empty())
                        return "std::string(\"" +
                               common::wioStringToEscapedCppString(nestedName) + "\")";

                    std::string expression = "std::string(\"" +
                        common::wioStringToEscapedCppString(nestedName + "<") + "\")";
                    for (size_t i = 0; i < nested->genericArguments.size(); ++i)
                    {
                        if (i > 0)
                            expression += " + std::string(\", \")";
                        expression += " + " + genericArgumentExpression(nested->genericArguments[i]);
                    }
                    return expression + " + std::string(\">\")";
                }

                return "std::string(\"" +
                       common::wioStringToEscapedCppString(resolvedArgument->toString()) + "\")";
            };

            if (structType->isExplicitSpecialization)
            {
                for (const auto& argument : structType->genericArguments)
                    genericMetadataArgumentExpressions.push_back(genericArgumentExpression(argument));
            }
            else
            {
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    const auto& parameter = declaration.genericParameters[i];
                    const std::string& backendName = reflectionParameterNames[i];
                    if (parameter->isConstGenericParameter)
                    {
                        Ref<sema::Type> valueType = parameter->genericValueType
                            ? unwrapAliasTypeForCodegen(parameter->genericValueType->refType.Lock())
                            : nullptr;
                        if (valueType && valueType->kind() == sema::TypeKind::Primitive)
                        {
                            const std::string& valueTypeName = valueType.AsFast<sema::PrimitiveType>()->name;
                            if (valueTypeName == "string")
                                genericMetadataArgumentExpressions.push_back(backendName + ".RuntimeValue()");
                            else if (valueTypeName == "text")
                                genericMetadataArgumentExpressions.push_back(backendName + ".RuntimeValue().Utf8()");
                            else
                                genericMetadataArgumentExpressions.push_back("std::to_string(" + backendName + ")");
                        }
                        else
                        {
                            genericMetadataArgumentExpressions.push_back("std::to_string(" + backendName + ")");
                        }
                    }
                    else
                    {
                        genericMetadataArgumentExpressions.push_back(
                            "wio::runtime::ReflectedTypeName<" + backendName + ">()" +
                            (declaration.hasGenericParameterPack && i + 1 == declaration.genericParameters.size()
                                ? "..."
                                : ""));
                    }
                }
            }
            const std::string wioTypeName = structType->scopePath.empty()
                ? structType->name
                : structType->scopePath + "::" + structType->name;
            if (declaration.genericParameters.empty())
            {
                emitLine("template <>");
            }
            else
            {
                emit("template <");
                for (size_t i = 0; i < declaration.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        emit(", ");
                    const bool isPack = declaration.hasGenericParameterPack &&
                                        i + 1 == declaration.genericParameters.size();
                    emit(formatCppTemplateParameter(
                        declaration.genericParameters[i],
                        isPack,
                        reflectionParameterNames[i]));
                }
                emitLine(">");
            }
            emitLine("struct wio::runtime::TypeReflection<" + cppTypeName + ">");
            emitLine("{");
            indent();
            emitLine("static constexpr std::string_view Name = \"" +
                     common::wioStringToEscapedCppString(wioTypeName) + "\";");
            emitLine("static constexpr wio::runtime::ReflectedTypeKind Kind = wio::runtime::ReflectedTypeKind::" +
                     std::string(reflectedKind) + ";");
            emitStringViewArray("FieldNames", fieldNames);
            emitStringViewArray("FieldTypes", fieldTypes);
            emitStringViewArray("FieldAccess", fieldAccess);
            emitStringViewArray("MethodNames", methodNames);
            emitStringViewArray("MethodSignatures", methodSignatures);
            emitStringViewArray("MethodAccess", methodAccess);
            emitStringViewArray("MethodBehaviorAttributeNames", methodBehaviorAttributeNames);
            emitStringViewArray("MethodBehaviorProcessorTypes", methodBehaviorProcessorTypes);
            emitStringViewArray("MethodBehaviorPhases", methodBehaviorPhases);
            emitStringViewArray("MethodBehaviorHooks", methodBehaviorHooks);
            emitStringViewArray("MethodBehaviorModes", methodBehaviorModes);
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(methodBehaviorOffsets.size()) + "> MethodBehaviorOffsets{ ");
            for (size_t index = 0; index < methodBehaviorOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(methodBehaviorOffsets[index]));
            }
            emitLine(" };");
            emitStringViewArray("BaseTypes", baseTypes);
            emitStringViewArray("TypeAttributes", typeAttributes);
            emitStringViewArray("TypeAttributeNames", typeAttributeNames);
            emitStringViewArray("TypeAttributeRetentions", typeAttributeRetentions);
            emitStringViewArray("TypeAttributeOrigins", typeAttributeOrigins);
            emitStringViewArray("TypeAttributeArgumentNames", typeAttributeArgumentNames);
            emitStringViewArray("TypeAttributeArgumentTypes", typeAttributeArgumentTypes);
            emitStringViewArray("TypeAttributeArgumentValues", typeAttributeArgumentValues);
            emit("static constexpr std::array<std::uint64_t, " +
                 std::to_string(typeAttributeStableIds.size()) + "> TypeAttributeStableIds{ ");
            for (size_t index = 0; index < typeAttributeStableIds.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeStableIds[index]) + "ull");
            }
            emitLine(" };");
            emit("static constexpr std::array<std::uint8_t, " +
                 std::to_string(typeAttributeArgumentUsedDefaults.size()) + "> TypeAttributeArgumentUsedDefaults{ ");
            for (size_t index = 0; index < typeAttributeArgumentUsedDefaults.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeArgumentUsedDefaults[index]));
            }
            emitLine(" };");
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(typeAttributeArgumentOffsets.size()) + "> TypeAttributeArgumentOffsets{ ");
            for (size_t index = 0; index < typeAttributeArgumentOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(typeAttributeArgumentOffsets[index]));
            }
            emitLine(" };");
            emitStringViewArray("FieldAttributeNames", fieldAttributeNames);
            emit("static constexpr std::array<std::size_t, " +
                 std::to_string(fieldAttributeOffsets.size()) + "> FieldAttributeOffsets{ ");
            for (size_t index = 0; index < fieldAttributeOffsets.size(); ++index)
            {
                if (index > 0) emit(", ");
                emit(std::to_string(fieldAttributeOffsets[index]));
            }
            emitLine(" };");
            if (!genericMetadataParameterNames.empty())
            {
                emitLine("static std::vector<std::string> _WIOGenericParameterNames()");
                emitLine("{");
                indent();
                emit("return { ");
                for (size_t i = 0; i < genericMetadataParameterNames.size(); ++i)
                {
                    if (i > 0) emit(", ");
                    const std::string suffix = structType->hasGenericParameterPack &&
                                               i + 1 == genericMetadataParameterNames.size()
                        ? "..."
                        : "";
                    emit("\"" + common::wioStringToEscapedCppString(
                        genericMetadataParameterNames[i] + suffix) + "\"");
                }
                emitLine(" };");
                dedent();
                emitLine("}");
                emitLine("static std::vector<std::string> _WIOGenericArguments()");
                emitLine("{");
                indent();
                emit("return { ");
                for (size_t i = 0; i < genericMetadataArgumentExpressions.size(); ++i)
                {
                    if (i > 0) emit(", ");
                    emit(genericMetadataArgumentExpressions[i]);
                }
                emitLine(" };");
                dedent();
                emitLine("}");
            }
            dedent();
            emitLine("};");
        };

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<UseStatement>())
                stmt->accept(*this);
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FlagDeclaration>())
                stmt->accept(*this);

            if (stmt->template is<FlagsetDeclaration>())
            {
                auto declaration = stmt->template as<FlagsetDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto enumType = getStructTypeFromSymbol(sym);
                if (enumType && usesNativePodAliasModelForCodegen(enumType))
                {
                    std::string nativeTypeName = enumType->nativeCppName.empty()
                        ? enumType->name
                        : enumType->nativeCppName;

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    stmt->accept(*this);
                }

                emitFlagsetReflectionSpecialization(*declaration);
            }
            else if (stmt->template is<EnumDeclaration>())
            {
                auto declaration = stmt->template as<EnumDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto enumType = getStructTypeFromSymbol(sym);
                if (enumType && usesNativePodAliasModelForCodegen(enumType))
                {
                    std::string nativeTypeName = enumType->nativeCppName.empty()
                        ? enumType->name
                        : enumType->nativeCppName;

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    stmt->accept(*this);
                }

                emitEnumReflectionSpecialization(*declaration);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<VariableDeclaration>())
            {
                auto variableDecl = stmt->template as<VariableDeclaration>();
                if (variableDecl->mutability == Mutability::Const)
                    stmt->accept(*this);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<ComponentDeclaration>())
            {
                auto declaration = stmt->template as<ComponentDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto componentType = getStructTypeFromSymbol(sym);
                if (componentType && componentType->isExplicitSpecialization &&
                    usesNativePodAliasModelForCodegen(componentType))
                {
                    // The primary alias already maps every concrete argument
                    // to the native C++ template. Wio specializations refine
                    // semantic layout only and must not emit an illegal alias
                    // template specialization.
                }
                else if (componentType && componentType->isExplicitSpecialization)
                {
                    if (componentType->isPartialSpecialization)
                        emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    else
                        emitLine("template <>");
                    emitLine(common::formatString("struct {};", mangleStructTypeName(componentType)));
                }
                else if (componentType && usesNativePodAliasModelForCodegen(componentType))
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);

                    std::string nativeTypeName = componentType->nativeCppName.empty()
                        ? componentType->name
                        : componentType->nativeCppName;
                    if (!declaration->genericParameters.empty())
                    {
                        nativeTypeName += "<";
                        for (size_t i = 0; i < declaration->genericParameters.size(); ++i)
                        {
                            if (i > 0)
                                nativeTypeName += ", ";

                            nativeTypeName += declaration->genericParameters[i]->token.value;
                            if (declaration->hasGenericParameterPack && i + 1 == declaration->genericParameters.size())
                                nativeTypeName += "...";
                        }
                        nativeTypeName += ">";
                    }

                    emitLine(common::formatString(
                        "using {} = {};",
                        Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : ""),
                        nativeTypeName
                    ));
                }
                else
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    emitLine(common::formatString("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
                }
            }
            else if (stmt->template is<ObjectDeclaration>())
            {
                auto declaration = stmt->template as<ObjectDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                auto objectType = getStructTypeFromSymbol(sym);
                if (objectType && objectType->isExplicitSpecialization)
                {
                    if (objectType->isPartialSpecialization)
                        emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    else
                        emitLine("template <>");
                    emitLine(common::formatString("struct {};", mangleStructTypeName(objectType)));
                }
                else
                {
                    emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                    emitLine(common::formatString("struct {};", Mangler::mangleStruct(declaration->name->token.value, sym ? sym->scopePath : "")));
                }
            }
            else if (stmt->template is<InterfaceDeclaration>())
            {
                auto declaration = stmt->template as<InterfaceDeclaration>();
                auto sym = declaration->name->referencedSymbol.Lock();
                emitTemplateForwardDeclarationPrefix(declaration->genericParameters);
                emitLine(common::formatString("struct {};", Mangler::mangleInterface(declaration->name->token.value, sym ? sym->scopePath : "")));
            }
        });

        // Member functions are emitted inline with their owning type. Declare
        // globals before those type definitions so lifecycle hooks and ordinary
        // methods can safely reference globals declared later in the module.
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (!stmt->template is<VariableDeclaration>())
                return;

            auto variable = stmt->template as<VariableDeclaration>();
            auto symbol = variable->name->referencedSymbol.Lock();
            Ref<sema::Type> type = symbol && symbol->type
                ? symbol->type
                : variable->name->refType.Lock();
            if (!type)
                return;

            const std::string declarationType =
                variable->mutability != Mutability::Mutable
                    ? "const " + toCppType(type)
                    : toCppType(type);
            emitLine(common::formatString(
                "extern {} {};",
                declarationType,
                Mangler::mangleGlobalVar(variable->name->token.value, symbol ? symbol->scopePath : "")
            ));
        });

        isEmittingPrototypes_ = true;
        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
            else if (stmt->template is<ExtensionDeclaration>())
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
            if (stmt->template is<ComponentDeclaration>())
            {
                auto declaration = stmt->template as<ComponentDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    &declaration->members,
                    nullptr,
                    nullptr,
                    "component_type");
            }
            else if (stmt->template is<ObjectDeclaration>())
            {
                auto declaration = stmt->template as<ObjectDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    nullptr,
                    &declaration->members,
                    nullptr,
                    "object_type");
            }
            else if (stmt->template is<InterfaceDeclaration>())
            {
                auto declaration = stmt->template as<InterfaceDeclaration>();
                emitStructuredTypeReflectionSpecialization(
                    *declaration,
                    nullptr,
                    nullptr,
                    &declaration->methods,
                    "interface_type");
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<VariableDeclaration>())
            {
                auto variableDecl = stmt->template as<VariableDeclaration>();
                if (variableDecl->mutability != Mutability::Const)
                    stmt->accept(*this);
            }
        });

        emitPhase(emitPhase, statements, [&](const auto& stmt)
        {
            if (stmt->template is<FunctionDeclaration>())
                stmt->accept(*this);
            else if (stmt->template is<ExtensionDeclaration>())
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

        if (node.packIndex)
        {
            if (auto resolvedType = node.refType.Lock();
                resolvedType && !containsGenericParameterTypeForCodegen(resolvedType))
            {
                emit(toCppType(resolvedType));
                return;
            }

            if (auto indexValue = tryEvaluateStaticPackIndex(node.packIndex, variableDeclarationsBySymbol_))
            {
                emit(common::formatString(
                    "typename wio::meta::TypePackView<{}...>::template At<{}>",
                    node.name.value,
                    *indexValue
                ));
                return;
            }
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
        auto emitAssignableLeftExpression = [&](const NodePtr<Expression>& expression)
        {
            Ref<sema::Type> expressionType = expression ? expression->refType.Lock() : nullptr;
            if (shouldTreatReferenceAsHandleForAssignment(expressionType))
            {
                expression->accept(*this);
                return;
            }

            emitReadableExpression(expression);
        };

        auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
        {
            std::string accessOperator = ".";
            std::size_t referenceDepth = 0;
            Ref<sema::Type> terminalType = nullptr;

            if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
            {
                auto baseType = receiverType;
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
                    accessOperator = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        accessOperator = "->";
                }
            }

            if (referenceDepth <= 1)
            {
                receiver->accept(*this);
            }
            else
            {
                emit("(");
                for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                    emit("*");
                emit("(");
                receiver->accept(*this);
                emit("))");
            }

            emit(accessOperator);
        };

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.left);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.right);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.left, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.left);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.right);
                emit(")");
            }
            return;
        }

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
            if (isAnyTypeForCodegen(node.left->refType.Lock()))
            {
                Ref<sema::Type> targetType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
                emit("([&](){ wio::runtime::Any _wio_any = ");
                node.left->accept(*this);
                emit("; return ");

                if (targetType && targetType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = targetType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                    {
                        emit("_wio_any.IsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                    }
                    else if (structType->isObject && !structType->isInterface)
                    {
                        emit("!_wio_any.IsNull() && _wio_any.CanCastObject<" + mangleStructTypeName(structType) + ">()");
                    }
                    else
                    {
                        emit("_wio_any.IsBoxed<" + toCppType(targetType) + ">()");
                    }
                }
                else if (isOpaqueTypeForCodegen(targetType))
                {
                    emit("_wio_any.IsOpaque()");
                }
                else
                {
                    emit("_wio_any.IsBoxed<" + toCppType(targetType) + ">()");
                }

                emit("; }())");
                return;
            }

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

            Ref<sema::Type> rhsType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
            auto targetStruct = rhsType && rhsType->kind() == sema::TypeKind::Struct
                ? rhsType.AsFast<sema::StructType>()
                : nullptr;

            if (targetStruct)
            {
                if (targetStruct->isInterface)
                    emit(mangleInterfaceTypeName(targetStruct) + "::TYPE_ID");
                else
                    emit(mangleStructTypeName(targetStruct) + "::TYPE_ID");
            }
            else if (auto rightSym = node.right->referencedSymbol.Lock(); rightSym && rightSym->kind == sema::SymbolKind::Struct)
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

        const Ref<sema::Type> arithmeticResultType = unwrapAliasTypeForCodegen(node.refType.Lock());
        const auto isIntegerPrimitive = [](const Ref<sema::Type>& type)
        {
            if (!type || type->kind() != sema::TypeKind::Primitive)
                return false;
            const std::string& name = type.AsFast<sema::PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "isize" || name == "u8" || name == "u16" || name == "u32" ||
                   name == "u64" || name == "usize" || name == "byte";
        };

        const auto isStringPrimitive = [](const Ref<sema::Type>& type)
        {
            return type && type->kind() == sema::TypeKind::Primitive &&
                   type.AsFast<sema::PrimitiveType>()->name == "string";
        };
        const Ref<sema::Type> textualLeftType = unwrapAliasTypeForCodegen(node.left->refType.Lock());
        const Ref<sema::Type> textualRightType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
        const bool isStringBinaryOperator =
            node.op.type == TokenType::opPlus ||
            node.op.type == TokenType::opEqual ||
            node.op.type == TokenType::opNotEqual ||
            node.op.type == TokenType::opLess ||
            node.op.type == TokenType::opLessEqual ||
            node.op.type == TokenType::opGreater ||
            node.op.type == TokenType::opGreaterEqual;
        if (isStringBinaryOperator &&
            isStringPrimitive(textualLeftType) &&
            isStringPrimitive(textualRightType))
        {
            emit("(wio::String(");
            emitReadableExpression(node.left);
            emit(") " + node.op.value + " ");
            emitReadableExpression(node.right);
            emit(")");
            return;
        }

        std::string_view integerHelper;
        if (isIntegerPrimitive(arithmeticResultType))
        {
            switch (node.op.type)
            {
            case TokenType::opPlus: integerHelper = "WrappingAdd"; break;
            case TokenType::opMinus: integerHelper = "WrappingSub"; break;
            case TokenType::opStar: integerHelper = "WrappingMul"; break;
            case TokenType::opSlash: integerHelper = "IntegerDivide"; break;
            case TokenType::opPercent: integerHelper = "IntegerRemainder"; break;
            default: break;
            }
        }

        if (!integerHelper.empty())
        {
            emit("wio::intrinsics::" + std::string(integerHelper) + "<" + toCppType(arithmeticResultType) + ">(");
            emitReadableExpression(node.left);
            emit(", ");
            emitReadableExpression(node.right);
            emit(")");
            return;
        }
        
        emit("(");
        const Ref<sema::Type> leftType = unwrapAliasTypeForCodegen(node.left->refType.Lock());
        const Ref<sema::Type> rightType = unwrapAliasTypeForCodegen(node.right->refType.Lock());
        const bool comparesWithNull =
            (node.op.type == TokenType::opEqual || node.op.type == TokenType::opNotEqual) &&
            ((leftType && leftType->kind() == sema::TypeKind::Null) ||
             (rightType && rightType->kind() == sema::TypeKind::Null));

        if (node.op.type == TokenType::opAssign)
            emitAssignableLeftExpression(node.left);
        else if (comparesWithNull && leftType && leftType->kind() == sema::TypeKind::Reference)
            node.left->accept(*this);
        else
            emitReadableExpression(node.left);
        
        std::string opStr = node.op.value;
        if (node.op.type == TokenType::kwAnd)
            opStr = "&&";
        else if (node.op.type == TokenType::kwOr)
            opStr = "||";
        
        emit(" " + opStr + " ");
        if (comparesWithNull && rightType && rightType->kind() == sema::TypeKind::Reference)
            node.right->accept(*this);
        else
            emitReadableExpression(node.right);
        emit(")");
    }

    void CppGenerator::visit(ConditionalExpression& node)
    {
        emit("(");
        emitReadableExpression(node.condition);
        emit(" ? ");
        emitExpressionWithExpectedType(node.whenTrue, node.refType.Lock(), false);
        emit(" : ");
        emitExpressionWithExpectedType(node.whenFalse, node.refType.Lock(), false);
        emit(")");
    }

    void CppGenerator::visit(UnaryExpression& node)
    {
        if (node.isMainExecutorAwait)
        {
            emit("(co_await wio::runtime::AsyncMainAwaiter{})");
            return;
        }

        auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
        {
            std::string accessOperator = ".";
            std::size_t referenceDepth = 0;
            Ref<sema::Type> terminalType = nullptr;

            if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
            {
                auto baseType = receiverType;
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
                    accessOperator = "->";
                }
                else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = terminalType.AsFast<sema::StructType>();
                    if (structType->isObject || structType->isInterface)
                        accessOperator = "->";
                }
            }

            if (referenceDepth <= 1)
            {
                receiver->accept(*this);
            }
            else
            {
                emit("(");
                for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                    emit("*");
                emit("(");
                receiver->accept(*this);
                emit("))");
            }

            emit(accessOperator);
        };

        if (node.op.type == TokenType::kwAwait)
        {
            emit("co_await ");
            node.operand->accept(*this);
            return;
        }

        if (node.op.type == TokenType::kwDeref)
        {
            Ref<sema::Type> operandType = unwrapAliasTypeForCodegen(node.operand ? node.operand->refType.Lock() : nullptr);
            if (operandType && operandType->kind() == sema::TypeKind::Reference)
            {
                Ref<sema::Type> referredType = unwrapAliasTypeForCodegen(
                    operandType.AsFast<sema::ReferenceType>()->referredType
                );
                if (referredType && referredType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = referredType.AsFast<sema::StructType>();
                    if (structType->isObject)
                    {
                        emit("wio::runtime::OwnObjectReference<" + mangleStructTypeName(structType) + ">(");
                        node.operand->accept(*this);
                        emit(")");
                        return;
                    }

                    if (structType->isInterface)
                    {
                        node.operand->accept(*this);
                        return;
                    }
                }
            }

            emit("*(");
            node.operand->accept(*this);
            emit(")");
            return;
        }

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 0u : 1u
            );
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.operand);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("())");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.operand, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.operand);
                emit(")");
            }
            return;
        }

        Ref<sema::Type> unaryResultType = unwrapAliasTypeForCodegen(node.refType.Lock());
        if (node.op.type == TokenType::opMinus &&
            unaryResultType && unaryResultType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = unaryResultType.AsFast<sema::PrimitiveType>()->name;
            const bool isInteger =
                name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" ||
                name == "u8" || name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "byte";
            if (isInteger)
            {
                emit("wio::intrinsics::WrappingNeg<" + toCppType(unaryResultType) + ">(");
                emitReadableExpression(node.operand);
                emit(")");
                return;
            }
        }

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
        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                std::string accessOperator = ".";
                std::size_t referenceDepth = 0;
                Ref<sema::Type> terminalType = nullptr;

                if (auto receiverType = receiver ? receiver->refType.Lock() : nullptr)
                {
                    auto baseType = receiverType;
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
                        accessOperator = "->";
                    else if (terminalType && terminalType->kind() == sema::TypeKind::Struct)
                    {
                        auto structType = terminalType.AsFast<sema::StructType>();
                        if (structType->isObject || structType->isInterface)
                            accessOperator = "->";
                    }
                }

                if (referenceDepth <= 1)
                {
                    receiver->accept(*this);
                }
                else
                {
                    emit("(");
                    for (std::size_t i = 0; i < referenceDepth - 1; ++i)
                        emit("*");
                    emit("(");
                    receiver->accept(*this);
                    emit("))");
                }

                emit(accessOperator);
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.left);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.right);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.left, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.left);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.right, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.right);
                emit(")");
            }
            return;
        }

        auto lhsType = node.left->refType.Lock();
        auto rhsType = node.right->refType.Lock();
        auto emitAssignmentTarget = [&]()
        {
            if (shouldTreatReferenceAsHandleForAssignment(lhsType))
            {
                node.left->accept(*this);
            }
            else
            {
                int derefCount = 0;

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
            }
        };

        Ref<sema::Type> assignmentValueType = unwrapAliasTypeForCodegen(lhsType);
        while (assignmentValueType && assignmentValueType->kind() == sema::TypeKind::Reference)
            assignmentValueType = unwrapAliasTypeForCodegen(assignmentValueType.AsFast<sema::ReferenceType>()->referredType);

        bool isIntegerAssignment = false;
        if (assignmentValueType && assignmentValueType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = assignmentValueType.AsFast<sema::PrimitiveType>()->name;
            isIntegerAssignment =
                name == "i8" || name == "i16" || name == "i32" || name == "i64" || name == "isize" ||
                name == "u8" || name == "u16" || name == "u32" || name == "u64" || name == "usize" || name == "byte";
        }

        std::string_view assignmentHelper;
        if (isIntegerAssignment)
        {
            switch (node.op.type)
            {
            case TokenType::opPlusAssign: assignmentHelper = "WrappingAddAssign"; break;
            case TokenType::opMinusAssign: assignmentHelper = "WrappingSubAssign"; break;
            case TokenType::opStarAssign: assignmentHelper = "WrappingMulAssign"; break;
            case TokenType::opSlashAssign: assignmentHelper = "IntegerDivideAssign"; break;
            case TokenType::opPercentAssign: assignmentHelper = "IntegerRemainderAssign"; break;
            default: break;
            }
        }

        if (!assignmentHelper.empty())
        {
            emit("wio::intrinsics::" + std::string(assignmentHelper) + "(");
            emitAssignmentTarget();
            emit(", ");
            emitExpressionWithExpectedType(node.right, assignmentValueType, false);
            emit(")");
            return;
        }

        emitAssignmentTarget();

        emit(" " + node.op.value + " "); // =, +=, -= ...

        emitExpressionWithExpectedType(node.right, lhsType, false);
    }

    void CppGenerator::visit(IntegerLiteral& node)
    {
        std::string valStr = common::stripIntegerLiteralTypeSuffix(node.token.value);

        auto type = node.refType.Lock();
        std::string tName = type ? type->toString() : "i32";

        if (tName == "i64" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<int64_t>::min)()");
        else if (tName == "isize" && valStr == "-9223372036854775808")
            emit("(std::numeric_limits<ptrdiff_t>::min)()");
        else
        {
            std::string literalExpression = valStr;
            if (tName == "u32")
                literalExpression += "u";
            else if (tName == "i64" || tName == "isize")
                literalExpression += "ll";
            else if (tName == "u64" || tName == "usize")
                literalExpression += "ull";

            // C++ fixed-width aliases differ across ABIs: int64_t/size_t are
            // long on LP64 Linux but long long literals remain long long.
            // Preserve the Wio type at the expression boundary so generic
            // deduction and overload resolution are platform-independent.
            const bool isIntegerType =
                tName == "i8" || tName == "i16" || tName == "i32" || tName == "i64" || tName == "isize" ||
                tName == "u8" || tName == "u16" || tName == "u32" || tName == "u64" || tName == "usize" ||
                tName == "byte";
            if (type && isIntegerType)
                emit("static_cast<" + type->toCppString() + ">(" + literalExpression + ")");
            else
                emit(literalExpression);
        }
    }

    void CppGenerator::visit(FloatLiteral& node)
    {
        std::string valStr = common::stripFloatLiteralTypeSuffix(node.token.value);
        if (valStr.find_first_of(".eE") == std::string::npos)
            valStr += ".0";

        auto type = node.refType.Lock();
        if (type && type->toString() == "f64")
            emit(valStr); 
        else
            emit(valStr + "f"); 
    }

    void CppGenerator::visit(StringLiteral& node)
    {
        const std::string literal = "\"" + common::wioStringToEscapedCppString(node.token.value) + "\"";
        if (node.token.isUnicodeString)
            emit("wio::runtime::Text::FromUtf8(" + literal + ")");
        else
            emit(literal);
    }
    
    void CppGenerator::visit(InterpolatedStringLiteral& node)
    {
        if (node.parts.empty())
        {
            emit(node.isUnicode ? "wio::runtime::Text{}" : "\"\"");
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

        if (node.isUnicode)
            emit("wio::runtime::Text::FromUtf8(");
        emit("wio::runtime::Format(\"" + formatString + "\"");

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
        if (node.isUnicode)
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
                emit(Mangler::mangleFunction(
                    sym->name,
                    funcType->paramTypes,
                    isStructMemberFunctionSymbol(sym) ? "" : sym->scopePath
                ));
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
                    emit(Mangler::mangleFunction(
                        sym->name,
                        funcType->paramTypes,
                        isStructMemberFunctionSymbol(sym) ? "" : scopePath
                    ));
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

        Ref<sema::Type> identifierType = unwrapAliasTypeForCodegen(node.refType.Lock());
        if (!node.referencedSymbol.Lock() &&
            identifierType && identifierType->kind() == sema::TypeKind::Primitive)
        {
            const std::string& name = identifierType.AsFast<sema::PrimitiveType>()->name;
            if (name == "string" || name == "text")
            {
                emit(sanitizeCppIdentifier(node.token.value) + ".RuntimeValue()");
                return;
            }
        }

        emit(sanitizeCppIdentifier(node.token.value));
    }

    void CppGenerator::visit(TypeExpression& node)
    {
        if (auto resolvedType = node.refType.Lock())
        {
            emit(toCppType(resolvedType));
            return;
        }

        if (node.type)
        {
            node.type->accept(*this);
            return;
        }

        emit("/* invalid type expression */");
    }

    void CppGenerator::visit(PackExpansionExpression& node)
    {
        if (node.operand)
            node.operand->accept(*this);
        emit("...");
    }

    void CppGenerator::visit(NullExpression& node)
    {
        auto lockedRefType = node.refType.Lock();
        if (lockedRefType && lockedRefType->kind() == sema::TypeKind::Null)
        {
            auto transformedType = lockedRefType.AsFast<sema::NullType>()->transformedType;
            auto resolvedTransformedType = unwrapAliasTypeForCodegen(transformedType);
            if (resolvedTransformedType && resolvedTransformedType->kind() == sema::TypeKind::Nullable)
                resolvedTransformedType = unwrapAliasTypeForCodegen(
                    resolvedTransformedType.AsFast<sema::NullableType>()->valueType
                );
            const bool isPointerLikeNull =
                !resolvedTransformedType ||
                resolvedTransformedType->kind() == sema::TypeKind::Null ||
                resolvedTransformedType->kind() == sema::TypeKind::Function ||
                resolvedTransformedType->kind() == sema::TypeKind::Reference ||
                (resolvedTransformedType->kind() == sema::TypeKind::Primitive &&
                 (resolvedTransformedType.AsFast<sema::PrimitiveType>()->name == "opaque" ||
                  resolvedTransformedType.AsFast<sema::PrimitiveType>()->name == "any")) ||
                (resolvedTransformedType->kind() == sema::TypeKind::Struct &&
                 ([&]()
                 {
                     auto structType = resolvedTransformedType.AsFast<sema::StructType>();
                     return structType && (structType->isObject || structType->isInterface);
                 })());

            if (isPointerLikeNull)
            {
                emit("nullptr");
                return;
            }

            emit(transformedType ? transformedType->toCppString() : std::string("void"));
            emit("{}");
        }
        else
        {
            emit("nullptr");
        }
    }

    void CppGenerator::visit(ArrayAccessExpression& node)
    {
        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 1u : 2u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.object);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.index, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.index);
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 1)
                    emitExpressionWithExpectedType(node.object, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.object);
                emit(", ");
                if (operatorFunctionType && operatorFunctionType->paramTypes.size() >= 2)
                    emitExpressionWithExpectedType(node.index, operatorFunctionType->paramTypes[1], true);
                else
                    emitReadableExpression(node.index);
                emit(")");
            }
            return;
        }

        Ref<sema::Type> objectType = unwrapAliasType(node.object ? node.object->refType.Lock() : nullptr);
        if (objectType)
        {
            while (objectType && objectType->kind() == sema::TypeKind::Reference)
                objectType = unwrapAliasType(objectType.AsFast<sema::ReferenceType>()->referredType);
        }

        const auto expectedPackName =
            (objectType && objectType->kind() == sema::TypeKind::GenericParameterPack) ? std::optional<std::string_view>(objectType.AsFast<sema::GenericParameterPackType>()->name) :
            (objectType && objectType->kind() == sema::TypeKind::ValuePackView) ? std::optional<std::string_view>(objectType.AsFast<sema::ValuePackViewType>()->packName) :
            (objectType && objectType->kind() == sema::TypeKind::PackStorage) ? std::optional<std::string_view>(objectType.AsFast<sema::PackStorageType>()->packName) :
            (objectType && objectType->kind() == sema::TypeKind::TypePackView) ? std::optional<std::string_view>(objectType.AsFast<sema::TypePackViewType>()->packName) :
            std::nullopt;
        const auto indexBinding = tryEvaluatePackIndexBinding(
            node.index,
            variableDeclarationsBySymbol_,
            expectedPackName,
            SymbolicPackNameMode::Direct);
        if (objectType && indexBinding.has_value())
        {
            auto buildTemplateIndexExpr = [&](const ParsedPackElementBinding& binding, std::string_view sizeExpr) -> std::string
            {
                if (binding.kind == PackElementBindingKind::FromEnd)
                    return common::formatString("({} - {})", sizeExpr, binding.value);

                return std::to_string(binding.value);
            };

            if (objectType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = objectType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                     (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);
                const std::string packValueName = isRuntimePackValue
                    ? sanitizeCppIdentifier(packValueSymbol->name)
                    : packTypeName;
                const std::string sizeExpr = isRuntimePackValue
                    ? common::formatString("sizeof...({})", packValueName)
                    : common::formatString("wio::meta::TypePackView<{}...>::size", packTypeName);
                emit(common::formatString(
                    "std::get<{}>(std::forward_as_tuple({}...))",
                    buildTemplateIndexExpr(*indexBinding, sizeExpr),
                    packValueName
                ));
                return;
            }

            if (objectType->kind() == sema::TypeKind::ValuePackView || objectType->kind() == sema::TypeKind::PackStorage)
            {
                std::string sizeExpr;
                if (objectType->kind() == sema::TypeKind::ValuePackView)
                {
                    auto packViewType = objectType.AsFast<sema::ValuePackViewType>();
                    sizeExpr = packViewType->elementTypes.empty()
                        ? common::formatString("wio::meta::ValuePackView<{}...>::size", packViewType->packName)
                        : std::to_string(packViewType->elementTypes.size());
                }
                else
                {
                    auto storageType = objectType.AsFast<sema::PackStorageType>();
                    sizeExpr = storageType->elementTypes.empty()
                        ? common::formatString("wio::meta::PackStorage<{}...>::size", storageType->packName)
                        : std::to_string(storageType->elementTypes.size());
                }

                emit("(");
                node.object->accept(*this);
                emit(common::formatString(").template Get<{}>()", buildTemplateIndexExpr(*indexBinding, sizeExpr)));
                return;
            }
        }

        auto emitIndexableReceiver = [&]()
        {
            const std::size_t derefCount = getAutoReadableReferenceDepth(node.object ? node.object->refType.Lock() : nullptr);
            for (std::size_t i = 0; i < derefCount; ++i)
                emit("*(");

            node.object->accept(*this);

            for (std::size_t i = 0; i < derefCount; ++i)
                emit(")");
        };

        emit("wio::intrinsics::Index(");
        emitIndexableReceiver();
        emit(", ");
        node.index->accept(*this);
        emit(")");
    }

    bool CppGenerator::emitIntrinsicMemberAccess(MemberAccessExpression& node)
    {
        if (node.intrinsicMember == IntrinsicMember::None)
            return false;

        Ref<sema::Type> receiverType = unwrapAliasType(node.object ? node.object->refType.Lock() : nullptr);
        while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
            receiverType = unwrapAliasType(receiverType.AsFast<sema::ReferenceType>()->referredType);

        auto emitPackReceiverArrayExpr = [&]()
        {
            if (!receiverType)
                return;

            if (receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);

                if (isRuntimePackValue)
                {
                    const std::string packValueName = sanitizeCppIdentifier(packValueSymbol->name);
                    emit(common::formatString("wio::meta::ValuePackView<{}...>({}...)", packTypeName, packValueName));
                }
                else
                {
                    emit(common::formatString("wio::meta::TypePackView<{}...>{{}}", packTypeName));
                }
                return;
            }

            if (receiverType->kind() == sema::TypeKind::TypePackView)
            {
                auto typePackView = receiverType.AsFast<sema::TypePackViewType>();
                if (!typePackView->elementTypes.empty())
                {
                    emit("wio::meta::TypePackView<");
                    for (size_t i = 0; i < typePackView->elementTypes.size(); ++i)
                    {
                        emit(toCppType(typePackView->elementTypes[i]));
                        if (i + 1 < typePackView->elementTypes.size())
                            emit(", ");
                    }
                    emit(">{}");
                }
                else
                {
                    emit(common::formatString("wio::meta::TypePackView<{}...>{{}}", typePackView->packName));
                }
                return;
            }

            if (receiverType->kind() == sema::TypeKind::ValuePackView)
            {
                node.object->accept(*this);
                return;
            }

            if (receiverType->kind() == sema::TypeKind::PackStorage)
            {
                emit("(");
                node.object->accept(*this);
                emit(").AsView()");
            }
        };

            if (node.intrinsicMember == IntrinsicMember::PackSize)
        {
            if (!receiverType)
                return false;

            if (receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = node.object ? node.object->referencedSymbol.Lock() : nullptr;
                const bool isRuntimePackValue =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable);
                const std::string packSizeTarget = isRuntimePackValue
                    ? sanitizeCppIdentifier(packValueSymbol->name)
                    : packTypeName;
                emit(common::formatString("sizeof...({})", packSizeTarget));
                return true;
            }

            emit("(");
            emitPackReceiverArrayExpr();
            emit(").Size()");
            return true;
        }

        if (node.intrinsicMember == IntrinsicMember::PackArray)
        {
            emitPackReceiverArrayExpr();
            return true;
        }

        auto functionType = node.refType.Lock();
        if (!functionType || functionType->kind() != sema::TypeKind::Function)
            return false;

        std::size_t referenceDepth = 0;
        receiverType = node.object->refType.Lock();
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

        if (node.intrinsicMember == IntrinsicMember::TaskPoll ||
            node.intrinsicMember == IntrinsicMember::TaskWithin)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(receiverType);
            if (!resolvedTaskType || resolvedTaskType->kind() != sema::TypeKind::AsyncTask)
                return false;
            Ref<sema::Type> valueType = resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType;
            const bool isVoidTask = valueType && valueType->isVoid();

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
            emit(" { return ");

            auto& typeContext = Compiler::get().getTypeContext();
            Ref<sema::Type> declarationTaskType = resolvedTaskType;
            std::string functionName;
            std::vector<Ref<sema::Type>> declarationParams;
            if (node.intrinsicMember == IntrinsicMember::TaskPoll)
            {
                functionName = "Poll";
                if (!isVoidTask)
                {
                    Ref<sema::Type> genericT = typeContext.getOrCreateGenericParameterType("T");
                    declarationTaskType = typeContext.getOrCreateAsyncTaskType(genericT);
                }
                declarationParams = {declarationTaskType};
            }
            else
            {
                functionName = isVoidTask ? "TimeoutCompleted" : "TimeoutOption";
                if (!isVoidTask)
                {
                    Ref<sema::Type> genericT = typeContext.getOrCreateGenericParameterType("T");
                    declarationTaskType = typeContext.getOrCreateAsyncTaskType(genericT);
                }
                declarationParams = {declarationTaskType, typeContext.getU64()};
            }

            emit(Mangler::mangleFunction(functionName, declarationParams, "std_async"));
            if (!isVoidTask)
            {
                emit("<");
                emit(toCppType(valueType));
                emit(">");
            }
            emit("(");
            emitReceiver();
            for (size_t i = 0; i < intrinsicFunctionType->paramTypes.size(); ++i)
            {
                emit(", ");
                emitArgumentName(i);
            }
            emit("); })");
            return true;
        }

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

        Ref<sema::Type> resolvedReturnType = unwrapAliasTypeForCodegen(intrinsicFunctionType->returnType);
        const bool returnsStdOption = resolvedReturnType &&
            resolvedReturnType->kind() == sema::TypeKind::Struct &&
            resolvedReturnType.AsFast<sema::StructType>()->name == "Option" &&
            resolvedReturnType.AsFast<sema::StructType>()->scopePath == "std";

        if (returnsStdOption &&
            (node.intrinsicMember == IntrinsicMember::ArrayGet ||
             node.intrinsicMember == IntrinsicMember::DictGet ||
             node.intrinsicMember == IntrinsicMember::StringGet))
        {
            Ref<sema::Type> valueType = nullptr;
            Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);
            if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Array)
                valueType = resolvedReceiverType.AsFast<sema::ArrayType>()->elementType;
            else if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Dictionary)
                valueType = resolvedReceiverType.AsFast<sema::DictionaryType>()->valueType;
            else if (resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Primitive &&
                     resolvedReceiverType.AsFast<sema::PrimitiveType>()->name == "string")
                valueType = Compiler::get().getTypeContext().getChar();
            if (!valueType)
                return false;

            const std::string cppValueType = toCppType(valueType);
            if (node.intrinsicMember == IntrinsicMember::StringGet)
            {
                emit("wio::String _wio_values = wio::String(");
                emitReceiver();
                emit(")");
            }
            else
            {
                emit("auto&& _wio_values = ");
                emitReceiver();
            }
            if (node.intrinsicMember == IntrinsicMember::DictGet)
            {
                emit("; auto _wio_it = _wio_values.find(_wio_arg0); ");
                emit("if (_wio_it != _wio_values.end()) return _WF_std_Some_T<" + cppValueType + ">(_wio_it->second); ");
            }
            else
            {
                emit("; if (_wio_arg0 < _wio_values.size()) return _WF_std_Some_T<" + cppValueType + ">(wio::intrinsics::Index(_wio_values, _wio_arg0)); ");
            }
            emit("return _WF_std_None<" + cppValueType + ">(); })");
            return true;
        }

        if (node.intrinsicMember == IntrinsicMember::StringGet)
        {
            emit("return wio::intrinsics::Index(wio::String(");
            emitReceiver();
            emit("), _wio_arg0); })");
            return true;
        }

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
                objSym->kind == sema::SymbolKind::Struct ||
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
        if (node.propagateResult)
        {
            const auto payloadType = node.refType.Lock();
            const std::string payloadCppType = payloadType ? toCppType(payloadType) : "void";

            emit("([&]() -> " + payloadCppType + " ");
            emitLine("{");
            indent();

            const bool previousPropagateResult = node.propagateResult;
            node.propagateResult = false;

            EMIT_TABS();
            emit("auto _wio_result = ");
            visit(node);
            emit(";\n");

            node.propagateResult = previousPropagateResult;

            emitLine("if (_wio_result->_WF_IsError())");
            indent();
            emitLine("throw _wio_result->_WF_ErrorValue();");
            dedent();

            EMIT_TABS();
            emit("return _wio_result->_WF_Unwrap();\n");

            dedent();
            EMIT_TABS();
            emit("}())");
            return;
        }

        auto beginResultUnwrap = [&]()
        {
            if (node.unwrapResult)
                emit("(");
        };

        auto endResultUnwrap = [&]()
        {
            if (node.unwrapResult)
                emit(")->_WF_Unwrap()");
        };

        if (const auto* memberAccess = node.callee ? node.callee->as<MemberAccessExpression>() : nullptr;
            memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
        {
            Ref<sema::Type> receiverType = unwrapAliasType(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
            while (receiverType && receiverType->kind() == sema::TypeKind::Reference)
                receiverType = unwrapAliasType(receiverType.AsFast<sema::ReferenceType>()->referredType);

            if (node.explicitTypeArguments.empty())
            {
                emit("/* invalid PackToStaticArray call */");
                return;
            }

            if (receiverType && receiverType->kind() == sema::TypeKind::GenericParameterPack)
            {
                const std::string packTypeName = receiverType.AsFast<sema::GenericParameterPackType>()->name;
                auto packValueSymbol = memberAccess->object ? memberAccess->object->referencedSymbol.Lock() : nullptr;
                const std::string packValueName =
                    packValueSymbol &&
                    packValueSymbol->flags.get_isParameterPack() &&
                    (packValueSymbol->kind == sema::SymbolKind::Parameter || packValueSymbol->kind == sema::SymbolKind::Variable)
                        ? sanitizeCppIdentifier(packValueSymbol->name)
                        : packTypeName;
                beginResultUnwrap();
                emit("wio::meta::PackToStaticArray<");
                node.explicitTypeArguments.front()->accept(*this);
                emit(common::formatString(">({}...)", packValueName));
                endResultUnwrap();
                return;
            }

            beginResultUnwrap();
            emit("(");
            memberAccess->object->accept(*this);
            emit(").template ToStaticArray<");
            node.explicitTypeArguments.front()->accept(*this);
            emit(">()");
            endResultUnwrap();
            return;
        }

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isCallOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member
                    ? node.arguments.size()
                    : node.arguments.size() + 1u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            beginResultUnwrap();
            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.callee);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("(");
                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    if (operatorFunctionType && i < operatorFunctionType->paramTypes.size())
                        emitExpressionWithExpectedType(node.arguments[i], operatorFunctionType->paramTypes[i], true);
                    else
                        emitReadableExpression(node.arguments[i]);

                    if (i + 1 < node.arguments.size())
                        emit(", ");
                }
                emit("))");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.callee, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.callee);

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    emit(", ");
                    if (operatorFunctionType && i + 1 < operatorFunctionType->paramTypes.size())
                        emitExpressionWithExpectedType(node.arguments[i], operatorFunctionType->paramTypes[i + 1], true);
                    else
                        emitReadableExpression(node.arguments[i]);
                }
                emit(")");
            }
            endResultUnwrap();
            return;
        }

        auto calleeType = node.callee->refType.Lock();
        auto calleeSym = node.callee->referencedSymbol.Lock();

        Ref<sema::Type> resolvedConstructorType = unwrapAliasType(calleeType);
        if (resolvedConstructorType && resolvedConstructorType->kind() == sema::TypeKind::Struct)
        {
            auto structType = resolvedConstructorType.AsFast<sema::StructType>();
            beginResultUnwrap();
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
            endResultUnwrap();
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
                else if (calleeSym->flags.get_isExtension())
                {
                    shouldEmitDirectFunctionCallee = true;
                }
            }
        }

        beginResultUnwrap();
        if (shouldEmitDirectFunctionCallee)
        {
            std::string scopePath = calleeSym->scopePath;
            if (scopePath.empty() && calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                scopePath = calleeSym->overloads.front()->scopePath;
            if (isStructMemberFunctionSymbol(calleeSym))
                scopePath.clear();

            Ref<sema::Symbol> extensionImplementation =
                calleeSym->extensionImplementation ? calleeSym->extensionImplementation : calleeSym;
            Ref<sema::FunctionType> mangledFunctionType =
                calleeSym->flags.get_isExtension() && extensionImplementation->type &&
                extensionImplementation->type->kind() == sema::TypeKind::Function
                    ? extensionImplementation->type.AsFast<sema::FunctionType>()
                    : functionType;
            if (calleeSym->flags.get_isExtension() && mangledFunctionType && !mangledFunctionType->hasParameterPack)
            {
                const size_t emittedParameterCount = std::min(
                    mangledFunctionType->paramTypes.size(),
                    node.arguments.size() + 1);
                mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                    mangledFunctionType->returnType,
                    getLeadingParameterTypes(mangledFunctionType, emittedParameterCount)
                ).AsFast<sema::FunctionType>();
            }
            if (!calleeSym->genericParameterNames.empty())
            {
                Ref<sema::Type> declarationType = calleeSym->flags.get_isExtension()
                    ? extensionImplementation->type
                    : calleeSym->type;
                if ((!declarationType || declarationType->kind() != sema::TypeKind::Function) &&
                    calleeSym->kind == sema::SymbolKind::FunctionGroup && !calleeSym->overloads.empty())
                {
                    declarationType = calleeSym->overloads.front()->type;
                }

                if (declarationType && declarationType->kind() == sema::TypeKind::Function)
                {
                    auto declarationFunctionType = declarationType.AsFast<sema::FunctionType>();
                    if (declarationFunctionType->hasParameterPack)
                    {
                        mangledFunctionType = declarationFunctionType;
                    }
                    else
                    {
                        const size_t visibleArgumentOffset = calleeSym->flags.get_isExtension() ? 1 : 0;
                        mangledFunctionType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            declarationFunctionType->returnType,
                            getLeadingParameterTypes(
                                declarationFunctionType,
                                node.arguments.size() + visibleArgumentOffset)
                        ).AsFast<sema::FunctionType>();
                    }
                }
            }

            if (calleeSym->flags.get_isDerived())
            {
                emit("wio::runtime::Ref<" + calleeSym->derivedProcessorCppType + ">::Create()->");
                emit(Mangler::mangleFunction(
                    extensionImplementation->name,
                    mangledFunctionType->paramTypes));
            }
            else
            {
                emit(Mangler::mangleFunction(calleeSym->name, mangledFunctionType->paramTypes, scopePath));
            }
        }
        else
        {
            node.callee->accept(*this);
        }
        if (!node.resolvedGenericArguments.empty())
        {
            emit("<");
            for (size_t i = 0; i < node.resolvedGenericArguments.size(); ++i)
            {
                emit(toCppType(node.resolvedGenericArguments[i].Lock()));
                if (i + 1 < node.resolvedGenericArguments.size())
                    emit(", ");
            }
            emit(">");
        }
        else if (!node.explicitTypeArguments.empty())
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
        bool emittedExtensionReceiver = false;
        if (calleeSym && calleeSym->flags.get_isExtension())
        {
            if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
            {
                auto implementation = calleeSym->extensionImplementation
                    ? calleeSym->extensionImplementation
                    : calleeSym;
                auto fullType = implementation->type.AsFast<sema::FunctionType>();
                Ref<sema::Type> receiverType =
                    fullType && !fullType->paramTypes.empty() ? fullType->paramTypes.front() : nullptr;
                emitExpressionWithExpectedType(memberAccess->object, receiverType, true);
                emittedExtensionReceiver = true;
            }
        }
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            if (emittedExtensionReceiver || i > 0)
                emit(", ");
            Ref<sema::Type> expectedType = nullptr;
            if (functionType && i < functionType->paramTypes.size())
                expectedType = functionType->paramTypes[i];

            if (node.arguments[i] && node.arguments[i]->is<PackExpansionExpression>())
                node.arguments[i]->accept(*this);
            else
                emitExpressionWithExpectedType(node.arguments[i], expectedType, false);
        }
        emit(")");
        endResultUnwrap();
    }

    void CppGenerator::visit(LambdaExpression& node)
    {
        auto functionType = node.refType.Lock().AsFast<sema::FunctionType>();
        emit("[=");
        if (currentClassIsObject_ && !currentClassName_.empty())
        {
            // C++ captures an implicit `this` as a raw pointer, even with a
            // value-default capture. Keep the Wio object alive for as long as
            // an escaping closure can evaluate `self`.
            emit(", this, _wio_lambda_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this)");
        }
        emit("](");
        
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
        emit(") mutable");

        if (functionType && functionType->returnType && !functionType->returnType->isUnknown())
        {
            emit(" -> ");
            emit(toCppType(functionType->returnType));
        }

        emit(" {\n");
        indent();

        const auto enclosingPostProcessors = currentBehavioralPostProcessors_;
        const auto enclosingFinallyProcessors = currentBehavioralFinallyProcessors_;
        currentBehavioralPostProcessors_.clear();
        currentBehavioralFinallyProcessors_.clear();

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

        currentBehavioralPostProcessors_ = enclosingPostProcessors;
        currentBehavioralFinallyProcessors_ = enclosingFinallyProcessors;

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

        if (auto operatorSymbol = node.referencedSymbol.Lock();
            operatorSymbol &&
            common::isOperatorOverloadName(operatorSymbol->name) &&
            node.operatorDispatchKind != OperatorDispatchKind::None)
        {
            auto operatorFunctionType = node.overloadFunctionType.Lock().AsFast<sema::FunctionType>();
            auto mangledOperatorFunctionType = getMangledCallableFunctionType(
                operatorSymbol,
                operatorFunctionType,
                node.operatorDispatchKind == OperatorDispatchKind::Member ? 0u : 1u
            );

            auto emitOperatorReceiverAndAccess = [&](const NodePtr<Expression>& receiver)
            {
                Ref<sema::Type> receiverType = receiver ? receiver->refType.Lock() : nullptr;
                Ref<sema::Type> resolvedReceiverType = unwrapAliasTypeForCodegen(receiverType);

                bool usePointerAccess =
                    resolvedReceiverType && resolvedReceiverType->kind() == sema::TypeKind::Reference;
                emit("(");
                receiver->accept(*this);
                emit(")");
                emit(usePointerAccess ? "->" : ".");
            };

            if (node.operatorDispatchKind == OperatorDispatchKind::Member)
            {
                emit("(");
                emitOperatorReceiverAndAccess(node.operand);
                emit(Mangler::mangleFunction(operatorSymbol->name, mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{}));
                emit("())");
            }
            else
            {
                emit(Mangler::mangleFunction(operatorSymbol->name,
                                             mangledOperatorFunctionType ? mangledOperatorFunctionType->paramTypes : std::vector<Ref<sema::Type>>{},
                                             operatorSymbol->scopePath));
                emit("(");
                if (operatorFunctionType && !operatorFunctionType->paramTypes.empty())
                    emitExpressionWithExpectedType(node.operand, operatorFunctionType->paramTypes[0], true);
                else
                    emitReadableExpression(node.operand);
                emit(")");
            }
            return;
        }

        if (isAnyTypeForCodegen(srcType))
        {
            Ref<sema::Type> resolvedDestType = unwrapAliasTypeForCodegen(destType);
            if (isOpaqueTypeForCodegen(resolvedDestType))
            {
                emit("(");
                node.operand->accept(*this);
                emit(").AsOpaque()");
                return;
            }

            if (resolvedDestType && resolvedDestType->kind() == sema::TypeKind::Struct)
            {
                auto structType = resolvedDestType.AsFast<sema::StructType>();
                if (structType->isInterface)
                {
                    emit("(");
                    node.operand->accept(*this);
                    emit(").AsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                    return;
                }

                if (structType->isObject && !structType->isInterface)
                {
                    emit("(");
                    node.operand->accept(*this);
                    emit(").CastObject<" + mangleStructTypeName(structType) + ">()");
                    return;
                }
            }

            emit("(");
            node.operand->accept(*this);
            emit(").AsBoxed<" + toCppType(destType) + ">()");
            return;
        }

        Ref<sema::Type> resolvedSrcType = unwrapAliasTypeForCodegen(srcType);
        Ref<sema::Type> resolvedDestType = unwrapAliasTypeForCodegen(destType);
        const bool useNumericFit =
            resolvedSrcType &&
            resolvedSrcType->isNumeric() &&
            resolvedDestType &&
            (resolvedDestType->isNumeric() || resolvedDestType->kind() == sema::TypeKind::GenericParameter);

        if (useNumericFit)
        {
            std::string cppDestType = toCppType(destType);
            emit("wio::FitNumeric<" + cppDestType + ">(");
            emitReadableExpression(node.operand);
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
        emit(currentExtensionMethod_ ? "_wio_self" : "this");
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
            if (matchCase.matchValues.empty() && matchCase.variantName.empty()) // assumed
            {
                emit("else {\n");
            }
            else if (!matchCase.variantName.empty())
            {
                if (!first) emit("else ");
                emit("if (");
                if (matchCase.variantName == "__array")
                    emit("_match_val.size() == " + std::to_string(matchCase.bindings.size()));
                else if (matchCase.variantName == "Some")
                    emit("_match_val->_WF_IsSome()");
                else if (matchCase.variantName == "None")
                    emit("_match_val->_WF_IsNone()");
                else if (matchCase.variantName == "Ok")
                    emit("_match_val->_WF_IsOk()");
                else
                    emit("_match_val->_WF_IsError()");
                if (matchCase.guard)
                {
                    emit(" && [&]() { ");
                    for (size_t bindingIndex = 0; bindingIndex < matchCase.bindings.size(); ++bindingIndex)
                    {
                        auto& binding = matchCase.bindings[bindingIndex];
                        emit("auto " + sanitizeCppIdentifier(binding->token.value) + " = ");
                        if (matchCase.variantName == "__array")
                        {
                            emit("wio::intrinsics::Index(_match_val, " + std::to_string(bindingIndex) + "); ");
                        }
                        else
                        {
                            emit("_match_val->");
                            emit(matchCase.variantName == "Err" ? "_WF_ErrorValue(); " : "_WF_Value(); ");
                        }
                    }
                    emit("return ");
                    matchCase.guard->accept(*this);
                    emit("; }()");
                }
                emit(") {\n");
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
                if (matchCase.guard)
                {
                    emit(" && (");
                    matchCase.guard->accept(*this);
                    emit(")");
                }
                emit(") {\n");
            }
            first = false;
            
            indent();

            for (size_t bindingIndex = 0; bindingIndex < matchCase.bindings.size(); ++bindingIndex)
            {
                auto& binding = matchCase.bindings[bindingIndex];
                EMIT_TABS();
                emit("auto " + sanitizeCppIdentifier(binding->token.value) + " = ");
                if (matchCase.variantName == "__array")
                    emit("wio::intrinsics::Index(_match_val, " + std::to_string(bindingIndex) + ");\n");
                else
                {
                    emit("_match_val->");
                    emit(matchCase.variantName == "Err" ? "_WF_ErrorValue();\n" : "_WF_Value();\n");
                }
            }
            
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

    void CppGenerator::visit(AttributeDeclaration& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(DeclarationGroup& node)
    {
        for (auto& declaration : node.declarations)
            if (declaration) declaration->accept(*this);
    }

    void CppGenerator::visit(VariableDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto sym = node.name->referencedSymbol.Lock();

        Ref<sema::Type> varType = (sym && sym->type) ? sym->type : node.name->refType.Lock();
        std::string typeStr = toCppType(varType);
        Ref<sema::Type> resolvedVarType = unwrapAliasType(varType);
        if (resolvedVarType &&
            resolvedVarType->kind() == sema::TypeKind::Array &&
            node.initializer)
        {
            auto arrayType = resolvedVarType.AsFast<sema::ArrayType>();
            if (arrayType->arrayKind == sema::ArrayType::ArrayKind::Static &&
                arrayType->size == 0)
            {
                if (const auto* functionCall = node.initializer->as<FunctionCallExpression>())
                {
                    if (const auto* memberAccess = functionCall->callee ? functionCall->callee->as<MemberAccessExpression>() : nullptr;
                        memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
                    {
                        typeStr = "auto";
                    }
                }
            }
        }

        if (typeStr.empty())
            typeStr = "auto";

        std::string prefix;
        std::string suffix;

        if (node.mutability == Mutability::Const)
        {
            std::function<bool(const NodePtr<Expression>&)> containsRuntimeTextValue;
            containsRuntimeTextValue = [&](const NodePtr<Expression>& expression) -> bool
            {
                if (!expression)
                    return false;

                Ref<sema::Type> expressionType = unwrapAliasTypeForCodegen(expression->refType.Lock());
                if (expressionType && expressionType->kind() == sema::TypeKind::Primitive)
                {
                    const std::string& name = expressionType.AsFast<sema::PrimitiveType>()->name;
                    if (name == "string" || name == "text")
                        return true;
                }

                if (const auto* binary = expression->as<BinaryExpression>())
                    return containsRuntimeTextValue(binary->left) || containsRuntimeTextValue(binary->right);
                if (const auto* unary = expression->as<UnaryExpression>())
                    return containsRuntimeTextValue(unary->operand);
                if (const auto* fit = expression->as<FitExpression>())
                    return containsRuntimeTextValue(fit->operand);
                return false;
            };

            const bool requiresRuntimeConstStorage =
                containsRuntimeTextValue(node.initializer) ||
                (resolvedVarType &&
                 resolvedVarType->kind() == sema::TypeKind::Primitive &&
                 (resolvedVarType.AsFast<sema::PrimitiveType>()->name == "string" ||
                  resolvedVarType.AsFast<sema::PrimitiveType>()->name == "text"));
            if (requiresRuntimeConstStorage)
                prefix = currentClassName_.empty() ? "const " : "inline static const ";
            else
                prefix = currentClassName_.empty() ? "constexpr " : "static constexpr ";
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
            emitExpressionWithExpectedType(node.initializer, varType, false);
        }
        else
        {
            // Explicitly typed declarations without an initializer use value
            // initialization, keeping scalar defaults deterministic as well.
            buffer_ << "{}";
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
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
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
        Ref<sema::Type> previousFunctionReturnType = currentFunctionReturnType_;
        bool previousFunctionIsAsync = currentFunctionIsAsync_;
        currentFunctionReturnType_ = funcType ? funcType->returnType : nullptr;
        currentFunctionIsAsync_ = node.isAsync;
        if (node.isAsync)
        {
            Ref<sema::Type> resolvedTaskType = unwrapAliasTypeForCodegen(currentFunctionReturnType_);
            currentFunctionReturnType_ = resolvedTaskType && resolvedTaskType->kind() == sema::TypeKind::AsyncTask
                ? resolvedTaskType.AsFast<sema::AsyncTaskType>()->valueType
                : nullptr;
        }
        auto instantiationTypeLists = getInstantiateTypeLists(node);

        std::string returnType = funcType->returnType ? toCppType(funcType->returnType) : "void";
        std::string funcName = node.name->token.value;
        bool isNative = isNativeFunction(node);
        bool isExported = isExportedFunction(node);
        bool hasModuleLifecycleExport = getModuleLifecycleAttribute(node).has_value();
        bool emitsExportWrapper = isExported || hasModuleLifecycleExport;

        struct BehavioralProcessorInstance
        {
            std::string phase;
            std::string cppTypeName;
            std::string hookCppName;
            std::string hookMode;
            Ref<sema::Type> hookValueType;
            std::string variableName;
            std::string finalizedFlagName;
        };
        std::vector<BehavioralProcessorInstance> behavioralProcessors;
        if (!isEmittingPrototypes_ && node.body && !isNative)
        {
            std::vector<const AttributeStatement*> orderedAttributes;
            orderedAttributes.reserve(node.attributes.size());
            for (const auto& attribute : node.attributes)
                if (attribute)
                    orderedAttributes.push_back(attribute.Get());
            std::ranges::stable_sort(
                orderedAttributes,
                {},
                &AttributeStatement::processorOrder);
            for (const auto* attribute : orderedAttributes)
            {
                for (const auto& processor : attribute->processorBindings)
                {
                    if ((processor.phase != "pre" && processor.phase != "post" &&
                         processor.phase != "finally" && processor.phase != "around") ||
                        processor.cppTypeName.empty() || processor.hookCppName.empty())
                    {
                        continue;
                    }
                    if (node.isAsync && processor.phase == "around")
                        continue;
                    const size_t index = behavioralProcessors.size();
                    behavioralProcessors.push_back(BehavioralProcessorInstance{
                        .phase = processor.phase,
                        .cppTypeName = processor.cppTypeName,
                        .hookCppName = processor.hookCppName,
                        .hookMode = processor.hookMode,
                        .hookValueType = processor.hookValueType.Lock(),
                        .variableName = "_wio_attribute_processor_" + std::to_string(index),
                        .finalizedFlagName = "_wio_attribute_finalized_" + std::to_string(index)
                    });
                }
            }
        }

        if (funcName == "Entry" && !node.isAsync &&
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
            auto bindings = buildExtendedGenericBindings(sym->genericParameterNames, sym->hasGenericParameterPack, instantiationTypes);
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
                if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                    emit("...");
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

        auto getWrapperParameterTypes = [&](size_t providedFixedParameterCount) -> std::vector<Ref<sema::Type>>
        {
            std::vector<Ref<sema::Type>> parameterTypes;
            if (!funcType)
                return parameterTypes;

            parameterTypes.reserve(providedFixedParameterCount + (funcType->hasParameterPack ? 1 : 0));
            for (size_t i = 0; i < providedFixedParameterCount && i < funcType->paramTypes.size(); ++i)
                parameterTypes.push_back(funcType->paramTypes[i]);

            if (funcType->hasParameterPack && !funcType->paramTypes.empty())
                parameterTypes.push_back(funcType->paramTypes.back());

            return parameterTypes;
        };

        auto emitWrapperParameters = [&](size_t providedFixedParameterCount)
        {
            for (size_t i = 0; i < providedFixedParameterCount; ++i)
            {
                emit(common::formatString(
                    "{} {}",
                    toCppType(funcType->paramTypes[i]),
                    sanitizeCppIdentifier(node.parameters[i].name->token.value)
                ));
                if (i + 1 < providedFixedParameterCount || funcType->hasParameterPack)
                    emit(", ");
            }

            if (funcType->hasParameterPack && !node.parameters.empty())
            {
                const size_t packParameterIndex = node.parameters.size() - 1;
                emit(common::formatString(
                    "{} {}...",
                    toCppType(funcType->paramTypes.back()),
                    sanitizeCppIdentifier(node.parameters[packParameterIndex].name->token.value)
                ));
            }
        };

        auto emitForwardingCallArguments = [&](size_t providedFixedArgumentCount)
        {
            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                if (i > 0)
                    emit(", ");

                if (funcType->hasParameterPack && i + 1 == node.parameters.size())
                {
                    emit(sanitizeCppIdentifier(node.parameters[i].name->token.value) + "...");
                }
                else if (i < providedFixedArgumentCount)
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
            if ((!node.body && !isNative) || !hasDefaultParameters(node) || (funcType && funcType->hasParameterPack))
                return;

            const size_t requiredParameterCount = getRequiredParameterCount(node);
            const size_t fixedParameterCount = getFixedParameterCount(node);

            if (funcName == "OnConstruct")
            {
                for (size_t wrapperArity = requiredParameterCount; wrapperArity < fixedParameterCount; ++wrapperArity)
                {
                    EMIT_TABS();
                    emit(currentClassName_ + "(");
                    emitWrapperParameters(wrapperArity);
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

            for (size_t wrapperArity = requiredParameterCount; wrapperArity < fixedParameterCount; ++wrapperArity)
            {
                const auto wrapperParameterTypes = getWrapperParameterTypes(wrapperArity);
                const std::string wrapperSymbol = currentClassName_.empty()
                    ? Mangler::mangleFunction(funcName, wrapperParameterTypes, scopePath)
                    : Mangler::mangleFunction(funcName, wrapperParameterTypes);

                if (!node.genericParameters.empty())
                {
                    EMIT_TABS();
                    emit("template <");
                    for (size_t i = 0; i < node.genericParameters.size(); ++i)
                    {
                        const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                        emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                        if (i + 1 < node.genericParameters.size())
                            emit(", ");
                    }
                    emitLine(">");
                }

                EMIT_TABS();
                emit(returnType + " " + wrapperSymbol + "(");
                emitWrapperParameters(wrapperArity);
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
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
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
                std::string parameterType = toCppType(param.name->refType.Lock());
                if (param.isParameterPack && parameterType.ends_with("..."))
                    parameterType = parameterType.substr(0, parameterType.size() - 3) + "...";
                emit(common::formatString("{} {}", parameterType, sanitizeCppIdentifier(param.name->token.value)));
                if (isEmittingPrototypes_ && funcType && funcType->hasParameterPack && param.defaultValue && !param.isParameterPack)
                {
                    emit(" = ");
                    param.defaultValue->accept(*this);
                }
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
            const bool isNativeMember = !currentClassName_.empty();
            std::function<std::string(const Ref<sema::Type>&)> getNativePodCppTypeName;
            std::function<std::string(const Ref<sema::StructType>&)> getNativePodCppName;

            getNativePodCppName = [&](const Ref<sema::StructType>& structType) -> std::string
            {
                if (!structType)
                    return {};

                std::string cppName = structType->nativeCppName.empty() ? structType->name : structType->nativeCppName;
                if (structType->genericArguments.empty())
                    return cppName;

                cppName += "<";
                for (size_t genericIndex = 0; genericIndex < structType->genericArguments.size(); ++genericIndex)
                {
                    if (genericIndex > 0)
                        cppName += ", ";
                    cppName += getNativePodCppTypeName(structType->genericArguments[genericIndex]);
                }
                cppName += ">";
                return cppName;
            };

            getNativePodCppTypeName = [&](const Ref<sema::Type>& type) -> std::string
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType)
                    return "void";

                if (resolvedType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedType.AsFast<sema::StructType>();
                    if (structType && !structType->isObject && !structType->isInterface && structType->isNativePodComponent)
                        return getNativePodCppName(structType);
                }

                return resolvedType->toCppString();
            };

            auto isDirectStringNativeInteropType = [](const Ref<sema::Type>& type) -> bool
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Primitive)
                    return false;

                auto primitiveType = resolvedType.AsFast<sema::PrimitiveType>();
                return primitiveType && primitiveType->name == "string";
            };

            auto buildNativeReferenceSignatureType = [](const std::string& referredCppType, bool isMutable) -> std::string
            {
                if (isMutable)
                    return referredCppType + "&";

                if (!referredCppType.empty() && referredCppType.back() == '*')
                    return referredCppType + " const&";

                return "const " + referredCppType + "&";
            };

            auto shouldUseNativeReferenceWrapper = [](const Ref<sema::Type>& type) -> bool
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                    return false;

                auto refType = resolvedType.AsFast<sema::ReferenceType>();
                auto referredType = unwrapAliasTypeForCodegen(refType->referredType);
                if (!referredType || referredType->kind() != sema::TypeKind::Struct)
                    return true;

                auto structType = referredType.AsFast<sema::StructType>();
                if (structType->isObject || structType->isInterface || structType->isNativePodComponent)
                    return false;

                return true;
            };

            auto buildNativeReferencePreferredExpr = [&](const std::string& expr, const Ref<sema::Type>& type) -> std::string
            {
                auto resolvedType = unwrapAliasTypeForCodegen(type);
                if (!resolvedType || resolvedType->kind() != sema::TypeKind::Reference)
                    return expr;

                return "*" + expr;
            };

            std::function<std::string(const std::string&, const Ref<sema::Type>&, bool)> buildWioToNativePodExpr;
            buildWioToNativePodExpr = [&](const std::string& expr, const Ref<sema::Type>& sourceType, bool sourceIsPointer) -> std::string
            {
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(sourceType);
                if (!nativeStruct)
                    return expr;

                if (usesNativePodAliasModelForCodegen(nativeStruct))
                    return sourceIsPointer ? "*" + expr : expr;

                std::string result = getNativePodCppName(nativeStruct) + "{";
                for (size_t fieldIndex = 0; fieldIndex < nativeStruct->fieldNames.size(); ++fieldIndex)
                {
                    const std::string wioFieldName = sanitizeCppIdentifier(nativeStruct->fieldNames[fieldIndex]);
                    const std::string fieldExpr = expr + (sourceIsPointer ? "->" : ".") + wioFieldName;
                    result += buildWioToNativePodExpr(fieldExpr, nativeStruct->fieldTypes[fieldIndex], false);
                    if (fieldIndex + 1 < nativeStruct->fieldNames.size())
                        result += ", ";
                }
                result += "}";
                return result;
            };

            std::function<void(const std::string&, const Ref<sema::Type>&, const std::string&, bool)> emitNativePodCopyBack;
            emitNativePodCopyBack = [&](const std::string& destinationExpr,
                                        const Ref<sema::Type>& destinationType,
                                        const std::string& sourceExpr,
                                        bool destinationIsPointer)
            {
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(destinationType);
                if (!nativeStruct)
                    return;

                if (usesNativePodAliasModelForCodegen(nativeStruct))
                {
                    EMIT_TABS();
                    emitLine((destinationIsPointer ? "*" + destinationExpr : destinationExpr) + " = " + sourceExpr + ";");
                    return;
                }

                for (size_t fieldIndex = 0; fieldIndex < nativeStruct->fieldNames.size(); ++fieldIndex)
                {
                    const std::string wioFieldName = sanitizeCppIdentifier(nativeStruct->fieldNames[fieldIndex]);
                    const std::string nativeFieldName = nativeStruct->fieldNames[fieldIndex];
                    const std::string destinationFieldExpr = destinationExpr + (destinationIsPointer ? "->" : ".") + wioFieldName;
                    const std::string sourceFieldExpr = sourceExpr + "." + nativeFieldName;
                    auto nestedNativeStruct = getNativePodComponentStructTypeForCodegen(nativeStruct->fieldTypes[fieldIndex]);
                    if (nestedNativeStruct)
                    {
                        emitNativePodCopyBack(destinationFieldExpr, nativeStruct->fieldTypes[fieldIndex], sourceFieldExpr, false);
                    }
                    else
                    {
                        EMIT_TABS();
                        emitLine(destinationFieldExpr + " = " + sourceFieldExpr + ";");
                    }
                }
            };

            struct NativePreparedArgument
            {
                std::string callExpr;
                std::string preferredCallExpr;
                std::string fallbackCallExpr;
                std::string mutableTargetName;
                Ref<sema::Type> mutableTargetType = nullptr;
                bool mutableTargetIsPointer = false;
                bool usesReferenceDispatch = false;
                std::string signatureType;
                std::string preferredSignatureType;
                std::string fallbackSignatureType;
            };

            std::vector<NativePreparedArgument> preparedArguments;
            preparedArguments.reserve(node.parameters.size() + (isNativeMember ? 1 : 0));
            bool usesNativeReferenceWrappers = false;

            emitLine();
            emitLine("{");
            indent();
            emitLine("try {");
            indent();

            if (isNativeMember)
            {
                NativePreparedArgument selfArgument;
                selfArgument.callExpr = "this";
                selfArgument.preferredCallExpr = "this";
                selfArgument.fallbackCallExpr = "this";
                selfArgument.signatureType = currentClassName_ + "*";
                selfArgument.preferredSignatureType = selfArgument.signatureType;
                selfArgument.fallbackSignatureType = selfArgument.signatureType;
                preparedArguments.push_back(std::move(selfArgument));
            }

            for (size_t i = 0; i < node.parameters.size(); ++i)
            {
                const std::string parameterName = sanitizeCppIdentifier(node.parameters[i].name->token.value);
                auto parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : nullptr;
                auto resolvedParameterType = unwrapAliasTypeForCodegen(parameterType);
                auto nativeStruct = getNativePodComponentStructTypeForCodegen(parameterType);
                const bool usesNativePodAliasModel =
                    nativeStruct && usesNativePodAliasModelForCodegen(nativeStruct);

                if (!nativeStruct)
                {
                    if (isDirectStringNativeInteropType(parameterType))
                    {
                        preparedArguments.push_back({
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            common::formatString("wio::intrinsics::NativeStringArg({})", parameterName),
                            "",
                            nullptr,
                            false,
                            false,
                            toCppType(parameterType),
                            toCppType(parameterType),
                            toCppType(parameterType)
                        });
                        continue;
                    }

                    if (shouldUseNativeReferenceWrapper(parameterType))
                    {
                        auto referenceType = resolvedParameterType.AsFast<sema::ReferenceType>();
                        const std::string referredCppType = toCppType(referenceType->referredType);
                        const std::string preferredSignatureType =
                            buildNativeReferenceSignatureType(referredCppType, referenceType->isMutable);
                        const std::string fallbackSignatureType = toCppType(parameterType);
                        preparedArguments.push_back({
                            parameterName,
                            buildNativeReferencePreferredExpr(parameterName, parameterType),
                            parameterName,
                            "",
                            nullptr,
                            false,
                            true,
                            fallbackSignatureType,
                            preferredSignatureType,
                            fallbackSignatureType
                        });
                        usesNativeReferenceWrappers = true;
                        continue;
                    }

                    preparedArguments.push_back({
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        parameterName + (node.parameters[i].isParameterPack ? "..." : ""),
                        "",
                        nullptr,
                        false,
                        false,
                        toCppType(parameterType),
                        toCppType(parameterType),
                        toCppType(parameterType)
                    });
                    continue;
                }

                if (usesNativePodAliasModel)
                {
                    const bool isReferenceParameter =
                        resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference;
                    const bool isMutableReference =
                        isReferenceParameter && resolvedParameterType.AsFast<sema::ReferenceType>()->isMutable;
                    const bool usesExtensionReceiverDispatch =
                        isReferenceParameter && node.isExtensionMethod && i == 0;
                    const std::string nativeSignatureType = getNativePodCppName(nativeStruct);

                    NativePreparedArgument preparedArgument;
                    preparedArgument.callExpr = isReferenceParameter && !usesExtensionReceiverDispatch
                        ? "*" + parameterName
                        : parameterName;
                    preparedArgument.preferredCallExpr = isReferenceParameter ? "*" + parameterName : parameterName;
                    preparedArgument.fallbackCallExpr = usesExtensionReceiverDispatch
                        ? parameterName
                        : preparedArgument.preferredCallExpr;
                    preparedArgument.mutableTargetName = "";
                    preparedArgument.mutableTargetType = nullptr;
                    preparedArgument.mutableTargetIsPointer = false;
                    preparedArgument.usesReferenceDispatch = usesExtensionReceiverDispatch;
                    preparedArgument.signatureType = isReferenceParameter
                        ? (isMutableReference ? nativeSignatureType + "&" : "const " + nativeSignatureType + "&")
                        : nativeSignatureType;
                    preparedArgument.preferredSignatureType = preparedArgument.signatureType;
                    preparedArgument.fallbackSignatureType = usesExtensionReceiverDispatch
                        ? (isMutableReference ? nativeSignatureType + "*" : "const " + nativeSignatureType + "*")
                        : preparedArgument.signatureType;
                    preparedArguments.push_back(std::move(preparedArgument));
                    usesNativeReferenceWrappers = usesNativeReferenceWrappers || usesExtensionReceiverDispatch;
                    continue;
                }

                const std::string nativeTempName = common::formatString("_wio_native_arg{}", i);
                EMIT_TABS();
                emitLine(getNativePodCppName(nativeStruct) + " " + nativeTempName + " = " + buildWioToNativePodExpr(
                    parameterName,
                    parameterType,
                    resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference
                ) + ";");

                bool isMutableReference = false;
                if (resolvedParameterType && resolvedParameterType->kind() == sema::TypeKind::Reference)
                    isMutableReference = resolvedParameterType.AsFast<sema::ReferenceType>()->isMutable;

                NativePreparedArgument preparedArgument;
                preparedArgument.callExpr = nativeTempName;
                preparedArgument.preferredCallExpr = nativeTempName;
                preparedArgument.fallbackCallExpr = nativeTempName;
                preparedArgument.mutableTargetName = isMutableReference ? parameterName : "";
                preparedArgument.mutableTargetType = isMutableReference ? parameterType : nullptr;
                preparedArgument.mutableTargetIsPointer = isMutableReference;
                preparedArgument.signatureType = getNativePodCppName(nativeStruct);
                preparedArgument.preferredSignatureType = preparedArgument.signatureType;
                preparedArgument.fallbackSignatureType = preparedArgument.signatureType;
                preparedArguments.push_back(std::move(preparedArgument));
            }

            const auto nativeReturnStruct = getNativePodComponentStructTypeForCodegen(funcType->returnType);
            const bool returnsNativePodComponent = static_cast<bool>(nativeReturnStruct);
            const bool returnsNativePodAliasComponent =
                nativeReturnStruct && usesNativePodAliasModelForCodegen(nativeReturnStruct);
            const bool emitExplicitNativeTemplateArguments =
                !node.genericParameters.empty() &&
                [&]() -> bool
                {
                    auto cppNameArg = getSingleAttributeArg(node.attributes, Attribute::CppName);
                    if (!cppNameArg.has_value())
                        return false;

                    if (funcType && funcType->paramTypes.empty())
                        return true;

                    return cppNameArg->value == "wio::runtime::EnumCount" ||
                           cppNameArg->value == "wio::runtime::EnumName" ||
                           cppNameArg->value == "wio::runtime::EnumValue" ||
                           cppNameArg->value == "wio::runtime::EnumIndex" ||
                           cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                           cppNameArg->value == "wio::runtime::EnumSize" ||
                           cppNameArg->value == "wio::runtime::EnumIsValid" ||
                           cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                           cppNameArg->value == "wio::runtime::EnumFromRaw";
                }();

            auto emitNativeSymbolInvocationTarget = [&]()
            {
                emit(nativeSymbol);
                if (emitExplicitNativeTemplateArguments)
                {
                    emit("<");
                    for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
                    {
                        emit(node.genericParameters[genericIndex]->token.value);
                        if (node.hasGenericParameterPack && genericIndex + 1 == node.genericParameters.size())
                            emit("...");
                        if (genericIndex + 1 < node.genericParameters.size())
                            emit(", ");
                    }
                    emit(">");
                }
            };

            auto emitNativeInvocation = [&](bool preferReferenceDispatch)
            {
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");

                    const auto& preparedArgument = preparedArguments[argumentIndex];
                    if (preferReferenceDispatch && preparedArgument.usesReferenceDispatch)
                        emit(preparedArgument.preferredCallExpr);
                    else if (!preferReferenceDispatch && preparedArgument.usesReferenceDispatch)
                        emit(preparedArgument.fallbackCallExpr);
                    else
                        emit(preparedArgument.callExpr);
                }
                emit(")");
            };

            // Keep backend errors in a native symbol expression anchored to the
            // declaration that introduced that symbol. The exception boundary
            // adds generated wrapper lines, so relying on the function-level
            // directive alone would otherwise report non-existent Wio lines.
            emitSourceDirective(node.location());

            if (usesNativeReferenceWrappers)
            {
                EMIT_TABS();
                emit(common::formatString("auto _wio_native_invoke = [&]("));
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(common::formatString("auto&& _wio_native_arg{}", argumentIndex));
                }
                emit(") -> ");
                emit(funcType->returnType && !funcType->returnType->isVoid()
                         ? (returnsNativePodAliasComponent
                                ? returnType
                                : (returnsNativePodComponent ? getNativePodCppName(nativeReturnStruct) : returnType))
                         : "void");
                emitLine(" {");
                indent();
                EMIT_TABS();
                emit("if constexpr (requires { ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    if (preparedArguments[argumentIndex].usesReferenceDispatch)
                    {
                        emit(common::formatString("*_wio_native_arg{}", argumentIndex));
                    }
                    else
                    {
                        emit(common::formatString(
                            "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                            argumentIndex,
                            argumentIndex
                        ));
                    }
                }
                emit("); })");
                emit("\n");
                EMIT_TABS();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    if (preparedArguments[argumentIndex].usesReferenceDispatch)
                    {
                        emit(common::formatString("*_wio_native_arg{}", argumentIndex));
                    }
                    else
                    {
                        emit(common::formatString(
                            "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                            argumentIndex,
                            argumentIndex
                        ));
                    }
                }
                emit(")");
                emit(";");
                emit("\n");
                dedent();
                EMIT_TABS();
                emitLine("}");
                EMIT_TABS();
                emitLine("else");
                EMIT_TABS();
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emitNativeSymbolInvocationTarget();
                emit("(");
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(common::formatString(
                        "std::forward<decltype(_wio_native_arg{})>(_wio_native_arg{})",
                        argumentIndex,
                        argumentIndex
                    ));
                }
                emit(")");
                emit(";");
                emit("\n");
                dedent();
                EMIT_TABS();
                emitLine("}");
                dedent();
                EMIT_TABS();
                emitLine("};");

                EMIT_TABS();
                if (returnsNativePodComponent)
                {
                    emit("auto _wio_native_result = _wio_native_invoke(");
                }
                else if (funcType->returnType && !funcType->returnType->isVoid())
                {
                    emit("return _wio_native_invoke(");
                }
                else
                {
                    emit("_wio_native_invoke(");
                }
                for (size_t argumentIndex = 0; argumentIndex < preparedArguments.size(); ++argumentIndex)
                {
                    if (argumentIndex > 0)
                        emit(", ");
                    emit(preparedArguments[argumentIndex].callExpr);
                }
                emit(");");
            }
            else
            {
                EMIT_TABS();
                if (returnsNativePodComponent)
                {
                    emit("auto _wio_native_result = ");
                }
                else if (funcType->returnType && !funcType->returnType->isVoid())
                {
                    emit("return ");
                }
                emitNativeInvocation(false);
                emit(";");
            }
            emit("\n");
            emitGeneratedDirective();

            for (const auto& preparedArgument : preparedArguments)
            {
                if (preparedArgument.mutableTargetName.empty())
                    continue;

                emitNativePodCopyBack(
                    preparedArgument.mutableTargetName,
                    preparedArgument.mutableTargetType,
                    preparedArgument.callExpr,
                    preparedArgument.mutableTargetIsPointer
                );
            }

            if (returnsNativePodComponent)
            {
                if (returnsNativePodAliasComponent)
                {
                    EMIT_TABS();
                    emitLine("return _wio_native_result;");
                }
                else
                {
                    const std::string wioReturnTypeName = toCppType(funcType->returnType);
                    EMIT_TABS();
                    emitLine(wioReturnTypeName + " _wio_result{};");
                    emitNativePodCopyBack("_wio_result", funcType->returnType, "_wio_native_result", false);
                    EMIT_TABS();
                    emitLine("return _wio_result;");
                }
            }

            dedent();
            emitLine("}");
            emitLine("catch (const wio::runtime::RuntimeException&)");
            emitLine("{");
            indent();
            emitLine("throw;");
            dedent();
            emitLine("}");
            emitLine("catch (const std::exception& ex)");
            emitLine("{");
            indent();
            emitLine(
                "throw wio::runtime::RuntimeException(\"Native call '" +
                common::wioStringToEscapedCppString(nativeSymbol) +
                "' failed: \" + std::string(ex.what()));"
            );
            dedent();
            emitLine("}");
            emitLine("catch (...)");
            emitLine("{");
            indent();
            emitLine(
                "throw wio::runtime::RuntimeException(\"Native call '" +
                common::wioStringToEscapedCppString(nativeSymbol) +
                "' failed with an unknown exception.\");"
            );
            dedent();
            emitLine("}");
            dedent();
            emitLine("}");
        }
        else if (node.body)
        {
            auto emitFunctionBody = [&]()
            {
                if (behavioralProcessors.empty() &&
                    !(node.isAsync && currentClassIsObject_ && node.body->is<BlockStatement>()))
                {
                    node.body->accept(*this);
                    return;
                }

                auto* block = node.body->as<BlockStatement>();
                emitLine("{");
                indent();
                if (node.isAsync && currentClassIsObject_)
                    emitLine("auto _wio_async_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this);");

                for (const auto& processor : behavioralProcessors)
                {
                    emitLine("auto " + processor.variableName + " = wio::runtime::Ref<" +
                             processor.cppTypeName + ">::Create();");
                    if (processor.phase == "finally")
                        emitLine("bool " + processor.finalizedFlagName + " = false;");
                }

                const bool hasFinallyProcessor = std::ranges::any_of(
                    behavioralProcessors,
                    [](const BehavioralProcessorInstance& processor) { return processor.phase == "finally"; });
                const bool hasAroundProcessor = std::ranges::any_of(
                    behavioralProcessors,
                    [](const BehavioralProcessorInstance& processor) { return processor.phase == "around"; });
                if (hasAroundProcessor)
                {
                    emitLine("auto _wio_attribute_core = [&]()" +
                             std::string(currentFunctionReturnType_ && !currentFunctionReturnType_->isVoid()
                                 ? " -> " + toCppType(currentFunctionReturnType_)
                                 : ""));
                    emitLine("{");
                    indent();
                }
                if (hasFinallyProcessor)
                {
                    emitLine("try");
                    emitLine("{");
                    indent();
                }

                for (const auto& processor : behavioralProcessors)
                {
                    if (processor.phase == "pre")
                    {
                        std::string arguments;
                        if (processor.hookMode.starts_with("receiver_any"))
                        {
                            arguments = "wio::runtime::Any::FromObject<" + currentClassName_ +
                                ">(wio::runtime::Ref<" + currentClassName_ + ">(this))";
                        }
                        else if (processor.hookMode.starts_with("receiver_typed"))
                        {
                            arguments = "static_cast<" + toCppType(processor.hookValueType) + ">(this)";
                        }
                        const std::string invocation = processor.variableName + "->" +
                            processor.hookCppName + "(" + arguments + ")";
                        if (processor.hookMode.ends_with("_guard"))
                            emitLine("if (!(" + invocation + ")) " +
                                     std::string(node.isAsync ? "co_return;" : "return;"));
                        else
                            emitLine(invocation + ";");
                    }
                }

                const auto previousPostProcessors = currentBehavioralPostProcessors_;
                const auto previousFinallyProcessors = currentBehavioralFinallyProcessors_;
                currentBehavioralPostProcessors_.clear();
                currentBehavioralFinallyProcessors_.clear();
                for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                {
                    if (processor->phase == "post")
                        currentBehavioralPostProcessors_.push_back(
                            processor->variableName + "->" + processor->hookCppName +
                            (processor->hookMode == "result" ? "({result});" : "();"));
                    else if (processor->phase == "finally")
                    {
                        currentBehavioralFinallyProcessors_.push_back(
                            "if (!" + processor->finalizedFlagName + ") { " + processor->finalizedFlagName +
                            " = true; " + processor->variableName + "->" + processor->hookCppName +
                            (processor->hookMode == "outcome_bool" ? "(true); }" : "(); }"));
                    }
                }

                if (block)
                {
                    for (auto& statement : block->statements)
                        statement->accept(*this);
                }
                else
                {
                    node.body->accept(*this);
                }

                if (currentFunctionReturnType_ && currentFunctionReturnType_->isVoid())
                {
                    for (const std::string& invocation : currentBehavioralPostProcessors_)
                        emitLine(invocation);
                    for (const std::string& invocation : currentBehavioralFinallyProcessors_)
                        emitLine(invocation);
                }

                currentBehavioralPostProcessors_ = previousPostProcessors;
                currentBehavioralFinallyProcessors_ = previousFinallyProcessors;

                if (hasFinallyProcessor)
                {
                    dedent();
                    emitLine("}");
                    emitLine("catch (...)");
                    emitLine("{");
                    indent();
                    for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                    {
                        if (processor->phase == "finally")
                        {
                            emitLine("if (!" + processor->finalizedFlagName + ") { " + processor->finalizedFlagName +
                                     " = true; " + processor->variableName + "->" + processor->hookCppName +
                                     (processor->hookMode == "outcome_bool" ? "(false); }" : "(); }"));
                        }
                    }
                    emitLine("throw;");
                    dedent();
                    emitLine("}");
                }

                if (hasAroundProcessor)
                {
                    dedent();
                    emitLine("};");
                    std::string nextProceed = "_wio_attribute_core";
                    size_t aroundIndex = 0;
                    for (auto processor = behavioralProcessors.rbegin(); processor != behavioralProcessors.rend(); ++processor)
                    {
                        if (processor->phase != "around")
                            continue;
                        const std::string wrapperName = "_wio_attribute_around_" + std::to_string(aroundIndex);
                        const std::string stateName = "_wio_attribute_proceed_state_" + std::to_string(aroundIndex);
                        const bool returnsResult = processor->hookMode == "proceed_result";
                        const std::string aroundResultType = returnsResult
                            ? toCppType(currentFunctionReturnType_)
                            : std::string("void");
                        emitLine("auto " + wrapperName + " = [&]()" +
                                 (returnsResult ? " -> " + aroundResultType : ""));
                        emitLine("{");
                        indent();
                        emitLine("auto " + stateName + " = std::make_shared<std::pair<bool, bool>>(true, false);");
                        emitLine("try");
                        emitLine("{");
                        indent();
                        emitLine(std::string(returnsResult ? "return " : "") +
                                 processor->variableName + "->" + processor->hookCppName +
                                 "(std::function<" + aroundResultType + "()>([&, " + stateName + "]()" +
                                 (returnsResult ? " -> " + aroundResultType : ""));
                        emitLine("{");
                        indent();
                        emitLine("if (!" + stateName + "->first) throw wio::runtime::RuntimeException(\"Attribute Proceed escaped its Around invocation.\");");
                        emitLine("if (" + stateName + "->second) throw wio::runtime::RuntimeException(\"Attribute Proceed may be invoked at most once.\");");
                        emitLine(stateName + "->second = true;");
                        emitLine(std::string(returnsResult ? "return " : "") + nextProceed + "();");
                        dedent();
                        emitLine("}));");
                        dedent();
                        emitLine("}");
                        emitLine("catch (...)");
                        emitLine("{");
                        indent();
                        emitLine(stateName + "->first = false;");
                        emitLine("throw;");
                        dedent();
                        emitLine("}");
                        emitLine(stateName + "->first = false;");
                        dedent();
                        emitLine("};");
                        nextProceed = wrapperName;
                        ++aroundIndex;
                    }
                    emitLine(std::string(currentFunctionReturnType_ && !currentFunctionReturnType_->isVoid()
                        ? "return "
                        : "") + nextProceed + "();");
                }
                dedent();
                emitLine("}");
            };

            const bool catchesResultPropagation = [&]()
            {
                auto resolvedReturnType = unwrapAliasTypeForCodegen(currentFunctionReturnType_);
                if (!resolvedReturnType || resolvedReturnType->kind() != sema::TypeKind::Struct)
                    return false;

                auto structType = resolvedReturnType.AsFast<sema::StructType>();
                return structType &&
                    (structType->name == "Result" || structType->name == "ResultValue") &&
                    structType->scopePath == "std" &&
                    structType->genericArguments.size() == 1;
            }();

            emitLine();

            if (catchesResultPropagation || node.whenCondition)
            {
                emitLine("{");
                indent();
            }

            if (catchesResultPropagation)
            {
                emitLine("try {");
                indent();
            }

            if (node.whenCondition)
            {
                EMIT_TABS();
                emit("if (!(");
                node.whenCondition->accept(*this);
                emit(node.isAsync ? ")) co_return" : ")) return");
                if (node.whenFallback)
                {
                    emit(" ");
                    node.whenFallback->accept(*this);
                }
                emit(";\n\n");
                
                if (node.body->is<BlockStatement>())
                {
                    auto block = node.body->as<BlockStatement>();
                    if (node.isAsync && currentClassIsObject_)
                        emitLine("auto _wio_async_self_guard = wio::runtime::Ref<" + currentClassName_ + ">(this);");
                    for (auto& stmt : block->statements)
                        stmt->accept(*this);
                }
                else
                {
                    emitFunctionBody();
                }
            }
            else
            {
                emitFunctionBody();
            }

            if (catchesResultPropagation)
            {
                dedent();
                emitLine("}");
                const std::string propagationReturnType = toCppType(currentFunctionReturnType_);
                emitLine("catch (const decltype(std::declval<" + propagationReturnType + ">()->_WF_ErrorValue())& _wio_result_error)");
                emitLine("{");
                indent();
                emitLine(std::string(node.isAsync ? "co_return " : "return ") + propagationReturnType + "::Create(_wio_result_error);");
                dedent();
                emitLine("}");
            }

            if (catchesResultPropagation || node.whenCondition)
            {
                dedent();
                emitLine("}");
            }
        }
        else
        {
            emitLine(";\n");
        }

        emitDefaultArgumentWrappers();

        if (!isEmittingPrototypes_ && !currentClassName_.empty() && sym &&
            !sym->overriddenSymbols.empty() && node.genericParameters.empty())
        {
            const std::string implementationName = Mangler::mangleFunction(funcName, funcType->paramTypes);
            std::unordered_set<std::string> emittedBridgeNames;
            for (const auto& overriddenWeak : sym->overriddenSymbols)
            {
                auto overridden = overriddenWeak.Lock();
                auto overriddenType = overridden && overridden->type
                    ? overridden->type.AsFast<sema::FunctionType>()
                    : nullptr;
                if (!overriddenType)
                    continue;

                const std::string bridgeName = Mangler::mangleFunction(funcName, overriddenType->paramTypes);
                if (bridgeName == implementationName || !emittedBridgeNames.insert(bridgeName).second)
                    continue;

                emitGeneratedDirective();
                EMIT_TABS();
                emit("virtual " + returnType + " " + bridgeName + "(");
                for (size_t parameterIndex = 0; parameterIndex < node.parameters.size(); ++parameterIndex)
                {
                    auto& parameter = node.parameters[parameterIndex];
                    emit(common::formatString(
                        "{} {}",
                        toCppType(parameter.name->refType.Lock()),
                        sanitizeCppIdentifier(parameter.name->token.value)
                    ));
                    if (parameterIndex + 1 < node.parameters.size())
                        emit(", ");
                }
                emitLine(") override");
                emitLine("{");
                indent();
                EMIT_TABS();
                if (funcType->returnType && !funcType->returnType->isVoid())
                    emit("return ");
                emit(implementationName + "(");
                for (size_t parameterIndex = 0; parameterIndex < node.parameters.size(); ++parameterIndex)
                {
                    emit(sanitizeCppIdentifier(node.parameters[parameterIndex].name->token.value));
                    if (parameterIndex + 1 < node.parameters.size())
                        emit(", ");
                }
                emitLine(");");
                dedent();
                emitLine("}");
            }
        }

        if (emitsExportWrapper && !isEmittingPrototypes_ && currentClassName_.empty() && node.body)
        {
            emitGeneratedDirective();
            std::string internalSymbol = Mangler::mangleFunction(funcName, funcType->paramTypes, sym ? sym->scopePath : "");
            if (!node.genericParameters.empty() && !instantiationTypeLists.empty())
            {
                const std::string exportBaseSymbol = getExportedCppSymbolName(node);
                const size_t fixedDeclaredParameterCount = getFixedParameterCount(node);
                auto getExportWrapperParameterName = [&](size_t parameterIndex) -> std::string
                {
                    if (parameterIndex < fixedDeclaredParameterCount && parameterIndex < node.parameters.size())
                        return sanitizeCppIdentifier(node.parameters[parameterIndex].name->token.value);

                    if (node.parameters.empty())
                        return common::formatString("arg_{}", parameterIndex);

                    const std::string packBaseName =
                        sanitizeCppIdentifier(node.parameters.back().name->token.value);
                    return common::formatString("{}_{}", packBaseName, parameterIndex - fixedDeclaredParameterCount);
                };

                for (const auto& instantiationTypes : instantiationTypeLists)
                {
                    auto instantiatedFunctionType = instantiateFunctionTypeForCodegen(instantiationTypes);
                    if (!instantiatedFunctionType)
                        continue;

                    emitLine();
                    EMIT_TABS();
                    emit("extern \"C\" WIO_EXPORT " + toCppType(instantiatedFunctionType->returnType) + " " +
                         formatInstantiatedExportSymbolName(exportBaseSymbol, instantiationTypes) + "(");
                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        emit(common::formatString(
                            "{} {}",
                            toCppType(instantiatedFunctionType->paramTypes[i]),
                            getExportWrapperParameterName(i)
                        ));
                        if (i + 1 < instantiatedFunctionType->paramTypes.size()) emit(", ");
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
                    for (size_t i = 0; i < instantiatedFunctionType->paramTypes.size(); ++i)
                    {
                        emit(getExportWrapperParameterName(i));
                        if (i + 1 < instantiatedFunctionType->paramTypes.size()) emit(", ");
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

        if (node.isAsync && funcName == "Entry" && !isEmittingPrototypes_ &&
            node.genericParameters.empty() && Compiler::get().getBuildTarget() == BuildTarget::Executable &&
            (!sym || sym->scopePath.empty()))
        {
            emitMain(node);
        }

        currentFunctionReturnType_ = previousFunctionReturnType;
        currentFunctionIsAsync_ = previousFunctionIsAsync;
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
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
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
        emitLine("virtual wio::runtime::RefCountedObject* _WF_RuntimeObject() noexcept = 0;");
    
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

    void CppGenerator::visit(ExtensionDeclaration& node)
    {
        WIO_UNUSED(node.name);
        WIO_UNUSED(node.targetType);
        const bool previousExtensionMethod = currentExtensionMethod_;
        currentExtensionMethod_ = true;
        for (auto& member : node.members)
        {
            if (member.method && member.method->name->referencedSymbol.Lock())
                member.method->accept(*this);
        }
        currentExtensionMethod_ = previousExtensionMethod;
    }

    void CppGenerator::visit(ComponentDeclaration& node)
        {
        emitSourceDirective(node.location());
        auto componentSym = node.name->referencedSymbol.Lock();
        auto componentType = getStructTypeFromSymbol(componentSym);
        auto enclosingScope = componentSym && componentSym->innerScope ? componentSym->innerScope->getParent().Lock() : nullptr;

        if (componentType && componentType->isExplicitSpecialization &&
            usesNativePodAliasModelForCodegen(componentType))
        {
            return;
        }

        if (componentType && componentType->isExplicitSpecialization && !componentType->isPartialSpecialization)
        {
            EMIT_TABS();
            emitLine("template <>");
        }
        else if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }

        std::string structName = mangleStructTypeName(componentType);
        const std::string declaredClassName = Mangler::mangleStruct(
            componentType ? componentType->name : node.name->token.value,
            componentType ? componentType->scopePath : ""
        );
        if (componentType && usesNativePodAliasModelForCodegen(componentType))
        {
            std::string nativeTypeName = componentType->nativeCppName.empty() ? componentType->name : componentType->nativeCppName;
            if (!node.genericParameters.empty())
            {
                nativeTypeName += "<";
                for (size_t i = 0; i < node.genericParameters.size(); ++i)
                {
                    if (i > 0)
                        nativeTypeName += ", ";

                    nativeTypeName += node.genericParameters[i]->token.value;
                    if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                        nativeTypeName += "...";
                }
                nativeTypeName += ">";
            }

            EMIT_TABS();
            emitLine("using " + structName + " = " + nativeTypeName + ";\n");
            return;
        }

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
    
        auto trustArgs = getFirstAttributeArgs(node.attributes, Attribute::Trust);
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
    
        const bool previousClassIsObject = currentClassIsObject_;
        currentClassIsObject_ = false;
        currentClassName_ = declaredClassName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                if (vDecl->mutability == Mutability::Const)
                    continue;
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
        currentClassIsObject_ = previousClassIsObject;
        dedent();
        emitLine("};\n");
    }
    
    void CppGenerator::visit(ObjectDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto symb = node.name->referencedSymbol.Lock();
        auto objectType = getStructTypeFromSymbol(symb);
        if (objectType && objectType->isExplicitSpecialization && !objectType->isPartialSpecialization)
        {
            EMIT_TABS();
            emitLine("template <>");
        }
        else if (!node.genericParameters.empty())
        {
            EMIT_TABS();
            emit("template <");
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                const bool isGenericParameterPack = node.hasGenericParameterPack && i + 1 == node.genericParameters.size();
                emit(formatCppTemplateParameter(node.genericParameters[i], isGenericParameterPack));
                if (i + 1 < node.genericParameters.size())
                    emit(", ");
            }
            emitLine(">");
        }
        std::string structName = mangleStructTypeName(objectType);
        const std::string declaredClassName = Mangler::mangleStruct(
            objectType ? objectType->name : node.name->token.value,
            objectType ? objectType->scopePath : ""
        );
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
        emitLine("virtual wio::runtime::RefCountedObject* _WF_RuntimeObject() noexcept override { return this; }");
        
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
        if (!node.genericParameters.empty() && !(objectType && objectType->isExplicitSpecialization))
        {
            objectRefFriend += "<";
            for (size_t i = 0; i < node.genericParameters.size(); ++i)
            {
                objectRefFriend += node.genericParameters[i]->token.value;
                if (node.hasGenericParameterPack && i + 1 == node.genericParameters.size())
                    objectRefFriend += "...";
                if (i + 1 < node.genericParameters.size())
                    objectRefFriend += ", ";
            }
            objectRefFriend += ">";
        }
        emitLine("friend class wio::runtime::Ref<" + objectRefFriend + ">;");
        auto trustArgs = getFirstAttributeArgs(node.attributes, Attribute::Trust);
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
    
        const bool previousClassIsObject = currentClassIsObject_;
        currentClassIsObject_ = true;
        currentClassName_ = declaredClassName;
        AccessModifier currentAccess = AccessModifier::Public;
    
        std::vector<std::pair<std::string, std::string>> memberVars;
        for (auto& member : node.members)
        {
            if (member.declaration->is<VariableDeclaration>())
            {
                auto vDecl = member.declaration->as<VariableDeclaration>();
                if (vDecl->mutability == Mutability::Const)
                    continue;
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
        currentClassIsObject_ = previousClassIsObject;
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
        
        auto typeArgs = getFirstAttributeArgs(node.attributes, Attribute::Type);
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

        // Wio enum members live in the enum's semantic scope. Keep the C++
        // representation scoped as well so two enums in one realm may reuse
        // natural member names such as `pending` or `closed`.
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
        emitLine("};\n");
    }

    void CppGenerator::visit(FlagsetDeclaration& node)
    {
        emitSourceDirective(node.location());
        auto flagsetType = getStructTypeFromSymbol(node.name->referencedSymbol.Lock());
        std::string enumName = mangleStructTypeName(flagsetType);

        if (flagsetType && usesNativePodAliasModelForCodegen(flagsetType))
        {
            std::string nativeTypeName = flagsetType->nativeCppName.empty() ? flagsetType->name : flagsetType->nativeCppName;
            EMIT_TABS();
            emitLine("using " + enumName + " = " + nativeTypeName + ";\n");
            return;
        }

        std::string underType = "uint32_t";
        
        auto typeArgs = getFirstAttributeArgs(node.attributes, Attribute::Type);
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
            auto targetType = binExpr->right->refType.Lock();

            if (isAnyTypeForCodegen(binExpr->left->refType.Lock()))
            {
                Ref<sema::Type> resolvedTargetType = unwrapAliasTypeForCodegen(targetType);
                const std::string targetCppType = toCppType(targetType);

                EMIT_TABS();
                emit("if (wio::runtime::Any _wio_any_match = ");
                binExpr->left->accept(*this);
                emit("; ");

                if (resolvedTargetType && resolvedTargetType->kind() == sema::TypeKind::Struct)
                {
                    auto structType = resolvedTargetType.AsFast<sema::StructType>();
                    if (structType->isInterface)
                    {
                        emit("_wio_any_match.IsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsInterface<" + Mangler::mangleInterface(structType->name, structType->scopePath) + ">();");
                    }
                    else if (structType->isObject && !structType->isInterface)
                    {
                        emit("_wio_any_match.CanCastObject<" + mangleStructTypeName(structType) + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.CastObject<" + mangleStructTypeName(structType) + ">();");
                    }
                    else
                    {
                        emit("_wio_any_match.IsBoxed<" + targetCppType + ">()");
                        emit(")\n");
                        emitLine("{");
                        indent();
                        emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsBoxed<" + targetCppType + ">();");
                    }
                }
                else if (isOpaqueTypeForCodegen(resolvedTargetType))
                {
                    emit("_wio_any_match.IsOpaque()");
                    emit(")\n");
                    emitLine("{");
                    indent();
                    emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsOpaque();");
                }
                else
                {
                    emit("_wio_any_match.IsBoxed<" + targetCppType + ">()");
                    emit(")\n");
                    emitLine("{");
                    indent();
                    emitLine(targetCppType + " " + node.matchVar.value + " = _wio_any_match.AsBoxed<" + targetCppType + ">();");
                }

                if (node.thenBranch) node.thenBranch->accept(*this);

                dedent();
                emitLine("}");

                if (node.elseBranch)
                {
                    if (node.elseBranch->is<IfStatement>())
                        emitLine("else");
                    else
                        emit("else ");
                    node.elseBranch->accept(*this);
                }
                return;
            }

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
                if (node.elseBranch->is<IfStatement>())
                    emitLine("else");
                else
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
            if (node.elseBranch->is<IfStatement>())
                emitLine("else");
            else
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

            emitLine("if (_wio_range_step == 0) throw wio::runtime::RuntimeException(\"Range step cannot be zero.\");");

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

            emitLine("if (_wio_array_step <= 0) throw wio::runtime::RuntimeException(\"Array step must be positive.\");");

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
        const bool hasBehavioralExit = !currentBehavioralPostProcessors_.empty() ||
                                       !currentBehavioralFinallyProcessors_.empty();
        if (node.value && hasBehavioralExit)
        {
            const std::string resultName = "_wio_attribute_return_" + std::to_string(behavioralReturnCounter_++);
            EMIT_TABS();
            buffer_ << "auto " << resultName << " = ";
            emitExpressionWithExpectedType(node.value, currentFunctionReturnType_, false);
            buffer_ << ";\n";
            for (std::string invocation : currentBehavioralPostProcessors_)
            {
                if (const size_t marker = invocation.find("{result}"); marker != std::string::npos)
                    invocation.replace(marker, std::string("{result}").size(), resultName);
                emitLine(invocation);
            }
            for (const std::string& invocation : currentBehavioralFinallyProcessors_)
                emitLine(invocation);
            EMIT_TABS();
            buffer_ << (currentFunctionIsAsync_ ? "co_return " : "return ") << resultName << ";\n";
            return;
        }

        for (const std::string& invocation : currentBehavioralPostProcessors_)
            emitLine(invocation);
        for (const std::string& invocation : currentBehavioralFinallyProcessors_)
            emitLine(invocation);

        EMIT_TABS();
        buffer_ << (currentFunctionIsAsync_ ? "co_return" : "return");
        if (node.value)
        {
            buffer_ << " ";
            emitExpressionWithExpectedType(node.value, currentFunctionReturnType_, false);
        }
        buffer_ << ";\n";
    }

    void CppGenerator::visit(UseStatement& node)
    {
        WIO_UNUSED(node);
    }

    void CppGenerator::visit(UsingAttributeStatement& node)
    {
        if (node.body)
            node.body->accept(*this);
    }
}
