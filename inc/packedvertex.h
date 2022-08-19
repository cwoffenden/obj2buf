/**
 * \file packedvertex.h
 * Utility to build packed vertex buffers.
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#pragma once

#include <cstdint>

#include "vec.h"

 /**
  * Single interleaved vertex. The callee will know the size of the allocated
  * backing \e block per vertex (all vertices in a mesh will be the same size)
  * with the individual attributes added as required. For example, to create a
  * mesh on-the-fly the user would sequentially add the vertex position and
  * texture coordinates, using this class to pack them in a GPU-friendly manner.
  * \code
  *	int const numVerts = 10;
  *	int const numBytes = PackedVertex::getBytes(PackedVertex::VERT_POSN | PackedVertex::TEXT_UV_0);
  *	Vector<uint8_t> buffer(numVerts * numBytes);
  *	for (int n = 0; n < numVerts; n++) {
  *		Vertex vert(buffer.data(), numBytes);
  *		vert.add(vec3(myX, myY, myZ));	// add position (packed as floats)
  *		vert.add(vec2(myU, myV));		// add texture  (packed as shorts)
  *	}
  * \endcode
  * More advanced usage would add duplicate removal by keeping track of the \c
  * %PackedVertex objects, comparing new instances with previous ones added.
  *
  * \todo this should be configured with the attributes and storage types in advance then verts added and packed
  *
  * \author cw
  *
  * \sa https://github.com/MSOpenTech/angle/wiki/Getting-Good-Performance-From-ANGLE
  */
class PackedVertex
{
public:
	/**
	 * Vertex attributes.
	 *
	 * \todo allow the storage to be configured?
	 * \todo this is like stepping back in time and needs an overhaul
	 */
	enum Attribute {
		/**
		 * Vertex positions.
		 * \n
		 * Stored as 3x \c float (12 bytes).
		 *
		 * \note For compatibility with 2D vertex data, adding a \c VERT_POSN
		 * with a \c vec2 will be stored as 2x \c float (8 bytes). This isn't
		 * accounted mfor in the calls to \c #getBytes() (with it being up to
		 * the caller to adjust for this).
		 */
		VERT_POSN = 0x001,
		/**
		 * Vertex normals.
		 * \n
		 * Stored as 3x (\e signed) \c byte, normalised, with one byte padding.
		 *
		 * \note Using ANGLE with \e Feature \e Level 9_3 this will force a
		 * conversion when loading the buffer. Since this should only affect
		 * older hardware such as Windows Phone it is left as-is (the fix would
		 * be to write normals as signed \c short with padding (8 bytes instead
		 * of 4).
		 */
		VERT_NORM = 0x02,
		/**
		 * Vertex tangents (present only if \c #VERT_BTAN is also present, even
		 * though bitangents could be derived from the tangents and normals).
		 * \n
		 * Stored as 3x (\e signed) \c byte, normalised, with one byte padding.
		 *
		 * \note the same ANGLE caveat applies as per \c #VERT_NORM.
		 */
		VERT_TANS = 0x004,
		/**
		 * Vertex bitangents (present only if \c #VERT_TANS is also present).
		 * \n
		 * Stored as 3x (\e signed) \c byte, normalised, with one byte padding.
		 *
		 * \note the same ANGLE caveat applies as per \c #VERT_NORM.
		 */
		VERT_BTAN = 0x008,
		/**
		 * %Vertex colours.
		 * \n
		 * Stored as 4x (\e unsigned) \c byte, normalised, in \c RGBA format.
		 */
		VERT_RGBA = 0x010,
		/**
		 * Coordinates for texture unit \c 0.
		 * \n
		 * Stored as 2x (\e signed) \c short, normalised.
		 */
		TEXT_UV_0 = 0x020,
		/**
		 * Coordinates for texture unit \c 1.
		 * \n
		 * Stored as 2x (\e signed) \c short, normalised.
		 */
		TEXT_UV_1 = 0x040,
		/**
		 * Internal marker for the last attribute.
		 */
		LAST = TEXT_UV_1
	};

	//*************************************************************************/

	/**
	 * Creates a new empty packing.
	 *
	 * \param[in] root start of the \e block where this vertex attributes will be stored
	 * \param[in] size number of bytes (used only for bounds tests in debug builds)
	 */
	PackedVertex(uint8_t* const root, int const size);

	/**
	 * Destroys the vertex (noting that the content is owned externally).
	 */
	~PackedVertex();

private:
	/**
	 * Internal storage types.
	 *
	 * \note Individual support notes are aimed at ANGLE and older D3D.
	 *
	 * \todo re-add support for \c 10_10_10_2 formats?
	 */
	enum Storage {
		/**
		 * Signed \c byte (normalised to fit the range \c -1.0 to \c +1.0).
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		SINT08N,
		/**
		 * Signed \c byte (clamped to the range \c -128 to \c +127).
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		SINT08C,
		/**
		 * Unsigned \c byte (normalised to fit the range \c 0.0 to \c 1.0).
		 */
		UINT08N,
		/**
		 * Unsigned \c byte (clamped to the range \c 0 to \c 255).
		 *
		 * \note Incompatible with D3D index buffers (use \c #UINT16C).
		 */
		UINT08C,
		/**
		 * Signed \c short (normalised to fit the range \c -1.0 to \c +1.0).
		 * Stored as two consecutive bytes.
		 */
		SINT16N,
		/**
		 * Signed \c short (clamped to the range \c -32768 to \c +32767).
		 * Stored as two consecutive bytes.
		 */
		SINT16C,
		/**
		 * Unsigned \c short (normalised to fit the range \c 0.0 to \c 1.0).
		 * Stored as two consecutive bytes.
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		UINT16N,
		/**
		 * Unsigned \c short (clamped to the range \c 0 to \c 65535). Stored as
		 * two consecutive bytes.
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		UINT16C,
		/**
		 * Half-precision \c float (IEEE 754-2008 format). Stored as two
		 * consecutive bytes.
		 *
		 * \note Hardware support should be queried before using.
		 */
		FLOAT16,
		/**
		 * Single-precision \c float (IEEE 754 format). Stored as four
		 * consecutive bytes.
		 */
		FLOAT32,
};
};
