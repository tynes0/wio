#include "rand_manager.h"
#include <algorithm>  // std::swap

namespace wio
{
    rand_manager::rand_manager()
        : rng(std::random_device{}()) {
    }

    long long rand_manager::random()
    {
        std::uniform_int_distribution<long long> dist(
            std::numeric_limits<long long>::min(),
            std::numeric_limits<long long>::max() - 1
        );
        return dist(rng);
    }

    double rand_manager::drandom()
    {
        std::uniform_real_distribution<double> dist(
            std::numeric_limits<double>::min(),
            std::numeric_limits<double>::max()
        );
        return dist(rng);
    }

    long long rand_manager::int_range(long long min, long long max)
    {
        if (min > max) std::swap(min, max);

        std::uniform_int_distribution<long long> dist(min, max - 1);
        return dist(rng);
    }

    double rand_manager::double_range(double min, double max)
    {
        if (min > max) std::swap(min, max);

        std::uniform_real_distribution<double> dist(min, max);
        return dist(rng);
    }

}
