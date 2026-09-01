#pragma once

#include <cstddef>
#include <cstdint>

// Stable C-shaped contract used by generated native thunks. Public C++
// libraries remain free to use classes, templates and overloads: the compiler
// emits one concrete thunk per Wio-visible specialization and both the native
// backend and VM call that thunk through this surface.
inline constexpr std::uint32_t WIO_NATIVE_ABI_VERSION = 1u;

enum WioNativeAbiStatus : std::int32_t
{
    WIO_NATIVE_ABI_OK = 0,
    WIO_NATIVE_ABI_INVALID_ARGUMENT = 1,
    WIO_NATIVE_ABI_TYPE_MISMATCH = 2,
    WIO_NATIVE_ABI_EXCEPTION = 3,
    WIO_NATIVE_ABI_PANIC = 4,
    WIO_NATIVE_ABI_WRONG_THREAD = 5,
    WIO_NATIVE_ABI_STALE_HANDLE = 6
};

enum WioNativeAbiValueKind : std::uint32_t
{
    WIO_NATIVE_ABI_VOID = 0,
    WIO_NATIVE_ABI_BOOL,
    WIO_NATIVE_ABI_I64,
    WIO_NATIVE_ABI_U64,
    WIO_NATIVE_ABI_F64,
    WIO_NATIVE_ABI_BYTES,
    WIO_NATIVE_ABI_TEXT_UTF32,
    WIO_NATIVE_ABI_POD,
    WIO_NATIVE_ABI_OPAQUE,
    WIO_NATIVE_ABI_OBJECT,
    WIO_NATIVE_ABI_CALLBACK,
    WIO_NATIVE_ABI_RUNTIME_VALUE
};

enum WioNativeAbiValueFlag : std::uint32_t
{
    WIO_NATIVE_ABI_VALUE_NONE = 0u,
    WIO_NATIVE_ABI_VALUE_NULLABLE = 1u << 0u,
    WIO_NATIVE_ABI_VALUE_BORROWED = 1u << 1u,
    WIO_NATIVE_ABI_VALUE_MUTABLE = 1u << 2u,
    WIO_NATIVE_ABI_VALUE_CONSUMED = 1u << 3u,
    WIO_NATIVE_ABI_VALUE_OWNED = 1u << 4u
};

struct WioNativeAbiSlice
{
    const void* data = nullptr;
    std::uint64_t size = 0;
};

struct WioNativeAbiMutableSlice
{
    void* data = nullptr;
    std::uint64_t size = 0;
};

struct WioNativeAbiHandleOps
{
    std::uint32_t abiVersion = WIO_NATIVE_ABI_VERSION;
    std::uint32_t reserved = 0;
    void (*retain)(void* state) noexcept = nullptr;
    void (*release)(void* state) noexcept = nullptr;
    std::uint64_t (*typeId)(const void* state) noexcept = nullptr;
};

// The owner that created a handle also supplies its operations. This keeps
// intrusive reference counting identical across DLL, native backend and VM
// boundaries and prevents either side from deleting foreign memory directly.
struct WioNativeAbiHandle
{
    void* state = nullptr;
    const WioNativeAbiHandleOps* ops = nullptr;
    std::uint64_t generation = 0;
};

struct WioNativeAbiCallbackOps
{
    std::uint32_t abiVersion = WIO_NATIVE_ABI_VERSION;
    std::uint32_t flags = 0;
    void (*retain)(void* userdata) noexcept = nullptr;
    void (*release)(void* userdata) noexcept = nullptr;
    WioNativeAbiStatus (*invoke)(
        void* userdata,
        const struct WioNativeAbiValue* arguments,
        std::uint64_t argumentCount,
        struct WioNativeAbiValue* result,
        struct WioNativeAbiFailure* failure) noexcept = nullptr;
};

struct WioNativeAbiCallback
{
    void* userdata = nullptr;
    const WioNativeAbiCallbackOps* ops = nullptr;
};

struct WioNativeAbiValue
{
    WioNativeAbiValueKind kind = WIO_NATIVE_ABI_VOID;
    std::uint32_t flags = WIO_NATIVE_ABI_VALUE_NONE;
    std::uint64_t typeId = 0;
    union Payload
    {
        bool boolean;
        std::int64_t signedInteger;
        std::uint64_t unsignedInteger;
        double floatingPoint;
        WioNativeAbiSlice slice;
        WioNativeAbiMutableSlice mutableSlice;
        WioNativeAbiHandle handle;
        WioNativeAbiCallback callback;
        void* opaque;

        constexpr Payload() : unsignedInteger(0) {}
    } payload{};
};

struct WioNativeAbiFailure
{
    WioNativeAbiStatus status = WIO_NATIVE_ABI_OK;
    std::int64_t nativeCode = 0;
    WioNativeAbiSlice category{};
    WioNativeAbiSlice message{};
};

using WioNativeAbiThunk = WioNativeAbiStatus(*)(
    const WioNativeAbiValue* arguments,
    std::uint64_t argumentCount,
    WioNativeAbiValue* result,
    WioNativeAbiFailure* failure) noexcept;

struct WioNativeAbiFunctionDescriptor
{
    std::uint32_t abiVersion = WIO_NATIVE_ABI_VERSION;
    std::uint32_t flags = 0;
    std::uint64_t stableId = 0;
    const char* stableKey = nullptr;
    const char* nativeSymbol = nullptr;
    const char* thunkSymbol = nullptr;
    WioNativeAbiThunk thunk = nullptr;
};

inline void WioNativeAbiRetain(WioNativeAbiHandle handle) noexcept
{
    if (handle.state != nullptr && handle.ops != nullptr && handle.ops->retain != nullptr)
        handle.ops->retain(handle.state);
}

inline void WioNativeAbiRelease(WioNativeAbiHandle* handle) noexcept
{
    if (handle == nullptr)
        return;
    WioNativeAbiHandle owned = *handle;
    *handle = {};
    if (owned.state != nullptr && owned.ops != nullptr && owned.ops->release != nullptr)
        owned.ops->release(owned.state);
}
