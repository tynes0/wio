#pragma once

struct vec2
{
    float x;
    float y;
};

namespace native_pod
{
    float LengthSquared(vec2 value);
    float SumView(const vec2& value);
    void Translate(vec2& value, float dx, float dy);
    vec2 Make(float x, float y);
}
