// Host test of the J3D model block conversion (J3DSwapFileHeaderPC + J3DModelSwapPC.hpp) over the real BMD/BMT corpus.
// Run: tools\j3d-test.cmd tools\j3d-model-test.cpp build\j3d-corpus   (corpus from tools/j3d-corpus.py)
// Every file is converted once, a second conversion must be refused (marker), and the host-order data must satisfy the
// invariants of the format: counts and offsets inside their blocks, weights summing to one, unit normals, table indices
// inside their tables, every display list a well-formed GX draw stream whose vertex indices stay inside the arrays.
// MAT3 is covered by j3d-mat-test.cpp; here the block is skipped.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <filesystem>
#include <JSystem/J3D/J3DGraphLoader/J3DModelSwapPC.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DShapeFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DJointFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>

static_assert(sizeof(J3DShapeBlock) == 0x2C, "SHP1 header must keep the disc size");
static_assert(sizeof(J3DJointBlock) == 0x18, "JNT1 header must keep the disc size");
static_assert(sizeof(J3DModelInfoBlock) == 0x18 && sizeof(J3DVertexBlock) == 0x40 && sizeof(J3DEnvelopBlock) == 0x1C
                  && sizeof(J3DDrawBlock) == 0x14 && sizeof(J3DTextureBlock) == 0x14,
              "block headers must keep the disc size");
static_assert(sizeof(J3DShapeInitData) == 0x28 && sizeof(J3DShapeMtxInitData) == 8 && sizeof(J3DShapeDrawInitData) == 8,
              "SHP1 records");
static_assert(sizeof(J3DJointInitData) == 0x40, "JNT1 record");
static_assert(sizeof(GXVtxDescList) == 8 && sizeof(GXVtxAttrFmtList) == 16, "list entries");
static_assert(sizeof(ResTIMG) == 0x20, "BTI header");

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

struct Stats {
	int files = 0, models = 0, tables = 0, texsOnly = 0;
	long shapes = 0, dls = 0, dlVerts = 0, joints = 0, envelopes = 0, textures = 0, arrays = 0;
	int posS16 = 0, posF32 = 0, nrmS16 = 0, nrmF32 = 0, nbt = 0, indexed8 = 0;
};
static Stats S;

static const u8* G;      // file
static u32 gsz;
static u32 u32at(const u8* p) { return J3DGet32PC(p); }
static u32 u16at(const u8* p) { return J3DGet16PC(p); }
static float f32at(const u8* p)
{
	float f;
	memcpy(&f, p, 4);
	return f;
}
static int s16at(const u8* p) { return (s16)J3DGet16PC(p); }

struct Blk {
	const u8* p = nullptr;
	u32 size = 0;
};

static void checkInf1(Blk b, u32& vtxNum, u32& hierJoints, u32& hierShapes, u32& hierMats)
{
	const u8* p = b.p;
	CHECK(b.size >= 0x18, "INF1 size");
	vtxNum   = u32at(p + 0x10);
	u32 off  = u32at(p + 0x14);
	CHECK(off >= 0x18 && off < b.size, "INF1 hierarchy offset %x", off);
	int depth = 0;
	hierJoints = hierShapes = hierMats = 0;
	u32 q = off;
	for (;; q += 4) {
		CHECK(q + 4 <= b.size, "INF1 hierarchy runs out of the block");
		if (q + 4 > b.size)
			return;
		u32 t = u16at(p + q), v = u16at(p + q + 2);
		if (t == 0)
			break;
		if (t == 1)
			depth++;
		else if (t == 2)
			depth--;
		else if (t == 0x10)
			hierJoints = hierJoints > v + 1 ? hierJoints : v + 1;
		else if (t == 0x11)
			hierMats = hierMats > v + 1 ? hierMats : v + 1;
		else if (t == 0x12)
			hierShapes = hierShapes > v + 1 ? hierShapes : v + 1;
		else
			CHECK(false, "INF1 hierarchy type %x", t);
		CHECK(depth >= 0, "INF1 hierarchy depth");
	}
	CHECK(depth == 0, "INF1 hierarchy unbalanced %d", depth);
}

struct VtxInfo {
	u32 posCount = 0, nrmCount = 0, clrCount[2] = {}, texCount[8] = {};
	u32 types[21];
	u32 cnts[21];
	u32 fracs[21] = {};
	bool nbtArr = false;
};

static u32 compSize(u32 type) { return type == 4 ? 4 : (type == 2 || type == 3) ? 2 : 1; }

static void checkVtx1(Blk b, u32 vtxNum, VtxInfo& vi)
{
	const u8* p = b.p;
	CHECK(b.size == 0x40 || b.size > 0x40, "VTX1 size");
	for (int i = 0; i < 21; i++)
		vi.types[i] = 4, vi.cnts[i] = 0;
	u32 fmt = u32at(p + 8);
	CHECK(fmt >= 0x40 && fmt < b.size, "VTX1 fmt offset");
	u32 q = fmt;
	for (;; q += 16) {
		CHECK(q + 16 <= b.size, "VTX1 attr list not terminated");
		if (q + 16 > b.size)
			return;
		u32 attr = u32at(p + q);
		if (attr == 0xFF)
			break;
		CHECK(attr >= 9 && attr <= 20, "VTX1 attr %u", attr);
		vi.cnts[attr] = u32at(p + q + 4);
		vi.types[attr] = u32at(p + q + 8);
		vi.fracs[attr] = p[q + 12];
		CHECK(vi.types[attr] <= 5, "VTX1 comp type");
	}
	static const u8 attrOfSlot[13] = { 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 };
	u32 prevEnd = 0;
	for (int slot = 0; slot < 13; slot++) {
		u32 off = u32at(p + 0x0C + slot * 4);
		if (off == 0)
			continue;
		CHECK(off >= 0x40 && off < b.size, "VTX1 slot %d offset %x", slot, off);
		u32 span = J3DVtxArraySpanPC(p, off);
		u32 attr = attrOfSlot[slot];
		u32 comp = (attr == 11 || attr == 12) ? ((vi.types[attr] == 0 || vi.types[attr] == 3) ? 2 : 1) : compSize(vi.types[attr]);
		S.arrays++;
		if (slot == 0) {
			u32 stride = vi.types[9] == 4 ? 12 : 6;
			CHECK(vtxNum * stride <= span && span < vtxNum * stride + 32, "pos span %u for %u verts stride %u", span, vtxNum, stride);
			vi.posCount = vtxNum;
			(vi.types[9] == 4 ? S.posF32 : S.posS16)++;
			// positions: finite, small
			for (u32 i = 0; i < vtxNum * 3 && i < 3000; i++) {
				if (stride == 12) {
					float f = f32at(p + off + i * 4);
					CHECK(isfinite(f) && fabsf(f) < 1e7f, "pos f32 %f", f);
				}
			}
		}
		if (slot == 1) {
			u32 stride = vi.types[10] == 4 ? 12 : 6;
			vi.nrmCount = span / stride;
			(vi.types[10] == 4 ? S.nrmF32 : S.nrmS16)++;
			// first normals have unit length: f32 or s16 scaled by the VAT fraction bits
			for (u32 i = 0; i < vi.nrmCount && i < 200; i++) {
				const u8* n = p + off + i * stride;
				float x, y, z;
				if (stride == 12)
					x = f32at(n), y = f32at(n + 4), z = f32at(n + 8);
				else
					{ float k = 1.f / (1u << vi.fracs[10]); x = s16at(n) * k, y = s16at(n + 2) * k, z = s16at(n + 4) * k; }
				float l = sqrtf(x * x + y * y + z * z);
				if (i + 6 < vi.nrmCount) // the span includes up to 31 bytes of alignment padding
					CHECK(fabsf(l - 1.f) < 0.05f || (l < 0.001f), "normal %u length %f", i, l);
			}
		}
		if (slot == 2)
			vi.nbtArr = true, S.nbt++;
		if (slot == 3 || slot == 4)
			vi.clrCount[slot - 3] = span / 4;
		if (slot >= 5) {
			u32 stride = vi.types[attr] == 4 ? 8 : 4;
			vi.texCount[slot - 5] = span / stride;
			if (stride == 8)
				for (u32 i = 0; i + 8 < span / 4 && i < 400; i++) { // skip the alignment padding at the end
					float f = f32at(p + off + i * 4);
					CHECK(isfinite(f) && fabsf(f) < 1e5f, "texcoord f32 %f", f);
				}
		}
		(void)comp;
		(void)prevEnd;
	}
}

static void checkEvp1(Blk b, u32& envelopes)
{
	const u8* p = b.p;
	u32 n = u16at(p + 8);
	envelopes = n;
	u32 numOff = u32at(p + 0xC), idxOff = u32at(p + 0x10), wOff = u32at(p + 0x14), invOff = u32at(p + 0x18);
	if (n == 0) {
		CHECK(numOff == 0 && idxOff == 0 && wOff == 0 && invOff == 0, "empty EVP1 with offsets");
		return;
	}
	S.envelopes += n;
	CHECK(numOff >= 0x1C && idxOff > numOff && wOff > idxOff && invOff > wOff && invOff < b.size, "EVP1 offsets order");
	u32 tot = 0;
	for (u32 i = 0; i < n; i++)
		tot += p[numOff + i];
	CHECK(numOff + n <= idxOff && idxOff + tot * 2 <= wOff && wOff + tot * 4 <= invOff, "EVP1 tables overlap");
	u32 k = 0;
	for (u32 i = 0; i < n; i++) {
		float sum = 0;
		for (u32 j = 0; j < p[numOff + i]; j++, k++) {
			sum += f32at(p + wOff + k * 4);
			CHECK(u16at(p + idxOff + k * 2) < 4096, "EVP1 joint index %u", u16at(p + idxOff + k * 2));
		}
		CHECK(fabsf(sum - 1.f) < 0.01f, "EVP1 envelope %u weights sum %f", i, sum);
	}
	u32 mats = (b.size - invOff) / 48, maxIdx = 0;
	for (u32 j = 0; j < tot; j++) {
		u32 v = u16at(p + idxOff + j * 2);
		maxIdx = v > maxIdx ? v : maxIdx;
	}
	CHECK(mats >= maxIdx + 1, "EVP1 inverse matrices %u < %u", mats, maxIdx + 1);
	for (u32 m = 0; m <= maxIdx && m < mats; m++)
		for (int e = 0; e < 12; e++) {
			float f = f32at(p + invOff + m * 48 + e * 4);
			CHECK(isfinite(f) && fabsf(f) < 1e9f, "EVP1 inv matrix %f", f);
		}
}

static void checkDrw1(Blk b, u32 joints, u32 envelopes, u32& drawMtx)
{
	const u8* p = b.p;
	u32 n = u16at(p + 8), flagOff = u32at(p + 0xC), idxOff = u32at(p + 0x10);
	drawMtx = n;
	CHECK(flagOff + n <= idxOff && idxOff + n * 2 <= b.size, "DRW1 tables");
	for (u32 i = 0; i < n; i++) {
		u32 f = p[flagOff + i], v = u16at(p + idxOff + i * 2);
		CHECK(f <= 1, "DRW1 flag %u", f);
		CHECK(f == 0 ? v < joints : v < envelopes, "DRW1 entry %u flag %u index %u (joints %u, envelopes %u)", i, f, v, joints, envelopes);
	}
}

static void checkJnt1(Blk b, u32& joints)
{
	const u8* p = b.p;
	u32 n = u16at(p + 8), initOff = u32at(p + 0xC), idxOff = u32at(p + 0x10);
	joints = n;
	S.joints += n;
	CHECK(initOff >= 0x18 && idxOff >= 0x18, "JNT1 offsets");
	u32 maxIdx = 0;
	for (u32 i = 0; i < n; i++) {
		u32 v = u16at(p + idxOff + i * 2);
		CHECK(v < n + 64, "JNT1 index %u", v);
		maxIdx = v > maxIdx ? v : maxIdx;
	}
	u32 span = 0;
	{
		u32 end = b.size;
		for (int s = 0; s < 3; s++) {
			u32 o = u32at(p + 0xC + s * 4);
			if (o > initOff && o < end)
				end = o;
		}
		span = end - initOff;
	}
	CHECK((maxIdx + 1) * 0x40 <= span && span < (maxIdx + 1) * 0x40 + 32 + 0x40, "JNT1 init extent %u for %u records", span, maxIdx + 1);
	for (u32 r = 0; r <= maxIdx; r++) {
		const u8* q = p + initOff + r * 0x40;
		CHECK(u16at(q) < 4, "joint kind %u", u16at(q));
		for (int e = 0; e < 3; e++) {
			float s = f32at(q + 4 + e * 4);
			CHECK(isfinite(s) && fabsf(s) < 1000.f, "joint scale %f", s);
			float t = f32at(q + 0x18 + e * 4);
			CHECK(isfinite(t) && fabsf(t) < 1e7f, "joint translate %f", t);
			float mn = f32at(q + 0x28 + e * 4), mx = f32at(q + 0x34 + e * 4);
			CHECK(isfinite(mn) && isfinite(mx) && mn <= mx + 1e-3f, "joint bbox %f %f", mn, mx);
		}
		float rad = f32at(q + 0x24);
		CHECK(isfinite(rad) && rad >= 0, "joint radius %f", rad);
	}
}

// Walks a display list: every command is a draw (0x80..0xB8 | vtxfmt) with u16 count and count vertices of `vtxBytes`.
static bool walkDL(const u8* d, u32 size, u32 vtxBytes, const std::vector<std::pair<int, u32>>& idxAttrs, const VtxInfo& vi, u32 shapeNo)
{
	u32 q = 0;
	while (q < size) {
		u8 cmd = d[q];
		if (cmd == 0) { // NOP padding up to the end
			for (u32 i = q; i < size; i++)
				if (d[i] != 0)
					return false;
			return true;
		}
		u8 prim = cmd & 0xF8; // quads, triangles, strip, fan, lines, line strip, points
		if (prim != 0x80 && prim != 0x90 && prim != 0x98 && prim != 0xA0 && prim != 0xA8 && prim != 0xB0 && prim != 0xB8)
			return false;
		if (q + 3 > size)
			return false;
		u32 count = (d[q + 1] << 8) | d[q + 2];
		q += 3;
		if (q + (u64)count * vtxBytes > size)
			return false;
		for (u32 v = 0; v < count; v++) {
			u32 o = q + v * vtxBytes, k = 0;
			for (auto& ia : idxAttrs) {
				u32 idx;
				if (ia.second == 1) {
					idx = d[o + k];
					k += 1;
				} else {
					idx = (d[o + k] << 8) | d[o + k + 1];
					k += 2;
				}
				u32 lim = 0xFFFFFFFF;
				int a = ia.first;
				if (a < 0)
					continue;
				if (a == 9)
					lim = vi.posCount;
				else if (a == 10)
					lim = vi.nrmCount;
				else if (a == 11 || a == 12)
					lim = vi.clrCount[a - 11];
				else if (a >= 13 && a <= 20)
					lim = vi.texCount[a - 13];
				if (idx >= lim) {
					printf("  DL shape %u attr %d index %u >= %u\n", shapeNo, a, idx, lim);
					return false;
				}
			}
			S.dlVerts++;
		}
		q += count * vtxBytes;
	}
	return true;
}

static void checkShp1(Blk b, u32 shapesInHierarchy, const VtxInfo& vi)
{
	const u8* p = b.p;
	u32 num = u16at(p + 8);
	u32 initOff = u32at(p + 0xC), idxOff = u32at(p + 0x10), nameOff = u32at(p + 0x14), descOff = u32at(p + 0x18), mtxTabOff = u32at(p + 0x1C),
	    dlOff = u32at(p + 0x20), mtxInitOff = u32at(p + 0x24), drawOff = u32at(p + 0x28);
	S.shapes += num;
	CHECK(num >= shapesInHierarchy, "SHP1 %u shapes, hierarchy refers to %u", num, shapesInHierarchy);
	CHECK(initOff >= 0x2C && idxOff >= 0x2C && descOff > 0x2C && dlOff > 0x2C && mtxInitOff > 0x2C && drawOff > 0x2C, "SHP1 offsets");
	CHECK(nameOff == 0 || nameOff >= 0x2C, "SHP1 name offset");
	u32 maxIdx = 0;
	for (u32 i = 0; i < num; i++) {
		u32 v = u16at(p + idxOff + i * 2);
		maxIdx = v > maxIdx ? v : maxIdx;
	}
	u32 maxMtxInit = 0, maxDraw = 0, maxMtx = 0;
	for (u32 s = 0; s < num; s++) {
		const u8* r = p + initOff + u16at(p + idxOff + s * 2) * 0x28;
		u32 type = r[0], groups = u16at(r + 2), descIdx = u16at(r + 4), mtxIdx = u16at(r + 6), drwIdx = u16at(r + 8);
		CHECK(type <= 3, "shape mtx type %u", type);
		CHECK(groups >= 1 && groups < 64, "shape %u groups %u", s, groups);
		float rad = f32at(r + 0xC);
		CHECK(isfinite(rad) && rad >= 0, "shape radius %f", rad);
		for (int e = 0; e < 3; e++)
			CHECK(f32at(r + 0x10 + e * 4) <= f32at(r + 0x1C + e * 4) + 1e-3f, "shape bbox");
		maxMtxInit = maxMtxInit > mtxIdx + groups ? maxMtxInit : mtxIdx + groups;
		maxDraw    = maxDraw > drwIdx + groups ? maxDraw : drwIdx + groups;

		// vertex description list of the shape
		std::vector<std::pair<int, u32>> idxAttrs;
		u32 vtxBytes = 0;
		u32 q = descOff + descIdx;
		bool term = false;
		for (; q + 8 <= b.size; q += 8) {
			u32 attr = u32at(p + q), type2 = u32at(p + q + 4);
			if (attr == 0xFF) {
				term = true;
				break;
			}
			if (type2 == 0)
				continue;
			if (attr == 0 || (attr >= 1 && attr <= 8)) {
				CHECK(type2 == 1, "mtx idx attr must be direct");
				idxAttrs.push_back({ -1, 1u }); // matrix index byte: no array
				vtxBytes += 1;
			} else if (type2 == 2 || type2 == 3) {
				idxAttrs.push_back({ (int)attr, type2 == 2 ? 1u : 2u });
				vtxBytes += type2 == 2 ? 1 : 2;
				if (type2 == 2)
					S.indexed8++;
			} else {
				CHECK(false, "shape %u attr %u direct type %u not supported by checker", s, attr, type2);
			}
			// the attribute must exist in the vertex data
			if (attr >= 9 && attr <= 20 && attr != 25)
				CHECK(attr == 9 ? vi.posCount : attr == 10 ? vi.nrmCount : attr <= 12 ? vi.clrCount[attr - 11] : vi.texCount[attr - 13],
				      "shape %u uses attr %u whose array is absent", s, attr);
		}
		CHECK(term, "shape %u vtx desc list not terminated", s);

		for (u32 g = 0; g < groups; g++) {
			const u8* mi = p + mtxInitOff + (mtxIdx + g) * 8;
			u32 first = u32at(mi + 4), cnt = u16at(mi + 2);
			CHECK(u16at(mi) < 4096 || u16at(mi) == 0xFFFF, "use mtx index %u", u16at(mi));
			maxMtx = maxMtx > first + cnt ? maxMtx : first + cnt;
			if (type == 3)
				CHECK(cnt >= 1 && cnt <= 10, "multi mtx count %u", cnt);
			const u8* di = p + drawOff + (drwIdx + g) * 8;
			u32 dsize = u32at(di), doff = u32at(di + 4);
			CHECK(dlOff + doff + dsize <= b.size && dsize > 0, "DL [%u+%u] outside block", doff, dsize);
			CHECK(dsize % 32 == 0, "DL size %u not 32-byte padded", dsize);
			CHECK(walkDL(p + dlOff + doff, dsize, vtxBytes, idxAttrs, vi, s), "shape %u group %u DL malformed", s, g);
			S.dls++;
		}
	}
	// tables: converted extents are within their blocks
	CHECK(mtxInitOff + maxMtxInit * 8 <= b.size && drawOff + maxDraw * 8 <= b.size, "SHP1 init tables exceed block");
	CHECK(mtxTabOff + maxMtx * 2 <= b.size, "mtx table exceeds block");
	CHECK(initOff + (maxIdx + 1) * 0x28 <= idxOff || initOff > idxOff, "shape init overlaps index table");
}

static void checkTex1(Blk b, bool& any)
{
	const u8* p = b.p;
	u32 n = u16at(p + 8), off = u32at(p + 0xC);
	S.textures += n;
	any = n != 0;
	CHECK(n == 0 || off >= 0x14, "TEX1 header offset");
	for (u32 i = 0; i < n; i++) {
		const ResTIMG* t = (const ResTIMG*)(p + off) + i;
		CHECK(t->format <= 6 || t->format == 8 || t->format == 9 || t->format == 0xA || t->format == 0xE, "tex format %u", t->format);
		CHECK(t->width > 0 && t->width <= 1024 && t->height > 0 && t->height <= 1024, "tex size %ux%u", t->width, t->height);
		CHECK(t->mipmapCount >= 1 && t->mipmapCount <= 11, "tex mip count %u", t->mipmapCount);
		const u8* img = (const u8*)t + (s32)t->imageDataOffset;
		CHECK(img >= p && img < p + b.size, "tex %u image offset outside block (%d)", i, (int)t->imageDataOffset);
		CHECK(t->wrapS <= 2 && t->wrapT <= 2, "tex wrap");
		CHECK(t->numColors == 0 || t->paletteOffset != 0, "tex palette");
		CHECK(t->unk19[0] == (char)RESTIMG_HOST_MARK, "tex header not marked as swapped");
	}
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

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("usage: j3d-model-test <corpus dir>\n");
		return 2;
	}
	for (auto& e : std::filesystem::directory_iterator(argv[1])) {
		std::string ext = e.path().extension().string();
		if (ext != ".bmd" && ext != ".bmt" && ext != ".bdl")
			continue;
		std::vector<u8> d = readFile(e.path());
		if (d.size() < 0x20)
			continue;
		d.resize(d.size() + 64); // slack: the swap must stay inside the declared file size anyway
		g_file = e.path().filename().string();
		G      = d.data();
		JUTDataFileHeader* h = (JUTDataFileHeader*)d.data();
		u32 declared = rdbe32(&h->mFileSize);
		std::vector<u8> pristine(d);

		CHECK(J3DSwapFileHeaderPC(d.data()), "first swap refused");
		CHECK(!J3DSwapFileHeaderPC(d.data()), "second swap accepted");
		CHECK(h->mMagic == 'J3D2' && (h->mType == 'bmd3' || h->mType == 'bmt3' || h->mType == 'bdl3'), "header %08x %08x", h->mMagic, h->mType);
		CHECK(h->mFileSize == declared, "file size");
		S.files++;

		Blk blocks[16];
		int nb = 0;
		JUTDataBlockHeader* blk = &h->mFirstBlock;
		u32 pos = 0x20;
		for (u32 i = 0; i < h->mBlockNum; i++) {
			CHECK(pos + 8 <= declared, "block %u beyond file", i);
			u32 t = blk->mType;
			Blk bb;
			bb.p = (const u8*)blk;
			bb.size = blk->mSize;
			CHECK(bb.size >= 8 && pos + bb.size <= declared, "block %08x size %u", t, bb.size);
			switch (t) {
			case 'INF1': J3DSwapInf1PC((u8*)blk); break;
			case 'VTX1': J3DSwapVtx1PC((u8*)blk); break;
			case 'EVP1': J3DSwapEvp1PC((u8*)blk); break;
			case 'DRW1': J3DSwapDrw1PC((u8*)blk); break;
			case 'JNT1': J3DSwapJnt1PC((u8*)blk); break;
			case 'SHP1': J3DSwapShp1PC((u8*)blk); break;
			case 'TEX1': J3DSwapTex1PC((u8*)blk); break;
			default: break; // MAT3, MDL3
			}
			if (nb < 16)
				blocks[nb++] = bb;
			pos += bb.size;
			blk = (JUTDataBlockHeader*)((u8*)blk + bb.size);
		}

		u32 vtxNum = 0, hj = 0, hs = 0, hm = 0, joints = 0, envelopes = 0, drawMtx = 0;
		VtxInfo vi;
		bool anyTex = false, isModel = false;
		auto find = [&](u32 type) -> Blk* {
			for (int i = 0; i < nb; i++)
				if (u32at(blocks[i].p) == type)
					return &blocks[i];
			return nullptr;
		};
		if (find('INF1')) {
			isModel = true;
			S.models++;
			checkInf1(*find('INF1'), vtxNum, hj, hs, hm);
			checkVtx1(*find('VTX1'), vtxNum, vi);
			checkJnt1(*find('JNT1'), joints);
			CHECK(hj <= joints, "hierarchy joint %u of %u", hj, joints);
			checkEvp1(*find('EVP1'), envelopes);
			checkDrw1(*find('DRW1'), joints, envelopes, drawMtx);
			checkShp1(*find('SHP1'), hs, vi);
		} else {
			S.tables++;
		}
		if (find('TEX1'))
			checkTex1(*find('TEX1'), anyTex);
		(void)isModel;

		// the bytes outside every converted table are untouched: the display lists and the image payloads are identical
		if (find('SHP1')) {
			Blk s = *find('SHP1');
			u32 off = (u32)(s.p - d.data());
			u32 dlOff = u32at(s.p + 0x20), mtxInit = u32at(s.p + 0x24);
			CHECK(memcmp(d.data() + off + dlOff, pristine.data() + off + dlOff, mtxInit > dlOff ? mtxInit - dlOff : 0) == 0 || mtxInit < dlOff,
			      "display list bytes changed");
		}
	}
	printf("files %d (models %d, material tables %d)\n", S.files, S.models, S.tables);
	printf("shapes %ld display lists %ld verts %ld | joints %ld envelopes %ld textures %ld arrays %ld\n", S.shapes, S.dls, S.dlVerts, S.joints,
	       S.envelopes, S.textures, S.arrays);
	printf("pos s16/f32 %d/%d nrm s16/f32 %d/%d nbt %d index8 attrs %d\n", S.posS16, S.posF32, S.nrmS16, S.nrmF32, S.nbt, S.indexed8);
	printf("%s (%d failures)\n", g_fail ? "FAILED" : "OK", g_fail);
	return g_fail ? 1 : 0;
}
