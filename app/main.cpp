#include "compiler.h"

#if defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // Windows Debug build -> memory leak check
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    wio::Compiler::get().loadArgs(argc, argv);
    return wio::Compiler::get().compile();
}
