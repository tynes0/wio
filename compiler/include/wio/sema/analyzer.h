#pragma once

#include "wio/ast/ast_visitor.h"
#include "symbol.h"
#include "scope.h"
#include "type.h"
#include <vector>

namespace wio::sema
{
    class SemanticAnalyzer : public ASTVisitor
    {
    public:
        SemanticAnalyzer();
        void analyze(const Ref<Program>& program);

#include "../ast/visitor_overloads.def"

    private:
        Ref<Scope> currentScope_;
        std::vector<Ref<Scope>> scopes_;
        std::vector<Ref<Symbol>> symbols_;
        Ref<Type> currentFunctionReturnType_ = nullptr;
        bool isDeclarationPass_ = true;
        bool isStructResolutionPass_ = false;
        
        void enterScope(ScopeKind kind);
        void exitScope();

        Ref<Symbol> createSymbol(std::string name, Ref<Type> type, SymbolKind kind, common::Location loc, SymbolFlags flags = SymbolFlags::createAllFalse());
        static bool hasAttribute(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr);
        static std::vector<Token> getAttributeArgs(const std::vector<NodePtr<AttributeStatement>>& attributes, Attribute targetAttr);
    };
}
