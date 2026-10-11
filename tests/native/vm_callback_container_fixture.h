#pragma once

#include <any.h>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace vm_callback_container
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
        throw std::runtime_error("container-callback-failure");
    }

    template <class T> T Call(std::function<T(T)> callback, T input)
    {
        return callback(std::move(input));
    }
    template <class T> T Pair(std::function<T(T, T)> callback, T first, T second)
    {
        return callback(std::move(first), std::move(second));
    }
    template <class T> std::int32_t Factory(std::function<T()> callback)
    {
        return static_cast<std::int32_t>(callback().size());
    }
    template <class T> void Capture(std::function<T(T)>, T)
    {
    }

    inline std::function<std::int32_t()> stored;
    template <class T> void Store(std::function<std::int32_t(T)> callback, T input)
    {
        stored = [callback = std::move(callback), input = std::move(input)] { return callback(input); };
    }
    inline std::int32_t Run()
    {
        return stored();
    }
    inline std::int32_t Worker()
    {
        std::int32_t result = 0;
        std::exception_ptr error;
        std::thread worker(
            [&]
            {
                try
                {
                    result = stored();
                }
                catch (...)
                {
                    error = std::current_exception();
                }
            });
        worker.join();
        if (error)
            std::rethrow_exception(error);
        return result;
    }
    inline void Clear()
    {
        stored = {};
    }
    inline wio::runtime::Any Nest(wio::runtime::Any value)
    {
        return wio::runtime::Any::Box(std::move(value));
    }
    inline void Print()
    {
        std::cout << "native-callback-container-ok:30\n";
    }
} // namespace vm_callback_container
