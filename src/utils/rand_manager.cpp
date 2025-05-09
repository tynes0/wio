#include "rand_manager.h"
#include <utility>

namespace wio
{
    rand_manager::rand_manager()
        : rng(std::random_device{}()) {
    }

    integer_t rand_manager::random()
    {
        std::uniform_int_distribution<integer_t> dist(
            std::numeric_limits<integer_t>::min(),
            std::numeric_limits<integer_t>::max() - 1
        );
        return dist(rng);
    }

    float_t rand_manager::frandom()
    {
        std::uniform_real_distribution<float_t> dist(
            std::numeric_limits<float_t>::min(),
            std::numeric_limits<float_t>::max()
        );
        return dist(rng);
    }

    integer_t rand_manager::int_range(integer_t min, integer_t max)
    {
        if (min > max) std::swap(min, max);

        std::uniform_int_distribution<integer_t> dist(min, max - 1);
        return dist(rng);
    }

    float_t rand_manager::float_range(float_t min, float_t max)
    {
        if (min > max) std::swap(min, max);

        std::uniform_real_distribution<float_t> dist(min, max);
        return dist(rng);
    }

}
