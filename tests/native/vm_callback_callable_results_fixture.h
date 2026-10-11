#pragma once

#include <cstdint>
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace vm_callback_callable_results
{
    inline std::atomic_int constructed{0};
    inline std::atomic_int destroyed{0};
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
    struct Point
    {
        std::int32_t x;
    };
    using Unary = std::function<std::int32_t(std::int32_t)>;
    inline std::int32_t Higher(std::function<std::int32_t(Unary)> callback)
    {
        return callback([](std::int32_t value) { return value + 2; });
    }
    inline std::int32_t Factory(std::function<Unary(std::int32_t)> callback)
    {
        auto result = callback(7);
        return result(11);
    }
    inline std::int32_t RejectBorrowFactory(std::function<Unary()> callback)
    {
        try
        {
            (void)callback();
        }
        catch (const std::exception&)
        {
            return 0;
        }
        return 1;
    }
    inline std::int32_t RejectMutatedFactory(std::function<Unary()> callback)
    {
        try
        {
            auto result = callback();
            (void)result(0);
            (void)result(11);
        }
        catch (const std::exception&)
        {
            return 0;
        }
        return 1;
    }
    inline std::int32_t Counter(std::function<Unary()> factory)
    {
        auto callback = factory();
        const auto first = callback(1);
        return first + callback(2);
    }
    inline Unary Make(std::int32_t seed)
    {
        return [seed](std::int32_t value) { return seed + value; };
    }
    inline Unary Relay(Unary callback)
    {
        return callback;
    }
    template <class R, class... A> std::function<R(A...)> RelayGeneric(std::function<R(A...)> callback)
    {
        return callback;
    }
    template <class A> std::int32_t RejectCapture(std::function<A(A)> callback)
    {
        std::int32_t value = 7;
        try
        {
            (void)callback(A::borrow(value));
        }
        catch (const std::exception&)
        {
            return 0;
        }
        return 1;
    }
    template <class T> std::int32_t LifetimeProbes()
    {
        using namespace wio::wir_backend::abi;
        using A = wio::wir_backend::Place<T>;
        using Signature = std::function<A(A)>;
        using Mutable =
            CallbackCodecWithResultBorrow<Signature, 301, 302, true, std::integer_sequence<bool, true>, 302>;
        A escaped;
        auto wire = Mutable::write(
            [&](A value)
            {
                escaped = value;
                return value;
            });
        T value = 7;
        auto restored = Mutable::read(wire);
        auto caller = A::borrow(value);
        if (restored(caller).borrowIdentity() != caller.borrowIdentity())
            return 1;
        try
        {
            (void)escaped.read();
            return 2;
        }
        catch (const std::exception&)
        {
        }
        auto bad = wire;
        bad.typeId = 999;
        try
        {
            (void)Mutable::read(bad);
            return 3;
        }
        catch (const std::exception&)
        {
        }
        WioNativeAbiReleaseValue(&wire);
        T other = 9;
        wire = Mutable::write([&](A) { return A::borrow(other); });
        try
        {
            (void)Mutable::read(wire)(caller);
            return 4;
        }
        catch (const std::exception&)
        {
        }
        WioNativeAbiReleaseValue(&wire);
        using Upgrade =
            CallbackCodecWithResultBorrow<Signature, 401, 302, true, std::integer_sequence<bool, false>, 302>;
        wire = Upgrade::write([](A value) { return value; });
        try
        {
            (void)Upgrade::read(wire)(caller.asView());
            return 5;
        }
        catch (const std::exception&)
        {
        }
        WioNativeAbiReleaseValue(&wire);
        wire = Mutable::write(
            [](A value) -> A
            {
                value.write(11);
                throw std::runtime_error("expected");
            });
        try
        {
            (void)Mutable::read(wire)(caller);
            return 6;
        }
        catch (const std::exception&)
        {
        }
        WioNativeAbiReleaseValue(&wire);
        return value == 11 ? 0 : 7;
    }
    inline std::optional<Unary> Maybe(bool present)
    {
        return present ? std::optional<Unary>{Make(7)} : std::nullopt;
    }
    template <class T, class A> T Borrow(std::function<A(A)> callback, T value)
    {
        auto caller = A::borrow(value);
        auto result = callback(caller);
        if (result.borrowIdentity() != caller.borrowIdentity())
            throw std::runtime_error("borrowed callback result lost its caller cell");
        return result.read();
    }
    template <class T, class A> T View(std::function<A(A)> callback, T value)
    {
        auto caller = A::borrowView(value);
        auto result = callback(caller);
        if (result.borrowIdentity() != caller.borrowIdentity() || result.canRebind())
            throw std::runtime_error("view callback result lost its readonly caller cell");
        return result.read();
    }
    template <class T, class A> T MakeBorrow(std::function<A(A)> callback, T value)
    {
        using Codec = wio::wir_backend::abi::CallbackCodecWithResultBorrow<std::function<A(A)>, 701, 702, true,
                                                                           std::integer_sequence<bool, true>, 702>;
        auto wire = Codec::write(std::move(callback));
        auto restored = Codec::read(wire);
        WioNativeAbiReleaseValue(&wire);
        auto caller = A::borrow(value);
        auto result = restored(caller);
        if (result.borrowIdentity() != caller.borrowIdentity())
            throw std::runtime_error("roundtrip result points at a temporary callback cell");
        return result.read();
    }
    template <class T> std::int32_t Hosted(wio::runtime::Ref<T> processor)
    {
        auto callback = processor->_WF_Make_i32(7);
        auto nested = processor->_WF_Factory();
        auto present = processor->_WF_Maybe_bool(true);
        auto missing = processor->_WF_Maybe_bool(false);
        if (!present || missing || processor->_WF_Optional_functionN(present) != 18 ||
            processor->_WF_Optional_functionN(missing) != 0)
            return -1;
        bool rejected = false;
        try
        {
            (void)processor->_WF_Borrowed();
        }
        catch (const std::exception&)
        {
            rejected = true;
        }
        if (!rejected)
            return -2;
        return callback(11) + nested(7)(11);
    }
    inline Unary retained;
    inline void Remember(Unary callback)
    {
        retained = std::move(callback);
    }
    inline std::int32_t Run()
    {
        return retained(11);
    }
    inline std::int32_t Async(Unary callback)
    {
        return std::async(std::launch::async, [callback = std::move(callback)] { return callback(11); }).get();
    }
    inline void Clear()
    {
        retained = {};
    }
    template <class F> void Capture(F)
    {
    }
    inline void Print()
    {
        std::cout << "native-callback-callable-results-ok:30\n";
    }
} // namespace vm_callback_callable_results
