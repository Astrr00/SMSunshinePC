#include <MoveBG/MapObjFence.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjMessenger.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <math.h>
#ifdef TARGET_PC
#include <string.h> // the MSL header is only here to match sinit/bss order and clashes with the CRT
#else
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

const char cDirtyFileName[] = "/scene/map/pollution/H_ma_rak.bti";
const char cDirtyTexName[]  = "H_ma_rak_dummy";

f32 TRevolvingFenceInner::mSpeed = 4.0f;

f32 TFenceWater::mWaterAccel     = 2.1f;
f32 TFenceWater::mBackSpeed      = 3.0f;
u32 TFenceWater::mTurnedWaitTime = 0x258;

f32 TRailFence::mFallHeight = 50000.0f;
u32 TRailFence::mWaitTime   = 0xF0;

BOOL TFence::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		startBck("fence_normal_shake");
		return TRUE;
	}
	return FALSE;
}

void TFence::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);
	if (strcmp(unkF4, "fence3x3") != 0) {
		if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
			mMapCollisionManager->init("fence_normal_v_tool", 0, nullptr);
		else
			mMapCollisionManager->init("fence_h_tool", 0, nullptr);
	} else {
		if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
			mMapCollisionManager->init("fence_half_v_tool", 0, nullptr);
		else
			mMapCollisionManager->init("fence_half_h_tool", 0, nullptr);
	}
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);
}

void TFence::initMapObj()
{
	if (strstr(unkF4, "bamboo"))
		unk138 = 1;
	TMapObjBase::initMapObj();
}

BOOL TRevolvingFenceOuter::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		startBck("fence_revolve_outer_shake");
		unk13C->startBck("fence_revolve_inner_shake");
		return TRUE;
	}
	return FALSE;
}

void TRevolvingFenceOuter::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);
	if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
		mMapCollisionManager->init("fence_revolve_outer_v_tool", 0, nullptr);
	else
		mMapCollisionManager->init("fence_revolve_outer_h_tool", 0, nullptr);
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	unk13C = unk138 ? TMapObjBaseManager::newAndRegisterObj(
	                      "bambooFence_revolve_inner", mPosition, mRotation)
	                : TMapObjBaseManager::newAndRegisterObj(
	                      "fence_revolve_inner", mPosition, mRotation);
	unk13C->appear();
}

BOOL TRevolvingFenceInner::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x38];
	if (message == HIT_MESSAGE_SUPER_HIP_DROP && !unk140) {
		if (isState(1)) {
			gpMSound->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE1, &mPosition, 0,
			                          nullptr, 0, 4);
			mState = 3;
			startBck("fence_revolve_inner_roll_down");
			offMapObjFlag(MAP_OBJ_FLAG_UNK100);
			return TRUE;
		} else if (isState(2)) {
			gpMSound->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE2, &mPosition, 0,
			                          nullptr, 0, 4);
			mState = 4;
			startBck("fence_revolve_inner_roll_up");
			offMapObjFlag(MAP_OBJ_FLAG_UNK100);
			return TRUE;
		}
	}

	if (message == HIT_MESSAGE_SUPER_HIP_DROP && unk140) {
		f32 yaw   = 180.0f * (getRotYFromAxisZ(SMS_GetMarioPos()) / 3.14f)
		            + mInitialRotation.y;
		f32 angle = yaw;
		while (angle >= 180.0f)
			angle -= 360.0f;
		while (angle < -180.0f)
			angle += 360.0f;

		if ((-180.0f < angle && angle < -90.0f)
		    || (0.0f < angle && angle < 90.0f)) {
			gpMSound->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE1, &mPosition, 0,
			                          nullptr, 0, 4);
			if (isState(1))
				mState = 3;
			else
				mState = 4;
		} else {
			gpMSound->startSoundActor(MSD_SE_OBJ_FENCE_REVERSE2, &mPosition, 0,
			                          nullptr, 0, 4);
			if (isState(1))
				mState = 5;
			else
				mState = 6;
		}
		return TRUE;
	}
	return FALSE;
}

// NOTE: the result of MsWrap is discarded, as in the retail code. This helper is
// inlined into controlWall; MsWrap and MsMtxSetRotY stay out of line only
// because they are nested one inline level deeper.
inline void TRevolvingFenceInner::calcCurrentMtx()
{
	MsWrap(mRotation.y, 0.0f, 360.0f);
	MtxPtr mtx = getModel()->getAnmMtx(0);
	MsMtxSetRotY(mtx, mRotation.y);
	mtx[0][3] = mPosition.x;
	mtx[1][3] = mPosition.y - mYOffset;
	mtx[2][3] = mPosition.z;
}

void TRevolvingFenceInner::controlWall()
{
	char trash[0x50];
	switch (mState) {
	case 1:
	case 2:
		break;
	case 3:
		unk13C += mSpeed;
		if (unk13C > 180.0f) {
			unk13C      = 180.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 2;
		}
		mRotation.y = unk13C + mInitialRotation.y;
		calcCurrentMtx();
		break;
	case 4:
		unk13C += mSpeed;
		if (unk13C > 360.0f) {
			unk13C      = 0.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 1;
		}
		mRotation.y = unk13C + mInitialRotation.y;
		calcCurrentMtx();
		break;
	case 5:
		unk13C -= mSpeed;
		if (unk13C < -180.0f) {
			unk13C      = 180.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 2;
		}
		mRotation.y = unk13C + mInitialRotation.y;
		calcCurrentMtx();
		break;
	case 6:
		unk13C -= mSpeed;
		if (unk13C < 0.0f) {
			unk13C      = 0.0f;
			mRotation.y = unk13C + mInitialRotation.y;
			mState      = 1;
		}
		mRotation.y = unk13C + mInitialRotation.y;
		calcCurrentMtx();
		break;
	}
}

void TRevolvingFenceInner::controlGroundRoof()
{
	char trash[0x10];
	switch (mState) {
	case 1:
	case 2:
		break;
	case 3:
	case 5:
		if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			mState = 2;
			mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);
			mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
			mMActor->calc();
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
		}
		break;
	case 4:
	case 6:
		if (mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
			mState = 1;
			mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);
			mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
			mMActor->calc();
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
		}
		break;
	}
}

void TRevolvingFenceInner::setGroundCollision()
{
	if (SMS_GetYoshi()->isHatched()
	    && mPosition.x - mBodyRadius < SMS_GetYoshi()->getTranslation().x
	    && mPosition.x + mBodyRadius > SMS_GetYoshi()->getTranslation().x
	    && mPosition.z - mBodyRadius < SMS_GetYoshi()->getTranslation().z
	    && mPosition.z + mBodyRadius > SMS_GetYoshi()->getTranslation().z) {
		TPosition3f mtx;
		mtx.set(getModel()->getAnmMtx(0));
		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->moveMtx(mtx);
	}
	TMapObjBase::setGroundCollision();
}

void TRevolvingFenceInner::control()
{
	TMapObjBase::control();
	if (unk140)
		controlWall();
	else
		controlGroundRoof();
}

void TRevolvingFenceInner::initMapCollisionData()
{
	mMapCollisionManager = new TMapCollisionManager(1, "mapObj", this);
	if (fabsf(mRotation.x) < 80.0f && fabsf(mRotation.z) < 80.0f)
		mMapCollisionManager->init("fence_revolve_inner_v_tool", 1, nullptr);
	else
		mMapCollisionManager->init("fence_revolve_inner_h_tool", 1, nullptr);
}

void TRevolvingFenceInner::initMapObj()
{
	if (strstr(unkF4, "bamboo"))
		unk138 = 1;
	TMapObjBase::initMapObj();

	if (fabsf(mRotation.x) < 1.0f && fabsf(mRotation.z) < 1.0f)
		unk140 = 1;
	else
		unk140 = 0;

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);
}

void TFenceWater::draw() const { }

BOOL TFenceWater::receiveMessage(THitActor* sender, u32 message)
{
	if (!isState(3) && message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		unk13C = mWaterAccel;
		if (unk13C > 0.0f)
			changeStatusToGo();
		return TRUE;
	}
	return FALSE;
}

void TFenceWater::changeStatusToGo()
{
	int newState;
	newState = 2;
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_WATER_FENCE_FW, &mPosition, 0,
	                                nullptr, 0, 4);
	mState = newState;
}

void TFenceWater::changeStatusToWait()
{
	unk140 = 0.0f;
	unk13C = 0.0f;
	mState = 1;
}

void TFenceWater::controlRotation()
{
	char trash[0x10];
	switch (mState) {
	case 1:
		break;
	case 2:
		unk140 -= unk13C;
		if (unk140 <= -90.0f) {
			unk140 = -90.0f;
			unk13C = 0.0f;
			mState = 3;
			startStateTimer(mTurnedWaitTime);
		}
		break;
	case 3:
		if (!isStateTimerEngaged()) {
			gpMSound->startSoundActor(MSD_SE_OBJ_WATER_FENCE_REV, &mPosition, 0,
			                          nullptr, 0, 4);
			unk13C = mBackSpeed;
			mState = 4;
		}
		break;
	case 4:
		unk140 += unk13C;
		if (unk140 >= 0.0f)
			changeStatusToWait();
		break;
	}
}

void TFenceWater::control()
{
	char trash[8];
	TMapObjBase::control();
	controlRotation();

	f32 angle = unk140 + mInitialRotation.y;
	while (angle >= 360.0f)
		angle -= 360.0f;
	while (angle < 0.0f)
		angle += 360.0f;
	mRotation.y = angle;

	unk144->mPosition.x = mPosition.x + 500.0f * MsCos(mRotation.y);
	unk144->mPosition.z = mPosition.z - 500.0f * MsSin(mRotation.y);
}

void TFenceWater::initMapCollisionData()
{
	TMapObjBase::initMapCollisionData();
}

void TFenceWater::initMapObj()
{
	if (strstr(unkF4, "bamboo"))
		unk138 = 1;
	TMapObjBase::initMapObj();

	unk144        = new TMapObjMessenger("地形オブジェメッセンジャー");
	unk144->unk68 = this;
	u32 actorType = mActorType;
	unk144->initHitActor(actorType, 1, 0, 0.0f, 0.0f, 100.0f, 300.0f);
	unk144->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk144->mPosition.set(mPosition.x, mPosition.y - 150.0f, mPosition.z);
	JDrama::TViewObjPtrListT<THitActor>* group
	    = static_cast<JDrama::TViewObjPtrListT<THitActor>*>(
	        JDrama::TNameRefGen::search("オブジェクトグループ"));
	group->getChildren().push_back(unk144);
}

void TFenceWaterH::control()
{
	TMapObjBase::control();
	controlRotation();

	mRotation.z = MsWrap(unk140 + mInitialRotation.z, 0.0f, 360.0f);

	TPosition3f rotY;
	rotY.identity();
	rotY.setEular(0.0f, 0.017453294f * mRotation.y, 0.0f);

	TPosition3f rotZ;
	rotZ.identity();
	rotZ.setEular(0.0f, 0.0f, 0.017453294f * mRotation.z);

	MTXConcat(rotY, rotZ, rotY);
	rotY.setTrans(mPosition.x, mPosition.y, mPosition.z);
	getModel()->setAnmMtx(0, rotY);

	// only x and y are updated here (z is set once in initMapObj)
	unk144->mPosition.x = mPosition.x;
	unk144->mPosition.y = mPosition.y - 150.0f;
}

void TFenceWaterH::changeStatusToGo()
{
	TFenceWater::changeStatusToGo();
	setUpMapCollision(1);
}

void TFenceWaterH::changeStatusToWait()
{
	TFenceWater::changeStatusToWait();
	setUpMapCollision(0);
}

BOOL TRailFence::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	if (message == HIT_MESSAGE_SUPER_HIP_DROP) {
		gpMSound->startSoundActor(MSD_SE_OBJ_MVING_FENCT_PNCH, &mPosition, 0,
		                          nullptr, 0, 4);
		setUpMapCollision(1);
		offMapObjFlag(MAP_OBJ_FLAG_UNK100);
		mState = 2;
		return TRUE;
	}
	return FALSE;
}

inline void TRailFence::falling()
{
	JGeometry::TVec3<f32> velocity = mVelocity;
	mPosition.y += velocity.y;
	mVelocity.y -= mGravity;
	if (mVelocity.y < -100.0f)
		mVelocity.y = -100.0f;

	if (mPosition.y < mInitialPosition.y - mFallHeight) {
		mPosition.x = mInitialPosition.x;
		mPosition.y = mInitialPosition.y;
		mPosition.z = mInitialPosition.z;
		setUpMapCollision(0);
		unk13C->setTo(
		    unk13C->unk0->findNearestNodeIndex(mPosition, 0xffffffff));
		makeObjAppeared();
		calcRootMatrix();
		getModel()->calc();
		onMapObjFlag(MAP_OBJ_FLAG_UNK100);
	}
}

void TRailFence::goOnRail()
{
	if (unk13C->unk0 == nullptr)
		return;

	JGeometry::TVec3<f32> toTarget = unk13C->getCurrentPos();
	toTarget.sub(mPosition);
	char trash[0x48];
	if (toTarget.squared() < 50.0f) {
		TRailNode* node = unk13C->unk0->unk0[unk13C->mCurrIdx].unk0;
		if (node->mConnectionNum == 0 && node->mFlags & 0x8) {
			gpMSound->startSoundActor(MSD_SE_OBJ_MVING_FENCT_SET, &mPosition, 0,
			                          nullptr, 0, 4);
			mStateTimer = mWaitTime;
			startAnim(1);
			mState = 3;
			return;
		}

		unk13C->moveToShortestNext();
		// retail quirk: the new direction is the node position itself, not
		// the offset from mPosition
		toTarget.set(unk13C->getCurrentPos());
	}

	gpMSound->startSoundActor(MSD_SE_OBJ_MVING_FENCE_MOVE, &mPosition, 0,
	                          nullptr, 0, 4);
	VECNormalize(&toTarget, &toTarget);
	toTarget.scale(unk140);
	mLinearVelocity.add(toTarget);
}

void TRailFence::control()
{
	char trash[0x28];
	TMapObjBase::control();

	switch (mState) {
	case 1:
		break;
	case 2:
		goOnRail();
		break;
	case 3:
		if (!isStateTimerEngaged()) {
			removeMapCollision();
			gpMSound->startSoundActor(MSD_SE_OBJ_SUPERBLOCK_BREAK, &mPosition,
			                          0, nullptr, 0, 4);
			mState = 4;
		}
		break;
	case 4:
		falling();
		break;
	}
}

void TRailFence::initMapCollisionData() { TMapObjBase::initMapCollisionData(); }

void TRailFence::load(JSUMemoryInputStream& stream)
{
	char trash[8];
	TMapObjBase::load(stream);

	char graphName[0x40];
	stream.readString(graphName, 0x40);
	TGraphWeb* graph = gpConductor->getGraphByName(graphName);
	if (graph != nullptr && !graph->isDummy()) {
		unk13C->unk0 = graph;
		unk13C->setTo(graph->findNearestNodeIndex(mPosition, 0xffffffff));
	}

	unk140   = 8.0f;
	mGravity = 0.3f;
}

TRailFence::TRailFence(const char* name)
    : TFence(name)
{
	unk13C = new TGraphTracer;
	unk140 = 0.0f;
}
