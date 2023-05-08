/**
 * \file vertexpacker.cpp
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#include "vertexpacker.h"

#include <cmath>
#include <algorithm>

#include <cassert>

#include "minifloat.h"

/**
 * Helper to constrain a value between upper and lower bounds.
 *
 * \param[in] val value to constrain
 * \param[in] min lower bound (inclusive)
 * \param[in] max upper bound (inclusive)
 * \return \a val constrained
 * \tparam T numeric type (e.g. \c float)
 */
template<typename T>
static inline T clamp(T const val, T const min, T const max) {
	return std::min(max, std::max(min, val));
}

/**
 * Helper to encode a signed float in the range \c -1 to \c 1 as a normalised
 * integer, following the rules for legacy OpenGL (desktop pre-4.2 and ES
 * pre-3.0) in that all of the available integer range is used but zero cannot
 * be stored exactly.
 *
 * \note See the OpenGL specification for component conversions, e.g. GL 3.0
 * Colors and Coloring table 2.10, or the ES 2.0 2.1.2 Data Conversions.
 * \note Values outside of \c -1 to \c 1 will overflow (see \c #clamp()).
 *
 * \param[in] val float value to convert
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 * \return \a val stored in the the specified number of bits
 *
 * \sa https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
 */
template<int Bits>
static inline int32_t encodeLegacy(float const val) {
	return static_cast<int32_t>(round((val * ((1 << Bits) - 1) - 1) / 2.0f));
}

/**
 * Performs the inverse of \c encodeLegacy() following the same OpenGL rules
 * (desktop pre-4.2 and ES pre-3.0).
 *
 * \note Values outside of the range of bits will overflow (see \c #clamp()) and
 * need to be accounted for in the next steps.
 *
 * \param[in] val stored value
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 * \return an approximation of the original value
 */
template<int Bits>
static inline float decodeLegacy(int32_t const val) {
	return static_cast<float>(2 * val + 1) / ((1 << Bits) - 1);
}

/**
 * Helper to encode a signed float in the range \c -1 to \c 1 as a normalised
 * integer, following the rules for modern OpenGL (desktop 4.2, ES 3.0 and WebGL
 * 2.0 onwards) preserving zero.
 *
 * \note See the OpenGL specification for component conversions, e.g. ES 3.0
 * 2.1.6.1 Conversion from Normalized Fixed-Point to Floating-Point.
 * \note Values outside of \c -1 to \c 1 will overflow (see \c #clamp()).
 *
 * \param[in] val float value to convert
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 * \return \a val stored in the the specified number of bits
 *
 * \sa https://www.khronos.org/opengl/wiki/Normalized_Integer#Signed
 */
template<int Bits>
static inline int32_t encodeModern(float const val) {
	return static_cast<int32_t>(round(val * ((1 << (Bits - 1)) - 1)));
}

/**
 * Performs the inverse of \c encodeModern() following the same OpenGL rules
 * (desktop 4.2, ES 3.0 and WebGL 2.0 onwards).
 *
 * \note Values outside of the range of bits will overflow (see \c #clamp()) and
 * need to be accounted for in the next steps.
 *
 * \param[in] val stored value
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 * \return an approximation of the original value
 */
template<int Bits>
static inline float decodeModern(int32_t const val) {
	return static_cast<float>(val) / ((1 << (Bits - 1)) - 1);
}

/**
 * Helper to encode a float using the \c VertexPacker#Storage rules. Signed
 * normalised values are encoded using separate rules, depending on whether this
 * is \e legacy OpenGL (desktop pre-4.2 and ES pre-3.0, see \c encodeLegacy())
 * or modern (desktop 4.2, ES 3.0 and WebGL 2.0 onwards, see \c encodeModern()).
 *
 * \param[in] val float value to convert
 * \param[in] type storage type (and rules to follow)
 * \param[in] legacy \c true if the old encoding rules for signed normalised values from OpenGL desktop pre-4.2 should be used
 * \return \a val stored using \a type
 */
static int32_t encode(float const val, VertexPacker::Storage const type, bool const legacy = false) {
	switch (type) {
	case VertexPacker::Storage::EXCLUDE:
		return 0;
	case VertexPacker::Storage::SINT08N:
		if (!legacy) {
			return clamp<int32_t>(encodeModern<8>(val), -INT8_MAX, INT8_MAX);
		} else {
			return clamp<int32_t>(encodeLegacy<8>(val),  INT8_MIN, INT8_MAX);
		}
	case VertexPacker::Storage::SINT08C:
		return clamp<int32_t>(int32_t(round(val)), INT8_MIN, INT8_MAX);
	case VertexPacker::Storage::UINT08N:
		return clamp<int32_t>(int32_t(round(val) * UINT8_MAX), 0, UINT8_MAX);
	case VertexPacker::Storage::UINT08C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT8_MAX);
	case VertexPacker::Storage::SINT16N:
		if (!legacy) {
			return clamp<int32_t>(encodeModern<16>(val), -INT16_MAX, INT16_MAX);
		} else {
			return clamp<int32_t>(encodeLegacy<16>(val),  INT16_MIN, INT16_MAX);
		}
	case VertexPacker::Storage::SINT16C:
		return clamp<int32_t>(int32_t(round(val)), INT16_MIN, INT16_MAX);
	case VertexPacker::Storage::UINT16N:
		return clamp<int32_t>(int32_t(round(val) * UINT16_MAX), 0, UINT16_MAX);
	case VertexPacker::Storage::UINT16C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT16_MAX);
	case VertexPacker::Storage::FLOAT16:
		return static_cast<int32_t>(utils::floatToHalf(val));
	case VertexPacker::Storage::SINT32C:
		return int32_t(clamp<long>(long(round(val)), INT32_MIN, INT32_MAX));
	case VertexPacker::Storage::UINT32C:
		return int32_t(clamp<long>(long(round(val)), 0, UINT32_MAX));
	default: {
		union {
			float   f; // where we write
			int32_t i; // where we read
		} temp = {val};
		return temp.i;
	}
	}
}

/*
 * Performs the inverse of \c encodeLegacy(float,VertexPacker::Storage),
 * extracting an encoded float from integer bits.
 */
/*static*/ float decode(int32_t const val, VertexPacker::Storage const type, bool const legacy = false) {
	switch (type) {
	case VertexPacker::Storage::EXCLUDE:
		return 0.0f;
	case VertexPacker::Storage::SINT08N:
		if (!legacy) {
			return decodeModern<8>(clamp<int32_t>(val, -INT8_MAX, INT8_MAX));
		} else {
			return decodeLegacy<8>(clamp<int32_t>(val,  INT8_MIN, INT8_MAX));
		}
	case VertexPacker::Storage::SINT08C:
		return static_cast<float>(clamp<int32_t>(val, INT8_MIN, INT8_MAX));
	case VertexPacker::Storage::UINT08N:
		return static_cast<float>(clamp<int32_t>(val, 0, UINT8_MAX)) / UINT8_MAX;
	case VertexPacker::Storage::UINT08C:
		return static_cast<float>(clamp<int32_t>(val, 0, UINT8_MAX));
	case VertexPacker::Storage::SINT16N:
		if (!legacy) {
			return decodeModern<16>(clamp<int32_t>(val, -INT16_MAX, INT16_MAX));
		} else {
			return decodeLegacy<16>(clamp<int32_t>(val,  INT16_MIN, INT16_MAX));
		}
	case VertexPacker::Storage::SINT16C:
		return static_cast<float>(clamp<int32_t>(val, INT16_MIN, INT16_MAX));
	case VertexPacker::Storage::UINT16N:
		return static_cast<float>(clamp<int32_t>(val, 0, UINT16_MAX)) / UINT16_MAX;
	case VertexPacker::Storage::UINT16C:
		return static_cast<float>(clamp<int32_t>(val, 0, UINT16_MAX));
	case VertexPacker::Storage::FLOAT16:
		return utils::halfToFloat(static_cast<utils::float16>(val));
	default:
		return 0.0f;
	}
}

/**
 * Helper to bypass encoding for \e clamped 8- and 16-bit integers and pass them
 * as-is. All other types are converted to floats and processed through \c
 * #encode(float,VertexPacker::Storage,bool).
 *
 * \note This hasn't had any testing on systems with large integer types (from
 * memory, MipsPro n64 comes to mind, but it's doubtful this even compiles on
 * there).
 *
 * \param[in] val integer value to convert
 * \param[in] type storage type (and rules to follow)
 * \param[in] legacy \c true if the old encoding rules for signed normalised values from OpenGL desktop pre-4.2 should be used
 * \return \a val stored using \a type
 */
static int32_t encode(int const val, VertexPacker::Storage const type, bool const legacy = false) {
	switch (type) {
	case VertexPacker::Storage::SINT08C:
		return clamp<int32_t>(val, INT8_MIN, INT8_MAX);
	case VertexPacker::Storage::UINT08C:
		return clamp<int32_t>(val, 0, UINT8_MAX);
	case VertexPacker::Storage::SINT16C:
		return clamp<int32_t>(val, INT16_MIN, INT16_MAX);
	case VertexPacker::Storage::UINT16C:
		return clamp<int32_t>(val, 0, UINT16_MAX);
	case VertexPacker::Storage::SINT32C:
		return clamp<int32_t>(val, INT32_MIN, INT32_MAX);
	case VertexPacker::Storage::UINT32C:
		return static_cast<int32_t>(clamp<uint32_t>(val, 0U, UINT32_MAX));
	default:
		return encode(static_cast<float>(val), type, legacy);
	}
}

void runTests() {
	for (int32_t n = INT8_MIN; n < INT8_MAX; n++) {
		float   f = decode(n, VertexPacker::Storage::SINT08N, true);
		assert(f >= -1.0 && f <= 1.0);
		int32_t i = encode(f, VertexPacker::Storage::SINT08N, true);
		assert(n == i);
		(void) i;
	}
	for (int32_t n = INT8_MIN; n < INT8_MAX; n++) {
		float   f = decode(n, VertexPacker::Storage::SINT08N, false);
		assert(f >= -1.0 && f <= 1.0);
		int32_t i = encode(f, VertexPacker::Storage::SINT08N, false);
		if (n == INT8_MIN) {
			assert(i == -INT8_MAX);
		} else {
			assert(n == i);
		}
		(void) i;
	}
	for (int32_t n = INT16_MIN; n < INT16_MAX; n++) {
		float   f = decode(n, VertexPacker::Storage::SINT16N, true);
		assert(f >= -1.0 && f <= 1.0);
		int32_t i = encode(f, VertexPacker::Storage::SINT16N, true);
		assert(n == i);
		(void) i;
	}
	for (int32_t n = INT16_MIN; n < INT16_MAX; n++) {
		float   f = decode(n, VertexPacker::Storage::SINT16N, false);
		assert(f >= -1.0 && f <= 1.0);
		int32_t i = encode(f, VertexPacker::Storage::SINT16N, false);
		if (n == INT16_MIN) {
			assert(i == -INT16_MAX);
		} else {
			assert(n == i);
		}
		(void) i;
	}
}

//*****************************************************************************/

VertexPacker::VertexPacker(void* const root, size_t const size, unsigned const opts)
	: root(static_cast<uint8_t*>(root))
	, next(static_cast<uint8_t*>(root))
	, over(static_cast<uint8_t*>(root) + size)
	, opts(opts) {}


size_t VertexPacker::size() const {
	return static_cast<size_t>(next - root);
}

VertexPacker::Failed VertexPacker::add(float const data, Storage const type) {
	if (hasFreeSpace(type)) {
		if (type) {
			int32_t bits = encode(data, type, (opts & OPTS_SIGNED_LEGACY) != 0);
			switch (type.bytes()) {
			case 1:
				*next++ = bits & 0xFF;
				break;
			case 2:
				if ((opts & OPTS_BIG_ENDIAN) == 0) {
					*next++ = (bits >>  0) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
				} else {
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >>  0) & 0xFF;
				}
				break;
			default:
				if ((opts & OPTS_BIG_ENDIAN) == 0) {
					*next++ = (bits >>  0) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >> 16) & 0xFF;
					*next++ = (bits >> 24) & 0xFF;
				} else {
					*next++ = (bits >> 24) & 0xFF;
					*next++ = (bits >> 16) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >>  0) & 0xFF;
				}
			}
		}
		return VP_SUCCEEDED;
	}
	return VP_FAILED;
}

VertexPacker::Failed VertexPacker::add(int const data, Storage const type) {
	if (hasFreeSpace(type)) {
		if (type) {
			int32_t bits;
			switch (type) {
			case Storage::SINT08C:
			case Storage::UINT08C:
			case Storage::SINT16C:
			case Storage::UINT16C:
			case Storage::SINT32C:
			case Storage::UINT32C:
				bits = encode(data, type, (opts & OPTS_SIGNED_LEGACY) != 0);
				break;
			default:
				/*
				 * For anything other than integer clamped types we treat the
				 * value as a float.
				 */
				return add(static_cast<float>(data), type);
			}
			switch (type.bytes()) {
			case 1:
				*next++ = bits & 0xFF;
				break;
			case 2:
				if ((opts & OPTS_BIG_ENDIAN) == 0) {
					*next++ = (bits >>  0) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
				} else {
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >>  0) & 0xFF;
				}
				break;
			default:
				if ((opts & OPTS_BIG_ENDIAN) == 0) {
					*next++ = (bits >>  0) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >> 16) & 0xFF;
					*next++ = (bits >> 24) & 0xFF;
				} else {
					*next++ = (bits >> 24) & 0xFF;
					*next++ = (bits >> 16) & 0xFF;
					*next++ = (bits >>  8) & 0xFF;
					*next++ = (bits >>  0) & 0xFF;
				}
			}
		}
		return VP_SUCCEEDED;
	}
	return VP_FAILED;
}

VertexPacker::Failed VertexPacker::align(size_t const base) {
	size_t used = size();
	if (used >= base) {
		used -= base;
	}
	if (unsigned bytes = static_cast<unsigned>(used) & 3) {
		bytes = 4 - bytes;
		if (next + bytes <= over) {
			for (unsigned n = 0; n < bytes; n++) {
				*next++ = 0;
			}
		} else {
			return VP_FAILED;
		}
	}
	return VP_SUCCEEDED;
}

void VertexPacker::rewind() {
	next = root;
}

bool VertexPacker::hasFreeSpace(Storage const type) const {
	return next + type.bytes() <= over;
}
