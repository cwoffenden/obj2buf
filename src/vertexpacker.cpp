#include "vertexpacker.h"

#include <cmath>
#include <algorithm>

using std::max;
using std::min;

VertexPacker::VertexPacker(uint8_t* const root, unsigned const size)
	: root(root)
	, next(root)
	, over(root + size) {}

bool VertexPacker::add(float data, Storage const type) {
	if (canFit(type)) {
		int32_t temp;
		switch (type) {
		case SINT08N:
			/*
			 * Note: we're using some made-up normalisation rule which, whilst the same
			 * code has been in use for years, is incorrect. Investigate:
			 *
			 * https://www.khronos.org/opengl/wiki/Normalized_Integer#Signed
			 */
			data *= ((data < 0.0f) ? 0x0080 : 0x007F);
			// fall through
		case SINT08C:
			temp = min(0x007F, max(static_cast<int32_t>(round(data)), -0x0080));
			*next++ = temp & 0xFF;
			break;
		case UINT08N:
			data *= 0xFF;
			// fall through
		case UINT08C:
			temp = min(0x00FF, max(static_cast<int32_t>(round(data)),  0x0000));
			*next++ = temp & 0xFF;
			break;
		case SINT16N:
			data *= ((data < 0.0f) ? 0x8000 : 0x7FFF);
			// fall through
		case SINT16C:
			temp = min(0x7FFF, max(static_cast<int32_t>(round(data)), -0x8000));
			*next++ = (temp >> 0) & 0xFF;
			*next++ = (temp >> 8) & 0xFF;
			break;
		default:
			break;
		}
		return true;
	}
	return false;
}

bool VertexPacker::canFit(Storage const type) const {
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
