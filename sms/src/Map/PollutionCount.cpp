#include <Map/PollutionCount.hpp>
#include <Map/PollutionLayer.hpp>
#include <Map/PollutionManager.hpp>
#include <MarioUtil/ReinitGX.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <System/DrawSyncManager.hpp>
#include <JSystem/ResTIMG.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

#ifdef TARGET_PC
#include <Map/PollutionObj.hpp>
#include <hostendian.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#endif

static void drawBlack(u16 param_1, u16 param_2)
{

	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 1);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetCullMode(GX_CULL_NONE);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0x0, 0x0, 0x0, 0xff });
	GXSetNumTexGens(0);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(0, 1);
	GXPosition2u16(param_1 << 1, 1);
	GXPosition2u16(param_1 << 1, (param_2 << 1) + 1);
	GXPosition2u16(0, (param_2 << 1) + 1);
	GXEnd();
}

#ifdef TARGET_PC
// TARGET_PC: Aurora's GXReadPixMetric returns occlusion visibility (0 or 1 per draw), not a pixel count, so the
// layer and object counters are computed on the CPU from mPollutionMap, the map both passes sample (kept in step
// with the GPU map by GXSetCopyReadback). The counts are net pixels, so the retail "- mPolygonCount * 4" (which
// cancels the hardware's per-triangle overhead) is not applied to them.
extern bool gHpLogOn;
extern int hpLogTick();

// Unit: pixels, as retail sees them (SDK GXReadPixMetric multiplies the PE quad counter by 4, dolphin/gx/GXPerf.c:381).

// Count of the layer pass (drawTex, alpha compare GREATER 0): texels whose TEV-updated value stays above 0.
static u32 pcLayerCount(const TPollutionLayer* layer)
{
	const bool decay = (layer->mFlags & TPollutionLayer::FLAG_DECAY) != 0;
	const int thr    = layer->mPerFrameChangeThreshold;
	const int step   = decay ? 2 : layer->mPerFrameChangeDelta;
	u8 pass[256];
	for (int t = 0; t < 256; ++t) {
		if (layer->mPollutionType == POLLUTION_TYPE_UNK7)
			pass[t] = t > 0; // T' = min(255, T + step) if T > thr, else T
		else if (decay)
			pass[t] = t > step; // T' = max(0, T - step)
		else
			pass[t] = t >= thr ? t > 0 : t > step; // T' = T if T >= thr, else max(0, T - step)
	}
	const u32 n = (u32)layer->mPollutionImage->width * layer->mPollutionImage->height;
	u32 cnt     = 0;
	for (u32 k = 0; k < n; ++k)
		cnt += pass[layer->mPollutionMap[k]];
	return cnt;
}

// Object pass footprints: per registered object, the mPollutionMap offsets of the texels its shapes sample, one
// entry per covered pixel of the 512x512 "graffito check" viewport (overlapping triangles add theirs twice, as on
// the GPU). The layer model is static, so this is rasterised once at registration. A null list means the build
// failed: countObjDegree then leaves the counter at its registration value -1 (never cleaned).
static u32** sPcFootprint;
static u32* sPcFootprintNum;
static int sPcFootprintMax;

struct TPcFpVtx {
	f32 x, y, u, v;
	bool ok; // false: POS or TEX0 index out of range, triangles using this vertex are skipped
};

// Same matrix as initCountObjDegree.
static void pcCountObjMtx(f32 minX, f32 minZ, Mtx out)
{
	TPosition3f m;
	TPosition3f t;
	m.identity();
	t.identity();
	t.setScale(0.03125f, 0.03125f, 0.0f);
	MTXConcat(m.mMtx, t.mMtx, m.mMtx);
	t.setEular((s16)DEG2SHORTANGLE(-90.0f), 0, 0);
	MTXConcat(m.mMtx, t.mMtx, m.mMtx);
	m.setTrans(-minX / 32, -minZ / 32, 0);
	MTXCopy(m.mMtx, out);
}

static f32 pcEdge(const TPcFpVtx& a, const TPcFpVtx& b, f32 px, f32 py)
{
	return (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
}

// A pixel centre exactly on an edge belongs to one of the two triangles sharing it.
static bool pcOwnsTie(const TPcFpVtx& a, const TPcFpVtx& b)
{
	return b.y > a.y || (b.y == a.y && b.x < a.x);
}

static void pcRasterTri(TPcFpVtx a, TPcFpVtx b, TPcFpVtx c, const TPollutionPos& pos, int texW, int texH,
                        u32* out, u32& num)
{
	enum { kViewW = 512, kViewH = 512 };
	f32 area = pcEdge(a, b, c.x, c.y);
	if (area == 0.0f)
		return;
	if (area < 0.0f) {
		TPcFpVtx tmp = b;
		b            = c;
		c            = tmp;
		area         = -area;
	}
	const int x0 = std::max((int)std::floor(std::min(a.x, std::min(b.x, c.x))), 0);
	const int x1 = std::min((int)std::ceil(std::max(a.x, std::max(b.x, c.x))), (int)kViewW - 1);
	const int y0 = std::max((int)std::floor(std::min(a.y, std::min(b.y, c.y))), 0);
	const int y1 = std::min((int)std::ceil(std::max(a.y, std::max(b.y, c.y))), (int)kViewH - 1);
	const bool ownA = pcOwnsTie(b, c), ownB = pcOwnsTie(c, a), ownC = pcOwnsTie(a, b);
	for (int py = y0; py <= y1; ++py) {
		for (int px = x0; px <= x1; ++px) {
			const f32 cx = px + 0.5f, cy = py + 0.5f;
			const f32 wa = pcEdge(b, c, cx, cy);
			const f32 wb = pcEdge(c, a, cx, cy);
			const f32 wc = pcEdge(a, b, cx, cy);
			if (wa < 0.0f || wb < 0.0f || wc < 0.0f)
				continue;
			if ((wa == 0.0f && !ownA) || (wb == 0.0f && !ownB) || (wc == 0.0f && !ownC))
				continue;
			if (out != nullptr) {
				const f32 u = (wa * a.u + wb * b.u + wc * c.u) / area;
				const f32 v = (wa * a.v + wb * b.v + wc * c.v) / area;
				int s       = (int)std::floor(u * texW); // GX_NEAR, GX_CLAMP
				int t       = (int)std::floor(v * texH);
				s           = s < 0 ? 0 : (s >= texW ? texW - 1 : s);
				t           = t < 0 ? 0 : (t >= texH ? texH - 1 : t);
				out[num]    = pos.index(s, t);
			}
			++num;
		}
	}
}

static f32 pcVtxComp(const u8* p, GXCompType type, u8 frac)
{
	switch (type) {
	case GX_U8:
		return *p / (f32)(1 << frac);
	case GX_S8:
		return *(const s8*)p / (f32)(1 << frac);
	case GX_U16:
		return *(const u16*)p / (f32)(1 << frac);
	case GX_S16:
		return *(const s16*)p / (f32)(1 << frac);
	default:
		return *(const f32*)p;
	}
}

static int pcCompSize(GXCompType type)
{
	return type == GX_F32 ? 4 : (type == GX_U16 || type == GX_S16) ? 2 : 1;
}

// Walks the object's shape display lists (big-endian, fans/strips as SMS_CountPolygonNumInShape) through the same
// transform, ortho and viewport as countObjDegree; two passes: count, then fill.
static void pcBuildFootprint(TPollutionObj* obj, u32** outList, u32* outNum)
{
	*outList = nullptr;
	*outNum  = 0;
	TPollutionLayer* layer   = obj->mLayer;
	const ResTIMG* img       = layer->getPollutionImage();
	const J3DVertexData& vd  = layer->getModelData()->getVertexData();
	const GXVtxAttrFmtList* posFmt = nullptr;
	const GXVtxAttrFmtList* texFmt = nullptr;
	for (const GXVtxAttrFmtList* f = vd.getVtxAttrFmtList(); f->attr != GX_VA_NULL; ++f) {
		if (f->attr == GX_VA_POS)
			posFmt = f;
		else if (f->attr == GX_VA_TEX0)
			texFmt = f;
	}
	const u8* posArr = (const u8*)vd.getVtxPosArray();
	const u8* texArr = (const u8*)vd.getVtxTexCoordArray(0);
	if (posFmt == nullptr || texFmt == nullptr || posArr == nullptr || texArr == nullptr) {
		std::printf("[pollution] object footprint: layer model without POS/TEX0, counter stays -1\n");
		return;
	}
	const int posStride = pcCompSize(posFmt->type) * (posFmt->cnt == GX_POS_XYZ ? 3 : 2);
	const int texStride = pcCompSize(texFmt->type) * (texFmt->cnt == GX_TEX_ST ? 2 : 1);
	const u32 posNum    = vd.getVtxPosSize() / posStride;
	const u32 texNum    = vd.getVtxTexCoordSize(0) / texStride;

	Mtx m;
	const Vec& minVec = obj->getJoint()->getMin();
	pcCountObjMtx(minVec.x, minVec.z, m);

	u32* list = nullptr;
	u32 num   = 0;
	for (int fill = 0; fill < 2; ++fill) {
		num = 0;
		for (int j = 0; j < obj->getShapeNum(); ++j) {
			J3DShape* shape = obj->getShape(j);
			int vtxSize = 0, posOff = -1, texOff = -1, posIdxSize = 0, texIdxSize = 0;
			for (const GXVtxDescList* d = shape->getVtxDesc(); d->attr != GX_VA_NULL; ++d) {
				const int size = d->type == GX_INDEX16 ? 2 : (d->type == GX_NONE ? 0 : 1);
				if (d->attr == GX_VA_POS && d->type >= GX_INDEX8) {
					posOff     = vtxSize;
					posIdxSize = size;
				} else if (d->attr == GX_VA_TEX0 && d->type >= GX_INDEX8) {
					texOff     = vtxSize;
					texIdxSize = size;
				} else if (d->attr == GX_VA_PNMTXIDX && fill == 0) {
					std::printf("[pollution] object footprint: shape with PNMTXIDX, using PNMTX0 only\n");
				}
				vtxSize += size;
			}
			if (posOff < 0 || texOff < 0) {
				std::printf("[pollution] object footprint: shape without indexed POS/TEX0 skipped\n");
				continue;
			}
			for (u16 k = 0; k < shape->getMtxGroupNum(); ++k) {
				const u8* dl  = shape->getShapeDraw(k)->getDisplayList();
				const u8* end = dl + shape->getShapeDraw(k)->getDisplayListSize();
				const u8* p   = dl;
				while (p + 3 <= end) {
					const u8 op = p[0];
					if (op != GX_TRIANGLEFAN && op != GX_TRIANGLESTRIP) {
						static bool sLoggedCmd = false;
						if (op >= 0x80 && fill == 0) {
							std::printf("[pollution] object footprint: primitive 0x%02X not counted\n", op);
						} else if (op < 0x80 && op != 0 && !sLoggedCmd) { // 0 = NOP padding at the end of the list
							sLoggedCmd = true;
							std::printf("[pollution] object footprint: display-list command 0x%02X ends the parse, rest of the shape not counted\n", op);
						}
						break;
					}
					const u16 n = rdbe16(p + 1);
					p += 3;
					if (p + n * vtxSize > end)
						break;
					TPcFpVtx v0 = {}, v1 = {}, v2 = {};
					for (u16 i = 0; i < n; ++i) {
						const u8* vp  = p + i * vtxSize;
						const u32 pi  = posIdxSize == 2 ? rdbe16(vp + posOff) : vp[posOff];
						const u32 ti  = texIdxSize == 2 ? rdbe16(vp + texOff) : vp[texOff];
						TPcFpVtx cur = {};
						if (pi < posNum && ti < texNum) {
							cur.ok       = true;
							const u8* pp = posArr + pi * posStride;
							const int cs = pcCompSize(posFmt->type);
							Vec world    = { pcVtxComp(pp, posFmt->type, posFmt->frac),
							                 pcVtxComp(pp + cs, posFmt->type, posFmt->frac),
							                 posFmt->cnt == GX_POS_XYZ
							                     ? pcVtxComp(pp + 2 * cs, posFmt->type, posFmt->frac)
							                     : 0.0f };
							Vec screen;
							MTXMultVec(m, &world, &screen); // ortho 0..512 onto the 512x512 viewport: 1:1
							const u8* tp = texArr + ti * texStride;
							cur.x        = screen.x;
							cur.y        = screen.y;
							cur.u        = pcVtxComp(tp, texFmt->type, texFmt->frac);
							cur.v        = texFmt->cnt == GX_TEX_ST
							                   ? pcVtxComp(tp + pcCompSize(texFmt->type), texFmt->type, texFmt->frac)
							                   : 0.0f;
						} else if (fill == 0) {
							static bool sLoggedIdx = false;
							if (!sLoggedIdx) {
								sLoggedIdx = true;
								std::printf("[pollution] object footprint: vertex index pos=%u tex=%u out of range (%u/%u), its triangles skipped\n",
								            (unsigned)pi, (unsigned)ti, (unsigned)posNum, (unsigned)texNum);
							}
						}
						if (i == 0)
							v0 = cur;
						const TPcFpVtx& va = op == GX_TRIANGLEFAN ? v0 : v1;
						if (i >= 2 && va.ok && v2.ok && cur.ok)
							pcRasterTri(va, v2, cur, layer->mPos, img->width,
							            img->height, list, num);
						v1 = v2;
						v2 = cur;
					}
					p += n * vtxSize;
				}
			}
		}
		if (fill == 0) {
			if (num == 0)
				return;
			list = new u32[num];
		}
	}
	*outList = list;
	*outNum  = num;
}
#endif

void TPollutionCounterBase::setCallback(int param_1) const
{
	TDrawSyncManager::smInstance->pushBreakPoint();
	GXSetDrawSync(getCounterNo(param_1));
	TDrawSyncManager::smInstance->pushBreakPoint();
	GXSetDrawSync(0);
}

void TPollutionCounterBase::drawSyncCallback(u16 param_1)
{
	int token = getTokenNo(param_1);
	u32 discard;
	GXReadPixMetric(&discard, &discard, &discard, &discard, mCounters[token],
	                &discard);
	*mCounters[token] -= mPolygonCount[token] * 4;
}

void TPollutionCounterBase::initCounters(int max_counters)
{
	mCounterCapacity = max_counters;
	mCounters        = new u32*[mCounterCapacity];
	mPolygonCount    = new u32[max_counters];
	for (int i = 0; i < max_counters; ++i) {
		mCounters[i]     = nullptr;
		mPolygonCount[i] = 0;
	}
}

void loadPollutionLayer(const u8* param_1, u16 param_2, u16 param_3,
                        GXTexMapID param_4)
{
	GXTexObj GStack_40;
	GXInitTexObj(&GStack_40, (u8*)param_1, param_2, param_3, GX_TF_I8, GX_CLAMP,
	             GX_CLAMP, 0);
	GXInitTexObjLOD(&GStack_40, GX_NEAR, GX_NEAR, 0.0f, 1.0f, 0.0f, GX_FALSE,
	                GX_FALSE, GX_ANISO_1);
	GXLoadTexObj(&GStack_40, param_4);
}

static void initDrawObjGX()
{
	Mtx local_34;

	local_34[2][3] = 0.0f;
	local_34[1][3] = 0.0f;
	local_34[0][3] = 0.0f;
	local_34[1][2] = 0.0f;
	local_34[0][2] = 0.0f;
	local_34[2][1] = 0.0f;
	local_34[0][1] = 0.0f;
	local_34[2][0] = 0.0f;
	local_34[1][0] = 0.0f;
	local_34[2][2] = 1.0f;
	local_34[1][1] = 1.0f;
	local_34[0][0] = 1.0f;
	GXLoadPosMtxImm(local_34, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(0);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, 0xff });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetCullMode(GX_CULL_NONE);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
}

static void initCountObjDegree(f32 fVar1, f32 fVar2)
{
	TPosition3f local_7C;
	TPosition3f local_AC;

	local_7C.identity();

	local_AC.identity();
	local_AC.setScale(0.03125f, 0.03125f, 0.0f);

	MTXConcat(local_7C.mMtx, local_AC.mMtx, local_7C.mMtx);
	local_AC.setEular((s16)DEG2SHORTANGLE(-90.0f), 0, 0);
	MTXConcat(local_7C.mMtx, local_AC.mMtx, local_7C.mMtx);
	local_7C.setTrans(-fVar1 / 32, -fVar2 / 32, 0);
	GXLoadPosMtxImm(local_7C.mMtx, GX_PNMTX0);
}

static void drawShape(J3DShape* shape)
{
#ifdef TARGET_PC
	GXCallDisplayList(shape->getVcdVatCmd(), shape->getVcdVatCmdSize());
#else
	GXCallDisplayList(shape->getVcdVatCmd(), J3DShape::kVcdVatDLSize);
#endif
	shape->loadVtxArray();
	for (u16 k = 0; k < shape->getMtxGroupNum(); ++k)
		shape->getShapeDraw(k)->draw();
}

void TPollutionCounterObj::draw(int index) const
{
	if (mObjects == nullptr)
		return;

	if (mObjects[index] == nullptr)
		return;

	const ResTIMG* pRVar6 = mObjects[index]->mLayer->getPollutionImage();
	loadPollutionLayer((u8*)pRVar6 + pRVar6->imageDataOffset, pRVar6->width,
	                   pRVar6->height, GX_TEXMAP0);
	j3dSys.setVtxPos(mObjects[index]->mLayer->getModelData()->getVtxPosArray());
#ifndef TARGET_PC
	GXClearPixMetric();
#endif
	for (int j = 0; j < mObjects[index]->getShapeNum(); ++j)
		drawShape(mObjects[index]->getShape(j));
}

void TPollutionCounterObj::countObjDegree() const
{
	ReInitializeGX();
	initDrawObjGX();
	for (u16 i = 0; i < mCounterNum; ++i) {
		const Vec& minVec = mObjects[i]->getJoint()->getMin();
		initCountObjDegree(minVec.x, minVec.z);
		draw(i);
#ifdef TARGET_PC
		if (sPcFootprint[i] != nullptr) { // no footprint: the counter keeps its registration value -1
			u32 cnt = 0;
			for (u32 k = 0; k < sPcFootprintNum[i]; ++k)
				cnt += mObjects[i]->mLayer->mPollutionMap[sPcFootprint[i][k]] > 0 ? 1 : 0;
			*mCounters[i] = cnt;
		}
		if (gHpLogOn && hpLogTick() % 10 == 0)
			std::printf("[hplog-obj] tick=%d O%d deg=%u fp=%u cleaned=%d\n", hpLogTick(), (int)i, (unsigned)*mCounters[i],
			            (unsigned)sPcFootprintNum[i], mObjects[i]->isCleaned() ? 1 : 0);
#else
		setCallback(i);
#endif
	}
}

void TPollutionCounterObj::registerPollutionObj(TPollutionObj* obj,
                                                u32* counter_ptr)
{
	mObjects[mCounterNum] = obj;
	for (int i = 0; i < obj->getShapeNum(); ++i) {
		J3DShape* shape = obj->getShape(i);
		mPolygonCount[mCounterNum] += SMS_CountPolygonNumInShape(shape);
	}
	mCounters[mCounterNum] = counter_ptr;
	*counter_ptr           = -1;
#ifdef TARGET_PC
	pcBuildFootprint(obj, &sPcFootprint[mCounterNum], &sPcFootprintNum[mCounterNum]);
#endif
	++mCounterNum;
}

void TPollutionCounterObj::init(int max_counters)
{
	TPollutionCounterBase::initCounters(max_counters);

	mObjects = new TPollutionObj*[max_counters];
	for (int i = 0; i < max_counters; ++i) {
		mObjects[i]      = nullptr;
		mPolygonCount[i] = 0;
	}
#ifdef TARGET_PC
	// Per stage. Plain `new` is the CRT heap on PC (sunshine-pc/src/sdk/jkr_new_delete.cpp), not the stage heap,
	// so the previous stage's lists are freed here.
	for (int i = 0; i < sPcFootprintMax; ++i)
		delete[] sPcFootprint[i];
	delete[] sPcFootprint;
	delete[] sPcFootprintNum;
	sPcFootprintMax = max_counters;
	sPcFootprint    = new u32*[max_counters];
	sPcFootprintNum = new u32[max_counters];
	for (int i = 0; i < max_counters; ++i) {
		sPcFootprint[i]    = nullptr;
		sPcFootprintNum[i] = 0;
	}
#endif
}

TPollutionCounterObj::TPollutionCounterObj()
    : mObjects(nullptr)
{
}

static void drawTex(u16 param_1, u16 param_2)
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetNumTexGens(1);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(0, 0);
	GXTexCoord2u16(0, 0);
	GXPosition2u16(param_1, 0);
	GXTexCoord2u16(1, 0);
	GXPosition2u16(param_1, param_2);
	GXTexCoord2u16(1, 1);
	GXPosition2u16(0, param_2);
	GXTexCoord2u16(0, 1);
	GXEnd();
}

static void initGXforPollutionLayer(int pol_type, u16 flags, u8 threshold,
                                    u8 step)
{
	if (flags & TPollutionLayer::FLAG_DECAY)
		step = 2;

	GXSetCurrentMtx(GX_PNMTX0);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetNumChans(0);
	GXSetTevColor(GX_TEVREG0, (GXColor) { step, step, step, step });
	GXSetTevColor(GX_TEVREG1,
	              (GXColor) { threshold, threshold, threshold, threshold });
	if (pol_type == POLLUTION_TYPE_UNK7) {
		// Basically, if pollution at a position is above the `threshold`, it
		// gets increased further by the `step`. Dirty stuff gets dirtier.

		GXSetNumTevStages(2);

		// clang-format off
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_C1, GX_CC_C0, GX_CC_ZERO);
		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_COMP_R8_GT, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_A1, GX_CA_A0, GX_CA_ZERO);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_R8_GT, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);

		GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
		GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		// clang-format on
	} else if (flags & TPollutionLayer::FLAG_DECAY) {
		// All pollution uniformly decayed by `step` every frame.

		GXSetNumTevStages(1);

		// clang-format off
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		// clang-format on
	} else {
		// If pollution at a position is below the threshold, decrease it
		// further by the `step`. Clan-ish stuff gets cleaner.

		GXSetNumTevStages(2);

		// clang-format off
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_ZERO);
		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_COMP_R8_GT, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A1, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_R8_GT, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);

		GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_APREV, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
		GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, 1, GX_TEVPREV);
		// clang-format on
	}
	GXSetCullMode(GX_CULL_NONE);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetZMode(0, GX_ALWAYS, 0);
	GXSetAlphaUpdate(0);
}

void TPollutionCounterLayer::drawPollutionLayer(int layer_index) const
{
	const TPollutionLayer* layer = mLayers[layer_index];
	const ResTIMG* img           = layer->mPollutionImage;
	drawBlack(img->width, img->height);
	loadPollutionLayer((u8*)img + img->imageDataOffset, img->width, img->height,
	                   GX_TEXMAP0);
	initGXforPollutionLayer(layer->mPollutionType, layer->mFlags,
	                        layer->mPerFrameChangeThreshold,
	                        layer->mPerFrameChangeDelta);

#ifdef TARGET_PC
	drawTex(mLayers[layer_index]->mPollutionImage->width,
	        mLayers[layer_index]->mPollutionImage->height);
	*mCounters[layer_index] = pcLayerCount(layer);
#else
	GXClearPixMetric();
	drawTex(mLayers[layer_index]->mPollutionImage->width,
	        mLayers[layer_index]->mPollutionImage->height);
	setCallback(layer_index);
#endif
}

static void makeWorldToPollutionMtx(f32 scale, f32 x, f32 z, TPosition3f* mtx)
{
	mtx->zero();

	mtx->mMtx[0][0] = scale;
	mtx->mMtx[0][3] = -x * scale;
	mtx->mMtx[1][2] = scale;
	mtx->mMtx[1][3] = -z * scale;
}

void TPollutionCounterLayer::drawJointObjStamp(int layer_index) const
{
	for (int i = 0; i < mJointObjStampTaskNum; ++i) {
		const TPollutionJointObjTaskInfo& info = mJointObjStampTaskQueue[i];
		if (info.mLayerIdx != layer_index)
			continue;

		TPollutionLayer* layer = gpPollution->getLayer(info.mLayerIdx);

		initDrawObjGX();
		GXSetNumChans(1);
		GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
		              GX_AF_NONE);
		GXSetChanCtrl(GX_COLOR1A1, 0, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
		              GX_AF_NONE);

		if (info.unk0 == 0) {
			GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0, 0xff });
		} else {
			GXSetChanMatColor(GX_COLOR0A0,
			                  (GXColor) { 0xff, 0xff, 0xff, 0xff });
		}
		GXSetNumTevStages(1);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
		              GX_COLOR0A0);
		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO,
		                GX_CC_ZERO);
		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
		                GX_TEVPREV);
		GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO,
		                GX_CA_ZERO);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
		                GX_TEVPREV);

		TPosition3f local_6c;
		char trash[0x10];
		makeWorldToPollutionMtx(layer->mPos.mInverseTexelScale, layer->mMinX,
		                        layer->mMinZ, &local_6c);
		GXLoadPosMtxImm(local_6c, GX_PNMTX0);

		j3dSys.setVtxPos(layer->getModelData()->getVtxPosArray());
		for (int j = 0; j < info.mJointObj->getShapeNum(); ++j)
			drawShape(info.mJointObj->getShape(j));
	}
}

static void doTask(u16 target_layer, int task_num,
                   TPollutionLayerTaskInfo* tasks, ResTIMG* stamp_tex,
                   u16 pollution_tex_width, u16 pollution_tex_height,
                   s32 param_7)
{
	JUTTexture texture(stamp_tex);
	texture.load(GX_TEXMAP0);

	int sVar10 = 1;
	int sVar9  = 1;
	if (pollution_tex_width <= pollution_tex_height) {
		sVar10 = pollution_tex_height / pollution_tex_width;
	} else {
		sVar9 = pollution_tex_width / pollution_tex_height;
	}

	for (u16 i = 0; i < task_num; ++i, ++tasks) {
		if (target_layer != tasks->mLayerIdx)
			continue;
		int iVar5 = tasks->mY - param_7;
		int iVar7 = tasks->mY + param_7;
		if (iVar5 < 0)
			iVar5 = 0;
		if (iVar7 > 0xff)
			iVar7 = 0xff;

		GXSetTevColor(GX_TEVREG0, (GXColor) { iVar5, iVar5, iVar5, iVar5 });
		GXSetTevColor(GX_TEVREG1, (GXColor) { iVar7, iVar7, iVar7, iVar7 });
		s16 x1 = tasks->mX - (tasks->mSize / 2);
		s16 y1 = tasks->mZ - (tasks->mSize / 2);
		s16 x2 = tasks->mX + (tasks->mSize / 2);
		s16 y2 = tasks->mZ + (tasks->mSize / 2);
		u16 s1 = x1 * sVar10;
		u16 s2 = x2 * sVar10;
		u16 t1 = y1 * sVar9;
		u16 t2 = y2 * sVar9;
		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition2s16(x1, y1);
		GXTexCoord2u16(0, 0);
		GXTexCoord2u16(s1, t1);
		GXPosition2s16(x2, y1);
		GXTexCoord2u16(1, 0);
		GXTexCoord2u16(s2, t1);
		GXPosition2s16(x2, y2);
		GXTexCoord2u16(1, 1);
		GXTexCoord2u16(s2, t2);
		GXPosition2s16(x1, y2);
		GXTexCoord2u16(0, 1);
		GXTexCoord2u16(s1, t2);
		GXEnd();
	}
}

static void initGXforStamp(const u8* param_1, u16 param_2, u16 param_3,
                           int param_4)
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_S16, param_4);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GXSetNumTexGens(2);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, 0, 0x7d);
	GXTexObj GStack_34;
	GXInitTexObj(&GStack_34, (void*)param_1, param_2, param_3, GX_TF_I8,
	             GX_CLAMP, GX_CLAMP, 0);
	GXInitTexObjLOD(&GStack_34, GX_NEAR, GX_NEAR, 0.0f, 1.0f, 0.0f, 0, 0,
	                GX_ANISO_1);
	GXLoadTexObj(&GStack_34, GX_TEXMAP1);
	GXSetNumTevStages(3);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_A0, GX_CA_KONST,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_RGB8_GT, GX_TB_ZERO,
	                GX_CS_SCALE_1, 1, GX_TEVREG2);
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_A1, GX_CA_TEXA, GX_CA_A2, GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_COMP_RGB8_GT, GX_TB_ZERO,
	                GX_CS_SCALE_1, 1, GX_TEVPREV);
	GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_TEXA, GX_CA_APREV,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
}

// TODO: size doesn't match MAP, may be wrong? Does it really draw a tex stamp?
void TPollutionCounterLayer::drawTexStamp(int target_layer) const
{
	const TPollutionLayer* layer2 = mLayers[target_layer];

	int frac = layer2->mPos.mLog2Width >= layer2->mPos.mLog2Height
	               ? frac = layer2->mPos.mLog2Width
	               : layer2->mPos.mLog2Height;

	initGXforStamp(layer2->mPos.mHeightMap, layer2->mPollutionImage->width,
	               layer2->mPollutionImage->height, frac);

	for (int i = 0; i < mTexStampNum; ++i) {
		TPollutionTexStamp& stamp = mTexStamps[i];
		setTevColorInByStampType(mTexStamps[i].mStampType);
		doTask(target_layer, stamp.mTaskNum, stamp.mTaskQueue,
		       stamp.mStampShapeTex,
		       mLayers[target_layer]->getPollutionImage()->width,
		       mLayers[target_layer]->getPollutionImage()->height,
		       mLayers[target_layer]->getUnk48());
	}
}

void TPollutionCounterLayer::drawRevivalTexStamp(int layer_index) const
{
	char trash[4];
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_S16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetNumTexGens(0);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3C, 0, 0x7D);
	GXSetNumChans(1);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, 0x14 });
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ONE, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetCullMode(GX_CULL_NONE);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0xff);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

	for (int i = 0; i < mRevivalTexStampNum; ++i) {
		TPollutionRevivalTexStamp& stamp = mRevivalTexStamps[i];

		if (stamp.mStampInterval > 0) {
			++stamp.mStampTimer;
			if (stamp.mStampTimer >= stamp.mStampInterval) {
				stamp.mStampTimer = 0;
			} else {
				continue;
			}
		}

		if (stamp.mTargetLayer != layer_index)
			continue;

		if (!stamp.isThing())
			continue;

		JUTTexture texture(stamp.mStampShapeTex);
		char trash[4];
		texture.load(GX_TEXMAP0);
		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition2s16(stamp.mLeft, stamp.mTop);
		GXTexCoord2u16(0, 0);
		GXPosition2s16(stamp.mLeft + stamp.mWidth, stamp.mTop);
		GXTexCoord2u16(1, 0);
		GXPosition2s16(stamp.mLeft + stamp.mWidth, stamp.mTop + stamp.mHeight);
		GXTexCoord2u16(1, 1);
		GXPosition2s16(stamp.mLeft, stamp.mTop + stamp.mHeight);
		GXTexCoord2u16(0, 1);
		GXEnd();
	}
}

void TPollutionCounterLayer::cleanProhibitArea(int param_1) const
{
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3C, 0, 0x7D);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U16, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetTevColor(GX_TEVREG1, (GXColor) { 0, 0, 0, 200 });
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
	                GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_A1, GX_CA_KONST,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_RGB8_GT, GX_TB_ZERO,
	                GX_CS_SCALE_1, 1, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_GREATER, 0);

	u32 w = mLayers[param_1]->getPollutionImage()->width;
	u32 h = mLayers[param_1]->getPollutionImage()->height;

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(0, 0);
	GXTexCoord2u16(0, 0);
	GXPosition2u16(w, 0);
	GXTexCoord2u16(1, 0);
	GXPosition2u16(w, h);
	GXTexCoord2u16(1, 1);
	GXPosition2u16(0, h);
	GXTexCoord2u16(0, 1);
	GXEnd();
}

void TPollutionCounterLayer::drawModelStamp(int) { }

void TPollutionCounterLayer::countTexDegree(int layer_index)
{
	char trash[0x58];
	if (!mIsLayerEnabled[layer_index])
		return;

	ReInitializeGX();
	drawPollutionLayer(layer_index);
	if (mModelStampTaskNum != 0) {
		j3dSys.setUnk4C(7);
		mModelStampDrawBuffers[layer_index]->draw();
		mModelStampDrawBuffers[layer_index]->frameInit();
	}

	ReInitializeGX();
	drawJointObjStamp(layer_index);
	drawTexStamp(layer_index);
	drawRevivalTexStamp(layer_index);
	if (mLayerModelStampTaskNum[layer_index] != 0)
		cleanProhibitArea(layer_index);
}

void TPollutionCounterLayer::resetTask()
{
	for (int i = 0; i < mTexStampNum; ++i)
		mTexStamps[i].mTaskNum = 0;

	for (int i = 0; i < mCounterNum; ++i)
		mLayerModelStampTaskNum[i] = 0;

	mModelStampTaskNum    = 0;
	mJointObjStampTaskNum = 0;
}

void TPollutionCounterLayer::setTevColorInByStampType(u16 type) const
{
	if (type == 0) {
		GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
		                GX_CC_ZERO);
	} else {
		GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ONE, GX_CC_ZERO, GX_CC_ZERO,
		                GX_CC_ZERO);
	}
}

void TPollutionCounterLayer::pushJointObjStampTask(u8 param_1, u8 param_2,
                                                   TPollutionObj* joint_obj)
{
	if (mJointObjStampTaskNum >= 20)
		return;

	mJointObjStampTaskQueue[mJointObjStampTaskNum].unk0      = param_1;
	mJointObjStampTaskQueue[mJointObjStampTaskNum].mLayerIdx = param_2;
	mJointObjStampTaskQueue[mJointObjStampTaskNum].mJointObj = joint_obj;
	++mJointObjStampTaskNum;
}

void TPollutionCounterLayer::calcViewMtx()
{
	TMtx34f afStack_68;
	afStack_68.set(j3dSys.getViewMtx());

	J3DDrawBuffer* oldDbOpa = j3dSys.getDrawBuffer(0);
	J3DDrawBuffer* oldDbXlu = j3dSys.getDrawBuffer(1);

	for (int i = 0; i < mCounterNum; ++i) {
		TPollutionLayer* layer = gpPollution->getLayer(i);

		TPosition3f local_a4;
		char trash[0x1c];
		makeWorldToPollutionMtx(layer->mPos.mInverseTexelScale, layer->mMinX,
		                        layer->mMinZ, &local_a4);

		j3dSys.setViewMtx(local_a4);
		j3dSys.setDrawBuffer(mModelStampDrawBuffers[i], 0);
		j3dSys.setDrawBuffer(mModelStampDrawBuffers[i], 1);

		for (int j = 0; j < mModelStampTaskNum; ++j) {
			if (mModelStampTaskQueue[j].mLayerIdx != i)
				continue;
			mModelStampTaskQueue[j].mModel->viewCalc();
			mModelStampTaskQueue[j].mModel->entry();
		}
	}
	j3dSys.setViewMtx(afStack_68);

	j3dSys.setDrawBuffer(oldDbOpa, 0);
	j3dSys.setDrawBuffer(oldDbXlu, 1);
}

void TPollutionCounterLayer::pushModelStampTask(u8 target_layer,
                                                J3DModel* model)
{
	if (mModelStampTaskNum >= 20)
		return;

#ifdef TARGET_PC
	// Polluters push their stamp on CUE_ENTRY, and the frame-rate replay (sunshineReplayFrame) re-runs the entry
	// stage before the queue is drawn. A second task for the same model enters it into the stamp draw buffer twice:
	// its packet list loops and countTexDegree draws until Aurora's index buffer aborts. Retail pushes each once.
	for (int i = 0; i < mModelStampTaskNum; ++i) {
		if (mModelStampTaskQueue[i].mModel == model && mModelStampTaskQueue[i].mLayerIdx == target_layer)
			return;
	}
#endif

	mModelStampTaskQueue[mModelStampTaskNum].mLayerIdx = target_layer;
	mModelStampTaskQueue[mModelStampTaskNum].mModel    = model;
	++mLayerModelStampTaskNum[target_layer];
	++mModelStampTaskNum;
}

int TPollutionCounterLayer::registerRevivalTexStamp(int param_1, s16 param_2,
                                                    s16 param_3, s16 param_4,
                                                    s16 param_5, int param_6,
                                                    ResTIMG* param_7)
{
	mRevivalTexStamps[mRevivalTexStampNum].registerTex(
	    param_1, param_2, param_3, param_4, param_5, param_6, param_7);
	return mRevivalTexStampNum++;
}

int TPollutionCounterLayer::registerTexStamp(u16 param_1, u16 param_2,
                                             ResTIMG* param_3)
{
	if (mTexStampCapacity == 0)
		return 0;

	mTexStamps[mTexStampNum].registerTexStamp(param_1, param_2, param_3);
	return mTexStampNum++;
}

void TPollutionCounterLayer::registerLayer(const TPollutionLayer* layer,
                                           u32* counter)
{
	mLayers[mCounterNum]       = layer;
	mPolygonCount[mCounterNum] = 2;
	mCounters[mCounterNum]     = counter;
	*counter                   = -1;
	++mCounterNum;
}

bool TPollutionCounterLayer::stampIsCleanType(u16 param_1) const
{
	return mTexStamps[param_1].mStampType == 0 ? true : false;
}

void TPollutionCounterLayer::onLayer(int layer_idx)
{
	mIsLayerEnabled[layer_idx] = true;
}

void TPollutionCounterLayer::offLayer(int layer_idx)
{
	mIsLayerEnabled[layer_idx] = false;
	*mCounters[layer_idx]      = 0;
}

void TPollutionCounterLayer::init(int max_counters, u16 max_tex_stamps,
                                  u16 max_revival_tex_stamps)
{
	TPollutionCounterBase::initCounters(max_counters);

	mLayers = new const TPollutionLayer*[mCounterCapacity];
	for (int i = 0; i < mCounterCapacity; ++i)
		mLayers[i] = nullptr;

	mTexStampCapacity        = max_tex_stamps;
	mTexStamps               = new TPollutionTexStamp[mTexStampCapacity];
	mRevivalTexStampCapacity = max_revival_tex_stamps;
	mRevivalTexStamps = new TPollutionRevivalTexStamp[mRevivalTexStampCapacity];
	mModelStampDrawBuffers  = new J3DDrawBuffer*[max_counters];
	mLayerModelStampTaskNum = new u16[max_counters];
	mIsLayerEnabled         = new bool[max_counters];
	for (int i = 0; i < max_counters; ++i) {
		mModelStampDrawBuffers[i]  = new J3DDrawBuffer(0x10);
		mLayerModelStampTaskNum[i] = 0;
		onLayer(i);
	}
}

TPollutionCounterLayer::TPollutionCounterLayer()
    : mLayers(nullptr)
    , mTexStampCapacity(0)
    , mTexStampNum(0)
    , mTexStamps(nullptr)
    , mRevivalTexStampCapacity(0)
    , mRevivalTexStampNum(0)
    , mRevivalTexStamps(nullptr)
    , mModelStampTaskNum(0)
    , mModelStampDrawBuffers(nullptr)
    , mLayerModelStampTaskNum(nullptr)
    , mJointObjStampTaskNum(0)
{
}

void TPollutionTexStamp::pushTask(u8 layer_idx, u16 size, u16 x, u16 z, s16 y)
{
	if (mTaskNum >= mTaskCapacity)
		return;

	mTaskQueue[mTaskNum].mX        = x;
	mTaskQueue[mTaskNum].mZ        = z;
	mTaskQueue[mTaskNum].mSize     = size;
	mTaskQueue[mTaskNum].mLayerIdx = layer_idx;
	mTaskQueue[mTaskNum].mY        = y;
	++mTaskNum;
}

void TPollutionTexStamp::registerTexStamp(u16 stamp_type, u16 param_2,
                                          ResTIMG* shape_tex)
{
	mStampType     = stamp_type;
	mStampShapeTex = shape_tex;
	mTaskCapacity  = param_2;
	mTaskQueue     = new TPollutionLayerTaskInfo[mTaskCapacity];
}

TPollutionTexStamp::TPollutionTexStamp()
    : mStampType(0)
    , unk2(0)
    , mStampShapeTex(0)
    , mTaskNum(0)
    , mTaskCapacity(0)
    , mTaskQueue(nullptr)
{
}

void TPollutionRevivalTexStamp::registerTex(int target_layer, s16 left, s16 top,
                                            s16 width, s16 height,
                                            int stamp_interval,
                                            ResTIMG* shape_tex)
{
	mTargetLayer   = target_layer;
	mLeft          = left;
	mTop           = top;
	mWidth         = width;
	mHeight        = height;
	mStampShapeTex = shape_tex;
	unk0           = 0;
	mStampInterval = stamp_interval;
}

TPollutionRevivalTexStamp::TPollutionRevivalTexStamp()
    : unk0(0)
    , mTargetLayer(0)
    , mLeft(0)
    , mTop(0)
    , mWidth(0)
    , mHeight(0)
    , mStampShapeTex(0)
    , mStampTimer(0)
    , mStampInterval(0)
{
}
