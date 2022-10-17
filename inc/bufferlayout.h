/**
 * \file bufferlayout.h
 * Output buffer descriptor.
 *
 * \copyright 2022 Numfum GmbH
 */
#pragma once

#include "objvertex.h"
#include "tooloptions.h"

/**
 * Output buffer layout descriptor. What the interleaved offsets are, where
 * attributes are packed, etc., to be sent to the rendering API.
 */
class BufferLayout
{
public:
	/**
	 * Calculate all of the packing and padding from the user's options.
	 *
	 * \param[in] opts command-line options
	 */
	BufferLayout(const ToolOptions& opts);

	/**
	 * Prints the layout to \c stdout (as GL calls).
	 */
	void dump() const;

	/**
	 * Write a single \a vertex to the \a packer using this buffer layout (all
	 * vertices will be written with the same layout).
	 *
	 * \param[in] packer target for the packed vertex
	 * \param[in] vertex data to write
	 * \return \c true if adding to \a packer was successful
	 */
	bool write(VertexPacker& packer, const ObjVertex& vertex) const;

private:
	BufferLayout  (const BufferLayout&) = delete; /**< Not copyable   */
	void operator=(const BufferLayout)  = delete; /**< Not assignable */

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
	 * Parameters associated with a generic interleaved vertex attribute. Later
	 * these will be passed, for example, to \c glVertexAttribPointer(), as
	 * component \c size and \c pointer offset, or in a \c WGPUVertexAttribute
	 * struct, as the \c format and \c offset (noting that in WebGPU the format
	 * also encompasses the data type, e.g.: \c WGPUVertexFormat_Float32x2).
	 */
	struct AttrParams {
		/**
		 * Zero constructor, with the attribute marked as excluded.
		 */
		AttrParams();

		/**
		 * Mark this attribute as valid and set the initial sizes (the offset
		 * will be fixed but the number of components might change as other
		 * components are packed).
		 *
		 * \param[in] attrType storage type
		 * \param[in] startOff offset to the start of the components
		 * \param[in] numComps number of components
		 */
		void fill(VertexPacker::Storage const attrType, unsigned const startOff, unsigned const numComps);

		/**
		 * Performs the test for whether this is \c #unaligned (it's nontrivial
		 * enough to factor into its own method).
		 */
		void validate();

		/**
		 * Calculates the total number of storage bytes required (from the \c
		 * #storage type and number of \c #components, then 4-byte aligned).
		 *
		 * \return the number of bytes required to store a single vertex attribute
		 */
		unsigned getAlignedSize() const;

		/**
		 * Allows testing that this attribute's storage type has been set (and
		 * isn't the default \c EXCLUDE).
		 */
		explicit operator bool() const {
			return storage;
		}

		/**
		 * Prints the attribute to \c stdout (as a GL call).
		 *
		 * \param[in] stride bytes between each complete vertex
		 * \param[in] name name to assign (a constant for the buffer index in GL)
		 */
		void dump(unsigned const stride, const char* name) const;

		/**
		 * Storage type for the attribute. The default is \c EXCLUDE, meaning
		 * this attribute is unused. Once set this should \e not change.
		 */
		VertexPacker::Storage storage;

		/**
		 * Offset to the first of the components in the interleaved buffer.
		 * Once set this should \e not change (it's accumulated from the
		 * previous attributes).
		 */
		unsigned offset;

		/**
		 * Number of components (e.g.: \c 2 for UVs). This starts off with an
		 * initial size but may grow if other attributes are packed in the
		 * padding (to a maximum of \c 4).
		 */
		unsigned components;

		/**
		 * \c true if the output needs 4-byte aligning. Initially being
		 * unaligned denotes padding will be written, so the space can be used
		 * to pack other attributes. This may change as \c #components grows.
		 */
		bool unaligned;
	};

	/**
	 * Test that \a what still needs packing, that \a attr isn't aligned and
	 * has enough free space for \a numComps components, and if so, flag it
	 * then increase the \c AttrParams#components.
	 * \n
	 * An example, instead of the convoluted description, would be:
	 * \code
	 *	tryPacking(myBuf.packSign, myBuf.posn, 1, PACK_POSN_W)
	 * \endcode
	 * Meaning: if \c posn has space for one extra component, then the single
	 * bitangent sign can be packed in the position's \c w axis. Further checks
	 * will have been performed beforehand, verifying the postitions can
	 * support the required signed type.
	 *
	 * \param[in,out] what item being packed (e.g. \c #packSign)
	 * \param[in,out] attr which of the attributes to target (e.g.: \c #posn)
	 * \param[in] numComps number of components required (e.g.: \c 1 for \c #packSign)
	 * \param[in] where where the item was packed (for \c posn this would be \c PACK_POSN_W)
	 * \param[in] force \c true if the item should be packed even if \a attr has no padding
	 */
	static void tryPacking(Packing& what, AttrParams& attr, int const numComps, Packing const where, bool const force = false);

	Packing packSign; /**< Where the single tangent sign was packed. */
	Packing packTans; /**< Where the encoded tangents pair were packed. */
	AttrParams posn;  /**< Position attributes. */
	AttrParams uv_0;  /**< UV channel 0 attributes. */
	AttrParams norm;  /**< Normal attributes. */
	AttrParams tans;  /**< Tangent attributes. */
	AttrParams btan;  /**< Bitangent attributes. */
	unsigned stride;  /**< Bytes between each complete vertex (total of all attributes). */
};
