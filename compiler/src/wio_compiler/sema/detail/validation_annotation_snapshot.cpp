#include "validation_annotation_snapshot.h"

#include "wio/ast/ast.h"

#include <utility>

namespace wio::sema::detail
{
    struct ValidationAnnotationSnapshot::State
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

    ValidationAnnotationSnapshot::ValidationAnnotationSnapshot()
        : state_(std::make_unique<State>())
    {
    }

    ValidationAnnotationSnapshot::~ValidationAnnotationSnapshot() = default;

    void ValidationAnnotationSnapshot::capture(ASTNode* root)
    {
        state_->capture(root);
    }

    void ValidationAnnotationSnapshot::restore() const
    {
        state_->restore();
    }
}
