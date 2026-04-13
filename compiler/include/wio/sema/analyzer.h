#pragma once

#include "wio/ast/ast_visitor.h"
#include "symbol.h"
#include "scope.h"
#include "type.h"
#include <unordered_map>
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
        std::vector<Ref<Scope>> scopes_;
        std::vector<Ref<Symbol>> symbols_;
        Ref<Scope> currentScope_ = nullptr;
        Ref<Type> currentFunctionReturnType_ = nullptr;
        Ref<Type> currentStructType_ = nullptr;
        Ref<Type> currentBaseStructType_ = nullptr;
        std::vector<std::string> currentNamespacePath_;
        std::vector<std::unordered_map<std::string, Ref<Type>>> genericTypeParameterScopes_;
        uint32_t loopDepth_ = 0;
        bool isDeclarationPass_ = true;
        bool isStructResolutionPass_ = false;
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
    };
}
