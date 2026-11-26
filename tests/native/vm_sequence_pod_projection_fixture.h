#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace vm_sequence_pod_projection
{
    inline std::int32_t calls = 0;

    template <class T> struct Cell
    {
        T value = 7;
        std::int32_t hidden = 43;
    };

    struct Packet
    {
        std::uint8_t tag = 3;
        Cell<std::int32_t> cell;
        std::array<Cell<std::int32_t>, 2> cells{};
        std::array<std::array<std::int32_t, 2>, 2> matrix{{{{1, 2}}, {{3, 4}}}};
        std::int32_t hidden = 83;
    };

    inline bool Mutate(std::vector<Packet>& values, std::int32_t& field)
    {
        ++calls;
        if (&values.at(0).cell.value != &field)
            return false;
        field = 19;
        values[0].cell.hidden = 47;
        return values[0].cell.value == 19;
    }

    inline bool Reverse(std::int32_t& field, const std::vector<Packet>& values)
    {
        if (&values.at(0).cell.value != &field)
            return false;
        field = 23;
        return values[0].cell.value == 23;
    }

    inline bool Alias(std::int32_t& first, Packet& item, std::vector<Packet>& values, std::int32_t& second)
    {
        if (&values[0] != &item || &first != &second || &first != &item.cells[1].value)
            return false;
        first = 31;
        item.cells[1].hidden = 59;
        values[0].hidden = 89;
        return second == 31;
    }

    inline bool Matrix(std::vector<Packet>& values, std::int32_t& field, std::array<std::int32_t, 2>& row)
    {
        if (&field != &values[0].matrix[1][0] || &row != &values[0].matrix[1])
            return false;
        field = 37;
        row[1] = 41;
        return values[0].matrix[1][0] == 37;
    }

    inline bool CellAlias(const std::vector<Packet>& values, Cell<std::int32_t>& cell, std::int32_t& field)
    {
        if (&cell != &values[0].cell || &field != &cell.value)
            return false;
        field = 79;
        return values[0].cell.value == 79;
    }

    inline void RowOnly(std::array<std::int32_t, 2>&)
    {
        ++calls;
    }

    inline bool Alternate(std::vector<Packet>& values, std::int32_t& field)
    {
        if (&values[0].cells[1].value != &field || values[0].cells[1].hidden != 43)
            return false;
        field = 97;
        return true;
    }

    inline bool Nested(std::int32_t& field, std::vector<Packet>& row, std::vector<std::vector<Packet>>& values)
    {
        if (&values.at(0) != &row || &row.at(0).cell.value != &field)
            return false;
        field = 53;
        return true;
    }

    inline bool Standalone(Packet& value, std::int32_t& field)
    {
        if (&value.cell.value != &field)
            return false;
        field = 61;
        return true;
    }

    inline void Grow(std::vector<Packet>& values, std::int32_t& field)
    {
        if (&values[0].cell.value != &field)
            throw std::runtime_error("POD field address mismatch");
        field = 67;
        values.push_back(Packet{}); // Do not use field after vector invalidation.
    }

    inline void Fail(std::vector<Packet>& values, std::int32_t& field)
    {
        if (&values[0].cell.value != &field)
            throw std::runtime_error("POD field address mismatch");
        field = 71;
        throw std::runtime_error("POD projection failure");
    }

    inline std::int32_t& Borrow(std::vector<Packet>&, std::int32_t& field)
    {
        return field;
    }

    inline std::int32_t Hidden(const Packet& value)
    {
        return value.hidden + value.cell.hidden + value.cells[1].hidden;
    }

    inline std::int32_t Calls()
    {
        return calls;
    }

    inline void Print()
    {
        std::cout << "native-sequence-pod-projection-ok:30\n";
    }
} // namespace vm_sequence_pod_projection
