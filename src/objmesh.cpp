#include "objmesh.h"

#include <cfloat>
#include <cstdio>

#include "meshoptimizer.h"

/**
 * \def O2B_SMALL_VERT_POS
 * Value that's considered \e small for a vertex position. Values above this can
 * be normalised, below this no processing is done. We choose 1/127, the LSB in
 * a signed 8-bit range (it's just small, and a number had to be picked). Used
 * by \c ObjMesh#normalise().
 */
#ifndef O2B_SMALL_VERT_POS
#define O2B_SMALL_VERT_POS (1.0f / 127.0f)
#endif

/**
 * Helpers to extract mesh data.
 */
namespace impl {
/**
 * Extracts the \c .obj file mesh data as vertex and index buffers.
 *
 * \todo indices should be optional (since they're optional for export) unless we generate then manually export the tris (since we miss out on other meshopt features otherwise)
 *
 * \param[in] obj valid \c fast_obj content
 * \param[in] genTans \c true if tangents should be generated
 * \param[in] flipG generate tangents for a flipped green channel (by negating the texture's y-axis)
 * \param[out] mesh destination for the \c .obj file content
 */
void extract(fastObjMesh* const obj, bool const genTans, bool const flipG, ObjMesh& mesh) {
	// No objects or groups, just one big triangle mesh from the file
	ObjVertex::Container verts;
	// Content should be in tris but we're going to create fans from any polys
	unsigned maxVerts = 0;
	for (unsigned face = 0; face < obj->face_count; face++) {
		maxVerts += 3 * (obj->face_vertices[face] - 2);
	}
	verts.reserve(maxVerts);
	// Fill the mesh from the expanded raw face data
	unsigned vertBase = 0;
	for (unsigned face = 0; face < obj->face_count; face++) {
		unsigned faceVerts = obj->face_vertices[face];
		unsigned polyStart = static_cast<unsigned>(verts.size());
		for (unsigned vert = 0; vert < faceVerts; vert++) {
			if (vert > 2) {
				/*
				 * We create fans for any faces with more than three verts
				 * (which will only work for convex polys, but this should
				 * really be processing only tris or quads). We create fans as
				 * [0, 1, 2], [2, 3, 0], [0, 3, 4], etc., with each added tri
				 * using the last tri's vert as its starting point, not for vert
				 * buffer locality but for ease of compression.
				 */
				if ((vert & 1) != 0) {
					verts.push_back(verts[verts.size() - 1]);
				} else {
					verts.push_back(verts[polyStart]);
					verts.push_back(verts[verts.size() - 3]);
				}
			}
			verts.emplace_back(obj, &obj->indices[vertBase + vert]);
			if (vert > 2) {
				if ((vert & 1) != 0) {
					verts.push_back(verts[polyStart]);
				}
			}
		}
		vertBase += faceVerts;
	}
	if (genTans) {
		ObjVertex::generateTangents(verts, flipG);
	}
	// Generate the indices
	std::vector<unsigned> remap(maxVerts);
	size_t numVerts = meshopt_generateVertexRemap(remap.data(), NULL, maxVerts, verts.data(), maxVerts, sizeof(ObjVertex));
	// Now create the buffers we'll be working with (overwriting any existing data)
	mesh.resize(numVerts, maxVerts);
	meshopt_remapIndexBuffer (mesh.index.data(), NULL, maxVerts, remap.data());
	meshopt_remapVertexBuffer(mesh.verts.data(), verts.data(), maxVerts, sizeof(ObjVertex), remap.data());
}

void extract(ufbx_mesh* const fbx, bool const genTans, bool const flipG, ObjMesh& mesh) {
	// Same as the fast_obj variant, create one big mesh
	ObjVertex::Container verts;
	size_t maxVerts = 0;
	for (size_t face = 0; face < fbx->num_faces; face++) {
		maxVerts += 3 * (fbx->faces[face].num_indices - 2);
	}
	verts.reserve(maxVerts);
	for (size_t face = 0; face < fbx->num_faces; face++) {
		size_t vertBase  = fbx->faces[face].index_begin;
		size_t faceVerts = fbx->faces[face].num_indices;
		size_t polyStart = verts.size();
		for (size_t vert = 0; vert < faceVerts; vert++) {
			if (vert > 2) {
				if ((vert & 1) != 0) {
					verts.push_back(verts[verts.size() - 1]);
				} else {
					verts.push_back(verts[polyStart]);
					verts.push_back(verts[verts.size() - 3]);
				}
			}
			verts.emplace_back(fbx, vertBase + vert);
			if (vert > 2) {
				if ((vert & 1) != 0) {
					verts.push_back(verts[polyStart]);
				}
			}
		}
	}
	/*
	 * It doesn't seem to matter (at least with ufbx importing) whether a Max
	 * file was exported with Z-up or Y-up, the result is the same (at least
	 * from the mesh's point of view). Going from Max to Modo, for example, with
	 * Y-up will add a root transform node, whereas Z-up seems to do the
	 * transform on import (though it could be tucked away in the pre-transform,
	 * I didn't look too much). Modo originated content is X-up.
	 *
	 * TL;DR: rotate by 90 on the X-axis if Max is the 'original_application' in
	 * the metadata (it probably needs more experimentation with other apps, but
	 * it's a simple enough rule for now).
	 */
	mat3 rot;
	rot.set(static_cast<float>(M_PI) / 2, 1.0f, 0.0f, 0.0f);
	for (ObjVertex::Container::iterator it = verts.begin(); it != verts.end(); ++it) {
		it->posn = rot.apply(it->posn);
		it->norm = rot.apply(it->norm);
	}
	if (genTans) {
		ObjVertex::generateTangents(verts, flipG);
	}
	std::vector<unsigned> remap(maxVerts);
	size_t numVerts = meshopt_generateVertexRemap(remap.data(), NULL, maxVerts, verts.data(), maxVerts, sizeof(ObjVertex));
	// Now create the buffers we'll be working with (overwriting any existing data)
	mesh.resize(numVerts, maxVerts);
	meshopt_remapIndexBuffer (mesh.index.data(), NULL, maxVerts, remap.data());
	meshopt_remapVertexBuffer(mesh.verts.data(), verts.data(), maxVerts, sizeof(ObjVertex), remap.data());
}
}

//*****************************************************************************/

bool ObjMesh::load(const char* const srcPath, bool const genTans, bool const flipG) {
	verts.clear();
	index.clear();
	scale = 1.0f;
	bias  = 0.0f;
	bool validSrc = false;
	if (srcPath) {
		size_t pathLen = strlen(srcPath);
		if (pathLen > 4) {
			if (strncmp(srcPath + (pathLen - 4), ".fbx", 4) == 0) {
				ufbx_load_opts opts = {};
				opts.ignore_animation   = true;
				opts.ignore_embedded    = true;
				opts.skip_skin_vertices = true;
				ufbx_scene* scene = ufbx_load_file(srcPath, &opts, NULL);

				for (size_t n = 0; n < scene->nodes.count; n++) {
					ufbx_node* node = scene->nodes.data[n];
					if (node->mesh) {
						printf("Mesh %s with %d faces\n", node->name.data, (int) node->mesh->faces.count);
						impl::extract(node->mesh, genTans, flipG, *this);
						validSrc = true;
						continue;
					}
				}

				ufbx_free_scene(scene);
			}
		}
		if (fastObjMesh* obj = fast_obj_read(srcPath)) {
			/*
			 * If fast_obj can open a file it will always return a mesh object,
			 * so we need to perform some minimal validation.
			 */
			if (obj->face_count > 0) {
				impl::extract(obj, genTans, flipG, *this);
				validSrc = true;
			}
			fast_obj_destroy(obj);
		}
	}
	return validSrc;
}

/**
 * Scale the mesh positions so that each is normalised between \c -1 and \c 1.
 *
 * \param[in,out] mesh mesh to in-place scale
 * \param[in] uniform \c true if the same scale should be applied to all axes (otherwise a per-axis scale is applied)
 * \param[in] unbiased  \c true if the origin should be maintained at zero (otherwise a bias is applied to make the most of the normalised range)
 */
void ObjMesh::normalise(bool const uniform, bool const unbiased) {
	// Get min and max for each component
	vec3 minPosn({ FLT_MAX,  FLT_MAX,  FLT_MAX});
	vec3 maxPosn({-FLT_MAX, -FLT_MAX, -FLT_MAX});
	for (std::vector<ObjVertex>::iterator it = verts.begin(); it != verts.end(); ++it) {
		minPosn = vec3::min(minPosn, it->posn);
		maxPosn = vec3::max(maxPosn, it->posn);
	}
	// Which gives the global mesh scale and offset
	scale = (maxPosn - minPosn);
	if (!unbiased) {
		scale = scale / 2.0f;
	}
	// Clamp scale so we don't divide-by-zero on 2D meshes
	scale = vec3::max(scale, vec3(O2B_SMALL_VERT_POS, O2B_SMALL_VERT_POS, O2B_SMALL_VERT_POS));
	if (uniform) {
		// Uniform needs to be max(), otherwise the verts could be clamped
		scale = std::max(std::max(scale.x, scale.y), scale.z);
	}
	// Optionally bias to make the most of the range
	if (!unbiased) {
		bias  = (maxPosn + minPosn) / 2.0f;
	}
	// Apply to each vert to normalise
	for (std::vector<ObjVertex>::iterator it = verts.begin(); it != verts.end(); ++it) {
		it->posn = (it->posn - bias) / scale;
	}
}

void ObjMesh::optimise() {
	meshopt_optimizeVertexCache(index.data(), index.data(), index.size(), verts.size());
	meshopt_optimizeOverdraw   (index.data(), index.data(), index.size(), verts[0].posn, verts.size(), sizeof(ObjVertex), 1.01f);
	meshopt_optimizeVertexFetch(verts.data(), index.data(), index.size(), verts.data(),  verts.size(), sizeof(ObjVertex));
}

void ObjMesh::resize(size_t const numVerts, size_t const numIndex) {
	verts.resize(numVerts);
	index.resize(numIndex);
}
