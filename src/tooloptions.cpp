/**
 * \file tooloptions.cpp
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#include "tooloptions.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

/**
 * Helper to convert a string data type to a storage type. \c b becomes \c
 * SINT08C (signed 8-bit int), \c ub becomes \c UINT08C (unsigned 8-bit int),
 * etc.
 *
 * \note Where available types are considered normalised, the final choice to
 * clamp is made later.
 *
 * \param[in] type string data type
 * \return appropriate storage type (or \c EXCLUDE if a match could not be made)
 */
static VertexPacker::Storage parseType(const char* const type) {
	if (type && strlen(type) > 0) {
		switch (type[0]) {
		case 'b':
			return VertexPacker::Storage::SINT08N;
		case 's':
			return VertexPacker::Storage::SINT16N;
		case 'i':
			return VertexPacker::Storage::SINT32C;
		case 'h':
			return VertexPacker::Storage::FLOAT16;
		case 'f':
			return VertexPacker::Storage::FLOAT32;
		case 'n':
		case 'x':
			return VertexPacker::Storage::EXCLUDE;
		default:
			if (strncmp(type, "ub", 2) == 0) {
				return VertexPacker::Storage::UINT08N;
			}
			if (strncmp(type, "us", 2) == 0) {
				return VertexPacker::Storage::UINT16N;
			}
			if (strncmp(type, "ui", 2) == 0) {
				return VertexPacker::Storage::UINT32C;
			}
		}
		fprintf(stderr, "Unknown data type: %s\n", type);
	}
	return VertexPacker::Storage::EXCLUDE;
}

/**
 * Helper to help \c parseType(const char*) to extract the current argument's type.
 *
 * \param[in] argv command-line arguments as passed-in from \c main, \e minus the application name
 * \param[in] argc number of entries in \a argv
 * \param[in,out] next index of the current argument being processed
 * \return appropriate storage type (or \c EXCLUDE if none was supplied)
 */
static VertexPacker::Storage parseType(const char* const argv[], int const argc, int& next) {
	VertexPacker::Storage type = VertexPacker::Storage::EXCLUDE;
	if (next + 2 < argc) {
		type = parseType(argv[++next]);
	} else {
		fprintf(stderr, "Not enough parameters (defaulting to exclude)\n");
	}
	return type;
}

//*****************************************************************************/

int ToolOptions::parseArgs(const char* const argv[], int const argc, bool const cli) {
	/*
	 * Two entries in argv are special: the first (0) which is the program name
	 * if called from the CLI, and the last, the file to process (without a
	 * filename we shortcut directly to the help.
	 */
	int next = 0;
	if (cli) {
		next++;
	}
	if (next < argc) {
		while ((argc - next) > 1 && next >= 0) {
			next = parseNext(argv, argc, next);
		}
		if (next < 0) {
			next = -next - 1;
		}
		if (argv[next][0] == '-') {
			help();
		}
	} else {
		help();
	}
	fixUp();
	return next;
}

void ToolOptions::fixUp() {
	if (!O2B_HAS_OPT(opts, OPTS_POSITIONS_SCALE)) {
		/*
		 * If positions aren't scaled the types are converted to clamped.
		 */
		switch (posn) {
		case VertexPacker::Storage::SINT08N:
			idxs = VertexPacker::Storage::SINT08C;
			break;
		case VertexPacker::Storage::UINT08N:
			idxs = VertexPacker::Storage::UINT08C;
			break;
		case VertexPacker::Storage::SINT16N:
			idxs = VertexPacker::Storage::SINT16C;
			break;
		case VertexPacker::Storage::UINT16N:
			idxs = VertexPacker::Storage::UINT16C;
			break;
		default:
			// no change
			break;
		}
	}
	if (tans) {
		/*
		 * Tangents without normals doesn't make any sense.
		 */
		if (!norm) {
			fprintf(stderr, "Tangents requested without normals\n");
			help();
		}
		/*
		 * Encoded normals with both normals and tangents having the same type
		 * mean with can pack the tangents with the normals.
		 */
		if (O2B_HAS_OPT(opts, OPTS_NORMALS_ENCODED) && norm == tans) {
			O2B_SET_OPT(opts, OPTS_TANGENTS_PACKED);
		}
	}
	/*
	 * Indices are always unsigned and clamped.
	 */
	switch (idxs) {
	case VertexPacker::Storage::SINT08N:
	case VertexPacker::Storage::SINT08C:
	case VertexPacker::Storage::UINT08N:
		idxs = VertexPacker::Storage::UINT08C;
		break;
	case VertexPacker::Storage::SINT16N:
	case VertexPacker::Storage::SINT16C:
	case VertexPacker::Storage::UINT16N:
		idxs = VertexPacker::Storage::UINT16C;
		break;
	case VertexPacker::Storage::SINT32C:
		idxs = VertexPacker::Storage::UINT32C;
		break;
	case VertexPacker::Storage::FLOAT16:
	case VertexPacker::Storage::FLOAT32:
		fprintf(stderr, "Indices cannot be floats\n");
		help();
		break;
	default:
		// no change
		break;
	}
}

int ToolOptions::parseNext(const char* const argv[], int const argc, int next) {
	if (next < 0 || next >= argc) {
		/*
		 * Not a valid index so shortcut to the end.
		 */
		return argc;
	}
	const char* arg = argv[next];
	if (arg && strlen(arg) > 1 && arg[0] == '-') {
		switch (arg[1]) {
		case 'h': // help
		case '?':
		case '-':
			help();
			break;
		case 'p': // positions
			posn = parseType(argv, argc, next);
			break;
		case 'n': // normals
			norm = parseType(argv, argc, next);
			break;
		case 'u': // UVs
			text = parseType(argv, argc, next);
			break;
		case 't': // tangents
			tans = parseType(argv, argc, next);
			break;
		case 'i': // indices
			idxs = parseType(argv, argc, next);
			break;
		case 's': // scaled positions
			O2B_SET_OPT(opts, OPTS_POSITIONS_SCALE);
			if (strcmp(arg + 1, "sb") == 0) {
				O2B_SET_OPT(opts, OPTS_SCALE_NO_BIAS);
			}
			break;
		case 'e': // encoded normals and tangents
			O2B_SET_OPT(opts, OPTS_NORMALS_ENCODED);
			if (strcmp(arg + 1, "ez") == 0) {
				O2B_SET_OPT(opts, OPTS_NORMALS_XY_ONLY);
			}
			break;
		case 'b': // bitangents as sign-only
			O2B_SET_OPT(opts, OPTS_BITANGENTS_SIGN);
			break;
		case 'o': // big endian order
			O2B_SET_OPT(opts, OPTS_BIG_ENDIAN);
			break;
		case 'l': // legacy GL signing rule
			O2B_SET_OPT(opts, OPTS_SIGNED_LEGACY);
			break;
		case 'z': // Zstd
			O2B_SET_OPT(opts, OPTS_COMPRESS_ZSTD);
			break;
		case 'a': // ASCII
			O2B_SET_OPT(opts, OPTS_ASCII_FILE);
			break;
		default:
			fprintf(stderr, "Unknown argument: %s\n", arg);
			help();
		}
		next++;
	} else {
		/*
		 * We choose "-1 - next" to return "parsing stopped at 'next'".
		 */
		next = -1 - next;
	}
	return next;
}

const char* ToolOptions::filename(const char* const path) {
	const char* found = nullptr;
	if (path) {
		found = strrchr(path, '/');
		if (!found) {
			 found = strrchr(path, '\\');
		}
		if (found && strlen(found) > 0) {
			return found + 1;
		}
	}
	return path;
}

void ToolOptions::dump() const {
	printf("Positions:   %s",   posn.toString());
	if (O2B_HAS_OPT(opts, OPTS_POSITIONS_SCALE)) {
		printf(" (apply %s)",   O2B_HAS_OPT(opts, OPTS_SCALE_NO_BIAS)   ? "scale"   : "scale & bias");
	}
	printf("\n");
	printf("Texture UVs: %s\n", text.toString());
	printf("Normals:     %s",   norm.toString());
	if (norm && O2B_HAS_OPT(opts, OPTS_NORMALS_ENCODED)) {
		printf(" (as %s)",      O2B_HAS_OPT(opts, OPTS_NORMALS_XY_ONLY) ? "XY-only" : "hemi-oct");
	}
	printf("\n");
	printf("Tangents:    %s",   tans.toString());
	if (tans) {
		const bool bitanSign = O2B_HAS_OPT(opts, OPTS_BITANGENTS_SIGN);
		if (O2B_HAS_OPT(opts, OPTS_TANGENTS_PACKED)) {
			printf(" (packed in normals%s)", bitanSign ? ", bitangents as sign" : "");
		} else {
			if (bitanSign) {
				printf(" (bitangents as sign)");
			}
		}
	}
	printf("\n");
	printf("Indices:     %s\n", idxs.toString());
	printf("Endianness:  %s\n", O2B_HAS_OPT(opts, OPTS_BIG_ENDIAN)      ? "big"     : "little");
	printf("Signed rule: %s\n", O2B_HAS_OPT(opts, OPTS_SIGNED_LEGACY)   ? "legacy"  : "modern");
	printf("Compression: %s\n", O2B_HAS_OPT(opts, OPTS_COMPRESS_ZSTD)   ? "Zstd"    : "none");
	printf("File format: %s\n", O2B_HAS_OPT(opts, OPTS_ASCII_FILE)      ? "ASCII"   : "binary");
}

void ToolOptions::help(const char* const path) {
	const char* name = filename(path);
	if (!name) {
		 name = "obj2buf";
	}
	printf("Usage: %s [-p|u|n|t|i type] [-s|sb] [-e|ez] [-b] [-o|l|z|a] in.obj [out.bin]\n", name);
	printf("\t-p vertex positions type\n");
	printf("\t-u vertex texture UVs type\n");
	printf("\t-n vertex normals type\n");
	printf("\t-t vertex tangents type (defaulting to none)\n");
	printf("\t-i index buffer type (defaulting to shorts)\n");
	printf("\t(vertex types are byte|short|half|float|none (none emits no data))\n");
	printf("\t(index types are byte|short|int|none (none emits unindexed triangles))\n");
	printf("\t-s normalises the positions to scale them in the range -1 to 1\n");
	printf("\t-sb as -s but without a bias, keeping the origin at zero\n");
	printf("\t-e encodes normals (and tangents) in two components as hemi-oct\n");
	printf("\t-ez as -e but as raw XY without the Z\n");
	printf("\t(encoded normals having the same type as tangents may be packed)\n");
	printf("\t-b store only the sign for bitangents\n");
	printf("\t(packing the sign if possible where any padding would normally go)\n");
	printf("\t-o writes multi-byte values in big endian order\n");
	printf("\t-l use the legacy OpenGL rule for normalised signed values\n");
	printf("\t-z compresses the output buffer using Zstandard\n");
	printf("\t-a writes the output as ASCII hex instead of binary\n");
	printf("The default is float positions, normals and UVs, as uncompressed LE binary\n");
	exit(EXIT_FAILURE);
}
