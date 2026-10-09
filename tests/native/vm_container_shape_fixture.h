#pragma once

#include <any.h>
#include <text.h>

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_shape
{
    using Scores = std::unordered_map<std::string, std::int32_t>;
    using Rows = std::unordered_map<std::string, std::vector<std::int32_t>>;
    struct Sample
    {
        std::int32_t value = 7;
        std::int32_t hidden = 43;
    };
    inline std::int32_t constructed = 0;
    inline std::int32_t destroyed = 0;
    inline void Constructed()
    {
        ++constructed;
    }
    inline void Destroyed()
    {
        ++destroyed;
    }
    inline void CleanupFailure()
    {
        throw std::runtime_error("shape-cleanup-oracle");
    }
    inline std::int32_t Constructions()
    {
        return constructed;
    }
    inline std::int32_t Destructions()
    {
        return destroyed;
    }

    template <class T> void Push(std::vector<T>& values, T value)
    {
        values.push_back(std::move(value));
    }
    template <class T> void Clear(std::vector<T>& values)
    {
        values.clear();
    }
    template <class T> void EraseFirst(std::vector<T>& values)
    {
        values.erase(values.begin());
    }

    inline void Grow(std::vector<std::int32_t>& values)
    {
        values.push_back(99);
    }
    inline void SameSize(std::vector<std::int32_t>& values)
    {
        values.at(0) = 41;
    }
    inline bool Aliased(const std::vector<std::int32_t>& first, std::vector<std::int32_t>& second)
    {
        if (&first != &second)
            return false;
        second.push_back(99);
        return first.back() == 99;
    }
    inline void Shrink(std::vector<std::int32_t>& values)
    {
        values.resize(1);
    }
    inline void Words(std::vector<std::string>& values)
    {
        values.at(0) += std::string{"\0tail", 5};
        values.push_back(std::string{"n\0w", 3});
    }
    inline void Texts(std::vector<wio::runtime::Text>& values)
    {
        values.push_back(wio::runtime::Text::FromUtf8("\xf0\x9f\x9a\x80"));
    }
    inline void Bits(std::vector<bool>& values)
    {
        values.push_back(true);
    }
    inline void Samples(std::vector<Sample>& values)
    {
        if (values.at(0).hidden != 43)
            throw std::runtime_error("POD prototype lost");
        values.push_back(Sample{19, 83});
    }
    inline std::int32_t Hidden(const Sample& value)
    {
        return value.hidden;
    }
    inline void AnyValues(std::vector<wio::runtime::Any>& values)
    {
        values.at(0).AsBoxed<std::int32_t>() = 41;
        values.push_back(wio::runtime::Any::Box(wio::runtime::Any::Box(std::int32_t{19})));
    }
    inline void Insert(Scores& values)
    {
        values.at("keep") = 41;
        values.emplace("new", 19);
        values.rehash(97);
    }
    inline void Remove(Scores& values)
    {
        values.erase("new");
    }
    inline void Rename(Scores& values)
    {
        values.erase("gone");
        values.emplace("replacement", 23);
    }
    inline void Ordered(std::map<std::int32_t, std::string>& values)
    {
        values.at(3) = "changed";
        values.emplace(1, "first");
        values.emplace(7, "last");
    }
    inline void ClearMap(Scores& values)
    {
        values.clear();
    }
    inline void GrowRow(Rows& values)
    {
        values.at("row").push_back(99);
    }
    inline void ChangeRows(Rows& values)
    {
        values.at("row").at(0) = 41;
        values.erase("gone");
        values.emplace("new", std::vector<std::int32_t>{23});
    }
    inline void Nested(std::vector<std::vector<std::int32_t>>& values)
    {
        values.at(0).push_back(99);
        values.at(1).at(0) = 41;
    }
    inline void Pair(std::vector<std::int32_t>& first, Scores& second)
    {
        first.push_back(99);
        second.emplace("new", 19);
    }
    inline void Overlap(std::vector<std::int32_t>& values, std::int32_t& item)
    {
        if (&values.at(0) != &item)
            throw std::runtime_error("overlap not mapped");
    }
    inline void Fail(std::vector<std::int32_t>& values)
    {
        values.push_back(99);
        throw std::runtime_error("shape-failure-oracle");
    }
    inline void Print()
    {
        std::cout << "native-container-shape-ok:30\n";
    }
} // namespace vm_shape
