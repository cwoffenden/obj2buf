/**
 * \file objvertex.h
 * \c .obj file vertex structure.
 */
#pragma once

#include "mikktspace.h"
#include "vec.h"

/**
 * Data structure for the vertex data extracted from an \c obj file.
 */
struct ObjVertex
{
	/**
	 * Vector of vertices.
	 */
	typedef std::vector<ObjVertex> Container;
	/**
	 * Uninitialised vertex data.
	 */
	ObjVertex() = default;
	/**
	 * Constructs a single vertex from the \c obj data, extracting the relevant
	 * position, normal and UV data. The tangents are zeroed.
	 *
	 * \param[in] obj the \c fast_obj file
	 * \param[in] idx current face index being processed
	 */
	ObjVertex(fastObjMesh* obj, fastObjIndex* idx) {
		posn.x = obj->positions[idx->p * 3 + 0];
		posn.y = obj->positions[idx->p * 3 + 1];
		posn.z = obj->positions[idx->p * 3 + 2];
		norm.x = obj->normals  [idx->n * 3 + 0];
		norm.y = obj->normals  [idx->n * 3 + 1];
		norm.z = obj->normals  [idx->n * 3 + 2];
		uv_0.x = obj->texcoords[idx->t * 2 + 0];
		uv_0.y = obj->texcoords[idx->t * 2 + 1];
		tans   = 0.0f;
		btan   = 0.0f;
		sign   = 0.0f;
	}
	vec3 posn; /**< Positions */
	vec3 norm; /**< Normals. */
	vec2 uv_0; /**< UV channel 0 */
	vec3 tans; /**< Tangents */
	vec3 btan; /**< Bitangents */
	/*
	 * An alternative to storing the bitangents is to recreate them from:
	 * \code
	 *	btan = sign * cross(norm, tans);
	 * \endcode
	 */
	float sign;
};

/**
 * Utility functions to bridge between this code and \e MikkTSpace.
 */
namespace mtutil {
	/**
	 * Helper to pull the (immutable) vertex \c Container from a \e MikkTSpace context.
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \return vertex collection (or \c null if \a mCtx is incomplete)
	 */
	static const ObjVertex::Container* getVerts(const SMikkTSpaceContext* mCtx) {
		if (mCtx && mCtx->m_pUserData) {
			return reinterpret_cast<const ObjVertex::Container*>(mCtx->m_pUserData);
		}
		return nullptr;
	}
	/**
	 * Given a \e MikkTSpace context holding a \c Container as its \e user \e
	 * data, extracts the correct vertex from the face and vertex indices.
	 *
	 * \param[in] mCtx MikkTSpace C interface context
	 * \param[in] face triangle index (given that we only operate on triangles)
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 * \return the requested vertex or \c null if the indices are out of bounds
	 */
	static const ObjVertex* getVertAt(const SMikkTSpaceContext* mCtx, int const face, int const vert) {
		if (const ObjVertex::Container* verts = getVerts(mCtx)) {
			size_t idx = face * 3 + vert;
			if (idx < verts->size()) {
				return &((*verts)[idx]);
			}
		}
		return nullptr;
	}
	//************************** Interface Functions **************************/
	/**
	 * \see SMikkTSpaceInterface#m_getNumFaces
	 * \param[in] ctx MikkTSpace C interface context
	 * \return the container size divided by \c 3 (since we only work internally in triangles)
	 */
	static int getNumFaces(const SMikkTSpaceContext* ctx) {
		if (const ObjVertex::Container* verts = getVerts(ctx)) {
			return static_cast<int>(verts->size() / 3);
		}
		return 0;
	}
	/**
	 * \see SMikkTSpaceInterface#m_getNumVerticesOfFace
	 * \return \c 3 (since we only work internally in triangles)
	 */
	static int getNumVerticesOfFace(const SMikkTSpaceContext*, int) {
		return 3;
	}
	/**
	 * \see SMikkTSpaceInterface#m_getPosition
	 * \param[in] ctx MikkTSpace C interface context
	 * \param[out] posn where to store the \c ObjVertex#posn values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getPosition(const SMikkTSpaceContext* ctx, float posn[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(ctx, face, vert)) {
			posn[0] = entry->posn.x;
			posn[1] = entry->posn.y;
			posn[2] = entry->posn.z;
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_getNormal
	 * \param[in] ctx MikkTSpace C interface context
	 * \param[out] norm where to store the \c ObjVertex#norm values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getNormal(const SMikkTSpaceContext* ctx, float norm[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(ctx, face, vert)) {
			norm[0] = entry->norm.x;
			norm[1] = entry->norm.y;
			norm[2] = entry->norm.z;
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_getTexCoord
	 * \param[in] ctx MikkTSpace C interface context
	 * \param[out] uv_0 where to store the \c ObjVertex#uv_0 values
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void getTexCoord(const SMikkTSpaceContext* ctx, float uv_0[], int const face, int const vert) {
		if (const ObjVertex* entry = getVertAt(ctx, face, vert)) {
			uv_0[0] = entry->uv_0.x;
			uv_0[1] = entry->uv_0.y;
		}
	}
	/**
	 * \see SMikkTSpaceInterface#m_setTSpace
	 * \param[in,out] ctx MikkTSpace C interface context (note: the interface is const but we need to store the results here)
	 * \param[in] tans generated tangents (\c 0, \c 1 and \c 2 or \c x, \c y and \c z)
	 * \param[in] btan generated bitangents (\c 0, \c 1 and \c 2 or \c x, \c y and \c z)
	 * \param[in] sign \c true if \c ObjVertex#sign is assigned \c 1.0, otherwise \c -1.0 (used if we wish to calculated instead of store the bitangent)
	 * \param[in] face triangle index
	 * \param[in] vert which of the triangle vertices (\c 0, \c 1 or \c 2)
	 */
	static void setTSpace(const SMikkTSpaceContext* ctx, const float tans[], const float btan[], float, float, tbool const sign, int const face, int const vert) {
		if (ObjVertex* entry = const_cast<ObjVertex*>(getVertAt(ctx, face, vert))) {
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
