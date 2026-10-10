#pragma once

#include <cstddef>
#include <cstdint>

// Stable C-shaped contract used by generated native thunks. Public C++
// libraries remain free to use classes, templates and overloads: the compiler
// emits one concrete thunk per Wio-visible specialization and both the native
// backend and VM call that thunk through this surface.
inline constexpr std::uint32_t WIO_NATIVE_ABI_VERSION = 2u;

enum WioNativeAbiStatus : std::int32_t
{
    WIO_NATIVE_ABI_OK = 0,
    WIO_NATIVE_ABI_INVALID_ARGUMENT = 1,
    WIO_NATIVE_ABI_TYPE_MISMATCH = 2,
    WIO_NATIVE_ABI_EXCEPTION = 3,
    WIO_NATIVE_ABI_PANIC = 4,
    WIO_NATIVE_ABI_WRONG_THREAD = 5,
    WIO_NATIVE_ABI_STALE_HANDLE = 6,
    WIO_NATIVE_ABI_NOT_READY = 7,
    WIO_NATIVE_ABI_CANCELLED = 8
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
    WIO_NATIVE_ABI_RUNTIME_VALUE,
    WIO_NATIVE_ABI_REFERENCE
};

enum WioNativeAbiValueFlag : std::uint32_t
{
    WIO_NATIVE_ABI_VALUE_NONE = 0u,
    WIO_NATIVE_ABI_VALUE_NULLABLE = 1u << 0u,
    WIO_NATIVE_ABI_VALUE_BORROWED = 1u << 1u,
    WIO_NATIVE_ABI_VALUE_MUTABLE = 1u << 2u,
    WIO_NATIVE_ABI_VALUE_CONSUMED = 1u << 3u,
    WIO_NATIVE_ABI_VALUE_OWNED = 1u << 4u,
    // Additive reference form; payload points to WioNativeAbiProjection.
    WIO_NATIVE_ABI_VALUE_PROJECTED = 1u << 5u,
    // Additive logical-index form; payload is WioNativeAbiSequenceProjection.
    WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED = 1u << 6u,
    // Logical array indices followed by a registered POD field projection.
    WIO_NATIVE_ABI_VALUE_SEQUENCE_POD_PROJECTED = 1u << 7u,
    // Mixed dynamic-array indices / dictionary key lookups, optionally followed by POD fields.
    WIO_NATIVE_ABI_VALUE_CONTAINER_PROJECTED = 1u << 8u
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
    WioNativeAbiStatus (*invoke)(void* userdata, const struct WioNativeAbiValue* arguments, std::uint64_t argumentCount,
                                 struct WioNativeAbiValue* result,
                                 struct WioNativeAbiFailure* failure) noexcept = nullptr;
};

enum WioNativeAbiCallbackFlag : std::uint32_t
{
    WIO_NATIVE_ABI_CALLBACK_CALLER_THREAD = 0u,
    WIO_NATIVE_ABI_CALLBACK_ANY_THREAD = 1u << 0u
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

        constexpr Payload() : unsignedInteger(0)
        {
        }
    } payload{};
    // v2: storage owner for slices/POD. Object/runtime handles own themselves.
    WioNativeAbiHandle owner{};
};

// Call-scoped projection into one producer-materialized POD reference token.
// root points to a tagged POD value, never to VM component storage. Offsets
// and extents come from validated producer layout metadata.
struct WioNativeAbiProjection
{
    WioNativeAbiValue* root = nullptr;
    std::uint64_t rootTypeId = 0;
    std::uint64_t typeId = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

// Call-scoped logical indices into one producer-materialized dynamic array.
// No std::vector layout or VM Value-cell address crosses the boundary. The producer
// resolves the path against the root Frame slot before invoking native code.
struct WioNativeAbiSequenceProjection
{
    WioNativeAbiValue* root = nullptr;
    std::uint64_t rootTypeId = 0;
    std::uint64_t typeId = 0;
    const std::uint64_t* indices = nullptr;
    std::uint64_t count = 0;
};

enum WioNativeAbiFunctionFlag : std::uint32_t
{
    WIO_NATIVE_ABI_FUNCTION_PROJECTED_REFERENCES = 1u << 0u,
    WIO_NATIVE_ABI_FUNCTION_SEQUENCE_PROJECTIONS = 1u << 1u,
    WIO_NATIVE_ABI_FUNCTION_SEQUENCE_POD_PROJECTIONS = 1u << 2u,
    WIO_NATIVE_ABI_FUNCTION_CONTAINER_PROJECTIONS = 1u << 3u
};

// Additive form: the existing projection structures retain their exact sizes.
// The producer validates the terminal POD identity and declared field path;
// offsets cannot address padding or undeclared fields.
struct WioNativeAbiSequencePodProjection
{
    WioNativeAbiSequenceProjection sequence{};
    std::uint64_t podTypeId = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

enum WioNativeAbiContainerStepKind : std::uint32_t
{
    WIO_NATIVE_ABI_CONTAINER_INDEX = 0,
    WIO_NATIVE_ABI_CONTAINER_VALUE = 1
};

// Key wires use portable scalar/string/text/enum contracts, never C++ map node
// addresses or unordered iteration indices. INDEX steps carry an empty key.
struct WioNativeAbiContainerStep
{
    std::uint32_t kind = WIO_NATIVE_ABI_CONTAINER_INDEX;
    std::uint32_t reserved = 0;
    std::uint64_t containerTypeId = 0;
    std::uint64_t index = 0;
    WioNativeAbiValue key{};
};

struct WioNativeAbiContainerProjection
{
    WioNativeAbiValue* root = nullptr;
    std::uint64_t rootTypeId = 0;
    std::uint64_t typeId = 0;
    const WioNativeAbiContainerStep* steps = nullptr;
    std::uint64_t count = 0;
    std::uint64_t podTypeId = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
};

struct WioNativeAbiFailure
{
    WioNativeAbiStatus status = WIO_NATIVE_ABI_OK;
    std::int64_t nativeCode = 0;
    WioNativeAbiSlice category{};
    WioNativeAbiSlice message{};
};

using WioNativeAbiThunk = WioNativeAbiStatus (*)(const WioNativeAbiValue* arguments, std::uint64_t argumentCount,
                                                 WioNativeAbiValue* result, WioNativeAbiFailure* failure) noexcept;

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

struct WioNativeAbiRegistry
{
    std::uint32_t abiVersion;
    std::uint32_t functionCount;
    const WioNativeAbiFunctionDescriptor* functions;
};
using WioGetNativeAbiRegistryFn = const WioNativeAbiRegistry* (*)();

struct WioNativeTaskApi
{
    std::uint32_t abiVersion;
    WioNativeAbiStatus (*ready)(const WioNativeAbiValue*, bool*) noexcept;
    WioNativeAbiStatus (*cancel)(const WioNativeAbiValue*) noexcept;
    WioNativeAbiStatus (*read)(const WioNativeAbiValue*, WioNativeAbiValue*, WioNativeAbiFailure*) noexcept;
    void (*bindMain)() noexcept;
    std::uint64_t (*pumpMain)() noexcept;
    void (*shutdown)() noexcept;
};
using WioGetNativeTaskApiFn = const WioNativeTaskApi* (*)();

// Completion registration is a separate additive sidecar so wire-v2 modules
// built before native task adaptation keep the original WioNativeTaskApi
// layout. A successful subscription owns one exactly-once callback delivery;
// delivery may happen synchronously or from any foreign thread.
using WioNativeTaskCompletionFn = void (*)(void*, WioNativeAbiStatus) noexcept;
struct WioNativeTaskCompletionApi
{
    std::uint32_t abiVersion;
    WioNativeAbiStatus (*onComplete)(const WioNativeAbiValue*, WioNativeTaskCompletionFn, void*) noexcept;
};
using WioGetNativeTaskCompletionApiFn = const WioNativeTaskCompletionApi* (*)();

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

inline void WioNativeAbiReleaseValue(WioNativeAbiValue* value) noexcept
{
    if (!value)
        return;
    if (value->flags & WIO_NATIVE_ABI_VALUE_OWNED)
    {
        if (value->kind == WIO_NATIVE_ABI_OBJECT || value->kind == WIO_NATIVE_ABI_RUNTIME_VALUE)
            WioNativeAbiRelease(&value->payload.handle);
        if (value->kind == WIO_NATIVE_ABI_CALLBACK && value->payload.callback.ops &&
            value->payload.callback.ops->release)
            value->payload.callback.ops->release(value->payload.callback.userdata);
        WioNativeAbiRelease(&value->owner);
    }
    *value = {};
}
