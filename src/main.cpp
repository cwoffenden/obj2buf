/**
 * \file main.cpp
 * Wavefront \c .obj to packed buffer.
 *
 * A common command-line test would be:
 * \code
 *	obj2buf -p short -u short -n byte -t byte -su -o -g -b -m -a in.obj out.inc
 * \endcode
 * Positions and UVs as shorts, normals and tangents encoded and packed as
 * bytes, positions uniformly scaled, octahedral encoded normals and tangents,
 * inverted Max-style G-channel for the normal map, only store the bitangent
 * sign (implicitly packed in the position's W-component), metadata header.
 *
 * Alternatively the above command-line could be replaced with a \e shortcode:
 * \code
 *	obj2buf -c 8115547B in.obj out.inc
 * \endcode
 */

#include <cstdlib>
#include <cstdio>

#include <chrono>

#include "bufferlayout.h"
#include "fileutils.h"
#include "objmesh.h"
#include "tooloptions.h"

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
	unsigned const startMs = millis();
	bool const tans = opts.tans != VertexPacker::Storage::EXCLUDE;
	bool const flip = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_TANGENTS_FLIP_G);
	if (!mesh.load(srcPath, tans, flip)) {
		fprintf(stderr, "Unable to read: %s\n", (srcPath) ? srcPath : "null");
		return EXIT_FAILURE;
	}
	// Perform an in-place scale/bias if requested
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_POSITIONS_SCALE)) {
		mesh.normalise(O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SCALE_UNIFORM),
					   O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_SCALE_NO_BIAS));
	}
	// In-place normals/tangents/bitangents encode (into the X/Y components)
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED)) {
		ObjVertex::encodeNormals(mesh.verts, opts.norm, opts.tans,
			!O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BITANGENTS_SIGN));
	}
	// Then the various optimisations
	// TODO: if we want to compare meshes, this needs to go before in-place changes
	mesh.optimise();
	printf("\n");
	printf("Vertices:  %d\n", static_cast<int>(mesh.verts.size()));
	printf("Indices:   %d\n", static_cast<int>(mesh.index.size()));
	printf("Triangles: %d\n", static_cast<int>(mesh.index.size() / 3));
	// Maximum buffer size: metadata + vert posn, norm, UVs, tans, bitans + indices
	size_t const maxBufBytes = 54 + 20 //<-- max metadata size
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
	VertexPacker::Failed failed = false;
	VertexPacker packer(backing.data(), maxBufBytes, packOpts);
	unsigned preOffBytes = 0;
	unsigned offsetBytes = 0;
	if (O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_WRITE_METADATA)) {
		// Endianness test/file magic
		packer.add(0xBDA7, VertexPacker::Storage::UINT16C);
		// Serialised tool 'shortcode' for exporting
		packer.add(opts.getAllOptions(), VertexPacker::Storage::UINT32C);
		// Metadata offsets placeholder (retroactively written after the content)
		preOffBytes = static_cast<unsigned>(packer.size());
		for (int n = 0; n < 5; n++) {
			failed |= packer.add(0, VertexPacker::Storage::UINT32C);
		}
		offsetBytes = static_cast<unsigned>(packer.size()) - preOffBytes;
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
		// Overwrite in the space for the offsets we reserved earlier
		VertexPacker header(backing.data() + preOffBytes, offsetBytes, packOpts);
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
	printf("\n");
	printf("Source file: %s\n", ToolOptions::filename(srcPath));
	printf("Destination: %s\n", ToolOptions::filename(dstPath));
	printf("Total time:  %dms\n", millis() - startMs);
	return EXIT_SUCCESS;
}
