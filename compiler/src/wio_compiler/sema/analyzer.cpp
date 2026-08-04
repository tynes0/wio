#include "wio/sema/analyzer.h"

#include <array>
#include <cctype>
#include <functional>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_set>

#include "wio/codegen/mangler.h"
#include "wio/common/exception.h"
#include "wio/common/logger.h"
#include "wio/common/operator_overload.h"
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

        struct ValidationAnnotationSnapshot
        {
            struct NodeState
            {
                ASTNode* node;
                WeakRef<Type> refType;
            };

            struct ExpressionState
            {
                Expression* node;
                WeakRef<Symbol> referencedSymbol;
            };

            struct BinaryExpressionState
            {
                BinaryExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
            };

            struct UnaryExpressionState
            {
                UnaryExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
            };

            struct AssignmentExpressionState
            {
                AssignmentExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
            };

            struct ArrayAccessExpressionState
            {
                ArrayAccessExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
            };

            struct FitExpressionState
            {
                FitExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
            };

            struct FunctionCallExpressionState
            {
                FunctionCallExpression* node;
                OperatorDispatchKind operatorDispatchKind;
                WeakRef<Type> overloadFunctionType;
                std::vector<WeakRef<Type>> resolvedGenericArguments;
            };

            struct MemberAccessState
            {
                MemberAccessExpression* node;
                IntrinsicMember intrinsicMember;
                std::vector<IntrinsicMember> intrinsicOverloadMembers;
                std::vector<WeakRef<Type>> intrinsicOverloadTypes;
            };

            std::vector<NodeState> nodeStates;
            std::vector<ExpressionState> expressionStates;
            std::vector<BinaryExpressionState> binaryExpressionStates;
            std::vector<UnaryExpressionState> unaryExpressionStates;
            std::vector<AssignmentExpressionState> assignmentExpressionStates;
            std::vector<ArrayAccessExpressionState> arrayAccessExpressionStates;
            std::vector<FitExpressionState> fitExpressionStates;
            std::vector<FunctionCallExpressionState> functionCallExpressionStates;
            std::vector<MemberAccessState> memberAccessStates;

            void capture(ASTNode* root)
            {
                traverse(root);
            }

            void restore() const
            {
                for (const auto& state : nodeStates)
                    state.node->refType = state.refType;

                for (const auto& state : expressionStates)
                    state.node->referencedSymbol = state.referencedSymbol;

                for (const auto& state : binaryExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                }

                for (const auto& state : unaryExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                }

                for (const auto& state : assignmentExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                }

                for (const auto& state : arrayAccessExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                }

                for (const auto& state : fitExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                }

                for (const auto& state : functionCallExpressionStates)
                {
                    state.node->operatorDispatchKind = state.operatorDispatchKind;
                    state.node->overloadFunctionType = state.overloadFunctionType;
                    state.node->resolvedGenericArguments = state.resolvedGenericArguments;
                }

                for (const auto& state : memberAccessStates)
                {
                    state.node->intrinsicMember = state.intrinsicMember;
                    state.node->intrinsicOverloadMembers = state.intrinsicOverloadMembers;
                    state.node->intrinsicOverloadTypes = state.intrinsicOverloadTypes;
                }
            }

        private:
            template <typename T>
            void traverse(const Ref<T>& node)
            {
                traverse(node.Get());
            }

            void snapshot(ASTNode& node)
            {
                nodeStates.push_back(NodeState{ &node, node.refType });

                if (auto* expression = dynamic_cast<Expression*>(&node))
                    expressionStates.push_back(ExpressionState{ expression, expression->referencedSymbol });

                if (auto* binaryExpression = node.as<BinaryExpression>())
                {
                    binaryExpressionStates.push_back(BinaryExpressionState{
                        binaryExpression,
                        binaryExpression->operatorDispatchKind,
                        binaryExpression->overloadFunctionType
                    });
                }

                if (auto* unaryExpression = node.as<UnaryExpression>())
                {
                    unaryExpressionStates.push_back(UnaryExpressionState{
                        unaryExpression,
                        unaryExpression->operatorDispatchKind,
                        unaryExpression->overloadFunctionType
                    });
                }

                if (auto* assignmentExpression = node.as<AssignmentExpression>())
                {
                    assignmentExpressionStates.push_back(AssignmentExpressionState{
                        assignmentExpression,
                        assignmentExpression->operatorDispatchKind,
                        assignmentExpression->overloadFunctionType
                    });
                }

                if (auto* arrayAccessExpression = node.as<ArrayAccessExpression>())
                {
                    arrayAccessExpressionStates.push_back(ArrayAccessExpressionState{
                        arrayAccessExpression,
                        arrayAccessExpression->operatorDispatchKind,
                        arrayAccessExpression->overloadFunctionType
                    });
                }

                if (auto* fitExpression = node.as<FitExpression>())
                {
                    fitExpressionStates.push_back(FitExpressionState{
                        fitExpression,
                        fitExpression->operatorDispatchKind,
                        fitExpression->overloadFunctionType
                    });
                }

                if (auto* functionCallExpression = node.as<FunctionCallExpression>())
                {
                    functionCallExpressionStates.push_back(FunctionCallExpressionState{
                        functionCallExpression,
                        functionCallExpression->operatorDispatchKind,
                        functionCallExpression->overloadFunctionType,
                        functionCallExpression->resolvedGenericArguments
                    });
                }

                if (auto* memberAccess = node.as<MemberAccessExpression>())
                {
                    memberAccessStates.push_back(MemberAccessState{
                        memberAccess,
                        memberAccess->intrinsicMember,
                        memberAccess->intrinsicOverloadMembers,
                        memberAccess->intrinsicOverloadTypes
                    });
                }
            }

            void traverse(ASTNode* node)
            {
                if (!node)
                    return;

                snapshot(*node);

                switch (node->kind())
                {
                case NodeKind::Program:
                {
                    auto* current = node->as<Program>();
                    for (const auto& statement : current->statements)
                        traverse(statement);
                    break;
                }
                case NodeKind::TypeSpecifier:
                {
                    auto* current = node->as<TypeSpecifier>();
                    for (const auto& generic : current->generics)
                        traverse(generic);
                    traverse(current->packIndex);
                    break;
                }
                case NodeKind::BinaryExpression:
                {
                    auto* current = node->as<BinaryExpression>();
                    traverse(current->left);
                    traverse(current->right);
                    break;
                }
                case NodeKind::UnaryExpression:
                {
                    auto* current = node->as<UnaryExpression>();
                    traverse(current->operand);
                    break;
                }
                case NodeKind::AssignmentExpression:
                {
                    auto* current = node->as<AssignmentExpression>();
                    traverse(current->left);
                    traverse(current->right);
                    break;
                }
                case NodeKind::ConditionalExpression:
                {
                    auto* current = node->as<ConditionalExpression>();
                    traverse(current->condition);
                    traverse(current->whenTrue);
                    traverse(current->whenFalse);
                    break;
                }
                case NodeKind::IntegerLiteral:
                case NodeKind::FloatLiteral:
                case NodeKind::StringLiteral:
                case NodeKind::BoolLiteral:
                case NodeKind::CharLiteral:
                case NodeKind::ByteLiteral:
                case NodeKind::DurationLiteral:
                case NodeKind::Identifier:
                case NodeKind::NullExpression:
                case NodeKind::SelfExpression:
                case NodeKind::SuperExpression:
                case NodeKind::BreakStatement:
                case NodeKind::ContinueStatement:
                case NodeKind::UseStatement:
                    break;
                case NodeKind::InterpolatedStringLiteral:
                {
                    auto* current = node->as<InterpolatedStringLiteral>();
                    for (const auto& part : current->parts)
                        traverse(part);
                    break;
                }
                case NodeKind::ArrayLiteral:
                {
                    auto* current = node->as<ArrayLiteral>();
                    for (const auto& element : current->elements)
                        traverse(element);
                    break;
                }
                case NodeKind::DictionaryLiteral:
                {
                    auto* current = node->as<DictionaryLiteral>();
                    for (const auto& [key, value] : current->pairs)
                    {
                        traverse(key);
                        traverse(value);
                    }
                    break;
                }
                case NodeKind::TypeExpression:
                {
                    auto* current = node->as<TypeExpression>();
                    traverse(current->type);
                    break;
                }
                case NodeKind::ArrayAccessExpression:
                {
                    auto* current = node->as<ArrayAccessExpression>();
                    traverse(current->object);
                    traverse(current->index);
                    break;
                }
                case NodeKind::MemberAccessExpression:
                {
                    auto* current = node->as<MemberAccessExpression>();
                    traverse(current->object);
                    traverse(current->member);
                    break;
                }
                case NodeKind::FunctionCallExpression:
                {
                    auto* current = node->as<FunctionCallExpression>();
                    traverse(current->callee);
                    for (const auto& explicitTypeArgument : current->explicitTypeArguments)
                        traverse(explicitTypeArgument);
                    for (const auto& argument : current->arguments)
                        traverse(argument);
                    break;
                }
                case NodeKind::PackExpansionExpression:
                {
                    auto* current = node->as<PackExpansionExpression>();
                    traverse(current->operand);
                    break;
                }
                case NodeKind::LambdaExpression:
                {
                    auto* current = node->as<LambdaExpression>();
                    for (const auto& parameter : current->parameters)
                    {
                        traverse(parameter.name);
                        traverse(parameter.type);
                        traverse(parameter.defaultValue);
                    }
                    traverse(current->returnType);
                    traverse(current->body);
                    break;
                }
                case NodeKind::RefExpression:
                {
                    auto* current = node->as<RefExpression>();
                    traverse(current->operand);
                    break;
                }
                case NodeKind::FitExpression:
                {
                    auto* current = node->as<FitExpression>();
                    traverse(current->operand);
                    traverse(current->targetType);
                    break;
                }
                case NodeKind::RangeExpression:
                {
                    auto* current = node->as<RangeExpression>();
                    traverse(current->start);
                    traverse(current->end);
                    break;
                }
                case NodeKind::MatchExpression:
                {
                    auto* current = node->as<MatchExpression>();
                    traverse(current->value);
                    for (const auto& matchCase : current->cases)
                    {
                        for (const auto& matchValue : matchCase.matchValues)
                            traverse(matchValue);
                        traverse(matchCase.body);
                    }
                    break;
                }
                case NodeKind::ExpressionStatement:
                {
                    auto* current = node->as<ExpressionStatement>();
                    traverse(current->expression);
                    break;
                }
                case NodeKind::AttributeStatement:
                {
                    auto* current = node->as<AttributeStatement>();
                    for (const auto& typeArg : current->typeArgs)
                        traverse(typeArg);
                    break;
                }
                case NodeKind::VariableDeclaration:
                {
                    auto* current = node->as<VariableDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    traverse(current->type);
                    traverse(current->initializer);
                    break;
                }
                case NodeKind::TypeAliasDeclaration:
                {
                    auto* current = node->as<TypeAliasDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    for (const auto& genericParameter : current->genericParameters)
                        traverse(genericParameter);
                    traverse(current->aliasedType);
                    break;
                }
                case NodeKind::FunctionDeclaration:
                {
                    auto* current = node->as<FunctionDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    for (const auto& genericParameter : current->genericParameters)
                        traverse(genericParameter);
                    for (const auto& parameter : current->parameters)
                    {
                        traverse(parameter.name);
                        traverse(parameter.type);
                        traverse(parameter.defaultValue);
                    }
                    traverse(current->returnType);
                    traverse(current->whenCondition);
                    traverse(current->whenFallback);
                    traverse(current->body);
                    break;
                }
                case NodeKind::InterfaceDeclaration:
                {
                    auto* current = node->as<InterfaceDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    for (const auto& genericParameter : current->genericParameters)
                        traverse(genericParameter);
                    for (const auto& method : current->methods)
                        traverse(method);
                    break;
                }
                case NodeKind::ComponentDeclaration:
                {
                    auto* current = node->as<ComponentDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    for (const auto& genericParameter : current->genericParameters)
                        traverse(genericParameter);
                    for (const auto& member : current->members)
                    {
                        for (const auto& attribute : member.attributes)
                            traverse(attribute);
                        traverse(member.declaration);
                    }
                    break;
                }
                case NodeKind::ExtensionDeclaration:
                {
                    auto* current = node->as<ExtensionDeclaration>();
                    traverse(current->name);
                    traverse(current->targetType);
                    for (const auto& member : current->members)
                        traverse(member.method);
                    break;
                }
                case NodeKind::ObjectDeclaration:
                {
                    auto* current = node->as<ObjectDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    for (const auto& genericParameter : current->genericParameters)
                        traverse(genericParameter);
                    for (const auto& member : current->members)
                    {
                        for (const auto& attribute : member.attributes)
                            traverse(attribute);
                        traverse(member.declaration);
                    }
                    break;
                }
                case NodeKind::EnumDeclaration:
                case NodeKind::FlagsetDeclaration:
                {
                    const auto traverseEnumLike = [&](auto* current)
                    {
                        for (const auto& attribute : current->attributes)
                            traverse(attribute);
                        traverse(current->name);
                        for (const auto& member : current->members)
                        {
                            traverse(member.name);
                            traverse(member.value);
                        }
                    };

                    if (node->kind() == NodeKind::EnumDeclaration)
                        traverseEnumLike(node->as<EnumDeclaration>());
                    else
                        traverseEnumLike(node->as<FlagsetDeclaration>());
                    break;
                }
                case NodeKind::FlagDeclaration:
                {
                    auto* current = node->as<FlagDeclaration>();
                    for (const auto& attribute : current->attributes)
                        traverse(attribute);
                    traverse(current->name);
                    break;
                }
                case NodeKind::BlockStatement:
                {
                    auto* current = node->as<BlockStatement>();
                    for (const auto& statement : current->statements)
                        traverse(statement);
                    break;
                }
                case NodeKind::IfStatement:
                {
                    auto* current = node->as<IfStatement>();
                    traverse(current->condition);
                    traverse(current->thenBranch);
                    traverse(current->elseBranch);
                    break;
                }
                case NodeKind::WhileStatement:
                {
                    auto* current = node->as<WhileStatement>();
                    traverse(current->condition);
                    traverse(current->body);
                    break;
                }
                case NodeKind::ForInStatement:
                {
                    auto* current = node->as<ForInStatement>();
                    for (const auto& binding : current->bindings)
                        traverse(binding);
                    traverse(current->iterable);
                    traverse(current->step);
                    traverse(current->body);
                    break;
                }
                case NodeKind::CForStatement:
                {
                    auto* current = node->as<CForStatement>();
                    traverse(current->initializer);
                    traverse(current->condition);
                    traverse(current->increment);
                    traverse(current->body);
                    break;
                }
                case NodeKind::ReturnStatement:
                {
                    auto* current = node->as<ReturnStatement>();
                    traverse(current->value);
                    break;
                }
                case NodeKind::RealmDeclaration:
                {
                    auto* current = node->as<RealmDeclaration>();
                    traverse(current->name);
                    for (const auto& statement : current->statements)
                        traverse(statement);
                    break;
                }
                case NodeKind::Unknown:
                    break;
                }
            }
        };

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

        size_t getFixedParameterCount(const FunctionDeclaration& node)
        {
            const bool hasParameterPack = std::ranges::any_of(node.parameters, [](const Parameter& parameter)
            {
                return parameter.isParameterPack;
            });

            if (hasParameterPack && !node.parameters.empty())
                return node.parameters.size() - 1;

            return node.parameters.size();
        }

        size_t getRequiredParameterCount(const FunctionDeclaration& node)
        {
            size_t requiredCount = getFixedParameterCount(node);
            while (requiredCount > 0 && node.parameters[requiredCount - 1].defaultValue)
                --requiredCount;

            return requiredCount;
        }

        size_t getRequiredParameterCount(const FunctionDeclaration* node)
        {
            return node ? getRequiredParameterCount(*node) : 0;
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

        Ref<Type> unwrapAliasType(Ref<Type> type)
        {
            while (type && type->kind() == TypeKind::Alias)
                type = type.AsFast<AliasType>()->aliasedType;

            return type;
        }

        bool isNullableCapableType(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->isUnknown() || current->kind() == TypeKind::Nullable)
                return false;

            if (current->kind() == TypeKind::Reference || current->kind() == TypeKind::Function)
                return true;

            if (current->kind() == TypeKind::Primitive)
                return current.AsFast<PrimitiveType>()->name == "opaque";

            if (current->kind() == TypeKind::Struct)
            {
                auto structType = current.AsFast<StructType>();
                return structType->isObject || structType->isInterface;
            }

            return false;
        }

        bool requiresExplicitNonNullInitialization(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->isUnknown() || current->kind() == TypeKind::Nullable)
                return false;
            if (current->kind() == TypeKind::Reference || current->kind() == TypeKind::Function)
                return true;
            if (current->kind() == TypeKind::Primitive)
                return current.AsFast<PrimitiveType>()->name == "opaque";
            if (current->kind() == TypeKind::Struct)
            {
                auto structType = current.AsFast<StructType>();
                return structType->isObject || structType->isInterface;
            }
            return false;
        }

        bool isMutableReferenceTypeChain(Ref<Type> type)
        {
            Ref<Type> current = unwrapAliasType(type);
            bool sawReference = false;

            while (current && current->kind() == TypeKind::Reference)
            {
                sawReference = true;

                auto referenceType = current.AsFast<ReferenceType>();
                if (!referenceType->isMutable)
                    return false;

                current = unwrapAliasType(referenceType->referredType);
            }

            return sawReference;
        }

        bool canMutateIntrinsicReceiver(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (auto memberAccess = expression->as<MemberAccessExpression>())
            {
                if (auto memberSymbol = memberAccess->referencedSymbol.Lock(); memberSymbol)
                {
                    if ((memberSymbol->kind == SymbolKind::Variable || memberSymbol->kind == SymbolKind::Parameter) &&
                        canMutateIntrinsicReceiver(memberAccess->object))
                    {
                        return true;
                    }
                }
            }

            if (auto receiverSymbol = expression->referencedSymbol.Lock(); receiverSymbol)
            {
                if (receiverSymbol->flags.get_isMutable())
                    return true;

                if (isMutableReferenceTypeChain(receiverSymbol->type))
                    return true;
            }

            return isMutableReferenceTypeChain(expression->refType.Lock());
        }

        bool isMutableAddressableOperand(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return false;

            if (auto receiverSymbol = expression->referencedSymbol.Lock(); receiverSymbol)
            {
                if (receiverSymbol->flags.get_isMutable())
                    return true;

                if (isMutableReferenceTypeChain(receiverSymbol->type))
                    return true;
            }

            if (expression->is<ArrayAccessExpression>())
            {
                auto* arrayAccess = expression->as<ArrayAccessExpression>();
                return isMutableAddressableOperand(arrayAccess->object) ||
                       isMutableReferenceTypeChain(arrayAccess->object ? arrayAccess->object->refType.Lock() : nullptr);
            }

            if (auto* memberAccess = expression->as<MemberAccessExpression>())
            {
                if (auto memberSymbol = memberAccess->referencedSymbol.Lock(); memberSymbol)
                {
                    if (memberSymbol->flags.get_isMutable())
                        return true;

                    if (memberSymbol->flags.get_isReadOnly())
                        return false;
                }

                return isMutableAddressableOperand(memberAccess->object) ||
                       isMutableReferenceTypeChain(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
            }

            return isMutableReferenceTypeChain(expression->refType.Lock());
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

        bool isOpaqueType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "opaque";
        }

        bool isAnyType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            return resolvedType &&
                   resolvedType->kind() == TypeKind::Primitive &&
                   resolvedType.AsFast<PrimitiveType>()->name == "any";
        }

        bool isConcreteObjectTypeSupportedByAny(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->kind() != TypeKind::Struct)
                return false;

            auto structType = resolvedType.AsFast<StructType>();
            return structType && structType->isObject && !structType->isInterface;
        }

        bool isStorableInAny(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->isUnknown())
                return false;

            switch (resolvedType->kind())
            {
            case TypeKind::Primitive:
            {
                const std::string& primitiveName = resolvedType.AsFast<PrimitiveType>()->name;
                return primitiveName != "void" &&
                       primitiveName != "<unknown>";
            }
            case TypeKind::Array:
            case TypeKind::Dictionary:
                return true;
            case TypeKind::Struct:
            {
                auto structType = resolvedType.AsFast<StructType>();
                if (!structType)
                    return false;

                return true;
            }
            default:
                return false;
            }
        }

        bool isSupportedAnyCastTargetType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType || resolvedType->isUnknown())
                return false;

            if (isAnyType(resolvedType))
                return false;

            if (resolvedType->kind() == TypeKind::Struct)
            {
                auto structType = resolvedType.AsFast<StructType>();
                if (structType && structType->isInterface)
                    return true;
            }

            return isStorableInAny(resolvedType);
        }

        bool isIntrinsicReceiverType(const Ref<Type>& type)
        {
            Ref<Type> resolvedType = unwrapAliasType(type);
            if (!resolvedType)
                return false;

            return resolvedType->kind() == TypeKind::Array ||
                   resolvedType->kind() == TypeKind::Dictionary ||
                   (resolvedType->kind() == TypeKind::Struct &&
                    (resolvedType.AsFast<StructType>()->isEnum || resolvedType.AsFast<StructType>()->isFlagset)) ||
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

        Ref<Type> getCommonNumericType(const Ref<Type>& left, const Ref<Type>& right)
        {
            Ref<Type> lhs = unwrapAliasType(left);
            Ref<Type> rhs = unwrapAliasType(right);
            if (!lhs || !rhs || !lhs->isNumeric() || !rhs->isNumeric())
                return nullptr;

            if (lhs.Get() == rhs.Get())
                return lhs;

            struct NumericInfo
            {
                int bits = 0;
                bool isSigned = false;
                bool isFloat = false;
                bool isSize = false;
            };

            auto describe = [](const Ref<Type>& type) -> NumericInfo
            {
                const std::string& name = type.AsFast<PrimitiveType>()->name;
                if (name == "f32") return {32, true, true, false};
                if (name == "f64") return {64, true, true, false};
                if (name == "isize") return {64, true, false, true};
                if (name == "usize") return {64, false, false, true};
                const bool isSigned = name.starts_with('i');
                if (name.ends_with("8")) return {8, isSigned, false, false};
                if (name.ends_with("16")) return {16, isSigned, false, false};
                if (name.ends_with("32")) return {32, isSigned, false, false};
                return {64, isSigned, false, false};
            };

            const NumericInfo lhsInfo = describe(lhs);
            const NumericInfo rhsInfo = describe(rhs);
            auto& types = Compiler::get().getTypeContext();

            if (lhsInfo.isFloat || rhsInfo.isFloat)
            {
                const bool hasF64 =
                    (lhsInfo.isFloat && lhsInfo.bits == 64) ||
                    (rhsInfo.isFloat && rhsInfo.bits == 64);
                return hasF64 ? types.getF64() : types.getF32();
            }

            const NumericInfo* resultInfo = nullptr;
            if (lhsInfo.isSigned == rhsInfo.isSigned)
                resultInfo = lhsInfo.bits >= rhsInfo.bits ? &lhsInfo : &rhsInfo;
            else
            {
                const NumericInfo& signedInfo = lhsInfo.isSigned ? lhsInfo : rhsInfo;
                const NumericInfo& unsignedInfo = lhsInfo.isSigned ? rhsInfo : lhsInfo;
                resultInfo = signedInfo.bits > unsignedInfo.bits ? &signedInfo : &unsignedInfo;
            }

            const int bits = resultInfo->bits;
            const bool useSizeType = resultInfo->isSize && bits == 64;
            if (resultInfo->isSigned)
            {
                if (useSizeType) return types.getISize();
                if (bits <= 8) return types.getI8();
                if (bits <= 16) return types.getI16();
                if (bits <= 32) return types.getI32();
                return types.getI64();
            }

            if (useSizeType) return types.getUSize();
            if (bits <= 8) return types.getU8();
            if (bits <= 16) return types.getU16();
            if (bits <= 32) return types.getU32();
            return types.getU64();
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

        struct NumericConversionInfo
        {
            int bits = 0;
            bool isSigned = false;
            bool isFloat = false;
        };

        std::optional<NumericConversionInfo> describeNumericConversionType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive || !resolved->isNumeric())
                return std::nullopt;

            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            if (name == "f32") return NumericConversionInfo{32, true, true};
            if (name == "f64") return NumericConversionInfo{64, true, true};
            if (name == "isize") return NumericConversionInfo{64, true, false};
            if (name == "usize") return NumericConversionInfo{64, false, false};
            if (name == "byte") return NumericConversionInfo{8, false, false};
            const bool isSigned = name.starts_with('i');
            if (name.ends_with("8")) return NumericConversionInfo{8, isSigned, false};
            if (name.ends_with("16")) return NumericConversionInfo{16, isSigned, false};
            if (name.ends_with("32")) return NumericConversionInfo{32, isSigned, false};
            if (name.ends_with("64")) return NumericConversionInfo{64, isSigned, false};
            return std::nullopt;
        }

        bool isSafeImplicitNumericConversion(const Ref<Type>& destination, const Ref<Type>& source)
        {
            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            if (!resolvedDestination || !resolvedSource)
                return false;
            if (resolvedDestination.Get() == resolvedSource.Get())
                return true;

            const auto destinationInfo = describeNumericConversionType(resolvedDestination);
            const auto sourceInfo = describeNumericConversionType(resolvedSource);
            if (!destinationInfo || !sourceInfo)
                return false;

            if (destinationInfo->isFloat)
            {
                if (sourceInfo->isFloat)
                    return destinationInfo->bits >= sourceInfo->bits;

                // IEEE-754 f32/f64 exactly represent all integers with at most
                // 24/53 value bits respectively. Wider integer conversions must
                // be explicit because they may silently lose precision.
                const int exactIntegerBits = destinationInfo->bits == 32 ? 24 : 53;
                const int sourceValueBits = sourceInfo->bits - (sourceInfo->isSigned ? 1 : 0);
                return sourceValueBits <= exactIntegerBits;
            }

            if (sourceInfo->isFloat)
                return false;

            if (destinationInfo->isSigned == sourceInfo->isSigned)
                return destinationInfo->bits >= sourceInfo->bits;
            if (destinationInfo->isSigned && !sourceInfo->isSigned)
                return destinationInfo->bits > sourceInfo->bits;
            return false;
        }

        bool isRejectedImplicitNumericConversion(const Ref<Type>& destination, const Ref<Type>& source)
        {
            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            return resolvedDestination && resolvedSource &&
                   resolvedDestination->isNumeric() && resolvedSource->isNumeric() &&
                   !isSafeImplicitNumericConversion(resolvedDestination, resolvedSource);
        }

        std::optional<std::string> tryGetNormalizedSymbolicPackName(const Ref<Type>& type);

        bool isAssignmentLikeCompatible(const Ref<Type>& destination, const Ref<Type>& source)
        {
            if (!destination || !source || source->isUnknown())
                return false;

            Ref<Type> resolvedDestination = unwrapAliasType(destination);
            Ref<Type> resolvedSource = unwrapAliasType(source);
            if (resolvedDestination && resolvedSource)
            {
                auto getPackElementTypes = [](const Ref<Type>& type) -> const std::vector<Ref<Type>>*
                {
                    if (!type)
                        return nullptr;

                    auto nested = [&](const std::vector<Ref<Type>>& elementTypes) -> const std::vector<Ref<Type>>*
                    {
                        if (elementTypes.size() == 1)
                        {
                            Ref<Type> nestedType = unwrapAliasType(elementTypes.front());
                            if (!nestedType)
                                return &elementTypes;

                            switch (nestedType->kind())
                            {
                            case TypeKind::ValuePackView:
                                if (!nestedType.AsFast<ValuePackViewType>()->elementTypes.empty())
                                    return &nestedType.AsFast<ValuePackViewType>()->elementTypes;
                                break;
                            case TypeKind::TypePackView:
                                if (!nestedType.AsFast<TypePackViewType>()->elementTypes.empty())
                                    return &nestedType.AsFast<TypePackViewType>()->elementTypes;
                                break;
                            case TypeKind::PackStorage:
                                if (!nestedType.AsFast<PackStorageType>()->elementTypes.empty())
                                    return &nestedType.AsFast<PackStorageType>()->elementTypes;
                                break;
                            default:
                                break;
                            }
                        }

                        return &elementTypes;
                    };

                    switch (type->kind())
                    {
                    case TypeKind::ValuePackView:
                        return nested(type.AsFast<ValuePackViewType>()->elementTypes);
                    case TypeKind::TypePackView:
                        return nested(type.AsFast<TypePackViewType>()->elementTypes);
                    case TypeKind::PackStorage:
                        return nested(type.AsFast<PackStorageType>()->elementTypes);
                    default:
                        return nullptr;
                    }
                };

                auto formatConcreteTypePackName = [](const std::vector<Ref<Type>>& elementTypes) -> std::string
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
                };

                const auto destinationPackName = tryGetNormalizedSymbolicPackName(resolvedDestination);
                const auto sourcePackName = tryGetNormalizedSymbolicPackName(resolvedSource);
                if (destinationPackName.has_value() && sourcePackName.has_value())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *destinationPackName == *sourcePackName)
                        return true;
                }

                const auto destinationElements = getPackElementTypes(resolvedDestination);
                const auto sourceElements = getPackElementTypes(resolvedSource);
                if (destinationPackName.has_value() && sourceElements && !sourceElements->empty())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *destinationPackName == formatConcreteTypePackName(*sourceElements))
                        return true;
                }

                if (sourcePackName.has_value() && destinationElements && !destinationElements->empty())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment && *sourcePackName == formatConcreteTypePackName(*destinationElements))
                        return true;
                }

                if (destinationElements && sourceElements &&
                    !destinationElements->empty() &&
                    destinationElements->size() == sourceElements->size())
                {
                    const auto destinationKind = resolvedDestination->kind();
                    const auto sourceKind = resolvedSource->kind();
                    const bool isPackAssignment =
                        (destinationKind == TypeKind::PackStorage &&
                         (sourceKind == TypeKind::ValuePackView || sourceKind == TypeKind::PackStorage)) ||
                        (destinationKind == TypeKind::ValuePackView && sourceKind == TypeKind::ValuePackView) ||
                        (destinationKind == TypeKind::TypePackView && sourceKind == TypeKind::TypePackView);

                    if (isPackAssignment)
                    {
                        bool allElementsCompatible = true;
                        for (size_t i = 0; i < destinationElements->size(); ++i)
                        {
                            if (!isAssignmentLikeCompatible((*destinationElements)[i], (*sourceElements)[i]))
                            {
                                allElementsCompatible = false;
                                break;
                            }
                        }

                        if (allElementsCompatible)
                            return true;
                    }
                }
            }

            if (destination->isCompatibleWith(source) ||
                isSafeImplicitNumericConversion(destination, source))
            {
                return true;
            }

            if (shouldAutoReadReferenceType(source))
            {
                Ref<Type> readableSource = getAutoReadableType(source);
                if (readableSource &&
                    (destination->isCompatibleWith(readableSource) ||
                     isSafeImplicitNumericConversion(destination, readableSource)))
                {
                    return true;
                }
            }

            return false;
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
            if (!resolved)
                return false;

            if (resolved->kind() == TypeKind::Struct)
            {
                auto structType = resolved.AsFast<StructType>();
                if (!structType || structType->isObject || structType->isInterface)
                    return false;

                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto structSymbol = structScope->resolve(structType->name))
                        return structSymbol->flags.get_isEnum() || structSymbol->flags.get_isFlagset();
                }

                return false;
            }

            if (resolved->kind() != TypeKind::Primitive)
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
                expression->is<FunctionCallExpression>())
            {
                return false;
            }

            if (const auto* identifier = expression->as<Identifier>())
            {
                auto symbol = identifier->referencedSymbol.Lock();
                return symbol && symbol->flags.get_isConst();
            }

            if (const auto* memberAccess = expression->as<MemberAccessExpression>())
            {
                auto memberSymbol = memberAccess->referencedSymbol.Lock();
                auto ownerSymbol = memberAccess->object ? memberAccess->object->referencedSymbol.Lock() : nullptr;
                if (!memberSymbol || !ownerSymbol)
                    return false;

                return memberSymbol->flags.get_isReadOnly() &&
                       memberSymbol->kind == SymbolKind::Variable &&
                       ownerSymbol->kind == SymbolKind::Struct &&
                       (ownerSymbol->flags.get_isEnum() || ownerSymbol->flags.get_isFlagset());
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

        using ConstVariableDeclarationMap = std::unordered_map<const Symbol*, const VariableDeclaration*>;

        std::optional<int64_t> tryEvaluateStaticIntegerExpression(
            const NodePtr<Expression>& expression,
            const ConstVariableDeclarationMap& variableDeclarationsBySymbol,
            std::unordered_set<const Symbol*>& activeSymbols)
        {
            if (!expression)
                return std::nullopt;

            auto convertIntegerLiteral = [](const IntegerLiteral& literal) -> std::optional<int64_t>
            {
                const IntegerResult result = common::getInteger(literal.token.value);
                if (!result.isValid)
                    return std::nullopt;

                switch (result.type)
                {
                case IntegerType::i8: return static_cast<int64_t>(result.value.v_i8);
                case IntegerType::i16: return static_cast<int64_t>(result.value.v_i16);
                case IntegerType::i32: return static_cast<int64_t>(result.value.v_i32);
                case IntegerType::i64: return result.value.v_i64;
                case IntegerType::u8: return static_cast<int64_t>(result.value.v_u8);
                case IntegerType::u16: return static_cast<int64_t>(result.value.v_u16);
                case IntegerType::u32: return static_cast<int64_t>(result.value.v_u32);
                case IntegerType::u64:
                    if (result.value.v_u64 > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
                        return std::nullopt;
                    return static_cast<int64_t>(result.value.v_u64);
                case IntegerType::isize: return static_cast<int64_t>(result.value.v_isize);
                case IntegerType::usize:
                    if (result.value.v_usize > static_cast<size_t>(std::numeric_limits<int64_t>::max()))
                        return std::nullopt;
                    return static_cast<int64_t>(result.value.v_usize);
                case IntegerType::Unknown: return std::nullopt;
                }

                return std::nullopt;
            };

            if (const auto* literal = expression->as<IntegerLiteral>())
                return convertIntegerLiteral(*literal);

            if (const auto* identifier = expression->as<Identifier>())
            {
                Ref<Symbol> symbol = identifier->referencedSymbol.Lock();
                if (!symbol || !symbol->flags.get_isConst())
                    return std::nullopt;

                auto declarationIt = variableDeclarationsBySymbol.find(symbol.Get());
                if (declarationIt == variableDeclarationsBySymbol.end() || !declarationIt->second || !declarationIt->second->initializer)
                    return std::nullopt;

                if (!activeSymbols.insert(symbol.Get()).second)
                    return std::nullopt;

                auto value = tryEvaluateStaticIntegerExpression(
                    declarationIt->second->initializer,
                    variableDeclarationsBySymbol,
                    activeSymbols
                );
                activeSymbols.erase(symbol.Get());
                return value;
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                auto operandValue = tryEvaluateStaticIntegerExpression(
                    unary->operand,
                    variableDeclarationsBySymbol,
                    activeSymbols
                );
                if (!operandValue.has_value())
                    return std::nullopt;

                switch (unary->op.type)
                {
                case TokenType::opPlus:
                    return *operandValue;
                case TokenType::opMinus:
                    if (*operandValue == std::numeric_limits<int64_t>::min())
                        return std::nullopt;
                    return -*operandValue;
                case TokenType::opBitNot:
                    return ~*operandValue;
                default:
                    return std::nullopt;
                }
            }

            if (const auto* fit = expression->as<FitExpression>())
            {
                return tryEvaluateStaticIntegerExpression(
                    fit->operand,
                    variableDeclarationsBySymbol,
                    activeSymbols
                );
            }

            if (const auto* binary = expression->as<BinaryExpression>())
            {
                auto lhs = tryEvaluateStaticIntegerExpression(binary->left, variableDeclarationsBySymbol, activeSymbols);
                auto rhs = tryEvaluateStaticIntegerExpression(binary->right, variableDeclarationsBySymbol, activeSymbols);
                if (!lhs.has_value() || !rhs.has_value())
                    return std::nullopt;

                switch (binary->op.type)
                {
                case TokenType::opPlus: return *lhs + *rhs;
                case TokenType::opMinus: return *lhs - *rhs;
                case TokenType::opStar: return *lhs * *rhs;
                case TokenType::opSlash:
                    if (*rhs == 0)
                        return std::nullopt;
                    return *lhs / *rhs;
                case TokenType::opPercent:
                    if (*rhs == 0)
                        return std::nullopt;
                    return *lhs % *rhs;
                case TokenType::opBitAnd: return *lhs & *rhs;
                case TokenType::opBitOr: return *lhs | *rhs;
                case TokenType::opBitXor: return *lhs ^ *rhs;
                case TokenType::opShiftLeft:
                    if (*rhs < 0 || *rhs >= 63)
                        return std::nullopt;
                    return *lhs << *rhs;
                case TokenType::opShiftRight:
                    if (*rhs < 0 || *rhs >= 63)
                        return std::nullopt;
                    return *lhs >> *rhs;
                default:
                    return std::nullopt;
                }
            }

            return std::nullopt;
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
            {
                auto* arrayAccess = expression->as<ArrayAccessExpression>();
                if (arrayAccess->operatorDispatchKind == OperatorDispatchKind::None)
                    return true;

                Ref<Type> indexedType = unwrapAliasType(arrayAccess->refType.Lock());
                return indexedType && indexedType->kind() == TypeKind::Reference;
            }

            if (expression->is<FunctionCallExpression>())
            {
                Ref<Type> callType = unwrapAliasType(expression->refType.Lock());
                return callType && callType->kind() == TypeKind::Reference;
            }

            if (expression->is<Identifier>() || expression->is<MemberAccessExpression>())
                return isVariableLikeSymbol(expression->referencedSymbol.Lock());

            return false;
        }

        BorrowOrigin classifyBorrowOrigin(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return BorrowOrigin::Temporary;

            if (expression->borrowOrigin != BorrowOrigin::None)
                return expression->borrowOrigin;

            if (expression->is<SelfExpression>() || expression->is<SuperExpression>())
                return BorrowOrigin::Caller;

            // A selected member or indexed element is owned by its receiver,
            // not by the field symbol recorded on the access expression.
            if (const auto* memberAccess = expression->as<MemberAccessExpression>())
                return classifyBorrowOrigin(memberAccess->object);
            if (const auto* arrayAccess = expression->as<ArrayAccessExpression>())
                return classifyBorrowOrigin(arrayAccess->object);
            if (const auto* refExpression = expression->as<RefExpression>())
                return classifyBorrowOrigin(refExpression->operand);

            if (const auto symbol = expression->referencedSymbol.Lock(); symbol)
            {
                if (symbol->flags.get_isGlobal())
                    return BorrowOrigin::Static;
                if (symbol->kind == SymbolKind::Parameter && symbol->type && symbol->type->kind() == TypeKind::Reference)
                    return BorrowOrigin::Caller;
                if (symbol->kind == SymbolKind::Variable || symbol->kind == SymbolKind::Parameter)
                    return BorrowOrigin::Local;
            }

            return BorrowOrigin::Temporary;
        }

        std::string_view borrowOriginName(BorrowOrigin origin)
        {
            switch (origin)
            {
            case BorrowOrigin::Static: return "static storage";
            case BorrowOrigin::Caller: return "caller-owned storage";
            case BorrowOrigin::Local: return "a local value";
            case BorrowOrigin::Temporary: return "a temporary value";
            case BorrowOrigin::None: return "an untracked value";
            }
            return "an unknown value";
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

        struct GenericBindingSet
        {
            std::unordered_map<std::string, Ref<Type>> directBindings;
            std::unordered_map<std::string, std::vector<Ref<Type>>> packBindings;
            std::unordered_map<std::string, std::string> packAliases;
        };

        size_t getMinimumGenericArgumentCount(const std::vector<std::string>& parameterNames, bool hasGenericParameterPack);
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

        std::string makePackElementBindingName(const std::string& packName, const size_t index)
        {
            return common::formatString("{}[{}]", packName, index);
        }

        std::string makePackTailElementBindingName(const std::string& packName, const size_t distanceFromEnd)
        {
            if (distanceFromEnd <= 1)
                return common::formatString("{}[last]", packName);

            return common::formatString("{}[last-{}]", packName, distanceFromEnd - 1);
        }

        enum class PackElementBindingKind : uint8_t
        {
            Absolute,
            FromEnd
        };

        struct ParsedPackElementBinding
        {
            std::string packName;
            size_t value = 0;
            PackElementBindingKind kind = PackElementBindingKind::Absolute;
        };

        std::optional<ParsedPackElementBinding> tryParsePackElementBindingName(const std::string_view name)
        {
            const size_t openBracket = name.find('[');
            if (openBracket == std::string_view::npos || !name.ends_with("]"))
                return std::nullopt;

            const std::string_view packName = name.substr(0, openBracket);
            const std::string_view indexText = name.substr(openBracket + 1, name.size() - openBracket - 2);
            if (packName.empty() || indexText.empty())
                return std::nullopt;

            if (indexText == "last")
                return ParsedPackElementBinding{std::string(packName), 1, PackElementBindingKind::FromEnd};

            if (indexText.starts_with("last-"))
            {
                const std::string_view offsetText = indexText.substr(5);
                if (offsetText.empty())
                    return std::nullopt;

                size_t offset = 0;
                for (const char ch : offsetText)
                {
                    if (ch < '0' || ch > '9')
                        return std::nullopt;

                    offset = (offset * 10) + static_cast<size_t>(ch - '0');
                }

                return ParsedPackElementBinding{
                    std::string(packName),
                    offset + 1,
                    PackElementBindingKind::FromEnd
                };
            }

            size_t index = 0;
            for (const char ch : indexText)
            {
                if (ch < '0' || ch > '9')
                    return std::nullopt;

                index = (index * 10) + static_cast<size_t>(ch - '0');
            }

            return ParsedPackElementBinding{
                std::string(packName),
                index,
                PackElementBindingKind::Absolute
            };
        }

        std::string makePackElementBindingName(const ParsedPackElementBinding& binding)
        {
            if (binding.kind == PackElementBindingKind::FromEnd)
                return makePackTailElementBindingName(binding.packName, binding.value);

            return makePackElementBindingName(binding.packName, binding.value);
        }

        std::optional<size_t> tryResolveConcretePackElementIndex(const ParsedPackElementBinding& binding, const size_t packSize)
        {
            if (binding.kind == PackElementBindingKind::Absolute)
            {
                if (binding.value >= packSize)
                    return std::nullopt;

                return binding.value;
            }

            if (binding.value == 0 || binding.value > packSize)
                return std::nullopt;

            return packSize - binding.value;
        }

        std::optional<std::string> tryGetSymbolicPackReferenceName(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return std::nullopt;

            switch (current->kind())
            {
            case TypeKind::GenericParameterPack:
                return current.AsFast<GenericParameterPackType>()->name;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (packViewType->elementTypes.empty())
                    return packViewType->packName;
                return std::nullopt;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (packViewType->elementTypes.empty())
                    return packViewType->packName;
                return std::nullopt;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (storageType->elementTypes.empty())
                    return storageType->packName;
                return std::nullopt;
            }
            default:
                return std::nullopt;
            }
        }

        std::optional<std::string> tryGetNormalizedSymbolicPackName(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return std::nullopt;

            if (auto directPackName = tryGetSymbolicPackReferenceName(current))
                return directPackName;

            auto tryGetPackAliasFromElements = [](const std::vector<Ref<Type>>& elementTypes) -> std::optional<std::string>
            {
                if (elementTypes.size() != 1)
                    return std::nullopt;
                return tryGetSymbolicPackReferenceName(elementTypes.front());
            };

            switch (current->kind())
            {
            case TypeKind::ValuePackView:
                return tryGetPackAliasFromElements(current.AsFast<ValuePackViewType>()->elementTypes);
            case TypeKind::TypePackView:
                return tryGetPackAliasFromElements(current.AsFast<TypePackViewType>()->elementTypes);
            case TypeKind::PackStorage:
                return tryGetPackAliasFromElements(current.AsFast<PackStorageType>()->elementTypes);
            default:
                return std::nullopt;
            }
        }

        size_t getMinimumGenericArgumentCount(const std::vector<std::string>& parameterNames, const bool hasGenericParameterPack)
        {
            if (parameterNames.empty())
                return 0;

            return hasGenericParameterPack ? parameterNames.size() - 1 : parameterNames.size();
        }

        GenericBindingSet buildExtendedGenericBindings(const std::vector<std::string>& parameterNames,
                                                       const bool hasGenericParameterPack,
                                                       const std::vector<Ref<Type>>& typeArguments)
        {
            GenericBindingSet bindings;
            const size_t fixedCount = getMinimumGenericArgumentCount(parameterNames, hasGenericParameterPack);
            bindings.directBindings.reserve(fixedCount + typeArguments.size());

            for (size_t i = 0; i < fixedCount && i < typeArguments.size(); ++i)
                bindings.directBindings.emplace(parameterNames[i], typeArguments[i]);

            if (!hasGenericParameterPack || parameterNames.empty())
                return bindings;

            const std::string& packName = parameterNames.back();
            std::vector<Ref<Type>> packTypes;
            if (typeArguments.size() > fixedCount)
            {
                if (typeArguments.size() == fixedCount + 1)
                {
                    if (auto symbolicPackName = tryGetSymbolicPackReferenceName(typeArguments[fixedCount]))
                    {
                        bindings.packAliases.emplace(packName, *symbolicPackName);
                        bindings.packBindings.emplace(packName, std::move(packTypes));
                        return bindings;
                    }
                }

                packTypes.reserve(typeArguments.size() - fixedCount);
                for (size_t i = fixedCount; i < typeArguments.size(); ++i)
                {
                    packTypes.push_back(typeArguments[i]);
                    bindings.directBindings.emplace(makePackElementBindingName(packName, i - fixedCount), typeArguments[i]);
                }
            }

            bindings.packBindings.emplace(packName, std::move(packTypes));
            return bindings;
        }

        std::optional<size_t> tryEvaluateStaticPackIndex(
            const NodePtr<Expression>& expression,
            const ConstVariableDeclarationMap& variableDeclarationsBySymbol)
        {
            std::unordered_set<const Symbol*> activeSymbols;
            auto value = tryEvaluateStaticIntegerExpression(expression, variableDeclarationsBySymbol, activeSymbols);
            if (!value.has_value() || *value < 0)
                return std::nullopt;

            return static_cast<size_t>(*value);
        }

        std::optional<bool> tryEvaluateStaticTruthiness(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return std::nullopt;

            if (const auto* literal = expression->as<BoolLiteral>())
                return literal->token.value == "true";

            if (const auto* literal = expression->as<IntegerLiteral>())
            {
                const IntegerResult result = common::getInteger(literal->token.value);
                if (!result.isValid)
                    return std::nullopt;

                switch (result.type)
                {
                case IntegerType::i8: return result.value.v_i8 != 0;
                case IntegerType::i16: return result.value.v_i16 != 0;
                case IntegerType::i32: return result.value.v_i32 != 0;
                case IntegerType::i64: return result.value.v_i64 != 0;
                case IntegerType::u8: return result.value.v_u8 != 0;
                case IntegerType::u16: return result.value.v_u16 != 0;
                case IntegerType::u32: return result.value.v_u32 != 0;
                case IntegerType::u64: return result.value.v_u64 != 0;
                case IntegerType::isize: return result.value.v_isize != 0;
                case IntegerType::usize: return result.value.v_usize != 0;
                case IntegerType::Unknown: return std::nullopt;
                }
            }

            if (const auto* literal = expression->as<FloatLiteral>())
            {
                const FloatResult result = common::getFloat(literal->token.value);
                if (!result.isValid)
                    return std::nullopt;

                switch (result.type)
                {
                case FloatType::f32: return result.value.v_f32 != 0.0f;
                case FloatType::f64: return result.value.v_f64 != 0.0;
                case FloatType::Unknown: return std::nullopt;
                }
            }

            if (const auto* unary = expression->as<UnaryExpression>())
            {
                if (unary->op.type == TokenType::kwNot)
                {
                    if (auto operandTruthiness = tryEvaluateStaticTruthiness(unary->operand))
                        return !*operandTruthiness;

                    return std::nullopt;
                }

                if (unary->op.type == TokenType::opPlus || unary->op.type == TokenType::opMinus)
                    return tryEvaluateStaticTruthiness(unary->operand);
            }

            if (const auto* fit = expression->as<FitExpression>())
                return tryEvaluateStaticTruthiness(fit->operand);

            return std::nullopt;
        }

        bool statementMayBreakCurrentLoop(const NodePtr<Statement>& statement);

        bool statementDefinitelyReturns(const NodePtr<Statement>& statement)
        {
            if (!statement)
                return false;

            if (statement->is<ReturnStatement>())
                return true;

            if (const auto* block = statement->as<BlockStatement>())
            {
                for (const auto& nestedStatement : block->statements)
                {
                    if (statementDefinitelyReturns(nestedStatement))
                        return true;
                }

                return false;
            }

            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(ifStatement->condition))
                {
                    return *truthiness
                        ? statementDefinitelyReturns(ifStatement->thenBranch)
                        : statementDefinitelyReturns(ifStatement->elseBranch);
                }

                return statementDefinitelyReturns(ifStatement->thenBranch) &&
                       statementDefinitelyReturns(ifStatement->elseBranch);
            }

            if (const auto* whileStatement = statement->as<WhileStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(whileStatement->condition);
                    truthiness.has_value() && *truthiness)
                {
                    return statementDefinitelyReturns(whileStatement->body) &&
                           !statementMayBreakCurrentLoop(whileStatement->body);
                }

                return false;
            }

            if (const auto* cForStatement = statement->as<CForStatement>())
            {
                std::optional<bool> truthiness = cForStatement->condition
                    ? tryEvaluateStaticTruthiness(cForStatement->condition)
                    : std::optional<bool>(true);

                if (truthiness.has_value() && *truthiness)
                {
                    return statementDefinitelyReturns(cForStatement->body) &&
                           !statementMayBreakCurrentLoop(cForStatement->body);
                }

                return false;
            }

            return false;
        }

        bool statementMayBreakCurrentLoop(const NodePtr<Statement>& statement)
        {
            if (!statement)
                return false;

            if (statement->is<BreakStatement>())
                return true;

            if (const auto* block = statement->as<BlockStatement>())
            {
                for (const auto& nestedStatement : block->statements)
                {
                    if (statementMayBreakCurrentLoop(nestedStatement))
                        return true;
                }

                return false;
            }

            if (const auto* ifStatement = statement->as<IfStatement>())
            {
                if (auto truthiness = tryEvaluateStaticTruthiness(ifStatement->condition))
                {
                    return *truthiness
                        ? statementMayBreakCurrentLoop(ifStatement->thenBranch)
                        : statementMayBreakCurrentLoop(ifStatement->elseBranch);
                }

                return statementMayBreakCurrentLoop(ifStatement->thenBranch) ||
                       statementMayBreakCurrentLoop(ifStatement->elseBranch);
            }

            if (statement->is<WhileStatement>() ||
                statement->is<CForStatement>() ||
                statement->is<ForInStatement>())
            {
                return false;
            }

            return false;
        }

        struct PackSizeReference
        {
            std::optional<std::string> packName;
            std::optional<size_t> concreteSize;
        };

        std::optional<PackSizeReference> tryResolvePackSizeReference(const NodePtr<Expression>& expression)
        {
            if (!expression)
                return std::nullopt;

            const auto* memberAccess = expression->as<MemberAccessExpression>();
            if (!memberAccess || !memberAccess->member)
                return std::nullopt;

            if (memberAccess->intrinsicMember != IntrinsicMember::PackSize &&
                memberAccess->member->token.value != "size")
            {
                return std::nullopt;
            }

            Ref<Type> objectType = unwrapAliasType(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
            if (!objectType)
                return std::nullopt;

            PackSizeReference result;
            switch (objectType->kind())
            {
            case TypeKind::GenericParameterPack:
                result.packName = objectType.AsFast<GenericParameterPackType>()->name;
                return result;
            case TypeKind::ValuePackView:
            {
                auto packViewType = objectType.AsFast<ValuePackViewType>();
                if (auto symbolicPackName = tryGetNormalizedSymbolicPackName(objectType))
                    result.packName = *symbolicPackName;
                if (!packViewType->elementTypes.empty())
                    result.concreteSize = packViewType->elementTypes.size();
                return result;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = objectType.AsFast<TypePackViewType>();
                if (auto symbolicPackName = tryGetNormalizedSymbolicPackName(objectType))
                    result.packName = *symbolicPackName;
                if (!packViewType->elementTypes.empty())
                    result.concreteSize = packViewType->elementTypes.size();
                return result;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = objectType.AsFast<PackStorageType>();
                if (auto symbolicPackName = tryGetNormalizedSymbolicPackName(objectType))
                    result.packName = *symbolicPackName;
                if (!storageType->elementTypes.empty())
                    result.concreteSize = storageType->elementTypes.size();
                return result;
            }
            default:
                return std::nullopt;
            }
        }

        std::optional<ParsedPackElementBinding> tryEvaluatePackIndexBinding(
            const NodePtr<Expression>& expression,
            const ConstVariableDeclarationMap& variableDeclarationsBySymbol,
            const std::optional<std::string_view> expectedPackName = std::nullopt)
        {
            if (!expression)
                return std::nullopt;

            if (auto absoluteIndex = tryEvaluateStaticPackIndex(expression, variableDeclarationsBySymbol))
            {
                ParsedPackElementBinding binding;
                binding.packName = expectedPackName ? std::string(*expectedPackName) : std::string();
                binding.value = *absoluteIndex;
                binding.kind = PackElementBindingKind::Absolute;
                return binding;
            }

            const auto* binary = expression->as<BinaryExpression>();
            if (!binary || binary->op.type != TokenType::opMinus)
                return std::nullopt;

            auto packSizeReference = tryResolvePackSizeReference(binary->left);
            auto rhsIndex = tryEvaluateStaticPackIndex(binary->right, variableDeclarationsBySymbol);
            if (!packSizeReference || !rhsIndex.has_value())
                return std::nullopt;

            if (expectedPackName.has_value() &&
                packSizeReference->packName.has_value() &&
                *packSizeReference->packName != *expectedPackName)
            {
                return std::nullopt;
            }

            if (packSizeReference->concreteSize.has_value())
            {
                if (*rhsIndex > *packSizeReference->concreteSize)
                    return std::nullopt;

                ParsedPackElementBinding binding;
                binding.packName = expectedPackName ? std::string(*expectedPackName) : packSizeReference->packName.value_or(std::string());
                binding.value = *packSizeReference->concreteSize - *rhsIndex;
                binding.kind = PackElementBindingKind::Absolute;
                return binding;
            }

            if (!packSizeReference->packName.has_value())
                return std::nullopt;

            ParsedPackElementBinding binding;
            binding.packName = *packSizeReference->packName;
            binding.value = *rhsIndex;
            binding.kind = PackElementBindingKind::FromEnd;
            return binding;
        }

        Ref<Type> makeSyntheticPackElementType(const std::string& packName, const size_t index)
        {
            return Compiler::get().getTypeContext().getOrCreateGenericParameterType(makePackElementBindingName(packName, index));
        }

        Ref<Type> makeSyntheticPackElementType(const ParsedPackElementBinding& binding)
        {
            return Compiler::get().getTypeContext().getOrCreateGenericParameterType(
                makePackElementBindingName(binding)
            );
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
                                                      bool isConstructor = false,
                                                      bool hasGenericParameterPack = false)
        {
            std::string signature(name);

            if (!genericParameterNames.empty())
            {
                signature += "<";
                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    signature += genericParameterNames[i];
                    if (hasGenericParameterPack && i + 1 == genericParameterNames.size())
                        signature += "...";
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
            case TypeKind::GenericParameterPack:
                return true;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (packViewType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (packViewType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (storageType->elementTypes.empty())
                    return true;
                return std::ranges::any_of(storageType->elementTypes, [](const Ref<Type>& elementType)
                {
                    return containsGenericParameterType(elementType);
                });
            }
            case TypeKind::Reference:
                return containsGenericParameterType(current.AsFast<ReferenceType>()->referredType);
            case TypeKind::Nullable:
                return containsGenericParameterType(current.AsFast<NullableType>()->valueType);
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
                if (std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end())
                    return true;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                    return std::ranges::find(genericParameterNames, parsedPackElement->packName) != genericParameterNames.end();

                return false;
            }
            case TypeKind::GenericParameterPack:
            {
                auto genericParam = current.AsFast<GenericParameterPackType>();
                return std::ranges::find(genericParameterNames, genericParam->name) != genericParameterNames.end();
            }
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (std::ranges::find(genericParameterNames, packViewType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (std::ranges::find(genericParameterNames, packViewType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (std::ranges::find(genericParameterNames, storageType->packName) != genericParameterNames.end())
                    return true;
                return std::ranges::any_of(storageType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsNamedGenericParameterType(elementType, genericParameterNames);
                });
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
                else if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name);
                         parsedPackElement &&
                         std::ranges::find(genericParameterNames, parsedPackElement->packName) != genericParameterNames.end() &&
                         !instances.contains(parsedPackElement->packName))
                {
                    instances.emplace(parsedPackElement->packName, current.Get());
                }
                return;
            }
            case TypeKind::GenericParameterPack:
                return;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                for (const auto& elementType : packViewType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
                return;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                for (const auto& elementType : packViewType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
                return;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                for (const auto& elementType : storageType->elementTypes)
                    collectGenericParameterInstances(elementType, genericParameterNames, instances);
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

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto foundInstance = instances.find(parsedPackElement->packName); foundInstance != instances.end())
                        return foundInstance->second == current.Get();
                }

                return false;
            }
            case TypeKind::GenericParameterPack:
                return true;
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                return std::ranges::any_of(packViewType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                return std::ranges::any_of(storageType->elementTypes, [&](const Ref<Type>& elementType)
                {
                    return containsGenericParameterInstance(elementType, instances);
                });
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

        std::string getGenericParameterPackName(const Ref<Type>& type)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current || current->kind() != TypeKind::GenericParameterPack)
                return {};

            return current.AsFast<GenericParameterPackType>()->name;
        }

        bool containsGenericParameterPackType(const Ref<Type>& type, std::string* outPackName = nullptr)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return false;

            switch (current->kind())
            {
            case TypeKind::GenericParameterPack:
                if (outPackName)
                    *outPackName = current.AsFast<GenericParameterPackType>()->name;
                return true;
            case TypeKind::Reference:
                return containsGenericParameterPackType(current.AsFast<ReferenceType>()->referredType, outPackName);
            case TypeKind::Array:
                return containsGenericParameterPackType(current.AsFast<ArrayType>()->elementType, outPackName);
            case TypeKind::Dictionary:
            {
                auto dictType = current.AsFast<DictionaryType>();
                return containsGenericParameterPackType(dictType->keyType, outPackName) ||
                       containsGenericParameterPackType(dictType->valueType, outPackName);
            }
            case TypeKind::Function:
            {
                auto functionType = current.AsFast<FunctionType>();
                if (containsGenericParameterPackType(functionType->returnType, outPackName))
                    return true;

                for (const auto& parameterType : functionType->paramTypes)
                {
                    if (containsGenericParameterPackType(parameterType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                for (const auto& genericArgument : structType->genericArguments)
                {
                    if (containsGenericParameterPackType(genericArgument, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::ValuePackView:
            {
                auto packViewType = current.AsFast<ValuePackViewType>();
                if (packViewType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = packViewType->packName;
                    return true;
                }
                for (const auto& elementType : packViewType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::TypePackView:
            {
                auto packViewType = current.AsFast<TypePackViewType>();
                if (packViewType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = packViewType->packName;
                    return true;
                }
                for (const auto& elementType : packViewType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (storageType->elementTypes.empty())
                {
                    if (outPackName)
                        *outPackName = storageType->packName;
                    return true;
                }
                for (const auto& elementType : storageType->elementTypes)
                {
                    if (containsGenericParameterPackType(elementType, outPackName))
                        return true;
                }
                return false;
            }
            case TypeKind::Alias:
                return containsGenericParameterPackType(current.AsFast<AliasType>()->aliasedType, outPackName);
            default:
                return false;
            }
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings);
        Ref<Type> instantiateGenericType(const Ref<Type>& type, const GenericBindingSet& bindings);

        bool matchSpecializationPattern(const Ref<Type>& pattern,
                                        const Ref<Type>& actual,
                                        std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            Ref<Type> expected = unwrapAliasType(pattern);
            Ref<Type> candidate = unwrapAliasType(actual);
            if (!expected || !candidate)
                return false;
            if (expected->kind() == TypeKind::GenericParameter)
            {
                const std::string& name = expected.AsFast<GenericParameterType>()->name;
                if (auto found = bindings.find(name); found != bindings.end())
                    return getGenericSpecializationKey({found->second}) == getGenericSpecializationKey({candidate});
                bindings.emplace(name, candidate);
                return true;
            }
            if (expected->kind() != candidate->kind())
                return false;

            switch (expected->kind())
            {
            case TypeKind::Primitive:
                return expected.AsFast<PrimitiveType>()->name == candidate.AsFast<PrimitiveType>()->name;
            case TypeKind::Reference:
            {
                auto expectedRef = expected.AsFast<ReferenceType>();
                auto candidateRef = candidate.AsFast<ReferenceType>();
                return expectedRef->isMutable == candidateRef->isMutable &&
                       matchSpecializationPattern(expectedRef->referredType, candidateRef->referredType, bindings);
            }
            case TypeKind::Nullable:
                return matchSpecializationPattern(expected.AsFast<NullableType>()->valueType,
                                                  candidate.AsFast<NullableType>()->valueType,
                                                  bindings);
            case TypeKind::Array:
            {
                auto expectedArray = expected.AsFast<ArrayType>();
                auto candidateArray = candidate.AsFast<ArrayType>();
                return expectedArray->arrayKind == candidateArray->arrayKind &&
                       expectedArray->size == candidateArray->size &&
                       matchSpecializationPattern(expectedArray->elementType, candidateArray->elementType, bindings);
            }
            case TypeKind::Dictionary:
            {
                auto expectedDict = expected.AsFast<DictionaryType>();
                auto candidateDict = candidate.AsFast<DictionaryType>();
                return expectedDict->isOrdered == candidateDict->isOrdered &&
                       matchSpecializationPattern(expectedDict->keyType, candidateDict->keyType, bindings) &&
                       matchSpecializationPattern(expectedDict->valueType, candidateDict->valueType, bindings);
            }
            case TypeKind::Function:
            {
                auto expectedFunction = expected.AsFast<FunctionType>();
                auto candidateFunction = candidate.AsFast<FunctionType>();
                if (expectedFunction->hasParameterPack != candidateFunction->hasParameterPack ||
                    expectedFunction->paramTypes.size() != candidateFunction->paramTypes.size() ||
                    !matchSpecializationPattern(expectedFunction->returnType, candidateFunction->returnType, bindings))
                    return false;
                for (size_t index = 0; index < expectedFunction->paramTypes.size(); ++index)
                {
                    if (!matchSpecializationPattern(expectedFunction->paramTypes[index], candidateFunction->paramTypes[index], bindings))
                        return false;
                }
                return true;
            }
            case TypeKind::Struct:
            {
                auto expectedStruct = expected.AsFast<StructType>();
                auto candidateStruct = candidate.AsFast<StructType>();
                if (expectedStruct->name != candidateStruct->name ||
                    expectedStruct->scopePath != candidateStruct->scopePath ||
                    expectedStruct->genericArguments.size() != candidateStruct->genericArguments.size())
                    return false;
                for (size_t index = 0; index < expectedStruct->genericArguments.size(); ++index)
                {
                    if (!matchSpecializationPattern(expectedStruct->genericArguments[index], candidateStruct->genericArguments[index], bindings))
                        return false;
                }
                return true;
            }
            default:
                return getGenericSpecializationKey({expected}) == getGenericSpecializationKey({candidate});
            }
        }

        size_t getSpecializationPatternSpecificity(const Ref<Type>& pattern)
        {
            Ref<Type> type = unwrapAliasType(pattern);
            if (!type || type->kind() == TypeKind::GenericParameter || type->kind() == TypeKind::GenericParameterPack)
                return 0;

            size_t score = 1;
            switch (type->kind())
            {
            case TypeKind::Reference:
                return score + getSpecializationPatternSpecificity(type.AsFast<ReferenceType>()->referredType);
            case TypeKind::Nullable:
                return score + getSpecializationPatternSpecificity(type.AsFast<NullableType>()->valueType);
            case TypeKind::Array:
                return score + getSpecializationPatternSpecificity(type.AsFast<ArrayType>()->elementType);
            case TypeKind::Dictionary:
                return score + getSpecializationPatternSpecificity(type.AsFast<DictionaryType>()->keyType) +
                               getSpecializationPatternSpecificity(type.AsFast<DictionaryType>()->valueType);
            case TypeKind::Struct:
                for (const auto& argument : type.AsFast<StructType>()->genericArguments)
                    score += getSpecializationPatternSpecificity(argument);
                return score;
            default:
                return score;
            }
        }

        Ref<Type> instantiateGenericStructType(const Ref<StructType>& structType,
                                               const std::vector<Ref<Type>>& explicitTypeArguments,
                                               const common::Location& errorLocation = common::Location::invalid())
        {
            if (!structType)
                return nullptr;

            if (structType->genericParameterNames.empty())
                return structType;

            if (auto specializationIt = structType->explicitSpecializations.find(
                    getGenericSpecializationKey(explicitTypeArguments));
                specializationIt != structType->explicitSpecializations.end())
            {
                if (auto specialization = specializationIt->second.Lock(); specialization)
                    return specialization;
            }

            Ref<StructType> selectedPartialSpecialization = nullptr;
            std::unordered_map<std::string, Ref<Type>> selectedBindings;
            size_t selectedSpecificity = 0;
            bool ambiguousPartialSpecialization = false;
            for (const auto& weakSpecialization : structType->partialSpecializations)
            {
                auto specialization = weakSpecialization.Lock();
                if (!specialization || specialization->genericArguments.size() != explicitTypeArguments.size())
                    continue;

                std::unordered_map<std::string, Ref<Type>> bindings;
                bool matches = true;
                size_t specificity = 0;
                for (size_t index = 0; index < explicitTypeArguments.size(); ++index)
                {
                    specificity += getSpecializationPatternSpecificity(specialization->genericArguments[index]);
                    if (!matchSpecializationPattern(specialization->genericArguments[index], explicitTypeArguments[index], bindings))
                    {
                        matches = false;
                        break;
                    }
                }
                if (!matches)
                    continue;
                if (!selectedPartialSpecialization || specificity > selectedSpecificity)
                {
                    selectedPartialSpecialization = specialization;
                    selectedBindings = std::move(bindings);
                    selectedSpecificity = specificity;
                    ambiguousPartialSpecialization = false;
                }
                else if (specificity == selectedSpecificity)
                {
                    ambiguousPartialSpecialization = true;
                }
            }

            if (ambiguousPartialSpecialization)
            {
                WIO_LOG_ADD_ERROR(errorLocation,
                    "Ambiguous partial specialization for '{}{}'.",
                    structType->name,
                    formatDiagnosticTypeList(explicitTypeArguments));
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (selectedPartialSpecialization)
            {
                auto instantiatedType = Compiler::get().getTypeContext().getOrCreateStructType(
                    selectedPartialSpecialization->name,
                    selectedPartialSpecialization->structScope.Lock(),
                    selectedPartialSpecialization->isObject,
                    selectedPartialSpecialization->isInterface
                ).AsFast<StructType>();
                instantiatedType->scopePath = selectedPartialSpecialization->scopePath;
                instantiatedType->genericParameterNames = selectedPartialSpecialization->genericParameterNames;
                instantiatedType->genericParameterDefaults = selectedPartialSpecialization->genericParameterDefaults;
                instantiatedType->genericArguments = explicitTypeArguments;
                instantiatedType->genericPrimaryType = structType;
                instantiatedType->isExplicitSpecialization = true;
                instantiatedType->isPartialSpecialization = true;
                instantiatedType->fieldNames = selectedPartialSpecialization->fieldNames;
                instantiatedType->trustedTypeKeys = selectedPartialSpecialization->trustedTypeKeys;
                instantiatedType->isFinal = selectedPartialSpecialization->isFinal;
                for (const auto& fieldType : selectedPartialSpecialization->fieldTypes)
                    instantiatedType->fieldTypes.push_back(instantiateGenericType(fieldType, selectedBindings));
                for (const auto& baseType : selectedPartialSpecialization->baseTypes)
                    instantiatedType->baseTypes.push_back(instantiateGenericType(baseType, selectedBindings));
                return instantiatedType;
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
            ).AsFast<StructType>();

            instantiatedType->scopePath = structType->scopePath;
            instantiatedType->genericParameterNames = structType->genericParameterNames;
            instantiatedType->genericParameterDefaults = structType->genericParameterDefaults;
            instantiatedType->genericArguments = explicitTypeArguments;
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

            validateInstantiatedNativePodComponent(instantiatedType, errorLocation);

            return instantiatedType;
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const std::unordered_map<std::string, Ref<Type>>& bindings)
        {
            GenericBindingSet bindingSet;
            bindingSet.directBindings = bindings;
            return instantiateGenericType(type, bindingSet);
        }

        Ref<Type> instantiateGenericType(const Ref<Type>& type, const GenericBindingSet& bindings)
        {
            Ref<Type> current = unwrapAliasType(type);
            if (!current)
                return nullptr;

            auto& ctx = Compiler::get().getTypeContext();

            switch (current->kind())
            {
            case TypeKind::GenericParameter:
            {
                auto genericParam = current.AsFast<GenericParameterType>();
                if (auto it = bindings.directBindings.find(genericParam->name); it != bindings.directBindings.end())
                    return it->second;

                if (auto parsedPackElement = tryParsePackElementBindingName(genericParam->name))
                {
                    if (auto aliasIt = bindings.packAliases.find(parsedPackElement->packName); aliasIt != bindings.packAliases.end())
                    {
                        ParsedPackElementBinding reboundBinding = *parsedPackElement;
                        reboundBinding.packName = aliasIt->second;
                        return makeSyntheticPackElementType(reboundBinding);
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
            case TypeKind::GenericParameterPack:
            {
                auto genericPack = current.AsFast<GenericParameterPackType>();
                if (auto aliasIt = bindings.packAliases.find(genericPack->name); aliasIt != bindings.packAliases.end())
                    return ctx.getOrCreateTypePackViewType(aliasIt->second);
                if (auto it = bindings.packBindings.find(genericPack->name); it != bindings.packBindings.end())
                    return ctx.getOrCreateTypePackViewType(genericPack->name, it->second);
                return current;
            }
            case TypeKind::ValuePackView:
            {
                auto viewType = current.AsFast<ValuePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::TypePackView:
            {
                auto viewType = current.AsFast<TypePackViewType>();
                if (!viewType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::PackStorage:
            {
                auto storageType = current.AsFast<PackStorageType>();
                if (!storageType->elementTypes.empty())
                {
                    std::vector<Ref<Type>> instantiatedElements;
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
            case TypeKind::Reference:
            {
                auto refType = current.AsFast<ReferenceType>();
                return ctx.getOrCreateReferenceType(
                    instantiateGenericType(refType->referredType, bindings),
                    refType->isMutable
                );
            }
            case TypeKind::Nullable:
                return ctx.getOrCreateNullableType(
                    instantiateGenericType(current.AsFast<NullableType>()->valueType, bindings)
                );
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
                bool hasParameterPack = funcType->hasParameterPack;

                if (funcType->hasParameterPack &&
                    !funcType->paramTypes.empty() &&
                    unwrapAliasType(funcType->paramTypes.back()) &&
                    unwrapAliasType(funcType->paramTypes.back())->kind() == TypeKind::GenericParameterPack)
                {
                    const auto packType = unwrapAliasType(funcType->paramTypes.back()).AsFast<GenericParameterPackType>();
                    for (size_t i = 0; i + 1 < funcType->paramTypes.size(); ++i)
                        instantiatedParams.push_back(instantiateGenericType(funcType->paramTypes[i], bindings));

                    if (auto aliasIt = bindings.packAliases.find(packType->name); aliasIt != bindings.packAliases.end())
                    {
                        instantiatedParams.push_back(
                            Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(aliasIt->second)
                        );
                    }
                    else if (auto it = bindings.packBindings.find(packType->name); it != bindings.packBindings.end())
                    {
                        if (it->second.empty())
                        {
                            hasParameterPack = false;
                        }
                        else
                        {
                            for (const auto& boundType : it->second)
                                instantiatedParams.push_back(boundType);
                            hasParameterPack = false;
                        }
                    }
                    else
                    {
                        instantiatedParams.push_back(funcType->paramTypes.back());
                    }
                }
                else
                {
                    for (const auto& paramType : funcType->paramTypes)
                        instantiatedParams.push_back(instantiateGenericType(paramType, bindings));
                }

                return ctx.getOrCreateFunctionType(
                    instantiateGenericType(funcType->returnType, bindings),
                    instantiatedParams,
                    hasParameterPack
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
                    const size_t fixedCount = getMinimumGenericArgumentCount(structType->genericParameterNames, structType->hasGenericParameterPack);
                    instantiatedArguments.reserve(fixedCount);
                    for (size_t i = 0; i < fixedCount; ++i)
                    {
                        auto bindingIt = bindings.directBindings.find(structType->genericParameterNames[i]);
                        if (bindingIt == bindings.directBindings.end())
                            return current;

                        instantiatedArguments.push_back(bindingIt->second);
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
            case TypeKind::Alias:
                return instantiateGenericType(current.AsFast<AliasType>()->aliasedType, bindings);
            default:
                return current;
            }
        }

        std::vector<Ref<Type>> resolveGenericParameterDefaults(
            SemanticAnalyzer& analyzer,
            const std::vector<NodePtr<Identifier>>& parameters,
            const bool hasGenericParameterPack,
            const std::string_view declarationKind)
        {
            std::vector<Ref<Type>> defaults(parameters.size());
            std::vector<std::string> parameterNames;
            parameterNames.reserve(parameters.size());
            for (const auto& parameter : parameters)
                parameterNames.push_back(parameter ? parameter->token.value : std::string{});

            for (size_t index = 0; index < parameters.size(); ++index)
            {
                const auto& parameter = parameters[index];
                if (!parameter || !parameter->genericDefaultType)
                    continue;

                if (hasGenericParameterPack && index + 1 == parameters.size())
                {
                    WIO_LOG_ADD_ERROR(parameter->location(), "Generic parameter packs cannot have default arguments.");
                    continue;
                }

                parameter->genericDefaultType->accept(analyzer);
                Ref<Type> defaultType = parameter->genericDefaultType->refType.Lock();
                defaults[index] = defaultType;

                std::unordered_map<std::string, const Type*> referencedParameters;
                collectGenericParameterInstances(defaultType, parameterNames, referencedParameters);
                for (size_t referencedIndex = index; referencedIndex < parameterNames.size(); ++referencedIndex)
                {
                    if (!parameterNames[referencedIndex].empty() &&
                        referencedParameters.contains(parameterNames[referencedIndex]))
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter->genericDefaultType->location(),
                            "Default for generic parameter '{}' on {} cannot reference '{}' because defaults may only use earlier parameters.",
                            parameter->token.value,
                            declarationKind,
                            parameterNames[referencedIndex]
                        );
                    }
                }
            }

            return defaults;
        }

        size_t getRequiredGenericArgumentCount(const std::vector<Ref<Type>>& defaults,
                                               const size_t fixedParameterCount)
        {
            size_t requiredCount = fixedParameterCount;
            while (requiredCount > 0 &&
                   requiredCount - 1 < defaults.size() &&
                   defaults[requiredCount - 1])
            {
                --requiredCount;
            }
            return requiredCount;
        }

        std::optional<std::vector<Ref<Type>>> completeGenericTypeArguments(
            const std::vector<std::string>& parameterNames,
            const std::vector<Ref<Type>>& defaults,
            const bool hasGenericParameterPack,
            const std::vector<Ref<Type>>& providedArguments)
        {
            const size_t fixedCount = getMinimumGenericArgumentCount(parameterNames, hasGenericParameterPack);
            const size_t requiredCount = getRequiredGenericArgumentCount(defaults, fixedCount);
            if (providedArguments.size() < requiredCount ||
                (!hasGenericParameterPack && providedArguments.size() > fixedCount))
            {
                return std::nullopt;
            }

            std::vector<Ref<Type>> completed = providedArguments;
            const size_t providedFixedCount = std::min(providedArguments.size(), fixedCount);
            GenericBindingSet bindings;
            for (size_t index = 0; index < providedFixedCount; ++index)
                bindings.directBindings[parameterNames[index]] = providedArguments[index];

            for (size_t index = providedFixedCount; index < fixedCount; ++index)
            {
                if (index >= defaults.size() || !defaults[index])
                    return std::nullopt;

                Ref<Type> instantiatedDefault = instantiateGenericType(defaults[index], bindings);
                completed.insert(completed.begin() + static_cast<std::ptrdiff_t>(index), instantiatedDefault);
                bindings.directBindings[parameterNames[index]] = instantiatedDefault;
            }

            return completed;
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

            if (resolvedExpected->kind() == TypeKind::GenericParameterPack)
                return resolvedActual->kind() == TypeKind::GenericParameterPack &&
                       resolvedExpected.AsFast<GenericParameterPackType>()->name == resolvedActual.AsFast<GenericParameterPackType>()->name;

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
            if (name == "byte") return ctx.getU8();
            if (name == "bit") return ctx.getBool();
            if (name == "f32") return ctx.getF32();
            if (name == "f64") return ctx.getF64();
            if (name == "bool") return ctx.getBool();
            if (name == "char") return ctx.getChar();
            if (name == "string") return ctx.getString();
            if (name == "any") return ctx.getAny();
            if (name == "opaque") return ctx.getOpaque();
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

        Ref<Symbol> findStructMemberInHierarchy(const Ref<Type>& type, const std::string& name, Ref<Type>* ownerType)
        {
            if (!type || type->kind() != TypeKind::Struct)
                return nullptr;

            auto structType = type.AsFast<StructType>();
            if (!structType)
                return nullptr;

            if (auto lockedScope = structType->structScope.Lock(); lockedScope)
            {
                if (auto found = lockedScope->resolveLocally(name); found)
                {
                    if (ownerType)
                        *ownerType = type;
                    return found;
                }
            }

            for (const auto& baseType : structType->baseTypes)
            {
                if (auto found = findStructMemberInHierarchy(baseType, name, ownerType); found)
                    return found;
            }

            return nullptr;
        }

        bool validateStructMemberAccess(const Ref<Type>& currentStructType,
                                        const Ref<Type>& ownerType,
                                        const Ref<Symbol>& member,
                                        const common::Location& location)
        {
            if (!member)
                return false;

            bool isInsideHierarchy = false;
            bool isInsideSameObject = false;
            bool isTrustedAccess = false;

            if (currentStructType && ownerType)
            {
                if (currentStructType == ownerType ||
                    isTypeDerivedFrom(currentStructType, ownerType) ||
                    isTypeDerivedFrom(ownerType, currentStructType))
                {
                    isInsideHierarchy = true;
                }

                isInsideSameObject = currentStructType == ownerType;

                if (auto ownerStruct = ownerType.AsFast<StructType>(); ownerStruct)
                {
                    const std::string trustedKey = getStructIdentityKey(currentStructType.AsFast<StructType>());
                    isTrustedAccess =
                        !trustedKey.empty() &&
                        std::ranges::find(ownerStruct->trustedTypeKeys, trustedKey) != ownerStruct->trustedTypeKeys.end();
                }
            }

            const std::string ownerTypeName = formatAccessContextType(ownerType);
            const std::string currentContextTypeName = formatAccessContextType(currentStructType);
            bool isAccessible = true;

            if (member->flags.get_isPrivate() && !isInsideSameObject && !isTrustedAccess)
            {
                WIO_LOG_ADD_ERROR(
                    location,
                    "Cannot access private member '{}' declared on '{}' from '{}'.",
                    member->name,
                    ownerTypeName,
                    currentContextTypeName
                );
                isAccessible = false;
            }

            if (member->flags.get_isProtected() && !isInsideHierarchy && !isTrustedAccess)
            {
                WIO_LOG_ADD_ERROR(
                    location,
                    "Cannot access protected member '{}' declared on '{}' from '{}'.",
                    member->name,
                    ownerTypeName,
                    currentContextTypeName
                );
                isAccessible = false;
            }

            return isAccessible;
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

        std::vector<Ref<Type>> resolveExplicitSpecializationArguments(
            SemanticAnalyzer& analyzer,
            const std::vector<NodePtr<AttributeStatement>>& attributes,
            const common::Location& declarationLocation,
            const bool allowGenericPatterns = false)
        {
            const auto specializationAttributes = getAttributeStatements(attributes, Attribute::Specialize);
            if (specializationAttributes.empty())
                return {};

            if (specializationAttributes.size() > 1)
            {
                WIO_LOG_ADD_ERROR(
                    specializationAttributes[1]->location(),
                    "Only one @Specialize(...) attribute is allowed per declaration."
                );
            }

            const auto* specializationAttribute = specializationAttributes.front();
            if (!specializationAttribute || specializationAttribute->args.empty())
            {
                WIO_LOG_ADD_ERROR(declarationLocation, "@Specialize expects at least one concrete type argument.");
                return {};
            }

            std::vector<Ref<Type>> arguments;
            arguments.reserve(specializationAttribute->args.size());
            for (size_t argumentIndex = 0; argumentIndex < specializationAttribute->args.size(); ++argumentIndex)
            {
                NodePtr<TypeSpecifier> typeSpecifier =
                    argumentIndex < specializationAttribute->typeArgs.size()
                        ? specializationAttribute->typeArgs[argumentIndex]
                        : nullptr;

                if (!typeSpecifier)
                {
                    WIO_LOG_ADD_ERROR(
                        specializationAttribute->location(),
                        "@Specialize arguments must be concrete types."
                    );
                    arguments.push_back(Compiler::get().getTypeContext().getUnknown());
                    continue;
                }

                typeSpecifier->accept(analyzer);
                Ref<Type> argumentType = typeSpecifier->refType.Lock();
                if (!argumentType || argumentType->isUnknown() ||
                    (!allowGenericPatterns && containsGenericParameterType(argumentType)))
                {
                    WIO_LOG_ADD_ERROR(
                        typeSpecifier->location(),
                        "@Specialize arguments must be fully concrete types."
                    );
                    argumentType = Compiler::get().getTypeContext().getUnknown();
                }
                arguments.push_back(argumentType);
            }

            return arguments;
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

        bool usesSingleGenericApplyConstraintList(const AttributeStatement& attribute,
                                                  const std::vector<std::string>& genericParameterNames)
        {
            return genericParameterNames.size() == 1 && !attribute.args.empty();
        }

        std::vector<AttributeTypeArgument> getApplyConstraintArguments(
            const AttributeStatement& attribute,
            const std::vector<std::string>& genericParameterNames,
            const size_t genericParameterIndex)
        {
            if (usesSingleGenericApplyConstraintList(attribute, genericParameterNames))
            {
                std::vector<AttributeTypeArgument> constraintArguments;
                constraintArguments.reserve(attribute.args.size());
                for (size_t argumentIndex = 0; argumentIndex < attribute.args.size(); ++argumentIndex)
                    constraintArguments.push_back(getAttributeTypeArgument(attribute, argumentIndex));
                return constraintArguments;
            }

            if (genericParameterIndex >= attribute.args.size())
                return {};

            return { getAttributeTypeArgument(attribute, genericParameterIndex) };
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

        bool isNumericConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->isNumeric();
        }

        bool isFloatingConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "f32" || name == "f64";
        }

        bool isSignedConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "i8" || name == "i16" || name == "i32" || name == "i64" ||
                   name == "isize" || name == "f32" || name == "f64";
        }

        bool isUnsignedConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Primitive)
                return false;
            const std::string& name = resolved.AsFast<PrimitiveType>()->name;
            return name == "u8" || name == "u16" || name == "u32" || name == "u64" ||
                   name == "usize";
        }

        bool isObjectConstraintType(const Ref<Type>& type)
        {
            auto objectType = getObjectOrInterfaceStructType(type);
            return objectType && objectType->isObject && !objectType->isInterface;
        }

        bool isInterfaceConstraintType(const Ref<Type>& type)
        {
            auto interfaceType = getObjectOrInterfaceStructType(type);
            return interfaceType && interfaceType->isInterface;
        }

        bool isComponentConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;
            auto structType = resolved.AsFast<StructType>();
            return structType && !structType->isObject && !structType->isInterface &&
                   !structType->isEnum && !structType->isFlagset;
        }

        bool isArrayConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->kind() == TypeKind::Array;
        }

        bool isReferenceConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            return resolved && resolved->kind() == TypeKind::Reference;
        }

        bool isEnumConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto structType = resolved.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface)
                return false;

            if (auto structScope = structType->structScope.Lock())
            {
                if (auto structSymbol = structScope->resolve(structType->name))
                    return structSymbol->flags.get_isEnum();
            }

            return false;
        }

        bool isFlagsetConstraintType(const Ref<Type>& type)
        {
            Ref<Type> resolved = unwrapAliasType(type);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto structType = resolved.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface)
                return false;

            if (auto structScope = structType->structScope.Lock())
            {
                if (auto structSymbol = structScope->resolve(structType->name))
                    return structSymbol->flags.get_isFlagset();
            }

            return false;
        }

        std::vector<Ref<Type>> getIntegerConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(),
                ctx.getISize(), ctx.getUSize()
            };
        }

        std::vector<Ref<Type>> getNumericConstraintCandidateTypes()
        {
            auto candidates = getIntegerConstraintCandidateTypes();
            auto& ctx = Compiler::get().getTypeContext();
            candidates.push_back(ctx.getF32());
            candidates.push_back(ctx.getF64());
            return candidates;
        }

        std::vector<Ref<Type>> getFloatingConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return { ctx.getF32(), ctx.getF64() };
        }

        std::vector<Ref<Type>> getSignedConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getI8(), ctx.getI16(), ctx.getI32(), ctx.getI64(),
                ctx.getISize(), ctx.getF32(), ctx.getF64()
            };
        }

        std::vector<Ref<Type>> getUnsignedConstraintCandidateTypes()
        {
            auto& ctx = Compiler::get().getTypeContext();
            return {
                ctx.getU8(), ctx.getU16(), ctx.getU32(), ctx.getU64(), ctx.getUSize()
            };
        }

        std::vector<Ref<Type>> getNoConcreteConstraintCandidateTypes()
        {
            return {};
        }

        enum class GenericConstraintTraitKind : uint8_t
        {
            IsInteger,
            IsNumeric,
            IsFloating,
            IsSigned,
            IsUnsigned,
            IsEnum,
            IsFlagset,
            IsObject,
            IsComponent,
            IsInterface,
            IsArray,
            IsReference
        };

        using GenericConstraintPredicateFn = bool (*)(const Ref<Type>&);
        using GenericConstraintCandidatesFn = std::vector<Ref<Type>> (*)();

        struct GenericConstraintTraitDescriptor
        {
            GenericConstraintTraitKind kind;
            std::string_view canonicalQualifiedName;
            std::string_view shortName;
            GenericConstraintPredicateFn predicate;
            GenericConstraintCandidatesFn candidateTypes;
        };

        const std::array<GenericConstraintTraitDescriptor, 12>& getGenericConstraintTraitDescriptors()
        {
            static const std::array<GenericConstraintTraitDescriptor, 12> descriptors = {{
                {
                    .kind = GenericConstraintTraitKind::IsInteger,
                    .canonicalQualifiedName = "std::traits::IsInteger",
                    .shortName = "IsInteger",
                    .predicate = isIntegerConstraintType,
                    .candidateTypes = getIntegerConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsNumeric,
                    .canonicalQualifiedName = "std::traits::IsNumeric",
                    .shortName = "IsNumeric",
                    .predicate = isNumericConstraintType,
                    .candidateTypes = getNumericConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsFloating,
                    .canonicalQualifiedName = "std::traits::IsFloating",
                    .shortName = "IsFloating",
                    .predicate = isFloatingConstraintType,
                    .candidateTypes = getFloatingConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsSigned,
                    .canonicalQualifiedName = "std::traits::IsSigned",
                    .shortName = "IsSigned",
                    .predicate = isSignedConstraintType,
                    .candidateTypes = getSignedConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsUnsigned,
                    .canonicalQualifiedName = "std::traits::IsUnsigned",
                    .shortName = "IsUnsigned",
                    .predicate = isUnsignedConstraintType,
                    .candidateTypes = getUnsignedConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsEnum,
                    .canonicalQualifiedName = "std::traits::IsEnum",
                    .shortName = "IsEnum",
                    .predicate = isEnumConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsFlagset,
                    .canonicalQualifiedName = "std::traits::IsFlagset",
                    .shortName = "IsFlagset",
                    .predicate = isFlagsetConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsObject,
                    .canonicalQualifiedName = "std::traits::IsObject",
                    .shortName = "IsObject",
                    .predicate = isObjectConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsComponent,
                    .canonicalQualifiedName = "std::traits::IsComponent",
                    .shortName = "IsComponent",
                    .predicate = isComponentConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsInterface,
                    .canonicalQualifiedName = "std::traits::IsInterface",
                    .shortName = "IsInterface",
                    .predicate = isInterfaceConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsArray,
                    .canonicalQualifiedName = "std::traits::IsArray",
                    .shortName = "IsArray",
                    .predicate = isArrayConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                },
                {
                    .kind = GenericConstraintTraitKind::IsReference,
                    .canonicalQualifiedName = "std::traits::IsReference",
                    .shortName = "IsReference",
                    .predicate = isReferenceConstraintType,
                    .candidateTypes = getNoConcreteConstraintCandidateTypes
                }
            }};
            return descriptors;
        }

        std::string_view getLastQualifiedSegment(std::string_view name)
        {
            const size_t separator = name.rfind("::");
            if (separator == std::string_view::npos)
                return name;

            return name.substr(separator + 2);
        }

        const GenericConstraintTraitDescriptor* findGenericConstraintTraitDescriptor(std::string_view rawName,
                                                                                     const Ref<Type>& resolvedType = nullptr)
        {
            const std::string_view lastSegment = getLastQualifiedSegment(rawName);
            for (const auto& descriptor : getGenericConstraintTraitDescriptors())
            {
                if (rawName == descriptor.shortName ||
                    rawName == descriptor.canonicalQualifiedName ||
                    lastSegment == descriptor.shortName)
                    return &descriptor;

                Ref<Type> unwrapped = unwrapAliasType(resolvedType);
                if (!unwrapped || unwrapped->kind() != TypeKind::Struct)
                    continue;

                auto structType = unwrapped.AsFast<StructType>();
                if (structType && structType->name == descriptor.shortName && structType->scopePath == "std::traits")
                    return &descriptor;
            }

            return nullptr;
        }

        std::string_view getGenericConstraintTraitDisplayName(const GenericConstraintTraitDescriptor& descriptor)
        {
            return descriptor.canonicalQualifiedName;
        }

        bool isUserDefinedTraitConstraint(const Ref<Type>& constraintType, std::string_view parameterName)
        {
            Ref<Type> resolved = unwrapAliasType(constraintType);
            if (!resolved || resolved->kind() != TypeKind::Struct)
                return false;

            auto traitType = resolved.AsFast<StructType>();
            if (!traitType || !traitType->isInterface || traitType->genericArguments.empty())
                return false;

            return std::ranges::any_of(traitType->genericArguments, [&](const Ref<Type>& argument)
            {
                Ref<Type> unwrappedArgument = unwrapAliasType(argument);
                if (!unwrappedArgument || unwrappedArgument->kind() != TypeKind::GenericParameter)
                    return false;
                return unwrappedArgument.AsFast<GenericParameterType>()->name == parameterName;
            });
        }

        bool matchesUserDefinedTraitConstraint(
            const Ref<Type>& bindingType,
            const Ref<Type>& constraintType,
            std::string_view parameterName)
        {
            Ref<Type> resolvedBinding = unwrapAliasType(bindingType);
            Ref<Type> resolvedConstraint = unwrapAliasType(constraintType);
            if (!resolvedBinding || !resolvedConstraint ||
                resolvedBinding->kind() != TypeKind::Struct ||
                resolvedConstraint->kind() != TypeKind::Struct)
                return false;

            auto constraintStruct = resolvedConstraint.AsFast<StructType>();
            if (!constraintStruct || !constraintStruct->isInterface)
                return false;

            std::function<bool(const Ref<Type>&)> matchesCandidate = [&](const Ref<Type>& candidateType) -> bool
            {
                Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                if (!resolvedCandidate || resolvedCandidate->kind() != TypeKind::Struct)
                    return false;

                auto candidateStruct = resolvedCandidate.AsFast<StructType>();
                if (!candidateStruct)
                    return false;

                if (candidateStruct->name == constraintStruct->name &&
                    candidateStruct->scopePath == constraintStruct->scopePath &&
                    candidateStruct->genericArguments.size() == constraintStruct->genericArguments.size())
                {
                    bool argumentsMatch = true;
                    for (size_t i = 0; i < constraintStruct->genericArguments.size(); ++i)
                    {
                        Ref<Type> expected = unwrapAliasType(constraintStruct->genericArguments[i]);
                        Ref<Type> actual = unwrapAliasType(candidateStruct->genericArguments[i]);
                        if (expected && expected->kind() == TypeKind::GenericParameter &&
                            expected.AsFast<GenericParameterType>()->name == parameterName)
                        {
                            if (!isExactConstraintTypeMatch(actual, resolvedBinding))
                                argumentsMatch = false;
                        }
                        else if (!isExactConstraintTypeMatch(actual, expected))
                        {
                            argumentsMatch = false;
                        }
                    }
                    if (argumentsMatch)
                        return true;
                }

                return std::ranges::any_of(candidateStruct->baseTypes, [&](const Ref<Type>& baseType)
                {
                    return matchesCandidate(baseType);
                });
            };

            return matchesCandidate(resolvedBinding);
        }

        bool isOpenNativeTemplateIntrinsic(const std::vector<NodePtr<AttributeStatement>>& attributes)
        {
            const Token* cppNameArg = getFirstAttributeArg(attributes, Attribute::CppName);
            if (!cppNameArg)
                return false;

            return cppNameArg->value == "wio::runtime::EnumCount" ||
                   cppNameArg->value == "wio::runtime::EnumName" ||
                   cppNameArg->value == "wio::runtime::EnumValue" ||
                   cppNameArg->value == "wio::runtime::EnumIndex" ||
                   cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                   cppNameArg->value == "wio::runtime::EnumSize" ||
                   cppNameArg->value == "wio::runtime::EnumIsValid" ||
                   cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                   cppNameArg->value == "wio::runtime::EnumFromRaw" ||
                   cppNameArg->value == "wio::runtime::ReflectedTypeName" ||
                   cppNameArg->value == "wio::runtime::ReflectedKind" ||
                   cppNameArg->value == "wio::runtime::ReflectedSize" ||
                   cppNameArg->value == "wio::runtime::ReflectedAlignment" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldAccess" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodNames" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodSignatures" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodAccess" ||
                   cppNameArg->value == "wio::runtime::ReflectedBaseTypes" ||
                   cppNameArg->value == "wio::runtime::ReflectedFieldCount" ||
                   cppNameArg->value == "wio::runtime::ReflectedMethodCount" ||
                   cppNameArg->value == "wio::runtime::traits::IsIntegerValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsNumericValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsFloatingValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsSignedValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsUnsignedValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsArrayValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsDictionaryValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsEnumValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsFlagsetValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsObjectValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsComponentValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsInterfaceValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsSameValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsDefaultConstructibleValue" ||
                   cppNameArg->value == "wio::runtime::traits::IsCopyConstructibleValue";
        }

        bool matchesOpenNativeTemplateIntrinsicConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                                           const std::vector<Ref<Type>>& bindingTypes)
        {
            const Token* cppNameArg = getFirstAttributeArg(attributes, Attribute::CppName);
            if (!cppNameArg)
                return true;

            if (cppNameArg->value == "wio::runtime::EnumCount" ||
                cppNameArg->value == "wio::runtime::EnumName" ||
                cppNameArg->value == "wio::runtime::EnumValue" ||
                cppNameArg->value == "wio::runtime::EnumIndex" ||
                cppNameArg->value == "wio::runtime::EnumUnderlyingTypeName" ||
                cppNameArg->value == "wio::runtime::EnumSize" ||
                cppNameArg->value == "wio::runtime::EnumIsValid" ||
                cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                cppNameArg->value == "wio::runtime::EnumFromRaw")
            {
                return bindingTypes.size() == 1 &&
                       (cppNameArg->value == "wio::runtime::EnumIsValid" ||
                        cppNameArg->value == "wio::runtime::EnumTryFromRaw" ||
                        cppNameArg->value == "wio::runtime::EnumFromRaw"
                            ? isEnumConstraintType(bindingTypes.front())
                            : (isEnumConstraintType(bindingTypes.front()) || isFlagsetConstraintType(bindingTypes.front())));
            }

            return true;
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
                Ref<Type> preparedConstraintType = getPreparedConstraintType(analyzer, argument.typeSpecifier);
                if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value, preparedConstraintType))
                {
                    if (argument.typeSpecifier->generics.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(
                            errorLocation,
                            "{} predicate '{}' expects exactly one generic parameter operand.",
                            attributeName,
                            getGenericConstraintTraitDisplayName(*predicateTrait)
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
                            getGenericConstraintTraitDisplayName(*predicateTrait),
                            expectedParameterName
                        );
                        return false;
                    }

                    return true;
                }

                Ref<Type> exactType = preparedConstraintType;
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
                    if (isUserDefinedTraitConstraint(exactType, expectedParameterName))
                        return true;

                    WIO_LOG_ADD_ERROR(
                        errorLocation,
                        "{} must use fully concrete type constraints or supported predicates like std::traits::IsInteger<{}>.",
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
                "{} supports concrete types{} or supported predicates like std::traits::IsInteger<{}> and std::traits::IsNumeric<{}>.",
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

                if (genericParameterNames.size() == 1)
                {
                    if (applyAttribute->args.empty())
                    {
                        WIO_LOG_ADD_ERROR(
                            applyAttribute->location(),
                            "@Apply on single-parameter generic {} declarations expects at least one type or predicate argument.",
                            declarationKind
                        );
                        allValid = false;
                        continue;
                    }
                }
                else if (applyAttribute->args.size() != genericParameterNames.size())
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
                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, genericParameterNames, i);
                    if (constraintArguments.empty())
                    {
                        allValid = false;
                        continue;
                    }

                    for (const auto& constraintArgument : constraintArguments)
                    {
                        if (!validateGenericConstraintArgument(
                                analyzer,
                                constraintArgument,
                                genericParameterNames[i],
                                "@Apply",
                                applyAttribute->location(),
                                true))
                        {
                            allValid = false;
                        }
                    }
                }
            }

            return allValid;
        }

        bool matchesApplyConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     const bool hasGenericParameterPack,
                                     const GenericBindingSet& bindings)
        {
            const auto applyAttributes = getAttributeStatements(attributes, Attribute::Apply);
            if (applyAttributes.empty())
                return true;

            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (genericParameterNames.size() != 1 && applyAttribute->args.size() != genericParameterNames.size())
                    continue;

                bool matches = true;
                for (size_t i = 0; i < genericParameterNames.size(); ++i)
                {
                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, genericParameterNames, i);
                    const bool isPackParameter = hasGenericParameterPack && i + 1 == genericParameterNames.size();
                    const std::string& genericParameterName = genericParameterNames[i];

                    if (constraintArguments.empty())
                    {
                        matches = false;
                        break;
                    }

                    auto evaluateConstraintAgainstSingleType = [&](const AttributeTypeArgument& argument,
                                                                   const Ref<Type>& bindingType) -> bool
                    {
                        if (!bindingType || bindingType->isUnknown() || containsGenericParameterType(bindingType))
                            return true;

                        if (argument.typeSpecifier)
                        {
                            if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(
                                    argument.typeSpecifier->name.value,
                                    argument.typeSpecifier->refType.Lock()))
                            {
                                return predicateTrait->predicate(bindingType);
                            }

                            Ref<Type> exactType = argument.typeSpecifier->refType.Lock();
                            if (isUserDefinedTraitConstraint(exactType, genericParameterName))
                            {
                                return matchesUserDefinedTraitConstraint(
                                    bindingType,
                                    exactType,
                                    genericParameterName);
                            }
                            return exactType && isExactConstraintTypeMatch(bindingType, exactType);
                        }

                        if (argument.token.type == TokenType::kwTrue)
                            return true;

                        if (argument.token.type == TokenType::kwFalse)
                            return false;

                        return false;
                    };

                    auto evaluateConstraintSetAgainstSingleType = [&](const Ref<Type>& bindingType) -> bool
                    {
                        return std::ranges::any_of(constraintArguments, [&](const AttributeTypeArgument& argument)
                        {
                            return evaluateConstraintAgainstSingleType(argument, bindingType);
                        });
                    };

                    if (!isPackParameter)
                    {
                        auto bindingIt = bindings.directBindings.find(genericParameterName);
                        if (bindingIt == bindings.directBindings.end())
                        {
                            matches = false;
                            break;
                        }

                        if (!evaluateConstraintSetAgainstSingleType(bindingIt->second))
                        {
                            matches = false;
                            break;
                        }

                        continue;
                    }

                    if (bindings.packAliases.contains(genericParameterName))
                        continue;

                    auto packIt = bindings.packBindings.find(genericParameterName);
                    if (packIt == bindings.packBindings.end())
                    {
                        matches = false;
                        break;
                    }

                    for (const auto& packElementType : packIt->second)
                    {
                        if (!evaluateConstraintSetAgainstSingleType(packElementType))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (!matches)
                        break;
                }

                if (matches)
                    return true;
            }

            return false;
        }

        bool matchesApplyConstraints(const std::vector<NodePtr<AttributeStatement>>& attributes,
                                     const std::vector<std::string>& genericParameterNames,
                                     const bool hasGenericParameterPack,
                                     const std::vector<Ref<Type>>& bindingTypes)
        {
            const size_t minimumBindingCount = getMinimumGenericArgumentCount(genericParameterNames, hasGenericParameterPack);
            if ((!hasGenericParameterPack && bindingTypes.size() != genericParameterNames.size()) ||
                (hasGenericParameterPack && bindingTypes.size() < minimumBindingCount))
            {
                return false;
            }

            return matchesApplyConstraints(
                attributes,
                genericParameterNames,
                hasGenericParameterPack,
                buildExtendedGenericBindings(genericParameterNames, hasGenericParameterPack, bindingTypes)
            );
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
                                                                         const std::vector<std::string>& genericParameterNames,
                                                                         const bool hasGenericParameterPack)
        {
            std::vector<std::vector<Ref<Type>>> instantiations;
            const auto instantiateAttributes = getAttributeStatements(attributes, Attribute::Instantiate);
            if (instantiateAttributes.empty())
                return instantiations;

            const size_t fixedCount = getMinimumGenericArgumentCount(genericParameterNames, hasGenericParameterPack);
            std::unordered_set<std::string> seenInstantiationSignatures;
            for (const auto* instantiateAttribute : instantiateAttributes)
            {
                if (!instantiateAttribute)
                    continue;

                if ((!hasGenericParameterPack && instantiateAttribute->args.size() != genericParameterNames.size()) ||
                    (hasGenericParameterPack && instantiateAttribute->args.size() < fixedCount))
                {
                    WIO_LOG_ADD_ERROR(
                        instantiateAttribute->location(),
                        hasGenericParameterPack
                            ? common::formatString("@Instantiate expects at least {} arguments for this generic pack declaration.", fixedCount)
                            : common::formatString("@Instantiate expects exactly {} arguments.", genericParameterNames.size())
                    );
                    continue;
                }

                std::vector<std::vector<Ref<Type>>> candidateLists;
                candidateLists.reserve(fixedCount);
                std::vector<Ref<Type>> concretePackTypes;
                bool isValidInstantiation = true;

                for (size_t i = 0; i < fixedCount; ++i)
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
                        if (const auto* predicateTrait = findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value,
                                                                                              argument.typeSpecifier->refType.Lock()))
                        {
                            candidateLists.push_back(predicateTrait->candidateTypes());
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

                if (hasGenericParameterPack)
                {
                    for (size_t i = fixedCount; i < instantiateAttribute->args.size(); ++i)
                    {
                        const auto argument = getAttributeTypeArgument(*instantiateAttribute, i);
                        if (!argument.typeSpecifier)
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate on generic pack declarations must provide concrete pack element types for '{}...'. Predicates belong in @Apply.",
                                genericParameterNames.back()
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        Ref<Type> exactType = getPreparedConstraintType(analyzer, argument.typeSpecifier);
                        if (findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value, exactType))
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate on generic pack declarations must provide concrete pack element types for '{}...'. Predicates belong in @Apply.",
                                genericParameterNames.back()
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        if (!exactType || exactType->isUnknown() || containsGenericParameterType(exactType))
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "@Instantiate pack element types must be fully concrete."
                            );
                            isValidInstantiation = false;
                            break;
                        }

                        concretePackTypes.push_back(exactType);
                    }
                }

                if (!isValidInstantiation)
                    continue;

                std::vector<Ref<Type>> currentInstantiation;
                currentInstantiation.reserve(candidateLists.size() + concretePackTypes.size());

                std::function<void(size_t)> expandCandidates = [&](size_t index)
                {
                    if (index == candidateLists.size())
                    {
                        std::vector<Ref<Type>> fullInstantiation = currentInstantiation;
                        fullInstantiation.insert(fullInstantiation.end(), concretePackTypes.begin(), concretePackTypes.end());

                        std::string signatureKey;
                        for (size_t i = 0; i < fullInstantiation.size(); ++i)
                        {
                            if (i > 0)
                                signatureKey += "|";
                            signatureKey += fullInstantiation[i] ? fullInstantiation[i]->toString() : "<unknown>";
                        }

                        if (!seenInstantiationSignatures.insert(signatureKey).second)
                        {
                            WIO_LOG_ADD_ERROR(
                                instantiateAttribute->location(),
                                "Duplicate @Instantiate declaration for '{}'.",
                                formatConcreteInstantiationSignature(fullInstantiation)
                            );
                            return;
                        }

                        instantiations.push_back(std::move(fullInstantiation));
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
            case TypeKind::Nullable:
                return isSdkExportableFieldType(current.AsFast<NullableType>()->valueType);
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

        bool isNativePodInteropFieldType(const Ref<Type>& type, const bool allowGenericPlaceholders)
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
                return typeName != "void" && typeName != "string" && typeName != "object";
            }
            case TypeKind::GenericParameter:
                return allowGenericPlaceholders;
            case TypeKind::Array:
            {
                auto arrayType = current.AsFast<ArrayType>();
                if (!arrayType || arrayType->arrayKind != ArrayType::ArrayKind::Static)
                    return false;

                return isNativePodInteropFieldType(arrayType->elementType, allowGenericPlaceholders);
            }
            case TypeKind::Struct:
            {
                auto structType = current.AsFast<StructType>();
                if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                    return false;

                if (allowGenericPlaceholders)
                    return true;

                return std::ranges::all_of(structType->fieldTypes, [](const Ref<Type>& fieldType)
                {
                    return isNativePodInteropFieldType(fieldType, false);
                });
            }
            default:
                return false;
            }
        }

        void validateInstantiatedNativePodComponent(const Ref<StructType>& structType,
                                                    const common::Location& errorLocation)
        {
            if (!structType || !structType->isNativePodComponent)
                return;

            const bool hasConcreteInstantiation =
                structType->genericParameterNames.empty() ||
                (!structType->genericArguments.empty() &&
                 structType->genericArguments.size() == structType->genericParameterNames.size() &&
                 std::ranges::all_of(structType->genericArguments, [](const Ref<Type>& genericArgument)
                 {
                     return genericArgument && !genericArgument->isUnknown() && !containsGenericParameterType(genericArgument);
                 }));

            if (!hasConcreteInstantiation)
                return;

            for (size_t fieldIndex = 0; fieldIndex < structType->fieldTypes.size() && fieldIndex < structType->fieldNames.size(); ++fieldIndex)
            {
                if (isNativePodInteropFieldType(structType->fieldTypes[fieldIndex], false))
                    continue;

                WIO_LOG_ADD_ERROR(
                    errorLocation,
                    "Declaration-level @Native component '{}' field '{}' resolves to type '{}' which is not POD-native-compatible yet. Supported field types are primitives, POD-compatible static arrays, and other declaration-level @Native components.",
                    structType->toString(),
                    structType->fieldNames[fieldIndex],
                    structType->fieldTypes[fieldIndex] ? structType->fieldTypes[fieldIndex]->toString() : "<unknown>"
                );
            }
        }

        Ref<StructType> getNativePodComponentStructType(const Ref<Type>& type)
        {
            Ref<Type> current = type;
            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current)
                return nullptr;

            if (current->kind() == TypeKind::Reference)
                current = current.AsFast<ReferenceType>()->referredType;

            while (current && current->kind() == TypeKind::Alias)
                current = current.AsFast<AliasType>()->aliasedType;

            if (!current || current->kind() != TypeKind::Struct)
                return nullptr;

            auto structType = current.AsFast<StructType>();
            if (!structType || structType->isObject || structType->isInterface || !structType->isNativePodComponent)
                return nullptr;

            if (!structType->genericArguments.empty())
            {
                if (auto structScope = structType->structScope.Lock())
                {
                    if (auto baseSymbol = structScope->resolve(structType->name);
                        baseSymbol && baseSymbol->kind == SymbolKind::Struct)
                    {
                        auto baseStruct = baseSymbol->type.AsFast<StructType>();
                        if (baseStruct && baseStruct.Get() != structType.Get() && !baseStruct->genericParameterNames.empty())
                            structType = instantiateGenericStructType(baseStruct, structType->genericArguments).AsFast<StructType>();
                    }
                }
            }

            return structType;
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
        variableDeclarationsBySymbol_.clear();
        attributeListsBySymbol_.clear();
        exportedCppSymbolLocations_.clear();
        validatedGenericFunctionBodyKeys_.clear();
        validatingGenericFunctionBodyKeys_.clear();
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

    bool SemanticAnalyzer::validateConcreteGenericFunctionBody(
        const FunctionDeclaration& node,
        const Ref<Symbol>& funcSym,
        const Ref<FunctionType>& declaredFunctionType,
        const Ref<StructType>& concreteOwnerType,
        const std::unordered_map<std::string, Ref<Type>>& directBindings,
        const std::unordered_map<std::string, std::vector<Ref<Type>>>& packBindings,
        const std::unordered_map<std::string, std::string>& packAliases)
    {
        if (!funcSym || !declaredFunctionType)
            return true;

        const bool hasFunctionGenericContext = !funcSym->genericParameterNames.empty();
        const bool hasOwnerGenericDeclarations =
            concreteOwnerType &&
            !concreteOwnerType->genericParameterNames.empty();
        const size_t minimumOwnerGenericArgumentCount =
            concreteOwnerType
                ? getMinimumGenericArgumentCount(
                    concreteOwnerType->genericParameterNames,
                    concreteOwnerType->hasGenericParameterPack
                )
                : 0u;
        const bool hasOwnerGenericContext =
            concreteOwnerType &&
            !concreteOwnerType->genericParameterNames.empty() &&
            concreteOwnerType->genericArguments.size() >= minimumOwnerGenericArgumentCount &&
            std::ranges::all_of(concreteOwnerType->genericArguments, [](const Ref<Type>& type)
            {
                return type && !type->isUnknown() && !containsGenericParameterType(type);
            });
        if (!hasFunctionGenericContext && !hasOwnerGenericDeclarations)
            return true;

        GenericBindingSet bindingSet;
        if (hasOwnerGenericContext)
        {
            bindingSet = buildExtendedGenericBindings(
                concreteOwnerType->genericParameterNames,
                concreteOwnerType->hasGenericParameterPack,
                concreteOwnerType->genericArguments
            );
        }

        for (const auto& [name, type] : directBindings)
            bindingSet.directBindings[name] = type;
        for (const auto& [name, types] : packBindings)
            bindingSet.packBindings[name] = types;
        for (const auto& [name, alias] : packAliases)
            bindingSet.packAliases[name] = alias;

        ValidationAnnotationSnapshot annotationSnapshot;
        annotationSnapshot.capture(node.whenCondition.Get());
        annotationSnapshot.capture(node.whenFallback.Get());
        annotationSnapshot.capture(node.body.Get());

        std::optional<std::vector<Ref<Type>>> materializedInstantiation;
        if (hasFunctionGenericContext)
        {
            materializedInstantiation = tryMaterializeConcreteInstantiation(
                funcSym->genericParameterNames,
                funcSym->hasGenericParameterPack,
                bindingSet
            );
            if (!materializedInstantiation.has_value())
                return true;
        }

        std::string validationKey = common::formatString(
            "{}::{}::owner<{}>::func<{}>",
            funcSym.Get(),
            node.name ? node.name->token.value : "<function>",
            hasOwnerGenericContext ? concreteOwnerType->toString() : std::string("<none>"),
            materializedInstantiation.has_value() ? formatDiagnosticTypeList(*materializedInstantiation) : std::string("<none>")
        );
        if (validatedGenericFunctionBodyKeys_.contains(validationKey))
            return true;
        if (validatingGenericFunctionBodyKeys_.contains(validationKey))
            return true;

        validatingGenericFunctionBodyKeys_.insert(validationKey);

        auto finalizeValidation = [&](const bool succeeded)
        {
            validatingGenericFunctionBodyKeys_.erase(validationKey);
            if (succeeded)
                validatedGenericFunctionBodyKeys_.insert(validationKey);
        };

        std::unordered_map<std::string, Ref<Type>> ownerGenericScope;
        if (hasOwnerGenericDeclarations)
        {
            ownerGenericScope.reserve(concreteOwnerType->genericParameterNames.size());
            for (size_t genericIndex = 0; genericIndex < concreteOwnerType->genericParameterNames.size(); ++genericIndex)
            {
                const auto& parameterName = concreteOwnerType->genericParameterNames[genericIndex];
                const bool isGenericParameterPack =
                    concreteOwnerType->hasGenericParameterPack &&
                    genericIndex + 1 == concreteOwnerType->genericParameterNames.size();

                if (isGenericParameterPack)
                {
                    ownerGenericScope.emplace(
                        parameterName,
                        Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    );
                }
                else if (genericIndex < concreteOwnerType->genericArguments.size() &&
                         concreteOwnerType->genericArguments[genericIndex] &&
                         !concreteOwnerType->genericArguments[genericIndex]->isUnknown() &&
                         !containsGenericParameterType(concreteOwnerType->genericArguments[genericIndex]))
                {
                    ownerGenericScope.emplace(parameterName, concreteOwnerType->genericArguments[genericIndex]);
                }
                else
                {
                    ownerGenericScope.emplace(
                        parameterName,
                        Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName)
                    );
                }
            }
        }

        std::unordered_map<std::string, Ref<Type>> functionGenericScope;
        if (hasFunctionGenericContext)
        {
            functionGenericScope.reserve(node.genericParameters.size());
            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();

                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                functionGenericScope.emplace(
                    parameterName,
                    isGenericParameterPack
                        ? parameterType
                        : instantiateGenericType(parameterType, bindingSet)
                );
            }
        }

        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        Ref<Type> previousFunctionReturnType = currentFunctionReturnType_;
        Ref<Type> previousCurrentStructType = currentStructType_;
        Ref<Type> previousCurrentBaseStructType = currentBaseStructType_;
        Ref<Symbol> previousFunctionParameterPackSymbol = currentFunctionParameterPackSymbol_;
        Ref<Type> previousFunctionParameterPackType = currentFunctionParameterPackType_;
        Ref<Scope> previousScope = currentScope_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;

        currentExpectedExpressionType_ = nullptr;
        currentFunctionReturnType_ = instantiateGenericType(declaredFunctionType->returnType, bindingSet);
        currentStructType_ = concreteOwnerType;
        currentBaseStructType_ = nullptr;
        if (concreteOwnerType)
        {
            for (const auto& baseType : concreteOwnerType->baseTypes)
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
        }
        currentFunctionParameterPackSymbol_ = nullptr;
        currentFunctionParameterPackType_ = nullptr;

        if (hasOwnerGenericDeclarations)
            genericTypeParameterScopes_.push_back(ownerGenericScope);
        if (hasFunctionGenericContext)
            genericTypeParameterScopes_.push_back(functionGenericScope);

        Ref<Symbol> ownerConstraintSymbol = nullptr;
        if (hasOwnerGenericDeclarations && !scopes_.empty())
        {
            Ref<Scope> globalScope = scopes_.front();
            const std::string qualifiedOwnerName = concreteOwnerType->scopePath.empty()
                ? concreteOwnerType->name
                : concreteOwnerType->scopePath + "::" + concreteOwnerType->name;
            ownerConstraintSymbol = resolveQualifiedSymbol(globalScope, qualifiedOwnerName);
        }

        if (ownerConstraintSymbol)
            activeGenericConstraintSymbols_.push_back(ownerConstraintSymbol);
        if (hasFunctionGenericContext)
            activeGenericConstraintSymbols_.push_back(funcSym);
        if (funcSym->innerScope)
            currentScope_ = funcSym->innerScope;
        enterScope(ScopeKind::Function);

        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            Ref<Type> sourceParameterType =
                funcSym->type && funcSym->type->kind() == TypeKind::Function
                    ? funcSym->type.AsFast<FunctionType>()->paramTypes[i]
                    : declaredFunctionType->paramTypes[i];
            Ref<Type> instantiatedParameterType = instantiateGenericType(sourceParameterType, bindingSet);

            if (param.isParameterPack)
            {
                std::string packName;
                if (containsGenericParameterPackType(sourceParameterType, &packName) && !packName.empty())
                {
                    auto& typeContext = Compiler::get().getTypeContext();
                    if (auto aliasIt = bindingSet.packAliases.find(packName); aliasIt != bindingSet.packAliases.end())
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(aliasIt->second);
                    }
                    else if (auto packIt = bindingSet.packBindings.find(packName); packIt != bindingSet.packBindings.end())
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(packName, packIt->second);
                    }
                    else
                    {
                        instantiatedParameterType = typeContext.getOrCreateValuePackViewType(packName);
                    }
                }
            }

            SymbolFlags parameterFlags = SymbolFlags::createAllFalse();
            if (param.isParameterPack)
                parameterFlags.set_isParameterPack(true);

            Ref<Symbol> parameterSymbol = createSymbol(
                param.name->token.value,
                instantiatedParameterType,
                SymbolKind::Parameter,
                param.name->location(),
                parameterFlags
            );
            currentScope_->define(param.name->token.value, parameterSymbol);

            if (param.isParameterPack)
            {
                currentFunctionParameterPackSymbol_ = parameterSymbol;
                currentFunctionParameterPackType_ = instantiatedParameterType;
            }
        }

        const int32_t previousErrorCount = Logger::get().getErrorCount();

        if (node.whenCondition)
        {
            node.whenCondition->accept(*this);

            if (auto conditionType = node.whenCondition->refType.Lock();
                !(conditionType == Compiler::get().getTypeContext().getBool() ||
                  allowsNumericSemantics(conditionType) ||
                  (conditionType && (conditionType->kind() == TypeKind::Reference || conditionType->kind() == TypeKind::Null))))
            {
                WIO_LOG_ADD_ERROR(
                    node.whenCondition->location(),
                    "When guard condition must be a boolean, numeric, or reference type. Got: {}",
                    conditionType->toString()
                );
            }

            if (node.whenFallback)
            {
                currentExpectedExpressionType_ = currentFunctionReturnType_;
                allowContextualNumericLiteralTyping_ = true;
                node.whenFallback->accept(*this);
                currentExpectedExpressionType_ = nullptr;
                allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

                Ref<Type> fallbackType = node.whenFallback->refType.Lock();
                if (currentFunctionReturnType_ &&
                    !currentFunctionReturnType_->isUnknown() &&
                    fallbackType &&
                    !fallbackType->isUnknown() &&
                    !isAssignmentLikeCompatible(currentFunctionReturnType_, fallbackType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.whenFallback->location(),
                        "When guard fallback type mismatch! Expected '{}', but got '{}'.",
                        currentFunctionReturnType_->toString(),
                        fallbackType->toString()
                    );
                }
            }
            else if (currentFunctionReturnType_ != Compiler::get().getTypeContext().getVoid())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Functions with a return value must provide an 'else' fallback for 'when' guards.");
            }
        }

        if (node.body)
            node.body->accept(*this);

        const bool requiresReturnValue = currentFunctionReturnType_ &&
                                         !currentFunctionReturnType_->isUnknown() &&
                                         !currentFunctionReturnType_->isVoid();
        const bool allPathsReturn = statementDefinitelyReturns(node.body);

        if (node.body && requiresReturnValue && !allPathsReturn)
        {
            WIO_LOG_ADD_ERROR(
                node.name ? node.name->location() : node.location(),
                "Non-void function '{}' must return a value on all control-flow paths.",
                node.name ? node.name->token.value : "<function>"
            );
        }

        const bool succeeded = Logger::get().getErrorCount() == previousErrorCount;

        exitScope();
        if (hasFunctionGenericContext)
            activeGenericConstraintSymbols_.pop_back();
        if (ownerConstraintSymbol)
            activeGenericConstraintSymbols_.pop_back();
        if (hasFunctionGenericContext)
            genericTypeParameterScopes_.pop_back();
        if (hasOwnerGenericContext)
            genericTypeParameterScopes_.pop_back();
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        currentFunctionReturnType_ = previousFunctionReturnType;
        currentStructType_ = previousCurrentStructType;
        currentBaseStructType_ = previousCurrentBaseStructType;
        currentFunctionParameterPackSymbol_ = previousFunctionParameterPackSymbol;
        currentFunctionParameterPackType_ = previousFunctionParameterPackType;
        currentScope_ = previousScope;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
        annotationSnapshot.restore();

        finalizeValidation(succeeded);
        return succeeded;
    }

    SemanticAnalyzer::GenericConstraintCapabilities SemanticAnalyzer::resolveGenericConstraintCapabilities(const Ref<Type>& type) const
    {
        GenericConstraintCapabilities info;

        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType || resolvedType->kind() != TypeKind::GenericParameter)
            return info;

        const std::string& genericParameterName = resolvedType.AsFast<GenericParameterType>()->name;

        for (auto symbolIt = activeGenericConstraintSymbols_.rbegin();
             symbolIt != activeGenericConstraintSymbols_.rend();
             ++symbolIt)
        {
            const Ref<Symbol>& activeSymbol = *symbolIt;
            if (!activeSymbol)
                continue;

            auto genericParameterIt = std::ranges::find(activeSymbol->genericParameterNames, genericParameterName);
            if (genericParameterIt == activeSymbol->genericParameterNames.end())
                continue;

            const size_t genericParameterIndex = static_cast<size_t>(std::distance(activeSymbol->genericParameterNames.begin(), genericParameterIt));

            auto attributeIt = attributeListsBySymbol_.find(activeSymbol.Get());
            if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                return info;

            std::vector<const AttributeStatement*> applyAttributes;
            for (const auto& attributeNode : *attributeIt->second)
            {
                if (!attributeNode || attributeNode->attribute != Attribute::Apply)
                    continue;

                applyAttributes.push_back(attributeNode.Get());
            }

            if (applyAttributes.empty())
                return info;

            bool sawApplicableConstraint = false;
            bool allConstraintsCompatible = true;
            auto& typeContext = Compiler::get().getTypeContext();

            for (const auto* applyAttribute : applyAttributes)
            {
                if (!applyAttribute)
                    continue;

                if (activeSymbol->genericParameterNames.size() != 1 &&
                    applyAttribute->args.size() != activeSymbol->genericParameterNames.size())
                {
                    continue;
                }

                const auto constraintArguments =
                    getApplyConstraintArguments(*applyAttribute, activeSymbol->genericParameterNames, genericParameterIndex);
                if (constraintArguments.empty())
                    continue;

                sawApplicableConstraint = true;
                bool attributeAllowsInteger = false;
                bool attributeAllowsNumeric = false;
                bool attributeAllowsEnum = false;
                bool attributeAllowsFlagset = false;
                bool attributeAllowsObjectLike = false;
                bool attributeIsCompatible = true;

                for (const auto& argument : constraintArguments)
                {
                    if (argument.typeSpecifier)
                    {
                        if (const auto* predicateTrait =
                                findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value,
                                                                     argument.typeSpecifier->refType.Lock()))
                        {
                            switch (predicateTrait->kind)
                            {
                            case GenericConstraintTraitKind::IsInteger:
                                attributeAllowsInteger = true;
                                attributeAllowsNumeric = true;
                                continue;
                            case GenericConstraintTraitKind::IsNumeric:
                            case GenericConstraintTraitKind::IsFloating:
                            case GenericConstraintTraitKind::IsSigned:
                            case GenericConstraintTraitKind::IsUnsigned:
                                attributeAllowsNumeric = true;
                                continue;
                            case GenericConstraintTraitKind::IsEnum:
                                attributeAllowsEnum = true;
                                continue;
                            case GenericConstraintTraitKind::IsFlagset:
                                attributeAllowsFlagset = true;
                                continue;
                            case GenericConstraintTraitKind::IsObject:
                            case GenericConstraintTraitKind::IsInterface:
                                attributeAllowsObjectLike = true;
                                continue;
                            case GenericConstraintTraitKind::IsComponent:
                            case GenericConstraintTraitKind::IsArray:
                            case GenericConstraintTraitKind::IsReference:
                                attributeIsCompatible = false;
                                break;
                            }
                        }

                        Ref<Type> exactType = unwrapAliasType(argument.typeSpecifier->refType.Lock());
                        if (!exactType || exactType->isUnknown() || containsGenericParameterType(exactType))
                        {
                            attributeIsCompatible = false;
                            break;
                        }

                        if (isIntegerConstraintType(exactType))
                        {
                            attributeAllowsInteger = true;
                            attributeAllowsNumeric = true;
                            continue;
                        }

                        if (isNumericConstraintType(exactType))
                        {
                            attributeAllowsNumeric = true;
                            continue;
                        }

                        if (isEnumConstraintType(exactType))
                        {
                            attributeAllowsEnum = true;
                            continue;
                        }

                        if (isFlagsetConstraintType(exactType))
                        {
                            attributeAllowsFlagset = true;
                            continue;
                        }

                        if (getObjectOrInterfaceStructType(exactType) || isExactType(exactType, typeContext.getObject()))
                        {
                            attributeAllowsObjectLike = true;
                            continue;
                        }
                    }

                    attributeIsCompatible = false;
                    break;
                }

                const bool attributeAllowsAny =
                    attributeAllowsInteger ||
                    attributeAllowsNumeric ||
                    attributeAllowsEnum ||
                    attributeAllowsFlagset ||
                    attributeAllowsObjectLike;

                const bool hasConflictingCategories =
                    (attributeAllowsObjectLike &&
                     (attributeAllowsInteger || attributeAllowsNumeric || attributeAllowsEnum || attributeAllowsFlagset)) ||
                    (attributeAllowsEnum && attributeAllowsFlagset);

                if (!attributeIsCompatible || !attributeAllowsAny || hasConflictingCategories)
                {
                    allConstraintsCompatible = false;
                    break;
                }

                if ((info.allowsObjectLike &&
                     (attributeAllowsInteger || attributeAllowsNumeric || attributeAllowsEnum || attributeAllowsFlagset)) ||
                    (attributeAllowsObjectLike &&
                     (info.allowsInteger || info.allowsNumeric || info.allowsEnum || info.allowsFlagset)) ||
                    (info.allowsEnum && attributeAllowsFlagset) ||
                    (info.allowsFlagset && attributeAllowsEnum))
                {
                    allConstraintsCompatible = false;
                    break;
                }

                if (attributeAllowsInteger)
                    info.allowsInteger = true;
                if (attributeAllowsNumeric)
                    info.allowsNumeric = true;
                if (attributeAllowsEnum)
                    info.allowsEnum = true;
                if (attributeAllowsFlagset)
                    info.allowsFlagset = true;
                if (attributeAllowsObjectLike)
                    info.allowsObjectLike = true;
            }

            if (!sawApplicableConstraint || !allConstraintsCompatible)
            {
                info.hasApplicableConstraints = sawApplicableConstraint;
                info.isIncompatible = sawApplicableConstraint;
                return info;
            }

            info.hasApplicableConstraints = true;
            info.isKnown =
                info.allowsInteger ||
                info.allowsNumeric ||
                info.allowsEnum ||
                info.allowsFlagset ||
                info.allowsObjectLike;
            info.isIncompatible = !info.isKnown;
            return info;
        }

        return info;
    }

    bool SemanticAnalyzer::allowsNumericSemantics(const Ref<Type>& type) const
    {
        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType)
            return false;

        if (resolvedType->isNumeric())
            return true;

        const GenericConstraintCapabilities capabilities = resolveGenericConstraintCapabilities(resolvedType);
        return capabilities.allowsNumeric || capabilities.allowsInteger;
    }

    bool SemanticAnalyzer::allowsIntegerSemantics(const Ref<Type>& type) const
    {
        Ref<Type> resolvedType = unwrapAliasType(type);
        if (!resolvedType)
            return false;

        if (isIntegralLikeType(resolvedType))
            return true;

        const GenericConstraintCapabilities capabilities = resolveGenericConstraintCapabilities(resolvedType);
        return capabilities.allowsInteger;
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
                stmt->is<ExtensionDeclaration>() ||
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

        auto applyNullableSuffix = [&](Ref<Type> type) -> Ref<Type>
        {
            if (!node.isNullable)
                return type;

            if (!isNullableCapableType(type))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Type '{}' cannot be nullable. Only object/interface handles, ref/view, function, and opaque types support '?'.",
                    type ? type->toString() : "<unknown>"
                );
                return Compiler::get().getTypeContext().getUnknown();
            }

            return Compiler::get().getTypeContext().getOrCreateNullableType(std::move(type));
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

            if (!matchesApplyConstraints(
                    *attributeIt->second,
                    symbol->genericParameterNames,
                    symbol->hasGenericParameterPack,
                    explicitTypeArguments))
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
            bool hasParameterPack = false;
            for (size_t i = 1; i < node.generics.size(); ++i)
            {
                node.generics[i]->accept(*this);
                Ref<Type> parameterType = node.generics[i]->refType.Lock();
                if (containsGenericParameterPackType(parameterType))
                {
                    if (i + 1 != node.generics.size())
                    {
                        WIO_LOG_ADD_ERROR(node.generics[i]->location(), "Function type parameter packs must be trailing.");
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return;
                    }
                    hasParameterPack = true;
                }
                paramTypes.push_back(parameterType);
            }

            if (containsGenericParameterPackType(retType))
            {
                WIO_LOG_ADD_ERROR(node.generics[0]->location(), "Function type return positions cannot use generic parameter packs.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = applyNullableSuffix(
                Compiler::get().getTypeContext().getOrCreateFunctionType(retType, paramTypes, hasParameterPack)
            );
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

            node.refType = applyNullableSuffix(Compiler::get().getTypeContext().getOrCreateDictionaryType(
                node.generics[0]->refType.Lock(),
                node.generics[1]->refType.Lock(),
                isOrdered
            ));
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
                            const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                                structType->genericParameterNames,
                                structType->hasGenericParameterPack
                            );
                            const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                                structType->genericParameterDefaults, fixedArgumentCount);
                            auto completedArguments = completeGenericTypeArguments(
                                structType->genericParameterNames,
                                structType->genericParameterDefaults,
                                structType->hasGenericParameterPack,
                                explicitTypeArguments);
                            if (!completedArguments)
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Generic type '{}' expects {} generic arguments, but got {}.",
                                    node.name.value,
                                    structType->hasGenericParameterPack
                                        ? common::formatString("at least {}", requiredArgumentCount)
                                        : requiredArgumentCount == fixedArgumentCount
                                            ? std::to_string(fixedArgumentCount)
                                            : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                explicitTypeArguments = std::move(*completedArguments);
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

                                type = instantiateGenericStructType(structType, explicitTypeArguments, node.location());
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
                            const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                                sym->genericParameterNames,
                                sym->hasGenericParameterPack
                            );
                            const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                                sym->genericParameterDefaults, fixedArgumentCount);
                            auto completedArguments = completeGenericTypeArguments(
                                sym->genericParameterNames,
                                sym->genericParameterDefaults,
                                sym->hasGenericParameterPack,
                                explicitTypeArguments);
                            if (!completedArguments)
                            {
                                WIO_LOG_ADD_ERROR(
                                    node.location(),
                                    "Type alias '{}' expects {} generic arguments, but got {}.",
                                    node.name.value,
                                    sym->hasGenericParameterPack
                                        ? common::formatString("at least {}", requiredArgumentCount)
                                        : requiredArgumentCount == fixedArgumentCount
                                            ? std::to_string(fixedArgumentCount)
                                            : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                                    explicitTypeArguments.size()
                                );
                                type = Compiler::get().getTypeContext().getUnknown();
                            }
                            else
                            {
                                explicitTypeArguments = std::move(*completedArguments);
                                if (!satisfiesApplyForSymbol(sym, explicitTypeArguments, "type alias"))
                                {
                                    type = Compiler::get().getTypeContext().getUnknown();
                                    node.refType = type;
                                    return;
                                }

                                const auto bindings = buildExtendedGenericBindings(
                                    sym->genericParameterNames,
                                    sym->hasGenericParameterPack,
                                    explicitTypeArguments
                                );

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

        if (node.packIndex)
        {
            node.packIndex->accept(*this);
            const auto expectedPackName =
                (type && type->kind() == TypeKind::GenericParameterPack) ? std::optional<std::string_view>(type.AsFast<GenericParameterPackType>()->name) :
                (type && type->kind() == TypeKind::TypePackView) ? std::optional<std::string_view>(type.AsFast<TypePackViewType>()->packName) :
                std::nullopt;
            auto indexBinding = tryEvaluatePackIndexBinding(node.packIndex, variableDeclarationsBySymbol_, expectedPackName);
            if (!indexBinding.has_value())
            {
                WIO_LOG_ADD_ERROR(node.packIndex->location(), "Pack type indexing requires a non-negative compile-time integer index.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
            else if (indexBinding->kind == PackElementBindingKind::FromEnd && indexBinding->value == 0)
            {
                WIO_LOG_ADD_ERROR(node.packIndex->location(), "Pack type indexing from '.size' must subtract at least 1.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
            else if (type && type->kind() == TypeKind::GenericParameterPack)
            {
                ParsedPackElementBinding reboundBinding = *indexBinding;
                reboundBinding.packName = type.AsFast<GenericParameterPackType>()->name;
                type = makeSyntheticPackElementType(reboundBinding);
            }
            else if (type && type->kind() == TypeKind::TypePackView)
            {
                auto packViewType = type.AsFast<TypePackViewType>();
                if (!packViewType->elementTypes.empty())
                {
                    if (auto resolvedIndex = tryResolveConcretePackElementIndex(*indexBinding, packViewType->elementTypes.size()))
                    {
                        type = packViewType->elementTypes[*resolvedIndex];
                    }
                    else
                    {
                        WIO_LOG_ADD_ERROR(
                            node.packIndex->location(),
                            "Pack type index is out of range for size {}.",
                            packViewType->elementTypes.size()
                        );
                        type = Compiler::get().getTypeContext().getUnknown();
                    }
                }
                else
                {
                    ParsedPackElementBinding reboundBinding = *indexBinding;
                    reboundBinding.packName = packViewType->packName;
                    type = makeSyntheticPackElementType(reboundBinding);
                }
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.location(), "Only generic type packs can be indexed in type position.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
        }

        if (node.isPackExpansion)
        {
            if (!type || type->kind() != TypeKind::GenericParameterPack)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Only generic parameter packs can use '...' in type position.");
                type = Compiler::get().getTypeContext().getUnknown();
            }
        }
        else if (type && type->kind() == TypeKind::GenericParameterPack)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Generic parameter packs must be written with '...' in type position.");
            type = Compiler::get().getTypeContext().getUnknown();
        }

        node.refType = applyNullableSuffix(type);
    }

    void SemanticAnalyzer::visit(BinaryExpression& node)
    {
        node.left->accept(*this);
        const auto previousNarrowing = nonNullNarrowedSymbols_;
        const bool isShortCircuitAnd =
            node.op.type == TokenType::opLogicalAnd || node.op.type == TokenType::kwAnd;
        const bool isShortCircuitOr =
            node.op.type == TokenType::opLogicalOr || node.op.type == TokenType::kwOr;
        if (isShortCircuitAnd || isShortCircuitOr)
        {
            auto comparison = node.left->as<BinaryExpression>();
            if (comparison &&
                (comparison->op.type == TokenType::opEqual || comparison->op.type == TokenType::opNotEqual))
            {
                const bool leftIsNull = comparison->left->is<NullExpression>();
                const bool rightIsNull = comparison->right->is<NullExpression>();
                if (leftIsNull != rightIsNull)
                {
                    auto candidate = leftIsNull ? comparison->right : comparison->left;
                    auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
                    Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
                    const bool trueMeansNonNull = comparison->op.type == TokenType::opNotEqual;
                    const bool rightExecutesOnTrue = isShortCircuitAnd;
                    if (symbol && symbolType && symbolType->kind() == TypeKind::Nullable &&
                        trueMeansNonNull == rightExecutesOnTrue)
                    {
                        nonNullNarrowedSymbols_.insert(symbol.Get());
                    }
                }
            }
        }
        node.right->accept(*this);
        nonNullNarrowedSymbols_ = previousNarrowing;

        const Ref<Type> initialLeftType = node.left->refType.Lock();
        const Ref<Type> initialRightType = node.right->refType.Lock();
        if (!initialLeftType || !initialRightType ||
            initialLeftType->isPoisoned() || initialRightType->isPoisoned())
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (node.op.type == TokenType::kwIn && node.right->is<RangeExpression>())
        {
            auto leftType = node.left->refType.Lock();
            if (leftType && !allowsNumericSemantics(leftType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "The left operand of 'in' operator must be numeric when used with a range.");
            }
            node.refType = Compiler::get().getTypeContext().getBool();
            return; 
        }
        if (node.op.type == TokenType::kwIs)
        {
            Ref<Type> lhsType = getAutoReadableType(node.left->refType.Lock());
            Ref<Type> rhsType = node.right->refType.Lock();

            if (isAnyType(lhsType))
            {
                if (!isSupportedAnyCastTargetType(rhsType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.right->location(),
                        "The right side of the 'is' operator must be a concrete runtime-storable type or interface when the left side is 'any'."
                    );
                }

                node.refType = Compiler::get().getTypeContext().getBool();
                return;
            }

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

        auto tryResolveBinaryOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getBinaryOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> lhsType = node.left->refType.Lock();
            Ref<Type> rhsType = node.right->refType.Lock();
            Ref<Type> readableLhsType = getAutoReadableType(lhsType);
            Ref<Type> readableRhsType = getAutoReadableType(rhsType);

            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                Ref<Type> ownerType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto deduceBindingsFromOperatorArgument = [&](const Ref<Type>& expectedType,
                                                          const Ref<Type>& argumentType,
                                                          const NodePtr<Expression>& argumentExpression,
                                                          std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                if (shouldAutoReadReferenceType(argumentType))
                {
                    Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    Ref<Type> referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (deduceGenericBindings(referredType, unwrapAliasType(argumentType), bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreOperatorArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                            const Ref<Type>& argumentType,
                                                            const Ref<Type>& readableArgumentType,
                                                            const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 880 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 840 : 820;
                        }

                        if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                            return 80;

                        if (isAssignmentLikeCompatible(referredType, argumentType))
                            return 75;
                    }
                }

                Ref<Type> readableParameterType = getAutoReadableType(parameterType);
                if (readableParameterType && readableArgumentType && isExactType(readableArgumentType, readableParameterType))
                    return 850;
                if (readableParameterType && readableArgumentType && isAssignmentLikeCompatible(readableParameterType, readableArgumentType))
                    return 70;

                return std::nullopt;
            };

            auto collectFreeOperatorCandidates = [&]() -> std::vector<Ref<Symbol>>
            {
                std::vector<Ref<Symbol>> collected;
                std::unordered_set<const Symbol*> seen;

                auto appendCandidate = [&](const Ref<Symbol>& symbol)
                {
                    if (!symbol || seen.contains(symbol.Get()))
                        return;

                    seen.insert(symbol.Get());
                    collected.push_back(symbol);
                };

                appendCandidate(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);

                Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front();
                auto appendAssociatedScopeCandidate = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendCandidate(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeCandidate(lhsType);
                appendAssociatedScopeCandidate(rhsType);
                appendAssociatedScopeCandidate(readableLhsType);
                appendAssociatedScopeCandidate(readableRhsType);

                return collected;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableLhsType ? readableLhsType : lhsType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        std::unordered_map<std::string, Ref<Type>> genericBindings;
                        if (!candidateSymbol->genericParameterNames.empty())
                        {
                            auto declaredFunctionType = candidateType.AsFast<FunctionType>();
                            if (!declaredFunctionType || declaredFunctionType->paramTypes.size() != 1)
                                continue;

                            if (!deduceBindingsFromOperatorArgument(
                                    declaredFunctionType->paramTypes[0],
                                    rhsType,
                                    node.right,
                                    genericBindings))
                            {
                                continue;
                            }

                            candidateType = instantiateGenericType(candidateType, genericBindings);
                        }

                        auto candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;

                        auto score = scoreOperatorArgumentAgainstParameter(
                            candidateFunctionType->paramTypes[0],
                            rhsType,
                            readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                            node.right
                        );
                        if (!score.has_value())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .ownerType = ownerType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = *score + 2
                        });
                    }
                }
            }

            for (const auto& candidateSymbol : collectFreeOperatorCandidates())
            {
                if (!candidateSymbol)
                    continue;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    continue;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromOperatorArgument(candidateFunctionType->paramTypes[0], lhsType, node.left, genericBindings) ||
                            !deduceBindingsFromOperatorArgument(candidateFunctionType->paramTypes[1], rhsType, node.right, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto lhsScore = scoreOperatorArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        lhsType,
                        readableLhsType ? readableLhsType : getAutoReadableType(lhsType),
                        node.left
                    );
                    if (!lhsScore.has_value())
                        continue;

                    auto rhsScore = scoreOperatorArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        rhsType,
                        readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                        node.right
                    );
                    if (!rhsScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *lhsScore + *rhsScore
                    });
                }
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (!bestCandidate.has_value())
                return false;

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator '{}' with operand types '{}' and '{}'.",
                    node.op.value,
                    lhsType ? lhsType->toString() : "<unknown>",
                    rhsType ? rhsType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveBinaryOperatorOverload())
            return;

        Ref<Type> lhsType = node.left->refType.Lock();
        Ref<Type> rhsType = node.right->refType.Lock();

        Ref<Type> resolvedLhsType = unwrapAliasType(lhsType);
        Ref<Type> resolvedRhsType = unwrapAliasType(rhsType);
        if (resolvedLhsType && resolvedRhsType &&
            resolvedLhsType->kind() == TypeKind::Reference &&
            resolvedRhsType->kind() == TypeKind::Reference &&
            classifyBorrowOrigin(node.right) == BorrowOrigin::Temporary)
        {
            WIO_LOG_ADD_ERROR(
                node.right->location(),
                "Cannot assign a reference borrowed from a temporary value; the borrow would outlive its owner."
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        Ref<Type> readableLhsType = getAutoReadableType(lhsType);
        Ref<Type> readableRhsType = getAutoReadableType(rhsType);

        if (!lhsType || !rhsType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        const Ref<Type> semanticLhsType = readableLhsType ? readableLhsType : lhsType;
        const Ref<Type> semanticRhsType = readableRhsType ? readableRhsType : rhsType;
        const Ref<Type> commonNumericType = getCommonNumericType(semanticLhsType, semanticRhsType);

        bool isCompatible = lhsType->isCompatibleWith(rhsType);
        if (!node.op.isAssignment() && commonNumericType)
            isCompatible = true;
        if (!isCompatible && readableLhsType && readableRhsType &&
            (shouldAutoReadReferenceType(lhsType) || shouldAutoReadReferenceType(rhsType)))
        {
            isCompatible = readableLhsType->isCompatibleWith(readableRhsType);
        }

        const bool isEqualityComparison =
            node.op.type == TokenType::opEqual || node.op.type == TokenType::opNotEqual;
        const bool isOrderingComparison =
            node.op.type == TokenType::opLess || node.op.type == TokenType::opLessEqual ||
            node.op.type == TokenType::opGreater || node.op.type == TokenType::opGreaterEqual;
        const bool isAnyComparison = isEqualityComparison || isOrderingComparison;

        if (!isCompatible && isAnyComparison)
        {
            isCompatible = rhsType->isCompatibleWith(lhsType);
            if (!isCompatible && readableLhsType && readableRhsType &&
                (shouldAutoReadReferenceType(lhsType) || shouldAutoReadReferenceType(rhsType)))
            {
                isCompatible = readableRhsType->isCompatibleWith(readableLhsType);
            }
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

        const bool requiresIntegerOperands =
            node.op.type == TokenType::opPercent ||
            node.op.type == TokenType::opBitAnd ||
            node.op.type == TokenType::opBitOr ||
            node.op.type == TokenType::opBitXor ||
            node.op.type == TokenType::opShiftLeft ||
            node.op.type == TokenType::opShiftRight;
        if (requiresIntegerOperands && commonNumericType &&
            (!isIntegralLikeType(semanticLhsType) || !isIntegralLikeType(semanticRhsType)))
        {
            WIO_LOG_ADD_ERROR(
                node.op.loc,
                "Operator '{}' requires integer operands, but got '{}' and '{}'.",
                node.op.value,
                semanticLhsType->toString(),
                semanticRhsType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (isAnyType(semanticLhsType) || isAnyType(semanticRhsType))
        {
            const bool isAssignment = node.op.type == TokenType::opAssign;
            const bool comparesWithNull =
                isEqualityComparison &&
                ((unwrapAliasType(lhsType) && unwrapAliasType(lhsType)->kind() == TypeKind::Null) ||
                 (unwrapAliasType(rhsType) && unwrapAliasType(rhsType)->kind() == TypeKind::Null));

            if (!isAssignment && !comparesWithNull)
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Any values support only assignment, the 'is' operator, the 'fit' operator, and equality comparisons with null. Operator '{}' is not supported.",
                    node.op.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        if (isOpaqueType(semanticLhsType) || isOpaqueType(semanticRhsType))
        {
            const bool isAssignment = node.op.type == TokenType::opAssign;
            if (!isAssignment && !isEqualityComparison)
            {
                WIO_LOG_ADD_ERROR(
                    node.op.loc,
                    "Opaque values support only assignment and equality comparisons. Operator '{}' is not supported.",
                    node.op.value
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }
        }

        if (isAnyComparison ||
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
            const bool producesCommonNumericType =
                node.op.type == TokenType::opPlus ||
                node.op.type == TokenType::opMinus ||
                node.op.type == TokenType::opStar ||
                node.op.type == TokenType::opSlash ||
                node.op.type == TokenType::opPercent ||
                node.op.type == TokenType::opBitAnd ||
                node.op.type == TokenType::opBitOr ||
                node.op.type == TokenType::opBitXor;
            node.refType = producesCommonNumericType && commonNumericType
                ? commonNumericType
                : semanticLhsType;
        }
    }

    void SemanticAnalyzer::visit(ConditionalExpression& node)
    {
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;

        currentExpectedExpressionType_ = Compiler::get().getTypeContext().getBool();
        allowContextualNumericLiteralTyping_ = false;
        node.condition->accept(*this);

        Ref<Type> conditionType = getAutoReadableType(node.condition->refType.Lock());
        if (conditionType &&
            !conditionType->isUnknown() &&
            conditionType != Compiler::get().getTypeContext().getBool())
        {
            WIO_LOG_ADD_ERROR(
                node.condition->location(),
                "Conditional operator condition must be bool, but got '{}'.",
                conditionType->toString()
            );
        }

        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = true;
        node.whenTrue->accept(*this);
        node.whenFalse->accept(*this);

        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

        Ref<Type> trueType = getAutoReadableType(node.whenTrue->refType.Lock());
        Ref<Type> falseType = getAutoReadableType(node.whenFalse->refType.Lock());

        if (!trueType || trueType->isUnknown())
        {
            node.refType = falseType ? falseType : Compiler::get().getTypeContext().getUnknown();
            return;
        }
        if (!falseType || falseType->isUnknown())
        {
            node.refType = trueType;
            return;
        }

        if (!isAssignmentLikeCompatible(trueType, falseType) &&
            !isAssignmentLikeCompatible(falseType, trueType))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Conditional operator branches must have compatible types. Got '{}' and '{}'.",
                trueType->toString(),
                falseType->toString()
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (previousExpectedExpressionType &&
            !previousExpectedExpressionType->isUnknown() &&
            isAssignmentLikeCompatible(previousExpectedExpressionType, trueType) &&
            isAssignmentLikeCompatible(previousExpectedExpressionType, falseType))
        {
            node.refType = previousExpectedExpressionType;
        }
        else if (isAssignmentLikeCompatible(trueType, falseType))
        {
            node.refType = trueType;
        }
        else
        {
            node.refType = falseType;
        }
    }

    void SemanticAnalyzer::visit(TypeExpression& node)
    {
        if (!node.type)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.type->accept(*this);
        node.refType = node.type->refType.Lock();

        if (node.type->name.type == TokenType::identifier)
            node.referencedSymbol = resolveQualifiedSymbol(currentScope_, node.type->name.value);
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

        auto tryResolveUnaryOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getUnaryOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> readableOperandType = getAutoReadableType(opType);
            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto scoreUnaryArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                          const Ref<Type>& argumentType,
                                                          const Ref<Type>& readableArgumentType,
                                                          const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 880 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 840 : 820;
                        }
                    }
                }

                return std::nullopt;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableOperandType ? readableOperandType : opType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        auto candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = 1002
                        });
                    }
                }
            }

            std::unordered_set<const Symbol*> seenFreeSymbols;
            auto appendFreeSymbol = [&](const Ref<Symbol>& symbol)
            {
                if (!symbol || seenFreeSymbols.contains(symbol.Get()))
                    return;
                seenFreeSymbols.insert(symbol.Get());

                std::vector<Ref<Symbol>> overloads;
                if (symbol->kind == SymbolKind::FunctionGroup)
                    overloads = symbol->overloads;
                else if (symbol->kind == SymbolKind::Function)
                    overloads.push_back(symbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceGenericBindings(candidateFunctionType->paramTypes[0], opType, genericBindings))
                        {
                            if (!(readableOperandType && deduceGenericBindings(candidateFunctionType->paramTypes[0], readableOperandType, genericBindings)))
                                continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto score = scoreUnaryArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        opType,
                        readableOperandType ? readableOperandType : getAutoReadableType(opType),
                        node.operand
                    );
                    if (!score.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *score
                    });
                }
            };

            appendFreeSymbol(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);
            if (Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front())
            {
                auto appendAssociatedScopeSymbol = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendFreeSymbol(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeSymbol(opType);
                appendAssociatedScopeSymbol(readableOperandType);
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for unary operator '{}' with operand type '{}'.",
                    node.op.value,
                    opType ? opType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (!bestCandidate.has_value())
                return false;

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveUnaryOperatorOverload())
            return;

        if (node.op.type == TokenType::kwDeref)
        {
            Ref<Type> resolvedType = unwrapAliasType(opType);
            if (!resolvedType || resolvedType->kind() != TypeKind::Reference)
            {
                WIO_LOG_ADD_ERROR(node.location(), "The 'deref' operator requires a readable reference.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            node.refType = resolvedType.AsFast<ReferenceType>()->referredType;
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
            if (!allowsNumericSemantics(opType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Unary minus (-) operator requires numeric operand.");
            }
            node.refType = opType;
        }
        else if (node.op.type == TokenType::opBitNot)
        {
            if (!allowsIntegerSemantics(opType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Bitwise NOT (~) operator requires integer operand.");
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
        Ref<Symbol> directlyAssignedSymbol = node.left->is<Identifier>()
            ? node.left->referencedSymbol.Lock()
            : nullptr;
        Ref<Type> declaredAssignmentType = directlyAssignedSymbol
            ? directlyAssignedSymbol->type
            : node.left->refType.Lock();
        Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
        bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
        currentExpectedExpressionType_ = getAutoReadableType(declaredAssignmentType);
        allowContextualNumericLiteralTyping_ = true;
        node.right->accept(*this);
        currentExpectedExpressionType_ = previousExpectedExpressionType;
        allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

        Ref<Type> lhsType = declaredAssignmentType;
        Ref<Type> rhsType = node.right->refType.Lock();
        if (directlyAssignedSymbol)
            nonNullNarrowedSymbols_.erase(directlyAssignedSymbol.Get());

        auto emitWriteabilityDiagnosticsForSymbol = [&](const Ref<Symbol>& referSym)
        {
            if (!referSym)
                return;

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
        };

        auto tryResolveAssignmentOperatorOverload = [&]() -> bool
        {
            auto overloadName = common::getAssignmentOperatorOverloadName(node.op.type);
            if (!overloadName.has_value())
                return false;

            Ref<Type> readableLhsType = getAutoReadableType(lhsType);
            Ref<Type> readableRhsType = getAutoReadableType(rhsType);

            struct OperatorCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<FunctionType> functionType = nullptr;
                Ref<Type> ownerType = nullptr;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = -1;
            };

            auto deduceBindingsFromAssignmentArgument = [&](const Ref<Type>& expectedType,
                                                           const Ref<Type>& argumentType,
                                                           const NodePtr<Expression>& argumentExpression,
                                                           std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                if (shouldAutoReadReferenceType(argumentType))
                {
                    Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (deduceGenericBindings(referredType, unwrapAliasType(argumentType), bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreAssignmentArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                               const Ref<Type>& argumentType,
                                                               const Ref<Type>& readableArgumentType,
                                                               const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !argumentType)
                    return std::nullopt;

                if (isExactType(argumentType, parameterType))
                    return 1000;

                if (isAssignmentLikeCompatible(parameterType, argumentType))
                    return 100;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    auto referredType = referenceType ? referenceType->referredType : nullptr;
                    if (referredType)
                    {
                        if (isExactType(readableArgumentType, referredType) || isExactType(argumentType, referredType))
                            return 900;

                        if (argumentExpression &&
                            isAddressableRefOperand(argumentExpression) &&
                            (!referenceType->isMutable || isMutableAddressableOperand(argumentExpression)))
                        {
                            if (isExactType(argumentType, referredType))
                                return 950;

                            if (isAssignmentLikeCompatible(referredType, argumentType))
                                return referenceType->isMutable ? 900 : 860;

                            if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                                return referenceType->isMutable ? 860 : 820;
                        }

                        if (readableArgumentType && isAssignmentLikeCompatible(referredType, readableArgumentType))
                            return 80;
                    }
                }

                return std::nullopt;
            };

            auto collectFreeOperatorCandidates = [&]() -> std::vector<Ref<Symbol>>
            {
                std::vector<Ref<Symbol>> collected;
                std::unordered_set<const Symbol*> seen;

                auto appendCandidate = [&](const Ref<Symbol>& symbol)
                {
                    if (!symbol || seen.contains(symbol.Get()))
                        return;

                    seen.insert(symbol.Get());
                    collected.push_back(symbol);
                };

                appendCandidate(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);

                Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front();
                auto appendAssociatedScopeCandidate = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendCandidate(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeCandidate(lhsType);
                appendAssociatedScopeCandidate(rhsType);
                appendAssociatedScopeCandidate(readableLhsType);
                appendAssociatedScopeCandidate(readableRhsType);

                return collected;
            };

            std::vector<OperatorCandidate> candidates;

            Ref<Type> receiverType = unwrapAliasType(readableLhsType ? readableLhsType : lhsType);
            if (receiverType && receiverType->kind() == TypeKind::Struct)
            {
                Ref<Type> ownerType = nullptr;
                if (Ref<Symbol> memberOperatorSymbol = findStructMemberInHierarchy(receiverType, std::string(*overloadName), &ownerType))
                {
                    if (!validateStructMemberAccess(currentStructType_, ownerType, memberOperatorSymbol, node.location()))
                    {
                        node.refType = Compiler::get().getTypeContext().getUnknown();
                        return true;
                    }

                    std::vector<Ref<Symbol>> memberSymbols;
                    if (memberOperatorSymbol->kind == SymbolKind::FunctionGroup)
                        memberSymbols = memberOperatorSymbol->overloads;
                    else if (memberOperatorSymbol->kind == SymbolKind::Function)
                        memberSymbols.push_back(memberOperatorSymbol);

                    for (const auto& candidateSymbol : memberSymbols)
                    {
                        if (!candidateSymbol || !candidateSymbol->type || candidateSymbol->type->kind() != TypeKind::Function)
                            continue;

                        Ref<Type> candidateType = candidateSymbol->type;
                        if (auto instantiatedOwnerType = ownerType ? ownerType.AsFast<StructType>() : nullptr;
                            instantiatedOwnerType && !instantiatedOwnerType->genericParameterNames.empty() && !instantiatedOwnerType->genericArguments.empty())
                        {
                            auto ownerBindings = buildExtendedGenericBindings(
                                instantiatedOwnerType->genericParameterNames,
                                instantiatedOwnerType->hasGenericParameterPack,
                                instantiatedOwnerType->genericArguments
                            );
                            candidateType = instantiateGenericType(candidateType, ownerBindings);
                        }

                        std::unordered_map<std::string, Ref<Type>> genericBindings;
                        auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;

                        if (!candidateSymbol->genericParameterNames.empty())
                        {
                            if (!deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[0], rhsType, node.right, genericBindings))
                                continue;

                            candidateType = instantiateGenericType(candidateType, genericBindings);
                            candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                            if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                                continue;
                        }

                        auto score = scoreAssignmentArgumentAgainstParameter(
                            candidateFunctionType->paramTypes[0],
                            rhsType,
                            readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                            node.right
                        );
                        if (!score.has_value())
                            continue;

                        candidates.push_back(OperatorCandidate{
                            .symbol = candidateSymbol,
                            .functionType = candidateFunctionType,
                            .ownerType = ownerType,
                            .dispatchKind = OperatorDispatchKind::Member,
                            .score = *score + 2
                        });
                    }
                }
            }

            for (const auto& candidateSymbol : collectFreeOperatorCandidates())
            {
                if (!candidateSymbol)
                    continue;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    continue;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[0], lhsType, node.left, genericBindings) ||
                            !deduceBindingsFromAssignmentArgument(candidateFunctionType->paramTypes[1], rhsType, node.right, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto lhsScore = scoreAssignmentArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        lhsType,
                        readableLhsType ? readableLhsType : getAutoReadableType(lhsType),
                        node.left
                    );
                    if (!lhsScore.has_value())
                        continue;

                    auto rhsScore = scoreAssignmentArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        rhsType,
                        readableRhsType ? readableRhsType : getAutoReadableType(rhsType),
                        node.right
                    );
                    if (!rhsScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *lhsScore + *rhsScore
                    });
                }
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (!bestCandidate.has_value())
                return false;

            if (!isAddressableRefOperand(node.left))
            {
                WIO_LOG_ADD_ERROR(node.op.loc, "Operator-assignment requires an assignable left operand.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
                emitWriteabilityDiagnosticsForSymbol(referSym);

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator '{}' with operand types '{}' and '{}'.",
                    node.op.value,
                    lhsType ? lhsType->toString() : "<unknown>",
                    rhsType ? rhsType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType ? bestCandidate->functionType->returnType : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveAssignmentOperatorOverload())
            return;

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
            if (isRejectedImplicitNumericConversion(lhsType, rhsType))
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Implicit narrowing conversion from '{}' to '{}' requires explicit 'fit'.",
                    rhsType->toString(), lhsType->toString());
            }
            else
            {
                WIO_LOG_ADD_ERROR(node.op.loc,
                    "Type mismatch in assignment: Cannot assign '{}' to '{}'.",
                    rhsType->toString(), lhsType->toString());
            }
        }

        if (auto* arrayAccess = node.left->as<ArrayAccessExpression>())
        {
            Ref<Type> receiverType = unwrapAliasType(arrayAccess->object ? arrayAccess->object->refType.Lock() : nullptr);
            if (receiverType)
            {
                if (receiverType->kind() == TypeKind::GenericParameterPack ||
                    receiverType->kind() == TypeKind::ValuePackView ||
                    receiverType->kind() == TypeKind::TypePackView)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Cannot assign through raw pack values or pack views. Assign to a mutable pack storage field instead."
                    );
                }
                else if (receiverType->kind() == TypeKind::PackStorage)
                {
                    emitWriteabilityDiagnosticsForSymbol(arrayAccess->object ? arrayAccess->object->referencedSymbol.Lock() : nullptr);
                }
            }

            if (arrayAccess->operatorDispatchKind != OperatorDispatchKind::None)
            {
                Ref<Type> indexedType = unwrapAliasType(arrayAccess->refType.Lock());
                if (!indexedType || indexedType->kind() != TypeKind::Reference)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Subscript assignment requires operator '[]' to return a mutable reference."
                    );
                }
                else if (!indexedType.AsFast<ReferenceType>()->isMutable)
                {
                    WIO_LOG_ADD_ERROR(
                        node.op.loc,
                        "Cannot assign through a read-only subscript result."
                    );
                }
            }
        }

        if (!isAutoDeref)
        {
            if (auto referSym = node.left->referencedSymbol.Lock(); referSym)
                emitWriteabilityDiagnosticsForSymbol(referSym);
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
            if (node.token.isType())
            {
                if (Ref<Type> builtinType = resolvePrimitiveType(node.token.value))
                {
                    node.refType = builtinType;
                    return;
                }
            }

            if (allowTypePackIdentifierReference_)
            {
                for (auto& genericTypeParameterScope : std::ranges::reverse_view(genericTypeParameterScopes_))
                {
                    if (auto genericTypeIt = genericTypeParameterScope.find(node.token.value); genericTypeIt != genericTypeParameterScope.end())
                    {
                        Ref<Type> genericType = genericTypeIt->second;
                        if (genericType && genericType->kind() == TypeKind::GenericParameterPack)
                        {
                            node.refType = genericType;
                            return;
                        }
                    }
                }
            }

            WIO_LOG_ADD_ERROR(node.location(), "Undefined symbol: '{}'", node.token.value);
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = sym;
        if (nonNullNarrowedSymbols_.contains(sym.Get()))
        {
            Ref<Type> narrowedType = unwrapAliasType(sym->type);
            node.refType = narrowedType && narrowedType->kind() == TypeKind::Nullable
                ? narrowedType.AsFast<NullableType>()->valueType
                : sym->type;
        }
        else
        {
            node.refType = sym->type;
        }
        if (sym->flags.get_isGlobal())
            node.borrowOrigin = BorrowOrigin::Static;
        else if (sym->kind == SymbolKind::Parameter && sym->type && sym->type->kind() == TypeKind::Reference)
            node.borrowOrigin = BorrowOrigin::Caller;
        else if (sym->kind == SymbolKind::Variable || sym->kind == SymbolKind::Parameter)
            node.borrowOrigin = BorrowOrigin::Local;

        if (sym->flags.get_isParameterPack() && !allowParameterPackIdentifierReference_)
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Parameter pack '{}' must be expanded as '{}...' inside a function call.",
                node.token.value,
                node.token.value
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
        }
    }

    void SemanticAnalyzer::visit(PackExpansionExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        node.operand->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;

        auto referencedSymbol = node.operand ? node.operand->referencedSymbol.Lock() : nullptr;
        if (!referencedSymbol || !referencedSymbol->flags.get_isParameterPack())
        {
            WIO_LOG_ADD_ERROR(node.location(), "Pack expansion expressions currently support only function parameter packs.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        node.referencedSymbol = referencedSymbol;
        node.refType = referencedSymbol->type;
    }

    void SemanticAnalyzer::visit(NullExpression& node)
    {
        auto& typeContext = Compiler::get().getTypeContext();
        Ref<Type> transformedType = currentExpectedExpressionType_
            ? getAutoReadableType(currentExpectedExpressionType_)
            : nullptr;

        if (!transformedType || transformedType->isUnknown())
            transformedType = typeContext.getVoid();

        node.refType = typeContext.getOrCreateNullType(transformedType);
    }

    void SemanticAnalyzer::visit(ArrayAccessExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        const bool previousAllowTypePackIdentifierReference = allowTypePackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        allowTypePackIdentifierReference_ = true;
        node.object->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;
        allowTypePackIdentifierReference_ = previousAllowTypePackIdentifierReference;
        Ref<Type> objType = node.object->refType.Lock();
        Ref<Type> resolvedObjType = getAutoReadableType(objType);
        resolvedObjType = unwrapAliasType(resolvedObjType);

        node.index->accept(*this);
        Ref<Type> idxType = getAutoReadableType(node.index->refType.Lock());

        if (!resolvedObjType)
        {
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto tryResolveIndexOperatorOverload = [&]() -> bool
        {
            const auto overloadName = common::getIndexOperatorOverloadName(TokenType::leftBracket);
            if (!overloadName.has_value())
                return false;

            struct OperatorCandidate
            {
                Ref<Symbol> symbol;
                Ref<FunctionType> functionType;
                Ref<StructType> ownerType;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = 0;
            };

            auto deduceBindingsFromIndexArgument = [&](const Ref<Type>& expectedType,
                                                       const Ref<Type>& argumentType,
                                                       const NodePtr<Expression>& argumentExpression,
                                                       std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                if (argumentExpression)
                    readableArgumentType = getAutoReadableType(argumentType);

                if (argumentType != readableArgumentType)
                {
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto expectedReferenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    if (expectedReferenceType)
                    {
                        Ref<Type> referredType = expectedReferenceType->referredType;
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression && isAddressableRefOperand(argumentExpression))
                        {
                            Ref<Type> resolvedArgumentType = unwrapAliasType(argumentType);
                            if (resolvedArgumentType && resolvedArgumentType->kind() == TypeKind::Reference)
                                resolvedArgumentType = resolvedArgumentType.AsFast<ReferenceType>()->referredType;

                            if (resolvedArgumentType && deduceGenericBindings(referredType, resolvedArgumentType, bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreIndexArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                          const Ref<Type>& rawArgumentType,
                                                          const Ref<Type>& readableArgumentType,
                                                          const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !rawArgumentType)
                    return std::nullopt;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    if (argumentExpression && isAddressableRefOperand(argumentExpression))
                    {
                        Ref<Type> addressableArgumentType = rawArgumentType;
                        Ref<Type> resolvedAddressableType = unwrapAliasType(addressableArgumentType);
                        if (resolvedAddressableType && resolvedAddressableType->kind() == TypeKind::Reference)
                            addressableArgumentType = resolvedAddressableType.AsFast<ReferenceType>()->referredType;

                        if (isAssignmentLikeCompatible(parameterType, addressableArgumentType))
                            return referenceType->isMutable ? 4 : 3;
                    }

                    if (readableArgumentType && isAssignmentLikeCompatible(referenceType->referredType, readableArgumentType))
                        return referenceType->isMutable ? 2 : 1;

                    return std::nullopt;
                }

                if (isAssignmentLikeCompatible(parameterType, rawArgumentType))
                    return 4;
                if (readableArgumentType && isAssignmentLikeCompatible(parameterType, readableArgumentType))
                    return 2;
                return std::nullopt;
            };

            std::vector<OperatorCandidate> candidates;

            auto appendMemberCandidates = [&](const Ref<Type>& candidateReceiverType)
            {
                Ref<Type> currentType = unwrapAliasType(candidateReceiverType);
                while (currentType && currentType->kind() == TypeKind::Reference)
                    currentType = unwrapAliasType(currentType.AsFast<ReferenceType>()->referredType);

                if (!currentType || currentType->kind() != TypeKind::Struct)
                    return;

                Ref<Type> ownerType = nullptr;
                Ref<Symbol> candidateSymbol = findStructMemberInHierarchy(currentType, std::string(*overloadName), &ownerType);
                if (!candidateSymbol)
                    return;

                if (!validateStructMemberAccess(currentStructType_, ownerType, candidateSymbol, node.location()))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                auto ownerStructType = ownerType ? unwrapAliasType(ownerType).AsFast<StructType>() : nullptr;
                if (!ownerStructType)
                    return;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[0], idxType, node.index, genericBindings))
                            continue;

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto score = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        idxType,
                        getAutoReadableType(idxType),
                        node.index
                    );
                    if (!score.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = ownerStructType,
                        .dispatchKind = OperatorDispatchKind::Member,
                        .score = *score + 2
                    });
                }
            };

            appendMemberCandidates(objType);
            if (resolvedObjType != objType)
                appendMemberCandidates(resolvedObjType);

            std::unordered_set<const Symbol*> seenFreeSymbols;
            auto appendFreeSymbol = [&](const Ref<Symbol>& candidateSymbol)
            {
                if (!candidateSymbol || seenFreeSymbols.contains(candidateSymbol.Get()))
                    return;

                seenFreeSymbols.insert(candidateSymbol.Get());

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[0], objType, node.object, genericBindings) ||
                            !deduceBindingsFromIndexArgument(candidateFunctionType->paramTypes[1], idxType, node.index, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 2)
                            continue;
                    }

                    auto objectScore = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        objType,
                        getAutoReadableType(objType),
                        node.object
                    );
                    if (!objectScore.has_value())
                        continue;

                    auto indexScore = scoreIndexArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[1],
                        idxType,
                        getAutoReadableType(idxType),
                        node.index
                    );
                    if (!indexScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *objectScore + *indexScore
                    });
                }
            };

            appendFreeSymbol(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);
            if (Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front())
            {
                auto appendAssociatedScopeSymbol = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendFreeSymbol(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeSymbol(objType);
                appendAssociatedScopeSymbol(resolvedObjType);
                appendAssociatedScopeSymbol(idxType);
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (!bestCandidate.has_value())
                return false;

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator '[]' with operand types '{}' and '{}'.",
                    objType ? objType->toString() : "<unknown>",
                    idxType ? idxType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType
                ? bestCandidate->functionType->returnType
                : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveIndexOperatorOverload())
            return;

        const auto resolvePackElementType = [&](const std::string& packName,
                                                const std::vector<Ref<Type>>& elementTypes) -> Ref<Type>
        {
            auto indexBinding = tryEvaluatePackIndexBinding(node.index, variableDeclarationsBySymbol_, packName);
            if (!indexBinding.has_value())
            {
                WIO_LOG_ADD_ERROR(node.index->location(), "Pack indexing requires a non-negative compile-time integer index.");
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (indexBinding->kind == PackElementBindingKind::FromEnd && indexBinding->value == 0)
            {
                WIO_LOG_ADD_ERROR(node.index->location(), "Pack indexing from '.size' must subtract at least 1.");
                return Compiler::get().getTypeContext().getUnknown();
            }

            if (!elementTypes.empty())
            {
                if (elementTypes.size() == 1)
                {
                    if (auto symbolicPackName = tryGetSymbolicPackReferenceName(elementTypes.front()))
                    {
                        ParsedPackElementBinding reboundBinding = *indexBinding;
                        reboundBinding.packName = *symbolicPackName;
                        return makeSyntheticPackElementType(reboundBinding);
                    }
                }

                if (auto resolvedIndex = tryResolveConcretePackElementIndex(*indexBinding, elementTypes.size()))
                    return elementTypes[*resolvedIndex];

                WIO_LOG_ADD_ERROR(
                    node.index->location(),
                    "Pack index is out of range for size {}.",
                    elementTypes.size()
                );
                return Compiler::get().getTypeContext().getUnknown();
            }

            ParsedPackElementBinding reboundBinding = *indexBinding;
            reboundBinding.packName = packName;
            return makeSyntheticPackElementType(reboundBinding);
        };

        if (resolvedObjType->kind() == TypeKind::GenericParameterPack)
        {
            node.refType = resolvePackElementType(
                resolvedObjType.AsFast<GenericParameterPackType>()->name,
                {}
            );
            return;
        }

        if (resolvedObjType->kind() == TypeKind::ValuePackView)
        {
            auto viewType = resolvedObjType.AsFast<ValuePackViewType>();
            node.refType = resolvePackElementType(viewType->packName, viewType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() == TypeKind::PackStorage)
        {
            auto storageType = resolvedObjType.AsFast<PackStorageType>();
            node.refType = resolvePackElementType(storageType->packName, storageType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() == TypeKind::TypePackView)
        {
            auto viewType = resolvedObjType.AsFast<TypePackViewType>();
            node.refType = resolvePackElementType(viewType->packName, viewType->elementTypes);
            return;
        }

        if (resolvedObjType->kind() != TypeKind::Array && !isStringType(resolvedObjType))
        {
            WIO_LOG_ADD_ERROR(node.object->location(), "Type '{}' is not an array or string and cannot be indexed.", objType->toString());
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        if (!allowsIntegerSemantics(idxType))
        {
            WIO_LOG_ADD_ERROR(node.index->location(), "Array and string indices must be integer values.");
        }
        
        if (resolvedObjType->kind() == TypeKind::Array)
        {
            auto arrType = resolvedObjType.AsFast<ArrayType>();

            if (auto staticIndex = tryEvaluateStaticPackIndex(node.index, variableDeclarationsBySymbol_);
                staticIndex.has_value() &&
                (arrType->arrayKind == ArrayType::ArrayKind::Static || arrType->arrayKind == ArrayType::ArrayKind::Literal) &&
                *staticIndex >= arrType->size)
            {
                WIO_LOG_ADD_ERROR(
                    node.index->location(),
                    "Index {} is out of range for compile-time array size {}.",
                    *staticIndex,
                    arrType->size
                );
            }

            node.refType = arrType->elementType;
            node.borrowOrigin = classifyBorrowOrigin(node.object);
            return;
        }

        node.refType = Compiler::get().getTypeContext().getChar();
    }
    
    void SemanticAnalyzer::visit(MemberAccessExpression& node)
    {
        const bool previousAllowParameterPackIdentifierReference = allowParameterPackIdentifierReference_;
        const bool previousAllowTypePackIdentifierReference = allowTypePackIdentifierReference_;
        allowParameterPackIdentifierReference_ = true;
        allowTypePackIdentifierReference_ = true;
        node.object->accept(*this);
        allowParameterPackIdentifierReference_ = previousAllowParameterPackIdentifierReference;
        allowTypePackIdentifierReference_ = previousAllowTypePackIdentifierReference;
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

        Ref<Type> resolvedLeftType = unwrapAliasType(leftType);
        if (resolvedLeftType && resolvedLeftType->kind() == TypeKind::Nullable)
        {
            WIO_LOG_ADD_ERROR(
                node.object->location(),
                "Member access requires a non-null value, but '{}' is nullable. Check it against null first.",
                leftType ? leftType->toString() : "<unknown>"
            );
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        if (resolvedLeftType &&
            (resolvedLeftType->kind() == TypeKind::GenericParameterPack ||
             resolvedLeftType->kind() == TypeKind::ValuePackView ||
             resolvedLeftType->kind() == TypeKind::TypePackView ||
             resolvedLeftType->kind() == TypeKind::PackStorage))
        {
            auto& typeContext = Compiler::get().getTypeContext();
            const auto memberName = node.member->token.value;

            auto resolvePackName = [&]() -> std::string
            {
                auto normalizePackName = [&](const std::string& fallbackName, const std::vector<Ref<Type>>& elementTypes) -> std::string
                {
                    if (elementTypes.size() == 1)
                    {
                        if (auto symbolicPackName = tryGetSymbolicPackReferenceName(elementTypes.front()))
                            return *symbolicPackName;
                    }
                    return fallbackName;
                };

                switch (resolvedLeftType->kind())
                {
                case TypeKind::GenericParameterPack:
                    return resolvedLeftType.AsFast<GenericParameterPackType>()->name;
                case TypeKind::ValuePackView:
                {
                    auto viewType = resolvedLeftType.AsFast<ValuePackViewType>();
                    return normalizePackName(viewType->packName, viewType->elementTypes);
                }
                case TypeKind::TypePackView:
                {
                    auto viewType = resolvedLeftType.AsFast<TypePackViewType>();
                    return normalizePackName(viewType->packName, viewType->elementTypes);
                }
                case TypeKind::PackStorage:
                {
                    auto storageType = resolvedLeftType.AsFast<PackStorageType>();
                    return normalizePackName(storageType->packName, storageType->elementTypes);
                }
                default:
                    return {};
                }
            };

            auto resolvePackElements = [&]() -> std::vector<Ref<Type>>
            {
                auto normalizePackElements = [](const std::vector<Ref<Type>>& elementTypes) -> std::vector<Ref<Type>>
                {
                    if (elementTypes.size() == 1 && tryGetSymbolicPackReferenceName(elementTypes.front()).has_value())
                        return {};
                    return elementTypes;
                };

                switch (resolvedLeftType->kind())
                {
                case TypeKind::ValuePackView:
                    return normalizePackElements(resolvedLeftType.AsFast<ValuePackViewType>()->elementTypes);
                case TypeKind::TypePackView:
                    return normalizePackElements(resolvedLeftType.AsFast<TypePackViewType>()->elementTypes);
                case TypeKind::PackStorage:
                    return normalizePackElements(resolvedLeftType.AsFast<PackStorageType>()->elementTypes);
                default:
                    return {};
                }
            };

            if (memberName == "size")
            {
                node.intrinsicMember = IntrinsicMember::PackSize;
                node.refType = typeContext.getUSize();
                node.member->refType = node.refType.Lock();
                return;
            }

            if (memberName == "array")
            {
                node.intrinsicMember = IntrinsicMember::PackArray;
                const std::string packName = resolvePackName();
                const auto packElements = resolvePackElements();
                const bool isValuePackReference =
                    resolvedLeftType->kind() == TypeKind::ValuePackView ||
                    resolvedLeftType->kind() == TypeKind::PackStorage ||
                    (resolvedLeftType->kind() == TypeKind::GenericParameterPack &&
                     leftSymbol &&
                     (leftSymbol->kind == SymbolKind::Variable || leftSymbol->kind == SymbolKind::Parameter));

                if (isValuePackReference)
                {
                    node.refType = typeContext.getOrCreateValuePackViewType(packName, packElements);
                }
                else
                {
                    node.refType = typeContext.getOrCreateTypePackViewType(packName, packElements);
                }
                node.member->refType = node.refType.Lock();
                return;
            }

            if (memberName == "ToStaticArray")
            {
                node.intrinsicMember = IntrinsicMember::PackToStaticArray;
                node.refType = typeContext.getOrCreateFunctionType(typeContext.getUnknown(), {});
                node.member->refType = node.refType.Lock();
                return;
            }
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
                    auto resolution = overloads.front();
                    if (node.member->token.value == "Get" &&
                        (resolution.member == IntrinsicMember::ArrayGet ||
                         resolution.member == IntrinsicMember::DictGet ||
                         resolution.member == IntrinsicMember::StringGet))
                    {
                        Ref<Type> resolvedCandidate = unwrapAliasType(candidateType);
                        Ref<Type> optionPayloadType = nullptr;
                        if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Array)
                            optionPayloadType = resolvedCandidate.AsFast<ArrayType>()->elementType;
                        else if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Dictionary)
                            optionPayloadType = resolvedCandidate.AsFast<DictionaryType>()->valueType;
                        else if (resolvedCandidate && resolvedCandidate->kind() == TypeKind::Primitive &&
                                 resolvedCandidate.AsFast<PrimitiveType>()->name == "string")
                            optionPayloadType = Compiler::get().getTypeContext().getChar();
                        Ref<Symbol> optionSymbol = resolveQualifiedSymbol(currentScope_, "std::Option");
                        auto optionStruct = optionSymbol && optionSymbol->type && optionSymbol->type->kind() == TypeKind::Struct
                            ? optionSymbol->type.AsFast<StructType>()
                            : nullptr;
                        if (!optionPayloadType || !optionStruct)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.member->location(),
                                "Container Get requires the built-in std::Option<T> module."
                            );
                            node.refType = Compiler::get().getTypeContext().getUnknown();
                            return true;
                        }

                        Ref<Type> optionType = instantiateGenericStructType(
                            optionStruct,
                            { optionPayloadType },
                            node.location()
                        );
                        resolution.memberType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            optionType,
                            resolution.memberType.AsFast<FunctionType>()->paramTypes
                        );
                    }
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
                if (!foundMember && resolveIntrinsicMemberOnType(actualStructType))
                    return;
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
                    if (!foundMember && resolveIntrinsicMemberOnType(actualStructType))
                        return;
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
    
        if (!foundMember && actualStructType)
        {
            auto typeMethods = extensionMethods_.find(actualStructType.Get());
            if (typeMethods != extensionMethods_.end())
            {
                auto method = typeMethods->second.find(node.member->token.value);
                if (method != typeMethods->second.end())
                {
                    Ref<Symbol> extensionSymbol = method->second;
                    auto fullType = extensionSymbol->type.AsFast<FunctionType>();
                    if (fullType && !fullType->paramTypes.empty())
                    {
                        std::vector<Ref<Type>> visibleParameters(
                            fullType->paramTypes.begin() + 1, fullType->paramTypes.end());
                        Ref<Type> visibleType = Compiler::get().getTypeContext().getOrCreateFunctionType(
                            fullType->returnType, visibleParameters, fullType->hasParameterPack);

                        auto receiverType = fullType->paramTypes.front();
                        auto receiverReference = receiverType && receiverType->kind() == TypeKind::Reference
                            ? receiverType.AsFast<ReferenceType>()
                            : nullptr;
                        if (receiverReference && receiverReference->isMutable &&
                            !canMutateIntrinsicReceiver(node.object))
                        {
                            WIO_LOG_ADD_ERROR(node.location(),
                                "Extension method '{}' requires a mutable receiver.",
                                node.member->token.value);
                        }

                        Ref<Symbol> callableSymbol = createSymbol(
                            extensionSymbol->name, visibleType, SymbolKind::Function,
                            extensionSymbol->definitionLoc);
                        callableSymbol->scopePath = extensionSymbol->scopePath;
                        callableSymbol->flags.set_isExtension(true);
                        callableSymbol->extensionTargetType = extensionSymbol->extensionTargetType;
                        callableSymbol->extensionMemberName = extensionSymbol->extensionMemberName;
                        callableSymbol->extensionImplementation = extensionSymbol;

                        node.referencedSymbol = callableSymbol;
                        node.refType = visibleType;
                        node.member->referencedSymbol = callableSymbol;
                        node.member->refType = visibleType;
                        return;
                    }
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
            auto bindings = buildExtendedGenericBindings(
                instantiatedStructType->genericParameterNames,
                instantiatedStructType->hasGenericParameterPack,
                instantiatedStructType->genericArguments
            );
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

            if (!matchesApplyConstraints(
                    *attributeIt->second,
                    symbol->genericParameterNames,
                    symbol->hasGenericParameterPack,
                    explicitTypeArguments))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic {} '{}' rejects type arguments {} because of @Apply constraints.",
                    declarationKind,
                    symbol->name,
                    formatConcreteInstantiationSignature(explicitTypeArguments)
                );
                return false;
            }

            return true;
        };

        auto finalizeCallResultType = [&](const Ref<Type>& resultType) -> bool
        {
            if (!node.unwrapResult && !node.propagateResult)
            {
                node.refType = resultType;
                Ref<Type> resolvedResultType = unwrapAliasType(resultType);
                if (resolvedResultType && resolvedResultType->kind() == TypeKind::Reference)
                {
                    if (const auto* memberAccess = node.callee->as<MemberAccessExpression>())
                    {
                        node.borrowOrigin = classifyBorrowOrigin(memberAccess->object);
                    }
                    else if (node.operatorDispatchKind == OperatorDispatchKind::Member)
                    {
                        node.borrowOrigin = classifyBorrowOrigin(node.callee);
                    }
                    else
                    {
                        Ref<Type> callableType = node.callee ? unwrapAliasType(node.callee->refType.Lock()) : nullptr;
                        auto functionType = callableType ? callableType.AsFast<FunctionType>() : nullptr;
                        node.borrowOrigin = BorrowOrigin::Static;
                        if (functionType)
                        {
                            const size_t argumentCount = std::min(node.arguments.size(), functionType->paramTypes.size());
                            for (size_t i = 0; i < argumentCount; ++i)
                            {
                                Ref<Type> parameterType = unwrapAliasType(functionType->paramTypes[i]);
                                if (parameterType && parameterType->kind() == TypeKind::Reference)
                                {
                                    node.borrowOrigin = classifyBorrowOrigin(node.arguments[i]);
                                    break;
                                }
                            }
                        }
                    }
                }
                return true;
            }

            if (auto payloadType = tryGetResultPayloadType(resultType))
            {
                if (node.propagateResult && !tryGetResultPayloadType(currentFunctionReturnType_))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "The '?()' propagation syntax requires the enclosing function to return std::Result<T>."
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return false;
                }

                node.refType = *payloadType;
                return true;
            }

            if (node.propagateResult)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The '?()' propagation syntax requires the called function to return std::Result<T>."
                );
            }
            else
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The '!()' unwrap syntax requires the called function to return std::Result<T>."
                );
            }
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return false;
        };

        node.callee->accept(*this);
        Ref<Symbol> calleeSym = node.callee->referencedSymbol.Lock();
        Ref<Symbol> genericOwnerSym = calleeSym;
        node.operatorDispatchKind = OperatorDispatchKind::None;
        node.overloadFunctionType = nullptr;
        std::vector<Ref<Type>> explicitTypeArguments;
        explicitTypeArguments.reserve(node.explicitTypeArguments.size());
        for (auto& explicitTypeArgument : node.explicitTypeArguments)
        {
            explicitTypeArgument->accept(*this);
            explicitTypeArguments.push_back(explicitTypeArgument->refType.Lock());
        }

        if (auto* memberAccess = node.callee->as<MemberAccessExpression>();
            memberAccess && memberAccess->intrinsicMember == IntrinsicMember::PackToStaticArray)
        {
            if (!node.arguments.empty())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Pack ToStaticArray does not accept runtime arguments.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (explicitTypeArguments.size() != 1 || !explicitTypeArguments.front())
            {
                WIO_LOG_ADD_ERROR(node.location(), "Pack ToStaticArray requires exactly one explicit element type argument.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            Ref<Type> receiverType = unwrapAliasType(memberAccess->object->refType.Lock());
            if (!receiverType || receiverType->kind() == TypeKind::TypePackView)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Type packs cannot be converted to runtime static arrays.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            size_t arraySize = 0;
            if (receiverType->kind() == TypeKind::ValuePackView)
                arraySize = receiverType.AsFast<ValuePackViewType>()->elementTypes.size();
            else if (receiverType->kind() == TypeKind::PackStorage)
                arraySize = receiverType.AsFast<PackStorageType>()->elementTypes.size();

            finalizeCallResultType(Compiler::get().getTypeContext().getOrCreateArrayType(
                explicitTypeArguments.front(),
                ArrayType::ArrayKind::Static,
                arraySize
            ));
            return;
        }

        bool isConstructorCall = false;
        Ref<Type> structReturnType = nullptr;
        Ref<StructType> constructorStructType = nullptr;
        std::vector<std::string> constructorGenericParameterNames;
        std::unordered_map<std::string, Ref<Type>> constructorGenericBindings;
        GenericBindingSet constructorGenericBindingSet;
        bool useExplicitFunctionTypeArguments = false;

        if (calleeSym && calleeSym->kind == SymbolKind::TypeAlias)
        {
            Ref<Type> resolvedAliasTargetType = nullptr;
            Ref<Type> resolvedAliasResultType = calleeSym->type;

            if (!calleeSym->genericParameterNames.empty())
            {
                const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                    calleeSym->genericParameterNames,
                    calleeSym->hasGenericParameterPack
                );
                const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                    calleeSym->genericParameterDefaults, fixedArgumentCount);
                auto completedArguments = completeGenericTypeArguments(
                    calleeSym->genericParameterNames,
                    calleeSym->genericParameterDefaults,
                    calleeSym->hasGenericParameterPack,
                    explicitTypeArguments);
                if (!completedArguments)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Type alias '{}' expects {} generic arguments, but got {}.",
                        calleeSym->name,
                        calleeSym->hasGenericParameterPack
                            ? common::formatString("at least {}", requiredArgumentCount)
                            : requiredArgumentCount == fixedArgumentCount
                                ? std::to_string(fixedArgumentCount)
                                : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                        explicitTypeArguments.size()
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                explicitTypeArguments = std::move(*completedArguments);

                if (!satisfiesApplyForSymbol(calleeSym, explicitTypeArguments, "type alias"))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                constructorGenericBindingSet = buildExtendedGenericBindings(
                    calleeSym->genericParameterNames,
                    calleeSym->hasGenericParameterPack,
                    explicitTypeArguments
                );
                constructorGenericBindings = constructorGenericBindingSet.directBindings;
                resolvedAliasTargetType = instantiateGenericType(calleeSym->aliasTargetType, constructorGenericBindingSet);
                resolvedAliasResultType = Compiler::get().getTypeContext().getOrCreateAliasType(
                    formatAppliedTypeName(calleeSym->name, explicitTypeArguments),
                    resolvedAliasTargetType
                );
            }
            else
            {
                if (!explicitTypeArguments.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Type alias '{}' does not accept generic arguments.",
                        calleeSym->name
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                resolvedAliasTargetType = calleeSym->aliasTargetType
                    ? calleeSym->aliasTargetType
                    : unwrapAliasType(calleeSym->type);
            }

            Ref<Type> resolvedConstructorTarget = unwrapAliasType(resolvedAliasTargetType);
            if (resolvedConstructorTarget && resolvedConstructorTarget->kind() == TypeKind::Struct)
            {
                isConstructorCall = true;
                auto structType = resolvedConstructorTarget.AsFast<StructType>();
                constructorStructType = structType;
                constructorGenericParameterNames = structType->genericParameterNames;
                structReturnType = resolvedAliasResultType;
                node.callee->refType = structReturnType;

                if (!constructorStructType->genericParameterNames.empty() &&
                    !constructorStructType->genericArguments.empty())
                {
                    constructorGenericBindings = buildGenericTypeBindings(
                        constructorStructType->genericParameterNames,
                        constructorStructType->genericArguments
                    );
                    constructorGenericBindingSet = buildExtendedGenericBindings(
                        constructorStructType->genericParameterNames,
                        constructorStructType->hasGenericParameterPack,
                        constructorStructType->genericArguments
                    );
                }

                if (auto lockedScope = constructorStructType->structScope.Lock())
                    calleeSym = lockedScope->resolveLocally("OnConstruct");

                if (!calleeSym)
                {
                    WIO_LOG_ADD_ERROR(node.location(), "No constructor found for type '{}'.", constructorStructType->name);
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }
        }

        if (calleeSym && calleeSym->kind == SymbolKind::Struct)
        {
            isConstructorCall = true;
            auto structType = calleeSym->type.AsFast<StructType>();
            constructorStructType = structType;
            constructorGenericParameterNames = structType->genericParameterNames;
            structReturnType = structType;

            if (!structType->genericParameterNames.empty())
            {
                  const size_t fixedArgumentCount = getMinimumGenericArgumentCount(
                      structType->genericParameterNames,
                      structType->hasGenericParameterPack
                  );
                  if (!explicitTypeArguments.empty())
                  {
                      const size_t requiredArgumentCount = getRequiredGenericArgumentCount(
                          structType->genericParameterDefaults, fixedArgumentCount);
                      auto completedArguments = completeGenericTypeArguments(
                          structType->genericParameterNames,
                          structType->genericParameterDefaults,
                          structType->hasGenericParameterPack,
                          explicitTypeArguments);
                      if (!completedArguments)
                      {
                          WIO_LOG_ADD_ERROR(
                              node.location(),
                              "Generic type '{}' expects {} generic arguments, but got {}.",
                              structType->name,
                              structType->hasGenericParameterPack
                                  ? common::formatString("at least {}", requiredArgumentCount)
                                  : requiredArgumentCount == fixedArgumentCount
                                      ? std::to_string(fixedArgumentCount)
                                      : common::formatString("{} to {}", requiredArgumentCount, fixedArgumentCount),
                              explicitTypeArguments.size()
                          );
                          node.refType = Compiler::get().getTypeContext().getUnknown();
                          return;
                      }
                      explicitTypeArguments = std::move(*completedArguments);
                  }

                if (!explicitTypeArguments.empty())
                {
                      constructorGenericBindings = buildGenericTypeBindings(structType->genericParameterNames, explicitTypeArguments);
                      constructorGenericBindingSet = buildExtendedGenericBindings(
                          structType->genericParameterNames,
                          structType->hasGenericParameterPack,
                          explicitTypeArguments
                      );
                    auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                    if (attributeIt != attributeListsBySymbol_.end() &&
                        attributeIt->second &&
                        !matchesApplyConstraints(
                            *attributeIt->second,
                            constructorGenericParameterNames,
                            structType->hasGenericParameterPack,
                            explicitTypeArguments))
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
                              (structType->hasGenericParameterPack
                                  ? expectedStructType->genericArguments.size() >= fixedArgumentCount
                                  : expectedStructType->genericArguments.size() == structType->genericParameterNames.size()))
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
                                  constructorGenericBindingSet = buildExtendedGenericBindings(
                                      structType->genericParameterNames,
                                      structType->hasGenericParameterPack,
                                      expectedStructType->genericArguments
                                  );
                                auto attributeIt = attributeListsBySymbol_.find(genericOwnerSym.Get());
                                if (attributeIt != attributeListsBySymbol_.end() &&
                                    attributeIt->second &&
                                    !matchesApplyConstraints(
                                        *attributeIt->second,
                                        constructorGenericParameterNames,
                                        structType->hasGenericParameterPack,
                                        expectedStructType->genericArguments))
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

            Ref<StructType> resolvedConstructorStructType = structType;
            if (auto resolvedReturnType = unwrapAliasType(structReturnType);
                resolvedReturnType && resolvedReturnType->kind() == TypeKind::Struct)
            {
                resolvedConstructorStructType = resolvedReturnType.AsFast<StructType>();
                constructorStructType = resolvedConstructorStructType;
            }

            if (auto lockedScope = resolvedConstructorStructType->structScope.Lock())
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

        std::optional<size_t> packExpansionIndex;
        for (size_t i = 0; i < node.arguments.size(); ++i)
        {
            if (!node.arguments[i] || !node.arguments[i]->is<PackExpansionExpression>())
                continue;

            if (packExpansionIndex.has_value())
            {
                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), "Only one pack expansion argument is supported in a function call.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (i + 1 != node.arguments.size())
            {
                WIO_LOG_ADD_ERROR(node.arguments[i]->location(), "Pack expansion arguments must be trailing.");
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            packExpansionIndex = i;
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

        auto getFunctionDeclarationForSymbol = [&](const Ref<Symbol>& symbol) -> const FunctionDeclaration*
        {
            if (!symbol)
                return nullptr;

            auto foundDeclaration = functionDeclarationsBySymbol_.find(symbol.Get());
            return foundDeclaration != functionDeclarationsBySymbol_.end() ? foundDeclaration->second : nullptr;
        };

        auto getRequiredArgumentCountForCallable = [&](const Ref<Symbol>& symbol,
                                                       const Ref<FunctionType>& functionType) -> size_t
        {
            if (const auto* functionDeclaration = getFunctionDeclarationForSymbol(symbol))
                return getRequiredParameterCount(functionDeclaration);

            if (functionType && functionType->hasParameterPack)
                return functionType->paramTypes.empty() ? 0 : functionType->paramTypes.size() - 1;

            return functionType ? functionType->paramTypes.size() : 0;
        };

        auto buildCallableSurfaceFunctionType = [&](const Ref<FunctionType>& functionType,
                                                   const std::vector<Ref<Type>>& actualArgumentTypes,
                                                   bool usesPackExpansion) -> Ref<FunctionType>
        {
            if (!functionType)
                return nullptr;

            if (!functionType->hasParameterPack)
            {
                const size_t surfaceArity = std::min(actualArgumentTypes.size(), functionType->paramTypes.size());
                std::vector<Ref<Type>> surfaceParamTypes;
                surfaceParamTypes.reserve(surfaceArity);
                for (size_t i = 0; i < surfaceArity; ++i)
                    surfaceParamTypes.push_back(functionType->paramTypes[i]);

                return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes)
                    .AsFast<FunctionType>();
            }

            const size_t fixedParameterCount = functionType->paramTypes.empty() ? 0 : functionType->paramTypes.size() - 1;
            std::vector<Ref<Type>> surfaceParamTypes;
            surfaceParamTypes.reserve(std::max(actualArgumentTypes.size(), functionType->paramTypes.size()));
            for (size_t i = 0; i < fixedParameterCount; ++i)
                surfaceParamTypes.push_back(functionType->paramTypes[i]);

            if (usesPackExpansion)
            {
                surfaceParamTypes.push_back(functionType->paramTypes.back());
                return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes, true)
                    .AsFast<FunctionType>();
            }

            for (size_t i = fixedParameterCount; i < actualArgumentTypes.size(); ++i)
                surfaceParamTypes.push_back(actualArgumentTypes[i]);

            return Compiler::get().getTypeContext().getOrCreateFunctionType(functionType->returnType, surfaceParamTypes, false)
                .AsFast<FunctionType>();
        };

        auto analyzeArgumentsForResolvedFunctionType = [&](const Ref<FunctionType>& functionType,
                                                           const FunctionDeclaration* functionDeclaration,
                                                           bool suppressDiagnostics,
                                                           bool requireExactArity) -> std::optional<std::vector<Ref<Type>>>
        {
            if (!functionType)
                return std::nullopt;

            const bool usesPackExpansion = packExpansionIndex.has_value();
            const bool hasParameterPack = functionType->hasParameterPack;
            const size_t fixedParameterCount = hasParameterPack && !functionType->paramTypes.empty()
                ? functionType->paramTypes.size() - 1
                : functionType->paramTypes.size();
            const size_t requiredArgumentCount = functionDeclaration
                ? getRequiredParameterCount(functionDeclaration)
                : (hasParameterPack ? fixedParameterCount : functionType->paramTypes.size());
            const size_t totalParameterCount = functionType->paramTypes.size();

            if (usesPackExpansion)
            {
                if (!hasParameterPack)
                    return std::nullopt;

                if (*packExpansionIndex != fixedParameterCount)
                    return std::nullopt;
            }
            else
            {
                if (node.arguments.size() < requiredArgumentCount)
                    return std::nullopt;

                if (!hasParameterPack && node.arguments.size() > totalParameterCount)
                    return std::nullopt;
            }

            if (requireExactArity)
            {
                if (usesPackExpansion)
                {
                    if (!hasParameterPack || node.arguments.size() != fixedParameterCount + 1)
                        return std::nullopt;
                }
                else if (hasParameterPack)
                {
                    if (node.arguments.size() < fixedParameterCount)
                        return std::nullopt;
                }
                else if (functionType->paramTypes.size() != node.arguments.size())
                {
                    return std::nullopt;
                }
            }

            std::vector<Ref<Type>> resolvedArgumentTypes;
            resolvedArgumentTypes.reserve(node.arguments.size());

            for (size_t i = 0; i < node.arguments.size(); ++i)
            {
                Ref<Type> expectedType = nullptr;
                if (usesPackExpansion)
                {
                    if (i < fixedParameterCount)
                        expectedType = functionType->paramTypes[i];
                    else if (i == *packExpansionIndex)
                        expectedType = functionType->paramTypes.back();
                }
                else if (!hasParameterPack && i < functionType->paramTypes.size())
                    expectedType = functionType->paramTypes[i];
                else if (hasParameterPack && i < fixedParameterCount)
                    expectedType = functionType->paramTypes[i];

                auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedType, suppressDiagnostics);
                if (!analyzedType.has_value())
                    return std::nullopt;

                resolvedArgumentTypes.push_back(*analyzedType);
            }

            return resolvedArgumentTypes;
        };

        std::vector<Ref<Symbol>> candidateSymbols;
        bool requiresOverloadResolution = false;
        bool isCallOperatorInvocation = false;
        bool callOperatorLookupFailed = false;
        OperatorDispatchKind callOperatorDispatchKind = OperatorDispatchKind::None;

        auto getCallableDisplayName = [&]() -> std::string
        {
            if (isConstructorCall && constructorStructType)
                return constructorStructType->name;

            if (isCallOperatorInvocation)
                return "operator()";

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

        auto diagnosePoisonedCallArguments = [&]() -> bool
        {
            const auto& argumentTypes = getUncontextualizedArgumentTypes();
            bool hasPoisonedArgument = false;
            for (size_t i = 0; i < argumentTypes.size(); ++i)
            {
                if (!argumentTypes[i] || !argumentTypes[i]->isPoisoned())
                    continue;

                hasPoisonedArgument = true;
                // Candidate probing intentionally suppresses diagnostics. Replay
                // only poisoned arguments outside the probe so the root error is
                // preserved without adding a derivative overload diagnostic.
                (void)analyzeArgumentWithExpectedType(node.arguments[i], nullptr, false);
            }

            if (hasPoisonedArgument)
                node.refType = Compiler::get().getTypeContext().getUnknown();
            return hasPoisonedArgument;
        };

        auto scoreResolvedCall = [&](const Ref<FunctionType>& functionType,
                                     const std::vector<Ref<Type>>& actualArgumentTypes,
                                     size_t requiredArgumentCount,
                                     bool isGenericCandidate,
                                     size_t genericParameterCount) -> std::optional<int>
        {
            if (!functionType ||
                actualArgumentTypes.size() < requiredArgumentCount ||
                (!functionType->hasParameterPack && actualArgumentTypes.size() > functionType->paramTypes.size()))
                return std::nullopt;

            int currentScore = 0;
            const size_t fixedParameterCount = functionType->hasParameterPack && !functionType->paramTypes.empty()
                ? functionType->paramTypes.size() - 1
                : functionType->paramTypes.size();
            for (size_t i = 0; i < actualArgumentTypes.size(); ++i)
            {
                if (functionType->hasParameterPack && i >= fixedParameterCount)
                {
                    currentScore += actualArgumentTypes[i] && actualArgumentTypes[i]->kind() == TypeKind::GenericParameterPack ? 4 : 8;
                    continue;
                }

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

            if (!functionType->hasParameterPack)
                currentScore -= static_cast<int>((functionType->paramTypes.size() - actualArgumentTypes.size()) * 25);

              if (genericParameterCount > 0)
                  currentScore -= static_cast<int>(genericParameterCount * 25);

              if (isGenericCandidate)
                  currentScore -= static_cast<int>(std::max<size_t>(1, genericParameterCount) * 10);

            return currentScore;
        };

        auto appendMemberCallOperatorCandidates = [&]() -> bool
        {
            const auto overloadName = common::getCallOperatorOverloadName(TokenType::leftParen);
            if (!overloadName.has_value())
                return false;

            std::unordered_set<const Symbol*> seenSymbols;
            bool accessViolation = false;

            auto appendCandidateSymbol = [&](const Ref<Type>& candidateReceiverType)
            {
                Ref<Type> currentType = unwrapAliasType(candidateReceiverType);
                while (currentType && currentType->kind() == TypeKind::Reference)
                    currentType = unwrapAliasType(currentType.AsFast<ReferenceType>()->referredType);

                if (!currentType || currentType->kind() != TypeKind::Struct)
                    return;

                Ref<Type> ownerType = nullptr;
                Ref<Symbol> candidateSymbol = findStructMemberInHierarchy(currentType, std::string(*overloadName), &ownerType);
                if (!candidateSymbol || seenSymbols.contains(candidateSymbol.Get()))
                    return;

                seenSymbols.insert(candidateSymbol.Get());

                if (!validateStructMemberAccess(currentStructType_, ownerType, candidateSymbol, node.location()))
                {
                    accessViolation = true;
                    return;
                }

                auto ownerStructType = ownerType ? unwrapAliasType(ownerType).AsFast<StructType>() : nullptr;
                if (!ownerStructType)
                    return;

                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    candidateSymbols = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    candidateSymbols = { candidateSymbol };
                else
                    return;

                isCallOperatorInvocation = !candidateSymbols.empty();
                callOperatorDispatchKind = OperatorDispatchKind::Member;
            };

            Ref<Type> calleeTypeForCallOperator = node.callee ? node.callee->refType.Lock() : nullptr;
            appendCandidateSymbol(calleeTypeForCallOperator);
            if (!isCallOperatorInvocation)
            {
                Ref<Type> readableCalleeType = getAutoReadableType(calleeTypeForCallOperator);
                if (readableCalleeType != calleeTypeForCallOperator)
                    appendCandidateSymbol(readableCalleeType);
            }

            if (accessViolation)
            {
                callOperatorLookupFailed = true;
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            if (isCallOperatorInvocation)
                requiresOverloadResolution = true;

            return isCallOperatorInvocation;
        };

        const bool calleeAlreadyCallable =
            (calleeSym && (calleeSym->kind == SymbolKind::Function || calleeSym->kind == SymbolKind::FunctionGroup)) ||
            (node.callee->refType.Lock() && node.callee->refType.Lock()->kind() == TypeKind::Function);
        if (!calleeAlreadyCallable)
            appendMemberCallOperatorCandidates();
        if (callOperatorLookupFailed)
            return;

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
            if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
                resolvedCalleeType = instantiateGenericType(resolvedCalleeType, constructorGenericBindingSet);

            requiresOverloadResolution = (!calleeSym->genericParameterNames.empty() && containsGenericParameterType(resolvedCalleeType)) ||
                                         useExplicitFunctionTypeArguments ||
                                         (isConstructorCall && !constructorGenericParameterNames.empty()) ||
                                          !constructorGenericBindings.empty() ||
                                          !constructorGenericBindingSet.packBindings.empty();
        }

        if (!requiresOverloadResolution)
        {
            for (const auto& candidateSymbol : candidateSymbols)
            {
                const auto* candidateDeclaration = getFunctionDeclarationForSymbol(candidateSymbol);
                if (candidateDeclaration &&
                    getRequiredParameterCount(candidateDeclaration) != candidateDeclaration->parameters.size())
                {
                    requiresOverloadResolution = true;
                    break;
                }
            }
        }

        if (requiresOverloadResolution)
        {
            struct ResolvedFunctionCandidate
            {
                Ref<Symbol> symbol = nullptr;
                Ref<Type> functionType = nullptr;
                Ref<FunctionType> fullFunctionType = nullptr;
                int score = -1;
                std::unordered_map<std::string, Ref<Type>> genericBindings;
                GenericBindingSet bindingSet;
                bool usedGenericDefaults = false;
            };

            auto formatCandidateSignature = [&](const Ref<Symbol>& overload) -> std::string
            {
                if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                    return "<invalid overload>";

                const auto functionType = overload->type.AsFast<FunctionType>();
                const auto& genericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                const std::string callableName =
                    isConstructorCall && constructorStructType ? constructorStructType->name :
                    isCallOperatorInvocation ? std::string("operator()") :
                    overload->name;

                return formatFunctionDiagnosticSignature(
                    callableName,
                    genericParameterNames,
                    functionType,
                    isConstructorCall,
                    overload->hasGenericParameterPack
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

            auto candidateAcceptsExplicitTypeArity = [&](const Ref<Symbol>& candidate, const size_t explicitArity) -> bool
            {
                if (!candidate)
                    return false;

                const auto& genericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                if (genericParameterNames.empty())
                    return false;

                const bool hasCandidatePack = isConstructorCall && constructorStructType
                    ? constructorStructType->hasGenericParameterPack
                    : candidate->hasGenericParameterPack;
                const auto& defaults = isConstructorCall && constructorStructType
                    ? constructorStructType->genericParameterDefaults
                    : candidate->genericParameterDefaults;
                const size_t fixedArity = getMinimumGenericArgumentCount(genericParameterNames, hasCandidatePack);
                const size_t minimumArity = getRequiredGenericArgumentCount(defaults, fixedArity);
                return explicitArity >= minimumArity && (hasCandidatePack || explicitArity <= fixedArity);
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
                                                   const bool hasActiveGenericParameterPack,
                                                   const GenericBindingSet& bindings) -> bool
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
                    return true;

                auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
                if (!resolvedBindingTypes.has_value())
                    return true;

                for (const auto& instantiationTypes : overload->resolvedGenericInstantiations)
                {
                    if (instantiationTypes.size() != resolvedBindingTypes->size())
                        continue;

                    bool matches = true;
                    for (size_t i = 0; i < instantiationTypes.size(); ++i)
                    {
                        if (!isExactConstraintTypeMatch(instantiationTypes[i], (*resolvedBindingTypes)[i]))
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
                                             const bool hasActiveGenericParameterPack,
                                             const GenericBindingSet& bindings) -> bool
            {
                if (!symbol || activeGenericParameterNames.empty())
                    return true;

                auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return true;

                return matchesApplyConstraints(
                    *attributeIt->second,
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
            };

            auto satisfiesOpenNativeTemplateIntrinsicBinding = [&](const Ref<Symbol>& symbol,
                                                                   const std::vector<std::string>& activeGenericParameterNames,
                                                                   const bool hasActiveGenericParameterPack,
                                                                   const GenericBindingSet& bindings) -> bool
            {
                if (!symbol || activeGenericParameterNames.empty())
                    return true;

                auto attributeIt = attributeListsBySymbol_.find(symbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return true;

                if (!isOpenNativeTemplateIntrinsic(*attributeIt->second))
                    return true;

                auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                    activeGenericParameterNames,
                    hasActiveGenericParameterPack,
                    bindings
                );
                if (!resolvedBindingTypes.has_value())
                    return true;

                return matchesOpenNativeTemplateIntrinsicConstraints(*attributeIt->second, *resolvedBindingTypes);
            };

            bool rejectedByInstantiationWhitelist = false;
            std::string rejectedInstantiationFunctionName;
            std::string rejectedInstantiationSignature;
            bool rejectedByApplyConstraints = false;
            std::string rejectedApplyTargetName;
            std::string rejectedApplySignature;
            bool rejectedByOpenNativeTemplateIntrinsic = false;
            std::string rejectedOpenNativeTemplateTargetName;
            std::string rejectedOpenNativeTemplateSignature;

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
                if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, constructorGenericBindingSet);

                auto declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                if (!declaredFunctionType)
                    return std::nullopt;
                const bool declaredFunctionHadParameterPack = declaredFunctionType->hasParameterPack;

                const std::vector<std::string>& activeGenericParameterNames =
                    isConstructorCall ? constructorGenericParameterNames : overload->genericParameterNames;
                const bool hasGenericParameterPack = isConstructorCall && constructorStructType
                    ? constructorStructType->hasGenericParameterPack
                    : overload->hasGenericParameterPack;
                const std::vector<Ref<Type>>& genericParameterDefaults = isConstructorCall && constructorStructType
                    ? constructorStructType->genericParameterDefaults
                    : overload->genericParameterDefaults;
                GenericBindingSet bindingSet;
                bool usedGenericDefaults = false;
                if (isConstructorCall &&
                    (!constructorGenericBindingSet.directBindings.empty() ||
                     !constructorGenericBindingSet.packBindings.empty() ||
                     !constructorGenericBindingSet.packAliases.empty()))
                {
                    bindingSet = constructorGenericBindingSet;
                }
                bool isGenericCandidate = containsGenericParameterType(resolvedFunctionType);

                if (useExplicitFunctionTypeArguments)
                {
                    if (!isGenericCandidate && activeGenericParameterNames.empty())
                        return std::nullopt;

                    auto completedExplicitArguments = completeGenericTypeArguments(
                        activeGenericParameterNames,
                        genericParameterDefaults,
                        hasGenericParameterPack,
                        explicitTypeArguments);
                    if (!completedExplicitArguments)
                    {
                        return std::nullopt;
                    }
                    usedGenericDefaults = completedExplicitArguments->size() > explicitTypeArguments.size();

                    for (const auto& explicitTypeArgument : *completedExplicitArguments)
                    {
                        if (!explicitTypeArgument ||
                            explicitTypeArgument->isUnknown())
                        {
                            return std::nullopt;
                        }
                    }

                    bindingSet = buildExtendedGenericBindings(
                        activeGenericParameterNames,
                        hasGenericParameterPack,
                        *completedExplicitArguments
                    );
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, bindingSet);
                    declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                    if (!declaredFunctionType)
                        return std::nullopt;

                    isGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                }

                const bool candidateHasParameterPack = declaredFunctionType->hasParameterPack;
                const size_t fixedParameterCount = candidateHasParameterPack && !declaredFunctionType->paramTypes.empty()
                    ? declaredFunctionType->paramTypes.size() - 1
                    : declaredFunctionType->paramTypes.size();
                const auto* candidateDeclaration = getFunctionDeclarationForSymbol(overload);
                size_t requiredArgumentCount = candidateDeclaration
                    ? getRequiredParameterCount(candidateDeclaration)
                    : (candidateHasParameterPack ? fixedParameterCount : declaredFunctionType->paramTypes.size());

                if (useExplicitFunctionTypeArguments && hasGenericParameterPack && declaredFunctionHadParameterPack && !candidateHasParameterPack && !activeGenericParameterNames.empty())
                {
                    auto packIt = bindingSet.packBindings.find(activeGenericParameterNames.back());
                    if (packIt != bindingSet.packBindings.end())
                        requiredArgumentCount += packIt->second.size();
                }

                if (packExpansionIndex.has_value())
                {
                    if (!candidateHasParameterPack || *packExpansionIndex != fixedParameterCount)
                        return std::nullopt;
                }
                else if (node.arguments.size() < requiredArgumentCount || (!candidateHasParameterPack && node.arguments.size() > declaredFunctionType->paramTypes.size()))
                {
                    return std::nullopt;
                }

                std::vector<std::string> deducibleGenericParameterNames = activeGenericParameterNames;
                if (hasGenericParameterPack && !deducibleGenericParameterNames.empty())
                    deducibleGenericParameterNames.pop_back();

                if (useExplicitFunctionTypeArguments && !isGenericCandidate)
                    deducibleGenericParameterNames.clear();

                std::unordered_map<std::string, const Type*> activeGenericParameterInstances;
                if (isGenericCandidate)
                    collectGenericParameterInstances(resolvedFunctionType, activeGenericParameterNames, activeGenericParameterInstances);

                std::unordered_map<std::string, Ref<Type>> bindings = bindingSet.directBindings;
                std::vector<Ref<Type>> candidateArgTypes(node.arguments.size());
                std::vector<bool> analyzedArguments(node.arguments.size(), false);

                auto getExpectedParameterType = [&](size_t index) -> Ref<Type>
                {
                    Ref<Type> expectedParameterType = nullptr;
                    if (candidateHasParameterPack)
                    {
                        if (index < fixedParameterCount)
                            expectedParameterType = declaredFunctionType->paramTypes[index];
                        else if (packExpansionIndex.has_value() && index == *packExpansionIndex)
                            expectedParameterType = declaredFunctionType->paramTypes.back();
                    }
                    else if (index < declaredFunctionType->paramTypes.size())
                    {
                        expectedParameterType = declaredFunctionType->paramTypes[index];
                    }

                    if (!bindingSet.directBindings.empty() ||
                        !bindingSet.packBindings.empty() ||
                        !bindingSet.packAliases.empty())
                    {
                        expectedParameterType = instantiateGenericType(expectedParameterType, bindingSet);
                    }
                    return expectedParameterType;
                };

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    const bool usesPackExpansionArgument = packExpansionIndex.has_value() && i == *packExpansionIndex;
                    const bool isConcretePackTailArgument = candidateHasParameterPack && !packExpansionIndex.has_value() && i >= fixedParameterCount;
                    bool shouldDeferArgument =
                        isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        isContextSensitiveArgument(node.arguments[i]) &&
                        containsNamedGenericParameterType(expectedParameterType, deducibleGenericParameterNames);

                    if (shouldDeferArgument)
                        continue;

                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }

                    bindingSet.directBindings = bindings;
                }

                for (size_t i = 0; i < node.arguments.size(); ++i)
                {
                    if (analyzedArguments[i])
                        continue;

                    Ref<Type> expectedParameterType = getExpectedParameterType(i);
                    const bool usesPackExpansionArgument = packExpansionIndex.has_value() && i == *packExpansionIndex;
                    const bool isConcretePackTailArgument = candidateHasParameterPack && !packExpansionIndex.has_value() && i >= fixedParameterCount;
                    auto analyzedType = analyzeArgumentWithExpectedType(node.arguments[i], expectedParameterType, true);
                    if (!analyzedType.has_value())
                        return std::nullopt;

                    candidateArgTypes[i] = *analyzedType;
                    analyzedArguments[i] = true;

                    if (isGenericCandidate &&
                        !usesPackExpansionArgument &&
                        !isConcretePackTailArgument &&
                        !deduceGenericBindings(expectedParameterType, candidateArgTypes[i], bindings))
                    {
                        return std::nullopt;
                    }

                    bindingSet.directBindings = bindings;
                }

                if (std::ranges::any_of(analyzedArguments, [](bool analyzed) { return !analyzed; }))
                    return std::nullopt;

                // Deduction wins for parameters mentioned by the function
                // signature. Defaults then fill only the remaining holes.
                const size_t fixedGenericParameterCount = getMinimumGenericArgumentCount(
                    activeGenericParameterNames, hasGenericParameterPack);
                for (size_t genericIndex = 0; genericIndex < fixedGenericParameterCount; ++genericIndex)
                {
                    const std::string& parameterName = activeGenericParameterNames[genericIndex];
                    if (bindings.contains(parameterName) && bindings.at(parameterName) && !bindings.at(parameterName)->isUnknown())
                        continue;
                    if (genericIndex >= genericParameterDefaults.size() || !genericParameterDefaults[genericIndex])
                        continue;

                    bindingSet.directBindings = bindings;
                    Ref<Type> defaultType = instantiateGenericType(genericParameterDefaults[genericIndex], bindingSet);
                    bindings[parameterName] = defaultType;
                    bindingSet.directBindings[parameterName] = defaultType;
                    usedGenericDefaults = true;
                }

                if (isGenericCandidate)
                {
                    for (const auto& genericParameterName : deducibleGenericParameterNames)
                    {
                        if (!bindings.contains(genericParameterName) ||
                            !bindings.at(genericParameterName) ||
                            bindings.at(genericParameterName)->isUnknown())
                        {
                            return std::nullopt;
                        }
                    }
                }

                bindingSet.directBindings = bindings;
                if (hasGenericParameterPack &&
                    candidateHasParameterPack &&
                    !activeGenericParameterNames.empty() &&
                    !packExpansionIndex.has_value())
                {
                    const std::string& packName = activeGenericParameterNames.back();
                    if (!bindingSet.packBindings.contains(packName) && !bindingSet.packAliases.contains(packName))
                    {
                        std::vector<Ref<Type>> packBindingTypes;
                        if (candidateArgTypes.size() > fixedParameterCount)
                        {
                            packBindingTypes.reserve(candidateArgTypes.size() - fixedParameterCount);
                            for (size_t i = fixedParameterCount; i < candidateArgTypes.size(); ++i)
                            {
                                packBindingTypes.push_back(candidateArgTypes[i]);
                                bindingSet.directBindings.emplace(
                                    makePackElementBindingName(packName, i - fixedParameterCount),
                                    candidateArgTypes[i]
                                );
                            }
                        }

                        bindingSet.packBindings.emplace(packName, std::move(packBindingTypes));
                    }
                }

                if (!activeGenericParameterNames.empty())
                {
                    if (!isAllowedInstantiateBinding(overload, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByInstantiationWhitelist = true;
                        rejectedInstantiationFunctionName = overload->name;
                        rejectedInstantiationSignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }

                    Ref<Symbol> applyConstraintOwner = isConstructorCall ? genericOwnerSym : overload;
                    if (!satisfiesApplyBinding(applyConstraintOwner, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByApplyConstraints = true;
                        rejectedApplyTargetName =
                            isConstructorCall && constructorStructType ? constructorStructType->name : overload->name;
                        rejectedApplySignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }

                    if (!satisfiesOpenNativeTemplateIntrinsicBinding(overload, activeGenericParameterNames, hasGenericParameterPack, bindingSet))
                    {
                        auto resolvedBindingTypes = tryMaterializeConcreteInstantiation(
                            activeGenericParameterNames,
                            hasGenericParameterPack,
                            bindingSet
                        );

                        rejectedByOpenNativeTemplateIntrinsic = true;
                        rejectedOpenNativeTemplateTargetName = overload->name;
                        rejectedOpenNativeTemplateSignature = resolvedBindingTypes.has_value()
                            ? formatInstantiationSignature(*resolvedBindingTypes)
                            : "<unresolved>";
                        return std::nullopt;
                    }
                }

                if (!bindingSet.directBindings.empty() ||
                    !bindingSet.packBindings.empty() ||
                    !bindingSet.packAliases.empty())
                {
                    resolvedFunctionType = instantiateGenericType(resolvedFunctionType, bindingSet);
                    declaredFunctionType = resolvedFunctionType.AsFast<FunctionType>();
                    if (!declaredFunctionType)
                        return std::nullopt;
                }

                const bool remainsGenericCandidate = containsGenericParameterType(resolvedFunctionType);
                if (auto score = scoreResolvedCall(
                        resolvedFunctionType.AsFast<FunctionType>(),
                        candidateArgTypes,
                        requiredArgumentCount,
                        remainsGenericCandidate,
                        activeGenericParameterNames.size()
                    ); score.has_value())
                {
                    auto callableSurfaceType = buildCallableSurfaceFunctionType(
                        resolvedFunctionType.AsFast<FunctionType>(),
                        candidateArgTypes,
                        packExpansionIndex.has_value()
                    );
                    return ResolvedFunctionCandidate{
                        .symbol = overload,
                        .functionType = callableSurfaceType ? callableSurfaceType : resolvedFunctionType,
                        .fullFunctionType = resolvedFunctionType.AsFast<FunctionType>(),
                        .score = *score,
                        .genericBindings = bindings,
                        .bindingSet = bindingSet,
                        .usedGenericDefaults = usedGenericDefaults
                    };
                }

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
                if (diagnosePoisonedCallArguments())
                    return;

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
                if (diagnosePoisonedCallArguments())
                    return;

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
                else if (rejectedByOpenNativeTemplateIntrinsic)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Generic native callable '{}' accepts only enum or flagset type arguments. Got {}.",
                        rejectedOpenNativeTemplateTargetName,
                        rejectedOpenNativeTemplateSignature
                    );
                }
                else
                {
                    const auto availableGenericArities = buildAvailableGenericAritySummary();
                    const bool providedExplicitTypeArguments = useExplicitFunctionTypeArguments || (isConstructorCall && !explicitTypeArguments.empty());
                    bool anyCandidateAcceptsExplicitTypeArity = false;
                    if (providedExplicitTypeArguments)
                    {
                        for (const auto& candidate : candidateSymbols)
                        {
                            if (candidateAcceptsExplicitTypeArity(candidate, explicitTypeArguments.size()))
                            {
                                anyCandidateAcceptsExplicitTypeArity = true;
                                break;
                            }
                        }
                    }

                    if (providedExplicitTypeArguments &&
                        !availableGenericArities.empty() &&
                        !anyCandidateAcceptsExplicitTypeArity)
                    {
                        std::vector<size_t> exactArities;
                        std::vector<size_t> minimumPackArities;
                        for (const auto& candidate : candidateSymbols)
                        {
                            if (!candidate)
                                continue;

                            const auto& genericParameterNames =
                                isConstructorCall ? constructorGenericParameterNames : candidate->genericParameterNames;
                            if (genericParameterNames.empty())
                                continue;

                            if (candidate->hasGenericParameterPack)
                                appendUniqueValue(minimumPackArities, getMinimumGenericArgumentCount(genericParameterNames, true));
                            else
                                appendUniqueValue(exactArities, genericParameterNames.size());
                        }

                        std::ranges::sort(exactArities);
                        std::ranges::sort(minimumPackArities);

                        if (exactArities.empty() && minimumPackArities.size() == 1)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                "Generic call to '{}' expects at least {} explicit type arguments, but got {}.",
                                getCallableDisplayName(),
                                minimumPackArities.front(),
                                explicitTypeArguments.size()
                            );
                        }
                        else if (availableGenericArities.size() == 1 && minimumPackArities.empty())
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
                            bool needsSeparator = false;
                            for (size_t i = 0; i < exactArities.size(); ++i)
                            {
                                if (needsSeparator)
                                    arityList += ", ";
                                arityList += std::to_string(exactArities[i]);
                                needsSeparator = true;
                            }

                            for (size_t i = 0; i < minimumPackArities.size(); ++i)
                            {
                                if (needsSeparator)
                                    arityList += ", ";
                                arityList += common::formatString("{}+", minimumPackArities[i]);
                                needsSeparator = true;
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

            if (isConstructorCall &&
                constructorStructType &&
                !constructorGenericParameterNames.empty() &&
                constructorGenericBindings.empty() &&
                constructorGenericBindingSet.packBindings.empty())
            {
                auto deducedGenericArguments = tryMaterializeConcreteInstantiation(
                    constructorGenericParameterNames,
                    constructorStructType->hasGenericParameterPack,
                    bestMatch->bindingSet
                );
                if (!deducedGenericArguments.has_value())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Cannot infer generic arguments for constructor '{}'. Use explicit type arguments.",
                        constructorStructType->name
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                constructorGenericBindings = bestMatch->bindingSet.directBindings;
                constructorGenericBindingSet = bestMatch->bindingSet;
                structReturnType = instantiateGenericStructType(constructorStructType, *deducedGenericArguments);
                node.callee->refType = structReturnType;
            }

            auto getConcreteCallableOwnerType = [&]() -> Ref<StructType>
            {
                auto resolveStructReceiverType = [&](const Ref<Type>& candidateType) -> Ref<StructType>
                {
                    Ref<Type> resolvedType = unwrapAliasType(candidateType);
                    while (resolvedType && resolvedType->kind() == TypeKind::Reference)
                        resolvedType = unwrapAliasType(resolvedType.AsFast<ReferenceType>()->referredType);
                    return resolvedType && resolvedType->kind() == TypeKind::Struct
                        ? resolvedType.AsFast<StructType>()
                        : nullptr;
                };

                if (isConstructorCall)
                    return resolveStructReceiverType(structReturnType);
                if (isCallOperatorInvocation)
                    return resolveStructReceiverType(node.callee ? node.callee->refType.Lock() : nullptr);
                if (const auto* memberAccess = node.callee ? node.callee->as<MemberAccessExpression>() : nullptr)
                    return resolveStructReceiverType(memberAccess->object ? memberAccess->object->refType.Lock() : nullptr);
                return nullptr;
            };

            if (bestMatch->symbol)
            {
                if (!isConstructorCall && bestMatch->usedGenericDefaults && !bestMatch->symbol->genericParameterNames.empty())
                {
                    if (auto resolvedArguments = tryMaterializeConcreteInstantiation(
                            bestMatch->symbol->genericParameterNames,
                            bestMatch->symbol->hasGenericParameterPack,
                            bestMatch->bindingSet))
                    {
                        node.resolvedGenericArguments.clear();
                        node.resolvedGenericArguments.reserve(resolvedArguments->size());
                        for (const auto& argument : *resolvedArguments)
                            node.resolvedGenericArguments.emplace_back(argument);
                    }
                }

                auto declarationIt = functionDeclarationsBySymbol_.find(bestMatch->symbol.Get());
                Ref<StructType> concreteOwnerType = getConcreteCallableOwnerType();
                if (declarationIt != functionDeclarationsBySymbol_.end() &&
                    declarationIt->second &&
                    bestMatch->fullFunctionType &&
                    !validateConcreteGenericFunctionBody(
                        *declarationIt->second,
                        bestMatch->symbol,
                        bestMatch->fullFunctionType,
                        concreteOwnerType,
                        bestMatch->bindingSet.directBindings,
                        bestMatch->bindingSet.packBindings,
                        bestMatch->bindingSet.packAliases))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
            }

            if (isCallOperatorInvocation)
            {
                node.referencedSymbol = bestMatch->symbol;
                node.operatorDispatchKind = callOperatorDispatchKind;
                node.overloadFunctionType = bestMatch->functionType.AsFast<Type>();
            }
            else if (!isConstructorCall)
            {
                node.callee->referencedSymbol = bestMatch->symbol;
                node.callee->refType = bestMatch->functionType;
            }
            else if (constructorStructType && !constructorGenericParameterNames.empty() && (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty()))
            {
                node.callee->refType = structReturnType;
            }

            if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(
                    bestMatch->functionType.AsFast<FunctionType>(),
                    nullptr,
                    false,
                    false
                ); !resolvedArgumentTypes.has_value())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (!finalizeCallResultType(isConstructorCall ? structReturnType : bestMatch->functionType.AsFast<FunctionType>()->returnType))
                return;
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
                    nullptr,
                    true,
                    true
                );
                if (!overloadArgumentTypes.has_value())
                    continue;

                auto score = scoreResolvedCall(overloadType.AsFast<FunctionType>(), *overloadArgumentTypes, overloadType.AsFast<FunctionType>()->paramTypes.size(), false, 0);
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
                if (diagnosePoisonedCallArguments())
                    return;

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
                if (diagnosePoisonedCallArguments())
                    return;

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
                    nullptr,
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

            if (!finalizeCallResultType(calleeType.AsFast<FunctionType>()->returnType))
                return;
            return;
        }

        if (!calleeSym && (!calleeType || calleeType->kind() != TypeKind::Function))
        {
            if (calleeType && calleeType->isPoisoned())
            {
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            WIO_LOG_ADD_ERROR(node.location(), "Called expression is undefined.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        if (!calleeType || calleeType->kind() != TypeKind::Function)
            calleeType = calleeSym->type;
        if (!constructorGenericBindings.empty() || !constructorGenericBindingSet.packBindings.empty())
            calleeType = instantiateGenericType(calleeType, constructorGenericBindingSet);

        if (!calleeType || calleeType->kind() != TypeKind::Function)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Called expression is not a function or struct.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        auto funcType = calleeType.AsFast<FunctionType>();
        std::vector<Ref<Type>> argTypes;
        const auto* directFunctionDeclaration = getFunctionDeclarationForSymbol(calleeSym);
        if (auto resolvedArgumentTypes = analyzeArgumentsForResolvedFunctionType(funcType, directFunctionDeclaration, false, false);
            resolvedArgumentTypes.has_value())
        {
            argTypes = std::move(*resolvedArgumentTypes);
        }
        else
        {
            const size_t requiredArgumentCount = getRequiredArgumentCountForCallable(calleeSym, funcType);
            const size_t totalParameterCount = funcType ? funcType->paramTypes.size() : 0;
            if (node.arguments.size() < requiredArgumentCount || (!funcType->hasParameterPack && node.arguments.size() > totalParameterCount))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Function expects '{}' arguments, but got '{}'.",
                    funcType->hasParameterPack
                        ? common::formatString("{} or more", requiredArgumentCount)
                        : formatExpectedArgumentCountDescription(requiredArgumentCount, totalParameterCount),
                    node.arguments.size()
                );
            }
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }

        const size_t requiredArgumentCount = getRequiredArgumentCountForCallable(calleeSym, funcType);
        if (argTypes.size() < requiredArgumentCount || (!funcType->hasParameterPack && argTypes.size() > funcType->paramTypes.size()))
            WIO_LOG_ADD_ERROR(node.location(), "Function expects '{}' arguments, but got '{}'.",
                funcType->hasParameterPack
                    ? common::formatString("{} or more", requiredArgumentCount)
                    : formatExpectedArgumentCountDescription(requiredArgumentCount, funcType->paramTypes.size()),
                argTypes.size());
        
        const size_t fixedParameterCount = funcType->hasParameterPack && !funcType->paramTypes.empty()
            ? funcType->paramTypes.size() - 1
            : funcType->paramTypes.size();
        size_t argCount = funcType->hasParameterPack ? std::min(argTypes.size(), fixedParameterCount) : std::min(argTypes.size(), funcType->paramTypes.size());
        for (size_t i = 0; i < argCount; ++i)
        {
            auto expectedType = funcType->paramTypes[i];
            const auto& actualType = argTypes[i];

            if (!expectedType || !actualType || expectedType->isPoisoned() || actualType->isPoisoned())
                continue;

            if (!isAssignmentLikeCompatible(expectedType, actualType) &&
                !isImplicitObjectViewBridge(expectedType, actualType) &&
                !isSafeRefCast(expectedType, actualType))
            {
                if (isRejectedImplicitNumericConversion(expectedType, actualType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.arguments[i]->location(),
                        "Implicit narrowing argument conversion from '{}' to '{}' at index {} requires explicit 'fit'.",
                        actualType->toString(), expectedType->toString(), i);
                    continue;
                }

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

        finalizeCallResultType(isConstructorCall ? structReturnType : funcType->returnType);
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
        
        bool isMut = isMutableAddressableOperand(node.operand);

        node.isMut = isMut;
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(node.operand->refType.Lock(), isMut);
        node.borrowOrigin = classifyBorrowOrigin(node.operand);
    }

    void SemanticAnalyzer::visit(FitExpression& node)
    {
        node.operand->accept(*this);
        node.targetType->accept(*this);

        auto srcType = node.operand->refType.Lock();
        auto destType = node.targetType->refType.Lock();

        auto tryResolveFitOperatorOverload = [&]() -> bool
        {
            const auto overloadName = common::getConversionOperatorOverloadName(TokenType::kwFit);
            if (!overloadName.has_value() || !srcType || !destType)
                return false;

            struct OperatorCandidate
            {
                Ref<Symbol> symbol;
                Ref<FunctionType> functionType;
                Ref<StructType> ownerType;
                OperatorDispatchKind dispatchKind = OperatorDispatchKind::None;
                int score = 0;
            };

            auto deduceBindingsFromFitArgument = [&](const Ref<Type>& expectedType,
                                                     const Ref<Type>& argumentType,
                                                     const NodePtr<Expression>& argumentExpression,
                                                     std::unordered_map<std::string, Ref<Type>>& bindings) -> bool
            {
                if (!expectedType || !argumentType)
                    return false;

                if (deduceGenericBindings(expectedType, argumentType, bindings))
                    return true;

                Ref<Type> readableArgumentType = getAutoReadableType(argumentType);
                if (argumentType != readableArgumentType)
                {
                    if (readableArgumentType && deduceGenericBindings(expectedType, readableArgumentType, bindings))
                        return true;
                }

                Ref<Type> resolvedExpectedType = unwrapAliasType(expectedType);
                if (resolvedExpectedType && resolvedExpectedType->kind() == TypeKind::Reference)
                {
                    auto expectedReferenceType = resolvedExpectedType.AsFast<ReferenceType>();
                    if (expectedReferenceType)
                    {
                        Ref<Type> referredType = expectedReferenceType->referredType;
                        if (deduceGenericBindings(referredType, argumentType, bindings))
                            return true;

                        if (readableArgumentType && deduceGenericBindings(referredType, readableArgumentType, bindings))
                            return true;

                        if (argumentExpression && isAddressableRefOperand(argumentExpression))
                        {
                            Ref<Type> resolvedArgumentType = unwrapAliasType(argumentType);
                            if (resolvedArgumentType && resolvedArgumentType->kind() == TypeKind::Reference)
                                resolvedArgumentType = resolvedArgumentType.AsFast<ReferenceType>()->referredType;

                            if (resolvedArgumentType && deduceGenericBindings(referredType, resolvedArgumentType, bindings))
                                return true;
                        }
                    }
                }

                return false;
            };

            auto scoreFitArgumentAgainstParameter = [&](const Ref<Type>& parameterType,
                                                        const Ref<Type>& rawArgumentType,
                                                        const Ref<Type>& readableArgumentType,
                                                        const NodePtr<Expression>& argumentExpression) -> std::optional<int>
            {
                if (!parameterType || !rawArgumentType)
                    return std::nullopt;

                Ref<Type> resolvedParameterType = unwrapAliasType(parameterType);
                if (resolvedParameterType && resolvedParameterType->kind() == TypeKind::Reference)
                {
                    auto referenceType = resolvedParameterType.AsFast<ReferenceType>();
                    if (argumentExpression && isAddressableRefOperand(argumentExpression))
                    {
                        Ref<Type> addressableArgumentType = rawArgumentType;
                        Ref<Type> resolvedAddressableType = unwrapAliasType(addressableArgumentType);
                        if (resolvedAddressableType && resolvedAddressableType->kind() == TypeKind::Reference)
                            addressableArgumentType = resolvedAddressableType.AsFast<ReferenceType>()->referredType;

                        if (isAssignmentLikeCompatible(parameterType, addressableArgumentType))
                            return referenceType->isMutable ? 4 : 3;
                    }

                    if (readableArgumentType && isAssignmentLikeCompatible(referenceType->referredType, readableArgumentType))
                        return referenceType->isMutable ? 2 : 1;

                    return std::nullopt;
                }

                if (isAssignmentLikeCompatible(parameterType, rawArgumentType))
                    return 4;
                if (readableArgumentType && isAssignmentLikeCompatible(parameterType, readableArgumentType))
                    return 2;
                return std::nullopt;
            };

            auto scoreFitReturnAgainstTarget = [&](const Ref<Type>& targetType, const Ref<Type>& returnType) -> std::optional<int>
            {
                if (!targetType || !returnType)
                    return std::nullopt;

                if (isExactType(targetType, returnType))
                    return 4;
                if (isAssignmentLikeCompatible(targetType, returnType))
                    return 2;
                return std::nullopt;
            };

            std::vector<OperatorCandidate> candidates;

            auto appendMemberCandidates = [&](const Ref<Type>& candidateReceiverType)
            {
                Ref<Type> currentType = unwrapAliasType(candidateReceiverType);
                while (currentType && currentType->kind() == TypeKind::Reference)
                    currentType = unwrapAliasType(currentType.AsFast<ReferenceType>()->referredType);

                if (!currentType || currentType->kind() != TypeKind::Struct)
                    return;

                Ref<Type> ownerType = nullptr;
                Ref<Symbol> candidateSymbol = findStructMemberInHierarchy(currentType, std::string(*overloadName), &ownerType);
                if (!candidateSymbol)
                    return;

                if (!validateStructMemberAccess(currentStructType_, ownerType, candidateSymbol, node.location()))
                {
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }

                auto ownerStructType = ownerType ? unwrapAliasType(ownerType).AsFast<StructType>() : nullptr;
                if (!ownerStructType)
                    return;

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceGenericBindings(candidateFunctionType->returnType, destType, genericBindings))
                            continue;

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || !candidateFunctionType->paramTypes.empty())
                            continue;
                    }

                    auto returnScore = scoreFitReturnAgainstTarget(destType, candidateFunctionType->returnType);
                    if (!returnScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = ownerStructType,
                        .dispatchKind = OperatorDispatchKind::Member,
                        .score = *returnScore + 2
                    });
                }
            };

            appendMemberCandidates(srcType);
            Ref<Type> readableSrcType = getAutoReadableType(srcType);
            if (readableSrcType != srcType)
                appendMemberCandidates(readableSrcType);

            std::unordered_set<const Symbol*> seenFreeSymbols;
            auto appendFreeSymbol = [&](const Ref<Symbol>& candidateSymbol)
            {
                if (!candidateSymbol || seenFreeSymbols.contains(candidateSymbol.Get()))
                    return;

                seenFreeSymbols.insert(candidateSymbol.Get());

                std::vector<Ref<Symbol>> overloads;
                if (candidateSymbol->kind == SymbolKind::FunctionGroup)
                    overloads = candidateSymbol->overloads;
                else if (candidateSymbol->kind == SymbolKind::Function)
                    overloads.push_back(candidateSymbol);
                else
                    return;

                for (const auto& overload : overloads)
                {
                    if (!overload || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    Ref<Type> candidateType = overload->type;
                    std::unordered_map<std::string, Ref<Type>> genericBindings;
                    auto candidateFunctionType = candidateType.AsFast<FunctionType>();
                    if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                        continue;

                    if (!overload->genericParameterNames.empty())
                    {
                        if (!deduceBindingsFromFitArgument(candidateFunctionType->paramTypes[0], srcType, node.operand, genericBindings) ||
                            !deduceGenericBindings(candidateFunctionType->returnType, destType, genericBindings))
                        {
                            continue;
                        }

                        candidateType = instantiateGenericType(candidateType, genericBindings);
                        candidateFunctionType = candidateType ? candidateType.AsFast<FunctionType>() : nullptr;
                        if (!candidateFunctionType || candidateFunctionType->paramTypes.size() != 1)
                            continue;
                    }

                    auto argumentScore = scoreFitArgumentAgainstParameter(
                        candidateFunctionType->paramTypes[0],
                        srcType,
                        getAutoReadableType(srcType),
                        node.operand
                    );
                    if (!argumentScore.has_value())
                        continue;

                    auto returnScore = scoreFitReturnAgainstTarget(destType, candidateFunctionType->returnType);
                    if (!returnScore.has_value())
                        continue;

                    candidates.push_back(OperatorCandidate{
                        .symbol = overload,
                        .functionType = candidateFunctionType,
                        .ownerType = nullptr,
                        .dispatchKind = OperatorDispatchKind::Free,
                        .score = *argumentScore + *returnScore
                    });
                }
            };

            appendFreeSymbol(currentScope_ ? currentScope_->resolve(std::string(*overloadName)) : nullptr);
            if (Ref<Scope> globalScope = scopes_.empty() ? nullptr : scopes_.front())
            {
                auto appendAssociatedScopeSymbol = [&](const Ref<Type>& type)
                {
                    Ref<Type> associatedType = unwrapAliasType(type);
                    while (associatedType && associatedType->kind() == TypeKind::Reference)
                        associatedType = unwrapAliasType(associatedType.AsFast<ReferenceType>()->referredType);

                    if (!associatedType || associatedType->kind() != TypeKind::Struct)
                        return;

                    auto structType = associatedType.AsFast<StructType>();
                    if (!structType)
                        return;

                    std::string qualifiedName = structType->scopePath.empty()
                        ? std::string(*overloadName)
                        : structType->scopePath + "::" + std::string(*overloadName);
                    appendFreeSymbol(resolveQualifiedSymbol(globalScope, qualifiedName));
                };

                appendAssociatedScopeSymbol(srcType);
                appendAssociatedScopeSymbol(readableSrcType);
                appendAssociatedScopeSymbol(destType);
            }

            std::optional<OperatorCandidate> bestCandidate;
            bool isAmbiguous = false;
            for (const auto& candidate : candidates)
            {
                if (!bestCandidate.has_value() || candidate.score > bestCandidate->score)
                {
                    bestCandidate = candidate;
                    isAmbiguous = false;
                }
                else if (candidate.score == bestCandidate->score)
                {
                    isAmbiguous = true;
                }
            }

            if (!bestCandidate.has_value())
                return false;

            if (isAmbiguous)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Ambiguous overload for operator 'fit' from '{}' to '{}'.",
                    srcType ? srcType->toString() : "<unknown>",
                    destType ? destType->toString() : "<unknown>"
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return true;
            }

            node.referencedSymbol = bestCandidate->symbol;
            node.operatorDispatchKind = bestCandidate->dispatchKind;
            node.overloadFunctionType = bestCandidate->functionType.AsFast<Type>();
            node.refType = bestCandidate->functionType
                ? bestCandidate->functionType->returnType
                : Compiler::get().getTypeContext().getUnknown();
            return true;
        };

        if (tryResolveFitOperatorOverload())
            return;

        struct GenericFitConstraintInfo
        {
            bool isKnown = false;
            bool hasApplicableConstraints = false;
            bool isIncompatible = false;
            bool allowsNumeric = false;
            bool allowsObjectLike = false;
        };

        auto resolveGenericFitConstraintInfo = [&](const Ref<Type>& candidateType) -> GenericFitConstraintInfo
        {
            GenericFitConstraintInfo info;

            Ref<Type> resolvedCandidateType = unwrapAliasType(candidateType);
            if (!resolvedCandidateType || resolvedCandidateType->kind() != TypeKind::GenericParameter)
                return info;

            const std::string& genericParameterName = resolvedCandidateType.AsFast<GenericParameterType>()->name;

            for (auto symbolIt = activeGenericConstraintSymbols_.rbegin();
                 symbolIt != activeGenericConstraintSymbols_.rend();
                 ++symbolIt)
            {
                const Ref<Symbol>& activeSymbol = *symbolIt;
                if (!activeSymbol)
                    continue;

                auto genericParameterIt = std::ranges::find(activeSymbol->genericParameterNames, genericParameterName);
                if (genericParameterIt == activeSymbol->genericParameterNames.end())
                    continue;

                auto attributeIt = attributeListsBySymbol_.find(activeSymbol.Get());
                if (attributeIt == attributeListsBySymbol_.end() || !attributeIt->second)
                    return {};

                const size_t genericParameterIndex = static_cast<size_t>(
                    std::distance(activeSymbol->genericParameterNames.begin(), genericParameterIt)
                );
                const auto applyAttributes = getAttributeStatements(*attributeIt->second, Attribute::Apply);
                if (applyAttributes.empty())
                    return {};

                bool sawApplicableConstraint = false;
                bool allConstraintsAreFitCompatible = true;
                auto& typeContext = Compiler::get().getTypeContext();

                for (const auto* applyAttribute : applyAttributes)
                {
                    if (!applyAttribute)
                        continue;

                    if (activeSymbol->genericParameterNames.size() != 1 &&
                        applyAttribute->args.size() != activeSymbol->genericParameterNames.size())
                        continue;

                    const auto constraintArguments =
                        getApplyConstraintArguments(*applyAttribute, activeSymbol->genericParameterNames, genericParameterIndex);
                    if (constraintArguments.empty())
                        continue;

                    sawApplicableConstraint = true;
                    bool attributeAllowsNumeric = false;
                    bool attributeAllowsObjectLike = false;
                    bool attributeIsCompatible = true;

                    for (const auto& argument : constraintArguments)
                    {
                        if (argument.typeSpecifier)
                        {
                            if (const auto* predicateTrait =
                                    findGenericConstraintTraitDescriptor(argument.typeSpecifier->name.value,
                                                                         argument.typeSpecifier->refType.Lock()))
                            {
                                if (predicateTrait->kind == GenericConstraintTraitKind::IsInteger ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsNumeric ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsFloating ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsSigned ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsUnsigned)
                                {
                                    attributeAllowsNumeric = true;
                                    continue;
                                }

                                if (predicateTrait->kind == GenericConstraintTraitKind::IsObject ||
                                    predicateTrait->kind == GenericConstraintTraitKind::IsInterface)
                                {
                                    attributeAllowsObjectLike = true;
                                    continue;
                                }

                                attributeIsCompatible = false;
                                break;
                            }

                            Ref<Type> exactType = unwrapAliasType(argument.typeSpecifier->refType.Lock());
                            if (!exactType || exactType->isUnknown())
                            {
                                attributeIsCompatible = false;
                                break;
                            }

                            if (isNumericConstraintType(exactType))
                            {
                                attributeAllowsNumeric = true;
                                continue;
                            }

                            if (getObjectOrInterfaceStructType(exactType) || isExactType(exactType, typeContext.getObject()))
                            {
                                attributeAllowsObjectLike = true;
                                continue;
                            }
                        }

                        attributeIsCompatible = false;
                        break;
                    }

                    if (!attributeIsCompatible || (attributeAllowsNumeric && attributeAllowsObjectLike))
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if (!attributeAllowsNumeric && !attributeAllowsObjectLike)
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if ((info.allowsNumeric && attributeAllowsObjectLike) ||
                        (info.allowsObjectLike && attributeAllowsNumeric))
                    {
                        allConstraintsAreFitCompatible = false;
                        break;
                    }

                    if (attributeAllowsNumeric)
                        info.allowsNumeric = true;
                    if (attributeAllowsObjectLike)
                        info.allowsObjectLike = true;
                }

                if (!sawApplicableConstraint || !allConstraintsAreFitCompatible)
                {
                    info.hasApplicableConstraints = sawApplicableConstraint;
                    info.isIncompatible = sawApplicableConstraint;
                    return info;
                }

                info.hasApplicableConstraints = true;
                info.isKnown = info.allowsNumeric || info.allowsObjectLike;
                info.isIncompatible = !info.isKnown;
                return info;
            }

            return info;
        };

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

            if (isAnyType(resolvedSrcType))
            {
                if (isSupportedAnyCastTargetType(resolvedDestType))
                {
                    node.refType = destType;
                    return;
                }

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "The 'fit' operator can only convert 'any' to concrete runtime-storable types or interfaces."
                );
                node.refType = Compiler::get().getTypeContext().getUnknown();
                return;
            }

            if (resolvedDestType && resolvedDestType->kind() == TypeKind::GenericParameter)
            {
                const GenericFitConstraintInfo constraintInfo = resolveGenericFitConstraintInfo(resolvedDestType);
                const bool sourceIsNumeric = resolvedSrcType && resolvedSrcType->isNumeric();
                const bool sourceIsObjectLike = getObjectOrInterfaceStructType(srcType) != nullptr;
                const bool isNumericFit =
                    sourceIsNumeric &&
                    (!constraintInfo.hasApplicableConstraints || constraintInfo.allowsNumeric);
                const bool isObjectLikeFit =
                    sourceIsObjectLike &&
                    (!constraintInfo.hasApplicableConstraints || constraintInfo.allowsObjectLike);

                if (isNumericFit || isObjectLikeFit)
                {
                    node.refType = destType;
                    return;
                }

                if ((sourceIsNumeric || sourceIsObjectLike) && constraintInfo.isIncompatible)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "The 'fit' target generic parameter '{}' does not allow {} conversions.",
                        resolvedDestType.AsFast<GenericParameterType>()->name,
                        sourceIsNumeric ? "numeric" : "object/interface"
                    );
                    node.refType = Compiler::get().getTypeContext().getUnknown();
                    return;
                }
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
        
        if (!currentStructType_ && !currentExtensionTargetType_) {
            WIO_LOG_ADD_ERROR(node.location(), "'self' can only be used inside a component or object method.");
            node.refType = Compiler::get().getTypeContext().getUnknown();
            return;
        }
        
        Ref<Type> selfType = currentExtensionTargetType_ ? currentExtensionTargetType_ : currentStructType_;
        node.refType = Compiler::get().getTypeContext().getOrCreateReferenceType(
            selfType, currentExtensionTargetType_ ? currentExtensionMutableReceiver_ : true);
        node.borrowOrigin = BorrowOrigin::Caller;
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
        
        if (sType && eType && (!allowsNumericSemantics(sType) || !allowsNumericSemantics(eType)))
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
                    if (!allowsNumericSemantics(matchValueType))
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
        if (hasAttribute(node.attributes, Attribute::Specialize) &&
            (isDeclarationPass_ || currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");
        }

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

              if (node.isPackField)
              {
                  if (!declaredType || declaredType->kind() != TypeKind::GenericParameterPack)
                  {
                      WIO_LOG_ADD_ERROR(node.location(), "Pack fields must declare a trailing generic pack type such as 'pack values: Args...;'.");
                      declaredType = Compiler::get().getTypeContext().getUnknown();
                  }
                  else
                  {
                      declaredType = Compiler::get().getTypeContext().getOrCreatePackStorageType(
                          declaredType.AsFast<GenericParameterPackType>()->name
                      );
                  }
              }

              SymbolFlags flags = SymbolFlags::createAllFalse();
              if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (node.mutability == Mutability::Const) flags.set_isConst(true);
            if (currentScope_->getKind() == ScopeKind::Global) flags.set_isGlobal(true);
    
              Ref<Symbol> sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
              currentScope_->define(node.name->token.value, sym);
              node.name->referencedSymbol = sym;
              variableDeclarationsBySymbol_[sym.Get()] = &node;

              return;
          }
    
        Ref<Symbol> sym = node.name->referencedSymbol.Lock();
        if (sym &&
            (currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            Ref<Symbol> localSymbol = currentScope_->resolveLocally(node.name->token.value);
            if (localSymbol.Get() != sym.Get())
                sym = nullptr;
        }
        
        if (!sym)
        {
              Ref<Type> declaredType = nullptr;
              if (node.type)
              {
                  node.type->accept(*this);
                  declaredType = node.type->refType.Lock();
              }

              if (node.isPackField)
              {
                  if (!declaredType || declaredType->kind() != TypeKind::GenericParameterPack)
                  {
                      WIO_LOG_ADD_ERROR(node.location(), "Pack fields must declare a trailing generic pack type such as 'pack values: Args...;'.");
                      declaredType = Compiler::get().getTypeContext().getUnknown();
                  }
                  else
                  {
                      declaredType = Compiler::get().getTypeContext().getOrCreatePackStorageType(
                          declaredType.AsFast<GenericParameterPackType>()->name
                      );
                  }
              }

              SymbolFlags flags = SymbolFlags::createAllFalse();
            if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
            if (node.mutability == Mutability::Const) flags.set_isConst(true);

                sym = createSymbol(node.name->token.value, declaredType, SymbolKind::Variable, node.location(), flags);
                currentScope_->define(node.name->token.value, sym);
                node.name->referencedSymbol = sym;
                variableDeclarationsBySymbol_[sym.Get()] = &node;
            }
            else if (sym->type && containsGenericParameterType(sym->type))
            {
              GenericBindingSet bindingSet;
              for (const auto& genericScope : genericTypeParameterScopes_)
              {
                  for (const auto& [name, boundType] : genericScope)
                      bindingSet.directBindings.insert_or_assign(name, boundType);
              }

              Ref<Type> instantiatedType = instantiateGenericType(sym->type, bindingSet);
              if (instantiatedType && !instantiatedType->isUnknown() && !containsGenericParameterType(instantiatedType))
              {
                  SymbolFlags flags = SymbolFlags::createAllFalse();
                  if (node.mutability == Mutability::Mutable) flags.set_isMutable(true);
                  if (node.mutability == Mutability::Const) flags.set_isConst(true);

                    sym = createSymbol(node.name->token.value, instantiatedType, SymbolKind::Variable, node.location(), flags);
                    currentScope_->define(node.name->token.value, sym);
                    node.name->referencedSymbol = sym;
                    node.name->refType = instantiatedType;
                    variableDeclarationsBySymbol_[sym.Get()] = &node;
                }
                else if (!currentScope_->resolveLocally(node.name->token.value))
                {
                  currentScope_->define(node.name->token.value, sym);
              }
          }
          else if (!currentScope_->resolveLocally(node.name->token.value))
            {
                currentScope_->define(node.name->token.value, sym);
            }

            if (sym)
                variableDeclarationsBySymbol_[sym.Get()] = &node;

          if (node.initializer)
          {
            auto shouldAutoReadInferredInitializer = [&](const NodePtr<Expression>& initializer,
                                                         const Ref<Type>& initializerType) -> bool
            {
                if (!initializer || !shouldAutoReadReferenceType(initializerType))
                    return false;

                if (initializer->is<RefExpression>())
                    return false;

                if (const auto* unary = initializer->as<UnaryExpression>())
                {
                    if (unary->op.type == TokenType::kwDeref)
                        return false;
                }

                return true;
            };

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = sym->type;
            allowContextualNumericLiteralTyping_ = true;
            node.initializer->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;
            Ref<Type> initType = node.initializer->refType.Lock();

            Ref<Type> resolvedDeclaredType = unwrapAliasType(sym->type);
            Ref<Type> resolvedInitializerType = unwrapAliasType(initType);
            if (resolvedDeclaredType && resolvedInitializerType &&
                resolvedDeclaredType->kind() == TypeKind::Reference &&
                resolvedInitializerType->kind() == TypeKind::Reference &&
                classifyBorrowOrigin(node.initializer) == BorrowOrigin::Temporary)
            {
                WIO_LOG_ADD_ERROR(
                    node.initializer->location(),
                    "Cannot store a reference borrowed from a temporary value; the borrow would outlive its owner."
                );
            }

            if (!sym->type || sym->type->isUnknown()) 
            {
                if (resolvedInitializerType && resolvedInitializerType->kind() == TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Cannot infer a variable type from null. Add an explicit nullable type."
                    );
                    sym->type = Compiler::get().getTypeContext().getUnknown();
                    node.name->refType = sym->type;
                    return;
                }

                Ref<Type> inferredType = initType;
                if (shouldAutoReadInferredInitializer(node.initializer, initType))
                    inferredType = getAutoReadableType(initType);

                sym->type = inferredType;
                node.name->refType = inferredType;
            }
            else if (initType && !initType->isUnknown() && !isAssignmentLikeCompatible(sym->type, initType)) 
            {
                if (isRejectedImplicitNumericConversion(sym->type, initType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Implicit narrowing conversion from '{}' to '{}' requires explicit 'fit'.",
                        initType->toString(), sym->type->toString());
                }
                else if (resolvedInitializerType && resolvedInitializerType->kind() == TypeKind::Null)
                {
                    WIO_LOG_ADD_ERROR(
                        node.initializer->location(),
                        "Type '{}' cannot be initialized with null.",
                        sym->type->toString()
                    );
                }
                else
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Type mismatch for '{}'.", node.name->token.value);
                }
            }

            if (node.mutability == Mutability::Const)
            {
                if (!isConstScalarType(sym->type))
                {
                    const std::string actualType = sym->type ? sym->type->toString() : "<unknown>";
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Const declarations currently support primitive scalar types plus enum/flagset values. Got '{}'.",
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
        else if (sym &&
                 currentScope_->getKind() != ScopeKind::Struct &&
                 requiresExplicitNonNullInitialization(sym->type))
        {
            WIO_LOG_ADD_ERROR(
                node.location(),
                "Non-null type '{}' requires an initializer. Use '{}?' if an empty state is required.",
                sym->type->toString(),
                sym->type->toString()
            );
        }
    }

    void SemanticAnalyzer::visit(TypeAliasDeclaration& node)
    {
        if (hasAttribute(node.attributes, Attribute::Specialize) &&
            (isDeclarationPass_ || currentScope_->getKind() == ScopeKind::Function || currentScope_->getKind() == ScopeKind::Block))
        {
            WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");
        }

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this type alias.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
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

        auto genericParameterDefaults = resolveGenericParameterDefaults(
            *this, node.genericParameters, node.hasGenericParameterPack, "type alias");

        validateApplyAttributes(*this, node.attributes, genericParameterNames, "type alias", node.location());

        node.aliasedType->accept(*this);
        Ref<Type> aliasedType = node.aliasedType->refType.Lock();
        Ref<Type> aliasType = Compiler::get().getTypeContext().getOrCreateAliasType(node.name->token.value, aliasedType);
        Ref<Symbol> aliasSym = createSymbol(node.name->token.value, aliasType, SymbolKind::TypeAlias, node.location());
        aliasSym->aliasTargetType = aliasedType;
        aliasSym->genericParameterNames = genericParameterNames;
        aliasSym->genericParameterDefaults = std::move(genericParameterDefaults);
        aliasSym->hasGenericParameterPack = node.hasGenericParameterPack;

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

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this function.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();

                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Specialize is supported only on generic object and component declarations."
                );
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);

            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "function");

            Ref<Type> returnType = Compiler::get().getTypeContext().getVoid();
            if (node.returnType)
            {
                node.returnType->accept(*this);
                returnType = node.returnType->refType.Lock();
            }

            std::vector<Ref<Type>> paramTypes;
            bool hasParameterPack = false;
            for (auto& param : node.parameters)
            {
                Ref<Type> pType = Compiler::get().getTypeContext().getUnknown();
                if (param.type)
                {
                    param.type->accept(*this);
                    pType = param.type->refType.Lock();
                }
                hasParameterPack = hasParameterPack || param.isParameterPack;
                paramTypes.push_back(pType);
            }

            auto funcType = Compiler::get().getTypeContext().getOrCreateFunctionType(returnType, paramTypes, hasParameterPack);
            
            Ref<Symbol> funcSym = createSymbol(node.name->token.value, funcType, SymbolKind::Function, node.location());
            if (node.isExtensionMethod)
            {
                funcSym->flags.set_isExtension(true);
                funcSym->extensionTargetType = currentExtensionTargetType_;
                funcSym->extensionMemberName = node.extensionMemberName;
                node.extensionTargetType = currentExtensionTargetType_;
            }
            funcSym->innerScope = currentScope_;
            funcSym->genericParameterNames.reserve(node.genericParameters.size());
            for (const auto& genericParameter : node.genericParameters)
            {
                if (genericParameter)
                    funcSym->genericParameterNames.push_back(genericParameter->token.value);
            }
            funcSym->hasGenericParameterPack = node.hasGenericParameterPack;
            funcSym->genericParameterDefaults = std::move(genericParameterDefaults);
            currentScope_->define(node.name->token.value, funcSym);
            functionDeclarationsBySymbol_[funcSym.Get()] = &node;
            attributeListsBySymbol_[funcSym.Get()] = &node.attributes;
            if (!funcSym->genericParameterNames.empty())
            {
                validateApplyAttributes(*this, node.attributes, funcSym->genericParameterNames, "function", node.location());
                funcSym->resolvedGenericInstantiations = resolveInstantiateAttributes(
                    *this,
                    node.attributes,
                    funcSym->genericParameterNames,
                    funcSym->hasGenericParameterPack
                );
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
        auto currentStruct = currentStructType_ ? currentStructType_.AsFast<StructType>() : nullptr;
        const bool isLifecycleMethod =
            node.name->token.value == "OnConstruct" || node.name->token.value == "OnDestruct";
        const bool isOperatorMethod = common::isOperatorOverloadName(node.name->token.value);
        const bool isUnaryOperatorMethod = common::isUnaryOperatorOverloadName(node.name->token.value);
        const bool isConversionOperatorMethod = common::isConversionOperatorOverloadName(node.name->token.value);
        const bool isIndexOperatorMethod = common::isIndexOperatorOverloadName(node.name->token.value);
        const bool isCallOperatorMethod = common::isCallOperatorOverloadName(node.name->token.value);
        const bool isComponentMethodContext = currentStruct && !currentStruct->isObject && !currentStruct->isInterface;
        bool hasInstantiate = hasAttribute(node.attributes, Attribute::Instantiate);
        const bool hasFunctionParameterPack = std::ranges::any_of(node.parameters, [](const Parameter& parameter)
        {
            return parameter.isParameterPack;
        });
        const bool isPackFunction = node.hasGenericParameterPack || hasFunctionParameterPack;

        if (isLifecycleMethod && funcType->returnType && !funcType->returnType->isVoid())
        {
            WIO_LOG_ADD_ERROR(node.location(), "{} must return void.", node.name->token.value);
        }

        if (node.name->token.value == "OnDestruct")
        {
            if (!node.parameters.empty())
                WIO_LOG_ADD_ERROR(node.location(), "OnDestruct must not declare parameters.");
            if (node.whenCondition || node.whenFallback)
                WIO_LOG_ADD_ERROR(node.location(), "OnDestruct does not support when/else clauses.");
        }

        if (isNative)
        {
            Ref<Type> nativeReturnType = unwrapAliasType(funcType->returnType);
            if (nativeReturnType && nativeReturnType->kind() == TypeKind::Reference)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Native functions cannot return ref/view values because the native borrow lifetime cannot be proven. Return an owning value or handle instead."
                );
            }
        }
        std::string activeFunctionPackName = node.hasGenericParameterPack && !node.genericParameters.empty()
            ? node.genericParameters.back()->token.value
            : "";
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

        if (isOperatorMethod)
        {
            if (node.whenCondition || node.whenFallback)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads do not support when/else clauses.");
            }

            if (hasFunctionParameterPack || node.hasGenericParameterPack)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads cannot use parameter packs.");
            }

            const bool isAssignmentOperator = common::isAssignmentOperatorOverloadName(node.name->token.value);
            if (isCallOperatorMethod && !isStructMethod)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Call operator overloads must be declared as member functions."
                );
            }
            else if (!isCallOperatorMethod)
            {
                const size_t expectedParameterCount = isStructMethod
                    ? (isUnaryOperatorMethod || isConversionOperatorMethod ? 0u : 1u)
                    : (isUnaryOperatorMethod || isConversionOperatorMethod ? 1u : 2u);
                if (node.parameters.size() != expectedParameterCount)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        isStructMethod
                            ? (isUnaryOperatorMethod
                                ? "Member unary operator overloads must declare zero parameters."
                                : (isConversionOperatorMethod
                                    ? "Member conversion operator overloads must declare zero parameters."
                                    : (isIndexOperatorMethod
                                        ? "Member subscript operator overloads must declare exactly one parameter."
                                        : (isAssignmentOperator
                                            ? "Member assignment operator overloads must declare exactly one parameter."
                                            : "Member binary operator overloads must declare exactly one parameter."))))
                            : (isUnaryOperatorMethod
                                ? "Free unary operator overloads must declare exactly one parameter."
                                : (isConversionOperatorMethod
                                    ? "Free conversion operator overloads must declare exactly one parameter."
                                    : (isIndexOperatorMethod
                                        ? "Free subscript operator overloads must declare exactly two parameters."
                                        : (isAssignmentOperator
                                            ? "Free assignment operator overloads must declare exactly two parameters."
                                            : "Free binary operator overloads must declare exactly two parameters."))))
                    );
                }
            }

            for (const auto& parameter : node.parameters)
            {
                if (parameter.defaultValue)
                {
                    WIO_LOG_ADD_ERROR(parameter.name->location(), "Operator overload parameters cannot declare default values.");
                }
            }

            if (!funcType->returnType || isExactType(funcType->returnType, Compiler::get().getTypeContext().getVoid()))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Operator overloads must return a value.");
            }

            const bool requiresBoolReturn =
                node.name->token.value == "__op_equal" ||
                node.name->token.value == "__op_not_equal" ||
                node.name->token.value == "__op_less" ||
                node.name->token.value == "__op_less_equal" ||
                node.name->token.value == "__op_greater" ||
                node.name->token.value == "__op_greater_equal" ||
                node.name->token.value == "__op_logical_not";
            if (requiresBoolReturn &&
                !isExactType(funcType->returnType, Compiler::get().getTypeContext().getBool()))
            {
                auto operatorDisplay = common::getOperatorDisplayText(node.name->token.value);
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Operator '{}' must return bool.",
                    operatorDisplay.has_value() ? std::string(*operatorDisplay) : node.name->token.value
                );
            }
        }

        if (isGenericFunction)
        {
            if (isPackFunction && isStructMethod)
            {
                WIO_LOG_ADD_ERROR(node.location(), "Generic parameter packs are currently supported only on top-level functions.");
            }

            if (isStructMethod)
            {
                if (!isOperatorMethod && (!currentStruct || !currentStruct->isObject))
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic methods are currently supported only on object methods.");
                }
                else if (!isOperatorMethod && (node.name->token.value == "OnConstruct" || node.name->token.value == "OnDestruct"))
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

            const bool allowsOpenNativeTemplateByConstraint =
                isNative && !isExported &&
                (isPackFunction || hasApply || isOpenNativeTemplateIntrinsic(node.attributes));

            if ((isNative || isExported) && !hasInstantiate && !allowsOpenNativeTemplateByConstraint)
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

        if (isPackFunction)
        {
            if (!activeFunctionPackName.empty())
            {
                // already declared on the function itself
            }
            else
            {
                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& parameter = node.parameters[i];
                    Ref<Type> parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : Compiler::get().getTypeContext().getUnknown();
                    std::string containedPackName;
                    const bool containsPack = containsGenericParameterPackType(parameterType, &containedPackName);
                    if (parameter.isParameterPack && containsPack)
                    {
                        if (activeFunctionPackName.empty())
                            activeFunctionPackName = containedPackName;
                        else if (activeFunctionPackName != containedPackName)
                            WIO_LOG_ADD_ERROR(parameter.name->location(), "Function parameter packs must reference a single trailing generic pack.");
                    }
                }
            }

            if (activeFunctionPackName.empty())
                WIO_LOG_ADD_ERROR(node.location(), "Function parameter packs require a matching trailing generic parameter pack.");

            if (isCommand || isEvent || hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic pack functions currently support interop only through @Native and @Export. @Command, @Event, and module lifecycle attributes are not allowed."
                );
            }

            if (isExported && !hasInstantiate)
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Generic @Export pack functions must declare at least one concrete @Instantiate(...)."
                );
            }

            if (node.whenCondition || node.whenFallback)
            {
                WIO_LOG_ADD_ERROR(node.location(), "when/else clauses are not supported on generic pack functions yet.");
            }

            if (node.returnType)
            {
                std::string returnPackName;
                if (containsGenericParameterPackType(funcType->returnType, &returnPackName) &&
                    (!activeFunctionPackName.empty() ? returnPackName != activeFunctionPackName : !returnPackName.empty()))
                {
                    WIO_LOG_ADD_ERROR(node.returnType->location(), "Generic parameter packs in function return positions must match the active trailing generic pack.");
                }
            }

            if (!activeFunctionPackName.empty())
            {
                const std::string& expectedPackName = activeFunctionPackName;
                bool sawParameterPack = false;

                for (size_t i = 0; i < node.parameters.size(); ++i)
                {
                    auto& parameter = node.parameters[i];
                    Ref<Type> parameterType = i < funcType->paramTypes.size() ? funcType->paramTypes[i] : Compiler::get().getTypeContext().getUnknown();
                    std::string containedPackName;
                    const bool containsPack = containsGenericParameterPackType(parameterType, &containedPackName);

                    if (parameter.isParameterPack)
                    {
                        sawParameterPack = true;

                        if (i + 1 != node.parameters.size())
                        {
                            WIO_LOG_ADD_ERROR(parameter.name->location(), "Function parameter packs must be trailing.");
                        }

                        if (!containsPack || containedPackName != expectedPackName)
                        {
                            WIO_LOG_ADD_ERROR(
                                parameter.name->location(),
                                "Function parameter pack '{}' requires the trailing generic pack '{}...'.",
                                parameter.name->token.value,
                                expectedPackName
                            );
                        }
                    }
                    else if (containsPack)
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter.name->location(),
                            "Generic pack parameter '{}...' is currently supported only in the trailing function parameter position.",
                            containedPackName
                        );
                    }

                    if (parameter.isParameterPack && parameter.defaultValue)
                    {
                        WIO_LOG_ADD_ERROR(
                            parameter.name->location(),
                            "Parameter pack '{}' cannot declare a default value. Default values apply only to fixed parameters before the trailing pack.",
                            parameter.name->token.value
                        );
                    }
                }

                if (!sawParameterPack && !node.parameters.empty())
                {
                    WIO_LOG_ADD_ERROR(node.location(), "Generic parameter pack '{}...' requires a matching trailing function parameter pack.", expectedPackName);
                }
            }
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

            if (isStructMethod)
            {
                if (!currentStruct || currentStruct->isInterface)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions."
                    );
                }
                else if (isComponentMethodContext && !isLifecycleMethod && !isOperatorMethod)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native methods are currently supported only on object methods and on object/component OnConstruct/OnDestruct lifecycle functions."
                    );
                }
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

        if (isNative)
        {
            auto validateNativeComponentInteropType = [&](const Ref<Type>& nativeType, std::string_view role, std::string_view displayName)
            {
                auto nativeComponent = getNativePodComponentStructType(nativeType);
                if (!nativeComponent)
                    return;

                validateInstantiatedNativePodComponent(nativeComponent, node.location());

                if (nativeComponent->nativeCppName.empty())
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Native {} '{}' uses component '{}' directly, but that component is missing a declaration-level @CppName for its native C++ POD type.",
                        role,
                        displayName,
                        nativeComponent->name
                    );
                }
            };

            for (size_t parameterIndex = 0; parameterIndex < funcType->paramTypes.size(); ++parameterIndex)
            {
                const std::string parameterName =
                    parameterIndex < node.parameters.size() && node.parameters[parameterIndex].name
                        ? node.parameters[parameterIndex].name->token.value
                        : common::formatString("param{}", parameterIndex);
                validateNativeComponentInteropType(funcType->paramTypes[parameterIndex], "parameter", parameterName);
            }

            if (funcType->returnType && !funcType->returnType->isVoid())
                validateNativeComponentInteropType(funcType->returnType, "return type of", node.name->token.value);
        }

        Ref<Type> prevRetType = currentFunctionReturnType_;
        Ref<Symbol> prevFunctionParameterPackSymbol = currentFunctionParameterPackSymbol_;
        Ref<Type> prevFunctionParameterPackType = currentFunctionParameterPackType_;
        currentFunctionReturnType_ = funcType->returnType;
        currentFunctionParameterPackSymbol_ = nullptr;
        currentFunctionParameterPackType_ = nullptr;

        auto genericScope = buildGenericTypeParameterScope();
        genericTypeParameterScopes_.push_back(genericScope);
        if (!funcSym->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(funcSym);

        bool sawDefaultParameter = false;
        for (size_t i = 0; i < node.parameters.size(); ++i)
        {
            auto& param = node.parameters[i];
            if (!param.defaultValue)
            {
                if (sawDefaultParameter)
                {
                    const bool isTrailingParameterPack =
                        param.isParameterPack &&
                        i + 1 == node.parameters.size() &&
                        funcType->hasParameterPack;
                    if (!isTrailingParameterPack)
                    {
                        WIO_LOG_ADD_ERROR(
                            param.name ? param.name->location() : node.location(),
                            "Parameters with default values must be trailing."
                        );
                    }
                }
                continue;
            }

            sawDefaultParameter = true;

            if (!param.type || !funcType->paramTypes[i] || funcType->paramTypes[i]->isUnknown())
            {
                WIO_LOG_ADD_ERROR(
                    param.name ? param.name->location() : param.defaultValue->location(),
                    "Default parameters require an explicit parameter type."
                );
                continue;
            }

            if (node.name->token.value == "Entry")
            {
                WIO_LOG_ADD_ERROR(param.defaultValue->location(), "Entry cannot declare default parameters.");
            }

            if (hasModuleLifecycle)
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "{} cannot declare default parameters.",
                    getModuleLifecycleAttributeName(moduleLifecycleAttributes.front())
                );
            }

            if (!node.body && !isNative)
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "Default parameters are currently supported only on functions with Wio bodies or on @Native declarations."
                );
            }

            if (param.isParameterPack)
            {
                continue;
            }

            Ref<Type> previousExpectedExpressionType = currentExpectedExpressionType_;
            bool previousAllowContextualNumericLiteralTyping = allowContextualNumericLiteralTyping_;
            currentExpectedExpressionType_ = funcType->paramTypes[i];
            allowContextualNumericLiteralTyping_ = true;
            param.defaultValue->accept(*this);
            currentExpectedExpressionType_ = previousExpectedExpressionType;
            allowContextualNumericLiteralTyping_ = previousAllowContextualNumericLiteralTyping;

            Ref<Type> defaultType = param.defaultValue->refType.Lock();
            if (funcType->paramTypes[i] &&
                !funcType->paramTypes[i]->isUnknown() &&
                defaultType &&
                !defaultType->isUnknown() &&
                !isAssignmentLikeCompatible(funcType->paramTypes[i], defaultType))
            {
                WIO_LOG_ADD_ERROR(
                    param.defaultValue->location(),
                    "Default argument type mismatch for parameter '{}'. Expected '{}', but got '{}'.",
                    param.name ? param.name->token.value : "<parameter>",
                    funcType->paramTypes[i]->toString(),
                    defaultType->toString()
                );
            }
        }

        if (auto overloadSymbol = currentScope_ ? currentScope_->resolve(node.name->token.value) : nullptr;
            overloadSymbol && overloadSymbol->kind == SymbolKind::FunctionGroup)
        {
            if (!funcType->hasParameterPack)
            {
                const size_t currentRequiredParameterCount = getRequiredParameterCount(node);
                const size_t currentTotalParameterCount = getFixedParameterCount(node);

                for (const auto& overload : overloadSymbol->overloads)
                {
                    if (!overload || overload == funcSym || !overload->type || overload->type->kind() != TypeKind::Function)
                        continue;

                    auto foundDeclaration = functionDeclarationsBySymbol_.find(overload.Get());
                    if (foundDeclaration == functionDeclarationsBySymbol_.end() || !foundDeclaration->second)
                        continue;

                    const auto* otherDeclaration = foundDeclaration->second;
                    auto otherFunctionType = overload->type.AsFast<FunctionType>();
                    if (otherFunctionType && otherFunctionType->hasParameterPack)
                        continue;
                    const size_t otherRequiredParameterCount = getRequiredParameterCount(otherDeclaration);
                    const size_t otherTotalParameterCount = getFixedParameterCount(*otherDeclaration);

                    if (currentRequiredParameterCount != currentTotalParameterCount &&
                        otherTotalParameterCount >= currentRequiredParameterCount &&
                        otherTotalParameterCount < currentTotalParameterCount)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Default parameters on '{}' would synthesize an overload with {} arguments, but that signature is already declared explicitly.",
                            node.name->token.value,
                            otherTotalParameterCount
                        );
                    }

                    if (otherRequiredParameterCount != otherTotalParameterCount &&
                        currentTotalParameterCount >= otherRequiredParameterCount &&
                        currentTotalParameterCount < otherTotalParameterCount)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Function '{}' conflicts with a default-parameter overload that already covers {} arguments.",
                            node.name->token.value,
                            currentTotalParameterCount
                        );
                    }
                }
            }
        }

        if (hasInstantiate)
        {
            for (const auto& instantiationTypes : funcSym->resolvedGenericInstantiations)
            {
                if (isExported)
                {
                    auto instantiationBindings = buildExtendedGenericBindings(
                        funcSym->genericParameterNames,
                        funcSym->hasGenericParameterPack,
                        instantiationTypes
                    );
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
            
            SymbolFlags parameterFlags = SymbolFlags::createAllFalse();
            if (param.isParameterPack)
                parameterFlags.set_isParameterPack(true);

            Ref<Symbol> pSym = createSymbol(param.name->token.value, funcType->paramTypes[i], SymbolKind::Parameter, param.name->location(), parameterFlags);
            currentScope_->define(param.name->token.value, pSym);
            
            param.name->referencedSymbol = pSym;
            param.name->refType = funcType->paramTypes[i];

            if (param.isParameterPack)
            {
                currentFunctionParameterPackSymbol_ = pSym;
                currentFunctionParameterPackType_ = funcType->paramTypes[i];
            }
        }

        if (node.whenCondition)
        {
            node.whenCondition->accept(*this);

            if (auto conditionType = node.whenCondition->refType.Lock();
                !(conditionType == Compiler::get().getTypeContext().getBool() ||
                  allowsNumericSemantics(conditionType) ||
                  (conditionType && (conditionType->kind() == TypeKind::Reference || conditionType->kind() == TypeKind::Null))))
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

        const bool requiresReturnValue = funcType->returnType &&
                                         !funcType->returnType->isUnknown() &&
                                         !funcType->returnType->isVoid();
        const bool allPathsReturn = statementDefinitelyReturns(node.body);

        if (node.body && !isNative && requiresReturnValue && !allPathsReturn)
        {
            WIO_LOG_ADD_ERROR(
                node.name ? node.name->location() : node.location(),
                "Non-void function '{}' must return a value on all control-flow paths.",
                node.name ? node.name->token.value : "<function>"
            );
        }

        exitScope();
        genericTypeParameterScopes_.pop_back();
        if (!funcSym->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentFunctionReturnType_ = prevRetType;
        currentFunctionParameterPackSymbol_ = prevFunctionParameterPackSymbol;
        currentFunctionParameterPackType_ = prevFunctionParameterPackType;
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
                    statement->is<ExtensionDeclaration>() ||
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

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this interface.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
            {
                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "@Specialize is supported only on generic object and component declarations."
                );
            }

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
            interfaceType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "interface");
            validateApplyAttributes(*this, node.attributes, interfaceType.AsFast<StructType>()->genericParameterNames, "interface", node.location());
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> interfaceSym = createSymbol(node.name->token.value, interfaceType, SymbolKind::Struct, node.location());
            interfaceSym->innerScope = interfaceScope;
            interfaceSym->flags.set_isInterface(true);
            interfaceSym->genericParameterNames = interfaceType.AsFast<StructType>()->genericParameterNames;
            interfaceType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            interfaceSym->genericParameterDefaults = std::move(genericParameterDefaults);
            interfaceSym->hasGenericParameterPack = node.hasGenericParameterPack;
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

    void SemanticAnalyzer::visit(ExtensionDeclaration& node)
    {
        if (isDeclarationPass_)
            return;

        if (isStructResolutionPass_)
        {
            node.targetType->accept(*this);
            Ref<Type> targetType = unwrapAliasType(node.targetType->refType.Lock());
            if (!targetType || targetType->kind() != TypeKind::Struct)
            {
                WIO_LOG_ADD_ERROR(node.targetType->location(), "Extension target must be a component type.");
                return;
            }

            auto targetStruct = targetType.AsFast<StructType>();
            if (targetStruct->isObject || targetStruct->isInterface)
            {
                WIO_LOG_ADD_ERROR(node.targetType->location(), "Extensions currently support component types only.");
                return;
            }

            Ref<Type> previousExtensionTarget = currentExtensionTargetType_;
            currentExtensionTargetType_ = targetType;
            const bool previousDeclarationPass = isDeclarationPass_;
            isDeclarationPass_ = true;

            for (auto& member : node.members)
            {
                auto& method = member.method;
                if (!method)
                    continue;
                if (member.access == AccessModifier::Private ||
                    member.access == AccessModifier::Protected)
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension methods are external APIs and must be public.");
                    continue;
                }
                if (!method->genericParameters.empty())
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Generic extension methods are not supported yet.");
                    continue;
                }
                if (common::isOperatorOverloadName(method->extensionMemberName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension operator overloads are not supported.");
                    continue;
                }
                if (std::ranges::any_of(method->parameters, [](const Parameter& parameter)
                    {
                        return parameter.defaultValue != nullptr;
                    }))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension methods do not support default parameters yet.");
                    continue;
                }

                const std::string publicName = method->extensionMemberName;
                if (auto scope = targetStruct->structScope.Lock();
                    scope && scope->resolveLocally(publicName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension method '{}' conflicts with a component member.", publicName);
                    continue;
                }

                method->name->token.value =
                    "__extension_" + node.name->token.value + "_" + publicName;
                currentExtensionMutableReceiver_ = member.mutableReceiver;
                method->accept(*this);

                auto symbol = method->name->referencedSymbol.Lock();
                if (!symbol)
                    continue;
                symbol->flags.set_isExtension(true);
                symbol->extensionTargetType = targetType;
                symbol->extensionMemberName = publicName;
                method->extensionTargetType = targetType;

                auto& methods = extensionMethods_[targetType.Get()];
                if (methods.contains(publicName))
                {
                    WIO_LOG_ADD_ERROR(method->location(),
                        "Extension method '{}' is ambiguous for '{}'.",
                        publicName, targetType->toString());
                }
                else
                {
                    methods.emplace(publicName, symbol);
                }
            }

            isDeclarationPass_ = previousDeclarationPass;
            currentExtensionTargetType_ = previousExtensionTarget;
            currentExtensionMutableReceiver_ = false;
            return;
        }

        Ref<Type> previousExtensionTarget = currentExtensionTargetType_;
        const bool previousMutableReceiver = currentExtensionMutableReceiver_;
        currentExtensionTargetType_ = unwrapAliasType(node.targetType->refType.Lock());
        for (auto& member : node.members)
        {
            if (!member.method || !member.method->name->referencedSymbol.Lock())
                continue;
            currentExtensionMutableReceiver_ = member.mutableReceiver;
            member.method->accept(*this);
        }
        currentExtensionTargetType_ = previousExtensionTarget;
        currentExtensionMutableReceiver_ = previousMutableReceiver;
    }

    void SemanticAnalyzer::visit(ComponentDeclaration& node)
    {
        const bool isExplicitSpecialization = hasAttribute(node.attributes, Attribute::Specialize);

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this component.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            Ref<Symbol> genericPrimarySymbol = nullptr;
            Ref<StructType> genericPrimaryType = nullptr;
            std::vector<Ref<Type>> specializationArguments;
            bool specializationCanRegister = false;

            if (isExplicitSpecialization)
            {
                const bool hasDeclarationLevelNativeInterop = hasAttribute(node.attributes, Attribute::Native);
                if (hasDeclarationLevelNativeInterop)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Specialize cannot be combined with declaration-level @Native component interop."
                    );
                }

                auto specializationScope = buildGenericTypeParameterScope();
                genericTypeParameterScopes_.push_back(specializationScope);
                specializationArguments = resolveExplicitSpecializationArguments(
                    *this, node.attributes, node.location(), !node.genericParameters.empty());
                genericTypeParameterScopes_.pop_back();

                genericPrimarySymbol = currentScope_->resolveLocally(node.name->token.value);
                if (!genericPrimarySymbol || genericPrimarySymbol->kind != SymbolKind::Struct ||
                    !genericPrimarySymbol->type || genericPrimarySymbol->type->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Specialize component '{}' requires an earlier generic component declaration with the same name.",
                        node.name->token.value
                    );
                }
                else
                {
                    genericPrimaryType = genericPrimarySymbol->type.AsFast<StructType>();
                    if (genericPrimaryType->isObject || genericPrimaryType->isInterface)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Specialize component '{}' must target a generic component, not an object or interface.",
                            node.name->token.value
                        );
                    }
                    else if (genericPrimaryType->genericParameterNames.empty())
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Specialize target '{}' is not generic.", node.name->token.value);
                    }
                    else if (genericPrimaryType->isNativePodComponent)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Specialize is not yet supported for declaration-level @Native components.");
                    }
                    else
                    {
                        const size_t minimumArgumentCount = getMinimumGenericArgumentCount(
                            genericPrimaryType->genericParameterNames,
                            genericPrimaryType->hasGenericParameterPack
                        );
                        const bool invalidArity = genericPrimaryType->hasGenericParameterPack
                            ? specializationArguments.size() < minimumArgumentCount
                            : specializationArguments.size() != genericPrimaryType->genericParameterNames.size();
                        if (invalidArity)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                genericPrimaryType->hasGenericParameterPack
                                    ? "@Specialize for '{}' expects at least {} type arguments, but got {}."
                                    : "@Specialize for '{}' expects {} type arguments, but got {}.",
                                node.name->token.value,
                                genericPrimaryType->hasGenericParameterPack
                                    ? minimumArgumentCount
                                    : genericPrimaryType->genericParameterNames.size(),
                                specializationArguments.size()
                            );
                        }
                        else
                        {
                            specializationCanRegister = !hasDeclarationLevelNativeInterop && std::ranges::all_of(
                                specializationArguments,
                                [](const Ref<Type>& type)
                                {
                                    return type && !type->isUnknown();
                                }
                            );
                        }
                    }
                }
            }

            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            if (isExplicitSpecialization)
            {
                structType.AsFast<StructType>()->genericArguments = specializationArguments;
                structType.AsFast<StructType>()->genericPrimaryType = genericPrimaryType;
                structType.AsFast<StructType>()->isExplicitSpecialization = true;
                structType.AsFast<StructType>()->isPartialSpecialization = !node.genericParameters.empty();
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            else
            {
                structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);
            const bool isNativePodComponent = hasAttribute(node.attributes, Attribute::Native);
            structType.AsFast<StructType>()->isNativePodComponent = isNativePodComponent;
            structType.AsFast<StructType>()->nativeCppName = node.name ? node.name->token.value : "";
            structType.AsFast<StructType>()->nativeCppHeader.clear();

            if (isNativePodComponent)
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native component expects exactly one string literal argument.");
                    }
                    else
                    {
                        structType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native component must be a valid C++ identifier path like foo::bar.");
                        }
                        else
                        {
                            structType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on components currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on components currently requires declaration-level @Native.");
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "component");
            if (!isExplicitSpecialization)
                validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "component", node.location());
            else if (hasAttribute(node.attributes, Attribute::Apply))
                WIO_LOG_ADD_ERROR(node.location(), "@Apply cannot be declared on an explicit specialization; constrain the generic primary declaration.");
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> compSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            compSym->innerScope = structScope;
            compSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            structType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            compSym->genericParameterDefaults = std::move(genericParameterDefaults);
            compSym->hasGenericParameterPack = structType.AsFast<StructType>()->hasGenericParameterPack;
            if (isExplicitSpecialization)
            {
                if (specializationCanRegister && genericPrimaryType)
                {
                    const std::string specializationKey = getGenericSpecializationKey(specializationArguments);
                    const bool isPartialSpecialization = structType.AsFast<StructType>()->isPartialSpecialization;
                    const bool duplicatePartial = isPartialSpecialization && std::ranges::any_of(
                        genericPrimaryType->partialSpecializations,
                        [&](const WeakRef<StructType>& candidate)
                        {
                            auto partial = candidate.Lock();
                            return partial && getGenericSpecializationKey(partial->genericArguments) == specializationKey;
                        });
                    if ((!isPartialSpecialization && genericPrimaryType->explicitSpecializations.contains(specializationKey)) || duplicatePartial)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Duplicate explicit specialization for '{}{}'.",
                            node.name->token.value,
                            formatConcreteInstantiationSignature(specializationArguments)
                        );
                    }
                    else
                    {
                        if (isPartialSpecialization)
                            genericPrimaryType->partialSpecializations.emplace_back(structType.AsFast<StructType>());
                        else
                            genericPrimaryType->explicitSpecializations.emplace(specializationKey, structType.AsFast<StructType>());
                    }
                }
            }
            else
            {
                currentScope_->define(node.name->token.value, compSym);
            }
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

            if (structType->isNativePodComponent)
            {
                for (const auto& member : node.members)
                {
                    if (!member.declaration)
                        continue;

                    if (!member.declaration->is<VariableDeclaration>())
                    {
                        WIO_LOG_ADD_ERROR(
                            member.declaration->location(),
                            "Declaration-level @Native components currently support only POD-style fields. Lifecycle functions and methods are not supported on the component declaration itself."
                        );
                        continue;
                    }
                }
            }

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

                        if (structType->isNativePodComponent && !isNativePodInteropFieldType(memberSym->type, true))
                        {
                            WIO_LOG_ADD_ERROR(
                                varDecl->location(),
                                "Declaration-level @Native component '{}' field '{}' uses type '{}' which is not POD-native-compatible yet. Supported field types are primitives, POD-compatible static arrays, and other declaration-level @Native components.",
                                node.name->token.value,
                                varDecl->name->token.value,
                                memberSym->type ? memberSym->type->toString() : "<unknown>"
                            );
                        }
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
                    const bool isOperatorMethod = common::isOperatorOverloadName(funcName);

                    if (funcName != "OnConstruct" && funcName != "OnDestruct" && !isOperatorMethod)
                    {
                        WIO_LOG_ADD_ERROR(
                            funcDecl->location(),
                            "Components cannot define ordinary methods. Use an object for behavior, an operator overload, or OnConstruct/OnDestruct for lifecycle."
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
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(sym);
        
        for (auto& member : node.members)
            if (member.declaration->is<FunctionDeclaration>())
                member.declaration->accept(*this);

        genericTypeParameterScopes_.pop_back();
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(ObjectDeclaration& node)
    {
        const bool isExplicitSpecialization = hasAttribute(node.attributes, Attribute::Specialize);

        auto buildGenericTypeParameterScope = [&]() -> std::unordered_map<std::string, Ref<Type>>
        {
            std::unordered_map<std::string, Ref<Type>> scope;
            scope.reserve(node.genericParameters.size());

            for (size_t genericIndex = 0; genericIndex < node.genericParameters.size(); ++genericIndex)
            {
                const auto& genericParameter = node.genericParameters[genericIndex];
                if (!genericParameter)
                    continue;

                const std::string& parameterName = genericParameter->token.value;
                if (scope.contains(parameterName))
                {
                    WIO_LOG_ADD_ERROR(genericParameter->location(), "Generic parameter '{}' is already declared on this object.", parameterName);
                    continue;
                }

                const bool isGenericParameterPack =
                    node.hasGenericParameterPack &&
                    genericIndex + 1 == node.genericParameters.size();
                Ref<Type> parameterType = isGenericParameterPack
                    ? Compiler::get().getTypeContext().getOrCreateGenericParameterPackType(parameterName)
                    : Compiler::get().getTypeContext().getOrCreateGenericParameterType(parameterName);
                genericParameter->refType = parameterType;
                scope.emplace(parameterName, parameterType);
            }

            return scope;
        };

        if (isDeclarationPass_)
        {
            Ref<Symbol> genericPrimarySymbol = nullptr;
            Ref<StructType> genericPrimaryType = nullptr;
            std::vector<Ref<Type>> specializationArguments;
            bool specializationCanRegister = false;

            if (isExplicitSpecialization)
            {
                auto specializationScope = buildGenericTypeParameterScope();
                genericTypeParameterScopes_.push_back(specializationScope);
                specializationArguments = resolveExplicitSpecializationArguments(
                    *this, node.attributes, node.location(), !node.genericParameters.empty());
                genericTypeParameterScopes_.pop_back();

                genericPrimarySymbol = currentScope_->resolveLocally(node.name->token.value);
                if (!genericPrimarySymbol || genericPrimarySymbol->kind != SymbolKind::Struct ||
                    !genericPrimarySymbol->type || genericPrimarySymbol->type->kind() != TypeKind::Struct)
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "@Specialize object '{}' requires an earlier generic object declaration with the same name.",
                        node.name->token.value
                    );
                }
                else
                {
                    genericPrimaryType = genericPrimarySymbol->type.AsFast<StructType>();
                    if (!genericPrimaryType->isObject || genericPrimaryType->isInterface)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "@Specialize object '{}' must target a generic object, not a component or interface.",
                            node.name->token.value
                        );
                    }
                    else if (genericPrimaryType->genericParameterNames.empty())
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@Specialize target '{}' is not generic.", node.name->token.value);
                    }
                    else
                    {
                        const size_t minimumArgumentCount = getMinimumGenericArgumentCount(
                            genericPrimaryType->genericParameterNames,
                            genericPrimaryType->hasGenericParameterPack
                        );
                        const bool invalidArity = genericPrimaryType->hasGenericParameterPack
                            ? specializationArguments.size() < minimumArgumentCount
                            : specializationArguments.size() != genericPrimaryType->genericParameterNames.size();
                        if (invalidArity)
                        {
                            WIO_LOG_ADD_ERROR(
                                node.location(),
                                genericPrimaryType->hasGenericParameterPack
                                    ? "@Specialize for '{}' expects at least {} type arguments, but got {}."
                                    : "@Specialize for '{}' expects {} type arguments, but got {}.",
                                node.name->token.value,
                                genericPrimaryType->hasGenericParameterPack
                                    ? minimumArgumentCount
                                    : genericPrimaryType->genericParameterNames.size(),
                                specializationArguments.size()
                            );
                        }
                        else
                        {
                            specializationCanRegister = std::ranges::all_of(
                                specializationArguments,
                                [](const Ref<Type>& type)
                                {
                                    return type && !type->isUnknown();
                                }
                            );
                        }
                    }
                }
            }

            auto structScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(structScope);
            
            Ref<Type> structType = Ref<StructType>::Create(node.name->token.value, structScope, true);
            structType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            if (isExplicitSpecialization)
            {
                structType.AsFast<StructType>()->genericArguments = specializationArguments;
                structType.AsFast<StructType>()->genericPrimaryType = genericPrimaryType;
                structType.AsFast<StructType>()->isExplicitSpecialization = true;
                structType.AsFast<StructType>()->isPartialSpecialization = !node.genericParameters.empty();
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            else
            {
                structType.AsFast<StructType>()->genericParameterNames.reserve(node.genericParameters.size());
                for (const auto& genericParameter : node.genericParameters)
                {
                    if (genericParameter)
                        structType.AsFast<StructType>()->genericParameterNames.push_back(genericParameter->token.value);
                }
                structType.AsFast<StructType>()->hasGenericParameterPack = node.hasGenericParameterPack;
            }
            structType.AsFast<StructType>()->isFinal = hasAttribute(node.attributes, Attribute::Final);

            if (hasAttribute(node.attributes, Attribute::Native))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Declaration-level @Native POD interop is currently supported only on components. Objects are not supported yet.");
            }
            if (hasAttribute(node.attributes, Attribute::CppHeader))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on objects is reserved for future declaration-level native object interop.");
            }
            if (hasAttribute(node.attributes, Attribute::CppName))
            {
                WIO_LOG_ADD_ERROR(node.location(), "@CppName on objects is reserved for future declaration-level native object interop.");
            }

            auto genericScope = buildGenericTypeParameterScope();
            genericTypeParameterScopes_.push_back(genericScope);
            auto genericParameterDefaults = resolveGenericParameterDefaults(
                *this, node.genericParameters, node.hasGenericParameterPack, "object");
            if (!isExplicitSpecialization)
                validateApplyAttributes(*this, node.attributes, structType.AsFast<StructType>()->genericParameterNames, "object", node.location());
            else if (hasAttribute(node.attributes, Attribute::Apply))
                WIO_LOG_ADD_ERROR(node.location(), "@Apply cannot be declared on an explicit specialization; constrain the generic primary declaration.");
            genericTypeParameterScopes_.pop_back();
            Ref<Symbol> objSym = createSymbol(node.name->token.value, structType, SymbolKind::Struct, node.location());
            objSym->innerScope = structScope;
            objSym->genericParameterNames = structType.AsFast<StructType>()->genericParameterNames;
            structType.AsFast<StructType>()->genericParameterDefaults = genericParameterDefaults;
            objSym->genericParameterDefaults = std::move(genericParameterDefaults);
            objSym->hasGenericParameterPack = structType.AsFast<StructType>()->hasGenericParameterPack;
            if (isExplicitSpecialization)
            {
                if (specializationCanRegister && genericPrimaryType)
                {
                    const std::string specializationKey = getGenericSpecializationKey(specializationArguments);
                    const bool isPartialSpecialization = structType.AsFast<StructType>()->isPartialSpecialization;
                    const bool duplicatePartial = isPartialSpecialization && std::ranges::any_of(
                        genericPrimaryType->partialSpecializations,
                        [&](const WeakRef<StructType>& candidate)
                        {
                            auto partial = candidate.Lock();
                            return partial && getGenericSpecializationKey(partial->genericArguments) == specializationKey;
                        });
                    if ((!isPartialSpecialization && genericPrimaryType->explicitSpecializations.contains(specializationKey)) || duplicatePartial)
                    {
                        WIO_LOG_ADD_ERROR(
                            node.location(),
                            "Duplicate explicit specialization for '{}{}'.",
                            node.name->token.value,
                            formatConcreteInstantiationSignature(specializationArguments)
                        );
                    }
                    else
                    {
                        if (isPartialSpecialization)
                            genericPrimaryType->partialSpecializations.emplace_back(structType.AsFast<StructType>());
                        else
                            genericPrimaryType->explicitSpecializations.emplace(specializationKey, structType.AsFast<StructType>());
                    }
                }
            }
            else
            {
                currentScope_->define(node.name->token.value, objSym);
            }
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
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.push_back(sym);

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
        if (!structType->genericParameterNames.empty())
            activeGenericConstraintSymbols_.pop_back();
        currentStructType_ = nullptr;
        currentBaseStructType_ = nullptr;
        currentScope_ = prevScope;
    }

    void SemanticAnalyzer::visit(FlagDeclaration& node)
    {
        if (isDeclarationPass_)
        {
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

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
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

            auto enumScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(enumScope);
            
            Ref<Type> enumType = Ref<StructType>::Create(node.name->token.value, enumScope);
            enumType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            enumType.AsFast<StructType>()->isEnum = true;
            const bool isNativeEnum = hasAttribute(node.attributes, Attribute::Native);
            enumType.AsFast<StructType>()->isNativePodComponent = isNativeEnum;
            enumType.AsFast<StructType>()->nativeCppName = node.name ? node.name->token.value : "";
            enumType.AsFast<StructType>()->nativeCppHeader.clear();

            if (isNativeEnum)
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native enum expects exactly one string literal argument.");
                    }
                    else
                    {
                        enumType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native enum must be a valid C++ identifier path like foo::bar.");
                        }
                        else
                        {
                            enumType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on enums currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on enums currently requires declaration-level @Native.");
            }

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

            if (sym && sym->type && sym->type->kind() == TypeKind::Struct)
                sym->type.AsFast<StructType>()->enumUnderlyingType = targetType;
            
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
            if (hasAttribute(node.attributes, Attribute::Specialize))
                WIO_LOG_ADD_ERROR(node.location(), "@Specialize is supported only on generic object and component declarations.");

            auto flagsetScope = Ref<Scope>::Create(currentScope_, ScopeKind::Struct);
            scopes_.push_back(flagsetScope);
            
            Ref<Type> flagsetType = Ref<StructType>::Create(node.name->token.value, flagsetScope);
            flagsetType.AsFast<StructType>()->scopePath = getCurrentNamespacePath();
            flagsetType.AsFast<StructType>()->isFlagset = true;
            const bool isNativeFlagset = hasAttribute(node.attributes, Attribute::Native);
            flagsetType.AsFast<StructType>()->isNativePodComponent = isNativeFlagset;
            flagsetType.AsFast<StructType>()->nativeCppName = node.name ? node.name->token.value : "";
            flagsetType.AsFast<StructType>()->nativeCppHeader.clear();

            if (isNativeFlagset)
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                {
                    auto headerArgs = getAttributeArgs(node.attributes, Attribute::CppHeader);
                    if (headerArgs.size() != 1 || headerArgs.front().type != TokenType::stringLiteral)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on a native flagset expects exactly one string literal argument.");
                    }
                    else
                    {
                        flagsetType.AsFast<StructType>()->nativeCppHeader = headerArgs.front().value;
                    }
                }

                if (hasAttribute(node.attributes, Attribute::CppName))
                {
                    auto cppNameArgs = getAttributeArgs(node.attributes, Attribute::CppName);
                    if (cppNameArgs.size() != 1)
                    {
                        WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset expects exactly one target symbol argument.");
                    }
                    else if (const Token* cppNameArg = getFirstAttributeArg(node.attributes, Attribute::CppName); cppNameArg)
                    {
                        if (cppNameArg->type != TokenType::identifier && cppNameArg->type != TokenType::stringLiteral)
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset expects an identifier path like foo::bar or a string literal.");
                        }
                        else if (!isValidCppSymbolPath(cppNameArg->value, true))
                        {
                            WIO_LOG_ADD_ERROR(node.location(), "@CppName on a native flagset must be a valid C++ identifier path like foo::bar.");
                        }
                        else
                        {
                            flagsetType.AsFast<StructType>()->nativeCppName = cppNameArg->value;
                        }
                    }
                }
            }
            else
            {
                if (hasAttribute(node.attributes, Attribute::CppHeader))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppHeader on flagsets currently requires declaration-level @Native.");
                if (hasAttribute(node.attributes, Attribute::CppName))
                    WIO_LOG_ADD_ERROR(node.location(), "@CppName on flagsets currently requires declaration-level @Native.");
            }
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

            if (sym && sym->type && sym->type->kind() == TypeKind::Struct)
                sym->type.AsFast<StructType>()->enumUnderlyingType = targetType;
            
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

        const auto narrowedSymbolForNullComparison = [&](bool nonNullBranch) -> Ref<Symbol>
        {
            auto binary = node.condition->as<BinaryExpression>();
            if (!binary ||
                (binary->op.type != TokenType::opEqual && binary->op.type != TokenType::opNotEqual))
            {
                return nullptr;
            }

            const bool leftIsNull = binary->left->is<NullExpression>();
            const bool rightIsNull = binary->right->is<NullExpression>();
            if (leftIsNull == rightIsNull)
                return nullptr;

            auto candidate = leftIsNull ? binary->right : binary->left;
            auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
            Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
            if (!symbolType || symbolType->kind() != TypeKind::Nullable)
                return nullptr;

            const bool conditionMeansNonNull = binary->op.type == TokenType::opNotEqual;
            return conditionMeansNonNull == nonNullBranch ? symbol : nullptr;
        };

        const auto previousNarrowing = nonNullNarrowedSymbols_;
        if (auto thenNarrowed = narrowedSymbolForNullComparison(true))
            nonNullNarrowedSymbols_.insert(thenNarrowed.Get());

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
                Ref<Type> lhsType = getAutoReadableType(binExpr->left->refType.Lock());
                Ref<Type> targetType = binExpr->right->refType.Lock();

                if (binExpr->op.type == TokenType::kwIs && isAnyType(lhsType) && isSupportedAnyCastTargetType(targetType))
                {
                    auto varSym = createSymbol(node.matchVar.value, targetType, SymbolKind::Variable, node.matchVar.loc);
                    currentScope_->define(node.matchVar.value, varSym);
                }
                else if (binExpr->op.type == TokenType::kwIs && targetStruct)
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
        nonNullNarrowedSymbols_ = previousNarrowing;
        if (auto elseNarrowed = narrowedSymbolForNullComparison(false))
            nonNullNarrowedSymbols_.insert(elseNarrowed.Get());
        if (node.elseBranch) node.elseBranch->accept(*this);
        nonNullNarrowedSymbols_ = previousNarrowing;

        const auto definitelyReturns = [&](const auto& self, const NodePtr<Statement>& statement) -> bool
        {
            if (!statement)
                return false;
            if (statement->is<ReturnStatement>())
                return true;
            if (auto block = statement->as<BlockStatement>())
                return !block->statements.empty() && self(self, block->statements.back());
            return false;
        };

        if (!node.elseBranch && definitelyReturns(definitelyReturns, node.thenBranch))
        {
            if (auto guardNarrowed = narrowedSymbolForNullComparison(false))
                nonNullNarrowedSymbols_.insert(guardNarrowed.Get());
        }
    }
    
    void SemanticAnalyzer::visit(WhileStatement& node)
    {
        if (isDeclarationPass_) return;
        node.condition->accept(*this);

        if (auto condType = node.condition->refType.Lock(); condType != Compiler::get().getTypeContext().getBool())
        {
            // Todo: Check null (i.g. it's not works well)
            if (!allowsNumericSemantics(condType) && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null)
            {
                WIO_LOG_ADD_ERROR(
                    node.condition->location(),
                    "While condition must be a boolean, numeric, or reference type. Got: {}",
                    condType->toString()
                );
            }
        }

        const auto previousNarrowing = nonNullNarrowedSymbols_;
        if (auto comparison = node.condition->as<BinaryExpression>();
            comparison && comparison->op.type == TokenType::opNotEqual)
        {
            const bool leftIsNull = comparison->left->is<NullExpression>();
            const bool rightIsNull = comparison->right->is<NullExpression>();
            if (leftIsNull != rightIsNull)
            {
                auto candidate = leftIsNull ? comparison->right : comparison->left;
                auto symbol = candidate->is<Identifier>() ? candidate->referencedSymbol.Lock() : nullptr;
                Ref<Type> symbolType = symbol ? unwrapAliasType(symbol->type) : nullptr;
                if (symbol && symbolType && symbolType->kind() == TypeKind::Nullable)
                    nonNullNarrowedSymbols_.insert(symbol.Get());
            }
        }

        loopDepth_++;
        if (node.body) node.body->accept(*this);
        loopDepth_--;
        nonNullNarrowedSymbols_ = previousNarrowing;
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
            else if (!allowsIntegerSemantics(startType) || !allowsIntegerSemantics(endType))
            {
                WIO_LOG_ADD_ERROR(node.location(), "Range iteration currently requires integer bounds.");
            }
            else if (node.step && !allowsIntegerSemantics(stepType))
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

            if (node.step && !allowsIntegerSemantics(stepType))
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
                if (!allowsNumericSemantics(condType) && condType->kind() != TypeKind::Reference && condType->kind() != TypeKind::Null)
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

        Ref<Type> resolvedExpectedReturnType = unwrapAliasType(currentFunctionReturnType_);
        Ref<Type> resolvedActualReturnType = unwrapAliasType(actualType);
        if (node.value &&
            resolvedExpectedReturnType && resolvedActualReturnType &&
            resolvedExpectedReturnType->kind() == TypeKind::Reference &&
            resolvedActualReturnType->kind() == TypeKind::Reference)
        {
            const BorrowOrigin origin = classifyBorrowOrigin(node.value);
            if (origin == BorrowOrigin::Local || origin == BorrowOrigin::Temporary)
            {
                WIO_LOG_ADD_ERROR(
                    node.value->location(),
                    "Cannot return a reference borrowed from {}; the borrow would outlive its owner.",
                    borrowOriginName(origin)
                );
            }
        }

        if (currentFunctionReturnType_)
        {
            if (!currentFunctionReturnType_->isUnknown() &&
                actualType &&
                !actualType->isUnknown() &&
                !isAssignmentLikeCompatible(currentFunctionReturnType_, actualType))
            {
                if (isRejectedImplicitNumericConversion(currentFunctionReturnType_, actualType))
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Implicit narrowing return conversion from '{}' to '{}' requires explicit 'fit'.",
                        actualType->toString(), currentFunctionReturnType_->toString());
                }
                else
                {
                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Return type mismatch! Expected '{}', but got '{}'.",
                        currentFunctionReturnType_->toString(),
                        actualType->toString());
                }
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

        if (auto importedNamespace = resolveImportedNamespace())
        {
            if (!node.aliasName.empty())
            {
                if (auto existingAlias = currentScope_->resolveLocally(node.aliasName))
                {
                    if (existingAlias == importedNamespace)
                        return;

                    WIO_LOG_ADD_ERROR(node.location(), "Symbol '{}' already exists and cannot be used as an import alias.", node.aliasName);
                    return;
                }

                currentScope_->define(node.aliasName, importedNamespace);
                if (!node.importAllIntoScope)
                    return;
            }

            if (!node.importAllIntoScope)
                return;

            if (!importedNamespace->innerScope)
                return;

            for (const auto& [symbolName, importedSymbol] : importedNamespace->innerScope->getSymbols())
            {
                if (!importedSymbol)
                    continue;

                if (auto existingSymbol = currentScope_->resolveLocally(symbolName))
                {
                    if (existingSymbol == importedSymbol)
                        continue;

                    WIO_LOG_ADD_ERROR(
                        node.location(),
                        "Symbol '{}' already exists and cannot be directly imported from module '{}'.",
                        symbolName,
                        node.modulePath
                    );
                    continue;
                }

                currentScope_->define(symbolName, importedSymbol);
            }
            return;
        }

        if (node.isStdLib)
        {
            WIO_LOG_ADD_ERROR(node.location(), "Standard library module 'std::{}' could not be resolved after merge.", node.modulePath);
            return;
        }

        if (node.aliasName.empty() && !node.importAllIntoScope)
            return;

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

        if (!node.aliasName.empty())
        {
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

            if (!node.importAllIntoScope)
                return;
        }

        for (const auto& importedSymbol : importedSymbols)
        {
            if (!importedSymbol)
                continue;

            if (auto existingSymbol = currentScope_->resolveLocally(importedSymbol->name))
            {
                if (existingSymbol == importedSymbol)
                    continue;

                WIO_LOG_ADD_ERROR(
                    node.location(),
                    "Symbol '{}' already exists and cannot be directly imported from module '{}'.",
                    importedSymbol->name,
                    node.modulePath
                );
                continue;
            }

            currentScope_->define(importedSymbol->name, importedSymbol);
        }
    }
}
