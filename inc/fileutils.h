/**
 * \file fileutils.h
 * Helpers to save out binary data with various options (raw, as hex data, raw
 * with Zstandard compression, as hex data with Zstandard compression).
 */
#pragma once

#include <cstddef>

/**
 * Helper to write a buffer to a binart or text file with optional Zstandard
 * compression.
 *
 * \param[in] dstPath filename of the destination file
 * \param[in] data start of the raw data
 * \param[in] size number of bytes to write
 * \param[in] text \c true if the file should be text containing hexadecimal bytes
 * \param[in] zstd \c true if the file should be compressed with Zstandard
 * \return \c true if writing the requested number of bytes was successful
 */
bool write(const char* const dstPath, const void* const data, size_t const size, const bool text = false, bool const zstd = false);
