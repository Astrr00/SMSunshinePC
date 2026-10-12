#include <Enemy/Kukku.hpp>
#include <Enemy/Graph.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <M3DUtil/MActor.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <Animal/AnimalBase.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

enum {
	PARTICLE_KUKKU_COOC_ASE  = 0x18C,
	PARTICLE_KUKKU_COOC_HANE = 0x18D,
};

static const char* tori_bastable[] = {
	"/scene/tori/bas/tori_back.bas",
	"/scene/tori/bas/tori_down.bas",
	nullptr,
	"/scene/tori/bas/tori_fall_end.bas",
	"/scene/tori/bas/tori_hit.bas",
	"/scene/tori/bas/tori_wait.bas",
};

namespace {
// number of coins dropped, indexed by the (clamped) random roll
const int cDropCoinNumTable[] = { 3, 3, 1, 2 };
} // namespace

void TKukkuBall::init()
{
	initHitActor(0x1000002E, 1, -0x80000000, 30.0f, 30.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);

	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);

	const ResTIMG* image
	    = (const ResTIMG*)JKRGetResource("/scene/map/pollution/H_ma_rak.bti");
	if (image)
		SMS_ChangeTextureAll(mMActor->getModel()->getModelData(),
		                     "K_name_dummy", *image);
}

void TKukkuBall::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (mFlags & 1)
		return;

	if (cue & 2) {
		Mtx mtx;
		mtx[0][0] = 1.0f;
		mtx[1][0] = 0.0f;
		mtx[2][0] = 0.0f;
		mtx[0][1] = 0.0f;
		mtx[1][1] = 1.0f;
		mtx[2][1] = 0.0f;
		mtx[0][2] = 0.0f;
		mtx[1][2] = 0.0f;
		mtx[2][2] = 1.0f;
		mtx[0][3] = mPosition.x;
		mtx[1][3] = mPosition.y;
		mtx[2][3] = mPosition.z;
		mMActor->getModel()->setBaseScale(mScaling);
		mMActor->getModel()->setBaseTRMtx(mtx);
		mMActor->getModel()->calc();
	}

	if (cue & 1) {
		THitActor** end = mCollisions + mColCount;
		for (THitActor** col = mCollisions; col != end; ++col) {
			switch ((*col)->mActorType) {
			case 0x80000001:
				SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
				onHitFlag(HIT_FLAG_NO_COLLISION);
				mFlags |= 1;
				gpPollution->stamp(1, mPosition.x, mPosition.y, mPosition.z,
				                   500.0f);
				break;
			}
		}

		mVelocity.y -= 0.9f;
		mVelocity.x *= 0.94f;
		mVelocity.y *= 0.94f;
		mVelocity.z *= 0.94f;

		JGeometry::TVec3<f32> nextPos = mPosition;
		nextPos.x += mVelocity.x;
		nextPos.y += mVelocity.y;
		nextPos.z += mVelocity.z;

		const TBGCheckData* ground;
		f32 groundY = gpMap->checkGround(
		    nextPos.x, nextPos.y + getAttackHeight(), nextPos.z, &ground);
		if (nextPos.y <= 0.05f + (groundY + 1.0f)) {
			onHitFlag(HIT_FLAG_NO_COLLISION);
			mFlags |= 1;
			gpPollution->stamp(1, mPosition.x, mPosition.y, mPosition.z,
			                   500.0f);
		}

		gpMap->isTouchedOneWallAndMoveXZ(&nextPos.x,
		                                 nextPos.y + getAttackHeight(),
		                                 &nextPos.z, getAttackRadius());
		mPosition = nextPos;
	}

	if (!(mFlags & 4))
		mMActor->perform(cue, graphics);
}

TKukku::TKukku(const char* name)
    : TSmallEnemy(name)
    , mMushroom(nullptr)
{
}

void TKukku::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 4);
	mMActor       = mMActorKeeper->createMActor("tori.bmd", 0);
	mSpine->initWith(&TNerveKukkuGraphWander::theNerve());
	mMushroom = TMapObjBaseManager::newAndRegisterObj("mushroom1upR");

	for (TKukkuBall** ball = &mBalls[0]; ball != &mBalls[3]; ++ball) {
		*ball = new TKukkuBall(mMActorKeeper->createMActor("torifun.bmd", 3));
		(*ball)->init();
	}

	initHitActor(0x1000002E, 1, -0x80000000, 30.0f, 30.0f, 100.0f, 100.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	initAnmSound();
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_ase.jpa",
	                 PARTICLE_KUKKU_COOC_ASE);
	SMS_LoadParticle("/scene/tori/jpa/ms_cooc_hane.jpa",
	                 PARTICLE_KUKKU_COOC_HANE);
	mCenterJointIndex
	    = getModel()->getModelData()->getJointName()->getIndex("center");
	reset();
}

void TKukku::reset()
{
	unk1A4      = 0;
	mShootTimer = 0;
	mGravity    = 0.0f;
	mDropCount  = 0;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	mScaledBodyRadius = 75.0f;
}

BOOL TKukku::receiveMessage(THitActor* sender, u32 message)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return FALSE;

	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	default:
		return TSmallEnemy::receiveMessage(sender, message);
	}
}

void TKukku::control()
{
	if (unk1A4 > 0)
		unk1A4 -= 1;

	TLiveActor::control();
}

// Fall and PostFall are the two nerves in which the bird plummets
inline bool TKukku::isFalling() const
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	return nerve == &TNerveKukkuFall::theNerve()
	       || nerve == &TNerveKukkuPostFall::theNerve();
}

void TKukku::calcRootMatrix()
{
	if (mSpine->getLatestNerve() == &TNerveSmallEnemyDie::theNerve()) {
		JGeometry::TVec3<f32> normal;
		if (mGroundPlane)
			normal.normalize(mGroundPlane->getNormal());
		else
			normal.set(0.0f, 1.0f, 0.0f);

		JGeometry::TQuat4<f32> yaw;
		yaw.setEulerY(0.017453294f * mRotation.y);

		JGeometry::TQuat4<f32> quat;
		quat.setRotate(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), normal, 1.0f);
		quat.mul(yaw);

		TPosition3f mtx;
		mtx.setQT(quat, mPosition);
		getModel()->setBaseTRMtx(mtx);
		getModel()->setBaseScale(mScaling);
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (isFalling()) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_KUKKU_COOC_HANE, getModel()->getAnmMtx(mCenterJointIndex),
		    1, this);
	}

	if (mMActor->checkCurAnm("tori_back", 0)) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_KUKKU_COOC_ASE, getModel()->getAnmMtx(mCenterJointIndex),
		    1, this);
	}
}

void TKukku::bind() { TLiveActor::bind(); }

void TKukku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	for (TKukkuBall** ball = &mBalls[0]; ball != &mBalls[3]; ++ball)
		(*ball)->perform(cue, graphics);
}

static inline bool isRecoverGraph(const TNerveBase<TLiveActor>* nerve)
{
	return nerve != &TNerveKukkuRecoverGraph::theNerve() ? false : true;
}

void TKukku::behaveToWater(THitActor*)
{
	if (!isFalling() && !isRecoverGraph(mSpine->getLatestNerve())) {
		mSpine->reset();
		mSpine->setNext(&TNerveKukkuFall::theNerve());
	}
}

#pragma dont_inline on
void TKukku::updateRotation()
{
	JGeometry::TVec3<f32> diff(unkF4.getPoint());
	diff -= mPosition;

	f32 dist = diff.length();
	if (dist < 100.0f)
		return;

	f32 marchSpeed = getSaveParam2()->mMarchSpeed.get();
	f32 turnSpeed  = getSaveParam2()->mTurnSpeed.get();

	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, diff, marchSpeed, turnSpeed);

	mRotation.x *= isFalling() ? 0.0f : 1.0f;
	mRotation.z *= isFalling() ? 0.0f : 1.0f;
}

JGeometry::TVec3<f32> TKukku::calcMomentum(f32 speed)
{
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> v(0.0f, 0.0f, speed);
	quat.rotate(v, v);
	return v;
}
#pragma dont_inline off

void TKukku::dropCoins()
{
	if (mDropCount > 10)
		return;

	if (mDropCount == 10 && mMushroom) {
		mDropCount += 1;
		mMushroom->appear();
		mMushroom->JSGSetTranslation(mPosition);
		mMushroom->mVelocity.zero();
		mMushroom->offLiveFlag(LIVE_FLAG_UNK10);
		return;
	}

	int roll = 4.0f * MsRandF();
	if (roll < 1)
		roll = 1;
	else if (roll > 3)
		roll = 3;

	int coinNum = cDropCoinNumTable[roll];

	JGeometry::TQuat4<f32> yawStep;
	yawStep.setEulerY(6.2831855f / coinNum);

	JGeometry::TQuat4<f32> pitch;
	pitch.setEulerX(3.1415927f * getSaveParam2()->mDropAngleX.get());

	f32 dropSpeed = getSaveParam2()->mDropSpeed.get();
	s16 angle     = DEG2SHORTANGLE(mRotation.y);
	JGeometry::TVec3<f32> velocity;
	velocity.set(dropSpeed * JMASSin(angle), 0.0f, dropSpeed * JMASCos(angle));

	JGeometry::TVec3<f32> dir = velocity;
	pitch.rotate(dir, dir);
	yawStep.rotate(dir, dir);

	for (int i = 0; i < coinNum; ++i) {
		TMapObjBase* coin = gpItemManager->makeObjAppear(0x2000000E);
		if (!coin)
			break;

		coin->appear();
		coin->JSGSetTranslation(mPosition);
		coin->mVelocity.x = dir.x;
		coin->mVelocity.y = dir.y;
		coin->mVelocity.z = dir.z;
		coin->offLiveFlag(LIVE_FLAG_UNK10);

		mDropCount += 1;
		if (mDropCount == 10)
			break;

		yawStep.rotate(dir, dir);
	}
}

void TKukku::setDeadAnm()
{
	mMActor->setBck("tori_down");
	setCurAnmSound();
}

void TKukku::setAfterDeadEffect()
{
	TSmallEnemy::setAfterDeadEffect();
	TSmallEnemyManager* manager = (TSmallEnemyManager*)getManager();
	gpPollution->stamp(manager->getUnk58(), mPosition.x, mPosition.y,
	                   mPosition.z, 1000.0f);
}

TKukkuManager::TKukkuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKukkuManager::load(JSUMemoryInputStream& stream)
{
	TKukkuParams* params = new TKukkuParams("/enemy/kukku.prm");

	unk38 = params;

	params->mSLAttackRadius.set(30);
	params->mSLAttackHeight.set(30);
	params->mSLDamageRadius.set(100);
	params->mSLDamageHeight.set(100);
	params->mSLBodyRadius.set(5.0f);
	TSmallEnemyManager::load(stream);
}

void TKukkuManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "tori.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

const char** TKukku::getBasNameTable() const { return tori_bastable; }

// first ball that is dead and not owned by anything
static TKukkuBall* findIdleBall(TKukkuBall** begin, TKukkuBall** end)
{
	for (; begin != end; ++begin) {
		if ((*begin)->isIdle())
			return *begin;
	}
	return nullptr;
}

DEFINE_NERVE(TNerveKukkuGraphWander, TLiveActor)
{
	TKukku* self = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mVelocity = JGeometry::TVec3<f32>(0.0f);
		self->unk124->reset();
		self->goToShortestNextGraphNode();

		if (self->getMActor()->checkCurAnm("tori_back", 0)) {
			self->getMActor()->setBck("tori_back");
			self->setCurAnmSound();
		} else {
			self->getMActor()->setBck("tori_wait");
			self->setCurAnmSound();
		}
	}

	if (self->isReachedToGoal()) {
		self->goToRandomNextGraphNode();
		if (!self->getMActor()->checkCurAnm("tori_wait", 0)) {
			self->getMActor()->setBck("tori_wait");
			self->setCurAnmSound();
		}
	}

	JGeometry::TVec3<f32> toMario = SMS_GetMarioPos();
	toMario -= self->mPosition;
	toMario.y = 0.0f;

	f32 range = self->getSaveParam2()->mSearchRange.get();
	if (toMario.squared() < range * range) {
		if (self->mShootTimer >= 0) {
			self->mShootTimer -= 1;
		} else if (TKukkuBall* ball
		           = findIdleBall(&self->mBalls[0], &self->mBalls[3])) {
			JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);
			dir.setLength(self->getSaveParam2()->mShootSpeed.get());

			self->getModel()->getModelData()->getJointName()->getIndex(
			    "null_osen");
			JGeometry::TVec3<f32> pos = self->mPosition;

			ball->offHitFlag(HIT_FLAG_NO_COLLISION);
			ball->mFlags &= ~1;
			ball->mPosition = pos;
			ball->mVelocity = dir;

			self->mShootTimer = self->getSaveParam2()->mShootInterval.get();
			SMSGetMSound()->startSoundActor(MSD_SE_EN_TORI_SHIT,
			                                &self->mPosition, 0, nullptr, 0,
			                                4);
		}
	}

	self->updateRotation();

	f32 marchSpeed              = self->getSaveParam2()->mMarchSpeed.get();
	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(self->mRotation);
	JGeometry::TVec3<f32> v;
	v.set(0.0f, 0.0f, marchSpeed);
	quat.rotate(v);
	self->mLinearVelocity = v;
	return FALSE;
}

// Not referenced by anything in the retail binary, so the linker strips the
// nerve itself; only its "tori_hit" literal and the bookkeeping of its local
// static survive in .rodata/.bss and have to be reproduced. The body is
// reconstructed from that literal alone.
DECLARE_NERVE(TNerveKukkuHit, TLiveActor);

DEFINE_NERVE(TNerveKukkuHit, TLiveActor)
{
	TKukku* self = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getMActor()->setBck("tori_hit");
		self->setCurAnmSound();
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuFall, TLiveActor)
{
	TKukku* self = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getMActor()->setBck("tori_wait");
		self->setCurAnmSound();
		J3DFrameCtrl* frameCtrl
		    = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		frameCtrl->setRate(2.0f * SMSGetAnmFrameRate());
		self->mVelocity = JGeometry::TVec3<f32>(
		    0.0f, -self->getSaveParam2()->mWaterPowerY.get(), 0.0f);
		self->dropCoins();
	}

	if (!self->checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
		SMS_EasyEmitParticle(PARTICLE_MS_POPO_BOMB_A, &self->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		SMS_EasyEmitParticle(PARTICLE_MS_POPO_BOMB_B, &self->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
		return TRUE;
	}

	JGeometry::TVec3<f32> velocity = self->mVelocity;
	f32 fric                       = self->getSaveParam2()->mAirFric.get();
	velocity.x *= fric;
	velocity.y *= fric;
	velocity.z *= fric;
	velocity.y += self->getSaveParam2()->mUpperVelocityY.get();

	bool recover = false;
	if (-0.1f < velocity.y) {
		velocity.y = 0.0f;
		recover    = true;
	}

	self->mVelocity = velocity;

	if (recover) {
		spine->pushAfterCurrent(&TNerveKukkuRecoverGraph::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuPostFall, TLiveActor)
{
	TKukku* self = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getMActor()->setBck("tori_back");
		self->setCurAnmSound();
		self->mVelocity = JGeometry::TVec3<f32>(0.0f);
	}

	if (self->checkCurAnmEnd(0)
	    && self->getSaveParam2()->mHabatakiTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveKukkuRecoverGraph, TLiveActor)
{
	TKukku* self = (TKukku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getMActor()->setBck("tori_back");
		self->setCurAnmSound();
		self->mVelocity = JGeometry::TVec3<f32>(0.0f);
	}

	if (self->getSaveParam2()->mHabatakiTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKukkuGraphWander::theNerve());
		return TRUE;
	}

	self->updateRotation();
	self->mLinearVelocity
	    = self->calcMomentum(self->getSaveParam2()->mMarchSpeed.get());
	return FALSE;
}
