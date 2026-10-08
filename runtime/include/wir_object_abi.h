#pragma once

#include "wir_container_abi.h"
#include <wio_native_object.h>
#include <array>

namespace wio::wir_backend::abi
{
    // Each codec can acquire an owned wire. Incremental initialization keeps
    // earlier arguments guarded if a later conversion or host call throws.
    template <std::size_t Count> class ObjectArguments final
    {
    public:
        ObjectArguments() = default;
        ObjectArguments(const ObjectArguments&) = delete;
        ObjectArguments& operator=(const ObjectArguments&) = delete;
        ~ObjectArguments()
        {
            for (auto& value : values_)
                WioNativeAbiReleaseValue(&value);
        }
        WioNativeAbiValue& operator[](std::size_t index) noexcept
        {
            return values_[index];
        }
        const WioNativeAbiValue* data() const noexcept
        {
            return values_.data();
        }

    private:
        std::array<WioNativeAbiValue, Count> values_{};
    };

    class ObjectDispatchError final : public Error
    {
    public:
        ObjectDispatchError(WioNativeAbiStatus status, const WioNativeAbiFailure& failure)
            : Error(status), message_(failure.message.data && failure.message.size <= 4096
                                          ? std::string(static_cast<const char*>(failure.message.data),
                                                        static_cast<std::size_t>(failure.message.size))
                                          : "object host dispatch failed")
        {
        }

        const char* what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };

    // Copies of a proxy Ref share this one lease. WeakRef storage does not own
    // it. OnZeroStrong releases the host before delayed weak deallocation.
    class ObjectHostLease final
    {
    public:
        explicit ObjectHostLease(const WioNativeObjectHost& host) : host_(host), caller_(std::this_thread::get_id())
        {
            validate(host_);
            WioNativeAbiRetain(host_.owner);
        }

        static void validate(const WioNativeObjectHost& host)
        {
            const auto& owner = host.owner;
            require(host.version == WIO_NATIVE_OBJECT_VERSION && host.flags == 0 && host.typeId && host.objectId &&
                        host.generation && owner.generation == host.generation && host.invoke && owner.state &&
                        owner.ops && owner.ops->abiVersion == WIO_NATIVE_ABI_VERSION && owner.ops->retain &&
                        owner.ops->release && owner.ops->typeId && owner.ops->typeId(owner.state) == host.typeId,
                    WIO_NATIVE_ABI_INVALID_ARGUMENT);
        }

        ~ObjectHostLease()
        {
            reset();
        }
        ObjectHostLease(const ObjectHostLease&) = delete;
        ObjectHostLease& operator=(const ObjectHostLease&) = delete;

        void reset() noexcept
        {
            // Invalidate before release, which can re-enter host cleanup.
            const auto owner = host_.owner;
            host_ = {};
            auto claim = owner;
            WioNativeAbiRelease(&claim);
        }

        [[nodiscard]] WioNativeObjectHost host() const
        {
            require(host_.owner.state != nullptr, WIO_NATIVE_ABI_STALE_HANDLE);
            return host_;
        }

        WioNativeAbiStatus invoke(std::uint64_t methodId, const WioNativeAbiValue* arguments, std::uint64_t count,
                                  WioNativeAbiValue* result, WioNativeAbiFailure* error) const noexcept
        {
            if (error)
                *error = {};
            if (!result)
                return failure(error, WIO_NATIVE_ABI_INVALID_ARGUMENT, "object dispatch requires an output cell");
            *result = {};
            if (!host_.owner.state)
                return failure(error, WIO_NATIVE_ABI_STALE_HANDLE, "object host lease is no longer alive");
            if (!methodId || (count && !arguments))
                return failure(error, WIO_NATIVE_ABI_INVALID_ARGUMENT,
                               "object dispatch has an invalid method or argument buffer");
            if (std::this_thread::get_id() != caller_)
                return failure(error, WIO_NATIVE_ABI_WRONG_THREAD, "object host dispatch requires its caller thread");
            return host_.invoke(&host_, methodId, arguments, count, result, error);
        }

        template <class R, std::uint64_t ResultId>
        R call(std::uint64_t methodId, const WioNativeAbiValue* arguments, std::uint64_t count) const
        {
            WioNativeAbiValue result{};
            struct Guard
            {
                WioNativeAbiValue& value;
                ~Guard()
                {
                    WioNativeAbiReleaseValue(&value);
                }
            } guard{result};
            WioNativeAbiFailure failure{};
            const auto status = invoke(methodId, arguments, count, &result, &failure);
            if (status != WIO_NATIVE_ABI_OK)
                throw ObjectDispatchError(status, failure);
            if constexpr (std::is_void_v<R>)
                require(result.kind == WIO_NATIVE_ABI_VOID);
            else
                return Codec<R, ResultId>::read(result);
        }

    private:
        WioNativeObjectHost host_{};
        std::thread::id caller_;
    };

    // Concrete generated proxies override native-visible virtual methods and
    // delegate them through hostLease(). Their native constructor/destructor
    // paths must not execute the hosted Wio object's lifecycle a second time.
    template <class T> class HostObject : public T
    {
    public:
        template <class... Args>
        explicit HostObject(const WioNativeObjectHost& host, Args&&... arguments)
            : T(std::forward<Args>(arguments)...), lease_(host)
        {
        }

        void OnZeroStrong() noexcept final
        {
            T::OnZeroStrong();
            lease_.reset();
        }

        [[nodiscard]] const ObjectHostLease& hostLease() const noexcept
        {
            return lease_;
        }

    private:
        ObjectHostLease lease_;
    };

    template <class T, class Proxy, std::uint64_t Id> struct ObjectAdapter
    {
        static_assert(std::is_base_of_v<HostObject<T>, Proxy>);
        using Ref = runtime::Ref<T>;
        using Weak = runtime::WeakRef<T>;
        using WeakStorage = Box<Weak, Id>;
        using Boundary = ContainerAdapter<Ref, Id>;

        static Ref storage(const WioNativeAbiValue* value)
        {
            require(value && value->kind == WIO_NATIVE_ABI_OBJECT && value->typeId == Id &&
                    value->flags & (WIO_NATIVE_ABI_VALUE_OWNED | WIO_NATIVE_ABI_VALUE_BORROWED) &&
                    value->payload.handle.state && value->payload.handle.generation == 1);
            return Codec<Ref, Id>::read(*value);
        }

        static WioNativeAbiStatus create(const WioNativeObjectHost* host, WioNativeAbiValue* result,
                                         WioNativeAbiFailure* error) noexcept
        {
            if (result)
                *result = {};
            return Boundary::boundary(error,
                                      [&]
                                      {
                                          require(host && result && host->typeId == Id,
                                                  WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          ObjectHostLease::validate(*host);
                                          auto proxy = runtime::Ref<Proxy>::Create(*host);
                                          *result = Codec<Ref, Id>::write(Ref(proxy));
                                      });
        }

        static WioNativeAbiStatus host(const WioNativeAbiValue* value, WioNativeObjectHost* result,
                                       WioNativeAbiFailure* error) noexcept
        {
            if (result)
                *result = {};
            return Boundary::boundary(error,
                                      [&]
                                      {
                                          require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          auto reference = storage(value);
                                          auto* proxy = dynamic_cast<const HostObject<T>*>(reference.Get());
                                          require(proxy);
                                          *result = proxy->hostLease().host();
                                      });
        }

        static WioNativeAbiStatus weak(const WioNativeAbiValue* value, WioNativeAbiHandle* result,
                                       WioNativeAbiFailure* error) noexcept
        {
            if (result)
                *result = {};
            return Boundary::boundary(error,
                                      [&]
                                      {
                                          require(result, WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          auto reference = storage(value);
                                          require(dynamic_cast<const HostObject<T>*>(reference.Get()) != nullptr);
                                          *result = {new WeakStorage(Weak(reference)), &WeakStorage::ops, 1};
                                      });
        }

        static WioNativeAbiStatus lock(const WioNativeAbiHandle* value, WioNativeAbiValue* result,
                                       WioNativeAbiFailure* error) noexcept
        {
            if (result)
                *result = {};
            return Boundary::boundary(error,
                                      [&]
                                      {
                                          require(result && value && value->state && value->ops == &WeakStorage::ops &&
                                                      value->generation == 1,
                                                  WIO_NATIVE_ABI_INVALID_ARGUMENT);
                                          auto reference = static_cast<const WeakStorage*>(value->state)->value.Lock();
                                          require(bool(reference), WIO_NATIVE_ABI_STALE_HANDLE);
                                          *result = Codec<Ref, Id>::write(std::move(reference));
                                      });
        }
    };
} // namespace wio::wir_backend::abi
