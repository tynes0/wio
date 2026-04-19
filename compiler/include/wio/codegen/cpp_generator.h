#pragma once

#include <sstream>
#include <string>
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

        void emitStatements(const std::vector<NodePtr<Statement>>& statements);
        void generateHeader();
        bool emitIntrinsicMemberAccess(MemberAccessExpression& node);

        void emit(const std::string& str);
        void emitLine(const std::string& str = "");
        void emitHeader(const std::string& str);
        void emitHeaderLine(const std::string& str = "");
        void emitMain(FunctionDeclaration& node);
        void emitModuleApiTable(const Ref<Program>& program);
        void indent();
        void dedent();
    };
}
