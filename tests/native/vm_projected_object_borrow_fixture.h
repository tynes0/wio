#pragma once

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vm_projected_object_borrow
{
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
        throw std::runtime_error("projected object destructor failure");
    }

    template <class T> T& Array(std::vector<T>& values, T& value, T next)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("object array projection address mismatch");
        value = std::move(next);
        return value;
    }

    template <class T> const T& View(const T& value, const std::vector<T>& values)
    {
        if (&values.at(0) != &value)
            throw std::runtime_error("readonly object projection address mismatch");
        return value;
    }

    template <class T> T& Repeat(const std::vector<T>& values, const T& first, T& second)
    {
        if (&values.at(0) != &first || &first != &second)
            throw std::runtime_error("repeated object projection address mismatch");
        return second;
    }

    template <class T> T& Distinct(const std::vector<T>& values, const T& first, T& second, T next)
    {
        if (&values.at(0) != &first || &values.at(1) != &second || &first == &second || first.Get() != second.Get())
            throw std::runtime_error("same object identity collapsed distinct handle cells");
        second = std::move(next);
        return second;
    }

    template <class T> T& Dictionary(std::unordered_map<std::string, T>& values, T& value, T next)
    {
        if (&values.at("kept") != &value)
            throw std::runtime_error("object map projection address mismatch");
        value = std::move(next);
        values.erase("removed");
        for (std::int32_t i = 0; i < 20; ++i)
            values.emplace("new" + std::to_string(i), value);
        values.rehash(128);
        return value;
    }

    template <class T> T& Mixed(std::unordered_map<std::string, std::vector<T>>& values, T& value, T next)
    {
        if (&values.at("kept").at(0) != &value)
            throw std::runtime_error("mixed object projection address mismatch");
        value = std::move(next);
        values.emplace("extra", std::vector<T>{value});
        return value;
    }

    template <class T> void Touch(T& value)
    {
        value->_WF_Set_i32(23);
    }

    template <class T> T& Shrink(std::vector<T>& values, T& value)
    {
        values.pop_back();
        return value;
    }

    template <class T> T& Grow(std::vector<T>& values, T&)
    {
        values.reserve(values.capacity() + 64);
        return values.at(0);
    }

    template <class T> T& Foreign(std::vector<T>&, T&)
    {
        static T value{};
        return value;
    }

    template <class T> T& Erase(std::unordered_map<std::string, T>& values, T&)
    {
        values.erase("kept");
        return values.at("other");
    }

    template <class T> T& Fail(std::vector<T>&, T& value, T next)
    {
        value = std::move(next);
        throw std::runtime_error("projected object native failure");
    }

    template <class T, class TNullable>
    T& MixedContract(std::vector<T>& values, T& required, TNullable& optional, std::vector<TNullable>& nullable)
    {
        if (&values.at(0) != &required || &nullable.at(0) != &optional)
            throw std::runtime_error("mixed object contract address mismatch");
        return required;
    }

    inline void Print()
    {
        std::cout << "native-projected-object-borrow-ok:30\n";
    }
} // namespace vm_projected_object_borrow
