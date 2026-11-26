#include <wio_native_abi.h>

#if defined(_WIN32)
#define WIO_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" WIO_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry()
{
    return nullptr;
}
