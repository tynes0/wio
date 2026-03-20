#pragma once

#include <sstream>
#include <string>

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

        void generateHeader();

        void emit(const std::string& str);
        void emitLine(const std::string& str = "");
        void emitMain(FunctionDeclaration& node);
        void indent();
        void dedent();
        
        std::string toCppType(const Ref<sema::Type>& type);
    };
}