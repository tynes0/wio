#pragma once

#include <any.h>
#include <text.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_any
{
    struct Sample
    {
        std::int32_t value = 7;
        const char* hidden = "producer-any";
    };

    inline std::int32_t Read(const wio::runtime::Any& value)
    {
        return value.AsBoxed<std::int32_t>();
    }
    inline wio::runtime::Any Echo(wio::runtime::Any value)
    {
        return value;
    }
    inline void MutateCopy(wio::runtime::Any value)
    {
        value.AsBoxed<std::int32_t>() += 10;
    }
    inline void Replace(wio::runtime::Any& value)
    {
        value = std::string("new\0value", 9);
    }
    inline bool Same(const wio::runtime::Any& first, wio::runtime::Any& second)
    {
        return &first == &second;
    }
    inline wio::runtime::Any MakeText()
    {
        return wio::runtime::Any::Box(wio::runtime::Text::FromUtf8("\xc4\xb0stanbul \xf0\x9f\x9a\x80"));
    }
    inline bool ReadText(const wio::runtime::Any& value)
    {
        return value.AsBoxed<wio::runtime::Text>() == wio::runtime::Text::FromUtf8("\xc4\xb0stanbul \xf0\x9f\x9a\x80");
    }
    inline void GrowCopy(wio::runtime::Any value)
    {
        value.AsBoxed<std::vector<std::int32_t>>().push_back(19);
    }
    inline void SetCopy(wio::runtime::Any value)
    {
        value.AsBoxed<std::unordered_map<std::string, std::int32_t>>()["native"] = 23;
    }
    inline wio::runtime::Any MakeSample()
    {
        return wio::runtime::Any::Box(Sample{});
    }
    inline bool ReadSample(const wio::runtime::Any& value)
    {
        return value.AsBoxed<Sample>().value == 7 && std::string(value.AsBoxed<Sample>().hidden) == "producer-any";
    }
    inline void* Token()
    {
        static int token;
        return &token;
    }
    inline bool ReadOpaque(const wio::runtime::Any& value)
    {
        return value.IsOpaque() && value.AsOpaque() == Token();
    }
    inline bool IsNull(const wio::runtime::Any& value)
    {
        return value.IsNull();
    }
    inline wio::runtime::Any Null()
    {
        return {};
    }
    inline wio::runtime::Any Unsupported()
    {
        return wio::runtime::Any::Box(std::vector<double>{1.5});
    }
    inline std::int32_t ReadUnsupported(const wio::runtime::Any& value)
    {
        return static_cast<std::int32_t>(value.AsBoxed<std::vector<double>>().size());
    }
    inline void Print(const std::string& value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_any
