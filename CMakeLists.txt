cmake_minimum_required(VERSION 3.15)

# mkdir out
# cd out
# cmake .. -G "Visual Studio 17 2022"
# obj2buf.sln

project("obj2buf" DESCRIPTION "Wavefront .obj to packed buffer")

include(CheckIPOSupported)
check_ipo_supported(RESULT hasLTO)

if (MSVC)
	# We build Release with the /MD (DLL) option instead of CMake's default /MT (static runtime)
	# Note: the disabled warnings are: C4576 C99 type compound literal; C4201 anonymous struct/union
	# Note: the regex is to remove CMake's own settings for exception handling (EH), warning level (W) and RTTI (GR)
	# TODO: any better way of setting these?
	string(REGEX REPLACE "\/EH[a-z]+|\/W[0-4]|\/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE "\/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4 /WX /wd4576 /wd4201 /D_HAS_EXCEPTIONS=0 /MP /D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} /EHs-c- /GR-")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /MDd /wd4189")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ot /Oi /MD /GS- /Gy")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF")
	if (hasLTO)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /INCREMENTAL:NO /LTCG")
	endif()
else()
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "ppc64|powerpc")
		# We're on one of my own machines here (no one has Power) so we can apply a tune
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mtune=native")
	endif()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -fno-exceptions -fno-rtti")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -D_DEBUG=1 -Wno-unused")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g0")
	if (hasLTO)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
	endif()
	if (EMSCRIPTEN)
		# And for ease-of-use we build Wasm all in one file (which makes it easier to test)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128")
		set(CMAKE_EXECUTABLE_SUFFIX ".html")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s SINGLE_FILE=1")
	endif()
endif()

file(GLOB INCS "inc/*.h")
file(GLOB SRCS "src/*.cpp" "src/*.c")
set(SRCS ${SRCS}
#	"src/meshopt/allocator.cpp"
#	"src/meshopt/clusterizer.cpp"
#	"src/meshopt/indexcodec.cpp"
	"src/meshopt/indexgenerator.cpp"
#	"src/meshopt/overdrawanalyzer.cpp"
	"src/meshopt/overdrawoptimizer.cpp"
#	"src/meshopt/simplifier.cpp"
#	"src/meshopt/spatialorder.cpp"
#	"src/meshopt/stripifier.cpp"
#	"src/meshopt/vcacheanalyzer.cpp"
	"src/meshopt/vcacheoptimizer.cpp"
#	"src/meshopt/vertexcodec.cpp"
#	"src/meshopt/vertexfilter.cpp"
#	"src/meshopt/vfetchanalyzer.cpp"
	"src/meshopt/vfetchoptimizer.cpp"
)

include_directories("inc")

source_group(TREE "${PROJECT_SOURCE_DIR}/inc" PREFIX "Headers" FILES ${INCS})
source_group(TREE "${PROJECT_SOURCE_DIR}/src" PREFIX "Sources" FILES ${SRCS})

add_executable(${CMAKE_PROJECT_NAME} ${INCS} ${SRCS})
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY CXX_STANDARD 11)

# Make this a little nicer in VS
# (Note: the working dir only works from a generated solution, not as a folder in VS)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dat")
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS "-c 8115547B cube.obj out.inc")

# Add some scheme defaults for an Xcode generated solution (note we chose CMake's own vars over, e.g., $PROJECT_DIR)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY XCODE_GENERATE_SCHEME TRUE)
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/dat")
set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY XCODE_SCHEME_ARGUMENTS "-c 8115547B cube.obj out.inc")
