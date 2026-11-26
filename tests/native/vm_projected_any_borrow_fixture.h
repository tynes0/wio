#pragma once

#include <any.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vm_projected_any_borrow
{
    using Any = wio::runtime::Any;
    using Map = std::unordered_map<std::string, Any>;
    using Rows = std::unordered_map<std::string, std::vector<Any>>;

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
        throw std::runtime_error("projected Any destructor failure");
    }

    struct Sample
    {
        std::int32_t value = 37;
        std::int32_t hidden = 43;
    };

    inline Any& Array(std::vector<Any>& values, Any& value, Any next)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("Any array projection address mismatch");
        value = std::move(next);
        return value;
    }

    inline const Any& View(const Any& value, const std::vector<Any>& values)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("Any readonly projection address mismatch");
        return value;
    }

    inline Any& Repeat(const std::vector<Any>& values, const Any& first, Any& second)
    {
        if (&values.at(0) != &first || &first != &second)
            throw std::runtime_error("Any repeated projection address mismatch");
        second = Any{std::int32_t{19}};
        return second;
    }

    inline Any& Distinct(const std::vector<Any>& values, const Any& first, Any& second)
    {
        if (&values.at(0) != &first || &values.at(1) != &second || &first == &second || first.Cell() != second.Cell())
            throw std::runtime_error("shared Any payload collapsed distinct caller cells");
        second = Any{std::int32_t{71}};
        return second;
    }

    inline Any& Dictionary(Map& values, Any& value, Any next)
    {
        if (&values.at("kept") != &value)
            throw std::runtime_error("Any map projection address mismatch");
        value = std::move(next);
        values.erase("removed");
        for (std::int32_t i = 0; i < 40; ++i)
            values.emplace("new" + std::to_string(i), Any{i});
        values.rehash(128);
        return value;
    }

    inline Any& Mixed(Rows& values, Any& value, Any next)
    {
        if (&values.at("kept").at(0) != &value)
            throw std::runtime_error("Any mixed projection address mismatch");
        value = std::move(next);
        values.emplace("extra", std::vector<Any>{Any{std::int32_t{41}}});
        return value;
    }

    inline void Payload(Any& value, std::int32_t next)
    {
        value.AsBoxed<std::int32_t>() = next;
    }
    inline Any Nest(Any value)
    {
        return Any::Box(std::move(value));
    }
    inline Any Null()
    {
        return {};
    }

    inline Any& Shrink(std::vector<Any>& values, Any& value)
    {
        values.pop_back(); // Valid C++ reference, conservatively expired VM projection.
        return value;
    }

    inline Any& Grow(std::vector<Any>& values, Any&)
    {
        values.reserve(values.capacity() + 64);
        values.emplace_back(std::int32_t{47});
        return values.at(0); // Fresh selection is not the supplied caller reference.
    }

    inline Any& Erase(Map& values, Any&)
    {
        values.erase("kept");
        return values.at("other");
    }

    inline Any& Foreign(std::vector<Any>&, Any&)
    {
        static Any value{std::int32_t{53}};
        return value;
    }

    inline Any& Fail(std::vector<Any>&, Any& value)
    {
        value = Any{std::int32_t{59}};
        throw std::runtime_error("projected Any native failure");
    }

    inline void Print()
    {
        std::cout << "native-projected-any-borrow-ok:30\n";
    }
} // namespace vm_projected_any_borrow
