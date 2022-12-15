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
Work in progress (not all combinations have been thoroughly tested). Examples for various APIs coming soon.
```
Usage: obj2buf [-p|u|n|t|i type] [-s|su|sz] [-e|g|b|m|o|l|z|a] in [out]
	-p vertex positions type
	-u vertex texture UVs type
	-n vertex normals type
	-t vertex tangents type (defaulting to none)
	-i index buffer type (defaulting to shorts)
	(vertex types are byte|short|half|float|none (none emits no data))
	(index types are byte|short|int|none (none emits unindexed triangles))
	-s normalises the positions to scale them in the range -1 to 1
	-su as -s but with uniform scaling for all axes
	-sz as -s but without a bias, keeping the origin at zero
	-e octahedral encoded normals (and tangents) in two components
	(encoded normals having the same type as tangents may be packed)
	-g tangents are generated for an inverted G-channel (e.g. match 3ds Max)
	-b store only the sign for bitangents
	(packing the sign if possible where any padding would normally go)
	-m writes metadata describing the buffer offsets, sizes and types
	-o writes multi-byte values in big endian order
	-l use the legacy OpenGL rule for normalised signed values
	-z compresses the output buffer using Zstandard
	-a writes the output as ASCII hex instead of binary
	-c hexadecimal shortcode encompassing all the options
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

3. `-e` option to octahedral encode normals and tangents.

4. `-g` options to generate tangents for a 3ds Max-style normal map with an inverted G-channel.

5. Since normals and tangents are both bytes and encoded, they will be packed together (normals in `xy`, tangents in `zw`).

6. `-b` option to only store the sign for the bitangents  (which will be packed into the padding for the positions, so the `w` component).

7. `-m` option to add metadata.
```
obj2buf -p short -u short -n byte -t byte -su -e -g -b -m -a cube.obj cube.inc
```
Or using the `-c` shortcode option, where the options are serialised:
```
obj2buf -c 8115547B cube.obj cube.inc
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
