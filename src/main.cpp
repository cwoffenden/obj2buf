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
#include <vector>

#include "fast_obj.h"
#include "meshoptimizer.h"
#include "zstd.h"

#include "tooloptions.h"
#include "vertexpacker.h"

/**
 * \def DEFAULT_TEST_FOLDER
 * \e Horrible workaround for launching from a CMake project in Xcode and MSVS
 * (Xcode needs the relative path, VS knows to launch from the \c dat folder,
 * which doesn't appear to be exposed in Xcode).
 */
#ifndef _MSC_VER
#define DEFAULT_TEST_FOLDER "../../dat/"
#else
#define DEFAULT_TEST_FOLDER
#endif

/**
 * \def DEFAULT_TEST_FILE
 * Default .obj file to load if none is supplied.
 */
#define DEFAULT_TEST_FILE DEFAULT_TEST_FOLDER "bunny.obj"

/**
 * Container for the vertex data extracted from an \c obj file.
 */
struct ObjVertex
{
	/**
	 * Uninitialised vertex data.
	 */
	ObjVertex() = default;
	/**
	 * Constructs a single vertex from the \c obj data, extracting the relevant
	 * position, normal and UV data.
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
	}
	vec3 posn;
	vec3 norm;
	vec2 uv_0;
};

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
	void resize(size_t numVerts, size_t numIndex) {
		verts.resize(numVerts);
		index.resize(numIndex);
	}
	/**
	 * Collection of (usually) unique vertices referenced by \c #index.
	 */
	std::vector<ObjVertex> verts;
	/**
	 * Collection of indices into \c #verts.
	 */
	std::vector<unsigned>  index;
	/**
	 * Scale to apply to each vertex position when drawing (the default is \c 1).
	 */
	vec3 scale;
	/**
	 * Offset to apply to each vertex position when drawing (the default is \c 0).
	 */
	vec3 bias;
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
 * \param[in] obj valid \c fast_obj content
 * \param[out] mesh destination for the \c .obj file content
 */
void extract(fastObjMesh* obj, ObjMesh& mesh) {
	// No objects or groups, just one big triangle mesh from the file
	std::vector<ObjVertex> verts;
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
				 * We create fans for any faces with more than three verts (which will only
				 * work for convex polys, but this should really be processing only tris or
				 * quads). We create fans as [0, 1, 2], [2, 3, 0], [0, 3, 4], etc., with
				 * each added tri using the last tri's vert as its starting point, not for
				 * vert buffer locality but for compression.
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
 * \param[out] mesh destination for the \c .obj file content
 * \return \c true if the file was valid and \a mesh has its content
 */
bool open(const char* const srcPath, ObjMesh& mesh) {
	if (srcPath) {
		if (fastObjMesh* obj = fast_obj_read(srcPath)) {
			extract(obj, mesh);
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
 * Helper to write a buffer to a file.
 *
 * \param[in] dstPath filename of the destination file
 * \param[in] data start of the raw data
 * \param[in] size number of bytes to write
 * \return \c true if writing the requested number of bytes was successful
 */
bool write(const char* const dstPath, const void* const data, size_t const size) {
	if (dstPath && data) {
		if (FILE *dstFile = fopen(dstPath, "wb")) {
			size_t wrote = fwrite(data, 1, size, dstFile);
			if (fclose(dstFile) == 0) {
				return wrote == size;
			}
		}
	}
	return false;
}

/**
 * Helper to write a buffer to a file with optional Zstandard compression.
 *
 * \param[in] dstPath filename of the destination file
 * \param[in] data start of the raw data
 * \param[in] size number of bytes to write
 * \param[in] zstd \c true if the file should be compressed with Zstandard
 * \return \c true if writing the requested number of bytes was successful
 */
bool write(const char* const dstPath, const void* const data, size_t const size, bool const zstd) {
	bool success = false;
	if (data) {
		if (zstd) {
			/*
			 * We use the simple API to compress in one go the entire buffer.
			 */
			size_t bounds = ZSTD_compressBound(size);
			void* compBuf = malloc(bounds);
			if (compBuf) {
				size_t compSize = ZSTD_compress(compBuf, bounds, data, size, ZSTD_maxCLevel());
				if (!ZSTD_isError(compSize)) {
					success = write(dstPath, compBuf, compSize);
				}
				free(compBuf);
			}
		} else {
			success = write(dstPath, data, size);
		}
	}
	return success;
}

/**
 * Load and convert.
 */
int main(int argc, const char* argv[]) {
	ObjMesh mesh;
	// Gather files and tool options
	const char* srcPath = DEFAULT_TEST_FILE;
	const char* dstPath = nullptr;
	ToolOptions opts;
	int srcIdx = opts.parseArgs(argv, argc);
	if (srcIdx < argc) {
		srcPath = argv[srcIdx];
		if (srcIdx + 1 < argc) {
			dstPath = argv[srcIdx + 1];
		}
	}
	// Now we start
	if (!open(srcPath, mesh)) {
		fprintf(stderr, "Unable to read: %s\n", (srcPath) ? srcPath : "null");
		return EXIT_FAILURE;
	}
	// Perform an in-place scale/bias if requested
	if ((opts.opts & ToolOptions::OPTS_POSITIONS_SCALE)) {
		scale(mesh, opts.opts & ToolOptions::OPTS_SCALE_NO_BIAS);
	}
	// Then the various optimisations
	optimise(mesh);
	printf("Vertices: %d\n", static_cast<int>(mesh.verts.size()));
	printf("Indices:  %d\n", static_cast<int>(mesh.index.size()));
	// Maximum buffer size: vert posn, norm, uv + indices
	size_t const maxBufBytes = mesh.verts.size() * sizeof(float) * (3 + 3 + 2)
							 + mesh.index.size() * sizeof(uint32_t);
	std::vector<uint8_t> backing(maxBufBytes);
	// Tool options to packer options
	unsigned packOpts = VertexPacker::OPTS_DEFAULT;
	if ((opts.opts & ToolOptions::OPTS_BIG_ENDIAN)) {
		packOpts |= VertexPacker::OPTS_BIG_ENDIAN;
	}
	if ((opts.opts & ToolOptions::OPTS_SIGNED_LEGACY)) {
		packOpts |= VertexPacker::OPTS_SIGNED_LEGACY;
	}
	VertexPacker packer(backing.data(), maxBufBytes, packOpts);
	for (std::vector<ObjVertex>::const_iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		if (!(opts.opts & ToolOptions::OPTS_SKIP_POSITIONS)) {
			packer.add(it->posn, VertexPacker::FLOAT32);
		}
		if (!(opts.opts & ToolOptions::OPTS_SKIP_NORMALS)) {
			packer.add(it->norm, VertexPacker::FLOAT32);
		}
		if (!(opts.opts & ToolOptions::OPTS_SKIP_TEXTURE_UVS)) {
			packer.add(it->uv_0, VertexPacker::FLOAT32);
		}
	}
	printf("Vertex buffer bytes: %d\n", static_cast<int>(packer.bytes()));

	bool zstd = (opts.opts & ToolOptions::OPTS_COMPRESS_ZSTD);
	//bool text = (opts.opts & ToolOptions::OPTS_ASCII_FILE);
	if (!write(dstPath, backing.data(), packer.bytes(), zstd)) {
		fprintf(stderr, "Unable to write: %s\n", (dstPath) ? dstPath : "null");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
