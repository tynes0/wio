#include <wio_native_pod.h>

#include <array>
#include <atomic>
#include <cstddef>

#if defined(_WIN32)
#define WIO_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace
{
    int tokens[]{17, 29};
    struct Pod
    {
        void* pointer;
        std::array<void*, 2> slots;
    };

    struct Owner
    {
        std::atomic_uint32_t references{1};
        std::uint64_t typeId;
        int token = 41;
        Pod value{&token, {&tokens[0], &tokens[1]}};
    };

    const WioNativeAbiHandleOps Operations{
        WIO_NATIVE_ABI_VERSION, 0, [](void* storage) noexcept { ++static_cast<Owner*>(storage)->references; },
        [](void* storage) noexcept
        {
            auto* owner = static_cast<Owner*>(storage);
            if (--owner->references == 0)
                delete owner;
        },
        [](const void* storage) noexcept { return static_cast<const Owner*>(storage)->typeId; }};

    WioNativeAbiStatus make(const WioNativeAbiValue*, std::uint64_t count, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (count || !result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* owner = new Owner{.typeId = 500};
        *result = {};
        result->kind = WIO_NATIVE_ABI_POD;
        result->typeId = 500;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->payload.slice = {&owner->value, sizeof(Pod)};
        result->owner = {owner, &Operations, 1};
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus unusedPack(const WioNativeAbiValue*, const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue*,
                                  WioNativeAbiFailure*) noexcept
    {
        return WIO_NATIVE_ABI_INVALID_ARGUMENT;
    }

    WioNativeAbiStatus read(const WioNativeAbiValue*, std::uint64_t index, WioNativeAbiValue* result,
                            WioNativeAbiFailure*) noexcept
    {
        if (index > 1 || !result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        auto* owner = new Owner{.typeId = index ? 503u : 501u};
        *result = {};
        result->kind = index ? WIO_NATIVE_ABI_POD : WIO_NATIVE_ABI_OPAQUE;
        result->typeId = owner->typeId;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->owner = {owner, &Operations, 1};
        if (index)
            result->payload.slice = {&owner->value.slots, sizeof(owner->value.slots)};
        else
            result->payload.opaque = &owner->token;
        return WIO_NATIVE_ABI_OK;
    }

    const WioNativeAbiFunctionDescriptor Functions[]{
        {WIO_NATIVE_ABI_VERSION, 0, 701, "test:vm-pod-pointer-lifetime", "make", "make", &make}};
    const WioNativeAbiRegistry Registry{WIO_NATIVE_ABI_VERSION, 1, Functions};
    const WioNativePodFieldDescriptor Fields[]{{"pointer", 501, offsetof(Pod, pointer), sizeof(void*), 20},
                                               {"slots", 503, offsetof(Pod, slots), sizeof(Pod{}.slots), 31}};
    const WioNativePodLayoutDescriptor Layouts[]{{500, sizeof(Pod), alignof(Pod), 2, Fields, &unusedPack, &read}};
    const WioNativePodLayoutApi LayoutApi{WIO_NATIVE_POD_LAYOUT_VERSION, 1, Layouts};
} // namespace

extern "C" WIO_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept
{
    return &Registry;
}

extern "C" WIO_TEST_EXPORT const WioNativePodLayoutApi* WioGetNativePodLayoutApi() noexcept
{
    return &LayoutApi;
}
