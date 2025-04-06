#pragma once

namespace wio
{
    class vec4
    {
    public:
        union
        {
            struct { double x, y, z, w; };
            struct { double r, g, b, a; };
            double v[4];
        };

        friend class vec2;
        friend class vec3;
    };
}