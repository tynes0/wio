#include "src/application.h"

#define WIO_TEST 1

#if WIO_TEST

int main(int argc, char* argv[])
{
    const char* args[] = { "",/*"-t", "-p",*/"tests/test1.wio"};
    wio::application app(sizeof(args) / sizeof(const char*), args);
    app.run();
}

#else // WIO_TEST

int main(int argc, char* argv[])
{
    wio::application app(argc, (const char**)argv);
    app.run();
}

#endif // WIO_TEST