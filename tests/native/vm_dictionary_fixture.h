#pragma once

#include <text.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_dictionary
{
    using Scores = std::unordered_map<std::string, std::int32_t>;
    using Rows = std::unordered_map<std::string, std::vector<std::int32_t>>;

    struct Sample
    {
        std::int32_t value = 7;
        std::int32_t hidden = 41;
    };

    inline std::int32_t Sum(const Scores& values)
    {
        std::int32_t sum = 0;
        for (const auto& [key, value] : values)
            sum += value;
        return sum;
    }

    inline Scores Transform(Scores values)
    {
        values.at(std::string{"a\0b", 3}) += 17;
        values.emplace("new", 23);
        return values;
    }

    inline void Mutate(Scores& values)
    {
        for (auto& [key, value] : values)
            value += 3;
        values.rehash(97); // Reordering must not change VM value-cell identity.
    }

    inline bool Same(const Scores& first, Scores& second)
    {
        if (&first != &second)
            return false;
        second.at("beta") = 29;
        return first.at("beta") == 29;
    }

    inline std::map<std::int32_t, std::string> Ordered(std::map<std::int32_t, std::string> values)
    {
        values.at(1) += std::string{"\0native", 7};
        values.emplace(2, "second");
        return values;
    }

    inline std::map<wio::runtime::Text, std::string> Labels(std::map<wio::runtime::Text, std::string> values)
    {
        values.emplace(wio::runtime::Text::FromUtf8("\xf0\x9f\x9a\x80"), "rocket");
        return values;
    }

    inline Rows TransformRows(Rows values)
    {
        values.at("row").push_back(19);
        values.emplace("empty", std::vector<std::int32_t>{});
        return values;
    }

    inline void MutateRows(Rows& values)
    {
        values.at("row").at(1) = 61;
    }

    inline std::vector<Scores> Batches(std::vector<Scores> values)
    {
        values.at(0).emplace("batch", 71);
        values.push_back({{"last", 73}});
        return values;
    }

    inline std::unordered_map<std::string, std::map<std::int32_t, std::string>>
    Nested(std::unordered_map<std::string, std::map<std::int32_t, std::string>> values)
    {
        values.at("tree").at(3) = "nested";
        return values;
    }

    inline std::unordered_map<std::string, Sample> Samples(std::unordered_map<std::string, Sample> values)
    {
        auto& sample = values.at("sample");
        if (sample.hidden != 41)
            throw std::runtime_error("dictionary POD backing oracle");
        sample.value += sample.hidden;
        sample.hidden = 83;
        return values;
    }

    inline std::int32_t Hidden(const std::unordered_map<std::string, Sample>& values)
    {
        return values.at("sample").hidden;
    }

    inline std::int32_t pointerToken = 67;
    inline void* Token()
    {
        return &pointerToken;
    }

    inline std::unordered_map<std::string, void*> Pointers(std::unordered_map<std::string, void*> values)
    {
        if (values.at("null") || values.at("token") != &pointerToken)
            throw std::runtime_error("nullable dictionary pointer oracle");
        values.emplace("copy", values.at("token"));
        return values;
    }

    inline Scores Empty()
    {
        return {};
    }

    inline void Resize(Scores& values)
    {
        values.emplace("new", 99);
    }

    inline void Rename(Scores& values)
    {
        values.erase("first");
        values.emplace("other", 99);
    }

    inline void GrowRow(Rows& values)
    {
        values.at("row").push_back(99);
    }

    inline void Overlap(Scores& values, std::int32_t& value)
    {
        if (&values.at("first") != &value)
            throw std::runtime_error("dictionary projection oracle");
    }

    inline void Fail(Scores& values)
    {
        values.at("first") = 101;
        throw std::runtime_error("dictionary failure oracle");
    }

    inline void Print(const std::string& value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_dictionary
