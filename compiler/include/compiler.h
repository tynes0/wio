#pragma once

#include "wio/ast/ast.h"
#include "wio/sema/type_context.h"
#include "wio/common/auto_flags.h"

#define COMPILER_FLAGS(X) X(SingleFile) X(ShowTokens) X(ShowAst) X(DryRun) X(NoBuiltin) X(WarnAsError) X(Run)
    DEFINE_FLAGS(CompilerFlags, COMPILER_FLAGS);
#undef COMPILER_FLAGS

namespace wio
{
    class Compiler
    {
    public:
        Compiler();

        void loadArgs(int argc, char* argv[]) const;
        int compile() const;

        sema::TypeContext& getTypeContext() const;
        CompilerFlags getFlags() const;

        static Compiler& get();

    private:
        static Ref<Program> parseAndMerge(const std::string& modulePath, bool isStdLib, const std::filesystem::path& currentDir);
    };
}
