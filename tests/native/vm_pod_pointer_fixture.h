#pragma once

#include <std_async.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_pointer
{
    inline std::int32_t tokens[]{17, 29};

    struct Links
    {
        void* required;
        void* optional;
        std::array<void*, 2> pointers;
        std::int32_t hidden = 41;
    };

    template <class T> struct Holder
    {
        T value;
    };

    struct Nested
    {
        Links links;
        std::array<Links, 2> copies;
    };

    inline void* Token(std::int32_t index)
    {
        return &tokens[index];
    }

    inline std::int32_t Read(void* pointer)
    {
        return pointer ? *static_cast<std::int32_t*>(pointer) : -1;
    }

    inline Links Make()
    {
        return {Token(0), nullptr, {nullptr, Token(1)}, 73};
    }

    inline Links Echo(Links value)
    {
        return value;
    }

    inline void Replace(Links& value)
    {
        value.required = Token(1);
        value.optional = Token(0);
        value.pointers = {Token(1), nullptr};
        ++value.hidden;
    }

    inline void Clear(Links& value)
    {
        value.optional = nullptr;
    }

    inline std::int32_t Hidden(const Links& value)
    {
        return value.hidden;
    }

    template <class T> inline Holder<T> EchoHolder(Holder<T> value)
    {
        return value;
    }

    inline Nested EchoNested(Nested value)
    {
        return value;
    }

    inline wio::runtime::AsyncTask<Links> Delayed()
    {
        co_await wio::runtime::AsyncSleep(1);
        co_return Make();
    }

    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_pointer
