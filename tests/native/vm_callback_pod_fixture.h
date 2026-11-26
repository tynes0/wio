#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace vm_callback_pod
{
    struct Point
    {
        float x;
        float y;
    };
    template <class T> struct Cell
    {
        T value;
    };
    struct Packet
    {
        std::uint8_t tag;
        Point position;
        Cell<std::int32_t> cell;
        std::array<std::int32_t, 2> samples;
        double energy;
        char marker;
        bool active;
        std::int32_t hidden = 71;
    };
    struct Empty
    {
    };

    template <class T> T Call(std::function<T(T)> callback, T value)
    {
        return callback(value);
    }
    inline Packet Make()
    {
        return {3, {1.25f, 2.5f}, {11}, {13, 17}, 4.5, '\0', false, 97};
    }
    inline Packet Combine(std::function<Packet(Point, Cell<std::int32_t>)> callback)
    {
        return callback({2.0f, 3.0f}, {19});
    }
    inline std::int32_t Hidden(const Packet& value)
    {
        return value.hidden;
    }
    inline void Notify(std::function<void(Packet)> callback)
    {
        callback(Make());
    }
    template <class T> void Ignore(std::function<void(T)>)
    {
    }
    inline std::function<Packet(Packet)> stored;
    inline void Store(std::function<Packet(Packet)> callback)
    {
        stored = std::move(callback);
    }
    inline Packet Run()
    {
        return stored(Make());
    }
    inline Packet Worker()
    {
        Packet result{};
        std::exception_ptr error;
        std::thread thread(
            [&]
            {
                try
                {
                    result = stored(Make());
                }
                catch (...)
                {
                    error = std::current_exception();
                }
            });
        thread.join();
        if (error)
            std::rethrow_exception(error);
        return result;
    }
    inline bool Expired()
    {
        try
        {
            (void)stored(Make());
        }
        catch (const std::exception&)
        {
            return true;
        }
        return false;
    }
    inline bool WrongThread(std::function<Packet(Packet)> callback)
    {
        bool rejected = false;
        std::thread thread(
            [&]
            {
                try
                {
                    (void)callback(Make());
                }
                catch (const std::exception&)
                {
                    rejected = true;
                }
            });
        thread.join();
        return rejected;
    }
    inline void Clear()
    {
        stored = {};
    }
    inline void Capture(std::function<Packet(Packet)> callback)
    {
        Store(std::move(callback));
    }
    inline void Fail()
    {
        throw std::runtime_error("pod-callback-user-failure");
    }
    inline void Print()
    {
        std::cout << "native-callback-pod-ok:30\n";
    }
} // namespace vm_callback_pod
