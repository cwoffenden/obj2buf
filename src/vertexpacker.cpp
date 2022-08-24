#include "vertexpacker.h"

#include <cmath>
#include <algorithm>

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
 * Helper to store a signed float in the range \c -1 to \c 1 as a normalised
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
 *
 * \sa https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
 */
template<int Bits>
static inline int32_t signedLegacy(float const val) {
	return static_cast<int32_t>(round((val * ((1 << Bits) - 1) - 1) / 2.0f));
}

/**
 * Helper to store a signed float in the range \c -1 to \c 1 as a normalised
 * integer, following the rules for modern OpenGL (desktop 4.2, ES 3.0 and WebGL
 * 2.0 onwards) preserving zero.
 *
 * \note See the OpenGL specification for component conversions, e.g. ES 3.0
 * 2.1.6.1 Conversion from Normalized Fixed-Point to Floating-Point.
 * \note Values outside of \c -1 to \c 1 will overflow (see \c #clamp()).
 *
 * \param[in] val float value to convert
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 *
 * \sa https://www.khronos.org/opengl/wiki/Normalized_Integer#Signed
 */
template<int Bits>
static inline int32_t signedModern(float const val) {
	return static_cast<int32_t>(round(val * ((1 << (Bits - 1)) - 1)));
}

/**
 * Helper to encode a float using the \c VertexPacker#Storage rules for legacy
 * OpenGL (desktop pre-4.2 and ES pre-3.0, see \c signedLegacy()).
 *
 * \param[in] val float value to convert
 * \param[in] type storage type (and rules to follow)
 */
static int32_t storeLegacy(float const val, VertexPacker::Storage const type) {
	switch (type) {
	case VertexPacker::SINT08N:
		return clamp<int32_t>(signedLegacy<8>(val), INT8_MIN, INT8_MAX);
	case VertexPacker::SINT08C:
		return clamp<int32_t>(int32_t(round(val)), INT8_MIN, INT8_MAX);
	case VertexPacker::UINT08N:
		return clamp<int32_t>(int32_t(round(val) * UINT8_MAX), 0, UINT8_MAX);
	case VertexPacker::UINT08C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT8_MAX);
	case VertexPacker::SINT16N:
		return clamp<int32_t>(signedLegacy<16>(val), INT16_MIN, INT16_MAX);
	case VertexPacker::SINT16C:
		return clamp<int32_t>(int32_t(round(val)), INT16_MIN, INT16_MAX);
	case VertexPacker::UINT16N:
		return clamp<int32_t>(int32_t(round(val) * UINT16_MAX), 0, UINT16_MAX);
	case VertexPacker::UINT16C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT16_MAX);
	default: {
		union {
			float   f; // where we write
			int32_t i; // where we read
		} temp = {val};
		return temp.i;
	}
	}
}

/**
 * Helper to encode a float using the \c VertexPacker#Storage rules for modern
 * OpenGL (desktop 4.2 and ES 3.0 onwards, see \c signedModern()).
 *
 * \param[in] val float value to convert
 * \param[in] type storage type (and rules to follow)
 */
static int32_t storeModern(float const val, VertexPacker::Storage const type) {
	switch (type) {
	case VertexPacker::SINT08N:
		return clamp<int32_t>(signedModern<8>(val), -INT8_MAX, INT8_MAX);
	case VertexPacker::SINT08C:
		return clamp<int32_t>(int32_t(round(val)), INT8_MIN, INT8_MAX);
	case VertexPacker::UINT08N:
		return clamp<int32_t>(int32_t(round(val) * UINT8_MAX), 0, UINT8_MAX);
	case VertexPacker::UINT08C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT8_MAX);
	case VertexPacker::SINT16N:
		return clamp<int32_t>(signedModern<16>(val), -INT16_MAX, INT16_MAX);
	case VertexPacker::SINT16C:
		return clamp<int32_t>(int32_t(round(val)), INT16_MIN, INT16_MAX);
	case VertexPacker::UINT16N:
		return clamp<int32_t>(int32_t(round(val) * UINT16_MAX), 0, UINT16_MAX);
	case VertexPacker::UINT16C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT16_MAX);
	default: {
		union {
			float   f; // where we write
			int32_t i; // where we read
		} temp = {val};
		return temp.i;
	}
	}
}

//*****************************************************************************/

VertexPacker::VertexPacker(void* const root, unsigned const size, unsigned const opts)
	: root(static_cast<uint8_t*>(root))
	, next(static_cast<uint8_t*>(root))
	, over(static_cast<uint8_t*>(root) + size)
	, opts(opts) {}

bool VertexPacker::add(float const data, Storage const type) {
	if (hasFreeSpace(type)) {
		int32_t temp;
		if ((opts & OPTS_SIGNED_LEGACY) == 0) {
			temp = storeModern(data, type);
		} else {
			temp = storeLegacy(data, type);
		}
		switch (type) {
		case SINT08N:
		case SINT08C:
		case UINT08N:
		case UINT08C:
			*next++ = temp & 0xFF;
			break;
		case SINT16N:
		case SINT16C:
		case UINT16N:
		case UINT16C:
		case FLOAT16:
			if ((opts & OPTS_BIG_ENDIAN) == 0) {
				*next++ = (temp >>  0) & 0xFF;
				*next++ = (temp >>  8) & 0xFF;
			} else {
				*next++ = (temp >>  8) & 0xFF;
				*next++ = (temp >>  0) & 0xFF;
			}
			break;
		default:
			if ((opts & OPTS_BIG_ENDIAN) == 0) {
				*next++ = (temp >>  0) & 0xFF;
				*next++ = (temp >>  8) & 0xFF;
				*next++ = (temp >> 16) & 0xFF;
				*next++ = (temp >> 24) & 0xFF;
			} else {
				*next++ = (temp >> 24) & 0xFF;
				*next++ = (temp >> 16) & 0xFF;
				*next++ = (temp >>  8) & 0xFF;
				*next++ = (temp >>  0) & 0xFF;
			}
		}
		return true;
	}
	return false;
}

void VertexPacker::rewind() {
	next = root;
}

bool VertexPacker::hasFreeSpace(Storage const type) const {
	switch (type) {
	case SINT08N:
	case SINT08C:
	case UINT08N:
	case UINT08C:
		return next + 1 <= over;
	case SINT16N:
	case SINT16C:
	case UINT16N:
	case UINT16C:
	case FLOAT16:
		return next + 2 <= over;
	default:
		return next + 4 <= over;
	}
}
