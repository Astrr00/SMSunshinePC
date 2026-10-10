#ifndef JUT_FONT_HPP
#define JUT_FONT_HPP

#include <JSystem/JUtility/JUTColor.hpp>
#include <string.h>

struct ResFONT {
	struct INF1 {
		/* 0x00 */ u32 magic;
		/* 0x04 */ u32 size;
		/* 0x08 */ u16 fontType;
		/* 0x0A */ u16 ascent;
		/* 0x0C */ u16 descent;
		/* 0x0E */ u16 width;
		/* 0x10 */ u16 leading;
		/* 0x12 */ u16 defaultCode;
	};

	struct WID1 {
		/* 0x00 */ u32 magic;
		/* 0x04 */ u32 size;
		/* 0x08 */ u16 startCode;
		/* 0x0A */ u16 endCode;
		/* 0x0C */ u8 mChunkNum[4];
	};

	struct MAP1 {
		/* 0x00 */ u32 magic;
		/* 0x04 */ u32 size;
		/* 0x08 */ u16 mappingMethod;
		/* 0x0A */ u16 startCode;
		/* 0x0C */ u16 endCode;
		/* 0x0E */ u16 numEntries;
		/* 0x10 */ u16 mLeading;
	};

	struct GLY1 {
		/* 0x00 */ u32 magic;
		/* 0x04 */ u32 size;
		/* 0x08 */ u16 startCode;
		/* 0x0A */ u16 endCode;
		/* 0x0C */ u16 cellWidth;
		/* 0x0E */ u16 cellHeight;
		/* 0x10 */ u32 textureSize;
		/* 0x14 */ u16 textureFormat;
		/* 0x16 */ u16 numRows;
		/* 0x18 */ u16 numColumns;
		/* 0x1A */ u16 textureWidth;
		/* 0x1C */ u16 textureHeight;
		/* 0x1E */ u16 padding;
		/* 0x20 */ u8 data[];
	};

	/* 0x00 */ u64 magic;
	/* 0x08 */ u32 filesize;
	/* 0x0C */ u32 numBlocks;
	/* 0x10 */ u8 padding[0x10];
	/* 0x20 */ u8 data[];
};

#ifdef TARGET_PC
#include <hostendian.h>

// BFN fonts are big-endian on disc. Every field of the header and of the INF1/WID1/MAP1/GLY1 blocks is u8/u16/u32
// (or u64 for the magic), so the structs keep their disc layout and the file is converted in place, once, where
// JUTResFont first sees it. The glyph sheets (GLY1::data) and the WID1 width pairs are bytes and stay untouched:
// Aurora's GX converts the I4/IA4/... texels itself.
// Idempotency: 'FONTbfn1' is the 8 magic bytes in file order. As a host u64 it reads RESFONT_MAGIC after the swap and
// its byte reverse before it, so a second call (the same ResFONT is handed to every J2DTextBox of an archive) sees
// RESFONT_MAGIC and returns; a buffer that is not a BFN at all is left untouched.
#define RESFONT_MAGIC 0x464F4E5462666E31ull

static inline void swapResFONT(ResFONT* font)
{
	if (font->magic != HOST_BSWAP64(RESFONT_MAGIC)) {
		return;
	}
	font->magic     = RESFONT_MAGIC;
	font->filesize  = be32(font->filesize);
	font->numBlocks = be32(font->numBlocks);

	u8* block = font->data;
	for (u32 i = 0; i < font->numBlocks; i++) {
		swapInPlace32(block);
		swapInPlace32(block + 4);
		u32 type = *(u32*)block;
		u32 size = *(u32*)(block + 4);

		u32 u16End = 0; // end of the run of u16 fields that follows the 8 byte block header
		switch (type) {
		case 'INF1':
			u16End = 0x14;
			break;
		case 'WID1':
			u16End = 0xC; // startCode, endCode; the width pairs behind them are bytes
			break;
		case 'MAP1':
			u16End = size & ~1u; // mappingMethod .. numEntries, then the u16 leading/code table
			break;
		case 'GLY1':
			u16End = 0x20;
			break;
		}
		for (u32 off = 8; off + 2 <= u16End; off += 2) {
			// GLY1::textureSize is the only u32 inside the run
			if (type == 'GLY1' && off == 0x10) {
				swapInPlace32(block + off);
				off += 2;
			} else {
				swapInPlace16(block + off);
			}
		}
		block += size;
	}
}
#endif

class JUTFont {
public:
	JUTFont();
	virtual ~JUTFont() { }

	struct TWidth {
		u8 field_0x0;
		u8 field_0x1;
	};

	/* 0x0C */ virtual void setGX() = 0;
	/* 0x10 */ virtual void setGX(JUtility::TColor col1, JUtility::TColor col2)
	{
		setGX();
	}
	/* 0x14 */ virtual f32 drawChar_scale(f32 a1, f32 a2, f32 a3, f32 a4,
	                                      int a5, bool a6)
	    = 0;
	/* 0x18 */ virtual int getLeading() const                            = 0;
	/* 0x1C */ virtual s32 getAscent() const                             = 0;
	/* 0x20 */ virtual s32 getDescent() const                            = 0;
	/* 0x24 */ virtual s32 getHeight() const                             = 0;
	/* 0x28 */ virtual s32 getWidth() const                              = 0;
	/* 0x2C */ virtual void getWidthEntry(int i_no, TWidth* width) const = 0;
	/* 0x30 */ virtual int getFontType() const                           = 0;
	/* 0x34 */ virtual bool isLeadByte(int a1) const                     = 0;

	static bool isLeadByte_1Byte(int b) { return false; }
	static bool isLeadByte_2Byte(int b) { return true; }
	static bool isLeadByte_ShiftJIS(int b)
	{
		return (b >= 0x81 && b <= 0x9f) || (b >= 0xe0 && b <= 0xfc);
	}
	static bool isLeadByte_EUC(int b)
	{
		return (b >= 0xA1 && b <= 0xFE) || b == 0x8E;
	}

	void initiate();
	void setCharColor(JUtility::TColor col1);
	void setGradColor(JUtility::TColor col1, JUtility::TColor col2);
	f32 drawString_size_scale(f32 posX, f32 posY, f32 width, f32 height,
	                          const char* str, u32 usz, bool visible);

	void drawString(int posX, int posY, const char* str, bool visible)
	{
		drawString_size(posX, posY, str, strlen(str), visible);
	}

	void drawString_size(int posX, int posY, const char* str, u32 len,
	                     bool visible)
	{
		drawString_size_scale(posX, posY, getWidth(), getHeight(), str, len,
		                      visible);
	}

	f32 drawString_scale(f32 posX, f32 posY, f32 width, f32 height,
	                     const char* str, bool visible)
	{
		return (int)drawString_size_scale(posX, posY, width, height, str,
		                                  strlen(str), visible);
	}

	int getWidth(int i_no) const
	{
		TWidth width;
		getWidthEntry(i_no, &width);
		return width.field_0x1;
	}

	bool isValid() const { return mValid; }
	bool isFixed() const { return mFixed; }
	int getFixedWidth() const { return mFixedWidth; }

	/* 0x04 */ bool mFixed;
	/* 0x05 */ bool mValid; // TODO: maybe non-existent
	/* 0x08 */ int mFixedWidth;
	/* 0x0C */ JUtility::TColor mColor1;
	/* 0x10 */ JUtility::TColor mColor2;
	/* 0x14 */ JUtility::TColor mColor3;
	/* 0x18 */ JUtility::TColor mColor4;
};

#endif
