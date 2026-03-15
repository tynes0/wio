#pragma once

namespace wio
{
#define X(name) struct name;
#include "ast_nodes.def"
#undef X

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct ASTVisitor
    {
        virtual ~ASTVisitor() = default;

        // NOLINTNEXTLINE(bugprone-macro-parentheses)
#       define X(item) virtual void visit(item& node) = 0;
#       include "ast_nodes.def"
#       undef X
    };
}
