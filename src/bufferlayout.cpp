/**
 * \file bufferlayout.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "bufferlayout.h"

#include <cstdio>

#include "objvertex.h"
#include "tooloptions.h"

BufferLayout::BufferLayout(const ToolOptions& opts)
	: packSign(PACK_NONE)
	, packTans(PACK_NONE)
{
	/*
	 * No packing if we don't have tangents.
	 */
	bool const hasBitansSign = opts.tans && O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BITANGENTS_SIGN);
	bool const hasTansPacked = opts.tans && O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_TANGENTS_PACKED);
	/*
	 * Starting with all the params at zero, we try to find the best fit.
	 */
	unsigned offset = 0;
	if (opts.posn) {
		/*
		 * Positions are always X, Y & Z (and the offset will always be zero).
		 * For a storage size of 1 or 2, the total bytes will 3 or 6, needing 1
		 * or 2 bytes padding, or (for signed types) allowing the bitangent
		 * sign to be packed.
		 */
		posn.fill(opts.posn, offset, 3);
		if (hasBitansSign && opts.posn.isSigned()) {
			tryPacking(packSign, posn, 1, PACK_POSN_W);
		}
		offset += posn.getAlignedSize();
	}
	if (opts.text) {
		/*
		 * UVs are always X & Y. A storage size of 1 needs 2 bytes, so has the
		 * extreme of also needing 2 bytes of padding. Shorts (or possibly
		 * float16s) fit nicely into 4 bytes so are preferred (plus the range
		 * is more suited). We try to fit the bitangent sign, but since signed
		 * bytes are the only type that will work, it's unlikely to go here.
		 */
		tex0.fill(opts.text, offset, 2);
		if (hasBitansSign && opts.text.isSigned()) {
			tryPacking(packSign, tex0, 1, PACK_TEX0_Z);
		}
		offset += tex0.getAlignedSize();
	}
	if (opts.norm) {
		/*
		 * Unencoded normals are X, Y & Z, encoded are two components (referred
		 * to as X & Y for simplicity). Unencoded can squeeze in the bitangent
		 * sign, but encoded can also fit the encoded tangents. The type should
		 * always be signed.
		 *
		 * TODO: test that we can pack encoded normals and tangents as shorts, for example, taking 8 bytes
		 */
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		norm.fill(opts.norm, offset, (encoded) ? 2 : 3);
		if (hasTansPacked && encoded) {
			tryPacking(packTans, norm, 2, PACK_NORM_Z);
		} else {
			if (hasBitansSign) {
				tryPacking(packSign, norm, 1, (encoded) ? PACK_NORM_Z : PACK_NORM_W);
			}
		}
		offset += norm.getAlignedSize();
	}
	if (opts.tans) {
		/*
		 * If the tangents weren't packed they're written standalone. We try to
		 * pack the bitangents sign but not the bitangents (simply because, if
		 * we've made it to here with unpacked items the formats chosen aren't
		 * packable).
		 */
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		if (packTans == PACK_NONE) {
			tans.fill(opts.tans, offset, (encoded) ? 2 : 3);
			if (hasBitansSign) {
				tryPacking(packSign, tans, 1, (encoded) ? PACK_TANS_Z : PACK_TANS_W);
			}
			offset += tans.getAlignedSize();
		}
		if (packSign == PACK_NONE) {
			/*
			 * Hmm, we've not packed the sign, so haven't picked where the
			 * bitangents will go. We write standalone with the following
			 * components: 1, the sign, 2 encoded, or 3 unencoded. Worst case
			 * is the sign and 3 bytes of padding.
			 */
			btan.fill(opts.tans, offset, (hasBitansSign) ? 1 : ((encoded) ? 2 : 3));
			offset += btan.getAlignedSize();
		}
	}
	stride = offset;
}

void BufferLayout::dump() const {
	posn.dump(stride, "VERT_POSN_ID");
	tex0.dump(stride, "VERT_TEX0_ID");
	norm.dump(stride, "VERT_NORM_ID");
	if (tans) {
		/*
		 * Tangents are (currently) only ever packed in the normals. The
		 * bitangent sign, though, varies.
		 *
		 * TODO: this won't be hit because tans shouldn't be set if packed
		 */
		if (packTans == PACK_NONE) {
			tans.dump(stride, "VERT_TANS_ID");
		} else {
			printf("// Encoded tangents packed in norm.zw (note the four components)\n");
		}
		if (packSign == PACK_NONE) {
			btan.dump(stride, "VERT_BTAN_ID");
		} else {
			const char* element;
			const char* numComp = "four";
			switch (packSign) {
			case PACK_POSN_W:
				element = "posn.w";
				break;
			case PACK_TEX0_Z:
				element = "tex0.z";
				numComp = "three";
				break;
			case PACK_NORM_Z:
				element = "norm.z";
				numComp = "three";
				break;
			case PACK_NORM_W:
				element = "norm.w";
				break;
			case PACK_TANS_Z:
				element = "tans.z";
				numComp = "three";
				break;
			case PACK_TANS_W:
				element = "tans.w";
				break;
			default:
				element = "unknown";
			}
			printf("// Bitangents sign packed in %s (note the %s components)\n", element, numComp);
		}
	}
}

VertexPacker::Failed BufferLayout::writeHeader(VertexPacker& packer) const {
	VertexPacker::Failed failed = false;
	// Horrible but... count the used attributes
	unsigned attrs = 0;
	attrs += (posn) ? 1 : 0;
	attrs += (tex0) ? 1 : 0;
	attrs += (norm) ? 1 : 0;
	attrs += (tans) ? 1 : 0;
	attrs += (btan) ? 1 : 0;
	// Write the header's header
	failed |= packer.add(stride, VertexPacker::Storage::UINT08C);
	failed |= packer.add(attrs,  VertexPacker::Storage::UINT08C);
	// Then each attribute's (if it has no storage it writes nothing)
	failed |= posn.write(packer, VERT_POSN_ID);
	failed |= tex0.write(packer, VERT_TEX0_ID);
	failed |= norm.write(packer, VERT_NORM_ID);
	failed |= tans.write(packer, VERT_TANS_ID);
	failed |= btan.write(packer, VERT_BTAN_ID);
	return failed;
}

VertexPacker::Failed BufferLayout::writeVertex(VertexPacker& packer, const ObjVertex& vertex, size_t const base) const {
	VertexPacker::Failed failed = false;
	/*
	 * Positions and UVs are straightforward. They always write all components,
	 * and optionally pack the tangent sign.
	 */
	if (posn) {
		failed |= vertex.posn.store(packer, posn.storage);
		if (packSign == PACK_POSN_W) {
			failed |= packer.add(vertex.sign, posn.storage);
		}
		if (posn.unaligned) {
			failed |= packer.align(base);
		}
	}
	if (tex0) {
		failed |= vertex.tex0.store(packer, tex0.storage);
		if (packSign == PACK_TEX0_Z) {
			failed |= packer.add(vertex.sign, tex0.storage);
		}
		if (tex0.unaligned) {
			failed |= packer.align(base);
		}
	}
	if (norm) {
		if (packTans == PACK_NORM_Z) {
			/*
			 * This means implicit encoding for both normals and tangents, so
			 * 2-components each. It also excludes packing the sign.
			 */
			failed |= vertex.norm.xy().store(packer, norm.storage);
			failed |= vertex.tans.xy().store(packer, norm.storage);
		} else {
			if (packSign == PACK_NORM_Z) {
				/*
				 * Sign is Z is also implicit encoding for normals.
				 */
				failed |= vertex.norm.xy().store(packer, norm.storage);
				failed |= packer.add(vertex.sign, norm.storage);
			} else {
				/*
				 * Otherwise we have unencoded, 3-component normals, with the
				 * optional sign packed at the end.
				 */
				failed |= vertex.norm.store(packer, norm.storage);
				if (packSign == PACK_NORM_W) {
					failed |= packer.add(vertex.sign, norm.storage);
				}
			}
		}
		if (norm.unaligned) {
			failed |= packer.align(base);
		}
	}
	if (tans) {
		/*
		 * Tangents are written if they weren't packed.
		 *
		 * TODO: this is unfinished
		 */
		if (packTans == PACK_NONE) {
			failed |= vertex.tans.store(packer, tans.storage);
			if (tans.unaligned) {
				failed |= packer.align(base);
			}
		}
	}
	if (btan) {
		if (packTans == PACK_NONE && packSign == PACK_NONE) {
			failed |= vertex.btan.store(packer, btan.storage);
			if (btan.unaligned) {
				failed |= packer.align(base);
			}
		}
	}
	return failed;
}

void BufferLayout::tryPacking(Packing& what, AttrParams& attr, int const numComps, Packing const where, bool const force) {
	if (what == PACK_NONE) {
		/*
		 * Simple rules: attr is being used, isn't aligned so needs padding,
		 * and whether adding extra components will still fit (our limit is GL,
		 * which supports 1, 2, & 4).
		 */
		if (attr && (attr.components + numComps) <= 4 && (attr.unaligned || force)) {
			attr.components += numComps;
			what = where;
			attr.validate();
		}
	}
}

//********************************* AttrParams ********************************/

BufferLayout::AttrParams::AttrParams()
	: storage   (VertexPacker::Storage::EXCLUDE)
	, offset    (0)
	, components(0)
	, unaligned (false) {}

void BufferLayout::AttrParams::fill(VertexPacker::Storage const attrType, unsigned const startOff, unsigned const numComps) {
	storage    = attrType;
	offset     = startOff;
	components = numComps;
	validate();
}

void BufferLayout::AttrParams::validate() {
	if (storage) {
		unaligned = ((components * storage.bytes()) & 3) != 0;
	}
}

unsigned BufferLayout::AttrParams::getAlignedSize() const {
	return ((components * storage.bytes() + 3) / 4) * 4;
}

void BufferLayout::AttrParams::dump(unsigned const stride, const char* name) const {
	if (storage) {
		const char* normalised = (storage.isNormalized()) ? "TRUE" : "FALSE";
		printf("glVertexAttribPointer(%s, %d, GL_%s, GL_%s, %d, (void*) %d);\n",
			name, components, storage.toString(true), normalised, stride, offset);
	}
}

VertexPacker::Failed BufferLayout::AttrParams::write(VertexPacker& packer, unsigned const index) const {
	if (storage) {
		VertexPacker::Failed failed = false;
		/*
		 * This has a limited number of values:
		 *
		 * - index: 0..4, equating to VERT_POSN_ID, VERT_TEX0_ID, etc
		 * - components: 2..4, xy, xyz & xyzw
		 * - type: 1..8, TYPE_BYTE to TYPE_FLOAT with the MSB set for normalised
		 * - offset: 0..44 (given a maximum stride of 56)
		 *
		 * It's not worth the overhead of packing the bits to save a few bytes.
		 */
		int type = static_cast<int>(storage.toBasicType());
		if (storage.isNormalized()) {
			type |= 0x80;
		}
		failed |= packer.add(index,      VertexPacker::Storage::UINT08C);
		failed |= packer.add(components, VertexPacker::Storage::UINT08C);
		failed |= packer.add(type,       VertexPacker::Storage::UINT08C);
		failed |= packer.add(offset,     VertexPacker::Storage::UINT08C);
		return failed;
	}
	return VP_SUCCEEDED;
}
