#include <MoveBG/MapObjMamma.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <Camera/Camera.hpp>
#include <Camera/CameraShake.hpp>
#include <Enemy/ChuuHana.hpp>
#include <Enemy/Beam.hpp>
#include <Enemy/SleepBossHanachan.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapMirror.hpp>
#include <Map/MapStaticObject.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjFlag.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/MirrorActor.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <System/TargetArrow.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

s32 TSandBase::mWitherTime                 = 800;
f32 TSandBase::mScaleMin                   = 0.00001f;
f32 TSandBombBase::mFiringFrameSpeed       = 3.0f;
f32 TSandBombBase::mFiringFrameDownSpeed   = 0.2f;
f32 TSandBombBase::mExplodeFrameSpeed      = 1.0f;
f32 TSandBombBase::mMarioJumpRate          = 0.12f;
s32 TSandBombBase::mExlodingRumbleTime     = 20;
f32 TSandCastle::mCollisionRate            = 1.7f;
s32 TLeanMirror::mGoTargetTime             = 600;
s32 TLeanMirror::mDemoWaitTime             = -1;
s32 TLeanMirror::mDemoLightTime            = 360;
f32 TMammaBlockRotate::mRotSpeed           = 0.1f;
f32 TMammaBlockRotate::mRotReturnSpeed     = 0.01f;
f32 TMammaBlockRotate::mRotEnd             = 130.0f;
f32 TMammaBlockRotate::mMapGoSpeed         = 1.0f;
f32 TMammaBlockRotate::mMapBackSpeed       = 0.1f;
s32 TMammaBlockRotate::mWaitTime           = 600;

inline void TLeanMirror::addShake(THitActor* actor, f32 rate)
{
	MtxPtr nodeMtx = getModel()->getAnmMtx(0);
	f32 dx = (actor->mPosition.x - mPosition.x) / fabsf(mHalfSize * nodeMtx[0][0]);
	f32 dz = (actor->mPosition.z - mPosition.z) / fabsf(mHalfSize * nodeMtx[2][2]);
	mShake.x += rate * (dx - nodeMtx[0][1]);
	mShake.z += rate * (dz - nodeMtx[2][1]);
}

u32 TSandLeaf::touchWater(THitActor*)
{
	mBase->grow();
	return 1;
}

void TSandLeaf::control()
{
	TMapObjBase::control();
	mGroundHeight
	    = gpMap->checkGround(mPosition.x, 200.0f + mPosition.y, mPosition.z,
	                         &mGroundPlane);
	mPosition.y = mGroundHeight;
}

bool TSandBase::withering()
{
	char trash[8];
	mScaling.y -= mWitherSpeed;
	if (mScaling.y < mScaleMin)
		mScaling.y = mScaleMin;

	SMSGetMSound()->startSoundActor(0x2099, &mLeaf->mPosition, 0, nullptr, 0,
	                                4);

	return mScaling.y <= mScaleMin ? true : false;
}

TSandBase::TSandBase(const char* name)
    : TMapObjBase(name)
    , mGrowSpeed(0.0f)
    , mWitherSpeed(0.0f)
    , mLeaf(nullptr)
{
}

void TSandLeafBase::grow()
{
	if (mState == STATE_NORMAL || mState == STATE_GROWN) {
		if (mScaling.y < 1.0f) {
			mScaling.y += mGrowSpeed;
			if (mScaling.y > 1.0f)
				mScaling.y = 1.0f;

			if (mState == STATE_NORMAL) {
				mMapCollisionManager->changeCollision(1);
				mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation,
				                                   mScaling);
				mLeaf->startControlAnim(2);
				mState = STATE_GROWN;
			}

			f32 rate        = SMSGetAnmFrameRate();
			char trash[0x18];
			TSandLeaf* leaf = mLeaf;
			f32 frame = leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
			leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(rate + frame);
			SMSRumbleMgr->start(0x15, 5, &mPosition);
			SMSGetMSound()->startSoundActor(0x2099, &mLeaf->mPosition, 0,
			                                nullptr, 0, 4);
			mStateTimer = mWitherTime;
		}
	}
}

void TSandLeafBase::control()
{
	char trash[0x10];
	TMapObjBase::control();

	switch (mState) {
	case STATE_NORMAL:
		break;
	case STATE_WITHER:
		SMSRumbleMgr->start(0x13, -1, &mPosition);
		if (withering()) {
			SMSRumbleMgr->stop(0x13);
			mMapCollisionManager->changeCollision(0);
			mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation,
			                                   mScaling);
			mStateTimer = mWaitTime;
			mState      = STATE_WAIT;
		}
		break;
	case STATE_WAIT:
		if (!isStateTimerEngaged() && mLeaf->animIsFinished()) {
			mLeaf->awake();
			mLeaf->startAnim(1);
			SMSGetMSound()->startSoundActor(0x3802, &mLeaf->mPosition, 0,
			                                nullptr, 0, 4);
			mState = STATE_GROWN2;
		}
		break;
	case STATE_GROWN2:
		if (mLeaf->animIsFinished()) {
			mLeaf->startAnim(0);
			mState = STATE_NORMAL;
		}
		break;
	}
}

void TSandLeafBase::initMapObj()
{
	char trash[8];
	mGrowSpeed   = 0.003f;
	mWitherSpeed = 0.001f;
	mWaitTime    = 0;
	mScaling.y   = mScaleMin;
	TMapObjBase::initMapObj();

	mLeaf = (TSandLeaf*)TMapObjBaseManager::newAndRegisterObj(
	    "SandLeaf", mPosition, mRotation,
	    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	mLeaf->mBase = this;
	mLeaf->appear();
}

void TSandBomb::makeObjAppeared()
{
	TMapObjBase::makeObjAppeared();
	startControlAnim(1);
	startControlAnim(2);
}

u32 TSandBomb::touchWater(THitActor*)
{
	char trash[0x30];
	f32 speed = TSandBombBase::mFiringFrameSpeed;
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);
	speed = TSandBombBase::mFiringFrameSpeed;
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(speed + frame);
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();

	soundBas(0x289A, 7.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289B, 50.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289C, 100.0f, TSandBombBase::mFiringFrameSpeed);
	soundBas(0x289D, 150.0f, TSandBombBase::mFiringFrameSpeed);

	if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		mBase->grow();
		startControlAnim(3);
		startControlAnim(4);
		startControlAnim(5);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}
	return 1;
}

u32 TSandBomb::getSDLModelFlag() const { return 0; }

void TSandBomb::initMapObj() { TMapObjBase::initMapObj(); }

void TSandBombBase::withered()
{
	mStateTimer = mWaitTime;
	mState      = STATE_WAIT;
	mLeaf->sleep();
}

void TSandBombBase::expanded()
{
	char trash[0x10];
	TSandLeaf* leaf = mLeaf;
	f32 speed       = mExpandFrameSpeed;
	f32 frame       = leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);

	SMSGetMSound()->startSoundActor(0x20C6, &mLeaf->mPosition, 0, nullptr, 0,
	                                4);
	if (mLeaf->animIsFinished())
		mState = 2;
}

void TSandBombBase::exploding()
{
	char trash[0x14];
	f32 speed = mExplodeFrameSpeed;
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);
	TSandLeaf* leaf = mLeaf;
	speed           = mExplodeFrameSpeed;
	frame = leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	leaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);

	f32 dist = getDistanceXZ(*gpMarioPos);
	if (!(isActorType(0x400000CE) ? true : false)) {
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 80.0f
		    && SMS_GetMarioGrLevel() > gpMarioPos->y - 30.0f
		    && dist < mExplodeRadius) {
			SMS_SendMessageToMario(this, 7);
			JGeometry::TVec3<f32> jump(0.0f, 1.0f, 0.0f);
			SMS_ThrowMario(jump, mMarioJumpRate * (mExplodeRadius - dist));
		}
	}

	if (animIsFinished()) {
		mLeaf->startControlAnim(6);
		mState = STATE_EXPANDED;
	}
}

void TSandBombBase::explode()
{
	char trash[8];
	startControlAnim(1);
	mScaling.y = 1.0f;
	mMapCollisionManager->changeCollision(1);
	mMapCollisionManager->unk8->setUp();
	if (mMapCollisionManager->unk8)
		mMapCollisionManager->unk8->moveSRT(mPosition, mRotation, mScaling);

	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emit(0x55, &mPosition, 0, nullptr);
	emitter->setGlobalScale(JGeometry::TVec3<f32>(mExplodeScale));

	if (!SMSGetMarDirector()->isDemoModeNow())
		gpCameraShake->startShake((EnumCamShakeMode)0xD, 1.0f);

	SMSGetMSound()->startSoundActor(0x28A4, &mPosition, 0, nullptr, 0, 4);
	SMSRumbleMgr->start(0x15, mExlodingRumbleTime, &mPosition);
	mState = STATE_EXPLODING;
}

void TSandBombBase::waitBeforeExplode()
{
	mState      = STATE_WAIT_BEFORE_EXPLODE;
	mStateTimer = mExplodeWaitTime;
}

void TSandBombBase::grow() { mState = STATE_FIRING; }

void TSandBombBase::control()
{
	char trash[0x28];
	TMapObjBase::control();

	TSandBomb* bomb = (TSandBomb*)mLeaf;
	switch (mState) {
	case STATE_NORMAL: {
		f32 frame = bomb->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
		            - mFiringFrameDownSpeed;
		if (frame >= 0.0f) {
			mLeaf->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(frame);
			mLeaf->getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(frame);
		}
		break;
	}
	case STATE_FIRING: {
		f32 speed = mExplodeFrameSpeed;
		f32 frame = bomb->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		bomb->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);
		TSandLeaf* leaf = mLeaf;
		speed           = mExplodeFrameSpeed;
		frame = leaf->getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
		leaf->getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(speed + frame);
		leaf  = mLeaf;
		speed = mExplodeFrameSpeed;
		frame = leaf->getMActor()->getFrameCtrl(ANM_TYPE_BTP)->getFrame();
		leaf->getMActor()->getFrameCtrl(ANM_TYPE_BTP)->setFrame(speed + frame);
		if (mLeaf->animIsFinished())
			waitBeforeExplode();
		break;
	}
	case STATE_WAIT_BEFORE_EXPLODE:
		if (!isStateTimerEngaged())
			explode();
		break;
	case STATE_EXPLODING:
		exploding();
		break;
	case STATE_EXPANDED:
		expanded();
		break;
	case STATE_WITHER:
		SMSRumbleMgr->start(0x13, -1, &mPosition);
		if (withering()) {
			withered();
			SMSRumbleMgr->stop(0x13);
		}
		break;
	case STATE_WAIT:
		if (!isStateTimerEngaged()) {
			mState = STATE_NORMAL;
			mLeaf->awake();
			mLeaf->startControlAnim(1);
			mLeaf->startControlAnim(2);
		}
		break;
	}

	if (mLeaf->getColNum() == 0)
		bomb->mTouched = false;
}

TSandBomb* TSandBombBase::findTriggerActor()
{
	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	return (TSandBomb*)TMapObjBaseManager::newAndRegisterObj(
	    "SandBomb", mPosition, mRotation, scale);
}

void TSandBombBase::loadAfter()
{
	mLeaf        = findTriggerActor();
	mLeaf->mBase = this;
	mLeaf->appear();
}

void TSandBombBase::initMapObj()
{
	mGrowSpeed         = 0.006f;
	mWitherSpeed       = 0.005f;
	mWaitTime          = 0;
	mExplodeWaitTime   = 60;
	mExplodeRadius     = 1000.0f;
	mScaling.y         = mScaleMin;
	TMapObjBase::initMapObj();
	mExpandFrameSpeed = 0.5f;

	if (strcmp(getUnkF4(), "SandBombBasePyramid") == 0) {
		mExplodeScale  = 1.3f;
		mExplodeRadius = 1200.0f;
	} else if (strcmp(getUnkF4(), "SandBombBaseShit") == 0) {
		mExplodeScale  = 1.3f;
		mExplodeRadius = 1500.0f;
	} else if (strcmp(getUnkF4(), "SandBombBaseStar") == 0) {
		mExplodeScale = 1.2f;
	} else if (strcmp(getUnkF4(), "SandBombBaseTurtle") == 0) {
		mExplodeScale = 1.2f;
	}

	SMS_LoadParticle("/scene/mapObj/SandBomb.jpa", 0x55);
}

TSandBombBase::TSandBombBase(const char* name)
    : TSandBase(name)
    , mExplodeWaitTime(0)
    , mExplodeScale(1.0f)
    , mExpandFrameSpeed(0.0f)
    , mExplodeRadius(0.0f)
{
}

bool TSandCastle::withering()
{
	char trash[0x28];
	f32 speed = mWitherSpeed;
	f32 frame = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(speed + frame);
	speed = mWitherSpeed;
	frame = getMActor()->getFrameCtrl(ANM_TYPE_BRK)->getFrame();
	getMActor()->getFrameCtrl(ANM_TYPE_BRK)->setFrame(speed + frame);

	frame   = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	f32 end = getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	mScaling.y = mCollisionRate * ((end - frame) / end);

	if (frame > 240.0f && !mStageSwitch->checkLiveFlag(LIVE_FLAG_DEAD)) {
		mStageSwitch->kill();
		gpTargetArrow->unk14 = 0;
	}

	if (animIsFinished()) {
		sleep();
		return true;
	}
	return false;
}

void TSandCastle::expanded()
{
	TSandBombBase::expanded();
	if (mLeaf->animIsFinished()) {
		mState = 2;
		startControlAnim(2);
		startControlAnim(3);
	}
}

void TSandCastle::explode()
{
	char trash[8];
	startControlAnim(1);
	mScaling.y = 1.0f;
	mMapCollisionManager->changeCollision(1);
	mMapCollisionManager->unk8->setUp();
	if (mMapCollisionManager->unk8)
		mMapCollisionManager->unk8->moveSRT(mPosition, mRotation, mScaling);

	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emit(0x55, &mPosition, 0, nullptr);
	emitter->setGlobalScale(JGeometry::TVec3<f32>(mExplodeScale));

	if (!SMSGetMarDirector()->isDemoModeNow())
		gpCameraShake->startShake((EnumCamShakeMode)0xD, 1.0f);

	SMSGetMSound()->startSoundActor(0x28A4, &mPosition, 0, nullptr, 0, 4);
	SMSRumbleMgr->start(0x15, mExlodingRumbleTime, &mPosition);
	mState = STATE_EXPLODING;
	awake();
	mStageSwitch->appear();
	startControlAnim(3);
}

static s32 SandCastleCallBack(TMarDirector::DemoCallbackArg, u32 state)
{
	if (state == 1) {
		gpTargetArrow->unk14 = 1;
		JGeometry::TVec3<f32> arrowPos(8400.0f, 300.0f, 8150.0f);
		char trash[0x20];
		gpTargetArrow->setPos(arrowPos);
		const JGeometry::TVec3<f32>& pos = gpCamera->unk124;
		const JGeometry::TVec3<f32>& at  = gpCamera->unk148;
		s16 angle = matan(pos.z - at.z, pos.x - at.x);
		gpCamera->warpPosAndAt(gpCamera->mCurrentTarget.unk28, angle);
	}
	return 1;
}

void TSandCastle::waitBeforeExplode()
{
	JDrama::TFlagT<u16> flag(0);
	mState      = STATE_WAIT_BEFORE_EXPLODE;
	mStateTimer = mExplodeWaitTime;
	gpMarDirector->fireStartDemoCamera(
	    "mamma1_sandcastle", nullptr, -1, 0.0f, true, &SandCastleCallBack, 0,
	    nullptr, flag);
	mCameraDemo = true;
}

void TSandCastle::calcRootMatrix()
{
	if (!isState(2))
		TMapObjBase::calcRootMatrix();
}

TSandBomb* TSandCastle::findTriggerActor()
{
	char trash[8];
	// Translated: "Sand castle explosion sprout"
	return (TSandBomb*)JDrama::TNameRefGen::getInstance()
	    ->getRootNameRef()
	    ->search("砂の城爆発の芽");
}

void TSandCastle::loadAfter()
{
	char trash[8];
	mLeaf        = findTriggerActor();
	mLeaf->mBase = this;
	mLeaf->appear();

	// Translated: "Stage change (sand castle)"
	mStageSwitch = (TMapObjBase*)JDrama::TNameRefGen::getInstance()
	                   ->getRootNameRef()
	                   ->search("ステージ切替（砂の城）");
	mStageSwitch->makeObjDead();
}

void TSandCastle::initMapObj()
{
	TSandBombBase::initMapObj();
	mWitherSpeed      = 0.11f;
	mExplodeWaitTime = 120;
	sleep();
}

TSandCastle::TSandCastle(const char* name)
    : TSandBombBase(name)
    , mStageSwitch(nullptr)
    , mCameraDemo(false)
{
}

void TLeanMirror::draw() const
{
	MtxPtr nodeMtx = getModel()->getAnmMtx(0);

	JGeometry::TVec3<f32> up;
	up.x = nodeMtx[0][1];
	up.y = nodeMtx[1][1];
	up.z = nodeMtx[2][1];

	JGeometry::TVec3<f32> far;
	JGeometry::TVec3<f32> near;

	near          = up;
	f32 nearScale = 0.001f * (350.0f * mBodyRadius);
	char trash[0x14];
	near.scale(nearScale);
	near += mPosition;

	far = up;
	far.scale(10000.0f);
	far += mPosition;

	gpBeamManager->requestCone(far, near, 1.7f * mBodyRadius, true, true,
	                           false);
}

BOOL TLeanMirror::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x18];
	if (message == 0) {
		sendMsg(0x10000016, message);
		addShake(sender, mTrampleRate);
		return TRUE;
	}
	if (message == 1) {
		sendMsg(0x10000016, message);
		addShake(sender, mHipDropRate);
		return TRUE;
	}
	if (message == 3) {
		addShake(sender, mSuperHipDropRate);
		return TRUE;
	}
	if (message == 8) {
		--mHitsLeft;
		if (mHitsLeft == 0)
			release();
		return TRUE;
	}
	return FALSE;
}

void TLeanMirror::touchPlayer(THitActor* actor)
{
	char trash[0x10];
	if (isState(STATE_SHAKE) && marioIsOn()) {
		addShake(actor, mPlayerRate);
		if (!mBgmStarted) {
			MSBgm::startBGM(0x80010011);
			MSBgm::setTrackVolume(0, 0.0f, 10, 0);
			mBgmStarted = true;
		}
	}
}

void TLeanMirror::touchEnemy(THitActor* actor)
{
	char trash[8];
	if (actor->isActorType(0x10000016) && ((TChuuHana*)actor)->unk1B0)
		addShake(actor, mEnemyRate);
}

// The original reaches the sqrt through nested inline calls, deeper than the
// compiler inlines, so the sqrt stays an out-of-line call.
static inline f32 getMagnitude(f32 squared)
{
	return JGeometry::TUtil<f32>::sqrt(squared);
}

static inline f32 getVecLength(f32 x, f32 y, f32 z)
{
	return getMagnitude(x * x + y * y + z * z);
}

static inline f32 getAngleBetween(const JGeometry::TVec3<f32>& axis,
                                  const JGeometry::TVec3<f32>& a,
                                  const JGeometry::TVec3<f32>& b)
{
	return atan2f(getVecLength(axis.x, axis.y, axis.z),
	              a.x * b.x + a.y * b.y + a.z * b.z);
}

void TLeanMirror::release()
{
	char trash[0x30];
	MtxPtr nodeMtx = getModel()->getAnmMtx(0);
	JGeometry::TVec3<f32> up;
	up.x = nodeMtx[0][1];
	up.y = nodeMtx[1][1];
	up.z = nodeMtx[2][1];

	mFallAxis.cross(up, mStoneDir);

	JGeometry::TVec3<f32> axis;
	axis.cross(up, mStoneDir);
	mFallSpeed = fabsf(getAngleBetween(axis, up, mStoneDir))
	             / (f32)mGoTargetTime;

	mStateTimer = mGoTargetTime;
	mState      = STATE_GO_TARGET;
	offMapObjFlag(MAP_OBJ_FLAG_UNK2);
	SMS_MarioMoveRequest(mMarioMovePos);

	if (strcmp(getUnkF4(), "mirrorS") == 0) {
		// Translated: "Lean mirror S camera"
		SMSGetMarDirector()->fireStartDemoCamera(
		    "ぐらぐら鏡Ｓカメラ", &mStone->mPosition,
		    mGoTargetTime + mDemoWaitTime, 0.0f, true, nullptr, 0, nullptr,
		    JDrama::TFlagT<u16>(0));
	} else if (strcmp(getUnkF4(), "mirrorM") == 0) {
		// Translated: "Lean mirror M camera"
		SMSGetMarDirector()->fireStartDemoCamera(
		    "ぐらぐら鏡Ｍカメラ", &mStone->mPosition,
		    mGoTargetTime + mDemoWaitTime, 0.0f, true, nullptr, 0, nullptr,
		    JDrama::TFlagT<u16>(0));
	} else if (strcmp(getUnkF4(), "mirrorL") == 0) {
		// Translated: "Lean mirror L camera"
		SMSGetMarDirector()->fireStartDemoCamera(
		    "ぐらぐら鏡Ｌカメラ", &mStone->mPosition,
		    mGoTargetTime + mDemoWaitTime, 0.0f, true, nullptr, 0, nullptr,
		    JDrama::TFlagT<u16>(0));
	}
	MSBgm::stopTrackBGM(1, 10);
}

static s32 startCameraShakeSE(TMarDirector::DemoCallbackArg position, u32 state)
{
	if (state == 0) {
		const Vec* pos = reinterpret_cast<const Vec*>(position);
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_QUAKE, pos, 0, nullptr, 0,
		                                4);
	}
	return 0;
}

void TLeanMirror::controlGoTarget()
{
	MtxPtr nodeMtx = getModel()->getAnmMtx(0);
	TMtx34f rot;
	rot.identity();
	makeMtxRotByAxis(mFallAxis, mFallSpeed, rot);
	concatOnlyRotFromLeft(rot, nodeMtx, nodeMtx);

	if (!isStateTimerEngaged()) {
		mStone->putOnLight(this);
		if (mStone->mStoneLit[3]) {
			// Translated: "Sleeping boss Hanachan"
			TSleepBossHanachan* hanachan
			    = (TSleepBossHanachan*)JDrama::TNameRefGen::getInstance()
			          ->getRootNameRef()
			          ->search("居眠りボスハナチャン");
			if (hanachan != nullptr) {
				hanachan->startFall(mStone->mPosition.x,
				                    1100.0f + mStone->mPosition.y,
				                    mStone->mPosition.z);
			}
			SMSGetMarDirector()->fireStartDemoCamera(
			    "demohanatyan_cam01", nullptr, -1, 0.0f, true,
			    &startCameraShakeSE, PTR_U32(&mPosition), nullptr,
			    JDrama::TFlagT<u16>(0));
		} else {
			// Translated: "Sun stone lighting camera"
			SMSGetMarDirector()->fireStartDemoCamera(
			    "太陽石点灯カメラ", &mStone->mPosition, mDemoLightTime, 0.0f,
			    true, nullptr, 0, nullptr, JDrama::TFlagT<u16>(0));
		}
		mStateTimer = mDemoLightTime;
		mState      = STATE_DEMO;
		char trash[0x1c];
	}
}

void TLeanMirror::controlShake()
{
	if ((mHitsLeft != 0 ? true : false) && mBgmStarted
	    && SMS_IsMarioTouchGround4cm()
	    && SMS_GetMarioGrPlane()->mActor != this) {
		MSBgm::stopTrackBGM(1, 10);
		MSBgm::setTrackVolume(0, 1.0f, 10, 0);
		mBgmStarted = false;
	}

	if (!mShake.isZero()) {
		mShake.scale(mShakeDamping);

		MtxPtr nodeMtx = getModel()->getAnmMtx(0);
		JGeometry::TVec3<f32> axis(mShake.x, 0.0f, mShake.z);
		TMapObjBase::rotateVecByAxisY(&axis, 1.5707963f);
		f32 mag = MsSqrtf(mShake.x * mShake.x + mShake.z * mShake.z)
		          * mShakeSoundRate;

		TMtx34f rot;
		rot.identity();
		makeMtxRotByAxis(axis, mag, rot);
		concatOnlyRotFromLeft(rot, nodeMtx, nodeMtx);

		if (getModel()->getAnmMtx(0)[1][1] < mTiltLimit
		    && mShake.x * getModel()->getAnmMtx(0)[0][1]
		               + mShake.z * getModel()->getAnmMtx(0)[2][1]
		           > 0.0f) {
			f32 speed = fabsf(mShake.length());
			if (gpMSound->gateCheck(0x3849)) {
				MSoundSESystem::MSoundSE::startSoundActorWithInfo(
				    0x3849, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
			}
			mShake.scale(-mBounceRate);
			MTXCopy(getModel()->getBaseTRMtx(), getModel()->getAnmMtx(0));
		} else {
			MtxPtr src = getModel()->getAnmMtx(0);
			MtxPtr dst = getModel()->getBaseTRMtx();
			MTXCopy(src, dst);
		}
	}
	char trash[0x8];
}

void TLeanMirror::control()
{
	TMapObjBase::control();

	switch (mState) {
	case STATE_SHAKE: {
		controlShake();
		f32 speed = fabsf(mShake.length());
		if (gpMSound->gateCheck(0x3048)) {
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    0x3048, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		}
		break;
	}
	case STATE_GO_TARGET: {
		controlGoTarget();
		f32 speed = fabsf(mShake.length());
		if (gpMSound->gateCheck(0x304A)) {
			MSoundSESystem::MSoundSE::startSoundActorWithInfo(
			    0x304A, &mPosition, nullptr, speed, 0, 0, nullptr, 0, 4);
		}
		break;
	}
	case STATE_DEMO:
		if (!isStateTimerEngaged()) {
			TShiningStone* stone = mStone;
			if (stone->mLightNum < 3)
				MSBgm::setTrackVolume(0, 1.0f, 10, 0);
			if (stone->mEmitterRate > 0.0f)
				stone->mEmitter->setRate(stone->mEmitterRate);
			mState = STATE_DONE;
		}
		break;
	case STATE_DONE:
		break;
	}
}

void TLeanMirror::loadAfter()
{
	TMapObjBase::loadAfter();

	mStone = (TShiningStone*)JDrama::TNameRefGen::getInstance()
	             ->getRootNameRef()
	             ->search("ShiningStone");
	mStoneDir = mStone->mPosition - mPosition;
	mStoneDir.normalize();
}

u32 TLeanMirror::getSDLModelFlag() const { return 0; }

void TLeanMirror::initMapObj()
{
	TMapObjBase::initMapObj();

	mShakeSoundRate   = 0.03f;
	mShakeDamping     = 0.999f;
	mPlayerRate       = 0.0001f;
	mHipDropRate      = 1.0f;
	mEnemyRate        = 0.0002f;
	mSuperHipDropRate = 0.0001f;
	mTiltLimit        = 0.865f;
	mBounceRate       = 0.5f;

	if (strcmp(getUnkF4(), "mirrorS") == 0) {
		mTrampleRate = 0.002f;
		mHipDropRate = 1.0f;
		mTiltLimit   = 0.87f;
		mHitsLeft    = 1;
	} else if (strcmp(getUnkF4(), "mirrorM") == 0) {
		mTrampleRate = 0.004f;
		mHitsLeft    = 2;
	} else {
		mTrampleRate = 0.006f;
		mHitsLeft    = 3;
	}
}

void TLeanMirror::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	f32 size;
#ifdef TARGET_PC
	size = stream.readF32();
#else
	stream.read(&size, 4);
#endif
	mHalfSize = 100.0f * size * 0.5f;
	unk13C    = mHalfSize;

	if (gpMarDirector->unk7D == 1) {
		char buffer[0x40];
		stream.readString(buffer, 0x40);
#ifdef TARGET_PC
		mMarioMovePos.x = stream.readF32();
		mMarioMovePos.y = stream.readF32();
		mMarioMovePos.z = stream.readF32();
#else
		stream.read(&mMarioMovePos.x, 4);
		stream.read(&mMarioMovePos.y, 4);
		stream.read(&mMarioMovePos.z, 4);
#endif
	}

	TMirrorModelObj* mirror = new TMirrorModelObj;
	char path[0x40];
	char trash[0x30];
	snprintf(path, 0x40, "/scene/mapObj/%sTop.bmd", getUnkF4());
	mirror->init(path);
	mirror->unk28 = getModel();

	if (gpMarDirector->unk7D != 1)
		mState = STATE_DONE;
}

TLeanMirror::TLeanMirror(const char* name)
    : TMapObjBase(name)
    , mHalfSize(0.0f)
    , unk13C(0.0f)
    , mShakeSoundRate(0.0f)
    , mShakeDamping(0.0f)
    , mPlayerRate(0.0f)
    , mTrampleRate(0.0f)
    , mHipDropRate(0.0f)
    , mEnemyRate(0.0f)
    , mSuperHipDropRate(0.0f)
    , mStone(nullptr)
    , mFallSpeed(0.0f)
    , mHitsLeft(0)
    , mBgmStarted(false)
    , unk1AE(0)
{
	unk140.zero();
	mShake.zero();
	mStoneDir.zero();
	mFallAxis.zero();
	mMarioMovePos.zero();
}

void TShiningStone::putOnLight(TLiveActor* mirror)
{
	char trash[0x20];
	if (strcmp(mirror->getName(), "mirrorS") == 0) {
		mStoneActors[0]->setBck("shiningstonegreen");
		mStoneActors[0]->setBrk("shiningstonegreen");
		mStoneLit[0] = true;
	} else if (strcmp(mirror->getName(), "mirrorM") == 0) {
		mStoneActors[1]->setBck("shiningstoneblue");
		mStoneActors[1]->setBrk("shiningstoneblue");
		mStoneLit[1] = true;
	} else if (strcmp(mirror->getName(), "mirrorL") == 0) {
		mStoneActors[2]->setBck("shiningstonered");
		mStoneActors[2]->setBrk("shiningstonered");
		mStoneLit[2] = true;
	}

	switch (mLightNum) {
	case 0:
		mEmitter = gpMarioParticleManager->emit(0x143, &mPosition, 1, this);
		mEmitter->mChildSpawnRate = 3.0f;
		mEmitterRate              = 1.5f;
		if (gpMSound->gateCheck(0x2893))
			MSoundSESystem::MSoundSE::startSoundActor(0x2893, &mPosition, 0,
			                                          nullptr, 0, 4);
		break;
	case 1:
		mEmitter = gpMarioParticleManager->emit(0x144, &mPosition, 1, this);
		mEmitter->mChildSpawnRate = 0.4f;
		mEmitterRate              = 0.2f;
		if (gpMSound->gateCheck(0x2894))
			MSoundSESystem::MSoundSE::startSoundActor(0x2894, &mPosition, 0,
			                                          nullptr, 0, 4);
		break;
	case 2:
		mEmitter     = gpMarioParticleManager->emit(0x145, &mPosition, 1, this);
		mEmitterRate = 0.0f;
		if (gpMSound->gateCheck(0x2895))
			MSoundSESystem::MSoundSE::startSoundActor(0x2895, &mPosition, 0,
			                                          nullptr, 0, 4);
		break;
	}

	gpMarioParticleManager->emit(0x56, &mPosition, 0, nullptr);
	++mLightNum;
	if (mLightNum == 3) {
		mStoneActors[3]->setBck("shiningstonewhite");
		mStoneActors[3]->setBrk("shiningstonewhite");
		mStoneLit[3] = true;
	}
}

void TShiningStone::perform(u32 cue, JDrama::TGraphics* graphics)
{
	for (int i = 0; i < 4; ++i) {
		mStoneActors[i]->perform(cue, graphics);
		if (mLightNum > 0)
			gpMarioParticleManager->emit(0x143, &mPosition, 1, this);
		if (mLightNum > 1)
			gpMarioParticleManager->emit(0x144, &mPosition, 1, this);
		if (mLightNum > 2)
			gpMarioParticleManager->emit(0x145, &mPosition, 1, this);
	}
	mLightActor->perform(cue, graphics);
}

void TShiningStone::load(JSUMemoryInputStream& stream)
{
	TActor::load(stream);

	const char* paths[4] = {
		"/scene/mapObj/ShiningStoneGreen.bmd",
		"/scene/mapObj/ShiningStoneBlue.bmd",
		"/scene/mapObj/ShiningStoneRed.bmd",
		"/scene/mapObj/ShiningStoneWhite.bmd",
	};

	Mtx mtx;
	MtxPtr mtxPtr = mtx;
	MsMtxSetXYZRPH(mtxPtr, mPosition.x, mPosition.y, mPosition.z, mRotation.x,
	               mRotation.y, mRotation.z);

	mStoneActors = new MActor*[4];
	for (int i = 0; i < 4; ++i) {
		mStoneActors[i] = SMS_MakeMActorWithAnmData(
		    paths[i], gpMapObjManager->getMActorAnmData(), 3, 0x10020000);
		MTXCopy(mtxPtr, mStoneActors[i]->getModel()->getBaseTRMtx());

		// Translated: "Sun stone in mirror"
		TMirrorActor* mirror = new TMirrorActor("太陽石in鏡");
		mirror->init(mStoneActors[i]->getModel(), 0x1A);
	}

	mLightActor = SMS_MakeMActorWithAnmData("/scene/mapObj/ShiningStone.bmd",
	                                        gpMapObjManager->getMActorAnmData(),
	                                        3, 0x10020000);
	mLightActor->setBpk("shiningstone");
	mLightActor->setBtk("shiningstone");
	MTXCopy(mtx, mLightActor->getModel()->getBaseTRMtx());

	SMS_LoadParticle("/scene/mapObj/ShiningStone1.jpa", 0x143);
	SMS_LoadParticle("/scene/mapObj/ShiningStone2.jpa", 0x144);
	SMS_LoadParticle("/scene/mapObj/ShiningStone3.jpa", 0x145);
	SMS_LoadParticle("/scene/mapObj/ShiningStoneF.jpa", 0x56);
}

TShiningStone::TShiningStone(const char* name)
    : THitActor(name)
    , mLightNum(0)
    , mEmitter(nullptr)
    , mEmitterRate(0.0f)
{
	for (int i = 0; i < 4; ++i)
		mStoneLit[i] = false;
}

u32 TMammaBlockRotate::touchWater(THitActor*)
{
	if (isState(STATE_NORMAL)) {
		mRotation.y += mRotSpeed;
		if (mRotation.y > mRotEnd)
			mState = STATE_GO;
	}
	return 1;
}

void TMammaBlockRotate::control()
{
	TMapObjBase::control();

	switch (mState) {
	case STATE_NORMAL:
		if (mRotation.y > 0.0f)
			mRotation.y -= mRotReturnSpeed;
		else
			mRotation.y = 0.0f;
		break;
	case STATE_GO: {
		moveJoint(mJointUp->getJoint(), 0.0f, -mMapGoSpeed, 0.0f);
		moveJoint(mJointDown->getJoint(), 0.0f, -mMapGoSpeed, 0.0f);
		J3DTransformInfo& info = mJointUp->getJoint()->getTransformInfo();
		mMapModel->mModel->calc();
		JGeometry::TVec3<f32> offset(0.0f, info.mTranslate.y, 0.0f);
		mColDown->moveTrans(offset);
		offset.set(0.0f, info.mTranslate.y, 0.0f);
		mColUp->moveTrans(offset);
		if (info.mTranslate.y < 0.0f) {
			mStateTimer = mWaitTime;
			mState      = STATE_WAIT;
		}
		break;
	}
	case STATE_WAIT:
		if (!isStateTimerEngaged())
			mState = STATE_BACK;
		break;
	case STATE_BACK: {
		moveJoint(mJointUp->getJoint(), 0.0f, mMapBackSpeed, 0.0f);
		moveJoint(mJointDown->getJoint(), 0.0f, mMapBackSpeed, 0.0f);
		J3DTransformInfo& info = mJointUp->getJoint()->getTransformInfo();
		JGeometry::TVec3<f32> offset(0.0f, info.mTranslate.y, 0.0f);
		mColDown->moveTrans(offset);
		offset.set(0.0f, info.mTranslate.y, 0.0f);
		mColUp->moveTrans(offset);
		mMapModel->mModel->calc();
		J3DJoint* joint = mJointUp->getJoint();
		if (info.mTranslate.y > joint->getMax().y - joint->getMin().y)
			mState = STATE_NORMAL;
		break;
	}
	}
	char trash[0x28];
}

void TMammaBlockRotate::initMapObj()
{
	TMapObjBase::initMapObj();

	mMapModel = gpMap->getRootJointModel();
	mJointDown
	    = mMapModel->getChild(0)->getChild(0)->getChild(0)->getChild(1);
	J3DJoint* joint = mJointDown->getJoint();
	f32 height      = joint->getMax().y - joint->getMin().y;
	moveJoint(joint, 0.0f, height, 0.0f);
	JGeometry::TVec3<f32> offset(0.0f, height, 0.0f);
	char trash[0x78];
	mColDown->setUp();
	mColDown->moveTrans(offset);

	mJointUp
	    = mMapModel->getChild(0)->getChild(0)->getChild(0)->getChild(2);
	joint  = mJointUp->getJoint();
	height = joint->getMax().y - joint->getMin().y;
	moveJoint(joint, 0.0f, joint->getMax().y - joint->getMin().y, 0.0f);
	mColUp->setUp();
	offset.set(0.0f, height, 0.0f);
	mColUp->moveTrans(offset);
	mMapModel->mModel->calc();
}

void TMammaBlockRotate::load(JSUMemoryInputStream& stream)
{
	mColDown = new TMapCollisionMove();
	mColDown->init("/scene/mapObj/MammaBlockDown.col", 0, this);
	mColUp = new TMapCollisionMove();
	mColUp->init("/scene/mapObj/MammaBlockUp.col", 0, this);
	TMapObjBase::load(stream);
}

TMammaBlockRotate::TMammaBlockRotate(const char* name)
    : TMapObjBase(name)
    , mJointDown(nullptr)
    , mJointUp(nullptr)
    , mColDown(nullptr)
    , mColUp(nullptr)
{
}

void TMammaYacht::control()
{
	TMapObjBase::control();

	if (mGroundPlane->isWaterSurface()) {
		mPosition.y = mInitialPosition.y
		              + gpMapObjWave->getWaveHeight(mPosition.x, mPosition.z);
		mFlag->mPosition.y = mPosition.y - 50.0f;
	}
}

void TMammaYacht::initMapObj()
{
	TMapObjBase::initMapObj();

	// Translated: "Flag"
	mFlag = new TMapObjFlag("旗");
	mFlag->mPosition.set(2.0f + mPosition.x, 1315.0f + mPosition.y - 190.0f,
	                     mPosition.z - 15.0f);
	mFlag->mRotation.set(0.0f, 180.0f, 0.0f);
	mFlag->mScaling.set(1.0f, 2.5f, 3.8f);
	mFlag->init("MammaYacht00");
}

void TSandBird::control()
{
	char trash[0x50];
	TJointCoin::control();

	if (gpMSound->gateCheck(0x217C)) {
		MSoundSESystem::MSoundSE::startSoundActor(0x217C, &mPosition, 0,
		                                          nullptr, 0, 4);
	}
	if (gpMSound->gateCheck(0x217D)) {
		MSoundSESystem::MSoundSE::startSoundSystemSE(0x217D, 0, nullptr, 0);
	}

	for (int i = 0; i < unk13C; ++i) {
		if (unk140[i]->isActorType(0x2000000E)
		    || unk140[i]->isActorType(0x40000023)) {
			gpMarioParticleManager->emitAndBindToPosPtr(
			    0x159, &unk140[i]->mPosition, 1, unk140[i]);
			gpMarioParticleManager->emitAndBindToPosPtr(
			    0x15A, &unk140[i]->mPosition, 1, unk140[i]);
		}
	}

	bool inDemo             = true;
	CPolarSubCamera* camera = gpCamera;
	if (!camera->isSimpleDemoCamera()) {
		bool reproducing
		    = camera->mMode == CAMERA_MODE_REPRODUCE_DEMO ? true : false;
		if (!reproducing)
			inDemo = false;
	}

	if (!(inDemo ? true : false) && !mBalloonShown) {
		const TLiveActor* plane = (*gpMarioGroundPlane)->mActor;
		if (plane != nullptr && plane->isActorType(0x400002C9)) {
			gpMarDirector->mConsole->startAppearBalloon(0xE002F, false);
			mStateTimer    = 0x960;
			mBalloonShown = true;
		}
	}

	if (!mBalloonHidden && mBalloonShown && !isStateTimerEngaged()) {
		gpMarDirector->mConsole->startDisappearBalloon(0xE002F, false);
		mBalloonHidden = true;
	}
}

TMapObjBase* TSandBird::makeObjFromJointName(const char* name, u16 index)
{
	TMapObjBase* obj = TJointCoin::makeObjFromJointName(name, index);
	if (obj != nullptr)
		return obj;

	if (strstr(name, "none") == nullptr)
		return makeObj("SandBirdBlock", index);
	return nullptr;
}

bool TSandBird::nameIsObj(const char* name)
{
	return strstr(name, "none") == nullptr ? true : false;
}

void TSandBird::initMapObj()
{
	TJointCoin::initMapObj();
	SMS_LoadParticle("/scene/map/map/ms_sunadori_a.jpa", 0x159);
	SMS_LoadParticle("/scene/map/map/ms_sunadori_b.jpa", 0x15A);
}

TSandBird::TSandBird(const char* name)
    : TJointCoin(name)
    , mBalloonShown(false)
    , mBalloonHidden(false)
{
}

void TGoalWatermelon::touchActor(THitActor* actor)
{
	if (isState(1) && actor->isActorType(0x400000D0)) {
		mWatermelon = (TMapObjBase*)actor;
		mWatermelon->mMActor->setBck("watermelon_shrink");
		mWatermelon->offMapObjFlag(0x100);

		JGeometry::TVec3<f32> zero(0.0f, 0.0f, 0.0f);
		mWatermelon->mVelocity = zero;

		// Translated: "Watermelon goal camera"
		gpMarDirector->fireStartDemoCamera(
		    "スイカゴールカメラ", &mWatermelon->mPosition, -1, 0.0f, true,
		    nullptr, 0, nullptr, JDrama::TFlagT<u16>(0));
		mState = 2;
	}
	char trash[0xc];
}

void TGoalWatermelon::control()
{
	TMapObjBase::control();

	switch (mState) {
	case 1:
		break;
	case 2:
		if (mWatermelon->animIsFinished()) {
			// Translated: "Shine (for the ghost watermelon)", "Watermelon Shine
			// camera"
			gpItemManager->makeShineAppearWithDemoOffset(
			    "シャイン（お化けスイカ用）", "スイカシャインカメラ", 0.0f, 0.0f,
			    0.0f);
			mState = 3;
		}
		break;
	case 3:
		break;
	}
}

void TGoalWatermelon::loadAfter()
{
	char trash[8];
	TMapObjBase::loadAfter();
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);

	// Translated: "Shine (for the ghost watermelon)"
	mShine = (TMapObjBase*)JDrama::TNameRefGen::getInstance()
	             ->getRootNameRef()
	             ->search("シャイン（お化けスイカ用）");
	mShine->mPosition.set(mShinePos);
	mShine->appear();
}

void TGoalWatermelon::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	char buffer[0x20];
	char trash[0x10];
	stream.readString(buffer, 0x20);
#ifdef TARGET_PC
	mShinePos.x = stream.readF32();
	mShinePos.y = stream.readF32();
	mShinePos.z = stream.readF32();
#else
	stream.read(&mShinePos.x, 4);
	stream.read(&mShinePos.y, 4);
	stream.read(&mShinePos.z, 4);
#endif
}

TGoalWatermelon::TGoalWatermelon(const char* name)
    : TMapObjBase(name)
    , mShine(nullptr)
    , mWatermelon(nullptr)
{
	mShinePos.zero();
}

void TMammaMirrorMapOperator::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x20];
	if (!(cue & CUE_CALC_ANIM))
		return;

	const int& mirrorIdx = gpMirrorModelManager->unk18;
	if (gpMirrorModelManager->isUnk18Present()) {
		const JGeometry::TVec3<f32>& cameraPos
		    = gpMirrorModelManager->unk24->unk98;
		const JGeometry::TVec3<f32>& mirrorPos = mMirrorPos[mirrorIdx];
		f32 mirrorDist = cameraPos.distance(mirrorPos);
		for (int i = 0; i < 8; ++i) {
			f32 dist = cameraPos.distance(mJointPos[i]);
			if (dist > mJointRadius[i] || dist > mirrorDist) {
				if (mJointHidden[i]) {
					SMS_ShowJoint(mJoints[i]->getMesh(), true);
					mJointHidden[i] = false;
				}
			} else if (!mJointHidden[i]) {
				SMS_ShowJoint(mJoints[i]->getMesh(), false);
				mJointHidden[i] = true;
			}
		}
	} else {
		for (int i = 0; i < 8; ++i) {
			if (!mJointHidden[i]) {
				SMS_ShowJoint(mJoints[i]->getMesh(), false);
				mJointHidden[i] = true;
			}
		}
	}
}

void TMammaMirrorMapOperator::loadAfter()
{
	char trash[0x10];
	JDrama::TPlacement* mirrorS = (JDrama::TPlacement*)
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->search("mirrorS");
	mMirrorPos[0].set(mirrorS->mPosition);

	JDrama::TPlacement* mirrorM = (JDrama::TPlacement*)
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->search("mirrorM");
	mMirrorPos[1].set(mirrorM->mPosition);

	JDrama::TPlacement* mirrorL = (JDrama::TPlacement*)
	    JDrama::TNameRefGen::getInstance()->getRootNameRef()->search("mirrorL");
	mMirrorPos[2].set(mirrorL->mPosition);

	// Translated: "Terrain in the mirror"
	TMapStaticObj* terrain = (TMapStaticObj*)JDrama::TNameRefGen::getInstance()
	                             ->getRootNameRef()
	                             ->search("鏡内地形");
	J3DJoint* joint = terrain->getModelData()->getJointNodePointer(2);
	for (int i = 0; i < 8; ++i) {
		mJoints[i] = joint;

		mJointPos[i].set(0.5f * (joint->getMax().x + joint->getMin().x),
		                 0.5f * (joint->getMax().y + joint->getMin().y),
		                 0.5f * (joint->getMax().z + joint->getMin().z));

		f32 halfX = 0.5f * (joint->getMax().x - joint->getMin().x);
		f32 halfZ = 0.5f * (joint->getMax().z - joint->getMin().z);
		if (halfX > halfZ)
			mJointRadius[i] = halfX;
		else
			mJointRadius[i] = halfZ;

		mJointRadius[i] += 2000.0f;
		if (mJointRadius[i] > 3000.0f)
			mJointRadius[i] = 3000.0f;

		joint = (J3DJoint*)joint->getYounger();
	}
}

TMammaMirrorMapOperator::TMammaMirrorMapOperator(const char* name)
    : TViewObj(name)
{
	for (int i = 0; i < 8; ++i) {
		mJoints[i] = nullptr;
		mJointPos[i].zero();
		mJointRadius[i] = 0.0f;
		mJointHidden[i] = false;
	}
	for (int i = 0; i < 3; ++i)
		mMirrorPos[i].zero();
}

u32 TSandEgg::getSDLModelFlag() const { return 0; }
