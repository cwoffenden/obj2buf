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
	 * Storage type to use when writing the vertex positions.
	 */
	VertexPacker::Storage posn;

	/**
	 * Storage type to use when writing the vertex normal.
	 */
	VertexPacker::Storage norm;

	/**
	 * Storage type to use when writing the vertex texture UVs.
	 */
	VertexPacker::Storage text;

	/**
	 * Creates the default options.
	 */
	ToolOptions()
		: posn(VertexPacker::FLOAT32)
		, norm(VertexPacker::FLOAT32)
		, text(VertexPacker::FLOAT32) {}

	/**
	 * Parse the command-lines arguments and populate this object.
	 *
	 * \param[in] argv command-line arguments \e exactly as passed-in from \c main()
	 * \param[in] argc number of entries in \a argv
	 */
	void parseArgs(const char* argv[], int argc);

	/**
	 * Helper to extract the filename from a path.
	 *
	 * \param[in] path full path
	 * \return the file at the end of the path (or an empty string if there is no file)
	 */
	static const char* extractFilename(const char* path);

private:
	/**
	 * Performs the work of \c #parseArgs().
	 *
	 * \param[in] argv command-line arguments as passed-in from \c main, \e minus the application name
	 * \param[in] argc number of entries in \a argv
	 * \param[in] next index of the next argument to process
	 * \return argument to process in the next call (\a argc if all arguments were processed; \c -1 \c - \c next if there are arguments after the switches)
	 */
	int parseNext(const char* argv[], int argc, int next);

	/**
	 * Print the CLI help then exit.
	 */
	static void help();
};
