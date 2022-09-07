#include "minifloat.h"

/**
 * \def HAS_BUILTIN_FLOAT16
 * The original code has multiple CPU specific versions for float16, with only
 * the fallback path remaining. Here we try to determine whether the compiler
 * has builtin support for the standard _Float16 storage type, in which case we
 * use that.
 *
 * \note Testing on a Mac M1 with Clang shows the builtin version to be faster
 * in debug but not in release (10% slower in release). Still, we take the
 * builtin since testing was on one device and, in theory, the compiler should
 * be using FC16 and other hardware instructions to enable 16-bit floats. To be
 * confirmed.
 */
#define __STDC_WANT_IEC_60559_TYPES_EXT__
#include <cfloat>
#ifdef FLT16_DIG
#define HAS_BUILTIN_FLOAT16
static_assert(sizeof(_Float16) == sizeof(utils::float16), "Compiler's builtin _Float16 size mismatch");
#else
namespace impl {
/**
 * Helper to convert single-precision floats to IEEE 754 half-precision.
 * \n
 * Based on Jeroen van der Zijp's "Fast Half Float Conversions" paper.
 *
 * \note Requires 1536 bytes of one-time generated tables.
 *
 * \sa ftp://ftp.fox-toolkit.org/pub/fasthalffloatconversion.pdf
 */
class FloatToHalfConv
{
public:
	/**
	 * Creates the look-up tables for the conversion \e to half-precision.
	 * \n
	 * This should run once and be stored.
	 *
	 * \note Most of the shifts are all one less than in the original paper, the
	 * exceptions being for truncating to zero and pushing to infinity (since we
	 * don't want the additional rounding \c 1 adding).
	 */
	FloatToHalfConv() {
		for (int n = 0, e = -127; n < 256; n++, e++) {
			if (e < -24) {
				/*
				 * Very small numbers are truncated to +/- 0 (zeroing the
				 * mantissa bits by shifting them beyond the supported range).
				 */
				base[n | 0x000] = 0x0000;
				base[n | 0x100] = 0x8000;
				shft[n | 0x000] =
				shft[n | 0x100] = 24;
			} else {
				if (e < -14) {
					/*
					 * Small numbers, with exponents ranging [2^-24, 2^-14), are
					 * mapped to subnormal half-floats.
					 */
					base[n | 0x100] = (base[n] = (0x0400 >> (-e - 14))) | 0x8000;
					shft[n | 0x000] =
					shft[n | 0x100] = static_cast<uint8_t>(-e - 2);
				} else {
					if (e <= 15) {
						/*
						 * Normal numbers, having exponents [2^-14, 2^15], are
						 * mapped to normal half-floats.
						 */
						base[n | 0x100] = (base[n] = static_cast<uint16_t>((e + 15) << 10)) | 0x8000;
						shft[n | 0x000] =
						shft[n | 0x100] = 12;
					} else {
						if (e < 128) {
							/*
							 * Large floats are mapped to infinity (zeroing the
							 * mantissa bits by shifting them beyond the
							 * supported range).
							 */
							base[n | 0x000] = 0x7C00;
							base[n | 0x100] = 0xFC00;
							shft[n | 0x000] =
							shft[n | 0x100] = 24;
						} else {
							/*
							 * Keep infinity and NaNs (the remaining floats),
							 * preserving as many mantissa bits as possible.
							 *
							 * TODO: the edge-case here is that some of the NaNs
							 * get slightly changed, but since all the signaling
							 * NaNs are changed anyway it can be ignored.
							 */
							base[n | 0x000] = 0x7C00;
							base[n | 0x100] = 0xFC00;
							shft[n | 0x000] =
							shft[n | 0x100] = 12;
						}
					}
				}
			}
		}
	}

	/**
	 * Converts a single-precision float to half-precision (noting the limitations
	 * of the conversion with respect to the precision, etc.).
	 *
	 * \param[in] val single-precision float
	 * \return equivalent half-precision float
	 */
	inline
	utils::float16 convert(float const val) const {
		/*
		 * Some notes:
		 *
		 * Probing Clang's implementation of casting float to _Float16 shows that with
		 * the default 'round to nearest' it follows the IEEE spec precisely, in that
		 * numerical ties are further resolved by rounding to the nearest even (in the
		 * combined large and small range tests this was 3243 entries out of the 48M
		 * tested).
		 *
		 * Different methods were tried here for the rounding, the simplest being: as
		 * floats, add a single LSB below where the 10 bits of the 16-bit float
		 * reside. This fails on sub-0.01 percent of entries, but nearly half of these
		 * are in 0x0000-0xFFFF roundtrip range, so this idea was dropped.
		 *
		 * In the end, the rounding method chosen also adds the LSB from beyond the 10
		 * bits mantissa range, but as an integer (which still correctly wraps into the
		 * exponent). This gets us within a reasonable margin of a full IEEE
		 * implementation with just a few instruction over the Fox Toolkit method,
		 * without the rounding. Testing the entire 0x0000-0xFFFF range gives an exact
		 * result, and the rest of 48M values tested have an off-by-1 LSB for 3244 of
		 * the entries, so all of the nearest even mentioned at the start, plus one.
		 *
		 * Testing shows it to be on-par with Clang's ARM implementation (which has,
		 * one would assume, CPU specific instructions).
		 */
		union {
			float    f; // where we write
			uint32_t u; // where we read
		} bits = {val};
		uint32_t sExp8 =  bits.u >> 23;
		uint32_t mnt11 = (bits.u & 0x007FFFFF) >> shft[sExp8];
		return static_cast<utils::float16>((base[sExp8] | (mnt11 >> 1)) + (mnt11 & 1));
	}

private:

	//******************************** Tables *******************************/

	/**
	 * Look-up table for the \e supported single-precision float exponents to
	 * their half equivalents.
	 */
	uint16_t base[512];

	/**
	 * Look-up table for the number of bits to \e logically shift the mantissa
	 * by, keyed by the single-precision float's exponent.
	 */
	uint8_t shft[512];
};

/**
 * Helper to convert IEEE 754 half-precision floats to single-precision.
 *
 * \note Requires 8576 bytes of one-time generated tables.
 *
 * \sa FloatToHalfConv
 */
class HalfToFloatConv
{
public:
	/**
	 * Creates the look-up tables for the conversion \e from half-precision.
	 * \n
	 * This should run once and be stored.
	 */
	HalfToFloatConv() {
		/*
		 * Generates the table for the 5-bit exponent, to its 8-bit equivalent
		 * plus the special cases for zeros and infinities, with each of the
		 * entries having signed and unsigned variants.
		 */
		for (int n = 1; n < 31; n++) {
			exps[n     ] =  n << 23;
			exps[n + 32] = (n << 23) | (1 << 31);
		}
		exps[ 0] = 0x00000000; // +0.0
		exps[32] = 0x80000000; // -0.0
		exps[31] = 0x47800000; // +inf/NaN
		exps[63] = 0xC7800000; // -inf/NaN
		/*
		 * Generates the mantissa tables for both subnormal, with a zero
		 * exponent and stored in the first half of the table, and normal floats
		 * (with an entry per 10-bit possibility).
		 */
		for (int n = 0; n < 1024; n++) {
			mant[n       ] = genForSub(n);
			mant[n + 1024] = genForNrm(n);
		}
		/*
		 * Generates the offset, essentially the switch between upper and lower
		 * halves of 'mant', based on the 5-bit exponent. The table includes the
		 * sign bit (hence 64 entries), with an all zero exponent denoting the
		 * lower half of 'mant' (the standalone entries at the end).
		 */
		for (int n = 1; n < 32; n++) {
			moff[n     ] = 1024;
			moff[n + 32] = 1024;
		}
		moff[ 0] = 0;
		moff[32] = 0;
	}

	/**
	 * Converts a half-precision float to single-precision.
	 *
	 * \param[in] val half-precision float
	 * \return equivalent single-precision float
	 */
	inline
	float convert(utils::float16 const val) const {
		/*
		 * Similarly to the conversion above, we apply the conversion directly
		 * to the float bits. Note that the mantissa table also contain a bias
		 * which gets *added* to the exponent table's value.
		 */
		union {
			uint32_t u; // where we write
			float    f; // where we read
		} bits = {
			exps[val >> 10] + mant[moff[val >> 10] | (val & 0x3FF)]
		};
		return bits.f;
	}

private:
	/**
	 * Generates the mantissa table look-up entries for the \e subnormal values,
	 * given the half-precision float mantissa bits. This takes the original bit
	 * pattern and normalises it (by shifting the mantissa and decrementing the
	 * exponent until arriving at a normalised value).
	 */
	inline
	static uint32_t genForSub(unsigned const bits) {
		if (bits) {
			/*
			 * See the original paper for the bit twiddling in detail.
			 */
			unsigned exp = 0;
			unsigned man = bits << 13;
			while (!(man &  0x00800000)) {
					 exp -= 0x00800000;
					 man <<= 1;
			}
			return  (exp +  0x38800000)
				 |  (man & ~0x00800000);
		}
		return 0;
	}

	/**
	 * Generates the mantissa table look-up entries for the \e normal values,
	 * given the half-precision float mantissa bits. This is a straightforward
	 * shift of the original bit pattern plus an exponent bias adjustment
	 * (essentially added to the \c #exps entry during conversion).
	 */
	inline
	static uint32_t genForNrm(unsigned const bits) {
		return 0x38000000 | (bits << 13);
	}

	//******************************** Tables *******************************/

	/**
	 * Look-up table for the 5-bit half-precision float exponents to their
	 * single-precision equivalents, plus the sign bit. This is pre-shifted into
	 * place (bits 23-31), saving an extra shift during the conversion.
	 */
	uint32_t exps[64];

	/**
	 * Look-up table for the 10-bit half-precision float mantissa values to
	 * their single-precision equivalents, containing both \e subnormal (the
	 * lower half of the table) and \e normal values (the upper half).
	 */
	uint32_t mant[2048];

	/**
	 * Look-up table with offsets into \c #mant, guided by the half-precision
	 * float exponent bits. For \e subnormal half floats, where the exponent bit
	 * contain all zeros, the table's value is zero, whereas for other values it
	 * contains \c 1024 (the offset into the \e normal entries in the upper half
	 * of \c #mant).
	 */
	uint16_t moff[64];
};

/**
 * Singleton \c FloatToHalfConv instance.
 */
static FloatToHalfConv const floatToHalfConv;

/**
 * Singleton \c HalfToFloatConv instance.
 */
static HalfToFloatConv const halfToFloatConv;
}
#endif

//******************************** Public API *********************************/

utils::float16 utils::floatToHalf(float const val) {
#ifdef HAS_BUILTIN_FLOAT16
	union {
		_Float16 f; // where we write
		uint16_t u; // where we read
	} bits = {
		static_cast<_Float16>(val)
	};
	return bits.u;
#else
	return impl::floatToHalfConv.convert(val);
#endif
}

float utils::halfToFloat(utils::float16 const val) {
#ifdef HAS_BUILTIN_FLOAT16
	union {
		uint16_t u; // where we write
		_Float16 f; // where we read
	} bits = {val};
	return bits.f;
#else
	return impl::halfToFloatConv.convert(val);
#endif
}
