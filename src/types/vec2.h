#pragma once

#include "../base/exception.h"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits>


namespace wio
{
    class vec2
    {
    public:
        union
        {
            struct { double x, y; };
            struct { double r, g; };
            double v[2];
        };

        friend class vec3;
        friend class vec4;

        static const vec2 zero;
        static const vec2 one;
        static const vec2 unit_x;
        static const vec2 unit_y;

        constexpr vec2() noexcept : x(0), y(0) { }
        constexpr vec2(double x, double y) noexcept : x(x), y(y) { }
        explicit constexpr vec2(double scalar) noexcept : x(scalar), y(scalar) { }
        constexpr vec2(const vec2& other) noexcept : x(other.x), y(other.y) { }

        // constexpr vec2(const vec3& other) noexcept : x(other.x), y(other.y) {}
        // constexpr vec2(const vec4& other) noexcept : x(other.x), y(other.y) {}

        vec2& operator=(double scalar) noexcept
        {
            x = scalar;
            y = scalar;

            return *this;
        }

        vec2& operator=(const vec2& other) noexcept
        {
            x = other.x;
            y = other.y;

            return *this;
        }

        // vec2& operator=(const vec3& other) noexcept
        // {
        //     x = other.x;
        //     y = other.y;
        // 
        //     return *this;
        // }
        // 
        // vec2& operator=(const vec4& other) noexcept
        // {
        //     x = other.x;
        //     y = other.y;
        // 
        //     return *this;
        // }

        constexpr vec2 operator+(const vec2& right) const noexcept
        {
            return vec2(x + right.x, y + right.y);
        }

        constexpr vec2 operator-(const vec2& right) const noexcept
        {
            return vec2(x - right.x, y - right.y);
        }

        constexpr vec2 operator*(const vec2& right) const noexcept
        {
            return vec2(x * right.x, y * right.y);
        }

        constexpr vec2 operator/(const vec2& right) const
        {
            return vec2(x / right.x, y / right.y);
        }

        constexpr vec2 operator+(double scalar) const noexcept
        {
            return vec2(x + scalar, y + scalar);
        }

        constexpr vec2 operator-(double scalar) const noexcept
        {
            return vec2(x - scalar, y - scalar);
        }

        constexpr vec2 operator*(double scalar) const noexcept
        {
            return vec2(x * scalar, y * scalar);
        }

        constexpr vec2 operator/(double scalar) const
        {
            return vec2(x / scalar, y / scalar);
        }

        vec2& operator+=(const vec2& right) noexcept
        {
            x += right.x;
            y += right.y;

            return *this;
        }

        vec2& operator-=(const vec2& right) noexcept
        {
            x -= right.x;
            y -= right.y;

            return *this;
        }

        vec2& operator*=(const vec2& right) noexcept
        {
            x *= right.x;
            y *= right.y;

            return *this;
        }

        vec2& operator/=(const vec2& right)
        {
            x /= right.x;
            y /= right.y;

            return *this;
        }

        vec2& operator+=(double scalar) noexcept
        {
            x += scalar;
            y += scalar;

            return *this;
        }

        vec2& operator-=(double scalar) noexcept
        {
            x -= scalar;
            y -= scalar;

            return *this;
        }

        vec2& operator*=(double scalar) noexcept
        {
            x *= scalar;
            y *= scalar;

            return *this;
        }

        vec2& operator/=(double scalar)
        {
            x /= scalar;
            y /= scalar;

            return *this;
        }

        vec2 operator++(int)
        {
            vec2 result(*this);
            x++;
            y++;

            return result;
        }

        vec2& operator++()
        {
            x++;
            y++;

            return *this;
        }

        vec2 operator--(int)
        {
            vec2 result(*this);
            x--;
            y--;

            return result;
        }

        vec2& operator--()
        {
            x--;
            y--;

            return *this;
        }

        constexpr double& operator[](size_t idx)
        {
            if (idx >= 2)
                throw out_of_bounds_error("vec2 operator[] out_of_bounds_error!");

            return v[idx];
        }

        constexpr const double& operator[](size_t idx) const
        {
            if (idx >= 2)
                throw out_of_bounds_error("vec2 operator[] out_of_bounds_error!");

            return v[idx];
        }

        constexpr vec2 operator-() const noexcept
        {
            return vec2(-x, -y);
        }

        constexpr bool operator==(const vec2& other) const noexcept
        {
            return (x == other.x) && (y == other.y);
        }

        constexpr bool operator!=(const vec2& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr double magnitude_squared() const noexcept
        {
            return x * x + y * y;
        }

        constexpr double dot(const vec2& other) const noexcept
        {
            return x * other.x + y * other.y;
        }

        constexpr double cross(const vec2& other) const noexcept
        {
            return x * other.y - y * other.x;
        }

        constexpr double distance_squared(const vec2& other) const noexcept
        {
            return (*this - other).magnitude_squared();
        }

        double magnitude() const
        {
            return std::sqrt(x * x + y * y);
        }

        double distance(const vec2& other) const
        {
            return (*this - other).magnitude();
        }

        double angle(const vec2& other) const
        {
            double dot_prod = dot(other);
            double mag_prod = magnitude() * other.magnitude();

            if (mag_prod == 0)
                return 0.0;

            double cos_theta = dot_prod / mag_prod;

            return std::acos(std::clamp(cos_theta, -1.0, 1.0));
        }

        vec2& normalize()
        {
            double mag = magnitude();

            if (mag > 0)
                (*this) /= mag;

            return *this;
        }

        vec2 normalized() const
        {
            vec2 temp = *this;
            return temp.normalize();
        }

        vec2& rotate(double angle_rad)
        {
            double cs = std::cos(angle_rad);
            double sn = std::sin(angle_rad);

            double newX = x * cs - y * sn;
            double newY = x * sn + y * cs;

            x = newX;
            y = newY;

            return *this;
        }

        vec2 rotated(double angle_rad) const
        {
            vec2 temp = *this;
            return temp.rotate(angle_rad);
        }

        bool equals(const vec2& other, double epsilon = 1e-9) const noexcept
        {
            return (std::fabs(x - other.x) < epsilon) && (std::fabs(y - other.y) < epsilon);
        }

        constexpr vec2 lerp(const vec2& target, double t) const noexcept
        {
            return (*this) * (1.0 - t) + target * t;
        }

        constexpr vec2 clamp(const vec2& min, const vec2& max) const
        {
            return vec2(std::clamp(x, min.x, max.x), std::clamp(y, min.y, max.y));
        }

        vec2 abs() const noexcept
        {
            return vec2(std::fabs(x), std::fabs(y));
        }

        vec2 floor() const noexcept
        {
            return vec2(std::floor(x), std::floor(y));
        }

        vec2 ceil() const noexcept
        {
            return vec2(std::ceil(x), std::ceil(y));
        }

        vec2 round() const noexcept
        {
            return vec2(std::round(x), std::round(y));
        }

        vec2 project(const vec2& onto) const
        {
            const double onto_mag_sq = onto.magnitude_squared();

            if (std::fabs(onto_mag_sq) < std::numeric_limits<double>::epsilon())
                return vec2::zero;

            return onto * (this->dot(onto) / onto_mag_sq);
        }

        vec2 reflect(const vec2& normal) const
        {
            const double normal_mag_sq = normal.magnitude_squared();

            if (std::fabs(normal_mag_sq) < std::numeric_limits<double>::epsilon())
                return *this;

            return *this - normal * (2.0 * this->dot(normal) / normal_mag_sq);
        }

        constexpr vec2 perpendicular() const noexcept
        {
            return vec2(-y, x);
        }

        constexpr bool one_of_zero() const noexcept
        {
            return (x == 0.0 || y == 0.0);
        }
    };
}