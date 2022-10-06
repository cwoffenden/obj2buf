# Wavefront `.obj` to Interleaved Buffer

[![CMake macOS/Windows/Linux](/../../actions/workflows/cmake-desktop.yml/badge.svg)](/../../actions/workflows/cmake-desktop.yml)

This is mostly a wrapper around [meshoptimizer](//github.com/zeux/meshoptimizer), [fast_obj](//github.com/thisistherk/fast_obj) and [MikkTSpace](//github.com/mmikk/MikkTSpace). It reads in an `.obj` file and outputs an interleaved buffer (with optional [Zstandard](//github.com/facebook/zstd) compression).

```
Usage: obj2buf [-p|n|u|t type] [-s|sb] [-e|exy] [-b] [-o|l|z|a] in.obj [out.bin]
        -p vertex positions type
        -n vertex normals type
        -u vertex texture UVs type
        -t tangents type (defaulting to none)
        (where type is byte|short|half|float|none (none emits no data))
        -s normalises the positions to scale them in the range -1 to 1
        -sb as -s but without a bias, keeping the origin at zero
        -e encodes normals (and tangents) in two components as hemi-oct
        -exy as -e but as raw XY without the Z
        -b store only the sign for bitangents
        (where possible packing the sign where any padding would normally go)
        -o writes multi-byte values in big endian order
        -l use the legacy OpenGL rule for normalised signed values
        -z compresses the output buffer using Zstandard
        -a writes the output as ASCII hex instead of binary
The default is float positions, normals and UVs, as uncompressed LE binary
```
