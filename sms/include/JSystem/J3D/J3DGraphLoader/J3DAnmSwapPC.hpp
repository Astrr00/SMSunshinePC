#ifndef J3D_ANM_SWAP_PC_HPP
#define J3D_ANM_SWAP_PC_HPP

// Host byte order conversion of the J3D animation files: BCK/BCA (ANK1/ANF1), BTK (TTK1), BRK (TRK1), BPK/BPA
// (PAK1/PAF1), BTP (TPT1), BVA (VAF1), BLK/BLA (CLK1/CLF1), BXK/BXA (VCK1/VCF1). TARGET_PC only.
//
// J3DSwapAnmFilePC() is the one entry: J3DAnmLoaderDataBase::load() calls it on the raw file. It returns true exactly
// once per buffer (the marker is the file magic word, see J3DSwapPC.hpp); a second call does not touch the bytes.
//
// All offsets inside a block are relative to the block start. For each block the header fields, the key/full tables
// and every s16/u16/f32 value array the loader hands out are swapped in place. u8 data (texture matrix ids, color and
// visibility values, the colour id + padding after each TRK1 table) and the nested name tables stay untouched.
// An array extent is the element count in the block header where the real files carry one (ANK1, TTK1, TRK1, PAK1,
// TPT1: it equals the highest table reference in every shipped file, tools/j3d-anm-test.cpp checks this), otherwise
// the highest element a table references. Each array is swapped once over that extent, so tables sharing values
// cannot swap a word twice.
// Layouts checked against the disc files: ANK1, TTK1, TRK1, PAK1, TPT1. UNVERIFIED (no sample on disc, derived from
// the decomp structs): ANF1, PAF1, VAF1, CLK1, CLF1, VCK1, VCF1.

#ifdef TARGET_PC

#include <JSystem/J3D/J3DGraphLoader/J3DSwapPC.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>

// On-disc VCK1/VCF1 index record. The host J3DAnmVtxColorIndexData holds a pointer (16 bytes), so the loader builds
// host records from these instead of relocating in place.
struct J3DAnmVtxColorIndexDisc {
	/* 0x00 */ u16 mNum;
	/* 0x02 */ u16 pad;
	/* 0x04 */ u32 mOffset; // in u16 units from the channel's index array
};

// Records overlaid on the file must have the disc size.
static_assert(sizeof(J3DAnmKeyTableBase) == 0x6, "disc layout");
static_assert(sizeof(J3DAnmTransformKeyTable) == 0x12, "disc layout");
static_assert(sizeof(J3DAnmTransformFullTable) == 0xC, "disc layout");
static_assert(sizeof(J3DAnmClusterFullTable) == 0x4, "disc layout");
static_assert(sizeof(J3DAnmClusterKeyTable) == 0x6, "disc layout");
static_assert(sizeof(J3DAnmColorFullTable) == 0x10, "disc layout");
static_assert(sizeof(J3DAnmColorKeyTable) == 0x18, "disc layout");
static_assert(sizeof(J3DAnmVisibilityFullTable) == 0x4, "disc layout");
static_assert(sizeof(J3DAnmTexPatternFullTable) == 0x8, "disc layout");
static_assert(sizeof(J3DAnmCRegKeyTable) == 0x1C, "disc layout");
static_assert(sizeof(J3DAnmKRegKeyTable) == 0x1C, "disc layout");
static_assert(sizeof(J3DAnmVtxColorIndexDisc) == 0x8, "disc layout");
static_assert(sizeof(J3DAnmTransformKeyData) == 0x24, "disc layout");
static_assert(sizeof(J3DAnmTransformFullData) == 0x24, "disc layout");
static_assert(sizeof(J3DAnmColorKeyData) == 0x34, "disc layout");
static_assert(sizeof(J3DAnmColorFullData) == 0x34, "disc layout");
static_assert(sizeof(J3DAnmTextureSRTKeyData) == 0x60, "disc layout");
static_assert(sizeof(J3DAnmTevRegKeyData) == 0x58, "disc layout");
static_assert(sizeof(J3DAnmTexPatternFullData) == 0x20, "disc layout");
static_assert(sizeof(J3DAnmVisibilityFullData) == 0x18, "disc layout");
static_assert(sizeof(J3DAnmClusterFullData) == 0x18, "disc layout");
static_assert(sizeof(J3DAnmClusterKeyData) == 0x18, "disc layout");
static_assert(sizeof(J3DAnmVtxColorFullData) == 0x40, "disc layout");
static_assert(sizeof(J3DAnmVtxColorKeyData) == 0x40, "disc layout");

// One past the last array element a key table references: mMaxFrame keys of 3 values (mType 0) or 4 values (else);
// a single key is a lone value.
static inline u32 J3DAnmKeyEndPC(const J3DAnmKeyTableBase& t)
{
	if (t.mMaxFrame < 2)
		return t.mMaxFrame == 0 ? 0 : t.mOffset + 1u;
	return t.mOffset + (u32)t.mMaxFrame * (t.mType == 0 ? 3u : 4u);
}

// Same for the "full" tables, which store one value per frame.
static inline u32 J3DAnmFullEndPC(u16 maxFrame, u16 offset) { return maxFrame == 0 ? 0 : offset + (u32)maxFrame; }

static inline void J3DAnmMaxPC(u32& m, u32 v)
{
	if (v > m)
		m = v;
}

// One block of the file being converted. Whatever does not fit inside the block is skipped.
struct J3DAnmBlockPC {
	u8* p;
	u32 size;

	bool fits(u32 off, u64 bytes) const { return off != 0 && off <= size && bytes <= size - off; }

	template <typename T> T* at(u32 off, u32 num) const
	{
		return fits(off, (u64)num * sizeof(T)) ? (T*)(p + off) : nullptr;
	}

	void swap16(u32 off, u32 num) const
	{
		if (fits(off, (u64)num * 2))
			J3DSwap16PC(p + off, num);
	}

	void swap32(u32 off, u32 num) const
	{
		if (fits(off, (u64)num * 4))
			J3DSwap32PC(p + off, num);
	}

	// Swaps the first `words` u16 of each of `num` records (trailing u8 fields stay); returns the records or null.
	template <typename T> T* swapRecs(u32 off, u32 num, u32 words) const
	{
		T* t = at<T>(off, num);
		for (u32 i = 0; t && i < num; ++i)
			J3DSwap16PC(&t[i], words);
		return t;
	}
};

// ANK1 (BCK): 0x0C joint count, scale/rotation/translation value counts; 3 key tables per joint.
static inline void J3DSwapANK1PC(J3DAnmTransformKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 4);
	J3DSwap32PC(&d->mTableOffset, 4);
	J3DAnmBlockPC b = { (u8*)d, size };
	b.swapRecs<J3DAnmTransformKeyTable>(d->mTableOffset, d->field_0xc * 3u, 9);
	b.swap32(d->mScaleOffset, d->field_0xe);
	b.swap16(d->mRotOffset, d->field_0x10);
	b.swap32(d->mTransOffset, d->field_0x12);
}

// ANF1 (BCA), unverified: same header as ANK1; 3 full tables per joint, extents from the tables.
static inline void J3DSwapANF1PC(J3DAnmTransformFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 4);
	J3DSwap32PC(&d->mTableOffset, 4);
	J3DAnmBlockPC b = { (u8*)d, size };
	u32 n = d->field_0xc * 3u, s = 0, r = 0, t = 0;
	J3DAnmTransformFullTable* tab = b.swapRecs<J3DAnmTransformFullTable>(d->mTableOffset, n, 6);
	for (u32 i = 0; tab && i < n; ++i) {
		J3DAnmMaxPC(s, J3DAnmFullEndPC(tab[i].mScaleMaxFrame, tab[i].mScaleOffset));
		J3DAnmMaxPC(r, J3DAnmFullEndPC(tab[i].mRotationMaxFrame, tab[i].mRotationOffset));
		J3DAnmMaxPC(t, J3DAnmFullEndPC(tab[i].mTranslateMaxFrame, tab[i].mTranslateOffset));
	}
	b.swap32(d->mScaleValOffset, s);
	b.swap16(d->mRotValOffset, r);
	b.swap32(d->mTransValOffset, t);
}

// PAK1 (BPK): 0x0E material count, R/G/B/A value counts; one key table and one material id per material.
static inline void J3DSwapPAK1PC(J3DAnmColorKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->mUpdateMaterialNum, 5);
	J3DSwap32PC(&d->mTableOffset, 7);
	J3DAnmBlockPC b = { (u8*)d, size };
	b.swapRecs<J3DAnmColorKeyTable>(d->mTableOffset, d->mUpdateMaterialNum, 12);
	b.swap16(d->mUpdateMaterialIDOffset, d->mUpdateMaterialNum);
	b.swap16(d->mRValOffset, d->field_0x10);
	b.swap16(d->mGValOffset, d->field_0x12);
	b.swap16(d->mBValOffset, d->field_0x14);
	b.swap16(d->mAValOffset, d->field_0x16);
}

// PAF1 (BPA), unverified: PAK1 header; full tables, u8 values.
static inline void J3DSwapPAF1PC(J3DAnmColorFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->mUpdateMaterialNum, 5);
	J3DSwap32PC(&d->mTableOffset, 7);
	J3DAnmBlockPC b = { (u8*)d, size };
	b.swapRecs<J3DAnmColorFullTable>(d->mTableOffset, d->mUpdateMaterialNum, 8);
	b.swap16(d->mUpdateMaterialIDOffset, d->mUpdateMaterialNum);
}

// TTK1 (BTK): 0x0C track count (3 key tables per material), 0x0E/0x10/0x12 scale/rotation/translation value counts;
// per material a u16 id, a u8 texture matrix id (untouched) and a Vec SRT center. The same set follows at 0x34..0x5B
// for the post-SRT matrix (all zero on the shipped files). The word at 0x5C is padding (stale memory on the shipped
// files: 0x801514A8, 'This') and stays untouched.
static inline void J3DSwapTTK1PC(J3DAnmTextureSRTKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->field_0xa, 1);
	J3DSwap16PC(&d->field_0xc, 4);
	J3DSwap32PC(&d->mTableOffset, 8);
	J3DSwap16PC(&d->field_0x34, 4);
	J3DSwap32PC(&d->mInfoTable2Offset, 8);
	J3DAnmBlockPC b = { (u8*)d, size };
	u32 n = d->field_0xc;
	b.swapRecs<J3DAnmTransformKeyTable>(d->mTableOffset, n, 9);
	b.swap16(d->mUpdateMatIDOffset, n / 3);
	b.swap32(d->unkOffset, n / 3 * 3);
	b.swap32(d->mScaleValOffset, d->field_0xe);
	b.swap16(d->mRotValOffset, d->field_0x10);
	b.swap32(d->mTransValOffset, d->field_0x12);
	n = d->field_0x34;
	b.swapRecs<J3DAnmTransformKeyTable>(d->mInfoTable2Offset, n, 9);
	b.swap16(d->field_0x40, n / 3);
	b.swap32(d->field_0x4c, n / 3 * 3);
	b.swap32(d->field_0x50, d->field_0x36);
	b.swap16(d->field_0x54, d->field_0x38);
	b.swap32(d->field_0x58, d->field_0x3a);
}

// TRK1 (BRK): 0x0C/0x0E C-/K-register material counts, 0x10..0x16 C and 0x18..0x1E K R/G/B/A value counts.
static inline void J3DSwapTRK1PC(J3DAnmTevRegKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->mCRegUpdateMaterialNum, 10);
	J3DSwap32PC(&d->mCRegTableOffset, 14);
	J3DAnmBlockPC b = { (u8*)d, size };
	b.swapRecs<J3DAnmCRegKeyTable>(d->mCRegTableOffset, d->mCRegUpdateMaterialNum, 12);
	b.swapRecs<J3DAnmKRegKeyTable>(d->mKRegTableOffset, d->mKRegUpdateMaterialNum, 12);
	b.swap16(d->mCRegUpdateMaterialIDOffset, d->mCRegUpdateMaterialNum);
	b.swap16(d->mKRegUpdateMaterialIDOffset, d->mKRegUpdateMaterialNum);
	b.swap16(d->mCRValuesOffset, d->field_0x10);
	b.swap16(d->mCGValuesOffset, d->field_0x12);
	b.swap16(d->mCBValuesOffset, d->field_0x14);
	b.swap16(d->mCAValuesOffset, d->field_0x16);
	b.swap16(d->mKRValuesOffset, d->field_0x18);
	b.swap16(d->mKGValuesOffset, d->field_0x1a);
	b.swap16(d->mKBValuesOffset, d->field_0x1c);
	b.swap16(d->mKAValuesOffset, d->field_0x1e);
}

// TPT1 (BTP): 0x0C material count, 0x0E texture index count; one 8-byte table per material (u8 texture number and the
// padding byte stay, the trailing u16 is swapped like its declaration).
static inline void J3DSwapTPT1PC(J3DAnmTexPatternFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 2);
	J3DSwap32PC(&d->mTableOffset, 4);
	J3DAnmBlockPC b = { (u8*)d, size };
	J3DAnmTexPatternFullTable* t = b.at<J3DAnmTexPatternFullTable>(d->mTableOffset, d->field_0xc);
	for (u32 i = 0; t && i < d->field_0xc; ++i) {
		J3DSwap16PC(&t[i], 2);
		J3DSwap16PC(&t[i]._6, 1);
	}
	b.swap16(d->mValuesOffset, d->field_0xe);
	b.swap16(d->mUpdateMaterialIDOffset, d->field_0xc);
}

// VAF1 (BVA), unverified: 0x0C table count, u8 visibility values.
static inline void J3DSwapVAF1PC(J3DAnmVisibilityFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 2);
	J3DSwap32PC(&d->mTableOffset, 2);
	J3DAnmBlockPC b = { (u8*)d, size };
	b.swapRecs<J3DAnmVisibilityFullTable>(d->mTableOffset, d->field_0xc, 2);
}

// CLK1 (BLK), unverified: 0x0C table count; f32 weights, extent from the tables.
static inline void J3DSwapCLK1PC(J3DAnmClusterKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 2);
	J3DSwap32PC(&d->mTableOffset, 2);
	J3DAnmBlockPC b = { (u8*)d, size };
	u32 w = 0;
	J3DAnmClusterKeyTable* t = b.swapRecs<J3DAnmClusterKeyTable>(d->mTableOffset, d->field_0xc, 3);
	for (u32 i = 0; t && i < d->field_0xc; ++i)
		J3DAnmMaxPC(w, J3DAnmKeyEndPC(t[i].mWeightTable));
	b.swap32(d->mWeightOffset, w);
}

// CLF1 (BLA), unverified: like CLK1 with full tables.
static inline void J3DSwapCLF1PC(J3DAnmClusterFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(&d->field_0xc, 2);
	J3DSwap32PC(&d->mTableOffset, 2);
	J3DAnmBlockPC b = { (u8*)d, size };
	u32 w = 0;
	J3DAnmClusterFullTable* t = b.swapRecs<J3DAnmClusterFullTable>(d->mTableOffset, d->field_0xc, 2);
	for (u32 i = 0; t && i < d->field_0xc; ++i)
		J3DAnmMaxPC(w, J3DAnmFullEndPC(t[i].mMaxFrame, t[i].mOffset));
	b.swap32(d->mWeightOffset, w);
}

// VCK1/VCF1, per channel: the 8-byte index records and the u16 index array they point into.
static inline void J3DSwapVtxColorIndexPC(const J3DAnmBlockPC& b, u32 recOff, u32 idxOff, u32 num)
{
	u32 end = 0;
	J3DAnmVtxColorIndexDisc* r = b.at<J3DAnmVtxColorIndexDisc>(recOff, num);
	for (u32 i = 0; r && i < num; ++i) {
		J3DSwap16PC(&r[i].mNum, 1);
		J3DSwap32PC(&r[i].mOffset, 1);
		J3DAnmMaxPC(end, r[i].mOffset + (u32)r[i].mNum);
	}
	b.swap16(idxOff, end);
}

// VCK1 (BXK), unverified: two channels of key tables; s16 R/G/B/A values shared by both, extents from the tables.
// The two words at 0x10 are unread by the loader; they are swapped as the declared u32.
static inline void J3DSwapVCK1PC(J3DAnmVtxColorKeyData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(d->mAnmTableNum, 2);
	J3DSwap32PC(d->mIndexNum, 12); // mIndexNum[2], then the 10 offsets up to 0x3C
	J3DAnmBlockPC b = { (u8*)d, size };
	u32 m[4] = { 0, 0, 0, 0 };
	for (int c = 0; c < 2; ++c) {
		u32 n = d->mAnmTableNum[c];
		J3DAnmColorKeyTable* t = b.swapRecs<J3DAnmColorKeyTable>(d->mTableOffsets[c], n, 12);
		for (u32 i = 0; t && i < n; ++i)
			for (int k = 0; k < 4; ++k)
				J3DAnmMaxPC(m[k], J3DAnmKeyEndPC(t[i].mColorInfo[k]));
		J3DSwapVtxColorIndexPC(b, d->mVtxColorIndexDataOffsets[c], d->mVtxColorIndexPointerOffsets[c], n);
	}
	b.swap16(d->mRValuesOffset, m[0]);
	b.swap16(d->mGValuesOffset, m[1]);
	b.swap16(d->mBValuesOffset, m[2]);
	b.swap16(d->mAValuesOffset, m[3]);
}

// VCF1 (BXA), unverified: two channels of full tables, u8 values. The words at 0x10..0x17 are unread by the loader;
// they are swapped as declared (u16[2], u32).
static inline void J3DSwapVCF1PC(J3DAnmVtxColorFullData* d, u32 size)
{
	if (size < sizeof(*d))
		return;
	J3DSwap16PC(&d->mFrameMax, 1);
	J3DSwap16PC(d->mAnmTableNum, 4);
	J3DSwap32PC(&d->field_0x14, 11);
	J3DAnmBlockPC b = { (u8*)d, size };
	for (int c = 0; c < 2; ++c) {
		u32 n = d->mAnmTableNum[c];
		b.swapRecs<J3DAnmColorFullTable>(d->mTableOffsets[c], n, 8);
		J3DSwapVtxColorIndexPC(b, d->mVtxColorIndexDataOffsets[c], d->mVtxColorIndexPointerOffsets[c], n);
	}
}

static inline void J3DSwapAnmBlockPC(JUTDataBlockHeader* h, u32 size)
{
	switch (h->mType) {
	case 'ANK1': J3DSwapANK1PC((J3DAnmTransformKeyData*)h, size); break;
	case 'ANF1': J3DSwapANF1PC((J3DAnmTransformFullData*)h, size); break;
	case 'PAK1': J3DSwapPAK1PC((J3DAnmColorKeyData*)h, size); break;
	case 'PAF1': J3DSwapPAF1PC((J3DAnmColorFullData*)h, size); break;
	case 'TTK1': J3DSwapTTK1PC((J3DAnmTextureSRTKeyData*)h, size); break;
	case 'TRK1': J3DSwapTRK1PC((J3DAnmTevRegKeyData*)h, size); break;
	case 'TPT1': J3DSwapTPT1PC((J3DAnmTexPatternFullData*)h, size); break;
	case 'VAF1': J3DSwapVAF1PC((J3DAnmVisibilityFullData*)h, size); break;
	case 'CLK1': J3DSwapCLK1PC((J3DAnmClusterKeyData*)h, size); break;
	case 'CLF1': J3DSwapCLF1PC((J3DAnmClusterFullData*)h, size); break;
	case 'VCK1': J3DSwapVCK1PC((J3DAnmVtxColorKeyData*)h, size); break;
	case 'VCF1': J3DSwapVCF1PC((J3DAnmVtxColorFullData*)h, size); break;
	default: break;
	}
}

// Converts a whole animation file in place; returns true if this call converted it (see the top of this file).
static inline bool J3DSwapAnmFilePC(void* file)
{
	if (!J3DSwapFileHeaderPC(file))
		return false;
	JUTDataFileHeader* header = (JUTDataFileHeader*)file;
	u32 off = sizeof(JUTDataFileHeader) - sizeof(JUTDataBlockHeader);
	for (u32 i = 0; i < header->mBlockNum; ++i) { // already clamped to the blocks inside the file
		JUTDataBlockHeader* block = (JUTDataBlockHeader*)((u8*)file + off);
		u32 avail                 = header->mFileSize - off;
		J3DSwapAnmBlockPC(block, block->mSize < avail ? block->mSize : avail);
		off += block->mSize;
	}
	return true;
}

#endif // TARGET_PC

#endif // J3D_ANM_SWAP_PC_HPP
