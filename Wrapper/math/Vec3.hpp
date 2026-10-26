#pragma once
#include <cmath>

struct Vec3 {
	float x = 0.f, y = 0.f, z = 0.f;

	Vec3() = default;
	Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

	Vec3 operator+(const Vec3 &o) const	{ return {x+o.x, y+o.y, z+o.z}; }
	Vec3 operator-(const Vec3 &o) const	{ return {x-o.x, y-o.y, z-o.z}; }
	Vec3 operator*(float s) const		{ return {x*s, y*s, z*s}; }
	Vec3 operator-() const				{ return {-x, -y, -z}; }

	Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
	Vec3& operator-=(const Vec3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }

	float length() const { return std::sqrt(x*x + y*y + z*z); }

	Vec3 normalized() const 
	{
		float l = length();
		return (l > 1e-8f) ? Vec3{x/l, y/l, z/l} : Vec3{0.f, 0.f, 0.f};
	}
};

inline float dot(const Vec3 &a, const Vec3 &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline Vec3 cross(const Vec3 &a, const Vec3 &b)
{
	return {
		a.y*b.z - a.z*b.y,
		a.z*b.x - a.x*b.z,
		a.x*b.y - a.y*b.x
	};
}
