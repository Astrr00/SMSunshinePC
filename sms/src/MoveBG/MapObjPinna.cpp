#include <MoveBG/MapObjPinna.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/Item.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <System/FlagManager.hpp>
#include <Player/Mario.hpp>
#include <Player/Yoshi.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <math.h>
#include <stdlib.h>
#ifdef TARGET_PC
#include <string.h> // the MSL header is only here to match sinit/bss order and clashes with the CRT
#else
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

f32 TShellCup::mOpenRotMax     = 90.0f;
f32 TShellCup::mShellDamageRot = 45.0f;
f32 TShellCup::mWaterOpenAccel = 5.0f;
f32 TShellCup::mCloseAccel     = 3.5f;
f32 TMerrygoround::mRotSpeed   = 0.1f;

static s32 switchSnd;

#ifdef TARGET_PC
s32 TFerrisWheel::becomeCalmlyCallback(uintptr_t arg, u32 state)
#else
s32 TFerrisWheel::becomeCalmlyCallback(u32 arg, u32 state)
#endif
{
	char trash[8];
	if (state == 0) {
		TFerrisWheel* wheel = reinterpret_cast<TFerrisWheel*>(arg);
		wheel->mState       = 2;
		MSound* sound       = gpMSound;
		if (sound->unk80) {
			sound->unk80->setVolume(0.0f, 200, 0);
			sound->unk80->setPitch(0.5f, 200, 0);
		}
		wheel->startStateTimer(120);
	}
	return 0;
}

void TFerrisWheel::control()
{
	char trash[0x18];
	TMapObjBase::control();
	if (isState(2) && !isStateTimerEngaged()) {
		if (unk140 > 0.25f * SMSGetAnmFrameRate())
			unk140 -= 0.015f;
		else
			mState = 1;
	}

	if (unk140 > 0.25f * SMSGetAnmFrameRate()) {
		MSound* sound = gpMSound;
		sound->startSoundActor(MSD_SE_OBJ_MAHRE_GATE_LIGHT, &mPosition, 0,
		                       &sound->unk80, 0, 4);
	}

	f32 speed = unk140;
	mMActor->getFrameCtrl(0)->setFrame(
	    speed + mMActor->getFrameCtrl(0)->getFrame());

	for (int i = 0; i < unk138; ++i) {
		TMapObjBase* gondola = unk13C[i];
		MtxPtr mtx           = getModel()->getAnmMtx(i + 1);
		MTXCopy(mtx, gondola->getModel()->getAnmMtx(0));
		gondola->mPosition.set(mtx[0][3], mtx[1][3] + gondola->mYOffset,
		                       mtx[2][3]);
	}
}

void TFerrisWheel::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = getModel()->getModelData()->getJointNum() - 1;
	unk13C = new TMapObjBase*[unk138];
	for (u16 i = 0; i < unk138; ++i) {
		unk13C[i] = TMapObjBaseManager::newAndRegisterObj("FerrisGondola");
		unk13C[i]->appear();
	}
	if (gpMarDirector->unk7D == 2)
		unk140 = 10.0f;
	else
		unk140 = SMSGetAnmFrameRate() * 0.25f;
	char trash[0x10];
}

TFerrisWheel::TFerrisWheel(const char* name)
    : TMapObjBase(name)
    , unk138(0)
    , unk13C(nullptr)
    , unk140(0.0f)
{
}

void THorizontalViking::control()
{
	char trash[0x8];
	TMapObjBase::control();
	switch (mState) {
	case 1:
		unk144 -= unk13C;
		unk148 += unk144;
		if (unk148 < 0.0f)
			mState = 2;
		break;
	case 2:
		unk144 += unk13C;
		unk148 += unk144;
		if (unk148 > 0.0f)
			mState = 1;
		break;
	}

	mPosition.x = unk138 * sinf(3.14f * (unk148 / 180.0f)) + mInitialPosition.x;
	f32 yOffset = mYOffset;
	mPosition.y = yOffset
	              + (unk138 * (1.0f - cosf(3.14f * (unk148 / 180.0f)))
	                 + mInitialPosition.y);
}

void THorizontalViking::reset()
{
	unk144 = unk140;
	unk148 = 0.0f;
	if (unk144 > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void THorizontalViking::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 2500.0f;
	unk13C = 0.0008f;
	unk140 = 0.23f;
	reset();
}

void TViking::roll()
{
	char trash[0x30];
	switch (mState) {
	case 1:
		unk144 *= unk154;
		unk144 -= unk13C;
		unk148 += unk144;
		if (unk148 < 0.0f) {
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			mState = 2;
		}
		if (unk148 > 180.0f) {
			unk148 -= 360.0f;
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			mState = 4;
		}
		break;
	case 2:
		unk144 *= unk154;
		unk144 += unk13C;
		unk148 += unk144;
		if (unk148 > 0.0f) {
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			mState = 1;
		}
		if (unk148 < -180.0f) {
			unk148 += 360.0f;
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			mState = 3;
		}
		break;
	case 3:
		unk144 *= unk158;
		unk144 -= unk13C;
		unk148 += unk144;
		if (unk148 < 0.0f) {
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			if (unk144 > -unk150)
				mState = 2;
			else
				mState = 4;
		}
		break;
	case 4:
		unk144 *= unk158;
		unk144 += unk13C;
		unk148 += unk144;
		if (unk148 > 0.0f) {
			gpMSound->startSoundActorWithInfo(
			    MSD_SE_OBJ_PIN_BIKING_WING, &mPosition, nullptr, fabsf(unk144),
			    0, 0, nullptr, 0, 4);
			if (unk144 < unk150)
				mState = 1;
			else
				mState = 3;
		}
		break;
	}
}

void TViking::control()
{
	char trash[0x8];
	switch (unk14C) {
	case 0:
		switch (mState) {
		case 1:
			unk144 -= unk13C;
			unk148 += unk144;
			if (unk148 < 0.0f)
				mState = 2;
			break;
		case 2:
			unk144 += unk13C;
			unk148 += unk144;
			if (unk148 > 0.0f)
				mState = 1;
			break;
		}
		break;
	case 1:
		roll();
		break;
	}

	mPosition.x = unk138 * sinf(3.14f * (unk148 / 180.0f)) + mInitialPosition.x;
	f32 yOffset = mYOffset;
	mPosition.y = yOffset
	              + (unk138 * (1.0f - cosf(3.14f * (unk148 / 180.0f)))
	                 + mInitialPosition.y);
	mRotation.z = unk148;
	updateObjMtx();
}

void TViking::reset()
{
	unk144 = unk140;
	unk148 = 0.0f;
	if (unk140 > 0.0f)
		mState = 1;
	else
		mState = 2;
}

void TViking::loadAfter()
{
	TMapObjBase::loadAfter();
	reset();
}

void TViking::initMapObj()
{
	unk14C = 1;
	if (strcmp(mName, "viking 0") == 0) {
		unk138 = 1400.0f;
		unk13C = 0.001f;
		unk154 = 1.001f;
		unk158 = 0.999f;
		unk140 = -0.3f;
		unk150 = 0.3f;
	} else {
		unk138 = 1400.0f;
		unk13C = 0.001f;
		unk154 = 1.001f;
		unk158 = 0.999f;
		unk140 = 0.3f;
		unk150 = 0.3f;
	}
	mPosition.y -= unk138;
	TMapObjBase::initMapObj();
}

TViking::TViking(const char* name)
    : THorizontalViking(name)
    , unk14C(0)
    , unk150(0.0f)
    , unk154(0.0f)
    , unk158(0.0f)
{
}

BOOL TPinnaShell::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0, 0.0f,
		                        0, 0, 4);
		if (unk68 == 0) {
			unk6C -= TShellCup::mWaterOpenAccel;
			if (unk6C < -TShellCup::mOpenRotMax)
				unk68 = 1;
		}
		return true;
	}
	return false;
}

void TPinnaShell::control()
{
	char trash[0x8];
	if (unk7C > 0)
		--unk7C;

	switch (unk68) {
	case 0:
		if (unk6C < 0.0f)
			unk6C += TShellCup::mCloseAccel * (0.5f + 0.5f * MsRandF());
		else
			unk6C = 0.0f;
		break;
	case 1:
		unk6C -= 0.8f;
		if (unk6C < -TShellCup::mOpenRotMax) {
			unk6C = -TShellCup::mOpenRotMax;
			unk7C = 360;
			if (unk80 && !unk80->checkLiveFlag(LIVE_FLAG_DEAD)) {
				if (unk80->isActorType(0x20000010))
					gpMSound->startSoundSystemSE(MSD_SE_SY_COLLECT_PRETTY, 0,
					                             nullptr, 0);
				else
					gpMSound->startSoundSystemSE(MSD_SE_SY_COIN_APPEAR, 0,
					                             nullptr, 0);
			} else {
				gpMSound->startSoundSystemSE(MSD_SE_SY_NOT_COLLECT, 0, nullptr,
				                             0);
			}
			unk68 = 2;
		}
		break;
	case 2:
		if (unk7C <= 0) {
			unk68 = 3;
			gpMSound->startSoundActor(MSD_SE_OBJ_PIN_SHELL_CLOSE, &mPosition, 0,
			                          nullptr, 0, 4);
		}
		break;
	case 3:
		unk6C += unk70;
		if (unk6C >= -TShellCup::mShellDamageRot)
			unk88->offHitFlag(HIT_FLAG_NO_COLLISION);
		if (unk6C >= 0.0f) {
			unk6C = 0.0f;
			unk68 = 0;
			unk88->onHitFlag(HIT_FLAG_NO_COLLISION);
		}
		break;
	}

	mPosition.set(unk8C->mPosition.x
	                  + 0.7f * (unk74[0][3] - unk8C->mPosition.x),
	              unk74[1][3] - 100.0f,
	              unk8C->mPosition.z
	                  + 0.7f * (unk74[2][3] - unk8C->mPosition.z));
	unk88->mPosition.set(mPosition);

	if (mColCount) {
		Mtx rotMtx;
		MsMtxSetRotX(rotMtx, unk6C);
		TMapObjBase::concatOnlyRotFromRight(unk74, rotMtx, rotMtx);
		unk84->moveMtx(rotMtx);
	}
	char trash2[0x38];
}

void TShellCup::control()
{
	mMActor->calc();
	for (int i = 0; i < 6; ++i)
		unk138[i].control();
}

#pragma dont_inline on
static void setShellRotX(MtxPtr mtx, f32 angle)
{
	MsMtxSetRotX(mtx, angle);
}
#pragma dont_inline off

void TShellCup::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
	if (cue & 2) {
		if (!gpMarDirector->isTalkModeNow() || gpMarDirector->isDemoModeNow()) {
			for (int i = 0; i < 6; ++i) {
				Mtx rotMtx;
				setShellRotX(rotMtx, unk138[i].unk6C);
				concatOnlyRotFromRight(unk138[i].unk74, rotMtx,
				                       unk138[i].unk74);
			}
			if (!unk498->checkLiveFlag(LIVE_FLAG_DEAD))
				unk498->mPosition.set(unk138[0].mPosition);
			if (!unk49C->checkLiveFlag(LIVE_FLAG_DEAD))
				unk49C->mPosition.set(unk138[2].mPosition);
			if (!unk4A0->checkLiveFlag(LIVE_FLAG_DEAD))
				unk4A0->mPosition.set(unk138[4].mPosition);
		}
	}
}

void TShellCup::loadAfter()
{
	TMapObjBase::loadAfter();
	for (int i = 0; i < 6; ++i)
		joinToGroup("オブジェクトグループ", unk138[i].unk88);

	unk498 = TMapObjBaseManager::newAndRegisterObj("coin_blue");
	unk49C = gpItemManager->newAndRegisterCoinReal();
	unk4A0 = gpItemManager->newAndRegisterCoinReal();
	unk498->setEventId(2);
	if (!TFlagManager::smInstance->getBlueCoinFlag(
	        gpMarDirector->getCurrentMap(), unk498->getEventId())) {
		unk498->makeObjAppeared();
		unk138[0].unk80 = unk498;
	}
	unk49C->makeObjAppeared();
	unk49C->onMapObjFlag(MAP_OBJ_FLAG_UNK10000000);
	unk4A0->makeObjAppeared();
	unk4A0->onMapObjFlag(MAP_OBJ_FLAG_UNK10000000);
	unk138[2].unk80 = unk49C;
	unk138[4].unk80 = unk4A0;
	char trash[0x8];
}

void TShellCup::initMapObj()
{
	TMapObjBase::initMapObj();
	for (int i = 0; i < 6; ++i) {
		unk138[i].unk68 = 0;
		unk138[i].unk6C = 0.0f;
		unk138[i].unk70 = 8.0f;
		unk138[i].unk74 = getModel()->getAnmMtx(i + 1);
		unk138[i].unk78
		    = getModel()->getModelData()->getJointNodePointer(i + 1);
		unk138[i].unk7C = (int)(MsRandF() * 1200.0f);
		unk138[i].unk8C = this;
		joinToGroup("オブジェクトグループ", &unk138[i]);

		unk138[i].unk84 = new TMapCollisionMove;
		unk138[i].unk84->init("/mapObj/ShellCup", 0, this);
		unk138[i].unk84->setUp();

		unk138[i].unk88 = new TDamageObj;
		unk138[i].unk88->mScaling.set(2.0f, 1.2f, 2.0f);
		unk138[i].unk88->init(0x10000036);
		unk138[i].unk88->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	TMapCollisionStatic* collision = new TMapCollisionStatic;
	collision->init("/mapObj/ShellCup_rink", 2, this);
	MTXCopy(getModel()->getAnmMtx(0), collision->unk20);
	collision->setUp();
	char trash[0x10];
}

TShellCup::TShellCup(const char* name)
    : TMapObjBase(name)
    , unk498(nullptr)
    , unk49C(nullptr)
    , unk4A0(nullptr)
{
}

void TMerrygoround::control()
{
	TMapObjBase::control();
	mRotation.y += mRotSpeed;
	if (mRotation.y > 360.0f)
		mRotation.y -= 360.0f;

	for (int i = 0; i < 2; ++i) {
		MtxPtr mtx = getModel()->getAnmMtx(unk140[i]);
		unk138[i]->setModelMtx(mtx);
		unk138[i]->mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}

	for (int i = 0; i < 9; ++i) {
		MtxPtr mtx = getModel()->getAnmMtx(unk18C[i]);
		unk144[i]->unk138.set(mtx);
		unk144[i]->mPosition.set(mtx[0][3], mtx[1][3] - 600.0f, mtx[2][3]);
	}

	if (SMS_IsMarioOnYoshi())
		unk1A0->offHitFlag(HIT_FLAG_NO_COLLISION);
	else
		unk1A0->onHitFlag(HIT_FLAG_NO_COLLISION);

	MtxPtr mtx = getModel()->getAnmMtx(unk1A4);
	unk1A0->mPosition.set(mtx[0][3], mtx[1][3] - 600.0f, mtx[2][3]);
}

void TMerrygoround::draw() const { }

void TMerrygoround::initMapObj()
{
	TMapObjBase::initMapObj();

	int eggNum  = 0;
	int poleNum = 0;
	for (u16 i = 1; i < getModel()->getModelData()->getJointNum(); ++i) {
		const char* name = getModel()->getModelData()->getJointName()->getName(i);
		if (strstr(name, "egg")) {
			unk140[eggNum] = i;
			++eggNum;
		} else if (strcmp(name, "yoshi_warp") == 0) {
			unk1A4 = i;
		} else if (strcmp(name, "up") != 0 && strcmp(name, "down") != 0
		           && strcmp(name, "KAGE_2") != 0) {
			unk18C[poleNum] = i;
			++poleNum;
		}
	}

	for (int i = 0; i < 2; ++i) {
		unk138[i] = TMapObjBaseManager::newAndRegisterObj("merry_egg");
		unk138[i]->appear();
	}

	for (int i = 0; i < 9; ++i) {
		unk144[i] = static_cast<TMerryPole*>(
		    TMapObjBaseManager::newAndRegisterObj("merry_pole"));
		unk144[i]->appear();
		unk168[i] = new TMapCollisionMove;
		unk168[i]->init("/scene/mapObj/merry_yoshi.col", 0, this);
		unk168[i]->setUp();
	}

	unk1A0 = static_cast<TChangeStageMerrygoround*>(
	    TMapObjBaseManager::newAndRegisterObj("ChangeStageMerrygoround"));
	unk1A0->unk138 = 0x29;
	unk1A0->mScaling.y *= 1.5f;
	unk1A0->makeObjAppeared();
	joinToGroup("マップグループ", unk1A0);
	char trash[0x8];
}

TMerrygoround::TMerrygoround(const char* name)
    : TMapObjBase(name)
    , unk1A0(nullptr)
    , unk1A4(0)
{
	for (int i = 0; i < 2; ++i) {
		unk138[i] = nullptr;
		unk140[i] = 0;
	}
	for (int i = 0; i < 9; ++i) {
		unk144[i] = nullptr;
		unk18C[i] = 0;
		unk168[i] = nullptr;
	}
}

void TChangeStageMerrygoround::touchPlayer(THitActor* actor)
{
	char trash[0x18];
	if (!isStateTimerEngaged()) {
		if (SMS_GetYoshi()->mType == TYoshi::ORANGE) {
			gpMSound->startSoundSystemSE(MSD_SE_SY_COLLECT_YOSHI, 0, nullptr,
			                             0);
			TMapObjChangeStage::touchPlayer(actor);
			unk13C = 1;
		} else {
			gpMSound->startSoundSystemSE(MSD_SE_SY_NOT_COLLECT_YOSHI, 0,
			                             nullptr, 0);
		}
		startStateTimer(600);
	}
}

void TChangeStageMerrygoround::calc()
{
	if (unk13C) {
		gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_HIKAGE1_A, &SMS_GetMarioPos(), 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_HIKAGE1_B, &SMS_GetMarioPos(), 1, this);
	}
}

void TBalloonKoopaJr::touchActor(THitActor*) { kill(); }

void TBalloonKoopaJr::kill()
{
	char trash[0x8];
	TMapObjGeneral::kill();
	emitAndScale(0x5A, 0, &unk148);
	emitAndScale(0x5B, 0, &unk148);
	emitAndScale(0x5C, 0, &unk148);
	TFlagManager::smInstance->incFlag(0x60001, 1);
	gpMSound->startSoundActor(MSD_SE_BS_BSPAKU_SLAP, &mPosition, 0, nullptr, 0,
	                          4);
}

void TBalloonKoopaJr::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJr.jpa", 0x5A);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrA.jpa", 0x5B);
	SMS_LoadParticle("/scene/mapObj/balloonKoopaJrB.jpa", 0x5C);
	s32 index = getModel()->getModelData()->getJointName()->getIndex("center");
	MtxPtr mtx = getModel()->getAnmMtx((u16)index);
	unk148.set(mtx[0][3], mtx[1][3], mtx[2][3]);
}

void TPinnaEntrance::loadAfter()
{
	TMapObjBase::loadAfter();
	JGeometry::TVec3<f32> rot(90.0f, 0.0f, 0.0f);
	TMapObjBaseManager::newAndRegisterObj(
	    "GateManta", mPosition, rot, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
}

void TWaterRecoverObj::touchPlayer(THitActor* other)
{
	if (other->isActorType(0x80000001) && !isStateTimerEngaged()) {
		other->receiveMessage(this, HIT_MESSAGE_ATTACK);
		startStateTimer(600);
	}
}

void TAmiKing::loadAfter()
{
	TMapObjBase::loadAfter();
	SMS_LoadParticle("/scene/Mapobj/amiking.jpa", 0x184);
}

void TAmiKing::initMapObj()
{
	TMapObjBase::initMapObj();
	initAnmSound();
	mMActor->setBck("amiking_sleep1");
	setAnmSound("/scene/mapObj/amiking_sleep1.bas");
	offLiveFlag(LIVE_FLAG_UNK10);
	for (u8 i = 0; i < getMActor()->getModel()->getModelData()->getJointNum();
	     ++i) {
	}
}

void TAmiKing::moveObject()
{
	char trash[0x70];
	TLiveActor::moveObject();
	if (unk138) {
		if (mMActor->checkCurAnm("amiking_flying1_start", 0)) {
			if (mMActor->curAnmEndsNext(0, nullptr))
				mMActor->setBck("amiking_flying1_loop");
		} else if (mGroundPlane->isWaterSurface() && !isAirborne()) {
			gpMSound->startSoundActor(MSD_SE_EN_AMIKING_DIVE, &mPosition, 0,
			                          nullptr, 0, 4);
			JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xCA, mMActor->getModel()->getAnmMtx(0), 0, nullptr);
			if (emitter) {
				emitter->mGlobalDynamicsScale.setAll(4.0f);
				emitter->mGlobalParticleScale.setAll(4.0f);
			}

			TSpineEnemy* effect = gpConductor->makeOneEnemyAppear(
			    mPosition, "エフェクト水柱マネージャー", 1);
			if (effect) {
				JGeometry::TVec3<f32> scale(4.0f, 4.0f, 4.0f);
				static_cast<TEffectColumWater*>(effect)->generate(mPosition,
				                                                  scale);
			}

			gpItemManager->makeShineAppearWithDemo(
			    "シャイン（観覧車シャイン用）", "観覧車シャインカメラ",
			    mPosition.x, mPosition.y, mPosition.z);

			TFerrisWheel* wheel
			    = static_cast<TFerrisWheel*>(JDrama::TNameRefGen::search("FerrisWheel"));
			SMSGetMarDirector()->fireStartDemoCamera(
			    "観覧車正常化カメラ", &wheel->mPosition, -1, 0.0f, true,
			    &TFerrisWheel::becomeCalmlyCallback, PTR_U32(wheel), nullptr,
			    JDrama::TFlagT<u16>(0));
			kill();
		}
	} else if (mGroundPlane->mActor) {
		TMapObjBase* ground = (TMapObjBase*)mGroundPlane->mActor;
		if (ground->mActorType == 0x4000006A
		    && (ground->isState(TMapObjGeneral::STATE_BREAKING)
		        || ground->isState(TMapObjGeneral::STATE_TOUCHING_WATER)
		        || ground->isState(TMapObjGeneral::STATE_TOUCHING_PLAYER)
		        || ground->isState(TMapObjGeneral::STATE_HOLDING))) {
			unk138 = 1;
			setVelocityAndFlag10(5.0f, 10.0f, -10.0f);
			mMActor->setBck("amiking_flying1_start");
			setAnmSound(nullptr);
			SMSGetMarDirector()->fireStartDemoCamera(
			    "観覧車ボス撃沈カメラ", &mPosition, -1, 0.0f, true, nullptr, 0,
			    nullptr, JDrama::TFlagT<u16>(0));
		}
	}
}

void TAmiKing::calcRootMatrix()
{
	TMapObjBase::calcRootMatrix();
	gpMarioParticleManager->emitAndBindToMtxPtr(0x184,
	                                            getModel()->getAnmMtx(0), 1,
	                                            this);
	if (!unk138) {
		MtxPtr jointMtx = mMActor->getModel()->getAnmMtx(6);
		unk13C.set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);

		JGeometry::TVec3<f32> offset(0.0f, 0.0f, 200.0f);
		Mtx rotMtx;
		MsMtxSetRotRPH(rotMtx, 0.0f, mRotation.y, 0.0f);
		MTXMultVec(rotMtx, offset, offset);
		unk13C.add(offset);

		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToPosPtr(0x124, &unk13C, 1,
		                                                  this);
		if (emitter) {
			emitter->mGlobalDynamicsScale.setAll(2.0f);
			emitter->mGlobalParticleScale.setAll(2.0f);
		}
		gpMSound->startSoundActor(MSD_SE_EN_AMIKING_SPARK, &mPosition, 0,
		                          nullptr, 0, 4);
	} else {
		gpMSound->startSoundActor(MSD_SE_EN_AMIKING_FLY, &mPosition, 0, nullptr,
		                          0, 4);
	}
	char trash[0x34];
}

void TAmiKing::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight, mPosition.z,
		                   &mGroundPlane);
	else
		TLiveActor::bind();
}

void TAmiKing::touchPlayer(THitActor*) { SMS_SendMessageToMario(this, 9); }

void TPinnaCoaster::control()
{
	TMapObjBase::control();
	unk138->frameUpdate();
	unk138->calc();
	MtxPtr railMtx = unk138->getModel()->getAnmMtx(0);
	MTXCopy(railMtx, getModel()->getBaseTRMtx());
	mMActor->frameUpdate();
	mMActor->calc();

	MtxPtr mtx = getModel()->getAnmMtx(0);
	mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);

	f32 speed = JGeometry::TVec3<f32>(mPosition - unk140).length();
	char trash[0x18];
	if (switchSnd)
		gpMSound->startSoundActorWithInfo(MSD_SE_OBJ_JET_COASTER, &mPosition,
		                                  nullptr, speed, 0, 0, nullptr, 0, 4);
	switchSnd ^= 1;
	unk140.set(mPosition);
}

void TPinnaCoaster::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = SMS_MakeMActorWithAnmData("/scene/mapObj/CoasterRail.bmd",
	                                   mManager->getMActorAnmData(), 3,
	                                   0x10210000);
	unk138->setBck("coasterrail");
	MsMtxSetXYZRPH(unk138->getModel()->getBaseTRMtx(), mPosition.x, mPosition.y,
	               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	unk138->getFrameCtrl(0)->setRate(SMSGetAnmFrameRate() * 0.25f);
	unk140.set(mPosition);
}

TPinnaCoaster::TPinnaCoaster(const char* name)
    : TMapObjBase(name)
    , unk138(nullptr)
{
	unk140.zero();
}
