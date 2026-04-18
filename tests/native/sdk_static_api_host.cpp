#include <cstdint>
#include <iostream>
#include <wio_sdk.h>

extern "C" const WioModuleApi* WioModuleGetApi();

int main()
{
    const WioModuleApi* api = WioModuleGetApi();
    auto addNumbers = wio_load_function<std::function<std::int32_t(std::int32_t, std::int32_t)>>(api, "AddNumbers");
    std::cout << "SDK static: " << addNumbers(19, 23);
    return 0;
}
