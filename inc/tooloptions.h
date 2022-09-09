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
		 * components (plus padding); data are written in little endian ordering.
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
		OPTS_SKIP_TEXURE_UVS = 4,
		/**
		 * Normals are written as X- and Y-coordinates (recovering the Z at runtime).
		 */
		OPTS_NORMALS_XY_ONLY = 8,
		/**
		 * Normals are hemi-oct encoded.
		 */
		OPTS_NORMALS_HEMI_OCT = 16,
		/**
		 * Output is big endian.
		 */
		OPTS_BIG_ENDIAN = 32,
	};

	/**
	 * Storage type to use when writing the positions. The default is three 32-bit
	 * \c float&nbsp;s (12 bytes).
	 */
	VertexPacker::Storage posn;

	/**
	 * Storage type to use when writing the normals. The default is three 32-bit
	 * \c float&nbsp;s (12 bytes).
	 */
	VertexPacker::Storage norm;

	/**
	 * Storage type to use when writing the texture UVs. The default is two 32-bit
	 * \c float&nbsp;s (8 bytes).
	 */
	VertexPacker::Storage text;

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
