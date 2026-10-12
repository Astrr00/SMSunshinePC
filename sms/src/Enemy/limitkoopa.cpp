#include <Enemy/LimitKoopa.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JGeometry.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <MoveBG/MapObjManager.hpp>

namespace {

int KoopaNeckCallBack(J3DNode*, int);

} // namespace

// ============= params =============

inline TLimitKoopaParams::TLimitKoopaParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(rotationSpeed, 1.0f)
    , PARAM_INIT(bodyScale, 1.0f)
    , PARAM_INIT(hipDropInitialSpeedY, 1.0f)
    , PARAM_INIT(hipDropGravityY, 1.0f)
    , PARAM_INIT(turnSpeed, 1.6f)
    , PARAM_INIT(turnAnim, 3.7f)
    , PARAM_INIT(waitStep, 600.0f)
    , PARAM_INIT(attackRadius, 800.0f)
    , PARAM_INIT(attackHeight, 1000.0f)
    , PARAM_INIT(focusRange, 2.0f)
    , PARAM_INIT(waitRange, 12.0f)
    , PARAM_INIT(fireSpeed, 4.0f)
    , PARAM_INIT(tumbleSpeed, 2.0f)
    , PARAM_INIT(waitSpeed, 2.0f)
    , PARAM_INIT(staggerSpeed, 2.0f)
    , PARAM_INIT(downSpeed, 1.8f)
    , PARAM_INIT(tumbleWeight, 4.2f)
    , PARAM_INIT(flameScale, 1.0f)
    , PARAM_INIT(flameCount, 300)
    , PARAM_INIT(flameFocusStartStep, 100)
    , PARAM_INIT(flameFocusEndStep, 300)
    , PARAM_INIT(flameRadius, 200.0f)
    , PARAM_INIT(flameHeight, 600.0f)
    , PARAM_INIT(headRadius, 400.0f)
    , PARAM_INIT(waterhitSpeed, 2.0f)
    , PARAM_INIT(flameOverStart, 0.9f)
    , PARAM_INIT(flameNeckRange, 16.0f)
    , PARAM_INIT(flameNeckDownRate, 0.3f)
    , PARAM_INIT(marioEstimationFire, 20.0f)
    , PARAM_INIT(marioEstimationWait, 10.0f)
{
	TParams::load(mPrmPath);
	rotationSpeed.set(0.25f);
	bodyScale.set(0.7f);
	hipDropInitialSpeedY.set(40.0f);
	hipDropGravityY.set(0.4f);
}

// ============= koopa helpers =============

inline TLimitKoopaParams* TLimitKoopa::getParam() const
{
	return (TLimitKoopaParams*)((TEnemyManager*)mManager)->getSaveParam();
}

inline void TLimitKoopa::changeAnm(int bck, f32 rate)
{
	mMActor->setBck(bck);
	mMActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(rate);
}

inline bool TLimitKoopa::isBreathing() const
{
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 4)
		return true;
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 5
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() >= 127.0f)
		return true;
	return false;
}

// ============= TLimitKoopaManager =============

void TLimitKoopaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopa_model.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TLimitKoopaManager::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_a.jpa", 0x1C0);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_b.jpa", 0x1C1);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_c.jpa", 0x1C2);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_d.jpa", 0x1C3);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_e.jpa", 0x1F3);
}

void TLimitKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TLimitKoopaParams("/enemy/limitkoopa.prm");
}

TSpineEnemy* TLimitKoopaManager::createEnemyInstance() { return nullptr; }

TLimitKoopaManager::TLimitKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

// ============= TLimitKoopa =============

void TLimitKoopa::setUpHitActors()
{
	MtxPtr headMtx = mMActor->getModel()->getAnmMtx(mHeadJntIdx);

	if (isBreathing()) {
		f32 reach = 1.0f;
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 5) {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			reach = (ctrl->getFrame() - 125.0f) / (ctrl->getEnd() - 125.0f);
		}

		for (int i = 0; i < 10; ++i) {
			f32 radius = getParam()->flameRadius.get();
			f32 height = getParam()->flameHeight.get();

			JGeometry::TVec3<f32> offset;
			offset.x = 0.8f * ((2.0f + i * 2) * radius) * reach;
			offset.y = 0.0f;
			offset.z = 0.0f;
			JGeometry::TVec3<f32> pos;
			((TMtx34f*)headMtx)->mult(offset, pos);

			if (height <= 0.0f)
				height = 2.0f * radius;
			mFlames[i]->set(JGeometry::TVec3<f32>(pos.x, mPosition.y, pos.z),
			                radius, height);
		}
	} else {
		for (int i = 0; i < 10; ++i)
			mFlames[i]->remove();
	}

	MtxPtr agoMtx = mMActor->getModel()->getAnmMtx(mAgoJntIdx);
	f32 radius    = getParam()->headRadius.get();
	mHead->set(
	    JGeometry::TVec3<f32>(agoMtx[0][3], agoMtx[1][3] - 200.0f, agoMtx[2][3]),
	    radius, 2.0f * radius);
}

void TLimitKoopa::calcRootMatrix()
{
	char trash[8];
	mScaling.setAll(getParam()->bodyScale.get());
	mRotation.y = TDirectionCalc::r2d(mDirection.mDirection);
	TSpineEnemy::calcRootMatrix();
}

BOOL TLimitKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

f32 TLimitKoopa::getGravityY() const
{
	return getParam()->hipDropGravityY.get();
}

void TLimitKoopa::startHipDrop()
{
	JGeometry::TVec3<f32> velocity;
	velocity.x = 0.0f;
	velocity.y = 1.0f;
	velocity.z = 0.0f;
	velocity.scale(getParam()->hipDropInitialSpeedY.get());

	JGeometry::TVec3<f32> target = *gpMarioPos;
	target.y                     = mGroundHeight;
	JGeometry::TVec3<f32> goal;
	goal.x = mPosition.x + (target.x - mPosition.x);
	goal.y = mPosition.y + (target.y - mPosition.y);
	goal.z = mPosition.z + (target.z - mPosition.z);

	velocity = calcVelocityToJumpToY(goal, velocity.y,
	                                 getParam()->hipDropGravityY.get());

	if (velocity.length() > 200.0f) {
		velocity.normalize();
		velocity.scale(200.0f);
	}

	mVelocity.x = velocity.x;
	mVelocity.y = velocity.y;
	mVelocity.z = velocity.z;
}

void TLimitKoopa::bind()
{
	JGeometry::TVec3<f32> pos = mPosition;
	pos += mLinearVelocity;
	pos += mVelocity;

	mVelocity += mAcceleration;

	// the arena floor is a fixed plane, there is no map collision
	mGroundHeight = 3500.0f;
	mGroundHeight += 1.0f;
	if (pos.y <= 0.05f + mGroundHeight) {
		pos.y   = mGroundHeight;
		mLanded = 1;
		mAcceleration.zero();
		mVelocity.zero();
	}

	mLinearVelocity = pos - mPosition;
}

void TLimitKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSpineEnemy::perform(cue, graphics);

	for (int i = 0; i < 10; ++i)
		mFlames[i]->perform(cue, graphics);
	mHead->perform(cue, graphics);
	mHands[0]->perform(cue, graphics);
	mHands[1]->perform(cue, graphics);
	mBody->perform(cue, graphics);

	if (cue & CUE_MOVE) {
		setUpHitActors();
		if (unk150 > 0)
			--unk150;
		if (mHipDropTimer > 0)
			--mHipDropTimer;
		if (unk158 > 0)
			--unk158;
	}

	if (cue & CUE_CALC_ANIM) {
		if (isBreathing()) {
			JGeometry::TVec3<f32> scale;
			scale.setAll(getParam()->flameScale.get());

			JPABaseEmitter* emitter;
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1F3, mMActor->getModel()->getAnmMtx(mHeadJntIdx), 3, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C3, mMActor->getModel()->getAnmMtx(mHeadJntIdx), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C2, mMActor->getModel()->getAnmMtx(mHeadJntIdx), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C1, mMActor->getModel()->getAnmMtx(mHeadJntIdx), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C0, mMActor->getModel()->getAnmMtx(mHeadJntIdx), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
		}
	}
}

void TLimitKoopa::reset()
{
	char trash[0x18];
	TSpineEnemy::reset();
	changeAnm(0xC, getParam()->waitSpeed.get());
	mSpine->reset();
	unk150                = 0;
	mHipDropTimer         = 0;
	unk158                = 0;
	mLanded               = 1;
	mDirection.mDirection = 0.0f;
}

void TLimitKoopa::init(TLiveManager* manager)
{
	mBodyRadius = 800.0f;
	mHeadHeight = 2000.0f;

	TSpineEnemy::init(manager);

	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);

	mSpine->initWith(&TNerveLimitKoopaWait::theNerve());

	if (mMActor->getAnmBck() != nullptr)
		mMActor->getAnmBck()->initSimpleMotionBlend(0x10);

	unk170 = 0.0f;
	reset();

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	for (u16 i = 0; i < jointNames->getNameNum(); ++i) {
	} // empty loop over the joint names in the original

	mAgoJntIdx  = jointNames->getIndex("ago");
	mHeadJntIdx = jointNames->getIndex("head");
	mNeckJntIdx = jointNames->getIndex("neck");

	J3DJoint* headJoint
	    = getModel()->getModelData()->getJointNodePointer(mHeadJntIdx);
	headJoint->setCallBack(KoopaNeckCallBack);
	headJoint->setCallBackUserData(this);
}

void TLimitKoopa::loadAfter()
{
	JDrama::TNameRef::loadAfter();

	for (int i = 0; i < 10; ++i)
		mFlames[i] = new TLimitKoopaFlame(this);

	for (int i = 0; i < 2; ++i)
		mHands[i] = new TLimitKoopaHand(this);

	mHead = new TLimitKoopaHead(this);
	mBody = new TLimitKoopaBody(this);
}

void TLimitKoopa::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
}

TLimitKoopa::TLimitKoopa(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	onLiveFlag(LIVE_FLAG_UNK10);
	mScaledBodyRadius = 1600.0f;
}

namespace {

int KoopaNeckCallBack(J3DNode*, int) { return 1; }

} // namespace

// ============= parts =============

void TLimitKoopaBody::attack_(THitActor* other)
{
	other->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

BOOL TLimitKoopaBody::receiveMessage(THitActor* sender, u32 message)
{
	return true;
}

void TLimitKoopaHead::attack_(THitActor* other)
{
	other->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

BOOL TLimitKoopaHead::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER: {
		TLimitKoopa* koopa = mOwner;
		if (koopa->mSpine->getCurrentNerve()
		    == &TNerveLimitKoopaTumble::theNerve())
			break;
		if (koopa->mSpine->getCurrentNerve()
		    == &TNerveLimitKoopaGetDown::theNerve())
			break;
		if (koopa->mSpine->getCurrentNerve()
		    == &TNerveLimitKoopaStagger::theNerve())
			koopa->mSpine->setNext(&TNerveLimitKoopaGetShowered::theNerve());
		koopa->mSpine->pushNerve(&TNerveLimitKoopaGetShowered::theNerve());
		break;
	}
	}
	return true;
}

void TLimitKoopaHand::attack_(THitActor* other)
{
	other->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

BOOL TLimitKoopaHand::receiveMessage(THitActor* sender, u32 message)
{
	return true;
}

void TLimitKoopaFlame::attack_(THitActor* other)
{
	if (other->receiveMessage(this, HIT_MESSAGE_UNKA)) {
		TLimitKoopa* koopa = mOwner;
		koopa->changeAnm(3, koopa->getParam()->fireSpeed.get());
	}
}

BOOL TLimitKoopaFlame::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		return false;
	}
	return true;
}

void TLimitKoopaParts::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TLiveActor::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		for (int i = 0; i < mColCount; ++i)
			attack_(mCollisions[i]);
	}
}

// ============= nerves =============

BOOL TNerveLimitKoopaGetDown::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();
	char trash[0x38];

	switch (koopa->mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 0:
		if (koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
			koopa->changeAnm(1, koopa->getParam()->downSpeed.get());
		break;
	case 1:
		if (koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
			koopa->changeAnm(7, koopa->getParam()->downSpeed.get());
		break;
	case 7:
		if (koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
			return true;
		break;
	default:
		koopa->changeAnm(0, koopa->getParam()->downSpeed.get());
		break;
	}

	return false;
}

BOOL TNerveLimitKoopaGetShowered::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();
	koopa->changeAnm(0xE, koopa->getParam()->waterhitSpeed.get());
	return koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr) ? TRUE : FALSE;
}

BOOL TNerveLimitKoopaStagger::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();
	koopa->changeAnm(9, koopa->getParam()->staggerSpeed.get());
	return koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr) ? TRUE : FALSE;
}

BOOL TNerveLimitKoopaTumble::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();
	koopa->changeAnm(8, koopa->getParam()->tumbleSpeed.get());
	return koopa->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr) ? TRUE : FALSE;
}

BOOL TNerveLimitKoopaWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();

	if (spine->getTime() == 0) {
		koopa->changeAnm(0xA, 2.0f);
		koopa->unk150 = 0xF0;
	}

	TDirectionCalc target;
	JGeometry::TVec3<f32> toMario;
	toMario.sub(*gpMarioPos, koopa->mPosition);
	toMario.y = 0.0f;
	target.makeDirection(toMario);
	f32 targetDir = target.mDirection;
	koopa->mDirection.mDirection = koopa->mDirection.calcTurnDirection(
	    targetDir, TDirectionCalc::d2r(koopa->getParam()->rotationSpeed.get()));

	TDirectionCalc facing;
	JGeometry::TVec3<f32> toMario2;
	toMario2.x = gpMarioPos->x - koopa->mPosition.x;
	toMario2.y = gpMarioPos->y - koopa->mPosition.y;
	toMario2.z = gpMarioPos->z - koopa->mPosition.z;
	toMario2.y = 0.0f;
	facing.makeDirection(toMario2);

	BOOL facingMario = koopa->mDirection.absDirection(facing.mDirection) <= 0.31415927f;
	if (facingMario && koopa->mMActor->isCurAnmAlreadyEnd(ANM_TYPE_BCK)) {
		spine->pushAfterCurrent(&TNerveLimitKoopaHipDropStart::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveLimitKoopaHipDropStart::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();
	char trash[8];

	if (spine->getTime() == 0) {
		koopa->changeAnm(5, 2.0f);
		koopa->mHipDropTimer = 30;
	}

	if (koopa->mHipDropTimer <= 0) {
		koopa->startHipDrop();
		koopa->mLanded = 0;
		spine->pushAfterCurrent(&TNerveLimitKoopaHipDropJump::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveLimitKoopaHipDropJump::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopa* koopa = (TLimitKoopa*)spine->getBody();

	f32 gravity = -koopa->getGravityY();
	koopa->mAcceleration.set(0.0f, gravity, 0.0f);

	if (koopa->mLanded) {
		spine->pushAfterCurrent(&TNerveLimitKoopaWait::theNerve());
		return true;
	}

	return false;
}

#pragma inline_depth(4)
