/**
 * \file minifloat.h
 * Utilities for converting to and from floating point values with fewer bits
 * than a regular single-precision float.
 *
 * \sa https://en.wikipedia.org/wiki/Minifloat
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#pragma once

#include <cstdint>

/**
 * \def FLT16_EPSILON
 * \n
 * Calculated as 2^-10.
 */
#ifndef FLT16_EPSILON
#define FLT16_EPSILON 9.765625e-4f
#endif

/**
 * \def FLT16_MAX
 * Maximum finite representable half-precision float.
 * \n
 * Calculated as (2 - 2^-10) * 2^15.
 */
#ifndef FLT16_MAX
#define FLT16_MAX 65504.0f
#endif

/**
 * \def FLT16_MIN
 * Minimum representable positive half-precision float.
 * \n
 * Calculated as 2^-14.
 */
#ifndef FLT16_MIN
#define FLT16_MIN 6.103515625e-5f
#endif

/**
 * \def FLT16_TRUE_MIN
 * Minimum \e subnormal representable positive half-precision float.
 * \n
 * Calculated as 2^-24.
 */
#ifndef FLT16_TRUE_MIN
#define FLT16_TRUE_MIN 5.9604645e-8f
#endif

namespace utils {
	//@{
	/**
	 * Type capable of representing a half-precision float, compatible with IEEE
	 * 754-2008 \c binary16, OpenGL \c GL_HALF_FLOAT, DirectX \e packed \c HALF,
	 * etc.
	 *
	 * \note The data type is only for \e containing a half-float, not
	 * performing maths operations on. It exists to simplify conversions and
	 * storage, and all operations should be performed on single-precision
	 * representations.
	 */
	typedef uint16_t float16;
	//@}

	//@{
	/**
	 * Converts a single-precision float to half-precision (noting the
	 * conversion limitations with respect to \c FLT16_MIN, \c FLT16_MAX, the
	 * variable precision, etc.).
	 *
	 * \param[in] val single-precision float
	 * \return equivalent half-precision float
	 */
	float16 floatToHalf(float const val);

	/**
	 * \copydoc #floatToHalf(float)
	 *
	 * \tparam T value type (should be inferred)
	 * \param[in] val value to convert
	 */
	template<typename T>
	inline
	float16 floatToHalf(T const val) {
		return floatToHalf(static_cast<float>(val));
	}

	/**
	 * Converts a half-precision float to single-precision.
	 *
	 * \param[in] val half-precision float
	 * \return equivalent single-precision float
	 */
	float halfToFloat(float16 const val);

	/**
	 * \copydoc #halfToFloat(float16)
	 *
	 * \tparam T value type (should be inferred)
	 * \param[in] val value to convert
	 */
	template<typename T>
	inline
	float halfToFloat(T const val) {
		return halfToFloat(static_cast<float16>(val));
	}
	//@}

	//@{
	/**
	 * Tests whether \a val is a \e NaN (not-a-number), e.g. \c 0/0.
	 *
	 * \param[in] val half-precision float to test
	 * \return \c true if \a val is an undefined other otherwise non-representable value
	 */
	inline
	bool halfIsNaN(float16 const val) {
		return (val & 0x7FFF) != 0x7C00
			&& (val & 0x7C00) == 0x7C00;
	}

	/**
	 * Tests whether \a val is infinite (which given \c FLT16_MAX is easier to
	 * reach than with single- or double-precision floats).
	 *
	 * \param[in] val half-precision float to test
	 * \return \c true if \a val represents an infinite value (positive or negative)
	 */
	inline
	bool halfIsInf(float16 const val) {
		return (val & 0x7FFF) == 0x7C00;
	}
	//@}
};

//*****************************************************************************/

#if __cplusplus >= 201103L
namespace std {
/**
 * Overloaded \c cmath \c std::isnan implementation for half-precision floats.
 *
 * \param[in] val half-precision float to test
 * \return \c true if \a val is an undefined other otherwise non-representable value
 */
inline
bool isnan(utils::float16 const val) {
	return utils::halfIsNaN(val);
}
/**
 * Overloaded \c cmath \c std::isinf implementation for half-precision floats.
 *
 * \param[in] val half-precision float to test
 * \return \c true if \a val represents an infinite value (positive or negative)
 */
inline
bool isinf(utils::float16 const val) {
	return utils::halfIsInf(val);
}
};
#endif
