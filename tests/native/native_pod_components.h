#pragma once

struct vec2
{
    float x;
    float y;
};

float NativeIdentity(const vec2& value);

namespace native_pod
{
    float LengthSquared(vec2 value);
    float SumView(const vec2& value);
    void Translate(vec2& value, float dx, float dy);
    float SumPointer(const vec2* value);
    float DotPointerSelf(const vec2* value, const vec2& other);
    void ScalePointer(vec2* value, float amount);
    vec2 Make(float x, float y);
}
