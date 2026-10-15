#pragma once

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#include <ref.h>

#ifdef WIO_COST_COVERAGE
extern "C" void __gcov_reset();
extern "C" void __gcov_dump();
#endif

namespace ownership_costs
{
    inline void Begin()
    {
#ifdef WIO_COST_COVERAGE
        __gcov_reset();
#endif
    }

    inline int Report(const std::uint64_t checksum)
    {
#ifdef WIO_COST_COVERAGE
        __gcov_dump();
#endif
        std::printf("checksum=%llu\n", static_cast<unsigned long long>(checksum));
        return 0;
    }

    inline int ParseCount(const std::string& text)
    {
        int result = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (error != std::errc{} || end != text.data() + text.size() || result < 1 || result > 100000)
        {
            throw std::invalid_argument("count must be in [1,100000]");
        }

        return result;
    }

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

    // Exercise the actual header-only runtime, including logical expiration and
    // delayed physical destruction. This is native weak support, not Wio syntax.
    inline std::uint64_t WeakRoundtrip(const std::uint64_t value)
    {
        bool destroyed = false;
        auto owner = wio::runtime::Ref<ObservedObject>::Create(&destroyed);
        wio::runtime::WeakRef<ObservedObject> weak{owner};
        {
            const auto locked = weak.Lock();
            if (!locked || weak.Expired())
            {
                throw std::logic_error("live weak lock failed");
            }
        }

        owner.Reset();
        if (!weak.Expired() || weak.Lock() || destroyed)
        {
            throw std::logic_error("weak expiration mismatch");
        }

        weak.Reset();
        if (!destroyed)
        {
            throw std::logic_error("weak destruction mismatch");
        }

        return value + 1;
    }

    // Calibration uses explicit operations with known counts. It is deliberately
    // separate from the Wio transport workloads and is not a C++ speed baseline.
    inline std::uint64_t Calibrate(const std::uint64_t value)
    {
        bool destroyed = false;
        using Handle = wio::runtime::Ref<ObservedObject>;
        auto owner = Handle::Create(&destroyed);
        Handle copy{owner};
        Handle moved{std::move(copy)};
        Handle assigned;
        assigned = owner;
        Handle target;
        target = std::move(assigned);
        wio::runtime::BorrowedObjectView<ObservedObject> view{owner};
        wio::runtime::WeakRef<ObservedObject> weak{owner};
        {
            const auto locked = weak.Lock();
            if (!locked || view.Get() != owner.Get() || copy || assigned || owner->StrongCount() != 4)
            {
                throw std::logic_error("copy/move calibration mismatch");
            }
        }

        moved.Reset();
        target.Reset();
        owner.Reset();
        if (destroyed || !weak.Expired() || weak.Lock())
        {
            throw std::logic_error("calibration expiration mismatch");
        }

        weak.Reset();
        if (!destroyed)
        {
            throw std::logic_error("calibration destruction mismatch");
        }

        return value + 1;
    }
} // namespace ownership_costs
