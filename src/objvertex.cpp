/**
 * \file objvertex.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "objvertex.h"

#include <cstdio>

#include "mikktspace.h"

/**
 * \def O2B_ATAN2_ERROR
 * Chooses which error calculation to use for angular error. The more accurate
 * with small differences is \c atan2 (otherwise the \c dot product is taken).
 *
 * \note \c atan2 is the clear winner, leaving the choice only for testing.
 */
#ifndef O2B_ATAN2_ERROR
#define O2B_ATAN2_ERROR
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
 * For tangents, angle encoding from a deterministic orthonormal vector looks
 * interesting, mentioned in the SIGGRAPH 2020 Doom eternal slides with more
 * details here:
 *
 * \see https://www.jeremyong.com/graphics/2023/01/09/tangent-spaces-and-diamond-encoding/
 *
 * But the decoding requires \c cos() and \c tan(), which may incur a
 * performance penalty when compared with the simple ALU needs of octahedral
 * encoding (the same reason why spherical coordinates aren't used), and it
 * really needs more bits to encode otherwise the accuracy suffers, so we end
 * up with the same storage as oct (though Doom packs into a byte).
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
		 * Notes: (1) float rounding errors can produce a dot product greater
		 * than one; (2) none of the angles should be larger than 90 degrees
		 * (even at 4-bit precision the maximum error is approx 10 degrees).
		 *
		 * Experimentation shows that atan2() is a better match for these small
		 * angular differences. Earlier code used to be:
		 *
		 *	rad = std::acos(std::min(vec3::dot(a, b), 1.0f));
		 *
		 * Which works, but see below for a detailed description.
		 */
	#ifdef O2B_ATAN2_ERROR
		float rad = std::atan2(vec3::cross(a, b).len(), vec3::dot(a, b));
	#else
		float rad = std::acos(std::min(vec3::dot(a, b), 1.0f));
	#endif
		float deg = rad * 180.0f / float(M_PI);
		sumAbs += deg;
		maxAbs = std::max(maxAbs, deg);
		count++;
	}
	/**
	 * Prints the mean and maximum errors.
	 *
	 * \param[in] name title to prefix the output, e.g. \c error
	 */
	void print(const char* const name) const {
		printf("%s: mean: %0.5f, max: %0.5f (all in degrees)\n", name, sumAbs / count, maxAbs);
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
 * Helper to call \c VertexPacker#roundtrip() on a vector's components.
 *
 * \param[in] vec vector to encode/decode
 * \param[in] type conversion and byte storage
 * \param[in] rounding rounding choice to use (default to rounding to nearest)
 * \param[in] legacy see \c Options#OPTS_SIGNED_LEGACY (default is modern encoding)
 * \param[in] rounding rounding choice to use (default to rounding to nearest)
 * \return a vector of the equivalent float values with the chosen storage
 */
static vec2 roundtrip(const vec2& vec, VertexPacker::Storage const type, bool const legacy = false, VertexPacker::Rounding rounding = VertexPacker::ROUND_NEAREST) {
	return vec2(
		VertexPacker::roundtrip(vec.x, type, legacy, rounding),
		VertexPacker::roundtrip(vec.y, type, legacy, rounding)
	);
}

//******************************* Oct Encoding ********************************/

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
 * \note Whilst this is designed for normalised ints it also improves encoding
 * for floats, converting using the internal \c SINT10N and \c SINT23N types
 * (significantly improving the decoded precision).
 *
 * \param[in] vec normal vector (the emphasis on this being normalised)
 * \param[in] type conversion and byte storage
 * \param[in] legacy see \c VertexPacker::Options#OPTS_SIGNED_LEGACY (default is modern encoding)
 * \return encoded normal
 */
vec2 encodeOct(const vec3& vec, VertexPacker::Storage type, bool const legacy = false) {
	if (!type) {
		return encodeOct(vec);
	}
	/*
	 * This has been through various implmentations, starting with an adaptation
	 * of float32x3_to_octn_precise() in the Survey paper at the top, though
	 * from the GLSL code instead of the C++ implementation, before taking this
	 * approach, similar in that it starts with the floor(), then extended to
	 * work with many encoding types (including legacy GL). Documented in-line.
	 */
	// The encoded oct at float32 precision
	vec2 const hires = encodeOct(vec);
	// Special-case 'type' for floats (to treat the mantissa bits as a normalised int)
	VertexPacker::Storage rtType;
	switch (type) {
	case VertexPacker::Storage::FLOAT16:
		rtType = VertexPacker::Storage::SINT10N;
		break;
	case VertexPacker::Storage::FLOAT32:
		rtType = VertexPacker::Storage::SINT23N;
		break;
	default:
		rtType = type;
	}
	// Roundtrip the high precision encoding to floor and ceiling lower precision
	vec2 const encFloor = roundtrip(hires, rtType, legacy, VertexPacker::ROUND_FLOOR);
	vec2 const encCeil  = roundtrip(hires, rtType, legacy, VertexPacker::ROUND_CEILING);
	// Then repackage the floor/ceiling to enable more convenient selection
	vec2 const flrCeilX = vec2(encFloor.x, encCeil.x);
	vec2 const flrCeilY = vec2(encFloor.y, encCeil.y);
	/*
	 * Then, starting with the floor, test the combination of floor and ceil to
	 * better bestDec's angular error (from the decoded value, closest to zero).
	 *
	 * From the original paper: no attempt is made to wrap the oct boundaries,
	 * but since this should be a worse encoding (when decoded) it will never
	 * class as best.
	 */
	vec2 bestEnc = encFloor;
	vec3 bestDec = decodeOct(bestEnc);
#ifdef O2B_ATAN2_ERROR
	float bestErr = std::atan2(vec3::cross(vec, bestDec).len(), vec3::dot(vec, bestDec));
#else
	float bestErr = std::max(1.0f - vec3::dot(vec, bestDec), 0.0f);
#endif
	float bestLen = std::abs(1.0f - bestDec.len());
	for (unsigned u = 0; u < 2; u++) {
		for (unsigned v = 0; v < 2; v++) {
			if ((u != 0) || (v != 0)) {
				vec2  testEnc = vec2(flrCeilX[u], flrCeilY[v]);
				vec3  testDec = decodeOct(testEnc);
			#ifdef O2B_ATAN2_ERROR
				float testErr = std::atan2(vec3::cross(vec, testDec).len(), vec3::dot(vec, testDec));
			#else
				float testErr = std::max(1.0f - vec3::dot(vec, testDec), 0.0f);
			#endif
				if (testErr <= bestErr) {
					float testLen = std::abs(1.0f - testDec.len());
					if (testErr == bestErr) {
						/*
						 * Notes on this: refining on the unit length doesn't
						 * affect the angular error, and using atan2 over the
						 * dot it takes until 23-bit encoding before see it used
						 * (otherwise it's a negligible percentage).
						 */
						if (testLen < bestLen) {
							bestEnc = testEnc;
							bestLen = testLen;
						}
					} else {
						bestEnc = testEnc;
						bestLen = testLen;
						bestErr = testErr;
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

//*****************************************************************************/

/**
 * Helper to copy FBX vertex data to our internal vector. A check is made to
 * verify the vertex data are valid, then the face index converted to its vertex
 * equivalent, finally the elements are copied. The destination is zeroed if no
 * source data exists.
 *
 * \param[in] src source vertex vector
 * \param[in] idx source face index
 * \param[out] dst destination vector
 * \return \c true if the vertex type has data
 */
bool copyVec(const ufbx_vertex_vec2& src, size_t const idx, vec2& dst) {
	if (src.exists) {
		ufbx_vec2& vec = src.values.data[src.indices.data[idx]];
		dst.x = static_cast<float>(vec.x);
		dst.y = static_cast<float>(vec.y);
		return true;
	} else {
		dst   = 0.0f;
	}
	return false;
}
/**
 * \copydoc copyVec(const ufbx_vertex_vec2&,size_t,vec2&)
 */
bool copyVec(const ufbx_vertex_vec3& src, size_t const idx, vec3& dst) {
	if (src.exists) {
		ufbx_vec3& vec = src.values.data[src.indices.data[idx]];
		dst.x = static_cast<float>(vec.x);
		dst.y = static_cast<float>(vec.y);
		dst.z = static_cast<float>(vec.z);
		return true;
	} else {
		dst   = 0.0f;
	}
	return false;
}
/**
 * \copydoc copyVec(const ufbx_vertex_vec2&,size_t,vec2&)
 */
bool copyVec(const ufbx_vertex_vec4& src, size_t const idx, vec4& dst) {
	if (src.exists) {
		ufbx_vec4& vec = src.values.data[src.indices.data[idx]];
		dst.x = static_cast<float>(vec.x);
		dst.y = static_cast<float>(vec.y);
		dst.z = static_cast<float>(vec.z);
		dst.w = static_cast<float>(vec.w);
		return true;
	} else {
		dst   = 0.0f;
	}
	return false;
}
}

//*****************************************************************************/

ObjVertex::ObjVertex(fastObjMesh* obj, fastObjIndex* idx) {
	/*
	 * Note: Max's default .obj exporter writes all floats at four decimal
	 * places, so the normals benefit from renormalising (plus any encoding is
	 * off if we don't).
	 *
	 * Note: indices for p, t, and n are 1-based, and a zero-inited array entry
	 * always exists with an array cound of 1 (unlike colours, for example,
	 * which will be null).
	 *
	 * TODO: migrating to 'tinyobjloader' since it supports vertex colours and multiple UVs (hmm, fast_obj supports vcols now)
	 */
	assert(idx->p < obj->position_count);
	assert(idx->t < obj->texcoord_count);
	assert(idx->n < obj->normal_count);
	posn.x = obj->positions[idx->p * 3 + 0];
	posn.y = obj->positions[idx->p * 3 + 1];
	posn.z = obj->positions[idx->p * 3 + 2];
	tex0.x = obj->texcoords[idx->t * 2 + 0];
	tex0.y = obj->texcoords[idx->t * 2 + 1];
	tex1   = 0.0f;
	norm.x = obj->normals  [idx->n * 3 + 0];
	norm.y = obj->normals  [idx->n * 3 + 1];
	norm.z = obj->normals  [idx->n * 3 + 2];
	tans   = 0.0f;
	btan   = 0.0f;
	sign   = 0.0f;
	norm   = norm.normalize();
}

ObjVertex::ObjVertex(ufbx_mesh* fbx, size_t const idx) {
	/*
	 * Initial attempt at this: all vertex data are copied, zeroing if they
	 * don't exist (no effort has been made so far to generate missing data).
	 *
	 * Note: we're recreating tangents (1. because they have the wrong
	 * orientation and need correcting, and 2. so that we have the same
	 * MikkTSpace calculations throughout).
	 *
	 * TODO: extract (and support) tex1 from ufbx_uv_set_list
  	 * TODO: if the FBX contains tangents (in the UV sets) take those?
	 */
	impl::copyVec(fbx->vertex_position,  idx, posn);
	impl::copyVec(fbx->vertex_uv,        idx, tex0);
	tex1 = 0.0f;
	impl::copyVec(fbx->vertex_normal,    idx, norm);
	impl::copyVec(fbx->vertex_color,     idx, rgba);
	tans = 0.0f;
	btan = 0.0f;
	sign = 0.0f;

	// Hack to put tex1 into tex0 for testing
	/*
	if (fbx->uv_sets.count > 1) {
		impl::copyVec(fbx->uv_sets[1].vertex_uv, idx, tex0);
	}
	 */
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

void ObjVertex::encodeNormals(Container& verts, VertexPacker::Storage norm, VertexPacker::Storage tans, bool const btan, bool const legacy) {
#ifndef NDEBUG
	impl::Accumulator normErr;
	impl::Accumulator tansErr;
	impl::Accumulator btanErr;
#endif
	for (Container::iterator it = verts.begin(); it != verts.end(); ++it) {
		vec2 enc = impl::encodeOct(it->norm, norm, legacy);
	#ifndef NDEBUG
		normErr.add(it->norm, impl::decodeOct(enc));
	#endif
		it->norm.x = enc.x;
		it->norm.y = enc.y;
		it->norm.z = 0.0f;
		if (tans) {
			enc = impl::encodeOct(it->tans, tans, legacy);
		#ifndef NDEBUG
			tansErr.add(it->tans, impl::decodeOct(enc));
		#endif
			it->tans.x = enc.x;
			it->tans.y = enc.y;
			it->tans.z = 0.0f;
			if (btan) {
				enc = impl::encodeOct(it->btan, tans, legacy);
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
