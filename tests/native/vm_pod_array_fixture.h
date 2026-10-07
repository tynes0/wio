#pragma once

#include <std_async.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_array
{
    enum class Mode : std::uint8_t
    {
        Idle = 0,
        Ready = 7
    };

    struct Point
    {
        float x;
        float y;
        std::int32_t hidden = 17;
    };

    template <class T> struct Triplet
    {
        std::array<T, 3> values;
    };

    struct Bundle
    {
        std::array<Point, 2> points;
        std::array<std::array<std::int16_t, 2>, 2> matrix;
        std::array<char, 2> markers;
        std::array<bool, 2> flags;
        std::array<float, 2> floats;
        std::array<std::int32_t, 0> empty;
        std::array<Mode, 2> modes;
    };

    inline Bundle Make()
    {
        return {{{{1.0f, 2.0f, 41}, {3.0f, 4.0f, 42}}},
                {{{{1, 2}}, {{3, 4}}}},
                {{'\0', static_cast<char>(255)}},
                {{false, true}},
                {{1.25f, -2.5f}},
                {},
                {{Mode::Idle, Mode::Ready}}};
    }

    inline Bundle Echo(Bundle value)
    {
        return value;
    }

    inline void Adjust(Bundle& value)
    {
        value.points[0].x += 10.0f;
        ++value.points[0].hidden;
        value.matrix[1][1] += 5;
        value.markers[1] = 'W';
        value.flags[0] = true;
        value.floats[0] += 0.5f;
    }

    inline std::int32_t Hidden(const Bundle& value)
    {
        return value.points[0].hidden + value.points[1].hidden;
    }

    template <class T> inline Triplet<T> Bump(Triplet<T> value)
    {
        value.values[1] += static_cast<T>(10);
        return value;
    }

    inline wio::runtime::AsyncTask<Bundle> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return Make();
    }

    inline std::uint32_t Code(char value)
    {
        return static_cast<unsigned char>(value);
    }
    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_array
