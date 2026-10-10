#ifndef RESTIMG_HPP
#define RESTIMG_HPP

#include <dolphin/types.h>

struct ResTIMG {
	/* 0x00 */ u8 format;
	/* 0x01 */ u8 alphaEnabled;
	/* 0x02 */ u16 width;
	/* 0x04 */ u16 height;
	/* 0x06 */ u8 wrapS;
	/* 0x07 */ u8 wrapT;
	/* 0x08 */ u8 isIndexTexture;
	/* 0x09 */ u8 colorFormat;
	/* 0x0A */ u16 numColors;
	/* 0x0C */ u32 paletteOffset;
	/* 0x10 */ u8 mipmapEnabled;
	/* 0x11 */ u8 doEdgeLod;
	/* 0x12 */ u8 biasClamp;
	/* 0x13 */ u8 maxAnisotropy;
	/* 0x14 */ u8 minFilter;
	/* 0x15 */ u8 magFilter;
	/* 0x16 */ s8 minLod;
	/* 0x17 */ s8 maxLod;
	/* 0x18 */ u8 mipmapCount;
	/* 0x19 */ char unk19[1];
	/* 0x1A */ s16 lodBias;
	/* 0x1C */ u32 imageDataOffset;
}; // Size: 0x20

#ifdef TARGET_PC
#include <hostendian.h>

// BTI headers are big-endian on disc, but the struct has the same 0x20 bytes on the host (no pointer or long
// members), so the header is converted in place and keeps its layout. Image and palette bytes are not touched: Aurora's
// GX converts the texel formats itself. paletteOffset/imageDataOffset stay 32-bit self-relative deltas; read them as
// s32 (they are negative once J3DTexture::setResTIMG copied the header away from its image).
//
// Idempotency marker: unk19 (byte 0x19, padding of the BTI format). The swapped header holds RESTIMG_HOST_MARK there;
// raw headers on the disc never do: byte 0x19 is 0 in all 733 .bti files and all 2959 JPA texture headers of the disc,
// and J3D TEX1 headers carry stray bytes there (226 distinct values in 41075 headers) but never 0xDE. Consequences:
//  - swapResTIMG may be called any number of times on the same header (shared resources, re-mounts, cache hits);
//  - a header built on the host in host byte order (JUTTexture(int, int, GXTexFmt), TMirrorCamera) MUST set the marker
//    itself, otherwise the next swapResTIMG call would swap it;
//  - copying a swapped header (J3DTexture::setResTIMG) keeps the marker.
#define RESTIMG_HOST_MARK 0xDE

static inline void swapResTIMG(ResTIMG* timg)
{
	if ((u8)timg->unk19[0] == RESTIMG_HOST_MARK) {
		return;
	}
	timg->width           = be16(timg->width);
	timg->height          = be16(timg->height);
	timg->numColors       = be16(timg->numColors);
	timg->paletteOffset   = be32(timg->paletteOffset);
	timg->lodBias         = (s16)be16((u16)timg->lodBias);
	timg->imageDataOffset = be32(timg->imageDataOffset);
	timg->unk19[0]        = (char)RESTIMG_HOST_MARK;
}
#endif

#endif
