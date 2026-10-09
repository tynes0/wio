#pragma once

#include "vm_object_any_fixture.h"
#include <functional>
#include <utility>

namespace vm_free_borrow
{
    struct Packet
    {
        std::int32_t value = 3;
        std::int32_t hidden = 91;
    };

    template <class T> T& Borrow(T& value)
    {
        return value;
    }

    template <class T> const T& Observe(const T& value)
    {
        return value;
    }

    template <class T> T& Choose(T& first, T& second, bool left)
    {
        return left ? first : second;
    }

    template <class T> const T& ViewChoose(const T& first, const T& second, bool left)
    {
        return left ? first : second;
    }

    template <class T> T& Rebind(T& target, T source)
    {
        target = std::move(source);
        return target;
    }

    inline void Set(std::int32_t& target, std::int32_t value)
    {
        target = value;
    }

    inline void Update(Packet& target)
    {
        target.value = 42;
        ++target.hidden;
    }

    inline std::int32_t Hidden(const Packet& target)
    {
        return target.hidden;
    }

    inline std::int32_t& Global(std::int32_t&)
    {
        static std::int32_t unrelated = 77;
        return unrelated;
    }

    inline void* PointerSeed()
    {
        static std::int32_t tag = 88;
        return &tag;
    }

    inline std::int32_t& Project(Packet& target, std::int32_t&)
    {
        return target.value;
    }

    inline bool Reject(const std::function<std::int32_t()>& action)
    {
        try
        {
            action();
            return false;
        }
        catch (const std::exception&)
        {
            return true;
        }
    }

    inline void Print()
    {
        std::cout << "native-free-borrow-ok:30\n";
    }
} // namespace vm_free_borrow
