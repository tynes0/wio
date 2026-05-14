#pragma once

#include <concepts>
#include <iostream>

namespace wio::runtime::std_vector
{
    struct Vec2 {
        double x, y;
    };

    void PrintVec(Vec2 v)
    {
        std::cout << "Vec.x = " << v.x << ", Vec.y = " << v.y;
    }
}