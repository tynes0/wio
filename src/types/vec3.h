#pragma once

namespace wio
{
	class vec3
	{
    public:
        union
        {
            struct { double x, y, z; };
            struct { double r, g, b; };
            double v[3];
        };

        friend class vec2;
        friend class vec4;
	};
}