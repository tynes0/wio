#include <cstdint>
#include <iostream>
#include <wio_sdk.h>

extern "C" const WioModuleApi* WioModuleGetApi();

int main()
{
    const WioModuleApi* api = WioModuleGetApi();
    auto addNumbers = wio_load_export<std::int32_t(std::int32_t, std::int32_t)>(api, "AddNumbers");
    auto weightedScore = wio_load_function<std::int32_t(std::int32_t, std::int32_t)>(api, "math.weighted");

    std::cout << "Static consumer: export=" << addNumbers(19, 23)
              << " command=" << weightedScore(20, 15);
    return 0;
}
