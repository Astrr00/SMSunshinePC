#include <MoveBG/MapObjBianco.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <System/MarDirector.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/WaterGun.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Camera/CubeMapTool.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#ifdef TARGET_PC
#include <string.h> // the MSL header is only here to match sinit/bss order and clashes with the CRT
#else
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static f32 sRadius = 2380.0f;
static f32 sSubZ   = 150.0f;
static f32 sSpeed  = 0.05f;

f32 TMapObjRootPakkun::mTremblePower  = 15.0f;
f32 TMapObjRootPakkun::mTrembleAccel  = 0.95f;
f32 TMapObjRootPakkun::mTrembleBrake  = 0.98f;
int TMapObjRootPakkun::mTrembleTime   = 360;

f32 TBiancoWatermillVertical::mRotAccel         = 0.15f;
f32 TBiancoWatermillVertical::mRotSpeedDownRate = 0.005f;
f32 TBiancoWatermillVertical::mRotSpeedMax      = 3.0f;
f32 TBiancoWatermillVertical::mBridgeRotRate    = 0.03f;

f32 TBiancoMiniWindmill::mRotWaterAccel = 0.01f;
f32 TBiancoMiniWindmill::mFriction      = 0.01f;
f32 TBiancoMiniWindmill::mRotSpeedMax   = 10.0f;

static f32 sMessengerPosZ = 200.0f;
static f32 sMessengerPosY = 6400.0f;

f32 TLeafBoatRotten::mAlphaDownSpeed       = 0.5f;
f32 TLeafBoatRotten::mCollisionRemoveAlpha = 100.0f;
GXColorS10 TLeafBoatRotten::mRottenColor   = { 100, 100, 180, 255 };

static f32 sAngleAdd;

void TWoodLog::control()
{
	TMapObjFloatOnSea::control();

	Mtx inv;
	MTXInverse(getModel()->getAnmMtx(0), inv);

	JGeometry::TVec3<f32> marioPos;
	marioPos.x = gpMarioPos->x;
	marioPos.y = gpMarioPos->y;
	marioPos.z = gpMarioPos->z;
	JGeometry::TVec3<f32> local;
	MTXMultVec(inv, marioPos, local);

	if (SMS_IsMarioStatusTypeSwimming() && -232.0f < local.y
	    && -141.0f < local.x && local.x < 141.0f && -441.0f < local.z
	    && local.z < 441.0f) {
		if (local.x > 0.0f)
			local.x = 141.0f;
		else
			local.x = -141.0f;

		JGeometry::TVec3<f32> world;
		MTXMultVec(getModel()->getAnmMtx(0), local, world);
		SMS_MarioMoveRequest(world);
	}
}

TBellWatermill::TBellWatermill(const char* name)
    : TMapObjTurn(name)
    , unk16C(0.0f)
    , unk170(0.0f)
    , unk174(0.0f)
    , unk178(0.0f)
    , unk17C(0.0f)
    , unk180(0.0f)
    , unk184(0.0f)
    , unk188(0.0f)
    , unk18C(0.0f)
    , unk190(0)
    , unk1A0(0)
    , unk1A4(nullptr)
{
}

void TBellWatermill::loadAfter()
{
	TMapObjTurn::loadAfter();

	unk150 = 2;
	unk15C = -0.02f;
	unk160 = -0.008f;
	unk164 = 10.0f;
	unk18C = 10.0f;
	unk174 = 1000.0f;
	unk180 = 0.15f;
	unk184 = 0.1f;
	unk16C = 4.0f;
	unk188 = 0.5f;
	unk17C = 1.0f;

	unk194 = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 0");
	unk198 = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 1");
	unk19C = (TBiancoBell*)JDrama::TNameRefGen::search("BiaBell 2");
	unk1A0 = 1;
}

// Fabricated: a bell restarts its ring animation when it is idle (frame 0) or
// about to finish. A macro, because an inline bool function would materialize
// the condition differently from the original.
#define BELL_CAN_RING(bell)                                                    \
	((bell)->getMActor()->getFrameCtrl(0)->getFrame() == 0.0f                  \
	 || (bell)->getMActor()->getFrameCtrl(0)->getFrame()                       \
	            + (bell)->getMActor()->getFrameCtrl(0)->getRate()              \
	        >= (bell)->getMActor()->getFrameCtrl(0)->getEnd() - 1.0f)

void TBellWatermill::control()
{
	TMapObjBase::control();

	if (unk158 != 0.0f || unk178 != 0.0f || unk170 != 0.0f) {
		f32 speed = unk158;
		if (speed > unk16C)
			speed = unk16C;
		else if (speed < -unk16C)
			speed = -unk16C;
		unk154 += speed;
		unk154 = MsWrap(unk154, 0.0f, 360.0f);

		SMSGetMSound()->startSoundActorWithInfo(
		    MSD_SE_OBJ_BI_UPDOWNMILL, &mPosition, nullptr, fabsf(unk158), 0, 0,
		    &unk1A4, 0, 4);

		if (fabsf(unk158) < fabsf(unk160))
			unk158 = 0.0f;

		if (!unk190 && unk158 != 0.0f)
			unk158 -= unk160;

		if (unk170 < 0.0f) {
			if (fabsf(unk178) < unk17C) {
				unk170 = 0.0f;
				unk178 = 0.0f;
			} else {
				unk170 -= unk178;
				unk178 = unk178 * -unk188;
			}
		} else if (!unk190 && unk170 != 0.0f) {
			unk178 -= unk184;
		}

		unk170 += unk178;
		if (unk170 > unk174) {
			TBiancoBell* bell = unk194;
			if (BELL_CAN_RING(bell)) {
				bell->startAnim(bell->unk138);
				bell->getMActor()->getFrameCtrl(0)->setRate(
				    SMSGetAnmFrameRate());
				if (bell->unk13A)
					SMSGetMSound()->startSoundActor(
					    MSD_SE_OBJ_BI_BELL, &bell->mPosition, 0, nullptr, 0,
					    4);
			}

			bell = unk198;
			if (BELL_CAN_RING(bell)) {
				bell->startAnim(bell->unk138);
				bell->getMActor()->getFrameCtrl(0)->setRate(
				    SMSGetAnmFrameRate());
				if (bell->unk13A)
					SMSGetMSound()->startSoundActor(
					    MSD_SE_OBJ_BI_BELL, &bell->mPosition, 0, nullptr, 0,
					    4);
			}

			bell = unk19C;
			if (BELL_CAN_RING(bell)) {
				bell->startAnim(bell->unk138);
				bell->getMActor()->getFrameCtrl(0)->setRate(
				    SMSGetAnmFrameRate());
				if (bell->unk13A)
					SMSGetMSound()->startSoundActor(
					    MSD_SE_OBJ_BI_BELL, &bell->mPosition, 0, nullptr, 0,
					    4);
			}

			unk170 = unk174;
			if (unk1A0) {
				for (int i = 0; i < 5; ++i) {
					TMapObjBase* coin = gpItemManager->makeObjAppeared(0x2000000E);
					if (coin) {
						coin->mPosition.x = mPosition.x;
						coin->mPosition.y = mPosition.y;
						coin->mPosition.z = mPosition.z;
						f32 a = rand() * (1.0f / (RAND_MAX + 1));
						f32 b = rand() * (1.0f / (RAND_MAX + 1));
						coin->mVelocity.x = 10.0f;
						coin->mVelocity.y = 100.0f * b + 10.0f;
						coin->mVelocity.z = 10.0f * a - 5.0f;
						coin->offLiveFlag(LIVE_FLAG_UNK10);
					}
				}
				unk1A0 = 0;
			}
		}

		mPosition.y = unk170 + mInitialPosition.y + mYOffset;
		unk190      = 0;

		mRotation.z = unk154;

		Mtx yRot;
		Mtx mtx;
		MsMtxSetRotZ(mtx, mRotation.z);
		if (mRotation.y != 0.0f) {
			MsMtxSetRotZ(mtx, mRotation.z);
			MsMtxSetRotY(yRot, mRotation.y);
			MTXConcat(yRot, mtx, mtx);
		} else {
			MsMtxSetRotZ(mtx, mRotation.z);
		}

		MtxPtr ptr = mtx;
		ptr[0][3]  = mPosition.x;
		ptr[1][3]  = mPosition.y;
		ptr[2][3]  = mPosition.z;
		ptr[1][3] -= mYOffset;
		getModel()->setAnmMtx(0, ptr);
	}
}

u32 TBellWatermill::touchWater(THitActor*)
{
	unk190 = 1;
	if (fabsf(unk158) > unk16C) {
		unk178 += unk180;
		unk158 += unk15C;
	} else {
		unk158 += unk15C;
	}

	if (unk158 > unk164)
		unk158 = unk164;
	return 1;
}

TBiancoBell::TBiancoBell(const char* name)
    : TMapObjBase(name)
    , unk138(0)
    , unk13A(0)
{
}

void TBiancoBell::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp(mName, "BiaBell 0") == 0) {
		unk138 = 1;
		unk13A = 0;
	} else if (strcmp(mName, "BiaBell 1") == 0) {
		unk138 = 2;
		unk13A = 1;
	} else {
		unk138 = 3;
		unk13A = 0;
	}
}

void TBiancoBell::touchPlayer(THitActor*)
{
	if (BELL_CAN_RING(this)) {
		startAnim(4);
		getMActor()->getFrameCtrl(0)->setRate(SMSGetAnmFrameRate());
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_BI_BELL, &mPosition, 0,
		                                nullptr, 0, 4);
	}
}

u32 TBiancoBell::touchWater(THitActor*)
{
	if (BELL_CAN_RING(this)) {
		startAnim(4);
		getMActor()->getFrameCtrl(0)->setRate(SMSGetAnmFrameRate());
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_BI_BELL, &mPosition, 0,
		                                nullptr, 0, 4);
	}
	return 1;
}

TLampSeesawMain::TLampSeesawMain(const char* name)
    : TLampSeesaw(name)
    , unk144(0.0f)
    , unk148(0.998f)
    , unk14C(0.8f)
    , unk150(0.5f)
{
}

void TLampSeesawMain::loadAfter()
{
	size_t len = strlen("ランプシーソーＡ");
	char buffer[4];
	buffer[0] = mName[len];
	buffer[1] = mName[len + 1];
	buffer[2] = mName[len + 2];
	buffer[3] = mName[len + 3];

	char buffer2[0x40];
	snprintf(buffer2, 0x40, "ランプシーソーＢ００");
	buffer2[len]     = buffer[0];
	buffer2[len + 1] = buffer[1];
	buffer2[len + 2] = buffer[2];
	buffer2[len + 3] = buffer[3];

	unk138         = (TLampSeesaw*)JDrama::TNameRefGen::search(buffer2);
	unk138->unk138 = this;
}

void TLampSeesawMain::control()
{
	TMapObjBase::control();

	switch (mState) {
	case 2:
		if (mPosition.y + unk144 < unk13C
		    || unk138->mPosition.y - unk144 < unk138->unk13C) {
			if (fabsf(unk144) < unk150)
				unk144 = 0.0f;
			else
				unk144 = unk144 * -unk14C;
		} else {
			mPosition.y += unk144;
			unk138->mPosition.y = unk138->mPosition.y - unk144;
		}

		unk144 = unk144 * unk148;
		if (fabsf(unk144) < unk150)
			mState = 3;
		break;

	case 3:
		if (unk144 < 0.0f)
			unk144 = -unk150;
		else
			unk144 = unk150;

		if (mPosition.y + unk144 < unk13C
		    || unk138->mPosition.y - unk144 < unk138->unk13C) {
			if (fabsf(unk144) < unk150)
				unk144 = 0.0f;
			else
				unk144 = unk144 * -unk14C;
		} else {
			mPosition.y += unk144;
			unk138->mPosition.y = unk138->mPosition.y - unk144;
		}

		unk144 = unk144 * unk148;
		if (fabsf(unk144) <= unk150
		    && fabsf(mInitialPosition.y - mPosition.y) < unk150) {
			unk144 = 0.0f;
			mState = 1;
		}
		break;

	case 1:
		break;
	}
}

void TLampSeesawMain::touchPlayer(THitActor*)
{
	if (marioIsOn())
		pushDown(unk140);
}

void TLampSeesawMain::pushDown(f32 amount)
{
	mState = 2;
	unk144 -= amount;
}

TLampSeesaw::TLampSeesaw(const char* name)
    : TMapObjBase(name)
    , unk138(nullptr)
    , unk140(0.01f)
{
}

void TLampSeesaw::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);

	f32 height;
	stream >> height;
	unk13C = mInitialPosition.y - height;
	stream >> unk140;
	unk140 *= 0.0001f;
}

void TLampSeesaw::touchPlayer(THitActor*)
{
	if (marioIsOn())
		unk138->pushDown(-unk140);
}

TLeafBoatRotten::TLeafBoatRotten(const char* name)
    : TLeafBoat(name)
    , unk170(0)
{
	unk178.r = 0xFF;
	unk178.g = 0xFF;
	unk178.b = 0xFF;
	unk178.a = 0xFF;
}

void TLeafBoatRotten::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream >> unk170;
	unk170 *= 10;
	SMS_InitPacket_OneTevColor(getModel(), 0, GX_TEVREG0, &unk178);
}

void TLeafBoatRotten::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
}

void TLeafBoatRotten::control()
{
	TLeafBoat::control();

	if (marioIsOn() && isState(1)) {
		mStateTimer = unk170;
		mState      = 2;
	}

	switch (mState) {
	case 2: {
		f32 ratio = (f32)mStateTimer / (f32)unk170;
		unk178.r  = (u8)((255 - mRottenColor.r) * ratio + mRottenColor.r);
		unk178.g  = (u8)((255 - mRottenColor.g) * ratio + mRottenColor.g);
		unk178.b  = (u8)((255 - mRottenColor.b) * ratio + mRottenColor.b);
		if (!isStateTimerEngaged()) {
			unk174 = 255.0f;
			mState = 3;
		}
		break;
	}

	case 3:
		unk174 -= mAlphaDownSpeed;
		unk178.a = (u8)unk174;
		if (unk174 < mCollisionRemoveAlpha) {
			if (mMapCollisionManager->unk8->isSetUp())
				removeMapCollision();
		}

		if (unk174 <= 0.0f) {
			mScaling.set(1.0f, 1.0f, 1.0f);
			makeObjDefault();
			makeObjAppeared();
			unk178.r = 0xFF;
			unk178.g = 0xFF;
			unk178.b = 0xFF;
			unk178.a = 0xFF;
			mState   = 1;
		}
		break;

	case 1:
		break;
	}
}

TLeafBoat::TLeafBoat(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0.03f)
    , unk148(0.0f)
    , unk14C(1.2f)
    , unk150(0.03f)
    , unk154(2.0f)
    , unk158(0.005f)
    , unk15C(0.98f)
    , unk160(0)
    , unk164(0.0f)
{
}

void TLeafBoat::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 1.0f;
	unk13C = 0.5f;
	unk140 = 0.5f;
	unk148 = 0.998f;
}

void TLeafBoat::calc()
{
	if (unk144 != 0.0f) {
		if (unk160 > 8) {
			if (fabsf(mVelocity.x) + fabsf(mVelocity.z) > 0.1f) {
				unk164.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
				JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
				emitAndBindScale(0x1E8, 3, &unk164, scale);
				emitAndBindScale(0x107, 1, &unk164, scale);
			}
			unk160 = 0;
		} else {
			unk160 = unk160 + 1;
		}
	}
}

void TLeafBoat::control()
{
	TMapObjBase::control();

	if (marioHipAttack())
		mVelocity.y -= unk154;

	if (marioIsOn()) {
		mVelocity.y -= unk150;
		if (SMS_GetMarioWaterGun()->mIsEmitWater > 0) {
			MtxPtr emitMtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
			mVelocity.x -= emitMtx[0][0] * unk144;
			mVelocity.z -= emitMtx[2][0] * unk144;
		}
	}

	int cubeNo = gpCubeStream->getInCubeNo(mPosition);
	if (cubeNo != -1) {
		TCubeStreamInfo* info
		    = (TCubeStreamInfo*)gpCubeStream->unk14->begin()[cubeNo];
		Mtx mtx;
		MsMtxSetXYZRPH(mtx, 0.0f, 0.0f, 0.0f, info->unk18.x, info->unk18.y,
		               info->unk18.z);
		f32 flow = 0.0001f * info->unk40;
		mVelocity.x += mtx[0][2] * flow;
		mVelocity.z += mtx[2][2] * flow;
	}

	mPosition.y += mVelocity.y;
	mVelocity.y += unk158 * (mInitialPosition.y - (mPosition.y - mYOffset));
	mVelocity.y *= unk15C;
	mVelocity.x *= unk148;
	mVelocity.z *= unk148;
}

void TLeafBoat::bind()
{
	const TBGCheckData* ground;
	JGeometry::TVec3<f32> newPos(mPosition);
	JGeometry::TVec3<f32> velX(mVelocity);
	newPos.x += velX.x;
	JGeometry::TVec3<f32> velZ(mVelocity);
	newPos.z += velZ.z;

	f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
	    newPos.x, mPosition.y - mYOffset, newPos.z, &ground);
	if (groundY > mPosition.y - mYOffset - 50.0f) {
		JGeometry::TVec3<f32> reflected(mVelocity);
		calcReflectingVelocity(ground, 1.0f, &reflected);
		mVelocity.x *= -1.0f;
		mVelocity.z *= -1.0f;
		newPos = mPosition;
	}

	JGeometry::TVec3<f32> center(newPos.x, newPos.y - mYOffset, newPos.z);
	TBGWallCheckRecord record(center, mBodyRadius, 4,
	                          TBGWallCheckRecord::DONT_MOVE_XZ);
	if (gpMap->isTouchedWallsAndMoveXZ(&record))
		touchWall(&newPos, &record);

	mLinearVelocity = newPos - mPosition;

	f32 y  = mPosition.y - mYOffset;
	f32 dx = gpMarioPos->x - mPosition.x;
	f32 dz = gpMarioPos->z - mPosition.z;
	if (gpMarioPos->y <= y && y - 100.0f < gpMarioPos->y
	    && dx * dx + dz * dz < mBodyRadius * mBodyRadius)
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
}

void TLeafBoat::touchWall(JGeometry::TVec3<f32>* pos,
                          TBGWallCheckRecord* record)
{
	int wallNum = record->mResultWallsNum;
	for (int i = 0; i < wallNum; ++i) {
		TBGCheckData* wall = record->mResultWalls[i];
		JGeometry::TVec3<f32> vel(mVelocity);
		if (vel.dot(wall->mNormal) < 0.0f) {
			f32 dist = pos->dot(wall->mNormal) + wall->mPlaneDistance;
			f32 pen  = mBodyRadius - dist;
			pos->x += pen * wall->mNormal.x;
			pos->z += pen * wall->mNormal.z;

			JGeometry::TVec3<f32> reflected(mVelocity);
			calcReflectingVelocity(wall, 1.0f, &reflected);
			mVelocity.x = reflected.x * unk140;
			mVelocity.z = reflected.z * unk140;
			break;
		}
	}
}

void TLeafBoat::touchActor(THitActor* actor)
{
	if (actor->isActorType(0x80000001))
		return;

	JGeometry::TVec3<f32> dir(actor->mPosition.x - mPosition.x, 0.0f,
	                          actor->mPosition.z - mPosition.z);
	JGeometry::TVec3<f32> vel(mVelocity);
	if (dir.dot(vel) < 0.0f)
		return;

	if (dir.x != 0.0f || dir.z != 0.0f)
		MsVECNormalize(&dir, &dir);

	JGeometry::TVec3<f32> vel2(mVelocity);
	f32 dot = dir.dot(vel2);
	if (actor->checkActorType(ACTOR_TYPE_ENEMY)) {
		mVelocity.x -= (1.0f + unk138) * (dir.x * dot);
		mVelocity.z -= (1.0f + unk138) * (dir.z * dot);
	} else {
		mVelocity.x -= (1.0f + unk13C) * (dir.x * dot);
		mVelocity.z -= (1.0f + unk13C) * (dir.z * dot);
	}
}

TBiancoMiniWindmill::TBiancoMiniWindmill(const char* name)
    : THideObjBase(name)
    , unk150(rand() * (1.0f / (RAND_MAX + 1)) * 360.0f)
    , unk154(0.0f)
    , unk158(1.0f + MsRandF())
    , unk15C(nullptr)
    , unk160(nullptr)
{
}

void TBiancoMiniWindmill::initMapObj()
{
	TMapObjBase::initMapObj();
	mAppearSpeed = 0.0f;

	unk15C = new TMapObjMessenger("地形オブジェメッセンジャー");
	unk15C->initHitActor(0, 1, 0, 0.0f, 0.0f, 300.0f, 500.0f);
	unk15C->mPosition.x
	    = sMessengerPosZ * JMASSin(mRotation.y * (65536.0f / 360.0f))
	      + mPosition.x;
	unk15C->mPosition.y = mPosition.y + sMessengerPosY;
	unk15C->mPosition.z
	    = sMessengerPosZ * JMASCos(mRotation.y * (65536.0f / 360.0f))
	      + mPosition.z;
}

void TBiancoMiniWindmill::control()
{
	if (unk154 > unk158)
		unk154 -= mFriction;
	else
		unk154 = unk158;

	unk150 += unk154;
	unk150 = MsWrap(unk150, 0.0f, 360.0f);
}

void TBiancoMiniWindmill::calc()
{
	TMtx34f rot;
	rot.identity();
	MsMtxSetRotZ(rot, unk150);
	MTXConcat(getModel()->getAnmMtx(0), rot, rot);

	MtxPtr joint = getModel()->getAnmMtx(1);
	rot.ref(0, 3) = joint[0][3];
	rot.ref(1, 3) = joint[1][3];
	rot.ref(2, 3) = joint[2][3];
	getModel()->setAnmMtx(1, rot);

	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_OBJ_BI_COINMILL, &mPosition, nullptr, fabsf(unk154), 0, 0,
	    &unk160, 0, 4);
}

u32 TBiancoMiniWindmill::touchWater(THitActor* water)
{
	const JGeometry::TVec3<f32>& waterPos = getWaterPos(water);
	if (waterPos.y < mPosition.y + sMessengerPosY - 300.0f)
		return 1;

	const JGeometry::TVec3<f32>& waterSpeed = getWaterSpeed(water);
	MtxPtr mtx                              = getModel()->getAnmMtx(0);
	if (waterSpeed.x * mtx[0][2] + waterSpeed.y * mtx[1][2]
	        + waterSpeed.z * mtx[2][2]
	    > 0.0f)
		return 0;

	unk154 += mRotWaterAccel;
	if (unk154 > mRotSpeedMax) {
		unk154 = mRotSpeedMax;
		JGeometry::TVec3<f32> pos(mPosition.x, 550.0f + unk15C->mPosition.y,
		                          mPosition.z);
		mAppearSpeed = 0.0f;
		appearObjFromPoint(pos);
	}
	return 1;
}

TBiancoWatermillVertical::TBiancoWatermillVertical(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(nullptr)
    , unk144(0)
    , unk148(nullptr)
    , unk14C(nullptr)
{
}

void TBiancoWatermillVertical::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream >> unk13C;
	unk13C /= 1000.0f;
	unk138 = unk13C;
}

void TBiancoWatermillVertical::loadAfter()
{
	TMapObjBase::loadAfter();
	if (strcmp(mName, "BiaWatermillVertical 0") == 0)
		unk140 = (TMapObjBase*)JDrama::TNameRefGen::search("BiaTurnBridge 0");
	else
		unk140 = (TMapObjBase*)JDrama::TNameRefGen::search("BiaTurnBridge 1");
	mBodyRadius = 1000.0f;
}

void TBiancoWatermillVertical::control()
{
	if (unk138 != unk13C) {
		if (unk138 > unk13C) {
			unk138 -= mRotSpeedDownRate;
			if (unk138 < unk13C)
				unk138 = unk13C;
		} else {
			unk138 += mRotSpeedDownRate;
			if (unk138 > unk13C)
				unk138 = unk13C;
		}
	}

	mRotation.y += unk138;
	mRotation.y = MsWrap(mRotation.y, 0.0f, 360.0f);

	f32 bridgeRot = unk138 * mBridgeRotRate;
	unk140->mRotation.y += bridgeRot;
	unk140->mRotation.y = MsWrap(unk140->mRotation.y, 0.0f, 360.0f);

	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_OBJ_BI_STEPMILL_WIND, &mPosition, nullptr, fabsf(unk138), 0, 0,
	    &unk148, 0, 4);
	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_OBJ_BI_STEPMILL_MOVE, &unk140->mPosition, nullptr,
	    fabsf(bridgeRot), 0, 0, &unk14C, 0, 4);
}

void TBiancoWatermillVertical::setGroundCollision()
{
	if (unk144 || mColCount != 0) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (TMapCollisionBase* col = mMapCollisionManager->unk8)
			col->moveMtx(mtx);
		unk144 = 0;
	}
}

u32 TBiancoWatermillVertical::touchWater(THitActor* water)
{
	if (getWaterPlane(water) == nullptr) {
		unk144 = 1;
		return 0;
	}

	if (!waterHitPlane(water))
		return 0;

	const JGeometry::TVec3<f32>& waterPos   = getWaterPos(water);
	const JGeometry::TVec3<f32>& waterSpeed = getWaterSpeed(water);

	JGeometry::TVec3<f32> dir(waterSpeed.x, 0.0f, waterSpeed.z);
	if (dir.x != 0.0f || dir.z != 0.0f)
		MsVECNormalize(&dir, &dir);

	JGeometry::TVec3<f32> perp;
	getVerticalVecToTargetXZ(waterPos.x, waterPos.z, &perp);
	MsVECNormalize(&perp, &perp);

	f32 radius = mBodyRadius;
	f32 ratio  = (radius - getDistanceXZ(waterPos)) / radius;
	if (dir.dot(perp) > 0.0f) {
		if (unk138 < mRotSpeedMax)
			unk138 += mRotAccel * ratio;
	} else {
		if (unk138 > -mRotSpeedMax)
			unk138 -= mRotAccel * ratio;
	}
	return 1;
}

TBiancoWatermill::TBiancoWatermill(const char* name)
    : TMapObjBase(name)
    , unk138(0.3f)
    , unk13C(nullptr)
{
}

void TBiancoWatermill::initMapObj()
{
	TMapObjBase::initMapObj();
	if (strcmp(unkF4, "BiaWatermill01") == 0)
		mBodyRadius = 1200.0f;
	else if (strcmp(unkF4, "BiaWatermill00") == 0)
		mBodyRadius = 1200.0f;
}

void TBiancoWatermill::control()
{
	mRotation.z -= unk138;
	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_OBJ_BI_BIGMILL, &mPosition, nullptr, fabsf(unk138), 0, 0,
	    &unk13C, 0, 4);
}

u32 TBiancoWatermill::touchWater(THitActor*) { return 0; }

void TBiancoWatermill::turnByEnemy(THitActor*, const TBGCheckData*) { }

void TMapObjRootPakkun::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = new TTrembleModelEffect;
	unk138->init(mMActor->getModel());
	unk138->tremble(100.0f, 1.0f, 1.0f, 12000);
}

void TMapObjRootPakkun::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (fabsf(gpMarioPos->z - mPosition.z) < 10000.0f) {
		unk138->movement();
		if (!isStateTimerEngaged()) {
			unk138->tremble(mTremblePower, mTrembleAccel, mTrembleBrake,
			                mTrembleTime);
			mStateTimer = mTrembleTime;
		}
	}
}

void TBigWindmill::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	for (int i = 0; i < 4; ++i) {
		unk138[i] = TMapObjBaseManager::newAndRegisterObj("bigWindmillBlock");
		unk138[i]->appear();
		unk138[i]->getModel()->calc();
	}
}

void TBigWindmill::control()
{
	TMapObjBase::control();

	mRotation.z -= sSpeed;
	mRotation.z = MsWrap(mRotation.z, 0.0f, 360.0f);
	setRootMtxRotZ();

	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_OBJ_BI_WINDMILLTOWER, &mPosition, nullptr, fabsf(sSpeed), 0, 0,
	    &unk148, 0, 4);

	f32 angle = mRotation.z + sAngleAdd;
	for (int i = 0; i < 4; ++i) {
		MtxPtr mtx = unk138[i]->getModel()->getAnmMtx(0);
		f32 rad    = 0.017453294f * angle;
		mtx[0][3]  = sRadius * cosf(rad) + mPosition.x;
		mtx[1][3]  = sRadius * sinf(rad) + mPosition.y - mYOffset;
		angle += 90.0f;
		mtx[2][3] = mPosition.z - sSubZ;
		unk138[i]->mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		if (angle > 360.0f)
			angle -= 360.0f;
	}
}
