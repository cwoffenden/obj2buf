/**
 * \file tooloptions.h
 * Parses the command-line and stores the packing options.
 */
#pragma once

#include "vertexpacker.h"

/**
 * Tool options specific to writing an interleaved buffer. Usage:
 * \code
 *	int main(int argc, const char* argv[]) {
 *		// default options
 *		ToolOptions opts;
 *		// fill the options from any command-line params
 *		opts.parseArgs(argv, argc);
 *	}
 * \endcode
 */
class ToolOptions
{
public:
	/**
	 * Additional tool output options.
	 */
	enum Options {
		/**
		 * Default options: write positions, normals and UVs; normals have three
		 * components (plus padding); data are written as uncompressed binary in
		 * little endian ordering.
		 */
		OPTS_DEFAULT = 0,
		/**
		 * Do \e not write positions.
		 */
		OPTS_SKIP_POSITIONS = 1,
		/**
		 * Do \e not write normals.
		 */
		OPTS_SKIP_NORMALS = 2,
		/**
		 * Do \e not write texture UVs.
		 */
		OPTS_SKIP_TEXTURE_UVS = 4,
		/**
		 * Scale the positions so all coordinates fit in the range \c -1 to \c 1
		 * (see \c #OPTS_SCALE_NO_BIAS, since the default is to apply a bias to
		 * make full use of the underlying data format's range).
		 */
		OPTS_POSITIONS_SCALE = 8,
		/**
		 * Maintain the origin for \c #OPTS_POSITIONS_SCALE at zero.
		 */
		OPTS_SCALE_NO_BIAS = 16,
		/**
		 * Normals are hemi-oct encoded (reconstituting X, Y and Z at runtime).
		 */
		OPTS_NORMALS_ENCODED = 32,
		/**
		 * Normals are written as X- and Y-coordinates (recovering the Z at
		 * runtime).
		 */
		OPTS_NORMALS_XY_ONLY = 64,
		/**
		 * The output file is ASCII (instead of binary).
		 */
		OPTS_ASCII_FILE = 128,
		/**
		 * The output byte order is big endian.
		 */
		OPTS_BIG_ENDIAN = 256,
		/**
		 * The output buffer is compressed (as Zstandard)
		 */
		OPTS_COMPRESS_ZSTD = 512,
		/**
		 * Normalised signed values are compatible with older APIs, where the
		 * full range of bits is used but zero cannot be represented.
		 */
		 OPTS_SIGNED_LEGACY = 1024,
	};

	/**
	 * Storage type to use when writing the positions. The default is three
	 * 32-bit \c float&nbsp;s (12 bytes).
	 *
	 * \note The \c Storage formats for 8- and 16-bit integers are here as \e
	 * normalised (because of how the type options are set) but when writing
	 * they are changed to the required \e clamped equivalent if \c #opts
	 * doesn't have Options#OPTS_POSITIONS_SCALE set.
	 */
	VertexPacker::Storage posn;

	/**
	 * Storage type to use when writing the normals. The default is three 32-bit
	 * \c float&nbsp;s (12 bytes).
	 */
	VertexPacker::Storage norm;

	/**
	 * Storage type to use when writing the texture UVs. The default is two
	 * 32-bit \c float&nbsp;s (8 bytes).
	 */
	VertexPacker::Storage text;

	/**
	 * Storage type to use when writing the tangents. The default is exclude
	 * tangents (and by extension bitangents).
	 */
	VertexPacker::Storage tans;

	/**
	 * Storage type to use when writing the bitangents. The default is exclude
	 * bitangents (and by extension tangents).
	 */
	VertexPacker::Storage btan;

	/**
	 * A bitfield of the tool's \c #Options (see \c #OPTS_DEFAULT).
	 */
	unsigned opts;

	/**
	 * Creates the default options.
	 */
	ToolOptions()
		: posn(VertexPacker::FLOAT32)
		, norm(VertexPacker::FLOAT32)
		, text(VertexPacker::FLOAT32)
		, tans(VertexPacker::EXCLUDE)
		, btan(VertexPacker::EXCLUDE)
		, opts(OPTS_DEFAULT) {}

	/**
	 * Parse the command-lines arguments and populate this object.
	 *
	 * \param[in] argv command-line arguments \e exactly as passed-in from \c main()
	 * \param[in] argc number of entries in \a argv
	 * \param[in] cli \c true if these were arguments from the command line (and so the first entry is the program name)
	 * \return index where the argument parsing ended (\c 0 if there were no arguments)
	 */
	int parseArgs(const char* const argv[], int const argc, bool const cli = true);

	/**
	 * Helper to extract the filename from a path.
	 *
	 * \param[in] path full path
	 * \return the file at the end of the path (or an empty string if there is no file)
	 */
	static const char* filename(const char* const path);

private:
	/**
	 * Performs the work of \c #parseArgs().
	 *
	 * \param[in] argv command-line arguments as passed-in from \c main, \e minus the application name
	 * \param[in] argc number of entries in \a argv
	 * \param[in] next index of the next argument to process
	 * \return argument to process in the next call (\a argc if all arguments were processed; \c -1 \c - \c next if there are arguments after the switches)
	 */
	int parseNext(const char* const argv[], int const argc, int next);

	/**
	 * Prints the options in a human readable form.
	 */
	void dump() const;

	/**
	 * Print the CLI help then exit.
	 *
	 * \param[in] path the application path as passed-in as the first parameter from the CLI
	 */
	static void help(const char* const path = nullptr);
};
