#define WIO_BENCHMARK_ALLOCATIONS
#include "std_benchmark.h"
#include "std_serialization.h"
#include <iostream>

int main()
{
    using wio::runtime::std_serialization::FormatJsonNumber;
    if (FormatJsonNumber(0.0) != "0.0" || FormatJsonNumber(-0.0) != "-0.0" ||
        FormatJsonNumber(std::numeric_limits<double>::infinity()) != "null" ||
        FormatJsonNumber(std::numeric_limits<double>::quiet_NaN()) != "null")
        return 8;
    using namespace wio::runtime::benchmark;
    auto* previous = ::operator new(17);
    if (!BeginAllocations() || BeginAllocations())
        return 1;
    auto* one = ::operator new(64);
    auto* two = ::operator new(128, std::align_val_t{256});
    auto* zero = ::operator new(0);
    if (reinterpret_cast<std::uintptr_t>(two) % 256)
        return 2;
    ::operator delete(previous);
    auto live = EndAllocations();
    if (!live.available || live.allocations != 3 || live.requestedBytes != 192 || live.liveBytes != 192 ||
        live.liveBlocks != 3 || live.peakBytes != 192 || BeginAllocations())
        return 3;
    ::operator delete(one);
    ::operator delete(two, std::align_val_t{256});
    ::operator delete(zero);
    auto released = Allocations();
    if (released.liveBytes || released.liveBlocks || released.frees != 3 || released.freedBytes != 192)
        return 4;
    if (!BeginAllocations())
        return 5;
    auto* failed = ::operator new((std::numeric_limits<std::size_t>::max)(), std::nothrow);
    if (failed)
        return 6;
    auto end = EndAllocations();
    if (end.failures != 1 || end.liveBlocks || end.allocations)
        return 7;
    std::cout << "allocation calibration passed\n";
    return 0;
}
