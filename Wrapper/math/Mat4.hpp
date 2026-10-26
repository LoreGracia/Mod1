#pragma once
#include <cmath>
#include "Vec3.hpp"

struct Mat4 {
	float m[16] = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};

	const float* data() const { return m; }
	float*       data()       { return m; }

	static Mat4 identity() { return Mat4{}; }

	static Mat4 multiply(const Mat4& a, const Mat4& b) {
		Mat4 r;
		for (int col = 0; col < 4; ++col)
			for (int row = 0; row < 4; ++row) {
				float sum = 0.f;
				for (int k = 0; k < 4; ++k)
					sum += a.m[k*4 + row] * b.m[col*4 + k];
				r.m[col*4 + row] = sum;
			}
		return r;
	}

	Mat4 operator*(const Mat4& o) const { return multiply(*this, o); }

	static Mat4 rotationX(float deg)
	{
		float r = deg * 3.14159265f / 180.f;
		float c = std::cos(r), s = std::sin(r);
		Mat4 out;
		out.m[5] = c;  out.m[6] = s;
		out.m[9] = -s; out.m[10] = c;
		return out;
	}

	static Mat4 rotationY(float deg)
	{
		float r = deg * 3.14159265f / 180.f;
		float c = std::cos(r), s = std::sin(r);
		Mat4 out;
		out.m[0] = c;  out.m[2] = -s;
		out.m[8] = s;  out.m[10] = c;
		return out;
	}

	static Mat4 rotationZ(float deg)
	{
		float r = deg * 3.14159265f / 180.f;
		float c = std::cos(r), s = std::sin(r);
		Mat4 out;
		out.m[0] = c;  out.m[1] = s;
		out.m[4] = -s; out.m[5] = c;
		return out;
	}

	static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
	{
		Vec3 f = (target - eye).normalized();
		Vec3 s = cross(f, up).normalized();
		Vec3 u = cross(s, f);

		Mat4 r;
		r.m[0]  =  s.x; r.m[4]  =  s.y; r.m[8]  =  s.z; r.m[12] = -dot(s, eye);
		r.m[1]  =  u.x; r.m[5]  =  u.y; r.m[9]  =  u.z; r.m[13] = -dot(u, eye);
		r.m[2]  = -f.x; r.m[6]  = -f.y; r.m[10] = -f.z; r.m[14] =  dot(f, eye);
		r.m[3]  =  0.f; r.m[7]  =  0.f; r.m[11] =  0.f; r.m[15] =  1.f;
		return r;
	}

	static Mat4 perspective(float fovDeg, float aspect, float nearZ, float farZ)
	{
		float f = 1.f / std::tan(fovDeg * 3.14159265f / 360.f);
		Mat4 r;
		r.m[0]  = f / aspect;
		r.m[5]  = f;
		r.m[10] = (farZ + nearZ) / (nearZ - farZ);
		r.m[11] = -1.f;
		r.m[14] = (2.f * farZ * nearZ) / (nearZ - farZ);
		r.m[15] = 0.f;
		return r;
	}
};
