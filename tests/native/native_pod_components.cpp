#include "native_pod_components.h"

namespace native_pod
{
    float LengthSquared(vec2 value)
    {
        return (value.x * value.x) + (value.y * value.y);
    }

    float SumView(const vec2& value)
    {
        return value.x + value.y;
    }

    void Translate(vec2& value, float dx, float dy)
    {
        value.x += dx;
        value.y += dy;
    }

    vec2 Make(float x, float y)
    {
        return vec2{ x, y };
    }
}
