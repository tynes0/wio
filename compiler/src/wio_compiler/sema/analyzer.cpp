#include "wio/sema/analyzer.h"

#include <array>
#include "wio/ast/attribute_contract.h"
#include "wio/ast/attribute_queries.h"
#include "wio/ast/declaration_queries.h"
#include <cctype>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_set>

#include "wio/codegen/cpp_identifier.h"
#include "wio/codegen/mangler.h"
#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/common/operator_overload.h"
#include "wio/common/utility.h"
#include "wio/sema/intrinsic_member_resolver.h"
#include "wio/sema/constant_evaluator.h"
#include "wio/sema/generic_support.h"
#include "wio/sema/scope_lookup.h"
#include "wio/sema/type_queries.h"

#include "compiler.h"
namespace wio::sema
{
    namespace
    {
        using attribute_queries::getAllAttributeArgs;
        using attribute_queries::getAttributeStatements;
        using attribute_queries::getFirstAttributeArg;
        using attribute_queries::hasAttribute;
        using codegen::cpp_identifier::isValidCppIdentifier;
        using codegen::cpp_identifier::isValidCppSymbolPath;
        using declaration_queries::getFixedParameterCount;
        using declaration_queries::getRequiredParameterCount;
        using scope_lookup::resolveQualifiedSymbol;
        using generic_support::GenericBindingSet;
        using generic_support::PackElementBindingKind;
        using generic_support::ParsedPackElementBinding;
        using generic_support::buildExtendedGenericBindings;
        using generic_support::buildGenericTypeBindings;
        using generic_support::getMinimumGenericArgumentCount;
        using generic_support::makePackElementBindingName;
        using generic_support::makePackTailElementBindingName;
        using generic_support::tryEvaluatePackIndexBinding;
        using generic_support::tryEvaluateStaticPackIndex;
        using generic_support::tryGetSymbolicPackReferenceName;
        using generic_support::tryGetNormalizedSymbolicPackName;
        using generic_support::tryParsePackElementBindingName;
        using generic_support::tryResolveConcretePackElementIndex;
        using type_queries::getAutoReadableType;
        using type_queries::isSdkValueBridgeType;
        using type_queries::isStdLibraryScopePath;
        using type_queries::shouldAutoReadReferenceType;
        using type_queries::unwrapAliasType;
        std::vector<Attribute> getModuleLifecycleAttributes(const std::vector<NodePtr<AttributeStatement>>& attributes);

        Ref<Type> unwrapTransferType(Ref<Type> type)
        {
            std::unordered_set<const Type*> visited;
            while (type && type->kind() == TypeKind::Alias && visited.insert(type.Get()).second)
                type = type.AsFast<AliasType>()->aliasedType;
            return type;
        }

        bool containsInferredArrayExtent(Ref<Type> type)
        {
            type = unwrapTransferType(std::move(type));
            if (!type || type->kind() != TypeKind::Array)
                return false;

            auto array = type.AsFast<ArrayType>();
            return array->hasInferredExtent || containsInferredArrayExtent(array->elementType);
        }

        bool haveIdenticalFixedArrayShape(Ref<Type> left, Ref<Type> right)
        {
            left = unwrapTransferType(std::move(left));
            right = unwrapTransferType(std::move(right));
            if (!left || !right || left->kind() != TypeKind::Array || right->kind() != TypeKind::Array)
                return true;

            auto leftArray = left.AsFast<ArrayType>();
            auto rightArray = right.AsFast<ArrayType>();
            if (leftArray->arrayKind == ArrayType::ArrayKind::Dynamic ||
                rightArray->arrayKind == ArrayType::ArrayKind::Dynamic)
            {
                return leftArray->arrayKind == rightArray->arrayKind;
            }
            if (!leftArray->hasInferredExtent && !rightArray->hasInferredExtent &&
                !leftArray->extentType && !rightArray->extentType &&
                leftArray->size != rightArray->size)
            {
                return false;
            }
            return haveIdenticalFixedArrayShape(leftArray->elementType, rightArray->elementType);
        }

        bool hasAsyncSafetyMarker(
            const Ref<StructType>& type,
            std::string_view marker,
            std::unordered_set<const Type*>& visited)
        {
            if (!type || !visited.insert(type.Get()).second)
                return false;
            if (type->isInterface && type->scopePath == "std_async" && type->name == marker)
                return true;
            for (const auto& base : type->baseTypes)
            {
                Ref<Type> resolved = unwrapTransferType(base);
                if (resolved && resolved->kind() == TypeKind::Struct &&
                    hasAsyncSafetyMarker(resolved.AsFast<StructType>(), marker, visited))
                {
                    return true;
                }
            }
            return false;
        }

        bool isExecutorTransferSafe(
            Ref<Type> type,
            std::unordered_set<const Type*>& active)
        {
            type = unwrapTransferType(std::move(type));
            if (!type)
                return false;
            if (!active.insert(type.Get()).second)
                return true;

            bool result = false;
            switch (type->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string& name = type.AsFast<PrimitiveType>()->name;
                result = name != "opaque" && name != "any";
                break;
            }
            case TypeKind::Null:
                result = true;
                break;
            case TypeKind::Nullable:
                result = isExecutorTransferSafe(type.AsFast<NullableType>()->valueType, active);
                break;
            case TypeKind::ConstGenericParameter:
                result = isExecutorTransferSafe(type.AsFast<ConstGenericParameterType>()->valueType, active);
                break;
            case TypeKind::ConstValue:
                result = isExecutorTransferSafe(type.AsFast<ConstValueType>()->valueType, active);
                break;
            case TypeKind::Array:
                result = isExecutorTransferSafe(type.AsFast<ArrayType>()->elementType, active);
                break;
            case TypeKind::Dictionary:
            {
                auto dictionary = type.AsFast<DictionaryType>();
                result = isExecutorTransferSafe(dictionary->keyType, active) &&
                         isExecutorTransferSafe(dictionary->valueType, active);
                break;
            }
            case TypeKind::AsyncTask:
                result = isExecutorTransferSafe(type.AsFast<AsyncTaskType>()->valueType, active);
                break;
            case TypeKind::Struct:
            {
                auto structure = type.AsFast<StructType>();
                std::unordered_set<const Type*> markerVisited;
                if (hasAsyncSafetyMarker(structure, "Send", markerVisited))
                {
                    result = true;
                    break;
                }
                if (structure->isEnum || structure->isFlagset)
                {
                    result = true;
                    break;
                }
                if (structure->isObject || structure->isInterface)
                    break;

                result = std::ranges::all_of(structure->fieldTypes, [&](const Ref<Type>& fieldType)
                {
                    return isExecutorTransferSafe(fieldType, active);
                });
                break;
            }
            case TypeKind::Alias:
                break;
            case TypeKind::Reference:
            case TypeKind::Function:
            case TypeKind::GenericParameter:
            case TypeKind::GenericParameterPack:
            case TypeKind::ValuePackView:
            case TypeKind::TypePackView:
            case TypeKind::PackStorage:
                break;
            }

            active.erase(type.Get());
            return result;
        }

        bool isExecutorTransferSafe(const Ref<Type>& type)
        {
            std::unordered_set<const Type*> active;
            return isExecutorTransferSafe(type, active);
        }

        #include "detail/validation_annotation_snapshot.inl"

        std::string getModuleLifecycleExportSymbol(Attribute lifecycleAttribute)
        {
            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
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

        std::string formatExpectedArgumentCountDescription(size_t requiredCount, size_t totalCount)
        {
            if (requiredCount == totalCount)
                return std::to_string(totalCount);

            return common::formatString("{} to {}", requiredCount, totalCount);
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

        #include "detail/type_semantics.inl"

        #include "detail/const_evaluation.inl"

        #include "detail/borrow_queries.inl"

        Ref<StructType> getObjectOrInterfaceStructType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (resolved && resolved->kind() == TypeKind::Reference)
                resolved = unwrapAliasType(resolved.AsFast<ReferenceType>()->referredType);

            if (!resolved || resolved->kind() != TypeKind::Struct)
                return nullptr;

            auto structType = resolved.AsFast<StructType>();
            if (!structType || (!structType->isObject && !structType->isInterface))
                return nullptr;

            return structType;
        }

        std::optional<Ref<Type>> tryGetResultPayloadType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return std::nullopt;

            auto structType = resolved.AsFast<StructType>();
            if (!structType ||
                structType->name != "Result" ||
                structType->scopePath != "std" ||
                structType->genericArguments.size() != 1 ||
                !structType->genericArguments.front())
            {
                return std::nullopt;
            }

            return structType->genericArguments.front();
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

        bool containsGenericParameterType(const Ref<Type>& type);
        bool isNativePodInteropFieldType(const Ref<Type>& type, bool allowGenericPlaceholders = false);
        void validateInstantiatedNativePodComponent(const Ref<StructType>& structType,
                                                    const common::Location& errorLocation = common::Location::invalid());

        std::optional<std::vector<Ref<Type>>> tryMaterializeConcreteInstantiation(
            const std::vector<std::string>& parameterNames,
            const bool hasGenericParameterPack,
            const GenericBindingSet& bindings)
        {
            std::vector<Ref<Type>> materializedTypes;
            const size_t fixedCount = getMinimumGenericArgumentCount(parameterNames, hasGenericParameterPack);
            materializedTypes.reserve(fixedCount + 4);

            for (size_t i = 0; i < fixedCount; ++i)
            {
                auto directIt = bindings.directBindings.find(parameterNames[i]);
                if (directIt == bindings.directBindings.end() ||
                    !directIt->second ||
                    directIt->second->isUnknown() ||
                    containsGenericParameterType(directIt->second))
                {
                    return std::nullopt;
                }

                materializedTypes.push_back(directIt->second);
            }

            if (!hasGenericParameterPack || parameterNames.empty())
                return materializedTypes;

            const std::string& packName = parameterNames.back();
            if (bindings.packAliases.contains(packName))
                return std::nullopt;

            auto packIt = bindings.packBindings.find(packName);
            if (packIt == bindings.packBindings.end())
                return std::nullopt;

            for (const auto& packType : packIt->second)
            {
                if (!packType || packType->isUnknown() || containsGenericParameterType(packType))
                    return std::nullopt;

                materializedTypes.push_back(packType);
            }

            return materializedTypes;
        }

        #include "detail/control_flow_queries.inl"

        #include "detail/generic_type_engine.inl"

        #include "detail/generic_constraint_engine.inl"

        #include "detail/native_interop_queries.inl"

        #include "detail/module_lifecycle_queries.inl"

    #include "detail/analyzer_core.inl"

    #include "detail/program_and_type_visitors.inl"

    #include "detail/operator_visitors.inl"

    #include "detail/literal_and_name_visitors.inl"

    #include "detail/access_and_call_visitors.inl"

    #include "detail/closure_and_conversion_visitors.inl"

    #include "detail/attribute_semantics.inl"

    #include "detail/declaration_visitors.inl"

    #include "detail/control_statement_visitors.inl"

}
