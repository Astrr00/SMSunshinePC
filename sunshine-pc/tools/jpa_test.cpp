// Native end-to-end check of the JPA (.jpa particle resource) pipeline on the host: Yaz0 + RARC archive through the real
// JKRDecomp/JKRMemArchive, every .jpa through the real JPAResourceManager::load (JPAEmitterLoaderDataBase, JPABaseShape,
// JPAExtraShape, JPASweepShape, JPAExTexShape, JPADataBlock copies, JPATexture), an emitter per resource through the
// real JPAEmitterManager::createEmitterBase (JPABaseEmitter::loadBaseEmitterBlock, JPABaseField::loadFieldBlock,
// JPADraw::initialize), and a few calc() frames of the emitters that do not need __fres.
// Prints "key value" lines (floats as f<hex bits>) that tools/jpa_test.py diffs against an independent big-endian parse
// of the same file bytes. Build/run: tools\jpa_test.cmd <private build name>.
//   jpa_test <archive.szs>... [--no-calc]       dump (default)
//   jpa_test --extract <dir> <archive.szs>...   write every .jpa of the archives to <dir>/<archive>/<name>
// The GX texture-object functions are recorded by test-local stand-ins (Aurora's live in the renderer library).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <type_traits>
#include <filesystem>
#include <new>
#define NOMINMAX // JGeometry declares its own min/max templates
#include <windows.h>

#define protected public
#define private public
#include <JSystem/JKernel/JKRExpHeap.hpp>
#include <JSystem/JKernel/JKRDecomp.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JParticle/JPAEmitterLoader.hpp>
#include <JSystem/JParticle/JPAParticle.hpp>
#include <JSystem/JParticle/JPABaseShape.hpp>
#include <JSystem/JParticle/JPAExtraShape.hpp>
#include <JSystem/JParticle/JPASweepShape.hpp>
#include <JSystem/JParticle/JPAExTexShape.hpp>
#include <JSystem/JMath.hpp>
#undef private
#undef protected
#include <hostendian.h>

void installCrashHandler(); // sunshine-pc/src/crash.cpp: symbolized stack on a fault
static LPTOP_LEVEL_EXCEPTION_FILTER g_prevFilter;
// A call through an unresolved symbol (linked with /FORCE:UNRESOLVED) jumps to address 0: report from the caller.
static LONG WINAPI nullCallFilter(EXCEPTION_POINTERS* i)
{
	if (i->ContextRecord->Rip == 0) {
		fprintf(stderr, "\ncall through an unresolved symbol; caller below\n");
		i->ContextRecord->Rip = *(u64*)i->ContextRecord->Rsp;
		i->ContextRecord->Rsp += 8;
	}
	return g_prevFilter ? g_prevFilter(i) : EXCEPTION_CONTINUE_SEARCH;
}

// ---- test-local GX texture object recorder (what Aurora's GXInitTexObj* would store) ------------------------------
struct TexObjRec {
	const void* data;
	u16 w, h;
	u32 fmt;
};
static TexObjRec& rec(GXTexObj* o) { return *reinterpret_cast<TexObjRec*>(o); }
static_assert(sizeof(TexObjRec) <= sizeof(GXTexObj), "GXTexObj too small for the test recorder");
void GXInitTexObj(GXTexObj* o, void* data, u16 w, u16 h, GXTexFmt f, GXTexWrapMode, GXTexWrapMode, GXBool)
{
	rec(o) = { data, w, h, (u32)f };
}
void GXInitTexObjCI(GXTexObj* o, void* data, u16 w, u16 h, GXCITexFmt f, GXTexWrapMode, GXTexWrapMode, GXBool, u32)
{
	rec(o) = { data, w, h, (u32)f };
}
void GXInitTexObjLOD(GXTexObj*, GXTexFilter, GXTexFilter, f32, f32, f32, GXBool, GXBool, GXAnisotropy) { }
void GXInitTlutObj(GXTlutObj*, void*, GXTlutFmt, u16) { }

// ---- output ---------------------------------------------------------------------------------------------------------
static int g_fail = 0;
static std::string g_file;
#define CHECK(c, ...)                                                                                                  \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			if (g_fail++ < 40) {                                                                                       \
				fprintf(stderr, "FAIL %s: ", g_file.c_str());                                                          \
				fprintf(stderr, __VA_ARGS__);                                                                          \
				fprintf(stderr, "\n");                                                                                 \
			}                                                                                                          \
		}                                                                                                              \
	} while (0)

template <class T> static void P(const std::string& key, T v)
{
	if constexpr (std::is_floating_point_v<T>) {
		float f = (float)v;
		u32 u;
		memcpy(&u, &f, 4);
		printf("%s f%08x\n", key.c_str(), u);
	} else {
		printf("%s %lld\n", key.c_str(), (long long)v);
	}
}
static void PH(const std::string& key, const u8* p, size_t n)
{
	printf("%s h", key.c_str());
	for (size_t i = 0; i < n; i++)
		printf("%02x", p[i]);
	printf("\n");
}
static void PC(const std::string& key, GXColor c) { printf("%s h%02x%02x%02x%02x\n", key.c_str(), c.r, c.g, c.b, c.a); }
static void PV(const std::string& key, const JGeometry::TVec3<f32>& v)
{
	P(key + ".x", v.x);
	P(key + ".y", v.y);
	P(key + ".z", v.z);
}

static u32 crc32(const u8* p, size_t n, u32 c = 0)
{
	c = ~c;
	for (size_t i = 0; i < n; i++) {
		c ^= p[i];
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1)));
	}
	return ~c;
}

// ---- heap -----------------------------------------------------------------------------------------------------------
static JKRHeap* g_heap;
// A root heap over a plain block (JKRExpHeap::createRoot would take the console arena, which does not exist here).
static void newHeap()
{
	const u32 size   = 256u << 20;
	u8* mem          = (u8*)_aligned_malloc(size, 32);
	const u32 head   = (sizeof(JKRExpHeap) + 15) & ~15u;
	JKRExpHeap* heap = new (mem) JKRExpHeap(mem + head, size - head, nullptr, false);
	JKRHeap::sRootHeap = heap;
	heap->mIsRoot      = true;
	heap->becomeCurrentHeap();
	g_heap = heap;
}

// ---- archive --------------------------------------------------------------------------------------------------------
static std::vector<u8> readFile(const char* path)
{
	std::vector<u8> v;
	FILE* f = fopen(path, "rb");
	if (!f)
		return v;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	v.resize(n);
	if (n)
		fread(v.data(), 1, n, f);
	fclose(f);
	return v;
}

struct JpaFile {
	std::string name; // path inside the archive
	const u8* data;
	u32 size;
};

static void collect(JKRMemArchive* a, u32 dirIdx, const std::string& prefix, std::vector<JpaFile>& out)
{
	JKRArchive::SDIDirEntry* dir = a->mDirectories + dirIdx;
	for (u32 i = 0; i < dir->mNum; i++) {
		JKRArchive::SDIFileEntry* f = a->mFileEntries + dir->mFirstIdx + i;
		std::string name            = a->mStrTable + (f->mFlagsAndNameOffset & 0xFFFFFF);
		std::string path            = prefix + "/" + name;
		if (f->mFlagsAndNameOffset >> 24 & 2) {
			if (name != "." && name != "..")
				collect(a, f->mDataOffset, path, out);
		} else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".jpa") == 0) {
			void* d = a->getResource(path.c_str());
			out.push_back({ path, (const u8*)d, d ? (u32)a->getResSize(d) : 0 });
		}
	}
}

// ---- dump ----------------------------------------------------------------------------------------------------------
static std::string K(const char* pre, int i, const char* k) { return std::string(pre) + std::to_string(i) + "." + k; }

static void dumpBaseShape(JPABaseShape* b)
{
	P("bsp.sizeY", b->mBaseSizeY);
	P("bsp.sizeX", b->mBaseSizeX);
	P("bsp.loopOffset", b->mLoopOffset);
	P("bsp.type", b->mType);
	P("bsp.dirType", b->mDirType);
	P("bsp.rotType", b->mRotType);
	P("bsp.colLoop", b->mColLoopOffset);
	P("bsp.texLoop", b->mTexLoopOffset);
	P("bsp.unk68", b->unk68);
	P("bsp.tev0", (int)b->unk48.unk0);
	P("bsp.tev1", (int)b->unk48.unk4);
	P("bsp.tev2", (int)b->unk48.unk8);
	P("bsp.tev3", (int)b->unk48.unkC);
	P("bsp.unk6C", b->unk6C);
	P("bsp.blend", b->mBlendMode1Index);
	P("bsp.src", b->mSrcBlendFactor1Index);
	P("bsp.dst", b->mDstBlendFactor1Index);
	P("bsp.blendOp", b->mBlendOp1Index);
	P("bsp.acmp0", b->mAlphaCmpComp0Index);
	P("bsp.aref0", b->mAlphaCmpRef0);
	P("bsp.aop", b->mAlphaCmpOpIndex);
	P("bsp.acmp1", b->mAlphaCmpComp1Index);
	P("bsp.aref1", b->mAlphaCmpRef1);
	P("bsp.zloc", b->mZCompLoc);
	P("bsp.unk77", b->unk77);
	P("bsp.zfunc", b->mZCmpFunctionIndex);
	P("bsp.unk79", b->unk79);
	P("bsp.unk7A", b->unk7A);
	P("bsp.unk7B", b->unk7B);
	P("bsp.flags", b->mFlags);
	P("bsp.unk7D", b->unk7D);
	P("bsp.texAnmKeyNum", b->mTextureAnmKeyNum);
	P("bsp.texIndex", b->mTextureIndex);
	P("bsp.unk80", b->unk80);
	P("bsp.unk81", b->unk81);
	P("bsp.colMaxFrm", b->mColorRegAnmMaxFrm);
	P("bsp.unk82", b->unk82);
	P("bsp.unk83", b->unk83);
	P("bsp.unk84", b->unk84);
	P("bsp.unk85", b->unk85);
	P("bsp.unk86", b->unk86);
	P("bsp.unk87", b->unk87);
	PC("bsp.prm", b->mPrmColor);
	PC("bsp.env", b->mEnvColor);
	P("bsp.texStaticTransX", b->mTexStaticTransX);
	P("bsp.texStaticTransY", b->mTexStaticTransY);
	P("bsp.texStaticScaleX", b->mTexStaticScaleX);
	P("bsp.texStaticScaleY", b->mTexStaticScaleY);
	P("bsp.tilingX", b->mTilingX);
	P("bsp.tilingY", b->mTilingY);
	P("bsp.scrollTransX", b->mTexScrollTransX);
	P("bsp.scrollTransY", b->mTexScrollTransY);
	P("bsp.scrollScaleX", b->mTexScrollScaleX);
	P("bsp.scrollScaleY", b->mTexScrollScaleY);
	P("bsp.scrollRotate", b->mTexScrollRotate);
	if (b->unk80)
		PH("bsp.texIndices", b->mTextureIndices, b->mTextureAnmKeyNum);
	else
		CHECK(b->mTextureIndices == nullptr, "texture index table without animation");
	if (b->unk83 & 2)
		PH("bsp.prmTable", (const u8*)b->mPrmColors, (b->mColorRegAnmMaxFrm + 1) * 4);
	else
		CHECK(b->mPrmColors == nullptr, "prm table without animation");
	if (b->unk84 & 2)
		PH("bsp.envTable", (const u8*)b->mEnvColors, (b->mColorRegAnmMaxFrm + 1) * 4);
	else
		CHECK(b->mEnvColors == nullptr, "env table without animation");
}

static void dumpExtraShape(JPAExtraShape* e)
{
	P("esp.alphaInTiming", e->mAlphaInTiming);
	P("esp.alphaOutTiming", e->mAlphaOutTiming);
	P("esp.alphaInValue", e->mAlphaInValue);
	P("esp.alphaBaseValue", e->mAlphaBaseValue);
	P("esp.unk18", e->unk18);
	P("esp.waveParam1", e->mAlphaWaveParam1);
	P("esp.waveParam2", e->mAlphaWaveParam2);
	P("esp.waveParam3", e->mAlphaWaveParam3);
	P("esp.waveRandom", e->mAlphaWaveRandom);
	P("esp.alphaIncRate", e->mAlphaIncreaseRate);
	P("esp.alphaDecRate", e->mAlphaDecreaseRate);
	P("esp.randomScale", e->mRandomScale);
	P("esp.scaleInTiming", e->mScaleInTiming);
	P("esp.scaleOutTiming", e->mScaleOutTiming);
	P("esp.scaleInValueY", e->mScaleInValueY);
	P("esp.unk44", e->unk44);
	P("esp.scaleInValueX", e->mScaleInValueX);
	P("esp.unk4C", e->unk4C);
	P("esp.incRateX", e->mIncreaseRateX);
	P("esp.decRateX", e->mDecreaseRateX);
	P("esp.incRateY", e->mIncreaseRateY);
	P("esp.decRateY", e->mDecreaseRateY);
	P("esp.rotAngle", e->mRotateAngle);
	P("esp.rotSpeed", e->mRotateSpeed);
	P("esp.rotRandSpeed", e->mRotateRandomSpeed);
	P("esp.rotRandAngle", e->mRotateRandomAngle);
	P("esp.rotDir", e->mRotateDirection);
	P("esp.cycleY", e->mAnmCycleY);
	P("esp.cycleX", e->mAnmCycleX);
	P("esp.unk78", e->unk78);
	P("esp.unk79", e->unk79);
	P("esp.unk7A", e->unk7A);
	P("esp.unk7B", e->unk7B);
	P("esp.unk7C", e->unk7C);
	P("esp.unk7D", e->unk7D);
	P("esp.unk7E", e->unk7E);
	P("esp.unk7F", e->unk7F);
}

static void dumpSweepShape(JPASweepShape* s)
{
	P("ssp.type", s->mType);
	P("ssp.dirType", s->mDirType);
	P("ssp.rotType", s->mRotType);
	P("ssp.life", s->mLife);
	P("ssp.rate", s->mRate);
	P("ssp.step", s->mStep);
	P("ssp.unk8", s->unk8);
	P("ssp.baseVel", s->mBaseVel);
	P("ssp.affectedByFields", s->mChildrenAffectedByFields);
	P("ssp.unk49", s->unk49);
	P("ssp.scaleOut", s->mEnableScaleOut);
	P("ssp.alphaOut", s->mEnableAlphaOut);
	P("ssp.texIndex", s->mTextureIndex);
	P("ssp.scaleY", s->mScaleY);
	P("ssp.scaleX", s->mScaleX);
	P("ssp.enableRotate", s->mEnableRotate);
	P("ssp.unk4E", s->unk4E);
	PC("ssp.prm", s->mPrmColor);
	PC("ssp.env", s->mEnvColor);
	P("ssp.timing", s->mTiming);
	P("ssp.velInh", s->mVelInhRate);
	P("ssp.baseVelRndm", s->mBaseVelRndm);
	P("ssp.unk24", s->unk24);
	P("ssp.inheritScale", s->mInheritScale);
	P("ssp.inheritAlpha", s->mInheritAlpha);
	P("ssp.inheritRGB", s->mInheritRGB);
	P("ssp.rotSpeed", s->mRotateSpeed);
}

static void dumpExTexShape(JPAExTexShape* x, const u8* raw)
{
	P("etx.mode", x->mIndTexMode);
	P("etx.mtxId", (int)x->mIndTexMtxID);
	if (raw[0x11] <= 1) // other modes leave the matrix unset
		for (int r = 0; r < 2; r++)
			for (int c = 0; c < 3; c++)
				P("etx.m" + std::to_string(r) + std::to_string(c), x->mIndTexMtx[r][c]);
	P("etx.expScale", x->mExpScale);
	P("etx.indTex", x->mIndTextureID);
	P("etx.subTex", x->mSubTextureID);
	P("etx.flags", x->mFlags);
	P("etx.secondTex", x->mSecondTexIndex);
}

static void dumpKeyAnim(int i, JPAKeyFrameAnime* k)
{
	const u8* b = (const u8*)k->mRawData;
	u32 n       = b[0x10];
	P(K("kfa", i, "count"), n);
	P(K("kfa", i, "loop"), b[0x12]);
	const f32* f = (const f32*)(b + 0x20);
	for (u32 j = 0; j < n * 4; j++)
		P(K("kfa", i, "k") + std::to_string(j), f[j]);
	// the copy past the converted floats is the raw block
	u32 size = rdbe32(b + 4);
	P(K("kfa", i, "size"), size);
	P(K("kfa", i, "crcHead"), crc32(b, 0x20));
}

static void dumpTexture(int i, JPATexture* t, JPATextureResource* tr)
{
	const u8* b = (const u8*)t->mRawData;
	u32 size    = rdbe32(b + 4); // the block size field stays big-endian in the copy
	CHECK(memchr(b + 0xC, 0, 0x14) != nullptr, "texture name not terminated");
	printf("%s s%s\n", K("tex", i, "nameStr").c_str(), (const char*)(b + 0xC));
	const ResTIMG& h = t->getTextureData()->mResTIMG;
	P(K("tex", i, "format"), h.format);
	P(K("tex", i, "alpha"), h.alphaEnabled);
	P(K("tex", i, "width"), h.width);
	P(K("tex", i, "height"), h.height);
	P(K("tex", i, "wrapS"), h.wrapS);
	P(K("tex", i, "wrapT"), h.wrapT);
	P(K("tex", i, "isIndex"), h.isIndexTexture);
	P(K("tex", i, "colorFormat"), h.colorFormat);
	P(K("tex", i, "numColors"), h.numColors);
	P(K("tex", i, "paletteOffset"), (s32)h.paletteOffset);
	P(K("tex", i, "mipmap"), h.mipmapEnabled);
	P(K("tex", i, "edgeLod"), h.doEdgeLod);
	P(K("tex", i, "biasClamp"), h.biasClamp);
	P(K("tex", i, "aniso"), h.maxAnisotropy);
	P(K("tex", i, "minFilter"), h.minFilter);
	P(K("tex", i, "magFilter"), h.magFilter);
	P(K("tex", i, "minLod"), h.minLod);
	P(K("tex", i, "maxLod"), h.maxLod);
	P(K("tex", i, "mipCount"), h.mipmapCount);
	P(K("tex", i, "lodBias"), h.lodBias);
	P(K("tex", i, "imageOffset"), (s32)h.imageDataOffset);
	P(K("tex", i, "size"), size);
	// JUTTexture state built from the swapped header; the texture object points at the image bytes of the copy
	P(K("tex", i, "jut.width"), t->unk8.getWidth());
	P(K("tex", i, "jut.height"), t->unk8.getHeight());
	P(K("tex", i, "jut.format"), t->unk8.getFormat());
	P(K("tex", i, "jut.dataOfs"), (const u8*)t->unk8.mTexData - b);
	const TexObjRec& r = *reinterpret_cast<const TexObjRec*>(&t->unk8.mTexObj);
	P(K("tex", i, "obj.dataOfs"), (const u8*)r.data - b);
	P(K("tex", i, "obj.width"), r.w);
	P(K("tex", i, "obj.height"), r.h);
	P(K("tex", i, "obj.format"), r.fmt);
	// everything but the 0x20-byte BTI header stays the raw big-endian block
	u32 c = crc32(b, 0x20);
	c     = crc32(b + 0x40, size - 0x40, c);
	P(K("tex", i, "crc"), c);
	(void)tr;
}

static void dumpEmitter(JPABaseEmitter* e)
{
	PV("bem.scale", e->mScale);
	PV("bem.trans", e->mTrans);
	P("bem.rot.x", e->mRot.x);
	P("bem.rot.y", e->mRot.y);
	P("bem.rot.z", e->mRot.z);
	P("bem.volumeType", e->mVolumeType);
	P("bem.emitInterval", e->mEmitInterval);
	P("bem.volumeSub", e->mVolumeSubdivision);
	P("bem.spawnRate", e->mChildSpawnRate);
	P("bem.spawnRateVar", e->mChildSpawnRateVariance);
	P("bem.maxFrame", e->mMaxFrame);
	P("bem.startFrame", e->mStartFrame);
	P("bem.volumeSize", e->mVolumeSize);
	P("bem.yawSweep", e->mVolumeYawSweep);
	P("bem.minRadius", e->mVolumeMinRadius);
	P("bem.baseLife", e->mBaseLifetime);
	P("bem.lifeRand", e->mLifetimeRandomScale);
	P("bem.baseWeight", e->mBaseWeight);
	P("bem.weightRand", e->mWeightRandomScale);
	P("bem.unk1C4", e->unk1C4);
	P("bem.unk1CC", e->unk1CC);
	P("bem.airRes", e->mBaseAirResistance);
	P("bem.airResVar", e->mAirResistanceVariance);
	P("bem.unk1FC", e->unk1FC);
	P("bem.unk200", e->unk200);
	P("bem.unk204", e->unk204);
	P("bem.unk208", e->unk208);
	P("bem.unk1C8", e->unk1C8);
	PV("bem.dir", e->mEmitterDirection); // normalized: compared with a tolerance
	P("bem.unk1E4", e->unk1E4);
	P("bem.emitFlags", e->mEmitFlags);
	P("bem.keyMask", e->mKeyAnmTypeMask);
}

static void dumpField(int i, JPABaseField* f)
{
	P(K("fld", i, "type"), f->unk50);
	P(K("fld", i, "unk51"), f->unk51);
	P(K("fld", i, "unk52"), f->unk52);
	P(K("fld", i, "unk53"), f->unk53);
	P(K("fld", i, "status"), f->unk54);
	P(K("fld", i, "unk10"), f->unk10);
	P(K("fld", i, "unk14"), f->unk14);
	P(K("fld", i, "maxDist"), f->mMaxDistance);
	PV(K("fld", i, "unk18"), f->unk18);
	PV(K("fld", i, "unk24"), f->unk24);
	P(K("fld", i, "unk30"), f->unk30);
	P(K("fld", i, "unk34"), f->unk34);
	P(K("fld", i, "unk38"), f->unk38);
	P(K("fld", i, "fadeInEnd"), f->mFadeInEnd);
	P(K("fld", i, "fadeOutStart"), f->mFadeOutStart);
	P(K("fld", i, "fadeInStart"), f->mFadeInStart);
	P(K("fld", i, "fadeOutEnd"), f->mFadeOutEnd);
}

static long g_files, g_emitters, g_calc, g_needFres, g_fadeFres, g_particles, g_fields, g_kfa, g_tex;
// the game's globals and loader (System/MarDirectorLoadResource.cpp)
extern JPAResourceManager* gpResourceManager;
extern bool gParticleFlagLoaded[0x201];
class TMarDirector {
public:
	static void loadParticleMario();
};

static void runArchive(const char* archivePath, bool calc)
{
	std::vector<u8> in = readFile(archivePath);
	if (in.size() < 16) {
		fprintf(stderr, "cannot read %s\n", archivePath);
		g_fail++;
		return;
	}
	std::vector<u8> out;
	JKRCompression c = JKRDecomp::checkCompressed(in.data());
	if (c == JKR_COMPRESSION_NONE) {
		out = in;
	} else {
		u32 n = JKRDecompExpandSize(in.data());
		out.assign(n + 32, 0);
		if (c == JKR_COMPRESSION_YAZ0)
			JKRDecomp::decodeSZS(in.data(), out.data(), n, 0);
		else
			JKRDecomp::decodeSZP(in.data(), out.data(), n, 0);
		out.resize(n);
	}
	JKRMemArchive* a = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
	std::vector<JpaFile> files;
	collect(a, 0, "", files);

	printf("@ARCHIVE %s %zu\n", std::filesystem::path(archivePath).filename().string().c_str(), files.size());
	if (files.empty())
		return;

	JPAResourceManager* rm = new JPAResourceManager((u32)files.size() + 1, 0x800, g_heap);
	JPAEmitterManager* em  = new JPAEmitterManager(rm, 4000, 16, 64, g_heap);
	JPATextureResource* tr = rm->getTextureResource();
	// the manager sizes its solid heaps from the host sizeof: every pooled object must have fitted
	g_file = "JPAEmitterManager";
	CHECK(em->unk0.getNumLinks() == 4000 && em->unk14.getNumLinks() == 16 && em->unk28.getNumLinks() == 64,
	      "pools hold %u particles, %u emitters, %u fields (wanted 4000, 16, 64)", em->unk0.getNumLinks(), em->unk14.getNumLinks(),
	      em->unk28.getNumLinks());

	u16 user = 1;
	for (const JpaFile& f : files) {
		g_file = f.name;
		if (!f.data || f.size < 0x20) {
			CHECK(false, "resource missing");
			continue;
		}
		std::vector<u8> pristine(f.data, f.data + f.size);
		printf("@FILE %s %u %u\n", f.name.c_str(), f.size, (unsigned)user);

		u32 texBefore = tr->registNum;
		int idx       = rm->load(f.data, user);
		CHECK(idx == (int)(user - 1), "registration index %d for user %u", idx, user);
		JPAEmitterData* ed = rm->getEmitterResource()->getByUserIndex(user);
		CHECK(ed != nullptr, "emitter data not registered");
		if (!ed)
			continue;
		g_files++;
		P("ed.count", ed->unk4);
		P("ed.user", ed->unk6);
		JPADataBlockLinkInfo* li = ed->getLinkInfo()[0];
		P("link.fld", li->mFieldNum);
		P("link.kfa", li->mKeyframeAnimationNum);
		P("link.tex", li->mTextureNum);
		P("has.bem", li->mBaseEmitterBlock != nullptr);
		P("has.bsp", li->mBaseShape != nullptr);
		P("has.esp", li->mExtraShape != nullptr);
		P("has.ssp", li->mSweepShape != nullptr);
		P("has.etx", li->mExTexShape != nullptr);
		P("tex.newCount", tr->registNum - texBefore);
		P("tex.total", tr->registNum);

		for (int i = 0; i < li->mTextureNum; i++) {
			u16 ti = li->mTextureDataBase[i];
			P(K("tex", i, "idx"), ti);
			CHECK(ti < tr->registNum, "texture index %u out of range", ti);
			if (ti < tr->registNum) {
				dumpTexture(i, tr->pTexResArray[ti], tr);
				g_tex++;
			}
		}
		if (li->mBaseShape) {
			dumpBaseShape(li->mBaseShape);
			// the texture indices of the shape (frame table or single) must address the file's texture list
			JPABaseShape* b = li->mBaseShape;
			if (!b->textureIsEmpty() && li->mTextureNum == 0) {
				// disc defect (ms_m_spinos.jpa in coronaBoss.szs: its TEX1 block is shifted by a byte, so the loader finds
				// no texture): drawing it would read mTexIndices[...] through a null pointer
				fprintf(stderr, "note: %s wants a texture but has none (corrupt TEX1 block on the disc)\n", f.name.c_str());
			} else if (!b->textureIsEmpty()) {
				if (b->isEnableTextureAnm())
					for (int k = 0; k < b->getTextureAnmKeyNum(); k++)
						CHECK(b->getTextureIndex((u8)k) < li->mTextureNum, "anm texture index %u", b->getTextureIndex((u8)k));
				else
					CHECK(b->getTextureIndex() < li->mTextureNum, "texture index %u of %u", b->getTextureIndex(), li->mTextureNum);
			}
		}
		if (li->mExtraShape)
			dumpExtraShape(li->mExtraShape);
		if (li->mSweepShape) {
			dumpSweepShape(li->mSweepShape);
			CHECK(li->mSweepShape->getTextureIndex() < li->mTextureNum || li->mTextureNum == 0, "child texture index");
		}
		if (li->mExTexShape) {
			// ETX1 block position: needed to see which matrix mode the raw block has
			const u8* blk = f.data + 0x20;
			u32 n         = rdbe32(f.data + 0xC);
			for (u32 bi = 0; bi < n; bi++) {
				if (rdbe32(blk) == 'ETX1')
					break;
				blk += rdbe32(blk + 4);
			}
			dumpExTexShape(li->mExTexShape, blk);
		}
		for (int i = 0; i < li->mKeyframeAnimationNum; i++) {
			dumpKeyAnim(i, li->mKeyframeAnimations[i]);
			g_kfa++;
		}
		P("bem.crc", crc32((const u8*)li->mBaseEmitterBlock->mRawData, rdbe32((const u8*)li->mBaseEmitterBlock->mRawData + 4)));
		for (int i = 0; i < li->mFieldNum; i++) {
			const u8* fb = (const u8*)li->mFields[i]->mRawData;
			P(K("fld", i, "crc"), crc32(fb, rdbe32(fb + 4)));
		}

		// the emitter pipeline: BEM1 + FLD1 loads and the draw setup
		JPABaseEmitter* e = em->createEmitterBase(user, 0, 0, nullptr, nullptr);
		CHECK(e != nullptr, "createEmitterBase failed");
		if (e) {
			g_emitters++;
			dumpEmitter(e);
			int nf = 0;
			for (JSULink<JPABaseField>* l = e->mFieldManager.unk0.getFirst(); l; l = l->getNext(), nf++) {
				dumpField(nf, l->getObject());
				g_fields++;
			}
			CHECK(nf == li->mFieldNum, "field count %d of %d", nf, li->mFieldNum);

			// calc a few frames. __fres is undefined on the port by design (it must be bit-exact), and it is needed by
			// calcEmitterGlobalParams() -> JPAVecToRotaMtx whenever the emitter direction is not +Z, and by
			// calcFieldFadeScale() for fields with fade in/out (status 0x20/0x40). Both are neutralized here (after the
			// dump): direction +Z, fades off. Everything else of the per-frame path (key frames, fields, particle creation,
			// draw calc visitors, child particles) runs on the real data.
			JGeometry::TVec3<f32> d, z(0.0f, 0.0f, 1.0f);
			d.normalize(e->mEmitterDirection);
			if (!(d == z))
				g_needFres++;
			bool fades = false;
			for (JSULink<JPABaseField>* l = e->mFieldManager.unk0.getFirst(); l; l = l->getNext())
				fades |= (l->getObject()->unk54 & 0x60) != 0;
			if (fades)
				g_fadeFres++;
			if (calc) {
				e->mEmitterDirection.set(0.0f, 0.0f, 1.0f);
				for (JSULink<JPABaseField>* l = e->mFieldManager.unk0.getFirst(); l; l = l->getNext())
					l->getObject()->unk54 &= ~0x60;
				long maxP = 0;
				int badFrame = -1;
				for (int frame = 0; frame < 8; frame++) {
					e->calc();
					long n = (long)e->getParticleNumber();
					if (n > maxP)
						maxP = n;
					for (JSULink<JPABaseParticle>* l = e->mParticleList.getFirst(); l; l = l->getNext()) {
						JPABaseParticle* p = l->getObject();
						if (!(std::isfinite(p->mLocalPosition.x) && std::isfinite(p->mLocalPosition.y)
						      && std::isfinite(p->mLocalPosition.z)) && badFrame < 0)
							badFrame = frame;
					}
				}
				CHECK(badFrame < 0, "non-finite particle position at frame %d", badFrame);
				g_calc++;
				g_particles += maxP;
			}
			em->forceDeleteEmitter(e);
		}

		CHECK(memcmp(pristine.data(), f.data, f.size) == 0, "the loader modified the file image");
		user++;
	}

	// The game's own load: TMarDirector::loadParticle() mounts particle.arc, makes it the current volume and calls
	// loadParticleMario() (JPAResourceManager::load(const char*) through JKRGetResource, once per particle id).
	if (std::filesystem::path(archivePath).filename() == "particle.szs") {
		g_file = "TMarDirector::loadParticleMario";
		a->becomeCurrent("/");
		gpResourceManager = new JPAResourceManager(0x201, 0x800, g_heap);
		TMarDirector::loadParticleMario();
		JPAEmitterResource* er = gpResourceManager->getEmitterResource();
		int flagged            = 0;
		for (int i = 0; i < 0x201; i++)
			flagged += gParticleFlagLoaded[i];
		CHECK(flagged > 100 && (int)er->registNum == flagged, "loadParticleMario registered %u resources, %d ids flagged", er->registNum, flagged);
		for (u32 i = 0; i < er->registNum; i++) {
			JPAEmitterData* ed = er->pEmtrResArray[i];
			CHECK(ed != nullptr && ed->getLinkInfo()[0]->getBaseEmitterBlock() != nullptr && gParticleFlagLoaded[ed->getUserIndex()],
			      "resource %u incomplete", i);
		}
		fprintf(stderr, "game order: loadParticleMario registered %u resources, %u textures\n", er->registNum,
		        gpResourceManager->getTextureResource()->registNum);
	}
}

int main(int argc, char** argv)
{
	setvbuf(stdout, nullptr, _IOFBF, 1 << 20);
	installCrashHandler();
	g_prevFilter = SetUnhandledExceptionFilter(nullCallFilter);
	newHeap();
	JMANewSinTable(14);
	fprintf(stderr, "sizeof JPAParticle %zu JPABaseEmitter %zu JPABaseField %zu JPADraw %zu\n", sizeof(JPAParticle),
	        sizeof(JPABaseEmitter), sizeof(JPABaseField), sizeof(JPADraw));

	bool calc = true;
	std::string extractDir;
	std::vector<const char*> archives;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--no-calc"))
			calc = false;
		else if (!strcmp(argv[i], "--extract") && i + 1 < argc)
			extractDir = argv[++i];
		else
			archives.push_back(argv[i]);
	}
	if (archives.empty()) {
		fprintf(stderr, "usage: jpa_test [--no-calc] <archive>...   |   jpa_test --extract <dir> <archive>...\n");
		return 2;
	}

	if (!extractDir.empty()) {
		// the host Yaz0/RARC path as the source of the files, for archives too large for the Python decoder
		for (const char* ap : archives) {
			std::vector<u8> in = readFile(ap);
			std::vector<u8> out;
			JKRCompression c = JKRDecomp::checkCompressed(in.data());
			if (c == JKR_COMPRESSION_NONE) {
				out = in;
			} else {
				u32 n = JKRDecompExpandSize(in.data());
				out.assign(n + 32, 0);
				JKRDecomp::decodeSZS(in.data(), out.data(), n, 0);
				out.resize(n);
			}
			JKRMemArchive* a = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
			std::vector<JpaFile> files;
			collect(a, 0, "", files);
			std::filesystem::path dir = std::filesystem::path(extractDir) / std::filesystem::path(ap).filename();
			for (const JpaFile& f : files) {
				std::filesystem::path p = dir / f.name.substr(1);
				std::filesystem::create_directories(p.parent_path());
				FILE* fp = fopen(p.string().c_str(), "wb");
				if (fp) {
					fwrite(f.data, 1, f.size, fp);
					fclose(fp);
				}
			}
			fprintf(stderr, "%s: %zu .jpa\n", ap, files.size());
		}
		return 0;
	}

	for (const char* ap : archives)
		runArchive(ap, calc);
	fflush(stdout);
	fprintf(stderr, "jpa_test: %ld resources, %ld textures, %ld emitters, %ld fields, %ld key anims; calc %ld emitters (%ld peak particles), "
	                "%ld emitters need __fres for the direction (JPAVecToRotaMtx), %ld for field fades (calcFieldFadeScale); %d failures\n",
	        g_files, g_tex, g_emitters, g_fields, g_kfa, g_calc, g_particles, g_needFres, g_fadeFres, g_fail);
	return g_fail ? 1 : 0;
}
