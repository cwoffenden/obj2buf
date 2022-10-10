/**
 * \file objvertex.h
 * \c .obj file vertex structure.
 *
 * \copyright 2022 Numfum GmbH
 */
#pragma once

#include <vector>

#include "fast_obj.h"
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
	ObjVertex(fastObjMesh* obj, fastObjIndex* idx);
	/**
	 * Generates the \c #tans, \c #btan and \c #sign from the extracted \c .obj
	 * data. \a cont is expected to contain unindexed triangles.
	 *
	 * \param[in,out] verts collection of triangles
	 * \return \c true if generation was successful
	 */
	static bool generateTangents(Container& verts);

	//*************************************************************************/

	vec3 posn; /**< Positions (from the \c .obj file). */
	vec2 uv_0; /**< UV channel 0 (from the \c .obj file). */
	vec3 norm; /**< Normals (from the \c .obj file). */
	vec3 tans; /**< Tangents (generated if needed). */
	vec3 btan; /**< Bitangents (generated if needed). */
	/**
	 * An alternative to storing the bitangents is to recreate them from:
	 * \code
	 *	btan = sign * cross(norm, tans);
	 * \endcode
	 * (Generated if needed)
	 */
	float sign;
};
