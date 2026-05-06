#pragma once

#include <cstdint>

namespace native_struct
{
    void InitCounter(void* self, std::int32_t start);
    std::int32_t AddToCounter(void* self, std::int32_t delta);
    void DestroyCounter(void* self);

    void InitPoint(void* self, std::int32_t x, std::int32_t y);
    void DestroyPoint(void* self);

    std::int32_t GetDestroyedCounterCount();
    std::int32_t GetDestroyedPointCount();
    std::int32_t GetLastPointSum();
}
