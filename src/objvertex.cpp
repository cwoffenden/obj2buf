/**
 * \file objvertex.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "objvertex.h"

#include <cstdio>

#include "mikktspace.h"

/**
 * \def M_PI
 * Missing POSIX definition (for MSVC/Win32).
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

/**
 * Utility functions to bridge between \c ObjVertex and \e MikkTSpace. These are
 * all internal to this implementation and only \c ObjVertex#generateTangents()
 * should be used.
 */
namespace mtsutil {
	/**
	 * Passed to \c SMikkTSpaceContext as the \e user \e data, containing the
	 * vertices and any options.
	 *
	 * \param[in,out] verts collection of triangles
	 * \param[in] flipG generate tangents for a flipped green channel
	 */
	struct UserData {
		UserData(ObjVertex::Container& verts, bool const flipG)
			: verts(verts)
			, flipG(flipG) {};
		/**
		 * Collection of triangles.
		 *
		 * \note No ownership is passed and this lives for as the call to \c
		 * genTangSpaceDefault().
		 */
		ObjVertex::Container& verts;
		/**
		 * \c true if the normal map's green-channel should be flipped, which is
		 * performed by negating the Y-axis when extracting UVs. This depends on
		 * the source content and whether it needs matching to the render API.
		 */
		bool flipG;
	};
	/*
	 * Helper to pull the \c UserData containing the vertex \c Container from a
	 * \e MikkTSpace context.
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \return vertex collection (or \c null if \a mCtx is incomplete)
	 */
	static const UserData* getUserData(const SMikkTSpaceContext* mCtx) {
		if (mCtx && mCtx->m_pUserData) {
			return reinterpret_cast<const UserData*>(mCtx->m_pUserData);
		}
		return nullptr;
	}
	/**
	 * Given a \e MikkTSpace context holding a \c Container as its \e user \e
	 * data, extracts the correct vertex from the face and vertex indices.
	 *
	 * \param[in] ctx MikkTSpace C interface context
	 * \param[in] face triangle index (given that we only operate on triangles)
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 * \return the requested vertex or \c null if the indices are out of bounds
	 */
	static ObjVertex* getVertAt(const SMikkTSpaceContext* mCtx, int const face, int const vert) {
		if (const UserData* udata = getUserData(mCtx)) {
			size_t idx = face * 3 + vert;
			if (idx < udata->verts.size()) {
				return &((udata->verts)[idx]);
			}
		}
		return nullptr;
	}
	//************************** Interface Functions **************************/
	/**
	 * \see SMikkTSpaceInterface#m_getNumFaces
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \return the container size divided by \c 3 (since we only work internally in triangles)
	 */
	static int getNumFaces(const SMikkTSpaceContext* mCtx) {
		if (const UserData* udata = getUserData(mCtx)) {
			return static_cast<int>(udata->verts.size() / 3);
		}
		return 0;
	}
	/**
	 * \see SMikkTSpaceInterface#m_getNumVerticesOfFace
	 *
	 * \return \c 3 (since we only work internally in triangles)
	 */
	static int getNumVerticesOfFace(const SMikkTSpaceContext*, int) {
		return 3;
	}
	/**
	 * \see SMikkTSpaceInterface#m_getPosition
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \param[out] posn where to store the \c ObjVertex#posn values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getPosition(const SMikkTSpaceContext* mCtx, float posn[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(mCtx, face, vert)) {
			posn[0] = entry->posn.x;
			posn[1] = entry->posn.y;
			posn[2] = entry->posn.z;
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_getNormal
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \param[out] norm where to store the \c ObjVertex#norm values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getNormal(const SMikkTSpaceContext* mCtx, float norm[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(mCtx, face, vert)) {
			norm[0] = entry->norm.x;
			norm[1] = entry->norm.y;
			norm[2] = entry->norm.z;
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_getTexCoord
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \param[out] tex0 where to store the \c ObjVertex#tex0 values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getTexCoord(const SMikkTSpaceContext* mCtx, float tex0[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(mCtx, face, vert)) {
			tex0[0] = entry->tex0.x;
			/*
			 * Handle the G-channel flip by negating the Y-axis. Note, since
			 * entry is non-null, getUserData() will always be valid too.
			 */
			if (getUserData(mCtx)->flipG) {
				tex0[1] = -entry->tex0.y;
			} else {
				tex0[1] =  entry->tex0.y;
			}
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_setTSpace
	 *
	 * \param[in,out] mCtx MikkTSpace C interface context (note: the interface is const but we need to store the results here)
	 * \param[in] tans generated tangents (\c 0, \c 1 and \c 2 or \c x, \c y and \c z)
	 * \param[in] btan generated bitangents (\c 0, \c 1 and \c 2 or \c x, \c y and \c z)
	 * \param[in] sign \c true if \c ObjVertex#sign is assigned \c 1.0, otherwise \c -1.0 (used if we wish to calculated instead of store the bitangent)
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void setTSpace(const SMikkTSpaceContext* mCtx, const float tans[], const float btan[], float, float, tbool const sign, int const face, int const vert) {
		if (ObjVertex* entry = getVertAt(mCtx, face, vert)) {
			entry->sign   = (sign) ? 1.0f : -1.0f;
			entry->tans.x = tans[0];
			entry->tans.y = tans[1];
			entry->tans.z = tans[2];
			entry->btan.x = btan[0];
			entry->btan.y = btan[1];
			entry->btan.z = btan[2];
		}
	}
}

/**
 * Helper for the encoding (and test decoding) of normal vectors. This paper
 * describes various schemes:
 *
 * \see https://jcgt.org/published/0003/02/01/paper.pdf
 *
 * Some graphical comparisons here:
 *
 * \see https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
 *
 * Other links of interest:
 *
 * \see https://developer.download.nvidia.com/whitepapers/2008/real-time-normal-map-dxt-compression.pdf
 * \see https://aras-p.info/texts/CompactNormalStorage.html
 *
 * And this, though we're out of places to pack the extra bits required:
 *
 * \see https://johnwhite3d.blogspot.com/2017/10/signed-octahedron-normal-encoding.html
 *
 * \todo add support of \c oct16P (the decoder is the same, but we optimise of a known bit depth)
 */
namespace impl {
/**
 * Helper to accumulate angular errors.
 *
 * \note We're not (currently) using this to look at the actual encoded error
 * when lowering the bit-depth, it's for verifying any encoding is correct.
 */
class Accumulator {
public:
	/**
	 * Starts with zero errors.
	 */
	Accumulator()
		: sumAbs(0.0f)
		, maxAbs(0.0f)
		, count (0) {}
	/**
	 * Adds the \e absolute angular error between two \e normalised vectors.
	 *
	 * \param[in] a first entry (e.g. original value)
	 * \param[in] b second entry (e.g. decoded value)
	 */
	void add(const vec3& a, const vec3& b) {
		/*
		 * Notes: float rounding errors can product a dot product greater than
		 * one; none of the angles should be larger than 90 degrees (even at
		 * 4-bit precision the maximum error is approx 10 degrees).
		 */
		float dot = vec3::dot(a, b);
		if (dot < 0.0f) {
			dot = 0.0f;
		}
		float rad = std::acos(std::min(dot, 1.0f));
		float deg = rad * 180.0f / float(M_PI);
		sumAbs += deg;
		maxAbs = std::max(maxAbs, deg);
		count++;
	}
	/**
	 * Prints the average and maximum errors.
	 *
	 * \param[in] name title to prefix the output, e.g. \c error
	 */
	void print(const char* const name) const {
		printf("%s: average: %0.5f, maximum: %0.5f (all in degrees)\n", name, sumAbs / count, maxAbs);
	}
private:
	float sumAbs;   /**< Absolute sum of the entries added. */
	float maxAbs;   /**< Absolute maximum of any of the entries added. */
	unsigned count; /**< Number of entries added. */
};

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
 * Helper to constrain a vector's contents between upper and lower bounds.
 *
 * \param[in] val vector to constrain
 * \param[in] min lower bound (inclusive)
 * \param[in] max upper bound (inclusive)
 * \return a copy of \a vec constrained
 * \tparam T numeric type (e.g. \c float)
 */
template<typename T>
static Vec2<T> clamp(const Vec2<T>& vec, T const min, T const max) {
	return Vec2<T>(
		clamp(vec.x, min, max),
		clamp(vec.y, min, max)
	);
}
/**
 * Helper to run a function per vector component.
 *
 * \param[in] func function to run (e.g. \c std::floor)
 * \param[in] vec vector on which to operate
 * \return a copy of \a vec having run \a func on each component
 * \tparam T numeric type (e.g. \c float)
 */
template<typename T>
static Vec2<T> run(T (* const func)(T), const Vec2<T>& vec) {
	return Vec2<T>(
		func(vec.x),
		func(vec.y)
	);
}
/**
 * Returns the sign of \a val with \c 0 considered as \c +1 (whereas the
 * standard \c std::sign() would return zero).
 *
 * \param[in] val value to derive the sign
 * \return either \c -1 or \c +1 depending on \a val
 */
inline
float _sign_(float const val) {
	return (val >= 0.0f) ? 1.0f : -1.0f;
}
/**
 * Encode a normal vector with octahedral encoding (Meyer et al. 2010).
 *
 * \param[in] vec normal vector (the emphasis on this being normalised)
 * \return encoded normal
 */
vec2 encodeOct(const vec3& vec) {
	float sum  = std::abs(vec.x) + std::abs(vec.y) + std::abs(vec.z);
	float vecX = (sum != 0.0f) ? (vec.x / sum) : 0.0f;
	float vecY = (sum != 0.0f) ? (vec.y / sum) : 0.0f;
	return vec2(
		(vec.z <= 0.0f) ? ((1.0f - std::abs(vecY)) * _sign_(vecX)) : vecX,
		(vec.z <= 0.0f) ? ((1.0f - std::abs(vecX)) * _sign_(vecY)) : vecY);
}
/**
 * Performs the reverse of \c encodeOct() returning a normal vector from an
 * octahedral encoding.
 *
 * \note This is here for test purposes and is \e not optimal.
 *
 * \param[in] enc encoded normal
 * \return normal vector
 */
vec3 decodeOct(const vec2& enc) {
	vec3 vec(enc.x, enc.y, 1.0f - std::abs(enc.x) - std::abs(enc.y));
	if (vec.z < 0.0f) {
		vec.x = (1.0f - std::abs(enc.y)) * _sign_(enc.x);
		vec.y = (1.0f - std::abs(enc.x)) * _sign_(enc.y);
	}
	return vec.normalize();
}
/**
 * Performs \c encodeOct() optimising for a more precise decode knowing the
 * number of bits the result will be stored in.
 *
 * \note The \e modern approach to storing signed values is used, e.g. for a
 * bit-depth of \c 8 the range is \c -127 to \c +127 preserving zero.
 *
 * \not Whilst this is designed for normalised ints it also improves encoding
 * for floats. Pass in \c 23 as the number of \a bits (the fraction bits for a
 * 32-bit float) and it will significantly reduce the average error.
 *
 * \todo is it worth adding a choice to use the legacy storage? Yes.
 *
 * \param[in] vec normal vector (the emphasis on this being normalised)
 * \param[in] bits bit-depth the encoded value will be stored in (e.g. \c 8 for byte storage)
 * \return encoded normal
 */
vec2 encodeOct(const vec3& vec, unsigned const bits) {
	/*
	 * This is adapted from float32x3_to_octn_precise() in the Survey paper at
	 * the top, from the GLSL code instead of the C++ implementation, operating
	 * on the data as floats instead of the float-bits (flip-flopping between
	 * the floor() and ceil() for the two components).
	 *
	 * Start with the signed normalised value of one (for the given bit-depth),
	 * and calculate the base (floor) from which other variants will be created.
	 *
	 * Note: the original C++ implementation wasn't tested against this for
	 * performance, so might be better. This GLSL adaptation was written because
	 * it's smaller, not relying on any support code that isn't already in use.
	 */
	float const one = (1 << (bits - 1)) - 1.0f;
	vec2 const base = run(std::floor, (clamp(encodeOct(vec), -1.0f, 1.0f) * one)) * (1.0f / one);
	vec2 bestEnc = base;
	vec3 bestDec = decodeOct(base);
	/*
	 * Then test the combination of floor() and ceil() to better (u = 0, v = 0)
	 * and its angular error (from the decoded value, closest to zero).
	 *
	 * From the original paper: no attempt is made to wrap the oct boundaries,
	 * but since this this should be a worse encoding (when decoded) it will
	 * never class as best.
	 */
	float bestErr = std::abs(1.0f - vec3::dot(bestDec, vec));
	float bestLen = std::abs(1.0f - bestDec.len());
	for (unsigned u = 0; u < 2; u++) {
		for (unsigned v = 0; v < 2; v++) {
			if ((u != 0) || (v != 0)) {
				/*
				 * We're adding (or not) the LSB (e.g. 1/127th for 8-bits)
				 * before testing the angular error. We pick the best angular
				 * error (closest to zero) and if we have a tie, the closest to
				 * unit length (best difference from 1.0).
				 */
				vec2  testEnc = (vec2(float(u), float(v)) * (1.0f / one)) + base;
				vec3  testDec = decodeOct(testEnc);
				float testErr = std::abs(1.0f - vec3::dot(testDec, vec));
				if (testErr <= bestErr) {
					float testLen = std::abs(1.0f - testDec.len());
					if (testErr == bestErr) {
						if (testLen < bestLen) {
							bestEnc = testEnc;
							bestLen = testLen;
						}
					} else {
						bestEnc = testEnc;
						bestLen = testLen;
					}
				}
			}
			if (bestErr == 0.0f && bestLen == 0.0f) {
				goto zeroed;
			}
		}
	}
zeroed:
	return bestEnc;
}
}

//*****************************************************************************/

ObjVertex::ObjVertex(fastObjMesh* obj, fastObjIndex* idx) {
	posn.x = obj->positions[idx->p * 3 + 0];
	posn.y = obj->positions[idx->p * 3 + 1];
	posn.z = obj->positions[idx->p * 3 + 2];
	tex0.x = obj->texcoords[idx->t * 2 + 0];
	tex0.y = obj->texcoords[idx->t * 2 + 1];
	norm.x = obj->normals  [idx->n * 3 + 0];
	norm.y = obj->normals  [idx->n * 3 + 1];
	norm.z = obj->normals  [idx->n * 3 + 2];
	tans   = 0.0f;
	btan   = 0.0f;
	sign   = 0.0f;
	/*
	 * Max's default .obj exporter writes all floats at four decimal places, so
	 * the normals benefit from renormalising (plus any encoding is off too).
	 */
	norm   = norm.normalize();
}

bool ObjVertex::generateTangents(Container& verts, bool const flipG) {
	/*
	 * We use the default generation call with the non-basic function.
	 */
	SMikkTSpaceInterface iface = {
		mtsutil::getNumFaces,
		mtsutil::getNumVerticesOfFace,
		mtsutil::getPosition,
		mtsutil::getNormal,
		mtsutil::getTexCoord,
		nullptr, // basic
		mtsutil::setTSpace
	};
	mtsutil::UserData udata(verts, flipG);
	SMikkTSpaceContext const mCtx = {
		&iface,
		&udata,
	};
	return genTangSpaceDefault(&mCtx) != 0;
}

void ObjVertex::encodeNormals(Container& verts, bool const tans, bool const btan, unsigned const /*bits*/) {
#ifndef NDEBUG
	impl::Accumulator normErr;
	impl::Accumulator tansErr;
	impl::Accumulator btanErr;
#endif
	for (Container::iterator it = verts.begin(); it != verts.end(); ++it) {
		vec2 enc = impl::encodeOct(it->norm);
	#ifndef NDEBUG
		normErr.add(it->norm, impl::decodeOct(enc));
	#endif
		it->norm.x = enc.x;
		it->norm.y = enc.y;
		it->norm.z = 0.0f;
		if (tans) {
			enc = impl::encodeOct(it->tans);
		#ifndef NDEBUG
			tansErr.add(it->tans, impl::decodeOct(enc));
		#endif
			it->tans.x = enc.x;
			it->tans.y = enc.y;
			it->tans.z = 0.0f;
			if (btan) {
				enc = impl::encodeOct(it->btan);
			#ifndef NDEBUG
				btanErr.add(it->btan, impl::decodeOct(enc));
			#endif
				it->btan.x = enc.x;
				it->btan.y = enc.y;
				it->btan.z = 0.0f;
			}
		}
	}
#ifndef NDEBUG
	printf("\n");
	normErr.print("Encoded norm error");
	if (tans) {
		tansErr.print("Encoded tans error");
		if (btan) {
			btanErr.print("Encoded btan error");
		}
	}
#endif
}
