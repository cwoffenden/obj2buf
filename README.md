# Wavefront `.obj` to Interleaved Buffer

[![CMake macOS/Windows/Linux](/../../actions/workflows/cmake-desktop.yml/badge.svg)](/../../actions/workflows/cmake-desktop.yml)

This is mostly a wrapper around [meshoptimizer](//github.com/zeux/meshoptimizer), [fast_obj](//github.com/thisistherk/fast_obj) and [MikkTSpace](//github.com/mmikk/MikkTSpace). It reads in an `.obj` file and outputs an interleaved buffer (with optional [Zstandard](//github.com/facebook/zstd) compression).

```
Usage: obj2buf [-p|n|u type] [-s|sb] [-e|ez] [-a|b|c|l] in.obj [out.bin]
	-p vertex positions type
	-n vertex normals type
	-u vertex texture UVs type
	(where type is byte|short|half|float|none (none emits no data))
	-s normalises the positions to scale them in the range -1 to 1
	-sb as -s but without a bias, keeping the origin at zero
	-e encodes normals in two components as hemi-oct
	-ez as -e but as raw XY without the Z
	-a writes the output as ASCII instead of binary
	-b writes multi-byte values in big endian order
	-c compresses the output buffer using Zstandard
	-l use the legacy OpenGL rule for normalised signed values
The default is float positions, normals and UVs, as uncompressed LE binary
```
