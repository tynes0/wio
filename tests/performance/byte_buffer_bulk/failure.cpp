#include "std_utility.h"

#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>

namespace
{
    bool failNext = false;
}

void* operator new(const std::size_t size)
{
    if (failNext)
    {
        failNext = false;
        throw std::bad_alloc();
    }

    if (void* result = std::malloc(size == 0 ? 1 : size))
    {
        return result;
    }

    throw std::bad_alloc();
}

void operator delete(void* value) noexcept
{
    std::free(value);
}

void operator delete(void* value, std::size_t) noexcept
{
    std::free(value);
}

int main()
{
    for (const bool alias : {false, true})
    {
        std::vector<std::uint8_t> value(64, 7);
        value.resize(value.capacity(), 7);
        const std::vector<std::uint8_t> source(value.capacity() + 17, 9);
        const auto snapshot = value;
        const auto capacity = value.capacity();
        const auto* storage = value.data();
        bool caught = false;
        failNext = true;
        try
        {
            (void)wio::runtime::std_heap::WriteByteBuffer(value, 1, alias ? value : source);
        }
        catch (const std::bad_alloc&)
        {
            caught = true;
        }

        failNext = false;
        if (!caught || value != snapshot || value.capacity() != capacity || value.data() != storage)
        {
            throw std::runtime_error("Failed bulk growth changed destination storage or bytes");
        }
    }

    std::cout << "byte-buffer-bulk-failure-ok: alias/nonalias allocation failure leaves destination unchanged\n";
}
