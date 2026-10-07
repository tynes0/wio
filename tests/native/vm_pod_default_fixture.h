#pragma once

#include <std_async.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_default
{
    inline std::int32_t constructions = 0;
    inline std::int32_t token = 47;

    inline std::int32_t Next()
    {
        ++constructions;
        return 17;
    }

    struct Settings
    {
        std::int32_t count = Next();
        bool enabled = true;
        double scale = 2.5;
        void* pointer = &token;
        std::array<std::int32_t, 3> values = {3, 5, 7};
        std::int32_t hidden = 61;
    };

    template <class T, std::size_t N> struct Buffer
    {
        std::array<T, N> values = []
        {
            std::array<T, N> result{};
            for (std::size_t index = 0; index < N; ++index)
                result[index] = static_cast<T>(index + 11);
            return result;
        }();
        std::int32_t hidden = 73;
    };

    struct Nested
    {
        Settings settings;
        std::array<Settings, 2> copies;
        Buffer<std::int32_t, 0> empty;
    };

    struct Empty
    {
        std::int32_t hidden = Next() + 31;
    };

    inline void Reset()
    {
        constructions = 0;
    }

    inline std::int32_t Count()
    {
        return constructions;
    }

    inline Settings Echo(Settings value)
    {
        return value;
    }

    inline void Bump(Settings& value)
    {
        ++value.count;
        ++value.hidden;
    }

    inline std::int32_t Hidden(const Settings& value)
    {
        return value.hidden;
    }

    inline std::int32_t HiddenEmpty(const Empty& value)
    {
        return value.hidden;
    }

    inline std::int32_t Read(void* value)
    {
        return value ? *static_cast<std::int32_t*>(value) : -1;
    }

    template <class T, std::size_t N> inline Buffer<T, N> EchoBuffer(Buffer<T, N> value)
    {
        return value;
    }

    inline wio::runtime::AsyncTask<Settings> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return Settings{};
    }

    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_default
