/**
 * \file packedvertex.h
 * Utility to build packed vertex buffers.
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

#include "vec.h"

/**
 * Writes packed vertex data to a buffer.
 */
class VertexPacker
{
public:
	/**
	 * Data storage types.
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

	/**
	 * Creates a new empty packer.
	 *
	 * \param[in] root start of the \e block where the vertex data will be stored
	 * \param[in] size number of bytes (used only for bounds tests in debug builds)
	 */
	VertexPacker(uint8_t* const root, unsigned const size);

#if SIZE_MAX > UINT_MAX
	/**
	 * \copydoc VertexPacker(uint8_t*,unsigned)
	 */
	inline
	VertexPacker(uint8_t* const root, size_t const size) : VertexPacker(root, static_cast<unsigned>(size)) {}
#endif

	/**
	 * Destroys the packer (noting that the content is owned externally).
	 */
	~VertexPacker() = default;

	//****************************************************************************/

	/**
	 * Adds a number to the data stream, converting to the fit as requested.
	 *
	 * \param[in] data value to convert add
	 * \param[in] type conversion and byte storage
	 * \return \c true if adding was successful (\c false if no more storage space is available)
	 */
	bool add(float const data, Storage const type);

	/**
	 * Starts adding to the stream from the beginning.
	 */
	void rewind();

private:
	/**
	 * Not copyable.
	 */
	VertexPacker(const VertexPacker&) = delete;

	/**
	 * Not assignable.
	 */
	void operator =(const VertexPacker&) = delete;

	//****************************************************************************/

	/**
	 * Determines whether \a type can be stored without going out of bounds.
	 *
	 * \param[in] type data type to store
	 * \return \c true if there is sufficient storage space in the underlying buffer
	 */
	bool hasFreeSpace(Storage const type) const;

	/**
	 * Start of the packed data.
	 */
	uint8_t* const root;

	/**
	 * Next available byte.
	 */
	uint8_t* next;

	/**
	 * Address \e after the last byte of the data allocation.
	 */
	uint8_t* const over;
};
