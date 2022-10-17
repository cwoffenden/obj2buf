#include "fileutils.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>

#include "zstd.h"

namespace impl {
/**
 * Helper to decide between either a space or a newline following an entry when
 * dumping or printing hex data.
 *
 * \param[in] count running count of written entries
 * \param[in] wrap after how many entries the line wraps (dictating space or newline)
 * \param[in] total total number of characters (dictating whether the last always writes a newline)
 * \return a single space or newline character
 */
char spaceOrNewline(size_t const count, size_t const wrap, size_t total) {
	return ((count > 0 && (count % wrap) == 0) || (count == total)) ? '\n' : ' ';
}

/**
 * Helper to write a buffer to a file.
 *
 * \param[in] dstPath filename of the destination file
 * \param[in] data start of the raw data
 * \param[in] size number of bytes to write
 * \return \c true if writing the requested number of bytes was successful
 */
bool write(const char* const dstPath, const void* const data, size_t const size) {
	if (dstPath && data) {
		if (FILE *dstFile = fopen(dstPath, "wb")) {
			size_t wrote = fwrite(data, 1, size, dstFile);
			if (fclose(dstFile) == 0) {
				return wrote == size;
			}
		}
	}
	return false;
}

/**
 * Helper to write a buffer to a binary or text file
 *
 * \param[in] dstPath filename of the destination file
 * \param[in] data start of the raw data
 * \param[in] size number of bytes to write
 * \param[in] text \c true if the file should be text containing hexadecimal bytes
 * \return \c true if writing the requested number of bytes was successful
 */
bool write(const char* const dstPath, const void* const data, size_t const size, const bool text) {
	if (!text) {
		return impl::write(dstPath, data, size);
	}
	if (dstPath && data) {
		if (FILE* dstFile = fopen(dstPath, "w")) {
			const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
			for (size_t n = 0; n < size; n++) {
				fprintf(dstFile, "0x%02X,%c", bytes[n], spaceOrNewline(n + 1, 12, size));
			}
			if (fclose(dstFile) == 0) {
				return true;
			}
		}
	}
	return false;
}
}

//********************************* Public API ********************************/

bool write(const char* const dstPath, const void* const data, size_t const size, const bool text, bool const zstd) {
	bool success = false;
	if (data) {
		if (zstd) {
			/*
			 * We use the simple API to compress the entire buffer in one go
			 * (with a mono-threaded Zstd).
			 */
			size_t bounds = ZSTD_compressBound(size);
			void* compBuf = malloc(bounds);
			if (compBuf) {
				size_t compSize = ZSTD_compress(compBuf, bounds, data, size, ZSTD_maxCLevel());
				if (ZSTD_isError(compSize)) {
					fprintf(stderr, "Compression failed: %s\n", ZSTD_getErrorName(compSize));
				} else {
					success = impl::write(dstPath, compBuf, compSize, text);
				}
				free(compBuf);
			}
		} else {
			success = impl::write(dstPath, data, size, text);
		}
	}
	return success;
}
