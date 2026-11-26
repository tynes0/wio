#pragma once

#include <text.h>

#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace vm_sequence
{
    struct Sample
    {
        std::int32_t value = 7;
        std::int32_t hidden = 41;
    };

    inline std::int32_t Sum(const std::vector<std::int32_t>& values)
    {
        return std::accumulate(values.begin(), values.end(), std::int32_t{0});
    }

    inline std::vector<std::int32_t> Transform(std::vector<std::int32_t> values)
    {
        for (auto& value : values)
            value *= 2;
        values.push_back(19);
        return values;
    }

    inline void Mutate(std::vector<std::int32_t>& values)
    {
        for (auto& value : values)
            value += 3;
    }

    inline bool Same(const std::vector<std::int32_t>& first, std::vector<std::int32_t>& second)
    {
        if (&first != &second)
            return false;
        second.at(1) = 29;
        return first.at(1) == 29;
    }

    inline std::vector<std::string> Words(std::vector<std::string> values)
    {
        values.at(0) += std::string{"\0native", 7};
        values.push_back("result");
        return values;
    }

    inline std::vector<wio::runtime::Text> Texts(std::vector<wio::runtime::Text> values)
    {
        values.push_back(wio::runtime::Text::FromUtf8("\xf0\x9f\x9a\x80"));
        return values;
    }

    inline std::vector<bool> Bits(std::vector<bool> values)
    {
        for (std::size_t index = 0; index < values.size(); ++index)
            values[index] = !values[index];
        return values;
    }

    inline std::int32_t pointerToken = 67;
    inline void* Token()
    {
        return &pointerToken;
    }

    inline std::vector<void*> Pointers(std::vector<void*> values)
    {
        if (values.size() != 2 || values[0] || values[1] != &pointerToken)
            throw std::runtime_error("nullable pointer sequence oracle");
        return {values[1], nullptr};
    }

    inline std::vector<std::vector<std::int32_t>> Nested(std::vector<std::vector<std::int32_t>> values)
    {
        values.at(0).at(1) += 17;
        values.push_back({23, 31});
        return values;
    }

    inline std::vector<Sample> Samples(std::vector<Sample> values)
    {
        if (values.size() != 1 || values.front().hidden != 41)
            throw std::runtime_error("producer POD defaults were lost");
        values.front().value += values.front().hidden;
        values.front().hidden = 83;
        return values;
    }

    inline std::int32_t Hidden(const std::vector<Sample>& values)
    {
        return values.at(0).hidden;
    }

    inline void Resize(std::vector<std::int32_t>& values)
    {
        values.push_back(99);
    }

    inline void Fail(std::vector<std::int32_t>& values)
    {
        values.at(0) = 101;
        throw std::runtime_error("sequence failure oracle");
    }

    inline void Overlap(std::vector<std::int32_t>& values, std::int32_t& item)
    {
        if (&values.at(0) != &item)
            throw std::runtime_error("sequence projection oracle");
    }

    inline void Print(const std::string& value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_sequence
