// rogue includes needed for matching sinit & bss; they have to come first so
// that their statics and literals precede the ones of the TU's own headers
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

#include <MoveBG/MapObjRicco.hpp>
#include <MoveBG/MapObjBall.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapCollisionManager.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#ifdef TARGET_PC
#include <string.h> // the MSL header is only here to match sinit/bss order and clashes with the CRT
#else
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#endif
#include <math.h>

u32 TCraneRotY::mWaitTime = 120;

f32 TCraneUpDown::mRotSpeed = 0.1f;
u32 TCraneUpDown::mWaitTime = 120;

f32 TRiccoWatermill::mRotAccel              = 1.0f;
f32 TRiccoWatermill::mRotSpeedMaxUp         = 3.0f;
f32 TRiccoWatermill::mRotSpeedMaxDown       = 1.0f;
f32 TRiccoWatermill::mRotDown               = 0.05f;
f32 TRiccoWatermill::mSubmarineMoveRate     = 0.5f;
f32 TRiccoWatermill::mSubmarineMaxTransY    = 750.0f;
f32 TRiccoWatermill::mSubmarineBottomTransY = -950.0f;
u32 TRiccoWatermill::mWaitTime              = 600;
f32 TRiccoWatermill::mSubmarineSurfaceTransY;

f32 TFruitLauncher::mObjSpeedXZ    = 1.0f;
f32 TFruitLauncher::mObjSpeedY     = 20.0f;
u32 TFruitLauncher::mFruitLiveTime = 4800;

static JGeometry::TVec3<f32> submarineCranePos_forSound(1956.0f, 1000.0f,
                                                        6425.0f);
static JGeometry::TVec3<f32> submarineSetWtPos_forSound(1956.0f, -100.0f,
                                                        6425.0f);

// NOTE: the functions below are in the same order as the class declarations;
// MWCC (-inline deferred) emits them in reverse order.

void TCraneRotY::calc() { setRootMtxRotY(); }

void TCraneRotY::control()
{
	char trash[0x18];
	TMapObjBase::control();

	switch (mState) {
	case 0:
		mRotation.y += unk144;
		if (mRotation.y > unk138 + unk140) {
			mStateTimer = mWaitTime;
			mState      = 3;
		}
		break;
	case 1:
		if (!isStateTimerEngaged())
			mState = 0;
		break;
	case 2:
		mRotation.y -= unk144;
		if (mRotation.y < unk138 + unk13C) {
			mStateTimer = mWaitTime;
			mState      = 1;
		}
		break;
	case 3:
		if (!isStateTimerEngaged())
			mState = 2;
		break;
	}

	if (isState(0) || isState(2)) {
		u32 sound = unk148;
		SMSGetMSound()->startSoundActor(sound, &mPosition, 0, nullptr, 0, 4);
	}
}

void TCraneRotY::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	stream >> unk140;
	unk138 = mRotation.y;
	unk144 = 0.05f + 0.1f * MsRandF();
	if (strcmp(getName(), "crane90 0") == 0)
		unk148 = MSD_SE_OBJ_CRANE_SIDEMOVE1;
	else
		unk148 = MSD_SE_OBJ_CRANE_SIDEMOVE2;
	mState = 0;
}

void TCraneUpDown::control()
{
	TMapObjBase::control();

	switch (mState) {
	case 1:
		if (!isStateTimerEngaged())
			mState = 0;
		break;
	case 0:
		mRotation.x += mRotSpeed;
		if (mRotation.x > unk140) {
			mStateTimer = mWaitTime;
			mState      = 3;
		}
		break;
	case 3:
		if (!isStateTimerEngaged())
			mState = 2;
		break;
	case 2:
		mRotation.x -= mRotSpeed;
		if (mRotation.x < unk144) {
			mStateTimer = mWaitTime;
			mState      = 1;
		}
		break;
	}

	unk138->mPosition.set(0.0f, 0.0f, 1500.0f);

	MtxPtr anmMtx = getModel()->getAnmMtx(0);
	Mtx rotY;
	char trash[0x28];
	MTXIdentity(rotY);
	MsMtxSetRotX(anmMtx, mRotation.x);
	MsMtxSetRotY(rotY, mRotation.y);
	MTXConcat(rotY, anmMtx, anmMtx);
	MTXMultVec(anmMtx, &unk138->mPosition, &unk138->mPosition);

	unk138->mPosition.x += mPosition.x;
	unk138->mPosition.y += mPosition.y - mYOffset + unk138->mYOffset;
	unk138->mPosition.z += mPosition.z;

	if (isState(2) || isState(0)) {
		u32 sound = unk13C;
		SMSGetMSound()->startSoundActor(sound, &mPosition, 0, nullptr, 0, 4);
	}
}

void TCraneUpDown::initMapObj()
{
	TMapObjBase::initMapObj();
	mMapCollisionManager->getUnk8()->setAllActor(nullptr);

	unk138 = TMapObjBaseManager::newAndRegisterObj("craneCargoUpDown");
	unk138->appear();

	if (strcmp(getName(), "craneUpDown 0") == 0) {
		unk144 = -25.0f;
		unk140 = 45.0f;
		unk13C = MSD_SE_OBJ_CRANE_UPDOWN1;
	} else {
		unk144 = -25.0f;
		unk140 = 30.0f;
		unk13C = MSD_SE_OBJ_CRANE_UPDOWN2;
	}
	mRotation.x = unk144 + (unk140 - unk144) * MsRandF();
}

void TCraneCargo::control()
{
	unk158.z = 0.0f;
	unk158.y = 0.0f;
	unk158.x = 0.0f;
	TMapObjBase::control();
}

void TCraneCargo::calc()
{
	updateRootMtxTrans();
	calcLeanMtx(getModel()->getAnmMtx(1));
}

u32 TRiccoWatermill::touchWater(THitActor*)
{
	if (isState(5))
		return 1;

	unk140 = 5;
	if (isState(1))
		unk13C->setUpMapCollision(1);

	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	unk13C->offMapObjFlag(MAP_OBJ_FLAG_UNK100);

	if (unk13C->mPosition.y < mSubmarineMaxTransY) {
		unk138 += mRotAccel;
		if (unk138 > mRotSpeedMaxUp)
			unk138 = mRotSpeedMaxUp;
		mState = 2;
	} else {
		unk138 = 0.0f;
	}
	return 1;
}

void TRiccoWatermill::control()
{
	TMapObjBase::control();

	if (isState(2) || isState(3) || isState(4)) {
		if (unk138 != 0.0f) {
			mRotation.z -= unk138;
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_SUBMARINE_MILL, &mPosition, nullptr, fabsf(unk138), 0, 0,
			    &unk14C, 0, 4);

			f32 moveY = unk138 * mSubmarineMoveRate;
			unk13C->mPosition.y += moveY;
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_SUBMARINE_CRANE, &submarineCranePos_forSound, nullptr,
			    fabsf(moveY), 0, 0, &unk150, 0, 4);
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_OBJ_SUBMARINE_SINK, &submarineCranePos_forSound, nullptr,
			    fabsf(moveY), 0, 0, &unk154, 0, 4);
		}

		if (unk140 == 0) {
			unk138 -= mRotDown;
			if (unk138 < -mRotSpeedMaxDown)
				unk138 = -mRotSpeedMaxDown;
		} else {
			unk140--;
		}
	}

	switch (mState) {
	case 1:
		break;
	case 2:
		if (unk13C->mPosition.y > mSubmarineMaxTransY) {
			unk13C->mPosition.y = mSubmarineMaxTransY;
			if (!isStateTimerEngaged()) {
				SMSGetMSound()->startSoundActor(MSD_SE_OBJ_SUBMARINE_SET,
				                                &unk13C->mPosition, 0,
				                                nullptr, 0, 4);
				if (!unk144) {
					JGeometry::TVec3<f32> point(
					    2008.0f, 500.0f + mSubmarineMaxTransY, 7066.0f);
					char trash[0x28];
					throwObjToFrontFromPoint(unk148, point, 20.0f, 20.0f);
				}
				unk144 = true;
			}
		}
		if (unk138 < 0.0f) {
			if (unk144)
				mState = 3;
			else
				mState = 4;
		}
		break;
	case 3:
		if (unk13C->mPosition.y <= mSubmarineSurfaceTransY) {
			unk13C->mPosition.y = mSubmarineSurfaceTransY;
			unk13C->setUpMapCollision(0);
			unk138 = 0.0f;
			SMSGetMSound()->startSoundActor(MSD_SE_OBJ_SUBMARINE_SET,
			                                &unk13C->mPosition, 0, nullptr, 0,
			                                4);
			mStateTimer = mWaitTime;
			mState      = 5;
		}
		break;
	case 4:
		if (unk13C->mPosition.y <= mSubmarineBottomTransY) {
			unk13C->mPosition.y = mSubmarineBottomTransY;
			unk13C->setUpMapCollision(0);
			unk138 = 0.0f;
			unk13C->onMapObjFlag(MAP_OBJ_FLAG_UNK100);
			onMapObjFlag(MAP_OBJ_FLAG_UNK100);
			SMSGetMSound()->startSoundActor(SMD_SE_OBJ_SUBMARINE_SET_WT,
			                                &unk13C->mPosition, 0, nullptr, 0,
			                                4);
			mState = 1;
		}
		break;
	case 5:
		break;
	}
}

void TRiccoWatermill::calc() { setRootMtxRotZ(); }

void TRiccoWatermill::loadAfter()
{
	TMapObjBase::loadAfter();

	unk13C = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("submarine"));
	unk148 = static_cast<TMapObjBase*>(
	    JDrama::TNameRefGen::search("青コイン（潜水艦用）"));
	unk148->makeObjDead();
	unk13C->mPosition.y = mSubmarineBottomTransY;
	unk13C->removeMapCollision();
	unk13C->setUpCurrentMapCollision();
}

TRiccoWatermill::TRiccoWatermill(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(nullptr)
    , unk140(0)
    , unk144(false)
    , unk148(nullptr)
    , unk14C(nullptr)
    , unk150(nullptr)
    , unk154(nullptr)
{
	char trash[0x10];
}

void TSurfGesoObj::initMapObj()
{
	char trash[8];
	TMapObjBase::initMapObj();

	if (strcmp(unkF4, "SurfGesoRed") == 0) {
		unk154.r = 255;
		unk154.g = 180;
		unk154.b = 255;
		unk154.a = 255;
	} else if (strcmp(unkF4, "SurfGesoYellow") == 0) {
		unk154.r = 255;
		unk154.g = 255;
		unk154.b = 125;
		unk154.a = 255;
	} else if (strcmp(unkF4, "SurfGesoGreen") == 0) {
		unk154.r = 180;
		unk154.g = 255;
		unk154.b = 180;
		unk154.a = 255;
	}

	SDLModelData* modelData = gpMapObjManager->mSurfGessoModelData;
	mMActor = SMS_MakeMActorFromSDLModelData(
	    modelData, gpMapObjManager->getMActorAnmData(), 3);
	TMapObjBase::initPacketMatColor(getModel(), GX_TEVREG1, &unk154);
	mMActor->setBck("surfgeso_run1");
}

inline void TFruitSwitch::pullUp()
{
	getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	getModel()->calc();
	MtxPtr mtx             = getModel()->getAnmMtx(0);
	TMapCollisionBase* col = mMapCollisionManager->getUnk8();
	MTXCopy(mtx, col->unk20);
	col->setUp();
}

inline void TFruitSwitch::pushDown()
{
	startBck("riccoswitch");
	onHitFlag(HIT_FLAG_NO_COLLISION);
	if (mMapCollisionManager->unk8)
		mMapCollisionManager->unk8->remove();
}

BOOL TFruitSwitch::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_HIP_DROP) {
		pushDown();
		unk138->fireObj();
		return 1;
	}
	return 0;
}

inline TMapObjBase* TFruitLauncher::appearFruit() const
{
	f32 rnd = 100.0f * MsRandF();
	if (rnd < 20.0f)
		return gpItemManager->makeObjAppear(mPosition.x, mPosition.y,
		                                    mPosition.z, 0x40000390, false);
	if (rnd < 40.0f)
		return gpItemManager->makeObjAppear(mPosition.x, mPosition.y,
		                                    mPosition.z, 0x40000391, false);
	if (rnd < 60.0f)
		return gpItemManager->makeObjAppear(mPosition.x, mPosition.y,
		                                    mPosition.z, 0x40000392, false);
	if (rnd < 80.0f)
		return gpItemManager->makeObjAppear(mPosition.x, mPosition.y,
		                                    mPosition.z, 0x40000393, false);
	return gpItemManager->makeObjAppear(mPosition.x, mPosition.y, mPosition.z,
	                                    0x40000394, false);
}

void TFruitLauncher::fireObj()
{
	char trash[0x10];
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_JUMP_ED_B,
	                                            &mPosition, 0, nullptr);
	SMSGetMSound()->startSoundActor(MSD_SE_OBJ_AP_BUTTON, &mPosition, 0,
	                                nullptr, 0, 4);
	SMSGetMSound()->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0, nullptr,
	                                0, 4);

	if (unk140 == 0)
		unk140 = 1;
	else
		unk140 = 0;
	unk138[unk140]->pullUp();

	TMapObjBase* fruit = appearFruit();
	if (fruit == nullptr)
		fruit = appearFruit();
	if (fruit == nullptr)
		fruit = appearFruit();

	if (fruit != nullptr) {
		fruit->mPosition.x = mPosition.x;
		fruit->mPosition.y = mPosition.y;
		fruit->mPosition.z = mPosition.z;
		fruit->setVelocityAndFlag10(mObjSpeedXZ * (MsRandF() - 0.5f),
		                            -mObjSpeedY,
		                            mObjSpeedXZ * (MsRandF() - 0.5f));
		gpMarDirector->fireStartDemoCamera(
		    "フルーツタンクカメラカメラ", &fruit->mPosition, -1, 0.0f, true,
		    nullptr, 0, nullptr, JDrama::TFlagT<u16>(0));
		SMSGetMSound()->startSoundSystemSE(MSD_SE_SY_COLLECT_DELIGHT, 0,
		                                   nullptr, 0);
		if (TMapObjBase::isFruit(fruit))
			static_cast<TResetFruit*>(fruit)->killByTimer(mFruitLiveTime);
	}
}

void TFruitLauncher::loadAfter()
{
	TMapObjBase::loadAfter();

	TResetFruit* fruit;
	fruit = static_cast<TResetFruit*>(TMapObjBaseManager::newAndRegisterObj("FruitCoconut"));
	fruit->unk1A4 = 1;
	fruit = static_cast<TResetFruit*>(TMapObjBaseManager::newAndRegisterObj("FruitDurian"));
	fruit->unk1A4 = 1;
	fruit = static_cast<TResetFruit*>(TMapObjBaseManager::newAndRegisterObj("FruitPapaya"));
	fruit->unk1A4 = 1;
	fruit = static_cast<TResetFruit*>(TMapObjBaseManager::newAndRegisterObj("FruitPine"));
	fruit->unk1A4 = 1;
	fruit = static_cast<TResetFruit*>(TMapObjBaseManager::newAndRegisterObj("FruitBanana"));
	fruit->unk1A4 = 1;

	unk138[0] = static_cast<TFruitSwitch*>(
	    JDrama::TNameRefGen::search("タンクスイッチＡ"));
	unk138[0]->unk138 = this;
	unk138[1] = static_cast<TFruitSwitch*>(
	    JDrama::TNameRefGen::search("タンクスイッチＢ"));
	unk138[1]->unk138 = this;
	unk140            = 1;
	unk138[0]->pushDown();
	char trash[0x8];
}
