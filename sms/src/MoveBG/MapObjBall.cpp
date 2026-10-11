#include <MoveBG/MapObjBall.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <Camera/CubeManagerBase.hpp>
#include <Enemy/PoiHana.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry/JGRotation3.hpp>
#include <JSystem/JGeometry/JGMatrix34.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// fabricated: ground planes that objects fall through (0x801: everything but
// map objects phase through, 0x203: map change phase through)
static inline bool isPhaseThroughPlane(const TBGCheckData* plane)
{
	if (plane->mBGType == 0x801 || plane->mBGType == 0x203)
		return true;
	else
		return false;
}

u32 TResetFruit::mFruitLivingTime       = 14400;
f32 TResetFruit::mScaleUpSpeed          = 1.05f;
f32 TResetFruit::mBreakingScaleSpeed    = 0.96f;
u32 TResetFruit::mFruitWaitTimeToAppear = 360;

void TMapObjBall::touchRoof(JGeometry::TVec3<f32>* pos)
{
	if (pos->y > unk140)
		pos->y = unk140;

	calcReflectingVelocity(unk13C, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
}

void TMapObjBall::touchWall(JGeometry::TVec3<f32>* pos,
                            TBGWallCheckRecord* record)
{
	if (!isAirborne() && !isActorType(0x400000D0)) {
		mVelocity.y += mWallRollUpRate
		               * JGeometry::TVec3<f32>(mVelocity).length();
	}

	for (int i = 0; i < record->mResultWallsNum; ++i) {
		const TBGCheckData* wall = record->mResultWalls[i];
		const JGeometry::TVec3<f32>& normal = wall->getNormal();

		f32 dot = JGeometry::TVec3<f32>(mVelocity).dot(normal);
		if (dot < 0.0f) {
			f32 dist = pos->x * normal.x + pos->y * normal.y
			           + pos->z * normal.z + wall->mPlaneDistance;
			pos->x += (mBodyRadius - dist) * normal.x;
			pos->z += (mBodyRadius - dist) * normal.z;

			f32 k = dot * -(1.0f + mMapObjData->mPhysical->unk4->unk8);
			mVelocity.x += k * normal.x;
			mVelocity.z += k * normal.z;

			if (isActorType(0x400000D0)) {
				if (mScaling.y >= 5.0f)
					SMSGetMSound()->startSoundActorWithInfo(
					    MSD_SE_OBJ_WATERMELON_BROLL, &mPosition, nullptr,
					    abs(JGeometry::TVec3<f32>(mVelocity).length()), 0, 0,
					    nullptr, 0, 4);
				else
					SMSGetMSound()->startSoundActorWithInfo(
					    MSD_SE_OBJ_WATERMELON_SROLL, &mPosition, nullptr,
					    abs(JGeometry::TVec3<f32>(mVelocity).length()), 0, 0,
					    nullptr, 0, 4);
			} else {
				SMSGetMSound()->startSoundActorWithInfo(
				    mMapObjData->mSound->unk4->unk0[4], &mPosition,
				    &mVelocity, 0.0f, 0, 0, nullptr, 0, 4);
			}
		}
	}
}

void TMapObjBall::touchPollution() { kill(); }

void TMapObjBall::touchWaterSurface() { kill(); }

void TMapObjBall::rebound(JGeometry::TVec3<f32>* pos)
{
	char trash[40];
	calcReflectingVelocity(mGroundPlane, mMapObjData->mPhysical->unk4->unk4,
	                       &mVelocity);
	pos->y = mGroundHeight;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (isActorType(0x400000D0)) {
		if (mScaling.y >= 5.0f)
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_BBUND, &mPosition, nullptr,
			    abs(mGroundPlane->mNormal.y), 0, 0, nullptr, 0, 4);
		else
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_SBUND, &mPosition, nullptr,
			    abs(mGroundPlane->mNormal.y), 0, 0, nullptr, 0, 4);
	} else {
		SMSGetMSound()->startSoundActorWithInfo(
		    mMapObjData->mSound->unk4->unk0[4], &mPosition, &mVelocity, 0.0f,
		    0, 0, nullptr, 0, 4);
	}
}

void TMapObjBall::touchGround(JGeometry::TVec3<f32>* pos)
{
	f32 speed = abs(JGeometry::TVec3<f32>(mVelocity).length());
	char trash[64];
	if (speed > 0.05f && isActorType(0x400000D0)) {
		if (mScaling.y >= 5.0f)
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_BROLL, &mPosition, nullptr, speed, 0, 0,
			    nullptr, 0, 4);
		else
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_WATERMELON_SROLL, &mPosition, nullptr, speed, 0, 0,
			    nullptr, 0, 4);
	}

	if (mGroundPlane->isWaterSurface()) {
		touchWaterSurface();
		pos->set(mPosition);
	} else if (gpPollution->isPolluted(pos->x, pos->y, pos->z)) {
		touchPollution();
		pos->set(mPosition);
	} else {
		if (mVelocity.y > -mMaxLandingSpeed) {
			offLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity.y = 0.0f;
			pos->y      = mGroundHeight;
		} else {
			rebound(pos);
		}

		if (!isAirborne()) {
			mVelocity.x += mSlopeRollRate * mGroundPlane->mNormal.x;
			mVelocity.z += mSlopeRollRate * mGroundPlane->mNormal.z;
		}

		mVelocity.x *= mMapObjData->mPhysical->unk4->unk10;
		mVelocity.z *= mMapObjData->mPhysical->unk4->unk10;

		if (isActorType(0x400000D0)) {
			if (abs(mVelocity.x) > mMapObjData->mPhysical->unk4->unkC
			    || abs(mVelocity.z) > mMapObjData->mPhysical->unk4->unkC)
				SMSGetMSound()->startSoundActor(MSD_SE_MA_SLIP, &mPosition, 0,
				                                nullptr, 0, 4);
		}
	}
}

void TMapObjBall::put()
{
	TMapObjGeneral::put();
	calcCurrentMtx();
}

void TMapObjBall::hold(TTakeActor* actor)
{
	char trash[4];
	if (!(JGeometry::TVec3<f32>(mVelocity).length() > 10.0f)) {
		TMapObjGeneral::hold(actor);
		mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
	}
}

void TMapObjBall::kicked()
{
	JGeometry::TVec3<f32> vel = mVelocity;
	if (JGeometry::TVec3<f32>(vel).y > 0.0f)
		return;

	if (JGeometry::TVec3<f32>(vel).y == 0.0f)
		mVelocity.y = mKickUpSpeed;
	else
		mVelocity.y = mKickRateY * *gpMarioSpeedY
		              - mPlayerBounceKeep * JGeometry::TVec3<f32>(vel).y;

	mVelocity.x += mKickRate * *gpMarioSpeedX;
	mVelocity.z += mKickRate * *gpMarioSpeedZ;

	if (abs(mVelocity.x) < mMapObjData->mPhysical->unk4->unkC
	    && abs(mVelocity.z) < mMapObjData->mPhysical->unk4->unkC) {
		mVelocity.x = 2.0f * MsRandF() - 1.0f;
		mVelocity.z = 2.0f * MsRandF() - 1.0f;
	}

	mHitCooldown = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	SMS_GetMarioHitActor()->receiveMessage(this, HIT_MESSAGE_ATTACK);
	if (!isActorType(0x400000D0))
		SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN, &mPosition, 0,
		                                nullptr, 0, 4);
}

u32 TMapObjBall::touchWater(THitActor* water)
{
	char trash[8];
	if (isState(STATE_HOLDING) || isState(STATE_APPEARING))
		return 1;

	JGeometry::TVec3<f32> newVel;
	newVel.set(JGeometry::TVec3<f32>(mVelocity));
	const JGeometry::TVec3<f32>& speed = getWaterSpeed(water);
	f32 rate                           = mWaterPushRate;
	newVel.x += speed.x * rate;
	newVel.y += speed.y * rate;
	newVel.z += speed.z * rate;
	mVelocity = newVel;
	offLiveFlag(LIVE_FLAG_UNK10);
	return 1;
}

void TMapObjBall::boundByActor(THitActor* other)
{
	JGeometry::TVec3<f32> diff(other->mPosition.x - mPosition.x, 0.0f,
	                           other->mPosition.z - mPosition.z);

	f32 radius;
	if (isActorType(0x400000D0))
		radius = mAttackRadius + other->mDamageRadius;
	else
		radius = mDamageRadius;

	if (radius * radius < diff.x * diff.x + diff.z * diff.z)
		return;

	if (diff.x != 0.0f && diff.z != 0.0f)
		MsVECNormalize(&diff, &diff);

	if (other->isActorType(0x80000001)) {
		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
			if (abs(*gpMarioSpeedX) > mMapObjData->mPhysical->unk4->unkC
			    || abs(*gpMarioSpeedZ)
			           > mMapObjData->mPhysical->unk4->unkC) {
				mVelocity.y += mPlayerFastPushUpSpeed;
				if (!isActorType(0x400000D0))
					SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN,
					                                &mPosition, 0, nullptr, 0,
					                                4);
			} else {
				mVelocity.y += mPlayerSlowPushUpSpeed;
			}

			mVelocity.x += mPlayerPushRate * *gpMarioSpeedX
			               - diff.x * mPlayerPushOutRate;
			mVelocity.z += mPlayerPushRate * *gpMarioSpeedZ
			               - diff.z * mPlayerPushOutRate;
			other->receiveMessage(this, HIT_MESSAGE_ATTACK);
		}
	} else {
		JGeometry::TVec3<f32> vel = mVelocity;
		f32 dot                   = JGeometry::TVec3<f32>(vel).dot(diff);
		if (dot >= 0.0f
		    && abs(JGeometry::TVec3<f32>(mVelocity).x)
		           > mMapObjData->mPhysical->unk4->unkC
		    && abs(JGeometry::TVec3<f32>(mVelocity).z)
		           > mMapObjData->mPhysical->unk4->unkC) {
			mVelocity.x -= (1.0f + mObjBounceRate) * (diff.x * dot);
			mVelocity.y += mObjPushUpSpeed;
			mVelocity.z -= (1.0f + mObjBounceRate) * (diff.z * dot);
			other->receiveMessage(this, HIT_MESSAGE_UNK10);
			if (!isActorType(0x400000D0))
				SMSGetMSound()->startSoundActor(MSD_SE_IT_DRIAN_BOUND,
				                                &mPosition, 0, nullptr, 0, 4);
		} else {
			mVelocity.x -= diff.x * mObjPushOutRate;
			mVelocity.y += mObjPushUpSpeed;
			mVelocity.z -= diff.z * mObjPushOutRate;
		}
	}

	if (other->isActorType(0x80000001)
	    && !checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000)) {
		JGeometry::TVec3<f32> vel = mVelocity;
		if (JGeometry::TVec3<f32>(vel).y < 0.0f) {
			if (130.0f + gpMarioPos->y < mPosition.y + mBodyRadius) {
				mVelocity.y = mPlayerBounceKeep * -JGeometry::TVec3<f32>(vel).y;
				mVelocity.x += mPlayerBounceRate * *gpMarioSpeedX;
				mVelocity.y += mPlayerBounceRateY * *gpMarioSpeedY;
				mVelocity.z += mPlayerBounceRate * *gpMarioSpeedZ;
				if (!isActorType(0x400000D0))
					SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN,
					                                &mPosition, 0, nullptr, 0,
					                                4);
			}
		}
	}

	mHitCooldown = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

#pragma dont_inline on
void TMapObjBall::touchActor(THitActor* other)
{
	char trash[8];
	if (mHitCooldown != 0 || isState(STATE_HOLDING) || isHideObj(other)
	    || other->isActorType(0x08000083) || other->isActorType(0x400000CA)
	    || other->isActorType(0x400000CC))
		return;

	if (other->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && *gpMarioSpeedY != 0.0f)
		kicked();
	else
		boundByActor(other);
}
#pragma dont_inline off

void TMapObjBall::calcCurrentMtx()
{
	TPosition3f rotation;
	rotation.identity();

	if (abs(JGeometry::TVec3<f32>(mVelocity).x)
	        < mMapObjData->mPhysical->unk4->unkC
	    && abs(JGeometry::TVec3<f32>(mVelocity).z)
	           < mMapObjData->mPhysical->unk4->unkC
	    && mGroundPlane->mNormal.y == 1.0f) {
		mVelocity.x = 0.0f;
		mVelocity.z = 0.0f;
	}

	if (abs(JGeometry::TVec3<f32>(mVelocity).x)
	        > mMapObjData->mPhysical->unk4->unkC
	    || abs(JGeometry::TVec3<f32>(mVelocity).z)
	           > mMapObjData->mPhysical->unk4->unkC) {
		JGeometry::TVec3<f32> axis;
		getVerticalVecToTargetXZ(
		    mPosition.x + JGeometry::TVec3<f32>(mVelocity).x,
		    mPosition.z + JGeometry::TVec3<f32>(mVelocity).z, &axis);

		JGeometry::TVec3<f32> vel1 = mVelocity;
		JGeometry::TVec3<f32> vel2 = vel1;
		JGeometry::TVec3<f32> vel3 = vel1;
		f32 speed = JGeometry::TUtil<f32>::sqrt(vel3.x * vel3.x
		                                        + vel2.z * vel2.z);

		rotation.setRotate(axis, 2.0f * (speed / mBodyRadius));
	}

	TPosition3f current;
	current.set(getModel()->getAnmMtx(0));
	current.ref(0, 3) = 0.0f;
	current.ref(1, 3) = 0.0f;
	current.ref(2, 3) = 0.0f;

	MTXConcat(rotation, current, rotation);

	rotation.ref(0, 3) = mPosition.x;
	rotation.ref(1, 3) = mPosition.y + mBodyRadius;
	rotation.ref(2, 3) = mPosition.z;

	if (isActorType(0x40000394)) {
		if (rotation.at(1, 1) > 0.0f)
			rotation.ref(1, 3) -= 50.0f * rotation.at(1, 1);
	}
	if (isActorType(0x40000392)) {
		rotation.ref(1, 3) -= 10.0f * (1.0f - rotation.at(1, 1));
	}

	MTXCopy(rotation, getModel()->getAnmMtx(0));
}

void TMapObjBall::checkWallCollision(JGeometry::TVec3<f32>* pos)
{
	JGeometry::TVec3<f32> checkPos;
	char trash[24];
	f32 radius = mBodyRadius;
	checkPos.x = pos->x;
	checkPos.y = pos->y + radius;
	checkPos.z = pos->z;
	TBGWallCheckRecord record(checkPos, radius, 4,
	                          mMapObjData->mPhysical->mWallCheckFlags);

	if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
		unk138 = record.mResultWalls[0];
		pos->x = checkPos.x;
		pos->z = checkPos.z;
		touchWall(pos, &record);
	} else {
		unk138 = 0;
	}
}

void TMapObjBall::makeObjDefault()
{
	char trash[8];
	TMapObjBase::makeObjDefault();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;
}

void TMapObjBall::makeObjAppeared()
{
	char trash[8];
	TMapObjBase::makeObjAppeared();
	calcCurrentMtx();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mtx[0][3]  = mPosition.x;
	mtx[1][3]  = mPosition.y + mBodyRadius;
	mtx[2][3]  = mPosition.z;

	if (isActorType(0x40000394)) {
		if (mtx[1][1] > 0.0f)
			mtx[1][3] -= 50.0f * mtx[1][1];
	}
	if (isActorType(0x40000392)) {
		mtx[1][3] -= 10.0f * (1.0f - mtx[1][1]);
	}

	unkE8 = 0;
}

#pragma dont_inline on
void TMapObjBall::control()
{
	char trash[8];
	TMapObjGeneral::control();

	if (mHitCooldown != 0)
		--mHitCooldown;

	if (isState(STATE_HOLDING)) {
		Mtx mtx;
		MTXCopy(mHolder->getTakingMtx(), mtx);
		mtx[1][3] += mHoldHeightOffset;
		MTXCopy(mtx, getModel()->getAnmMtx(0));
	} else {
		if (!JGeometry::TVec3<f32>(mVelocity).isZero()
		    || mGroundPlane->getActor())
			calcCurrentMtx();
	}
}
#pragma dont_inline off

BOOL TMapObjBall::receiveMessage(THitActor* sender, u32 message)
{
	if (TMapObjGeneral::receiveMessage(sender, message))
		return true;

	if (message == HIT_MESSAGE_TAKE
	    && checkMapObjFlag(MAP_OBJ_FLAG_UNK100000)) {
		hold((TTakeActor*)sender);
		return true;
	}

	if (sender->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && message != HIT_MESSAGE_TAKE) {
		kicked();
		return true;
	}

	return false;
}

void TMapObjBall::initMapObj()
{
	TMapObjGeneral::initMapObj();
	mInitialScaling.set(mScaling);

	switch (mActorType) {
	case 0x400000D0:
		mPlayerPushOutRate     = 4.0f;
		mPlayerFastPushUpSpeed = 0.0f;
		mPlayerSlowPushUpSpeed = 0.0f;
		mPlayerBounceRate      = 0.15f;
		mPlayerBounceRateY     = 0.0f;
		mPlayerBounceKeep      = 0.9f;
		mObjPushOutRate        = 0.06f;
		mObjPushUpSpeed        = 1.5f;
		mObjBounceRate         = 0.5f;
		mKickRate              = 0.5f;
		mKickRateY             = 0.2f;
		mKickUpSpeed           = 2.5f;
		mWaterPushRate         = 0.001f;
		mSlopeRollRate         = 0.3f;
		mWallRollUpRate        = 1.5f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = mBodyRadius / 3.0f;
		break;

	case 0x40000064:
		mPlayerPushRate        = 0.6f;
		mPlayerPushOutRate     = 2.0f;
		mPlayerFastPushUpSpeed = 0.02f;
		mPlayerSlowPushUpSpeed = 0.0f;
		mPlayerBounceRate      = 0.055f;
		mPlayerBounceRateY     = 0.02f;
		mPlayerBounceKeep      = 0.83f;
		mKickRate              = 0.9f;
		mKickRateY             = 0.13f;
		mKickUpSpeed           = 20.0f;
		mObjPushOutRate        = 0.5f;
		mObjPushUpSpeed        = 0.02f;
		mObjBounceRate         = 0.5f;
		mWaterPushRate         = 1.2f;
		mSlopeRollRate         = 0.8f;
		mWallRollUpRate        = 1.0f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = mBodyRadius / 3.0f;
		break;

	case 0x40000393:
		mPlayerPushRate        = 0.6f;
		mPlayerPushOutRate     = 0.2f;
		mPlayerFastPushUpSpeed = 1.3f;
		mPlayerSlowPushUpSpeed = 15.0f;
		mPlayerBounceRate      = 0.5f;
		mPlayerBounceRateY     = 1.3f;
		mPlayerBounceKeep      = 1.0f;
		mKickRate              = 0.9f;
		mKickRateY             = 0.13f;
		mKickUpSpeed           = 20.0f;
		mObjPushOutRate        = 2.0f;
		mObjPushUpSpeed        = 0.02f;
		mObjBounceRate         = 0.3f;
		mWaterPushRate         = 0.05f;
		mSlopeRollRate         = 0.5f;
		mWallRollUpRate        = 1.0f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = 50.0f;
		break;

	case 0x40000390:
	case 0x40000391:
	case 0x40000392:
		mPlayerPushRate        = 0.4f;
		mPlayerPushOutRate     = 0.2f;
		mPlayerFastPushUpSpeed = 1.3f;
		mPlayerSlowPushUpSpeed = 0.0f;
		mPlayerBounceRate      = 1.2f;
		mPlayerBounceRateY     = 0.8f;
		mPlayerBounceKeep      = 0.5f;
		mKickRate              = 0.9f;
		mKickRateY             = 0.13f;
		mKickUpSpeed           = 20.0f;
		mObjPushOutRate        = 2.0f;
		mObjPushUpSpeed        = 0.02f;
		mObjBounceRate         = 0.3f;
		mWaterPushRate         = 0.05f;
		mSlopeRollRate         = 0.5f;
		mWallRollUpRate        = 1.0f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = 50.0f;
		break;

	case 0x40000394:
		mPlayerPushRate        = 0.2f;
		mPlayerPushOutRate     = 0.0f;
		mPlayerFastPushUpSpeed = 0.0f;
		mPlayerSlowPushUpSpeed = 0.0f;
		mPlayerBounceRate      = 0.0f;
		mPlayerBounceRateY     = 0.0f;
		mPlayerBounceKeep      = 0.0f;
		mKickRate              = 0.0f;
		mKickRateY             = 0.0f;
		mKickUpSpeed           = 0.0f;
		mObjPushOutRate        = 0.0f;
		mObjPushUpSpeed        = 0.0f;
		mObjBounceRate         = 0.0f;
		mWaterPushRate         = 0.05f;
		mSlopeRollRate         = 0.5f;
		mWallRollUpRate        = 1.0f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = 50.0f;
		break;

	case 0x40000395:
		mPlayerPushRate        = 0.4f;
		mPlayerPushOutRate     = 0.2f;
		mPlayerFastPushUpSpeed = 1.3f;
		mPlayerSlowPushUpSpeed = 0.0f;
		mPlayerBounceRate      = 1.2f;
		mPlayerBounceRateY     = 0.8f;
		mPlayerBounceKeep      = 0.5f;
		mKickRate              = 0.9f;
		mKickRateY             = 0.13f;
		mKickUpSpeed           = 20.0f;
		mObjPushOutRate        = 2.0f;
		mObjPushUpSpeed        = 0.02f;
		mObjBounceRate         = 0.3f;
		mWaterPushRate         = 0.05f;
		mSlopeRollRate         = 0.5f;
		mWallRollUpRate        = 1.0f;
		mMaxLandingSpeed       = 1.5f;
		mBodyRadius            = 50.0f * mScaling.y;
		mFloatingDepth         = 50.0f;
		break;
	}

	if (isActorType(0x40000393)) {
		mBodyRadius       = 45.0f * mScaling.y;
		mHoldHeightOffset = mBodyRadius;
	}
	if (isActorType(0x40000390)) {
		mBodyRadius       = 40.0f * mScaling.y;
		mHoldHeightOffset = 20.0f;
	}
	if (isActorType(0x40000391)) {
		mBodyRadius       = 40.0f * mScaling.y;
		mHoldHeightOffset = 20.0f;
	}
	if (isActorType(0x40000392)) {
		mHoldHeightOffset = 10.0f;
	}
}

TMapObjBall::TMapObjBall(const char* name)
    : TMapObjGeneral(name)
    , mPlayerPushRate(0.0f)
    , mPlayerPushOutRate(0.0f)
    , mPlayerFastPushUpSpeed(0.0f)
    , mPlayerSlowPushUpSpeed(0.0f)
    , mPlayerBounceRate(0.0f)
    , mPlayerBounceRateY(0.0f)
    , mPlayerBounceKeep(0.0f)
    , mObjPushOutRate(0.0f)
    , mObjPushUpSpeed(0.0f)
    , mObjBounceRate(0.0f)
    , mKickRate(0.0f)
    , mKickRateY(0.0f)
    , mKickUpSpeed(0.0f)
    , mWaterPushRate(0.0f)
    , mSlopeRollRate(0.0f)
    , mWallRollUpRate(0.0f)
    , mMaxLandingSpeed(0.0f)
    , mFloatingDepth(0.0f)
    , mHoldHeightOffset(0.0f)
    , mHitCooldown(0)
{
	mInitialScaling.x = mInitialScaling.y = mInitialScaling.z = 0.0f;
}

void TResetFruit::checkGroundCollision(JGeometry::TVec3<f32>* pos)
{
	char trash[48];
	u8 map = gpMarDirector->mMap;
	if (map != 7 && map != 4) {
		TMapObjGeneral::checkGroundCollision(pos);
		return;
	}

	if (map == 4) {
		mGroundHeight
		    = gpMap->checkGround(pos->x, 200.0f + pos->y, pos->z, &mGroundPlane);
		mGroundHeight += 1.0f;
		if (pos->y <= mGroundHeight)
			touchGround(pos);
		else
			onLiveFlag(LIVE_FLAG_AIRBORNE);
	} else {
		mGroundHeight = gpMap->checkGround(pos->x, pos->y + mHeadHeight,
		                                   pos->z, &mGroundPlane);
		if (isPhaseThroughPlane(mGroundPlane))
			mGroundHeight = gpMap->checkGroundExactY(
			    pos->x, mGroundHeight - 200.0f, pos->z, &mGroundPlane);

		mGroundHeight += 1.0f;
		if (pos->y <= mGroundHeight)
			touchGround(pos);
		else
			onLiveFlag(LIVE_FLAG_AIRBORNE);
	}
}

void TResetFruit::waitingToAppear()
{
	if (gpMarDirector->mMap == 3 && unk1A4)
		makeObjDead();

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()
	    && getColNum() == 0) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		makeObjAppeared();

		Mtx scaleMtx;
		char trash[32];
		MTXScale(scaleMtx, 0.2f, 0.2f, 0.2f);
		concatOnlyRotFromLeft(scaleMtx, getModel()->getAnmMtx(0),
		                      getModel()->getAnmMtx(0));
		mScaling.y = 0.2f;
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mState = STATE_APPEARING;
		SMSGetMSound()->startSoundActor(0x3802, &mPosition, 0, nullptr, 0, 4);
	}
}

void TResetFruit::makeObjWaitingToAppear()
{
	char trash[16];
	mState = 0xB;
	makeObjDefault();
	makeObjDead();
	calcRootMatrix();
	getModel()->calc();
	mStateTimer = mFruitWaitTimeToAppear;
	offMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	mState = STATE_WAITING_TO_APPEAR;
	if (gpMarDirector->mMap == 3 && unk1A4)
		makeObjDead();
}

void TResetFruit::thrown()
{
	TMapObjGeneral::thrown();
	mState = 0xB;
}

void TResetFruit::hold(TTakeActor* actor)
{
	char trash[4];
	TMapObjBall::hold(actor);
	mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK10);

	if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && !isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		mStateTimer = getLivingTime();
	}
}

void TResetFruit::touchPollution()
{
	char trash[32];
	gpMarioParticleManager->emitAndBindToPosPtr(0x8B, &mPosition, 0, nullptr);
	SMSGetMSound()->startSoundActor(0x3881, &mPosition, 0, nullptr, 0, 4);
	makeObjDefault();
	makeObjWaitingToAppear();
}

void TResetFruit::touchWaterSurface()
{
	char trash[24];
	emitColumnWater();
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DRINA_TO_WATER, &mPosition, 0,
	                                nullptr, 0, 4);
	makeObjWaitingToAppear();
}

u32 TResetFruit::touchWater(THitActor* water)
{
	TMapObjBall::touchWater(water);
	makeObjLiving();
	return 1;
}

void TResetFruit::touchActor(THitActor* other)
{
	char trash[16];
	if (!(isState(STATE_APPEARING) || isState(STATE_BREAKING) || isState(0xC)
	      || isState(STATE_WAITING_TO_APPEAR))) {
		TMapObjBall::touchActor(other);

		if (!checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000) && isState(STATE_NORMAL)
		    && !checkLiveFlag(LIVE_FLAG_UNK10))
			makeObjLiving();
	}
}

void TResetFruit::touchGround(JGeometry::TVec3<f32>* pos)
{
	char trash[16];
	if (mGroundPlane->isDeathPlane()) {
		makeObjWaitingToAppear();
		pos->set(mPosition);
	} else {
		TMapObjBall::touchGround(pos);
	}
}

void TResetFruit::makeObjLiving()
{
	if (!isStateTimerEngaged()) {
		onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
		mStateTimer = getLivingTime();
	}
	offLiveFlag(LIVE_FLAG_UNK10);
	mState = 0xB;
}

void TResetFruit::kicked()
{
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK2000000))
		return;
	if (isState(STATE_HOLDING))
		return;

	f32 speedY = *gpMarioSpeedY;
	if (!(speedY >= 0.0f))
		return;

	JGeometry::TVec3<f32> vel = mVelocity;
	if (!(JGeometry::TVec3<f32>(vel).y <= 0.0f))
		return;

	f32 dot = JGeometry::TVec3<f32>(vel).dot(JGeometry::TVec3<f32>(
	    gpMarioPos->x - mPosition.x, 0.0f, gpMarioPos->z - mPosition.z));
	if (isAirborne() && dot > 0.0f)
		return;

	if (JGeometry::TVec3<f32>(vel).y == 0.0f)
		mVelocity.y = mKickUpSpeed;
	else
		mVelocity.y = mKickRateY * speedY
		              - mPlayerBounceKeep * JGeometry::TVec3<f32>(vel).y;

	mVelocity.x += mKickRate * *gpMarioSpeedX;
	mVelocity.z += mKickRate * *gpMarioSpeedZ;

	if (abs(mVelocity.x) < mMapObjData->mPhysical->unk4->unkC
	    && abs(mVelocity.z) < mMapObjData->mPhysical->unk4->unkC) {
		mVelocity.x = 2.0f * MsRandF() - 1.0f;
		mVelocity.z = 2.0f * MsRandF() - 1.0f;
	}

	mHitCooldown = 10;
	offLiveFlag(LIVE_FLAG_UNK10);
	SMS_GetMarioHitActor()->receiveMessage(this, HIT_MESSAGE_ATTACK);
	SMSGetMSound()->startSoundActor(MSD_SE_MA_KICK_DRIAN, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TResetFruit::breaking()
{
	Mtx scaleMtx;
	char trash[16];
	MTXScale(scaleMtx, 1.0f, mBreakingScaleSpeed, 1.0f);
	MtxPtr mtx = getModel()->getAnmMtx(0);
	concatOnlyRotFromLeft(scaleMtx, mtx, mtx);

	mScaling.y *= mBreakingScaleSpeed;
	mtx[1][3] = mBodyRadius * mScaling.y + mPosition.y;

	if (mScaling.y < 0.2f) {
		mPosition.y += 0.5f * mBodyRadius;
		mScaling.set(mInitialScaling);
		emitAndScale(0xE5, 0, &mPosition);
		SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr, 0, 4);
		mStateTimer = 240;
		sleep();
		mState = 0xD;
	}
}

void TResetFruit::appearing()
{
	Mtx scaleMtx;
	char trash[16];
	MTXScale(scaleMtx, mScaleUpSpeed, mScaleUpSpeed, mScaleUpSpeed);
	MtxPtr mtx = getModel()->getAnmMtx(0);
	concatOnlyRotFromLeft(scaleMtx, mtx, mtx);

	mScaling.y *= mScaleUpSpeed;
	mScaledBodyRadius = mBodyRadius * mScaling.y;
	mtx[1][3]         = mBodyRadius * mScaling.y + mPosition.y;

	if (mScaling.y >= mInitialScaling.y) {
		mScaling.set(mInitialScaling);
		getModel()->calc();
		offHitFlag(HIT_FLAG_NO_COLLISION);
		makeObjAppeared();
		mState = STATE_NORMAL;
	}
}

void TResetFruit::control()
{
	char trash[96];
	switch (mState) {
	case STATE_NORMAL:
		offHitFlag(HIT_FLAG_NO_COLLISION);
		for (int i = 0; i < getColNum(); ++i)
			TResetFruit::touchActor(getCollision(i));

		if (mGroundPlane->getActor())
			calcCurrentMtx();
		break;

	case 0xB:
		offHitFlag(HIT_FLAG_NO_COLLISION);
		if (gpMarDirector->mMap == 4 && checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		if (mGroundPlane->getActor()) {
			if (checkLiveFlag(LIVE_FLAG_UNK10))
				offLiveFlag(LIVE_FLAG_UNK10);

			f32 limit               = 200.0f + mGroundHeight;
			const TLiveActor* actor = mGroundPlane->getActor();
			if (mPosition.y < limit
			    && (actor->isActorType(0x400000CD)
			        || actor->isActorType(0x400000CD))) {
				f32 prevRatio  = mSandRiseRatio;
				mSandRiseRatio = SMS_GetSandRiseUpRatio(actor);
				if (mSandRiseRatio > 0.05f && mSandRiseRatio > prevRatio)
					mVelocity.y += 20.0f;
			}
		} else {
			mSandRiseRatio = 0.0f;
		}

		TMapObjBall::control();

		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
			break;
		if (isStateTimerEngaged())
			break;

		if (mHolder) {
			mHolder->receiveMessage(this, HIT_MESSAGE_UNK8);
			mHolder->mHeldObject = nullptr;
			mHolder              = nullptr;
		}
		mVelocity.set(0.0f, 0.0f, 0.0f);
		mState = 0xC;
		break;

	case STATE_HOLDING:
		TMapObjBall::control();

		if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
			break;
		if (isStateTimerEngaged())
			break;

		if (mHolder) {
			mHolder->receiveMessage(this, HIT_MESSAGE_UNK8);
			mHolder->mHeldObject = nullptr;
			mHolder              = nullptr;
		}
		mVelocity.set(0.0f, 0.0f, 0.0f);
		mState = 0xC;
		break;

	case STATE_APPEARING:
	case STATE_BREAKING:
		// inlined TMapObjBall::control
		TMapObjGeneral::control();

		if (mHitCooldown != 0)
			--mHitCooldown;

		if (isState(STATE_HOLDING)) {
			Mtx mtx;
			MTXCopy(mHolder->getTakingMtx(), mtx);
			mtx[1][3] += mHoldHeightOffset;
			MTXCopy(mtx, getModel()->getAnmMtx(0));
		} else {
			if (!JGeometry::TVec3<f32>(mVelocity).isZero()
			    || mGroundPlane->getActor())
				calcCurrentMtx();
		}
		break;

	case 0xC:
		mPosition.y += 0.5f * mBodyRadius;
		mScaling.set(mInitialScaling);
		emitAndScale(0xE5, 0, &mPosition);
		SMSGetMSound()->startSoundActor(0x387D, &mPosition, 0, nullptr, 0, 4);
		mStateTimer = 240;
		sleep();
		mState = 0xD;
		break;

	case 0xD:
		if (!isStateTimerEngaged()) {
			mTevColor.r = 0xFF;
			mTevColor.g = 0xFF;
			mTevColor.b = 0xFF;
			awake();
			makeObjWaitingToAppear();
		}
		break;
	}
}

void TResetFruit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[48];
	if (gpMarDirector->mMap == 7) {
		if (isState(STATE_HOLDING)
		    || !JGeometry::TVec3<f32>(mVelocity).isZero()) {
			if (checkLiveFlag(LIVE_FLAG_UNK200))
				offLiveFlag(LIVE_FLAG_UNK200);
		} else if (!gpCubeArea->isInAreaCube(mPosition)) {
			if (isState(0xB)
			    && (mPosition.x != mInitialPosition.x
			        || mPosition.z != mInitialPosition.z)) {
				makeObjWaitingToAppear();
				return;
			}
		}
	}

	TMapObjGeneral::perform(cue, graphics);
}

void TResetFruit::killByTimer(int time)
{
	mStateTimer = time;
	onMapObjFlag(MAP_OBJ_FLAG_DISAPPEARING);
	mState = 0xB;
}

void TResetFruit::makeObjAppeared()
{
	char trash[8];
	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		makeObjDefault();

	TMapObjBall::makeObjAppeared();

	if (checkMapObjFlag(MAP_OBJ_FLAG_UNK4000000))
		mState = 0xB;
}

BOOL TResetFruit::receiveMessage(THitActor* sender, u32 message)
{
	char trash[48];
	if (message == HIT_MESSAGE_UNKB) {
		if (isState(STATE_NORMAL) || isState(STATE_HOLDING) || isState(0xB)) {
			makeObjWaitingToAppear();
			return true;
		}
		return false;
	}

	if (message == HIT_MESSAGE_UNKD) {
		kill();
		return true;
	}

	if (isState(STATE_NORMAL) || isState(STATE_HOLDING) || isState(0xB)) {
		TResetFruit::touchActor(sender);

		BOOL ret = TMapObjBall::receiveMessage(sender, message);

		if (message == HIT_MESSAGE_PUT && isState(STATE_NORMAL))
			mState = 0xB;

		return ret;
	}

	return false;
}

void TResetFruit::initMapObj()
{
	TMapObjBall::initMapObj();
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &mTevColor);
}

TResetFruit::TResetFruit(const char* name)
    : TMapObjBall(name)
    , mSandRiseRatio(0.0f)
    , unk1A4(0)
{
	mTevColor.r = 0xFF;
	mTevColor.g = 0xFF;
	mTevColor.b = 0xFF;
	mTevColor.a = 0xFF;
}

void TRandomFruit::initMapObj()
{
	switch ((int)(5.0f * MsRandF())) {
	case 0:
		snprintf(mFruitName, sizeof(mFruitName), "FruitCoconut");
		break;
	case 1:
		snprintf(mFruitName, sizeof(mFruitName), "FruitDurian");
		break;
	case 2:
		snprintf(mFruitName, sizeof(mFruitName), "FruitPapaya");
		break;
	case 3:
		snprintf(mFruitName, sizeof(mFruitName), "FruitPine");
		break;
	case 4:
	case 5:
	default:
		snprintf(mFruitName, sizeof(mFruitName), "FruitPine");
		break;
	}
	unkF4 = mFruitName;

	TResetFruit::initMapObj();
}

TRandomFruit::TRandomFruit(const char* name)
    : TResetFruit(name)
{
	memset(mFruitName, 0, sizeof(mFruitName));
}

void TCoverFruit::calcRootMatrix()
{
	if (mHolder) {
		MtxPtr src = mHolder->getTakingMtx();
		MTXCopy(src, getModel()->getBaseTRMtx());
		mPosition.set(src[0][3], src[1][3], src[2][3]);
	} else {
		f32 y = mPosition.y - mYOffset;
		MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x, y,
		               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	}
	getModel()->setBaseScale(mScaling);
}

BOOL TCoverFruit::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x08000083) && message == HIT_MESSAGE_TAKE) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return true;
	}

	if (message == HIT_MESSAGE_UNKB) {
		kill();
		TFlagManager::smInstance->setBool(true, 0x1038B);
		return true;
	}

	return false;
}

void TCoverFruit::loadAfter()
{
	TMapObjBase::loadAfter();
	if (TFlagManager::smInstance->getBool(0x1038B))
		makeObjDead();
}

void TBigWatermelon::touchWaterSurface()
{
	char trash[8];
	emitColumnWater();
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_DRINA_TO_WATER, &mPosition, 0,
	                                nullptr, 0, 4);
	kill();
}

void TBigWatermelon::touchWall(JGeometry::TVec3<f32>* pos,
                               TBGWallCheckRecord* record)
{
	TMapObjBall::touchWall(pos, record);
}

void TBigWatermelon::rebound(JGeometry::TVec3<f32>* pos)
{
	char trash[48];
	if (isState(0xC)) {
		kill();
		*pos = mPosition;
	} else {
		TMapObjBall::rebound(pos);
		if (isState(0xB))
			mState = 0xC;
	}
}

void TBigWatermelon::touchGround(JGeometry::TVec3<f32>* pos)
{
	TMapObjBall::touchGround(pos);
}

void TBigWatermelon::touchActor(THitActor* other)
{
	char trash[24];
	if (isState(STATE_APPEARING))
		return;

	if (!isState(STATE_NORMAL) && JGeometry::TVec3<f32>(mVelocity).y < 0.0f) {
		kill();
		return;
	}

	if (other->isActorType(0x80000001)) {
		if (mPosition.distance(other->mPosition) < 0.6f * mBodyRadius) {
			kill();
			return;
		}
	}

	if (other->isActorType(0x10000015) && ((TPoiHana*)other)->isMoving()) {
		if (abs(mVelocity.y) < mMapObjData->mPhysical->unk4->unkC) {
			mVelocity.y += 30.0f;
			mState = 0xB;
		}
		return;
	}

	// inlined TMapObjBall::touchActor
	if (mHitCooldown != 0)
		return;
	if (isState(STATE_HOLDING))
		return;
	if (isHideObj(other))
		return;
	if (other->isActorType(0x08000083))
		return;
	if (other->isActorType(0x400000CA))
		return;
	if (other->isActorType(0x400000CC))
		return;

	if (other->isActorType(0x80000001) && !isActorType(0x400000D0)
	    && *gpMarioSpeedY != 0.0f)
		kicked();
	else
		boundByActor(other);
}

void TBigWatermelon::kill()
{
	emitAndScale(0x5D, 0, &mPosition);
	emitAndScale(0x5E, 0, &mPosition);
	emitAndScale(0x5F, 0, &mPosition);
	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	char trash[12];
	emitAndScale(0x6B, 0, &mPosition, scale);
	emitAndScale(0x6C, 0, &mPosition, scale);

	mWaterEmitInfo->mPos.value = mPosition;
	gpModelWaterManager->emitRequest(*mWaterEmitInfo);
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_WATERMELON_BLOCK, &mPosition, 0,
	                                nullptr, 0, 4);

	if (mShineCoinNum < 10) {
		TMapObjBase* obj = gpItemManager->makeObjAppear(
		    mPosition.x, mPosition.y, mPosition.z, 0x2000000E, true);
		if (obj) {
			obj->setVelocityAndFlag10(0.0f, 25.0f, 0.0f);
			++mShineCoinNum;
		}
	}

	TMapObjGeneral::kill();
}

void TBigWatermelon::appearing()
{
	char trash[16];
	TMapObjGeneral::appearing();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	calcRootMatrix();
	getModel()->calc();
	mtx[1][3] = mBodyRadius * (mScaling.y / mInitialScaling.y) + mPosition.y;

	mScaledBodyRadius = 50.0f * mScaling.x;
	setDamageRadius(50.0f * mScaling.x);

	if (isState(STATE_NORMAL)) {
		mActorType = 0x400000D0;
		setAttackRadius(50.0f * mScaling.x);
	} else {
		mActorType = 0x400000DB;
		setAttackRadius(0.0f);
	}
}

void TBigWatermelon::control()
{
	char trash[8];
	// inlined TMapObjBall::control
	TMapObjGeneral::control();

	if (mHitCooldown != 0)
		--mHitCooldown;

	if (isState(STATE_HOLDING)) {
		Mtx mtx;
		MTXCopy(mHolder->getTakingMtx(), mtx);
		mtx[1][3] += mHoldHeightOffset;
		MTXCopy(mtx, getModel()->getAnmMtx(0));
	} else {
		if (!JGeometry::TVec3<f32>(mVelocity).isZero()
		    || mGroundPlane->getActor())
			calcCurrentMtx();
	}

	switch (mState) {
	case STATE_NORMAL: {
		if (checkLiveFlag(LIVE_FLAG_UNK10))
			offLiveFlag(LIVE_FLAG_UNK10);

		f32 limit = 200.0f + mGroundHeight;
		const TLiveActor* actor = mGroundPlane->getActor();
		if (mPosition.y < limit && actor
		    && (actor->isActorType(0x400000CD)
		        || actor->isActorType(0x400000CD))) {
			f32 prevRatio  = mSandRiseRatio;
			mSandRiseRatio = SMS_GetSandRiseUpRatio(actor);
			if (mSandRiseRatio > 0.05f && mSandRiseRatio > prevRatio)
				mVelocity.y += 20.0f;
		}
		break;
	}
	case STATE_APPEARING:
		break;
	case STATE_WAITING_TO_APPEAR:
		break;
	case 0xD:
		if (!isStateTimerEngaged()) {
			JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
			emitAndScale(0x6B, 0, &mPosition, scale);
			emitAndScale(0x6C, 0, &mPosition, scale);
			mStateTimer = 30;
		}
		if (animIsFinished())
			makeObjDead();
		break;
	}
}

void TBigWatermelon::startEvent()
{
	// Translated: "Watermelon (large)"
	if (strcmp(mName, "スイカ（大）") == 0) {
		mPosition.set(-4660.0f, 1300.0f, 13600.0f);
		offMapObjFlag(MAP_OBJ_FLAG_UNK100);
		onLiveFlag(LIVE_FLAG_UNK10);
		mVelocity.x = mVelocity.y = mVelocity.z = 0.0f;
		onLiveFlag(LIVE_FLAG_UNK10);
		startAnim(7);

		// Translated: "Watermelon goal camera"
		TMarDirector* director = gpMarDirector;
		director->fireStartDemoCamera("スイカゴールカメラ", &mPosition, -1,
		                              0.0f, true, nullptr, 0, nullptr,
		                              JDrama::TFlagT<u16>(0));
		// Translated: "Shine (for ghost watermelon)", "Watermelon shine camera"
		gpItemManager->makeShineAppearWithDemoOffset(
		    "シャイン（お化けスイカ用）", "スイカシャインカメラ", 0.0f, 0.0f,
		    0.0f);
		mStateTimer = 0x17C;
		mState      = 0xD;
	} else {
		for (int i = 0; i < 10; ++i) {
			TCoin* coin = (TCoin*)gpItemManager->makeObjAppear(
			    gpMarioPos->x, gpMarioPos->y, gpMarioPos->z, 0x2000000E, true);
			if (coin) {
				coin->setVelocityAndFlag10(20.0f * (MsRandF() - 0.5f),
				                           20.0f * MsRandF() + 20.0f,
				                           20.0f * (MsRandF() - 0.5f));
				coin->unk14C = 0x3C0;
			}
		}
		makeObjDead();
	}
}

void TBigWatermelon::checkWallCollision(JGeometry::TVec3<f32>* pos)
{
	TMapObjGeneral::checkWallCollision(pos);
}

BOOL TBigWatermelon::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->isActorType(0x80000001)) {
		boundByActor(sender);
		return true;
	}

	return TMapObjBall::receiveMessage(sender, message);
}

void TBigWatermelon::loadAfter()
{
	char trash[8];
	TMapObjGeneral::loadAfter();

	// Translated: "Shine (for ghost watermelon)"
	JDrama::TActor* shine
	    = (JDrama::TActor*)JDrama::TNameRefGen::getInstance()
	          ->getRootNameRef()
	          ->search("シャイン（お化けスイカ用）");
	shine->mPosition.set(-4659.0f, 460.0f, 13620.0f);
}

void TBigWatermelon::initMapObj()
{
	TMapObjBall::initMapObj();

	SMS_LoadParticle("/scene/mapObj/watermelon_bomb.jpa", 0x5D);
	SMS_LoadParticle("/scene/mapObj/watermelon_bomb_a.jpa", 0x5E);
	SMS_LoadParticle("/scene/mapObj/watermelon_bomb_b.jpa", 0x5F);
	SMS_LoadParticle("/scene/mapObj/watermelon_shrink_a.jpa", 0x6B);
	SMS_LoadParticle("/scene/mapObj/watermelon_shrink_b.jpa", 0x6C);

	mWaterEmitInfo = new TWaterEmitInfo("/watermelon.prm");
}

TBigWatermelon::TBigWatermelon(const char* name)
    : TMapObjBall(name)
    , mWaterEmitInfo(nullptr)
    , mShineCoinNum(0)
    , mSandRiseRatio(0.0f)
{
}
