// Manual diagnostic wrapper: compile with the measured snapshot include path
// and tests/native on the include path. Not part of the timed median experiment.
#include "scalar_codegen_probe.h"
#define main measured_generated_main
#ifdef MEASURE_BASELINE
#include "baseline.wio.cpp"
#else
#include "compute.wio.cpp"
#endif
#undef main

int main()
{
    scalar_codegen_probe::Begin();
    const int result = measured_generated_main();
    const auto allocations = scalar_codegen_probe::End();
    std::printf("program_new_calls=%llu\n", static_cast<unsigned long long>(allocations));
    return result;
}
