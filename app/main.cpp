#include "compiler.h"
#include "general/core.h"

#if defined(_MSC_VER) && defined(_DEBUG)
    #define _CRTDBG_MAP_ALLOC
    #include <crtdbg.h>
#endif

#define WIO_TEST 1

#if WIO_TEST

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // Windows Debug build -> memory leak check
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    WIO_UNUSED(argc);
    WIO_UNUSED(argv);

    const char* args[] = { "", "-r", "Debug/tests/test1.wio" };

    wio::Compiler::get().loadArgs(sizeof(args) / sizeof(const char*), const_cast<char**>(args));
    wio::Compiler::get().compile();
}

#else

int main(int argc, char* argv[])
{
#if defined(_MSC_VER) && defined(_DEBUG)
    // Windows Debug build -> memory leak check
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    wio::Compiler::get().loadArgs(argc, argv);
    wio::Compiler::get().compile();
}

#endif