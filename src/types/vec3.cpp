#include "vec3.h"

#include "vec2.h"
#include "vec4.h"

namespace wio
{
    const vec3 vec3::zero{ 0.0 };
    const vec3 vec3::one{ 1.0 };
    const vec3 vec3::unit_x{ 1.0, 0.0, 0.0 };
    const vec3 vec3::unit_y{ 0.0, 1.0, 0.0 };
    const vec3 vec3::unit_z{ 0.0, 0.0, 1.0 };

    vec3::vec3(const vec2& other, double z) noexcept : x(other.x), y(other.y), z(z) {}

    vec3::vec3(const vec4& other) noexcept : x(other.x), y(other.y), z(other.z) {}

    vec3& vec3::operator=(const vec2& other) noexcept
    {
        x = other.x;
        y = other.y;
        y = 0.0;

        return *this;
    }

    vec3& vec3::operator=(const vec4& other) noexcept
    {
        x = other.x;
        y = other.y;
        z = other.z;

        return *this;
    }
}