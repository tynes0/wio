#pragma once

#include <any.h>
#include <text.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vm_sequence_projection
{
    inline std::int32_t calls = 0;
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
    inline bool Balanced()
    {
        return constructed == destroyed;
    }
    inline void CleanupFailure()
    {
        throw std::runtime_error("projection-cleanup-failure");
    }
    inline std::int32_t Calls()
    {
        return calls;
    }

    struct Sample
    {
        std::int32_t value = 7;
        std::int32_t hidden = 43;
    };

    inline bool Mutate(std::vector<std::int32_t>& values, std::int32_t& item)
    {
        ++calls;
        if (&values.at(0) != &item)
            return false;
        item = 41;
        values.at(1) = 19;
        return values.front() == item;
    }
    inline bool Reverse(std::int32_t& item, const std::vector<std::int32_t>& values)
    {
        if (&values.at(1) != &item)
            return false;
        item = 67;
        return values.at(1) == 67;
    }
    inline bool ReadOnly(const std::int32_t& item, const std::vector<std::int32_t>& values)
    {
        return &values.at(0) == &item;
    }
    inline bool Pair(std::vector<std::int32_t>& values, std::int32_t& first, std::int32_t& second)
    {
        if (&values.at(0) != &first || &values.at(1) != &second || &first == &second)
            return false;
        first = 23;
        second = 29;
        return true;
    }
    inline bool Alias(std::int32_t& first, std::vector<std::int32_t>& values, std::int32_t& second)
    {
        if (&first != &second || &first != &values.front())
            return false;
        first = 31;
        return second == 31;
    }
    inline bool Nested(std::int32_t& cell, std::vector<std::int32_t>& row,
                       std::vector<std::vector<std::int32_t>>& values)
    {
        if (&row != &values.at(0) || &cell != &row.at(1))
            return false;
        cell = 37;
        row.push_back(99); // No use of cell after possible reallocation.
        return values.at(0).at(1) == 37 && values.at(0).size() == 3;
    }
    inline bool Words(std::string& word, std::vector<std::string>& words)
    {
        if (&word != &words.front())
            return false;
        word.append("\0tail", 5);
        return words.front() == word;
    }
    inline bool Samples(std::vector<Sample>& values, Sample& item)
    {
        if (&values.front() != &item || item.hidden != 43)
            return false;
        item.value = 41;
        item.hidden = 83;
        return true;
    }
    inline std::int32_t Hidden(const Sample& item)
    {
        return item.hidden;
    }
    inline bool Anys(std::vector<wio::runtime::Any>& values, wio::runtime::Any& item)
    {
        if (&values.front() != &item)
            return false;
        item = wio::runtime::Any::Box(std::int32_t{19});
        return values.front().AsBoxed<std::int32_t>() == 19;
    }
    inline bool Texts(std::vector<wio::runtime::Text>& values, wio::runtime::Text& item)
    {
        if (&values.front() != &item)
            return false;
        item = wio::runtime::Text::FromUtf8("\xc4\xb0\xf0\x9f\x9a\x80");
        return true;
    }
    template <class T> bool Replace(std::vector<T>& values, T& item, T next)
    {
        if (&values.front() != &item)
            return false;
        item = std::move(next);
        return true;
    }
    template <class T> T& Borrow(std::vector<T>&, T& item)
    {
        return item;
    }
    inline std::vector<std::int32_t>& BorrowRoot(std::vector<std::int32_t>& values, std::int32_t& item)
    {
        item = 53;
        return values;
    }
    inline void Fail(std::vector<std::int32_t>& values, std::int32_t& item)
    {
        if (&values.front() != &item)
            throw std::runtime_error("projection-address-mismatch");
        item = 43;
        throw std::runtime_error("sequence-projection-failure");
    }
    inline void PodField(std::vector<Sample>&, std::int32_t&)
    {
    }
    inline void Print()
    {
        std::cout << "native-sequence-projection-ok:30\n";
    }
} // namespace vm_sequence_projection
