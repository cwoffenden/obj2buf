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

	//****************************** Conversions ******************************/

	/**
	 * Generates the \c #tans, \c #btan and \c #sign from the extracted \c .obj
	 * data. \a cont is expected to contain unindexed triangles.
	 *
	 * \note This \e must be called before running \c #encodeNormals() since it
	 * requires unencoded normals.
	 *
	 * \param[in,out] verts collection of triangles
	 * \param[in] flipG generate tangents for a flipped green channel (by negating the texture's y-axis)
	 * \return \c true if generation was successful
	 */
	static bool generateTangents(Container& verts, bool const flipG);

	/*
	 * In-place encoding of normals, tangents and bitangents. This in-place
	 * conversion zeroes the Z and stores the encoded results in the X and Y
	 * for each of the affected attributes.
	 *
	 * \note Hemi-oct encoding trades a slightly higher error for simpler
	 * decoder (but as its name suggests, works on a single hemisphere).
	 *
	 * \todo look at spherical encoding? Is it worth the overhead?
	 *
	 * \param[in,out] verts collection of triangles
	 * \param[in] hemi \c true to encode using hemi-ict (otherwise octahedron encoding is used)
	 * \param[in] tans \c true if tangents and bitangents should also be converted (otherwise only normals are affected)
	 */
	static void encodeNormals(Container& verts, bool const hemi, bool const tans);

	//*************************************************************************/

	vec3 posn; /**< Positions (from the \c .obj file). */
	vec2 tex0; /**< UV channel 0 (from the \c .obj file). */
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
