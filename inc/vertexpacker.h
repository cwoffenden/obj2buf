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
 * \def VP_FAILED
 * \c VertexPacker#Failed type denoting that an operation failed.
 */
#ifndef VP_FAILED
#define VP_FAILED true
#endif

/**
 * \def VP_SUCCEEDED
 * \c VertexPacker#Failed type denoting that an operation succeeded.
 */
#ifndef VP_SUCCEEDED
#define VP_SUCCEEDED false
#endif

/**
 * Writes packed vertex data to a buffer.
 */
class VertexPacker
{
public:
	/**
	 * Type to denote a failure when packing.
	 *
	 * \note Notifying of a failure is preferable to a success since a simple
	 * \c or can accumulate if any occurred (rather than needing to negate all
	 * the results).
	 */
	typedef bool Failed;

	/**
	 * Data storage types.
	 *
	 * \note Individual support notes are aimed at ANGLE and older D3D.
	 *
	 * \todo re-add support for \c 10_10_10_2 formats?
	 */
	class Storage
	{
	public:
		/**
		 * Internal enum (accessed as \c VertexPacker::Storage::MY_TYPE).
		 */
		enum Type {
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
		 * Basic data types, underlying each \c Type. For the rendering API, \c
		 * TYPE_BYTE will map to GL_BYTE, etc.
		 */
		enum BasicType {
			TYPE_NONE = 0,           /**< \c EXCLUDE. */
			TYPE_BYTE = 1,           /**< \c SINT08N and \c SINT08C. */
			TYPE_UNSIGNED_BYTE = 2,  /**< \c UINT08N and \c UINT08C.. */
			TYPE_SHORT = 3,          /**< \c SINT16N and \c SINT16C. */
			TYPE_UNSIGNED_SHORT = 4, /**< \c UINT16N and \c UINT16C. */
			TYPE_INT = 5,            /**< \c SINT32C. */
			TYPE_UNSIGNED_INT = 6,   /**< \c UINT32C. */
			TYPE_HALF_FLOAT = 7,     /**< \c FLOAT16. */
			TYPE_FLOAT = 8,          /**< \c FLOAT32. */
		};

		/**
		 * Defaults to \c EXCLUDE.
		 */
		Storage() : type(EXCLUDE) {}

		/**
		 * Implicit wrapper to get from \c Type to \c Storage.
		 */
		constexpr Storage(Type type) : type(type) {}

		/**
		 * Wrapper to get from \c Storage to \c Type.
		 */
		constexpr operator Type() const {
			return type;
		}

		/**
		 * Allows testing that a storage type has been set (and isn't the
		 * default \c EXCLUDE).
		 */
		explicit constexpr operator bool() const {
			return type != EXCLUDE;
		}

		//************************ Helpers/Conversions ************************/

		/**
		 * Returns the number of bytes each storage type requires, for example
		 * \c 1 byte for \c SINT08N, \c 2 for SINT16N, etc.
		 *
		 * \return the number of bytes each storage type requires
		 */
		unsigned bytes() const {
			switch (type) {
			case EXCLUDE:
				return 0;
			case SINT08N:
			case SINT08C:
			case UINT08N:
			case UINT08C:
				return 1;
			case SINT16N:
			case SINT16C:
			case UINT16N:
			case UINT16C:
			case FLOAT16:
				return 2;
			default:
				return 4;
			}
		}

		/**
		 * Returns the basic data type, underlying each \c Type
		 *
		 * \return underlying data type (e.g. \c TYPE_BYTE for \c SINT08N)
		*/
		BasicType toBasicType() const {
			switch (type) {
			case SINT08N:
			case SINT08C:
				return TYPE_BYTE;
			case UINT08N:
			case UINT08C:
				return TYPE_UNSIGNED_BYTE;
			case SINT16N:
			case SINT16C:
				return TYPE_SHORT;
			case UINT16N:
			case UINT16C:
				return TYPE_UNSIGNED_SHORT;
			case FLOAT16:
				return TYPE_HALF_FLOAT;
			case SINT32C:
				return TYPE_INT;
			case UINT32C:
				return TYPE_UNSIGNED_INT;
			case FLOAT32:
				return TYPE_FLOAT;
			default:
				return TYPE_NONE;
			}
		}

		/**
		 * Returns the storage type to a string.
		 *
		 * \param[in] upper \c true if the string should be all upper-case
		 * \return string equivalent (or \c N/A of there is no match)
		 */
		const char* toString(bool const upper = false) const {
			if (upper) {
				switch (type) {
				case SINT08N:
				case SINT08C:
					return "BYTE";
				case UINT08N:
				case UINT08C:
					return "UNSIGNED_BYTE";
				case SINT16N:
				case SINT16C:
					return "SHORT";
				case UINT16N:
				case UINT16C:
					return "UNSIGNED_SHORT";
				case FLOAT16:
					return "HALF_FLOAT";
				case SINT32C:
					return "INT";
				case UINT32C:
					return "UNSIGNED_INT";
				case FLOAT32:
					return "FLOAT";
				default:
					return "N/A";
				}
			} else {
				switch (type) {
				case SINT08N:
				case SINT08C:
					return "byte";
				case UINT08N:
				case UINT08C:
					return "unsigned byte";
				case SINT16N:
				case SINT16C:
					return "short";
				case UINT16N:
				case UINT16C:
					return "unsigned short";
				case FLOAT16:
					return "half float";
				case SINT32C:
					return "int";
				case UINT32C:
					return "unsigned int";
				case FLOAT32:
					return "float";
				default:
					return "N/A";
				}
			}
		}

		/**
		 * Queries whether a storage type is \e signed (otherwise it's \e unsigned).
		 *
		 * \return \c true if \a type is signed
		 */
		bool isSigned() const {
			switch (type) {
			case EXCLUDE:
			case UINT08N:
			case UINT08C:
			case UINT16N:
			case UINT16C:
			case UINT32C:
				return false;
			default:
				return true;
			}
		}

		/**
		 * Returns whether a type is normalised or not.
		 *
		 * \return \c true if \a type is normalised
		 */
		bool isNormalized() const {
			switch (type) {
			case SINT08N:
			case UINT08N:
			case SINT16N:
			case UINT16N:
				return true;
			default:
				return false;
			}
		}

		/**
		 * Returns the number of bits used to pack the fractional part of a
		 * number. All the clamped types return zero, 8- and 16-bit normalised
		 * types \c 8 and \c 16 respectively, single-precision floats \c 23,
		 * and half-precision \c 10.
		 *
		 * \return number of fractional bits for a type
		 */
		unsigned fractionBits() const {
			switch (type) {
			case SINT08N:
			case UINT08N:
				return 8;
			case SINT16N:
			case UINT16N:
				return 16;
			case FLOAT16:
				return 10;
			case FLOAT32:
				return 23;
			default:
				return 0;
			}
		}

	private:
		/**
		 * Internal type.
		 */
		Type type;
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
		 * Normalised \e signed values are compatible with older APIs, where the
		 * full range of bits is used but zero cannot be represented.
		 */
		OPTS_SIGNED_LEGACY = 2,
	};

	/**
	 * Creates a new empty packer.
	 *
	 * \param[in] root start of the \e block where the vertex data will be stored
	 * \param[in] size number of bytes (used only for bounds tests in debug builds)
	 * \param[in] opts packing options (set once before any data are added)
	 */
	VertexPacker(void* const root, size_t const size, unsigned const opts = OPTS_DEFAULT);

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
	 * \return \c VP_FAILED if adding failed (e.g. if no more storage space is available)
	 */
	Failed add(float const data, Storage const type);

	/**
	 * \copydoc #add(float,Storage)
	 */
	Failed add(int const data, Storage const type);

	/**
	 * \copydoc #add(float,Storage)
	 */
	inline
	Failed add(unsigned const data, Storage const type) {
		return add(static_cast<int>(data), type);
	}

	/**
	 * Add padding to 4-byte align the next \c #add(). This will add \c 1, \c 2
	 * or \c 3 bytes if padding is required (otherwise zero).
	 *
	 * \note The \a base offset allows a large buffer to be split, without
	 * needing to add alignments between each part (for example, a header
	 * followed by vertex data doesn't need the header aligning in order to keep
	 * the vertex data aligned, simply by passing the header size as the base).
	 *
	 * \param[in] base offset from the start to align against (defaulting to zero)
	 * \return \c VP_FAILED if adding failed (e.g. if no more storage space is available)
	 */
	Failed align(size_t const base = 0);

	/**
	 * Starts adding to the stream from the beginning (overwriting any existing
	 * content and allowing underlying storage to be reused).
	 */
	void rewind();

private:
	VertexPacker   (const VertexPacker&) = delete; /**< Not copyable   */
	void operator =(const VertexPacker&) = delete; /**< Not assignable */

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
