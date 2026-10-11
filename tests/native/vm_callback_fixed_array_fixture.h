#pragma once

#include <wir_sequence_abi.h>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace vm_callback_fixed_array
{
    inline std::int32_t defaults = 0;
    inline std::int32_t Initialize()
    {
        ++defaults;
        return 71;
    }
    inline std::int32_t Defaults()
    {
        return defaults;
    }
    struct Point
    {
        std::int32_t x;
        std::int32_t hidden = Initialize();
    };
    struct Bundle
    {
        std::int32_t hidden = 83;
        std::array<std::array<std::int32_t, 2>, 2> matrix{};
        std::array<std::int32_t, 3> values{};
        std::array<bool, 2> flags{};
        std::array<std::int32_t, 0> empty{};
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
    template <class T> T Echo(T value)
    {
        return value;
    }
    template <class T> T Call(std::function<T(T)> callback, T value)
    {
        return callback(std::move(value));
    }
    template <class T, class A> T Borrow(std::function<T(A, A)> callback, T value)
    {
        (void)callback(A::borrowView(value), A::borrow(value));
        return value;
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
        Callback change = [](Cell readonly, Cell writable)
        {
            auto copy = readonly.read();
            copy[0] += 2;
            writable.write(copy);
            return readonly.read();
        };
        return processor->_WF_Apply_function(change) == 9 ? 0 : 1;
    }
    template <class T, class A> void Capture(std::function<T(A, A)>, T)
    {
    }
    template <class T> void Modify(T& value)
    {
        value[0] += 2;
    }
    inline void SetBool(bool& value)
    {
        value = !value;
    }
    template <class T> bool Alias(typename T::value_type& first, T& values)
    {
        if (std::addressof(first) != std::addressof(values[0]))
            return false;
        first += 2;
        return true;
    }
    inline Bundle PodEcho(Bundle value)
    {
        return value;
    }
    inline std::int32_t Hidden(Bundle value)
    {
        return value.hidden;
    }
    template <class T> std::int32_t Probe(T value)
    {
        using Adapter = wio::wir_backend::abi::SequenceAdapter<T, 101, 102>;
        std::array<WioNativeAbiValue, std::tuple_size_v<T>> elements{};
        WioNativeAbiValue result{};
        WioNativeAbiFailure failure{};
        // Validate extent before attempting to decode any element.
        if (Adapter::pack(elements.data(), elements.size() + 1, &result, &failure) != WIO_NATIVE_ABI_INVALID_ARGUMENT ||
            result.kind != WIO_NATIVE_ABI_VOID)
            return 1;
        if constexpr (std::tuple_size_v<T> != 0)
            if (Adapter::pack(elements.data(), elements.size() - 1, &result, &failure) !=
                WIO_NATIVE_ABI_INVALID_ARGUMENT)
                return 2;
        auto wire = wio::wir_backend::abi::Codec<T, 101>::write(value);
        const auto status = Adapter::read(&wire, value.size(), &result, &failure);
        WioNativeAbiReleaseValue(&wire);
        if (status != WIO_NATIVE_ABI_INVALID_ARGUMENT || result.kind != WIO_NATIVE_ABI_VOID)
            return 3;
        return 0;
    }
    inline void Print()
    {
        std::cout << "native-callback-fixed-array-ok:30\n";
    }
} // namespace vm_callback_fixed_array
