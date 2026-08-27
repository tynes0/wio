#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <wio_sdk.h>

namespace
{
    struct Resource
    {
        std::int32_t value;
        std::int32_t* releases;
    };

    void releaseResource(void* state) noexcept
    {
        auto* resource = static_cast<Resource*>(state);
        ++*resource->releases;
        delete resource;
    }
}

int main()
{
    static_assert(!std::is_copy_constructible_v<wio::sdk::UniqueNativeResource>);
    static_assert(!std::is_copy_assignable_v<wio::sdk::UniqueNativeResource>);
    static_assert(std::is_nothrow_move_constructible_v<wio::sdk::UniqueNativeResource>);

    std::int32_t releases = 0;
    {
        wio::sdk::UniqueNativeResource first(WioOwnedNativeResource{
            new Resource{42, &releases},
            "example.Resource",
            WIO_NATIVE_RESOURCE_RELEASE_THREAD_SAFE,
            0u,
            &releaseResource
        });
        assert(first);
        assert(first.type_name() == "example.Resource");
        assert(first.release_is_thread_safe());
        assert(static_cast<Resource*>(first.borrow().state)->value == 42);

        wio::sdk::UniqueNativeResource second(std::move(first));
        assert(!first);
        assert(second);

        WioOwnedNativeResource transferred = second.into_abi();
        assert(!second);
        WioReleaseNativeResource(&transferred);
        WioReleaseNativeResource(&transferred);
    }
    assert(releases == 1);

    bool rejected = false;
    try
    {
        wio::sdk::UniqueNativeResource invalid(WioOwnedNativeResource{
            reinterpret_cast<void*>(1), "invalid", 0u, 0u, nullptr
        });
    }
    catch (const wio::sdk::Error& error)
    {
        rejected = error.code() == wio::sdk::ErrorCode::InvalidArgument;
    }
    assert(rejected);
    return 0;
}
