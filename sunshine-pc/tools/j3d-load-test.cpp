// Native end-to-end check of the J3D loaders on the host: the real J3DModelLoaderDataBase::load (blocks, factories,
// material creation, shape VCD/VAT display lists) and J3DAnmLoaderDataBase::load run on real BMD/BCK files from the disc,
// against sms_game.lib, and the result is compared with the raw big-endian file bytes parsed independently here.
// Build/run: tools\j3d-load-test.cmd <private build name>   (needs build/j3d-corpus from tools/j3d-corpus.py)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>
#include <new>
#include <windows.h>
#include <aurora/aurora.h>

// Standalone OS address checks need configuration even without an initialized Aurora renderer.
namespace aurora {
AuroraConfig g_config{};
}

#define protected public
#define private public
#include <JSystem/JKernel/JKRExpHeap.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp> // before J3DAnimation.hpp: J3DTransformInfo is a struct (MSVC mangles class/struct)
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#undef private
#undef protected
#include <hostendian.h>

void installCrashHandler(); // sunshine-pc/src/crash.cpp: symbolized stack on a fault (an unresolved decomp symbol faults when called)
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

static int g_fail = 0;
static std::string g_file;
#define CHECK(c, ...)                                                                                                  \
	do {                                                                                                               \
		if (!(c)) {                                                                                                    \
			if (g_fail++ < 40) {                                                                                       \
				printf("FAIL %s: ", g_file.c_str());                                                                   \
				printf(__VA_ARGS__);                                                                                   \
				printf("\n");                                                                                          \
			}                                                                                                          \
		}                                                                                                              \
	} while (0)

// raw (big-endian) readers over the pristine copy
static u32 B32(const u8* p) { return rdbe32(p); }
static u32 B16(const u8* p) { return rdbe16(p); }
static float BF(const u8* p)
{
	u32 v = rdbe32(p);
	float f;
	memcpy(&f, &v, 4);
	return f;
}

static const u8* findBlock(const std::vector<u8>& d, u32 type)
{
	u32 n = B32(d.data() + 0xC), off = 0x20;
	for (u32 i = 0; i < n && off + 8 <= d.size(); i++) {
		if (B32(d.data() + off) == type)
			return d.data() + off;
		off += B32(d.data() + off + 4);
	}
	return nullptr;
}

static std::vector<u8> readFile(const std::filesystem::path& f)
{
	FILE* fp = fopen(f.string().c_str(), "rb");
	std::vector<u8> v;
	if (!fp)
		return v;
	fseek(fp, 0, SEEK_END);
	long n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	v.resize(n);
	if (n)
		fread(v.data(), 1, n, fp);
	fclose(fp);
	return v;
}

static JKRHeap* g_heap;
// A root heap over a plain block (JKRExpHeap::createRoot would take the console arena, which does not exist here).
static void newHeap()
{
	const u32 size = 256u << 20;
	u8* mem = (u8*)_aligned_malloc(size, 32);
	const u32 head = (sizeof(JKRExpHeap) + 15) & ~15u;
	JKRExpHeap* heap = new (mem) JKRExpHeap(mem + head, size - head, nullptr, false);
	JKRHeap::sRootHeap = heap;
	heap->mIsRoot = true;
	g_heap = heap;
}

static long g_vcdArrays, g_materials, g_shapes, g_joints;

// The VCD/VAT display list of a shape: CP register writes (0x08), XF writes (0x10) and the Aurora array commands
// (0x50, sub 0x10+idx, u64 address, u32 size, u8 little-endian), then zero padding.
static void checkVcdVat(J3DShape* sh, J3DModelData* md, const std::vector<u8>& raw, u32 vtxBlockOff)
{
	const u8* dl = (const u8*)sh->getVcdVatCmd();
	u32 q = 0;
	bool seen[12] = {};
	while (q < J3DShape::kVcdVatDLSize && dl[q] != 0) {
		u8 op = dl[q];
		if (op == 0x08) {
			q += 6;
		} else if (op == 0x10) {
			u32 len = B16(dl + q + 1) + 1;
			q += 5 + len * 4;
		} else if (op == 0x50) {
			u32 sub = B16(dl + q + 1);
			if (sub >= 0x10 && sub < 0x20) {
				u32 idx = sub - 0x10;
				u64 hi = B32(dl + q + 3), lo = B32(dl + q + 7);
				void* ptr = (void*)((hi << 32) | lo);
				u32 size = B32(dl + q + 11);
				u8 le = dl[q + 15];
				CHECK(idx < 12, "array index %u", idx);
				if (idx < 12) {
					seen[idx] = true;
					g_vcdArrays++;
					J3DVertexData& vd = md->getVertexData();
					void* want = nullptr;
					u32 wsize = 0;
					if (idx == 0) want = vd.getVtxPosArray(), wsize = vd.getVtxPosSize();
					else if (idx == 1) want = vd.getVtxNormArray(), wsize = vd.getVtxNormSize();
					else if (idx <= 3) want = vd.getVtxColorArray(idx - 2), wsize = vd.getVtxColorSize(idx - 2);
					else want = vd.getVtxTexCoordArray(idx - 4), wsize = vd.getVtxTexCoordSize(idx - 4);
					if (!(idx == 1 && sh->mHasNBT)) {
						CHECK(ptr == want, "array %u pointer %p != vertex data %p", idx, ptr, want);
						CHECK(size == wsize && size != 0, "array %u size %u != %u", idx, size, wsize);
					}
					CHECK(le == 1, "array %u endianness flag %u", idx, le);
				}
			}
			q += 16 + 6; // aurora command + the stride register write that follows it
		} else {
			CHECK(false, "unknown VCD/VAT command %02x at %u", op, q);
			return;
		}
	}
	CHECK(q < J3DShape::kVcdVatDLSize - 1, "VCD/VAT list does not fit with padding");
	for (u32 i = q; i < J3DShape::kVcdVatDLSize; i++)
		CHECK(dl[i] == 0, "VCD/VAT padding");
	// every attribute the shape fetches through an index has an array command
	for (GXVtxDescList* d = sh->getVtxDesc(); d->attr != GX_VA_NULL; ++d) {
		if (d->type != GX_INDEX8 && d->type != GX_INDEX16)
			continue;
		int idx = d->attr == GX_VA_NBT ? 1 : (int)d->attr - GX_VA_POS;
		CHECK(idx >= 0 && idx < 12 && seen[idx], "attr %d indexed but no array command", (int)d->attr);
	}
}

static void checkModel(const std::vector<u8>& raw, J3DModelData* md)
{
	const u8* inf = findBlock(raw, 'INF1');
	const u8* vtx = findBlock(raw, 'VTX1');
	const u8* jnt = findBlock(raw, 'JNT1');
	const u8* shp = findBlock(raw, 'SHP1');
	const u8* mat = findBlock(raw, 'MAT3');
	const u8* tex = findBlock(raw, 'TEX1');
	const u8* drw = findBlock(raw, 'DRW1');
	CHECK(md->getVtxNum() == B32(inf + 0x10), "vtx num %u vs %u", md->getVtxNum(), B32(inf + 0x10));
	CHECK(md->getJointNum() == B16(jnt + 8), "joint num");
	CHECK(md->getShapeNum() == B16(shp + 8), "shape num");
	CHECK(md->getMaterialNum() == B16(mat + 8), "material num");
	CHECK(md->getDrawMtxNum() == B16(drw + 8), "draw mtx num");
	CHECK(md->getTexture() != nullptr && md->getTexture()->getNum() == B16(tex + 8), "texture num");
	CHECK(md->getRootNode() != nullptr, "no root joint (hierarchy not built)");

	// joints: transform info through the index table
	u32 jIdx = B32(jnt + 0x10), jInit = B32(jnt + 0xC);
	for (u16 i = 0; i < md->getJointNum(); i++) {
		const u8* r = jnt + jInit + B16(jnt + jIdx + i * 2) * 0x40;
		J3DJoint* j = md->getJointNodePointer(i);
		CHECK(j && j->mTransformInfo.mScale.x == BF(r + 4) && j->mTransformInfo.mTranslate.z == BF(r + 0x20)
		          && j->mTransformInfo.mRotation.y == (s16)B16(r + 0x12) && j->mRadius == BF(r + 0x24) && j->mMax.z == BF(r + 0x3C),
		      "joint %u transform", i);
		g_joints++;
	}

	// shapes: display list bytes are the file bytes, matrices and the VCD/VAT list are built
	u32 sIdx = B32(shp + 0x10), sInit = B32(shp + 0xC), sDl = B32(shp + 0x20), sDraw = B32(shp + 0x28);
	for (u16 i = 0; i < md->getShapeNum(); i++) {
		J3DShape* sh = md->getShapeNodePointer(i);
		CHECK(sh != nullptr, "shape %u missing", i);
		if (!sh)
			continue;
		const u8* r = shp + sInit + B16(shp + sIdx + i * 2) * 0x28;
		CHECK(sh->getMtxGroupNum() == B16(r + 2) && sh->mRadius == BF(r + 0xC), "shape %u record", i);
		u32 drwIdx = B16(r + 8);
		for (u16 g = 0; g < sh->getMtxGroupNum(); g++) {
			const u8* di = shp + sDraw + (drwIdx + g) * 8;
			J3DShapeDraw* d = sh->getShapeDraw(g);
			CHECK(d->getDisplayListSize() == B32(di), "DL size");
			CHECK(memcmp(d->getDisplayList(), raw.data() + (shp - raw.data()) + sDl + B32(di + 4), B32(di)) == 0, "DL bytes differ");
		}
		checkVcdVat(sh, md, raw, (u32)(vtx - raw.data()));
		g_shapes++;
	}

	// materials were created from the MAT3 tables
	u32 mInit = B32(mat + 0xC), mId = B32(mat + 0x10), mTexNo = B32(mat + 0x48), mStage = B32(mat + 0x58);
	for (u16 i = 0; i < md->getMaterialNum(); i++) {
		J3DMaterial* m = md->getMaterialNodePointer(i);
		CHECK(m != nullptr, "material %u missing", i);
		if (!m)
			continue;
		const u8* r = mat + mInit + B16(mat + mId + i * 2) * 0x14C;
		u8 sNo = r[4];
		if (sNo != 0xFF)
			CHECK(m->getTevBlock()->getTevStageNum() == mat[mStage + sNo], "material %u tev stage num", i);
		u16 t = B16(r + 0x84);
		if (t != 0xFFFF)
			CHECK(m->getTexNo(0) == B16(mat + mTexNo + t * 2), "material %u texno", i);
		CHECK(m->mMaterialMode == r[0], "material %u mode", i);
		g_materials++;
	}
}

// BCK: at the first key time of a track the animation must return the first key value
static long g_tracks;
static void checkBck(const std::vector<u8>& raw, J3DAnmBase* a)
{
	J3DAnmTransformKey* k = (J3DAnmTransformKey*)a;
	const u8* b = raw.data() + 0x20;
	CHECK(a->getFrameMax() == (s16)B16(b + 0xA), "frame max");
	u32 joints = B16(b + 0xC), tab = B32(b + 0x14), scl = B32(b + 0x18), rot = B32(b + 0x1C), trn = B32(b + 0x20);
	int shift = b[9];
	for (u32 j = 0; j < joints && j < 64; j++) {
		J3DTransformInfo out;
		k->calcTransform(0.0f, (u16)j, &out);
		for (int axis = 0; axis < 3; axis++) {
			const u8* t = b + tab + (j * 3 + axis) * 18;
			for (int comp = 0; comp < 3; comp++) { // scale, rotation, translation
				const u8* e = t + comp * 6;
				u32 count = B16(e), off = B16(e + 2);
				if (count == 0)
					continue;
				if (comp == 1) { // rotation: s16 keys, shifted by the decimal shift
					s16 want;
					if (count == 1)
						want = (s16)B16(b + rot + off * 2);
					else if ((s16)B16(b + rot + off * 2) != 0)
						continue; // first key is not at frame 0
					else
						want = (s16)B16(b + rot + (off + 1) * 2);
					int got = axis == 0 ? out.mRotation.x : axis == 1 ? out.mRotation.y : out.mRotation.z;
					CHECK(got == (s16)((int)want << shift), "rot j%u axis%d: %d vs %d", j, axis, got, (int)(s16)((int)want << shift));
					g_tracks++;
					continue;
				}
				const u8* arr = b + (comp == 0 ? scl : trn);
				float want;
				if (count == 1) {
					want = BF(arr + off * 4);
				} else {
					if (BF(arr + off * 4) != 0.0f)
						continue; // first key is not at frame 0
					want = BF(arr + (off + 1) * 4);
				}
				float got = comp == 0 ? (axis == 0 ? out.mScale.x : axis == 1 ? out.mScale.y : out.mScale.z)
				                      : (axis == 0 ? out.mTranslate.x : axis == 1 ? out.mTranslate.y : out.mTranslate.z);
				CHECK(got == want, "%s j%u axis%d: %f vs %f", comp == 0 ? "scale" : "trans", j, axis, got, want);
				g_tracks++;
			}
		}
	}
	// the whole animation evaluates to finite values at several frames
	for (float f : { 0.5f, 3.25f, 10.f, 1e6f })
		for (u32 j = 0; j < joints && j < 8; j++) {
			J3DTransformInfo out;
			k->calcTransform(f, (u16)j, &out);
			CHECK(std::isfinite(out.mScale.x) && std::isfinite(out.mTranslate.y) && std::isfinite(out.mTranslate.z), "frame %f joint %u non-finite", f, j);
		}
}

static void checkWeightedEnvelopeTransform()
{
	g_file = "weighted envelope transform";
	J3DModelData data;
	u8 mixCounts[] = { 2 };
	u16 indices[] = { 0, 1 };
	f32 weights[] = { 0.25f, 0.75f };
	Mtx inverse[] = {
		{ { 1, 0, 0, -2 }, { 0, 1, 0, 0 }, { 0, 0, 1, 1 } },
		{ { 1, 0, 0, 1 }, { 0, 1, 0, -2 }, { 0, 0, 1, 3 } },
	};
	Mtx nodes[] = {
		{ { 1, 0, 0, 10 }, { 0, 1, 0, 20 }, { 0, 0, 1, 30 } },
		{ { 2, 0, 0, -6 }, { 0, 3, 0, 8 }, { 0, 0, 4, 2 } },
	};
	data.mJointNum = 2;
	data.mWEvlpMtxNum = 1;
	data.unk88 = mixCounts;
	data.unk8C = indices;
	data.unk90 = weights;
	data.unk94 = inverse;

	Mtx envelope;
	for (auto& row : envelope)
		for (f32& value : row)
			value = -17.0f; // The method must replace every coefficient, not depend on zeroed allocation.
	u8 scaleFlags[] = { 1, 0 };
	u8 envelopeScale[] = { 0xA5 };
	J3DModel model;
	model.mModelData = &data;
	model.mScaleFlagArr = scaleFlags;
	model.mEvlpScaleFlagArr = envelopeScale;
	model.mNodeMatrices = nodes;
	model.mWeightEvlpMatrices = &envelope;
	model.calcWeightEnvelopeMtx();

	const Mtx expected = { { 1.75f, 0, 0, -1 }, { 0, 2.5f, 0, 6.5f }, { 0, 0, 3.25f, 18.25f } };
	for (int row = 0; row < 3; ++row)
		for (int column = 0; column < 4; ++column)
			CHECK(envelope[row][column] == expected[row][column], "matrix[%d][%d] %g != %g",
			      row, column, envelope[row][column], expected[row][column]);
	CHECK(envelopeScale[0] == 0, "mixed scale flag %u != 0", envelopeScale[0]);

	Mtx view = { { 1, 0, 0, 5 }, { 0, 1, 0, -7 }, { 0, 0, 1, 9 } };
	Mtx draw;
	J3DPSMtxArrayConcat(view, model.getWeightAnmMtx(0), draw, 1);
	Vec point = { 2, -4, 1 }, visible;
	MTXMultVec(draw, &point, &visible);
	CHECK(visible.x == 7.5f && visible.y == -10.5f && visible.z == 30.5f,
	      "consumer point (%g,%g,%g) != (7.5,-10.5,30.5)", visible.x, visible.y, visible.z);
	printf("weighted envelope: consumer point (%g,%g,%g)\n", visible.x, visible.y, visible.z);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("usage: j3d-load-test <corpus dir> [max models] [max bck]\n");
		return 2;
	}
	setvbuf(stdout, nullptr, _IONBF, 0);
	installCrashHandler();
	g_prevFilter = SetUnhandledExceptionFilter(nullCallFilter);
	int maxModels = argc > 2 ? atoi(argv[2]) : 100000;
	int maxBck = argc > 3 ? atoi(argv[3]) : 100000;
	int models = 0, bcks = 0, btks = 0, brks = 0, btps = 0, bpks = 0, tables = 0;
	newHeap();
	checkWeightedEnvelopeTransform();
	for (auto& e : std::filesystem::directory_iterator(argv[1])) {
		std::string ext = e.path().extension().string();
		bool isModel = ext == ".bmd", isBck = ext == ".bck";
		if (!isModel && !isBck && ext != ".btk" && ext != ".brk" && ext != ".btp" && ext != ".bpk" && ext != ".bmt")
			continue;
		std::vector<u8> pristine = readFile(e.path());
		if (pristine.size() < 0x20)
			continue;
		g_file = e.path().filename().string();
		if (getenv("J3D_TRACE"))
			printf("> %s\n", g_file.c_str());
		// the game hands the loaders a 32-byte aligned buffer it owns
		u8* buf = (u8*)_aligned_malloc((pristine.size() + 63) & ~31ull, 32);
		memcpy(buf, pristine.data(), pristine.size());
		if (isModel) {
			if (models >= maxModels)
				continue;
			if (models % 40 == 39)
				newHeap();
			J3DModelData* md = J3DModelLoaderDataBase::load(buf, (models & 1) ? 0x11240000 : 0x10240000);
			CHECK(md != nullptr, "load failed");
			if (md)
				checkModel(pristine, md);
			// a second load of the same (already converted) buffer must give the same result
			J3DModelData* md2 = J3DModelLoaderDataBase::load(buf, 0x11240000);
			CHECK(md2 != nullptr && md2->getJointNum() == md->getJointNum() && md2->getVtxNum() == md->getVtxNum(), "reload differs");
			models++;
		} else if (ext == ".bmt") {
			J3DMaterialTable* t = J3DModelLoaderDataBase::loadMaterialTable(buf);
			const u8* m3 = findBlock(pristine, 'MAT3');
			CHECK(t != nullptr && (!m3 || t->mMaterialNum == B16(m3 + 8)), "bmt material num");
			tables++;
		} else {
			if (isBck && bcks >= maxBck)
				continue;
			J3DAnmBase* a = J3DAnmLoaderDataBase::load(buf);
			CHECK(a != nullptr, "anim load failed");
			if (a && isBck) {
				checkBck(pristine, a);
				bcks++;
			}
			if (a && !isBck)
				(ext == ".btk" ? btks : ext == ".brk" ? brks : ext == ".btp" ? btps : bpks)++;
			J3DAnmBase* a2 = J3DAnmLoaderDataBase::load(buf); // reload of the converted buffer
			CHECK(a2 != nullptr && a2->getFrameMax() == a->getFrameMax(), "anim reload differs");
		}
	}
	printf("models %d (joints %ld shapes %ld materials %ld vcd arrays %ld) tables %d | bck %d (%ld tracks) btk %d brk %d btp %d bpk %d\n", models,
	       g_joints, g_shapes, g_materials, g_vcdArrays, tables, bcks, g_tracks, btks, brks, btps, bpks);
	printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
	return g_fail ? 1 : 0;
}
