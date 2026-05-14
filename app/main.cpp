#include "compiler.h"
#include "tooling_cli.h"

#if defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    if (const auto toolingResult = wio::tooling::tryHandleToolingCommand(argc, argv); toolingResult.has_value())
        return *toolingResult;

    wio::Compiler::get().loadArgs(argc, argv);
    return wio::Compiler::get().compile();
}
