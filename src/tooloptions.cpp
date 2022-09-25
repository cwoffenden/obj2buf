#include "tooloptions.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>

/**
 * Helper to convert a string data type to a storage type. \c b becomes \c
 * SINT08N (signed 8-bit int), \c ub becomes \c UINT08N (unsigned 8-bit int),
 * etc.
 *
 * \param[in] type string data type
 * \return appropriate storage type (or \c EXCLUDE if a match could not be made)
 */
static VertexPacker::Storage parseType(const char* const type) {
	if (type && strlen(type) > 0) {
		switch (type[0]) {
		case 'b':
			return VertexPacker::SINT08N;
		case 's':
			return VertexPacker::SINT16N;
		case 'h':
			return VertexPacker::FLOAT16;
		case 'f':
			return VertexPacker::FLOAT32;
		case 'n':
		case 'x':
			return VertexPacker::EXCLUDE;
		default:
			if (strncmp(type, "ub", 2) == 0) {
				return VertexPacker::UINT08C;
			}
			if (strncmp(type, "us", 2) == 0) {
				return VertexPacker::UINT16C;
			}
		}
		fprintf(stderr, "Unknown data type: %s\n", type);
	}
	return VertexPacker::EXCLUDE;
}

/**
 * Helper to help \c parseType(const char*) to extract the current argument's type.
 *
 * \param[in] argv command-line arguments as passed-in from \c main, \e minus the application name
 * \param[in] argc number of entries in \a argv
 * \param[in,out] next index of the current argument being processed
 * \param[in,out] opts options bitfield to store \a flag if the parsed value is to \c EXCLUDE
 * \return appropriate storage type (or \c FLOAT32 if none was supplied)
 */
static VertexPacker::Storage parseType(const char* const argv[], int const argc, int& next, unsigned& opts, unsigned const flag) {
	VertexPacker::Storage type = VertexPacker::FLOAT32;
	if (next + 2 < argc) {
		type = parseType(argv[++next]);
		if (type == VertexPacker::EXCLUDE) {
			opts |= flag;
		}
	} else {
		fprintf(stderr, "Not enough parameters (defaulting to float)\n");
	}
	return type;
}

/**
 * Helper to convert the storage type to a string.
 *
 * \param[in] type storage type
 * \return string equivalent (or \c N/A of there is no match)
 */
static const char* stringType(VertexPacker::Storage const type) {
	switch (type) {
	case VertexPacker::SINT08N:
	case VertexPacker::SINT08C:
		return "byte";
	case VertexPacker::UINT08N:
	case VertexPacker::UINT08C:
		return "unsigned byte";
	case VertexPacker::SINT16N:
	case VertexPacker::SINT16C:
		return "short";
	case VertexPacker::UINT16N:
	case VertexPacker::UINT16C:
		return "unsigned short";
	case VertexPacker::FLOAT16:
		return "half";
	case VertexPacker::FLOAT32:
		return "float";
	default:
		return "N/A";
	}
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
	dump();
	return next;
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
			posn = parseType(argv, argc, next, opts, OPTS_SKIP_POSITIONS);
			break;
		case 'n': // normals
			norm = parseType(argv, argc, next, opts, OPTS_SKIP_NORMALS);
			break;
		case 'u': // UVs
			text = parseType(argv, argc, next, opts, OPTS_SKIP_TEXURE_UVS);
			break;
		case 's': // scaled positions
			opts |= OPTS_POSITIONS_SCALE;
			if (strcmp(arg + 1, "sb") == 0) {
				opts |= OPTS_SCALE_NO_BIAS;
			}
			break;
		case 'e': // encoded normals
			opts |= OPTS_NORMALS_ENCODED;
			if (strcmp(arg + 1, "ez") == 0) {
				opts |= OPTS_NORMALS_XY_ONLY;
			}
			break;
		case 'a': // ASCII
			opts |= OPTS_ASCII_FILE;
			break;
		case 'b': // big endian
			opts |= OPTS_BIG_ENDIAN;
			break;
		case 'c': // compression
			opts |= OPTS_COMPRESS_ZSTD;
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

void ToolOptions::dump() const {
	printf("Positions:   %s",   (opts & OPTS_SKIP_POSITIONS)  ? "N/A"     : stringType(posn));
	if (opts & OPTS_POSITIONS_SCALE) {
		printf(" (apply %s)",   (opts & OPTS_SCALE_NO_BIAS)   ? "scale"   : "scale & bias");
	}
	printf("\n");
	printf("Normals:     %s",   (opts & OPTS_SKIP_NORMALS)    ? "N/A"     : stringType(norm));
	if (opts & OPTS_NORMALS_ENCODED) {
		printf(" (as %s)",      (opts & OPTS_NORMALS_XY_ONLY) ? "XY-only" : "hemi-oct");
	}
	printf("\n");
	printf("Texture UVs: %s\n", (opts & OPTS_SKIP_TEXURE_UVS) ? "N/A"     : stringType(text));
	printf("File format: %s\n", (opts & OPTS_ASCII_FILE)      ? "ASCII"   : "binary");
	printf("Endianness:  %s\n", (opts & OPTS_BIG_ENDIAN)      ? "big"     : "little");
	printf("Compression: %s\n", (opts & OPTS_COMPRESS_ZSTD)   ? "Zstd"    : "none");
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

void ToolOptions::help(const char* const path) {
	const char* name = filename(path);
	if (!name) {
		 name = "obj2buf";
	}
	printf("Usage: %s [-p|n|u type] [-s|sb] [-e|ez] [-a|b|c] in.obj [out.dat]\n", name);
	printf("\t-p vertex positions type\n");
	printf("\t-n vertex normals type\n");
	printf("\t-u vertex texture UVs type\n");
	printf("\t(where type is byte|short|half|float|none (none emits no data))\n");
	printf("\t-s normalises the positions to scale them in the range -1 to 1\n");
	printf("\t-sb as -s but without a bias, keeping the origin at zero\n");
	printf("\t-e encodes normals in two components as hemi-oct\n");
	printf("\t-ez as -e but as raw XY without the Z\n");
	printf("\t-a writes the output as ASCII instead of binary\n");
	printf("\t-b writes multi-byte values in big endian order\n");
	printf("\t-c compresses the output buffer using Zstandard\n");
	printf("The default is float positions, normals and UVs, as uncompressed LE binary\n");
	exit(EXIT_FAILURE);
}
