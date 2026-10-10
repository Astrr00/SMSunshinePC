#include <Enemy/BombHei.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Camera/CameraShake.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/MapData.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// The nerves are only used in this file, and their theNerve() is defined
// in-class so that every use is inlined (no out-of-line copy).
#define BOMBHEI_NERVE(Name)                                                    \
	class Name : public TNerveBase<TLiveActor> {                               \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   \
		static const Name& theNerve()                                          \
		{                                                                      \
			static Name instance;                                              \
			return instance;                                                   \
		}                                                                      \
	};

BOMBHEI_NERVE(TNerveBombHeiGenerate)
BOMBHEI_NERVE(TNerveBombHeiAttack)
BOMBHEI_NERVE(TNerveBombHeiWalkExplosion)
BOMBHEI_NERVE(TNerveBombHeiWaitExplosion)
BOMBHEI_NERVE(TNerveBombHeiPickUp)
BOMBHEI_NERVE(TNerveBombHeiThrown)
BOMBHEI_NERVE(TNerveBombHeiExplosion)

bool TBombHei::mSerialBomb = true;

static const char* bombhei_bastable[] = {
	"/scene/bombhei/bas/downnejibomb_down1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_land1.bas",
	nullptr,
	nullptr,
	"/scene/bombhei/bas/nejibomb_stop_down1.bas",
};

// NOTE: with -inline deferred MWCC emits functions (and their rodata/data) in
// reverse definition order, so everything below is listed in the reverse of
// the .text order.

TBombHeiManager::TBombHeiManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(0)
{
}

void TBombHeiManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TBombHeiSaveLoadParams("/enemy/bombhei.prm");
}

void TBombHeiManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "nejibomb_model1.bmd", 0x10230000, 0 },
		{ "downnejibomb_model1.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

TSpineEnemy* TBombHeiManager::createEnemyInstance() { return new TBombHei; }

TBombHei::TBombHei(const char* name)
    : TWalkerEnemy(name)
    , unk194(nullptr)
    , unk198(0)
    , unk19C(1)
    , unk1A4(0)
{
}

void TBombHei::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);

	mActorType = 0x1000001E;
	unk150     = 0x11;
	unk194     = (TBombHeiSaveLoadParams*)getSaveParam();

	mSpine->initWith(&TNerveBombHeiGenerate::theNerve());

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); ++i)
			; // assert?
	}
}

void TBombHei::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("nejibomb_model1.bmd", 0);
	mMActorKeeper->createMActor("downnejibomb_model1.bmd", 3);
}

void TBombHei::behaveToWater(THitActor* param_1)
{
	if (isBckAnm(4) || isBckAnm(3)) {
		if (mHitPoints == 0)
			mSpine->pushNerve(&TNerveBombHeiWaitExplosion::theNerve());

		mSprayedByWaterCooldown = 20;
	}
}

void TBombHei::changeOut()
{
	SMSGetMSound()->startSoundActor(MSD_SE_EN_TELSA_RECOVER, &mPosition, 0,
	                                nullptr, 0, 4);

	onLiveFlag(LIVE_FLAG_DEAD);
	genEventCoin();
	onHitFlag(HIT_FLAG_NO_COLLISION);

	mPosition = mJuiceBlock->mPosition;

	gpMarioParticleManager->emitAndBindToPosPtr(0xCD, &mPosition, 0, nullptr);
	getMActor()->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
	mJuiceBlock->kill();
	mJuiceBlock = nullptr;
}

bool TBombHei::isHitValid(u32 param_1)
{
	if (param_1 == 0xB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		genEventCoin();
		return false;
	}

	return false;
}

void TBombHei::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		mHitPoints = 1;
		if (mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveBombHeiExplosion::theNerve());
			mSpine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		}
		onLiveFlag(LIVE_FLAG_UNK40);
	}
}

void TBombHei::genEventCoin()
{
	TBombHeiManager* manager = (TBombHeiManager*)mManager;
	if (unk1A4 && manager->tryCountCoin()) {
		TCoin* coin = (TCoin*)gpItemManager->makeObjAppear(
		    mPosition.x, mPosition.y, mPosition.z, 0x2000000E, true);
		if (coin) {
			coin->mPosition.y = mPosition.y;

			JGeometry::TVec3<f32> dir = *gpMarioPos - mPosition;
			MsVECNormalize(&dir, &dir);
			coin->mVelocity.set(20.0f * dir.x, 20.0f, 20.0f * dir.z);
			coin->offLiveFlag(LIVE_FLAG_UNK10);
		}
	}
}

void TBombHei::setWalkAnm() { setBckAnm(4); }

void TBombHei::setFreezeAnm() { setBckAnm(1); }

void TBombHei::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("downnejibomb_model1.bmd");
	mRotation.y = TMsRange<f32>(0.0f, 360.0f).rand();
	unk19C      = 0;
	setBckAnm(0);
	gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	SMSRumbleMgr->start(0x15, 5, (f32*)nullptr);
}

void TBombHei::calcRootMatrix()
{
	TSpineEnemy::calcRootMatrix();

	if (gpMarDirector->checkUnk4CFlag(0xF)) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (isBckAnm(0)) {
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(2.0f)) {
			TEffectExplosion* effect
			    = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
			        mPosition, "エフェクト爆発マネージャー", 1);
			if (effect)
				effect->generate(mPosition, mScaling);
		}
	}
}

void TBombHei::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve())
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
}

void TBombHei::behaveToTaken(THitActor* param_1)
{
	if (mSpine->getCurrentNerve() != &TNerveBombHeiPickUp::theNerve()) {
		if (param_1->isActorType(0x80000001))
			unk1A4 = 1;
		mSpine->pushNerve(&TNerveBombHeiPickUp::theNerve());
	}
}

void TBombHei::behaveToRelease()
{
	if (unk164 != 0
	    || mSpine->getCurrentNerve() == &TNerveBombHeiWalkExplosion::theNerve()) {
		if (mSpine->getCurrentNerve() != &TNerveBombHeiThrown::theNerve())
			mSpine->pushNerve(&TNerveBombHeiThrown::theNerve());
	}
}

void TBombHei::reset()
{
	TWalkerEnemy::reset();
	unk198 = 0;
	unk1A4 = 0;
	unk164 = 0;
	unk19C = 1;
	mMActor = getActorKeeper()->getMActor("nejibomb_model1.bmd");
}

f32 TBombHei::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve())
		return unk194->mSLThrownGravityY.get();

	return mGravity;
}

void TBombHei::walkBehavior(int param_1, f32 param_2)
{
	SMSGetMSound()->startSoundActor(MSD_SE_EN_BOMBHEI_ZENMAI, &mPosition, 0,
	                                nullptr, 0, 4);
	TWalkerEnemy::walkBehavior(param_1, param_2);
}

void TBombHei::moveObject()
{
	TWalkerEnemy::moveObject();

	if (mSpine->getCurrentNerve() != &TNerveBombHeiThrown::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveBombHeiGenerate::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveSmallEnemyChange::theNerve()) {
		unk198 += 1;
		if (unk198 > unk194->mSLBombTime.get()) {
			unk164 = 0;
			unk198 = 0;
			if (mSpine->getCurrentNerve()
			    != &TNerveBombHeiWaitExplosion::theNerve()) {
				mSpine->pushNerve(&TNerveBombHeiWalkExplosion::theNerve());
			}
		}
	}
}

static inline bool isExploding(TBombHei* hei)
{
	return hei->mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve()
	           ? true
	           : false;
}

bool TBombHei::isCollidMove(THitActor* other)
{
	if (mSerialBomb) {
		if (other->isActorType(0x1000001E)) {
			TBombHei* hei = (TBombHei*)other;
			if (isExploding(hei)) {
				if (mSpine->getCurrentNerve()
				    != &TNerveBombHeiExplosion::theNerve()) {
					mSpine->pushNerve(&TNerveBombHeiExplosion::theNerve());
				}
			}
		}
	}

	if (other->isActorType(0x08000013)) {
		if (mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve())
			other->receiveMessage(this, 0);

		if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve())
			mSpine->pushNerve(&TNerveBombHeiExplosion::theNerve());
	}

	return true;
}

void TBombHei::forceKill()
{
	if (!(mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL)
	      || (!mGroundPlane->isDeathPlane() && !mGroundPlane->isPool()
	          && !mGroundPlane->isWaterSurface())
	      || isAirborne() || checkLiveFlag(LIVE_FLAG_UNK10))) {
		if (mSpine->getCurrentNerve() != &TNerveBombHeiExplosion::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveBombHeiExplosion::theNerve());
			mSpine->pushAfterCurrent(mSpine->getDefault());

			onLiveFlag(LIVE_FLAG_UNK20000);
			mHitPoints = 1;
		}
	}
}

bool TBombHei::isDamageToCannon()
{
	if (mSpine->getCurrentNerve() == &TNerveBombHeiThrown::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveBombHeiExplosion::theNerve()) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return true;
	}

	return false;
}

const char** TBombHei::getBasNameTable() const { return bombhei_bastable; }

BOOL TNerveBombHeiGenerate::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor = self->getActorKeeper()->getMActor("nejibomb_model1.bmd");
		self->setBckAnm(2);
		self->mMActor->setBtpFromIndex(1);
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	}

	if (self->mHolder != nullptr)
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);

	if (!self->isAirborne() && self->mHolder == nullptr) {
		if (self->isBckAnm(2)) {
			self->setBckAnm(3);
		} else if (self->checkCurAnmEnd(0)) {
			spine->pushAfterCurrent(&TNerveBombHeiAttack::theNerve());
			return true;
		}
	} else if (!self->isBckAnm(2)) {
		self->mMActor = self->getActorKeeper()->getMActor("nejibomb_model1.bmd");
		self->setBckAnm(2);
		self->mMActor->setBtpFromIndex(1);
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	}

	return false;
}

BOOL TNerveBombHeiAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setWalkAnm();
		self->unk164 = 0;
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

BOOL TNerveBombHeiWalkExplosion::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(5);
		self->mMActor->setBtpFromIndex(0);
	} else if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		return true;
	}

	int frame = (int)self->mMActor->getFrameCtrl(ANM_TYPE_BTP)->getFrame();
	if (frame % 40 == 0) {
		SMSGetMSound()->startSoundActor(MSD_SE_EN_BOMBHEI_COUNT,
		                                &self->mPosition, 0, nullptr, 0, 4);
	}

	self->walkBehavior(2, 0.6f);

	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x17F, self->mMActor->getModel()->getAnmMtx(1), 1, self);
	return false;
}

BOOL TNerveBombHeiWaitExplosion::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(6);
		self->unk164 = 1;
	}

	if (self->unk164) {
		if (self->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(60.0f))
			self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);

		if (self->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(10.0f)) {
			MActor* actor = self->mMActor;
			actor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BTP);
			if (self->getCurAnmFrameNo(3) >= 1.0f)
				self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
		}
	} else {
		MActor* actor = self->mMActor;
		actor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
		if (!self->mMActor->checkCurAnmFromIndex(0, ANM_TYPE_BTP))
			self->mMActor->setBtpFromIndex(0);

		if (self->checkCurAnmEnd(0) && spine->getTime() > 150) {
			spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
			return true;
		}

		int frame
		    = (int)self->mMActor->getFrameCtrl(ANM_TYPE_BTP)->getFrame();
		if (frame % 40 == 0) {
			SMSGetMSound()->startSoundActor(MSD_SE_EN_BOMBHEI_COUNT,
			                                &self->mPosition, 0, nullptr, 0, 4);
		}

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x17F, self->mMActor->getModel()->getAnmMtx(1), 1, self);
	}

	return false;
}

BOOL TNerveBombHeiPickUp::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0 && self->unk164 == 0)
		return true;

	return false;
}

BOOL TNerveBombHeiThrown::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		TBombHeiSaveLoadParams* params = self->getBombHeiParams();

		f32 power = SMS_GetMarioThrowPower();
		f32 rate  = params->mSLThrownRateXZ.get();
		JGeometry::TVec3<f32> velocity;
		f32 c = JMASCos(SMS_GetMarioAngleY());
		f32 s = JMASSin(SMS_GetMarioAngleY());
		velocity.x = rate * (power * s);
		velocity.y = params->mSLThrownVY.get();
		velocity.z = rate * (power * c);
		self->setVelocity(velocity);

		self->mPosition.y += 2.0f;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	if (spine->getTime() == 120)
		self->offHitFlag(HIT_FLAG_NO_COLLISION);

	if (!self->isAirborne()) {
		self->genEventCoin();
		spine->pushAfterCurrent(&TNerveBombHeiExplosion::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveBombHeiExplosion::execute(TSpineBase<TLiveActor>* spine) const
{
	TBombHei* self = (TBombHei*)spine->getBody();

	if (spine->getTime() == 0) {
		self->unk1A0 = self->getBombHeiParams()->mSLBombRange.get()
		               * self->getBodyScale() / self->getAttackRadius();
		self->setDeadAnm();
		self->onLiveFlag(LIVE_FLAG_UNK8);
		if (self->mHolder == gpMarioAddress)
			self->sendAttackMsgToMario();
		self->unk164 = 0;

		if (self->mGroundPlane->isWaterSurface()) {
			if (TEffectBombColumWater* effect
			    = (TEffectBombColumWater*)gpConductor->makeOneEnemyAppear(
			        self->mPosition, "エフェクト爆発水柱マネージャー", 1)) {
				JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
				effect->generate(self->mPosition, scale);
			}
		}

		if (self->mGroundPlane->isSand()) {
			if (TEffectColumSand* effect
			    = (TEffectColumSand*)gpConductor->makeOneEnemyAppear(
			        self->mPosition, "エフェクト砂柱マネージャー", 1)) {
				JGeometry::TVec3<f32> scale(0.7f, 0.7f, 0.7f);
				effect->generate(self->mPosition, scale);
			}
		}
	}

	if (self->unk190 < self->unk1A0) {
		self->unk190 *= 1.2f;
	} else if (self->checkCurAnmEnd(0)) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();

		spine->reset();
		spine->setDefaultNext();
		spine->pushAfterCurrent(spine->getDefault());
		return true;
	}

	self->expandCollision();
	return false;
}
