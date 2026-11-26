#pragma once

#include <std_async.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_template
{
    template <class T, std::size_t N = 3> struct Buffer
    {
        std::array<T, N> values;
        std::int32_t hidden = 17;
    };

    template <class T> struct Buffer<T, 4>
    {
        std::array<T, 4> values;
        std::int32_t bonus;
        std::int32_t hidden = 27;
    };

    template <class T, std::int32_t Tag> struct Tagged
    {
        T value;
        std::int32_t hidden = Tag;
    };

    template <> struct Tagged<std::int32_t, 7>
    {
        std::int32_t value;
        std::int32_t bonus;
        std::int32_t hidden = 7;
    };

    template <class T, class... Tail> struct Head
    {
        T value;
        std::int32_t hidden = sizeof...(Tail);
    };

    template <class... Types> struct Marker
    {
        std::int32_t tag;
        std::int32_t hidden = sizeof...(Types);
    };

    template <class T, std::size_t N> struct Nested
    {
        Buffer<T, N> buffer;
        std::array<Buffer<T, N>, 2> copies;
    };

    template <class T, std::size_t N> inline Buffer<T, N> Echo(Buffer<T, N> value)
    {
        return value;
    }

    template <class T, std::size_t N> inline void Bump(Buffer<T, N>& value)
    {
        if constexpr (N != 0)
            value.values[0] += T{10};
        ++value.hidden;
    }

    template <class T, std::size_t N> inline std::int32_t Hidden(const Buffer<T, N>& value)
    {
        return value.hidden;
    }

    template <class T, std::int32_t Tag> inline Tagged<T, Tag> EchoTagged(Tagged<T, Tag> value)
    {
        return value;
    }

    template <class T, std::int32_t Tag> inline std::int32_t TagValue(const Tagged<T, Tag>& value)
    {
        return value.hidden;
    }

    template <class T, class... Tail> inline Head<T, Tail...> EchoHead(Head<T, Tail...> value)
    {
        return value;
    }

    template <class T, class... Tail> inline std::int32_t TailCount(const Head<T, Tail...>& value)
    {
        return value.hidden;
    }

    template <class... Types> inline Marker<Types...> EchoMarker(Marker<Types...> value)
    {
        return value;
    }

    template <class... Types> inline std::int32_t MarkerCount(const Marker<Types...>& value)
    {
        return value.hidden;
    }

    inline Nested<std::int32_t, 3> EchoNested(Nested<std::int32_t, 3> value)
    {
        return value;
    }

    inline wio::runtime::AsyncTask<Buffer<std::int32_t, 3>> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return Buffer<std::int32_t, 3>{{1, 2, 3}, 73};
    }

    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_template
