#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_projected_borrow
{
    template <class T> struct Cell
    {
        T value = 7;
    };

    struct Packet
    {
        std::uint8_t tag = 3;
        Cell<std::int32_t> cell;
        std::array<std::int32_t, 2> row{11, 13};
    };

    inline std::int32_t& Scalar(std::vector<std::int32_t>& values, std::int32_t& value)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("scalar projection address mismatch");
        value += 2;
        return value;
    }

    inline const std::int32_t& View(const std::int32_t& value, const std::vector<std::int32_t>& values)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("view projection address mismatch");
        return value;
    }

    inline std::int32_t& Repeat(const std::vector<std::int32_t>& values, const std::int32_t& first,
                                std::int32_t& second)
    {
        if (&first != &second || &first != &values.at(0))
            throw std::runtime_error("repeated projection address mismatch");
        second = 19;
        return second;
    }

    inline std::int32_t& Field(const std::vector<Packet>& values, std::int32_t& value)
    {
        if (&values.at(0).cell.value != &value)
            throw std::runtime_error("field projection address mismatch");
        value = 23;
        return value;
    }

    inline std::int32_t& Pod(Packet& packet, std::int32_t& value)
    {
        if (&packet.cell.value != &value)
            throw std::runtime_error("POD projection address mismatch");
        value = 29;
        return value;
    }

    inline Packet& Item(std::vector<Packet>& values, Packet& item)
    {
        if (&values.at(0) != &item)
            throw std::runtime_error("item projection address mismatch");
        return item;
    }

    inline std::array<std::int32_t, 2>& Row(std::vector<Packet>& values, std::array<std::int32_t, 2>& row)
    {
        if (&values.at(0).row != &row)
            throw std::runtime_error("fixed row projection address mismatch");
        return row;
    }

    inline std::vector<std::int32_t>& Nested(std::vector<std::vector<std::int32_t>>& values,
                                             std::vector<std::int32_t>& row)
    {
        if (&values.at(0) != &row)
            throw std::runtime_error("nested sequence projection address mismatch");
        return row;
    }

    inline std::int32_t& Map(std::unordered_map<std::string, std::int32_t>& values, std::int32_t& value)
    {
        if (&values.at("kept") != &value)
            throw std::runtime_error("dictionary projection address mismatch");
        value = 31;
        values.erase("removed");
        for (std::int32_t i = 0; i < 80; ++i)
            values.emplace("new" + std::to_string(i), i);
        values.rehash(512); // Mapped-value references remain valid.
        return value;
    }

    inline std::int32_t& Mixed(std::unordered_map<std::string, std::vector<Packet>>& values, std::int32_t& value)
    {
        if (&values.at("kept").at(0).cell.value != &value)
            throw std::runtime_error("mixed projection address mismatch");
        value = 37;
        values.emplace("extra", std::vector<Packet>{Packet{}});
        return value;
    }

    inline std::string& String(std::unordered_map<std::string, std::string>& values, std::string& value)
    {
        if (&values.at("kept") != &value)
            throw std::runtime_error("string projection address mismatch");
        value = "native";
        return value;
    }

    inline std::vector<std::int32_t>& Root(std::vector<std::int32_t>& values)
    {
        return values;
    }

    inline std::int32_t& Shrink(std::vector<std::int32_t>& values, std::int32_t& first)
    {
        values.pop_back(); // first is valid in C++, but the VM retires structural array projections.
        return first;
    }

    inline std::int32_t& Grow(std::vector<std::int32_t>& values, std::int32_t&)
    {
        values.reserve(values.capacity() + 64); // Do not use the old reference after invalidation.
        values.push_back(41);
        return values.at(0); // Fresh, valid C++ reference, not the supplied reference.
    }

    inline std::int32_t& Erase(std::unordered_map<std::string, std::int32_t>& values, std::int32_t&)
    {
        values.erase("kept");
        return values.at("other"); // No dangling-reference formation/dereference.
    }

    inline std::int32_t& Foreign(std::vector<std::int32_t>&, std::int32_t&)
    {
        static std::int32_t global = 43;
        return global;
    }

    inline std::int32_t& Fail(std::vector<std::int32_t>&, std::int32_t& value)
    {
        value = 47;
        throw std::runtime_error("projected borrow failure");
    }

    inline void Print()
    {
        std::cout << "native-projected-borrow-ok:30\n";
    }
} // namespace vm_projected_borrow
