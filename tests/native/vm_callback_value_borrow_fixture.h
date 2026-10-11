#pragma once

#include <wir_any_abi.h>
#include <wir_object_abi.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace vm_callback_value_borrow
{
    struct Point
    {
        std::int32_t x;
        std::int32_t hidden = 71;
    };
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;
    inline void Constructed()
    {
        ++constructed;
    }
    inline void Destroyed()
    {
        ++destroyed;
    }
    inline bool Balanced()
    {
        return constructed == destroyed;
    }
    inline std::int32_t Live()
    {
        return constructed - destroyed;
    }
    inline void Fail()
    {
        throw std::runtime_error("value-borrow-failure");
    }

    template <class T, class A> T Call(std::function<T(A, A)> callback, T initial)
    {
        auto value = std::move(initial);
        (void)callback(A::borrowView(value), A::borrow(value));
        return value;
    }
    template <class T, class A> T Reverse(std::function<T(A, A)> callback, T initial)
    {
        auto value = std::move(initial);
        (void)callback(A::borrow(value), A::borrowView(value));
        return value;
    }
    template <class T, class A> void Capture(std::function<T(A, A)>, T)
    {
    }
    template <class T, class A> std::int32_t View(std::function<std::int32_t(A)> callback, T initial)
    {
        return callback(A::borrowView(initial));
    }
    inline std::function<std::int32_t()> retained;
    template <class T, class A> void Store(std::function<T(A, A)> callback, T initial)
    {
        retained = [callback, initial]()
        {
            auto value = initial;
            (void)callback(A::borrowView(value), A::borrow(value));
            return static_cast<std::int32_t>(value.size());
        };
    }
    inline std::int32_t Run()
    {
        return retained();
    }
    inline void Clear()
    {
        retained = {};
    }

    inline const WioNativeAbiCallbackOps AtomicOps{
        WIO_NATIVE_ABI_VERSION, 0, nullptr, nullptr,
        [](void* state, const WioNativeAbiValue* args, std::uint64_t count, WioNativeAbiValue* result,
           WioNativeAbiFailure*) noexcept
        {
            if (count != 2)
                return WIO_NATIVE_ABI_INVALID_ARGUMENT;
            auto* first = static_cast<WioNativeAbiValue*>(args[0].payload.mutableSlice.data);
            auto* second = static_cast<WioNativeAbiValue*>(args[1].payload.mutableSlice.data);
            WioNativeAbiReleaseValue(first);
            *first = wio::wir_backend::abi::Codec<std::vector<std::int32_t>, 101>::write({9, 23});
            WioNativeAbiReleaseValue(second);
            const auto mode = *static_cast<unsigned*>(state);
            if (mode == 0)
            {
                second->kind = WIO_NATIVE_ABI_U64;
                second->payload.unsignedInteger = 3;
            }
            else
                *second = wio::wir_backend::abi::Codec<std::vector<std::int32_t>, 102>::write({29});
            if (mode == 2)
                first->typeId = 102;
            *result = {};
            result->kind = WIO_NATIVE_ABI_I64;
            result->payload.signedInteger = 9;
            return WIO_NATIVE_ABI_OK;
        }};
    template <class A> std::int32_t AtomicProbe(std::function<std::int32_t(A, A)>)
    {
        using Codec =
            wio::wir_backend::abi::CallbackCodecWithBorrows<std::function<std::int32_t(A, A)>, 100, 103,
                                                            std::integer_sequence<bool, true, true>, 101, 102>;
        unsigned mode = 0;
        WioNativeAbiValue wire{};
        wire.kind = WIO_NATIVE_ABI_CALLBACK;
        wire.payload.callback = {&mode, &AtomicOps};
        auto callback = Codec::read(wire);
        std::vector<std::int32_t> first{7}, second{19};
        for (const auto invalid : {0u, 2u})
        {
            mode = invalid;
            try
            {
                (void)callback(A::borrow(first), A::borrow(second));
                return 1;
            }
            catch (const wio::wir_backend::abi::Error&)
            {
                if (first != std::vector<std::int32_t>{7} || second != std::vector<std::int32_t>{19})
                    return 2;
            }
        }
        mode = 1;
        return callback(A::borrow(first), A::borrow(second)) == 9 && first == std::vector<std::int32_t>{9, 23} &&
                       second == std::vector<std::int32_t>{29}
                   ? 0
                   : 3;
    }

    template <class R, class C, class A> A parameter(R (C::*)(A));
    template <class Signature> struct Arguments;
    template <class R, class A, class B> struct Arguments<std::function<R(A, B)>>
    {
        using Cell = A;
    };
    template <class T> std::int32_t Hosted(wio::runtime::Ref<T> processor)
    {
        using Callback = decltype(parameter(&T::_WF_Apply_function));
        using Cell = typename Arguments<Callback>::Cell;
        Callback callback = [](Cell readonly, Cell writable)
        {
            auto value = readonly.read();
            value.push_back(23);
            writable.write(std::move(value));
            return readonly.read();
        };
        return processor->_WF_Apply_function(callback) == 23 && processor->_WF_Forward_function(callback) == 23 ? 0 : 1;
    }
    inline WioNativeObjectHost capturedHost{};
    inline std::function<void()> hostLease;
    template <class T> std::uint64_t CaptureHost(wio::runtime::Ref<T> processor)
    {
        const auto* proxy = dynamic_cast<const wio::wir_backend::abi::HostObject<T>*>(processor.Get());
        wio::wir_backend::abi::require(proxy != nullptr);
        capturedHost = proxy->hostLease().host();
        hostLease = [processor] {};
        return reinterpret_cast<std::uintptr_t>(&capturedHost);
    }
    inline void ClearHost()
    {
        hostLease = {};
        capturedHost = {};
    }
    inline void Print()
    {
        std::cout << "native-callback-value-borrow-ok:30\n";
    }
} // namespace vm_callback_value_borrow
