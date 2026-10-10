#ifndef J3D_MODEL_SWAP_PC_HPP
#define J3D_MODEL_SWAP_PC_HPP

// Host byte order conversion of the J3D model blocks INF1 VTX1 EVP1 DRW1 JNT1 SHP1 TEX1 (BMD/BMT), TARGET_PC only.
// Called block by block from J3DModelLoader.cpp after J3DSwapFileHeaderPC() converted the file once (see
// J3DSwapPC.hpp for the idempotency marker). Every function takes the address of the block, whose {fourcc, size}
// header is already host order, and converts the rest in place. Layout is the on-disc layout; every offset is relative
// to the block start. The 32-bit offset members of the block structs are `u32` on the host (see J3DModelLoader.hpp),
// so the structs keep their disc size.
//
// Table extents: every table is converted for exactly the entries the loaders can index, derived from the data
// itself: the table indexing it (u16 index table -> init records, init records -> sub tables) or, for arrays that
// carry no count, the distance to the next table offset. A table is never converted twice (ranges are single
// contiguous [0, max) spans), and a count that would run past the block is clamped to the block.
// Not converted on purpose: display lists (SHP1 mpDisplayListData: GX command streams that Aurora's FIFO parser reads
// big-endian), BTI image/palette payloads, name tables (JUTNameTab swaps its ResNTAB itself), u8 tables.
// Vertex arrays (VTX1) ARE converted: the game code skins/mirrors them on the CPU as host floats, and the arrays are
// registered with Aurora as little-endian (see J3DShape::loadVtxArray).

#ifdef TARGET_PC

#include <JSystem/J3D/J3DGraphLoader/J3DSwapPC.hpp>
#include <JSystem/ResTIMG.hpp>

static inline u16 J3DGet16PC(const void* p)
{
	u16 v;
	memcpy(&v, p, sizeof(v));
	return v;
}

static inline u32 J3DGet32PC(const void* p)
{
	u32 v;
	memcpy(&v, p, sizeof(v));
	return v;
}

// Whole `elem`-byte entries of a table at `off` that fit in a block of `size` bytes, at most `want`.
static inline u32 J3DTableCountPC(u32 size, u32 off, u32 want, u32 elem)
{
	if (off >= size)
		return 0;
	u32 fit = (size - off) / elem;
	return want < fit ? want : fit;
}

// INF1: {u16 flags; u32 packetNum; u32 vtxNum; u32 hierarchyOffset}, hierarchy = {u16 type, u16 value} entries up to
// and including the type 0 terminator.
static inline void J3DSwapInf1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	J3DSwap32PC(b + 0x0C, 3);
	u32 off = J3DGet32PC(b + 0x14);
	if (off == 0)
		return;
	for (u32 p = off; p + 4 <= size; p += 4) {
		J3DSwap16PC(b + p, 2);
		if (J3DGet16PC(b + p) == 0)
			break;
	}
}

// VTX1 header: 14 u32 offsets at 0x08: attribute format list, position, normal, NBT, color0/1, texcoord0..7.
static inline u32 J3DVtxArrayOffsetPC(const u8* b, u32 slot) { return J3DGet32PC(b + 0x0C + slot * 4); }

// Byte span of the array at `off`: up to the next table offset of the header, or the block end.
// The span includes the alignment padding after the last element.
static inline u32 J3DVtxArraySpanPC(const u8* b, u32 off)
{
	u32 end = J3DGet32PC(b + 4);
	for (u32 i = 0; i < 14; i++) {
		u32 o = J3DGet32PC(b + 0x08 + i * 4);
		if (o > off && o < end)
			end = o;
	}
	return end - off;
}

// Width in bytes of one array component (the unit that is byte-swapped). `attr` is the GXAttr (9 pos, 10 nrm,
// 11/12 color, 13..20 texcoord), `type` the GXCompType; colors use their own enum (RGB565 0, RGB8 1, RGBX8 2,
// RGBA4 3, RGBA6 4, RGBA8 5): only the 16-bit formats need swapping, RGBA6 (24-bit packed) does not occur in J3D.
static inline u32 J3DVtxCompSizePC(u32 attr, u32 type)
{
	if (attr == 11 || attr == 12)
		return (type == 0 || type == 3) ? 2 : 1;
	switch (type) {
	case 2: // GX_U16
	case 3: // GX_S16
		return 2;
	case 4: // GX_F32
		return 4;
	default: // GX_U8, GX_S8
		return 1;
	}
}

static inline void J3DSwapVtx1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	J3DSwap32PC(b + 0x08, 14);

	// {u32 attr, u32 cnt, u32 type, u8 frac, pad[3]} until attr 0xFF (GX_VA_NULL); no entry means "not used": the
	// default of J3DModelLoader's getFmtType (F32) applies to arrays without one.
	u32 types[21];
	for (u32 i = 0; i < 21; i++)
		types[i] = i >= 11 && i <= 12 ? 5 : 4;
	u32 fmt = J3DGet32PC(b + 0x08);
	if (fmt != 0) {
		for (u32 p = fmt; p + 16 <= size; p += 16) {
			J3DSwap32PC(b + p, 3);
			u32 attr = J3DGet32PC(b + p);
			if (attr == 0xFF)
				break;
			if (attr < 21)
				types[attr] = J3DGet32PC(b + p + 8);
		}
	}

	// slot 0 position, 1 normal, 2 NBT (same format as the normal), 3/4 colors, 5..12 texcoords
	static const u8 attrOfSlot[13] = { 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
	for (u32 slot = 0; slot < 13; slot++) {
		u32 off = J3DVtxArrayOffsetPC(b, slot);
		if (off == 0 || off >= size)
			continue;
		bool seen = false;
		for (u32 j = 0; j < slot; j++)
			seen = seen || J3DVtxArrayOffsetPC(b, j) == off;
		if (seen)
			continue;
		u32 attr = attrOfSlot[slot];
		u32 comp = J3DVtxCompSizePC(attr, types[attr]);
		u32 n    = J3DVtxArraySpanPC(b, off) / comp;
		if (comp == 4)
			J3DSwap32PC(b + off, n);
		else if (comp == 2)
			J3DSwap16PC(b + off, n);
	}
}

// EVP1: {u16 envelopeNum; u32 mixMtxNum (u8 per envelope), mixMtxIndex (u16 per weight), mixWeight (f32 per weight),
// invJointMtx (3x4 f32 per joint)}. The weight tables hold sum(mixMtxNum) entries; the inverse bind matrices carry no
// count (indexed by joint), so every whole matrix up to the block end is converted.
static inline void J3DSwapEvp1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	u32 num = J3DGet16PC(b + 0x08);
	J3DSwap32PC(b + 0x0C, 4);
	u32 numOff = J3DGet32PC(b + 0x0C);
	u32 idxOff = J3DGet32PC(b + 0x10);
	u32 wgtOff = J3DGet32PC(b + 0x14);
	u32 invOff = J3DGet32PC(b + 0x18);

	u32 total = 0;
	if (numOff != 0)
		for (u32 i = 0, n = J3DTableCountPC(size, numOff, num, 1); i < n; i++)
			total += b[numOff + i];
	if (idxOff != 0)
		J3DSwap16PC(b + idxOff, J3DTableCountPC(size, idxOff, total, 2));
	if (wgtOff != 0)
		J3DSwap32PC(b + wgtOff, J3DTableCountPC(size, wgtOff, total, 4));
	if (invOff != 0)
		J3DSwap32PC(b + invOff, J3DTableCountPC(size, invOff, 0xFFFFFFFF, 48) * 12);
}

// DRW1: {u16 matrixNum; u32 flagTable (u8 per entry), indexTable (u16 per entry)}.
static inline void J3DSwapDrw1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	u32 num = J3DGet16PC(b + 0x08);
	J3DSwap32PC(b + 0x0C, 2);
	u32 idxOff = J3DGet32PC(b + 0x10);
	if (idxOff != 0)
		J3DSwap16PC(b + idxOff, J3DTableCountPC(size, idxOff, num, 2));
}

// JNT1: {u16 jointNum; u32 initData, indexTable (u16 per joint), nameTable}. Joint init record (0x40 bytes, indexed
// through the index table): u16 kind, u8 scaleCompensate, pad, scale f32[3], rotation s16[3], pad s16,
// translation f32[3], radius f32, min f32[3], max f32[3]. The record count is max(index) + 1.
static inline void J3DSwapJnt1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	u32 num = J3DGet16PC(b + 0x08);
	J3DSwap32PC(b + 0x0C, 3);
	u32 initOff = J3DGet32PC(b + 0x0C);
	u32 idxOff  = J3DGet32PC(b + 0x10);
	if (idxOff == 0 || initOff == 0)
		return;

	u32 n = J3DTableCountPC(size, idxOff, num, 2);
	J3DSwap16PC(b + idxOff, n);
	u32 records = 0;
	for (u32 i = 0; i < n; i++) {
		u32 idx = J3DGet16PC(b + idxOff + i * 2) + 1;
		records = idx > records ? idx : records;
	}
	records = J3DTableCountPC(size, initOff, records, 0x40);
	for (u32 i = 0; i < records; i++) {
		u8* r = b + initOff + i * 0x40;
		swapInPlace16(r + 0x00);
		J3DSwap32PC(r + 0x04, 3);
		J3DSwap16PC(r + 0x10, 3);
		J3DSwap32PC(r + 0x18, 10);
	}
}

// SHP1: {u16 shapeNum; u32 shapeInitData, indexTable (u16 per shape), nameTable, vtxDescList, mtxTable (u16),
// displayListData (bytes, NOT converted), mtxInitData, drawInitData}. Records:
//   shape init (0x28, via the index table): u8 mtxType, pad, u16 mtxGroupNum, u16 vtxDescListIndex (byte offset into
//     the desc list), u16 mtxInitDataIndex, u16 drawInitDataIndex, pad, f32 radius, f32 min[3], f32 max[3]
//   mtx init (8): u16 useMtxIndex, u16 useMtxCount, u32 firstUseMtxIndex   (mtxGroupNum records per shape)
//   draw init (8): u32 displayListSize, u32 displayListOffset               (mtxGroupNum records per shape)
//   vtx desc (8): u32 attr, u32 type, each list ends with attr 0xFF
// Counts come from the references above, in dependency order (each referenced table is a single [0, max) span).
static inline void J3DSwapShp1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	u32 num = J3DGet16PC(b + 0x08);
	J3DSwap32PC(b + 0x0C, 8);
	u32 initOff    = J3DGet32PC(b + 0x0C);
	u32 idxOff     = J3DGet32PC(b + 0x10);
	u32 descOff    = J3DGet32PC(b + 0x18);
	u32 mtxTabOff  = J3DGet32PC(b + 0x1C);
	u32 mtxInitOff = J3DGet32PC(b + 0x24);
	u32 drawOff    = J3DGet32PC(b + 0x28);
	if (idxOff == 0 || initOff == 0)
		return;

	u32 n = J3DTableCountPC(size, idxOff, num, 2);
	J3DSwap16PC(b + idxOff, n);
	u32 records = 0;
	for (u32 i = 0; i < n; i++) {
		u32 idx = J3DGet16PC(b + idxOff + i * 2) + 1;
		records = idx > records ? idx : records;
	}
	records = J3DTableCountPC(size, initOff, records, 0x28);

	u32 mtxInitNum = 0, drawInitNum = 0, descEnd = 0;
	for (u32 i = 0; i < records; i++) {
		u8* r = b + initOff + i * 0x28;
		J3DSwap16PC(r + 0x02, 4);
		J3DSwap32PC(r + 0x0C, 7);
		u32 groups = J3DGet16PC(r + 0x02);
		u32 mtxEnd = J3DGet16PC(r + 0x06) + groups;
		u32 drwEnd = J3DGet16PC(r + 0x08) + groups;
		mtxInitNum = mtxEnd > mtxInitNum ? mtxEnd : mtxInitNum;
		drawInitNum = drwEnd > drawInitNum ? drwEnd : drawInitNum;
		descEnd = J3DGet16PC(r + 0x04) > descEnd ? J3DGet16PC(r + 0x04) : descEnd; // start of the last list
	}

	u32 mtxNum = 0;
	if (mtxInitOff != 0) {
		mtxInitNum = J3DTableCountPC(size, mtxInitOff, mtxInitNum, 8);
		for (u32 i = 0; i < mtxInitNum; i++) {
			u8* r = b + mtxInitOff + i * 8;
			J3DSwap16PC(r, 2);
			J3DSwap32PC(r + 4, 1);
			u32 end = J3DGet32PC(r + 4) + J3DGet16PC(r + 2);
			mtxNum  = end > mtxNum ? end : mtxNum;
		}
	}
	if (mtxTabOff != 0)
		J3DSwap16PC(b + mtxTabOff, J3DTableCountPC(size, mtxTabOff, mtxNum, 2));
	if (drawOff != 0) {
		drawInitNum = J3DTableCountPC(size, drawOff, drawInitNum, 8);
		J3DSwap32PC(b + drawOff, drawInitNum * 2);
	}
	if (descOff != 0) {
		// swap every list up to and including the terminator of the last referenced one
		u32 p = descOff;
		while (p + 8 <= size) {
			J3DSwap32PC(b + p, 2);
			bool last = J3DGet32PC(b + p) == 0xFF;
			p += 8;
			if (last && p - descOff > descEnd)
				break;
		}
	}
}

// TEX1: {u16 textureNum; u32 headerTable (ResTIMG, 0x20 bytes each), nameTable}. Image and palette data stay bytes.
static inline void J3DSwapTex1PC(u8* b)
{
	u32 size = J3DGet32PC(b + 4);
	swapInPlace16(b + 0x08);
	u32 num = J3DGet16PC(b + 0x08);
	J3DSwap32PC(b + 0x0C, 2);
	u32 off = J3DGet32PC(b + 0x0C);
	if (off == 0)
		return;
	for (u32 i = 0, n = J3DTableCountPC(size, off, num, sizeof(ResTIMG)); i < n; i++)
		swapResTIMG((ResTIMG*)(b + off) + i);
}

#endif // TARGET_PC

#endif // J3D_MODEL_SWAP_PC_HPP
