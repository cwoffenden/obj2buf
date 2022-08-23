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
 * integer. This follows the rules for OpenGL pre-4.2, in that all of the
 * available integer range is used but zero cannot be stored exactly.
 *
 * \note Values outside of \c -1 to \c 1 will overflow (see \c #clamp()).
 *
 * \param[in] val float value to convert
 * \tparam Bits bit-depth to use (e.g. \c 8 for bytes)
 *
 * \sa https://www.khronos.org/opengl/wiki/Normalized_Integer#Alternate_mapping
 */
template<int Bits>
static inline int32_t signedPre42(float const val) {
	return static_cast<int32_t>(round((val * ((1 << Bits) - 1) - 1) / 2.0f));
}

/**
 * Helper to encode a float using the \c VertexPacker#Storage rules for OpenGL
 * pre-4.2 (see \c signedPre42()).
 *
 * \param[in] val float value to convert
 * \param[in] type storage type (and rules to follow)
 */
static int32_t storePre42(float const val, VertexPacker::Storage const type) {
	switch (type) {
	case VertexPacker::SINT08N:
		return clamp<int32_t>(signedPre42<8>(val), INT8_MIN, INT8_MAX);
	case VertexPacker::SINT08C:
		return clamp<int32_t>(int32_t(round(val)), INT8_MIN, INT8_MAX);
	case VertexPacker::UINT08N:
		return clamp<int32_t>(int32_t(round(val) * UINT8_MAX), 0, UINT8_MAX);
	case VertexPacker::UINT08C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT8_MAX);
	case VertexPacker::SINT16N:
		return clamp<int32_t>(signedPre42<16>(val), INT16_MIN, INT16_MAX);
	case VertexPacker::SINT16C:
		return clamp<int32_t>(int32_t(round(val)),  INT16_MIN, INT16_MAX);
	case VertexPacker::UINT16N:
		return clamp<int32_t>(int32_t(round(val) * UINT16_MAX), 0, UINT16_MAX);
	case VertexPacker::UINT16C:
		return clamp<int32_t>(int32_t(round(val)), 0, UINT16_MAX);
	default:
		return val;
	}
}

//*****************************************************************************/

VertexPacker::VertexPacker(uint8_t* const root, unsigned const size)
	: root(root)
	, next(root)
	, over(root + size) {}

bool VertexPacker::add(float const data, Storage const type) {
	if (hasFreeSpace(type)) {
		int32_t temp = storePre42(data, type);
		switch (type) {
		case SINT08N:
		case SINT08C:
		case UINT08N:
		case UINT08C:
			*next++ = (temp >>  0) & 0xFF;
			break;
		case SINT16N:
		case SINT16C:
		case UINT16N:
		case UINT16C:
		case FLOAT16:
			*next++ = (temp >>  0) & 0xFF;
			*next++ = (temp >>  8) & 0xFF;
			break;
		default:
			*next++ = (temp >>  0) & 0xFF;
			*next++ = (temp >>  8) & 0xFF;
			*next++ = (temp >> 16) & 0xFF;
			*next++ = (temp >> 24) & 0xFF;
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
