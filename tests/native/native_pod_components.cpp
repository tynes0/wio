#include "native_pod_components.h"

float NativeIdentity(const vec2& value)
{
    return value.x;
}

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

    float SumPointer(const vec2* value)
    {
        return value->x + value->y;
    }

    float DotPointerSelf(const vec2* value, const vec2& other)
    {
        return value->x * other.x + value->y * other.y;
    }

    void ScalePointer(vec2* value, float amount)
    {
        value->x *= amount;
        value->y *= amount;
    }

    vec2 Make(float x, float y)
    {
        return vec2{ x, y };
    }
}
