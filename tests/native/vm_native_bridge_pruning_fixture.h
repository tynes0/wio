#pragma once

#include <any.h>
#include <std_async.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>

namespace vm_bridge_pruning
{
    struct Point
    {
        std::int32_t x;
        std::int32_t y;
    };
    template <class T> struct Cell
    {
        T value;
    };
    inline std::int32_t closed = 0;
    inline void Close()
    {
        ++closed;
    }
    inline std::int32_t Closed()
    {
        return closed;
    }
    template <class T> T Make(std::int32_t value)
    {
        return T::Create(value);
    }
    template <class T> std::int32_t Read(T value)
    {
        return value->_WF_Read();
    }
    inline Point Adjust(Point value)
    {
        value.x += 3;
        value.y += 5;
        return value;
    }
    template <class T> Cell<T> Echo(Cell<T> value)
    {
        return value;
    }
    inline std::int32_t Callback(std::function<std::int32_t(std::int32_t)> callback)
    {
        return callback(11);
    }
    inline wio::runtime::Any Identity(wio::runtime::Any value)
    {
        return value;
    }
    inline wio::runtime::AsyncTask<std::int32_t> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return 23;
    }
    inline void Print(std::string message)
    {
        std::cout << message << '\n';
    }
} // namespace vm_bridge_pruning
