#pragma once

// Explicit diagnostic single-translation-unit executable ONLY. Never enable in
// shipping executables, DLLs, SDK hosts, or multiple TUs. All linked allocations
// routed through ordinary/aligned global C++ new/delete are intercepted; malloc,
// custom allocators and managed-runtime allocations are NOT counted.
#ifndef WIO_BENCHMARK_ALLOCATIONS
#error Define WIO_BENCHMARK_ALLOCATIONS for this diagnostic executable.
#endif

#include "benchmark_memory.h"
#include <cstdlib>
#include <memory>
#include <new>

namespace wio::runtime::benchmark::allocation_detail
{
    struct alignas(std::max_align_t) Header
    {
        void* base;
        std::size_t size;
        std::uint64_t epoch;
    };

    inline void RecordFailure() noexcept
    {
        const Lock lock;
        if (active)
            ++counters.failures;
    }

    [[noreturn]] inline void Fail()
    {
        RecordFailure();
        throw std::bad_alloc{};
    }

    inline void* Allocate(std::size_t size, std::size_t alignment)
    {
        alignment = alignment < alignof(Header) ? alignof(Header) : alignment;
        const auto limit = std::numeric_limits<std::size_t>::max();
        const auto payload = size ? size : 1;
        if ((alignment & (alignment - 1)) || alignment > limit - sizeof(Header) ||
            payload > limit - sizeof(Header) - alignment)
            Fail();
        const auto total = sizeof(Header) + alignment + payload;
        void* base = nullptr;
        while (!(base = std::malloc(total)))
        {
            const auto handler = std::get_new_handler();
            if (!handler)
                Fail();
            try
            {
                handler();
            }
            catch (...)
            {
                RecordFailure();
                throw;
            }
        }
        void* address = static_cast<char*>(base) + sizeof(Header);
        auto space = total - sizeof(Header);
        if (!std::align(alignment, payload, address, space))
        {
            std::free(base);
            Fail();
        }
        auto* header = ::new (static_cast<char*>(address) - sizeof(Header)) Header{base, size, 0};
        const Lock lock;
        if (active)
        {
            header->epoch = epoch;
            ++counters.allocations;
            counters.requestedBytes += size;
            counters.liveBytes += size;
            ++counters.liveBlocks;
            if (counters.liveBytes > counters.peakBytes)
                counters.peakBytes = counters.liveBytes;
            if (counters.liveBlocks > counters.peakBlocks)
                counters.peakBlocks = counters.liveBlocks;
        }
        return address;
    }

    inline void Free(void* address) noexcept
    {
        if (!address)
            return;
        auto* header = reinterpret_cast<Header*>(static_cast<char*>(address) - sizeof(Header));
        if (header->epoch)
        {
            const Lock lock;
            if (header->epoch != epoch || !counters.liveBlocks || counters.liveBytes < header->size)
                std::abort();
            ++counters.frees;
            counters.freedBytes += header->size;
            counters.liveBytes -= header->size;
            --counters.liveBlocks;
        }
        std::free(header->base);
    }
} // namespace wio::runtime::benchmark::allocation_detail

void* operator new(std::size_t n)
{
    return wio::runtime::benchmark::allocation_detail::Allocate(n, alignof(std::max_align_t));
}
void* operator new[](std::size_t n)
{
    return ::operator new(n);
}
void* operator new(std::size_t n, std::align_val_t a)
{
    return wio::runtime::benchmark::allocation_detail::Allocate(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a)
{
    return ::operator new(n, a);
}
void operator delete(void* p) noexcept
{
    wio::runtime::benchmark::allocation_detail::Free(p);
}
void operator delete[](void* p) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept
{
    ::operator delete(p);
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(n);
    }
    catch (...)
    {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new[](n);
    }
    catch (...)
    {
        return nullptr;
    }
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(n, a);
    }
    catch (...)
    {
        return nullptr;
    }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new[](n, a);
    }
    catch (...)
    {
        return nullptr;
    }
}
void operator delete(void* p, const std::nothrow_t&) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept
{
    ::operator delete(p);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
    ::operator delete(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept
{
    ::operator delete(p);
}
