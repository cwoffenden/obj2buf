/**
 * \file objvertex.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "objvertex.h"

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
