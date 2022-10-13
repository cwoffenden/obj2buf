/**
 * \file vertexpacker.h
 * Utility to build packed vertex buffers.
 *
 * \copyright 2011-2022 Numfum GmbH
 */
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>

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
		 * Flag to mark as excluded from writing.
		 */
		EXCLUDE = 0,
		/**
		 * Signed \c byte (normalised to fit the range \c -1.0 to \c 1.0).
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		SINT08N,
		/**
		 * Signed \c byte (clamped to the range \c -128 to \c 127).
		 *
		 * \note Incompatible with D3D11 \e Feature \e Level 9_3 vertex data.
		 */
		SINT08C,
		/**
		 * Unsigned \c byte (normalised to fit the range \c 0.0 to \c 1.0).
		 *
		 * \note D3D11 \e Feature \e Level 9_3 requires multiples of four components.
		 */
		UINT08N,
		/**
		 * Unsigned \c byte (clamped to the range \c 0 to \c 255).
		 *
		 * \note D3D11 \e Feature \e Level 9_3 requires multiples of four components.
		 * \note Incompatible with D3D index buffers (use \c #UINT16C).
		 */
		UINT08C,
		/**
		 * Signed \c short (normalised to fit the range \c -1.0 to \c 1.0).
		 * Stored as two consecutive bytes.
		 *
		 * \note D3D11 \e Feature \e Level 9_3 requires multiples of two components.
		 */
		SINT16N,
		/**
		 * Signed \c short (clamped to the range \c -32768 to \c 32767).
		 * Stored as two consecutive bytes.
		 *
		 * \note D3D11 \e Feature \e Level 9_3 requires multiples of two components.
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
		 * Signed \c int (clamped to the range \c -2147483648 to \c
		 * 2147483647). Stored as two consecutive bytes.
		 *
		 * \note Here for completeness.
		 */
		SINT32C,
		/**
		 * Unsigned \c short (clamped to the range \c 0 to \c 4294967295).
		 * Stored as four consecutive bytes.
		 *
		 * \note May be useful as an index buffer format.
		 */
		UINT32C,
		/**
		 * Single-precision \c float (IEEE 754 format). Stored as four
		 * consecutive bytes.
		 */
		FLOAT32,
	};

	/**
	 * Packing options.
	 */
	enum Options {
		/**
		 * Default packing: little endian, normalised signed values preserve
		 * zero (which is the case for current hardware and graphics APIs).
		 */
		OPTS_DEFAULT = 0,
		/**
		 * Multi-byte values are stored as big endian.
		 */
		OPTS_BIG_ENDIAN = 1,
		/**
		 * Normalised signed values are compatible with older APIs, where the
		 * full range of bits is used but zero cannot be represented.
		 */
		OPTS_SIGNED_LEGACY = 2,
	};

	/**
	 * Data type, without the packing or conversion. These are what would be
	 * passed to OpenGL or other graphics APIs.
	 */
	enum BasicType {
		TYPE_UNKNOWN,        /**< Unknown type. */
		TYPE_BYTE,           /**< Signed \c byte. */
		TYPE_UNSIGNED_BYTE,  /**< Unsigned \c byte. */
		TYPE_SHORT,          /**< Signed \c short. */
		TYPE_UNSIGNED_SHORT, /**< Unsigned \c short. */
		TYPE_INT,            /**< Signed \c int. */
		TYPE_UNSIGNED_INT,   /**< Unsigned \c int. */
		TYPE_HALF_FLOAT,     /**< Half-precision \c float. */
		TYPE_FLOAT           /**< Single-precision \c float. */
	};

	/**
	 * Creates a new empty packer.
	 *
	 * \param[in] root start of the \e block where the vertex data will be stored
	 * \param[in] size number of bytes (used only for bounds tests in debug builds)
	 * \param[in] opts packing options (set once before any data are added)
	 */
	VertexPacker(void* const root, unsigned const size, unsigned const opts = OPTS_DEFAULT);

#if SIZE_MAX > UINT_MAX
	/**
	 * \copydoc VertexPacker(uint8_t*,unsigned,unsigned)
	 */
	inline
	VertexPacker(void* const root, size_t const size, unsigned const opts = OPTS_DEFAULT) : VertexPacker(root, static_cast<unsigned>(size), opts) {}
#endif

	/**
	 * Destroys the packer (noting that the content is owned externally).
	 */
	~VertexPacker() = default;

	//*************************************************************************/

	/**
	 * Returns the number of bytes added to the underlying storage.
	 *
	 * \return the number of bytes added
	 */
	size_t size() const;

	/**
	 * Adds a value to the data stream, converting and storing to \a type.
	 *
	 * \param[in] data value to add
	 * \param[in] type conversion and byte storage
	 * \return \c true if adding was successful (\c false if no more storage space is available)
	 */
	bool add(float const data, Storage const type);

	/**
	 * \copydoc #add(float,Storage)
	 */
	bool add(int const data, Storage const type);

	/**
	 * Add padding to 4-byte align the next \c #add(). This will add \c 1, \c 2
	 * or \c 3 bytes if padding is required (otherwise zero).
	 *
	 * \return \c true if adding padding was successful (\c false if no more storage space is available)
	 */
	bool align();

	/**
	 * Starts adding to the stream from the beginning (overwriting any existing
	 * content and allowing underlying storage to be reused).
	 */
	void rewind();

	//*************************************************************************/

	/**
	 * Returns the number of bytes each storage type requires, for example \c 1
	 * byte for \c SINT08N, \c 2 for SINT16N, etc.
	 *
	 * \param[in] type storage type
	 * \return the number of bytes each storage type requires
	 */
	static unsigned bytes(Storage const type);

	/**
	 * Queries whether a storage type is \e signed (otherwise it's \e unsigned).
	 *
	 * \param[in] type storage type
	 * \return \c true if \a type is signed
	 */
	static bool isSigned(Storage const type);

	/**
	 * Returns a data type without the packing or conversion.
	 *
	 * \param[in] type storage type
	 * \return underlying storage type
	 */
	static BasicType toBasicType(Storage const type);

	/**
	 * Returns whether a type is normalised or not.
	 *
	 * \param[in] type storage type
	 * \return \c true if \a type is normalised
	 */
	static bool isNormalized(Storage const type);

private:
	/**
	 * Not copyable.
	 */
	VertexPacker(const VertexPacker&) = delete;

	/**
	 * Not assignable.
	 */
	void operator =(const VertexPacker&) = delete;

	//*************************************************************************/

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
	 * Next available byte (the initial address being \c #root, the final though
	 * unwritable address being \c #over).
	 */
	uint8_t* next;

	/**
	 * Address \e after the last byte of the data allocation.
	 */
	const uint8_t* const over;

	/**
	 * A bitfield of the packer's \c #Options.
	 */
	unsigned const opts;
};
