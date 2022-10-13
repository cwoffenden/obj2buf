/**
 * \file bufferdescriptor.h
 * Output buffer descriptor.
 *
 * \copyright 2022 Numfum GmbH
 */
#pragma once

#include "tooloptions.h"

/**
 * Output buffer descriptor. What the interleaved offsets are, where attributes
 * are packed, etc., to be sent to the rendering API.
 */
class BufferDescriptor
{
public:
	/**
	 * Packing of the tangent's sign or other components. Where multiple
	 * components are packed, as would be the case for a second UV channel or
	 * encoded tangents, this marks the first entry.
	 */
	enum Packing {
		PACK_NONE,   /**< No packing, either the component isn't used or there was no space. */
		PACK_POSN_W, /**< Packed in the position's \c w (4th) component. */
		PACK_UV_0_Z, /**< Packed in UV channel 0's \c z (3rd) component. */
		PACK_NORM_Z, /**< Packed in the encoded normal's \c z (3rd) component. */
		PACK_NORM_W, /**< Packed in the normal's \c w (4th) component. */
		PACK_TANS_Z, /**< Packed in the encoded tangent's \c z (3rd) component. */
		PACK_TANS_W, /**< Packed in the tangent's \c w (4th) component. */
	};

	/**
	 * Calculate all of the packing and padding from the user's options.
	 *
	 * \param[in] opts command-line options
	 */
	BufferDescriptor(const ToolOptions& opts);

	/**
	 * Prints the descriptor to \c stdout (as GL calls, since that or similar
	 * is what will be used).
	 *
	 * \param[in] opts command-line options
	 */
	void dump(const ToolOptions& opts) const;

	Packing packSign; /**< Where the single tangent sign was packed. */
	Packing packTans; /**< Where the encoded tangents pair were packed. */

private:
	BufferDescriptor(const BufferDescriptor&) = delete; /**< No copy */
	void operator=(const BufferDescriptor)    = delete; /**< No assign */

	/**
	 * Parameters associated with a generic interleaved vertex attribute. Later
	 * these  will be passed, for example, to \c glVertexAttribPointer(), as
	 * component \c size and \c pointer offset, or in a \c WGPUVertexAttribute
	 * struct, as the \c format and \c offset (noting that in WebGPU the format
	 * also encompasses the data type, e.g.: \c WGPUVertexFormat_Float32x2).
	 */
	struct AttrParams {
		/**
		 * Zero constructor, with the attributes marked as invalid.
		 */
		AttrParams();

		/**
		 * Mark this attribute as valid and set the initial size (the offset
		 * will be fixed but the size might change as other components are
		 * packed).
		 *
		 * \param[in] numComps number of components
		 * \param[in] startOff offset to the start of the components
		 * \param[in] compSize number of bytes in a single component
		 */
		void fill(unsigned const numComps, unsigned const startOff, unsigned const compSize);

		bool valid;      /**< \c true if these attributes are used and exported. */
		unsigned size;   /**< Number of components (e.g.: \c 2 for UVs). */
		unsigned offset; /**< Offset to the first of the components in the interleaved buffer */
		bool aligned;    /**< \c true if the stream needs 4-byte aligning (and has free space). */
	};

	/**
	 * Test that \a what still needs packing, that \a attr isn't aligned and
	 * has enough free space for \a numComps components, and if so, flag it
	 * then increase the \c AttrParams#size by \c 1.
	 * \n
	 * An example, instead of the convoluted description, would be:
	 * \code
	 *	tryPacking(myBuf.packSign, myBuf.posn, 1, PACK_POSN_W)
	 * \endcode
	 * Meaning: if \c posn has space for one extra component, then the single
	 * tangent sign can be packed in the position's \c w axis. Further checks
	 * will have been performed beforehand, verifying the postitions can
	 * support the required signed type.
	 *
	 * \param[in,out] what attribute being packed (e.g. \c #packSign)
	 * \param[in,out] attr which of the attributes to target (e.g.: \c #posn)
	 * \param[in] numComps number of components required (e.g.: \c 1 for \c #packSign)
	 * \param[in] where where the sign was packed (for \c posn this would be \c PACK_POSN_W)
	 */
	static void tryPacking(Packing& what, AttrParams& attr, int const numComps, Packing const where);

	AttrParams posn;  /**< Position attributes. */
	AttrParams uv_0;  /**< UV channel 0 attributes. */
	AttrParams norm;  /**< Normal attributes. */
	AttrParams tans;  /**< Tangent attributes. */
	AttrParams btan;  /**< Bitangent attributes. */
	unsigned stride;  /**< Bytes between each complete vertex (total of all attributes). */
};
