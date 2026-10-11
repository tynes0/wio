#pragma once

#include <ref.h>
#include <wir_abi.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace vm_callback_borrow
{
    struct Packet
    {
        std::uint8_t tag = 3;
        double weight = 1.5;
        std::int32_t value = 7;
        std::int32_t hidden = 97;
    };

    template <class A> std::int32_t Scalar(std::function<std::int32_t(A, A)> callback)
    {
        std::int32_t value = 7;
        const auto result = callback(A::borrowView(value), A::borrow(value));
        return result == 12 && value == 12 ? 0 : 1;
    }
    template <class A> std::int32_t Reverse(std::function<std::int32_t(A, A)> callback)
    {
        std::int32_t value = 7;
        const auto result = callback(A::borrow(value), A::borrowView(value));
        return result == 12 && value == 12 ? 0 : 1;
    }
    template <class A> std::int32_t Pod(std::function<std::int32_t(A, A)> callback)
    {
        Packet value;
        const auto result = callback(A::borrowView(value), A::borrow(value));
        return result == 12 && value.value == 12 && value.hidden == 97 && value.tag == 3 && value.weight == 1.5 ? 0 : 1;
    }
    inline std::function<std::int32_t()> retained;
    template <class A> void Remember(std::function<std::int32_t(A, A)> callback)
    {
        retained = [callback] { return Scalar(callback); };
    }
    inline std::int32_t Run()
    {
        return retained();
    }
    inline void Clear()
    {
        retained = {};
    }
    template <class A, class B> void Capture(std::function<std::int32_t(A, B)>)
    {
    }
    template <class A> std::int32_t CodecProbe(std::function<std::int32_t(A, A)>)
    {
        using Codec = wio::wir_backend::abi::CallbackCodecWithBorrows<std::function<std::int32_t(A, A)>, 77, 78,
                                                                      std::integer_sequence<bool, false, true>, 79, 79>;
        auto callback = Codec::write(
            [](A readonly, A writable) -> std::int32_t
            {
                writable.write(readonly.read() + 5);
                throw std::runtime_error("native callback failure after write");
            });
        WioNativeAbiValue token{};
        token.kind = WIO_NATIVE_ABI_I64;
        token.typeId = 79;
        token.payload.signedInteger = 7;
        WioNativeAbiValue arguments[2]{};
        for (auto& argument : arguments)
        {
            argument.kind = WIO_NATIVE_ABI_REFERENCE;
            argument.typeId = 79;
            argument.flags = WIO_NATIVE_ABI_VALUE_BORROWED;
            argument.payload.mutableSlice = {&token, sizeof(WioNativeAbiValue)};
        }
        arguments[1].flags |= WIO_NATIVE_ABI_VALUE_MUTABLE;
        WioNativeAbiValue result{};
        WioNativeAbiFailure failure{};
        const auto status =
            callback.payload.callback.ops->invoke(callback.payload.callback.userdata, arguments, 2, &result, &failure);
        WioNativeAbiReleaseValue(&callback);
        if (status != WIO_NATIVE_ABI_EXCEPTION || token.payload.signedInteger != 12 ||
            result.kind != WIO_NATIVE_ABI_VOID)
            return 1;
        callback = Codec::write(
            [](A readonly, A) -> std::int32_t
            {
                readonly.write(99);
                return 0;
            });
        const auto readonlyStatus =
            callback.payload.callback.ops->invoke(callback.payload.callback.userdata, arguments, 2, &result, &failure);
        WioNativeAbiReleaseValue(&callback);
        return readonlyStatus == WIO_NATIVE_ABI_EXCEPTION && token.payload.signedInteger == 12 &&
                       result.kind == WIO_NATIVE_ABI_VOID
                   ? 0
                   : 2;
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
            writable.write(readonly.read() + 5);
            return readonly.read();
        };
        if (processor->_WF_Apply_function(callback) != 24 || processor->_WF_Forwarded_function(callback) != 0)
            return 1;
        using PodCallback = decltype(parameter(&T::_WF_ApplyPod_function));
        using PodCell = typename Arguments<PodCallback>::Cell;
        PodCallback pod = [](PodCell readonly, PodCell writable)
        {
            if (readonly.read().hidden != 97)
                throw std::runtime_error("POD hidden storage lost");
            auto value = readonly.read();
            value.value += 5;
            writable.write(value);
            return readonly.read().value;
        };
        return processor->_WF_ApplyPod_function(pod) == 24 ? 0 : 2;
    }
    inline void Print()
    {
        std::cout << "native-callback-borrow-ok:30\n";
    }
} // namespace vm_callback_borrow
