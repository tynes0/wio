#pragma once
#include <random>
#include <cstdint>
#include <limits>

namespace wio
{
    class rand_manager
    {
    public:
        rand_manager();

        long long random();
        double drandom();

        long long int_range(long long min, long long max);
        double double_range(double min, double max);

    private:
        std::mt19937_64 rng;
    };
}