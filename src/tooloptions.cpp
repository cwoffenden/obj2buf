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
 * Helper to set \c ToolOptions#opts from an \c Options ordinal. E.g.:
 * \code
 *	O2B_SET_OPT(myVar, OPTS_POSITIONS_SCALE)
 * \endcode
 */
#ifndef O2B_SET_OPT
#define O2B_SET_OPT(var, ordinal) var |= (1 << ordinal)
#endif

/**
 * Helper to clear \c ToolOptions#opts from an \c Options ordinal. E.g.:
 * \code
 *	O2B_CLEAR_OPT(myVar, OPTS_POSITIONS_SCALE)
 * \endcode
 */
#ifndef O2B_CLEAR_OPT
#define O2B_CLEAR_OPT(var, ordinal) var &= ~(1 << ordinal)
#endif

/**
 * Helper to constrain the deserialised type to valid values.
 */
#ifndef O2B_VALIDATE_TYPE
#define O2B_VALIDATE_TYPE(type) (((type) <= VertexPacker::Storage::FLOAT32) ? static_cast<VertexPacker::Storage::Type>(type) : VertexPacker::Storage::FLOAT32)
#endif

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
		case '?': // Window's style help
		case '-': // --flags
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
			if (strcmp(arg + 1, "su") == 0) {
				O2B_SET_OPT(opts, OPTS_SCALE_UNIFORM);
			}
			if (strcmp(arg + 1, "sz") == 0) {
				O2B_SET_OPT(opts, OPTS_SCALE_NO_BIAS);
			}
			if (strcmp(arg + 1, "suz") == 0 ||
				strcmp(arg + 1, "szu") == 0) {
				O2B_SET_OPT(opts, OPTS_SCALE_UNIFORM);
				O2B_SET_OPT(opts, OPTS_SCALE_NO_BIAS);
			}
			break;
		case 'o': // oct encoded normals (and tangents)
			O2B_SET_OPT(opts, OPTS_NORMALS_ENCODED);
			break;
		case 'g':
			O2B_SET_OPT(opts, OPTS_TANGENTS_FLIP_G);
			break;
		case 'b': // bitangents as sign-only
			O2B_SET_OPT(opts, OPTS_BITANGENTS_SIGN);
			break;
		case 'm': // metadata
			O2B_SET_OPT(opts, OPTS_WRITE_METADATA);
			break;
		case 'e': // big endian order
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
		case 'c': // shortcode
			if (next + 2 < argc) {
				setAllOptions(static_cast<uint32_t>(strtoul(argv[++next], nullptr, 16)));
				fixUp();
			} else {
				fprintf(stderr, "Missing shortcode\n");
				help();
			}
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

void ToolOptions::fixUp() {
	if (posn) {
		if (!O2B_HAS_OPT(opts, OPTS_POSITIONS_SCALE)) {
			/*
			 * If positions are unscaled the types are converted to clamped.
			 */
			switch (posn) {
			case VertexPacker::Storage::SINT08N:
				posn = VertexPacker::Storage::SINT08C;
				break;
			case VertexPacker::Storage::UINT08N:
				posn = VertexPacker::Storage::UINT08C;
				break;
			case VertexPacker::Storage::SINT16N:
				posn = VertexPacker::Storage::SINT16C;
				break;
			case VertexPacker::Storage::UINT16N:
				posn = VertexPacker::Storage::UINT16C;
				break;
			default:
				// no change
				break;
			}
		}
	} else {
		O2B_CLEAR_OPT(opts, OPTS_POSITIONS_SCALE);
	}
	if (tans) {
		/*
		 * Encoded normals with both normals and tangents having the same type
		 * mean with can pack the tangents with the normals.
		 *
		 * TODO: for bytes and shorts this is interesting, but for floats is this worth it?
		 */
		if (O2B_HAS_OPT(opts, OPTS_NORMALS_ENCODED) && norm == tans && norm.bytes() <= 2) {
			O2B_SET_OPT(opts, OPTS_TANGENTS_PACKED);
		}
	} else {
		O2B_CLEAR_OPT(opts, OPTS_BITANGENTS_SIGN);
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

uint32_t ToolOptions::getAllOptions() const {
	/*
	 * There are currently 11 user settable options, plus one reserved, which
	 * take up the first 12 bits, then each of the storage types is packed into
	 * 4 bits.
	 */
	uint32_t val = opts & ((1 << (OPTS_LAST_USER + 1)) - 1);
	val |= posn << (OPTS_LAST_USER + 1 +  0);
	val |= text << (OPTS_LAST_USER + 1 +  4);
	val |= norm << (OPTS_LAST_USER + 1 +  8);
	val |= tans << (OPTS_LAST_USER + 1 + 12);
	val |= idxs << (OPTS_LAST_USER + 1 + 16);
	return val;
}

void ToolOptions::setAllOptions(uint32_t const val) {
	/*
	 * See above for the serialisation. The only thing special here is the
	 * types are constrained to valid values.
	 */
	opts = val & ((1 << (OPTS_LAST_USER + 1)) - 1);
	posn = O2B_VALIDATE_TYPE((val >> (OPTS_LAST_USER + 1 +  0)) & 0xF);
	text = O2B_VALIDATE_TYPE((val >> (OPTS_LAST_USER + 1 +  4)) & 0xF);
	norm = O2B_VALIDATE_TYPE((val >> (OPTS_LAST_USER + 1 +  8)) & 0xF);
	tans = O2B_VALIDATE_TYPE((val >> (OPTS_LAST_USER + 1 + 12)) & 0xF);
	idxs = O2B_VALIDATE_TYPE((val >> (OPTS_LAST_USER + 1 + 16)) & 0xF);
}

void ToolOptions::dump() const {
	printf("Positions:   %s",   posn.toString());
	if (O2B_HAS_OPT(opts, OPTS_POSITIONS_SCALE)) {
		const char* scaleOpts;
		if (O2B_HAS_OPT(opts, OPTS_SCALE_UNIFORM)) {
			if (O2B_HAS_OPT(opts, OPTS_SCALE_NO_BIAS)) {
				scaleOpts = "uniform scale";
			} else {
				scaleOpts = "uniform scale with bias";
			}
		} else {
			if (O2B_HAS_OPT(opts, OPTS_SCALE_NO_BIAS)) {
				scaleOpts = "scale";
			} else {
				scaleOpts = "scale with bias";
			}
		}
		printf(" (apply %s)", scaleOpts);
	}
	printf("\n");
	printf("Texture UVs: %s\n", text.toString());
	printf("Normals:     %s",   norm.toString());
	if (norm && O2B_HAS_OPT(opts, OPTS_NORMALS_ENCODED)) {
		printf(" (octahedral encoded)");
	}
	printf("\n");
	printf("Tangents:    %s",   tans.toString());
	if (tans) {
		bool flip = O2B_HAS_OPT(opts, OPTS_TANGENTS_FLIP_G);
		bool pack = O2B_HAS_OPT(opts, OPTS_TANGENTS_PACKED);
		bool sign = O2B_HAS_OPT(opts, OPTS_BITANGENTS_SIGN);
		if (flip || pack || sign) {
			printf(" (");
			if (flip) {
				printf("g-flipped%s", (pack || sign) ? ", " : "");
			}
			if (pack) {
				printf("packed in normals%s", (sign) ? ", " : "");
			}
			if (sign) {
				printf("bitangents as sign");
			}
			printf(")");
		}
	}
	printf("\n");
	printf("Indices:     %s\n", idxs.toString());
	printf("Metadata:    %s\n", O2B_HAS_OPT(opts, OPTS_WRITE_METADATA) ? "yes"    : "no (raw)");
	printf("Endianness:  %s\n", O2B_HAS_OPT(opts, OPTS_BIG_ENDIAN)     ? "big"    : "little");
	printf("Signed rule: %s\n", O2B_HAS_OPT(opts, OPTS_SIGNED_LEGACY)  ? "legacy" : "modern");
	printf("Compression: %s\n", O2B_HAS_OPT(opts, OPTS_COMPRESS_ZSTD)  ? "Zstd"   : "none");
	printf("File format: %s\n", O2B_HAS_OPT(opts, OPTS_ASCII_FILE)     ? "ASCII"  : "binary");
	printf("(As -c code: %08X)\n", getAllOptions());
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
	printf("Usage: %s [-p|u|n|t|i type] [-s|su|sz] [-o|g|b|m|e|l|z|a] in [out]\n", name);
	printf("Usage: %s [-c shortcode] in [out]\n", name);
	printf("\t-p vertex positions type\n");
	printf("\t-u vertex texture UVs type\n");
	printf("\t-n vertex normals type\n");
	printf("\t-t vertex tangents type (defaulting to none)\n");
	printf("\t-i index buffer type (defaulting to shorts)\n");
	printf("\t(vertex types are byte|short|half|float|none (none emits no data))\n");
	printf("\t(index types are byte|short|int|none (none emits unindexed triangles))\n");
	printf("\t-s normalises the positions to scale them in the range -1 to 1\n");
	printf("\t-su as -s but with uniform scaling for all axes\n");
	printf("\t-sz as -s but without a bias, keeping the origin at zero\n");
	printf("\t-o octahedral encoded normals (and tangents) in two components\n");
	printf("\t(encoded normals having the same type as tangents may be packed)\n");
	printf("\t-g tangents are generated for an inverted G-channel (e.g. match 3ds Max)\n");
	printf("\t-b store only the sign for bitangents\n");
	printf("\t(packing the sign if possible where any padding would normally go)\n");
	printf("\t-m writes metadata describing the buffer offsets, sizes and types\n");
	printf("\t-e writes multi-byte values in big endian order (e.g. PPC, MIPS)\n");
	printf("\t-l use the legacy OpenGL rule for normalised signed values\n");
	printf("\t-z compresses the output buffer using Zstandard\n");
	printf("\t-a writes the output as ASCII hex instead of binary\n");
	printf("\t-c hexadecimal shortcode encompassing all the options\n");
	printf("The default is float positions, normals and UVs, as uncompressed LE binary\n");
	exit(EXIT_FAILURE);
}
