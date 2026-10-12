#include <MoveBG/MapObjMonte.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <System/MarDirector.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Watergun.hpp>
#include <Player/Yoshi.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <dolphin/gx.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

f32 THangingBridgeBoard::mMarioAccelY        = 0.15f;
f32 THangingBridgeBoard::mMarioHipDropAccelY = 2.0f;
f32 THangingBridgeBoard::mReturnAccelRate    = 0.005f;
f32 THangingBridgeBoard::mSpeedDownRate      = 0.98f;
f32 THangingBridgeBoard::mRopeWidthX         = 10.0f;
f32 THangingBridgeBoard::mRopeWidthZ         = 7.0f;
f32 THangingBridgeBoard::mTexPosRate         = 0.01f;

f32 THangingBridge::mRopeWidthBetweenBoards  = 10.0f;
f32 THangingBridge::mRopeWidthBetweenBoardsY = 10.0f;
int THangingBridge::mPointNumBetweenBoards   = 10;
f32 THangingBridge::mBetweenBoardsTexPosRate = 0.01f;
f32 THangingBridge::mRopeHeight;

f32 TSwingBoard::mBoardWidth      = 315.0f;
f32 TSwingBoard::mRopeWidthX      = 10.0f;
f32 TSwingBoard::mRopeWidthZ      = 7.0f;
f32 TSwingBoard::mTexPosRate      = 0.01f;
f32 TSwingBoard::mReturnAccelRate = 0.0001f;
f32 TSwingBoard::mSpeedDownRate   = 0.998f;

f32 TFluff::mScaleUpSpeed   = 0.05f;
f32 TFluff::mScaleDownSpeed = 0.01f;

f32 TFluffManager::mWindMin = 1.0f;

inline f32 TFluffManager::getRandomX() const
{
	return mRangeX * (2.0f * MsRandF() - 1.0f);
}

inline f32 TFluffManager::getRandomZ() const
{
	return mRangeZ * (2.0f * MsRandF() - 1.0f);
}

inline TFluff* TFluffManager::newFluff(const char* name)
{
	TFluff* fluff = new TFluff(name);
	fluff->initAndRegister("Fluff");
	fluff->mManager = this;
	return fluff;
}

inline void TFluffManager::findNextFluff()
{
	f32 range = 3000.0f;
	for (int i = 3; i < mFluffCapacity; ++i) {
		TFluff* fluff = mFluffs[i];
		if (!fluff->mIsLeader && fluff->mHeldObject == nullptr) {
			if (fluff->mPosition.distance(*gpMarioPos) > range) {
				mNextFluff = mFluffs[i];
				mFluffs[i]->kill();
				break;
			}
		}
	}
}

inline void TFluffManager::registerNextFluff(TFluff* fluff)
{
	if (mNextFluff == nullptr) {
		mNextFluff = fluff;
		mNextFluff->makeObjDead();
	}
}

inline void TFluffManager::setUpNextFluff()
{
	mCurrentFluff = mNextFluff;
	mCurrentFluff->mRotation.set(mRotation);
	mCurrentFluff->mInitialRotation = mRotation;
	mCurrentFluff->appear();
	mCurrentFluff->mPosition.set(mPosition);
	mCurrentFluff->mInitialPosition = mPosition;
	mCurrentFluff->mRotation.set(0.0f, 0.0f, 0.0f);
	mCurrentFluff->mInitialRotation = mCurrentFluff->mRotation;
	mCurrentFluff->mSwayAngle       = 0.0f;
	mCurrentFluff->mWindRatio       = 1.0f;
	mCurrentFluff->mIsLeader        = true;
	mNextFluff                      = nullptr;
	mState                          = 1;
}

TFluffManager::TFluffManager(const char* name)
    : TMapObjBase(name)
    , mRangeX(0.0f)
    , mRangeZ(0.0f)
    , mDropHeight(0.0f)
    , mWindFrames(0)
    , mWindDecay(0.0f)
    , mCurrentFluff(nullptr)
    , mNextFluff(nullptr)
    , mFluffNum(0)
    , mFluffCapacity(0)
{
	mWind.set(0.0f, 0.0f, 0.0f);
}

void TFluffManager::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream >> mDropHeight;

	f32 windScale;
	stream >> windScale;
	windScale *= 0.01f;

	stream >> mWindFrames;

	mRangeX    = 5000.0f;
	mRangeZ    = 5000.0f;
	mWindDecay = 0.998f;

	TPosition3f mtx;
	MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, mRotation.x, mRotation.y,
	               mRotation.z);

	mWind.set(0.0f, 0.0f, 1.0f);
	mtx.mult(mWind, mWind);
	mWind.scale(windScale);
}

void TFluffManager::loadAfter()
{
	mFluffNum      = 0;
	mFluffCapacity = 0x20;
	mFluffs        = new TFluff*[mFluffCapacity];

	mCurrentFluff            = newFluff("１つ目のわた毛");
	mCurrentFluff->mIsLeader = true;
	mCurrentFluff->appear();
	mCurrentFluff->mPosition.set(mPosition);
	mCurrentFluff->mRotation.set(mRotation);
	JGeometry::TVec3<f32> offset;
	offset.set(getRandomX(), mPosition.y * MsRandF(), getRandomZ());
	mCurrentFluff->mInitialPosition = offset;
	mFluffs[mFluffNum]              = mCurrentFluff;
	++mFluffNum;

	mNextFluff = newFluff("２つ目のわた毛");
	mNextFluff->mPosition.set(mPosition);
	mNextFluff->mRotation.set(mRotation);
	offset.set(getRandomX(), mPosition.y * MsRandF(), getRandomZ());
	mNextFluff->mInitialPosition = offset;
	mNextFluff->makeObjDead();
	mFluffs[mFluffNum] = mNextFluff;
	++mFluffNum;

	for (int i = 2; i < mFluffCapacity; ++i) {
		mFluffs[mFluffNum] = newFluff("わた毛");
		mFluffs[mFluffNum]->appear();
		++mFluffNum;
	}
}

void TFluffManager::control()
{
	switch (mState) {
	case 1:
		if (mNextFluff == nullptr) {
			if (mCurrentFluff->mPosition.y - 100.0f < mPosition.y - mDropHeight)
				findNextFluff();
		}
		if (mCurrentFluff->mPosition.y < mPosition.y - mDropHeight) {
			gpMSound->startSoundActor(MSD_SE_OBJ_WATAGE_WIND,
			                          &mCurrentFluff->mPosition, 0, nullptr, 0,
			                          4);
			mStateTimer = mWindFrames;
			mState      = 2;
		}
		break;
	case 2:
		gpMapObjManager->unkD0.set(gpMapObjManager->unkD0.x + mWind.x,
		                           gpMapObjManager->unkD0.y + mWind.y,
		                           gpMapObjManager->unkD0.z + mWind.z);
		if (!isStateTimerEngaged())
			mState = 3;
		break;
	case 3: {
		JGeometry::TVec3<f32> wind;
		wind.set(gpMapObjManager->unkD0);
		wind.scale(mWindDecay);
		if (fabsf(wind.x) < mWindMin && fabsf(wind.y) < mWindMin
		    && fabsf(wind.z) < mWindMin) {
			wind.set(0.0f, 0.0f, 0.0f);
			setUpNextFluff();
		}
		{
			TMapObjManager* manager = gpMapObjManager;
			manager->unkD0.x        = wind.x;
			manager->unkD0.y        = wind.y;
			manager->unkD0.z        = wind.z;
		}
	} break;
	}
}

TFluff::TFluff(const char* name)
    : TMapObjBase(name)
    , mSwayRadius(0.0f)
    , mFallSpeed(0.0f)
    , mHeadingSin(0.0f)
    , mHeadingCos(0.0f)
    , mSwayAngle(0.0f)
    , mSwaySpeed(0.0f)
    , mWindRatio(0.0f)
    , mWaterPushRate(1.0f)
    , mVelocityDamping(0.95f)
    , mManager(nullptr)
    , mIsLeader(false)
{
	mWindOffset.zero();
}

void TFluff::initMapObj()
{
	TMapObjBase::initMapObj();
	mSwayRadius = 300.0f;
	mFallSpeed  = 0.5f;
}

void TFluff::appear()
{
	makeObjAppeared();

	mPosition.set(mManager->getRandomX(), mManager->mPosition.y * MsRandF(),
	              mManager->getRandomZ());
	mInitialPosition = mPosition;
	mScaling.set(0.0001f, 0.0001f, 0.0001f);
	mWindOffset.zero();
	mSwayAngle  = 0.0f;
	mWindRatio  = MsRandF() * 0.8f + 0.2f;
	mHeadingSin = sinf(3.14f * mRotation.y / 180.0f);
	mHeadingCos = cosf(3.14f * mRotation.y / 180.0f);
	mSwayAngle  = 360.0f * MsRandF();
	mSwaySpeed  = 0.3f;
	mState      = 2;
}

void TFluff::control()
{
	TMapObjBase::control();
	move();

	switch (mState) {
	case 2:
		mScaling.x += mScaleUpSpeed;
		mScaling.y += mScaleUpSpeed;
		mScaling.z += mScaleUpSpeed;
		if (mScaling.x > 1.0f) {
			mScaling.set(1.0f, 1.0f, 1.0f);
			setObjHitData(0);
			mState = 1;
		}
		break;
	case 1: {
		mGroundHeight = gpMap->checkGround(mPosition, &mGroundPlane);
		JGeometry::TVec3<f32> velocity = mVelocity;
		char trash[0x14]; // unused stack slot (present in the retail frame)
		if (velocity.y < 0.0f) {
			if (mGroundHeight > mPosition.y - mFallSpeed
			    || mPosition.y < -1000.0f)
				kill();
		}
		if (gpMap->isTouchedOneWall(mPosition.x, mPosition.y, mPosition.z,
		                            100.0f))
			kill();
		if (mPosition.x < -14848.0f || 14848.0f < mPosition.x
		    || mPosition.z < -19968.0f || 19968.0f < mPosition.z)
			kill();
	} break;
	case 3:
		mScaling.x -= mScaleDownSpeed;
		mScaling.y -= mScaleDownSpeed;
		mScaling.z -= mScaleDownSpeed;
		if (mScaling.x < 0.1f) {
			gpMarioParticleManager->emitAndBindToPosPtr(0xE5, &mPosition, 0,
			                                            nullptr);
			gpMSound->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0,
			                          nullptr, 0, 4);
			mScaling.set(0.0001f, 0.0001f, 0.0001f);
			mStateTimer = 0xF0;
			mState      = 4;
		}
		break;
	case 4:
		if (!isStateTimerEngaged()) {
			appear();
			mRotation.set(0.0f, 360.0f * MsRandF(), 0.0f);
			mInitialRotation = mRotation;
			mIsLeader        = false;
			mManager->registerNextFluff(this);
		}
		break;
	}
}

void TFluff::kill()
{
	if (mHeldObject != nullptr) {
		mHeldObject->receiveMessage(this, HIT_MESSAGE_UNK8);
		mHeldObject = nullptr;
	}
	mState = 3;
}

void TFluff::move()
{
	mPosition.y -= mFallSpeed;
	if (mPosition.y < 0.0f)
		mPosition.y = 5000.0f;

	mWindOffset.x += mWindRatio * gpMapObjManager->unkD0.x;
	mWindOffset.z += mWindRatio * gpMapObjManager->unkD0.z;

	JGeometry::TVec3<f32> velocity = mVelocity;
	char trash[0x40]; // unused stack slot (present in the retail frame)
	mWindOffset.x += velocity.x;
	mWindOffset.y += velocity.y;
	mWindOffset.z += velocity.z;
	mVelocity.scale(mVelocityDamping);

	f32 sway = mSwayRadius * sinf(3.14f * mSwayAngle / 180.0f);
	mPosition.x
	    = mWindOffset.x
	      + (sway * (mHeadingCos + mHeadingSin) + getInitialPosition().x);
	mPosition.y += mWindRatio * gpMapObjManager->unkD0.y;
	mPosition.z
	    = mWindOffset.z
	      + (sway * (mHeadingSin - mHeadingCos) + getInitialPosition().z);

	Vec* wind = &gpMapObjManager->unkD0;
	if (wind->x * wind->x + wind->y * wind->y + wind->z * wind->z
	    <= 0.0000038146973f) {
		mSwayAngle += mSwaySpeed;
		if (mSwayAngle > 360.0f)
			mSwayAngle -= 360.0f;
	}

	if (mHeldObject != nullptr && mHeldObject->isActorType(0x80000001))
		gpMarioPos->y -= mFallSpeed;
}

u32 TFluff::touchWater(THitActor* water)
{
	const JGeometry::TVec3<f32>& waterPos = getWaterPos(water);
	JGeometry::TVec3<f32> normal;
	getNormalVecFromTarget(waterPos.x, waterPos.y, waterPos.z, &normal);
	mVelocity.x -= normal.x * mWaterPushRate;
	mVelocity.y -= normal.y * mWaterPushRate;
	mVelocity.z -= normal.z * mWaterPushRate;
	return 1;
}

void TGoalFlag::initMapObj() { TMapObjBase::initMapObj(); }

void TGoalFlag::touchActor(THitActor* actor)
{
	char trash[8]; // unused stack slot (present in the retail frame)
	if (actor->isActorType(0x80000001)) {
		if (!TFlagManager::smInstance->getBool(0x50005))
			TFlagManager::smInstance->setBool(true, 0x50005);
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	} else if (actor->isActorType(0x08000002)) {
		actor->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}
}

TSwingBoard::TSwingBoard(const char* name)
    : TMapObjBase(name)
    , mRopeLength(5000.0f)
    , mSwingAngle(0.0f)
    , mPushAccel(0.0f)
    , mAngularSpeed(0.0f)
    , mSlowDownSpeed(0.0f)
    , mSound(nullptr)
{
	mYawMtx.identity();
	mPivot.zero();
}

void TSwingBoard::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream >> mRopeLength;
	if (mRopeLength == -1.0f)
		mRopeLength = 5000.0f;

	stream >> mPushAccel;
	if (mPushAccel > 10.0f || mPushAccel == 0.0f)
		mPushAccel = 0.003f;

	mPivot.set(mPosition.x, mPosition.y + mRopeLength, mPosition.z);

	mSlowDownSpeed = 0.05f * ((1.0f + MsRandF()) * 0.5f);
	mSwingAngle    = 20.0f * (MsRandF() - 0.5f);
	if (mSwingAngle > 0.0f)
		mAngularSpeed = mSlowDownSpeed * -MsRandF();
	else
		mAngularSpeed = mSlowDownSpeed * MsRandF();

	MsMtxSetRotY(mYawMtx, mRotation.y);
}

void TSwingBoard::control()
{
	TMapObjBase::control();

	if (marioIsOn() && marioIsOn()
	    && SMS_GetMarioWaterGun()->mIsEmitWater != 0) {
		MtxPtr emit = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 dirX    = -emit[0][0];
		f32 dirZ    = -emit[2][0];
		JGeometry::TVec3<f32> push(dirX, 0.0f, dirZ);
		MtxPtr mtx = getModel()->getAnmMtx(0);
		mAngularSpeed
		    += mPushAccel
		       * (mtx[0][2] * push.x + mtx[1][2] * push.y + mtx[2][2] * push.z);
	}

	mSwingAngle += mAngularSpeed;
	f32 prevSpeed = mAngularSpeed;
	mAngularSpeed = prevSpeed - mSwingAngle * mReturnAccelRate;
	if (fabsf(mAngularSpeed) > mSlowDownSpeed)
		mAngularSpeed *= mSpeedDownRate;

	if (prevSpeed * mAngularSpeed <= 0.0f) {
		if (mSound != nullptr)
			mSound->stop(1);

		if (mAngularSpeed > 0.0f) {
			gpMSound->startSoundActorWithInfo(MSD_SE_OBJ_SWING1, &mPosition,
			                                  nullptr, fabsf(mSwingAngle), 0, 0,
			                                  &mSound, 0, 4);
		} else {
			gpMSound->startSoundActorWithInfo(MSD_SE_OBJ_SWING2, &mPosition,
			                                  nullptr, fabsf(mSwingAngle), 0, 0,
			                                  &mSound, 0, 4);
		}
	}

	mRotation.x = -mSwingAngle;
	f32 s       = sinf(3.14f * (mRotation.x / 180.0f));
	f32 c       = cosf(3.14f * (mRotation.x / 180.0f));
	TMtx34f swingMtx;
	char trash[0x88]; // unused stack slot (present in the retail frame)
	swingMtx.set(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, -s, 0.0f, 0.0f, s, c, 0.0f);
	MtxPtr mtx = getModel()->getAnmMtx(0);
	PSMTXConcat(mYawMtx, swingMtx, mtx);

	// The original computes sin/cos of the swing angle here and drops the
	// results; the calls are kept so the call sequence stays identical.
	cosf(3.14f * (mSwingAngle / 180.0f));
	sinf(3.14f * (mSwingAngle / 180.0f));
	mPosition.x = getInitialPosition().x - mtx[0][1] * mRopeLength;
	mPosition.y
	    = (mRopeLength + getInitialPosition().y) - mtx[1][1] * mRopeLength;
	mPosition.z = getInitialPosition().z - mtx[2][1] * mRopeLength;
	mtx[0][3]   = mPosition.x;
	mtx[1][3]   = mPosition.y;
	mtx[2][3]   = mPosition.z;
}

void TSwingBoard::draw() const
{
	initDraw();
	MtxPtr mtx = getModel()->getAnmMtx(0);
	JGeometry::TVec3<f32> board;
	JGeometry::TVec3<f32> pivot;
	char trash[12]; // unused stack slot (present in the retail frame)
	pivot.x = mBoardWidth * mtx[0][0] + getInitialPosition().x;
	pivot.y = mRopeLength + getInitialPosition().y;
	pivot.z = mBoardWidth * mtx[2][0] + getInitialPosition().z;
	board.x = mBoardWidth * mtx[0][0] + mPosition.x;
	board.y = 60.0f + mPosition.y;
	board.z = mBoardWidth * mtx[2][0] + mPosition.z;
	drawOneRope(board, pivot);

	pivot.x = getInitialPosition().x - mBoardWidth * mtx[0][0];
	pivot.z = getInitialPosition().z - mBoardWidth * mtx[2][0];
	board.x = mPosition.x - mBoardWidth * mtx[0][0];
	board.z = mPosition.z - mBoardWidth * mtx[2][0];
	drawOneRope(board, pivot);
}

void TSwingBoard::initDraw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.mViewMtx, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0x64, 0xFF });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY,
	                  GX_FALSE, GX_PTIDENTITY);
	JUTTexture texture(gpMapObjManager->unkCC);
	texture.load(GX_TEXMAP0);
	// dead temp below the live ones + 4-byte pad above them (see decomp-worker-protocol Recipes)
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	int pad4;
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
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
}

void TSwingBoard::drawOneRope(const JGeometry::TVec3<f32>& lower,
                              const JGeometry::TVec3<f32>& upper) const
{
	f32 upperXPlus  = upper.x + mRopeWidthX;
	f32 upperZPlus  = upper.z + mRopeWidthZ;
	f32 upperZMinus = upper.z - mRopeWidthZ;
	f32 lowerXPlus  = lower.x + mRopeWidthX;
	f32 lowerZPlus  = lower.z + mRopeWidthZ;
	f32 lowerZMinus = lower.z - mRopeWidthZ;
	f32 texV        = mRopeLength * mTexPosRate;
	f32 upperXMinus = upper.x - mRopeWidthX;
	f32 lowerXMinus = lower.x - mRopeWidthX;
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(lower.x, lower.y, lowerZPlus);
	GXTexCoord2f32(0.0f, texV);
	GXPosition3f32(upper.x, upper.y, upperZPlus);
	GXTexCoord2f32(0.0f, 0.0f);
	GXPosition3f32(lowerXPlus, lower.y, lowerZMinus);
	GXTexCoord2f32(1.0f, texV);
	GXPosition3f32(upperXPlus, upper.y, upperZMinus);
	GXTexCoord2f32(1.0f, 0.0f);
	GXPosition3f32(lowerXMinus, lower.y, lowerZMinus);
	GXTexCoord2f32(2.0f, texV);
	GXPosition3f32(upperXMinus, upper.y, upperZMinus);
	GXTexCoord2f32(2.0f, 0.0f);
	GXPosition3f32(lower.x, lower.y, lowerZPlus);
	GXTexCoord2f32(3.0f, texV);
	GXPosition3f32(upper.x, upper.y, upperZPlus);
	GXTexCoord2f32(3.0f, 0.0f);
}

THangingBridge::THangingBridge(const char* name)
    : JDrama::TViewObj(name)
    , mBoardNum(0)
    , mBoards(nullptr)
    , mSagTable(nullptr)
    , mBoardHalfWidth(0.0f)
    , mNeighborPush1(0.0f)
    , mNeighborPush2(0.0f)
{
}

inline void THangingBridge::initMonte()
{
	struct BoardPosition {
		f32 x, y, z, rotation;
	};
	// clang-format off
	const BoardPosition positions[21] = {
		{ 0.0f, -130.0f, 11965.0f, 30.0f },
		{ 0.0f, -275.0f, 12225.0f, 29.0f },
		{ 0.0f, -415.0f, 12490.0f, 28.0f },
		{ 0.0f, -540.0f, 12760.0f, 26.0f },
		{ 0.0f, -660.0f, 13035.0f, 24.0f },
		{ 0.0f, -770.0f, 13315.0f, 22.0f },
		{ 0.0f, -875.0f, 13595.0f, 20.0f },
		{ 0.0f, -960.0f, 13895.0f, 12.0f },
		{ 0.0f, -1020.0f, 14190.0f, 8.0f },
		{ 0.0f, -1060.0f, 14490.0f, 4.0f },
		{ 0.0f, -1090.0f, 14790.0f, 2.0f },
		{ 0.0f, -1090.0f, 15090.0f, 0.0f },
		{ 0.0f, -1080.0f, 15395.0f, -4.0f },
		{ 0.0f, -1040.0f, 15695.0f, -6.0f },
		{ 0.0f, -995.0f, 15990.0f, -8.0f },
		{ 0.0f, -945.0f, 16285.0f, -8.0f },
		{ 0.0f, -900.0f, 16580.0f, -8.0f },
		{ 0.0f, -855.0f, 16880.0f, -8.0f },
		{ 0.0f, -800.0f, 17175.0f, -10.0f },
		{ -1.0f, 0.0f, 0.0f, 0.0f },
		{ -99999.0f, 0.0f, 0.0f, 0.0f },
	};
	// clang-format on
	for (int i = 0; i < mBoardNum; ++i) {
		// The original tests the terminator twice (break, then skip).
		if (positions[i].x == -1.0f)
			break;
		if (positions[i].x != -1.0f) {
			mBoards[i]->mInitialPosition.set(positions[i].x, positions[i].y,
			                                 positions[i].z);
			mBoards[i]->mPosition.set(mBoards[i]->mInitialPosition);
			mBoards[i]->mRotation.x = positions[i].rotation;
			mBoards[i]->calcDefaultMtx();
		}
	}
}

void THangingBridge::loadAfter()
{
	JDrama::TNameRef::loadAfter();

	f32 boardRotY = 0.0f;
	if (gpMarDirector->getCurrentMap() == 13) {
		mBoardNum = 14;
		mStartPos.set(1550.0f, 2980.0f, -9410.0f);
		mEndPos.set(3570.0f, 2455.0f, -9410.0f);
		boardRotY       = 90.0f;
		mRopeHeight     = 200.0f;
		mNeighborPush1  = 0.8f;
		mNeighborPush2  = 0.5f;
		mBoardHalfWidth = 150.0f;
	} else if (gpMarDirector->getCurrentMap() == 8) {
		mBoardNum = 19;
		mStartPos.set(0.0f, 0.0f, 11356.0f);
		mEndPos.set(0.0f, -750.0f, 17743.0f);
		mRopeHeight     = 1000.0f;
		mNeighborPush1  = 1.0f;
		mNeighborPush2  = 0.5f;
		mBoardHalfWidth = 315.0f;
	}

	mSideDir.set(mEndPos.x - mStartPos.x, mEndPos.z - mStartPos.z);
	mSideDir.normalize();
	mSideDir.rotate(1.5707964f);

	mBoards = new THangingBridgeBoard*[mBoardNum];
	f32 sag = 0.0f;
	for (int i = 0; i < mBoardNum; ++i) {
		f32 t = (f32)i / (f32)(mBoardNum - 1);
		JGeometry::TVec3<f32> pos;
		pos.x = t * (mEndPos.x - mStartPos.x) + mStartPos.x;
		// Discarded in the original as well: a bridge sag that always stays 0.
		sinf(3.14f * t);
		pos.y = t * (mEndPos.y - mStartPos.y) + mStartPos.y - sag;
		JGeometry::TVec3<f32> rot(15.0f, boardRotY, sag);
		pos.z = t * (mEndPos.z - mStartPos.z) + mStartPos.z;
		if (gpMarDirector->getCurrentMap() == 8) {
			mBoards[i]
			    = (THangingBridgeBoard*)TMapObjBaseManager::newAndRegisterObj(
			        "HangingBridgeBoard", pos, rot);
		} else {
			mBoards[i]
			    = (THangingBridgeBoard*)TMapObjBaseManager::newAndRegisterObj(
			        "PinnaHangingBridgeBoard", pos, rot);
		}
		mBoards[i]->mBridge = this;
		mBoards[i]->appear();
	}

	if (gpMarDirector->getCurrentMap() == 8)
		initMonte();

	if (gpMarDirector->getCurrentMap() == 13) {
		mStartPos.set(1350.0f, 2980.0f, -9410.0f);
		mEndPos.set(3650.0f, 2455.0f, -9410.0f);
	}

	for (int i = 0; i < mBoardNum; ++i) {
		if (i > 0)
			mBoards[i]->mPrev = mBoards[i - 1];
		if (i > 1)
			mBoards[i]->mPrev2 = mBoards[i - 2];
		if (i < mBoardNum - 1)
			mBoards[i]->mNext = mBoards[i + 1];
		if (i < mBoardNum - 2)
			mBoards[i]->mNext2 = mBoards[i + 2];
	}

	mSagTable = new f32[mPointNumBetweenBoards];
	f32 step  = 1.0f / (f32)mPointNumBetweenBoards;
	f32 t     = 0.0f;
	for (int i = 0; i < mPointNumBetweenBoards; ++i) {
		mSagTable[i] = 50.0f * sinf(3.14f * t);
		t += step;
	}
}

void THangingBridge::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x10]; // unused stack slot (present in the retail frame)
	if (cue & CUE_DRAW) {
		initDraw();
		for (int i = 0; i < mBoardNum; ++i)
			mBoards[i]->drawRopes();
		drawRopeBetweenBoards(0.0f, mPointNumBetweenBoards);
		drawRopeBetweenBoards(mRopeHeight, 1);
	}
}

void THangingBridge::initDraw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXLoadPosMtxImm(j3dSys.mViewMtx, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0,
	              GX_DF_NONE, GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0, 0, 0x64, 0xFF });
	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY,
	                  GX_FALSE, GX_PTIDENTITY);
	if (gpMarDirector->getCurrentMap() == 13) {
		JUTTexture texture(gpMapObjManager->unkCC);
		texture.load(GX_TEXMAP0);
	} else {
		JUTTexture texture(gpMapObjManager->unkCC);
		texture.load(GX_TEXMAP0);
	}
	char trash[0x20]; // unused stack slot (present in the retail frame)
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
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
}

void THangingBridge::drawRopeBetweenBoards(f32 yOffset, int count) const
{
	char trash[0x8];
	f32 offsetX                 = mSideDir.x * mBoardHalfWidth;
	f32 offsetZ                 = mSideDir.y * mBoardHalfWidth;
	JGeometry::TVec2<f32> width = mSideDir;
	width.scale(mRopeWidthBetweenBoards);
	u16 vertexNum = (mBoardNum + 2) * count * 2;

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	JGeometry::TVec3<f32> from(mStartPos.x + offsetX, mStartPos.y + yOffset,
	                           mStartPos.z + offsetZ);
	JGeometry::TVec3<f32> to;
	char trash2[0x4c];
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[0];
		to.y += yOffset;
		drawLowerMinus(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x + offsetX, mEndPos.y + yOffset, mEndPos.z + offsetZ);
	drawLowerMinus(from, to, width, count);
	from = to;
	drawLowerMinus(from, to, width, count);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	from.set(mStartPos.x + offsetX, mStartPos.y + yOffset,
	         mStartPos.z + offsetZ);
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[0];
		to.y += yOffset;
		drawLowerPlus(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x + offsetX, mEndPos.y + yOffset, mEndPos.z + offsetZ);
	drawLowerPlus(from, to, width, count);
	from = to;
	drawLowerPlus(from, to, width, count);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	from.set(mStartPos.x + offsetX, mStartPos.y + yOffset,
	         mStartPos.z + offsetZ);
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[0];
		to.y += yOffset;
		drawUpper(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x + offsetX, mEndPos.y + yOffset, mEndPos.z + offsetZ);
	drawUpper(from, to, width, count);
	from = to;
	drawUpper(from, to, width, count);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	from.set(mStartPos.x - offsetX, mStartPos.y + yOffset,
	         mStartPos.z - offsetZ);
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[1];
		to.y += yOffset;
		drawLowerMinus(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x - offsetX, mEndPos.y + yOffset, mEndPos.z - offsetZ);
	drawLowerMinus(from, to, width, count);
	from = to;
	drawLowerMinus(from, to, width, count);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	from.set(mStartPos.x - offsetX, mStartPos.y + yOffset,
	         mStartPos.z - offsetZ);
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[1];
		to.y += yOffset;
		drawLowerPlus(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x - offsetX, mEndPos.y + yOffset, mEndPos.z - offsetZ);
	drawLowerPlus(from, to, width, count);
	from = to;
	drawLowerPlus(from, to, width, count);

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, vertexNum);
	from.set(mStartPos.x - offsetX, mStartPos.y + yOffset,
	         mStartPos.z - offsetZ);
	for (int i = 0; i < mBoardNum; ++i) {
		to = mBoards[i]->mRopePos[1];
		to.y += yOffset;
		drawUpper(from, to, width, count);
		from = to;
	}
	to.set(mEndPos.x - offsetX, mEndPos.y + yOffset, mEndPos.z - offsetZ);
	drawUpper(from, to, width, count);
	from = to;
	drawUpper(from, to, width, count);
}

void THangingBridge::drawUpper(const JGeometry::TVec3<f32>& from,
                               const JGeometry::TVec3<f32>& to,
                               const JGeometry::TVec2<f32>& width,
                               int count) const
{
	f32 step  = 1.0f / (f32)count;
	f32 x     = from.x;
	f32 y     = from.y;
	f32 z     = from.z;
	f32 stepX = step * (to.x - from.x);
	f32 stepY = step * (to.y - from.y);
	f32 stepZ = step * (to.z - from.z);
	for (int i = 0; i < count; ++i) {
		f32 low  = y - mSagTable[i];
		f32 texT = mBetweenBoardsTexPosRate * (x + z);
		GXPosition3f32(x + width.x, low, z + width.y);
		GXTexCoord2f32(0.0f, texT);
		GXPosition3f32(x - width.x, low, z - width.y);
		GXTexCoord2f32(1.0f, texT);
		x += stepX;
		y += stepY;
		z += stepZ;
	}
}

void THangingBridge::drawLowerPlus(const JGeometry::TVec3<f32>& from,
                                   const JGeometry::TVec3<f32>& to,
                                   const JGeometry::TVec2<f32>& width,
                                   int count) const
{
	f32 step  = 1.0f / (f32)count;
	f32 x     = from.x;
	f32 y     = from.y;
	f32 z     = from.z;
	f32 stepX = step * (to.x - from.x);
	f32 stepY = step * (to.y - from.y);
	f32 stepZ = step * (to.z - from.z);
	for (int i = 0; i < count; ++i) {
		f32 low  = y - mSagTable[i];
		f32 texT = mBetweenBoardsTexPosRate * (x + z);
		GXPosition3f32(x, low - mRopeWidthBetweenBoardsY, z);
		GXTexCoord2f32(0.0f, texT);
		GXPosition3f32(x + width.x, low, z + width.y);
		GXTexCoord2f32(1.0f, texT);
		x += stepX;
		y += stepY;
		z += stepZ;
	}
}

void THangingBridge::drawLowerMinus(const JGeometry::TVec3<f32>& from,
                                    const JGeometry::TVec3<f32>& to,
                                    const JGeometry::TVec2<f32>& width,
                                    int count) const
{
	f32 step  = 1.0f / (f32)count;
	f32 x     = from.x;
	f32 y     = from.y;
	f32 z     = from.z;
	f32 stepX = step * (to.x - from.x);
	f32 stepY = step * (to.y - from.y);
	f32 stepZ = step * (to.z - from.z);
	for (int i = 0; i < count; ++i) {
		f32 low  = y - mSagTable[i];
		f32 texT = mBetweenBoardsTexPosRate * (x + z);
		GXPosition3f32(x - width.x, low, z - width.y);
		GXTexCoord2f32(0.0f, texT);
		GXPosition3f32(x, low - mRopeWidthBetweenBoardsY, z);
		GXTexCoord2f32(1.0f, texT);
		x += stepX;
		y += stepY;
		z += stepZ;
	}
}

THangingBridgeBoard::THangingBridgeBoard(const char* name)
    : TLeanBlock(name)
    , mBridge(nullptr)
{
	mPrev  = nullptr;
	mNext  = nullptr;
	mPrev2 = nullptr;
	mNext2 = nullptr;
	mRopePos[0].zero();
	mRopePos[1].zero();
}

void THangingBridgeBoard::initMapObj()
{
	TLeanBlock::initMapObj();
	unk140 = 0.01f;
	unk144 = 0.02f;
	unk148 = 0.08f;
}

void THangingBridgeBoard::setGroundCollision()
{
	if (SMS_GetYoshi()->isHatched()
	    && mPosition.x - mBodyRadius < SMS_GetYoshi()->getTranslation().x
	    && mPosition.x + mBodyRadius > SMS_GetYoshi()->getTranslation().x
	    && mPosition.z - mBodyRadius < SMS_GetYoshi()->getTranslation().z
	    && mPosition.z + mBodyRadius > SMS_GetYoshi()->getTranslation().z) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->moveMtx(mtx);
	} else {
		TMapObjBase::setGroundCollision();
	}
}

void THangingBridgeBoard::calcDefaultMtx()
{
	Mtx rotX;
	Mtx rotY;
	char trash[4]; // unused stack slot (present in the retail frame)
	makeRootMtxRotX(rotX);
	makeRootMtxRotY(rotY);
	PSMTXConcat(rotY, rotX, rotY);
	getDefaultMtx().set(rotY);
	mVelocity.y = 0.0f;
	mPosition.y = getInitialPosition().y;
}

inline void THangingBridgeBoard::pushNeighbor(f32 accel)
{
	if (mPrev != nullptr) {
		f32 push1 = accel * mBridge->mNeighborPush1;
		mPrev->mVelocity.y -= push1;
		if (mPrev2 != nullptr) {
			f32 push2 = accel * mBridge->mNeighborPush2;
			mPrev2->mVelocity.y -= push2;
		}
	}
	if (mNext != nullptr) {
		f32 push1 = accel * mBridge->mNeighborPush1;
		mNext->mVelocity.y -= push1;
		if (mNext2 != nullptr) {
			f32 push2 = accel * mBridge->mNeighborPush2;
			mNext2->mVelocity.y -= push2;
		}
	}
}

void THangingBridgeBoard::control()
{
	char trash[0xd8]; // unused stack slot (present in the retail frame)
	TLeanBlock::control();

	if (marioIsOn()) {
		mVelocity.y -= mMarioAccelY;
		pushNeighbor(mMarioAccelY);
	}

	if (marioHipAttack()) {
		mVelocity.y -= mMarioHipDropAccelY;
		pushNeighbor(mMarioHipDropAccelY);
	}

	mPosition.y += mVelocity.y;
	mVelocity.y += mReturnAccelRate * (getInitialPosition().y - mPosition.y);
	mVelocity.y *= mSpeedDownRate;

	MtxPtr mtx    = getModel()->getAnmMtx(0);
	mRopePos[0].x = mPosition.x - mtx[0][0] * mBridge->mBoardHalfWidth;
	mRopePos[0].y
	    = 70.0f + (mPosition.y - mtx[1][0] * mBridge->mBoardHalfWidth);
	mRopePos[0].z = mPosition.z - mtx[2][0] * mBridge->mBoardHalfWidth;
	mRopePos[1].x = mPosition.x + mtx[0][0] * mBridge->mBoardHalfWidth;
	mRopePos[1].y
	    = 70.0f + (mPosition.y + mtx[1][0] * mBridge->mBoardHalfWidth);
	mRopePos[1].z = mPosition.z + mtx[2][0] * mBridge->mBoardHalfWidth;
}

inline void THangingBridgeBoard::drawRopes() const
{
	JGeometry::TVec3<f32> pos = mRopePos[0];
	drawOneRope(pos);
	pos = mRopePos[1];
	drawOneRope(pos);
}

void THangingBridgeBoard::drawOneRope(const JGeometry::TVec3<f32>& pos) const
{
	char trash[8]; // unused stack slot (present in the retail frame)
	f32 bottom    = pos.y;
	f32 xPlus     = pos.x + mRopeWidthX;
	f32 xMinus    = pos.x - mRopeWidthX;
	f32 zPlus     = pos.z + mRopeWidthZ;
	f32 zMinus    = pos.z - mRopeWidthZ;
	f32 top       = bottom + THangingBridge::mRopeHeight;
	f32 texTop    = mTexPosRate * (top - bottom);
	f32 texBottom = mTexPosRate * (bottom - bottom);
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(pos.x, top, zPlus);
	GXTexCoord2f32(0.0f, texTop);
	GXPosition3f32(pos.x, bottom, zPlus);
	GXTexCoord2f32(0.0f, texBottom);
	GXPosition3f32(xMinus, top, zMinus);
	GXTexCoord2f32(1.0f, texTop);
	GXPosition3f32(xMinus, bottom, zMinus);
	GXTexCoord2f32(1.0f, texBottom);
	GXPosition3f32(xPlus, top, zMinus);
	GXTexCoord2f32(2.0f, texTop);
	GXPosition3f32(xPlus, bottom, zMinus);
	GXTexCoord2f32(2.0f, texBottom);
	GXPosition3f32(pos.x, top, zPlus);
	GXTexCoord2f32(3.0f, texTop);
	GXPosition3f32(pos.x, bottom, zPlus);
	GXTexCoord2f32(3.0f, texBottom);
}

void TJumpMushroom::load(JSUMemoryInputStream& stream)
{
	s32 value;
	char trash[8]; // unused stack slot (present in the retail frame)
	TMapObjBase::load(stream);
	stream >> value;
	if (mMapCollisionManager)
		mMapCollisionManager->unk8->setAllData(value);
}

BOOL TJumpMushroom::receiveMessage(THitActor* sender, u32 message)
{
	startAnim(1);
	return TRUE;
}

void TMapObjMonteRoot::initMapObj()
{
	TMapObjBase::initMapObj();
	setDamageHeight(1400.0f * mScaling.y);
	f32 y       = getInitialPosition().y;
	mPosition.y = y + mYOffset;
}
