/**
 * \file vec.h
 * Very basic 2-, 3- and 4-component vectors.
 */
#pragma once

template<typename T>
struct Vec2
{
	T x;
	T y;
	operator T* () {
		return &x;
	}
};

template<typename T>
struct Vec3
{
	T x;
	T y;
	T z;
	Vec3() {}
	Vec3(T const x, T const y, T const z = T(0))
		: x(x)
		, y(y)
		, z(z) {}
	operator T* () {
		return &x;
	}
	Vec3 operator +(const Vec3& vec) const {
		return Vec3(
			x + vec.x,
			y + vec.y,
			z + vec.z
		);
	}
	Vec3 operator -(const Vec3& vec) const {
		return Vec3(
			x - vec.x,
			y - vec.y,
			z - vec.z
		);
	}
	Vec3 operator *(const Vec3& vec) const {
		return Vec3(
			x * vec.x,
			y * vec.y,
			z * vec.z
		);
	}
	Vec3 operator *(T const val) const {
		return Vec3(
			x * val,
			y * val,
			z * val
		);
	}
	Vec3 operator /(const Vec3& vec) const {
		return Vec3(
			x / vec.x,
			y / vec.y,
			z / vec.z
		);
	}
	/**
	 * Component-wise minimum of two vectors.
	 */
	static Vec3 min(const Vec3& a, const Vec3& b) {
		return Vec3(
			a.x < b.x ? a.x : b.x,
			a.y < b.y ? a.y : b.y,
			a.z < b.z ? a.z : b.z
		);
	}
	/**
	 * Component-wise maximum of two vectors.
	 */
	static Vec3 max(const Vec3& a, const Vec3& b) {
		return Vec3(
			a.x > b.x ? a.x : b.x,
			a.y > b.y ? a.y : b.y,
			a.z > b.z ? a.z : b.z
		);
	}
};

template<typename T>
struct Vec4
{
	T x;
	T y;
	T z;
	T w;
	operator T* () {
		return &x;
	}
};

typedef Vec2<float> vec2;
typedef Vec3<float> vec3;
typedef Vec4<float> vec4;
