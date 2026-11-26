#pragma once

// Diagnostic executable only. These global operators must never be linked into
// production, a shared native library, or the timing/allocation-count probes.
#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include <ref.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace memory_costs
{
    struct Counters
    {
        std::uint64_t allocations = 0;
        std::uint64_t frees = 0;
        std::uint64_t requested = 0;
        std::uint64_t freed = 0;
        std::uint64_t live = 0;
        std::uint64_t peak = 0;
        std::uint64_t liveBlocks = 0;
        std::uint64_t peakBlocks = 0;
        std::uint64_t aligned = 0;
        std::uint64_t failures = 0;
    };

    inline std::atomic_flag guard = ATOMIC_FLAG_INIT;
    inline Counters counters;
    inline bool active = false;
    inline bool begun = false;
    inline void* beforeWindow = nullptr;

    struct Lock
    {
        Lock() noexcept
        {
            while (guard.test_and_set(std::memory_order_acquire))
            {
            }
        }

        ~Lock()
        {
            guard.clear(std::memory_order_release);
        }
    };

    struct alignas(std::max_align_t) Header
    {
        void* base;
        std::size_t requested;
        bool tracked;
    };

    inline void RecordFailure() noexcept
    {
        const Lock lock;
        if (active)
        {
            ++counters.failures;
        }
    }

    [[noreturn]] inline void Failure()
    {
        RecordFailure();
        throw std::bad_alloc{};
    }

    inline void* Allocate(const std::size_t size, std::size_t alignment, const bool overAligned)
    {
        alignment = alignment < alignof(Header) ? alignof(Header) : alignment;
        const auto limit = std::numeric_limits<std::size_t>::max();
        const std::size_t payload = size ? size : 1;
        if ((alignment & (alignment - 1)) != 0 || alignment > limit - sizeof(Header) ||
            payload > limit - sizeof(Header) - alignment)
        {
            Failure();
        }

        const std::size_t total = sizeof(Header) + alignment + payload;
        void* base = nullptr;
        while (!(base = std::malloc(total)))
        {
            const auto handler = std::get_new_handler();
            if (!handler)
            {
                Failure();
            }

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
        std::size_t available = total - sizeof(Header);
        if (!std::align(alignment, payload, address, available))
        {
            std::free(base);
            Failure();
        }

        auto* header = ::new (static_cast<char*>(address) - sizeof(Header)) Header{base, size, false};
        {
            const Lock lock;
            header->tracked = active;
            if (active)
            {
                ++counters.allocations;
                counters.requested += size;
                counters.live += size;
                ++counters.liveBlocks;
                counters.aligned += overAligned;
                counters.peak = counters.peak < counters.live ? counters.live : counters.peak;
                counters.peakBlocks =
                    counters.peakBlocks < counters.liveBlocks ? counters.liveBlocks : counters.peakBlocks;
            }
        }

        return address;
    }

    inline void Free(void* address) noexcept
    {
        if (!address)
        {
            return;
        }

        auto* header = reinterpret_cast<Header*>(static_cast<char*>(address) - sizeof(Header));
        void* base = header->base;
        if (header->tracked)
        {
            const Lock lock;
            if (!counters.liveBlocks || counters.live < header->requested)
            {
                std::abort();
            }

            ++counters.frees;
            counters.freed += header->requested;
            counters.live -= header->requested;
            --counters.liveBlocks;
        }

        header->~Header();
        std::free(base);
    }

    struct ProcessMemory
    {
        const char* source = "unsupported";
        std::uint64_t resident = 0;
        std::uint64_t lifetimePeak = 0;
        std::uint64_t privateCommit = 0;
        bool valid = false;
    };

    inline ProcessMemory ReadProcessMemory()
    {
        ProcessMemory result;
#if defined(_WIN32)
        result.source = "win32-working-set";
        PROCESS_MEMORY_COUNTERS_EX memory{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                                 sizeof(memory)))
        {
            result.resident = memory.WorkingSetSize;
            result.lifetimePeak = memory.PeakWorkingSetSize;
            result.privateCommit = memory.PrivateUsage;
            result.valid = true;
        }
#elif defined(__linux__)
        result.source = "linux-proc-status-approximate";
        if (FILE* file = std::fopen("/proc/self/status", "r"))
        {
            char line[256];
            unsigned long long value = 0;
            bool current = false;
            bool peak = false;
            while (std::fgets(line, sizeof(line), file))
            {
                if (std::sscanf(line, "VmRSS: %llu kB", &value) == 1)
                {
                    result.resident = value * 1024;
                    current = true;
                }

                if (std::sscanf(line, "VmHWM: %llu kB", &value) == 1)
                {
                    result.lifetimePeak = value * 1024;
                    peak = true;
                }
            }

            std::fclose(file);
            result.valid = current && peak;
        }
#elif defined(__APPLE__)
        result.source = "darwin-mach-task-basic-info";
        mach_task_basic_info_data_t memory{};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&memory), &count) ==
            KERN_SUCCESS)
        {
            result.resident = memory.resident_size;
            result.lifetimePeak = memory.resident_size_max;
            result.valid = true;
        }
#endif
        return result;
    }

    inline void Print(const char* phase, const std::uint64_t checksum = 0)
    {
        Counters value;
        {
            const Lock lock;
            value = counters;
        }

        const auto process = ReadProcessMemory();
        std::printf("{\"Phase\":\"%s\",\"Checksum\":\"%llu\",\"Allocations\":%llu,\"Frees\":%llu,"
                    "\"RequestedBytes\":%llu,\"FreedBytes\":%llu,\"LiveBytes\":%llu,\"PeakBytes\":%llu,"
                    "\"LiveBlocks\":%llu,\"PeakBlocks\":%llu,\"AlignedAllocations\":%llu,\"Failures\":%llu,"
                    "\"MemorySource\":\"%s\",\"MemoryAvailable\":%s,\"ResidentBytes\":",
                    phase, static_cast<unsigned long long>(checksum),
                    static_cast<unsigned long long>(value.allocations), static_cast<unsigned long long>(value.frees),
                    static_cast<unsigned long long>(value.requested), static_cast<unsigned long long>(value.freed),
                    static_cast<unsigned long long>(value.live), static_cast<unsigned long long>(value.peak),
                    static_cast<unsigned long long>(value.liveBlocks),
                    static_cast<unsigned long long>(value.peakBlocks), static_cast<unsigned long long>(value.aligned),
                    static_cast<unsigned long long>(value.failures), process.source, process.valid ? "true" : "false");
        if (process.valid)
        {
            std::printf("%llu,\"ProcessLifetimePeakResidentBytes\":%llu,\"PrivateCommitBytes\":",
                        static_cast<unsigned long long>(process.resident),
                        static_cast<unsigned long long>(process.lifetimePeak));
#if defined(_WIN32)
            std::printf("%llu", static_cast<unsigned long long>(process.privateCommit));
#else
            std::printf("null");
#endif
        }
        else
        {
            std::printf("null,\"ProcessLifetimePeakResidentBytes\":null,\"PrivateCommitBytes\":null");
        }

        std::printf("}\n");
    }

    inline void Begin()
    {
        if (begun)
        {
            throw std::logic_error("Only one diagnostic allocation window is allowed per process");
        }

        begun = true;
        beforeWindow = ::operator new(13);
        Print("baseline");
        const Lock lock;
        active = true;
    }

    inline void Checkpoint()
    {
        ::operator delete(beforeWindow);
        beforeWindow = nullptr;
        Print("live");
    }

    inline int Report(const std::uint64_t checksum)
    {
        {
            const Lock lock;
            active = false;
        }

        Print("released", checksum);
        return 0;
    }

    inline std::int32_t ParseCount(const std::string& text)
    {
        std::int32_t count = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), count);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || count < 1 || count > 100000)
        {
            throw std::invalid_argument("count must be in [1,100000]");
        }

        return count;
    }

    inline std::uint64_t Hash(const std::string& text)
    {
        std::uint64_t result = 14695981039346656037ULL;
        for (const unsigned char value : text)
        {
            result = (result ^ value) * 1099511628211ULL;
        }

        return result;
    }

    std::uint64_t Control(std::int32_t count);
    std::uint64_t AlignedControl(std::int32_t count);
    std::uint64_t WeakControl(std::int32_t count);
} // namespace memory_costs

void* operator new(const std::size_t size)
{
    return memory_costs::Allocate(size, alignof(std::max_align_t), false);
}

void* operator new[](const std::size_t size)
{
    return ::operator new(size);
}

void* operator new(const std::size_t size, const std::align_val_t alignment)
{
    return memory_costs::Allocate(size, static_cast<std::size_t>(alignment), true);
}

void* operator new[](const std::size_t size, const std::align_val_t alignment)
{
    return ::operator new(size, alignment);
}

void* operator new(const std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](const std::size_t size, const std::nothrow_t& tag) noexcept
{
    return ::operator new(size, tag);
}

void* operator new(const std::size_t size, const std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(size, alignment);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](const std::size_t size, const std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
    return ::operator new(size, alignment, tag);
}

void operator delete(void* value) noexcept
{
    memory_costs::Free(value);
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

void operator delete(void* value, std::align_val_t) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, std::align_val_t) noexcept
{
    ::operator delete(value);
}

void operator delete(void* value, std::size_t, std::align_val_t) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, std::size_t, std::align_val_t) noexcept
{
    ::operator delete(value);
}

void operator delete(void* value, const std::nothrow_t&) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, const std::nothrow_t&) noexcept
{
    ::operator delete(value);
}

void operator delete(void* value, std::align_val_t, const std::nothrow_t&) noexcept
{
    ::operator delete(value);
}

void operator delete[](void* value, std::align_val_t, const std::nothrow_t&) noexcept
{
    ::operator delete(value);
}

namespace memory_costs
{
    struct ObservedObject final : wio::runtime::RefCountedObject
    {
        bool* destroyed;

        explicit ObservedObject(bool* flag) : destroyed(flag)
        {
        }

        ~ObservedObject() override
        {
            *destroyed = true;
        }
    };

    inline std::uint64_t WeakControl(const std::int32_t count)
    {
        bool destroyed = false;
        auto strong = wio::runtime::Ref<ObservedObject>::Create(&destroyed);
        wio::runtime::WeakRef<ObservedObject> weak{strong};
        strong.Reset();
        if (!weak.Expired() || weak.Lock() || destroyed)
        {
            throw std::logic_error("weak lifetime checkpoint mismatch");
        }

        Checkpoint();
        weak.Reset();
        if (!destroyed)
        {
            throw std::logic_error("final weak release did not destroy allocation");
        }

        return static_cast<std::uint64_t>(count) + 1;
    }

    inline void Touch(void* address, const std::size_t size)
    {
        auto* bytes = static_cast<volatile unsigned char*>(address);
        for (std::size_t index = 0; index < size; index += 4096)
        {
            bytes[index] = static_cast<unsigned char>(index);
        }

        if (size)
        {
            bytes[size - 1] = 42;
        }
    }

    inline std::uint64_t Control(const std::int32_t count)
    {
        const auto firstSize = static_cast<std::size_t>(count) * 64 + 1;
        const auto secondSize = static_cast<std::size_t>(count) * 128 + 3;
        void* first = ::operator new(firstSize);
        void* second = ::operator new[](secondSize);
        Touch(first, firstSize);
        Touch(second, secondSize);
        Checkpoint();
        ::operator delete(first);
        ::operator delete[](second);
        return firstSize + secondSize;
    }

    inline std::uint64_t AlignedControl(const std::int32_t count)
    {
        void* zero = ::operator new(0);
        void* array = ::operator new[](count + 7);
        void* aligned = ::operator new(257, std::align_val_t{64});
        void* page = ::operator new[](8193, std::align_val_t{4096});
        void* safe = ::operator new(19, std::nothrow);
        void* safeAligned = ::operator new[](23, std::align_val_t{128}, std::nothrow);
        if (!safe || !safeAligned || reinterpret_cast<std::uintptr_t>(aligned) % 64 ||
            reinterpret_cast<std::uintptr_t>(page) % 4096 || reinterpret_cast<std::uintptr_t>(safeAligned) % 128)
        {
            throw std::logic_error("allocation alignment mismatch");
        }

        bool threw = false;
        try
        {
            ::operator delete(::operator new(std::numeric_limits<std::size_t>::max()));
        }
        catch (const std::bad_alloc&)
        {
            threw = true;
        }

        if (!threw || ::operator new(std::numeric_limits<std::size_t>::max(), std::nothrow))
        {
            throw std::logic_error("overflow allocation did not fail safely");
        }

        Touch(array, count + 7);
        Touch(aligned, 257);
        Touch(page, 8193);
        Touch(safe, 19);
        Touch(safeAligned, 23);
        Checkpoint();
        ::operator delete(zero);
        ::operator delete[](array, static_cast<std::size_t>(count + 7));
        ::operator delete(aligned, std::align_val_t{64});
        ::operator delete[](page, std::size_t{8193}, std::align_val_t{4096});
        ::operator delete(safe, std::nothrow);
        ::operator delete[](safeAligned, std::align_val_t{128}, std::nothrow);
        ::operator delete(nullptr);
        return static_cast<std::uint64_t>(count) + 8499;
    }
} // namespace memory_costs
