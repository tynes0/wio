#include <iostream>
#include <fstream>

#include "src/lexer.h"
#include "src/parser.h"
#include "src/utils/filesystem.h"
#include "src/application.h"

#define WIO_TEST 1

#if WIO_TEST

int main(int argc, char* argv[])
{
    const char* args[] = { "-t", "-p","tests/test1.wio"};
    wio::application app(sizeof(args) / sizeof(const char*), args);
    app.run();
    long long a = 5;
    double x = 2.5;
}

#else // WIO_TEST

int main(int argc, char* argv[])
{
    wio::application app(argc, argv);
    app.run();
}

#endif // WIO_TEST