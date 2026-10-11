#include <Enemy/Kazekun.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* Kazekun_bastable[] = {
	"/scene/Kazekun/bas/kazekun_appear.bas",
	"/scene/Kazekun/bas/kazekun_attack.bas",
	nullptr,
	"/scene/Kazekun/bas/kazekun_vanish.bas",
	"/scene/Kazekun/bas/kazekun_wait.bas",
};

void SMS_CalcToDirMatrix(TPosition3f& mtx, const JGeometry::TVec3<f32>& dir,
                         const JGeometry::TVec3<f32>& up)
{
	JGeometry::TVec3<f32> z = dir;
	if (z.isZero()) {
		z.x = 0.0f;
		z.y = 0.0f;
		z.z = 1.0f;
	} else {
		z.normalize();
	}

	JGeometry::TVec3<f32> x;
	x.cross(up, z);
	if (x.isZero()) {
		x.x = 1.0f;
		x.y = 0.0f;
		x.z = 0.0f;
	} else {
		x.normalize();
	}

	JGeometry::TVec3<f32> y;
	y.cross(z, x);
	y.normalize();

	mtx.setXDir(x);
	mtx.setYDir(y);
	mtx.setZDir(z);
}

// Matrix and quaternion that face dir (y up).
static inline void calcLookQuat(TPosition3f& mtx, JGeometry::TQuat4<f32>& q,
                                const JGeometry::TVec3<f32>& dir)
{
	SMS_CalcToDirMatrix(mtx, dir, JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f));
	mtx.getQuat(q);
}

TKazekun::TKazekun(const char* name)
    : TSmallEnemy(name)
    , mWaitTime(0)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKazekun::init(TLiveManager* live_manager)
{
	mManager = live_manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("kazekun.bmd", 0);
	mSpine->initWith(&TNerveKazekunSearch::theNerve());

	mHeadHeight       = 40.0f;
	mScaledBodyRadius = mBodyRadius = 50.0f;
	initHitActor(0x10000029, 1, 0x80000000, mBodyScale * mBodyRadius,
	             mBodyScale * mHeadHeight, mBodyScale * mBodyRadius,
	             mBodyScale * mHeadHeight);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_appear.jpa",
	                 SCENE_KAZEKUN_JPA_MS_KAZE_APPEAR);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_wind.jpa",
	                 SCENE_KAZEKUN_JPA_MS_KAZE_WIND);
	SMS_LoadParticle("/scene/kazekun/jpa/ms_kaze_blur.jpa",
	                 SCENE_KAZEKUN_JPA_MS_KAZE_BLUR);

	initAnmSound();
	mInitPos.set(mPosition);
	reset();
}

void TKazekun::reset()
{
	mQuat.x = 0.0f;
	mQuat.y = 0.0f;
	mQuat.z = 0.0f;
	mQuat.w = 1.0f;

	mPosition.set(JGeometry::TVec3<f32>(mInitPos));
	onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
	setAnmSound(nullptr);
}

void TKazekun::calcRootMatrix()
{
	if (isTaken()) {
		TSpineEnemy::calcRootMatrix();
		return;
	}

	TPosition3f mtx;
	mtx.setQuat(mQuat);
	mtx.setTrans(mPosition);
	getModel()->setBaseTRMtx(mtx);

	TSpineBase<TLiveActor>* spine = mSpine;
	bool flying = spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveKazekunPreAttack::theNerve()
	              || spine->getLatestNerve() == &TNerveKazekunAttack::theNerve()
	              || spine->getLatestNerve()
	                     == &TNerveKazekunHitWater::theNerve();
	if (flying) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    SCENE_KAZEKUN_JPA_MS_KAZE_WIND, getModel()->getBaseTRMtx(), 1,
		    this);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    SCENE_KAZEKUN_JPA_MS_KAZE_BLUR, getModel()->getBaseTRMtx(), 1,
		    this);
	}
}

void TKazekun::bind() { mLinearVelocity += mVelocity; }

void TKazekun::behaveToWater(THitActor*)
{
	TSpineBase<TLiveActor>* spine = mSpine;
	bool attacking = spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	                 || spine->getLatestNerve()
	                        == &TNerveKazekunPreAttack::theNerve()
	                 || spine->getLatestNerve()
	                        == &TNerveKazekunAttack::theNerve();
	if (attacking) {
		mSpine->reset();
		mSpine->setNext(&TNerveKazekunHitWater::theNerve());
	}
}

const char** TKazekun::getBasNameTable() const { return Kazekun_bastable; }

bool TKazekun::isDamage() const { return isHitWater(); }

bool TKazekun::isHitWater() const
{
	TSpineBase<TLiveActor>* spine = mSpine;
	return spine->getLatestNerve() == &TNerveKazekunTurn::theNerve()
	       || spine->getLatestNerve() == &TNerveKazekunPreAttack::theNerve()
	       || spine->getLatestNerve() == &TNerveKazekunAttack::theNerve();
}

void TKazekun::attackToMario()
{
	if (isDamage())
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
}

bool TKazekun::isCollidMove(THitActor*) { return false; }

void TKazekun::setDeadAnm()
{
	mMActor->getFrameCtrl(ANM_TYPE_BCK)->init(1);
	mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
}

// The extra inline layers (here and in calcLookQuat/calcQuatToDir/
// aimQuatAtVelocity) reproduce the call nesting of the retail build: MWCC only
// inlines to a limited depth, which keeps TVec3::dot/sqrt/getQuat/slerp out of
// line exactly like in the original.
static inline f32 calcLength(const JGeometry::TVec3<f32>& v)
{
	return v.length();
}

static inline f32 calcDistXZ(const JGeometry::TVec3<f32>& v)
{
	return calcLength(v);
}

void TKazekun::flyAroundMario()
{
	JGeometry::TVec3<f32> toMario = *gpMarioPos;
	toMario.y += getSaveParam2()->mTurnOffsetY.get();
	toMario -= mPosition;

	f32 lift
	    = JGeometry::TUtil<f32>::clamp(toMario.y, -400.0f, 400.0f) * 0.0025f;
	toMario.y = 0.0f;

	TKazekunParams* params = getSaveParam2();
	f32 dist               = calcDistXZ(toMario);
	f32 ratio = JGeometry::TUtil<f32>::clamp(dist / params->mAroundDist.get(),
	                                         0.0f, 2.0f);

	TPosition3f mtx;
	JGeometry::TQuat4<f32> q;
	calcLookQuat(mtx, q, toMario);

	JGeometry::TVec3<f32> axis(mtx.at(0, 1), mtx.at(1, 1), mtx.at(2, 1));
	JGeometry::TQuat4<f32> rot;
	rot.setRotate(axis, (2.0f - ratio) * 1.5707964f);
	q.mul(q, rot);
	mQuat = q;

	JGeometry::TVec3<f32> vel(0.0f, 0.0f, 1.0f);
	q.rotate(vel, vel);
	vel.y = lift;
	vel.scale(1.0f + fabsf(lift));
	vel.scale(getSaveParam2()->mAroundSpeed.get());
	mLinearVelocity = vel;
}

void TKazekun::doAttackPose(bool init)
{
	JGeometry::TVec3<f32> toMario = *gpMarioPos;
	toMario -= mPosition;
	toMario.y = 0.0f;

	if (init) {
		TPosition3f mtx;
		JGeometry::TQuat4<f32> q;
		calcLookQuat(mtx, q, toMario);

		JGeometry::TVec3<f32> axis(mtx.at(0, 1), mtx.at(1, 1), mtx.at(2, 1));
		JGeometry::TQuat4<f32> rot;
		rot.setRotate(axis, 1.5707964f);
		q.mul(q, rot);
		mQuat = q;

		JGeometry::TVec3<f32> vel(0.0f, 0.0f, getSaveParam2()->mPoseSpeed.get());
		mQuat.rotate(vel, vel);
		mVelocity = vel;
	}

	JGeometry::TVec3<f32> axis;
	mQuat.rotate(JGeometry::TVec3<f32>(-1.0f, 0.0f, 0.0f), axis);
	JGeometry::TQuat4<f32> rot;
	rot.setRotate(axis, JGeometry::TUtil<f32>::PI()
	                        * getSaveParam2()->mPoseOmegaRate.get());
	JGeometry::TQuat4<f32> q;
	q.mul(rot, mQuat);
	mQuat = q;

	JGeometry::TVec3<f32> cur = mVelocity;
	JGeometry::TVec3<f32> vel(0.0f, 0.0f, cur.length());
	mQuat.rotate(vel, vel);
	mVelocity = vel;
}

TKazekunParams::TKazekunParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mAppearDist, 1000.0f)
    , PARAM_INIT(mAroundDist, 400.0f)
    , PARAM_INIT(mAroundSpeed, 30.0f)
    , PARAM_INIT(mAroundTime, 600)
    , PARAM_INIT(mAttackSpeed, 30.0f)
    , PARAM_INIT(mAirFric, 0.97f)
    , PARAM_INIT(mResetTime, 300)
    , PARAM_INIT(mResetTimeHitting, 1500)
    , PARAM_INIT(mPoseTime, 120)
    , PARAM_INIT(mDicideTiming, 0.1f)
    , PARAM_INIT(mTurnOffsetY, 200.0f)
    , PARAM_INIT(mLostOffsetYUp, 500.0f)
    , PARAM_INIT(mLostOffsetYDown, 500.0f)
    , PARAM_INIT(mPoseSpeed, 7.6f)
    , PARAM_INIT(mPoseOmegaRate, 0.04f)
{
	TParams::load(mPrmPath);
}

TKazekunManager::TKazekunManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TKazekunManager::load(JSUMemoryInputStream& stream)
{
	TKazekunParams* params = new TKazekunParams("/enemy/kazekun.prm");

	unk38 = params;

	params->mSLAttackRadius.set(50);
	params->mSLAttackHeight.set(40);
	params->mSLDamageRadius.set(50);
	params->mSLDamageHeight.set(40);
	TSmallEnemyManager::load(stream);
	unk5C = 0;
}

void TKazekunManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "kazekun.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveKazekunSearch, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0)
		self->reset();

	self->updateSquareToMario();
	f32 dist = self->getSaveParam2()->mAppearDist.get();
	if (self->mDistToMarioSquared <= dist * dist) {
		spine->pushAfterCurrent(&TNerveKazekunAppear::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunAppear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		gpMarioParticleManager->emit(SCENE_KAZEKUN_JPA_MS_KAZE_APPEAR,
		                             &self->mPosition, 0, nullptr);
		self->mMActor->setBck("kazekun_appear");
		self->setCurAnmSound();
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunTurn::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunTurn, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_wait");
		self->setCurAnmSound();
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	self->flyAroundMario();

	bool lost = true;
	f32 dy    = gpMarioPos->y - self->mInitPos.y;
	if (!(dy < -self->getSaveParam2()->mLostOffsetYDown.get())
	    && !(self->getSaveParam2()->mLostOffsetYUp.get() < dy))
		lost = false;

	if (lost) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		return true;
	}

	int time = spine->getTime();
	if ((f32)self->getSaveParam2()->mAroundTime.get() < (f32)time) {
		spine->pushAfterCurrent(&TNerveKazekunPreAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunPreAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->doAttackPose(true);
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_READY, &self->mPosition, 0,
		                          nullptr, 0, 4);
		self->setAnmSound(nullptr);
	}

	if (self->getSaveParam2()->mDicideTiming.get()
	        * self->getSaveParam2()->mPoseTime.get()
	    < spine->getTime())
		self->setGoalPath(TPathNode(JGeometry::TVec3<f32>(*gpMarioPos)));

	self->doAttackPose(false);

	if (self->getSaveParam2()->mPoseTime.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunAttack::theNerve());
		return true;
	}

	return false;
}

static inline void calcQuatToDir(JGeometry::TQuat4<f32>& target,
                                 const JGeometry::TVec3<f32>& dir)
{
	TPosition3f mtx;
	SMS_CalcToDirMatrix(mtx, dir, JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f));
	mtx.getQuat(target);

	JGeometry::TVec3<f32> axis;
	mtx.getYDir(axis);
	JGeometry::TQuat4<f32> rot;
	rot.setRotate(axis, 0.0f);
	target.mul(target, rot);
}

static inline void aimQuatAtVelocity(TKazekun* self)
{
	JGeometry::TQuat4<f32> cur = self->mQuat;
	JGeometry::TVec3<f32> dir  = self->mVelocity;

	JGeometry::TQuat4<f32> target;
	calcQuatToDir(target, dir);

	cur.slerp(target, 0.1f);
	cur.normalize();
	self->mQuat = cur;
}

static inline void calcAttackVelocity(TKazekun* self)
{
	JGeometry::TVec3<f32> vel = self->unk104.getPoint();
	vel -= self->mPosition;
	vel.setLength(self->getSaveParam2()->mAttackSpeed.get());
	self->mVelocity = vel;
}

DEFINE_NERVE(TNerveKazekunAttack, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_attack");
		self->setCurAnmSound();

		calcAttackVelocity(self);

		aimQuatAtVelocity(self);
	}

	aimQuatAtVelocity(self);

	JGeometry::TVec3<f32> vel = self->mVelocity;
	vel.scale(self->getSaveParam2()->mAirFric.get());
	self->mVelocity = vel;

	if (vel.squared() < 1.0f) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->mWaitTime = self->getSaveParam2()->mResetTime.get();
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunDisappear, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_vanish");
		self->setCurAnmSound();
		gpMarioParticleManager->emit(SCENE_KAZEKUN_JPA_MS_KAZE_APPEAR,
		                             &self->mPosition, 0, nullptr);
		self->mVelocity = JGeometry::TVec3<f32>(0.0f);
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunWait::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunWait, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8);
		self->setAnmSound(nullptr);
	}

	if (self->mWaitTime < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveKazekunSearch::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveKazekunHitWater, TLiveActor)
{
	TKazekun* self = (TKazekun*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBck("kazekun_hit");
		self->setCurAnmSound();
		gpMSound->startSoundActor(MSD_SE_EN_KAZEKUN_DOWN, &self->mPosition, 0,
		                          nullptr, 0, 4);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveKazekunDisappear::theNerve());
		self->mWaitTime = self->getSaveParam2()->mResetTimeHitting.get();
		return true;
	}

	self->mVelocity = JGeometry::TVec3<f32>(0.0f);
	return false;
}
