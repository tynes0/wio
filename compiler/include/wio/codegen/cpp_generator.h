#pragma once

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "wio/ast/ast.h"
#include "wio/ast/ast_visitor.h"
#include "wio/codegen/mangler.h"

namespace wio::codegen
{
    class CppGenerator : public ASTVisitor
    {
    public:
        CppGenerator();
        
        std::string generate(const Ref<Program>& program);

#include "../ast/visitor_overloads.def"

    private:
        std::stringstream buffer_;
        std::stringstream header_;
        
        int indentationLevel_ = 0;

        bool isEmittingPrototypes_ = false;
        std::string currentClassName_;
        bool currentExtensionMethod_ = false;
        Ref<sema::Type> currentFunctionReturnType_;
        std::unordered_map<const sema::Symbol*, const VariableDeclaration*> variableDeclarationsBySymbol_;

        void emitStatements(const std::vector<NodePtr<Statement>>& statements);
        void generateHeader();
        bool emitIntrinsicMemberAccess(MemberAccessExpression& node);

        void emit(const std::string& str);
        void emitLine(const std::string& str = "");
        void emitHeader(const std::string& str);
        void emitHeaderLine(const std::string& str = "");
        bool emitAnyBoxingIfNeeded(const NodePtr<Expression>& expression, const Ref<sema::Type>& expectedType);
        void emitReadableExpression(const NodePtr<Expression>& expression);
        void emitExpressionWithExpectedType(const NodePtr<Expression>& expression, const Ref<sema::Type>& expectedType, bool allowAutoRef = false);
        Ref<sema::FunctionType> getMangledCallableFunctionType(const Ref<sema::Symbol>& callableSymbol,
                                                               const Ref<sema::FunctionType>& resolvedFunctionType,
                                                               size_t argumentCount) const;
        void emitSourceDirective(const common::Location& loc);
        void emitGeneratedDirective();
        void emitMain(FunctionDeclaration& node);
        void emitModuleApiTable(const Ref<Program>& program);
        void indent();
        void dedent();
    };
}
