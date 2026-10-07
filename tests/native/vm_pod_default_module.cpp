#include <wio_native_pod.h>

namespace
{
    constexpr std::uint64_t PodId = 910;
    std::int32_t token = 47;

    struct Pod
    {
        void* pointer = &token;
    };

    const WioNativeAbiHandleOps Operations{WIO_NATIVE_ABI_VERSION, 0, [](void*) noexcept {},
                                           [](void* value) noexcept { delete static_cast<Pod*>(value); },
                                           [](const void*) noexcept { return PodId; }};

    WioNativeAbiStatus create(WioNativeAbiValue* result, WioNativeAbiFailure*) noexcept
    {
        auto* value = new Pod{};
        *result = {};
        result->kind = WIO_NATIVE_ABI_POD;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = PodId;
        result->payload.slice = {value, sizeof(Pod)};
        result->owner = {value, &Operations, 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus pack(const WioNativeAbiValue*, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                            WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiStatus read(const WioNativeAbiValue* value, std::uint64_t index, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (index != 0)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        *result = {};
        result->kind = WIO_NATIVE_ABI_OPAQUE;
        result->typeId = 911;
        result->payload.opaque = static_cast<const Pod*>(value->payload.slice.data)->pointer;
        return WIO_NATIVE_ABI_OK;
    }

    const WioNativePodFieldDescriptor field{"pointer", 911, 0, sizeof(void*), 20};
    const WioNativePodLayoutDescriptor layout{PodId, sizeof(Pod), alignof(Pod), 1, &field, &pack, &read};
    const WioNativePodLayoutApi layouts{WIO_NATIVE_POD_LAYOUT_VERSION, 1, &layout};
    const WioNativePodDefaultDescriptor factory{PodId, &create};
    const WioNativePodDefaultApi defaults{WIO_NATIVE_POD_DEFAULT_VERSION, 1, &factory};
    const WioNativeAbiRegistry registry{WIO_NATIVE_ABI_VERSION, 0, nullptr};
} // namespace

#ifdef _WIN32
#define WIO_DEFAULT_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_DEFAULT_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" WIO_DEFAULT_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept
{
    return &registry;
}

extern "C" WIO_DEFAULT_TEST_EXPORT const WioNativePodLayoutApi* WioGetNativePodLayoutApi() noexcept
{
    return &layouts;
}

extern "C" WIO_DEFAULT_TEST_EXPORT const WioNativePodDefaultApi* WioGetNativePodDefaultApi() noexcept
{
    return &defaults;
}
