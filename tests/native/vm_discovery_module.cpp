#include <wio_native_abi.h>
#include <atomic>

#if defined(_WIN32)
#define WIO_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace
{
    constexpr std::uint64_t ObjectTypeId = 0x20'05'00u;
    std::atomic_uint32_t ShutdownCount{0};
    struct Object final
    {
        std::atomic_uint32_t references{1};
    };
    const WioNativeAbiHandleOps ObjectOps{
        WIO_NATIVE_ABI_VERSION,
        0,
        +[](void* state) noexcept { static_cast<Object*>(state)->references.fetch_add(1); },
        +[](void* state) noexcept
        {
            if (static_cast<Object*>(state)->references.fetch_sub(1) == 1)
                delete static_cast<Object*>(state);
        },
        +[](const void*) noexcept -> std::uint64_t { return ObjectTypeId; },
    };

    WioNativeAbiStatus makeObject(const WioNativeAbiValue*, std::uint64_t, WioNativeAbiValue* result,
                                  WioNativeAbiFailure*) noexcept
    {
        if (!result)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        *result = {};
        result->kind = WIO_NATIVE_ABI_OBJECT;
        result->flags = WIO_NATIVE_ABI_VALUE_OWNED;
        result->typeId = ObjectTypeId;
        result->payload.handle = {new Object{}, &ObjectOps, 0};
        return WIO_NATIVE_ABI_OK;
    }
    WioNativeAbiStatus answer(const WioNativeAbiValue*, const std::uint64_t argumentCount, WioNativeAbiValue* result,
                              WioNativeAbiFailure* failure) noexcept
    {
        if (argumentCount != 0 || !result)
        {
            if (failure)
                failure->status = WIO_NATIVE_ABI_INVALID_ARGUMENT;
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        }
        *result = {};
        result->kind = WIO_NATIVE_ABI_I64;
        result->payload.signedInteger = 42 + ShutdownCount.load();
        return WIO_NATIVE_ABI_OK;
    }

    const WioNativeAbiFunctionDescriptor Functions[]{
        {WIO_NATIVE_ABI_VERSION, 0, 0x20'05u, "test:vm-discovery-answer", "answer", "_wio_test_vm_discovery_answer",
         &answer},
        {WIO_NATIVE_ABI_VERSION, 0, 0x20'06u, "test:vm-discovery-object", "makeObject", "_wio_test_vm_discovery_object",
         &makeObject},
    };
    const WioNativeAbiRegistry Registry{WIO_NATIVE_ABI_VERSION, 2, Functions};
} // namespace

extern "C" WIO_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry() noexcept
{
    return &Registry;
}

// A legacy task sidecar without completion registration must still permit
// synchronous calls. Its shutdown counter also observes shared machine leases.
extern "C" WIO_TEST_EXPORT const WioNativeTaskApi* WioGetNativeTaskApi() noexcept
{
    static const WioNativeTaskApi tasks{
        WIO_NATIVE_ABI_VERSION, nullptr, nullptr, nullptr, nullptr, nullptr, +[]() noexcept { ++ShutdownCount; }};
    return &tasks;
}
