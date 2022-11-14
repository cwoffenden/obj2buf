/**
 * \file tooloptions.h
 * Parses the command-line and stores the packing options.
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#pragma once

#include "vertexpacker.h"

/**
 * Evaluates to \c true if \c ToolOptions#opts has an \c Options ordinal set.
 * E.g.:
 * \code
 *	if (O2B_HAS_OPT(myVar, OPTS_POSITIONS_SCALE)) {
 *		// Scale the vertex positions
 *	}
 * \endcode
 */
#ifndef O2B_HAS_OPT
#define O2B_HAS_OPT(var, ordinal) ((var & (1 << ordinal)) != 0)
#endif

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
	 * Additional tool output options. These values are the ordinals, requiring
	 * conversion to the bitpattern via \c OPT_TO_FLAG.
	 */
	enum Options {
		/**
		 * Default options: normals have three components (plus padding); data
		 * are written as uncompressed binary in little endian ordering.
		 */
		OPTS_DEFAULT = 0,
		/**
		 * Scale the positions so all coordinates fit in the range \c -1 to \c 1
		 * (see \c #OPTS_SCALE_NO_BIAS, since the default is to apply a bias to
		 * make full use of the underlying data format's range, and \c
		 * OPTS_SCALE_UNIFORM, to maintain the proportions).
		 */
		OPTS_POSITIONS_SCALE,
		/**
		 * Maintain the origin for \c OPTS_POSITIONS_SCALE at zero.
		 */
		OPTS_SCALE_NO_BIAS,
		/**
		 * Maintains the proportions when using \c OPTS_POSITIONS_SCALE so the
		 * mesh can be drawn without applying the scale (otherwise each axis
		 * will be scaled individually). This trades resolution for ease of
		 * drawing.
		 */
		OPTS_SCALE_UNIFORM,
		/**
		 * Normals and tangents are octahedral encoded (reconstituting X, Y and
		 * Z at runtime). This, and when both \c #norm and \c #tans have the
		 * same type, will result in \c OPTS_TANGENTS_PACKED being set.
		 */
		OPTS_NORMALS_ENCODED,
		/**
		 * Normals and tangents are written as X- and Y-coordinates (recovering
		 * the Z at runtime). An option for \c OPTS_NORMALS_ENCODED.
		 */
		OPTS_NORMALS_XY_ONLY,
		/**
		 * When generating tangents, negate the texture coordinates' Y-channel,
		 * which effectively inverts the normal map's green channel. The would
		 * match 3ds Max's convention, for example (the alternative being to
		 * invert in the shader).
		 */
		OPTS_TANGENTS_FLIP_G,
		/**
		 * Try to pack tangents with the normals. See \c OPTS_NORMALS_ENCODED
		 * (this is not a manually set option).
		 */
		OPTS_TANGENTS_PACKED,
		/**
		 * Only the sign is stored for bitangents (requiring reconstitution
		 * from the normals and tangents at runtime).
		 */
		OPTS_BITANGENTS_SIGN,
		/**
		 * Write metadata before the vertex and index data, specifying the
		 * various buffer offsets and sizes (information which is ordinarily
		 * only displayed).
		 */
		OPTS_WRITE_METADATA,
		/**
		 * The output byte order is big endian.
		 */
		OPTS_BIG_ENDIAN,
		/**
		 * Normalised signed values are compatible with older APIs, where the
		 * full range of bits is used but zero cannot be represented.
		 */
		OPTS_SIGNED_LEGACY,
		/**
		 * The output buffer is compressed (using Zstandard)
		 */
		OPTS_COMPRESS_ZSTD,
		/**
		 * The output file is ASCII encoded (instead of binary). The ASCII
		 * files can be included as headers or otherwise in-lined into code.
		 */
		OPTS_ASCII_FILE,
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
	 * Storage type to use when writing the texture UVs. The default is two
	 * 32-bit \c float&nbsp;s (8 bytes).
	 */
	VertexPacker::Storage text;

	/**
	 * Storage type to use when writing the normals. The default is three 32-bit
	 * \c float&nbsp;s (12 bytes).
	 */
	VertexPacker::Storage norm;

	/**
	 * Storage type to use when writing the tangents. The default is exclude
	 * tangents (and by extension bitangents).
	 */
	VertexPacker::Storage tans;

	/**
	 * Storage type for the index buffer. The default is shorts.
	 */
	VertexPacker::Storage idxs;

	/**
	 * A bitfield of the tool's \c #Options (see \c #OPTS_DEFAULT).
	 */
	unsigned opts;

	/**
	 * Creates the default options.
	 */
	ToolOptions()
		: posn(VertexPacker::Storage::FLOAT32)
		, text(VertexPacker::Storage::FLOAT32)
		, norm(VertexPacker::Storage::FLOAT32)
		, tans(VertexPacker::Storage::EXCLUDE)
		, idxs(VertexPacker::Storage::UINT16C)
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

	/**
	 * Prints the options to \c stdout in a human readable form.
	 */
	void dump() const;

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
	 * Assess the options and tweak any that need changing or cleaning up. For
	 * example, index buffer types should be unsigned clamped.
	 */
	void fixUp();

	/**
	 * Print the CLI help then exit.
	 *
	 * \param[in] path the application path as passed-in as the first parameter from the CLI
	 */
	static void help(const char* const path = nullptr);
};
