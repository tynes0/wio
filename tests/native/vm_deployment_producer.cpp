#include "vm_deployment_calc.h"
#include <wio_module_contract.h>
#include <wio_native_abi.h>

#include <atomic>
#include <string>

#if defined(_WIN32)
#define WIO_TEST_EXPORT __declspec(dllexport)
#else
#define WIO_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace
{
    std::atomic<int> loads{0}, unloads{0}, shutdowns{0};

    struct String
    {
        std::atomic<unsigned> references{1};
        std::string value;
        explicit String(std::string text) : value(std::move(text))
        {
        }
    };

    const WioNativeAbiHandleOps StringOps{WIO_NATIVE_ABI_VERSION, 0,
                                          [](void* state) noexcept { ++static_cast<String*>(state)->references; },
                                          [](void* state) noexcept
                                          {
                                              auto* value = static_cast<String*>(state);
                                              if (--value->references == 0)
                                                  delete value;
                                          },
                                          [](const void*) noexcept -> std::uint64_t { return 0; }};

    WioNativeAbiStatus add(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* output,
                           WioNativeAbiFailure*) noexcept
    {
        if (count != 2 || !args || !output || args[0].kind != WIO_NATIVE_ABI_I64 || args[1].kind != WIO_NATIVE_ABI_I64)
            return WIO_NATIVE_ABI_TYPE_MISMATCH;
        *output = {};
        output->kind = WIO_NATIVE_ABI_I64;
        output->payload.signedInteger = WioDeploymentAdd(static_cast<int>(args[0].payload.signedInteger),
                                                         static_cast<int>(args[1].payload.signedInteger));
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus decorate(const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* output,
                                WioNativeAbiFailure*) noexcept
    {
        if (count != 1 || !args || !output || args[0].kind != WIO_NATIVE_ABI_BYTES)
            return WIO_NATIVE_ABI_TYPE_MISMATCH;
        try
        {
            const auto& input = args[0].payload.slice;
            auto* text = new String{"native:" + (input.size ? std::string{static_cast<const char*>(input.data),
                                                                          static_cast<std::size_t>(input.size)}
                                                            : std::string{})};
            *output = {};
            output->kind = WIO_NATIVE_ABI_BYTES;
            output->flags = WIO_NATIVE_ABI_VALUE_OWNED;
            output->payload.slice = {text->value.data(), text->value.size()};
            output->owner = {text, &StringOps, 1};
            return WIO_NATIVE_ABI_OK;
        }
        catch (...)
        {
            return WIO_NATIVE_ABI_EXCEPTION;
        }
    }

    WioNativeAbiStatus initialize(const WioNativeAbiValue*, std::uint64_t count, WioNativeAbiValue* output,
                                  WioNativeAbiFailure*) noexcept
    {
        if (count || !output)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        ++loads;
        *output = {};
        output->kind = WIO_NATIVE_ABI_I64;
        return WIO_NATIVE_ABI_OK;
    }

    WioNativeAbiStatus finalize(const WioNativeAbiValue*, std::uint64_t count, WioNativeAbiValue* output,
                                WioNativeAbiFailure*) noexcept
    {
        if (count || !output)
            return WIO_NATIVE_ABI_INVALID_ARGUMENT;
        ++unloads;
        *output = {};
        return WIO_NATIVE_ABI_OK;
    }

    const WioNativeAbiFunctionDescriptor Functions[] = {
        {WIO_NATIVE_ABI_VERSION, 0, 1, "cpp:deploy_native::Add:fn(i32, i32) -> i32", "deploy_native::Add", "add", &add},
        {WIO_NATIVE_ABI_VERSION, 0, 2, "cpp:deploy_native::Decorate:fn(string) -> string", "deploy_native::Decorate",
         "decorate", &decorate},
        {WIO_NATIVE_ABI_VERSION, 0, 3, "cpp:deploy_native::Initialize:fn() -> i32", "deploy_native::Initialize",
         "initialize", &initialize},
        {WIO_NATIVE_ABI_VERSION, 0, 4, "cpp:deploy_native::Finalize:fn() -> void", "deploy_native::Finalize",
         "finalize", &finalize}};
    const WioNativeAbiRegistry Registry{WIO_NATIVE_ABI_VERSION, 4, Functions};
    const WioNativeTaskApi Tasks{WIO_NATIVE_ABI_VERSION,
                                 [](const WioNativeAbiValue*, bool*) noexcept
                                 { return WIO_NATIVE_ABI_INVALID_ARGUMENT; },
                                 [](const WioNativeAbiValue*) noexcept { return WIO_NATIVE_ABI_INVALID_ARGUMENT; },
                                 [](const WioNativeAbiValue*, WioNativeAbiValue*, WioNativeAbiFailure*) noexcept
                                 { return WIO_NATIVE_ABI_INVALID_ARGUMENT; },
                                 []() noexcept {},
                                 []() noexcept -> std::uint64_t { return 0; },
                                 []() noexcept { ++shutdowns; }};
    const WioNativeTaskCompletionApi Completions{WIO_NATIVE_ABI_VERSION,
                                                 [](const WioNativeAbiValue*, WioNativeTaskCompletionFn, void*) noexcept
                                                 { return WIO_NATIVE_ABI_INVALID_ARGUMENT; }};
} // namespace

extern "C" WIO_TEST_EXPORT const WioNativeAbiRegistry* WioGetNativeAbiRegistry()
{
    return &Registry;
}
extern "C" WIO_TEST_EXPORT const WioNativeTaskApi* WioGetNativeTaskApi()
{
    return &Tasks;
}
extern "C" WIO_TEST_EXPORT const WioNativeTaskCompletionApi* WioGetNativeTaskCompletionApi()
{
    return &Completions;
}
extern "C" WIO_TEST_EXPORT int WioDeploymentLoads()
{
    return loads;
}
extern "C" WIO_TEST_EXPORT int WioDeploymentUnloads()
{
    return unloads;
}
extern "C" WIO_TEST_EXPORT int WioDeploymentShutdowns()
{
    return shutdowns;
}
