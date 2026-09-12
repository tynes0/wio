#pragma once
#include <wio_native_abi.h>
#include <atomic>
#include <any>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "ref.h"
#include "text.h"
#include "std_async.h"

namespace wio::wir_backend::abi {
class Tasks {
    std::mutex mutex_;
    std::vector<std::weak_ptr<runtime::detail::AsyncTaskStateBase>> tasks_;
public:
    static Tasks& get() { static Tasks value; return value; }
    template<class T> void track(const runtime::AsyncTask<T>& task) {
        std::lock_guard lock(mutex_);
        std::erase_if(tasks_, [](const auto& item) { return item.expired(); });
        tasks_.push_back(task.SharedState());
    }
    void shutdown() noexcept {
        try {
            std::vector<std::weak_ptr<runtime::detail::AsyncTaskStateBase>> pending;
            { std::lock_guard lock(mutex_); pending.swap(tasks_); }
            for (auto& weak : pending) if (auto task=weak.lock()) task->Cancel();
            runtime::DrainAsyncMainExecutor();
            runtime::ShutdownAsyncRuntime();
            runtime::DrainAsyncMainExecutor();
        } catch (...) {}
    }
};
struct Error : std::runtime_error {
    WioNativeAbiStatus status;
    explicit Error(WioNativeAbiStatus code) : std::runtime_error("Wio ABI value contract mismatch"), status(code) {}
};
inline void require(bool valid, WioNativeAbiStatus code = WIO_NATIVE_ABI_TYPE_MISMATCH) { if (!valid) throw Error(code); }
inline WioNativeAbiStatus failure(WioNativeAbiFailure* out, WioNativeAbiStatus code, const char* message) noexcept {
    // Diagnostic storage lives until the next failing boundary call on this thread.
    thread_local char buffer[1024];
    std::size_t length = 0;
    if (message) while (length + 1 < sizeof(buffer) && message[length]) { buffer[length] = message[length]; ++length; }
    buffer[length] = 0;
    if (out) *out = {code, 0, {"wio.native", 10}, {buffer, length}};
    return code;
}
template<class T, std::uint64_t Id> struct Box {
    std::atomic<std::uint32_t> refs{1};
    T value;
    explicit Box(T input) : value(std::move(input)) {}
    inline static const WioNativeAbiHandleOps ops{WIO_NATIVE_ABI_VERSION, 0,
        [](void* p) noexcept { ++static_cast<Box*>(p)->refs; },
        [](void* p) noexcept { auto* b = static_cast<Box*>(p); if (--b->refs == 0) delete b; },
        [](const void*) noexcept { return Id; }};
};
template<class T, std::uint64_t Id> struct Codec {
    static T read(const WioNativeAbiValue& v) {
        if constexpr (std::is_same_v<T, bool>) { require(v.kind == WIO_NATIVE_ABI_BOOL); return v.payload.boolean; }
        else if constexpr (std::is_integral_v<T>) {
            if constexpr (std::is_signed_v<T>) {
                require(v.kind == WIO_NATIVE_ABI_I64 && v.payload.signedInteger >= std::numeric_limits<T>::min() && v.payload.signedInteger <= std::numeric_limits<T>::max());
                return static_cast<T>(v.payload.signedInteger);
            } else {
                require(v.kind == WIO_NATIVE_ABI_U64 && v.payload.unsignedInteger <= std::numeric_limits<T>::max());
                return static_cast<T>(v.payload.unsignedInteger);
            }
        } else if constexpr (std::is_floating_point_v<T>) { require(v.kind == WIO_NATIVE_ABI_F64); return static_cast<T>(v.payload.floatingPoint); }
        else if constexpr (std::is_same_v<T, void*>) { require(v.kind == WIO_NATIVE_ABI_OPAQUE); return v.payload.opaque; }
        else if constexpr (std::is_same_v<T, std::string>) {
            require(v.kind == WIO_NATIVE_ABI_BYTES && (v.payload.slice.data || v.payload.slice.size == 0));
            return v.payload.slice.size ? std::string(static_cast<const char*>(v.payload.slice.data), static_cast<std::size_t>(v.payload.slice.size)) : std::string{};
        } else if constexpr (std::is_trivially_copyable_v<T>) {
            require(v.kind == WIO_NATIVE_ABI_POD && v.typeId == Id && v.payload.slice.data && v.payload.slice.size == sizeof(T));
            T result; std::memcpy(&result, v.payload.slice.data, sizeof(T)); return result;
        } else {
            require(v.kind == WIO_NATIVE_ABI_RUNTIME_VALUE && v.typeId == Id && v.payload.handle.state && v.payload.handle.ops == &Box<T, Id>::ops);
            return static_cast<Box<T, Id>*>(v.payload.handle.state)->value;
        }
    }
    static WioNativeAbiValue write(T input) {
        WioNativeAbiValue v; v.typeId = Id;
        if constexpr (std::is_same_v<T, bool>) { v.kind = WIO_NATIVE_ABI_BOOL; v.payload.boolean = input; }
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) { v.kind = WIO_NATIVE_ABI_I64; v.payload.signedInteger = input; }
        else if constexpr (std::is_integral_v<T>) { v.kind = WIO_NATIVE_ABI_U64; v.payload.unsignedInteger = input; }
        else if constexpr (std::is_floating_point_v<T>) { v.kind = WIO_NATIVE_ABI_F64; v.payload.floatingPoint = input; }
        else if constexpr (std::is_same_v<T, void*>) { v.kind = WIO_NATIVE_ABI_OPAQUE; v.payload.opaque = input; }
        else {
            auto* box = new Box<T, Id>(std::move(input));
            v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
            WioNativeAbiHandle owner{box, &Box<T, Id>::ops, 1};
            if constexpr (std::is_same_v<T, std::string>) {
                v.kind = WIO_NATIVE_ABI_BYTES; v.payload.slice = {box->value.data(), box->value.size()}; v.owner = owner;
            } else if constexpr (std::is_trivially_copyable_v<T>) {
                v.kind = WIO_NATIVE_ABI_POD; v.payload.slice = {&box->value, sizeof(T)}; v.owner = owner;
            } else { v.kind = WIO_NATIVE_ABI_RUNTIME_VALUE; v.payload.handle = owner; }
        }
        return v;
    }
};
// UTF-32 slices use byte counts and contain Unicode scalar values, not wchar_t.
template<std::uint64_t Id> struct Codec<runtime::Text, Id> {
    static runtime::Text read(const WioNativeAbiValue& v) {
        require(v.kind == WIO_NATIVE_ABI_TEXT_UTF32 && v.payload.slice.size % sizeof(std::uint32_t) == 0 &&
            (v.payload.slice.data || v.payload.slice.size == 0));
        std::vector<std::uint32_t> points(v.payload.slice.size / sizeof(std::uint32_t));
        if (!points.empty()) std::memcpy(points.data(), v.payload.slice.data, v.payload.slice.size);
        std::string utf8; std::size_t error = 0;
        require(runtime::std_unicode::TryEncode(points, utf8, error));
        return runtime::Text::FromUtf8(utf8);
    }
    static WioNativeAbiValue write(const runtime::Text& input) {
        using Storage = Box<std::vector<std::uint32_t>, Id>;
        auto* box = new Storage(input.codePoints());
        WioNativeAbiValue v; v.kind = WIO_NATIVE_ABI_TEXT_UTF32; v.typeId = Id;
        v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
        v.payload.slice = {box->value.data(), box->value.size() * sizeof(std::uint32_t)};
        v.owner = {box, &Storage::ops, 1}; return v;
    }
};

template<class T, std::uint64_t Id> struct Codec<runtime::Ref<T>, Id> {
    inline static const WioNativeAbiHandleOps ops{WIO_NATIVE_ABI_VERSION, 0,
        [](void* p) noexcept { runtime::Ref<T> claim(static_cast<T*>(p)); (void)claim.Detach(); },
        [](void* p) noexcept { if (p) runtime::RefDeleter<T>::Execute(static_cast<T*>(p)); },
        [](const void*) noexcept { return Id; }};
    static runtime::Ref<T> read(const WioNativeAbiValue& v) {
        require(v.kind == WIO_NATIVE_ABI_OBJECT && v.typeId == Id && v.payload.handle.ops == &ops);
        return runtime::Ref<T>(static_cast<T*>(v.payload.handle.state));
    }
    static WioNativeAbiValue write(runtime::Ref<T> input) {
        WioNativeAbiValue v; v.kind = WIO_NATIVE_ABI_OBJECT; v.typeId = Id; v.flags = WIO_NATIVE_ABI_VALUE_OWNED;
        v.payload.handle = {input.Detach(), &ops, 1}; return v;
    }
};

// A ref/view wire argument points at another tagged value, not a foreign C++
// string/object layout. Repeated borrows of the same token share one local
// storage cell, preserving aliasing. Copy-back runs even on a caught failure.
class Frame {
    struct Slot { std::uint64_t type; std::any storage; std::function<void()> commit; };
    std::map<WioNativeAbiValue*, Slot> slots_;
public:
    template<class T, std::uint64_t Id> T& borrow(const WioNativeAbiValue& wire, bool mutableAccess) {
        require(wire.kind == WIO_NATIVE_ABI_REFERENCE && (wire.flags & WIO_NATIVE_ABI_VALUE_BORROWED) &&
            (!mutableAccess || (wire.flags & WIO_NATIVE_ABI_VALUE_MUTABLE)) &&
            wire.payload.mutableSlice.data && wire.payload.mutableSlice.size == sizeof(WioNativeAbiValue), WIO_NATIVE_ABI_INVALID_ARGUMENT);
        auto* token = static_cast<WioNativeAbiValue*>(wire.payload.mutableSlice.data);
        require(reinterpret_cast<std::uintptr_t>(token) % alignof(WioNativeAbiValue) == 0, WIO_NATIVE_ABI_INVALID_ARGUMENT);
        auto found = slots_.find(token);
        if (found == slots_.end()) {
            auto storage = std::make_shared<T>(Codec<T, Id>::read(*token));
            found = slots_.emplace(token, Slot{Id, storage, {}}).first;
        }
        require(found->second.type == Id);
        auto storage = std::any_cast<std::shared_ptr<T>>(found->second.storage);
        if (mutableAccess && !found->second.commit) found->second.commit = [token, storage] {
            auto replacement = Codec<T, Id>::write(*storage);
            WioNativeAbiReleaseValue(token); *token = replacement;
        };
        return *storage;
    }
    void commit() { for (auto& [_, slot] : slots_) if (slot.commit) { slot.commit(); slot.commit = {}; } }
};

template<class R, class... A, std::uint64_t Id> struct Codec<std::function<R(A...)>, Id> {
    struct Storage {
        std::atomic<std::uint32_t> refs{1};
        std::function<R(A...)> fn;
        std::thread::id thread=std::this_thread::get_id();
        explicit Storage(std::function<R(A...)> value) : fn(std::move(value)) {}
    };
    template<std::size_t... I> static void invoke(Storage& storage, const WioNativeAbiValue* args, WioNativeAbiValue* result, std::index_sequence<I...>) {
        if constexpr (std::is_void_v<R>) { storage.fn(Codec<A,0>::read(args[I])...); *result={}; }
        else *result=Codec<R,0>::write(storage.fn(Codec<A,0>::read(args[I])...));
    }
    inline static const WioNativeAbiCallbackOps ops{WIO_NATIVE_ABI_VERSION,0,
        [](void* p) noexcept { ++static_cast<Storage*>(p)->refs; },
        [](void* p) noexcept { auto* s=static_cast<Storage*>(p); if (--s->refs==0) delete s; },
        [](void* p,const WioNativeAbiValue* args,std::uint64_t count,WioNativeAbiValue* result,WioNativeAbiFailure* error) noexcept {
            if (!p || !result || count!=sizeof...(A) || (count&&!args)) return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            *result={};
            try { auto& storage=*static_cast<Storage*>(p); require(storage.thread==std::this_thread::get_id(),WIO_NATIVE_ABI_WRONG_THREAD);
                invoke(storage,args,result,std::index_sequence_for<A...>{}); return WIO_NATIVE_ABI_OK;
            } catch (const Error& e) { return failure(error,e.status,e.what()); }
            catch (const std::exception& e) { return failure(error,WIO_NATIVE_ABI_EXCEPTION,e.what()); }
            catch (...) { return failure(error,WIO_NATIVE_ABI_EXCEPTION,"callback failure"); }
        }};
    static WioNativeAbiValue write(std::function<R(A...)> fn) {
        WioNativeAbiValue v; v.kind=WIO_NATIVE_ABI_CALLBACK; v.typeId=Id; v.flags=WIO_NATIVE_ABI_VALUE_OWNED;
        v.payload.callback={new Storage(std::move(fn)),&ops}; return v;
    }
    static std::function<R(A...)> read(const WioNativeAbiValue& value) {
        require(value.kind == WIO_NATIVE_ABI_CALLBACK && value.payload.callback.ops &&
            value.payload.callback.ops->abiVersion == WIO_NATIVE_ABI_VERSION && value.payload.callback.ops->invoke);
        const auto callback = value.payload.callback;
        std::shared_ptr<void> owner;
        if (callback.ops->retain && callback.ops->release) {
            callback.ops->retain(callback.userdata);
            owner=std::shared_ptr<void>(callback.userdata,[ops=callback.ops](void* p) { ops->release(p); });
        }
        const auto thread = std::this_thread::get_id();
        return [callback, thread, owner](A... args) -> R {
            require(thread == std::this_thread::get_id(), WIO_NATIVE_ABI_WRONG_THREAD);
            std::vector<WioNativeAbiValue> arguments{Codec<A, 0>::write(args)...};
            struct Guard { std::vector<WioNativeAbiValue>& values; ~Guard() { for (auto& v : values) WioNativeAbiReleaseValue(&v); } } guard{arguments};
            WioNativeAbiValue result; WioNativeAbiFailure failure;
            auto status = callback.ops->invoke(callback.userdata, arguments.data(), arguments.size(), &result, &failure);
            struct ResultGuard { WioNativeAbiValue& value; ~ResultGuard() { WioNativeAbiReleaseValue(&value); } } resultGuard{result};
            require(status == WIO_NATIVE_ABI_OK, status);
            if constexpr (!std::is_void_v<R>) return Codec<R, 0>::read(result);
        };
    }
};
}
