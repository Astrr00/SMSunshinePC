#include <MoveBG/MapObjWave.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JUtility/JUTColor.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Player/MarioAccess.hpp>
#include <System/MarDirector.hpp>
#include <dolphin/gx.h>
#include <math.h>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static u8 sAlphaCompLarge = 0x55;
static u8 sAlphaCompSmall = 0x23;
static JUtility::TColor sColor;
TMapObjWave* gpMapObjWave;

static inline bool isType700(const TBGCheckData* data)
{
	if (data->mBGType == 0x700)
		return true;
	return false;
}

inline s32 TMapObjWave::getAlpha(f32 x, f32 z) const
{
	if (fabsf(x) > fabsf(z))
		return mAlpha * (1.0f - mInvHalfAreaSize * fabsf(x));
	return mAlpha * (1.0f - mInvHalfAreaSize * fabsf(z));
}

inline f32 TMapObjWave::getStaticTexPos0(f32 pos) const
{
	return pos * mTexScale0;
}

inline f32 TMapObjWave::getStaticTexPos1(f32 pos) const
{
	return pos * mTexScale1;
}

inline f32 TMapObjWave::getMoveTexPos0(f32 pos) const
{
	return mTexScrollU + getStaticTexPos0(pos);
}

inline f32 TMapObjWave::getMoveTexPos1(f32 pos) const
{
	return mTexScrollV + getStaticTexPos1(pos);
}

TMapObjWave::TMapObjWave(const char* name)
    : JDrama::TViewObj(name)
{
	mAreaSize        = 0.0f;
	mHalfAreaSize    = 0.0f;
	mInvHalfAreaSize = 0.0f;
	mCellNum         = 0;
	mFreqX           = 0.0f;
	mFreqZ           = 0.0f;
	mDeepAmpX        = 0.0f;
	mDeepAmpZ        = 0.0f;
	mShallowAmpX     = 0.0f;
	mShallowAmpZ     = 0.0f;
	mAmpX            = 0.0f;
	mAmpZ            = 0.0f;
	mExtraAmp        = 0.0f;
	mExtraAmpSpeed   = 0.1f;
	mAmpDepth        = 0.0f;
	mAlphaDepth      = 0.0f;
	mAlpha           = 255.0f;
	mDeepAlpha       = 255.0f;
	mShallowAlpha    = 0.0f;
	mTexScrollSpeed  = 0.0f;
	mPhaseX          = 360.0f * (rand() * 0.000030517578f);
	mPhaseZ          = 360.0f * (rand() * 0.000030517578f);
	mTexScrollU      = rand() * 0.000030517578f;
	mTexScrollV      = rand() * 0.000030517578f;
	mTexScale0       = 0.0f;
	mTexScale1       = 0.0f;
	mTexture         = nullptr;
	unk98            = 0;
	sColor.set(200, 200, 255, 0);
	mTevColor0.r = 194;
	mTevColor0.g = 242;
	mTevColor0.b = 190;
	mTevColor0.a = 0;
	mTevColor1.r = 0;
	mTevColor1.g = 0;
	mTevColor1.b = 0;
	mTevColor1.a = 72;
	mTevColor2.r = 0;
	mTevColor2.g = 0;
	mTevColor2.b = 0;
	mTevColor2.a = 144;
	gpMapObjWave = this;
}

void TMapObjWave::load(JSUMemoryInputStream& stream)
{
	char trash[0x10];
	JDrama::TViewObj::load(stream);

	mAreaSize        = 5200.0f;
	mCellSize        = 200.0f;
	mHalfAreaSize    = mAreaSize / 2.0f;
	mInvHalfAreaSize = 1.0f / mHalfAreaSize;
	mCellNum         = mAreaSize / mCellSize;
	mTexture = (ResTIMG*)JKRFileLoader::getGlbResource("/scene/map/map/wave.bti");
	mTexScrollSpeed = 0.0015f;
	mTexScale0      = 0.0012f;
	mTexScale1      = 0.0015f;
	mAmpDepth       = 400.0f;
	mAlphaDepth     = 150.0f;
	mFreqX          = 0.02f;
	mFreqZ          = 0.03f;

	switch (gpMarDirector->mMap) {
	case 3:
	case 30:
		mDeepAmpX    = 25.0f;
		mDeepAmpZ    = 20.0f;
		mShallowAmpX = 0.0f;
		mShallowAmpZ = 0.0f;
		mAmpX        = mDeepAmpX;
		mAmpZ        = mDeepAmpZ;
		break;
	case 4:
		mDeepAmpX    = 40.0f;
		mDeepAmpZ    = 30.0f;
		mShallowAmpX = 5.0f;
		mShallowAmpZ = 0.0f;
		break;
	case 13:
		mDeepAmpX    = 30.0f;
		mDeepAmpZ    = 25.0f;
		mShallowAmpX = 5.0f;
		mShallowAmpZ = 0.0f;
		break;
	case 9:
	case 52:
		mDeepAmpX    = 10.0f;
		mDeepAmpZ    = 15.0f;
		mShallowAmpX = 0.0f;
		mShallowAmpZ = 0.0f;
		break;
	default:
		mDeepAmpX    = 30.0f;
		mDeepAmpZ    = 25.0f;
		mShallowAmpX = 0.0f;
		mShallowAmpZ = 0.0f;
		break;
	}

	mAmpX = mDeepAmpX;
	mAmpZ = mDeepAmpZ;
}

void TMapObjWave::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x20];
	if (mTexture != nullptr) {
		if (cue & CUE_MOVE) {
			updateTime();
			if (gpMarDirector->mMap == 4 || gpMarDirector->mMap == 6)
				updateHeightAndAlpha();
		}
		if (cue & CUE_DRAW) {
			initDraw();
			draw();
		}
	}
}

#pragma dont_inline on
void TMapObjWave::updateTime()
{
	mPhaseX += mFreqX;
	if (mPhaseX > 6.28318f)
		mPhaseX -= 6.28318f;

	mPhaseZ += mFreqZ;
	if (mPhaseZ > 6.28318f)
		mPhaseZ -= 6.28318f;

	mTexScrollU += mTexScrollSpeed;
	if (mTexScrollU > 1.0f)
		mTexScrollU -= 1.0f;

	mTexScrollV += mTexScrollSpeed;
	if (mTexScrollV > 1.0f)
		mTexScrollV -= 1.0f;
}

#pragma dont_inline off

void TMapObjWave::updateHeightAndAlpha()
{
	const TBGCheckData* ground;
	const TBGCheckData* groundExact;
	char trash[0x34];
	gpMap->checkGround(SMS_GetMarioPos(), &ground);
	gpMap->checkGroundExactY(gpMarioPos->x, 10.0f, gpMarioPos->z,
	                         &groundExact);

	if (SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER)
	    || groundExact->isWaterSurface() || ground->isWaterSurface()) {
		f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
		    gpMarioPos->x, 0.0f, gpMarioPos->z, &groundExact);

		f32 depth = mAmpDepth + groundY;
		if (depth < 0.0f || isType700(groundExact)) {
			mAmpX = mDeepAmpX;
			mAmpZ = mDeepAmpZ;
		} else {
			f32 t = 1.0f - depth / mAmpDepth;
			mAmpX = t * (mDeepAmpX - mShallowAmpX) + mShallowAmpX;
			mAmpZ = t * (mDeepAmpZ - mShallowAmpZ) + mShallowAmpZ;
		}

		f32 alphaDepth = mAlphaDepth + groundY;
		if (alphaDepth < 0.0f || isType700(groundExact)) {
			mAlpha = mDeepAlpha;
		} else {
			f32 t  = 1.0f - alphaDepth / mAlphaDepth;
			mAlpha = t * (mDeepAlpha - mShallowAlpha) + mShallowAlpha;
		}
	} else {
		mAmpX  = mShallowAmpX;
		mAmpZ  = mShallowAmpZ;
		mAlpha = mShallowAlpha;
	}

	if (gpMarDirector->mMap == 4) {
		if (-4950.0f < gpMarioPos->x && -4340.0f > gpMarioPos->x
		    && 7660.0f < gpMarioPos->z && 8040.0f > gpMarioPos->z) {
			mAmpX  = mShallowAmpX;
			mAmpZ  = mShallowAmpZ;
			mAlpha = mShallowAlpha;
		}
	}

	int cubeNo = gpCubeStream->getInCubeNo(SMS_GetMarioPos());
	if (cubeNo != -1) {
		if (mExtraAmp
		    < ((TCubeStreamInfo*)gpCubeStream->unk14->begin()[cubeNo])->unk3C)
			mExtraAmp += mExtraAmpSpeed;
	} else {
		if (mExtraAmp > 0.0f)
			mExtraAmp -= mExtraAmpSpeed;
		else
			mExtraAmp = 0.0f;
	}

	if (mExtraAmp > 0.0f) {
		mAmpX = mDeepAmpX + mExtraAmp;
		mAmpZ = mDeepAmpZ + mExtraAmp;
	}
}

void TMapObjWave::draw()
{
	char trash[0x10];
	for (f32 offZ = -mHalfAreaSize; offZ <= mHalfAreaSize - mCellSize;
	     offZ += mCellSize) {
		f32 z0 = offZ + gpMarioPos->z;
		f32 z1 = z0 + mCellSize;
		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, mCellNum * 2);
		for (f32 offX = -mHalfAreaSize; offX <= mHalfAreaSize - mCellSize;
		     offX += mCellSize) {
			f32 x = offX + gpMarioPos->x;

			s32 alpha0 = getAlpha(offX, offZ);
			s32 alpha1 = getAlpha(offX, offZ + mCellSize);

			f32 height0;
			if (mTexture == nullptr) {
				height0 = 0.0f;
			} else {
				f32 waveX = mAmpX * sinf(mFreqX * (0.15915507f * x) + mPhaseX);
				f32 waveZ = mAmpZ * sinf(mFreqZ * (0.15915507f * z0) + mPhaseZ);
				height0   = waveX + waveZ;
			}
			GXPosition3f32(x, height0, z0);
			GXColor4u8(sColor.r, sColor.g, sColor.b, alpha0);
			GXTexCoord2f32(getMoveTexPos0(x), getStaticTexPos0(z0));
			GXTexCoord2f32(0.8f * getStaticTexPos1(x), getMoveTexPos1(z0));

			f32 height1;
			if (mTexture == nullptr) {
				height1 = 0.0f;
			} else {
				f32 waveX = mAmpX * sinf(mFreqX * (0.15915507f * x) + mPhaseX);
				f32 waveZ = mAmpZ * sinf(mFreqZ * (0.15915507f * z1) + mPhaseZ);
				height1   = waveX + waveZ;
			}
			GXPosition3f32(x, height1, z1);
			GXColor4u8(sColor.r, sColor.g, sColor.b, alpha1);
			GXTexCoord2f32(getMoveTexPos0(x), getStaticTexPos0(z1));
			GXTexCoord2f32(0.8f * getStaticTexPos1(x), getMoveTexPos1(z1));
		}
	}
}

void TMapObjWave::noWave()
{
	mShallowAmpX = 0.0f;
	mShallowAmpZ = 0.0f;
	mDeepAmpX    = 0.0f;
	mDeepAmpZ    = 0.0f;
	mAmpX        = 0.0f;
	mAmpZ        = 0.0f;
}

f32 TMapObjWave::getHeight(f32 x, f32 y, f32 z) const
{
	const TBGCheckData* ground;
	f32 result = gpMap->checkGroundExactY(x, 50.0f + y, z, &ground);

	if (ground->isWaterSurface()) {
		if (ground->isSea()) {
			if (mTexture == nullptr)
				return 0.0f;

			f32 waveX = mAmpX * sinf(mFreqX * (0.15915507f * x) + mPhaseX);
			f32 waveZ = mAmpZ * sinf(mFreqZ * (0.15915507f * z) + mPhaseZ);
			return waveX + waveZ;
		}
		return result;
	}
	return y;
}

f32 TMapObjWave::getWaveHeight(f32 x, f32 z) const
{
	if (mTexture == nullptr)
		return 0.0f;

	f32 waveX = mAmpX * sinf(mFreqX * (0.15915507f * x) + mPhaseX);
	f32 waveZ = mAmpZ * sinf(mFreqZ * (0.15915507f * z) + mPhaseZ);
	return waveX + waveZ;
}

void TMapObjWave::initDraw()
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetNumTexGens(2);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, 0, 0x7d);

	JUTTexture texture(mTexture);
	texture.load(GX_TEXMAP0);

	GXSetTevColorS10(GX_TEVREG0, mTevColor0);
	GXSetTevColorS10(GX_TEVREG1, mTevColor1);
	GXSetTevColorS10(GX_TEVREG2, mTevColor2);
	GXSetNumTevStages(2);

	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_TEXA, GX_CA_APREV,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_2,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_SRCCLR, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GEQUAL, sAlphaCompLarge, GX_AOP_OR, GX_LEQUAL,
	                  sAlphaCompSmall);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
}
