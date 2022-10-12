/**
 * \file bufferdescriptor.cpp
 *
 * \copyright 2022 Numfum GmbH
 */
#include "bufferdescriptor.h"

#include <cstdio>

#include "tooloptions.h"

void BufferDescriptor::AttrParams::fill(unsigned const numComps, unsigned const startOff, unsigned const compSize) {
	valid   = true;
	size    = numComps;
	offset  = startOff;
	aligned = ((size * compSize) & 3) == 0;
}

BufferDescriptor::BufferDescriptor(const ToolOptions& opts)
	: packSign(PACK_NONE)
	, packTans(PACK_NONE)
{
	bool const hasBitansSign = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_BITANGENTS_SIGN);
	bool const hasTansPacked = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_TANGENTS_PACKED);
	/*
	 * Starting with all the params at zero, we try to find the best fit.
	 */
	unsigned offset = 0;
	if (unsigned compBytes = VertexPacker::bytes(opts.posn)) {
		/*
		 * Positions are always X, Y & Z (and the pointer will always be zero).
		 * For a component size or 1 or 2, the bytes will 3 or 6, needing 1 or
		 * 2 bytes padding, or (for signed types) allowing the tangent sign to
		 * be packed.
		 */
		posn.fill(3, offset, compBytes);
		if (hasBitansSign && VertexPacker::isSigned(opts.posn)) {
			tryPacking(packSign, posn, 1, PACK_POSN_W);
		}
		offset += ((posn.size * compBytes + 3) / 4) * 4;

	}
	if (unsigned compBytes = VertexPacker::bytes(opts.text)) {
		/*
		 * UVs are always X & Y. A component size of 1 needs 2 bytes, so has
		 * the extreme of also needing 2 bytes of padding. Shorts (or possibly
		 * float16s) fit nicely into 4 bytes so are preferred (plus the range is
		 * more suited). We try to fit the tangent sign, but since signed bytes
		 * are the only type that will work, it's unlikely to go here.
		 */
		uv_0.fill(2, offset, compBytes);
		if (hasBitansSign && VertexPacker::isSigned(opts.text)) {
			tryPacking(packSign, uv_0, 1, PACK_UV_0_Z);
		}
		offset += ((uv_0.size * compBytes + 3) / 4) * 4;
	}
	if (unsigned compBytes = VertexPacker::bytes(opts.norm)) {
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		norm.fill((encoded) ? 2 : 3, offset, compBytes);
		if (hasTansPacked && encoded) {
			tryPacking(packTans, norm, 2, PACK_NORM_Z);
		} else {
			if (hasBitansSign && VertexPacker::isSigned(opts.norm)) {
				tryPacking(packSign, norm, 1, (encoded) ? PACK_NORM_Z : PACK_NORM_W);
			}
		}
		offset += ((norm.size * compBytes + 3) / 4) * 4;
	}
	if (unsigned compBytes = VertexPacker::bytes(opts.tans)) {
		bool const encoded = O2B_HAS_OPT(opts.opts, ToolOptions::OPTS_NORMALS_ENCODED);
		if (packTans == PACK_NONE) {
			tans.fill((encoded) ? 2 : 3, offset, compBytes);
			if (hasBitansSign && VertexPacker::isSigned(opts.tans)) {
				tryPacking(packSign, tans, 1, (encoded) ? PACK_TANS_Z : PACK_TANS_W);
			}
			offset += ((tans.size * compBytes + 3) / 4) * 4;
		}
	}
	printf("glVertexAttribPointer(VERT_POSN, %d, GL_FLOAT, GL_FALSE, %d, %d)\n", posn.size, offset, posn.offset);
	printf("glVertexAttribPointer(VERT_UV_0, %d, GL_FLOAT, GL_FALSE, %d, %d)\n", uv_0.size, offset, uv_0.offset);
	printf("glVertexAttribPointer(VERT_NORM, %d, GL_FLOAT, GL_FALSE, %d, %d)\n", norm.size, offset, norm.offset);
	if (packTans == PACK_NONE) {
		printf("glVertexAttribPointer(VERT_TANS, %d, GL_FLOAT, GL_FALSE, %d, %d)\n", tans.size, offset, tans.offset);
	} else {
		printf("/* Encoded tangents packed in normals */\n");
	}
	if (packSign != PACK_NONE) {
		printf("/* Tangents sign in posn.w */\n");
	}
}

void BufferDescriptor::tryPacking(Packing& what, AttrParams& attr, int const numComps, Packing const where) {
	if (what == PACK_NONE) {
		if (attr.aligned == false && (attr.size + numComps) <= 4) {
			attr.size += numComps;
			what = where;
		}
	}
}
