#include <GC2D/SelectShine2.hpp>
#include <math.h>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAnm.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DAnmLoader.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <System/Application.hpp>

// TODO: get rid of this
static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

// Center of the circle the shines are placed on
JGeometry::TVec3<f32> TSelectShineManager::cCenter(300.0f, 160.0f, -9000.0f);

static inline JGeometry::TVec3<f32> getShinePos(s16 angle)
{
	return JGeometry::TVec3<f32>(
	    TSelectShineManager::cCenter.x + 1500.0f * JMASSin(angle),
	    TSelectShineManager::cCenter.y,
	    TSelectShineManager::cCenter.z + 9000.0f * JMASCos(angle));
}

static inline JGeometry::TVec3<f32> getScrollPos(s32 scroll, int idx)
{
	s16 angle = 57.295776f * (f32)(s16)(scroll + idx * 0x28);
	return getShinePos(angle);
}

static inline s16 getShineYaw(const JGeometry::TVec3<f32>& pos,
                              const JGeometry::TVec3<f32>& shinePos)
{
	JGeometry::TVec2<f32> diff;
	diff.sub(JGeometry::TVec2<f32>(300.0f, 1300.0f), JGeometry::TVec2<f32>(shinePos.x, shinePos.z));

	f32 cs  = 1.0f;
	f32 sn  = 0.0f;
	f32 rx  = diff.x * cs - diff.y * sn;
	f32 ry  = diff.x * sn + diff.y * cs;
	s16 yaw = 57.295776f * fabsf(atan2f(rx, ry));
	if (pos.x > TSelectShineManager::cCenter.x)
		yaw *= -1;
	return yaw;
}

// Quadratic bezier curve through (a, b, c) at t in [0, 1]
static inline f32 bezier(f32 t, f32 a, f32 b, f32 c)
{
	f32 s = 1.0f - t;
	return a * (s * s) + b * (2.0f * s * t) + c * (t * t);
}

inline void TSelectShine::stopEmitters()
{
	if (mKind != 2) {
		if (mKind == 0) {
			mEmitters[1]->stopCreateParticle();
			mEmitters[0]->stopCreateParticle();
		}
		mEmitters[2]->stopCreateParticle();
	}
}

inline void TSelectShine::playEmitters()
{
	if (mKind != 2) {
		if (mKind == 0) {
			mEmitters[1]->playCreateParticle();
			mEmitters[0]->playCreateParticle();
		}
		mEmitters[2]->playCreateParticle();
	}
}

void TSelectShine::move()
{
	char trash[0x20];
#ifdef TARGET_PC
	// retail leaves y uninitialized when mTime >= 4
	f32 y = 0.0f;
#else
	f32 y;
#endif
	if (mTime < 1.0f) {
		y = bezier(mTime, 0.0f, 0.9f * mAmplitude, mAmplitude);
	} else if (mTime < 2.0f) {
		y = bezier(mTime - 1.0f, mAmplitude, 0.9f * mAmplitude, 0.0f);
	} else if (mTime < 3.0f) {
		y = bezier(mTime - 2.0f, 0.0f, 0.9f * -mAmplitude, -mAmplitude);
	} else if (mTime < 4.0f) {
		y = bezier(mTime - 3.0f, -mAmplitude, 0.9f * -mAmplitude, 0.0f);
	}
	mOffset.y = y;

	MtxPtr base               = mModel->getBaseTRMtx();
	JGeometry::TVec3<f32> pos = mPos + mOffset;

	base[0][3] = pos.x;
	base[1][3] = pos.y;
	base[2][3] = pos.z;

	s16 frame = -0.05f * pos.z;
	if (frame < 30)
		frame = 30;

	if (mFrameDown) {
		mFrame -= 8;
		if (mFrame < frame)
			mFrame = frame;
	} else if (mFrameUp) {
		mFrame += 8;
		if (mFrame > mAnm->getFrameMax() - 1)
			mFrame = mAnm->getFrameMax() - 1;
	} else {
		mFrame = frame;
		if (frame < 0)
			mFrame = 0;
		if (frame > mAnm->getFrameMax())
			mFrame = mAnm->getFrameMax();
	}

	s32 alpha = 255.0f * (1.0f - (f32)(mFrame / mAnm->getFrameMax()));
	if (mKind != 2) {
		if (mKind == 0) {
			mEmitters[0]->setGlobalRTMatrix(base);
			mEmitters[1]->setGlobalRTMatrix(base);
			mEmitters[0]->mGlobalPrmColor.a = alpha;
			mEmitters[1]->mGlobalPrmColor.a = alpha;
		}
		mEmitters[2]->setGlobalRTMatrix(base);
		mEmitters[2]->mGlobalPrmColor.a = alpha;
	}

	if (mSelected) {
		Mtx rot;
		PSMTXRotRad(rot, 'y',
		            0.017453292f * ((f32)mSpinSpeed * SMSGetAnmFrameRate()));
		mSpinAngle = (f32)mSpinAngle + (f32)mSpinSpeed * SMSGetAnmFrameRate();
		if (mSpinAngle > 360)
			mSpinAngle -= 360;
		if (mSpinAngle < 0)
			mSpinAngle += 360;
		PSMTXConcat(base, rot, base);
	} else if (mSpinAngle != 0) {
		s16 step = (f32)mSpinSpeed * SMSGetAnmFrameRate();
		mSpinAngle += step;
		if (mSpinAngle > 360) {
			step       = step - mSpinAngle + 360;
			mSpinAngle = 0;
		}
		Mtx rot;
		PSMTXRotRad(rot, 'y', 0.017453292f * (f32)step);
		PSMTXConcat(base, rot, base);
	}

	mTime += mTimeSpeed;
	if (mTime > 4.0f)
		mTime = 0.0f;
}

TSelectShine::TSelectShine(J3DModelData* pModelData, J3DAnmColor* pAnm,
                           JPAEmitterManager* pEmitterMgr,
                           JGeometry::TVec3<f32>& rPos, s16 yaw, u8 kind,
                           f32 time, f32 timeSpeed, f32 amplitude)
    : mSelected(false)
    , mTime(0.0f)
    , mAmplitude(0.0f)
    , mTimeSpeed(0.0f)
    , mSpinAngle(0)
    , mSpinSpeed(0)
    , mYaw(yaw)
    , mFrame(0)
    , unk3E(0)
    , unk40(0.0f)
    , unk44(0.0f)
    , mFrameDown(0)
    , mFrameUp(0)
    , mKind(0)
    , mEmitterMgr(nullptr)
{
	char trash[0x30];
	mEmitters[0] = nullptr;
	mEmitters[1] = nullptr;
	mEmitters[2] = nullptr;

	mModel = new J3DModel(pModelData, 0, 1);
	mAnm   = pAnm;
	mPos.set(rPos);
	mOffset.x   = 0.0f;
	mOffset.y   = 0.0f;
	mOffset.z   = 0.0f;
	mEmitterMgr = pEmitterMgr;
	mTimeSpeed  = timeSpeed;
	mAmplitude  = amplitude;
	mTime       = time;
	mSpinSpeed  = 3;

	MtxPtr base = mModel->getBaseTRMtx();
	base[0][3]  = mPos.x;
	base[1][3]  = mPos.y;
	base[2][3]  = mPos.z;
	Mtx rot;
	PSMTXRotRad(rot, 'y', 0.017453292f * (f32)mYaw);
	PSMTXConcat(base, rot, base);

	mKind = kind;
	if (mKind != 2) {
		JGeometry::TVec3<f32> emitPos = mPos + mOffset;
		if (mKind == 0) {
			mEmitterMgr->createEmitter(emitPos, 0, nullptr, nullptr);
			mEmitters[0] = mEmitterMgr->unkC8[0][0];
			mEmitterMgr->createEmitter(emitPos, 1, nullptr, nullptr);
			mEmitters[1] = mEmitterMgr->unkC8[0][0];
			mEmitterMgr->createEmitter(emitPos, 2, nullptr, nullptr);
		} else {
			mEmitterMgr->createEmitter(emitPos, 3, nullptr, nullptr);
		}
		mEmitters[2] = mEmitterMgr->unkC8[0][0];
		stopEmitters();
	}
}

void TSelectShineManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x88];
	if (cue & CUE_MOVE) {
		for (int i = 0; i < mNumShines; ++i)
			mShines[i]->move();

		if (mIncreasing || mDecreasing) {
			mScroll = (f32)mScroll + mScrollSpeed;
			if (mIncreasing && mScroll < mCurrent * -0x28) {
				mScroll     = mCurrent * -0x28;
				mIncreasing = 0;
			}
			if (mDecreasing && mScroll > mCurrent * -0x28) {
				mScroll     = mCurrent * -0x28;
				mDecreasing = 0;
			}

			for (int i = 0; i < mNumShines; ++i) {
				JGeometry::TVec3<f32> pos = getScrollPos(mScroll, i);
				mShines[i]->mPos          = pos;

				JGeometry::TVec3<f32> shinePos
				    = mShines[i]->mPos + mShines[i]->mOffset;

				s16 yaw = getShineYaw(pos, shinePos);

				TSelectShine* shine = mShines[i];
				MtxPtr base         = shine->mModel->getBaseTRMtx();
				Mtx rot;
				PSMTXRotRad(rot, 'y', 0.017453292f * (f32)(yaw - shine->mYaw));
				PSMTXConcat(base, rot, base);
				shine->mYaw = yaw;
			}
		}
	}

	if (cue & CUE_CALC_ANIM) {
		for (int i = 0; i < mNumShines; ++i) {
			mShines[i]->mAnm->setFrame((f32)mShines[i]->mFrame);
			mShines[i]->mModel->update();
			mShines[i]->mModel->viewCalc();
		}
	}

	if (cue & CUE_DRAW) {
		MTXCopy(graphics->mViewMtx, j3dSys.getViewMtx());
		j3dSys.drawInit();
		j3dSys.setUnk4C(3);
		mDrawBufferOpa->draw();
		j3dSys.setUnk4C(4);
		mDrawBufferXlu->draw();
		mDrawBufferOpa->frameInit();
		mDrawBufferXlu->frameInit();
	}
}

void TSelectShineManager::startDecrease(int count)
{
	mShines[mCurrent]->stopEmitters();

	mDecreasing  = 1;
	mScrollSpeed = count * 0x28 / 10;
	mCurrent -= count;

	mShines[mCurrent]->playEmitters();
}

void TSelectShineManager::startIncrease(int count)
{
	mShines[mCurrent]->stopEmitters();

	mIncreasing  = 1;
	mScrollSpeed = count * -0x28 / 10;
	mCurrent += count;

	mShines[mCurrent]->playEmitters();
}

void TSelectShineManager::startClose()
{
	mShines[mCurrent]->mSelected = false;

	for (int i = 0; i < 8; ++i) {
		TSelectShine* shine = mShines[i];
		if (shine != nullptr && i != mCurrent && shine->mFrameUp == 0) {
			shine->mFrameUp   = 1;
			shine->mFrameDown = 0;
		}
	}

	mClosing = 1;
}

void TSelectShineManager::initData(u8* pTypes, u8 numShines, u8 current,
                                   JPAEmitterManager* pEmitterMgr)
{
	char trash[0x48];
	mDrawBufferOpa = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBufferOpa, 0);
	mDrawBufferXlu = new J3DDrawBuffer(0x400);
	j3dSys.setDrawBuffer(mDrawBufferXlu, 1);

	void* bmd               = JKRGetResource("/select/shine_menu.bmd");
	void* bpk               = JKRGetResource("/select/shine_menu.bpk");
	J3DModelData* modelData = J3DModelLoaderDataBase::load(
	    bmd, J3DMLF_MaterialColorLightOn | J3DMLF_MaterialPEFull
	             | J3DMLF_MaterialUseIndirect | (4 << J3DMLF_TevStageNumShift));
	J3DAnmColor* anm = (J3DAnmColor*)J3DAnmLoaderDataBase::load(bpk);
	anm->searchUpdateMaterialID(modelData);
	for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
		J3DMaterialAnm* materialAnm = new J3DMaterialAnm;
		modelData->getMaterialNodePointer(i)->change();
		modelData->getMaterialNodePointer(i)->setMaterialAnm(materialAnm);
	}
	modelData->entryMatColorAnimator(anm);

	void* emptyBmd = JKRGetResource("/select/shine_menu_empty.bmd");
	void* emptyBpk = JKRGetResource("/select/shine_menu_empty.bpk");
	J3DModelData* emptyModelData = J3DModelLoaderDataBase::load(
	    emptyBmd, J3DMLF_MaterialColorLightOn | J3DMLF_MaterialPEFull
	                  | J3DMLF_MaterialUseIndirect
	                  | (4 << J3DMLF_TevStageNumShift));
	J3DAnmColor* emptyAnm = (J3DAnmColor*)J3DAnmLoaderDataBase::load(emptyBpk);
	emptyAnm->searchUpdateMaterialID(emptyModelData);
	// NOTE: the original loops up to the material count of the first model
	for (u16 i = 0; i < modelData->getMaterialNum(); ++i) {
		J3DMaterialAnm* materialAnm = new J3DMaterialAnm;
		emptyModelData->getMaterialNodePointer(i)->change();
		emptyModelData->getMaterialNodePointer(i)->setMaterialAnm(materialAnm);
	}
	emptyModelData->entryMatColorAnimator(emptyAnm);

	mNumShines = numShines;
	mCurrent   = current;
	mScroll    = mCurrent * -0x28;

	for (int i = 0; i < 8; ++i) {
		s16 angle                 = 57.295776f * (f32)(s16)(mScroll + i * 0x28);
		JGeometry::TVec3<f32> pos = getShinePos(angle);

		s16 yaw = getShineYaw(pos, pos);

		if (pTypes[i] == 3) {
			mShines[i] = new TSelectShine(
			    modelData, anm, pEmitterMgr, pos, yaw, 0,
			    (f32)(s32)(MsRandF() * 4000.0f) / 1000.0f, 0.01f, 10.0f);
		} else if (pTypes[i] == 1 || pTypes[i] == 2) {
			mShines[i] = new TSelectShine(
			    emptyModelData, emptyAnm, pEmitterMgr, pos, yaw, 1,
			    (f32)(s32)(MsRandF() * 4000.0f) / 1000.0f, 0.01f, 10.0f);
		} else {
			mShines[i] = nullptr;
		}
	}

	mShines[mCurrent]->playEmitters();
}

TSelectShineManager::TSelectShineManager(const char* name)
    : JDrama::TViewObj(name)
    , mDrawBufferOpa(nullptr)
    , mDrawBufferXlu(nullptr)
    , unk78(0.0f)
    , unk7C(0.0f)
    , mNumShines(0)
    , unk90(0.0f)
    , unk94(0.0f)
    , unk98(0)
    , mScroll(0)
    , mScrollSpeed(0.0f)
    , mIncreasing(0)
    , mDecreasing(0)
    , unkA6(0)
    , mClosing(0)
{
}
