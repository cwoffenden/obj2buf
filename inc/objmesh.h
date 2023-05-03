/**
 * \file objmesh.h
 * Wrapper for mesh data.
 *
 * \copyright 2022 Numfum GmbH
 */

#include "objvertex.h"

/**
 * The \c .obj file as vertex and index data. The mesh is loaded into here,
 * manipulated in-place, then saved out. Once the process starts, if the
 * original mesh data is needed it will need to be reloaded (or retrieved from a
 * copy).
 *
 * \note This was retrofitted to experimentally support FBX files (but the
 * naming kept, since \c .obj is what it's modelled on).
 */
struct ObjMesh
{
public:
	/**
	 * Creates a zero-sized mesh (empty buffers, no scale or bias).
	 */
	ObjMesh()
		: scale(1.0f, 1.0f, 1.0f)
		, bias (0.0f, 0.0f, 0.0f) {};

	/**
	 * Clears the content, sets the scale to \c 1 and bias to \c 0.
	 */
	void reset();

	/**
	 * Opens an \c .obj file and extracts its content (experimental support was
	 * added for FBX files, extracting the first mesh found).
	 *
	 * \note Any existing content is replaced.
	 *
	 * \param[in] srcPath filename of the \c .obj or FBX file
	 * \param[in] genTans \c true if tangents should be generated
	 * \param[in] flipG generate tangents for a flipped green channel (by negating the texture's y-axis)
	 * \return \c true if the file was valid and \a mesh has its content
	 */
	bool load(const char* const srcPath, bool const genTans, bool const flipG);

	/**
	 * Scale the mesh positions so that each is normalised between \c -1 and \c 1.
	 *
	 * \param[in] uniform \c true if the same scale should be applied to all axes (otherwise a per-axis scale is applied)
	 * \param[in] unbiased  \c true if the origin should be maintained at zero (otherwise a bias is applied to make the most of the normalised range)
	 */
	void normalise(bool const uniform, bool const unbiased);

	/**
	 * Run meshopt's various processes (namely vertex cache, overdraw and vertex
	 * vetch optimisations).
	 */
	void optimise();

	/**
	 * Resizes the buffers (usually as a prelude to filling them).
	 */
	void resize(size_t const numVerts, size_t const numIndex);

	//*************************** Public Properties ***************************/

	/**
	 * Collection of (usually) unique vertices referenced by \c #index.
	 */
	ObjVertex::Container verts;
	/**
	 * Collection of indices into \c #verts.
	 */
	std::vector<unsigned> index;
	/**
	 * Scale to apply to each vertex position when drawing (the default is \c 1.0).
	 */
	vec3 scale;
	/**
	 * Offset to apply to each vertex position when drawing (the default is \c 0.0).
	 */
	vec3 bias;
};
