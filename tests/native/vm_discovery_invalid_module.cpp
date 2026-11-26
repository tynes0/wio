#include <wio_native_abi.h>

#if defined(_WIN32)
#define WIO_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_TEST_EXPORT __attribute__((visibility("default")))
#endif

#if WIO_TEST_MODULE_MODE == 1
extern "C" WIO_TEST_EXPORT void NotAWioRegistry() noexcept
{
}
#else
extern "C" WIO_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept
{
    static const WioNativeAbiRegistry registry{
        WIO_TEST_MODULE_MODE == 2 ? WIO_NATIVE_ABI_VERSION + 1 : WIO_NATIVE_ABI_VERSION, 0, nullptr};
    return &registry;
}

#if WIO_TEST_MODULE_MODE == 3
extern "C" WIO_TEST_EXPORT const WioNativeTaskCompletionApi* WioGetNativeTaskCompletionApi() noexcept
{
    return nullptr;
}
#endif
#endif
