#include "native_struct_methods.h"

#include <cstdint>
#include <unordered_map>

namespace
{
    struct PointMirror
    {
        std::int32_t x;
        std::int32_t y;
    };

    std::unordered_map<void*, std::int32_t> g_counterValues;
    std::int32_t g_destroyedCounterCount = 0;
    std::int32_t g_destroyedPointCount = 0;
    std::int32_t g_lastPointSum = 0;
}

namespace native_struct
{
    void InitCounter(void* self, std::int32_t start)
    {
        g_counterValues[self] = start;
    }

    std::int32_t AddToCounter(void* self, std::int32_t delta)
    {
        auto it = g_counterValues.find(self);
        if (it == g_counterValues.end())
            return -1;

        it->second += delta;
        return it->second;
    }

    void DestroyCounter(void* self)
    {
        if (g_counterValues.erase(self) > 0)
            ++g_destroyedCounterCount;
    }

    void InitPoint(void* self, std::int32_t x, std::int32_t y)
    {
        auto* point = static_cast<PointMirror*>(self);
        point->x = x;
        point->y = y;
    }

    void DestroyPoint(void* self)
    {
        auto* point = static_cast<PointMirror*>(self);
        g_lastPointSum = point->x + point->y;
        ++g_destroyedPointCount;
    }

    std::int32_t GetDestroyedCounterCount()
    {
        return g_destroyedCounterCount;
    }

    std::int32_t GetDestroyedPointCount()
    {
        return g_destroyedPointCount;
    }

    std::int32_t GetLastPointSum()
    {
        return g_lastPointSum;
    }
}
