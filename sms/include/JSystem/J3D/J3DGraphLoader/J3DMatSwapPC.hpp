#ifndef J3D_MAT_SWAP_PC_HPP
#define J3D_MAT_SWAP_PC_HPP

// Host byte order conversion of the J3D material blocks MAT3 (J3DMaterialBlock) and MAT2 (J3DMaterialBlock_v21).
// TARGET_PC only; the console build never includes this API.
//
// J3DSwapMat3BlockPC / J3DSwapMat2BlockPC run once per buffer, right after J3DSwapFileHeaderPC() returned true (that
// call is the idempotency marker, see J3DSwapPC.hpp). `block` points at the JUTDataBlockHeader whose {mType, mSize} are
// already host order. They convert in place: the block header (mMaterialNum and the table offsets), the material ID
// table, the init records, and every table the J3DMaterialFactory indexes whose elements hold multi-byte fields.
// Tables of u8-only elements (colors, color channel info, tex coords, tev order/stage/swap, alpha/blend/z mode and
// the like) are byte data and stay untouched. The name table is a nested format (JUTNameTab) and swaps itself.
//
// Element counts: the init records and the ID table give the counts, not the offset deltas. A table has
// (highest index referenced by any init record) + 1 elements; this is exact even when an empty table shares its offset
// with the next one or a table is followed by the ASCII alignment padding ("This is padding data to align..."), where
// the offset delta would swap bytes of a neighbour or of the padding. Elements past the highest referenced index are
// never read by the factory and stay raw. The real data has such unreferenced tails only in the cull mode table (the
// exporter always writes all three modes), see tools/j3d-mat-test.cpp which cross-checks the counts against the
// offset deltas for the whole corpus. Counts are clamped so no table reaches past the block.

#ifdef TARGET_PC

#include <stddef.h>
#include <string.h>
#include <JSystem/J3D/J3DGraphLoader/J3DSwapPC.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DMaterialFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DMaterialFactory_v21.hpp>

// The factories overlay these structs on the disc bytes: the host size must equal the disc record size.
static_assert(sizeof(J3DMaterialBlock) == 0x84 && sizeof(J3DMaterialBlock_v21) == 0x78, "MAT3/MAT2 header");
static_assert(sizeof(J3DMaterialInitData) == 0x14C && sizeof(J3DMaterialInitData_v21) == 0x138, "init record");
static_assert(sizeof(J3DIndInitData) == 0x138 && sizeof(J3DIndTexMtxInfo) == 0x1C, "indirect record");
static_assert(sizeof(J3DLightInfo) == 0x34 && sizeof(J3DTexMtxInfo) == 0x64, "light/texmtx");
static_assert(sizeof(J3DFogInfo) == 0x2C && sizeof(J3DNBTScaleInfo) == 0x10, "fog/nbt");
static_assert(sizeof(GXCullMode) == 4 && sizeof(GXColorS10) == 8, "cull/tev color");

// One swap-relevant table of the block.
struct J3DMatTblPC {
	u8 hdr;            // byte offset of the table offset in the block header
	u16 ref;           // byte offset in an init record of the first index that references the table
	u8 num;            // number of consecutive index slots
	u8 idx;            // index width in bytes: 1 (unused = 0xFF) or 2 (unused = 0xFFFF)
	u8 size;           // disc element size
	void (*swap)(u8*); // converts one element
};

static inline void J3DSwapMatU16PC(u8* e) { swapInPlace16(e); }
static inline void J3DSwapMatU32PC(u8* e) { swapInPlace32(e); }
static inline void J3DSwapMatColorS10PC(u8* e) { J3DSwap16PC(e, 4); } // GXColorS10

static inline void J3DSwapMatLightPC(u8* e) // position, direction | GXColor | cos/dist attenuation
{
	J3DLightInfo* l = (J3DLightInfo*)e;
	J3DSwap32PC(&l->mLightPosition, 6);
	J3DSwap32PC(&l->mCosAtten, 6);
}

static inline void J3DSwapMatTexMtxPC(u8* e) // u8 projection/info | center | scale, s16 rotation, translation | mtx
{
	J3DTexMtxInfo* m = (J3DTexMtxInfo*)e;
	J3DSwap32PC(&m->mCenter, 3);
	J3DSwap32PC(&m->mSRT.mScaleX, 2);
	swapInPlace16(&m->mSRT.mRotation);
	J3DSwap32PC(&m->mSRT.mTranslationX, 2);
	J3DSwap32PC(m->mEffectMtx, 16);
}

static inline void J3DSwapMatFogPC(u8* e) // u8 type/adjust | s16 center | 4 floats | GXColor | 10 u16 adjust table
{
	J3DFogInfo* f = (J3DFogInfo*)e;
	swapInPlace16(&f->mCenter);
	J3DSwap32PC(&f->mStartZ, 4);
	J3DSwap16PC(f->mFogAdjTable, 10);
}

static inline void J3DSwapMatNBTScalePC(u8* e) { J3DSwap32PC(&((J3DNBTScaleInfo*)e)->mScale, 3); }

// Elements that fit completely into the block when `count` elements of `elem` bytes start at `off` (0 = absent).
static inline u32 J3DMatFitPC(u32 size, u32 off, u32 count, u32 elem)
{
	u32 fit = off != 0 && off < size ? (size - off) / elem : 0;
	return count < fit ? count : fit;
}

// Common part of MAT3 and MAT2. The block header is converted; `recSize` is the init record size and `selOff` the
// offset of its two u8 arrays (tev k color / k alpha selectors, 16 + 16 bytes), everything else in a record is u16.
static inline void J3DSwapMatTablesPC(u8* blk, u32 matNum, u32 recSize, u32 selOff, const J3DMatTblPC* tbl, u32 tblNum)
{
	u32 size    = ((JUTDataBlockHeader*)blk)->mSize;
	u32 initOff = ((u32*)blk)[3];
	u32 idOff   = ((u32*)blk)[4];

	// Material IDs; the init record count is the highest ID + 1.
	matNum   = J3DMatFitPC(size, idOff, matNum, 2);
	u16* ids = (u16*)(blk + idOff);
	J3DSwap16PC(ids, matNum);
	u32 nInit = 0;
	for (u32 i = 0; i < matNum; ++i)
		if (ids[i] >= nInit)
			nInit = ids[i] + 1;
	nInit    = J3DMatFitPC(size, initOff, nInit, recSize);
	u8* init = blk + initOff;
	for (u32 i = 0; i < nInit; ++i) {
		J3DSwap16PC(init + i * recSize + 8, (selOff - 8) / 2);
		J3DSwap16PC(init + i * recSize + selOff + 32, (recSize - selOff - 32) / 2);
	}

	for (const J3DMatTblPC* t = tbl; t != tbl + tblNum; ++t) {
		u32 off  = *(u32*)(blk + t->hdr);
		u32 none = t->idx == 1 ? 0xFF : 0xFFFF;
		u32 count = 0; // highest referenced index + 1
		for (u32 i = 0; i < nInit; ++i) {
			for (u32 k = 0; k < t->num; ++k) {
				const u8* p = init + i * recSize + t->ref + k * t->idx;
				u32 v       = t->idx == 1 ? *p : *(const u16*)p;
				if (v != none && v >= count)
					count = v + 1;
			}
		}
		count = J3DMatFitPC(size, off, count, t->size);
		for (u32 i = 0; i < count; ++i)
			t->swap(blk + off + i * t->size);
	}
}

static inline void J3DSwapMat3BlockPC(void* block)
{
	// header offset, init record index slots (offset, count, width), element size, element swapper
	static const J3DMatTblPC tbl[] = {
		{ 0x1C, 0x001, 1, 1, 4, J3DSwapMatU32PC },          // cull mode (GXCullMode, 4 bytes)
		{ 0x30, 0x018, 8, 2, 0x34, J3DSwapMatLightPC },     // light info
		{ 0x40, 0x048, 10, 2, 0x64, J3DSwapMatTexMtxPC },   // tex mtx
		{ 0x44, 0x05C, 20, 2, 0x64, J3DSwapMatTexMtxPC },   // tex mtx 2
		{ 0x48, 0x084, 8, 2, 2, J3DSwapMatU16PC },          // tex no
		{ 0x50, 0x0DC, 4, 2, 8, J3DSwapMatColorS10PC },     // tev color
		{ 0x68, 0x144, 1, 2, 0x2C, J3DSwapMatFogPC },       // fog
		{ 0x80, 0x14A, 1, 2, 0x10, J3DSwapMatNBTScalePC },  // nbt scale
	};
	J3DMaterialBlock* b = (J3DMaterialBlock*)block;
	swapInPlace16(&b->mMaterialNum);
	J3DSwap32PC(&b->mpMaterialInitData, (sizeof(*b) - offsetof(J3DMaterialBlock, mpMaterialInitData)) / 4);
	J3DSwapMatTablesPC((u8*)b, b->mMaterialNum, sizeof(J3DMaterialInitData), 0x9C, tbl, sizeof(tbl) / sizeof(tbl[0]));

	// Indirect init data: one record per material (indexed by material, not by init record), present under the
	// same condition as in J3DMaterialFactory. Only the offset matrices hold multi-byte fields.
	if (b->mpIndInitData != 0 && (u32)(b->mpIndInitData - b->mpNameTable) > 4) {
		u32 n = J3DMatFitPC(b->mSize, b->mpIndInitData, b->mMaterialNum, sizeof(J3DIndInitData));
		J3DIndInitData* ind = (J3DIndInitData*)((u8*)b + b->mpIndInitData);
		for (u32 i = 0; i < n; ++i)
			for (u32 m = 0; m < 3; ++m)
				J3DSwap32PC(ind[i].mIndTexMtxInfo[m].mOffsetMtx, 6);
	}
}

// MAT2 (older block version): UNVERIFIED against real data, no MAT2 block exists in the shipped files. Same as MAT3
// without the indirect, ambient color and light tables; see the layout notes in J3DMaterialFactory_v21.hpp.
static inline void J3DSwapMat2BlockPC(void* block)
{
	static const J3DMatTblPC tbl[] = {
		{ 0x18, 0x001, 1, 1, 4, J3DSwapMatU32PC },
		{ 0x34, 0x034, 10, 2, 0x64, J3DSwapMatTexMtxPC },
		{ 0x38, 0x048, 20, 2, 0x64, J3DSwapMatTexMtxPC },
		{ 0x3C, 0x070, 8, 2, 2, J3DSwapMatU16PC },
		{ 0x44, 0x0C8, 4, 2, 8, J3DSwapMatColorS10PC },
		{ 0x5C, 0x130, 1, 2, 0x2C, J3DSwapMatFogPC },
		{ 0x74, 0x136, 1, 2, 0x10, J3DSwapMatNBTScalePC },
	};
	J3DMaterialBlock_v21* b = (J3DMaterialBlock_v21*)block;
	swapInPlace16(&b->mMaterialNum);
	J3DSwap32PC(&b->mpMaterialInitData, (sizeof(*b) - offsetof(J3DMaterialBlock_v21, mpMaterialInitData)) / 4);
	J3DSwapMatTablesPC((u8*)b, b->mMaterialNum, sizeof(J3DMaterialInitData_v21), 0x88, tbl, sizeof(tbl) / sizeof(tbl[0]));
}

#endif // TARGET_PC

#endif // J3D_MAT_SWAP_PC_HPP
