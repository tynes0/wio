#pragma once

#include "wio/sema/type_context.h"
#include "wio/common/auto_flags.h"

#define COMPILER_FLAGS(X) X(SingleFile) X(ShowTokens) X(ShowAst) X(DryRun) X(NoBuiltin) X(WarnAsError)
    DEFINE_FLAGS(CompilerFlags, COMPILER_FLAGS);
#undef COMPILER_FLAGS

namespace wio
{
    class Compiler
    {
    public:
        Compiler();

        void loadArgs(int argc, char* argv[]) const;
        void compile() const;

        sema::TypeContext& getTypeContext() const;
        CompilerFlags getFlags() const;

        static Compiler& get();
    };
}