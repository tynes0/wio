#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace vm_callback_object
{
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
        throw std::runtime_error("object-callback-failure");
    }
    template <class T> T Call(std::function<T(T)> callback, T value)
    {
        return callback(value);
    }
    template <class T> T Pair(std::function<T(T, T)> callback, T first, T second)
    {
        return callback(first, second);
    }
    template <class T> std::int32_t Factory(std::function<T()> callback)
    {
        auto value = callback();
        return value->_WF_Read();
    }
    template <class T> std::int32_t Mutate(std::function<void(T)> callback, T value)
    {
        callback(value);
        return value->_WF_Read();
    }
    inline std::function<std::int32_t()> stored;
    template <class T> void Store(std::function<T(T)> callback, T input)
    {
        stored = [callback = std::move(callback), input = std::move(input)]
        {
            auto value = callback(input);
            return value->_WF_Read();
        };
    }
    inline std::int32_t Run()
    {
        return stored();
    }
    inline std::int32_t Worker()
    {
        std::int32_t value = 0;
        std::exception_ptr error;
        std::thread worker(
            [&]
            {
                try
                {
                    value = stored();
                }
                catch (...)
                {
                    error = std::current_exception();
                }
            });
        worker.join();
        if (error)
            std::rethrow_exception(error);
        return value;
    }
    inline bool Expired()
    {
        try
        {
            (void)stored();
        }
        catch (const std::exception&)
        {
            return true;
        }
        return false;
    }
    inline void Clear()
    {
        stored = {};
    }
    template <class T> void Capture(std::function<T(T)>, T)
    {
    }
    inline void Print()
    {
        std::cout << "native-callback-object-ok:30\n";
    }
} // namespace vm_callback_object
