#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace scalar_codegen_probe
{
    inline bool counting = false;
    inline std::uint64_t allocations = 0;

    inline void Begin()
    {
        allocations = 0;
        counting = true;
    }

    inline std::uint64_t End()
    {
        counting = false;
        return allocations;
    }

    inline void Bump(std::int32_t& value)
    {
        value += 7;
    }
} // namespace scalar_codegen_probe

void* operator new(std::size_t size)
{
    if (scalar_codegen_probe::counting)
        ++scalar_codegen_probe::allocations;
    if (void* value = std::malloc(size ? size : 1))
        return value;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete(void* value) noexcept
{
    std::free(value);
}

void operator delete[](void* value) noexcept
{
    ::operator delete(value);
}

void operator delete(void* value, std::size_t) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, std::size_t) noexcept
{
    ::operator delete(value);
}
