#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <string>

namespace vm_pod_alias
{
    inline std::int32_t token = 47;
    struct Inner
    {
        std::int32_t value = 3;
        std::int32_t hidden = 71;
    };

    struct Packet
    {
        double weight = 2.5;
        std::int32_t count = 7;
        bool enabled = true;
        void* pointer = nullptr;
        Inner inner;
        std::array<Inner, 2> items;
        std::array<std::array<std::int32_t, 2>, 2> matrix = {{{1, 2}, {3, 4}}};
        std::int32_t hidden = 99;
    };

    inline bool Whole(Packet& packet, std::int32_t& count)
    {
        if (&packet.count != &count)
            return false;
        packet.count = 10;
        count += 5;
        ++packet.hidden;
        return packet.count == 15;
    }

    inline bool Reversed(std::int32_t& count, Packet& packet)
    {
        return Whole(packet, count);
    }

    inline bool ReadOnly(const Packet& packet, std::int32_t& count)
    {
        if (&packet.count != &count)
            return false;
        count = 23;
        return packet.count == 23;
    }

    inline bool ReadField(const std::int32_t& count, Packet& packet)
    {
        packet.count = 31;
        return &packet.count == &count && count == 31;
    }

    inline bool Nested(std::int32_t& value, Inner& inner, Packet& packet)
    {
        if (&packet.inner != &inner || &inner.value != &value)
            return false;
        value = 41;
        inner.value += 1;
        packet.inner.hidden += 1;
        return packet.inner.value == 42;
    }

    inline bool Array(std::int32_t& value, Packet& packet, Inner& item)
    {
        if (&packet.items[1] != &item || &item.value != &value)
            return false;
        packet.items[1].value = 50;
        value += 2;
        ++item.hidden;
        return item.value == 52;
    }

    inline bool Matrix(Packet& packet, std::int32_t& value)
    {
        if (&packet.matrix[1][0] != &value)
            return false;
        value = 61;
        return packet.matrix[1][0] == 61;
    }

    inline bool Same(std::int32_t& first, std::int32_t& second)
    {
        first = 70;
        second += 3;
        return &first == &second && first == 73;
    }

    inline bool Scalars(bool& enabled, void*& pointer, Packet& packet, double& weight)
    {
        if (&packet.enabled != &enabled || &packet.pointer != &pointer || &packet.weight != &weight)
            return false;
        enabled = false;
        pointer = &token;
        weight = 9.5;
        return !packet.enabled && packet.pointer == &token && packet.weight == 9.5;
    }

    inline bool Independent(Packet& packet, std::int32_t& other)
    {
        if (&packet.count == &other)
            return false;
        packet.count = 81;
        other = 82;
        return packet.count == 81;
    }

    inline std::int32_t Hidden(const Packet& packet)
    {
        return packet.hidden + packet.inner.hidden + packet.items[1].hidden;
    }

    inline void Print(std::string value)
    {
        std::cout << value << '\n';
    }
} // namespace vm_pod_alias
