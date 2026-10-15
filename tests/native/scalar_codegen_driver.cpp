#define main scalar_generated_main
#include "main.wio.cpp"
#undef main

int main()
{
    static_assert(sizeof(wio::wir_backend::ScalarPlace<double>) <= 2 * sizeof(double));
    auto cell = wio::wir_backend::ScalarPlace<int>::local();
    bool rejected = false;
    try
    {
        (void)cell.read();
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    if (!rejected)
        return 10;
    cell.write(42);
    if (cell.read() != 42)
        return 11;
    cell.clear();
    rejected = false;
    try
    {
        (void)cell.read();
    }
    catch (const std::runtime_error&)
    {
        rejected = true;
    }
    if (!rejected)
        return 12;
    cell.write(7);
    if (cell.read() != 7)
        return 13;
    return scalar_generated_main();
}
