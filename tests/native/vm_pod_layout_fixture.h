#pragma once

#include <std_async.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_layout
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
        double energy;
        Point position;
        Cell<std::int32_t> cell;
        std::int32_t hidden = 123;
        char marker = '\0';
        bool active = false;
    };
    struct Empty
    {
    };

    inline Packet Make()
    {
        return {7, 12.5, {2.0f, 3.0f}, {11}, 99};
    }
    inline double Score(const Packet& value)
    {
        return value.tag + value.energy + value.position.x + value.position.y + value.cell.value;
    }
    inline double Score(Packet&)
    {
        return -999.0;
    }
    inline void Adjust(Packet& value)
    {
        value.tag += 1;
        value.energy += 0.5;
        value.position.x += 10.0f;
        value.cell.value += 4;
        ++value.hidden;
        value.marker = 'W';
        value.active = true;
    }
    inline void Aliased(Packet& left, Packet& right)
    {
        left.tag = 20;
        right.tag += left.tag;
    }
    inline Packet Identity(Packet value)
    {
        return value;
    }
    inline std::int32_t Hidden(const Packet& value)
    {
        return value.hidden;
    }
    inline void HighByte(Packet& value)
    {
        value.marker = static_cast<char>(255);
    }
    inline std::uint32_t Code(char value)
    {
        return static_cast<unsigned char>(value);
    }
    inline Empty IdentityEmpty(Empty value)
    {
        return value;
    }
    template <class T> inline void Increment(Cell<T>& value)
    {
        ++value.value;
    }
    template <class T> inline Cell<T> EchoCell(Cell<T> value)
    {
        return value;
    }
    inline wio::runtime::AsyncTask<Packet> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return Make();
    }
    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_layout
