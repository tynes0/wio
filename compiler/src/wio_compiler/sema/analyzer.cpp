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

        std::optional<IntegerType> tryGetContextualIntegerLiteralType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "i8") return IntegerType::i8;
            if (name == "i16") return IntegerType::i16;
            if (name == "i32") return IntegerType::i32;
            if (name == "i64") return IntegerType::i64;
            if (name == "u8" || name == "byte") return IntegerType::u8;
            if (name == "u16") return IntegerType::u16;
            if (name == "u32") return IntegerType::u32;
            if (name == "u64") return IntegerType::u64;
            if (name == "isize") return IntegerType::isize;
            if (name == "usize") return IntegerType::usize;
            return std::nullopt;
        }

        std::optional<FloatType> tryGetContextualFloatLiteralType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "f32") return FloatType::f32;
            if (name == "f64") return FloatType::f64;
            return std::nullopt;
        }

        bool isAssignmentLikeCompatible(const Ref<Type>& destination, const Ref<Type>& source)
        {
            if (!destination || !source || source->isUnknown())
                return false;

            return destination->isCompatibleWith(source) ||
                   (destination->isNumeric() && source->isNumeric());
        }

        bool areMatchTypesCompatible(const Ref<Type>& lhs, const Ref<Type>& rhs)
        {
            if (!lhs || !rhs || lhs->isUnknown() || rhs->isUnknown())
                return true;

            return lhs->isCompatibleWith(rhs) ||
                   rhs->isCompatibleWith(lhs) ||
                   (lhs->isNumeric() && rhs->isNumeric());
        }

        bool isGuardConditionTypeAllowed(const Ref<Type>& type)
        {
            if (!type || type->isUnknown())
                return true;

            return type == Compiler::get().getTypeContext().getBool() ||
                   type->isNumeric() ||
                   type->kind() == TypeKind::Reference ||
                   type->kind() == TypeKind::Null;
        }

        bool isVariableLikeSymbol(const Ref<Symbol>& symbol)
        {
            return symbol &&
                   (symbol->kind == SymbolKind::Variable ||
                    symbol->kind == SymbolKind::Parameter);
        }

        bool isConstScalarType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "bool" ||
                   name == "char" ||
                   name == "uchar" ||
                   name == "byte" ||
                   name == "i8" ||
                   name == "i16" ||
                   name == "i32" ||
                   name == "i64" ||
                   name == "u8" ||
                   name == "u16" ||
                   name == "u32" ||
                   name == "u64" ||
                   name == "f32" ||
                   name == "f64" ||
                   name == "isize" ||
                   name == "usize";
        }

        bool isAllowedConstBinaryOperator(TokenType op)
        {
            return op == TokenType::opPlus ||
                   op == TokenType::opMinus ||
                   op == TokenType::opStar ||
                   op == TokenType::opSlash ||
                   op == TokenType::opPercent ||
                   op == TokenType::opBitAnd ||
                   op == TokenType::opBitOr ||
                   op == TokenType::opBitXor ||
                   op == TokenType::opShiftLeft ||
                   op == TokenType::opShiftRight ||
                   op == TokenType::opLogicalAnd ||
                   op == TokenType::opLogicalOr ||
                   op == TokenType::kwAnd ||
                   op == TokenType::kwOr ||
                   op == TokenType::opEqual ||
                   op == TokenType::opNotEqual ||
                   op == TokenType::opLess ||
                   op == TokenType::opLessEqual ||
                   op == TokenType::opGreater ||
                   op == TokenType::opGreaterEqual;
        }

        bool isAllowedConstUnaryOperator(TokenType op)
        {
            return op == TokenType::opPlus ||
                   op == TokenType::opMinus ||
                   op == TokenType::opBitNot ||
                   op == TokenType::opLogicalNot ||
                   op == TokenType::kwNot;
        }

        bool isConstEvaluableExpression(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (expression->is<IntegerLiteral>() ||
                expression->is<FloatLiteral>() ||
                expression->is<BoolLiteral>() ||
                expression->is<CharLiteral>() ||
                expression->is<ByteLiteral>())
            {
                return true;
            }

            if (expression->is<StringLiteral>() ||
                expression->is<InterpolatedStringLiteral>() ||
                expression->is<ArrayLiteral>() ||
                expression->is<DictionaryLiteral>() ||
                expression->is<NullExpression>() ||
                expression->is<LambdaExpression>() ||
                expression->is<RefExpression>() ||
                expression->is<SelfExpression>() ||
                expression->is<SuperExpression>() ||
                expression->is<RangeExpression>() ||
                expression->is<MatchExpression>() ||
                expression->is<AssignmentExpression>() ||
                expression->is<ArrayAccessExpression>() ||
                expression->is<MemberAccessExpression>() ||
                expression->is<FunctionCallExpression>())
            {
                return false;
            }

            if (const auto* identifier = expression->as<Identifier>())
            {
                auto symbol = identifier->referencedSymbol.Lock();
                return symbol && symbol->flags.get_isConst();
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                return isAllowedConstUnaryOperator(unary->op.type) &&
                       isConstEvaluableExpression(unary->operand);
            }

            if (const auto* binary = expression->as<BinaryExpression>())
            {
                return isAllowedConstBinaryOperator(binary->op.type) &&
                       isConstEvaluableExpression(binary->left) &&
                       isConstEvaluableExpression(binary->right);
            }

            if (const auto* fit = expression->as<FitExpression>())
            {
                Ref<Type> operandType = unwrapAliasType(fit->operand->refType.Lock());
                Ref<Type> targetType = fit->targetType ? unwrapAliasType(fit->targetType->refType.Lock()) : nullptr;
                return isConstEvaluableExpression(fit->operand) &&
                       operandType && targetType &&
                       operandType->isNumeric() &&
                       targetType->isNumeric();
            }

            return false;
        }

        std::string formatAccessContextType(const Ref<Type>& type)
        {
            if (!type)
                return "<non-object context>";

            return type->toString();
        }

        bool isAddressableRefOperand(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (expression->is<ArrayAccessExpression>())
                return true;

            if (expression->is<Identifier>() || expression->is<MemberAccessExpression>())
                return isVariableLikeSymbol(expression->referencedSymbol.Lock());

            return false;
        }

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

        std::string formatDiagnosticType(const Ref<Type>& type)
        {
            if (!type)
                return "<unresolved>";

            return type->toString();
        }

        std::string formatDiagnosticTypeList(const std::vector<Ref<Type>>& types)
        {
            std::string result = "(";
            for (size_t i = 0; i < types.size(); ++i)
            {
                result += formatDiagnosticType(types[i]);
                if (i + 1 < types.size())
                    result += ", ";
            }
            result += ")";
            return result;
        }

        std::string formatFunctionDiagnosticSignature(std::string_view name,
                                                      const std::vector<std::string>& genericParameterNames,
                                                      const Ref<FunctionType>& functionType,
                                                      bool isConstructor = false)
        {
            std::string signature(name);

            if (!genericParameterNames.empty())
            {
                signature += "<";
                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    signature += genericParameterNames[i];
                    if (i + 1 < genericParameterNames.size())
                        signature += ", ";
                }
                signature += ">";
            }

            if (!functionType)
                return signature + "(<invalid>)";

            signature += formatDiagnosticTypeList(functionType->paramTypes);
            if (!isConstructor)
                signature += " -> " + formatDiagnosticType(functionType->returnType);

            return signature;
        }

        template <typename T>
        void appendUniqueValue(std::vector<T>& values, const T& value)
        {
            if (std::ranges::find(values, value) == values.end())
                values.push_back(value);
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

        void collectGenericParameterInstances(const Ref<Type>& type,
                                              const std::vector<std::string>& genericParameterNames,
                                              std::unordered_map<std::string, const Type*>& instances)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return;

            // NOLINTNEXTLINE(clang-diagnostic-switch-enum)
            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end() &&
                    !instances.contains(genericParam->name))
                {
                    instances.emplace(genericParam->name, current.Get());
                }
                return;
            }
            case TypeKind::Reference:
                collectGenericParameterInstances(current.AsFast<ReferenceType>()->referredType, genericParameterNames, instances);
                return;
            case TypeKind::Array:
                collectGenericParameterInstances(current.AsFast<ArrayType>()->elementType, genericParameterNames, instances);
                return;
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                collectGenericParameterInstances(dictType->keyType, genericParameterNames, instances);
                collectGenericParameterInstances(dictType->valueType, genericParameterNames, instances);
                return;
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                collectGenericParameterInstances(funcType->returnType, genericParameterNames, instances);
                for (const auto& paramType : funcType->paramTypes)
                    collectGenericParameterInstances(paramType, genericParameterNames, instances);
                return;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                    collectGenericParameterInstances(genericArgument, genericParameterNames, instances);
                return;
            }
            case TypeKind::Alias:
                collectGenericParameterInstances(current.AsFast<AliasType>()->aliasedType, genericParameterNames, instances);
                return;
            default:
                return;
            }
        }

        bool containsGenericParameterInstance(const Ref<Type>& type,
                                              const std::unordered_map<std::string, const Type*>& instances)
        {
            if (instances.empty())
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
                if (auto foundInstance = instances.find(genericParam->name); foundInstance != instances.end())
                    return foundInstance->second == current.Get();
                return false;
            }
            case TypeKind::Reference:
                return containsGenericParameterInstance(current.AsFast<ReferenceType>()->referredType, instances);
            case TypeKind::Array:
                return containsGenericParameterInstance(current.AsFast<ArrayType>()->elementType, instances);
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterInstance(dictType->keyType, instances) ||
                       containsGenericParameterInstance(dictType->valueType, instances);
            }
            case TypeKind::Function:
            {
                auto funcType = current.AsFast<FunctionType>();
                if (containsGenericParameterInstance(funcType->returnType, instances))
                    return true;

                return std::ranges::any_of(funcType->paramTypes, [&](const Ref<Type>& paramType)
                {
                    return containsGenericParameterInstance(paramType, instances);
                });
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                return std::ranges::any_of(structType->genericArguments, [&](const Ref<Type>& genericArgument)
                {
                    return containsGenericParameterInstance(genericArgument, instances);
                });
            }
            case TypeKind::Alias:
                return containsGenericParameterInstance(current.AsFast<AliasType>()->aliasedType, instances);
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
            instantiatedType->trustedTypeKeys = structType->trustedTypeKeys;
            instantiatedType->isFinal = structType->isFinal;

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

        std::string getStructIdentityKey(const Ref<StructType>& structType)
        {
            if (!structType)
                return {};

            if (structType->scopePath.empty())
                return structType->name;

            return structType->scopePath + "::" + structType->name;
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

        AccessModifier getDefaultAccessModifier(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                AccessModifier fallbackAccess)
        {
            AccessModifier resolvedAccess = fallbackAccess;
            const auto defaultAttributes = getAttributeStatements(attributes, Attribute::Default);
            if (defaultAttributes.empty())
                return resolvedAccess;

            if (defaultAttributes.size() > 1)
            {
                WIO_LOG_ADD_ERROR(defaultAttributes[1]->location(), "Only one @Default(...) attribute is allowed per declaration.");
            }

            for (const auto* defaultAttribute : defaultAttributes)
            {
                const bool hasExactlyOneRawArg = defaultAttribute->args.size() == 1;
                const bool hasTypeArg =
                    !defaultAttribute->typeArgs.empty() &&
                    defaultAttribute->typeArgs.front() != nullptr;

                if (!hasExactlyOneRawArg || hasTypeArg)
                {
                    WIO_LOG_ADD_ERROR(defaultAttribute->location(), "@Default expects exactly one access modifier: public, private, or protected.");
                    continue;
                }

                const Token& accessToken = defaultAttribute->args.front();
                if (accessToken.type == TokenType::kwPublic)
                {
                    resolvedAccess = AccessModifier::Public;
                }
                else if (accessToken.type == TokenType::kwPrivate)
                {
                    resolvedAccess = AccessModifier::Private;
                }
                else if (accessToken.type == TokenType::kwProtected)
                {
                    resolvedAccess = AccessModifier::Protected;
                }
                else
                {
                    WIO_LOG_ADD_ERROR(defaultAttribute->location(), "@Default expects exactly one access modifier: public, private, or protected.");
                }
            }

            return resolvedAccess;
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

        enum class GenericConstraintPredicateKind : uint8_t
        {
            None,
            IsInteger,
            IsNumeric
        };

        std::optional<GenericConstraintPredicateKind> tryGetGenericConstraintPredicateKind(std::string_view name)
        {
            if (name == "IsInteger")
                return GenericConstraintPredicateKind::IsInteger;
            if (name == "IsNumeric")
                return GenericConstraintPredicateKind::IsNumeric;
            return std::nullopt;
        }

        std::string_view getGenericConstraintPredicateName(GenericConstraintPredicateKind predicateKind)
        {
            switch (predicateKind)
            {
            case GenericConstraintPredicateKind::IsInteger: return "IsInteger";
            case GenericConstraintPredicateKind::IsNumeric: return "IsNumeric";
            default: return "<unknown>";
            }
        }

        AttributeTypeArgument getAttributeTypeArgument(const AttributeStatement& attribute, size_t index)
        {
            NodePtr<TypeSpecifier> typeSpecifier = nullptr;
            if (index < attribute.typeArgs.size())
                typeSpecifier = attribute.typeArgs[index];

            return AttributeTypeArgument{
                .token = index < attribute.args.size() ? attribute.args[index] : Token::invalid(),
                .typeSpecifier = typeSpecifier
            };
        }

        bool isExactConstraintTypeMatch(const Ref<Type>& actual, const Ref<Type>& expected)
        {
            Ref<Type> lhs = unwrapAliasType(actual);
            Ref<Type> rhs = unwrapAliasType(expected);
            return lhs && rhs && lhs->isCompatibleWith(rhs) && rhs->isCompatibleWith(lhs);
        }

        bool isIntegerConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "isize" || name == "usize";
        }

        bool evaluateGenericConstraintPredicate(GenericConstraintPredicateKind predicateKind, const Ref<Type>& type)
        {
            switch (predicateKind)
            {
            case GenericConstraintPredicateKind::IsInteger:
                return isIntegerConstraintType(type);
            case GenericConstraintPredicateKind::IsNumeric:
            {
                Ref<Type> resolved = unwrapAliasType(type);
                return resolved && resolved->isNumeric();
            }
            default:
                return false;
            }
        }

        std::vector<Ref<Type>> getGenericConstraintCandidateTypes(GenericConstraintPredicateKind predicateKind)
        {
            auto& ctx = Compiler::get().getTypeContext();

            switch (predicateKind)
            {
            case GenericConstraintPredicateKind::IsInteger:
                return {
                    ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                    ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(),
                    ctx.getISize(), ctx.getUSize()
                };
            case GenericConstraintPredicateKind::IsNumeric:
                return {
                    ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                    ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(),
                    ctx.getISize(), ctx.getUSize(),
                    ctx.getF32(), ctx.getF64()
                };
            default:
                return {};
            }
        }

        Ref<Type> getPreparedConstraintType(SemanticAnalyzer& analyzer, const NodePtr<TypeSpecifier>& typeSpecifier)
        {
            if (!typeSpecifier)
                return nullptr;

            if (auto lockedType = typeSpecifier->refType.Lock(); lockedType && !lockedType->isUnknown())
                return lockedType;

            typeSpecifier->accept(analyzer);
            return typeSpecifier->refType.Lock();
        }

        bool validateGenericConstraintArgument(SemanticAnalyzer& analyzer,
                                               const AttributeTypeArgument& argument,
                                               std::string_view expectedParameterName,
                                               std::string_view attributeName,
                                               common::Location errorLocation,
                                               bool allowBoolLiteral)
        {
            if (argument.typeSpecifier)
            {
                if (auto predicateKind = tryGetGenericConstraintPredicateKind(argument.typeSpecifier->name.value))
                {
                    if (argument.typeSpecifier->generics.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(
                            errorLocation,
                            "{} predicate '{}' expects exactly one generic parameter operand.",
                            attributeName,
                            getGenericConstraintPredicateName(*predicateKind)
                        );
                        return false;
                    }

                    const auto& predicateOperand = argument.typeSpecifier->generics.front();
                    if (!predicateOperand ||
                        predicateOperand->name.type != TokenType::identifier ||
                        !predicateOperand->generics.empty() ||
                        predicateOperand->name.value != expectedParameterName)
                    {
                        WIO_LOG_ADD_ERROR(
                            errorLocation,
                            "{} predicate '{}' must target the matching generic parameter '{}'.",
                            attributeName,
                            getGenericConstraintPredicateName(*predicateKind),
                            expectedParameterName
                        );
                        return false;
                    }

                    return true;
                }

                Ref<Type> exactType = getPreparedConstraintType(analyzer, argument.typeSpecifier);
                if (!exactType || exactType->isUnknown())
                {
                    WIO_LOG_ADD_ERROR(
                        errorLocation,
                        "{} contains an unresolved type constraint.",
                        attributeName
                    );
                    return false;
                }

                if (containsGenericParameterType(exactType))
                {
                    WIO_LOG_ADD_ERROR(
                        errorLocation,
                        "{} must use fully concrete type constraints or supported predicates like IsInteger<{}>.",
                        attributeName,
                        expectedParameterName
                    );
                    return false;
                }

                return true;
            }

            if (allowBoolLiteral && (argument.token.type == TokenType::kwTrue || argument.token.type == TokenType::kwFalse))
                return true;

            WIO_LOG_ADD_ERROR(
                errorLocation,
                "{} supports concrete types{} or supported predicates like IsInteger<{}> and IsNumeric<{}>.",
                attributeName,
                allowBoolLiteral ? ", boolean constants" : "",
                expectedParameterName,
                expectedParameterName
            );
            return false;
        }

        bool validateApplyAttributes(SemanticAnalyzer& analyzer,
                                     const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     std::string_view declarationKind,
                                     common::Location errorLocation)
        {
            const auto applyAttributes = getAttributeStatements(attributes, Attribute::Apply);
            if (applyAttributes.empty())
                return true;

            if (genericParameterNames.empty())
            {
                WIO_LOG_ADD_ERROR(errorLocation, "@Apply can only be used on generic {} declarations.", declarationKind);
                return false;
            }

            bool allValid = true;
            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (applyAttribute->args.size() != genericParameterNames.size())
                {
                    WIO_LOG_ADD_ERROR(
                        applyAttribute->location(),
                        "@Apply expects exactly {} arguments.",
                        genericParameterNames.size()
                    );
                    allValid = false;
                    continue;
                }

                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    if (!validateGenericConstraintArgument(
                            analyzer,
                            getAttributeTypeArgument(*applyAttribute, i),
                            genericParameterNames[i],
                            "@Apply",
                            applyAttribute->location(),
                            true))
                    {
                        allValid = false;
                    }
                }
            }

            return allValid;
        }

        bool matchesApplyConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     const std::vector<Ref<Type>>& bindingTypes)
        {
            const auto applyAttributes = getAttributeStatements(attributes, Attribute::Apply);
            if (applyAttributes.empty())
                return true;

            if (bindingTypes.size() != genericParameterNames.size())
                return false;

            if (std::ranges::any_of(bindingTypes, [](const Ref<Type>& bindingType)
            {
                return !bindingType || bindingType->isUnknown() || containsGenericParameterType(bindingType);
            }))
            {
                return true;
            }

            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute || applyAttribute->args.size() != bindingTypes.size())
                    continue;

                bool matches = true;
                for (size_t i = 0; i < bindingTypes.size(); ++i)
                {
                    const auto argument = getAttributeTypeArgument(*applyAttribute, i);
                    if (argument.typeSpecifier)
                    {
                        if (auto predicateKind = tryGetGenericConstraintPredicateKind(argument.typeSpecifier->name.value))
                        {
                            if (!evaluateGenericConstraintPredicate(*predicateKind, bindingTypes[i]))
                            {
                                matches = false;
                                break;
                            }
                            continue;
                        }

                        Ref<Type> exactType = argument.typeSpecifier->refType.Lock();
                        if (!exactType || !isExactConstraintTypeMatch(bindingTypes[i], exactType))
                        {
                            matches = false;
                            break;
                        }
                        continue;
                    }

                    if (argument.token.type == TokenType::kwTrue)
                        continue;

                    if (argument.token.type == TokenType::kwFalse)
                    {
                        matches = false;
                        break;
                    }

                    matches = false;
                    break;
                }

                if (matches)
                    return true;
            }

            return false;
        }

        std::string formatConcreteInstantiationSignature(const std::vector<Ref<Type>>& instantiationTypes)
        {
            std::string signature = "<";
            for (size_t i = 0; i < instantiationTypes.size(); ++i)
            {
                signature += instantiationTypes[i] ? instantiationTypes[i]->toString() : "<unknown>";
                if (i + 1 < instantiationTypes.size())
                    signature += ", ";
            }
            signature += ">";
            return signature;
        }

        std::vector<std::vector<Ref<Type>>> resolveInstantiateAttributes(SemanticAnalyzer& analyzer,
                                                                         const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                                         const std::vector<std::string>& genericParameterNames)
        {
            std::vector<std::vector<Ref<Type>>> instantiations;
            const auto instantiateAttributes = getAttributeStatements(attributes, Attribute::Instantiate);
            if (instantiateAttributes.empty())
                return instantiations;

            std::unordered_set<std::string> seenInstantiationSignatures;
            for (const auto* instantiateAttribute : instantiateAttributes)
            {
                if (!instantiateAttribute)
                    continue;

                if (instantiateAttribute->args.size() != genericParameterNames.size())
                {
                    WIO_LOG_ADD_ERROR(
                        instantiateAttribute->location(),
                        "@Instantiate expects exactly {} arguments.",
                        genericParameterNames.size()
                    );
                    continue;
                }

                std::vector<std::vector<Ref<Type>>> candidateLists;
                candidateLists.reserve(genericParameterNames.size());
                bool isValidInstantiation = true;

                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    const auto argument = getAttributeTypeArgument(*instantiateAttribute, i);
                    if (!validateGenericConstraintArgument(
                            analyzer,
                            argument,
                            genericParameterNames[i],
                            "@Instantiate",
                            instantiateAttribute->location(),
                            false))
                    {
                        isValidInstantiation = false;
                        break;
                    }

                    if (argument.typeSpecifier)
                    {
                        if (auto predicateKind = tryGetGenericConstraintPredicateKind(argument.typeSpecifier->name.value))
                        {
                            candidateLists.push_back(getGenericConstraintCandidateTypes(*predicateKind));
                            continue;
                        }

                        Ref<Type> exactType = argument.typeSpecifier->refType.Lock();
                        if (!exactType)
                        {
                            isValidInstantiation = false;
                            break;
                        }

                        candidateLists.push_back({ exactType });
                        continue;
                    }

                    isValidInstantiation = false;
                    break;
                }

                if (!isValidInstantiation)
                    continue;

                std::vector<Ref<Type>> currentInstantiation;
                currentInstantiation.reserve(candidateLists.size());

                std::function<void(size_t)> expandCandidates = [&](size_t index)
                {
                    if (index == candidateLists.size())
                    {
                        std::string signatureKey;
                        for (size_t i = 0; i < currentInstantiation.size(); ++i)
                        {
                            if (i > 0)
                                signatureKey += "|";
                            signatureKey += currentInstantiation[i] ? currentInstantiation[i]->toString() : "<unknown>";
                        }

                        if (!seenInstantiationSignatures.insert(signatureKey).second)
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "Duplicate @Instantiate declaration for '{}'.",
                                formatConcreteInstantiationSignature(currentInstantiation)
                            );
                            return;
                        }

                        instantiations.push_back(currentInstantiation);
                        return;
                    }

                    for (const auto& candidateType : candidateLists[index])
                    {
                        currentInstantiation.push_back(candidateType);
                        expandCandidates(index + 1);
                        currentInstantiation.pop_back();
                    }
                };

                expandCandidates(0);
            }

            return instantiations;
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

        Ref<StructType> resolveTrustedStructType(SemanticAnalyzer& analyzer,
                                                 const Ref<Scope>& startScope,
                                                 const AttributeTypeArgument& trustArg,
                                                 common::Location errorLocation)
        {
            Ref<Type> trustedType = nullptr;

            if (trustArg.typeSpecifier)
            {
                trustArg.typeSpecifier->accept(analyzer);
                trustedType = trustArg.typeSpecifier->refType.Lock();
            }
            else if (auto trustSym = resolveAttributeSymbol(startScope, trustArg.token))
            {
                trustedType = trustSym->type;
            }

            trustedType = unwrapAliasType(trustedType);
            if (!trustedType || trustedType->kind() != TypeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(errorLocation, "@Trust expects object/component/interface type names.");
                return nullptr;
            }

            return trustedType.AsFast<StructType>();
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

        bool isSdkExportableFieldType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return false;

            switch (current->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string typeName = current->toString();
                return typeName != "void" && typeName != "object";
            }
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                return arrayType && isSdkExportableFieldType(arrayType->elementType);
            }
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return dictType &&
                    isSdkExportableFieldType(dictType->keyType) &&
                    isSdkExportableFieldType(dictType->valueType);
            }
            case TypeKind::Function:
            {
                auto functionType = current.AsFast<FunctionType>();
                if (!functionType || !isSdkExportableFieldType(functionType->returnType))
                    return false;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (!isSdkExportableFieldType(parameterType))
                        return false;
                }

                return true;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                return structType && !structType->isInterface;
            }
            default:
                return false;
            }
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
        attributeListsBySymbol_.clear();
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

        auto satisfiesApplyForSymbol = [&](const Ref<Symbol>& symbol,
                                           const std::vector<Ref<Type>>& explicitTypeArguments,
                                           std::string_view declarationKind) -> bool
        {
            if (!symbol || symbol->genericParameterNames.empty())
                return true;

            auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
            if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                return true;

            if (!matchesApplyConstraints(*attributeIt->second, symbol->genericParameterNames, explicitTypeArguments))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic {} '{}' rejects type arguments {} because of @Apply constraints.",
                    declarationKind,
                    node.name.value,
                    formatConcreteInstantiationSignature(explicitTypeArguments)
                );
                return false;
            }

            return true;
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
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

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
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type alias"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

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
            Ref<StructType> targetStruct = (typeSym && typeSym->kind == SymbolKind::Struct)
                ? getObjectOrInterfaceStructType(typeSym->type)
                : nullptr;

            if (!targetStruct)
            {
                WIO_LOG_ADD_ERROR(node.right->location(), "The right side of the 'is' operator must be an object or interface type.");
            }

            if (!getObjectOrInterfaceStructType(node.left->refType.Lock()))
            {
                const std::string actualType = node.left->refType.Lock()
                    ? node.left->refType.Lock()->toString()
                    : "<unknown>";
                WIO_LOG_ADD_ERROR(
                    node.left->location(),
                    "The left side of the 'is' operator must be an object or interface value/reference. Got '{}'.",
                    actualType
                );
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
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
        currentExpectedExpressionType_ = getAutoReadableType(node.left->refType.Lock());
        allowContextualNumericLiteralTyping_ = true;
        node.right->accept(*this);
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        bool isCompatible = false;
        bool isAutoDeref = false;

        if (lhsType && rhsType)
        {
            isCompatible = isAssignmentLikeCompatible(lhsType, rhsType);

            if (!isCompatible && lhsType->kind() == TypeKind::Reference)
            {
                Ref<Type> currentRef = lhsType;
                bool canMutate = true;

                while (currentRef && currentRef->kind() == TypeKind::Reference)
                {
                    auto rType = currentRef.AsFast<ReferenceType>();
                    
                    if (!rType->isMutable) canMutate = false;
                    
                    if (isAssignmentLikeCompatible(rType->referredType, rhsType))
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

        if (lhsType && rhsType && !lhsType->isUnknown() && !rhsType->isUnknown() && !isCompatible)
        {
            WIO_LOG_ADD_ERROR(node.op.loc,
                "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                rhsType->toString(),
                lhsType->toString()
            );
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
        IntegerResult result{};
        bool usedContextualType = false;

        if (allowContextualNumericLiteralTyping_ &&
            currentExpectedExpressionType_ &&
            !common::hasIntegerLiteralTypeSuffix(node.token.value))
        {
            if (auto contextualType = tryGetContextualIntegerLiteralType(currentExpectedExpressionType_); contextualType.has_value())
            {
                result = common::getIntegerAsType(node.token.value, *contextualType);
                usedContextualType = true;
            }
        }

        if (!usedContextualType)
            result = common::getInteger(node.token.value);

        if (!result.isValid)
        {
            if (usedContextualType && currentExpectedExpressionType_)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Integer literal '{}' does not fit into expected type '{}'.",
                    node.token.value,
                    currentExpectedExpressionType_->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Invalid integer literal or value out of bounds: '{}'", node.token.value);
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Type::getTypeFromIntegerResult(result);
    }
    
    void SemanticAnalyzer::visit(FloatLiteral& node)
    {
        FloatResult result{};
        bool usedContextualType = false;

        if (allowContextualNumericLiteralTyping_ &&
            currentExpectedExpressionType_ &&
            !common::hasFloatLiteralTypeSuffix(node.token.value))
        {
            if (auto contextualType = tryGetContextualFloatLiteralType(currentExpectedExpressionType_); contextualType.has_value())
            {
                result = common::getFloatAsType(node.token.value, *contextualType);
                usedContextualType = true;
            }
        }

        if (!usedContextualType)
            result = common::getFloat(node.token.value);

        if (!result.isValid)
        {
            if (usedContextualType && currentExpectedExpressionType_)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Float literal '{}' does not fit into expected type '{}'.",
                    node.token.value,
                    currentExpectedExpressionType_->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Invalid float literal or value out of bounds: '{}'", node.token.value);
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Type::getTypeFromFloatResult(result);
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
        Ref<Type> expectedArrayLiteralType = nullptr;
        Ref<ArrayType> expectedArrayType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Array)
            {
                expectedArrayLiteralType = expectedType;
                expectedArrayType = expectedType.AsFast<ArrayType>();
            }
        }

        if (node.elements.empty())
        {
            if (expectedArrayType)
            {
                node.refType = expectedArrayLiteralType;
                return;
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

        auto analyzeElement = [&](NodePtr<Expression>& element)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedArrayType ? expectedArrayType->elementType : previousExpectedExpressionType;
            element->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        analyzeElement(node.elements[0]);
        Ref<Type> baseType = expectedArrayType ? expectedArrayType->elementType : node.elements[0]->refType.Lock();
        if (!baseType)
            baseType = node.elements[0]->refType.Lock();

        for (size_t i = 1; i < node.elements.size(); ++i)
        {
            analyzeElement(node.elements[i]);
            if (auto lockedType = node.elements[i]->refType.Lock(); lockedType)
            {
                if (!baseType || (!baseType->isCompatibleWith(lockedType) && !(baseType->isNumeric() && lockedType->isNumeric())))
                {
                    const std::string expectedTypeName = baseType ? baseType->toString() : "<unknown>";
                    WIO_LOG_ADD_ERROR(
                        node.elements[i]->location(),
                        "Array elements must be of the same type. Expected '{}', but found '{}'.",
                        expectedTypeName,
                        lockedType->toString()
                    );
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.elements[i]->location(), "Undefined element type in array.");
            }
        }

        if (expectedArrayType)
        {
            if (expectedArrayType->arrayKind == ArrayType::ArrayKind::Static &&
                expectedArrayType->size != node.elements.size())
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Static array expects '{}' elements, but literal provides '{}'.",
                    expectedArrayType->size,
                    node.elements.size()
                );
            }

            node.refType = expectedArrayLiteralType;
            return;
        }

        node.refType = Compiler::get().getTypeContext().getOrCreateArrayType(baseType, ArrayType::ArrayKind::Literal, node.elements.size());
    }
    
    void SemanticAnalyzer::visit(DictionaryLiteral& node)
    {
        Ref<Type> expectedDictionaryLiteralType = nullptr;
        Ref<DictionaryType> expectedDictionaryType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Dictionary)
            {
                expectedDictionaryLiteralType = expectedType;
                expectedDictionaryType = expectedType.AsFast<DictionaryType>();
            }
        }

        if (node.pairs.empty())
        {
            if (expectedDictionaryType)
            {
                if (expectedDictionaryType->isOrdered != node.isOrdered)
                {
                    if (node.isOrdered)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Ordered dictionary literal '{< >}' cannot initialize unordered dictionary type '{}'.",
                            expectedDictionaryType->toString()
                        );
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Unordered dictionary literal '{}' cannot initialize ordered dictionary type '{}'. Use '{< >}' syntax instead.",
                            "{}",
                            expectedDictionaryType->toString()
                        );
                    }
                }

                node.refType = expectedDictionaryLiteralType;
                return;
            }

            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (expectedDictionaryType && expectedDictionaryType->isOrdered != node.isOrdered)
        {
            if (node.isOrdered)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ordered dictionary literal '{< >}' cannot initialize unordered dictionary type '{}'.",
                    expectedDictionaryType->toString()
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Unordered dictionary literal '{}' cannot initialize ordered dictionary type '{}'. Use '{< >}' syntax instead.",
                    "{}",
                    expectedDictionaryType->toString()
                );
            }
        }

        auto analyzeKey = [&](NodePtr<Expression>& expression)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedDictionaryType ? expectedDictionaryType->keyType : previousExpectedExpressionType;
            expression->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        auto analyzeValue = [&](NodePtr<Expression>& expression)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            currentExpectedExpressionType_ = expectedDictionaryType ? expectedDictionaryType->valueType : previousExpectedExpressionType;
            expression->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
        };

        analyzeKey(node.pairs[0].first);
        analyzeValue(node.pairs[0].second);

        auto keyType = expectedDictionaryType ? expectedDictionaryType->keyType : node.pairs[0].first->refType.Lock();
        auto valType = expectedDictionaryType ? expectedDictionaryType->valueType : node.pairs[0].second->refType.Lock();
        if (!keyType)
            keyType = node.pairs[0].first->refType.Lock();
        if (!valType)
            valType = node.pairs[0].second->refType.Lock();

        for (size_t i = 1; i < node.pairs.size(); ++i)
        {
            analyzeKey(node.pairs[i].first);
            analyzeValue(node.pairs[i].second);
            
            auto k = node.pairs[i].first->refType.Lock();
            auto v = node.pairs[i].second->refType.Lock();

            if (!keyType || !valType ||
                !k || !v ||
                (!keyType->isCompatibleWith(k) && !(keyType->isNumeric() && k->isNumeric())) ||
                (!valType->isCompatibleWith(v) && !(valType->isNumeric() && v->isNumeric())))
            {
                WIO_LOG_ADD_ERROR(node.pairs[i].first->location(), "All keys and values in a dictionary literal must have the same type.");
            }
        }

        if (expectedDictionaryType)
        {
            node.refType = expectedDictionaryLiteralType;
            return;
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

        std::function<Ref<Symbol>(Ref<Type>, const std::string&, Ref<Type>*)> findMemberInHierarchy =
            [&](const Ref<Type>& t, const std::string& name, Ref<Type>* ownerType) -> Ref<Symbol> {
                if (!t || t->kind() != TypeKind::Struct)
                    return nullptr;
                auto sType = t.AsFast<StructType>();
            
                if (auto lockedScope = sType->structScope.Lock(); lockedScope)
                {
                    if (auto found = lockedScope->resolveLocally(name); found)
                    {
                        if (ownerType)
                            *ownerType = t;
                        return found;
                    }
                }
                for (auto& base : sType->baseTypes)
                {
                    if (auto found = findMemberInHierarchy(base, name, ownerType); found)
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
                foundMember = findMemberInHierarchy(actualStructType, node.member->token.value, &actualStructType);
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
                    foundMember = findMemberInHierarchy(actualStructType, node.member->token.value, &actualStructType);
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
        bool isTrustedAccess = false;

        if (currentStructType_ && actualStructType)
        {
            if (currentStructType_ == actualStructType || 
                isTypeDerivedFrom(currentStructType_, actualStructType) || 
                isTypeDerivedFrom(actualStructType, currentStructType_))
            {
                isInsideHierarchy = true;
            }

            isInsideSameObject = currentStructType_ == actualStructType;

            if (auto ownerStruct = actualStructType.AsFast<StructType>(); ownerStruct)
            {
                const std::string trustedKey = getStructIdentityKey(currentStructType_.AsFast<StructType>());
                isTrustedAccess =
                    !trustedKey.empty() &&
                    std::ranges::find(ownerStruct->trustedTypeKeys, trustedKey) != ownerStruct->trustedTypeKeys.end();
            }
        }

        const std::string ownerTypeName = formatAccessContextType(actualStructType);
        const std::string currentContextTypeName = formatAccessContextType(currentStructType_);

        if (foundMember->flags.get_isPrivate() && !isInsideSameObject && !isTrustedAccess)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Cannot access private member '{}' declared on '{}' from '{}'.",
                foundMember->name,
                ownerTypeName,
                currentContextTypeName
            );
        }

        if (foundMember->flags.get_isProtected() && !isInsideHierarchy && !isTrustedAccess)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Cannot access protected member '{}' declared on '{}' from '{}'.",
                foundMember->name,
                ownerTypeName,
                currentContextTypeName
            );
        }

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
        Ref<Symbol> genericOwnerSym = calleeSym;
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
                    auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                    if (attributeIt != attributeListsBySymbol_.end() &&
                        attributeIt->second &&
                        !matchesApplyConstraints(*attributeIt->second, constructorGenericParameterNames, explicitTypeArguments))
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Generic type '{}' rejects type arguments {} because of @Apply constraints.",
                            structType->name,
                            formatConcreteInstantiationSignature(explicitTypeArguments)
                        );
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return;
                    }

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
                                auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                                if (attributeIt != attributeListsBySymbol_.end() &&
                                    attributeIt->second &&
                                    !matchesApplyConstraints(*attributeIt->second, constructorGenericParameterNames, expectedStructType->genericArguments))
                                {
                                    WIO_LOG_ADD_ERROR(
                                        node.location(),
                                        "Generic type '{}' rejects type arguments {} because of @Apply constraints.",
                                        structType->name,
                                        formatConcreteInstantiationSignature(expectedStructType->genericArguments)
                                    );
                                    node.refType = Compiler::get().getTypeContext().getUnknown();
                                    return;
                                }

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

        auto isContextSensitiveArgument = [&](const NodePtr<Expression>& argument) -> bool
        {
            if (!argument)
                return false;

            if (argument->is<LambdaExpression>())
                return true;

            if (auto* arrayLiteral = argument->as<ArrayLiteral>())
                return arrayLiteral->elements.empty();

            if (auto* dictionaryLiteral = argument->as<DictionaryLiteral>())
                return dictionaryLiteral->pairs.empty();

            return false;
        };

        auto analyzeArgumentWithExpectedType = [&](const NodePtr<Expression>& argument,
                                                   const Ref<Type>& expectedType,
                                                   bool suppressDiagnostics) -> std::optional<Ref<Type>>
        {
            if (!argument)
                return std::nullopt;

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = expectedType ? getAutoReadableType(expectedType) : nullptr;
            allowContextualNumericLiteralTyping_ = !suppressDiagnostics;

            if (suppressDiagnostics)
                Logger::get().beginDiagnosticProbe();

            argument->accept(*this);

            const int32_t suppressedErrors = suppressDiagnostics ? Logger::get().endDiagnosticProbe() : 0;
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

            if (suppressDiagnostics && suppressedErrors > 0)
                return std::nullopt;

            Ref<Type> analyzedType = argument->refType.Lock();
            if (!analyzedType)
                return std::nullopt;

            if (suppressDiagnostics && analyzedType->isUnknown())
                return std::nullopt;

            return analyzedType;
        };

        auto analyzeArgumentsForResolvedFunctionType = [&](const Ref<FunctionType>& functionType,
                                                           bool suppressDiagnostics,
                                                           bool requireExactArity) -> std::optional<std::vector<Ref<Type>>>
        {
            if (!functionType)
                return std::nullopt;

            if (requireExactArity && functionType->paramTypes.size() != node.arguments.size())
                return std::nullopt;

            std::vector<Ref<Type>> resolvedArgumentTypes;
            resolvedArgumentTypes.reserve(node.arguments.size());

            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                Ref<Type> expectedType = nullptr;
                if (i < functionType->paramTypes.size())
                    expectedType = functionType->paramTypes[i];

                auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedType, suppressDiagnostics);
                if (!analyzedType.has_value())
                    return std::nullopt;

                resolvedArgumentTypes.push_back(*analyzedType);
            }

            return resolvedArgumentTypes;
        };

        auto getCallableDisplayName = [&]() -> std::string
        {
            if (isConstructorCall && constructorStructType)
                return constructorStructType->name;

            if (calleeSym)
                return calleeSym->name;

            if (auto* memberAccess = node.callee->as<MemberAccessExpression>())
                return memberAccess->member ? memberAccess->member->token.value : "<member>";

            if (auto* identifier = node.callee->as<Identifier>())
                return identifier->token.value;

            return "<callable>";
        };

        std::optional<std::vector<Ref<Type>>> cachedUncontextualizedArgumentTypes;
        auto getUncontextualizedArgumentTypes = [&]() -> const std::vector<Ref<Type>>&
        {
            if (!cachedUncontextualizedArgumentTypes.has_value())
            {
                std::vector<Ref<Type>> argumentTypes;
                argumentTypes.reserve(node.arguments.size());

                for (const auto& argument : node.arguments)
                {
                    auto analyzedType = analyzeArgumentWithExpectedType(argument, nullptr, true);
                    argumentTypes.push_back(analyzedType.has_value() ? *analyzedType : Compiler::get().getTypeContext().getUnknown());
                }

                cachedUncontextualizedArgumentTypes = std::move(argumentTypes);
            }

            return *cachedUncontextualizedArgumentTypes;
        };

        auto formatCallArgumentTypesForDiagnostic = [&]() -> std::string
        {
            return formatDiagnosticTypeList(getUncontextualizedArgumentTypes());
        };

        auto scoreResolvedCall = [&](const Ref<FunctionType>& functionType,
                                     const std::vector<Ref<Type>>& actualArgumentTypes,
                                     bool isGenericCandidate,
                                     size_t genericParameterCount) -> std::optional<int>
        {
            if (!functionType || functionType->paramTypes.size() != actualArgumentTypes.size())
                return std::nullopt;

            int currentScore = 0;
            for (size_t i = 0; i < actualArgumentTypes.size(); ++i)
            {
                auto dest = functionType->paramTypes[i];
                const auto& src = actualArgumentTypes[i];

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

                        if ((destIsUn && srcIsUn) || (destIsInt && srcIsInt) || (destIsFlt && srcIsFlt))
                            currentScore += 50;

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

            auto formatCandidateSignature = [&](const Ref<Symbol>& overload) -> std::string
            {
                if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                    return "<invalid overload>";

                const auto functionType = overload->type.AsFast<FunctionType>();
                const auto& genericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;

                return formatFunctionDiagnosticSignature(
                    isConstructorCall && constructorStructType ? constructorStructType->name : overload->name,
                    genericParameterNames,
                    functionType,
                    isConstructorCall
                );
            };

            auto buildCandidateSummary = [&](size_t maxCount = 4) -> std::string
            {
                std::vector<std::string> signatures;
                signatures.reserve(candidateSymbols.size());

                for (const auto& candidate : candidateSymbols)
                {
                    std::string signature = formatCandidateSignature(candidate);
                    appendUniqueValue(signatures, signature);
                }

                std::string summary;
                const size_t displayCount = std::min(maxCount, signatures.size());
                for (size_t i = 0; i < displayCount; ++i)
                {
                    summary += signatures[i];
                    if (i + 1 < displayCount)
                        summary += "; ";
                }

                if (signatures.size() > maxCount)
                    summary += "; ...";

                return summary;
            };

            auto buildAvailableGenericAritySummary = [&]() -> std::vector<size_t>
            {
                std::vector<size_t> arities;
                for (const auto& candidate : candidateSymbols)
                {
                    if (!candidate)
                        continue;

                    const auto& genericParameterNames =
                        isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                    if (!genericParameterNames.empty())
                        appendUniqueValue(arities, genericParameterNames.size());
                }

                std::ranges::sort(arities);
                return arities;
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

                if (overload->resolvedGenericInstantiations.empty())
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

                for (const auto& instantiationTypes : overload->resolvedGenericInstantiations)
                {
                    if (instantiationTypes.size() != resolvedBindingTypes.size())
                        continue;

                    bool matches = true;
                    for (size_t i = 0; i < instantiationTypes.size(); ++i)
                    {
                        if (!isExactConstraintTypeMatch(instantiationTypes[i], resolvedBindingTypes[i]))
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

            auto satisfiesApplyBinding = [&](const Ref<Symbol>& symbol,
                                             const std::vector<std::string>& activeGenericParameterNames,
                                             const std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!symbol || activeGenericParameterNames.empty())
                    return true;

                auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return true;

                std::vector<Ref<Type>> resolvedBindingTypes;
                resolvedBindingTypes.reserve(activeGenericParameterNames.size());
                for (const auto& genericParameterName : activeGenericParameterNames)
                {
                    auto foundBinding = bindings.find(genericParameterName);
                    if (foundBinding == bindings.end() || !foundBinding->second)
                        return false;

                    resolvedBindingTypes.push_back(foundBinding->second);
                }

                return matchesApplyConstraints(*attributeIt->second, activeGenericParameterNames, resolvedBindingTypes);
            };

            bool rejectedByInstantiationWhitelist = false;
            std::string rejectedInstantiationFunctionName;
            std::string rejectedInstantiationSignature;
            bool rejectedByApplyConstraints = false;
            std::string rejectedApplyTargetName;
            std::string rejectedApplySignature;

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
                if (!declaredFunctionType || declaredFunctionType->paramTypes.size() != node.arguments.size())
                    return std::nullopt;

                const std::vector<std::string>& activeGenericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                bool isGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                if (useExplicitFunctionTypeArguments && !isGenericCandidate)
                    return std::nullopt;

                std::unordered_map<std::string, const Type*> activeGenericParameterInstances;
                if (isGenericCandidate)
                    collectGenericParameterInstances(resolvedFunctionType, activeGenericParameterNames, activeGenericParameterInstances);

                std::unordered_map<std::string, Ref<Type>> bindings;
                std::vector<Ref<Type>> candidateArgTypes(node.arguments.size());
                std::vector<bool> analyzedArguments(node.arguments.size(), false);

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
                }

                auto getExpectedParameterType = [&](size_t index) -> Ref<Type>
                {
                    Ref<Type> expectedParameterType = declaredFunctionType->paramTypes[index];
                    if (!bindings.empty())
                        expectedParameterType = instantiateGenericType(expectedParameterType, bindings);
                    return expectedParameterType;
                };

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    bool shouldDeferArgument =
                        isGenericCandidate &&
                        isContextSensitiveArgument(node.arguments[i]) &&
                        containsNamedGenericParameterType(expectedParameterType, activeGenericParameterNames);

                    if (shouldDeferArgument)
                        continue;

                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }
                }

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    if (analyzedArguments[i])
                        continue;

                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }
                }

                if (std::ranges::any_of(analyzedArguments, [](bool analyzed) { return !analyzed; }))
                    return std::nullopt;

                if (isGenericCandidate)
                {
                    for (const auto& genericParameterName : activeGenericParameterNames)
                    {
                        if (!bindings.contains(genericParameterName) ||
                            !bindings.at(genericParameterName) ||
                            bindings.at(genericParameterName)->isUnknown() ||
                            containsGenericParameterInstance(bindings.at(genericParameterName), activeGenericParameterInstances))
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

                    Ref<Symbol> applyConstraintOwner = isConstructorCall ? genericOwnerSym : overload;
                    if (!satisfiesApplyBinding(applyConstraintOwner, activeGenericParameterNames, bindings))
                    {
                        std::vector<Ref<Type>> resolvedBindingTypes;
                        resolvedBindingTypes.reserve(activeGenericParameterNames.size());
                        for (const auto& genericParameterName : activeGenericParameterNames)
                        {
                            auto foundBinding = bindings.find(genericParameterName);
                            if (foundBinding != bindings.end())
                                resolvedBindingTypes.push_back(foundBinding->second);
                        }

                        rejectedByApplyConstraints = true;
                        rejectedApplyTargetName =
                            isConstructorCall && constructorStructType ? constructorStructType->name : overload->name;
                        rejectedApplySignature = formatInstantiationSignature(resolvedBindingTypes);
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
                        candidateArgTypes,
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
                const std::string candidateSummary = buildCandidateSummary();
                if (candidateSummary.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Ambiguous function call to '{}' with arguments {}.",
                        getCallableDisplayName(),
                        formatCallArgumentTypesForDiagnostic()
                    );
                }
                else
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Ambiguous function call to '{}' with arguments {}. Candidates: {}.",
                        getCallableDisplayName(),
                        formatCallArgumentTypesForDiagnostic(),
                        candidateSummary
                    );
                }
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
                else if (rejectedByApplyConstraints)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic callable '{}' rejects type arguments {} because of @Apply constraints.",
                        rejectedApplyTargetName,
                        rejectedApplySignature
                    );
                }
                else
                {
                    const auto availableGenericArities = buildAvailableGenericAritySummary();
                    const bool providedExplicitTypeArguments = useExplicitFunctionTypeArguments || (isConstructorCall && !explicitTypeArguments.empty());

                    if (providedExplicitTypeArguments &&
                        !availableGenericArities.empty() &&
                        std::ranges::find(availableGenericArities, explicitTypeArguments.size()) == availableGenericArities.end())
                    {
                        if (availableGenericArities.size() == 1)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "Generic call to '{}' expects {} explicit type arguments, but got {}.",
                                getCallableDisplayName(),
                                availableGenericArities.front(),
                                explicitTypeArguments.size()
                            );
                        }
                        else
                        {
                            std::string arityList;
                            for (size_t i = 0; i < availableGenericArities.size(); ++i)
                            {
                                arityList += std::to_string(availableGenericArities[i]);
                                if (i + 1 < availableGenericArities.size())
                                    arityList += ", ";
                            }

                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No overload of '{}' accepts {} explicit type arguments. Available generic arities: {}.",
                                getCallableDisplayName(),
                                explicitTypeArguments.size(),
                                arityList
                            );
                        }
                    }
                    else
                    {
                        const std::string candidateSummary = buildCandidateSummary();
                        if (candidateSummary.empty())
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No matching overload for '{}' with arguments {}.",
                                getCallableDisplayName(),
                                formatCallArgumentTypesForDiagnostic()
                            );
                        }
                        else
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "No matching overload for '{}' with arguments {}. Candidates: {}.",
                                getCallableDisplayName(),
                                formatCallArgumentTypesForDiagnostic(),
                                candidateSummary
                            );
                        }
                    }
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

            if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    bestMatch->functionType.AsFast<FunctionType>(),
                    false,
                    false
                ); !resolvedArgumentTypes.has_value())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = isConstructorCall ? structReturnType : bestMatch->functionType.AsFast<FunctionType>()->returnType;
            return; 
        }

        Ref<Type> calleeType = node.callee->refType.Lock();
        if (auto* memberAccess = node.callee->as<MemberAccessExpression>();
            memberAccess && !memberAccess->intrinsicOverloadMembers.empty())
        {
            std::optional<size_t> bestIndex;
            std::optional<int> bestScore;
            bool isAmbiguous = false;

            for (size_t i = 0; i < memberAccess->intrinsicOverloadTypes.size(); ++i)
            {
                Ref<Type> overloadType = memberAccess->intrinsicOverloadTypes[i].Lock();
                if (!overloadType || overloadType->kind() != TypeKind::Function)
                    continue;

                auto overloadArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    overloadType.AsFast<FunctionType>(),
                    true,
                    true
                );
                if (!overloadArgumentTypes.has_value())
                    continue;

                auto score = scoreResolvedCall(overloadType.AsFast<FunctionType>(), *overloadArgumentTypes, false, 0);
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
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous function call to intrinsic member '{}' with arguments {}.",
                    memberAccess->member->token.value,
                    formatCallArgumentTypesForDiagnostic()
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!bestIndex.has_value())
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "No matching overload for intrinsic member '{}' with arguments {}.",
                    memberAccess->member->token.value,
                    formatCallArgumentTypesForDiagnostic()
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            memberAccess->intrinsicMember = memberAccess->intrinsicOverloadMembers[*bestIndex];
            memberAccess->refType = memberAccess->intrinsicOverloadTypes[*bestIndex].Lock();
            node.callee->refType = memberAccess->refType;
            calleeType = memberAccess->refType.Lock();

            if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    calleeType.AsFast<FunctionType>(),
                    false,
                    false
                ); !resolvedArgumentTypes.has_value())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

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
        std::vector<Ref<Type>> argTypes;
        if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(funcType, false, false);
            resolvedArgumentTypes.has_value())
        {
            argTypes = std::move(*resolvedArgumentTypes);
        }
        else
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

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
        Ref<FunctionType> expectedFunctionType = nullptr;
        if (currentExpectedExpressionType_)
        {
            Ref<Type> expectedType = unwrapAliasType(currentExpectedExpressionType_);
            if (expectedType && expectedType->kind() == TypeKind::Function)
                expectedFunctionType = expectedType.AsFast<FunctionType>();
        }

        if (expectedFunctionType && expectedFunctionType->paramTypes.size() != node.parameters.size())
            expectedFunctionType = nullptr;

        const bool shouldEnforceExpectedReturnType =
            node.returnType ||
            (expectedFunctionType &&
             expectedFunctionType->returnType &&
             !containsGenericParameterType(expectedFunctionType->returnType));

        enterScope(ScopeKind::Function);
        Ref<Type> previousFunctionReturnType = currentFunctionReturnType_;

        std::vector<Ref<Type>> paramTypes;
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
            if (param.type)
            {
                param.type->accept(*this);
                pType = param.type->refType.Lock();
            }
            else if (expectedFunctionType && i < expectedFunctionType->paramTypes.size())
            {
                pType = expectedFunctionType->paramTypes[i];
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
        else if (shouldEnforceExpectedReturnType && expectedFunctionType)
        {
            retType = expectedFunctionType->returnType;
        }

        currentFunctionReturnType_ = shouldEnforceExpectedReturnType
            ? retType
            : Compiler::get().getTypeContext().getUnknown();

        if (node.body)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            if (node.body->is<ExpressionStatement>() && shouldEnforceExpectedReturnType)
                currentExpectedExpressionType_ = retType;

            node.body->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            
            if (!node.returnType && !shouldEnforceExpectedReturnType)
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
            else if (!node.returnType && shouldEnforceExpectedReturnType && node.body->is<ExpressionStatement>())
            {
                Ref<Type> actualReturnType = node.body->as<ExpressionStatement>()->expression->refType.Lock();
                if (actualReturnType &&
                    !retType->isCompatibleWith(actualReturnType) &&
                    !(retType->isNumeric() && actualReturnType->isNumeric()))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Lambda return type mismatch! Expected '{}', but got '{}'.",
                        retType->toString(),
                        actualReturnType->toString()
                    );
                }
            }
        }

        exitScope();
        currentFunctionReturnType_ = previousFunctionReturnType;

        node.refType = Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes);
    }
    
    void SemanticAnalyzer::visit(RefExpression& node)
    {
        node.operand->accept(*this);

        if (!isAddressableRefOperand(node.operand))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "The 'ref' operator requires an addressable variable, member, or indexed element."
            );
        }
        
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

        if (srcType && destType && !srcType->isUnknown() && !destType->isUnknown())
        {
            Ref<Type> resolvedSrcType = unwrapAliasType(srcType);
            Ref<Type> resolvedDestType = unwrapAliasType(destType);

            if (resolvedSrcType && resolvedDestType && resolvedSrcType->isNumeric() && resolvedDestType->isNumeric())
            {
                node.refType = destType;
                return;
            }

            if (getObjectOrInterfaceStructType(srcType) && getObjectOrInterfaceStructType(destType))
            {
                node.refType = destType;
                return;
            }

            WIO_LOG_ADD_ERROR(
                node.location(),
                "The 'fit' operator can only be used with numeric types or object/interface casts."
            );
        }

        node.refType = Compiler::get().getTypeContext().getUnknown();
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

        Ref<Type> matchValueType = node.value->refType.Lock();
        Ref<Type> commonReturnType = currentExpectedExpressionType_;
        if (commonReturnType && (commonReturnType->isVoid() || commonReturnType->isUnknown()))
            commonReturnType = nullptr;

        bool allBodiesAreExpressionStatements = true;
        bool hasAssumedCase = false;
        size_t assumedCaseCount = 0;

        for (size_t caseIndex = 0; caseIndex < node.cases.size(); ++caseIndex)
        {
            auto& matchCase = node.cases[caseIndex];

            if (matchCase.matchValues.empty())
            {
                hasAssumedCase = true;
                assumedCaseCount++;

                if (assumedCaseCount > 1)
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "Match expressions can only contain one 'assumed' case."
                    );
                }

                if (caseIndex + 1 != node.cases.size())
                {
                    WIO_LOG_ADD_ERROR(
                        matchCase.body ? matchCase.body->location() : node.location(),
                        "The 'assumed' match case must be the last case."
                    );
                }
            }

            for (auto& val : matchCase.matchValues)
            {
                Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
                bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
                currentExpectedExpressionType_ = matchValueType;
                allowContextualNumericLiteralTyping_ = true;
                val->accept(*this);
                currentExpectedExpressionType_ = previousExpectedExpressionType;
                allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

                Ref<Type> caseValueType = val->refType.Lock();
                if (!matchValueType || matchValueType->isUnknown())
                    continue;

                if (val->is<RangeExpression>())
                {
                    if (!matchValueType->isNumeric())
                    {
                        WIO_LOG_ADD_ERROR(
                            val->location(),
                            "Range match cases require the matched value to be numeric, but got '{}'.",
                            matchValueType->toString()
                        );
                    }

                    continue;
                }

                if (!caseValueType || caseValueType->isUnknown())
                    continue;

                if (!areMatchTypesCompatible(matchValueType, caseValueType))
                {
                    WIO_LOG_ADD_ERROR(
                        val->location(),
                        "Match case value type '{}' is not compatible with matched value type '{}'.",
                        caseValueType->toString(),
                        matchValueType->toString()
                    );
                }
            }

            matchCase.body->accept(*this);

            if (!matchCase.body->is<ExpressionStatement>())
            {
                allBodiesAreExpressionStatements = false;
            }

            if (matchCase.body->is<ExpressionStatement>())
            {
                auto exprStmt = matchCase.body->as<ExpressionStatement>();
                auto bodyType = exprStmt->expression->refType.Lock();
                if (!bodyType || bodyType->isUnknown())
                    continue;

                if (!commonReturnType)
                {
                    commonReturnType = bodyType;
                    continue;
                }

                if (!areMatchTypesCompatible(commonReturnType, bodyType))
                {
                    WIO_LOG_ADD_ERROR(
                        exprStmt->location(),
                        "All value-producing match cases must return compatible types. Expected '{}', but got '{}'.",
                        commonReturnType->toString(),
                        bodyType->toString()
                    );
                    commonReturnType = Compiler::get().getTypeContext().getUnknown();
                    continue;
                }

                if (!commonReturnType->isCompatibleWith(bodyType) && bodyType->isCompatibleWith(commonReturnType))
                    commonReturnType = bodyType;
            }
        }

        if (allBodiesAreExpressionStatements)
        {
            if (!hasAssumedCase)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Value-producing match expressions must include an 'assumed' fallback case."
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = commonReturnType ? commonReturnType : Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.refType = Compiler::get().getTypeContext().getVoid();
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
            if (node.mutability == Mutability::Const) flags.set_isConst(true);
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
            if (node.mutability == Mutability::Const) flags.set_isConst(true);
            
            sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
            currentScope_->define(node.name->token.value, sym);
            node.name->referencedSymbol = sym;
        }
    
        if (node.initializer)
        {
            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = sym->type;
            allowContextualNumericLiteralTyping_ = true;
            node.initializer->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
            Ref<Type> initType = node.initializer->refType.Lock();
    
            if (!sym->type || sym->type->isUnknown()) 
            {
                sym->type = initType;
                node.name->refType = initType;
            }
            else if (initType && !initType->isUnknown() && !isAssignmentLikeCompatible(sym->type, initType)) 
            {
                WIO_LOG_ADD_ERROR(node.location(), "Type mismatch for '{}'.", node.name->token.value);
            }

            if (node.mutability == Mutability::Const)
            {
                if (!isConstScalarType(sym->type))
                {
                    const std::string actualType = sym->type ? sym->type->toString() : "<unknown>";
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Const declarations currently support only scalar primitive types. Got '{}'.",
                        actualType
                    );
                }
                else if (!isConstEvaluableExpression(node.initializer))
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Const initializer must be a compile-time scalar expression and may reference only other const declarations."
                    );
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

        std::vector<std::string> genericParameterNames;
        genericParameterNames.reserve(node.genericParameters.size());
        for (const auto& genericParameter : node.genericParameters)
        {
            if (genericParameter)
                genericParameterNames.push_back(genericParameter->token.value);
        }

        validateApplyAttributes(*this, node.attributes, genericParameterNames, "type alias", node.location());

        node.aliasedType->accept(*this);
        Ref<Type> aliasedType = node.aliasedType->refType.Lock();
        Ref<Type> aliasType = Compiler::get().getTypeContext().getOrCreateAliasType(node.name->token.value, aliasedType);
        Ref<Symbol> aliasSym = createSymbol(node.name->token.value, aliasType, SymbolKind::TypeAlias, node.location());
        aliasSym->aliasTargetType = aliasedType;
        aliasSym->genericParameterNames = genericParameterNames;

        currentScope_->define(node.name->token.value, aliasSym);
        attributeListsBySymbol_[aliasSym.Get()] = &node.attributes;
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
            attributeListsBySymbol_[funcSym.Get()] = &node.attributes;
            if (!funcSym->genericParameterNames.empty())
            {
                validateApplyAttributes(*this, node.attributes, funcSym->genericParameterNames, "function", node.location());
                funcSym->resolvedGenericInstantiations = resolveInstantiateAttributes(*this, node.attributes, funcSym->genericParameterNames);
            }

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
        bool hasApply = hasAttribute(node.attributes, Attribute::Apply);
        bool isStructMethod = currentScope_ && currentScope_->getKind() == ScopeKind::Struct;
        bool hasInstantiate = hasAttribute(node.attributes, Attribute::Instantiate);
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

        if (hasApply && !isGenericFunction)
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Apply can only be used on generic functions.");
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

        if (node.name->token.value == "Entry")
        {
            auto& typeContext = Compiler::get().getTypeContext();
            const Ref<Type> expectedArgsType = typeContext.getOrCreateArrayType(typeContext.getString(), ArrayType::ArrayKind::Dynamic);

            if (isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry is currently supported only for top-level functions.");
            }

            if (!node.body)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry functions must define a Wio body.");
            }

            if (!isExactType(funcType->returnType, typeContext.getI32()) &&
                !isExactType(funcType->returnType, typeContext.getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry must return i32 or void.");
            }

            if (node.parameters.size() > 1)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Entry must declare zero parameters or exactly one string[] parameter.");
            }
            else if (node.parameters.size() == 1 && !isExactType(funcType->paramTypes[0], expectedArgsType))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Entry parameter must be string[] when present. Got '{}'.",
                    funcType->paramTypes[0] ? funcType->paramTypes[0]->toString() : "<unknown>"
                );
            }
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
            for (const auto& instantiationTypes : funcSym->resolvedGenericInstantiations)
            {
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

                    const std::string instantiationSignature = formatConcreteInstantiationSignature(instantiationTypes);

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
                for (const auto& instantiationTypes : funcSym->resolvedGenericInstantiations)
                {
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

            if (auto conditionType = node.whenCondition->refType.Lock(); !isGuardConditionTypeAllowed(conditionType))
            {
                WIO_LOG_ADD_ERROR(
                    node.whenCondition->location(),
                    "When guard condition must be a boolean, numeric, or reference type. Got: {}",
                    conditionType->toString()
                );
            }

            if (node.whenFallback)
            {
                Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
                bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
                currentExpectedExpressionType_ = funcType->returnType;
                allowContextualNumericLiteralTyping_ = true;
                node.whenFallback->accept(*this);
                currentExpectedExpressionType_ = previousExpectedExpressionType;
                allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

                Ref<Type> fallbackType = node.whenFallback->refType.Lock();
                if (funcType->returnType &&
                    !funcType->returnType->isUnknown() &&
                    fallbackType &&
                    !fallbackType->isUnknown() &&
                    !isAssignmentLikeCompatible(funcType->returnType, fallbackType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.whenFallback->location(),
                        "When guard fallback type mismatch! Expected '{}', but got '{}'.",
                        funcType->returnType->toString(),
                        fallbackType->toString()
                    );
                }
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
        {
            if (isStructResolutionPass_)
            {
                if (statement->is<ComponentDeclaration>() ||
                    statement->is<ObjectDeclaration>() ||
                    statement->is<EnumDeclaration>() ||
                    statement->is<FlagsetDeclaration>() ||
                    statement->is<FlagDeclaration>() ||
                    statement->is<RealmDeclaration>())
                {
                    statement->accept(*this);
                }
            }
            else
            {
                statement->accept(*this);
            }
        }

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
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            validateApplyAttributes(*this, node.attributes, interfaceType.AsFast<StructType>()->genericParameterNames, "interface", node.location());
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> interfaceSym = createSymbol(node.name->token.value, interfaceType, SymbolKind::Struct, node.location());
            interfaceSym->innerScope = interfaceScope;
            interfaceSym->flags.set_isInterface(true);
            interfaceSym->genericParameterNames = interfaceType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, interfaceSym);
            attributeListsBySymbol_[interfaceSym.Get()] = &node.attributes;
            
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
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "component", node.location());
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> compSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            compSym->innerScope = structScope;
            compSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, compSym);
            attributeListsBySymbol_[compSym.Get()] = &node.attributes;
            
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

            AccessModifier defaultAccess = getDefaultAccessModifier(node.attributes, AccessModifier::Public);
            structType->trustedTypeKeys.clear();

            for (const auto& trustArg : getAttributeTypeArgs(node.attributes, Attribute::Trust))
            {
                auto trustedStruct = resolveTrustedStructType(*this, prevScope, trustArg, node.location());
                if (!trustedStruct)
                    continue;

                const std::string trustedKey = getStructIdentityKey(trustedStruct);
                if (!trustedKey.empty() &&
                    std::ranges::find(structType->trustedTypeKeys, trustedKey) == structType->trustedTypeKeys.end())
                {
                    structType->trustedTypeKeys.push_back(trustedKey);
                }
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

                    if (funcName != "OnConstruct" && funcName != "OnDestruct")
                    {
                        WIO_LOG_ADD_ERROR(
                            funcDecl->location(),
                            "Components cannot define ordinary methods. Use an object for behavior or OnConstruct/OnDestruct for lifecycle."
                        );
                    }
                    
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

            if (hasAttribute(node.attributes, Attribute::Export))
            {
                if (!node.genericParameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@Export is not yet supported for generic components.");
                }

                bool hasHostCallableConstructor = false;

                for (const auto& member : node.members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor =
                        functionType->paramTypes.size() == 1 &&
                        functionType->paramTypes[0]->kind() == TypeKind::Reference &&
                        functionType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType;
                    if (isCopyCtor)
                        continue;

                    if (member.access != AccessModifier::Public)
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export component '{}' constructor must be public to be host-callable.",
                            node.name->token.value
                        );
                    }

                    bool allParametersAbiSafe = true;
                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                            continue;

                        allParametersAbiSafe = false;
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export component '{}' constructor parameter {} uses non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            parameterIndex,
                            functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                        );
                    }

                    if (member.access == AccessModifier::Public && allParametersAbiSafe)
                        hasHostCallableConstructor = true;
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        hasHostCallableConstructor = true;

                    if (!hasMemberCtor && !memberTypes.empty())
                    {
                        bool memberCtorAbiSafe = true;
                        for (const auto& memberType : memberTypes)
                        {
                            if (isCAbiSafeExportType(memberType))
                                continue;

                            memberCtorAbiSafe = false;
                            break;
                        }

                        if (memberCtorAbiSafe)
                            hasHostCallableConstructor = true;
                    }
                }

                if (!hasHostCallableConstructor)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export component '{}' must expose at least one public host-callable constructor with only C-ABI-safe parameters.",
                        node.name->token.value
                    );
                }

                for (const auto& member : node.members)
                {
                    if (member.access != AccessModifier::Public || !member.declaration || !member.declaration->is<VariableDeclaration>())
                        continue;

                    auto variableDecl = member.declaration->as<VariableDeclaration>();
                    auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                    Ref<Type> fieldType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock();
                    if (!isSdkExportableFieldType(fieldType))
                    {
                        WIO_LOG_ADD_ERROR(
                            variableDecl->location(),
                            "@Export component '{}' exposes public field '{}' with type '{}' that is not yet SDK-exportable.",
                            node.name->token.value,
                            variableDecl->name->token.value,
                            fieldType ? fieldType->toString() : "<unknown>"
                        );
                    }
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
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "object", node.location());
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> objSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            objSym->innerScope = structScope;
            objSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            currentScope_->define(node.name->token.value, objSym);
            attributeListsBySymbol_[objSym.Get()] = &node.attributes;
            
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

            AccessModifier defaultAccess = getDefaultAccessModifier(node.attributes, AccessModifier::Private);
            structType->trustedTypeKeys.clear();

            for (const auto& trustArg : getAttributeTypeArgs(node.attributes, Attribute::Trust))
            {
                auto trustedStruct = resolveTrustedStructType(*this, prevScope, trustArg, node.location());
                if (!trustedStruct)
                    continue;

                const std::string trustedKey = getStructIdentityKey(trustedStruct);
                if (!trustedKey.empty() &&
                    std::ranges::find(structType->trustedTypeKeys, trustedKey) == structType->trustedTypeKeys.end())
                {
                    structType->trustedTypeKeys.push_back(trustedKey);
                }
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

            structType->baseTypes.clear();

            std::vector<Ref<StructType>> resolvedBaseTypes;
            resolvedBaseTypes.reserve(baseArgs.size());
            int structBaseCount = 0;
            for (const auto& baseArg : baseArgs)
            {
                if (auto baseType = resolveBaseType(baseArg))
                {
                    if (baseType->isInterface)
                    {
                        structType->baseTypes.emplace_back(baseType);
                        resolvedBaseTypes.push_back(baseType);
                        continue;
                    }

                    if (!baseType->isObject)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Object '{}' cannot inherit from component '{}'. Objects may inherit from one object and any number of interfaces.",
                            node.name->token.value,
                            baseArg.token.value
                        );
                        continue;
                    }

                    if (baseType->isFinal)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Object '{}' cannot inherit from final object '{}'.",
                            node.name->token.value,
                            baseArg.token.value
                        );
                        continue;
                    }

                    structBaseCount++;
                    if (structBaseCount > 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "Object '{}' cannot inherit from multiple objects/components. Single object inheritance only!", node.name->token.value);
                        continue;
                    }

                    structType->baseTypes.emplace_back(baseType);
                    resolvedBaseTypes.push_back(baseType);

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

            if (hasAttribute(node.attributes, Attribute::Export))
            {
                if (!node.genericParameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "@Export is not yet supported for generic objects.");
                }

                bool hasHostCallableConstructor = false;

                for (const auto& member : node.members)
                {
                    if (!member.declaration || !member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    if (!functionDecl || functionDecl->name->token.value != "OnConstruct")
                        continue;

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    const bool isCopyCtor =
                        functionType->paramTypes.size() == 1 &&
                        functionType->paramTypes[0]->kind() == TypeKind::Reference &&
                        functionType->paramTypes[0].AsFast<ReferenceType>()->referredType == structType;
                    if (isCopyCtor)
                        continue;

                    if (member.access != AccessModifier::Public)
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' constructor must be public to be host-callable.",
                            node.name->token.value
                        );
                    }

                    bool allParametersAbiSafe = true;
                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                            continue;

                        allParametersAbiSafe = false;
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' constructor parameter {} uses non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            parameterIndex,
                            functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                        );
                    }

                    if (member.access == AccessModifier::Public && allParametersAbiSafe)
                        hasHostCallableConstructor = true;
                }

                if ((!hasCustomCtor && !hasNoDefaultCtor) || forceGenerateCtors)
                {
                    if (!hasEmptyCtor)
                        hasHostCallableConstructor = true;

                    if (!hasMemberCtor && !memberTypes.empty())
                    {
                        bool memberCtorAbiSafe = true;
                        for (const auto& memberType : memberTypes)
                        {
                            if (isCAbiSafeExportType(memberType))
                                continue;

                            memberCtorAbiSafe = false;
                            break;
                        }

                        if (memberCtorAbiSafe)
                            hasHostCallableConstructor = true;
                    }
                }

                if (!hasHostCallableConstructor)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Export object '{}' must expose at least one public host-callable constructor with only C-ABI-safe parameters.",
                        node.name->token.value
                    );
                }

                for (const auto& member : node.members)
                {
                    if (member.access != AccessModifier::Public || !member.declaration)
                        continue;

                    if (member.declaration->is<VariableDeclaration>())
                    {
                        auto variableDecl = member.declaration->as<VariableDeclaration>();
                        auto variableSymbol = variableDecl->name ? variableDecl->name->referencedSymbol.Lock() : nullptr;
                        Ref<Type> fieldType = variableSymbol && variableSymbol->type ? variableSymbol->type : variableDecl->name->refType.Lock();
                        if (!isSdkExportableFieldType(fieldType))
                        {
                            WIO_LOG_ADD_ERROR(
                                variableDecl->location(),
                                "@Export object '{}' exposes public field '{}' with type '{}' that is not yet SDK-exportable.",
                                node.name->token.value,
                                variableDecl->name->token.value,
                                fieldType ? fieldType->toString() : "<unknown>"
                            );
                        }

                        continue;
                    }

                    if (!member.declaration->is<FunctionDeclaration>())
                        continue;

                    auto functionDecl = member.declaration->as<FunctionDeclaration>();
                    const std::string& functionName = functionDecl->name->token.value;
                    if (functionName == "OnConstruct" || functionName == "OnDestruct")
                        continue;

                    if (!functionDecl->genericParameters.empty())
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' public method '{}' cannot be generic yet.",
                            node.name->token.value,
                            functionName
                        );
                        continue;
                    }

                    auto functionSymbol = functionDecl->name ? functionDecl->name->referencedSymbol.Lock() : nullptr;
                    auto functionType = functionSymbol && functionSymbol->type ? functionSymbol->type.AsFast<FunctionType>() : nullptr;
                    if (!functionType)
                        continue;

                    for (size_t parameterIndex = 0; parameterIndex < functionType->paramTypes.size(); ++parameterIndex)
                    {
                        if (!isCAbiSafeExportType(functionType->paramTypes[parameterIndex]))
                        {
                            WIO_LOG_ADD_ERROR(
                                functionDecl->location(),
                                "@Export object '{}' public method '{}' parameter {} uses non-C-ABI-safe type '{}'.",
                                node.name->token.value,
                                functionName,
                                parameterIndex,
                                functionType->paramTypes[parameterIndex] ? functionType->paramTypes[parameterIndex]->toString() : "<unknown>"
                            );
                        }
                    }

                    if (!isCAbiSafeExportType(functionType->returnType))
                    {
                        WIO_LOG_ADD_ERROR(
                            functionDecl->location(),
                            "@Export object '{}' public method '{}' returns non-C-ABI-safe type '{}'.",
                            node.name->token.value,
                            functionName,
                            functionType->returnType ? functionType->returnType->toString() : "<unknown>"
                        );
                    }
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
                Ref<StructType> targetStruct = (typeSym && typeSym->kind == SymbolKind::Struct)
                    ? getObjectOrInterfaceStructType(typeSym->type)
                    : nullptr;

                if (binExpr->op.type == TokenType::kwIs && targetStruct)
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
            else
            {
                WIO_LOG_ADD_ERROR(node.matchVar.loc, "Pattern matching 'fit' can only be used with the 'is' operator (e.g., target is Boss fit t).");
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
                WIO_LOG_ADD_ERROR(node.location(), "Step clauses are currently supported only for range or array iteration.");

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
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = currentFunctionReturnType_;
            allowContextualNumericLiteralTyping_ = true;
            node.value->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
            actualType = node.value->refType.Lock();
        }

        if (currentFunctionReturnType_)
        {
            if (!currentFunctionReturnType_->isUnknown() &&
                actualType &&
                !actualType->isUnknown() &&
                !isAssignmentLikeCompatible(currentFunctionReturnType_, actualType))
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
