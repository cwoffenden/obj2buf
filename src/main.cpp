/**
 * \file main.cpp
 * Wavefront \c .obj to packed buffer.
 *
 * \todo read again the HemiOct16 details, page 27 here: https://jcgt.org/published/0003/02/01/paper.pdf
 */

#include <cfloat>
#include <cstdlib>
#include <cstdio>

#include <chrono>
#include <vector>

#include "meshoptimizer.h"
#include "fast_obj.h"

#include "tooloptions.h"
#include "vertexpacker.h"
#include "minifloat.h"

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
		: scale (1, 1, 1)
		, offset(0, 0, 0) {};
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
	vec3 offset;
};

/**
 * Helper to return the current time in milliseconds.
 *
 * \return elapsed time in milliseconds (only valid for calculating time differences)
 */
static unsigned millis() {
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
 * Scale the mesh positions so that each is normalised between \c -1 and \c 1.
 */
void scale(ObjMesh& mesh) {
	// Get min and max for each component
	vec3 minPosn({ FLT_MAX,  FLT_MAX,  FLT_MAX});
	vec3 maxPosn({-FLT_MAX, -FLT_MAX, -FLT_MAX});
	for (std::vector<ObjVertex>::iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		minPosn = vec3::min(minPosn, it->posn);
		maxPosn = vec3::max(maxPosn, it->posn);
	}
	// Which gives the global mesh scale and offset
	mesh.scale  = (maxPosn - minPosn) * 0.5f;
	mesh.offset = (maxPosn + minPosn) * 0.5f;
	// Apply to each vert to normalise
	for (std::vector<ObjVertex>::iterator it = mesh.verts.begin(); it != mesh.verts.end(); ++it) {
		it->posn = (it->posn - mesh.offset) / mesh.scale;
	}
}

/**
 * Optimise (and eventually export) the mesh.
 */
void process(ObjMesh& mesh) {
	meshopt_optimizeVertexCache(mesh.index.data(), mesh.index.data(), mesh.index.size(), mesh.verts.size());
	meshopt_optimizeOverdraw   (mesh.index.data(), mesh.index.data(), mesh.index.size(), mesh.verts[0].posn, mesh.verts.size(), sizeof(ObjVertex), 1.01f);
	meshopt_optimizeVertexFetch(mesh.verts.data(), mesh.index.data(), mesh.index.size(), mesh.verts.data(),  mesh.verts.size(), sizeof(ObjVertex));
}

#ifdef _MSC_VER
#include <directxpackedvector.h>
#endif
#include <cmath>
utils::float16 native32To16(float val) {
#if defined(__GNUC__) || defined(__llvm__)
	union {
		_Float16 f; // where we write
		uint16_t u; // where we read
	} bits = {
		static_cast<_Float16>(val)
	};
	return bits.u;
#else
	return DirectX::PackedVector::XMConvertFloatToHalf(val);
#endif
}
float native16To32(utils::float16 val) {
#if defined(__GNUC__) || defined(__llvm__)
	union {
		uint16_t u; // where we write
		_Float16 f; // where we read
	} bits = {
		val
	};
	return static_cast<float>(bits.f);
#else
	return DirectX::PackedVector::XMConvertHalfToFloat(val);
#endif
}

struct FloatStats {
	unsigned floats;
	unsigned errors;
	unsigned wayOff;
	unsigned minVal;
	unsigned maxVal;
	FloatStats()
		: floats(0)
		, errors(0)
		, wayOff(0)
		, minVal(UINT_MAX)
		, maxVal(0) {}
	void compare(utils::float16 known, utils::float16 target) {
		floats++;
		if (known != target) {
			errors++;
			if (abs(known - target) > 1 && !utils::halfIsNaN(target)) {
				wayOff++;
			}
		}
		if (known < minVal) {
			minVal = known;
		}
		if (known > maxVal) {
			maxVal = known;
		}
	}
	// compares comparison a float with both the native and our implementation
	void compare(float val) {
		compare(native32To16(val), utils::floatToHalf(val));
	}
	// converts a float16 back to float32 then calls compare(float)
	void compare(utils::float16 target) {
		compare(native16To32(target));
	}
	void print() {
		printf("Total floats: %d\n", floats);
		printf("Total errors: %d\n", errors);
		printf("Way off entries %d\n", wayOff);
		printf("Error rate: %.3f%%\n", errors * 100.0 / floats);
		printf("Range: 0x%04X - 0x%04X\n", minVal, maxVal);
	}
};

void testMinifloat() {
	FloatStats stats;
	// big numbers
	for (float n = -FLT16_MAX; n <= FLT16_MAX; n += 0.0025f) {
		stats.compare(n);
	}
	// small numbers
	for (float n = -1.0f; n <= 1.0f; n += 0.000001f) {
		stats.compare(n);
	}
	// zeroes, infs and NaN
	stats.compare(0.0f);
	stats.compare(-0.0f);
	stats.compare(HUGE_VALF);
	stats.compare(-HUGE_VALF);
	stats.compare(NAN);
	// All variants, including signalling NaNs
	for (int n = 0; n <= 0xFFFF; n++) {
	//	stats.compare(static_cast<utils::float16>(n));
	}
	stats.print();
}

/**
 * Load and convert.
 */
int main(int argc, const char* argv[]) {
	(void) argc;
	(void) argv;
	unsigned time = millis();
	testMinifloat();
	time = millis() - time;
	printf("Processing took: %dms\n", time);
	/*
	const char* file = DEFAULT_TEST_FILE;
	if (argc < 2) {
		if (argc == 1) {
			printf("Usage: %s in.obj [out.dat]\n", ToolOptions::extractFilename(argv[0]));
		}
	} else {
		file = argv[1];
	}
	printf("Opening file: %s\n", ToolOptions::extractFilename(file));
	unsigned time = millis();
	if (fastObjMesh* obj = fast_obj_read(file)) {
		ObjMesh mesh;
		printf("Parsing...\n");
		extract(obj, mesh);
		fast_obj_destroy(obj);
		scale(mesh);
		process(mesh);
		time = millis() - time;
		printf("Vertices: %d, Indices %d\n", static_cast<int>(mesh.verts.size()), static_cast<int>(mesh.index.size()));
		printf("Processing took: %dms\n", time);
	} else {
		printf("Error reading\n");
	}
	 */
	/*
	std::vector<uint8_t> temp(12);
	VertexPacker out(temp.data(), temp.size());
	
	out.add(-1.0f, VertexPacker::SINT08N);
	out.add( 0.0f, VertexPacker::SINT08N);
	out.add( 1.0f, VertexPacker::SINT08N);

	out.add(-1.0f, VertexPacker::SINT08C);
	out.add( 0.0f, VertexPacker::SINT08C);
	out.add( 1.0f, VertexPacker::SINT08C);

	out.add(-1.0f, VertexPacker::UINT08N);
	out.add( 0.0f, VertexPacker::UINT08N);
	out.add( 1.0f, VertexPacker::UINT08N);

	out.add(-1.0f, VertexPacker::UINT08C);
	out.add( 0.0f, VertexPacker::UINT08C);
	out.add( 1.0f, VertexPacker::UINT08C);
	 */
	return EXIT_SUCCESS;
}
