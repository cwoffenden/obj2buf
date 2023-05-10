/**
 * \file vec.h
 * Very basic 2-, 3- and 4-component vectors.
 */
#pragma once

#include <cmath>

/**
 * \def M_PI
 * Missing POSIX definition (for MSVC/Win32).
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#include "vertexpacker.h"

/**
 * \def NO_DISCARD
 * Attribute to verify that a function return is used.
 *
 * \note In the current CMake build these are only enabled in release.
 */
#ifndef NO_DISCARD
#if __cplusplus >= 201703L
#define NO_DISCARD [[nodiscard]]
#else
#if defined(__GNUC__) || defined(__llvm__)
#define NO_DISCARD __attribute__((warn_unused_result))
#else
#define NO_DISCARD
#endif
#endif
#endif

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
#define VEC3_SIMPLE_OPERATOR_WITH_VECTOR(op) NO_DISCARD Vec3 operator op(const Vec3& vec) const {return Vec3(x op vec.x, y op vec.y, z op vec.z);}
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
#define VEC3_SIMPLE_OPERATOR_WITH_SCALAR(op) NO_DISCARD Vec3 operator op(T   const   val) const {return Vec3(x op val,   y op val,   z op val  );}
#endif

/**
 * \def VEC2_SIMPLE_OPERATOR_WITH_VECTOR
 * Helper to emit code for the simple operators such as \c +, \c -, etc., for
 * the \c vec2 type with a vector parameter. For example, with a \c + as its
 * parameter the macro will output:
 * \code
 *	Vec2 operator +(const Vec2& vec) const {
 *		return Vec2(
 *			x + vec.x,
 *			y + vec.y
 *		);
 *	}
 * \endcode
 */
#ifndef VEC2_SIMPLE_OPERATOR_WITH_VECTOR
#define VEC2_SIMPLE_OPERATOR_WITH_VECTOR(op) NO_DISCARD Vec2 operator op(const Vec2& vec) const {return Vec2(x op vec.x, y op vec.y);}
#endif

/**
 * \def VEC2_SIMPLE_OPERATOR_WITH_VECTOR
 * Helper to emit code for the simple operators such as \c +, \c -, etc., for
 * the \c vec2 type with a scalar parameter. For example, with a \c + as its
 * parameter the macro will output:
 * \code
 *	Vec2 operator +(T const val) const {
 *		return Vec2(
 *			x + val,
 *			y + val
 *		);
 *	}
 * \endcode
 */
#ifndef VEC2_SIMPLE_OPERATOR_WITH_SCALAR
#define VEC2_SIMPLE_OPERATOR_WITH_SCALAR(op) NO_DISCARD Vec2 operator op(T   const   val) const {return Vec2(x op val,   y op val  );}
#endif

template<typename T>
struct Vec2
{
	T x;
	T y;
    Vec2() {}
	Vec2(T const x, T const y)
		: x(x)
		, y(y) {}
	Vec2& operator =(T const val) {
		x = val;
		y = val;
		return *this;
	}
	operator T*() {
		return &x;
	}
	VEC2_SIMPLE_OPERATOR_WITH_VECTOR(+)
	VEC2_SIMPLE_OPERATOR_WITH_VECTOR(-)
	VEC2_SIMPLE_OPERATOR_WITH_VECTOR(*)
	VEC2_SIMPLE_OPERATOR_WITH_VECTOR(/)
	VEC2_SIMPLE_OPERATOR_WITH_SCALAR(*)
	VEC2_SIMPLE_OPERATOR_WITH_SCALAR(/)
	/**
	 * Adds this vector to a buffer.
	 *
	 * \param[in] dest vertex packer wrapping the destination buffer
	 * \param[in] type conversion and byte storage
	 * \return \c VP_FAILED if adding failed (e.g. if no more storage space is available)
	 */
	VertexPacker::Failed store(VertexPacker& dest, VertexPacker::Storage const type) const {
		VertexPacker::Failed failed = false;
		failed |= dest.add(x, type);
		failed |= dest.add(y, type);
		return failed;
	}
	/**
	 * Dot product.
	 */
	NO_DISCARD
	static T dot(const Vec2& a, const Vec2& b) {
		return a.x * b.x
			 + a.y * b.y;
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
	Vec3& operator =(T const val) {
		x = val;
		y = val;
		z = val;
		return *this;
	}
	operator T*() {
		return &x;
	}
	/**
	 * Getter for the \c x and \c y components.
	 *
	 * \return a new 2-component vector
	 */
	NO_DISCARD
	Vec2<T> xy() const {
		return Vec2<T>(x, y);
	}
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(+)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(-)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(*)
	VEC3_SIMPLE_OPERATOR_WITH_VECTOR(/)
	VEC3_SIMPLE_OPERATOR_WITH_SCALAR(*)
	VEC3_SIMPLE_OPERATOR_WITH_SCALAR(/)
	/**
	 * Vector length.
	 */
	NO_DISCARD
	T len() const {
		return static_cast<T>(std::sqrt(x * x + y * y + z * z));
	}
	/**
	 * Return a normalised \e copy of this vector.
	 *
	 * \note Normalising a zero vector returns a new zero vector (and not a
	 * vector of \c NaN as may be expected) which allows this template to be
	 * used with integer types too.
	 *
	 * \return normalised vector
	 */
	NO_DISCARD
	Vec3 normalize() const {
		T l = len();
		if (l > T(0)) {
			return Vec3(
				x / l,
				y / l,
				z / l
			);
		}
		return Vec3(
			T(0),
			T(0),
			T(0)
		);
	}
	/**
	 * \copydoc Vec2::store()
	 */
	VertexPacker::Failed store(VertexPacker& dest, VertexPacker::Storage const type) const {
		VertexPacker::Failed failed = false;
		failed |= dest.add(x, type);
		failed |= dest.add(y, type);
		failed |= dest.add(z, type);
		return failed;
	}
	/**
	 * Dot product.
	 */
	static T dot(const Vec3& a, const Vec3& b) {
		return a.x * b.x
			 + a.y * b.y
			 + a.z * b.z;
	}
	/**
	 * Cross product.
	 */
	NO_DISCARD
	static Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
		return Vec3(
			lhs.y * rhs.z - lhs.z - rhs.y,
			lhs.z * rhs.x - lhs.x - rhs.z,
			lhs.x * rhs.y - lhs.y - rhs.x
		);
	}
	/**
	 * Component-wise minimum of two vectors.
	 */
	NO_DISCARD
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
	NO_DISCARD
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
	Vec4& operator =(T const val) {
		x = val;
		y = val;
		z = val;
		w = val;
		return *this;
	}
	operator T*() {
		return &x;
	}
	/**
	 * Getter for the \c x, \c y and \c z components.
	 *
	 * \return a new 3-component vector
	 */
	NO_DISCARD
	Vec3<T> xyz() const {
		return Vec3<T>(x, y, z);
	}
	/**
	 * \copydoc Vec2::store()
	 */
	VertexPacker::Failed store(VertexPacker& dest, VertexPacker::Storage const type) const {
		VertexPacker::Failed failed = false;
		failed |= dest.add(x, type);
		failed |= dest.add(y, type);
		failed |= dest.add(z, type);
		failed |= dest.add(w, type);
		return failed;
	}
};

/**
 * 3x3 matrix. It exists primarily to perform axis conversion (so is missing
 * most features).
 */
template<typename T>
struct Mat3
{
	Vec3<T> m[3];
	/**
	 * Creates an identity matrix.
	 */
	explicit Mat3() {
		m[0] = Vec3<T>(T(1), T(0), T(0));
		m[1] = Vec3<T>(T(0), T(1), T(0));
		m[2] = Vec3<T>(T(0), T(0), T(1));
	}
	operator T*() {
		return m;
	}
	/**
	 * Sets the matrix from an angle rotating around the supplied vector,
	 * overwriting any existing values.
	 *
	 * \note No test is performed to ensure \a x, \a y and \a z form a unit
	 * vector.
	 *
	 * \param[in] a rotation in radians (following the \e right-hand rule)
	 * \param[in] x x-axis coordinate of the vector around which the rotation occurs
	 * \param[in] y y-axis coordinate of the vector around which the rotation occurs
	 * \param[in] z z-axis coordinate of the vector around which the rotation occurs
	 */
	void set(T const a, T const x, T const y, T const z) {
		T const cosA(std::cos(a));
		T const sinA(std::sin(a));
		T const omc (T(1) - cosA);
		T const omcX(omc * x);
		T const omcY(omc * y);
		T const omcZ(omc * z);
		m[0][0] = omcX * x +     cosA;	// xx * (1 - cos) +     cos
		m[0][1] = omcX * y + z * sinA;	// yx * (1 - cos) + z * sin
		m[0][2] = omcX * z - y * sinA;	// xz * (1 - cos) - y * sin
		m[1][0] = omcY * x - z * sinA;	// xy * (1 - cos) - z * sin
		m[1][1] = omcY * y +     cosA;	// yy * (1 - cos) +     cos
		m[1][2] = omcY * z + x * sinA;	// yz * (1 - cos) + x * sin
		m[2][0] = omcZ * x + y * sinA;	// xz * (1 - cos) + y * sin
		m[2][1] = omcZ * y - x * sinA;	// yz * (1 - cos) - x * sin
		m[2][2] = omcZ * z +     cosA;	// zz * (1 - cos) +     cos
	}
	/**
	 * Transforms the supplied vector by this matrix.
	 */
	NO_DISCARD
	Vec3<T> apply(Vec3<T> vec) const {
		return Vec3<T>(
			Vec3<T>::dot(m[0], vec),
			Vec3<T>::dot(m[1], vec),
			Vec3<T>::dot(m[2], vec)
		);
	}
};

typedef Vec2<float> vec2;
typedef Vec3<float> vec3;
typedef Vec4<float> vec4;
typedef Mat3<float> mat3;
