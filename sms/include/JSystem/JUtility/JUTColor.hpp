#ifndef JUT_COLOR_HPP
#define JUT_COLOR_HPP

#include "dolphin/gx/GXStruct.h"

namespace JUtility {

struct TColor : public GXColor {
	TColor(u8 r, u8 g, u8 b, u8 a) { set(r, g, b, a); }
	TColor() { set(0xffffffff); }
	TColor(u32 u32Color) { set(u32Color); }
	TColor(GXColor color) { set(color); }

	// TColor(const TColor& other) { *(GXColor*)this = *(GXColor*)&other; }

	operator u32() const { return toUInt32(); }
#ifdef TARGET_PC
	// A u32 color is the value 0xRRGGBBAA (what a big-endian read of the bytes r, g, b, a yields, what BLO/BMD loaders
	// produce and what GXColor1u32 expects). The console reinterprets the bytes; on the little-endian host that
	// would reverse the channels, so the conversion is explicit.
	u32 toUInt32() const { return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a; }
#else
	u32 toUInt32() const { return *(u32*)&r; }
#endif

	void set(u8 cR, u8 cG, u8 cB, u8 cA)
	{
		r = cR;
		g = cG;
		b = cB;
		a = cA;
	}

#ifdef TARGET_PC
	void set(u32 u32Color)
	{
		r = (u8)(u32Color >> 24);
		g = (u8)(u32Color >> 16);
		b = (u8)(u32Color >> 8);
		a = (u8)u32Color;
	}
#else
	void set(u32 u32Color) { *(u32*)&r = u32Color; }
#endif
	void set(GXColor gxColor) { *(GXColor*)&r = gxColor; }
	GXColor get() const { return *this; }
};

} // namespace JUtility

#endif
