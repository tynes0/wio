#include <wio_native_abi.h>

#if defined(_WIN32)
#define WIO_EXPORT __declspec(dllexport)
#else
#define WIO_EXPORT __attribute__((visibility("default")))
#endif

extern "C" WIO_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept
{
    static const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 0, nullptr};
    return &registry;
}
