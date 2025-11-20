#pragma once
#include <random>
#include "../base/base.h"

namespace wio
{
    class rand_manager
    {
    public:
        rand_manager();

        integer_t random();
        float_t frandom();

        integer_t int_range(integer_t min, integer_t max);
        float_t float_range(float_t min, float_t max);

    private:
        std::mt19937_64 rng;
    };
}