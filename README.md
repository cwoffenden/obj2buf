# Wavefront `.obj` to Interleaved Buffer

[![CMake macOS/Windows/Linux](/../../actions/workflows/cmake-desktop.yml/badge.svg)](/../../actions/workflows/cmake-desktop.yml)

When writing quick tests or graphics experiements there's often a need for mesh data without pulling in an asset importer or library. This tool takes a Wavefront `.obj` file and outputs raw mesh data ready for passing directly to `glBufferData()`, Metal's `newBufferWithBytes`, `wgpuQueueWriteBuffer()`, etc.

This is mostly a wrapper around [meshoptimizer](//github.com/zeux/meshoptimizer), [fast_obj](//github.com/thisistherk/fast_obj) and [MikkTSpace](//github.com/mmikk/MikkTSpace). It reads in an `.obj` file and outputs an interleaved buffer (with optional [Zstandard](//github.com/facebook/zstd) compression).
```
Usage: obj2buf [-p|u|n|t|i type] [-s|sb] [-e|ez] [-b] [-o|l|z|a] in.obj [out.bin]
        -p vertex positions type
        -u vertex texture UVs type
        -n vertex normals type
        -t tangents type (defaulting to none)
        -i index buffer type (defaulting to shorts)
        (vertex types are byte|short|half|float|none (none emits no data))
        (index types are byte|short|int|none (none emits unindexed triangles))
        -s normalises the positions to scale them in the range -1 to 1
        -sb as -s but without a bias, keeping the origin at zero
        -e encodes normals (and tangents) in two components as hemi-oct
        -ez as -e but as raw XY without the Z
        -b store only the sign for bitangents
        (packing the sign if possible where any padding would normally go)
        -o writes multi-byte values in big endian order
        -l use the legacy OpenGL rule for normalised signed values
        -z compresses the output buffer using Zstandard
        -a writes the output as ASCII hex instead of binary
The default is float positions, normals and UVs, as uncompressed LE binary
```
For simple cases it's probably enough to take the defaults, with the addition of the `-a` option to output a text file:
```
obj2buf -a cube.obj cube.inc
```
Then `include` this file at compile time:
```
uint8_t buffer[] = {
	#include "cube.inc"
};
```
By passing the `-s` option the mesh will be scaled to fit the range `-1` to `1`, allowing any object to be drawn without considering the camera or mesh size.
