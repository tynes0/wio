#pragma once

#include <any.h>
#include <text.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_dictionary_projection
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
        throw std::runtime_error("dictionary projection cleanup failure");
    }
    using Scores = std::unordered_map<std::string, std::int32_t>;
    using Rows = std::unordered_map<std::string, std::vector<std::int32_t>>;

    struct Sample
    {
        std::int32_t value = 7;
        std::array<std::int32_t, 2> cells{3, 11};
        std::int32_t hidden = 43;
    };

    enum class Mode : std::uint32_t
    {
        Ready = 7
    };

    inline bool Mutate(Scores& values, std::int32_t& item)
    {
        ++calls;
        if (&values.at(std::string{"a\0b", 3}) != &item)
            return false;
        item = 19;
        values.rehash(97);
        return item == values.at(std::string{"a\0b", 3});
    }

    inline bool Reverse(std::int32_t& item, const Scores& values)
    {
        if (&values.at("beta") != &item)
            return false;
        item = 23;
        return values.at("beta") == 23;
    }

    inline bool Pair(std::int32_t& first, Scores& values, std::int32_t& second, std::int32_t& same)
    {
        if (&values.at("") != &first || &values.at("beta") != &second || &same != &second || &first == &second)
            return false;
        first = 29;
        same = 31;
        return second == 31;
    }

    inline bool Structural(Scores& values, std::int32_t& kept)
    {
        if (&kept != &values.at("beta"))
            return false;
        kept = 37;
        values.erase("");
        values.emplace("new", 41);
        values.rehash(131);
        return kept == 37;
    }

    inline bool Ordered(std::string& item, std::map<std::int32_t, std::string>& values)
    {
        if (&values.at(-3) != &item)
            return false;
        item.append("\0native", 7);
        values.emplace(5, "new");
        return true;
    }

    inline bool Unicode(std::map<wio::runtime::Text, std::int32_t>& values, std::int32_t& item)
    {
        if (&values.at(wio::runtime::Text::FromUtf8("\xc4\xb0\xf0\x9f\x9a\x80")) != &item)
            return false;
        item = 47;
        return true;
    }

    template <class K> bool Key(std::unordered_map<K, std::int32_t>& values, K key, std::int32_t& item)
    {
        if (&values.at(key) != &item)
            return false;
        item = 53;
        return true;
    }

    inline bool Nested(std::int32_t& item, std::vector<std::int32_t>& row, Rows& values)
    {
        if (&values.at("row") != &row || &row.at(1) != &item)
            return false;
        item = 59;
        row.push_back(61); // Do not use the invalidated item after growth.
        values.emplace("other", std::vector<std::int32_t>{67});
        return true;
    }

    inline bool Batches(std::vector<Scores>& values, Scores& dictionary, std::int32_t& item)
    {
        ++calls;
        if (&dictionary != &values.at(0) || &dictionary.at("beta") != &item)
            return false;
        item = 71;
        dictionary.emplace("batch", 73);
        return true;
    }

    inline bool Trees(std::unordered_map<std::string, std::map<std::int32_t, std::int32_t>>& values,
                      std::map<std::int32_t, std::int32_t>& inner, std::int32_t& item)
    {
        if (&values.at("tree") != &inner || &inner.at(7) != &item)
            return false;
        item = 79;
        return true;
    }

    inline bool Samples(std::int32_t& field, std::unordered_map<std::string, Sample>& values, Sample& item,
                        std::array<std::int32_t, 2>& cells)
    {
        if (&values.at("sample") != &item || &item.cells[1] != &field || &item.cells != &cells)
            return false;
        field = 83;
        cells[0] = 89;
        item.hidden = 97;
        return true;
    }

    inline std::int32_t Hidden(const Sample& item)
    {
        return item.hidden;
    }

    template <class T> bool Replace(std::unordered_map<std::string, T>& values, T& item, T next)
    {
        if (&values.at("item") != &item)
            return false;
        item = std::move(next);
        return true;
    }

    inline void Erase(Scores& values, std::int32_t& item)
    {
        if (&values.at("beta") != &item)
            throw std::runtime_error("dictionary alias mismatch");
        item = 101;
        values.erase("beta"); // Do not use item afterward.
    }

    inline void Fail(Scores& values, std::int32_t& item)
    {
        if (&values.at("beta") != &item)
            throw std::runtime_error("dictionary alias mismatch");
        item = 103;
        throw std::runtime_error("dictionary projected failure");
    }

    inline std::int32_t& Borrow(Scores&, std::int32_t& item)
    {
        return item;
    }

    inline Scores& BorrowRoot(Scores& values, std::int32_t& item)
    {
        item = 107;
        return values;
    }

    inline void Print()
    {
        std::cout << "native-dictionary-projection-ok:30\n";
    }
    inline std::int32_t Calls()
    {
        return calls;
    }
} // namespace vm_dictionary_projection
