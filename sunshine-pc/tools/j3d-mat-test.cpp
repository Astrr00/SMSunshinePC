// Host check of the MAT3/MAT2 block conversion (J3DMatSwapPC.hpp) and of the J3DMaterialFactory readers on the result.
// Usage: tools\j3d-test.cmd tools\j3d-mat-test.cpp [corpus dir, default build\j3d-corpus]   (run from sunshine-pc)
// Corpus: build\j3d-corpus\<md5>.{bmd,bmt} from tools\j3d-corpus.py (raw big-endian files). Per file it
//  1. calls J3DSwapFileHeaderPC (true once, false on the second call), then J3DSwapMat3BlockPC on the MAT3 block,
//  2. derives every table's element count two ways (highest index referenced by an init record, and the offset delta
//     to the next table minus the ASCII alignment padding) and requires that they agree, with the documented exceptions,
//  3. rebuilds the expected block from the raw bytes with an independent oracle (struct member offsets, per-field
//     swaps, byte coverage: no byte may be swapped twice) and requires byte equality with the production result,
//  4. checks value ranges on the host-order block (finite floats, enum ranges, texture numbers < TEX1 count),
//  5. constructs J3DMaterialFactory over the swapped block and compares what its readers return with direct big-endian
//     reads of the raw bytes, and does the same for a MAT2 block synthesized from the MAT3 data (UNVERIFIED layout, see
//     J3DMaterialFactory_v21.hpp) through J3DSwapMat2BlockPC and J3DMaterialFactory_v21.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

// The test reads the factories' table pointers directly.
#define private public
#include <JSystem/J3D/J3DGraphLoader/J3DMatSwapPC.hpp>
#include "../../sms/src/JSystem/J3D/J3DGraphLoader/J3DMaterialFactory.cpp"
#include "../../sms/src/JSystem/J3D/J3DGraphLoader/J3DMaterialFactory_v21.cpp"
#undef private

// Definitions the factories reference but only create()/default branches use; never executed here.
J3DColorBlock* J3DMaterial::createColorBlock(int) { return nullptr; }
J3DTexGenBlock* J3DMaterial::createTexGenBlock(int) { return nullptr; }
J3DTevBlock* J3DMaterial::createTevBlock(int) { return nullptr; }
J3DIndBlock* J3DMaterial::createIndBlock(int) { return nullptr; }
J3DPEBlock* J3DMaterial::createPEBlock(int, u32) { return nullptr; }
void J3DMaterial::initialize() { }
const J3DBlendInfo j3dDefaultBlendInfo                         = {};
const J3DColorChanInfo j3dDefaultColorChanInfo                 = {};
const J3DIndTevStageInfo j3dDefaultIndTevStageInfo             = {};
const J3DIndTexCoordScaleInfo j3dDefaultIndTexCoordScaleInfo   = {};
const J3DIndTexMtxInfo j3dDefaultIndTexMtxInfo                 = {};
const J3DIndTexOrderInfo j3dDefaultIndTexOrderNull             = {};
const J3DNBTScaleInfo j3dDefaultNBTScaleInfo                   = {};
const J3DTevOrderInfo j3dDefaultTevOrderInfoNull               = {};
const J3DTevStageInfo j3dDefaultTevStageInfo                   = {};
const J3DTevSwapModeInfo j3dDefaultTevSwapMode                 = {};
const J3DTevSwapModeTableInfo j3dDefaultTevSwapModeTable       = {};
const J3DTexCoordInfo j3dDefaultTexCoordInfo[8]                = {};

// ---- helpers --------------------------------------------------------------------------------------------------

static int g_fail;
static std::string g_cur;
#define CHECK(c, ...) \
	do { \
		if (!(c)) { \
			if (g_fail++ < 40) { \
				std::printf("FAIL %s:%d [%s] ", "j3d-mat-test.cpp", __LINE__, g_cur.c_str()); \
				std::printf(__VA_ARGS__); \
				std::printf("\n"); \
			} \
		} \
	} while (0)

static u16 r16(const u8* p) { return (u16)(p[0] << 8 | p[1]); }
static u32 r32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u32 fbits(f32 f)
{
	u32 u;
	memcpy(&u, &f, 4);
	return u;
}
static bool fin(f32 f) { return std::isfinite(f) && std::fabs(f) < 1e8f; }
static void put16(std::vector<u8>& v, u32 at, u32 x)
{
	v[at]     = (u8)(x >> 8);
	v[at + 1] = (u8)x;
}
static void put32(std::vector<u8>& v, u32 at, u32 x)
{
	put16(v, at, x >> 16);
	put16(v, at + 2, x & 0xFFFF);
}

static const char kPad[] = "This is padding data to align..."; // 32 chars, cut to the padding length
static bool isPad(const u8* p, u32 n) { return n < 32 && memcmp(p, kPad, n) == 0; }

// ---- table description (all 26 MAT3 tables; kind = how the elements are converted) -----------------------------

enum Kind { RAW, U16, U32, S10, LIGHT, TEXMTX, FOG, NBT };
struct Tbl {
	const char* name;
	u32 hdr;        // header offset of the table offset
	u32 es;         // disc element size
	u32 ref, num, w; // init record: first index slot, slot count, index width in bytes
	Kind kind;
};
#define H(m) ((u32)offsetof(J3DMaterialBlock, m))
#define I(m) ((u32)offsetof(J3DMaterialInitData, m))
static const Tbl kTbl[] = {
	{ "cull", H(mpCullMode), 4, I(mCullModeIdx), 1, 1, U32 },
	{ "matcol", H(mpMatColor), 4, I(mMatColorIdx), 2, 2, RAW },
	{ "chnum", H(mpColorChanNum), 1, I(mColorChanNumIdx), 1, 1, RAW },
	{ "chinfo", H(mpColorChanInfo), 8, I(mColorChanIdx), 4, 2, RAW },
	{ "amb", H(mpAmbColor), 4, I(mAmbColorIdx), 2, 2, RAW },
	{ "light", H(mpLightInfo), 0x34, I(field_0x018), 8, 2, LIGHT },
	{ "tgnum", H(mpTexGenNum), 1, I(mTexGenNumIdx), 1, 1, RAW },
	{ "tc", H(mpTexCoordInfo), 4, I(mTexCoordIdx), 8, 2, RAW },
	{ "tc2", H(mpTexCoord2Info), 4, I(field_0x038), 8, 2, RAW },
	{ "tmtx", H(mpTexMtxInfo), 0x64, I(mTexMtxIdx), 10, 2, TEXMTX },
	{ "tmtx2", H(field_0x44), 0x64, I(field_0x058) + 4, 20, 2, TEXMTX },
	{ "texno", H(mpTexNo), 2, I(mTexNoIdx), 8, 2, U16 },
	{ "tevord", H(mpTevOrderInfo), 4, I(mTevOrderIdx), 16, 2, RAW },
	{ "tevcol", H(mpTevColor), 8, I(mTevColorIdx), 4, 2, S10 },
	{ "tevk", H(mpTevKColor), 4, I(mTevKColorIdx), 4, 2, RAW },
	{ "stnum", H(mpTevStageNum), 1, I(mTevStageNumIdx), 1, 1, RAW },
	{ "stinfo", H(mpTevStageInfo), 0x14, I(mTevStageIdx), 16, 2, RAW },
	{ "swap", H(mpTevSwapModeInfo), 4, I(mTevSwapModeIdx), 16, 2, RAW },
	{ "swaptab", H(mpTevSwapModeTableInfo), 4, I(mTevSwapModeTableIdx), 4, 2, RAW },
	{ "fog", H(mpFogInfo), 0x2C, I(mFogIdx), 1, 2, FOG },
	{ "alpha", H(mpAlphaCompInfo), 8, I(mAlphaCompIdx), 1, 2, RAW },
	{ "blend", H(mpBlendInfo), 4, I(mBlendIdx), 1, 2, RAW },
	{ "zmode", H(mpZModeInfo), 4, I(mZModeIdx), 1, 1, RAW },
	{ "zcomp", H(mpZCompLoc), 1, I(mZCompLocIdx), 1, 1, RAW },
	{ "dither", H(mpDither), 1, I(mDitherIdx), 1, 1, RAW },
	{ "nbt", H(mpNBTScaleInfo), 0x10, I(mNBTScaleIdx), 1, 2, NBT },
};
static const u32 kNumTbl = sizeof(kTbl) / sizeof(kTbl[0]);

// Disc sizes of the structs the factory overlays on the tables (host size must equal them), checked against kTbl.
static_assert(sizeof(J3DColorChanInfo) == 8 && sizeof(J3DTexCoordInfo) == 4 && sizeof(J3DTevOrderInfo) == 4);
static_assert(sizeof(J3DTevStageInfo) == 0x14 && sizeof(J3DTevSwapModeInfo) == 4 && sizeof(J3DTevSwapModeTableInfo) == 4);
static_assert(sizeof(J3DAlphaCompInfo) == 8 && sizeof(J3DBlendInfo) == 4 && sizeof(J3DZModeInfo) == 4);
static_assert(sizeof(GXColor) == 4 && sizeof(J3DIndTexOrderInfo) == 4 && sizeof(J3DIndTexCoordScaleInfo) == 4);
static_assert(sizeof(J3DIndTevStageInfo) == 12 && sizeof(J3DIndTexMtxInfo) == 0x1C && sizeof(J3DIndInitData) == 0x138);
static_assert(offsetof(J3DIndInitData, mIndTexMtxInfo) == 0x14 && offsetof(J3DIndInitData, mIndTevStageInfo) == 0x78);
static_assert(offsetof(J3DTexMtxInfo, mEffectMtx) == 0x24 && offsetof(J3DTexMtxInfo, mSRT) == 0x10);
static_assert(offsetof(J3DFogInfo, mFogAdjTable) == 0x18 && offsetof(J3DFogInfo, mColor) == 0x14);
static_assert(offsetof(J3DMaterialInitData, mTevKColorSel) == 0x9C && offsetof(J3DMaterialInitData, mTevOrderIdx) == 0xBC);
static_assert(offsetof(J3DMaterialInitData_v21, mTevKColorSel) == 0x88 && offsetof(J3DMaterialInitData_v21, mTevOrderIdx) == 0xA8);
static_assert(offsetof(J3DMaterialInitData_v21, mNBTScaleIdx) == 0x136 && offsetof(J3DMaterialBlock_v21, mpNBTScaleInfo) == 0x74);

// Independent oracle: converts a raw block field by field; cov counts how often a byte was converted.
struct Oracle {
	std::vector<u8> out, cov;
	void s16(u32 at)
	{
		std::swap(out[at], out[at + 1]);
		cov[at]++, cov[at + 1]++;
	}
	void s32(u32 at)
	{
		std::swap(out[at], out[at + 3]);
		std::swap(out[at + 1], out[at + 2]);
		for (u32 k = 0; k < 4; k++)
			cov[at + k]++;
	}
	void elem(u32 at, Kind k)
	{
		switch (k) {
		case U16: s16(at); break;
		case U32: s32(at); break;
		case S10:
			for (u32 o : { offsetof(GXColorS10, r), offsetof(GXColorS10, g), offsetof(GXColorS10, b), offsetof(GXColorS10, a) })
				s16(at + o);
			break;
		case LIGHT:
			for (u32 v : { offsetof(J3DLightInfo, mLightPosition), offsetof(J3DLightInfo, mLightDirection),
			               offsetof(J3DLightInfo, mCosAtten), offsetof(J3DLightInfo, mDistAtten) })
				for (u32 c = 0; c < 3; c++)
					s32(at + v + 4 * c);
			break;
		case TEXMTX:
			for (u32 c = 0; c < 3; c++)
				s32(at + offsetof(J3DTexMtxInfo, mCenter) + 4 * c);
			for (u32 o : { offsetof(J3DTextureSRTInfo, mScaleX), offsetof(J3DTextureSRTInfo, mScaleY),
			               offsetof(J3DTextureSRTInfo, mTranslationX), offsetof(J3DTextureSRTInfo, mTranslationY) })
				s32(at + offsetof(J3DTexMtxInfo, mSRT) + o);
			s16(at + offsetof(J3DTexMtxInfo, mSRT) + offsetof(J3DTextureSRTInfo, mRotation));
			for (u32 c = 0; c < 16; c++)
				s32(at + offsetof(J3DTexMtxInfo, mEffectMtx) + 4 * c);
			break;
		case FOG:
			s16(at + offsetof(J3DFogInfo, mCenter));
			for (u32 o : { offsetof(J3DFogInfo, mStartZ), offsetof(J3DFogInfo, mEndZ), offsetof(J3DFogInfo, mNearZ),
			               offsetof(J3DFogInfo, mFarZ) })
				s32(at + o);
			for (u32 c = 0; c < 10; c++)
				s16(at + offsetof(J3DFogInfo, mFogAdjTable) + 2 * c);
			break;
		case NBT:
			for (u32 c = 0; c < 3; c++)
				s32(at + offsetof(J3DNBTScaleInfo, mScale) + 4 * c);
			break;
		case RAW: break;
		}
	}
};

// ---- per-block derivation from the RAW (big-endian) bytes ---------------------------------------------------------

struct TblInfo {
	u32 off     = 0;
	u32 refCnt  = 0; // highest referenced index + 1
	s32 extCnt  = 0; // elements by offset delta (padding stripped), -1: tail is neither elements nor padding
	bool shared = false;
};
struct Raw {
	const u8* b;
	u32 size, n, initOff, idOff, nInit;
	std::vector<u16> ids;
	TblInfo t[kNumTbl];
	std::vector<u32> so; // sorted unique table offsets + block size
	const u8* rec(u32 i) const { return b + initOff + i * sizeof(J3DMaterialInitData) + 0; }
};

static void derive(Raw& r, const u8* b, u32 size)
{
	r.b       = b;
	r.size    = size;
	r.n       = r16(b + 8);
	r.initOff = r32(b + H(mpMaterialInitData));
	r.idOff   = r32(b + H(mpMaterialID));
	r.nInit   = 0;
	for (u32 i = 0; i < r.n; i++) {
		r.ids.push_back(r16(b + r.idOff + 2 * i));
		r.nInit = std::max<u32>(r.nInit, r.ids.back() + 1u);
	}
	for (u32 o = H(mpMaterialInitData); o <= H(mpNBTScaleInfo); o += 4)
		if (r32(b + o))
			r.so.push_back(r32(b + o));
	r.so.push_back(size);
	std::sort(r.so.begin(), r.so.end());
	r.so.erase(std::unique(r.so.begin(), r.so.end()), r.so.end());
	for (u32 ti = 0; ti < kNumTbl; ti++) {
		const Tbl& t = kTbl[ti];
		TblInfo& in  = r.t[ti];
		in.off       = r32(b + t.hdr);
		for (u32 i = 0; i < r.nInit; i++)
			for (u32 k = 0; k < t.num; k++) {
				const u8* p = r.rec(i) + t.ref + k * t.w;
				u32 v       = t.w == 1 ? *p : r16(p);
				if (v != (t.w == 1 ? 0xFFu : 0xFFFFu))
					in.refCnt = std::max(in.refCnt, v + 1);
			}
		if (in.off == 0)
			continue;
		for (u32 o2 = H(mpMaterialInitData); o2 <= H(mpNBTScaleInfo); o2 += 4)
			if (o2 != t.hdr && r32(b + o2) == in.off)
				in.shared = true;
		u32 next = *std::upper_bound(r.so.begin(), r.so.end(), in.off);
		u32 ext  = next - in.off;
		in.extCnt = -1;
		for (u32 k = in.refCnt; k * t.es <= ext; k++)
			if (isPad(b + in.off + k * t.es, ext - k * t.es)) {
				in.extCnt = k;
				break;
			}
	}
}

// ---- factory probe: the same observations from the factory and from the raw bytes -----------------------------------

typedef std::vector<s64> Obs;
static void addF(Obs& o, f32 f) { o.push_back(fbits(f)); }
static u32 rgba(const u8* p) { return r32(p); }
static u32 rgba(const GXColor& c) { return (u32)c.r << 24 | (u32)c.g << 16 | (u32)c.b << 8 | c.a; }

template <class F> static Obs probe(F& f, bool v26, u32 n)
{
	Obs o;
	o.push_back(f.countUniqueMaterials());
	if constexpr (std::is_same_v<F, J3DMaterialFactory>)
		o.push_back(f.mpIndInitData != nullptr);
	for (u32 i = 0; i < n; i++) {
		auto* d = &f.mpMaterialInitData[f.mpMaterialID[i]];
		o.push_back(f.getMaterialMode(i));
		o.push_back(f.countStages(i));
		o.push_back(f.newColorChanNum(i));
		o.push_back(f.newTexGenNum(i));
		o.push_back(f.newCullMode(i));
		o.push_back(f.newTevStageNum(i));
		o.push_back(f.newZCompLoc(i));
		o.push_back(f.newDither(i));
		for (int s = 0; s < 2; s++)
			o.push_back(rgba(f.newMatColor(i, s).color));
		for (int s = 0; s < 4; s++)
			o.push_back(rgba(f.newTevKColor(i, s).color));
		for (int s = 0; s < 4; s++) {
			GXColorS10 c = f.newTevColor(i, s).color;
			o.insert(o.end(), { c.r, c.g, c.b, c.a });
		}
		for (int s = 0; s < 8; s++)
			o.push_back(f.newTexNo(i, s));
		if constexpr (std::is_same_v<F, J3DMaterialFactory>) {
			for (int s = 0; s < 2; s++)
				o.push_back(rgba(f.newAmbColor(i, s).color));
			o.push_back(f.countTexGens(i));
		}
		for (int s = 0; s < 8; s++) {
			if (d->mTexMtxIdx[s] == 0xFFFF) {
				o.push_back(-1);
				continue;
			}
			J3DTexMtx* m = f.newTexMtx(i, s);
			o.insert(o.end(), { m->mProjection, m->mInfo, (s64)m->mSRT.mRotation });
			for (f32 x : { m->mCenter.x, m->mSRT.mScaleX, m->mSRT.mScaleY, m->mSRT.mTranslationY, m->mEffectMtx[0][0],
			               m->mEffectMtx[3][3] })
				addF(o, x);
			delete m;
		}
		if (d->mFogIdx == 0xFFFF)
			o.push_back(-1);
		else {
			J3DFog* g = f.newFog(i);
			o.insert(o.end(), { g->mType, g->mAdjEnable, g->mCenter, (s64)rgba(g->mColor), g->mFogAdjTable[0], g->mFogAdjTable[9] });
			for (f32 x : { g->mStartZ, g->mEndZ, g->mNearZ, g->mFarZ })
				addF(o, x);
			delete g;
		}
		if (d->mAlphaCompIdx == 0xFFFF)
			o.push_back(-1);
		else {
			J3DAlphaComp a = f.newAlphaComp(i);
			o.insert(o.end(), { a.mAlphaCmpID, a.mRef0, a.mRef1 });
		}
		if (d->mZModeIdx == 0xFF)
			o.push_back(-1);
		else
			o.push_back(f.newZMode(i).mZModeID);
		if (d->mBlendIdx == 0xFFFF)
			o.push_back(-1);
		else {
			J3DBlend bl = f.newBlend(i);
			o.insert(o.end(), { bl.mBlendMode, bl.mSrcFactor, bl.mDstFactor, bl.mLogicOp });
		}
		if (d->mNBTScaleIdx == 0xFFFF)
			o.push_back(-1);
		else {
			J3DNBTScale nb = f.newNBTScale(i);
			o.push_back(nb.mbHasScale);
			for (f32 x : { nb.mScale.x, nb.mScale.y, nb.mScale.z })
				addF(o, x);
		}
		if constexpr (std::is_same_v<F, J3DMaterialFactory>) {
			if (f.mpIndInitData) {
				u32 st = f.newIndTexStageNum(i);
				o.push_back(st);
				for (u32 s = 0; s < st; s++) {
					J3DIndTexMtx m = f.newIndTexMtx(i, s);
					addF(o, m.mOffsetMtx[0][0]);
					addF(o, m.mOffsetMtx[1][2]);
					o.push_back(m.mScaleExp);
				}
			}
		}
	}
	return o;
}

// Expected observations straight from the raw big-endian block (independent of every swap and of the factories).
static Obs expect(const Raw& r, bool v26, bool ind)
{
	Obs o;
	u32 uniq = 0;
	for (; uniq < r.n; uniq++)
		if (r.ids[uniq] < uniq)
			break;
	o.push_back(v26 ? uniq : r.n); // J3DMaterialFactory_v21::countUniqueMaterials has no break: always the material count
	if (v26)
		o.push_back(ind);
	auto tb = [&](const char* name) {
		for (u32 k = 0; k < kNumTbl; k++)
			if (!strcmp(kTbl[k].name, name))
				return r.b + r.t[k].off;
		return (const u8*)nullptr;
	};
	auto idx8  = [&](const u8* rec, u32 o2) { return (u32)rec[o2]; };
	auto idx16 = [&](const u8* rec, u32 o2) { return (u32)r16(rec + o2); };
	for (u32 i = 0; i < r.n; i++) {
		const u8* d = r.rec(r.ids[i]);
		o.push_back(d[0]);
		u32 st = idx8(d, I(mTevStageNumIdx)) == 0xFF ? 0 : tb("stnum")[idx8(d, I(mTevStageNumIdx))];
		u32 tn = 0;
		for (u32 s = 0; s < 8; s++)
			tn += idx16(d, I(mTexNoIdx) + 2 * s) != 0xFFFF;
		o.push_back(st != tn && tn != 0 ? std::max(st, tn) : st);
		auto u8t = [&](const char* name, u32 slot, s64 dflt) {
			u32 x = idx8(d, slot);
			o.push_back(x == 0xFF ? dflt : tb(name)[x]);
		};
		u8t("chnum", I(mColorChanNumIdx), 0);
		u8t("tgnum", I(mTexGenNumIdx), 0);
		u32 c = idx8(d, I(mCullModeIdx));
		o.push_back(c == 0xFF ? 0xFF : r32(tb("cull") + 4 * c) & 0xFF);
		u8t("stnum", I(mTevStageNumIdx), 0xFF);
		u8t("zcomp", I(mZCompLocIdx), 0xFF);
		u8t("dither", I(mDitherIdx), 0xFF);
		for (u32 s = 0; s < 2; s++) {
			u32 x = idx16(d, I(mMatColorIdx) + 2 * s);
			o.push_back(x == 0xFFFF ? 0xFFFFFFFFu : rgba(tb("matcol") + 4 * x));
		}
		for (u32 s = 0; s < 4; s++) {
			u32 x = idx16(d, I(mTevKColorIdx) + 2 * s);
			o.push_back(x == 0xFFFF ? 0xFFFFFFFFu : rgba(tb("tevk") + 4 * x));
		}
		for (u32 s = 0; s < 4; s++) {
			u32 x = idx16(d, I(mTevColorIdx) + 2 * s);
			for (u32 k = 0; k < 4; k++)
				o.push_back(x == 0xFFFF ? 0 : (s16)r16(tb("tevcol") + 8 * x + 2 * k));
		}
		for (u32 s = 0; s < 8; s++) {
			u32 x = idx16(d, I(mTexNoIdx) + 2 * s);
			o.push_back(x == 0xFFFF ? 0xFFFF : r16(tb("texno") + 2 * x));
		}
		if (v26) {
			for (u32 s = 0; s < 2; s++) {
				u32 x = idx16(d, I(mAmbColorIdx) + 2 * s);
				o.push_back(x == 0xFFFF ? 0x32323232u : rgba(tb("amb") + 4 * x));
			}
			o.push_back(idx8(d, I(mTexGenNumIdx)) == 0xFF ? 0 : tb("tgnum")[idx8(d, I(mTexGenNumIdx))]);
		}
		for (u32 s = 0; s < 8; s++) {
			u32 x = idx16(d, I(mTexMtxIdx) + 2 * s);
			if (x == 0xFFFF) {
				o.push_back(-1);
				continue;
			}
			const u8* m = tb("tmtx") + 0x64 * x;
			o.insert(o.end(), { m[0], m[1], (s16)r16(m + 0x18) });
			for (u32 at : { 4u, 0x10u, 0x14u, 0x20u, 0x24u, 0x24u + 15 * 4 })
				o.push_back(r32(m + at));
		}
		u32 fg = idx16(d, I(mFogIdx));
		if (fg == 0xFFFF)
			o.push_back(-1);
		else {
			const u8* g = tb("fog") + 0x2C * fg;
			o.insert(o.end(), { g[0], g[1], r16(g + 2), (s64)rgba(g + 0x14), r16(g + 0x18), r16(g + 0x18 + 18) });
			for (u32 at : { 4u, 8u, 12u, 16u })
				o.push_back(r32(g + at));
		}
		u32 al = idx16(d, I(mAlphaCompIdx));
		if (al == 0xFFFF)
			o.push_back(-1);
		else {
			const u8* a = tb("alpha") + 8 * al;
			o.insert(o.end(), { (a[0] << 5) + (a[2] << 3) + a[3], a[1], a[4] });
		}
		u32 zm = idx8(d, I(mZModeIdx));
		if (zm == 0xFF)
			o.push_back(-1);
		else {
			const u8* z = tb("zmode") + 4 * zm;
			o.push_back(z[1] * 2 + z[0] * 0x10 + z[2]);
		}
		u32 bl = idx16(d, I(mBlendIdx));
		if (bl == 0xFFFF)
			o.push_back(-1);
		else {
			const u8* x = tb("blend") + 4 * bl;
			o.insert(o.end(), { x[0], x[1], x[2], x[3] });
		}
		u32 nb = idx16(d, I(mNBTScaleIdx));
		if (nb == 0xFFFF)
			o.push_back(-1);
		else {
			const u8* x = tb("nbt") + 0x10 * nb;
			o.insert(o.end(), { x[0], r32(x + 4), r32(x + 8), r32(x + 12) });
		}
		if (v26 && ind) {
			const u8* x = r.b + r32(r.b + H(mpIndInitData)) + i * sizeof(J3DIndInitData);
			u32 n2      = x[0] == 1 ? x[1] : 0;
			o.push_back(n2);
			for (u32 s = 0; s < n2; s++) {
				const u8* m = x + 0x14 + 0x1C * s;
				o.insert(o.end(), { r32(m), r32(m + 20), (s8)m[0x18] });
			}
		}
	}
	return o;
}

// MAT2 block (UNVERIFIED layout) built from the raw MAT3 block: header without ind/amb/light, records without the
// ambient color and light index slots, tables in MAT3 order minus those, only referenced elements. Big-endian.
static std::vector<u8> makeMat2(const Raw& r)
{
	std::vector<u8> v(sizeof(J3DMaterialBlock_v21), 0);
	memcpy(v.data(), "MAT2", 4);
	put16(v, 8, r.n);
	put16(v, 10, 0xFFFF);
	auto al = [&] { while (v.size() % 4) v.push_back(0); };
	put32(v, 0x0C, v.size());
	for (u32 i = 0; i < r.nInit; i++) {
		const u8* d = r.rec(i);
		v.insert(v.end(), d, d + 0x14);
		v.insert(v.end(), d + 0x28, d + sizeof(J3DMaterialInitData));
	}
	al();
	put32(v, 0x10, v.size());
	for (u32 i = 0; i < r.n; i++) {
		v.push_back(r.ids[i] >> 8);
		v.push_back((u8)r.ids[i]);
	}
	al();
	u32 slot = 0x18;
	for (u32 ti = 0; ti < kNumTbl; ti++) {
		if (!strcmp(kTbl[ti].name, "amb") || !strcmp(kTbl[ti].name, "light"))
			continue;
		u32 bytes = r.t[ti].refCnt * kTbl[ti].es;
		if (bytes) {
			put32(v, slot, v.size());
			v.insert(v.end(), r.b + r.t[ti].off, r.b + r.t[ti].off + bytes);
			al();
		}
		slot += 4;
	}
	while (v.size() % 32)
		v.push_back(kPad[v.size() % 32]);
	put32(v, 4, v.size());
	return v;
}

// ---- main -------------------------------------------------------------------------------------------------------

static std::map<std::string, u32> g_swapped, g_unref;
static u32 g_files[2][2]; // [bmd,bmt][with MAT3, without]
static u32 g_mats, g_inits, g_ind, g_noInd, g_amb12, g_mat2;

static void checkRanges(const u8* blk, const Raw& r, s32 texNum, bool hasInd)
{
	const J3DMaterialBlock* b = (const J3DMaterialBlock*)blk;
	for (u32 ti = 0; ti < kNumTbl; ti++) {
		const Tbl& t = kTbl[ti];
		std::string nm = t.name;
		for (u32 e = 0; e < r.t[ti].refCnt; e++) {
			const u8* p = blk + r.t[ti].off + e * t.es;
			if (nm == "cull") CHECK(*(const u32*)p <= 3, "cull %u", *(const u32*)p);
			else if (nm == "texno") CHECK(texNum < 0 || *(const u16*)p < texNum, "texno %u >= %d", *(const u16*)p, texNum);
			else if (nm == "tevcol") {
				const GXColorS10* c = (const GXColorS10*)p;
				for (s16 x : { c->r, c->g, c->b, c->a })
					CHECK(x >= -1024 && x <= 1023, "tevcol %d", x);
			} else if (nm == "light") {
				const J3DLightInfo* l = (const J3DLightInfo*)p;
				for (f32 x : { l->mLightPosition.x, l->mLightPosition.z, l->mLightDirection.y, l->mCosAtten.x, l->mDistAtten.z })
					CHECK(fin(x), "light float");
			} else if (nm == "tmtx" || nm == "tmtx2") {
				const J3DTexMtxInfo* m = (const J3DTexMtxInfo*)p;
				CHECK(m->mProjection <= 1 && (m->mInfo & 0x7F) <= 11, "texmtx proj %u info %u", m->mProjection, m->mInfo);
				for (f32 x : { m->mCenter.x, m->mCenter.y, m->mCenter.z, m->mSRT.mScaleX, m->mSRT.mScaleY, m->mSRT.mTranslationX,
				               m->mSRT.mTranslationY })
					CHECK(fin(x), "texmtx srt/center float");
				for (int a = 0; a < 4; a++)
					for (int c = 0; c < 4; c++)
						CHECK(fin(m->mEffectMtx[a][c]), "texmtx mtx float");
			} else if (nm == "fog") {
				const J3DFogInfo* f = (const J3DFogInfo*)p;
				CHECK(f->mType <= 7 && f->mAdjEnable <= 1, "fog type %u adj %u", f->mType, f->mAdjEnable);
				for (f32 x : { f->mStartZ, f->mEndZ, f->mNearZ, f->mFarZ })
					CHECK(fin(x), "fog float");
				for (u16 x : f->mFogAdjTable)
					CHECK(x <= 0x3FF, "fog adj %u", x);
			} else if (nm == "nbt") {
				const J3DNBTScaleInfo* n = (const J3DNBTScaleInfo*)p;
				CHECK(n->mbHasScale <= 1 && fin(n->mScale.x) && fin(n->mScale.y) && fin(n->mScale.z), "nbt");
			} else if (nm == "chinfo") {
				const J3DColorChanInfo* c = (const J3DColorChanInfo*)p;
				CHECK(c->mEnable <= 1 && c->mMatSrc <= 1 && c->mDiffuseFn <= 2 && c->mAttnFn <= 2
				          && (c->mAmbSrc <= 1 || c->mAmbSrc == 0xFF),
				      "chaninfo");
			} else if (nm == "tc") {
				const J3DTexCoordInfo* c = (const J3DTexCoordInfo*)p;
				CHECK(c->mTexGenType <= 10 && c->mTexGenSrc <= 21 && c->mTexGenMtx <= 60, "texcoord");
			} else if (nm == "tevord") {
				const J3DTevOrderInfo* c = (const J3DTevOrderInfo*)p;
				CHECK((c->mTexCoord <= 8 || c->mTexCoord == 0xFF) && (c->mTexMap <= 8 || c->mTexMap == 0xFF), "tevorder");
			} else if (nm == "alpha") {
				const J3DAlphaCompInfo* c = (const J3DAlphaCompInfo*)p;
				CHECK(c->mComp0 <= 7 && c->mOp <= 3 && c->mComp1 <= 7, "alpha");
			} else if (nm == "blend") {
				const J3DBlendInfo* c = (const J3DBlendInfo*)p;
				CHECK(c->mBlendMode <= 3 && c->mSrcFactor <= 7 && c->mDstFactor <= 7 && c->mLogicOp <= 15, "blend");
			} else if (nm == "zmode") {
				const J3DZModeInfo* c = (const J3DZModeInfo*)p;
				CHECK(c->mCompareEnable <= 1 && c->mFunc <= 7 && c->mUpdateEnable <= 1, "zmode");
			} else if (nm == "swap") {
				const J3DTevSwapModeInfo* c = (const J3DTevSwapModeInfo*)p;
				CHECK(c->mRasSel <= 3 && c->mTexSel <= 3, "tevswap");
			} else if (nm == "stnum") CHECK(*p <= 16, "tev stage num %u", *p);
			else if (nm == "tgnum") CHECK(*p <= 8, "texgen num %u", *p);
			else if (nm == "chnum") CHECK(*p <= 2, "chan num %u", *p);
			else if (nm == "zcomp" || nm == "dither") CHECK(*p <= 1, "%s %u", t.name, *p);
		}
	}
	// init records: material mode, selector arrays
	const J3DMaterialInitData* init = (const J3DMaterialInitData*)(blk + b->mpMaterialInitData);
	for (u32 i = 0; i < r.nInit; i++) {
		CHECK(init[i].mMaterialMode == 1 || init[i].mMaterialMode == 2 || init[i].mMaterialMode == 4, "mode %u", init[i].mMaterialMode);
		for (u8 x : init[i].mTevKColorSel)
			CHECK(x <= 0x1F || x == 0xFF, "kcolor sel %u", x);
		for (u8 x : init[i].mTevKAlphaSel)
			CHECK(x <= 0x1F || x == 0xFF, "kalpha sel %u", x);
	}
	if (hasInd) {
		const J3DIndInitData* ind = (const J3DIndInitData*)(blk + b->mpIndInitData);
		for (u32 i = 0; i < r.n; i++) {
			const J3DIndInitData& d = ind[i];
			CHECK(*(const u8*)&d.mEnabled <= 1 && d.mIndTexStageNum <= 4, "ind enabled/stages");
			for (int m = 0; m < 3; m++) {
				for (int a = 0; a < 2; a++)
					for (int c = 0; c < 3; c++)
						CHECK(fin(d.mIndTexMtxInfo[m].mOffsetMtx[a][c]), "ind mtx float");
				CHECK(d.mIndTexMtxInfo[m].mScaleExp >= -32 && d.mIndTexMtxInfo[m].mScaleExp <= 32, "ind scale exp");
			}
			for (auto& o : d.mIndTexOrderInfo)
				CHECK((o.mCoord <= 8 || o.mCoord == 0xFF) && (o.mMap <= 8 || o.mMap == 0xFF), "ind order");
			for (auto& s : d.mIndTevStageInfo)
				CHECK(s.mIndStage <= 3 && s.mIndFormat <= 3 && s.mBiasSel <= 7 && s.mMtxSel <= 12 && s.mWrapS <= 6 && s.mWrapT <= 6
				          && s.mPrev <= 1 && s.mLod <= 1 && s.mAlphaSel <= 3,
				      "ind tev stage");
		}
	}
}

static void sameObs(const Obs& got, const Obs& want, const char* what)
{
	size_t n = std::min(got.size(), want.size()), i = 0;
	while (i < n && got[i] == want[i])
		i++;
	CHECK(i == n && got.size() == want.size(), "%s: first difference at observation %zu of %zu/%zu: %lld vs %lld", what, i, got.size(),
	      want.size(), i < got.size() ? (long long)got[i] : -1LL, i < want.size() ? (long long)want[i] : -1LL);
}

// Element swappers against the oracle on random bytes, including the kinds the corpus never references (light).
static void swapperTest()
{
	static const struct {
		Kind k;
		u32 es;
		void (*fn)(u8*);
	} sw[] = {
		{ U16, 2, J3DSwapMatU16PC }, { U32, 4, J3DSwapMatU32PC }, { S10, 8, J3DSwapMatColorS10PC },
		{ LIGHT, 0x34, J3DSwapMatLightPC }, { TEXMTX, 0x64, J3DSwapMatTexMtxPC }, { FOG, 0x2C, J3DSwapMatFogPC },
		{ NBT, 0x10, J3DSwapMatNBTScalePC },
	};
	u32 seed = 12345;
	for (auto& s : sw) {
		Oracle o;
		o.out.resize(s.es);
		for (u8& x : o.out)
			x = (u8)((seed = seed * 1664525 + 1013904223) >> 24);
		o.cov.assign(s.es, 0);
		std::vector<u8> prod = o.out;
		s.fn(prod.data());
		o.elem(0, s.k);
		CHECK(prod == o.out, "swapper kind %d differs from the oracle", (int)s.k);
		for (u8 c : o.cov)
			CHECK(c <= 1, "oracle converts a byte twice, kind %d", (int)s.k);
	}
}

static void processFile(const std::filesystem::path& path, int kind)
{
	FILE* fp = fopen(path.string().c_str(), "rb");
	std::vector<u8> file(std::filesystem::file_size(path));
	fread(file.data(), 1, file.size(), fp);
	fclose(fp);
	g_cur = path.filename().string();
	const std::vector<u8> rawFile = file;

	// 1. file header: converted once
	CHECK(J3DSwapFileHeaderPC(file.data()), "first J3DSwapFileHeaderPC");
	JUTDataFileHeader* fh = (JUTDataFileHeader*)file.data();
	u32 mat = 0, tex = 0;
	for (u32 off = 0x20, k = 0; k < fh->mBlockNum; k++) {
		JUTDataBlockHeader* bh = (JUTDataBlockHeader*)(file.data() + off);
		if (bh->mType == 'MAT3') mat = off;
		if (bh->mType == 'TEX1') tex = off;
		CHECK(bh->mSize == r32(rawFile.data() + off + 4), "block size");
		off += bh->mSize;
	}
	if (!mat) {
		CHECK(!J3DSwapFileHeaderPC(file.data()), "second J3DSwapFileHeaderPC");
		g_files[kind][1]++;
		return;
	}
	g_files[kind][0]++;
	u32 size = ((JUTDataBlockHeader*)(file.data() + mat))->mSize;
	CHECK(mat + size <= file.size() && size % 32 == 0 && size >= sizeof(J3DMaterialBlock), "block extent");

	// 2. production swap, second header call must be a no-op
	J3DSwapMat3BlockPC(file.data() + mat);
	CHECK(!J3DSwapFileHeaderPC(file.data()), "second J3DSwapFileHeaderPC");
	const u8* blk = file.data() + mat;

	// 3. raw derivation
	Raw r;
	std::vector<u8> rawBlk(rawFile.begin() + mat, rawFile.begin() + mat + size);
	derive(r, rawBlk.data(), size);
	g_mats += r.n;
	g_inits += r.nInit;
	CHECK(r.n == 0 || (r.idOff > r.initOff && r.idOff - r.initOff == r.nInit * sizeof(J3DMaterialInitData)), "init extent");
	u32 nameOff = r32(rawBlk.data() + H(mpNameTable));
	CHECK(r.n * 2 <= nameOff - r.idOff && nameOff - r.idOff == ((r.n * 2 + 3) & ~3u), "id table extent");
	CHECK(r16(rawBlk.data() + nameOff) == r.n, "name table count");
	u32 prev = 0;
	for (u32 o = H(mpMaterialInitData); o <= H(mpNBTScaleInfo); o += 4) {
		u32 v = r32(rawBlk.data() + o);
		CHECK(v == 0 || (v >= prev && v >= sizeof(J3DMaterialBlock) && v <= size && v % 4 == 0), "offset order at 0x%X", o);
		if (v)
			prev = v;
	}

	// table extents: two derivations must agree
	bool ambBug = false;
	for (u32 ti = 0; ti < kNumTbl; ti++) {
		const Tbl& t = kTbl[ti];
		const TblInfo& in = r.t[ti];
		if (in.off == 0) {
			CHECK(in.refCnt == 0, "%s absent but referenced", t.name);
			continue;
		}
		if (!strcmp(t.name, "chinfo") && in.extCnt < 0) { // known exporter bug, see below
			u32 amb = r.t[4].off;
			CHECK(amb == in.off + 8 && r.t[5].off == amb, "chinfo overrun without the amb offset bug");
			ambBug = true;
			continue;
		}
		if (in.refCnt == 0)
			continue; // empty tables share their offset with the next table
		CHECK(in.extCnt >= 0 && (u32)in.extCnt >= in.refCnt, "%s: refs %u, extent %d", t.name, in.refCnt, in.extCnt);
		if (in.extCnt > (s32)in.refCnt)
			g_unref[t.name]++;
		if (t.kind != RAW && in.extCnt > (s32)in.refCnt)
			CHECK(!strcmp(t.name, "cull") && in.extCnt == 3, "%s: unreferenced tail of %d elements", t.name, in.extCnt - (s32)in.refCnt);
		if (!strcmp(t.name, "cull"))
			for (s32 e = 0; e < in.extCnt; e++)
				CHECK(r32(rawBlk.data() + in.off + 4 * e) <= 3, "cull entry beyond refs is not a cull mode");
	}
	if (ambBug) { // chinfo and amb are laid out back to back although the amb offset points into chinfo
		g_amb12++;
		u32 end = r.t[3].off + r.t[3].refCnt * 8 + r.t[4].refCnt * 4;
		CHECK(end <= r.t[6].off && r.t[6].off - end < 4, "amb bug: chinfo+amb do not end at tgnum");
	}

	// oracle
	Oracle o;
	o.out = rawBlk;
	o.cov.assign(size, 0);
	o.s32(0);
	o.s32(4);
	o.s16(8);
	for (u32 x = H(mpMaterialInitData); x <= H(mpNBTScaleInfo); x += 4)
		o.s32(x);
	for (u32 i = 0; i < r.n; i++)
		o.s16(r.idOff + 2 * i);
	for (u32 i = 0; i < r.nInit; i++)
		for (u32 x = 8; x < sizeof(J3DMaterialInitData); x += 2)
			if (x < I(mTevKColorSel) || x >= I(mTevKColorSel) + 32)
				o.s16(r.initOff + i * sizeof(J3DMaterialInitData) + x);
	for (u32 ti = 0; ti < kNumTbl; ti++) {
		for (u32 e = 0; e < r.t[ti].refCnt; e++)
			o.elem(r.t[ti].off + e * kTbl[ti].es, kTbl[ti].kind);
		if (kTbl[ti].kind != RAW)
			g_swapped[kTbl[ti].name] += r.t[ti].refCnt;
	}
	u32 indOff = r32(rawBlk.data() + H(mpIndInitData)), nameO = nameOff;
	bool hasInd = indOff != 0 && indOff - nameO > 4;
	g_ind += hasInd, g_noInd += !hasInd;
	if (hasInd) {
		CHECK(r32(rawBlk.data() + H(mpCullMode)) - indOff == r.n * sizeof(J3DIndInitData), "ind extent");
		for (u32 i = 0; i < r.n; i++)
			for (u32 m = 0; m < 3; m++)
				for (u32 c = 0; c < 6; c++)
					o.s32(indOff + i * sizeof(J3DIndInitData) + offsetof(J3DIndInitData, mIndTexMtxInfo) + m * sizeof(J3DIndTexMtxInfo)
					      + 4 * c);
	}
	for (u32 x = 0; x < size; x++)
		if (o.cov[x] > 1) {
			CHECK(false, "byte 0x%X converted %u times", x, o.cov[x]);
			break;
		}
	CHECK(memcmp(o.out.data(), blk, size) == 0, "production result differs from the oracle");

	// 4. value ranges on the host-order block
	const J3DMaterialBlock* hb = (const J3DMaterialBlock*)blk;
	CHECK(hb->mType == 'MAT3' && hb->mMaterialNum == r.n, "host header");
	const u16* ids = (const u16*)(blk + hb->mpMaterialID);
	for (u32 i = 0; i < r.n; i++)
		CHECK(ids[i] == r.ids[i] && ids[i] < r.nInit, "material id");
	s32 texNum = tex ? (s32)r16(rawFile.data() + tex + 8) : -1;
	checkRanges(blk, r, texNum, hasInd);

	// 5. factory on the swapped block vs raw reads; MAT2 built from the same data
	J3DMaterialFactory f(*hb);
	CHECK((f.mpIndInitData != nullptr) == hasInd, "factory indirect presence");
	sameObs(probe(f, true, r.n), expect(r, true, hasInd), "J3DMaterialFactory");

	std::vector<u8> m2 = makeMat2(r);
	JUTDataBlockHeader* h2 = (JUTDataBlockHeader*)m2.data(); // J3DSwapFileHeaderPC converts these two words
	h2->mType = be32(h2->mType);
	h2->mSize = be32(h2->mSize);
	J3DSwapMat2BlockPC(m2.data());
	J3DMaterialFactory_v21 f2(*(const J3DMaterialBlock_v21*)m2.data());
	sameObs(probe(f2, false, r.n), expect(r, false, false), "J3DMaterialFactory_v21");
	g_mat2++;
}

int main(int argc, char** argv)
{
	std::filesystem::path dir = argc > 1 ? argv[1] : "build/j3d-corpus";
	std::vector<std::filesystem::path> files;
	for (auto& e : std::filesystem::directory_iterator(dir)) {
		std::string x = e.path().extension().string();
		if ((x == ".bmd" || x == ".bmt") && e.file_size() >= 0x20)
			files.push_back(e.path());
	}
	std::sort(files.begin(), files.end());
	g_cur = "swapper unit test";
	swapperTest();
	for (auto& p : files)
		processFile(p, p.extension() == ".bmt");
	std::printf("files: .bmd %u with MAT3 + %u without, .bmt %u with MAT3 + %u without\n", g_files[0][0], g_files[0][1], g_files[1][0],
	            g_files[1][1]);
	std::printf("materials %u, init records %u, indirect data in %u blocks (none in %u), amb/light offset bug in %u blocks, "
	            "MAT2 round trips %u\n",
	            g_mats, g_inits, g_ind, g_noInd, g_amb12, g_mat2);
	for (auto& k : g_swapped)
		std::printf("  swapped elements %-7s %u\n", k.first.c_str(), k.second);
	for (auto& k : g_unref)
		std::printf("  blocks with unreferenced tail elements in %-7s %u\n", k.first.c_str(), k.second);
	std::printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
	return g_fail != 0;
}
