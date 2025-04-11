#include "vec2.h"
namespace wio
{
	/* ------------- OPERATORS --------------- */

	inline vec2& vec2::operator=(double scalar) noexcept { x = y = scalar; return *this; }
	inline vec2& vec2::operator=(const vec2& other) noexcept { x = y = other.y; return *this; }

	inline constexpr vec2 vec2::operator+(const vec2& right) const noexcept { return vec2(x + right.x, y + right.y); }
	inline constexpr vec2 vec2::operator-(const vec2& right) const noexcept { return vec2(x - right.x, y - right.y); }
	inline constexpr vec2 vec2::operator*(const vec2& right) const noexcept { return vec2(x * right.x, y * right.y); }
	inline constexpr vec2 vec2::operator/(const vec2& right) const { return vec2(x / right.x, y / right.y); }

	inline constexpr vec2 vec2::operator+(double scalar) const noexcept { return vec2(x + scalar, y + scalar); }
	inline constexpr vec2 vec2::operator-(double scalar) const noexcept { return vec2(x - scalar, y - scalar); }
	inline constexpr vec2 vec2::operator*(double scalar) const noexcept { return vec2(x * scalar, y * scalar); }
	inline constexpr vec2 vec2::operator/(double scalar) const { return vec2(x / scalar, y / scalar); }

	inline vec2& vec2::operator+=(const vec2& right) noexcept { x += right.x; y += right.y; return *this; }
	inline vec2& vec2::operator-=(const vec2& right) noexcept { x -= right.x; y -= right.y; return *this; }
	inline vec2& vec2::operator*=(const vec2& right) noexcept { x *= right.x; y *= right.y; return *this; }
	inline vec2& vec2::operator/=(const vec2& right) { x /= right.x; y /= right.y; return *this; }

	inline vec2& vec2::operator+=(double scalar) noexcept { x += scalar; y += scalar; return *this; }
	inline vec2& vec2::operator-=(double scalar) noexcept { x -= scalar; y -= scalar; return *this; }
	inline vec2& vec2::operator*=(double scalar) noexcept { x *= scalar; y *= scalar; return *this; }
	inline vec2& vec2::operator/=(double scalar) { x /= scalar; y /= scalar; return *this; }

	inline vec2 vec2::operator++(int) { vec2 result(*this); x++; y++; return result; }
	inline vec2& vec2::operator++() { x++; y++; return *this; }
	inline vec2 vec2::operator--(int) { vec2 result(*this); x--; y--; return result; }
	inline vec2& vec2::operator--() { x--; y--; return *this; }

	inline constexpr double& vec2::operator[](size_t idx) { if (idx >= 2) throw out_of_bounds_error("vec2 operator[] out_of_bounds_error!"); return v[idx]; }
	inline constexpr const double& vec2::operator[](size_t idx) const { if (idx >= 2) throw out_of_bounds_error("vec2 operator[] out_of_bounds_error!"); return v[idx]; }

	inline constexpr vec2 vec2::operator-() const noexcept { return vec2(-x, -y); }
	inline constexpr bool vec2::operator==(const vec2& other) const noexcept { return (x == other.x) && (y == other.y); }
	inline constexpr bool vec2::operator!=(const vec2& other) const noexcept { return !(*this == other); }

	/* ------------- METHODS --------------- */

	inline constexpr double vec2::magnitude_squared() const noexcept
	{
		return x * x + y * y;
	}

	inline constexpr double vec2::dot(const vec2& other) const noexcept
	{
		return x * other.x + y * other.y;
	}

	inline constexpr double vec2::cross(const vec2& other) const noexcept
	{
		return x * other.y - y * other.x;
	}

	inline constexpr double vec2::distance_squared(const vec2& other) const noexcept
	{
		return (*this - other).magnitude_squared();
	}

	inline double vec2::magnitude() const
	{
		return std::sqrt(x * x + y * y);
	}

	inline double vec2::distance(const vec2& other) const
	{
		return (*this - other).magnitude();
	}

	inline double vec2::angle(const vec2& other) const
	{
		double dot_prod = dot(other);
		double mag_prod = magnitude() * other.magnitude();

		if (mag_prod == 0)
			return 0.0;

		double cos_theta = dot_prod / mag_prod;

		return std::acos(std::clamp(cos_theta, -1.0, 1.0));
	}

	inline vec2& vec2::normalize()
	{
		double mag = magnitude();

		if (mag > 0)
			(*this) /= mag;

		return *this;
	}

	inline vec2 vec2::normalized() const
	{
		vec2 temp = *this;
		return temp.normalize();
	}

	inline vec2& vec2::rotate(double angle_rad)
	{
		double cs = std::cos(angle_rad);
		double sn = std::sin(angle_rad);

		double newX = x * cs - y * sn;
		double newY = x * sn + y * cs;

		x = newX;
		y = newY;

		return *this;
	}

	inline vec2 vec2::rotated(double angle_rad) const
	{
		vec2 temp = *this;
		return temp.rotate(angle_rad);
	}

	inline constexpr vec2 vec2::lerp(const vec2& target, double t) const noexcept
	{
		return (*this) * (1.0 - t) + target * t;
	}

	inline constexpr vec2 vec2::clamp(const vec2& min, const vec2& max) const
	{
		return vec2(std::clamp(x, min.x, max.x), std::clamp(y, min.y, max.y));
	}

	inline vec2 vec2::abs() const noexcept
	{
		return vec2(std::fabs(x), std::fabs(y));
	}

	inline vec2 vec2::floor() const noexcept
	{
		return vec2(std::floor(x), std::floor(y));
	}

	inline vec2 vec2::ceil() const noexcept
	{
		return vec2(std::ceil(x), std::ceil(y));
	}

	inline vec2 vec2::round() const noexcept
	{
		return vec2(std::round(x), std::round(y));
	}

	inline vec2 vec2::project(const vec2& onto) const
	{
		const double onto_mag_sq = onto.magnitude_squared();

		if (std::fabs(onto_mag_sq) < std::numeric_limits<double>::epsilon())
			return vec2::zero;

		return onto * (this->dot(onto) / onto_mag_sq);
	}

	inline vec2 vec2::reflect(const vec2& normal) const
	{
		const double normal_mag_sq = normal.magnitude_squared();

		if (std::fabs(normal_mag_sq) < std::numeric_limits<double>::epsilon())
			return *this;

		return *this - normal * (2.0 * this->dot(normal) / normal_mag_sq);
	}

	inline constexpr vec2 vec2::perpendicular() const noexcept
	{
		return vec2(-y, x);
	}

	inline bool vec2::equals(const vec2& other, double epsilon) const noexcept
	{
		return (std::fabs(x - other.x) < epsilon) && (std::fabs(y - other.y) < epsilon);
	}

	inline constexpr bool vec2::is_one_of_zero() const noexcept
	{
		return (x == 0.0 || y == 0.0);
	}

	inline constexpr bool wio::vec2::is_zero() const noexcept
	{
		return (x == 0.0 && y == 0.0);
	}

	inline bool vec2::is_nan() const noexcept
	{
		return (std::isnan(x) || std::isnan(y));
	}

	inline bool vec2::is_infinite() const noexcept
	{
		return (std::isinf(x) || std::isinf(y));
	}
}