/**
 * \file bufferlayout.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "bufferlayout.h"

#include <cstdio>

BufferLayout::BufferLayout(const ToolOptions& opts)
	: packSign(PACK_NONE)
	, packTans(PACK_NONE)
{
	bool const hasBitansSign = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BITANGENTS_SIGN);
	bool const hasTansPacked = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_TANGENTS_PACKED);
	/*
	 * Starting with all the params at zero, we try to find the best fit.
	 */
	unsigned offset = 0;
	if (opts.posn) {
		/*
		 * Positions are always X, Y & Z (and the offset will always be zero).
		 * For a storage size or 1 or 2, the total bytes will 3 or 6, needing 1
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
		uv_0.fill(opts.text, offset, 2);
		if (hasBitansSign && opts.text.isSigned()) {
			tryPacking(packSign, uv_0, 1, PACK_UV_0_Z);
		}
		offset += uv_0.getAlignedSize();
	}
	// TODO: test that we can pack encoded normals and tangents that are shorts, for example, taking 8 bytes
	if (opts.norm) {
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		norm.fill(opts.norm, offset, (encoded) ? 2 : 3);
		if (hasTansPacked && encoded) {
			tryPacking(packTans, norm, 2, PACK_NORM_Z);
		} else {
			if (hasBitansSign && opts.norm.isSigned()) {
				tryPacking(packSign, norm, 1, (encoded) ? PACK_NORM_Z : PACK_NORM_W);
			}
		}
		offset += norm.getAlignedSize();
	}
	if (opts.tans) {
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		if (packTans == PACK_NONE) {
			tans.fill(opts.tans, offset, (encoded) ? 2 : 3);
			if (hasBitansSign && opts.tans.isSigned()) {
				tryPacking(packSign, tans, 1, (encoded) ? PACK_TANS_Z : PACK_TANS_W);
			}
			offset += tans.getAlignedSize();
		}
	}
	stride = offset;
}

void BufferLayout::dump() const {
	posn.dump(stride);
	uv_0.dump(stride);
	norm.dump(stride);
	if (tans.storage) {
		if (packTans == PACK_NONE) {
			tans.dump(stride);
		} else {
			printf("// Encoded tangents packed in norm.zw (note the four components)\n");
		}
		if (packSign == PACK_NONE) {
			btan.dump(stride);
		} else {
			printf("// Bitangents sign packed in posn.w (note the four components)\n");
		}
	}
}

bool BufferLayout::write(VertexPacker& packer, const ObjVertex& vertex) const {
	/*
	 * TODO: encoding normals and tangents
	 * TODO: general padding
	 */
	if (posn) {
		vertex.posn.store(packer, posn.storage);
		if (packSign == PACK_POSN_W) {
			packer.add(vertex.sign, posn.storage);
		}
		if (posn.unaligned) {
			packer.align();
		}
	}
	if (uv_0) {
		vertex.uv_0.store(packer, uv_0.storage);
		if (packSign == PACK_UV_0_Z ) {
			packer.add(vertex.sign, uv_0.storage);
		}
		if (uv_0.unaligned) {
			packer.align();
		}
	}
	if (norm) {
		vertex.norm.store(packer, norm.storage);
		if (packTans == PACK_NORM_W) {
			packer.add(vertex.tans.x, norm.storage);
			packer.add(vertex.tans.y, norm.storage);
		}
		if (norm.unaligned) {
			packer.align();
		}
	}
	if (tans) {
		if (packTans == PACK_NONE) {
			vertex.tans.store(packer, tans.storage);
			if (tans.unaligned) {
				packer.align();
			}
		}
	}
	if (btan) {
		if (packTans == PACK_NONE) {
			vertex.btan.store(packer, btan.storage);
			if (btan.unaligned) {
				packer.align();
			}
		}
	}
	return false;
}

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

void BufferLayout::AttrParams::dump(unsigned const stride) const {
	if (storage) {
		const char* normalised = (storage.isNormalized()) ? "TRUE" : "FALSE";
		printf("glVertexAttribPointer(VERT_POSN, %d, GL_%s, GL_%s, %d, %d)\n",
			components, storage.toString(true), normalised, stride, offset);
	}
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
