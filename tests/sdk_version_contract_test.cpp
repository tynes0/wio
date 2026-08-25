#include <cstdint>
#include <iostream>
#include <string_view>

#include <wio_sdk.h>

#ifndef WIO_EXPECTED_PRODUCT_VERSION
#error "WIO_EXPECTED_PRODUCT_VERSION must be supplied by the build."
#endif

int main()
{
    static_assert(WIO_SDK_VERSION_MAJOR == 0);
    static_assert(WIO_SDK_VERSION_MINOR == 15);
    static_assert(WIO_SDK_VERSION_PATCH == 0);
    static_assert(wio::sdk::product_version.major == WIO_SDK_VERSION_MAJOR);
    static_assert(wio::sdk::product_version.minor == WIO_SDK_VERSION_MINOR);
    static_assert(wio::sdk::product_version.patch == WIO_SDK_VERSION_PATCH);
    static_assert(!wio::sdk::product_version.is_prerelease());

    if (wio::sdk::product_version_string != WIO_EXPECTED_PRODUCT_VERSION)
    {
        std::cerr << "SDK product version " << wio::sdk::product_version_string
                  << " does not match Wio " << WIO_EXPECTED_PRODUCT_VERSION << ".\n";
        return 1;
    }

    if (WIO_MODULE_API_DESCRIPTOR_VERSION != 10u)
    {
        std::cerr << "Unexpected module ABI descriptor version.\n";
        return 2;
    }

    std::cout << "Wio SDK " << wio::sdk::product_version_string
              << " (ABI descriptor " << WIO_MODULE_API_DESCRIPTOR_VERSION << ")\n";
    return 0;
}
