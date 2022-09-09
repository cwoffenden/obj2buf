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
				return VertexPacker::UINT08N;
			}
			if (strncmp(type, "us", 2) == 0) {
				return VertexPacker::UINT16N;
			}
		}
		fprintf(stderr, "Unknown data type: %s\n", type);
	}
	return VertexPacker::EXCLUDE;
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
		return "byte";
	case VertexPacker::UINT08N:
		return "unsigned byte";
	case VertexPacker::SINT16N:
		return "short";
	case VertexPacker::UINT16N:
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
		case '?': // help
		case '-':
			 help();
			 break;
		case 'n': // normals
			if (next + 2 < argc) {
				norm = parseType(argv[++next]);
				if (norm == VertexPacker::EXCLUDE) {
					opts |= OPTS_SKIP_NORMALS;
				}
			} else {
				fprintf(stderr, "Type parameter required\n");
				help();
			}
			break;
		case 'p': // positions
			if (next + 2 < argc) {
				posn = parseType(argv[++next]);
				if (posn == VertexPacker::EXCLUDE) {
					opts |= OPTS_SKIP_POSITIONS;
				}
			} else {
				fprintf(stderr, "Type parameter required\n");
				help();
			}
			break;
		case 'u': // UVs
			if (next + 2 < argc) {
				text = parseType(argv[++next]);
				if (text == VertexPacker::EXCLUDE) {
					opts |= OPTS_SKIP_TEXURE_UVS;
				}
			} else {
				fprintf(stderr, "Type parameter required\n");
				help();
			}
			break;
		case 'x':
			 opts |= OPTS_NORMALS_XY_ONLY;
			 break;
		case 'h':
			if (strcmp(arg + 1, "help") == 0) {
				help();
			} else {
				opts |= OPTS_NORMALS_HEMI_OCT;
			}
			break;
		case 'b': // big endian
			opts |= OPTS_BIG_ENDIAN;
			break;
		default:
			fprintf(stderr, "Unknown argument: %s\n", arg);
			help();
		}
		next++;
	} else {
		/*
		 * We choose "-1 - next" to return "parsing stopped at 0".
		 */
		next = -1 - next;
	}
	return next;
}

void ToolOptions::dump() const {
	printf("Positions:   %s\n", (opts & OPTS_SKIP_POSITIONS)  ? "skipped" : stringType(posn));
	printf("Normals:     %s",   (opts & OPTS_SKIP_NORMALS)    ? "skipped" : stringType(norm));
	if (opts & (OPTS_NORMALS_XY_ONLY | OPTS_NORMALS_HEMI_OCT)) {
		printf(" (encoding: %s)", (opts & OPTS_NORMALS_XY_ONLY) ? "XY-only" : "hemi-oct");
	}
	printf("\n");
	printf("Texture UVs: %s\n", (opts & OPTS_SKIP_TEXURE_UVS) ? "skipped" : stringType(text));
	printf("Endianness:  %s\n", (opts & OPTS_BIG_ENDIAN)      ? "big" : "little");
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
	printf("Usage: %s [-p type] [-n type] [-u type] [-b] input.obj [output.dat]\n", (name) ? name : "obj2buf");
	printf("\t-p vertex positions datatype\n");
	printf("\t-n vertex normals datatype\n");
	printf("\t-u vertex texture UVs datatype\n");
	printf("\ttype is byte|short|half|float|none (where none emits no data)\n");
	printf("\t-b writes bytes in big endian order\n");
	exit(EXIT_FAILURE);
}
