#include "src/interpreter.h"

#define WIO_TEST 1

#if WIO_TEST

int main(int argc, char* argv[])
{
    const char* args[] = { "", /*"-h", "-nr", "-st",*/ "tests/test1.wio" };
    wio::interpreter::get().load_args(sizeof(args) / sizeof(const char*), (char**)args);
    wio::interpreter::get().run();
}

#else // WIO_TEST

int main(int argc, char* argv[])
{
    wio::interpreter::get().load_args(argc, argv);
    wio::interpreter::get().run();
}

#endif // WIO_TEST