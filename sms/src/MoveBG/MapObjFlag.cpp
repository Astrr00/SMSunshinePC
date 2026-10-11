#include <MoveBG/MapObjFlag.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <System/MarDirector.hpp>
#include <dolphin/gx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

TMapObjFlagManager* gpMapObjFlagManager;

f32 TMapObjFlag::mFlutterSpeed = 4.0f;

TMapObjFlagManager::TMapObjFlagManager(const char* name)
    : JDrama::TViewObj(name)
{
	gpMapObjFlagManager = this;
}

void TMapObjFlagManager::load(JSUMemoryInputStream& stream)
{
	JDrama::TViewObj::load(stream);

	char buffer[8];
	char trash[0x10];
	stream.readString(buffer, 8);

	switch (gpMarDirector->mMap) {
	case 0:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 2:
		TMapObjFlag::mFlutterSpeed = 16.0f;
		break;
	case 4:
		TMapObjFlag::mFlutterSpeed = 12.0f;
		break;
	default:
		TMapObjFlag::mFlutterSpeed = 8.0f;
		break;
	}
}

void TMapObjFlagManager::registerObj(TMapObjFlag* flag, const char* name)
{
	if (strcmp(name, "flagSun") == 0) {
		if (mInfos[0].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[0].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[0].mFlags[mInfos[0].mFlagNum] = flag;
		mInfos[0].mFlagNum++;
	} else if (strcmp(name, "flagWhite") == 0) {
		if (mInfos[1].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[1].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[1].mFlags[mInfos[1].mFlagNum] = flag;
		mInfos[1].mFlagNum++;
	} else if (strcmp(name, "flagRedsun") == 0) {
		if (mInfos[2].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[2].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[2].mFlags[mInfos[2].mFlagNum] = flag;
		mInfos[2].mFlagNum++;
	} else if (strcmp(name, "flagMonte") == 0) {
		if (mInfos[3].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[3].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[3].mFlags[mInfos[3].mFlagNum] = flag;
		mInfos[3].mFlagNum++;
	} else if (strcmp(name, "flagBird") == 0) {
		if (mInfos[4].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[4].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[4].mFlags[mInfos[4].mFlagNum] = flag;
		mInfos[4].mFlagNum++;
	} else if (strcmp(name, "flagHigekuri") == 0) {
		if (mInfos[5].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[5].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[5].mFlags[mInfos[5].mFlagNum] = flag;
		mInfos[5].mFlagNum++;
	} else if (strcmp(name, "flagBenvenuto") == 0) {
		if (mInfos[6].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[6].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[6].mFlags[mInfos[6].mFlagNum] = flag;
		mInfos[6].mFlagNum++;
	} else if (strcmp(name, "flagDolpicDolphin") == 0) {
		if (mInfos[7].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[7].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[7].mFlags[mInfos[7].mFlagNum] = flag;
		mInfos[7].mFlagNum++;
	} else if (strcmp(name, "flagDolSun") == 0) {
		if (mInfos[8].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[8].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[8].mFlags[mInfos[8].mFlagNum] = flag;
		mInfos[8].mFlagNum++;
	} else if (strcmp(name, "flagDolSunWelcome") == 0) {
		if (mInfos[9].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[9].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[9].mFlags[mInfos[9].mFlagNum] = flag;
		mInfos[9].mFlagNum++;
	} else if (strcmp(name, "flagBianco") == 0) {
		if (mInfos[10].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[10].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[10].mFlags[mInfos[10].mFlagNum] = flag;
		mInfos[10].mFlagNum++;
	} else if (strcmp(name, "flagRiccoBuoy") == 0) {
		if (mInfos[11].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[11].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[11].mFlags[mInfos[11].mFlagNum] = flag;
		mInfos[11].mFlagNum++;
	} else if (strcmp(name, "flagSailMonte") == 0) {
		if (mInfos[12].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[12].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[12].mFlags[mInfos[12].mFlagNum] = flag;
		mInfos[12].mFlagNum++;
	} else if (strcmp(name, "MammaYacht00") == 0) {
		if (mInfos[13].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[13].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[13].mFlags[mInfos[13].mFlagNum] = flag;
		mInfos[13].mFlagNum++;
	} else if (strcmp(name, "flagMare") == 0) {
		if (mInfos[14].mTexture == nullptr) {
			char path[0x40];
			snprintf(path, 0x40, "/scene/mapObj/%s.bti", name);
			mInfos[14].mTexture = (ResTIMG*)JKRFileLoader::getGlbResource(path);
		}
		mInfos[14].mFlags[mInfos[14].mFlagNum] = flag;
		mInfos[14].mFlagNum++;
	}
}

void TMapObjFlagManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjFlag* flag;
	if (cue & CUE_CALC_ANIM) {
		for (int i = 0; i < 15; ++i) {
			TMapObjFlagInfo& info = mInfos[i];
			for (int j = 0; j < info.mFlagNum; ++j) {
				flag = info.mFlags[j];
				MsMtxSetXYZRPH(flag->mModelMtx, flag->mPosition.x,
				               flag->mPosition.y, flag->mPosition.z,
				               flag->mRotation.x, flag->mRotation.y,
				               flag->mRotation.z);
				flag->updateVertex();

				flag->mWavePhase += TMapObjFlag::mFlutterSpeed;
				if (flag->mWavePhase > 360.0f)
					flag->mWavePhase -= 360.0f;

				if (flag->mScaling.y > 3.0f && flag->mScaling.z > 3.0f) {
					SMSGetMSound()->startSoundActor(
					    MSD_SE_OBJ_FLAG, &flag->mPosition, 0, nullptr, 0, 4);
				}
			}
		}
	}

	if (cue & CUE_DRAW) {
		initDraw();
		for (int i = 0; i < 15; ++i) {
			if (mInfos[i].mFlagNum != 0) {
				JUTTexture texture(mInfos[i].mTexture);
				texture.load(GX_TEXMAP0);
				for (int j = 0; j < mInfos[i].mFlagNum; ++j)
					mInfos[i].mFlags[j]->draw();
			}
		}
	}
}

void TMapObjFlagManager::initDraw()
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
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
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_GREATER, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetZCompLoc(GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
}

TMapObjFlag::TMapObjFlag(const char* name)
    : THitActor(name)
{
	char trash[8];
	mWidth      = 0.0f;
	mHeight     = 0.0f;
	mVtxNumX    = 0;
	mVtxNumY    = 0;
	mVertices   = nullptr;
	mWaveFreqX  = 125.0f;
	mWaveFreqY  = 130.0f;
	mWaveAmp    = 20.0f;
	mWavePhase  = 360.0f * (rand() * 0.000030517578f);
	mVtxStep    = 1;
	mModelMtx.identity();
}

void TMapObjFlag::load(JSUMemoryInputStream& stream)
{
	char buffer[0x40];
	JDrama::TActor::load(stream);
	stream.readString(buffer, 0x40);
	init(buffer);
}

void TMapObjFlag::init(const char* name)
{
	mWidth  = 100.0f * mScaling.z;
	mHeight = 100.0f * mScaling.y;
	mWaveFreqX /= mScaling.z;
	mWaveFreqY /= mScaling.y;
	mWaveAmp *= mScaling.z;
	mVtxNumX = mWidth / 50.0f;
	mVtxNumY = mHeight / 100.0f;
	if (mVtxNumX < 2)
		mVtxNumX = 3;
	if (mVtxNumY < 2)
		mVtxNumY = 3;

	MsMtxSetXYZRPH(mModelMtx, mPosition.x, mPosition.y, mPosition.z,
	               mRotation.x, mRotation.y, mRotation.z);

	f32 stepX = mWidth / mVtxNumX;
	f32 stepY = mHeight / mVtxNumY;
	s32 freeSizeBefore = JKRHeap::getCurrentHeap()->getTotalFreeSize();
	mVertices = new JGeometry::TVec3<f32>*[mVtxNumY];
	for (int i = 0; i < mVtxNumY; ++i) {
		mVertices[i] = new JGeometry::TVec3<f32>[mVtxNumX];
		for (int j = 0; j < mVtxNumX; ++j)
			mVertices[i][j].set(0.0f, i * stepY, j * stepX);
	}

	static int total_use_size = 0;
	s32 used = freeSizeBefore - JKRHeap::getCurrentHeap()->getTotalFreeSize();

	gpMapObjFlagManager->registerObj(this, name);
	initHitActor(0x4000000D, 1, 0, 0.0f, 0.0f, 0.0f, 0.0f);
}

void TMapObjFlag::updateVertex()
{
	char trash[0x10];
	for (int i = 0; i < mVtxNumY; i += mVtxStep) {
		f32 rowPhase = i * mWaveFreqY;
		for (int j = 0; j < mVtxNumX; j += mVtxStep) {
			f32 ratio = (f32)j / (f32)mVtxNumX;
			f32 angle = mWavePhase + (-j * mWaveFreqX + rowPhase);
			while (angle >= 180.0f)
				angle -= 360.0f;
			while (angle < -180.0f)
				angle += 360.0f;

			s16 angleS16 = angle * (65536.0f / 360.0f);
			mVertices[i][j].x
			    = mWaveAmp * ratio
			      * jmaSinTable[static_cast<u16>(angleS16) >> jmaSinShift];
		}
	}
}

void TMapObjFlag::draw()
{
	TMtx34f mtx;
	char trash[0x50];
	mtx.set(j3dSys.getViewMtx());
	MTXConcat(mtx, mModelMtx, mtx);
	GXLoadPosMtxImm(mtx, GX_PNMTX0);

	u16 vtxNum = ((mVtxNumX - mVtxStep * 2) / mVtxStep + 2) * 2;
	f32 texU = 1.0f / (mVtxNumX - 1);
	f32 texV = 1.0f / (mVtxNumY - 1);
	for (int i = 0; i < mVtxNumY - mVtxStep; i += mVtxStep) {
		f32 v0 = texV * (mVtxNumY - 1 - i);
		f32 v1 = texV * (mVtxNumY - 1 - (i + 1));
		GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vtxNum);

		const JGeometry::TVec3<f32>& a0 = mVertices[i][0];
		GXPosition3f32(a0.x, a0.y, a0.z);
		GXTexCoord2f32(0.0f, v0);
		const JGeometry::TVec3<f32>& b0 = mVertices[i + 1][0];
		GXPosition3f32(b0.x, b0.y, b0.z);
		GXTexCoord2f32(0.0f, v1);

		for (int j = 1; j < mVtxNumX - mVtxStep; j += mVtxStep) {
			f32 u = texU * j;
			const JGeometry::TVec3<f32>& a = mVertices[i][j];
			GXPosition3f32(a.x, a.y, a.z);
			GXTexCoord2f32(u, v0);
			const JGeometry::TVec3<f32>& b = mVertices[i + 1][j];
			GXPosition3f32(b.x, b.y, b.z);
			GXTexCoord2f32(u, v1);
		}

		const JGeometry::TVec3<f32>& a1 = mVertices[i][mVtxNumX - 1];
		GXPosition3f32(a1.x, a1.y, a1.z);
		GXTexCoord2f32(1.0f, v0);
		const JGeometry::TVec3<f32>& b1 = mVertices[i + 1][mVtxNumX - 1];
		GXPosition3f32(b1.x, b1.y, b1.z);
		GXTexCoord2f32(1.0f, v1);
	}
}
