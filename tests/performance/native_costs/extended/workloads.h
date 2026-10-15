#pragma once

#include "probe.h"
#include <ref.h>

namespace native_costs_extended
{
    struct WeakObject final : wio::runtime::RefCountedObject
    {
        std::uint64_t value;
        bool* destroyed;

        WeakObject(const std::uint64_t initial, bool* observed) : value(initial), destroyed(observed)
        {
        }

        ~WeakObject() override
        {
            *destroyed = true;
        }
    };

    // The same real runtime helper is called from both fixtures. Wio adds its
    // native boundary; this is not a new source-language weak-reference feature.
#if defined(_MSC_VER)
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    inline std::uint64_t
    WeakRoundtrip(const std::uint64_t initial)
    {
        bool destroyed = false;
        auto strong = wio::runtime::Ref<WeakObject>::Create(initial, &destroyed);
        wio::runtime::WeakRef<WeakObject> weak{strong};
        std::uint64_t value = 0;
        {
            const auto locked = weak.Lock();
            if (!locked || weak.Expired())
                throw std::logic_error("live weak lock failed");
            value = locked->value;
        }

        strong.Reset();
        if (!weak.Expired() || weak.Lock() || destroyed)
            throw std::logic_error("weak logical/physical lifetime mismatch");
        weak.Reset();
        if (!destroyed)
            throw std::logic_error("final weak release did not destroy object");
        return value + 1;
    }
} // namespace native_costs_extended
