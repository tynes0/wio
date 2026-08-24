#pragma once

#include "wio/ast/ast_visitor.h"
#include "symbol.h"
#include "scope.h"
#include "type.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace wio::sema
{
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions
    class SemanticAnalyzer : public ASTVisitor
    {
    public:
        SemanticAnalyzer();
        ~SemanticAnalyzer() override;
        void analyze(const Ref<Program>& program);

#include "../ast/visitor_overloads.def"

    private:
        struct GenericConstraintCapabilities
        {
            bool isKnown = false;
            bool hasApplicableConstraints = false;
            bool isIncompatible = false;
            bool allowsInteger = false;
            bool allowsNumeric = false;
            bool allowsEnum = false;
            bool allowsFlagset = false;
            bool allowsObjectLike = false;
        };

        std::vector<Ref<Scope>> scopes_;
        std::vector<Ref<Symbol>> symbols_;
        std::unordered_map<const Symbol*, const FunctionDeclaration*> functionDeclarationsBySymbol_;
        std::unordered_map<const Symbol*, const VariableDeclaration*> variableDeclarationsBySymbol_;
        std::unordered_set<const Symbol*> nonNullNarrowedSymbols_;
        std::unordered_map<const Symbol*, const std::vector<NodePtr<AttributeStatement>>*> attributeListsBySymbol_;
        std::unordered_map<std::string, common::Location> exportedCppSymbolLocations_;
        Ref<Scope> currentScope_ = nullptr;
        Ref<Type> currentExpectedExpressionType_ = nullptr;
        bool allowContextualNumericLiteralTyping_ = false;
        bool allowInferredStaticArrayExtent_ = false;
        Ref<Type> currentFunctionReturnType_ = nullptr;
        bool currentFunctionIsAsync_ = false;
        struct LambdaCaptureContext
        {
            LambdaExpression* node = nullptr;
            std::unordered_set<const Symbol*> localSymbols;
            std::unordered_set<const Symbol*> capturedSymbols;
            std::vector<Ref<Symbol>> captures;
        };
        std::vector<LambdaCaptureContext> lambdaCaptureContexts_;
        Ref<Type> currentStructType_ = nullptr;
        Ref<Type> currentBaseStructType_ = nullptr;
        Ref<Type> currentExtensionTargetType_ = nullptr;
        bool currentExtensionMutableReceiver_ = false;
        std::unordered_map<const Type*, std::unordered_map<std::string, Ref<Symbol>>> extensionMethods_;
        Ref<Symbol> currentFunctionParameterPackSymbol_ = nullptr;
        Ref<Type> currentFunctionParameterPackType_ = nullptr;
        std::vector<Ref<Symbol>> activeGenericConstraintSymbols_;
        bool allowParameterPackIdentifierReference_ = false;
        bool allowTypePackIdentifierReference_ = false;
        std::vector<std::string> currentNamespacePath_;
        std::vector<std::unordered_map<std::string, Ref<Type>>> genericTypeParameterScopes_;
        std::vector<NodePtr<AttributeStatement>> activeScopedAttributes_;
        uint32_t loopDepth_ = 0;
        bool isDeclarationPass_ = true;
        bool isStructResolutionPass_ = false;
        bool isAttributeContractPass_ = false;
        bool isDeriveExpansionPass_ = false;
        bool seenModuleApiVersion_ = false;
        bool seenModuleLoad_ = false;
        bool seenModuleUpdate_ = false;
        bool seenModuleUnload_ = false;
        bool seenModuleSaveState_ = false;
        bool seenModuleRestoreState_ = false;
        
        void enterScope(ScopeKind kind);
        void exitScope();

        [[nodiscard]] std::string getCurrentNamespacePath() const;
        Ref<Symbol> createSymbol(std::string name, Ref<Type> type, SymbolKind kind, common::Location loc, SymbolFlags flags = SymbolFlags::createAllFalse());
        void validateExecutorTransfer(FunctionCallExpression& node, const Ref<Symbol>& functionSymbol);
        bool validateConcreteGenericFunctionBody(
            const FunctionDeclaration& node,
            const Ref<Symbol>& funcSym,
            const Ref<FunctionType>& declaredFunctionType,
            const Ref<StructType>& concreteOwnerType,
            const std::unordered_map<std::string, Ref<Type>>& directBindings,
            const std::unordered_map<std::string, std::vector<Ref<Type>>>& packBindings,
            const std::unordered_map<std::string, std::string>& packAliases
        );
        [[nodiscard]] GenericConstraintCapabilities resolveGenericConstraintCapabilities(const Ref<Type>& type) const;
        [[nodiscard]] bool allowsNumericSemantics(const Ref<Type>& type) const;
        [[nodiscard]] bool allowsIntegerSemantics(const Ref<Type>& type) const;
        void validateAttributeApplications(std::vector<NodePtr<AttributeStatement>>& attributes,
                                           std::string_view target,
                                           bool validateTarget = true);
        void applyActiveScopedAttributes(std::vector<NodePtr<AttributeStatement>>& attributes,
                                         std::string_view target);
        void registerDerivedMethods(std::vector<NodePtr<AttributeStatement>>& attributes,
                                    const Ref<Type>& targetType,
                                    std::string_view target);

        std::unordered_set<std::string> validatedGenericFunctionBodyKeys_;
        std::unordered_set<std::string> validatingGenericFunctionBodyKeys_;
    };
}
