#include "probe.h"

int main()
{
    if (native_costs::Hash("abc") != 16654208175385433931ULL)
        return 1;
    if (native_costs::ParseCount("2000") != 2000)
        return 2;
    for (const auto invalid : {"", "0", "-1", "100001", "12tail"})
    {
        try
        {
            native_costs::ParseCount(invalid);
            return 3;
        }
        catch (const std::invalid_argument&)
        {
        }
    }

    native_costs::Begin();
    void* first = ::operator new(123);
    void* second = ::operator new[](45);
    native_costs::counting.store(false);
    ::operator delete(first);
    ::operator delete[](second);
    if (native_costs::allocations.load() != 2 || native_costs::requestedBytes.load() != 168)
        return 4;
    native_costs::Record(99);
    if (native_costs::requestedBytes.load() != 168)
        return 5;
    native_costs::Begin();
    return native_costs::Report(42);
}
