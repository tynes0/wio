#pragma once
#include <wio_native_abi.h>
#include <array>
#include <algorithm>
#include <initializer_list>
#include <bit>
#include <atomic>
#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <utility>
#include "ref.h"
#include "text.h"
#include "std_async.h"
#include "wir_sequence_projection.h"
#include "wir_container_projection.h"
#include "wir_borrow_identity.h"

namespace wio::wir_backend
{
    template <class T> class Place;
    // A callback borrow is an invocation capability, never closure-owned
    // storage. Keep only this tiny expiry token if a Place accidentally escapes.
    struct CallbackBorrowLifetime
    {
        std::atomic<bool> active{true};
    };
    inline thread_local std::shared_ptr<CallbackBorrowLifetime> activeCallbackBorrow;

    // Preserve borrow provenance through std::function's type erasure. Local
    // closures may borrow, but a native-owned callback must own its environment.
    inline thread_local bool nativeCallableInvocation = false;
    struct NativeCallableInvocationScope
    {
        bool previous = nativeCallableInvocation;
        explicit NativeCallableInvocationScope(bool native)
        {
            nativeCallableInvocation = nativeCallableInvocation || native;
        }
        ~NativeCallableInvocationScope()
        {
            nativeCallableInvocation = previous;
        }
    };
    template <class Signature> struct NativeCallableEnvironment;
    template <class R, class... A> struct NativeCallableEnvironment<R(A...)>
    {
        std::function<R(A...)> function;
        bool borrowed = false;
        bool nativeBound = false;
        R operator()(A... arguments)
        {
            NativeCallableInvocationScope scope{nativeBound};
            return function(std::forward<A>(arguments)...);
        }
    };
    template <class T> struct NativeCaptureBorrow
    {
        static bool contains(const T& value)
        {
            if constexpr (requires { value.borrowIdentity(); })
                return true;
            else if constexpr (requires { value.nativeCaptureHasBorrow(); })
                return value.nativeCaptureHasBorrow();
            else
                return false;
        }
    };
    template <class R, class... A> struct NativeCaptureBorrow<std::function<R(A...)>>
    {
        static bool contains(const std::function<R(A...)>& value)
        {
            const auto* environment = value.template target<NativeCallableEnvironment<R(A...)>>();
            return environment && environment->borrowed;
        }
    };
    template <class T> struct NativeCaptureBorrow<std::optional<T>>
    {
        static bool contains(const std::optional<T>& value)
        {
            return value && NativeCaptureBorrow<T>::contains(*value);
        }
    };
    template <class T, std::size_t N> struct NativeCaptureBorrow<std::array<T, N>>
    {
        static bool contains(const std::array<T, N>& value)
        {
            for (const auto& item : value)
                if (NativeCaptureBorrow<T>::contains(item))
                    return true;
            return false;
        }
    };
    template <class T, class Allocator> struct NativeCaptureBorrow<std::vector<T, Allocator>>
    {
        static bool contains(const std::vector<T, Allocator>& value)
        {
            for (const auto& item : value)
                if (NativeCaptureBorrow<T>::contains(item))
                    return true;
            return false;
        }
    };
    template <class K, class V, class Compare, class Allocator>
    struct NativeCaptureBorrow<std::map<K, V, Compare, Allocator>>
    {
        static bool contains(const std::map<K, V, Compare, Allocator>& value)
        {
            for (const auto& [key, item] : value)
                if (NativeCaptureBorrow<K>::contains(key) || NativeCaptureBorrow<V>::contains(item))
                    return true;
            return false;
        }
    };
    template <class Function, class... Captures>
    Function nativeCallableEnvironment(Function function, const Captures&... captures)
    {
        const bool borrowed = (NativeCaptureBorrow<std::decay_t<Captures>>::contains(captures) || ...);
        return []<class R, class... A>(std::function<R(A...)> value, bool hasBorrow)
        {
            return std::function<R(A...)>{NativeCallableEnvironment<R(A...)>{std::move(value), hasBorrow}};
        }(std::move(function), borrowed);
    }
    template <class R, class... A> void requireOwningNativeCallable(const std::function<R(A...)>& function)
    {
        if (NativeCaptureBorrow<std::function<R(A...)>>::contains(function))
            throw std::runtime_error(
                "Native callbacks cannot capture borrowed references; pass the caller borrow explicitly");
    }
    template <class R, class... A> void bindOwningNativeCallable(std::function<R(A...)>& function)
    {
        requireOwningNativeCallable(function);
        if (auto* environment = function.template target<NativeCallableEnvironment<R(A...)>>())
            environment->nativeBound = true;
    }
    template <class... Captures> void validateNativeCallableCaptures(const Captures&... captures)
    {
        if (nativeCallableInvocation && (NativeCaptureBorrow<std::decay_t<Captures>>::contains(captures) || ...))
            throw std::runtime_error("Native callback captured an expired or non-owning reference environment");
    }
    template <class T> T bindOwningNativeResult(T value)
    {
        if (NativeCaptureBorrow<T>::contains(value))
            throw std::runtime_error("Native callbacks cannot return a borrowed closure environment");
        if constexpr (requires { bindOwningNativeCallable(value); })
            bindOwningNativeCallable(value);
        return value;
    }
    template <class T> std::optional<T> bindOwningNativeResult(std::optional<T> value)
    {
        if (value)
            *value = bindOwningNativeResult(std::move(*value));
        return value;
    }
} // namespace wio::wir_backend

namespace wio::wir_backend::abi
{
    class Tasks
    {
        std::mutex mutex_;
        std::vector<std::weak_ptr<runtime::detail::AsyncTaskStateBase>> tasks_;

    public:
        static Tasks& get()
        {
            static Tasks value;
            return value;
        }
        template <class T> void track(const runtime::AsyncTask<T>& task)
        {
            std::lock_guard lock(mutex_);
            std::erase_if(tasks_, [](const auto& item) { return item.expired(); });
            tasks_.push_back(task.SharedState());
        }
        void shutdown() noexcept
        {
            try
            {
                std::vector<std::weak_ptr<runtime::detail::AsyncTaskStateBase>> pending;
                {
                    std::lock_guard lock(mutex_);
                    pending.swap(tasks_);
                }
                for (auto& weak : pending)
                    if (auto task = weak.lock())
                        task->Cancel();
                runtime::DrainAsyncMainExecutor();
                runtime::ShutdownAsyncRuntime();
                runtime::DrainAsyncMainExecutor();
            }
            catch (...)
            {
            }
        }
    };
    struct Error : std::runtime_error
    {
        WioNativeAbiStatus status;
        explicit Error(WioNativeAbiStatus code) : std::runtime_error("Wio ABI value contract mismatch"), status(code)
        {
        }
    };
    inline void require(bool valid, WioNativeAbiStatus code = WIO_NATIVE_ABI_TYPE_MISMATCH)
    {
        if (!valid)
            throw Error(code);
    }
    inline WioNativeAbiStatus failure(WioNativeAbiFailure* out, WioNativeAbiStatus code, const char* message) noexcept
    {
        // Diagnostic storage lives until the next failing boundary call on this thread.
        thread_local char buffer[1024];
        std::size_t length = 0;
        if (message)
            while (length + 1 < sizeof(buffer) && message[length])
            {
                buffer[length] = message[length];
                ++length;
            }
        buffer[length] = 0;
        if (out)
            *out = {code, 0, {"wio.native", 10}, {buffer, length}};
        return code;
    }
    template <class T, std::uint64_t Id> struct Box
    {
        std::atomic<std::uint32_t> refs{1};
        T value;
        explicit Box(T input) : value(std::move(input))
        {
        }
        static void retain(void* p) noexcept
        {
            ++static_cast<Box*>(p)->refs;
        }
        static void release(void* p) noexcept
        {
            auto* box = static_cast<Box*>(p);
            if (--box->refs == 0)
                delete box;
        }
        static std::uint64_t typeId(const void*) noexcept
        {
            return Id;
        }
        // Named operations avoid instantiating incomplete Box members inside an
        // inline static initializer on MSVC. The owner contract is unchanged.
        inline static const WioNativeAbiHandleOps ops{WIO_NATIVE_ABI_VERSION, 0, &retain, &release, &typeId};
    };
    // Physical POD fields and structural sequence values are separate contracts.
    // In particular, std::array may be trivially copyable without being a POD wire.
    template <class T, std::uint64_t Id> struct PodStorageCodec
    {
        static_assert(std::is_trivially_copyable_v<T>);
        static T read(const WioNativeAbiValue& value)
        {
            require(value.kind == WIO_NATIVE_ABI_POD && value.typeId == Id && value.payload.slice.data &&
                    value.payload.slice.size == sizeof(T));
            std::array<std::byte, sizeof(T)> bytes;
            std::memcpy(bytes.data(), value.payload.slice.data, sizeof(T));
            return std::bit_cast<T>(bytes);
        }
        static WioNativeAbiValue write(T input)
        {
            auto* box = new Box<T, Id>(std::move(input));
            WioNativeAbiValue value{};
            value.kind = WIO_NATIVE_ABI_POD;
            value.typeId = Id;
            value.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            value.payload.slice = {&box->value, sizeof(T)};
            value.owner = {box, &Box<T, Id>::ops, 1};
            return value;
        }
    };
    template <class T, std::uint64_t Id> struct Codec
    {
        static T read(const WioNativeAbiValue& v)
        {
            if constexpr (std::is_enum_v<T>)
            {
                return static_cast<T>(Codec<std::underlying_type_t<T>, Id>::read(v));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                require(v.kind == WIO_NATIVE_ABI_BOOL);
                return v.payload.boolean;
            }
            else if constexpr (std::is_same_v<T, char>)
            {
                require(v.kind == WIO_NATIVE_ABI_U64 &&
                        v.payload.unsignedInteger <= std::numeric_limits<unsigned char>::max());
                return static_cast<char>(static_cast<unsigned char>(v.payload.unsignedInteger));
            }
            else if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_signed_v<T>)
                {
                    require(v.kind == WIO_NATIVE_ABI_I64 && v.payload.signedInteger >= std::numeric_limits<T>::min() &&
                            v.payload.signedInteger <= std::numeric_limits<T>::max());
                    return static_cast<T>(v.payload.signedInteger);
                }
                else
                {
                    require(v.kind == WIO_NATIVE_ABI_U64 && v.payload.unsignedInteger <= std::numeric_limits<T>::max());
                    return static_cast<T>(v.payload.unsignedInteger);
                }
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                require(v.kind == WIO_NATIVE_ABI_F64);
                return static_cast<T>(v.payload.floatingPoint);
            }
            else if constexpr (std::is_same_v<T, void*>)
            {
                if (v.kind == WIO_NATIVE_ABI_VOID && (v.flags & WIO_NATIVE_ABI_VALUE_NULLABLE))
                    return nullptr;
                require(v.kind == WIO_NATIVE_ABI_OPAQUE);
                return v.payload.opaque;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                require(v.kind == WIO_NATIVE_ABI_BYTES && (v.payload.slice.data || v.payload.slice.size == 0));
                return v.payload.slice.size ? std::string(static_cast<const char*>(v.payload.slice.data),
                                                          static_cast<std::size_t>(v.payload.slice.size))
                                            : std::string{};
            }
            else if constexpr (std::is_trivially_copyable_v<T>)
            {
                // Materializing an existing POD must not execute its C++ member
                // default initializers before overwriting the object bytes.
                return PodStorageCodec<T, Id>::read(v);
            }
            else
            {
                require(v.kind == WIO_NATIVE_ABI_RUNTIME_VALUE && v.typeId == Id && v.payload.handle.state &&
                        v.payload.handle.ops == &Box<T, Id>::ops);
                return static_cast<Box<T, Id>*>(v.payload.handle.state)->value;
            }
        }
        static WioNativeAbiValue write(T input)
        {
            WioNativeAbiValue v;
            v.typeId = Id;
            if constexpr (std::is_enum_v<T>)
            {
                return Codec<std::underlying_type_t<T>, Id>::write(static_cast<std::underlying_type_t<T>>(input));
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                v.kind = WIO_NATIVE_ABI_BOOL;
                v.payload.boolean = input;
            }
            else if constexpr (std::is_same_v<T, char>)
            {
                v.kind = WIO_NATIVE_ABI_U64;
                v.payload.unsignedInteger = static_cast<unsigned char>(input);
            }
            else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
            {
                v.kind = WIO_NATIVE_ABI_I64;
                v.payload.signedInteger = input;
            }
            else if constexpr (std::is_integral_v<T>)
            {
                v.kind = WIO_NATIVE_ABI_U64;
                v.payload.unsignedInteger = input;
            }
            else if constexpr (std::is_floating_point_v<T>)
            {
                v.kind = WIO_NATIVE_ABI_F64;
                v.payload.floatingPoint = input;
            }
            else if constexpr (std::is_same_v<T, void*>)
            {
                v.kind = WIO_NATIVE_ABI_OPAQUE;
                v.payload.opaque = input;
            }
            else
            {
                auto* box = new Box<T, Id>(std::move(input));
                v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
                WioNativeAbiHandle owner{box, &Box<T, Id>::ops, 1};
                if constexpr (std::is_same_v<T, std::string>)
                {
                    v.kind = WIO_NATIVE_ABI_BYTES;
                    v.payload.slice = {box->value.data(), box->value.size()};
                    v.owner = owner;
                }
                else if constexpr (std::is_trivially_copyable_v<T>)
                {
                    v.kind = WIO_NATIVE_ABI_POD;
                    v.payload.slice = {&box->value, sizeof(T)};
                    v.owner = owner;
                }
                else
                {
                    v.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
                    v.payload.handle = owner;
                }
            }
            return v;
        }
    };
    template <class T, std::size_t Extent, std::uint64_t Id> struct Codec<std::array<T, Extent>, Id>
    {
        using Array = std::array<T, Extent>;
        static Array read(const WioNativeAbiValue& value)
        {
            require(value.kind == WIO_NATIVE_ABI_RUNTIME_VALUE && value.typeId == Id && value.payload.handle.state &&
                    value.payload.handle.ops == &Box<Array, Id>::ops);
            return static_cast<const Box<Array, Id>*>(value.payload.handle.state)->value;
        }
        static WioNativeAbiValue write(Array input)
        {
            auto* box = new Box<Array, Id>(std::move(input));
            WioNativeAbiValue value{};
            value.kind = WIO_NATIVE_ABI_RUNTIME_VALUE;
            value.typeId = Id;
            value.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            value.payload.handle = {box, &Box<Array, Id>::ops, 1};
            return value;
        }
    };
    // UTF-32 slices use byte counts and contain Unicode scalar values, not wchar_t.
    template <std::uint64_t Id> struct Codec<runtime::Text, Id>
    {
        static runtime::Text read(const WioNativeAbiValue& v)
        {
            require(v.kind == WIO_NATIVE_ABI_TEXT_UTF32 && v.payload.slice.size % sizeof(std::uint32_t) == 0 &&
                    (v.payload.slice.data || v.payload.slice.size == 0));
            std::vector<std::uint32_t> points(v.payload.slice.size / sizeof(std::uint32_t));
            if (!points.empty())
                std::memcpy(points.data(), v.payload.slice.data, v.payload.slice.size);
            std::string utf8;
            std::size_t error = 0;
            require(runtime::std_unicode::TryEncode(points, utf8, error));
            return runtime::Text::FromUtf8(utf8);
        }
        static WioNativeAbiValue write(const runtime::Text& input)
        {
            using Storage = Box<std::vector<std::uint32_t>, Id>;
            auto* box = new Storage(input.codePoints());
            WioNativeAbiValue v;
            v.kind = WIO_NATIVE_ABI_TEXT_UTF32;
            v.typeId = Id;
            v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            v.payload.slice = {box->value.data(), box->value.size() * sizeof(std::uint32_t)};
            v.owner = {box, &Storage::ops, 1};
            return v;
        }
    };

    // Exact transport remains independently addressable by generated polymorphic
    // codecs. Its operations identify the concrete pointer/ownership representation.
    template <class T, std::uint64_t Id> struct ExactObjectCodec
    {
        inline static const WioNativeAbiHandleOps ops{WIO_NATIVE_ABI_VERSION, 0,
                                                      [](void* p) noexcept
                                                      {
                                                          runtime::Ref<T> claim(static_cast<T*>(p));
                                                          (void)claim.Detach();
                                                      },
                                                      [](void* p) noexcept
                                                      {
                                                          if (p)
                                                              runtime::RefDeleter<T>::Execute(static_cast<T*>(p));
                                                      },
                                                      [](const void*) noexcept { return Id; }};
        static runtime::Ref<T> read(const WioNativeAbiValue& v)
        {
            require(v.kind == WIO_NATIVE_ABI_OBJECT && v.typeId == Id && v.payload.handle.ops == &ops);
            return runtime::Ref<T>(static_cast<T*>(v.payload.handle.state));
        }
        static WioNativeAbiValue write(runtime::Ref<T> input)
        {
            WioNativeAbiValue v;
            v.kind = WIO_NATIVE_ABI_OBJECT;
            v.typeId = Id;
            v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            v.payload.handle = {input.Detach(), &ops, 1};
            return v;
        }
    };

    template <class T, std::uint64_t Id> struct Codec<runtime::Ref<T>, Id> : ExactObjectCodec<T, Id>
    {
    };

    // Nullable objects keep the ordinary object handle and type identity. Null is
    // tagged void, never a boxed optional or a fake object with a null state.
    template <class T, std::uint64_t Id> struct Codec<std::optional<runtime::Ref<T>>, Id>
    {
        using Handle = runtime::Ref<T>;
        using Nullable = std::optional<Handle>;
        static Nullable read(const WioNativeAbiValue& value)
        {
            if (value.kind == WIO_NATIVE_ABI_VOID)
            {
                require(value.flags == WIO_NATIVE_ABI_VALUE_NULLABLE && (!value.typeId || value.typeId == Id) &&
                        !value.owner.state && !value.owner.ops);
                return std::nullopt;
            }
            require(value.kind == WIO_NATIVE_ABI_OBJECT && value.payload.handle.state &&
                    value.payload.handle.generation == 1 &&
                    (value.flags & (WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_BORROWED)));
            return Codec<Handle, Id>::read(value);
        }
        static WioNativeAbiValue write(Nullable input)
        {
            if (!input || !*input)
            {
                WioNativeAbiValue value{};
                value.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                value.typeId = Id;
                return value;
            }
            auto value = Codec<Handle, Id>::write(std::move(*input));
            value.flags |= WIO_NATIVE_ABI_VALUE_NULLABLE;
            return value;
        }
    };

    // A consumed argument transfers a wire claim, not unique ownership of the
    // referenced object. Decode retains/materializes a typed invocation value;
    // this guard retires the transferred wire claim on every exit path.
    class ConsumedArguments
    {
        struct Claims
        {
            const WioNativeAbiValue* arguments;
            std::size_t count;
            ~Claims()
            {
                for (std::size_t index = 0; index < count; ++index)
                    if ((arguments[index].flags & (WIO_NATIVE_ABI_VALUE_CONSUMED | WIO_NATIVE_ABI_VALUE_OWNED)) ==
                        (WIO_NATIVE_ABI_VALUE_CONSUMED | WIO_NATIVE_ABI_VALUE_OWNED))
                    {
                        auto claim = arguments[index];
                        WioNativeAbiReleaseValue(&claim);
                    }
            }
        };
        Claims claims_;

    public:
        ConsumedArguments(const ConsumedArguments&) = delete;
        ConsumedArguments& operator=(const ConsumedArguments&) = delete;
        ConsumedArguments(const WioNativeAbiValue* arguments, std::size_t count,
                          std::initializer_list<std::size_t> consumed)
            : claims_{arguments, count}
        {
            // Allocation-free adoption precedes all decoding. Member cleanup
            // also runs when validation itself throws from this constructor.
            for (auto index : consumed)
                require(index < count, WIO_NATIVE_ABI_INVALID_ARGUMENT);
            for (std::size_t index = 0; index < count; ++index)
            {
                const bool expected = std::find(consumed.begin(), consumed.end(), index) != consumed.end();
                const auto flags = arguments[index].flags;
                require(expected ? (flags & WIO_NATIVE_ABI_VALUE_CONSUMED) && (flags & WIO_NATIVE_ABI_VALUE_OWNED) &&
                                       !(flags & WIO_NATIVE_ABI_VALUE_BORROWED) &&
                                       arguments[index].kind != WIO_NATIVE_ABI_REFERENCE
                                 : !(flags & WIO_NATIVE_ABI_VALUE_CONSUMED),
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
            }
        }
    };

    // A ref/view wire argument points at another tagged value, not a foreign C++
    // string/object layout. Repeated borrows of the same token share one local
    // storage cell, preserving aliasing. Copy-back runs even on a caught failure.
    class Frame
    {
        struct Slot
        {
            std::uint64_t type;
            std::any storage;
            std::function<void()> commit;
            void* address;
            std::size_t size;
            bool projectable;
            void* (*podAddress)(void*, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t,
                                const std::type_info&) noexcept;
            void* (*sequenceAddress)(void*, const std::uint64_t*, std::uint64_t, const std::type_info&) noexcept;
            void* (*sequencePodAddress)(void*, const std::uint64_t*, std::uint64_t, std::uint64_t, std::uint64_t,
                                        std::uint64_t, std::uint64_t, const std::type_info&) noexcept;
            void* (*containerAddress)(void*, std::uint64_t, const WioNativeAbiContainerProjection&,
                                      const WioNativeAbiContainerStep*, std::uint64_t, const std::type_info&);
            bool dirty = false;
        };
        std::map<WioNativeAbiValue*, Slot> slots_;
        // All projection forms converge on the same typed producer cell. The
        // representation of the path is not an identity. A struct and its
        // first field can share an address, hence the explicit C++ type key.
        std::map<std::pair<const void*, std::type_index>, std::uint64_t> identities_;
        std::shared_ptr<CallbackBorrowLifetime> lifetime_;
        std::shared_ptr<CallbackBorrowLifetime> previousLifetime_;

        template <class T, std::uint64_t Id> T& bindCell(Slot& root, void* address, bool mutableAccess)
        {
            require(address && reinterpret_cast<std::uintptr_t>(address) % alignof(T) == 0,
                    WIO_NATIVE_ABI_INVALID_ARGUMENT);
            const auto [found, inserted] =
                identities_.emplace(std::pair<const void*, std::type_index>{address, typeid(T)}, Id);
            require(inserted || found->second == Id, WIO_NATIVE_ABI_TYPE_MISMATCH);
            root.dirty = root.dirty || mutableAccess;
            return *static_cast<T*>(address);
        }

    public:
        explicit Frame(bool callback = false)
        {
            if (callback)
            {
                lifetime_ = std::make_shared<CallbackBorrowLifetime>();
                previousLifetime_ = std::exchange(activeCallbackBorrow, lifetime_);
            }
        }
        ~Frame()
        {
            if (lifetime_)
            {
                lifetime_->active.store(false, std::memory_order_release);
                activeCallbackBorrow = std::move(previousLifetime_);
            }
        }
        template <class T, std::uint64_t Id> T& borrow(const WioNativeAbiValue& wire, bool mutableAccess)
        {
            require(wire.kind == WIO_NATIVE_ABI_REFERENCE && (wire.flags & WIO_NATIVE_ABI_VALUE_BORROWED) &&
                        !(wire.flags & WIO_NATIVE_ABI_VALUE_ARGUMENT_ALIAS) &&
                        (!mutableAccess || (wire.flags & WIO_NATIVE_ABI_VALUE_MUTABLE)) &&
                        wire.payload.mutableSlice.data,
                    WIO_NATIVE_ABI_INVALID_ARGUMENT);
            if (wire.flags & WIO_NATIVE_ABI_VALUE_CONTAINER_PROJECTED)
            {
                require(!(wire.flags & (WIO_NATIVE_ABI_VALUE_PROJECTED | WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED |
                                        WIO_NATIVE_ABI_VALUE_SEQUENCE_POD_PROJECTED)) &&
                            wire.typeId == Id &&
                            wire.payload.mutableSlice.size == sizeof(WioNativeAbiContainerProjection) &&
                            reinterpret_cast<std::uintptr_t>(wire.payload.mutableSlice.data) %
                                    alignof(WioNativeAbiContainerProjection) ==
                                0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                const auto& projection =
                    *static_cast<const WioNativeAbiContainerProjection*>(wire.payload.mutableSlice.data);
                const auto found = slots_.find(projection.root);
                require(found != slots_.end() && found->second.type == projection.rootTypeId &&
                            projection.typeId == Id && projection.steps && projection.count && projection.count <= 64 &&
                            (!projection.podTypeId || projection.size == sizeof(T)) &&
                            reinterpret_cast<std::uintptr_t>(projection.steps) % alignof(WioNativeAbiContainerStep) ==
                                0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                auto* address = found->second.containerAddress(found->second.address, found->second.type, projection,
                                                               projection.steps, projection.count, typeid(T));
                return bindCell<T, Id>(found->second, address, mutableAccess);
            }
            if (wire.flags & WIO_NATIVE_ABI_VALUE_SEQUENCE_POD_PROJECTED)
            {
                require(!(wire.flags & (WIO_NATIVE_ABI_VALUE_PROJECTED | WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED)) &&
                            wire.typeId == Id &&
                            wire.payload.mutableSlice.size == sizeof(WioNativeAbiSequencePodProjection) &&
                            reinterpret_cast<std::uintptr_t>(wire.payload.mutableSlice.data) %
                                    alignof(WioNativeAbiSequencePodProjection) ==
                                0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                const auto& projection =
                    *static_cast<const WioNativeAbiSequencePodProjection*>(wire.payload.mutableSlice.data);
                const auto& sequence = projection.sequence;
                const auto found = slots_.find(sequence.root);
                require(found != slots_.end() && found->second.type == sequence.rootTypeId && sequence.typeId == Id &&
                            sequence.indices && sequence.count && sequence.count <= 64 &&
                            projection.size == sizeof(T) &&
                            reinterpret_cast<std::uintptr_t>(sequence.indices) % alignof(std::uint64_t) == 0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                auto* address = found->second.sequencePodAddress(found->second.address, sequence.indices,
                                                                 sequence.count, projection.podTypeId, Id,
                                                                 projection.offset, projection.size, typeid(T));
                return bindCell<T, Id>(found->second, address, mutableAccess);
            }
            if (wire.flags & WIO_NATIVE_ABI_VALUE_SEQUENCE_PROJECTED)
            {
                require(!(wire.flags & WIO_NATIVE_ABI_VALUE_PROJECTED) && wire.typeId == Id &&
                            wire.payload.mutableSlice.size == sizeof(WioNativeAbiSequenceProjection) &&
                            reinterpret_cast<std::uintptr_t>(wire.payload.mutableSlice.data) %
                                    alignof(WioNativeAbiSequenceProjection) ==
                                0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                const auto& projection =
                    *static_cast<const WioNativeAbiSequenceProjection*>(wire.payload.mutableSlice.data);
                const auto found = slots_.find(projection.root);
                require(found != slots_.end() && found->second.type == projection.rootTypeId &&
                            projection.typeId == Id && projection.indices && projection.count &&
                            projection.count <= 64 &&
                            reinterpret_cast<std::uintptr_t>(projection.indices) % alignof(std::uint64_t) == 0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                auto* address = found->second.sequenceAddress(found->second.address, projection.indices,
                                                              projection.count, typeid(T));
                return bindCell<T, Id>(found->second, address, mutableAccess);
            }
            if (wire.flags & WIO_NATIVE_ABI_VALUE_PROJECTED)
            {
                require(wire.payload.mutableSlice.size == sizeof(WioNativeAbiProjection) &&
                            reinterpret_cast<std::uintptr_t>(wire.payload.mutableSlice.data) %
                                    alignof(WioNativeAbiProjection) ==
                                0,
                        WIO_NATIVE_ABI_INVALID_ARGUMENT);
                const auto& projection = *static_cast<const WioNativeAbiProjection*>(wire.payload.mutableSlice.data);
                const auto found = slots_.find(projection.root);
                require(found != slots_.end() && found->second.projectable &&
                        found->second.type == projection.rootTypeId && projection.typeId == Id &&
                        projection.size == sizeof(T) && projection.offset <= found->second.size &&
                        projection.size <= found->second.size - projection.offset);
                auto* address = found->second.podAddress(found->second.address, projection.rootTypeId, Id,
                                                         projection.offset, projection.size, typeid(T));
                return bindCell<T, Id>(found->second, address, mutableAccess);
            }
            require(wire.payload.mutableSlice.size == sizeof(WioNativeAbiValue), WIO_NATIVE_ABI_INVALID_ARGUMENT);
            auto* token = static_cast<WioNativeAbiValue*>(wire.payload.mutableSlice.data);
            require(reinterpret_cast<std::uintptr_t>(token) % alignof(WioNativeAbiValue) == 0,
                    WIO_NATIVE_ABI_INVALID_ARGUMENT);
            auto found = slots_.find(token);
            if (found == slots_.end())
            {
                auto storage = std::make_shared<T>(Codec<T, Id>::read(*token));
                auto commit = [token, storage]
                {
                    auto replacement = Codec<T, Id>::write(*storage);
                    WioNativeAbiReleaseValue(token);
                    *token = replacement;
                };
                found =
                    slots_
                        .emplace(token,
                                 Slot{Id, storage, std::move(commit), storage.get(), sizeof(T),
                                      token->kind == WIO_NATIVE_ABI_POD, &NativePodProjectionAccess<T>::resolve,
                                      &SequenceProjectionAccess<T>::resolve, &SequenceProjectionAccess<T>::resolvePod,
                                      &NativeContainerProjectionAccess<T>::resolve})
                        .first;
            }
            require(found->second.type == Id);
            const auto* storage = std::any_cast<std::shared_ptr<T>>(&found->second.storage);
            require(storage != nullptr);
            return bindCell<T, Id>(found->second, storage->get(), mutableAccess);
        }
        // Resolve against the live producer container again after the native call.
        // Only compare addresses: the original/returned pointer may be invalidated
        // by native structural mutation and must never be dereferenced here.
        template <class T, std::uint64_t Id> bool aliasesLiveBorrow(const WioNativeAbiValue& wire, const void* address)
        {
            return address && std::addressof(borrow<T, Id>(wire, false)) == address;
        }

        void commit()
        {
            for (auto& [_, slot] : slots_)
                if (slot.dirty)
                {
                    slot.commit();
                    slot.dirty = false;
                }
        }
    };

    // Callback argument/result identities belong to the concrete Wio signature,
    // not just its C++ spelling. Keep them even when two declarations share a POD.
    template <std::size_t Count> class CallbackArguments
    {
    public:
        CallbackArguments() = default;
        CallbackArguments(const CallbackArguments&) = delete;
        CallbackArguments& operator=(const CallbackArguments&) = delete;
        ~CallbackArguments()
        {
            for (auto& value : values)
                WioNativeAbiReleaseValue(&value);
            for (auto& token : tokens_)
                WioNativeAbiReleaseValue(&token);
        }
        template <class T, std::uint64_t Id> void borrow(std::size_t index, Place<T> place, bool mutableAccess)
        {
            require(!mutableAccess || place.canRebind(), WIO_NATIVE_ABI_INVALID_ARGUMENT);
            cells_.add(place);
            const auto* address = place.borrowIdentity();
            require(address != nullptr, WIO_NATIVE_ABI_INVALID_ARGUMENT);
            std::size_t slot = 0;
            while (slot < count_ && addresses_[slot] != address)
                ++slot;
            if (slot == count_)
            {
                tokens_[slot] = Codec<T, Id>::write(place.read());
                addresses_[slot] = address;
                contracts_[slot] = &contract<T, Id>;
                places_[slot] = place;
                ++count_;
            }
            require(contracts_[slot] == &contract<T, Id>);
            writable_[slot] |= mutableAccess;
            if (mutableAccess && !prepares_[slot])
                prepares_[slot] = [this, slot, place]() mutable -> std::function<void()>
                {
                    // Decode every returned cell before writing any caller place.
                    // A malformed later UTF-32 slice must not partially commit.
                    // Object tokens name the concrete dynamic type. Their codec
                    // validates assignability to this statically typed place.
                    require(tokens_[slot].kind == WIO_NATIVE_ABI_OBJECT || tokens_[slot].typeId == 0 ||
                            tokens_[slot].typeId == Id);
                    auto replacement = Codec<T, Id>::read(tokens_[slot]);
                    return [place, replacement = std::move(replacement)]() mutable
                    { place.write(std::move(replacement)); };
                };
            auto& wire = values[index];
            wire.kind = WIO_NATIVE_ABI_REFERENCE;
            wire.flags = WIO_NATIVE_ABI_VALUE_BORROWED | (mutableAccess ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0u);
            wire.typeId = Id;
            wire.payload.mutableSlice = {&tokens_[slot], sizeof(WioNativeAbiValue)};
        }
        template <class T, std::uint64_t Id> Place<T> result(const WioNativeAbiValue& wire, bool mutableAccess)
        {
            require(wire.kind == WIO_NATIVE_ABI_REFERENCE && wire.typeId == Id &&
                    wire.payload.mutableSlice.size == sizeof(WioNativeAbiValue) && !wire.owner.state &&
                    !wire.owner.ops &&
                    (wire.flags == WIO_NATIVE_ABI_VALUE_BORROWED ||
                     wire.flags == (WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE)) &&
                    (!mutableAccess || (wire.flags & WIO_NATIVE_ABI_VALUE_MUTABLE)));
            for (std::size_t index = 0; index < count_; ++index)
                if (wire.payload.mutableSlice.data == &tokens_[index])
                {
                    require(contracts_[index] == &contract<T, Id> && (!mutableAccess || writable_[index]));
                    auto place = std::any_cast<Place<T>>(places_[index]);
                    return mutableAccess ? place : place.asView();
                }
            throw Error(WIO_NATIVE_ABI_TYPE_MISMATCH);
        }
        void commit()
        {
            if (count_ == 0)
                return;
            std::array<std::function<void()>, Count> commits;
            for (std::size_t index = 0; index < Count; ++index)
                if (prepares_[index])
                    commits[index] = prepares_[index]();
            for (auto& commit : commits)
                if (commit)
                {
                    commit();
                }
            for (auto& prepare : prepares_)
                prepare = {};
        }
        std::array<WioNativeAbiValue, Count> values{};

    private:
        template <class T, std::uint64_t Id> inline static char contract = 0;
        std::array<WioNativeAbiValue, Count> tokens_{};
        std::array<const void*, Count> addresses_{};
        CallbackBorrowCells<Count> cells_;
        std::array<const void*, Count> contracts_{};
        std::array<std::any, Count> places_{};
        std::array<bool, Count> writable_{};
        std::array<std::function<std::function<void()>()>, Count> prepares_{};
        std::size_t count_ = 0;
    };

    template <class T, std::uint64_t Id> struct CallbackArgument
    {
        static T read(Frame&, const WioNativeAbiValue& value, bool)
        {
            return Codec<T, Id>::read(value);
        }
        template <std::size_t Count>
        static void write(CallbackArguments<Count>& arguments, std::size_t index, T value, bool)
        {
            arguments.values[index] = Codec<T, Id>::write(std::move(value));
        }
    };
    template <class T, std::uint64_t Id> struct CallbackArgument<Place<T>, Id>
    {
        static Place<T> read(Frame& frame, const WioNativeAbiValue& value, bool mutableAccess)
        {
            require((value.flags == WIO_NATIVE_ABI_VALUE_BORROWED ||
                     value.flags == (WIO_NATIVE_ABI_VALUE_BORROWED | WIO_NATIVE_ABI_VALUE_MUTABLE)) &&
                        !value.owner.state && !value.owner.ops,
                    WIO_NATIVE_ABI_INVALID_ARGUMENT);
            require(value.typeId == 0 || value.typeId == Id);
            require(
                value.kind == WIO_NATIVE_ABI_REFERENCE && value.payload.mutableSlice.data &&
                    value.payload.mutableSlice.size == sizeof(WioNativeAbiValue) &&
                    reinterpret_cast<std::uintptr_t>(value.payload.mutableSlice.data) % alignof(WioNativeAbiValue) == 0,
                WIO_NATIVE_ABI_INVALID_ARGUMENT);
            const auto* token = static_cast<const WioNativeAbiValue*>(value.payload.mutableSlice.data);
            require(token->kind == WIO_NATIVE_ABI_OBJECT || token->typeId == 0 || token->typeId == Id);
            auto& cell = frame.borrow<T, Id>(value, mutableAccess);
            return mutableAccess ? Place<T>::borrow(cell) : Place<T>::borrowView(cell);
        }
        template <std::size_t Count>
        static void write(CallbackArguments<Count>& arguments, std::size_t index, Place<T> value, bool mutableAccess)
        {
            arguments.template borrow<T, Id>(index, std::move(value), mutableAccess);
        }
    };

    template <class T, std::uint64_t Id> struct CallbackResult
    {
        static WioNativeAbiValue write(Frame&, const WioNativeAbiValue*, std::size_t, T value, bool)
        {
            return Codec<T, Id>::write(std::move(value));
        }
        template <std::size_t Count> static T read(CallbackArguments<Count>&, const WioNativeAbiValue& wire, bool)
        {
            return Codec<T, Id>::read(wire);
        }
    };

    template <class T, std::uint64_t Id> struct CallbackResult<Place<T>, Id>
    {
        static WioNativeAbiValue write(Frame& frame, const WioNativeAbiValue* arguments, std::size_t count,
                                       Place<T> place, bool mutableAccess)
        {
            require(!mutableAccess || place.canRebind());
            // Compare cell identities only. An untrusted producer may return an
            // already-invalid stack/projection address; never dereference it here.
            const auto* address = place.borrowIdentity();
            for (std::size_t index = 0; index < count; ++index)
            {
                const auto& wire = arguments[index];
                if (wire.kind != WIO_NATIVE_ABI_REFERENCE || (wire.typeId && wire.typeId != Id) ||
                    !(wire.flags & WIO_NATIVE_ABI_VALUE_BORROWED) ||
                    (mutableAccess && !(wire.flags & WIO_NATIVE_ABI_VALUE_MUTABLE)))
                    continue;
                if (frame.template aliasesLiveBorrow<T, Id>(wire, address))
                {
                    auto result = wire;
                    result.typeId = Id;
                    result.flags = WIO_NATIVE_ABI_VALUE_BORROWED | (mutableAccess ? WIO_NATIVE_ABI_VALUE_MUTABLE : 0u);
                    return result;
                }
            }
            throw Error(WIO_NATIVE_ABI_TYPE_MISMATCH);
        }
        template <std::size_t Count>
        static Place<T> read(CallbackArguments<Count>& arguments, const WioNativeAbiValue& wire, bool mutableAccess)
        {
            return arguments.template result<T, Id>(wire, mutableAccess);
        }
    };

    template <class T, std::uint64_t Id> struct CallbackArgument<std::optional<Place<T>>, Id>
    {
        static std::optional<Place<T>> read(Frame& frame, const WioNativeAbiValue& wire, bool mutableAccess)
        {
            if (wire.kind == WIO_NATIVE_ABI_VOID)
            {
                require(wire.flags == WIO_NATIVE_ABI_VALUE_NULLABLE && wire.typeId == Id && !wire.owner.state &&
                        !wire.owner.ops);
                return std::nullopt;
            }
            return CallbackArgument<Place<T>, Id>::read(frame, wire, mutableAccess);
        }
        template <std::size_t Count>
        static void write(CallbackArguments<Count>& arguments, std::size_t index, std::optional<Place<T>> place,
                          bool mutableAccess)
        {
            if (place)
                arguments.template borrow<T, Id>(index, *place, mutableAccess);
            else
            {
                arguments.values[index].flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
                arguments.values[index].typeId = Id;
            }
        }
    };

    template <class T, std::uint64_t Id> struct CallbackResult<std::optional<Place<T>>, Id>
    {
        static WioNativeAbiValue write(Frame& frame, const WioNativeAbiValue* arguments, std::size_t count,
                                       std::optional<Place<T>> place, bool mutableAccess)
        {
            if (place)
                return CallbackResult<Place<T>, Id>::write(frame, arguments, count, *place, mutableAccess);
            WioNativeAbiValue wire{};
            wire.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
            wire.typeId = Id;
            return wire;
        }
        template <std::size_t Count>
        static std::optional<Place<T>> read(CallbackArguments<Count>& arguments, const WioNativeAbiValue& wire,
                                            bool mutableAccess)
        {
            if (wire.kind == WIO_NATIVE_ABI_VOID)
            {
                require(wire.flags == WIO_NATIVE_ABI_VALUE_NULLABLE && wire.typeId == Id && !wire.owner.state &&
                        !wire.owner.ops);
                return std::nullopt;
            }
            return CallbackResult<Place<T>, Id>::read(arguments, wire, mutableAccess);
        }
    };

    template <class Signature, std::uint64_t Id, std::uint64_t ResultId, bool ResultMutable, class Mutability,
              std::uint64_t... ArgumentIds>
    struct CallbackCodecWithResultBorrow;

    template <class R, class... A, std::uint64_t Id, std::uint64_t ResultId, bool ResultMutable, bool... Mutable,
              std::uint64_t... ArgumentIds>
    struct CallbackCodecWithResultBorrow<std::function<R(A...)>, Id, ResultId, ResultMutable,
                                         std::integer_sequence<bool, Mutable...>, ArgumentIds...>
    {
        static_assert(sizeof...(A) == sizeof...(ArgumentIds));
        static_assert(sizeof...(A) == sizeof...(Mutable));
        inline static constexpr std::array<std::uint64_t, sizeof...(A)> argumentIds{ArgumentIds...};
        inline static constexpr std::array<bool, sizeof...(A)> mutableArguments{Mutable...};
        struct Storage
        {
            std::atomic<std::uint32_t> refs{1};
            std::function<R(A...)> fn;
            std::thread::id thread = std::this_thread::get_id();
            explicit Storage(std::function<R(A...)> value) : fn(std::move(value))
            {
            }
        };
        template <std::size_t... I>
        static void invoke(Storage& storage, const WioNativeAbiValue* args, WioNativeAbiValue* result,
                           std::index_sequence<I...>)
        {
            Frame frame{true};
            try
            {
                if constexpr (std::is_void_v<R>)
                {
                    storage.fn(CallbackArgument<A, argumentIds[I]>::read(frame, args[I], mutableArguments[I])...);
                    *result = {};
                }
                else
                    *result = CallbackResult<R, ResultId>::write(
                        frame, args, sizeof...(A),
                        storage.fn(CallbackArgument<A, argumentIds[I]>::read(frame, args[I], mutableArguments[I])...),
                        ResultMutable);
            }
            catch (...)
            {
                frame.commit();
                throw;
            }
            frame.commit();
        }
        inline static const WioNativeAbiCallbackOps ops{
            WIO_NATIVE_ABI_VERSION, 0, [](void* p) noexcept { ++static_cast<Storage*>(p)->refs; },
            [](void* p) noexcept
            {
                auto* s = static_cast<Storage*>(p);
                if (--s->refs == 0)
                    delete s;
            },
            [](void* p, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
               WioNativeAbiFailure* error) noexcept
            {
                if (!p || !result || count != sizeof...(A) || (count && !args))
                    return WIO_NATIVE_ABI_INVALID_ARGUMENT;
                *result = {};
                try
                {
                    auto& storage = *static_cast<Storage*>(p);
                    require(storage.thread == std::this_thread::get_id(), WIO_NATIVE_ABI_WRONG_THREAD);
                    invoke(storage, args, result, std::index_sequence_for<A...>{});
                    return WIO_NATIVE_ABI_OK;
                }
                catch (const Error& e)
                {
                    WioNativeAbiReleaseValue(result);
                    *result = {};
                    return failure(error, e.status, e.what());
                }
                catch (const std::exception& e)
                {
                    WioNativeAbiReleaseValue(result);
                    *result = {};
                    return failure(error, WIO_NATIVE_ABI_EXCEPTION, e.what());
                }
                catch (...)
                {
                    WioNativeAbiReleaseValue(result);
                    *result = {};
                    return failure(error, WIO_NATIVE_ABI_EXCEPTION, "callback failure");
                }
            }};
        static WioNativeAbiValue write(std::function<R(A...)> fn)
        {
            bindOwningNativeCallable(fn);
            WioNativeAbiValue v;
            v.kind = WIO_NATIVE_ABI_CALLBACK;
            v.typeId = Id;
            v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            v.payload.callback = {new Storage(std::move(fn)), &ops};
            return v;
        }
        static std::function<R(A...)> read(const WioNativeAbiValue& value)
        {
            require(value.kind == WIO_NATIVE_ABI_CALLBACK && value.payload.callback.ops &&
                    value.payload.callback.ops->abiVersion == WIO_NATIVE_ABI_VERSION &&
                    value.payload.callback.ops->invoke);
            require(!(value.payload.callback.ops->flags &
                      ~(WIO_NATIVE_ABI_CALLBACK_ANY_THREAD | WIO_NATIVE_ABI_CALLBACK_CALLER_THREAD)));
            require(Id == 0 || value.typeId == 0 || value.typeId == Id);
            const auto callback = value.payload.callback;
            std::shared_ptr<void> owner;
            if (callback.ops->retain && callback.ops->release)
            {
                callback.ops->retain(callback.userdata);
                owner = std::shared_ptr<void>(callback.userdata, [ops = callback.ops](void* p) { ops->release(p); });
            }
            const auto thread = std::this_thread::get_id();
            const bool anyThread = (callback.ops->flags & WIO_NATIVE_ABI_CALLBACK_ANY_THREAD) != 0;
            return [callback, thread, owner, anyThread](A... args) -> R
            {
                require(anyThread || thread == std::this_thread::get_id(), WIO_NATIVE_ABI_WRONG_THREAD);
                CallbackArguments<sizeof...(A)> arguments;
                // Install cleanup before converting the first owning argument;
                // a later codec failure must release every preceding wire value.
                [&]<std::size_t... I>(std::index_sequence<I...>)
                {
                    (CallbackArgument<A, argumentIds[I]>::write(arguments, I, args, mutableArguments[I]), ...);
                }(std::index_sequence_for<A...>{});
                WioNativeAbiValue result{};
                WioNativeAbiFailure failure{};
                auto status = callback.ops->invoke(callback.userdata, arguments.values.data(), arguments.values.size(),
                                                   &result, &failure);
                struct ResultGuard
                {
                    WioNativeAbiValue& value;
                    ~ResultGuard()
                    {
                        WioNativeAbiReleaseValue(&value);
                    }
                } resultGuard{result};
                arguments.commit();
                require(status == WIO_NATIVE_ABI_OK, status);
                if constexpr (!std::is_void_v<R>)
                    return CallbackResult<R, ResultId>::read(arguments, result, ResultMutable);
            };
        }
    };

    template <class Signature, std::uint64_t Id, std::uint64_t ResultId, class Mutability, std::uint64_t... ArgumentIds>
    struct CallbackCodecWithBorrows
        : CallbackCodecWithResultBorrow<Signature, Id, ResultId, true, Mutability, ArgumentIds...>
    {
    };

    template <class Signature, std::uint64_t Id, std::uint64_t ResultId, std::uint64_t... ArgumentIds>
    struct CallbackCodec;
    template <class R, class... A, std::uint64_t Id, std::uint64_t ResultId, std::uint64_t... ArgumentIds>
    struct CallbackCodec<std::function<R(A...)>, Id, ResultId, ArgumentIds...>
        : CallbackCodecWithBorrows<std::function<R(A...)>, Id, ResultId,
                                   std::integer_sequence<bool, (sizeof(A) && false)...>, ArgumentIds...>
    {
    };

    // Legacy scalar callers do not carry nominal identities. Generated native
    // thunks use CallbackCodec with the resolved signature's stable identities.
    template <class R, class... A, std::uint64_t Id>
    struct Codec<std::function<R(A...)>, Id>
        : CallbackCodec<std::function<R(A...)>, Id, 0, (sizeof(A) * std::uint64_t{0})...>
    {
    };
    template <class R, class... A, std::uint64_t Id> struct Codec<std::optional<std::function<R(A...)>>, Id>
    {
        using Nullable = std::optional<std::function<R(A...)>>;
        static WioNativeAbiValue write(Nullable value)
        {
            if (value)
                return Codec<std::function<R(A...)>, Id>::write(std::move(*value));
            WioNativeAbiValue wire{};
            wire.flags = WIO_NATIVE_ABI_VALUE_NULLABLE;
            wire.typeId = Id;
            return wire;
        }
        static Nullable read(const WioNativeAbiValue& wire)
        {
            if (wire.kind == WIO_NATIVE_ABI_VOID)
            {
                require(wire.flags == WIO_NATIVE_ABI_VALUE_NULLABLE && wire.typeId == Id && !wire.owner.state &&
                        !wire.owner.ops);
                return std::nullopt;
            }
            return Codec<std::function<R(A...)>, Id>::read(wire);
        }
    };
} // namespace wio::wir_backend::abi
