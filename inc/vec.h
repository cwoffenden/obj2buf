/**
 * \file vec.h
 * Very basic 2-, 3- and 4-component vectors.
 */
#pragma once

/**
 * \def VEC3_SIMPLE_OPERATOR_WITH_VECTOR
 * Helper to emit code for the simple operators such as \c +, \c -, etc., for
 * the \c vec3 type with a vector parameter. For example, with a \c + as its
 * parameter the macro will output:
 * \code
 *	Vec3 operator +(const Vec3& vec) const {
 *		return Vec3(
 *			x + vec.x,
 *			y + vec.y,
 *			z + vec.z
 *		);
 *	}
 * \endcode
 */
#ifndef VEC3_SIMPLE_OPERATOR_WITH_VECTOR
#define VEC3_SIMPLE_OPERATOR_WITH_VECTOR(op) Vec3 operator op(const Vec3& vec) const {return Vec3(x op vec.x, y op vec.y, z op vec.z);}
#endif

/**
 * \def VEC3_SIMPLE_OPERATOR_WITH_VECTOR
 * Helper to emit code for the simple operators such as \c +, \c -, etc., for
 * the \c vec3 type with a scalar parameter. For example, with a \c + as its
 * parameter the macro will output:
 * \code
 *	Vec3 operator +(T const val) const {
 *		return Vec3(
 *			x + val,
 *			y + val,
 *			z + val
 *		);
 *	}
 * \endcode
 */
#ifndef VEC3_SIMPLE_OPERATOR_WITH_SCALAR
#define VEC3_SIMPLE_OPERATOR_WITH_SCALAR(op) Vec3 operator op(T   const   val) const {return Vec3(x op val,   y op val,   z op val  );}
#endif

template<typename T>
struct Vec2
{
	T x;
	T y;
    Vec2() {}
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
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(+)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(-)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(*)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(/)
	VEC3_SIMPLE_OPERATOR_WITH_SCALAR(*)
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
