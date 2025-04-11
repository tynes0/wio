#include "vec2.h"

#include "vec3.h"
#include "vec4.h"

namespace wio
{
    const vec2 vec2::zero{ 0.0 };
    const vec2 vec2::one{ 1.0 };
    const vec2 vec2::unit_x{ 1.0, 0.0 };
    const vec2 vec2::unit_y{ 0.0, 1.0 };

    vec2::vec2(const vec3& other) noexcept : x(other.x), y(other.y) {}

    vec2::vec2(const vec4& other) noexcept : x(other.x), y(other.y) {}

    vec2& vec2::operator=(const vec3& other) noexcept
    {
        x = other.x;
        y = other.y;

        return *this;
    }

    vec2& vec2::operator=(const vec4& other) noexcept
    {
        x = other.x;
        y = other.y;

        return *this;
    }
}