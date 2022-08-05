/**
 * \file main.cpp
 * Wavefront \c .obj to packed buffer.
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "meshoptimizer.h"
#include "fast_obj.h"

/**
 * \def DEFAULT_TEST_FILE
 * Default .obj file to load if none is supplied.
 */
#define DEFAULT_TEST_FILE "bunny.obj"

/**
 * Helper to extract the filename from a path.
 *
 * \param[in] path full path
 * \return the file at the end of the path (or an empty string if there is no file)
 */
const char* extractName(const char* path) {
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

/*
 * Load and convert.
 */
int main(int argc, const char* argv[]) {
	const char* file = DEFAULT_TEST_FILE;
	if (argc <= 1) {
		if (argc == 1) {
			printf("Usage: %s in.obj [out.dat]\n", extractName(argv[0]));
		}
	} else {
		file = argv[1];
	}
	if (fastObjMesh* obj = fast_obj_read(file)) {
		printf("Faces: %d\n", obj->face_count);
		fast_obj_destroy(obj);
	}
	return EXIT_SUCCESS;
}
