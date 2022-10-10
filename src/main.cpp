/**
 * \file main.cpp
 * Wavefront \c .obj to packed buffer.
 *
 * \todo read again the HemiOct16 details, page 27 here: https://jcgt.org/published/0003/02/01/paper.pdf
 * \todo and compare with https://gist.github.com/Niadb/794ea32f856820bc7e4f5a67c4246791
 */

#include <cfloat>
#include <cstdlib>
#include <cstdio>

#include <chrono>

#include "meshoptimizer.h"

#include "fileutils.h"
#include "objvertex.h"
#include "tooloptions.h"

/**
 * The \c obj file as a vertex and index data.
 */
struct ObjMesh
{
	/**
	 * Creates a zero-sized buffer.
	 */
	ObjMesh()
		: scale(1, 1, 1)
		, bias (0, 0, 0) {};
	/**
	 * Resizes the buffers (usually as a prelude to filling them).
	 */
	void resize(size_t const numVerts, size_t const numIndex) {
		verts.resize(numVerts);
		index.resize(numIndex);
	}
	/**
	 * Collection of (usually) unique vertices referenced by \c #index.
	 */
	ObjVertex::Container verts;
	/**
	 * Collection of indices into \c #verts.
	 */
	std::vector<unsigned> index;
	/**
	 * Scale to apply to each vertex position when drawing (the default is \c 1).
	 */
	vec3 scale;
	/**
	 * Offset to apply to each vertex position when drawing (the default is \c 0).
	 */
	vec3 bias;
};

/*
 * Output buffer descriptor. What the interleaved offsets are, where attributes
 * are packed, etc.
 */
struct BufDesc
{
	BufDesc()
		: padPosn(false)
		, padUv_0(false)
		, padNorm(false) {}
	bool padPosn;
	bool padUv_0;
	bool padNorm;
};

/**
 * Helper to return the current time in milliseconds.
 *
 * \return elapsed time in milliseconds (only valid for calculating time differences)
 */
/*static*/ unsigned millis() {
	return static_cast<unsigned>(std::chrono::duration_cast<std::chrono::milliseconds>(
						  std::chrono::steady_clock::now().time_since_epoch()).count());
}

/**
 * Extracts the mesh data as vertex and index buffers.
 *
 * \todo indices should be optional (since they're optional for export) unless we generate then manually export the tris (since we miss out on other meshopt features otherwise)
 *
 * \param[in] obj valid \c fast_obj content
 * \param[in] genTans \c true if tangents should be generated
 * \param[out] mesh destination for the \c .obj file content
 */
void extract(fastObjMesh* const obj, bool const genTans, ObjMesh& mesh) {
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
		ObjVertex::generateTangents(verts);
	}
	// Generate the indices
	std::vector<unsigned> remap(maxVerts);
	size_t numVerts = meshopt_generateVertexRemap(remap.data(), NULL, maxVerts, verts.data(), maxVerts, sizeof(ObjVertex));
	// Now create the buffers we'll be working with (overwriting any existing data)
	mesh.resize(numVerts, maxVerts);
	meshopt_remapIndexBuffer (mesh.index.data(), NULL, maxVerts, remap.data());
	meshopt_remapVertexBuffer(mesh.verts.data(), verts.data(), maxVerts, sizeof(ObjVertex), remap.data());
}

/**
 * Opens an \c .obj file and extract its content into \a mesh.
 *
 * \param[in] srcPath filename of the \c .obj file
 * \param[in] genTans \c true if tangents should be generated
 * \param[out] mesh destination for the \c .obj file content
 * \return \c true if the file was valid and \a mesh has its content
 */
bool open(const char* const srcPath, bool const genTans, ObjMesh& mesh) {
	if (srcPath) {
		if (fastObjMesh* obj = fast_obj_read(srcPath)) {
			extract(obj, genTans, mesh);
			fast_obj_destroy(obj);
			return true;
		}
	}
	return false;
}

/**
 * Scale the mesh positions so that each is normalised between \c -1 and \c 1.
 *
 * \param[in,out] mesh mesh to in-place scale
 * \param[in] unbiased  \c true if the origin should be maintained at zero (otherwise a bias is applied to make the most of the normalised range)
 */
void scale(ObjMesh& mesh, bool const unbiased = false) {
	(void) unbiased;
	// Get min and max for each component
	vec3 minPosn({ FLT_MAX,  FLT_MAX,  FLT_MAX});
	vec3 maxPosn({-FLT_MAX, -FLT_MAX, -FLT_MAX});
	for (std::vector<ObjVertex>::iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		minPosn = vec3::min(minPosn, it->posn);
		maxPosn = vec3::max(maxPosn, it->posn);
	}
	// Which gives the global mesh scale and offset
	mesh.scale = (maxPosn - minPosn) * 0.5f;
	mesh.bias  = (maxPosn + minPosn) * 0.5f;
	// Apply to each vert to normalise
	for (std::vector<ObjVertex>::iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		it->posn = (it->posn - mesh.bias) / mesh.scale;
	}
}

/**
 * Run meshopt's various processes.
 *
 * \param[in,out] mesh mesh to in-place optimise
 */
void optimise(ObjMesh& mesh) {
	meshopt_optimizeVertexCache(mesh.index.data(), mesh.index.data(), mesh.index.size(), mesh.verts.size());
	meshopt_optimizeOverdraw   (mesh.index.data(), mesh.index.data(), mesh.index.size(), mesh.verts[0].posn, mesh.verts.size(), sizeof(ObjVertex), 1.01f);
	meshopt_optimizeVertexFetch(mesh.verts.data(), mesh.index.data(), mesh.index.size(), mesh.verts.data(),  mesh.verts.size(), sizeof(ObjVertex));
}

/**
 * Load and convert.
 */
int main(int argc, const char* argv[]) {
	ObjMesh mesh;
	// Gather files and tool options
	const char* srcPath = nullptr;
	const char* dstPath = nullptr;
	ToolOptions opts;
	int srcIdx = opts.parseArgs(argv, argc);
	if (srcIdx < argc) {
		srcPath = argv[srcIdx];
		if (srcIdx + 1 < argc) {
			dstPath = argv[srcIdx + 1];
		} else {
			if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_ASCII_FILE)) {
				dstPath = "out.inc";
			} else {
				dstPath = "out.bin";
			}
		}
	}
	opts.dump();
	// Now we start
	if (!open(srcPath, opts.tans != VertexPacker::EXCLUDE, mesh)) {
		fprintf(stderr, "Unable to read: %s\n", (srcPath) ? srcPath : "null");
		return EXIT_FAILURE;
	}
	// Perform an in-place scale/bias if requested
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_POSITIONS_SCALE)) {
		scale(mesh, O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SCALE_NO_BIAS));
	}
	// Then the various optimisations
	optimise(mesh);
	printf("\n");
	printf("Vertices: %d\n", static_cast<int>(mesh.verts.size()));
	printf("Indices:  %d\n", static_cast<int>(mesh.index.size()));
	// Maximum buffer size: vert posn, norm, uv, tans, bitans + indices
	size_t const maxBufBytes = mesh.verts.size() * sizeof(float) * (3 + 3 + 2 + 3 + 3)
							 + mesh.index.size() * sizeof(uint32_t);
	std::vector<uint8_t> backing(maxBufBytes);
	// Tool options to packer options
	unsigned packOpts = VertexPacker::OPTS_DEFAULT;
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BIG_ENDIAN)) {
		packOpts |= VertexPacker::OPTS_BIG_ENDIAN;
	}
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SIGNED_LEGACY)) {
		packOpts |= VertexPacker::OPTS_SIGNED_LEGACY;
	}
	VertexPacker packer(backing.data(), maxBufBytes, packOpts);
	// Pack the vertex data
	for (ObjVertex::Container::const_iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		/*
		 * TODO: encoding normals and tangents
		 * TODO: general padding
		 * TODO: packing bitangents' sign wherever possible
		 */
		if (opts.posn != VertexPacker::EXCLUDE) {
			it->posn.store(packer, opts.posn);
		}
		if (opts.text != VertexPacker::EXCLUDE) {
			it->uv_0.store(packer, opts.text);
		}
		if (opts.norm != VertexPacker::EXCLUDE) {
			it->norm.store(packer, opts.norm);
		}
		if (opts.tans != VertexPacker::EXCLUDE) {
			it->tans.store(packer, opts.tans);
			it->btan.store(packer, opts.tans);
		}
	}
	// TODO: we shouldn't need to align this (since each vertex should be aligned)
	packer.align();
	size_t vertexBytes = packer.size();
	// Add the indices
	if (opts.idxs != VertexPacker::EXCLUDE) {
		for (std::vector<unsigned>::const_iterator it = mesh.index.begin(); it != mesh.index.end(); ++it) {
			packer.add(static_cast<int>(*it), opts.idxs);
		}
	}
	packer.align();
	size_t totalBytes = packer.size();
	printf("\n");
	printf("Vertex bytes: %d\n", static_cast<int>(vertexBytes));
	printf("Index bytes:  %d\n", static_cast<int>(totalBytes - vertexBytes));
	printf("Total bytes:  %d\n", static_cast<int>(totalBytes));
	// Write the result
	bool written = write(dstPath, backing.data(), packer.size(),
		O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_ASCII_FILE),
		O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_COMPRESS_ZSTD));
	if (!written) {
		fprintf(stderr, "Unable to write: %s\n", (dstPath) ? dstPath : "null");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
