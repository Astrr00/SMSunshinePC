#include <Enemy/ElecNokonoko.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/MapData.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/Camera.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

u8 TElecNokonoko::mReflectSw = 1;

static const char* dennoko_bastable[] = {
	"/scene/dennoko/bas/dennoko_catch1.bas",
	"/scene/dennoko/bas/dennoko_down1.bas",
	"/scene/dennoko/bas/dennoko_elec_down1.bas",
	"/scene/dennoko/bas/dennoko_hit1.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_mogaki1_loop.bas",
	"/scene/dennoko/bas/dennoko_mogaki1_start.bas",
	nullptr,
	nullptr,
	"/scene/dennoko/bas/dennoko_run1_loop.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_shoot1.bas",
	"/scene/dennoko/bas/dennoko_supply1.bas",
	nullptr,
	"/scene/dennoko/bas/dennoko_turn1_loop.bas",
	nullptr,
	nullptr,
};

// Nerves of this unit are only used here and their theNerve() is inlined.
#define DECLARE_INLINE_NERVE(Name)                                             \
	class Name : public TNerveBase<TLiveActor> {                               \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   \
		static const Name& theNerve()                                          \
		{                                                                      \
			static Name instance;                                              \
			return instance;                                                   \
		}                                                                      \
	};

DECLARE_INLINE_NERVE(TNerveElecNokonokoShoot)
DECLARE_INLINE_NERVE(TNerveElecNokonokoCollect)
DECLARE_INLINE_NERVE(TNerveElecNokonokoTurn)
DECLARE_INLINE_NERVE(TNerveElecNokonokoFreeze)
DECLARE_INLINE_NERVE(TNerveElecNokonokoRebirth)
DECLARE_INLINE_NERVE(TNerveElecNokonokoAttack)
DECLARE_INLINE_NERVE(TNerveElecCarapaceMove)
DECLARE_INLINE_NERVE(TNerveElecCarapaceWait)
DECLARE_INLINE_NERVE(TNerveElecCarapaceReturn)

// The nesting depth of these helpers decides which of the TVec3 inlines
// (sub, dot, TUtil<f32>::sqrt) the compiler expands in the callers.
static inline f32 dist(const JGeometry::TVec3<f32>& a,
                       const JGeometry::TVec3<f32>& b)
{
	JGeometry::TVec3<f32> tmp = a;
	tmp.sub(b);
	return tmp.length();
}

static inline f32 distBetween(const JGeometry::TVec3<f32>& a,
                              const JGeometry::TVec3<f32>& b)
{
	return (a - b).length();
}

static inline f32 distToGoal(const JGeometry::TVec3<f32>& a,
                             const JGeometry::TVec3<f32>& b)
{
	return distBetween(a, b);
}

BOOL TNerveElecCarapaceReturn::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[24]; // matching: stack padding
	TElecCarapace* self = (TElecCarapace*)spine->getBody();

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> goal = self->mOwner->mPosition;
		f32 dx                     = goal.x - self->mPosition.x;
		f32 dy                     = goal.y - self->mPosition.y;
		f32 dz                     = goal.z - self->mPosition.z;
		self->mReturnSpeed.x       = 0.015625f * dx;
		self->mReturnSpeed.y       = 0.015625f * dy;
		self->mReturnSpeed.z       = 0.015625f * dz;
		if (self->mOwner->isBckAnm(8))
			self->mOwner->setBckAnm(0);
	}

	if (spine->getTime() < 20) {
		if (self->mOwner->isBckAnm(8))
			self->mOwner->setBckAnm(0);
	}

	TElecNokonoko* owner = self->mOwner;
	if (owner->mSpine->getCurrentNerve()
	    == &TNerveElecNokonokoFreeze::theNerve()) {
		JGeometry::TVec3<f32> diff
		    = owner->mPosition - owner->mCarapace->mPosition;
		bool reached = VECMag(diff) < 200.0f ? true : false;
		if (reached) {
			self->mOwner->onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			self->mOwner->kill();
			gpMarioParticleManager->emitAndBindToPosPtr(
			    PARTICLE_MS_TLS_CHANGE, &self->mPosition, 0, nullptr);
		}
	}

	self->mSpinAngle += self->mOwner->mParams->mSLCarapaceSpinSpeed.get();
	if (self->mSpinAngle > 360.0f)
		self->mSpinAngle -= 360.0f;

	self->mPosition.x += self->mReturnSpeed.x;
	self->mPosition.y += self->mReturnSpeed.y;
	self->mPosition.z += self->mReturnSpeed.z;

	JGeometry::TVec3<f32> goal = self->mOwner->mPosition;
	if (self->mReturnSpeed.x > 0.0f) {
		if (self->mPosition.x > goal.x)
			self->mPosition.x = goal.x;
	} else if (self->mPosition.x < goal.x) {
		self->mPosition.x = goal.x;
	}

	if (self->mReturnSpeed.y > 0.0f) {
		if (self->mPosition.y > goal.y)
			self->mPosition.y = goal.y;
	} else if (self->mPosition.y < goal.y) {
		self->mPosition.y = goal.y;
	}

	if (self->mReturnSpeed.z > 0.0f) {
		if (self->mPosition.z > goal.z)
			self->mPosition.z = goal.z;
	} else if (self->mPosition.z < goal.z) {
		self->mPosition.z = goal.z;
	}

	if (self->mOwner->checkLiveFlag(LIVE_FLAG_DEAD)) {
		self->mScaling.y *= 0.8f;
		if (self->mScaling.y < 0.01f)
			self->kill();
	}

	return false;
}

BOOL TNerveElecCarapaceWait::execute(TSpineBase<TLiveActor>* spine) const
{
	if (spine->getTime() > 60)
		return true;

	return false;
}

BOOL TNerveElecCarapaceMove::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[48]; // matching: stack padding
	TElecCarapace* self = (TElecCarapace*)spine->getBody();

	TElecNokonokoSaveLoadParams* params = self->mOwner->mParams;
	f32 spinSpeed                       = params->mSLCarapaceSpinSpeed.get();
	f32 speed                           = params->mSLCarapaceSpeed.get();
	f32 turnSpeed                       = params->mSLCarapaceTurnSpeed.get();
	if (self->unk175)
		self->walkToCurPathNode(speed, turnSpeed, 0.0f);
	else
		self->zigzagToCurPathNode(speed, turnSpeed, self->unk178,
		                          self->unk17C);

	self->mSpinAngle += spinSpeed;
	if (self->mSpinAngle > 360.0f)
		self->mSpinAngle -= 360.0f;

	if (self->unk184) {
		f32 range = 64.0f * self->mOwner->mParams->mSLCarapaceSpeed.get();
		if (dist(self->unk104.getPoint(), self->mPosition) < range) {
			TElecNokonoko* owner = self->mOwner;
			bool collecting
			    = owner->mSpine->getCurrentNerve()
			              == &TNerveElecNokonokoCollect::theNerve()
			          ? true
			          : false;
			if (!collecting) {
				owner = self->mOwner;
				if (owner->mSpine->getCurrentNerve()
				        != &TNerveSmallEnemyDie::theNerve()
				    && owner->mSpine->getCurrentNerve()
				           != &TNerveElecNokonokoFreeze::theNerve()
				    && owner->mSpine->getCurrentNerve()
				           != &TNerveElecNokonokoCollect::theNerve()) {
					owner->mSpine->setNext(
					    &TNerveElecNokonokoCollect::theNerve());
				}
			}

			spine->pushAfterCurrent(&TNerveElecCarapaceReturn::theNerve());
			return true;
		}
	}

	JGeometry::TVec3<f32> diff = self->unk104.getPoint();
	diff -= self->mPosition;
	diff.y = 0.0f;
	if (!self->unk176) {
		if (MsVECMag2(&diff) < 100.0f) {
			if (self->unk184) {
				spine->pushAfterCurrent(
				    &TNerveElecCarapaceReturn::theNerve());
				return true;
			}

			self->unk184 = 1;
			self->setGoalPath(TPathNode(self->mOwner->mPosition));
		}
	}

	return false;
}

BOOL TNerveElecNokonokoAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();

	if (spine->getTime() == 0 || !self->isBckAnm(3))
		self->setBckAnm(3);

	if (self->checkCurAnmEnd(0))
		return true;

	return false;
}

BOOL TNerveElecNokonokoRebirth::execute(TSpineBase<TLiveActor>* spine) const
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(13);
		self->mCarapaceShot = 0;
		gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_TLS_CHANGE, &self->mCarapace->mPosition, 0, nullptr);
		self->mCarapace->kill();
	}

	if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(88.0f)) {
		gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_TLS_CHANGE, &self->mPosition, 0, nullptr);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveElecNokonokoFreeze::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[16];
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();

	if (spine->getTime() == 0) {
		if (self->hasCarapace()) {
			self->setBckAnm(3);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xCA, self->mMActor->getModel()->getAnmMtx(0), 0, nullptr);
		} else {
			self->setBckAnm(7);
		}
	}

	if (self->isBckAnm(3)) {
		if (self->getCurAnmFrameNo(0) < 25.0f) {
			MtxPtr mtx = self->mMActor->getModel()->getAnmMtx(8);
			self->unk1A8.set(mtx[0][3], mtx[1][3], mtx[2][3]);
			if (JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToPosPtr(
			        0x17E, &self->unk1A8, 1, self))
				emitter->setGlobalScale(self->mScaling);
		}
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(7)) {
			self->setBckAnm(6);
		} else if (self->isBckAnm(6)) {
			if (!self->unsetUnk165() && self->hasCarapace())
				self->setBckAnm(5);
			else
				self->setBckAnm(6);
		} else {
			return true;
		}
	}

	if (spine->getTime() > 400) {
		spine->reset();
		spine->setDefaultNext();
		spine->pushAfterCurrent(&TNerveElecNokonokoRebirth::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveElecNokonokoTurn::execute(TSpineBase<TLiveActor>* spine) const
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();
	char trash[8];

	if (spine->getTime() == 0) {
		self->setBckAnm(16);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	if (self->isBckAnm(15)) {
		if (MsIsInSight(self->mPosition, self->mRotation.y, SMS_GetMarioPos(),
		                self->getSaveParams()->getSLSearchLength(), 60.0f,
		                0.0f))
			self->setBckAnm(14);
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(16)) {
			self->setBckAnm(15);
		} else if (self->isBckAnm(14)) {
			return true;
		}
	}

	if (0.0f == self->mPosition.x - self->mCarapace->mPosition.x
	    && 0.0f == self->mPosition.z - self->mCarapace->mPosition.z)
		self->mPosition.x += 1.0f;

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);

	if (spine->getTime() > 500)
		return true;

	return false;
}

BOOL TNerveElecNokonokoCollect::execute(TSpineBase<TLiveActor>* spine) const
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();

	if (spine->getTime() == 0) {
		if (!self->isBckAnm(0))
			self->setBckAnm(8);

		self->setGoalPath(TPathNode((THitActor*)self->mCarapace));
	}

	TElecCarapace* carapace = self->mCarapace;
	bool moving = carapace->mSpine->getCurrentNerve()
	                      == &TNerveElecCarapaceWait::theNerve()
	                  ? false
	                  : true;
	if (moving)
		self->getMActor()->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
	else
		self->getMActor()->setFrameRate(0.0f, ANM_TYPE_BCK);

	if (self->isBckAnm(0)) {
		int frame = (int)self->getCurAnmFrameNo(0);
		if (frame > 20)
			self->mCarapace->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (frame > 32) {
			self->mCarapaceShot = 0;
			self->mCarapace->kill();
		}

		if (self->checkCurAnmEnd(0))
			return true;
	} else if (spine->getTime() > 800
	           || self->mCarapace->checkLiveFlag(LIVE_FLAG_DEAD)) {
		spine->pushAfterCurrent(&TNerveElecNokonokoRebirth::theNerve());
		return true;
	}

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	return false;
}

BOOL TNerveElecNokonokoShoot::execute(TSpineBase<TLiveActor>* spine) const
{
	TElecNokonoko* self = (TElecNokonoko*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(9);

	if (self->isBckAnm(9)) {
		if (self->checkCurAnmEnd(0)) {
			self->setBckAnm(12);
			self->unk198 = 0;
		}
	} else if (self->isBckAnm(12)) {
		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(60.0f))
			self->mCarapace->appear();

		if (self->getCurAnmFrameNo(0) < 62.0f)
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);

		if (self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(62.0f)) {
			self->mCarapace->shoot();
			self->mCarapaceShot = 1;
		}

		if (self->checkCurAnmEnd(0)) {
			spine->pushAfterCurrent(&TNerveElecNokonokoCollect::theNerve());
			return true;
		}
	}

	return false;
}

void TElecCarapace::reflect(THitActor* actor)
{
	if (mReflectedBy == actor)
		return;

	unk184      = 0;
	mReflectedBy = actor;
	unk176      = 1;
	unk175      = 0;

	JGeometry::TVec3<f32> dir(actor->mPosition.x - mPosition.x, 0.0f,
	                          actor->mPosition.z - mPosition.z);
	if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
		dir.x += 1.0f;

	MsVECNormalize(&dir, &dir);

	f32 axisX = 0.0f;
	f32 axisZ = 0.0f;
	if (abs(dir.z / dir.x) > 1.0f) {
		if (actor->mPosition.z > mPosition.z)
			axisZ = 1.0f;
		else
			axisZ = -1.0f;
	} else {
		if (actor->mPosition.x > mPosition.x)
			axisX = 1.0f;
		else
			axisX = -1.0f;
	}

	f32 scale = -7.0f * (dir.x * axisX + dir.y * 0.0f + dir.z * axisZ);
	mVelocity.x = dir.x * scale;
	mVelocity.y = 2.0f;
	mVelocity.z = dir.z * scale;
	mPosition.y = 2.0f + mGroundHeight;

	unk174 = 0;
	if ((mVelocity.x > 0.0f && mVelocity.z > 0.0f)
	    || (mVelocity.x < 0.0f && mVelocity.z < 0.0f))
		unk174 = 1;

	setGoalPath(TPathNode(mOwner->mPosition));
	char trash[12]; // matching: stack padding
}

BOOL TElecCarapace::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_UNKD) {
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
		                                            &mPosition, 0, nullptr);
		kill();
	}

	if (message == HIT_MESSAGE_TRAMPLE)
		SMS_SendMessageToMario(this, 9);

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return true;

	return false;
}

void TElecCarapace::sendMessage()
{
	int j;
	char trash[80];
	for (int i = 0; i < mColCount; ++i) {
		THitActor* hit = mCollisions[i];
		if (hit->isActorType(0x80000001)) {
			if (SMS_SendMessageToMario(this, 9)) {
				onHitFlag(HIT_FLAG_NO_COLLISION);
				if (mSpine->getCurrentNerve()
				    != &TNerveElecCarapaceWait::theNerve())
					mSpine->pushNerve(&TNerveElecCarapaceWait::theNerve());
			}
		} else if (hit == mOwner) {
			offHitFlag(HIT_FLAG_NO_COLLISION);
		} else if (hit->isActorType(0x1000001)) {
			j = 0;
			TMsRange<s32> range(0, 360);
			for (; j < 5; ++j) {
				rand();
				rand();
				rand();
			}
		} else if (TElecNokonoko::mReflectSw) {
			reflect(hit);
		}
	}
}

void TElecCarapace::calcRootMatrix()
{
	char trash[32]; // matching: stack padding
	MsMtxSetXYZRPH(mMActor->getModel()->getBaseTRMtx(), mPosition.x,
	               mPosition.y, mPosition.z, mRotation.x,
	               mRotation.y + mSpinAngle, mRotation.z);
	mMActor->getModel()->setBaseScale(mScaling);

	if (gpMSound->gateCheck(MSD_SE_EN_DENNOKO_SPARK2)) {
		MSoundSESystem::MSoundSE::startSoundActor(MSD_SE_EN_DENNOKO_SPARK2,
		                                          &mPosition, 0, nullptr, 0, 4);
	}

	JPABaseEmitter* emitter;
	emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x17A, mMActor->getModel()->getAnmMtx(2), 1, this);
	if (emitter)
		emitter->setGlobalScale(mOwner->mScaling);

	emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x17B, mMActor->getModel()->getAnmMtx(2), 1, this);
	if (emitter)
		emitter->setGlobalScale(mOwner->mScaling);

	emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x17C, mMActor->getModel()->getAnmMtx(2), 1, this);
	if (emitter)
		emitter->setGlobalScale(mOwner->mScaling);
}

void TElecCarapace::bind()
{
	control();
	TEnemyAttachment::bind();
}

void TElecCarapace::shoot()
{
	unk180 = 0;
	if (unk150 == 2)
		return;

	JGeometry::TVec3<f32> dir = SMS_GetMarioPos() - mPosition;
	MsVECNormalize(&dir, &dir);

	f32 flyDist = mOwner->mParams->mSLCarapaceFlyDist.get();
	dir.x *= flyDist;
	dir.y *= mPosition.y;
	dir.z *= flyDist;
	dir.x += mPosition.x;
	dir.y += mPosition.y;
	dir.z += mPosition.z;

	unk174 = !unk174;
	mSpinAngle = 0.0f;
	unk150     = 2;
	unk176     = 0;
	unk168     = 0;
	unk184     = 0;
	unk175     = 0;
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mSpine->initWith(&TNerveElecCarapaceMove::theNerve());
	setGoalPath(TPathNode(dir));

	f32 flightSpeed = TMsRange<f32>(3.0f, 5.0f).rand();
	unk178          = flightSpeed * distToGoal(unk104.getPoint(), mPosition);
	unk17C = TMsRange<f32>(20.0f, 30.0f).rand();
}

void TElecCarapace::appear()
{
	if (unk150 != 0)
		return;

	unk150    = 1;
	mPosition = mOwner->mPosition;
	mScaling.x = mScaling.y = mScaling.z = unk164 = mOwner->mScaling.x;
	mBodyRadius                          = 50.0f;
	mReflectedBy                         = nullptr;
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

f32 TElecCarapace::getNowGravity()
{
	return ((TElecNokonokoSaveLoadParams*)mOwner->getSaveParam())
	    ->mSLCarapaceGravity.get();
}

void TElecCarapace::behaveToHitWall(const TBGCheckData* wall)
{
	if (unk180 <= 0 && TElecNokonoko::mReflectSw) {
		unk180 = 1;
		unk184 = 0;
		unk176 = 1;
		unk175 = 1;

		f32 scale = -1.5f
		            * (mLinearVelocity.x * wall->mNormal.x
		               + mLinearVelocity.y * wall->mNormal.y
		               + mLinearVelocity.z * wall->mNormal.z);
		mVelocity.x = scale * wall->mNormal.x;
		mVelocity.y = 3.0f;
		mVelocity.z = scale * wall->mNormal.z;
		mPosition.y = 2.0f + mGroundHeight;
		setGoalPath(TPathNode(mOwner->mPosition));
		char trash[20]; // matching: stack padding
	}
}

void TElecCarapace::kill() { TEnemyAttachment::kill(); }

void TElecCarapace::behaveToHitGround()
{
	if (unk176)
		unk184 = 1;

	if (mGroundPlane->isWaterSurface())
		kill();

	unk176 = 0;
	unk168 = 1;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mVelocity.x = 0.0f;
	mVelocity.y = 0.0f;
	mVelocity.z = 0.0f;
}

void TElecCarapace::setBehavior()
{
	if (mOwner->checkLiveFlag(LIVE_FLAG_DEAD))
		kill();

	if (unk168)
		mPosition.y = mGroundHeight;

	unk168 = 0;
}

void TElecCarapace::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyAttachment::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		if (unk180 != 0) {
			unk180 += 1;
			if (unk180 > 5)
				unk180 = 0;
		}
	}

	if (cue & CUE_ENTRY) {
		if (isState(0) || mOwner->checkLiveFlag(LIVE_FLAG_DEAD)
		    || checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)) {
		} else {
			TCircleShadowRequest request;
			char trash[12]; // matching: stack padding
			request.mPosition = mPosition;
			if (!isAirborne()) {
				request.mPosition.y       = mGroundHeight;
				request.mNeedsGroundCheck = 0;
			}

			request.mRadiusX = request.mRadiusZ = mOwner->mScaledBodyRadius;
			request.mRotationY                  = mRotation.y;
			gpBindShadowManager->request(request, getActorType());
		}
	}
}

void TElecCarapace::loadInit(TSpineEnemy* host, const char* name)
{
	TEnemyAttachment::loadInit(host, name);

	mOwner = (TElecNokonoko*)unk160;
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);

	THitActor::initHitActor(0x1000000B, 3, 0x98000000, 80.0f, 80.0f, 60.0f,
	                        60.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	unk150 = 0;
	mSpine->initWith(&TNerveElecCarapaceMove::theNerve());

	TMsRange<s32> range(0, 300);
	if (range.rand() < 150)
		unk174 = 0;

	mHeadHeight = 80.0f;
}

TElecCarapace::TElecCarapace(const char* name)
    : TEnemyAttachment(name)
    , mOwner(nullptr)
    , mReflectedBy(nullptr)
    , unk174(1)
    , unk175(0)
    , unk176(0)
    , unk178(0.0f)
    , unk17C(0.0f)
    , unk180(0)
    , unk184(0)
    , mSpinAngle(0.0f)
    , unk198(0.0f)
{
}

void TElecNokonoko::genRandomItem()
{
	mCarapace->kill();
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_TLS_CHANGE,
	                                            &mPosition, 0, nullptr);
	TSmallEnemy::genRandomItem();

	if (checkLiveFlag(LIVE_FLAG_MELT_ON_DEATH)) {
		TMapObjBase* obj = gpItemManager->makeObjAppear(
		    mCarapace->mPosition.x, mCarapace->mPosition.y,
		    mCarapace->mPosition.z, 0x2000000E, true);
		if (obj)
			obj->setVelocityAndFlag10(0.0f, 20.0f, 0.0f);
	}
}

void TElecNokonoko::setMeltAnm()
{
	setBckAnm(2);
	onLiveFlag(LIVE_FLAG_UNK8);
	JGeometry::TVec3<f32> zero(0.0f, 0.0f, 0.0f);
	char trash[4];
	unk18C = 3;
	mCarapace->setVelocity(zero);

	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0xCA, mMActor->getModel()->getAnmMtx(0), 0, nullptr))
		emitter->setGlobalScale(mScaling);
}

void TElecNokonoko::setDeadAnm() { setBckAnm(1); }

void TElecNokonoko::setRunAnm()
{
	if (!isBckAnm(10))
		setBckAnm(11);
}

void TElecNokonoko::setWalkAnm()
{
	if (!isBckAnm(10))
		setBckAnm(11);
}

void TElecNokonoko::setWaitAnm()
{
	unk198 = 0;
	setBckAnm(17);
}

const char** TElecNokonoko::getBasNameTable() const
{
	return dennoko_bastable;
}

void TElecNokonoko::behaveToWater(THitActor* actor)
{
	if (isBckAnm(12) && getCurAnmFrameNo(0) > 58.0f)
		return;

	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		if (!isBckAnm(0) || mCarapaceShot != 0) {
			unk165                  = 1;
			mSprayedByWaterCooldown = 0;
			if (mSpine->getCurrentNerve()
			    != &TNerveElecNokonokoFreeze::theNerve())
				mSpine->pushNerve(&TNerveElecNokonokoFreeze::theNerve());
		}
	}
}

void TElecNokonoko::behaveToFindMario()
{
	char trash[8];
	mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
	mSpine->pushAfterCurrent(&TNerveWalkerAttack::theNerve());
	mSpine->pushAfterCurrent(&TNerveElecNokonokoTurn::theNerve());
	setGoalPath(TPathNode((THitActor*)gpMarioAddress));
}

bool TElecNokonoko::isResignationAttack()
{
	f32 range = mParams->mSLCarapaceShootRange.get();
	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if (dist(unk104.getPoint(), mPosition) < range) {
			mSpine->pushAfterCurrent(&TNerveElecNokonokoShoot::theNerve());
			return true;
		}
	}

	return false;
}

BOOL TElecNokonoko::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	if (message == HIT_MESSAGE_UNKD || message == HIT_MESSAGE_UNKB) {
		onLiveFlag(LIVE_FLAG_DEAD);
		kill();
	}

	if (message == HIT_MESSAGE_TAKE && mHolder == nullptr) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		return true;
	}

	if ((message == HIT_MESSAGE_PUT || message == HIT_MESSAGE_THROWN)
	    && mHolder == (TTakeActor*)sender) {
		mHolder = nullptr;
		return true;
	}

	if (message == HIT_MESSAGE_TRAMPLE) {
		if (mCarapaceShot == 1) {
			mHitPoints = 1;
			kill();
			return true;
		}

		SMS_SendMessageToMario(this, 9);
		return false;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (!changeByJuice())
			behaveToWater(sender);
		else
			mCarapace->kill();
		return true;
	}

	return false;
}

void TElecNokonoko::sendAttackMsgToMario()
{
	if (mCarapaceShot == 0)
		SMS_SendMessageToMario(this, 9);
	else
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
}

void TElecNokonoko::calcRootMatrix()
{
	char trash[24];
	TSpineEnemy::calcRootMatrix();

	if (hasCarapace()) {
		if (mSpine->getCurrentNerve() != &TNerveElecNokonokoFreeze::theNerve()
		    && mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()
		    && mSpine->getCurrentNerve()
		           != &TNerveElecNokonokoCollect::theNerve()) {
			SMSGetMSound()->startSoundActor(MSD_SE_EN_DENNOKO_SPARK1,
			                                &mPosition, 0, nullptr, 0, 4);

			JPABaseEmitter* emitter;
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x17A, mMActor->getModel()->getAnmMtx(7), 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);

			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x17B, mMActor->getModel()->getAnmMtx(7), 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);

			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x17C, mMActor->getModel()->getAnmMtx(7), 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);
		}
	}

	if (mCurrentBckAnm == 2) {
		JPABaseEmitter* emitter;
		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x17D, mMActor->getModel()->getAnmMtx(0), 1, this);
		if (emitter)
			emitter->setGlobalScale(mScaling);

		MtxPtr mtx = mMActor->getModel()->getAnmMtx(8);
		unk1A8.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		emitter = gpMarioParticleManager->emitAndBindToPosPtr(0x17E, &unk1A8,
		                                                      1, this);
		if (emitter)
			emitter->setGlobalScale(mScaling);

		if (getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(72.0f)) {
			emitter = gpMarioParticleManager->emitAndBindToPosPtr(
			    0x17F, &unk1A8, 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);
		}
	}
}

void TElecNokonoko::attackToMario()
{
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		TSmallEnemy::attackToMario();

		if (mSpine->getCurrentNerve() != &TNerveElecNokonokoAttack::theNerve()
		    && mSpine->getCurrentNerve()
		           != &TNerveElecNokonokoCollect::theNerve()
		    && mSpine->getCurrentNerve()
		           != &TNerveElecNokonokoShoot::theNerve()) {
			if (hasCarapace())
				mSpine->pushNerve(&TNerveElecNokonokoAttack::theNerve());
		}
	}
}

void TElecNokonoko::moveObject()
{
	TWalkerEnemy::moveObject();

	if (isBckAnm(11) && checkCurAnmEnd(0))
		setBckAnm(10);
}

void TElecNokonoko::setMActorAndKeeper()
{
	char trash[8];
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("dennoko_model1.bmd", 3);
	MActor* actor = mMActor;
	TElecNokonokoManager* mgr = (TElecNokonokoManager*)mManager;
	actor->getModel()->getModelData()->setMaterialTable(mgr->mMatTable,
	                                                    J3DMatCopyFlag_All);
	actor->initDL();
	actor->getModel()->lock();
}

void TElecNokonoko::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
}

void TElecNokonoko::rest()
{
	TWalkerEnemy::reset();
	mScaledBodyRadius = 140.0f;
}

void TElecNokonoko::init(TLiveManager* manager)
{
	char trash[4];
	TWalkerEnemy::init(manager);
	mActorType = 0x1000000A;
	unk150     = 0x11;
	mParams    = (TElecNokonokoSaveLoadParams*)getSaveParam();
	mCarapace  = new TElecCarapace;
	mSpine->initWith(&TNerveWalkerGraphWander::theNerve());
	mCarapace->loadInit(this, "koura_model1.bmd");

	MActor* actor = mCarapace->getMActor();
	TElecNokonokoManager* mgr = (TElecNokonokoManager*)mManager;
	actor->getModel()->getModelData()->setMaterialTable(mgr->mMatTable,
	                                                    J3DMatCopyFlag_All);
	actor->initDL();
	actor->getModel()->lock();

	TMsRange<s32> range(0, 300);
	unk19C = range.rand();
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

TElecNokonoko::TElecNokonoko(const char* name)
    : TWalkerEnemy(name)
    , mCarapace(nullptr)
    , unk198(0)
    , mCarapaceShot(0)
{
}

void TElecNokonokoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);

	for (int i = 0; i < getActiveObjNum(); ++i)
		((TElecNokonoko*)getObj(i))->mCarapace->perform(cue, graphics);
}

void TElecNokonokoManager::clipEnemies(JDrama::TGraphics* graphics)
{
	char trash[8];
	f32 clipRadius;
	f32 farClip;
	if (unk38 == nullptr) {
		clipRadius = 300.0f;
		farClip    = gpConductor->unk84.mEnemyFarClip.get();
	} else {
		farClip    = unk38->mSLFarClip.get();
		clipRadius = unk38->mSLClipRadius.get();
	}

	SetViewFrustumClipCheckPerspective(gpCamera->getFovy(),
	                                   gpCamera->getAspect(),
	                                   graphics->getNearPlane(), farClip);

	for (int i = 0; i < getObjNum(); ++i) {
		TElecNokonoko* enemy = (TElecNokonoko*)getObj(i);
		if (ViewFrustumClipCheck(graphics, &enemy->mPosition, clipRadius))
			enemy->offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
		else
			enemy->onLiveFlag(LIVE_FLAG_CLIPPED_OUT);

		TElecCarapace* carapace = enemy->mCarapace;
		if (!carapace->isState(0)) {
			if (ViewFrustumClipCheck(graphics, &carapace->mPosition,
			                         clipRadius))
				enemy->mCarapace->offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
			else
				enemy->mCarapace->onLiveFlag(LIVE_FLAG_CLIPPED_OUT);
		}
	}
}

void TElecNokonokoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "dennoko_model1.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSmallEnemy* TElecNokonokoManager::createEnemyInstance()
{
	return new TElecNokonoko;
}

void TElecNokonokoManager::initSetEnemies() { }

void TElecNokonokoManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TElecNokonokoSaveLoadParams("/enemy/elecNokonoko.prm");
	mMatTable = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRFileLoader::getGlbResource("/scene/dennoko/dennoko_model1.bmt"));
}

TElecNokonokoManager::TElecNokonokoManager(const char* name)
    : TSmallEnemyManager(name)
{
}
