/**
 * \file main.cpp
 * Wavefront \c .obj to packed buffer.
 *
 * A common command-line test would be:
 * \code
 *	obj2buf -p short -u short -n byte -t byte -su -e -g -b -m -a
 * \endcode
 * Positions and UVs as shorts, normals and tangents encoded and packed as
 * bytes, positions uniformly scaled, octahedral encoded normals and tangents,
 * inverted Max-style G-channel for the normal map, only store the bitangent
 * sign (implicitly packed in the position's W-component), metadata header.
 */

#include <cfloat>
#include <cstdlib>
#include <cstdio>

#include <chrono>

#include "meshoptimizer.h"

#include "bufferlayout.h"
#include "fileutils.h"
#include "objvertex.h"
#include "tooloptions.h"

/**
 * \def O2B_SMALL_VERT_POS
 * Value that's considered \e small for a vertex position. Values above this can
 * be normalised, below this no processing is done. We choose 1/127, the LSB in
 * a signed 8-bit range (it's just small, and a number had to be picked).
 */
#ifndef O2B_SMALL_VERT_POS
#define O2B_SMALL_VERT_POS (1.0f / 127.0f)
#endif

/**
 * The \c obj file as vertex and index data. The mesh is loaded into here,
 * manipulated in-place, then saved out. Once the process starts, if the
 * original mesh data is needed it will need to be reloaded.
 */
struct ObjMesh
{
	/**
	 * Creates a zero-sized buffer.
	 */
	ObjMesh()
		: scale(1.0f, 1.0f, 1.0f)
		, bias (0.0f, 0.0f, 0.0f) {};
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
	 * Scale to apply to each vertex position when drawing (the default is \c 1.0).
	 */
	vec3 scale;
	/**
	 * Offset to apply to each vertex position when drawing (the default is \c 0.0).
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

/**
 * Opens an \c .obj file and extract its content into \a mesh.
 *
 * \param[in] srcPath filename of the \c .obj file
 * \param[in] genTans \c true if tangents should be generated
 * \param[in] flipG generate tangents for a flipped green channel (by negating the texture's y-axis)
 * \param[out] mesh destination for the \c .obj file content
 * \return \c true if the file was valid and \a mesh has its content
 */
bool open(const char* const srcPath, bool const genTans, bool const flipG, ObjMesh& mesh) {
	if (srcPath) {
		if (fastObjMesh* obj = fast_obj_read(srcPath)) {
			extract(obj, genTans, flipG, mesh);
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
 * \param[in] uniform \c true if the same scale should be applied to all axes (otherwise a per-axis scale is applied)
 * \param[in] unbiased  \c true if the origin should be maintained at zero (otherwise a bias is applied to make the most of the normalised range)
 */
void scale(ObjMesh& mesh, bool const uniform, bool const unbiased) {
	// Get min and max for each component
	vec3 minPosn({ FLT_MAX,  FLT_MAX,  FLT_MAX});
	vec3 maxPosn({-FLT_MAX, -FLT_MAX, -FLT_MAX});
	for (std::vector<ObjVertex>::iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		minPosn = vec3::min(minPosn, it->posn);
		maxPosn = vec3::max(maxPosn, it->posn);
	}
	// Which gives the global mesh scale and offset
	mesh.scale = (maxPosn - minPosn);
	if (!unbiased) {
		mesh.scale = mesh.scale / 2.0f;
	}
	// Clamp scale so we don't divide-by-zero on 2D meshes
	mesh.scale = vec3::max(mesh.scale, vec3(O2B_SMALL_VERT_POS, O2B_SMALL_VERT_POS, O2B_SMALL_VERT_POS));
	if (uniform) {
		// Uniform needs to be max(), otherwise the verts could be clamped
		mesh.scale = std::max(std::max(mesh.scale.x, mesh.scale.y), mesh.scale.z);
	}
	// Optionally bias to make the most of the range
	if (!unbiased) {
		mesh.bias  = (maxPosn + minPosn) / 2.0f;
	}
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
	// Decide how the options create the buffer layout
	BufferLayout const layout(opts);
	// Now we start
	bool tans = opts.tans != VertexPacker::Storage::EXCLUDE;
	bool flip = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_TANGENTS_FLIP_G);
	if (!open(srcPath, tans, flip, mesh)) {
		fprintf(stderr, "Unable to read: %s\n", (srcPath) ? srcPath : "null");
		return EXIT_FAILURE;
	}
	// Perform an in-place scale/bias if requested
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_POSITIONS_SCALE)) {
		scale(mesh, O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SCALE_UNIFORM),
					O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SCALE_NO_BIAS));
	}
	// In-place normals/tangents/bitangents encode (into the X/Y components)
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED)) {
		ObjVertex::encodeNormals(mesh.verts, tans,
			!O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BITANGENTS_SIGN),
				std::max(opts.norm.fractionBits(), opts.tans.fractionBits()));
	}
	// Then the various optimisations
	optimise(mesh);
	printf("\n");
	printf("Vertices:  %d\n", static_cast<int>(mesh.verts.size()));
	printf("Indices:   %d\n", static_cast<int>(mesh.index.size()));
	printf("Triangles: %d\n", static_cast<int>(mesh.index.size() / 3));
	// Maximum buffer size: metadata + vert posn, norm, UVs, tans, bitans + indices
	size_t const maxBufBytes = 68 //<-- max metadata size
			+ std::max(mesh.verts.size(), mesh.index.size())
				* sizeof(float) * (3 + 3 + 2 + 3 + 3)
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
	// Pack the vertex data
	VertexPacker packer(backing.data(), maxBufBytes, packOpts);
	VertexPacker::Failed failed = false;
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_WRITE_METADATA)) {
		// Endianness test/file magic
		packer.add(0xBDA7, VertexPacker::Storage::UINT16C);
		// Metadata offsets placeholder (retroactively written after the content)
		for (int n = 0; n < 5; n++) {
			failed |= packer.add(0, VertexPacker::Storage::UINT32C);
		}
		// Mesh scale/bias (more than likely not used but it's only 24 bytes)
		failed |= mesh.scale.store(packer, VertexPacker::Storage::FLOAT32);
		failed |= mesh.bias.store (packer, VertexPacker::Storage::FLOAT32);
		// Buffer layout (attributes, sizes, offset, etc.)
		failed |= layout.writeHeader(packer);
	}
	unsigned headerBytes = static_cast<unsigned>(packer.size());
	unsigned vertexBytes = 0;
	unsigned indexBytes  = 0;
	if (opts.idxs) {
		// Indexed vertices
		for (ObjVertex::Container::const_iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
			failed |= layout.writeVertex(packer, *it, headerBytes);
		}
		vertexBytes = static_cast<unsigned>(packer.size()) - headerBytes;
		// Add the indices
		for (std::vector<unsigned>::const_iterator it = mesh.index.begin(); it != mesh.index.end(); ++it) {
			failed |= packer.add(static_cast<int>(*it), opts.idxs);
		}
		indexBytes  = static_cast<unsigned>(packer.size()) - (headerBytes + vertexBytes);
	} else {
		// Manually write unindexed vertices from the indices
		for (std::vector<unsigned>::const_iterator it = mesh.index.begin(); it != mesh.index.end(); ++it) {
			unsigned idx = static_cast<unsigned>(*it);
			if (idx < mesh.verts.size()) {
				failed |= layout.writeVertex(packer, mesh.verts[idx], headerBytes);
			}
		}
		vertexBytes = static_cast<unsigned>(packer.size()) - headerBytes;
	}
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_WRITE_METADATA)) {
		// Overwrite in the space for 5x ints we reserved earlier (+2 for the magic)
		VertexPacker header(backing.data() + 2, 20, packOpts);
		failed |= header.add(headerBytes,  VertexPacker::Storage::UINT32C);
		failed |= header.add(vertexBytes,  VertexPacker::Storage::UINT32C);
		failed |= header.add(headerBytes + vertexBytes, VertexPacker::Storage::UINT32C);
		failed |= header.add(indexBytes,   VertexPacker::Storage::UINT32C);
		failed |= header.add((opts.idxs) ? static_cast<int>(mesh.index.size()) : 0, VertexPacker::Storage::UINT32C);
	}
	if (failed) {
		printf("Buffer packing failed (bytes used: %d)\n", vertexBytes + indexBytes);
	}
	// Dump the buffer sizes and GL layout calls
	printf("\n");
	printf("Header bytes: %d\n", headerBytes);
	printf("Vertex bytes: %d\n", vertexBytes);
	printf("Index bytes:  %d\n", indexBytes);
	printf("Total bytes:  %d\n", headerBytes + vertexBytes + indexBytes);
	printf("\n");
	layout.dump();
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
