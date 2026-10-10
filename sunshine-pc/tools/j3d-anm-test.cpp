// Host test of the J3D animation file swap (sms/include/JSystem/J3D/J3DGraphLoader/J3DAnmSwapPC.hpp).
//   tools\j3d-test.cmd tools\j3d-anm-test.cpp build\j3d-corpus      (cwd = sunshine-pc, LLVM_BIN set)
// Real corpus (tools/j3d-corpus.py): every .bck/.btk/.brk/.btp/.bpk goes through J3DSwapAnmFilePC, the entry the loader
// uses. Per file the test (1) derives from the RAW big-endian bytes which words must be swapped (header fields, tables,
// value arrays), asserting the layout invariants on the way (regions inside the block, aligned, no overlap, padding
// between regions < 0x20 so no array extent is missing, header counts == highest key table reference); (2) checks the
// buffer equals exactly that conversion (every other byte unchanged); (3) checks idempotency; (4) checks invariants on
// the host-order structs (key tables index inside their arrays, key times monotonic, floats finite, ids, name table).
// The formats without a sample on disc (ANF1, PAF1, VAF1, CLK1, CLF1, VCK1, VCF1, the TTK1 post set) are checked on
// small synthetic files only: that proves the code does what its layout notes say, not that the notes match real files.
#include <JSystem/J3D/J3DGraphLoader/J3DAnmSwapPC.hpp>

#include <algorithm>
#include <filesystem>
#include <map>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

typedef std::vector<u8> Bytes;

static std::string g_name;

#define CHECK(c)                                                                                                       \
	do {                                                                                                           \
		if (!(c)) {                                                                                            \
			printf("FAIL %s:%d: %s [%s]\n", __FILE__, __LINE__, #c, g_name.c_str());                       \
			exit(1);                                                                                       \
		}                                                                                                      \
	} while (0)

static u32 g_tables, g_keys;

static const u32 B = 0x20; // block start

// ---- layout plan built from the raw file ------------------------------------------------------------------------

static u32 rd16(const Bytes& o, u32 off) { return rdbe16(&o[off]); }
static u32 rd32(const Bytes& o, u32 off) { return rdbe32(&o[off]); }

static u32 keyEnd(u32 mf, u32 off, u32 type) { return mf == 0 ? 0 : mf == 1 ? off + 1 : off + mf * (type == 0 ? 3 : 4); }

struct Plan {
	u32 hdr = 0, end = 0;    // end of the block header struct, end of the block (absolute)
	std::vector<u8> own;     // per byte: 0 free, 1 swapped, 2 raw (claimed, untouched)
	std::vector<u8> width;   // swap group width at the first byte of each swapped group
	std::vector<u32> lo, hi; // body regions; hi == ~0u marks an open-ended name table

	explicit Plan(const Bytes& o) : own(o.size()), width(o.size())
	{
		// 6 shipped .btk end 4 bytes before the end their block size claims: the data is complete, the padding is cut
		u32 blk = rd32(o, B + 4);
		CHECK(B + blk <= o.size() + 4);
		end = std::min<u32>(B + blk, (u32)o.size());
		for (u32 off : { 0u, 4u, 8u, 0xCu, 0x1Cu, 0x20u, 0x24u }) { // file header words, block type and size
			for (u32 k = off; k < off + 4; ++k)
				own[k] = 1;
			width[off] = 4;
		}
	}

	void claim(u32 off, u32 bytes, u8 kind, u32 w)
	{
		if (bytes == 0)
			return;
		CHECK(off % (w ? w : 1) == 0);
		CHECK(off >= hdr && (u64)off + bytes <= end);
		for (u32 k = off; k < off + bytes; ++k) {
			CHECK(own[k] == 0);
			own[k] = kind;
		}
		for (u32 k = off; w && k < off + bytes; k += w)
			width[k] = w;
		lo.push_back(off);
		hi.push_back(off + bytes);
	}
	void swp(u32 off, u32 w, u32 n) { claim(off, w * n, 1, w); }
	void raw(u32 off, u32 bytes) { claim(off, bytes, 2, 0); }
	void name(u32 off)
	{
		claim(off, 4, 2, 0);
		hi.back() = ~0u;
	}
	// swapped words inside the block header struct [B, hdr)
	void hw(u32 off, u32 w, u32 n)
	{
		CHECK(off % w == 0 && off + w * n <= hdr);
		for (u32 k = off; k < off + w * n; ++k) {
			CHECK(own[k] == 0);
			own[k] = 1;
		}
		for (u32 k = off; k < off + w * n; k += w)
			width[k] = w;
	}

	// The padding between consecutive body regions (and to the block end) is below 0x20: an array whose extent was
	// derived too short would leave a hole of at least that size.
	void finish()
	{
		std::vector<std::pair<u32, u32>> v;
		for (size_t i = 0; i < lo.size(); ++i)
			v.push_back({ lo[i], hi[i] });
		std::sort(v.begin(), v.end());
		u32 prev  = hdr;
		bool open = false;
		for (auto& r : v) {
			if (!open)
				CHECK(r.first - prev < 0x20);
			open = r.second == ~0u;
			prev = r.second;
		}
		if (!open)
			CHECK(end - prev < 0x20);
	}

	Bytes convert(const Bytes& o) const
	{
		Bytes e = o;
		for (size_t k = 0; k < o.size(); ++k)
			if (width[k])
				std::reverse(e.begin() + k, e.begin() + k + width[k]);
		return e;
	}
};

// Highest array element each key table column references.
struct Max4 {
	u32 m[4] = { 0, 0, 0, 0 };
	void add(int k, const Bytes& o, u32 at) { m[k] = std::max(m[k], keyEnd(rd16(o, at), rd16(o, at + 2), rd16(o, at + 4))); }
};

static void planBCK(Plan& p, const Bytes& o)
{
	u32 jc = rd16(o, B + 0xC), sc = rd16(o, B + 0xE), rc = rd16(o, B + 0x10), tc = rd16(o, B + 0x12);
	u32 tab = B + rd32(o, B + 0x14), so = B + rd32(o, B + 0x18), ro = B + rd32(o, B + 0x1C), to = B + rd32(o, B + 0x20);
	p.hdr = B + 0x24;
	p.hw(B + 0x0A, 2, 1);
	p.hw(B + 0x0C, 2, 4);
	p.hw(B + 0x14, 4, 4);
	p.swp(tab, 2, jc * 27); // 3 tables per joint, 9 u16 each
	p.swp(so, 4, sc);
	p.swp(ro, 2, rc);
	p.swp(to, 4, tc);
	Max4 m;
	for (u32 i = 0; i < jc * 3; ++i)
		for (int k = 0; k < 3; ++k)
			m.add(k, o, tab + i * 0x12 + k * 6);
	CHECK(m.m[0] == sc && m.m[1] == rc && m.m[2] == tc);
}

static void planBTK(Plan& p, const Bytes& o)
{
	p.hdr = B + 0x60;
	p.hw(B + 0x0A, 2, 1);
	p.hw(B + 0x0C, 2, 4);
	p.hw(B + 0x14, 4, 8);
	p.hw(B + 0x34, 2, 4);
	p.hw(B + 0x3C, 4, 8); // 0x5C is padding: stays raw
	for (int set = 0; set < 2; ++set) { // pre-SRT set (counts 0x0C, offsets 0x14) and post-SRT set (0x34, 0x3C)
		u32 c = B + (set ? 0x34 : 0x0C), f = B + (set ? 0x3C : 0x14);
		u32 tn = rd16(o, c), ns = rd16(o, c + 2), nr = rd16(o, c + 4), nt = rd16(o, c + 6);
		u32 tab = rd32(o, f), ids = rd32(o, f + 4), nm = rd32(o, f + 8), txm = rd32(o, f + 12);
		u32 ctr = rd32(o, f + 16), so = rd32(o, f + 20), ro = rd32(o, f + 24), to = rd32(o, f + 28);
		CHECK(tn % 3 == 0);
		p.swp(B + tab, 2, tn * 9);
		p.swp(B + ids, 2, tn / 3);
		p.raw(B + txm, tn / 3);
		p.swp(B + ctr, 4, tn / 3 * 3);
		p.swp(B + so, 4, ns);
		p.swp(B + ro, 2, nr);
		p.swp(B + to, 4, nt);
		if (nm)
			p.name(B + nm);
		Max4 m;
		for (u32 i = 0; i < tn; ++i)
			for (int k = 0; k < 3; ++k)
				m.add(k, o, B + tab + i * 0x12 + k * 6);
		CHECK(m.m[0] == ns && m.m[1] == nr && m.m[2] == nt);
	}
}

static void planBRK(Plan& p, const Bytes& o)
{
	p.hdr = B + 0x58;
	p.hw(B + 0x0A, 2, 1);
	p.hw(B + 0x0C, 2, 10);
	p.hw(B + 0x20, 4, 14);
	u32 n[2] = { rd16(o, B + 0xC), rd16(o, B + 0xE) };
	for (int s = 0; s < 2; ++s) { // 0: C registers, 1: K registers
		u32 tab = B + rd32(o, B + 0x20 + s * 4), ids = rd32(o, B + 0x28 + s * 4), nm = rd32(o, B + 0x30 + s * 4);
		for (u32 i = 0; i < n[s]; ++i) {
			p.swp(tab + i * 0x1C, 2, 12);
			p.raw(tab + i * 0x1C + 0x18, 4); // colour id + padding
		}
		p.swp(B + ids, 2, n[s]);
		if (nm)
			p.name(B + nm);
		Max4 m;
		for (u32 i = 0; i < n[s]; ++i)
			for (int k = 0; k < 4; ++k)
				m.add(k, o, tab + i * 0x1C + k * 6);
		for (int k = 0; k < 4; ++k) {
			u32 cnt = rd16(o, B + 0x10 + s * 8 + k * 2);
			p.swp(B + rd32(o, B + 0x38 + s * 16 + k * 4), 2, cnt);
			CHECK(m.m[k] == cnt);
		}
	}
}

static void planBPK(Plan& p, const Bytes& o)
{
	p.hdr = B + 0x34;
	p.hw(B + 0x0C, 2, 1);
	p.hw(B + 0x0E, 2, 5);
	p.hw(B + 0x18, 4, 7);
	u32 n = rd16(o, B + 0xE), tab = B + rd32(o, B + 0x18), ids = rd32(o, B + 0x1C), nm = rd32(o, B + 0x20);
	p.swp(tab, 2, n * 12);
	p.swp(B + ids, 2, n);
	if (nm)
		p.name(B + nm);
	Max4 m;
	for (u32 i = 0; i < n; ++i)
		for (int k = 0; k < 4; ++k)
			m.add(k, o, tab + i * 0x18 + k * 6);
	for (int k = 0; k < 4; ++k) {
		u32 cnt = rd16(o, B + 0x10 + k * 2);
		p.swp(B + rd32(o, B + 0x24 + k * 4), 2, cnt);
		CHECK(m.m[k] == cnt);
	}
}

static void planBTP(Plan& p, const Bytes& o)
{
	p.hdr = B + 0x20;
	p.hw(B + 0x0A, 2, 1);
	p.hw(B + 0x0C, 2, 2);
	p.hw(B + 0x10, 4, 4);
	u32 n = rd16(o, B + 0xC), vn = rd16(o, B + 0xE);
	u32 tab = B + rd32(o, B + 0x10), vals = rd32(o, B + 0x14), ids = rd32(o, B + 0x18), nm = rd32(o, B + 0x1C);
	u32 mx = 0;
	for (u32 i = 0; i < n; ++i) {
		p.swp(tab + i * 8, 2, 2);
		p.raw(tab + i * 8 + 4, 2); // u8 texture number + padding byte
		p.swp(tab + i * 8 + 6, 2, 1);
		mx = std::max(mx, rd16(o, tab + i * 8) ? rd16(o, tab + i * 8) + rd16(o, tab + i * 8 + 2) : 0);
	}
	p.swp(B + vals, 2, vn);
	p.swp(B + ids, 2, n);
	if (nm)
		p.name(B + nm);
	CHECK(mx == vn);
}

// ---- invariants on the host-order structs ------------------------------------------------------------------------

template <typename T> static bool fin(T v) { return isfinite((double)v); } // BCK translations reach 1.6e7 on disc

template <typename T> static void keys(const J3DAnmKeyTableBase& t, const T* arr, u32 count)
{
	CHECK(t.mType <= 1);
	CHECK(J3DAnmKeyEndPC(t) <= count);
	++g_tables;
	if (t.mMaxFrame == 0)
		return;
	if (t.mMaxFrame == 1) {
		CHECK(fin(arr[t.mOffset]));
		return;
	}
	u32 stride = t.mType == 0 ? 3 : 4;
	for (u32 i = 0; i < t.mMaxFrame; ++i) {
		const T* k = arr + t.mOffset + i * stride;
		for (u32 j = 0; j < stride; ++j)
			CHECK(fin(k[j]));
		if (i)
			CHECK(k[0] >= k[-(int)stride]); // key times never decrease
		++g_keys;
	}
}

static void allFinite(const f32* a, u32 n)
{
	for (u32 i = 0; i < n; ++i)
		CHECK(fin(a[i]));
}

static void hostHeader(u8* f, u32 len, u32 type, u32 blk)
{
	auto* h = (JUTDataFileHeader*)f;
	CHECK(h->mMagic == 'J3D1' && h->mType == type && h->mBlockNum == 1);
	CHECK(h->mFileSize == len && h->mFirstBlock.mType == blk);
	CHECK(h->mFirstBlock.mSize % 4 == 0 && B + h->mFirstBlock.mSize <= len + 4); // see Plan: 6 .btk lack 4 padding bytes
}

// ids are model material indices (0xFFFF only after searchUpdateMaterialID); the name table starts with its count.
static void idsAndNames(u8* b, u32 size, u32 idsOff, u32 nameOff, u32 n)
{
	CHECK(nameOff != 0 && nameOff + 4 <= size && rdbe16(b + nameOff) == n); // name table is nested: still big-endian
	const u16* ids = (const u16*)(b + idsOff);
	for (u32 i = 0; i < n; ++i)
		CHECK(ids[i] < 0x100);
}

static void hostBCK(u8* f, u32 len)
{
	hostHeader(f, len, 'bck1', 'ANK1');
	auto* d = (J3DAnmTransformKeyData*)(f + B);
	u8* b   = (u8*)d;
	CHECK(d->mFrameMax > 0 && d->mAttribute <= 4 && d->mDecShift <= 15 && d->field_0xc > 0);
	auto* tab = (J3DAnmTransformKeyTable*)(b + d->mTableOffset);
	auto* sc  = (const f32*)(b + d->mScaleOffset);
	auto* ro  = (const s16*)(b + d->mRotOffset);
	auto* tr  = (const f32*)(b + d->mTransOffset);
	for (u32 i = 0; i < d->field_0xc * 3u; ++i) {
		keys(tab[i].mScale, sc, d->field_0xe);
		keys(tab[i].mRotation, ro, d->field_0x10);
		keys(tab[i].mTranslate, tr, d->field_0x12);
	}
	allFinite(sc, d->field_0xe);
	allFinite(tr, d->field_0x12);
}

static void hostBTK(u8* f, u32 len)
{
	hostHeader(f, len, 'btk1', 'TTK1');
	auto* d = (J3DAnmTextureSRTKeyData*)(f + B);
	u8* b   = (u8*)d;
	u32 tn  = d->field_0xc;
	CHECK(d->field_0xa > 0 && d->field_0x8 <= 4 && tn > 0 && tn % 3 == 0);
	auto* tab = (J3DAnmTransformKeyTable*)(b + d->mTableOffset);
	auto* sc  = (const f32*)(b + d->mScaleValOffset);
	auto* ro  = (const s16*)(b + d->mRotValOffset);
	auto* tr  = (const f32*)(b + d->mTransValOffset);
	for (u32 i = 0; i < tn; ++i) {
		keys(tab[i].mScale, sc, d->field_0xe);
		keys(tab[i].mRotation, ro, d->field_0x10);
		keys(tab[i].mTranslate, tr, d->field_0x12);
	}
	allFinite(sc, d->field_0xe);
	allFinite(tr, d->field_0x12);
	allFinite((const f32*)(b + d->unkOffset), tn / 3 * 3);
	idsAndNames(b, d->mHeader.mSize, d->mUpdateMatIDOffset, d->mNameTab1Offset, tn / 3);
	for (u32 i = 0; i < tn / 3; ++i)
		CHECK(b[d->mUpdateTexMtxIDOffset + i] < 10);
	// post-SRT set: empty on every shipped file; the word at 0x5C is padding (stale memory) and not looked at
	CHECK(d->field_0x34 == 0 && d->field_0x36 == 0 && d->field_0x38 == 0 && d->field_0x3a == 0);
	CHECK(d->mInfoTable2Offset == 0 && d->field_0x40 == 0 && d->mNameTab2Offset == 0 && d->field_0x48 == 0);
	CHECK(d->field_0x4c == 0 && d->field_0x50 == 0 && d->field_0x54 == 0 && d->field_0x58 == 0);
}

static void hostBRK(u8* f, u32 len)
{
	hostHeader(f, len, 'brk1', 'TRK1');
	auto* d = (J3DAnmTevRegKeyData*)(f + B);
	u8* b   = (u8*)d;
	CHECK(d->mFrameMax > 0 && d->field_0x8 <= 4);
	const s32 voff[2][4] = { { d->mCRValuesOffset, d->mCGValuesOffset, d->mCBValuesOffset, d->mCAValuesOffset },
		                 { d->mKRValuesOffset, d->mKGValuesOffset, d->mKBValuesOffset, d->mKAValuesOffset } };
	const u16 cnt[2][4]  = { { d->field_0x10, d->field_0x12, d->field_0x14, d->field_0x16 },
		                 { d->field_0x18, d->field_0x1a, d->field_0x1c, d->field_0x1e } };
	const u32 n[2]       = { d->mCRegUpdateMaterialNum, d->mKRegUpdateMaterialNum };
	for (int s = 0; s < 2; ++s) {
		auto* t = (const J3DAnmCRegKeyTable*)(b + (s ? d->mKRegTableOffset : d->mCRegTableOffset));
		for (u32 i = 0; i < n[s]; ++i) {
			const J3DAnmKeyTableBase* col[4] = { &t[i].mRTable, &t[i].mGTable, &t[i].mBTable, &t[i].mATable };
			for (int k = 0; k < 4; ++k)
				keys(*col[k], (const s16*)(b + voff[s][k]), cnt[s][k]);
			CHECK(t[i].mColorId <= 3);
		}
		idsAndNames(b, d->mHeader.mSize, s ? d->mKRegUpdateMaterialIDOffset : d->mCRegUpdateMaterialIDOffset,
		            s ? d->mKRegNameTabOffset : d->mCRegNameTabOffset, n[s]);
	}
}

static void hostBPK(u8* f, u32 len)
{
	hostHeader(f, len, 'bpk1', 'PAK1');
	auto* d = (J3DAnmColorKeyData*)(f + B);
	u8* b   = (u8*)d;
	CHECK(d->mFrameMax > 0 && d->field_0x8 <= 4);
	const s32 voff[4] = { d->mRValOffset, d->mGValOffset, d->mBValOffset, d->mAValOffset };
	const u16 cnt[4]  = { d->field_0x10, d->field_0x12, d->field_0x14, d->field_0x16 };
	auto* t           = (const J3DAnmColorKeyTable*)(b + d->mTableOffset);
	for (u32 i = 0; i < d->mUpdateMaterialNum; ++i)
		for (int k = 0; k < 4; ++k)
			keys(t[i].mColorInfo[k], (const s16*)(b + voff[k]), cnt[k]);
	idsAndNames(b, d->mHeader.mSize, d->mUpdateMaterialIDOffset, d->mNameTabOffset, d->mUpdateMaterialNum);
}

static void hostBTP(u8* f, u32 len)
{
	hostHeader(f, len, 'btp1', 'TPT1');
	auto* d = (J3DAnmTexPatternFullData*)(f + B);
	u8* b   = (u8*)d;
	u32 n   = d->field_0xc;
	CHECK(d->mFrameMax > 0 && d->field_0x8 <= 4);
	auto* t    = (const J3DAnmTexPatternFullTable*)(b + d->mTableOffset);
	auto* vals = (const u16*)(b + d->mValuesOffset);
	for (u32 i = 0; i < n; ++i) {
		CHECK(t[i].mMaxFrame > 0 && J3DAnmFullEndPC(t[i].mMaxFrame, t[i].mOffset) <= d->field_0xe);
		++g_tables;
	}
	for (u32 i = 0; i < d->field_0xe; ++i)
		CHECK(vals[i] < 0x1000); // texture indices
	idsAndNames(b, d->mHeader.mSize, d->mUpdateMaterialIDOffset, d->mNameTabOffset, n);
}

// ---- real corpus ----------------------------------------------------------------------------------------------------

static void checkFile(const std::string& ext, const Bytes& raw)
{
	CHECK(rdbe32(&raw[0]) == 'J3D1');
	Plan p(raw);
	if (ext == "bck")
		planBCK(p, raw);
	else if (ext == "btk")
		planBTK(p, raw);
	else if (ext == "brk")
		planBRK(p, raw);
	else if (ext == "bpk")
		planBPK(p, raw);
	else
		planBTP(p, raw);
	p.finish();

	Bytes buf = raw;
	CHECK(J3DSwapAnmFilePC(buf.data()));
	CHECK(buf == p.convert(raw)); // exactly the planned words swapped, every other byte untouched
	Bytes after = buf;
	CHECK(!J3DSwapAnmFilePC(buf.data())); // idempotent: second call refuses and leaves the bytes alone
	CHECK(buf == after);

	u32 len = (u32)buf.size();
	if (ext == "bck")
		hostBCK(buf.data(), len);
	else if (ext == "btk")
		hostBTK(buf.data(), len);
	else if (ext == "brk")
		hostBRK(buf.data(), len);
	else if (ext == "bpk")
		hostBPK(buf.data(), len);
	else
		hostBTP(buf.data(), len);
}

// ---- synthetic files for the formats without a sample on disc ------------------------------------------------------

struct W {
	Bytes b;
	struct E {
		u32 off, width, value;
	};
	std::vector<E> exp; // host value each slot must hold after the swap (width 1 = untouched byte)

	void put(u32 w, u32 v)
	{
		exp.push_back({ (u32)b.size(), w, v });
		for (int i = (int)w - 1; i >= 0; --i)
			b.push_back((u8)(v >> (8 * i)));
	}
	void b8(u8 v) { put(1, v); }
	void f(f32 v)
	{
		u32 u;
		memcpy(&u, &v, 4);
		put(4, u);
	}
	void at(u32 rel) // pad with zeros up to a block-relative offset
	{
		CHECK(b.size() <= B + rel);
		b.resize(B + rel, 0);
	}
	void sentinel() // bytes right behind an array: they must survive an array swap that ends too late
	{
		for (u8 v : { 0xA1, 0xA2, 0xA3, 0xA4 })
			b8(v);
	}
	void begin(u32 type, u32 blk)
	{
		put(4, 'J3D1');
		put(4, type);
		put(4, 0); // file size, patched
		put(4, 1);
		b.resize(0x1C, 0);
		put(4, 0);
		put(4, blk);
		put(4, 0); // block size, patched
	}
	void u16s(std::initializer_list<u16> l)
	{
		for (u16 v : l)
			put(2, v);
	}
	void patch(u32 off, u32 v)
	{
		for (E& e : exp)
			if (e.off == off)
				e.value = v;
		for (int i = 0; i < 4; ++i)
			b[off + i] = (u8)(v >> (8 * (3 - i)));
	}
	void end()
	{
		b.resize((b.size() + 0x1F) & ~0x1Fu, 0);
		patch(8, (u32)b.size());
		patch(0x24, (u32)b.size() - B);
	}
};

static void runSyn(const char* name, W& w)
{
	g_name = name;
	w.end();
	Bytes buf = w.b;
	CHECK(J3DSwapAnmFilePC(buf.data()));
	for (const W::E& e : w.exp) {
		u32 v = 0;
		memcpy(&v, &buf[e.off], e.width); // little endian host
		CHECK(v == e.value);
	}
	Bytes after = buf;
	CHECK(!J3DSwapAnmFilePC(buf.data()) && buf == after);
}

static void synthetic()
{
	{ // ANF1: 2 joints; extents 3/5/4 come from the tables only
		W w;
		w.begin('bca1', 'ANF1');
		w.b8(2); w.b8(1); w.u16s({ 100, 2, 3, 5, 4 });
		w.put(4, 0x40); w.put(4, 0xA0); w.put(4, 0xC0); w.put(4, 0xE0);
		w.at(0x40);
		w.u16s({ 3, 0, 2, 0, 2, 0,  0, 0, 3, 2, 2, 2,  1, 2, 0, 0, 0, 0 });
		w.u16s({ 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0 });
		w.at(0xA0); w.f(1.5f); w.f(-2.25f); w.f(3.0f); w.sentinel();
		w.at(0xC0); w.u16s({ 0x1234, 0xFEDC, 7, 0x8001, 0x7FFF }); w.sentinel();
		w.at(0xE0); w.f(10.0f); w.f(11.5f); w.f(-12.0f); w.f(13.0f); w.sentinel();
		runSyn("synthetic ANF1", w);
	}
	{ // PAF1: 2 materials, u8 values untouched
		W w;
		w.begin('bpa1', 'PAF1');
		w.b8(0); w.b8(0xFF); w.b8(0xFF); w.b8(0xFF); w.u16s({ 60, 2, 4, 4, 4, 4 });
		w.put(4, 0x40); w.put(4, 0x60); w.put(4, 0x68); w.put(4, 0x70); w.put(4, 0x78); w.put(4, 0x80); w.put(4, 0x88);
		w.at(0x40); w.u16s({ 4, 0, 4, 0, 4, 0, 4, 0,  4, 0, 4, 0, 4, 0, 4, 0 });
		w.at(0x60); w.u16s({ 3, 4 });
		w.at(0x68); for (u8 v : { 0, 2, 0xFF, 0xFF, 0x55, 0x66, 0x77, 0x88 }) w.b8(v);
		w.at(0x70); for (u8 v = 0x10; v < 0x30; ++v) w.b8(v);
		runSyn("synthetic PAF1", w);
	}
	{ // VAF1: 3 tables, u8 values untouched
		W w;
		w.begin('bva1', 'VAF1');
		w.b8(0); w.b8(0xFF); w.u16s({ 30, 3, 6 });
		w.put(4, 0x18); w.put(4, 0x28);
		w.at(0x18); w.u16s({ 2, 0, 2, 2, 2, 4 });
		w.at(0x28); for (u8 v : { 1, 0, 1, 1, 0, 1 }) w.b8(v);
		w.sentinel();
		runSyn("synthetic VAF1", w);
	}
	{ // CLK1: 2 cluster tables: 2 keys of 3 + a lone value = 7 weights
		W w;
		w.begin('blk1', 'CLK1');
		w.b8(0); w.b8(0xFF); w.u16s({ 30, 2, 7 });
		w.put(4, 0x18); w.put(4, 0x40);
		w.at(0x18); w.u16s({ 2, 0, 0, 1, 6, 0 });
		w.at(0x40); w.f(0.0f); w.f(1.0f); w.f(0.5f); w.f(5.0f); w.f(0.0f); w.f(0.25f); w.f(0.75f); w.sentinel();
		runSyn("synthetic CLK1", w);
	}
	{ // CLF1: 2 cluster tables: 4 + 3 frames = 7 weights
		W w;
		w.begin('bla1', 'CLF1');
		w.b8(0); w.b8(0xFF); w.u16s({ 30, 2, 7 });
		w.put(4, 0x18); w.put(4, 0x40);
		w.at(0x18); w.u16s({ 4, 0, 3, 4 });
		w.at(0x40); for (int i = 0; i < 7; ++i) w.f(0.5f * (float)i - 1.0f);
		w.sentinel();
		runSyn("synthetic CLF1", w);
	}
	{ // VCK1: ch0 2 tables, ch1 1 table; index records {3 at 0, 2 at 3} / {4 at 0}; R/G/B/A extents 15/10/7/9
		W w;
		w.begin('bxk1', 'VCK1');
		w.b8(0); w.b8(0xFF); w.u16s({ 30, 2, 1 });
		w.put(4, 7); w.put(4, 9);                            // mIndexNum (unread, swapped as u32)
		w.put(4, 0x40); w.put(4, 0xA0);                      // tables
		w.put(4, 0xD0); w.put(4, 0xE0);                      // index records
		w.put(4, 0xF0); w.put(4, 0x100);                     // index arrays
		w.put(4, 0x110); w.put(4, 0x150); w.put(4, 0x190); w.put(4, 0x1D0); // R G B A
		w.at(0x40);
		w.u16s({ 2, 0, 0,  1, 3, 0,  0, 0, 0,  2, 0, 1 });  // ch0 t0: R G B A
		w.u16s({ 1, 6, 0,  2, 4, 0,  1, 0, 0,  0, 0, 0 });  // ch0 t1
		w.at(0xA0);
		w.u16s({ 2, 7, 1,  1, 0, 0,  2, 1, 0,  1, 8, 0 });  // ch1 t0
		w.at(0xD0); w.u16s({ 3 }); w.b8(0x5A); w.b8(0x5B); w.put(4, 0); w.u16s({ 2 }); w.b8(0x5C); w.b8(0x5D); w.put(4, 3);
		w.at(0xE0); w.u16s({ 4 }); w.b8(0x5E); w.b8(0x5F); w.put(4, 0);
		w.at(0xF0); w.u16s({ 10, 11, 12, 13, 14 }); w.sentinel();
		w.at(0x100); w.u16s({ 20, 21, 22, 23 }); w.sentinel();
		for (int c = 0; c < 4; ++c) {
			static const int n[4] = { 15, 10, 7, 9 };
			w.at(0x110 + c * 0x40);
			for (int i = 0; i < n[c]; ++i)
				w.u16s({ (u16)(0x8000 + c * 0x100 + i) });
			w.sentinel();
		}
		runSyn("synthetic VCK1", w);
	}
	{ // VCF1: same shape with full tables
		W w;
		w.begin('bxa1', 'VCF1');
		w.b8(0); w.b8(0xFF); w.u16s({ 30, 2, 1, 5, 4 });
		w.put(4, 0x1234);                                    // 0x14, unread
		w.put(4, 0x40); w.put(4, 0x80);
		w.put(4, 0xA0); w.put(4, 0xB0);
		w.put(4, 0xC0); w.put(4, 0xD0);
		w.put(4, 0xE0); w.put(4, 0xF0); w.put(4, 0x100); w.put(4, 0x110);
		w.at(0x40); w.u16s({ 4, 0, 4, 0, 4, 0, 4, 0 }); w.u16s({ 4, 4, 4, 4, 4, 4, 4, 4 });
		w.at(0x80); w.u16s({ 4, 8, 4, 8, 4, 8, 4, 8 });
		w.at(0xA0); w.u16s({ 3 }); w.b8(0x5A); w.b8(0x5B); w.put(4, 0); w.u16s({ 2 }); w.b8(0x5C); w.b8(0x5D); w.put(4, 3);
		w.at(0xB0); w.u16s({ 4 }); w.b8(0x5E); w.b8(0x5F); w.put(4, 0);
		w.at(0xC0); w.u16s({ 10, 11, 12, 13, 14 }); w.sentinel();
		w.at(0xD0); w.u16s({ 20, 21, 22, 23 }); w.sentinel();
		for (int c = 0; c < 4; ++c) {
			w.at(0xE0 + c * 0x10);
			for (int i = 0; i < 8; ++i)
				w.b8((u8)(0x20 * c + i));
		}
		runSyn("synthetic VCF1", w);
	}
	{ // TTK1 with a post set: pre 1 material, post 1 material
		W w;
		w.begin('btk1', 'TTK1');
		w.b8(2); w.b8(1); w.u16s({ 100, 3, 2, 2, 2 });
		w.put(4, 0x60); w.put(4, 0x98); w.put(4, 0xA0); w.put(4, 0xA8);
		w.put(4, 0xB0); w.put(4, 0xC0); w.put(4, 0xD0); w.put(4, 0xE0);
		w.u16s({ 3, 1, 1, 1 });
		w.put(4, 0xF0); w.put(4, 0x128); w.put(4, 0x130); w.put(4, 0x138);
		w.put(4, 0x140); w.put(4, 0x150); w.put(4, 0x158); w.put(4, 0x160);
		for (u8 v : { 0x80, 0x15, 0x14, 0xA8 }) w.b8(v);     // 0x5C: padding, untouched
		w.at(0x60);
		for (int i = 0; i < 9; ++i) w.u16s({ 1, 0, 0 }); // pre: 3 tables of scale/rotation/translation lone values
		w.at(0x98); w.u16s({ 5 });
		w.at(0xA0); for (u8 v : { 0, 1, 0xFF, 0xFF, 7, 8, 9, 10 }) w.b8(v);
		w.at(0xA8); w.b8(3);
		w.at(0xB0); w.f(0.5f); w.f(0.25f); w.f(0.0f); w.sentinel();
		w.at(0xC0); w.f(1.0f); w.f(2.0f); w.sentinel();
		w.at(0xD0); w.u16s({ 100, 200 }); w.sentinel();
		w.at(0xE0); w.f(-1.0f); w.f(-2.0f); w.sentinel();
		w.at(0xF0);
		for (int i = 0; i < 9; ++i) w.u16s({ 1, 0, 0 }); // post
		w.at(0x128); w.u16s({ 6 });
		w.at(0x130); for (u8 v : { 0, 1, 0xFF, 0xFF, 17, 18, 19, 20 }) w.b8(v);
		w.at(0x138); w.b8(4);
		w.at(0x140); w.f(0.125f); w.f(0.375f); w.f(0.625f); w.sentinel();
		w.at(0x150); w.f(9.0f); w.sentinel();
		w.at(0x158); w.u16s({ 0xBEEF }); w.sentinel();
		w.at(0x160); w.f(8.0f); w.sentinel();
		runSyn("synthetic TTK1 post", w);
	}
}

int main(int argc, char** argv)
{
	synthetic();
	std::string dir = argc > 1 ? argv[1] : "build/j3d-corpus";
	std::map<std::string, std::pair<u32, u32>> count; // ext -> files, files checked
	std::vector<std::filesystem::path> paths;
	for (auto& e : std::filesystem::directory_iterator(dir))
		paths.push_back(e.path());
	std::sort(paths.begin(), paths.end());
	u32 checked = 0;
	for (auto& path : paths) {
		std::string ext = path.extension().string();
		ext.erase(0, 1);
		if (ext != "bck" && ext != "btk" && ext != "brk" && ext != "btp" && ext != "bpk") {
			++count[ext].first;
			continue;
		}
		auto& c = count[ext];
		++c.first;
		FILE* fp = fopen(path.string().c_str(), "rb");
		CHECK(fp);
		Bytes raw;
		u8 tmp[65536];
		for (size_t n; (n = fread(tmp, 1, sizeof(tmp), fp)) != 0;)
			raw.insert(raw.end(), tmp, tmp + n);
		fclose(fp);
		g_name = path.filename().string();
		if (raw.size() < 0x20) // junk file in the corpus
			continue;
		checkFile(ext, raw);
		++c.second;
		++checked;
	}
	for (auto& kv : count)
		printf(".%s: %u files, %u checked\n", kv.first.c_str(), kv.second.first, kv.second.second);
	printf("synthetic: 8 files (ANF1 PAF1 VAF1 CLK1 CLF1 VCK1 VCF1 TTK1-post)\n");
	printf("OK: %u corpus files checked, %u key tables, %u keys\n", checked, g_tables, g_keys);
	return 0;
}
