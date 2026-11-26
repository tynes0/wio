#pragma once

#include <any.h>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace vm_any_generic
{
    inline void* Token()
    {
        static int token;
        return &token;
    }

    template <class T, std::size_t N = 3> struct Buffer
    {
        std::array<T, N> values{};
        void* token = Token();
        std::int32_t hidden = 17;
    };

    template <class T> struct Buffer<T, 4>
    {
        std::array<T, 4> values{};
        std::int32_t bonus = 23;
        void* token = Token();
        std::int32_t hidden = 27;
    };

    template <class... Types> struct Marker
    {
        std::int32_t tag = 7;
        void* token = Token();
        std::int32_t hidden = sizeof...(Types);
    };

    template <class T, std::size_t N> struct Nested
    {
        Buffer<T, N> value;
        std::array<Buffer<T, N>, 2> copies;
    };

    using Record = Buffer<std::int32_t>;
    using Records = std::vector<Record>;
    using Map = std::unordered_map<std::string, Record>;

    inline wio::runtime::Any Echo(wio::runtime::Any value)
    {
        return value;
    }
    inline bool Same(const wio::runtime::Any& first, wio::runtime::Any& second)
    {
        return &first == &second;
    }
    inline void Mutate(wio::runtime::Any value)
    {
        auto& record = value.AsBoxed<Record>();
        record.values[0] += 10;
        ++record.hidden;
    }
    template <class T> std::int32_t Hidden(const wio::runtime::Any& value)
    {
        return value.AsBoxed<T>().hidden;
    }
    inline Marker<> Empty()
    {
        return Marker<>{9};
    }
    inline std::int32_t HiddenEmpty(const wio::runtime::Any& value)
    {
        return value.AsBoxed<Marker<>>().hidden;
    }
    inline void Replace(wio::runtime::Any& value)
    {
        Buffer<double, 2> record;
        record.values = {1.5, 2.5};
        record.hidden = 73;
        value = wio::runtime::Any::Box(record);
    }
    inline wio::runtime::Any Make()
    {
        Record record;
        record.values = {31, 32, 33};
        record.hidden = 91;
        return wio::runtime::Any::Box(record);
    }
    inline Records EchoArray(Records value)
    {
        value.at(0).values[0] += 100;
        value.at(0).hidden += 100;
        return value;
    }
    inline bool SameArray(const Records& first, Records& second)
    {
        return &first == &second;
    }
    inline void UpdateArray(Records& value)
    {
        value.at(0).values[0] += 10;
        ++value.at(0).hidden;
    }
    inline std::int32_t ArrayHidden(const Records& value)
    {
        return value.at(0).hidden;
    }
    inline void GrowAny(wio::runtime::Any value)
    {
        Record next;
        next.values = {41, 42, 43};
        next.hidden = 83;
        value.AsBoxed<Records>().push_back(next);
    }
    inline Map EchoMap(Map value)
    {
        value.at("local").values[0] += 100;
        value.at("local").hidden += 100;
        return value;
    }
    inline bool SameMap(const Map& first, Map& second)
    {
        return &first == &second;
    }
    inline void UpdateMap(Map& value)
    {
        value.at("local").values[0] += 10;
        ++value.at("local").hidden;
    }
    inline std::int32_t MapHidden(const Map& value)
    {
        return value.at("local").hidden;
    }
    inline void GrowMapAny(wio::runtime::Any value)
    {
        Record next;
        next.values = {51, 52, 53};
        next.hidden = 93;
        value.AsBoxed<Map>().emplace("native", next);
    }
    inline void Print()
    {
        std::cout << "native-any-generic-ok:30\n";
    }
} // namespace vm_any_generic
