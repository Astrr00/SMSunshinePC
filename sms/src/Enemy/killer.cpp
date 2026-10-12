#include <Enemy/Killer.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Camera/CameraShake.hpp>
#include <Player/MarioAccess.hpp>
#include <M3DUtil/MActor.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MoveBG/Item.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>

#define KILLER_NERVE(Name)                                                     \
	class Name : public TNerveBase<TLiveActor> {                               \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   \
		static const Name& theNerve();                                         \
	};                                                                         \
	const Name& Name::theNerve()                                               \
	{                                                                          \
		static Name instance;                                                  \
		return instance;                                                       \
	}

KILLER_NERVE(TNerveFlyEnemyNormalFly)
KILLER_NERVE(TNerveFlyEnemyChaseFly)
KILLER_NERVE(TNerveKillerExplosion)

#undef KILLER_NERVE

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static TKiller* gpCurKiller;

static const char* killer_bastable[] = {
	"/scene/killer/bas/downkiller_down1.bas", nullptr, nullptr,
	"/scene/killer/bas/killer_search1.bas",   nullptr,
};

bool TKiller::mSerialBomb = true;
bool TKiller::mTrampleDie = true;
bool TKiller::mRollSw;

f32 TFlyEnemy::mTestSp         = 2.5f;
s32 TFlyEnemy::mInvalidTime    = 200;
f32 TFlyEnemy::mTestMarioSpMax = 12.0f;

BOOL TNerveKillerExplosion::execute(TSpineBase<TLiveActor>* spine) const
{
	TKiller* self = (TKiller*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mBombScale = self->getSaveParam2()->mSLBombRange.get()
		                   * self->getBodyScale() / self->getAttackRadius();
		self->mRotation.x = 0.0f;
		self->setDeadAnm();

		if (!self->isAirborne()) {
			if (self->mGroundPlane->isWaterSurface()) {
				TEffectBombColumWater* effect
				    = (TEffectBombColumWater*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト爆発水柱マネージャー", 1);
				if (effect != nullptr) {
					JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
					effect->generate(self->mPosition, scale);
				}
			}

			if (self->mGroundPlane->isSand()) {
				TEffectColumSand* effect
				    = (TEffectColumSand*)gpConductor->makeOneEnemyAppear(
				        self->mPosition, "エフェクト砂柱マネージャー", 1);
				if (effect != nullptr) {
					JGeometry::TVec3<f32> scale(0.6f, 0.9f, 0.6f);
					effect->generate(self->mPosition, scale);
				}
			}
		}

		SMSRumbleMgr->start(0x13, &self->mPosition);
	}

	if (self->unk190 < self->mBombScale) {
		self->unk190 *= 1.3f;
	} else {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (self->checkCurAnmEnd(0)) {
			self->onLiveFlag(LIVE_FLAG_DEAD);
			self->onLiveFlag(LIVE_FLAG_UNK8);
			self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			self->mHolder = nullptr;
			self->stopAnmSound();
			spine->reset();
			spine->setNext(&TNerveSmallEnemyDie::theNerve());
			spine->pushAfterCurrent(spine->getDefault());
			self->mPosition.y -= 200.0f;
			return true;
		}
	}

	self->expandCollision();
	if (0) {
		(void)(GXColor){1, 2, 3, 4};
	}
	return false;
}

bool TKiller::isFindMario(f32 scale)
{
	TSmallEnemyParams* prms = getSaveParams();
	f32 searchHeight        = prms->mSLSearchHeight.get();

	if (fabsf(SMS_GetMarioPos().y - mPosition.y) < searchHeight) {
		char trash[8];
		JGeometry::TVec3<f32> marioPos(gpMarioPos->x, gpMarioPos->y,
		                               gpMarioPos->z);

		f32 searchLength = prms->mSLSearchLength.get();
		f32 searchAngle  = prms->mSLSearchAngle.get();
		f32 searchAware  = prms->mSLSearchAware.get();

		if (isInSight(marioPos, searchLength * scale, searchAngle * scale,
		              searchAware * scale))
			return true;
		else
			return false;
	}

	return false;
}

void TKiller::calcRootMatrix()
{
	char trash[0x28];
	if (gpMarDirector->unk4C & 0xF) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (isBckAnm(2)) {
		if (mIsGolden) {
			mEyesColor.r = 0xAA;
			mNoseColor.r = 0xAA;
			mEyesColor.g = 0x8C;
			mNoseColor.g = 0x8C;
			mEyesColor.b = 0;
			mNoseColor.b = 0;
		} else {
			mEyesColor.b = 0;
			mEyesColor.g = 0;
			mEyesColor.r = 0;
			mNoseColor.b = 0;
			mNoseColor.g = 0;
			mNoseColor.r = 0;
		}
	}

	if (isBckAnm(3)) {
		mNoseColor.b = 0;
		mNoseColor.g = 0;
		mEyesColor.r = 0;
		if (mSpine->getTime() % 10 < 5) {
			mNoseColor.r = 0;
			mEyesColor.b = 0;
			mEyesColor.g = 0;
		}

		if (mIsGolden) {
			if (mSpine->getTime() % 10 < 5) {
				mNoseColor.r = 0xAA;
				mNoseColor.g = 0x8C;
				mNoseColor.b = 0;
			} else {
				mNoseColor.r = 0xB4;
				mNoseColor.g = 0x8C;
				mNoseColor.b = 0x96;
			}
		}

		if (mIsHoming) {
			if (mSpine->getTime() % 10 < 5) {
				mEyesColor.r = 0xC8;
				mNoseColor.r = 0xC8;
				mNoseColor.g = 0;
				mNoseColor.b = 0;
			} else {
				mNoseColor.r = 0x46;
				mNoseColor.g = 0x14;
				mNoseColor.b = 0x46;
			}
		}
	}

	if (isBckAnm(1)) {
		mEyesColor.r = 0;
		mEyesColor.b = 0;
		mEyesColor.g = 0;
		mNoseColor.b = 0;
		mNoseColor.g = 0;

		f32 s = JMASSin(360.0f * mSpine->getTime() / 120.0f
		                * (65536.0f / 360.0f));
		s     = fabsf(s);

		if (mIsGolden) {
			mBodyColor.r = 0xAA;
			mBodyColor.g = 0x8C;
			mBodyColor.b = 0;
			mNoseColor.r = (u8)(10.0f * s + 160.0f);
			mNoseColor.g = (u8)(30.0f * s + 140.0f);
			mNoseColor.b = (u8)(150.0f * s);
		}

		if (mIsHoming) {
			mBodyColor.r = 0x46;
			mBodyColor.g = 0x14;
			mBodyColor.b = 0x46;
			mBaseColor.r = 0x46;
			mBaseColor.g = 0x14;
			mBaseColor.b = 0x46;
			mNoseColor.r = (u8)(130.0f * s + 70.0f);
			mNoseColor.g = (u8)(20.0f - 20.0f * s);
			mNoseColor.b = (u8)(70.0f - 70.0f * s);
		}
	}

	gpCurKiller = this;
	mRotation.x = MsClamp(mRotation.x, -25.0f, 90.0f);
	TSpineEnemy::calcRootMatrix();
}

void TKiller::bind()
{
	TFlyEnemy::bind();
	mFlyTime++;

	if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
		if (!isAirborne() && mFlyTime > mInvalidTime) {
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		} else if (mFlyTime > mInvalidTime) {
			TBGWallCheckRecord record(mPosition.x, mPosition.y + mHeadHeight,
			                          mPosition.z, 2.0f * mBodyRadius, 1, 0);
			if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
				const TLiveActor* actor = record.mResultWalls[0]->getActor();
				if (actor != nullptr && actor->isActorType(0x4000000A))
					const_cast<TLiveActor*>(actor)->kill();

				mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
			}
		}
	}

	if (isAirborne()) {
		SMSGetMSound()->startSoundActor(MSD_SE_EN_KILLER_FLY, &mPosition, 0,
		                                nullptr, 0, 4);
		mRotation.x = MsClamp(mRotation.x, -25.0f, 90.0f);
		MsMtxSetXYZRPH(mParticlePos, mPosition.x, mPosition.y, mPosition.z,
		               mRotation.x, mRotation.y, mRotation.z);
		gpMarioParticleManager->emitAndBindToMtxPtr(PARTICLE_MS_KIL_SMOKE,
		                                            mParticlePos, 1, this);
	}
}

void TKiller::setColorType()
{
	if (mIsGolden)
		mIsHoming = 0;

	if (mIsHoming) {
		mBodyColor.r = 0x46;
		mBodyColor.g = 0x14;
		mBodyColor.b = 0x46;
		mBaseColor.r = 0x46;
		mBaseColor.g = 0x14;
		mBaseColor.b = 0x46;
	}
}

void TKiller::reset()
{
	gpCurKiller = this;
	TFlyEnemy::reset();

	TMsRange<f32> range(0.0f, 1.0f);
	mBaseColor.b = 0;
	mBaseColor.g = 0;
	mBaseColor.r = 0;
	mBodyColor.b = 0;
	mBodyColor.g = 0;
	mBodyColor.r = 0;
	mIsGolden    = 0;

	if (range.rand() < 0.05f) {
		mIsGolden    = 1;
		mBodyColor.r = 200;
		mBodyColor.g = 185;
		mBodyColor.b = 0;
		mBaseColor.r = 255;
		mBaseColor.g = 225;
		mBaseColor.b = 70;
	}
}

void TKiller::changeOut()
{
	char trash[8];
	SMSGetMSound()->startSoundActor(MSD_SE_EN_TELSA_RECOVER, &mPosition, 0,
	                                nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_DEAD);
	genEventCoin();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mPosition = mJuiceBlock->mPosition;

	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	getMActor()->setFrameRate(SMSGetAnmFrameRate(), 0);
	mJuiceBlock->kill();
	mJuiceBlock = nullptr;
}

void TKiller::flyBehavior()
{
	char trash[0x18];
	mTurnSpeed = mKillerParams->mSLTurnSpeedLow.get();

	TKillerSaveLoadParams* prms = mKillerParams;
	if (mSpine->getTime() > prms->mSLChaseTimer.get())
		mFlyGravityY -= prms->mSLWaterAddGravityY.get();

	if (checkCurAnmEnd(0) && isBckAnm(3))
		setBckAnm(1);

	mRollAngle += 2.5f;
}

bool TKiller::isCollidMove(THitActor* other)
{
	if (other->isActorType(0x4000000A))
		static_cast<TLiveActor*>(other)->kill();

	if (mSerialBomb) {
		if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
	}

	return true;
}

bool TKiller::isRollFly()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    && isBckAnm(1))
		return true;
	return false;
}

void TKiller::setChaseFlyAnm() { setBckAnm(3); }

void TKiller::setNormalFlyAnm()
{
	mMActor = mMActorKeeper->getMActor("killer_model1.bmd");
	setBckAnm(2);
	mRollAngle = 0.0f;
	mFlyTime   = 0;
}

const char** TKiller::getBasNameTable() const { return killer_bastable; }

void TKiller::attackToMario()
{
	char trash[8];
	if (gpMarioPos->y < mPosition.y) {
		if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
			mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
			sendAttackMsgToMario();
		} else {
			SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
		}
	}
}

void TKiller::setDeadAnm()
{
	mMActor = mMActorKeeper->getMActor("downkiller_model1.bmd");
	setBckAnm(0);

	TEffectExplosion* effect = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
	    mPosition, "エフェクト爆発マネージャー", 1);
	if (effect != nullptr)
		effect->generate(mPosition, mScaling);

	gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	SMSRumbleMgr->start(0x15, 5, static_cast<f32*>(nullptr));
}

bool TKiller::isHitValid(u32 message)
{
	if (message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		genEventCoin();
		return false;
	}

	if (mTrampleDie)
		mFlyGravityY -= 12.0f;

	return false;
}

void TKiller::genEventCoin()
{
	int coinNum = 2;
	if (mIsGolden)
		coinNum = 8;

	char trash[4];
	Vec offset;
	Mtx rotMtx;
	MtxPtr mtxp = rotMtx;
	char trash2[8];

	for (int i = 0; i < coinNum; ++i) {
		offset.x = 0.0f;
		offset.y = 0.0f;
		offset.z = 30.0f;

		MsMtxSetRotY(mtxp, 360.0f * (1.0f / coinNum) * (i + 1));
		MTXMultVec(mtxp, &offset, &offset);

		TCoin* coin = (TCoin*)gpItemManager->makeObjAppear(
		    mPosition.x + offset.x, mPosition.y, mPosition.z + offset.z,
		    0x2000000E, true);
		if (coin != nullptr) {
			coin->mPosition.y = mPosition.y;
			MsVECNormalize(&offset, &offset);
			coin->setVelocityAndFlag10(3.0f * offset.x, 20.0f,
			                           3.0f * offset.z);
		}
	}
}

void TKiller::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyNormalFly::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()) {
		genEventCoin();
	}

	if (mSpine->getCurrentNerve() != &TNerveKillerExplosion::theNerve()) {
		mSpine->pushNerve(&TNerveKillerExplosion::theNerve());
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
	}
}

void TKiller::setMActorAndKeeper()
{
	char trash[8];
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("killer_model1.bmd", 3);
	mMActorKeeper->createMActor("downkiller_model1.bmd", 3);

	u32 noseMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_nosemat1");
	u32 eyesMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_eyesmat1");
	u32 bodyMatIdx = getActorKeeper()
	                     ->getMActor("killer_model1.bmd")
	                     ->getModel()
	                     ->getModelData()
	                     ->getMaterialName()
	                     ->getIndex("_body1");

	SMS_InitPacket_OneTevColor(mMActor->getModel(), noseMatIdx, GX_TEVREG0,
	                           &mNoseColor);
	SMS_InitPacket_OneTevColor(mMActor->getModel(), eyesMatIdx, GX_TEVREG0,
	                           &mEyesColor);
	SMS_InitPacket_OneTevColor(mMActor->getModel(), bodyMatIdx, GX_TEVREG0,
	                           &mBodyColor);
	SMS_InitPacket_OneTevColor(
	    getActorKeeper()->getMActor("downkiller_model1.bmd")->getModel(),
	    bodyMatIdx, GX_TEVREG0, &mBaseColor);
}

static int KillerBodyCallback(J3DNode*, int);

void TKiller::init(TLiveManager* manager)
{
	TFlyEnemy::init(manager);
	mActorType = 0x1000001F;
	unk150     = 17;
	mKillerParams = getSaveParam2();

	mSpine->initWith(&TNerveFlyEnemyNormalFly::theNerve());

	onLiveFlag(LIVE_FLAG_UNK400);
	offLiveFlag(LIVE_FLAG_UNK800);
	onHitFlag(HIT_FLAG_UNK40000000);

	J3DModel* model = mMActor->getModel();
	if (model->getSkinDeform() == nullptr)
		model->setSkinDeform(new J3DSkinDeform, (J3DDeformAttachFlag)1);

	mMActor->resetDL();

	if (getInstanceIndex() == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); i++) { }
	}

	mMActor->setJointCallback(1, KillerBodyCallback);
	unk188 = 0.0f;
}

TKiller::TKiller(const char* name)
    : TFlyEnemy(name)
{
}

static int KillerBodyCallback(J3DNode* node, int timing)
{
	if (timing == 0) {
		if (gpCurKiller == nullptr || !TKiller::mRollSw
		    || !gpCurKiller->isRollFly())
			return true;

		J3DJoint* joint = (J3DJoint*)node;
		MtxPtr anmMtx   = gpCurKiller->getModel()->getAnmMtx(joint->getJntNo());

		f32 scale = gpCurKiller->getBodyScale();
		Mtx scaleMtx;
		scaleMtx[0][3] = 0.0f;
		scaleMtx[1][3] = 0.0f;
		scaleMtx[2][3] = 0.0f;
		scaleMtx[0][0] = scale;
		scaleMtx[0][1] = 0.0f;
		scaleMtx[0][2] = 0.0f;

		scaleMtx[1][0] = 0.0f;
		scaleMtx[1][1] = scale;
		scaleMtx[1][2] = 0.0f;

		scaleMtx[2][0] = 0.0f;
		scaleMtx[2][1] = 0.0f;
		scaleMtx[2][2] = scale;

		Mtx rollMtx;
		char trash[8];
		MsMtxSetRotZ(rollMtx, gpCurKiller->mRollAngle);

		MTXConcat(anmMtx, rollMtx, anmMtx);
		MTXConcat(anmMtx, scaleMtx, anmMtx);
		MTXConcat(J3DSys::mCurrentMtx, rollMtx, J3DSys::mCurrentMtx);
		MTXConcat(J3DSys::mCurrentMtx, scaleMtx, J3DSys::mCurrentMtx);
	}
	return true;
}

TSpineEnemy* TKillerManager::createEnemyInstance() { return new TKiller; }

void TKillerManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "killer_model1.bmd", 0x10220000, 0 },
		{ "downkiller_model1.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKillerManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TKillerSaveLoadParams("/enemy/killer.prm");
}

TKillerManager::TKillerManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurKiller = nullptr;
}

// Retail calls MsSin/MsCos out of line from the chase nerve.
#pragma dont_inline on
static f32 msSinOOL(f32 v) { return MsSin(v); }
static f32 msCosOOL(f32 v) { return MsCos(v); }
#pragma dont_inline off

BOOL TNerveFlyEnemyChaseFly::execute(TSpineBase<TLiveActor>* spine) const
{
	TFlyEnemy* self = (TFlyEnemy*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mChaseVelocity = self->mVelocity;
		self->calcChaseParam();
		self->setChaseFlyAnm();
	}

	f32 speed = 1.0f;
	if (!self->mIsHoming) {
		f32 marchSpeed = self->mMarchSpeed;
		f32 marchSq    = marchSpeed * marchSpeed;
		self->mChaseVelocity.y = 0.1f;
		speed = self->mChaseVelocity.length() / marchSq * TFlyEnemy::mTestSp;
	} else if (self->mChaseState == 0) {
		speed = 2.0f;
	}
	self->walkBehavior(2, speed);

	if ((self->mIsHoming && self->isReachedToGoalXZ())
	    || (self->mChaseState == 0
	        && self->mPosition.y < 100.0f + self->mGroundHeight)) {
		self->calcChaseParam();
	}

	switch (self->mChaseState) {
	case 0:
	case 1:
		self->walkBehavior(3, 1.0f);
		break;

	case 2: {
		const TPathNode& goal = self->unkF4;

		JGeometry::TVec3<f32> vel = self->mVelocity;
		vel.scale(0.9f);
		self->mVelocity = vel;

		JGeometry::TVec3<f32> dir = goal.getPoint();
		dir.sub(self->mPosition);
		VECMag((Vec*)&dir);

		f32 target = MsAngleWrap(MsGetRotFromZaxisY(dir));
		f32 diff   = MsAngleDiff(target, self->mRotation.y);

		if (diff > 0.0f) {
			diff = (diff > self->mTurnSpeed) ? self->mTurnSpeed : diff;
		} else {
			diff = (diff > -self->mTurnSpeed) ? diff : -self->mTurnSpeed;
		}

		self->mRotation.y = MsAngleWrap(self->mRotation.y + diff);

		JGeometry::TVec3<f32> linVel = self->mLinearVelocity;
		f32 rotYNow                  = self->mRotation.y;
		f32 spd                      = self->mMarchSpeed;
		f32 dz                       = spd * msCosOOL(rotYNow);
		f32 dx                       = spd * msSinOOL(rotYNow);
		JGeometry::TVec3<f32> delta;
		delta.set(dx, 0.0f, dz);
		linVel.add(delta);
		self->mLinearVelocity = linVel;
		break;
	}
	}

	if (self->mChaseState != 2 && 200.0f + self->mGroundHeight < self->mPosition.y) {
		f32 spd = self->mMarchSpeed;
		JGeometry::TVec3<f32> axis(self->mMarchSpeed,
		                           -40.0f * self->getGravityY(), spd);
		self->mRotation.x = MsGetRotFromZaxis(axis).x;

		JGeometry::TVec3<f32> lin = self->mLinearVelocity;
		lin.y                     = self->getGravityY();
		self->mRotation.x         = MsGetRotFromZaxis(lin).x;
	} else {
		self->mRotation.x *= 0.99f;
	}

	self->flyBehavior();

	f32 s = MsClamp(1.1f * self->mScaling.x, 0.0f, self->mBodyScale);
	self->mScaling.x = self->mScaling.y = self->mScaling.z = s;

	return false;
}

BOOL TNerveFlyEnemyNormalFly::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x28];
	TFlyEnemy* self = (TFlyEnemy*)spine->getBody();

	if (spine->getTime() == 0)
		self->setNormalFlyAnm();

	if (self->mIsTimedChase && self->mFlyTime > 500) {
		self->updateSquareToMario();
		f32 chaseDist = self->mFlyParams->mSLChaseDist.get();
		if (self->mDistToMarioSquared < chaseDist * chaseDist) {
			spine->pushAfterCurrent(&TNerveFlyEnemyChaseFly::theNerve());
			return true;
		}
	} else if (!self->mIsGolden) {
		if (self->isFindMario(1.0f) && self->mFlyTime > 100) {
			self->updateSquareToMario();
			f32 chaseDist = self->mFlyParams->mSLChaseDist.get();
			if (self->mDistToMarioSquared < chaseDist * chaseDist) {
				spine->pushAfterCurrent(&TNerveFlyEnemyChaseFly::theNerve());
				return true;
			}
		}
	}

	JGeometry::TVec3<f32> vel = self->mVelocity;
	self->mRotation.x         = MsGetRotFromZaxis(vel).x;

	f32 s = MsClamp(1.05f * self->mScaling.x, 0.0f, self->mBodyScale);
	self->mScaling.x = self->mScaling.y = self->mScaling.z = s;
	return false;
}

void TFlyEnemy::bind()
{
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve()
	    || mFlyTime < mInvalidTime) {
		fly();
	} else {
		TLiveActor::bind();
	}
}

void TFlyEnemy::calcChaseParam()
{
	JGeometry::TVec3<f32> toMario;
	toMario.x = gpMarioPos->x - mPosition.x;
	toMario.y = gpMarioPos->y - mPosition.y;
	toMario.z = gpMarioPos->z - mPosition.z;
	toMario.x *= 1.1f;
	toMario.z *= 1.1f;

	JGeometry::TVec3<f32> target;
	target.x = mPosition.x + toMario.x;
	target.y = mPosition.y + toMario.y;
	target.z = mPosition.z + toMario.z;
	char trash[16];

	if (mIsHoming)
		setGoalPath(TPathNode(target));

	if (toMario.y > 100.0f || fabsf(toMario.y) < 100.0f) {
		if (mIsHoming) {
			if (mChaseState != 2 && mSprayedByWaterCooldown == 0)
				mFlyGravityY = mFlyParams->mSLChaseFlyGravityY.get();
			mChaseState = 2;
		} else {
			mPosition.y -= 1.0f;
		}
	} else {
		mFlyGravityY = mFlyParams->mSLNormalFlyGravityY.get();
		JGeometry::TVec3<f32> vel(0.0f, 0.0f, 0.0f);
		if (mChaseState != 2 || toMario.y > 150.0f) {
			mChaseState = 0;
			MsVECNormalize(&toMario, &toMario);
			f32 flySpeed = mFlyParams->mSLNormalFlySpeed.get();
			vel.x        = toMario.x * flySpeed;
			vel.z        = toMario.z * flySpeed;
			mFlyGravityY = mFlyParams->mSLForceGravityY.get();
		} else {
			mPosition.y -= 3.0f;
		}
		vel.z     = 0.0f;
		vel.x     = 0.0f;
		mVelocity = vel;
	}
}

void TFlyEnemy::fly()
{
	JGeometry::TVec3<f32> nextPos = mPosition;
	nextPos += mLinearVelocity;

	JGeometry::TVec3<f32> vel = mVelocity;
	char trash[24];
	vel.x += *gpMarioSpeedX / mTestMarioSpMax;
	vel.z += *gpMarioSpeedZ / mTestMarioSpMax;
	nextPos += vel;
	nextPos.y += mFlyGravityY;

	f32 nextY = nextPos.y;
	mGroundHeight = gpMap->checkGround(nextPos.x, nextY + mHeadHeight, nextPos.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (nextY <= mGroundHeight) {
		if (mFlyTime > mInvalidTime) {
			offLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity.set(0.0f, 0.0f, 0.0f);
			nextPos.y = mGroundHeight;
		}

		const TLiveActor* actor = mGroundPlane->getActor();
		if (actor != nullptr && actor->isActorType(0x4000000A))
			const_cast<TLiveActor*>(actor)->kill();

		if (mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL))
			kill();
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	mLinearVelocity = nextPos - mPosition;
}

void TFlyEnemy::reset()
{
	TWalkerEnemy::reset();
	mFlyTime       = 0;
	mChaseState    = 1;
	mIsTimedChase  = 0;
	mFlyGravityY   = mFlyParams->mSLNormalFlyGravityY.get();
	mIsHoming      = 0;
}

f32 TFlyEnemy::getGravityY() const
{
	char trash[8];
	if (mSpine->getCurrentNerve() == &TNerveFlyEnemyChaseFly::theNerve())
		return mFlyGravityY;
	return mFlyParams->mSLNormalFlyGravityY.get();
}

void TFlyEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mFlyParams = (TFlyEnemyParams*)getSaveParam();
}
