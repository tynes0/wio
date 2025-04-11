#pragma once

#include "../base/exception.h"

#include <cstddef>
#include <cmath>
#include <algorithm>
#include <limits>


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

        static const vec3 zero;
        static const vec3 one;
        static const vec3 unit_x;
        static const vec3 unit_y;
        static const vec3 unit_z;

        constexpr vec3() noexcept : x(0), y(0), z(0) {}
        constexpr vec3(double x, double y, double z) noexcept : x(x), y(y), z(z) {}
        explicit constexpr vec3(double scalar) noexcept : x(scalar), y(scalar), z(scalar) {}
        constexpr vec3(const vec3& other) noexcept : x(other.x), y(other.y), z(other.z) {}

        vec3(const vec2& other, double z = 0.0) noexcept;
        vec3(const vec4& other) noexcept;
        vec3& operator=(const vec2& other) noexcept;
        vec3& operator=(const vec4& other) noexcept;

        vec3& operator=(double scalar) noexcept;
        vec3& operator=(const vec3& other) noexcept;

        constexpr vec3 operator+(const vec3& right) const noexcept;
        constexpr vec3 operator-(const vec3& right) const noexcept;
        constexpr vec3 operator*(const vec3& right) const noexcept;
        constexpr vec3 operator/(const vec3& right) const;

        constexpr vec3 operator+(double scalar) const noexcept;
        constexpr vec3 operator-(double scalar) const noexcept;
        constexpr vec3 operator*(double scalar) const noexcept;
        constexpr vec3 operator/(double scalar) const;

        vec3& operator+=(const vec3& right) noexcept;
        vec3& operator-=(const vec3& right) noexcept;
        vec3& operator*=(const vec3& right) noexcept;
        vec3& operator/=(const vec3& right);

        vec3& operator+=(double scalar) noexcept;
        vec3& operator-=(double scalar) noexcept;
        vec3& operator*=(double scalar) noexcept;
        vec3& operator/=(double scalar);

        vec3 operator++(int);
        vec3& operator++();
        vec3 operator--(int);
        vec3& operator--();

        constexpr double& operator[](size_t i);
        constexpr const double& operator[](size_t i) const;

        constexpr vec3 operator-() const noexcept;
        constexpr bool operator==(const vec3& other) const noexcept;
        constexpr bool operator!=(const vec3& other) const noexcept;

        constexpr double magnitude_squared() const noexcept;
        constexpr double dot(const vec3& other) const noexcept;
        constexpr vec3 cross(const vec3& other) const noexcept;
        constexpr double distance_squared(const vec3& other) const noexcept;

        double magnitude() const;
        double distance(const vec3& other) const;
        double angle(const vec3& other) const;

        vec3& normalize();
        vec3 normalized() const;
        bool is_unit(double epsilon = 1e-9) const noexcept;

        constexpr vec3 lerp(const vec3& target, double t) const noexcept;
        constexpr vec3 clamp(const vec3& min, const vec3& max) const;
        constexpr vec3 clamp(double min, double max) const;

        vec3 abs() const noexcept;
        vec3 floor() const noexcept;
        vec3 ceil() const noexcept;
        vec3 round() const noexcept;

        constexpr vec3 project(const vec3& onto) const;
        constexpr vec3 reflect(const vec3& normal) const;
        vec3 refract(const vec3& normal, double eta1, double eta2) const;
        vec3 rotate_around(const vec3& axis, double angle) const;

        constexpr vec3 smoothstep(const vec3& target, double t) const noexcept;
        constexpr vec3 smootherstep(const vec3& target, double t) const noexcept;

        constexpr double min_component() const noexcept;
        constexpr size_t min_dimension() const noexcept;
        constexpr double max_component() const noexcept;
        constexpr size_t max_dimension() const noexcept;

        int constexpr sign_dot(const vec3& other) const noexcept;

        bool equals(const vec3& other, double epsilon = 1e-9) const noexcept;
        constexpr bool is_one_of_zero() const noexcept;
        constexpr bool is_zero() const noexcept;
        bool is_nan() const noexcept;
        bool is_infinite() const noexcept;
    };
}

#include "vec3.inl"