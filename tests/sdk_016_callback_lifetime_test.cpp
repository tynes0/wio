#include <atomic>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

#include <wio_sdk.h>

int main()
{
    WioHostCallback retained{};
    std::atomic<std::int32_t> calls{0};
    {
        wio::sdk::HostCallback<std::int32_t(std::int32_t)> callback(
            [&](const std::int32_t value)
            {
                calls.fetch_add(1);
                return value * 2;
            }, true);
        retained = callback.retained();
        assert((retained.flags & WIO_CALLBACK_RETAINABLE_USERDATA) != 0u);
        assert((retained.flags & WIO_CALLBACK_CONTAINS_FAILURES) != 0u);
        assert((retained.flags & WIO_CALLBACK_THREAD_SAFE) != 0u);
    }

    WioValue argument{};
    argument.type = WIO_ABI_I32;
    argument.value.v_i32 = 21;
    WioValue result{};
    std::int32_t status = WIO_CALLBACK_BAD_ARGUMENTS;
    std::thread eventThread([&]
    {
        status = WioInvokeHostCallback(&retained, &argument, 1u, &result);
    });
    eventThread.join();
    assert(status == WIO_CALLBACK_OK);
    assert(result.type == WIO_ABI_I32 && result.value.v_i32 == 42);
    assert(calls.load() == 1);
    WioReleaseHostCallback(&retained);

    wio::sdk::HostCallback<std::int32_t()> failing([]() -> std::int32_t
    {
        throw std::runtime_error("contained callback failure");
    });
    const WioHostCallback failingRaw = failing.borrowed();
    assert(WioInvokeHostCallback(&failingRaw, nullptr, 0u, &result) == WIO_CALLBACK_FAULTED);
    assert(std::string(failingRaw.lastError(failingRaw.userData)) == "contained callback failure");

    WioValue wrong{};
    wrong.type = WIO_ABI_F64;
    wrong.value.v_f64 = 21.0;
    wio::sdk::HostCallback<void(std::int32_t)> typed([](std::int32_t) {});
    const WioHostCallback typedRaw = typed.borrowed();
    assert(WioInvokeHostCallback(&typedRaw, &wrong, 1u, nullptr) == WIO_CALLBACK_TYPE_MISMATCH);
    return 0;
}
