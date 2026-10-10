#include <JSystem/JUtility/JUTResource.hpp>
#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JSupport/JSUInputStream.hpp>
#include <string.h>
#ifdef TARGET_PC
#include <JSystem/ResTIMG.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <glyphs.h>
#endif

void* JUTResReference::getResource(JSUInputStream* stream, u32 resType,
                                   JKRArchive* archive)
{
	stream->read(&mType, 1);
	stream->read(&mNameLength, 1);
	stream->read(&mName, mNameLength);

	if (mType == RESTYPE_Unk2 || mType == RESTYPE_Unk3
	    || mType == RESTYPE_Unk4) {
		mName[mNameLength] = 0;
	}

	return getResource(resType, archive);
}

#ifdef TARGET_PC
namespace {

struct GlyphSlot {
	const char* name;
	PadButton button;
};

// Button-icon BTIs and the glyph each one shows. Other textures are never touched.
const GlyphSlot kGlyphSlots[] = {
	{"guide_tx_a.bti", PadButton::A},
	{"guide_tx_b.bti", PadButton::B},
	{"water_button_x.bti", PadButton::X},
	{"select_start.bti", PadButton::Start},
};
const size_t kGlyphSlotCount = sizeof(kGlyphSlots) / sizeof(kGlyphSlots[0]);

// One replacement header per slot and style. Allocated once, never freed (same as JUTTexture images).
ResTIMG* sGlyphTimg[kGlyphSlotCount][(size_t)GlyphStyle::Count];

// Aspect-fits the glyph into a w x h RGBA8 texture (transparent border), box-filtered with alpha weighting.
ResTIMG* buildGlyphTimg(const GlyphImage& g, u32 w, u32 h)
{
	u8* block = new (0x20) u8[sizeof(ResTIMG) + w * h * 4];
	ResTIMG* t = (ResTIMG*)block;
	memset(t, 0, sizeof(ResTIMG));
	t->format          = 0x6 | 0x40; // GX_TF_RGBA8_PC (Aurora: 0x6 | _GX_TF_PC); the game's dolphin/gx.h hides it
	t->alphaEnabled    = 1;
	t->width           = w;
	t->height          = h;
	t->minFilter       = 1;
	t->magFilter       = 1;
	t->mipmapCount     = 1;
	t->imageDataOffset = sizeof(ResTIMG);
	t->unk19[0]        = (char)RESTIMG_HOST_MARK; // built in host byte order: storeTIMG must not swap it

	u8* px = block + sizeof(ResTIMG);
	memset(px, 0, w * h * 4);
	u32 fw = w, fh = h;
	if (w * g.height <= h * g.width) {
		fh = w * g.height / g.width;
	} else {
		fw = h * g.width / g.height;
	}
	u32 ox = (w - fw) / 2, oy = (h - fh) / 2;
	for (u32 y = 0; y < fh; ++y) {
		u32 sy0 = y * g.height / fh, sy1 = (y + 1) * g.height / fh;
		if (sy1 <= sy0) {
			sy1 = sy0 + 1;
		}
		for (u32 x = 0; x < fw; ++x) {
			u32 sx0 = x * g.width / fw, sx1 = (x + 1) * g.width / fw;
			if (sx1 <= sx0) {
				sx1 = sx0 + 1;
			}
			u32 sumR = 0, sumG = 0, sumB = 0, sumA = 0, n = 0;
			for (u32 sy = sy0; sy < sy1; ++sy) {
				for (u32 sx = sx0; sx < sx1; ++sx) {
					const u8* s = g.rgba + (sy * g.width + sx) * 4;
					u32 a = s[3];
					sumR += s[0] * a;
					sumG += s[1] * a;
					sumB += s[2] * a;
					sumA += a;
					++n;
				}
			}
			u8* d = px + ((oy + y) * w + (ox + x)) * 4;
			if (sumA) {
				d[0] = (u8)(sumR / sumA);
				d[1] = (u8)(sumG / sumA);
				d[2] = (u8)(sumB / sumA);
			}
			d[3] = (u8)(sumA / n);
		}
	}
	return t;
}

// ponytail: a style change takes effect the next time each icon's texture is loaded; live J2DPicture textures keep
// their current style. Upgrade: re-store TIMG on the live pictures when glyphsSetStyle is called.
void* glyphReplaceTIMG(u32 resType, const char* name, void* res)
{
	if (resType != 'TIMG' || !res) {
		return res;
	}
	for (size_t i = 0; i < kGlyphSlotCount; ++i) {
		if (strcmp(name, kGlyphSlots[i].name) != 0) {
			continue;
		}
		ResTIMG* orig = (ResTIMG*)res;
		swapResTIMG(orig); // BTI header is big-endian on disc; swapping here is idempotent with storeTIMG's own swap
		const GlyphImage* g = glyphFor(g_glyphStyle, kGlyphSlots[i].button);
		if (!g) {
			return res;
		}
		ResTIMG*& cached = sGlyphTimg[i][(size_t)g_glyphStyle];
		if (!cached) {
			cached = buildGlyphTimg(*g, orig->width, orig->height);
		}
		return cached;
	}
	return res;
}

} // namespace
#endif

void* JUTResReference::getResource(u32 resType, JKRArchive* archive)
{
	void* res = NULL;
	switch (mType) {
	case RESTYPE_Unk1:
		break;
	case RESTYPE_Unk2:
#ifdef TARGET_PC
		res = glyphReplaceTIMG(resType, mName, JKRArchive::getGlbResource(resType, mName, archive));
#else
		res = JKRArchive::getGlbResource(resType, mName, archive);
#endif
		break;
	case RESTYPE_Unk3:
		res = JKRFileLoader::getGlbResource(mName, archive);
		break;
	case RESTYPE_Unk4:
		res = JKRFileLoader::getGlbResource(mName);
		break;
	}

	return res;
}
