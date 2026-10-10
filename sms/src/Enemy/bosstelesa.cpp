#include <Enemy/BossTelesa.hpp>
#include <Enemy/AreaCylinder.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/NameKuri.hpp>
#include <Enemy/Walker.hpp>
#include <Camera/cameralib.hpp>
#include <Camera/CameraShake.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JMath.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSModBgm.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <System/TargetArrow.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <stdlib.h>
#include <Enemy/Telesa.hpp>
#include <Enemy/HamuKuri.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/LightUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Strategic/SharedParts.hpp>
#include <MoveBG/Item.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>

// file-level data
f32 TBossTelesa::mEnemyGenRate         = 0.5f;
f32 TBossTelesa::mItemGenRate          = 0.1f;
u8 TBossTelesa::mNormalAlpha           = 150;
f32 TBossTelesa::mBaseHoseiPosY        = -300.0f;
f32 TBossTelesa::mRouletteUpRate       = 0.03f;
s32 TBossTelesa::mTelesaGenerateInterval = 400;
f32 TBossTelesa::mCameraMoveLimit      = 1000.0f;
f32 TBossTelesa::mCameraMoveSp         = 0.02f;

static const char* btelesa_bastable[] = {
	"/scene/btelesa/bas/btelesa_appear.bas",
	"/scene/btelesa/bas/btelesa_bero_hit.bas",
	"/scene/btelesa/bas/btelesa_damage.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_lick.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_roll.bas",
	"/scene/btelesa/bas/btelesa_spicy.bas",
	nullptr,
	nullptr,
	"/scene/btelesa/bas/btelesa_wait.bas",
	"/scene/btelesa/bas/btelesa_wet.bas",
};

// Inlined clamp (the MsClamp template is not inlined in this unit).
template <class T> static inline T clampBossTelesa(T t, T l, T r)
{
	if (t > r)
		t = r;
	else if (t < l)
		t = l;
	return t;
}

// Inline "start BCK" core shared by the nerves (no symbol in the original).
// The "already playing" check is a call-site condition.
static inline void setBossTelesaBck(TBossTelesa* self, int idx)
{
	self->mPrevBck       = self->mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	self->mCurBck        = idx;
	self->mBckBlendRatio = 1.0f;
	MActor* actor        = self->mMActor;
	actor->setBckOldMotionBlendAnmPtr(actor->getBckAnm());
	self->mMActor->setBckFromIndex(idx);
	self->mMActor->setMotionBlendRatioForBck(self->mBckBlendRatio);
	const char** table = self->getBasNameTable();
	self->setAnmSound(!table ? nullptr : table[idx]);
}

// True when the spat-out slot items are all gone (or there are none).
static inline bool isAllSlotItemDead(TBossTelesa* self)
{
	if (self->mSlotResult == -1)
		return true;
	for (int i = 0; i < self->mItemNum; ++i) {
		if (!self->mItems[i]->checkLiveFlag(LIVE_FLAG_DEAD))
			return false;
	}
	return true;
}

static inline bool isSlotStopped(TBossTelesa* self)
{
	if (!self->mSlot->isRollDrum() && self->unk18C != 0) {
		self->unk18C = 0;
		self->mSpine->pushAfterCurrent(&TNerveBossTelesaSpitSlotItem::theNerve());
		return true;
	}
	return false;
}

TBubbleManager::TBubbleManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TBubbleManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBubbleSaveLoadParams("/enemy/bubble.prm");
	TSmallEnemyManager::load(stream);
}

TSpineEnemy* TBubbleManager::createEnemyInstance() { return new TBubble; }

void TBubbleManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "btelesa_osenbubbles_ind.bmd", 0x11020000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBubble::init(TLiveManager* manager)
{
	char trash[8];
	TWalkerEnemy::init(manager);

	mActorType    = 0x10000020;
	unk150        = 0x11;
	mBubbleParams = (TBubbleSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveBubbleLive::theNerve());
	mMActor->setLightType(LIGHT_TYPE_INDIRECT);

	TScreenTexture* tex = static_cast<TScreenTexture*>(
	    JDrama::TNameRefGen::search("スクリーンテクスチャ"));
	const ResTIMG* img = tex->getTexture()->getTexInfo();
	SMS_ChangeTextureAll(mMActor->getModel()->getModelData(), "H_ma_rak_dummy",
	                     *img);
}

void TBubble::reset()
{
	TWalkerEnemy::reset();

	onLiveFlag(LIVE_FLAG_UNK8);
	unk1CC = TMsRange<f32>(50.0f, 150.0f).rand();
	unk1D0 = false;
	unk1D1 = true;
	unk1D2 = false;
	mAppended = nullptr;
	mSpine->initWith(&TNerveBubbleLive::theNerve());
}

void TBubble::split()
{
	int num = mBubbleParams->mSLNumDivision.get();
	for (int i = 0; i < num; i++) {
		TBubble* bubble = (TBubble*)gpConductor->makeOneEnemyAppear(
		    mPosition, "バブルマネージャー", 1);
		if (!bubble)
			break;

		bubble->reset();
		bubble->mPosition = mPosition;
		bubble->mPosition.y += unk1CC;
		bubble->unk1CC = 0.0f;
		bubble->unk1D0 = true;

		TMsRange<f32> range(-2.0f, 2.0f);
		JGeometry::TVec3<f32> velocity;
		velocity.x = range.rand();
		velocity.y = range.rand();
		velocity.z = range.rand();
		bubble->setVelocity(velocity);
	}
}

f32 TBubble::getGravityY() const
{
	if (unk1D0) {
		if (unk1D1)
			return 0.001f;
		return 0.0f;
	}
	return mGravity;
}

void TBubble::kill()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (mAppended) {
		if (unk1D2)
			mAppended->receiveMessage(this, HIT_MESSAGE_THROWN);
		else
			mAppended->kill();
		mAppended = nullptr;
	}

	mHitPoints = 1;
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		mSpine->pushAfterCurrent(mSpine->getDefault());
		onLiveFlag(LIVE_FLAG_UNK20000);
	}
	onLiveFlag(LIVE_FLAG_UNK40);
}

void TBubble::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() == &TNerveBubbleLive::theNerve()
	    && mMActor->checkCurBckFromIndex(10)) {
		kill();
		TItem* item = (TItem*)gpItemManager->makeObjAppear(
		    mPosition.x, 20.0f + mPosition.y, mPosition.z, 0x20000002, true);
		if (item)
			item->killByTimer(0x4B0);
	}
}

void TBubble::attackToMario()
{
	sendAttackMsgToMario();
	kill();
}

void TBubble::calcRootMatrix()
{
	char trash[8];
	if (!isEaten()) {
		mPosition.y = mGroundHeight + unk1CC + 150.0f;
		MsMtxSetXYZRPH(mMActor->getModel()->getBaseTRMtx(), mPosition.x,
		               mPosition.y, mPosition.z, mRotation.x, mRotation.y,
		               mRotation.z);
		mMActor->getModel()->setBaseScale(mScaling);
	}
}

void TBubble::setDeadAnm() { setBckAnm(9); }

MtxPtr TBubble::getTakingMtx() { return mMActor->getModel()->getBaseTRMtx(); }

const char** TBubble::getBasNameTable() const { return btelesa_bastable; }

void TBubble::appendEnemy()
{
	mAppended = nullptr;

	TSmallEnemy* enemy;
	char trash[0x10];
	f32 r = TMsRange<f32>(0.0f, 100.0f).rand();
	if (r < 50.0f) {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "ポポマネージャー", 1);
		enemy->unk154 = 0.6f;
		enemy->reset();
	} else if (r < 100.0f) {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "ボム兵マネージャー", 1);
		enemy->unk154 = 0.3f;
		enemy->reset();
	} else if (r < 150.0f) {
		TTelesa* telesa = (TTelesa*)gpConductor->makeOneEnemyAppear(
		    mPosition, "テレサマネージャー", 1);
		enemy = telesa;
		if (!telesa)
			return;
		telesa->unk154 = 0.6f;
		telesa->reset();
		telesa->setAttacker();
	} else {
		enemy = (TSmallEnemy*)gpConductor->makeOneEnemyAppear(
		    mPosition, "パックンマネージャー", 1);
		enemy->unk154 = 0.6f;
		enemy->reset();
	}

	if (enemy) {
		if (enemy->receiveMessage(this, HIT_MESSAGE_TAKE)) {
			enemy->onHitFlag(HIT_FLAG_UNK8000000);
			mHeldObject = enemy;
			enemy->setVelocity(JGeometry::TVec3<f32>(0.0f, 2.0f, 10.0f));
			enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
			mAppended = enemy;
		}
	}
}

DEFINE_NERVE(TNerveBubbleLive, TLiveActor)
{
	TBubble* self = static_cast<TBubble*>(spine->getBody());

	if (spine->getTime() == 0) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		if (!self->unk1D0) {
			self->setBckAnm(8);
		} else {
			self->setBckAnm(10);
			self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		}

		J3DFrameCtrl* frameCtrl = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		frameCtrl->setFrame(TMsRange<f32>(0.0f, 20.0f).rand());
		self->onLiveFlag(LIVE_FLAG_UNK8);
	} else if (self->checkCurAnmEnd(0)) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->setBckAnm(10);
	}

	f32 addPosBase = self->mBubbleParams->mSLAddPosBase.get();
	if (!self->unk1D0) {
		if (self->unk1CC < addPosBase)
			self->unk1CC += 2.0f;
	} else {
		if (spine->getTime() > 40 && self->unk1D1) {
			JGeometry::TVec3<f32> velocity = self->getVelocity();
			JGeometry::TVec3<f32> scaled;
			scaled.set(velocity);
			scaled.scale(0.98f);
			self->setVelocity(scaled);
		} else {
			self->walkBehavior(0, 1.0f);
		}

		if (spine->getTime() == 80) {
			self->unk1D1 = false;
			self->setVelocity(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
		}
	}

	self->unk1CC += 0.001f;
	if (self->unk1CC
	    > self->mPosition.y + self->mBubbleParams->mSLDeadHeight.get()) {
		self->unk1D2 = false;
		self->kill();
	}

	if (self->mScaling.x < self->mBubbleParams->mSLMaxScale.get()) {
		self->mScaling.x = self->mScaling.y = self->mScaling.z
		    = self->mScaling.z * self->mBubbleParams->mSLRateExpand.get();
	}

	if (spine->getTime() > self->mBubbleParams->mSLLiveTime.get()) {
		spine->pushAfterCurrent(&TNerveBubbleSplit::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveBubbleSplit, TLiveActor)
{
	TBubble* self = static_cast<TBubble*>(spine->getBody());

	if (spine->getTime() == 0) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->split();
	}

	if (spine->getTime() == 10)
		self->setBckAnm(9);

	if (self->checkCurAnmEnd(0) && self->mMActor->checkCurBckFromIndex(9)) {
		self->unk1D2 = false;
		self->kill();
	}

	return false;
}

TBossTelesaSaveLoadParams::TBossTelesaSaveLoadParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLDamageRadius, 200)
    , PARAM_INIT(mSLDamageHeight, 100)
    , PARAM_INIT(mSLAttackRadius, 220)
    , PARAM_INIT(mSLAttackHeight, 120)
    , PARAM_INIT(mSLGenAttackerTime, 500)
    , PARAM_INIT(mSLGenBubbleTime, 600)
    , PARAM_INIT(mSLHitAngle, 20.0f)
    , PARAM_INIT(mSLNumGenBubble, 5)
    , PARAM_INIT(mSL1stBubbleSp, 10.0f)
    , PARAM_INIT(mSLHideAreaRadius, 500.0f)
    , PARAM_INIT(mSLSlotItemNum, 5)
    , PARAM_INIT(mSLSlotFruitNum, 10)
    , PARAM_INIT(mSLSlotFirstHitCollectRate, 0.1f)
    , PARAM_INIT(mSLSlotHitCollectRate, 0.1f)
    , PARAM_INIT(mSLTransYOffset, 350.0f)
    , PARAM_INIT(mSLStopSlotTime0, 3000)
    , PARAM_INIT(mSLStopSlotTime1, 2000)
    , PARAM_INIT(mSLStopSlotTime2, 1000)
    , PARAM_INIT(mSLSpicyTime, 2000)
{
	TParams::load(mPrmPath);
}

TBossTelesaManager::TBossTelesaManager(const char* name)
    : TEnemyManager(name)
{
}

void TBossTelesaManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBossTelesaSaveLoadParams("/enemy/bosstelesa.prm");
	TEnemyManager::load(stream);
}

TSpineEnemy* TBossTelesaManager::createEnemyInstance()
{
	return new TBossTelesa;
}

void TBossTelesaManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "btelesa.bmd", 0x15300000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBossTelesaManager::perform(u32 flags, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(flags, graphics);
}

BOOL TBossTelesaBody::receiveMessage(THitActor* sender, u32 message)
{
	TBossTelesa* owner = mOwner;
	if (message == HIT_MESSAGE_TRAMPLE && !sender->isActorType(0x80000001)) {
		if (owner->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaPrepareSlot::theNerve())
			owner->mSpine->pushNerve(&TNerveBossTelesaSpit::theNerve());
	}
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (owner->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaPrepareSlot::theNerve())
			owner->mSpine->pushNerve(&TNerveBossTelesaFreeze::theNerve());
	}
	return true;
}

BOOL TBossTelesaTongue::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		TBossTelesa* owner = mOwner;
		if (owner->mSpine->getCurrentNerve()
		    == &TNerveBossTelesaAppear::theNerve())
			owner->mSpine->pushNerve(&TNerveBossTelesaSlotStart::theNerve());
	}
	return true;
}

void TBossTelesaKillSmallEnemy::checkHit()
{
	char trash[8];
	unk6C = 0;
	for (int i = 0; i < mColCount; ++i) {
		THitActor* hit = mCollisions[i];
		if (hit->checkActorType(0x10000000)) {
			TLiveActor* enemy = static_cast<TLiveActor*>(hit);
			if (enemy->getActorType() == 0x10000013)
				static_cast<THamuKuri*>(enemy)->selectCapHolder();
			enemy->kill();
		}
	}

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= mPosition;
	diff.y = 0.0f;
	if (MsVECMag2(&diff) < 300.0f) {
		mOwner->forceHide();
		unk6C = 1;
	}
}
static inline void randomResetDrums(TTelesaSlot* slot)
{
	TMsRange<s32> range(0, 8);
	for (int i = 0; i < 3; ++i) {
		slot->unk13C[i] = slot->unk168 * range.rand();
		slot->unk198[i] = false;
	}
}

void TTelesaSlot::initMapObj()
{
	TSlotDrum::initMapObj();
	onLiveFlag(LIVE_FLAG_UNK10);
	unk14C = 160.0f;
	unk150 = mPosition.y;
	unk154 = 2.0f;
	unk158 = 2.0f;
	unk15C = 0.01f;
	unk160 = 0.5f;
	unk164 = 0;
	unk168 = 45;
	unk140 = mDamageRadius / 3.0f;
	unk144 = mDamageHeight;
	mMapCollisionMove = new TMapCollisionMove;
	mMapCollisionMove->init(2, 0, 0, nullptr);
	randomResetDrums(this);
}

// Called (not inlined) from TNerveBossTelesaAppear::execute in the original.
#pragma dont_inline on
void TTelesaSlot::randomReset()
{
	randomResetDrums(this);
}

#pragma dont_inline off

void TTelesaSlot::calcRootMatrix()
{
	char trash[8];
	bool moving = false;
	for (int i = 0; i < 3; ++i) {
		if (0.0f != unk138[i])
			moving = true;
	}
	if (moving) {
		if (unk1E0) {
			if (gpMSound->gateCheck(MSD_SE_OBJ_SLOT_SPIN)) {
				MSoundSESystem::MSoundSE::startSoundActor(
				    MSD_SE_OBJ_SLOT_SPIN, &mPosition, 0, nullptr, 0, 4);
			}
		}
		unk1E0 = 1 - unk1E0;
	}
	TSlotDrum::calcRootMatrix();
}

void TTelesaSlot::moveObject()
{
	char trash[0x2c];
	TLiveActor::moveObject();
	for (int i = 0; i < unk148; ++i) {
		if (unk1A8[i] && unk1A4 == getForcastResult(i)) {
			unk198[i] = false;
			unk1A8[i] = false;
		}
		if (unk138[i] != 0.0f) {
			if (fabsf(unk138[i]) > unk160) {
				unk13C[i] += unk138[i];
				if (!unk198[i]) {
					if (unk138[i] > 0.0f)
						unk138[i] -= unk15C;
					else
						unk138[i] += unk15C;
				}
				if (unk13C[i] >= 360.0f)
					unk13C[i] -= 360.0f;
				if (unk13C[i] <= 0.0f)
					unk13C[i] += 360.0f;
			} else {
				unk13C[i] += unk138[i];
				if (unk13C[i] >= 360.0f)
					unk13C[i] -= 360.0f;
				if (unk13C[i] <= 0.0f)
					unk13C[i] += 360.0f;
				if (!unk198[i] && (int)fabsf(unk13C[i]) % unk168 == 0) {
					unk13C[i] = (f32)(unk168 * (int)(unk13C[i] / (f32)unk168));
					unk138[i] = 0.0f;
					if (gpMSound->gateCheck(MSD_SE_BS_TELESA_SLT_STOP)) {
						MSoundSESystem::MSoundSE::startSoundActor(
						    MSD_SE_BS_TELESA_SLT_STOP, &mPosition, 0, nullptr, 0,
						    4);
					}
					for (int j = 0; j < unk148; ++j) {
						if (unk198[j]) {
							TMsRange<f32> range(0.0f, 1.0f);
							f32 rate = mBoss->mBossParams->mSLSlotHitCollectRate.get();
							if (range.rand() <= rate)
								unk1A8[j] = true;
							else
								unk198[j] = false;
						}
					}
					bool allStopped = true;
					for (int k = 0; k < 3; ++k) {
						if (0.0f != unk138[k])
							allStopped = false;
					}
					if (allStopped) {
						TBossTelesa* boss = mBoss;
						if (boss->mSlot->getSlotResult() == 2
						    || boss->mSlot->getSlotResult() == 0) {
							boss->mHitPos.set(0.0f, 0.0f, 0.0f);
							gpMarioParticleManager->emit(0xE1, &boss->mHitPos, 0,
							                             nullptr);
							if (boss->mSlot->getSlotResult() == 2) {
								if (gpMSound->gateCheck(
								        MSD_SE_BS_TELESA_FANFALE_1)) {
									MSoundSESystem::MSoundSE::startSoundActor(
									    MSD_SE_BS_TELESA_FANFALE_1,
									    &boss->mPosition, 0, nullptr, 0, 4);
								}
							} else {
								if (gpMSound->gateCheck(
								        MSD_SE_BS_TELESA_FANFALE_2)) {
									MSoundSESystem::MSoundSE::startSoundActor(
									    MSD_SE_BS_TELESA_FANFALE_2,
									    &boss->mPosition, 0, nullptr, 0, 4);
								}
							}
						} else {
							if (gpMSound->gateCheck(MSD_SE_BS_TELESA_FANFALE_3)) {
								MSoundSESystem::MSoundSE::startSoundActor(
								    MSD_SE_BS_TELESA_FANFALE_3,
								    &boss->mPosition, 0, nullptr, 0, 4);
							}
						}
					}
				}
			}
		}
	}
}

// Stays out of line (the original SlotStart nerve calls it).
#pragma dont_inline on
void TTelesaSlot::moveStart()
{
	unk19C = true;
	unk19B = true;
	for (int i = 0; i < 3; ++i) {
		unk198[i] = true;
		unk1A8[i] = false;
		f32 dir   = 1.0f;
		if (i == 0)
			dir = -1.0f;
		if (i == 1)
			dir = -0.8f;
		unk138[i] = dir * unk158;
	}
}
#pragma dont_inline off

u32 TTelesaSlot::touchWater(THitActor*) { return 0; }

void TTelesaSlot::forceStopSlot(int idx)
{
	TMsRange<f32> range(0.0f, 1.0f);
	if (unk19C) {
		f32 rate = mBoss->mBossParams->mSLSlotFirstHitCollectRate.get();
		if (SMS_GetMarioHP() == 1)
			rate = 0.9f;

		if (range.rand() <= rate) {
			unk1A4 = 2;
			if (SMS_GetMarioHP() <= 3)
				unk1A4 = 0;
			unk1A8[idx] = true;
		} else {
			unk1A4      = getForcastResult(idx);
			unk198[idx] = false;
		}

		if (unk1A4 == mBoss->mSlotResult)
			unk1A4 = 3;

		if (unk1A4 == 0) {
			if (mBoss->unk370 == 0)
				unk1A4 = 1;
			else if (SMS_GetMarioHP() >= 6)
				unk1A4 = 3;
		}
		unk19C = false;
	}
}

bool TTelesaSlot::isRollDrum()
{
	for (int i = 0; i < 3; ++i) {
		if (unk198[i])
			return true;
	}
	unk19B = false;
	return false;
}

// Called (not inlined) from moveObject in the original.
#pragma dont_inline on
int TTelesaSlot::getSlotResult()
{
	int result = getResultFromAng(unk13C[0]);
	for (int i = 1; i < 3; ++i)
		if (result != getResultFromAng(unk13C[i]))
			return -1;

	return result;
}
#pragma dont_inline off

int TTelesaSlot::getForcastResult(int idx)
{
	f32 angle = unk13C[idx];
	f32 speed = unk138[idx];
	for (;;) {
		if (fabsf(speed) > unk160) {
			angle += speed;
			if (speed > 0.0f)
				speed -= unk15C;
			else
				speed += unk15C;
			if (angle >= 360.0f)
				angle -= 360.0f;
			if (angle <= 0.0f)
				angle += 360.0f;
		} else {
			angle += speed;
			if (angle >= 360.0f)
				angle -= 360.0f;
			if (angle <= 0.0f)
				angle += 360.0f;
			if ((int)fabsf(angle) % unk168 == 0)
				break;
		}
	}
	angle = (int)(angle / unk168) * unk168;
	return getResultFromAng(angle);
}

int TTelesaSlot::getResultFromAng(f32 ang)
{
	if (ang < 44.0f)
		return 0;
	if (ang < 89.0f)
		return 1;
	if (ang < 134.0f)
		return 3;
	if (ang < 179.0f)
		return 2;
	if (ang < 224.0f)
		return 0;
	if (ang < 269.0f)
		return 1;
	if (ang < 314.0f)
		return 3;
	if (ang < 359.0f)
		return 2;
	return 2;
}

TBossTelesa::TBossTelesa(const char* name)
    : TSpineEnemy(name)
    , mGenTelesa(1)
    , unk154(0)
    , unk158(0)
    , mBossParams(nullptr)
    , mCurBck(-1)
    , mPrevBck(-1)
    , mBckBlendRatio(0.0f)
    , mBody(nullptr)
    , mSlot(nullptr)
    , mSlotFrame(nullptr)
    , unk18C(0)
    , mSlotResult(0)
    , mItemNum(0)
    , mIsSpicy(false)
    , mTelesaManager(nullptr)
    , mMarioHP(0)
    , unk35A(1)
    , unk35B(1)
    , mHitCount(0)
    , mCameraMove(0.0f)
    , mHoseiPosY(0.0f)
    , mSlotTimer(0)
    , mSpicyTimer(0)
    , unk370(3)
    , mBgmStarted(0)
{
}

void TBossTelesa::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("btelesa.bmd", 3);
	onLiveFlag(LIVE_FLAG_DEAD);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	TSpineEnemyParams* params = getSaveParam();
	if (params != nullptr) {
		mBodyRadius       = params->mSLBodyRadius.get();
		mWallRadius       = params->mSLWallRadius.get();
		mHeadHeight       = params->mSLHeadHeight.get();
		mScaledBodyRadius = mBodyScale * mBodyRadius;
		mHitPoints        = getMaxHitPoints();
	}

	initHitActor(0, 5, 0x98000000, mBodyRadius, mHeadHeight, mBodyRadius,
	             mHeadHeight);
	mGroundPlane = TMap::getIllegalCheckData();
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	initAnmSound();
	mBossParams = static_cast<TBossTelesaSaveLoadParams*>(getSaveParam());
	mActorType  = 0x8000013;
	onHitFlag(0xD0000000);

	mSpine->initWith(&TNerveBossTelesaFallDemo::theNerve());
	onLiveFlag(LIVE_FLAG_UNK8);
	mMActor->setLightType(LIGHT_TYPE_OBJECT);
	mMActor->unk40 = true;
	mMActor->initNormalMotionBlend();

	J3DModel* model = mMActor->getModel();
	if (model->getSkinDeform() == nullptr) {
		J3DSkinDeform* deform = new J3DSkinDeform;
		model->setSkinDeform(deform, J3D_DEFORM_ATTACH_FLAG_UNK_1);
	}
	mMActor->resetDL();

	mKColor1.r = mKColor1.g = mKColor1.b = mKColor1.a = 0xFF;
	mKColor0.r = mKColor0.g = mKColor0.b = mKColor0.a = mNormalAlpha;
	for (int i = 0; i < mMActor->getModel()->getModelData()->getMaterialNum();
	     ++i) {
		if (i
		    == getModel()->getModelData()->getMaterialName()->getIndex(
		        "_mat_body"))
			SMS_InitPacket_TwoTevKColor(mMActor->getModel(), i, GX_KCOLOR0,
			                            &mKColor0, GX_KCOLOR1, &mKColor1);
		else
			SMS_InitPacket_OneTevKColor(mMActor->getModel(), i, GX_KCOLOR0,
			                            &mKColor0);
	}

	reset();

	for (int i = 0; i < gpMapObjManager->getObjNum(); ++i) {
		if (gpMapObjManager->getObj(i)->isActorType(0x4000019A)) {
			unk154 = 0;
			unk158 = 0;
			mPosition = gpMapObjManager->getObj(i)->mPosition;
		}
	}

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < mMActor->getModel()->getModelData()->getJointNum();
		     i++) { }
	}

	mBody = new TBossTelesaBody;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(mBody);
	mBody->initHitActor(0x8000013, 5, 0xD1000000, 350.0f, 550.0f, 300.0f,
	                    500.0f);
	mBody->mOwner = this;
	mBody->offHitFlag(HIT_FLAG_NO_COLLISION);

	mTongue = new TBossTelesaTongue;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(mTongue);
	mTongue->initHitActor(0x8000013, 5, 0xC0000000, 180.0f, 350.0f, 180.0f,
	                      350.0f);
	mTongue->mOwner = this;
	mTongue->offHitFlag(HIT_FLAG_NO_COLLISION);

	mKillSmallEnemy = new TBossTelesaKillSmallEnemy;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(mKillSmallEnemy);
	mKillSmallEnemy->initHitActor(0x1000000C, 5, 0x10000000, 400.0f, 300.0f,
	                              400.0f, 300.0f);
	mKillSmallEnemy->offHitFlag(HIT_FLAG_NO_COLLISION);
	mKillSmallEnemy->mOwner = this;

	mMActor->setLightType(LIGHT_TYPE_INDIRECT);
	TScreenTexture* tex = static_cast<TScreenTexture*>(
	    JDrama::TNameRefGen::search("スクリーンテクスチャ"));
	const ResTIMG* img = tex->getTexture()->getTexInfo();
	SMS_ChangeTextureAll(mMActor->getModel()->getModelData(), "H_ma_rak_dummy",
	                     *img);
	mMarioHP = SMS_GetMarioHP();
}

void TBossTelesa::loadAfter()
{
	u32 count = gpMapObjManager->getObjNumWithActorType(0x4000019A);
	if (count != 0) {
		int n = 0;
		for (int i = 0; i < gpMapObjManager->getObjNum(); ++i) {
			if (gpMapObjManager->getObj(i)->isActorType(0x4000019A))
				mRoulettes[n++] = static_cast<TRoulette*>(gpMapObjManager->getObj(i));
		}
	}

	count = gpMapObjManager->getObjNumWithActorType(0x400001A6);
	if (count != 0) {
		for (int i = 0; i < gpMapObjManager->getObjNum(); ++i) {
			if (gpMapObjManager->getObj(i)->isActorType(0x400001A6)) {
				mSlot        = static_cast<TTelesaSlot*>(gpMapObjManager->getObj(i));
				mSlot->mBoss = this;
			}
		}
	}

	int n = 0;
	for (int i = 0; i < 6; ++i) {
		mFruits[n] = TMapObjBaseManager::newAndRegisterObj("FruitCoconut");
		++n;
	}
	for (int i = 0; i < 6; ++i) {
		mFruits[n] = TMapObjBaseManager::newAndRegisterObj("FruitPapaya");
		++n;
	}
	for (int i = 0; i < 2; ++i) {
		mFruits[n] = TMapObjBaseManager::newAndRegisterObj("FruitPine");
		++n;
	}
	for (int i = 0; i < 6; ++i) {
		mFruits[n] = TMapObjBaseManager::newAndRegisterObj("FruitDurian");
		++n;
	}

	for (int i = 0; i < 20; ++i) {
		mFruits[i]->onMapObjFlag(0x4000000);
		mFruits[i]->makeObjDead();
	}

	mChilis[0] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 0"));
	mChilis[1] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 1"));
	mChilis[2] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 2"));
	mChilis[3] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 3"));
	mChilis[4] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 4"));
	mChilis[5] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 5"));
	mChilis[6] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 6"));
	mChilis[7] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 7"));
	mChilis[8] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 8"));
	mChilis[9] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("唐辛子 9"));
	for (int i = 0; i < 10; ++i)
		mChilis[i]->makeObjDead();

	mCoins[0] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 0"));
	mCoins[1] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 1"));
	mCoins[2] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 2"));
	mCoins[3] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 3"));
	mCoins[4] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 4"));
	mCoins[5] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 5"));
	mCoins[6] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 6"));
	mCoins[7] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 7"));
	mCoins[8] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 8"));
	mCoins[9] = static_cast<TMapObjBase*>(JDrama::TNameRefGen::search("コイン 9"));
	for (int i = 0; i < 10; ++i)
		mCoins[i]->makeObjDead();

	mSlot->unk1E4[0] = 0.0f;
	mSlot->unk1E4[1] = 0.0f;
	mSlot->unk1E4[2] = 0.0f;

	mTelesaManager = static_cast<TTelesaManager*>(
	    JDrama::TNameRefGen::search("テレサマネージャー"));

	for (int i = 0; i < 5; ++i)
		TMapObjBaseManager::newAndRegisterObj("bottle_large");

	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit.jpa", 0xD7);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_pe.jpa", 0xD8);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_gr.jpa", 0xD9);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fhit_or.jpa", 0xDA);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_damage.jpa", 0xDB);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down.jpa", 0xDC);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_pe.jpa", 0xDD);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_gr.jpa", 0xDE);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_down_or.jpa", 0xDF);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_hit.jpa", 0xE0);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_fubuki.jpa", 0xE1);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare1.jpa", 0x19E);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare2.jpa", 0x19F);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_yodare3.jpa", 0x1A0);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_ase.jpa", 0x1A1);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_a.jpa", 0x1A2);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_b.jpa", 0x1A3);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_d.jpa", 0x1A4);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_chika_a.jpa", 0x1A5);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_chika_b.jpa", 0x1A6);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_glow.jpa", 0x1A7);
	SMS_LoadParticle("/scene/btelesa/jpa/ms_btls_spicy_c.jpa", 0x1F0);

	void* frameRes = JKRFileLoader::getGlbResource("/scene/btelesa/srot_waku.bmd");
	SDLModelData* frameModel
	    = new SDLModelData(J3DModelLoaderDataBase::load(frameRes, 0x10220000));
	mSlotFrame = new TSharedParts(mSlot, 0, frameModel, 3, "<TSharedParts>");

	TLiveActor* gesso = static_cast<TLiveActor*>(
	    JDrama::TNameRefGen::search("ゲッソー 0"));
	if (gesso != nullptr)
		gesso->onLiveFlag(LIVE_FLAG_DEAD);
	gesso = static_cast<TLiveActor*>(JDrama::TNameRefGen::search("ゲッソー 1"));
	if (gesso != nullptr)
		gesso->onLiveFlag(LIVE_FLAG_DEAD);

	TSpineEnemy::loadAfter();
}

void TBossTelesa::reset()
{
	char trash[8];
	TSpineEnemy::reset();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onLiveFlag(LIVE_FLAG_UNK8);
	onLiveFlag(LIVE_FLAG_UNK10);
	onLiveFlag(LIVE_FLAG_HIDDEN);
	unk18C = 0;
	f32 attackRadius = mBossParams->mSLAttackRadius.get();
	f32 attackHeight = mBossParams->mSLAttackHeight.get();
	f32 damageRadius = mBossParams->mSLDamageRadius.get();
	f32 damageHeight = mBossParams->mSLDamageHeight.get();
	setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
	TMarDirector* director = gpMarDirector;
	director->fireStartDemoCamera("btelesa_roll_camera", nullptr, -1, 0.0f,
	                              true, nullptr, 0, nullptr,
	                              JDrama::TFlagT<u16>(0));
}
static inline JGeometry::TVec3<f32> telesaVecCopy(const Vec& v)
{
	return JGeometry::TVec3<f32>(v);
}

void TBossTelesa::moveObject()
{
	char trash[0x60];
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	JGeometry::TVec3<f32> toCamera
	    = *gpMarioPos - telesaVecCopy(gpCamera->unk124);
	if (toCamera.length() < mCameraMoveLimit) {
		mCameraMove += mCameraMoveSp * (gpMarioPos->y - gpCamera->unk148.y);
		gpCamera->unk290 = mCameraMove;
	} else if (fabsf(mCameraMove) > 1.0f) {
		mCameraMove      = mCameraMove * mCameraMoveSp;
		gpCamera->unk290 = mCameraMove;
	}

	if (SMS_CheckMarioFlag(MARIO_FLAG_GAME_OVER)) {
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_V_LAUGH1, &mPosition,
		                                0, nullptr, 0, 4);
	}

	if (mSpine->getCurrentNerve() == &TNerveBossTelesaFallDemo::theNerve()) {
		if (mRoulettes[0]->mPosition.y > 5.0f + mRoulettes[1]->mPosition.y) {
			mSlot->mPosition = mRoulettes[0]->mPosition;
			mSlot->mPosition.y += 300.0f;
		}
	}

	if (mSpine->getCurrentNerve() != &TNerveBossTelesaPrepareSlot::theNerve()) {
		mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
		mMActor->getFrameCtrl(ANM_TYPE_BTP)->setFrame(0.0f);
	}

	if (mSpine->getCurrentNerve() == &TNerveBossTelesaDie::theNerve()) {
		u8 targetAlpha = clampBossTelesa<u8>(
		    mNormalAlpha + (getMaxHitPoints() - mHitPoints) * 30, 0, 254);
		if ((mMActor->checkCurBckFromIndex(7)
		     && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 50.0f)
		    || mMActor->checkCurBckFromIndex(6)) {
			if (mKColor0.a > targetAlpha)
				mKColor0.a--;
		} else {
			if (mKColor0.a < 0xFF)
				mKColor0.a++;
		}
	}

	JGeometry::TVec3<f32> toCamera2
	    = *gpMarioPos - telesaVecCopy(gpCamera->unk124);
	mRoulettePos = mRoulettes[0]->mPosition;
	mRoulettePos.x += 0.67f * toCamera2.x;
	mRoulettePos.z += 0.67f * toCamera2.z;

	int rollingNum = 0;
	if (mRoulettes[0]->unk13C != 0.0f)
		rollingNum = 1;
	if (mRoulettes[1]->unk13C != 0.0f)
		rollingNum++;
	if (mRoulettes[2]->unk13C != 0.0f)
		rollingNum++;

	switch (rollingNum) {
	case 1:
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_RLT_MOVE1,
		                                &mRoulettePos, 0, nullptr, 0, 4);
		break;
	case 2:
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_RLT_MOVE2,
		                                &mRoulettePos, 0, nullptr, 0, 4);
		break;
	case 3:
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_RLT_MOVE3,
		                                &mRoulettePos, 0, nullptr, 0, 4);
		break;
	}

	mLinearVelocity.zero();
	mAngularVelocity.zero();
	control();

	for (int i = 0; i < mColCount; ++i) {
		if (mCollisions[i]->isActorType(0x80000001))
			SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
	}

	bind();

	mPosition.add(mLinearVelocity);
	mPosition.y = 300.0f + mGroundHeight;

	mBckBlendRatio = clampBossTelesa<f32>(mBckBlendRatio - 0.05f, 0.0f, 1.0f);
	mMActor->setMotionBlendRatioForBck(mBckBlendRatio);

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		mBody->mPosition          = mPosition;
		mTongue->mPosition        = mPosition;
		mKillSmallEnemy->mPosition = mPosition;
	} else {
		MtxPtr joint1 = mMActor->getModel()->getAnmMtx(1);
		mBody->mPosition.set(joint1[0][3], joint1[1][3] - 200.0f,
		                     joint1[2][3]);
		mKillSmallEnemy->mPosition.set(joint1[0][3], mPosition.y - 350.0f,
		                               joint1[2][3]);
		MtxPtr joint7 = mMActor->getModel()->getAnmMtx(7);
		mTongue->mPosition.set(joint7[0][3], joint7[1][3] - 350.0f,
		                       joint7[2][3]);
	}

	TBossTelesaBody* body = mBody;
	body->unk6C           = 0;
	for (int i = 0; i < body->mColCount; ++i) {
		THitActor* hit = body->mCollisions[i];
		if (hit->isActorType(0x80000001))
			SMS_SendMessageToMario(body, HIT_MESSAGE_ATTACK);
		else
			body->mOwner->checkHitObject(hit);
	}

	TBossTelesaTongue* tongue = mTongue;
	for (int i = 0; i < tongue->mColCount; ++i) {
		THitActor* hit = tongue->mCollisions[i];
		if (hit->isActorType(0x80000001)) {
			SMS_SendMessageToMario(tongue, HIT_MESSAGE_ATTACK);
		} else if (hit->isActorType(0x40000395)) {
			tongue->mOwner->setSpicy(static_cast<TLiveActor*>(hit));
		} else if (100.0f + tongue->mPosition.y < hit->mPosition.y) {
			tongue->mOwner->checkHitObject(hit);
		}
	}

	mKillSmallEnemy->checkHit();
}

void TBossTelesa::kill()
{
	if (mSpine->getCurrentNerve() != &TNerveBossTelesaDie::theNerve())
		mSpine->pushNerve(&TNerveBossTelesaDie::theNerve());
}

MtxPtr TBossTelesa::getTakingMtx()
{
	char trash[16];
	reinterpret_cast<TPosition3f*>(mTakingMtx)->set(
	    mRoulettes[0]->mMActor->getModel()->getAnmMtx(1));
	mTakingMtx[1][3] = mRoulettes[0]->mPosition.y - 120.0f;
	return mTakingMtx;
}

void TBossTelesa::calcRootMatrix()
{
	mMActor->getModel()->setBaseScale(mScaling);

	f32 x = mPosition.x;
	f32 z = mPosition.z;
	f32 y = mPosition.y + mBossParams->mSLTransYOffset.get()
	        + clampBossTelesa<f32>(mHoseiPosY, mBaseHoseiPosY, 0.0f);

	TPosition3f mtx(x, y, z);

	Mtx rot;
	MsMtxSetRotRPH(rot, mRotation.x, mRotation.y, mRotation.z);
	MTXConcat(mtx, rot, mtx);
	mMActor->getModel()->setBaseTRMtx(mtx);

	if (mSlot != nullptr) {
		mSlot->mRotation = mRotation;

		f32 slotOffsetY = -700.0f;
		MtxPtr joint;
		if (mMActor->checkCurBckFromIndex(4) || mMActor->checkCurBckFromIndex(0)
		    || mMActor->checkCurBckFromIndex(2)
		    || mMActor->checkCurBckFromIndex(5)) {
			joint = mMActor->getModel()->getAnmMtx(1);
			if (!mMActor->checkCurBckFromIndex(0))
				slotOffsetY = -2400.0f;
		} else {
			joint = mMActor->getModel()->getAnmMtx(0);
		}

		mSlot->mPosition.x = joint[0][3];
		mSlot->mPosition.y = mHoseiPosY + (joint[1][3] + slotOffsetY);
		mSlot->mPosition.z = joint[2][3];
	}

	for (u16 i = 0; i < mMActor->getModel()->getModelData()->getMaterialNum();
	     ++i) {
		Mtx effectMtx;
		SMS_GetLightPerspectiveForEffectMtx(effectMtx);
		mMActor->getModel()
		    ->getModelData()
		    ->getMaterialNodePointer(i)
		    ->getTexGenBlock()
		    ->getTexMtx(1)
		    ->setEffectMtx(effectMtx);
	}

	mHitPos.setAll(0.0f);
	gpMarioParticleManager->emit(0x1A5, &mHitPos, 1, this);
	gpMarioParticleManager->emit(0x1A6, &mHitPos, 1, this);

	MtxPtr joint1 = mMActor->getModel()->getAnmMtx(1);
	mHitPos.set(joint1[0][3], joint1[1][3], joint1[2][3]);
	gpMarioParticleManager->emitAndBindToPosPtr(0x1A7, &mHitPos, 1, this);

	if (!mMActor->checkCurBckFromIndex(4) && !mMActor->checkCurBckFromIndex(6)
	    && !mMActor->checkCurBckFromIndex(12)
	    && !mMActor->checkCurBckFromIndex(13)) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x19E, mMActor->getModel()->getAnmMtx(5), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x19F, mMActor->getModel()->getAnmMtx(5), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1A0, mMActor->getModel()->getAnmMtx(10), 1, this);
	}

	if (mMActor->checkCurBckFromIndex(1)
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 20.0f) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xE0, mMActor->getModel()->getAnmMtx(5), 0, nullptr);
	}

	if (mMActor->checkCurBckFromIndex(12)) {
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_FIRE, &mPosition, 0,
		                                nullptr, 0, 4);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1A1, mMActor->getModel()->getAnmMtx(1), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1A2, mMActor->getModel()->getAnmMtx(9), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1A3, mMActor->getModel()->getAnmMtx(9), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1A4, mMActor->getModel()->getAnmMtx(9), 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1F0, mMActor->getModel()->getAnmMtx(9), 3, this);
	}

	if (mMActor->checkCurBckFromIndex(14)
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(40.0f)) {
		if (mSpine->getCurrentNerve()
		    == &TNerveBossTelesaSpitSlotItem::theNerve())
			generateSlotItem();
		else
			genAttacker();
	}
}

void TBossTelesa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if ((cue & CUE_ENTRY) && !checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_CLIPPED_OUT)) {
		if (mSpine->getCurrentNerve() == &TNerveBossTelesaDie::theNerve()
		    && mIsSpicy) {
			mMActor->offMakeDL();
			SMS_AddDamageFogEffect(mMActor->getModel()->getModelData(),
			                       mPosition, graphics);
		}
	}

	offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
	TSpineEnemy::perform(cue, graphics);
	mBody->THitActor::perform(cue, graphics);
	mTongue->THitActor::perform(cue, graphics);
	mKillSmallEnemy->THitActor::perform(cue, graphics);
	if (mSlot != nullptr)
		mSlot->testPerform(cue, graphics);
	if (mSlotFrame != nullptr)
		mSlotFrame->testPerform(cue, graphics);
}

BOOL TBossTelesa::receiveMessage(THitActor*, u32) { return false; }

void TBossTelesa::checkHitObject(THitActor* hit)
{
	char trash[0x28];
	mHitParticleId = -1;

	if ((hit->getActorType() & ACTOR_TYPE_MASK) != ACTOR_TYPE_UNK40000000)
		return;

	if (mSpine->getCurrentNerve() != &TNerveBossTelesaPrepareSlot::theNerve())
		return;

	switch (hit->getActorType()) {
	case 0x40000390:
		mKColor1.r     = 0xE6;
		mKColor1.g     = 0x64;
		mKColor1.b     = 0xB4;
		mHitParticleId = 0xD8;
		kill();
		break;
	case 0x40000391:
	case 0x40000392:
		mKColor1.r     = 0xE6;
		mKColor1.g     = 0xB4;
		mKColor1.b     = 0;
		mHitParticleId = 0xDA;
		kill();
		break;
	case 0x40000393:
		mKColor1.r     = 0x96;
		mKColor1.g     = 0x32;
		mKColor1.b     = 0xE6;
		mHitParticleId = 0xD9;
		kill();
		break;
	case 0x40000395:
		break;
	default:
		return;
	}

	if (!mIsSpicy && hit->getActorType() != 0x40000395) {
		if (unk35A) {
			unk35A = 0;
			gpMarDirector->mConsole->startAppearBalloon(0xE000F, true);
		}

		mHitCount++;
		if (mHitCount > 2)
			gpMarDirector->mConsole->startAppearBalloon(0xE0010, true);
	} else {
		mHitCount = 0;
	}

	mHitPos = hit->mPosition;
	gpMarioParticleManager->emit(0xD7, &mHitPos, 0, nullptr);
	if (mHitParticleId >= 0)
		gpMarioParticleManager->emit(mHitParticleId, &mHitPos, 0, nullptr);

	if (mIsSpicy) {
		gpMarioParticleManager->emit(0xDB, &mHitPos, 0, nullptr);
	} else {
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_FRUIT_HIT, &mPosition,
		                                0, nullptr, 0, 4);
	}

	static_cast<TMapObjBase*>(hit)->makeObjDead();
}

#pragma dont_inline on
void TBossTelesa::setSpicy(TLiveActor* actor)
{
	char trash[0x20];
	if (mSpine->getCurrentNerve() != &TNerveBossTelesaSpitSlotItem::theNerve()) {
		if (!mMActor->checkCurBckFromIndex(1)) {
			mIsSpicy    = true;
			mSpicyTimer = 0;
			setBossTelesaBck(this, 1);
			if (unk35B != 0) {
				unk35B = 0;
				gpMarDirector->mConsole->startAppearBalloon(0xE0011, true);
			}
			actor->kill();
		}
	}
}
#pragma dont_inline off
void TBossTelesa::damageRecover()
{
	for (int i = 0; i < 20; ++i) {
		TMapObjBase* item = mFruits[i];
		if (!item->checkLiveFlag(LIVE_FLAG_DEAD)) {
			if (item->mHolder == nullptr)
				SMS_SendMessageToMario(item, HIT_MESSAGE_UNK8);
			mFruits[i]->makeObjDead();
			gpMarioParticleManager->emit(PARTICLE_MS_TLS_CHANGE,
			                             &mFruits[i]->mPosition, 0, nullptr);
			mFruits[i]->mPosition.set(0.0f, 0.0f, 0.0f);
		}
	}

	for (int i = 0; i < 10; ++i) {
		TMapObjBase* chili = mChilis[i];
		if (!chili->checkLiveFlag(LIVE_FLAG_DEAD)) {
			if (chili->mHolder == nullptr)
				SMS_SendMessageToMario(chili, HIT_MESSAGE_UNK8);
			mChilis[i]->makeObjDead();
			gpMarioParticleManager->emit(PARTICLE_MS_TLS_CHANGE,
			                             &mChilis[i]->mPosition, 0, nullptr);
			mChilis[i]->mPosition.set(0.0f, 0.0f, 0.0f);
		}

		if (!mCoins[i]->checkLiveFlag(LIVE_FLAG_DEAD)) {
			mCoins[i]->makeObjDead();
			gpMarioParticleManager->emit(PARTICLE_MS_TLS_CHANGE,
			                             &mCoins[i]->mPosition, 0, nullptr);
			mCoins[i]->mPosition.set(0.0f, 0.0f, 0.0f);
		}
	}

	if (mIsSpicy) {
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_ESCAPE, &mPosition, 0,
		                                nullptr, 0, 4);
	} else {
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_DISAPPEAR, &mPosition,
		                                0, nullptr, 0, 4);
	}

	mSpine->pushAfterCurrent(&TNerveBossTelesaHide::theNerve());
	mSlotTimer = 0;
}
bool TBossTelesa::rouletteFall()
{
	char trash[0x20];
	if (mRoulettes[0]->mPosition.y > mRoulettes[1]->mPosition.y) {
		mRoulettes[0]->mPosition.y -= 2.0f;
		mRoulettes[0]->mMActor->setBck("rulet00");

		if (mRoulettes[0]->mPosition.y > mRoulettes[1]->mPosition.y + 3.0f) {
			if (SMS_SendMessageToMario(this, HIT_MESSAGE_TAKE))
				mHeldObject = (TTakeActor*)SMS_GetMarioHitActor();
		} else {
			if (SMS_SendMessageToMario(this, HIT_MESSAGE_UNK8)) {
				SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_RLT_SET,
				                                &mPosition, 0, nullptr, 0, 4);
				mHeldObject = nullptr;
			}
		}

		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_V_LAUGH1B, &mPosition,
		                                0, nullptr, 0, 4);
		SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_RLT_DOWN, &mPosition,
		                                0, nullptr, 0, 4);
		gpMarioOriginal->mGamePad->onNeutralMarioKey();
		return false;
	}

	mRoulettes[0]->mPosition.y = mRoulettes[1]->mPosition.y;
	return true;
}
bool TBossTelesa::slotFall()
{
	char trash[8];
	if (mSlot->mPosition.y > mRoulettes[0]->mPosition.y - 800.0f) {
		mSlot->mPosition.y -= 5.0f;
		return false;
	}

	mSlot->mPosition.y -= 1.0f;
	if (mSlot->mPosition.y < mRoulettes[0]->mPosition.y - 900.0f) {
		int rolling = 0;
		for (int i = 0; i < 3; ++i) {
			if (mRoulettes[i]->unk13C != 0.0f)
				++rolling;
		}
		if (rolling != 3)
			rouletteStart();
	}

	if (mSlot->mPosition.y < mRoulettes[0]->mPosition.y - 1100.0f)
		return true;

	mRoulettes[0]->unk150->setHitParams(280.0f, 100.0f, 280.0f, 100.0f);
	mRoulettes[1]->unk150->setHitParams(280.0f, 100.0f, 280.0f, 100.0f);
	return false;
}

void TBossTelesa::flashItem(int t)
{
	const int time = t % 16;

	for (int i = 0; i < 20; ++i) {
		if (!mFruits[i]->checkLiveFlag(LIVE_FLAG_DEAD)
		    && mFruits[i]->mHolder == nullptr) {
			if (time < 8)
				mFruits[i]->onLiveFlag(LIVE_FLAG_HIDDEN);
			else
				mFruits[i]->offLiveFlag(LIVE_FLAG_HIDDEN);
		}
	}

	for (int i = 0; i < 10; ++i) {
		if (!mChilis[i]->checkLiveFlag(LIVE_FLAG_DEAD)
		    && mChilis[i]->mHolder == nullptr) {
			if (time < 8)
				mChilis[i]->onLiveFlag(LIVE_FLAG_HIDDEN);
			else
				mChilis[i]->offLiveFlag(LIVE_FLAG_HIDDEN);
		}

		if (!mCoins[i]->checkLiveFlag(LIVE_FLAG_DEAD)) {
			if (time < 8)
				mCoins[i]->onLiveFlag(LIVE_FLAG_HIDDEN);
			else
				mCoins[i]->offLiveFlag(LIVE_FLAG_HIDDEN);
		}
	}
}

const char** TBossTelesa::getBasNameTable() const { return btelesa_bastable; }
void TBossTelesa::genAttacker()
{
	if (mGenTelesa) {
		TSmallEnemy* telesa = static_cast<TSmallEnemy*>(
		    gpConductor->makeOneEnemyAppear(mPosition, "テレサマネージャー", 1));
		if (telesa != nullptr)
			telesa->initAttacker(this);
		return;
	}

	int num       = mBossParams->mSLNumGenBubble.get();
	MtxPtr mouth  = mMActor->getModel()->getAnmMtx(5);
	f32 angleStep = 180.0f / num;
	f32 angleHalf = angleStep * num * 0.5f;

	for (int i = 0; i < num; ++i) {
		TBubble* bubble = static_cast<TBubble*>(
		    gpConductor->makeOneEnemyAppear(mPosition, "バブルマネージャー", 1));
		if (bubble == nullptr)
			break;

		JGeometry::TVec3<f32> dir(0.0f, 0.0f, -50.0f);
		Mtx rot;
		MsMtxSetRotRPH(rot, mRotation.x,
		               angleStep * i + (mRotation.y - angleHalf), mRotation.z);
		MTXMultVec(rot, &dir, &dir);
		MsVECNormalize(&dir, &dir);
		dir.y            = 2.0f;
		f32 firstSpeed   = mBossParams->mSL1stBubbleSp.get();
		dir.x           *= firstSpeed;
		dir.z           *= firstSpeed;

		bubble->mPosition.set(mouth[0][3], mouth[1][3] - 50.0f, mouth[2][3]);
		bubble->mVelocity = dir;
		bubble->mPosition.y += 10.0f;
		bubble->onLiveFlag(LIVE_FLAG_AIRBORNE);

		TMsRange<f32> range(0.0f, 1.0f);
		char trash[0x10];
		if (range.rand() < mItemGenRate) {
			bubble->mAppended = nullptr;
			TMapObjBase* item = gpItemManager->makeObjAppear(
			    bubble->mPosition.x, bubble->mPosition.y, bubble->mPosition.z,
			    0x20000008, true);
			if (item != nullptr && item->receiveMessage(bubble, HIT_MESSAGE_TAKE)) {
				item->appear();
				item->mPosition = bubble->mPosition;
				item->setVelocityAndFlag10(0.0f, 15.0f, 0.0f);
				bubble->mHeldObject = item;
				bubble->mAppended   = static_cast<TSpineEnemy*>(static_cast<TLiveActor*>(item));
			}
		} else if (range.rand() < mEnemyGenRate) {
			bubble->appendEnemy();
		}
	}
}

void TBossTelesa::rouletteStart()
{
	for (int i = 0; i < 3; ++i) {
		if (mRoulettes[i]->unk13C != 0.0f)
			(void)mRoulettes[i]->unk13C;
	}

	TMsRange<f32> speedRange(0.05f, 0.1f);
	TMsRange<f32> dirRange(-1.0f, 1.0f);
	char trash[0x14];
	f32 randDir = dirRange.rand();
	u8 maxHitPoints = getSaveParam() ? getSaveParam()->mSLHitPointMax.get() : 1;
	f32 upSpeed     = mRouletteUpRate * (maxHitPoints - getHitPoints());

	for (int i = 0; i < 3; ++i) {
		f32 dir;
		if (randDir > 0.0f) {
			dir = -1.0f;
			if (i == 0 || i == 2)
				dir = 1.0f;
		} else {
			dir = 1.0f;
			if (i == 0 || i == 2)
				dir = -1.0f;
		}
		mRoulettes[i]->unk144 = dir * (upSpeed + speedRange.rand());
		mSlot->unk1E4[i]      = dir * (upSpeed + speedRange.rand());
	}

	for (int i = 0; i < 3; ++i)
		mRoulettes[i]->setRollSp(mSlot->unk1E4[i]);

	SMSRumbleMgr->start(0x14, 0xF, static_cast<f32*>(nullptr));
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x23), 1.0f);
}

void TBossTelesa::generateSlotItem()
{
	static const f32 xzTable[8]
	    = { 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f };
	static const char* manNameTable[] = {
		"バブルマネージャー",         "ハムクリマネージャー",
		"ヤキグリマネージャー",       "ボム兵マネージャー",
		"ポイハナマネージャー",       "電気ノコノコマネージャー",
		"ポポマネージャー",           "ゲッソーマネージャー",
		"とびプクマネージャー",
	};

	mSlotTimer = 0;
	mItemNum   = 0;

	TTelesaSlot* slot = mSlot;
	int result        = slot->getResultFromAng(slot->unk13C[0]);
	for (int i = 1; i < 3; ++i) {
		if (result != slot->getResultFromAng(slot->unk13C[i])) {
			result = -1;
			break;
		}
	}
	mSlotResult = result;

	int itemNum   = mBossParams->mSLSlotItemNum.get();
	MtxPtr mouth  = mMActor->getModel()->getAnmMtx(5);
	f32 angleStep = 120.0f / itemNum;
	f32 angleHalf = angleStep * itemNum * 0.5f;

	if (mSlotResult == 2) {
		int fruitNum = mBossParams->mSLSlotFruitNum.get();
		if (fruitNum > 20)
			fruitNum = 20;

		int start = TMsRange<s32>(0, fruitNum).rand();
		for (int i = 0; i < fruitNum; ++i) {
			TMapObjBase** item = &mFruits[i];
			if ((*item)->mHolder != nullptr)
				continue;

			Mtx rot;
			JGeometry::TVec3<f32> offset(0.0f, 0.0f, 200.0f);
			MsMtxSetRotRPH(rot, mRotation.x,
			               160.0f / fruitNum * start
			                   + (mRotation.y - angleHalf),
			               mRotation.z);
			start = MsWrap(start + i, 0, fruitNum);
			MTXMultVec(rot, &offset, &offset);

			JGeometry::TVec3<f32> dir;
			MsVECNormalize(&offset, &dir);

			TMsRange<f32> speed(6.0f, 10.0f);
			if (i == 0 || i == 4) {
				item = &mChilis[i];
				(*item)->makeObjAppeared();
				(*item)->offLiveFlag(LIVE_FLAG_HIDDEN);
				(*item)->setVelocityAndFlag10(dir.x * speed.rand(), -2.0f,
				                              dir.z * speed.rand());
				if (i == 0) {
					(*item)->setVelocityAndFlag10(2.0f * (dir.x * speed.rand()),
					                              -2.0f,
					                              2.0f * (dir.z * speed.rand()));
				}
				(*item)->mRotation.set(0.0f, 90.0f, 0.0f);
				mItems[mItemNum] = *item;
			} else {
				(*item)->makeObjAppeared();
				(*item)->offLiveFlag(LIVE_FLAG_HIDDEN);
				(*item)->setVelocityAndFlag10(dir.x * speed.rand(), -2.0f,
				                              dir.z * speed.rand());
				mItems[mItemNum] = *item;
			}

			mItems[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
			mItems[i]->mScaling.setAll(1.5f);
			mItems[mItemNum]->mPosition.set(mouth[0][3] + offset.x,
			                                mouth[1][3] - 50.0f,
			                                mouth[2][3] + offset.z);
			++mItemNum;
		}
	} else if (mSlotResult == 0) {
		int coinNum = mBossParams->mSLSlotFruitNum.get();
		if (coinNum > 10)
			coinNum = 10;

		f32 step = 120.0f / coinNum;
		f32 half = step * coinNum * 0.5f;

		if (unk370 != 0)
			--unk370;
		else
			unk370 = 0;

		for (int i = 0; i < coinNum; ++i) {
			if (i >= 10)
				break;

			Mtx rot;
			JGeometry::TVec3<f32> dir(0.0f, 0.0f, 250.0f);
			MsMtxSetRotRPH(rot, mRotation.x, step * i + (mRotation.y - half),
			               mRotation.z);
			MTXMultVec(rot, &dir, &dir);
			MsVECNormalize(&dir, &dir);

			TMsRange<f32> speed(0.8f, 3.5f);
			dir.y = 10.0f;
			f32 firstSpeed = mBossParams->mSL1stBubbleSp.get();
			dir.x *= firstSpeed * speed.rand();
			dir.z *= firstSpeed * speed.rand();

			TCoin* coin = static_cast<TCoin*>(
			    gpItemManager->makeObjAppeared(0x2000000E));
			coin->mPosition.set(mouth[0][3], mouth[1][3] - 250.0f,
			                    mouth[2][3]);
			coin->setVelocityAndFlag10(dir.x, dir.y, dir.z);
			coin->mRotation.setAll(0.0f);
			coin->killByTimer(960);

			mItems[mItemNum] = mCoins[i];
			mItems[mItemNum]->offLiveFlag(LIVE_FLAG_HIDDEN);
			++mItemNum;
		}
	} else {
		int num = itemNum;
		int man = 0;
		switch (mSlotResult) {
		case -1:
			num *= 2;
			man = 0;
			break;
		case 1:
			if (mHitPoints > 2)
				man = 1;
			else
				man = 2;
			break;
		case 3:
			break;
		}

		s32 lo = 1;
		s32 hi = 7;
		if (mHitPoints == 1) {
			lo = 1;
			hi = 8;
		}
		TMsRange<s32> range(lo, hi);
		int idx = range.rand();

		for (int i = 0; i < num; ++i) {
			if (mSlotResult == 3) {
				if (i % 2 == 0)
					++idx;
				if (idx > hi)
					idx = 1;
				man = idx;
			}

			TSmallEnemy* enemy = static_cast<TSmallEnemy*>(
			    gpConductor->makeOneEnemyAppear(mPosition, manNameTable[man],
			                                    2));
			if (enemy == nullptr)
				continue;

			if (man != 0) {
				mItems[mItemNum] = (TMapObjBase*)enemy;
				++mItemNum;
			}

			Mtx rot;
			JGeometry::TVec3<f32> dir(0.0f, 0.0f, 200.0f);
			MsMtxSetRotRPH(rot, mRotation.x,
			               angleStep * i + (mRotation.y - angleHalf),
			               mRotation.z);
			MTXMultVec(rot, &dir, &dir);
			MsVECNormalize(&dir, &dir);
			dir.y = 2.0f;

			f32 firstSpeed = mBossParams->mSL1stBubbleSp.get();
			TMsRange<f32> speed(0.5f, 1.0f);
			dir.x *= firstSpeed * speed.rand();
			dir.y *= 2.0f + speed.rand();
			dir.z *= firstSpeed * speed.rand();

			enemy->mPosition.set(mouth[0][3], mouth[1][3] - 250.0f,
			                     mouth[2][3]);
			enemy->mVelocity = dir;
			enemy->mPosition.y += 10.0f;
			enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
			enemy->getMActor()->getModel()->setBaseTRMtx(mouth);
			enemy->getMActor()->calc();
			enemy->initAttacker(this);
		}
	}
}

// Called (not inlined) from several nerves in the original.
#pragma dont_inline on
void TBossTelesa::forceAllItemKill()
{
	for (int i = 0; i < mItemNum; ++i) {
		if (mItems[i]->mHolder != nullptr) {
			SMS_SendMessageToMario(mItems[i], HIT_MESSAGE_UNK8);
			mItems[i]->mHolder = nullptr;
		}

		mItems[i]->mPosition.set(0.0f, 0.0f, 0.0f);
		mItems[i]->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (!mItems[i]->checkLiveFlag(LIVE_FLAG_DEAD)) {
			mItems[i]->kill();
			gpMarioParticleManager->emit(PARTICLE_MS_TLS_CHANGE,
			                             &mItems[i]->mPosition, 0, nullptr);
		}
	}
}
#pragma dont_inline off

// Called (not inlined) from TBossTelesaKillSmallEnemy::checkHit in the original.
#pragma dont_inline on
void TBossTelesa::forceHide()
{
	if (mSpine->getCurrentNerve() != &TNerveBossTelesaDie::theNerve()) {
		if (!mMActor->checkCurBckFromIndex(4)
		    && !mMActor->checkCurBckFromIndex(0)) {
			forceAllItemKill();
			mSlotTimer = 0;

			if (mIsSpicy) {
				SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_ESCAPE,
				                                &mPosition, 0, nullptr, 0, 4);
			} else {
				SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_DISAPPEAR,
				                                &mPosition, 0, nullptr, 0, 4);
			}

			mSpine->reset();
			mSpine->setNext(&TNerveBossTelesaHide::theNerve());
		}
	}
}
#pragma dont_inline off

DEFINE_NERVE(TNerveBossTelesaDie, TLiveActor)
{
	TBossTelesa* self = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0) {
		self->mDieTimer = 0;
		if (self->mIsSpicy)
			self->decHitPoints();
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mBody->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mTongue->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (self->getHitPoints() != 0) {
			if (self->mIsSpicy) {
				self->mMActor->setBrkFromIndex(1);
				setBossTelesaBck(self, 2);
				gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x1F),
				                          1.0f);
			} else {
				setBossTelesaBck(self, 5);
				self->mMActor->setBrkFromIndex(0);
				gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x20),
				                          1.0f);
			}
		} else {
			MSBgm::stopBGM(MSD_BGM_MAP_SELECT, 10);
			gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x21),
			                          1.0f);
			setBossTelesaBck(self, 3);
			self->mMActor->setBrkFromIndex(1);
			self->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
			SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_DOWN,
			                                &self->mPosition, 0, nullptr, 0, 4);
		}
	}

	if (self->getHitPoints() == 0) {
		if (self->checkCurAnmEnd(0)) {
			if (self->mDieTimer == 0) {
				MtxPtr mtx = self->mMActor->getModel()->getAnmMtx(1);
				self->mHitPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);
				gpMarioParticleManager->emit(0xDC, &self->mHitPos, 0, nullptr);
				if (self->mHitParticleId == 0xD8)
					gpMarioParticleManager->emit(0xDD, &self->mHitPos, 0,
					                             nullptr);
				else if (self->mHitParticleId == 0xD9)
					gpMarioParticleManager->emit(0xDE, &self->mHitPos, 0,
					                             nullptr);
				else
					gpMarioParticleManager->emit(0xDF, &self->mHitPos, 0,
					                             nullptr);
				self->forceAllItemKill();
				for (int i = 0; i < 3; ++i) {
					self->mRoulettes[i]->unk144 = 0.0f;
					self->mSlot->unk1E4[i]      = 0.0f;
				}
				for (int i = 0; i < 3; ++i)
					self->mRoulettes[i]->setRollSp(self->mSlot->unk1E4[i]);
			}

			if (self->mDieTimer > 240) {
				self->mDieTimer = 0;
				gpItemManager->makeShineAppearWithDemo(
				    "シャイン（ボス用）", "ボスシャインカメラ",
				    self->mPosition.x, self->mPosition.y, self->mPosition.z);
				self->onLiveFlag(LIVE_FLAG_DEAD);
				self->onLiveFlag(LIVE_FLAG_UNK8);
				self->offLiveFlag(LIVE_FLAG_UNK10000);
				self->mHolder = nullptr;
				self->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->mBody->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->mTongue->onHitFlag(HIT_FLAG_NO_COLLISION);
				self->stopAnmSound();
				spine->reset();
				return TRUE;
			}
			self->mDieTimer += 1;
		}
	} else if (self->checkCurAnmEnd(0)) {
		if (self->mMActor->checkCurBckFromIndex(5)) {
			self->mMActor->setBrkFromIndex(2);
			setBossTelesaBck(self, 7);
		} else if (self->mMActor->checkCurBckFromIndex(7)) {
			setBossTelesaBck(self, 6);
		} else {
			SMS_ResetDamageFogEffect(self->mMActor->getModel()->getModelData());
			if (self->mMActor->checkCurBckFromIndex(6)) {
				self->offHitFlag(HIT_FLAG_NO_COLLISION);
				self->mBody->offHitFlag(HIT_FLAG_NO_COLLISION);
				self->mTongue->offHitFlag(HIT_FLAG_NO_COLLISION);
				setBossTelesaBck(self, 15);
				self->mMActor->setBtpFromIndex(2);
				spine->reset();
				spine->setNext(&TNerveBossTelesaPrepareSlot::theNerve());
				spine->pushAfterCurrent(&TNerveBossTelesaPrepareSlot::theNerve());
			} else {
				self->damageRecover();
			}
			return TRUE;
		}
	}
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSpit, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0 || !boss->mMActor->checkCurBckFromIndex(14)) {
		setBossTelesaBck(boss, 14);
	} else if (boss->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(40.0f)) {
		boss->genAttacker();
	}

	if (boss->checkCurAnmEnd(0))
		return true;
	return false;
}

DEFINE_NERVE(TNerveBossTelesaHide, TLiveActor)
{
	char trash[8];
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (!boss->mMActor->checkCurBckFromIndex(4)) {
		setBossTelesaBck(boss, 4);
		boss->mMActor->setBtpFromIndex(2);
	}

	if (boss->checkCurAnmEnd(0)) {
		boss->onHitFlag(HIT_FLAG_NO_COLLISION);
		boss->mBody->onHitFlag(HIT_FLAG_NO_COLLISION);
		boss->mTongue->onHitFlag(HIT_FLAG_NO_COLLISION);
		SMSRumbleMgr->start(20, 15, (f32*)nullptr);
		boss->rouletteStart();
		spine->pushAfterCurrent(&TNerveBossTelesaHideWait::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossTelesaHideWait, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0) {
		boss->onLiveFlag(LIVE_FLAG_HIDDEN);
		boss->mIsSpicy = false;
		u8 maxHp         = boss->getMaxHitPoints();
		boss->mKColor0.a = clampBossTelesa<u8>(
		    TBossTelesa::mNormalAlpha
		        + (maxHp - boss->getHitPoints()) * 30,
		    0, 254);
		boss->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
		boss->mMActor->setBrkFromIndex(2);
		boss->mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(
		    boss->mMActor->getFrameCtrl(ANM_TYPE_BRK)->getEnd());
	} else {
		JGeometry::TVec3<f32> toMario(boss->mPosition - *gpMarioPos);
		if (spine->getTime() > 400 && boss->mKillSmallEnemy->unk6C == 0) {
			spine->pushAfterCurrent(&TNerveBossTelesaAppear::theNerve());
			boss->offLiveFlag(LIVE_FLAG_HIDDEN);
			return true;
		}
	}
	return false;
}

DEFINE_NERVE(TNerveBossTelesaAppear, TLiveActor)
{
	TBossTelesa* self = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0 && !self->mMActor->checkCurBckFromIndex(0)) {
		setBossTelesaBck(self, 0);
		if (!self->mBgmStarted) {
			self->mBgmStarted = true;
			MSBgm::startBGM(MSD_BGM_MAP_SELECT);
		}
		self->mSlot->mScaling.set(1.0f, 1.0f, 1.0f);
		self->mSlot->randomReset();
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mBody->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mTongue->offHitFlag(HIT_FLAG_NO_COLLISION);
	} else if (self->checkCurAnmEnd(0)) {
		if (!self->mMActor->checkCurBckFromIndex(15)) {
			setBossTelesaBck(self, 15);
			self->mMActor->setBtpFromIndex(2);
		}
	}

	if (self->mMActor->checkCurBckFromIndex(0)) {
		if (self->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(40.0f)) {
			gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0x22),
			                          1.0f);
			SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_SLT_LAND,
			                                &self->mPosition, 0, nullptr, 0, 4);
		}
	}

	if (spine->getTime() > 800) {
		u8 maxHp     = self->getMaxHitPoints();
		s32 interval = TBossTelesa::mTelesaGenerateInterval
		               + (maxHp - self->getHitPoints()) * 100;
		if (spine->getTime() % interval == 1)
			setBossTelesaBck(self, 14);
	}

	self->mHoseiPosY *= 0.9f;
	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaSlotStart, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0)
		setBossTelesaBck(boss, 11);

	if (boss->mMActor->checkCurBckFromIndex(11)) {
		if (boss->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(53.0f)) {
			boss->unk18C = 1;
			boss->mSlot->moveStart();
			boss->mTelesaManager->telesaForceKill();
		}
		if (boss->checkCurAnmEnd(0)) {
			setBossTelesaBck(boss, 15);
			boss->mMActor->setBtpFromIndex(2);
			boss->mSlot->forceStopSlot(1);
		}
	}

	if (isSlotStopped(boss))
		return true;

	boss->mHoseiPosY *= 0.99f;
	return false;
}

DEFINE_NERVE(TNerveBossTelesaSpitSlotItem, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (!boss->mMActor->checkCurBckFromIndex(14)
	    && boss->mHoseiPosY < TBossTelesa::mBaseHoseiPosY - 300.0f) {
		setBossTelesaBck(boss, 14);
		return false;
	}

	if (boss->checkCurAnmEnd(0) && spine->getTime() > 600) {
		spine->pushAfterCurrent(&TNerveBossTelesaPrepareSlot::theNerve());
		boss->mSlotTimer = 0;
		for (int i = 0; i < boss->mItemNum; ++i) {
			TMapObjBase* item = boss->mItems[i];
			if (!item->checkLiveFlag(LIVE_FLAG_DEAD)
			    && !item->isActorType(0x2000000E)
			    && !item->isActorType(0x20000002))
				item->offHitFlag(HIT_FLAG_NO_COLLISION);
		}
		return true;
	}

	if (spine->getTime() > 200)
		boss->mHoseiPosY -= 2.0f;
	return false;
}

DEFINE_NERVE(TNerveBossTelesaPrepareSlot, TLiveActor)
{
	TBossTelesa* self = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0) {
		setBossTelesaBck(self, 15);
		self->mMActor->setBtpFromIndex(2);
	}

	if (self->mIsSpicy) {
		self->mSpicyTimer += 1;
		if (self->checkCurAnmEnd(0)) {
			if (self->mMActor->checkCurBckFromIndex(1)) {
				setBossTelesaBck(self, 12);
				self->mMActor->setBtpFromIndex(1);
			} else if (self->mMActor->checkCurBckFromIndex(12)) {
				if (self->mSpicyTimer > self->mBossParams->mSLSpicyTime.get())
					setBossTelesaBck(self, 13);
			} else {
				self->mSpicyTimer = 0;
				self->mIsSpicy    = false;
				u8 maxHp         = self->getMaxHitPoints();
				self->mKColor0.a = clampBossTelesa<u8>(
				    TBossTelesa::mNormalAlpha
				        + (maxHp - self->getHitPoints()) * 30,
				    0, 254);
				setBossTelesaBck(self, 15);
				self->mMActor->setBtpFromIndex(2);
			}
		}
	}

	self->mSlotTimer += 1;

	s32 stopTime = self->mBossParams->mSLStopSlotTime0.get();
	if (self->getHitPoints() == 2)
		stopTime = self->mBossParams->mSLStopSlotTime1.get();
	if (self->getHitPoints() == 1)
		stopTime = self->mBossParams->mSLStopSlotTime2.get();

	bool noResult = self->mSlot->getSlotResult() == 0 ? true : false;
	if (noResult)
		stopTime = (s32)(stopTime * 0.5f);

	if (self->mSlotTimer > stopTime - 120)
		self->flashItem(stopTime - self->mSlotTimer);

	if (self->mMActor->checkCurBckFromIndex(15)) {
		if (isAllSlotItemDead(self) || self->mSlotTimer > stopTime) {
			self->mSlotTimer = 0;
			self->forceAllItemKill();
			if (self->mIsSpicy) {
				SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_ESCAPE,
				                                &self->mPosition, 0, nullptr, 0,
				                                4);
			} else {
				SMSGetMSound()->startSoundActor(MSD_SE_BS_TELESA_DISAPPEAR,
				                                &self->mPosition, 0, nullptr, 0,
				                                4);
			}
			spine->reset();
			spine->setNext(&TNerveBossTelesaHide::theNerve());
			spine->pushAfterCurrent(&TNerveBossTelesaHide::theNerve());
			return TRUE;
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveBossTelesaFreeze, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (boss->mMActor->checkCurBckFromIndex(16)) {
		if (boss->checkCurAnmEnd(0)) {
			boss->mIsSpicy = false;
			u8 maxHp = boss->getMaxHitPoints();
			boss->mKColor0.a = clampBossTelesa<u8>(
			    TBossTelesa::mNormalAlpha
			        + (maxHp - boss->getHitPoints()) * 30,
			    0, 254);
			return true;
		}
	} else {
		setBossTelesaBck(boss, 16);
		if (gpMSound->gateCheck(MSD_SE_BS_TELESA_THANKYOU)) {
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_BS_TELESA_THANKYOU, &boss->mPosition, 0, nullptr, 0, 4);
		}
	}
	return false;
}

DEFINE_NERVE(TNerveBossTelesaFallDemo, TLiveActor)
{
	TBossTelesa* boss = static_cast<TBossTelesa*>(spine->getBody());

	if (spine->getTime() == 0) {
		boss->onLiveFlag(LIVE_FLAG_HIDDEN);
		if (SMS_SendMessageToMario(boss, 4))
			boss->mHeldObject = (TTakeActor*)SMS_GetMarioHitActor();
		boss->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);
		boss->mSlot->mScaling.set(0.0f, 0.0f, 0.0f);
	}

	if (boss->rouletteFall()) {
		JGeometry::TVec3<f32> toMario = boss->mPosition - *gpMarioPos;
		if (boss->slotFall()) {
			boss->offHitFlag(HIT_FLAG_NO_COLLISION);
			boss->mBody->offHitFlag(HIT_FLAG_NO_COLLISION);
			boss->mTongue->offHitFlag(HIT_FLAG_NO_COLLISION);
			spine->reset();
			spine->setNext(&TNerveBossTelesaHideWait::theNerve());
			spine->pushAfterCurrent(&TNerveBossTelesaHideWait::theNerve());
			return true;
		}
	}
	return false;
}
