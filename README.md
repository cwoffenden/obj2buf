# Wavefront `.obj` to Interleaved Buffer

[![CMake macOS/Windows/Linux](/../../actions/workflows/cmake-desktop.yml/badge.svg)](/../../actions/workflows/cmake-desktop.yml)

This is mostly a wrapper around [meshoptimizer](//github.com/zeux/meshoptimizer) and [fast_obj](//github.com/thisistherk/fast_obj). It reads in an `.obj` file and outputs an interleaved buffer (with optional [Zstandard](//github.com/facebook/zstd) compression).
