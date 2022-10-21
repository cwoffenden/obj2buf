# Wavefront `.obj` to Interleaved Buffer

[![CMake macOS/Windows/Linux](/../../actions/workflows/cmake-desktop.yml/badge.svg)](/../../actions/workflows/cmake-desktop.yml)

When writing quick tests or graphics experiements there's often a need for mesh data without pulling in an asset importer or library. This tool takes a Wavefront `.obj` file and outputs raw mesh data ready for passing directly to `glBufferData()`, Metal's `newBufferWithBytes`, `wgpuQueueWriteBuffer()`, etc.

This is mostly a wrapper around [meshoptimizer](//github.com/zeux/meshoptimizer), [fast_obj](//github.com/thisistherk/fast_obj) and [MikkTSpace](//github.com/mmikk/MikkTSpace). It reads in an `.obj` file and outputs an interleaved buffer (with optional [Zstandard](//github.com/facebook/zstd) compression).

Build with:
```
clone https://github.com/cwoffenden/obj2buf
cd obj2buf
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Work in progress (not all combinations have been thoroughly tested).
```
Usage: obj2buf [-p|u|n|t|i type] [-s|sb|su] [-e|ez] [-b] [-m|o|l|z|a] in [out]
	-p vertex positions type
	-u vertex texture UVs type
	-n vertex normals type
	-t vertex tangents type (defaulting to none)
	-i index buffer type (defaulting to shorts)
	(vertex types are byte|short|half|float|none (none emits no data))
	(index types are byte|short|int|none (none emits unindexed triangles))
	-s normalises the positions to scale them in the range -1 to 1
	-sb as -s but without a bias, keeping the origin at zero
	-su as -s but with uniform scaling for all axes
	-e encodes normals (and tangents) in two components as hemi-oct
	-ez as -e but as raw XY without the Z
	(encoded normals having the same type as tangents may be packed)
	-b store only the sign for bitangents
	(packing the sign if possible where any padding would normally go)
	-m writes metadata describing the buffer offsets, sizes and types
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
Which would then be included at compile time:
```
uint8_t buffer[] = {
#include "cube.inc"
};
```
A more complex example could be:

1. Vertex positions and UVs as `short`, normals and tangents as `byte`.

2. `-su` option to scale the mesh uniformly in the range `-1` to `1` (allowing any object to be drawn without considering the camera position or mesh size).

3. `-b` option to only store the sign for the bitangents  (which will be packed into the padding for the positions).

4. `-ez` option to encode normals and tangents as drop-Z.

5. Since normals and tangents are both bytes and encoded, they will be packed.

6. `-m` option to write metadata (offsets and number of bytes for the vertex and index data, and the number of triangles).
```
obj2buf -p short -u short -n byte -t byte -su -ez -b -m -a cube.obj cube.inc
```
The resulting layout would be:

| Bits 0-15 | Bits 16-31 |
|:---------:|:----------:|
|   posn.x  |   posn.y   |
|   posn.z  |    sign    |
|    uv.x   |    uv.y    |
|  norm.xy  |  tans.xy   |

With each vertex packed into 16 bytes (instead of the 56 bytes storing everything a floats).

The `-m` option adds an extra 50-66 bytes as a header at the start, depending on the attributes written:

|       Data      |    Size    |
|:---------------:|:----------:|
| vertices offset | uint32     |
| vertices bytes  | uint32     |
| indices offset  | uint32     |
| indices bytes   | uint32     |
| indices count   | uint32     |
| mesh scale      | 3x float32 |
| mesh bias       | 3x float32 |
| vertex stride   | uint8      |
| attribute count | uint8      |
| attributes      | 4-20 bytes |

The offsets are from the start of the file. Each attribute entry is stored as:

|       Data      |  Size |
|:---------------:|:-----:|
| shader index    | uint8 |
| component count | uint8 |
| data type       | uint8 |
| offset          | uint8 |

See the examples as for how this maps to various APIs.

