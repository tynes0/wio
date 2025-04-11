#include "vec3.h"
namespace wio
{
    inline vec3& vec3::operator=(double scalar) noexcept { x = y = z = scalar; return *this; }
    inline vec3& vec3::operator=(const vec3& other) noexcept { x = other.x; y = other.y; z = other.z; return *this; }

    inline constexpr vec3 vec3::operator+(const vec3& right) const noexcept { return { x + right.x, y + right.y, z + right.z }; }
    inline constexpr vec3 vec3::operator-(const vec3& right) const noexcept { return { x - right.x, y - right.y, z - right.z }; }
    inline constexpr vec3 vec3::operator*(const vec3& right) const noexcept { return { x * right.x, y * right.y, z * right.z }; }
    inline constexpr vec3 vec3::operator/(const vec3& right) const { return { x / right.x, y / right.y, z / right.z }; }

    inline constexpr vec3 vec3::operator+(double scalar) const noexcept { return { x + scalar, y + scalar, z + scalar }; }
    inline constexpr vec3 vec3::operator-(double scalar) const noexcept { return { x - scalar, y - scalar, z - scalar }; }
    inline constexpr vec3 vec3::operator*(double scalar) const noexcept { return { x * scalar, y * scalar, z * scalar }; }
    inline constexpr vec3 vec3::operator/(double scalar) const { return { x / scalar, y / scalar, z / scalar }; }

    inline vec3& vec3::operator+=(const vec3& right) noexcept { x += right.x; y += right.y; z += right.z; return *this; }
    inline vec3& vec3::operator-=(const vec3& right) noexcept { x -= right.x; y -= right.y; z -= right.z; return *this; }
    inline vec3& vec3::operator*=(const vec3& right) noexcept { x *= right.x; y *= right.y; z *= right.z; return *this; }
    inline vec3& vec3::operator/=(const vec3& right) { x /= right.x; y /= right.y; z /= right.z; return *this; }

    inline vec3& vec3::operator+=(double scalar) noexcept { x += scalar; y += scalar; z += scalar; return *this; }
    inline vec3& vec3::operator-=(double scalar) noexcept { x -= scalar; y -= scalar; z -= scalar; return *this; }
    inline vec3& vec3::operator*=(double scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }
    inline vec3& vec3::operator/=(double scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    inline vec3 vec3::operator++(int) { vec3 r = *this; ++x; ++y; ++z; return r; }
    inline vec3& vec3::operator++() { ++x; ++y; ++z; return *this; }
    inline vec3 vec3::operator--(int) { vec3 r = *this; --x; --y; --z; return r; }
    inline vec3& vec3::operator--() { --x; --y; --z; return *this; }

    inline constexpr double& vec3::operator[](size_t i) { if (i >= 3) throw out_of_bounds_error("vec3 operator[] out_of_bounds_error!"); return v[i]; }
    inline constexpr const double& vec3::operator[](size_t i) const { if (i >= 3) throw out_of_bounds_error("vec3 operator[] out_of_bounds_error!"); return v[i]; }

    inline constexpr vec3 vec3::operator-() const noexcept { return vec3(- x, -y, -z); }
    inline constexpr bool vec3::operator==(const vec3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
    inline constexpr bool vec3::operator!=(const vec3& o) const noexcept { return !(*this == o); }

    inline constexpr double vec3::magnitude_squared() const noexcept
    {
        return x * x + y * y + z * z;
    }

    inline constexpr double vec3::dot(const vec3& other) const noexcept
    { 
        return x * other.x + y * other.y + z * other.z;
    }

    inline constexpr vec3 vec3::cross(const vec3& other) const noexcept
    {
        return vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    inline constexpr double vec3::distance_squared(const vec3& other) const noexcept
    { 
        return (*this - other).magnitude_squared();
    }

    inline double vec3::magnitude() const
    {
        return std::sqrt(magnitude_squared());
    }

    inline double vec3::distance(const vec3& other) const
    { 
        return (*this - other).magnitude(); 
    }

    inline double vec3::angle(const vec3& other) const
    {
        double dp = dot(other);
        double mp = magnitude() * other.magnitude();
        if (mp == 0) return 0.0;
        double cos_theta = dp / mp;
        return std::acos(std::clamp(cos_theta, -1.0, 1.0));
    }

    inline vec3& vec3::normalize()
    {
        double mag = magnitude();
        if (mag > 0) *this /= mag;
        return *this;
    }

    inline vec3 vec3::normalized() const
    { 
        return vec3(*this).normalize();
    }

    inline bool vec3::is_unit(double epsilon) const noexcept
    {
        return std::abs(magnitude_squared() - 1.0) < epsilon;
    }

    inline constexpr vec3 vec3::lerp(const vec3& target, double t) const noexcept
    {
        return *this * (1.0 - t) + target * t;
    }

    inline constexpr vec3 vec3::clamp(const vec3& min, const vec3& max) const
    {
        return vec3(
            std::clamp(x, min.x, max.x),
            std::clamp(y, min.y, max.y),
            std::clamp(z, min.z, max.z)
        );
    }

    inline constexpr vec3 vec3::clamp(double min, double max) const
    {
        return vec3(
            std::clamp(x, min, max),
            std::clamp(y, min, max),
            std::clamp(z, min, max)
        );
    }

    inline vec3 vec3::abs() const noexcept
    { 
        return vec3(std::fabs(x), std::fabs(y), std::fabs(z)); 
    }

    inline vec3 vec3::floor() const noexcept
    {
        return vec3(std::floor(x), std::floor(y), std::floor(z)); 
    }

    inline vec3 vec3::ceil() const noexcept
    { 
        return vec3(std::ceil(x), std::ceil(y), std::ceil(z)); 
    }

    inline vec3 vec3::round() const noexcept
    {
        return vec3(std::round(x), std::round(y), std::round(z)); 
    }

    inline constexpr vec3 vec3::project(const vec3& onto) const
    {
        double omag = onto.magnitude_squared();
        if (omag < std::numeric_limits<double>::epsilon()) return vec3::zero;
        return onto * (dot(onto) / omag);
    }

    inline constexpr vec3 vec3::reflect(const vec3& normal) const
    {
        double nmag = normal.magnitude_squared();
        if (nmag < std::numeric_limits<double>::epsilon()) return *this;
        return *this - normal * (2.0 * dot(normal) / nmag);
    }

    inline vec3 vec3::refract(const vec3& normal, double eta1, double eta2) const
    {
        double n_dot_i = dot(normal);
        double k = 1.0 - (eta1 / eta2) * (eta1 / eta2) * (1.0 - n_dot_i * n_dot_i);
        if (k < 0.0)
            return zero;
        return (*this - normal * n_dot_i) * (eta1 / eta2) - normal * std::sqrt(k);
    }

    inline vec3 vec3::rotate_around(const vec3& axis, double angle) const
    {
        double kx = axis.x, ky = axis.y, kz = axis.z;
        double vx = x, vy = y, vz = z;
        double cos_theta = std::cos(angle);
        double sin_theta = std::sin(angle);
        double one_minus_cos = 1.0 - cos_theta;
        
        return vec3(
            vx * (kx * kx * one_minus_cos + cos_theta) + vy * (kx * ky * one_minus_cos - kz * sin_theta) + vz * (kx * kz * one_minus_cos + ky * sin_theta),
            vx * (ky * kx * one_minus_cos + kz * sin_theta) + vy * (ky * ky * one_minus_cos + cos_theta) + vz * (ky * kz * one_minus_cos - kx * sin_theta),
            vx * (kz * kx * one_minus_cos - ky * sin_theta) + vy * (kz * ky * one_minus_cos + kx * sin_theta) + vz * (kz * kz * one_minus_cos + cos_theta)
        );
    }

    inline constexpr vec3 vec3::smoothstep(const vec3& target, double t) const noexcept
    {
        double clamped_t = std::clamp(t, 0.0, 1.0);
        double smooth_t = clamped_t * clamped_t * (3.0 - 2.0 * clamped_t);
        return lerp(target, smooth_t);
    }

    inline constexpr vec3 vec3::smootherstep(const vec3& target, double t) const noexcept
    {
        double clamped_t = std::clamp(t, 0.0, 1.0);
        double smooth_t = clamped_t * clamped_t * clamped_t * (t * (t * 6.0 - 15.0) + 10.0);
        return lerp(target, smooth_t);
    }

    inline constexpr double vec3::min_component() const noexcept
    {
        return std::min({ x, y, z });
    }

    inline constexpr size_t vec3::min_dimension() const noexcept
    {
        if (y < x)
        {
            if (z < y) return 2;
            return 1;
        }
        if (z < x) return 2;
        return 0;
    }

    inline constexpr double vec3::max_component() const noexcept
    {
        return std::max({ x, y, z });
    }

    inline constexpr size_t vec3::max_dimension() const noexcept
    {
        if (y > x)
        {
            if (z > y) return 2;
            return 1;
        }
        if (z > x) return 2;
        return 0;
    }

    inline constexpr int vec3::sign_dot(const vec3& other) const noexcept
    {
        double dot_product = dot(other);
        if (dot_product > std::numeric_limits<double>::epsilon()) return 1;
        if (dot_product < -std::numeric_limits<double>::epsilon()) return -1;
        return 0;
    }

    inline bool vec3::equals(const vec3& other, double epsilon) const noexcept
    {
        return (std::fabs(x - other.x) < epsilon) && (std::fabs(y - other.y) < epsilon) && (std::fabs(z - other.z) < epsilon);
    }

    inline constexpr bool vec3::is_one_of_zero() const noexcept
    { 
        return (x == 0.0 || y == 0.0 || z == 0.0);
    }


    inline constexpr bool wio::vec3::is_zero() const noexcept
    {
        return (x == 0.0 && y == 0.0 && z == 0.0);
    }

    inline bool vec3::is_nan() const noexcept
    {
        return (std::isnan(x) || std::isnan(y) || std::isnan(z));
    }

    inline bool vec3::is_infinite() const noexcept
    {
        return (std::isinf(x) || std::isinf(y) || std::isinf(z));
    }
}