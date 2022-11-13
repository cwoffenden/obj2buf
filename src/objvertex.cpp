/**
 * \file objvertex.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "objvertex.h"

#include <cstdio>

#include "mikktspace.h"

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

/*
 * Helper for the encoding (and test decoding) of normal vectors.
 *
 * \see https://jcgt.org/published/0003/02/01/paper.pdf
 *
 * We keep this simple and dismiss more precise options such as \c oct16P, etc.,
 * from the document, choosing decode speed as the priority. Some graphical
 * comparisons here:
 *
 * \see https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
 *
 * \todo compare with https://gist.github.com/Niadb/794ea32f856820bc7e4f5a67c4246791
 */
namespace impl {
/**
 * Helper to accumulate error differences.
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
	 * Adds the \e absolute difference of two vectors.
	 *
	 * \param[in] a first entry
	 * \param[in] b second entry
	 */
	void add(const vec3& a, const vec3& b) {
		vec3 diff = a - b;
		float sum = std::abs(diff.x) + std::abs(diff.y) + std::abs(diff.z);
		sumAbs += sum;
		maxAbs = std::max(maxAbs, sum);
		count++;
	}
	/**
	 * Prints the average and maximum errors.
	 *
	 * \param[in] name title to prefix the output, e.g. \c error
	 */
	void print(const char* const name) const {
		printf("%s: average: %0.5f, maximum: %0.5f\n", name, sumAbs / count, maxAbs);
	}
private:
	float sumAbs;   /**< Absolute sum of the entries added. */
	float maxAbs;   /**< Absolute maximum of any of the entries added. */
	unsigned count; /**< Number of entries added. */
};
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
 * Encode a normal vector with octahedron encoding (Meyer et al. 2010).
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
 * octahedron encoding.
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
	return vec.normalize();;
}

/**
 * Encode a normal vector with hemi-oct encoding (van Waveren and Castaño 2008).
 *
 * \param[in] vec normal vector (the emphasis on this being normalised)
 * \return encoded normal
 */
vec2 encodeHemiOct(const vec3& /*vec*/) {
	return vec2(0.0f, 0.0f);
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

void ObjVertex::encodeNormals(Container& verts, bool const /*hemi*/, bool const tans, bool const btan) {
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
	normErr.print("Encoded normals error");
	if (tans) {
		tansErr.print("Encoded tangents error");
		if (btan) {
			btanErr.print("Encoded bitangents error");
		}
	}
#endif
}
