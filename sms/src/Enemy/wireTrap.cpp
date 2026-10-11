#include <Enemy/WireTrap.hpp>
#include <Enemy/Launcher.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSe.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

namespace {
const char cMatName[]       = "_mat_1";
const GXColorS10 cRedColor  = { 210, 20, 15, 0 };
const GXColorS10 cBlueColor = { 15, 20, 210, 0 };
} // namespace

// fabricated
static inline JGeometry::TVec3<f32> headingXZ(f32 yaw)
{
	s16 angle = 182.04445f * yaw;
	return JGeometry::TVec3<f32>(1.0f * JMASSin(angle), 0.0f,
	                             1.0f * JMASCos(angle));
}

// Wraps t into the range [l, r).
static inline f32 wrapAround(f32 t, f32 l, f32 r)
{
	return l + std::fmodf((r - l) + (t - l), r - l);
}

TWireTrap::TWireTrap(const char* name)
    : TSpineEnemy(name)
    , unk160(0)
    , mInWaterTimer(0)
{
}

void TWireTrap::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("wire_trap.bmd", 0);
	mBinder       = new TWireBinder;
	mSpine->initWith(&TNerveWaitForever<TLiveActor>::theNerve());
	initHitActor(0x10000026, 2, 0x90000000, 20.0f, 30.0f, 40.0f, 40.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	SMS_LoadParticle("/scene/wireTrap/jpa/ms_wrt_biri_a.jpa", 0x190);
	SMS_LoadParticle("/scene/wireTrap/jpa/ms_wrt_biri_b.jpa", 0x191);
}

void TWireTrap::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);

	int scale;
	int mode;
	int color;
	stream.read(&scale, 4);
	stream.read(&mode, 4);
	stream.read(&mWaitTime, 4);
	stream.read(&color, 4);

	mSpeedScale = scale / 10.0f;
	if (mode == -1)
		mode = 0;
	mMode = mode;

	if (color == -1) {
		s32 index
		    = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
		SMS_InitPacket_OneTevColor(getModel(), index, GX_TEVREG2, &cRedColor);
	} else {
		s32 index
		    = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
		SMS_InitPacket_OneTevColor(getModel(), index, GX_TEVREG2, &cBlueColor);
	}

	getWireBinder()->init(mPosition);
	mSpine->reset();
	mSpine->setNext(getNerveFromMode(mMode));

	const JGeometry::TVec3<f32>& wireDir = getWireBinder()->getDir();
	JGeometry::TVec3<f32> dir            = headingXZ(mRotation.y);
	mDirSign   = 0.0f <= dir.dot(wireDir) ? 1.0f : -1.0f;
	mScaleRate = 1.0f;
}

BOOL TWireTrap::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER: {
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		SMSGetMSound()->startSoundSet(0x6802, &mPosition, 0, 0.0f, 0, 0, 4);
		mInWaterTimer = 30;

		JGeometry::TVec3<f32> diff = mPosition;
		diff -= SMS_GetMarioPos();
		JGeometry::TVec3<f32> dir = getWireBinder()->getDir();
		dir.scale(mDirSign);
		if (0.0f <= diff.dot(dir))
			mInWaterPower = getSaveParam2()->mInWaterPowerRate.get();
		else
			mInWaterPower = -getSaveParam2()->mInWaterPowerRate.get();
		return true;
	}
	case HIT_MESSAGE_TAKE:
		if (mHolder == nullptr) {
			onHitFlag(HIT_FLAG_NO_COLLISION);
			mHolder = (TTakeActor*)sender;
			return true;
		}
		break;
	case HIT_MESSAGE_THROWN:
	case HIT_MESSAGE_UNK8:
		if (mHolder != nullptr) {
			mHolder = nullptr;
			return true;
		}
		break;
	case HIT_MESSAGE_UNKB:
		kill();
		return true;
	}
	char trash[8];
	return TSpineEnemy::receiveMessage(sender, message);
}

void TWireTrap::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		onLiveFlag(LIVE_FLAG_DEAD);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mSpine->reset();
		mSpine->setNext(&TNerveWaitForever<TLiveActor>::theNerve());
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_A, &mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_B, &mPosition, nullptr,
		                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	}
}

void TWireTrap::calcRootMatrix()
{
	if (mHolder) {
		TSpineEnemy::calcRootMatrix();
		return;
	}

	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
	        0x190, &mPosition, 1, this))
		emitter->setGlobalScale(mScaling);
	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
	        0x191, &mPosition, 1, this))
		emitter->setGlobalScale(mScaling);

	JGeometry::TVec3<f32> forward
	    = getWireBinder()->getDirAtPos(mPosition, mDirSign);
	JGeometry::TVec3<f32> up;
	up.set(0.0f, 1.0f, 0.0f);
	forward.scale(mDirSign);

	TPosition3f mtx;
	SMS_CalcToDirMatrix(mtx, forward, up);

	JGeometry::TQuat4<f32> quat;
	mtx.getQuat(quat);

	mRotation.z += -17.75f;
	mRotation.z = wrapAround(mRotation.z, 0.0f, 360.0f);

	JGeometry::TQuat4<f32> spin;
	JGeometry::TVec3<f32> axis;
	axis.set(0.0f, 0.0f, 1.0f);
	spin.setRotate(axis, 0.017453294f * mRotation.z);
	quat.mul(spin);

	mtx.setQuat(quat);
	mtx.setTrans(mPosition);
	getModel()->setBaseTRMtx(mtx);

	JGeometry::TVec3<f32> scale = mScaling;
	scale.scale(mScaleRate);
	getModel()->setBaseScale(scale);
}

void TWireTrap::moveObject()
{
	if (mInWaterTimer > 0) {
		mInWaterTimer -= 1;
		if (mInWaterTimer <= 0)
			mInWaterPower = 0.0f;
	}

	setHitParams(20.0f * mScaling.x, 30.0f * mScaling.y, 40.0f * mScaling.x,
	             40.0f * mScaling.y);
	checkHitActors();
	TLiveActor::moveObject();
	SMSGetMSound()->startSoundActor(0x20BF, &mPosition, 0, nullptr, 0, 4);
}

void TWireTrap::checkHitActors()
{
	THitActor** end = mCollisions + mColCount;
	for (THitActor** it = mCollisions; it != end; ++it) {
		THitActor* actor = *it;
		switch (actor->mActorType) {
		case 0x80000001:
			SMS_SendMessageToMario(this, 9);
			break;
		case 0x10000026: {
			if (actor == this)
				break;

			TWireTrap* other = (TWireTrap*)actor;

			JGeometry::TVec3<f32> thisDir  = getVelocityOnWire();
			JGeometry::TVec3<f32> otherDir = other->getVelocityOnWire();

			if (mCoolTimer <= 0 && mMode == 0) {
				mCoolTimer = 30;

				JGeometry::TVec3<f32> a = getVelocityAlongWireDir();
				JGeometry::TVec3<f32> b = other->getVelocityAlongWireDir();

				if (thisDir.dot(otherDir) < 0.0f)
					mDirSign *= -1.0f;
				mSpine->pushNerve(&TNerveWireTrapWait::theNerve());
			}

			if (other->mCoolTimer <= 0 && other->mMode == 0) {
				other->mCoolTimer = 30;

				JGeometry::TVec3<f32> a = other->getVelocityAlongWireDir();
				JGeometry::TVec3<f32> b = other->getVelocityAlongWireDir();

				if (otherDir.dot(thisDir) < 0.0f)
					other->mDirSign *= -1.0f;
				other->mSpine->pushNerve(&TNerveWireTrapWait::theNerve());
			}
			break;
		}
		}
	}
}

#pragma dont_inline on
const JGeometry::TVec3<f32>& TWireTrap::getWireDir() const
{
	return getWireBinder()->getDir();
}
#pragma dont_inline off

#pragma dont_inline on
const TNerveBase<TLiveActor>* TWireTrap::getNerveFromMode(int mode)
{
	switch (mode) {
	case MODE_RETURN_MOVE:
		return &TNerveWireTrapReturnMove::theNerve();
	case MODE_ONEWAY_MOVE:
		return &TNerveWireTrapOnewayMove::theNerve();
	case MODE_SEARCH:
		return &TNerveWireTrapSearch::theNerve();
	}
	return nullptr;
}
#pragma dont_inline off

TWireBinder* TWireTrap::getWireBinder() const { return (TWireBinder*)mBinder; }

TWireTrapManager::TWireTrapManager(const char* name)
    : TEnemyManager(name)
{
}

TWireTrapParams::TWireTrapParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mInWaterPowerRate, 0.9f)
    , PARAM_INIT(mScaleTimerMax, 60)
    , PARAM_INIT(mGoTimerMax, 90)
{
	TParams::load(mPrmPath);
}

void TWireTrapManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TWireTrapParams("/enemy/wiretrap.prm");
	TEnemyManager::load(stream);
}

void TWireTrapManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "wire_trap.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveWireTrapReturnMove, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (self->mCoolTimer > 0)
		self->mCoolTimer -= 1;

	bool done;
	if (self->getWireBinder()->isEndWire(self->mPosition, self->mDirSign)) {
		self->mDirSign *= -1.0f;
		done = true;
	} else {
		self->mLinearVelocity = self->getVelocityAlongWireDir();
		done                  = false;
	}

	if (done) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveWireTrapOnewayMoveStart, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	self->onHitFlag(HIT_FLAG_NO_COLLISION);

	bool done;
	self->mScaleRate += 1.0f / self->getSaveParam2()->mScaleTimerMax.get();
	if (1.0f <= self->mScaleRate) {
		self->mScaleRate = 1.0f;
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		done = true;
	} else {
		done = false;
	}

	if (done)
		return true;
	return false;
}

DEFINE_NERVE(TNerveWireTrapOnewayMove, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	if (spine->getTime() == 0 && self->mScaleRate < 1.0f) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapGoWait::theNerve());
		spine->pushAfterCurrent(&TNerveWireTrapOnewayMoveStart::theNerve());
		return true;
	}

	if (self->mCoolTimer > 0)
		self->mCoolTimer -= 1;

	bool done;
	if (self->getWireBinder()->isEndWire(self->mPosition, self->mDirSign)) {
		done = true;
	} else {
		self->mLinearVelocity = self->getVelocityAlongWireDir();
		done                  = false;
	}

	if (done) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		spine->pushAfterCurrent(&TNerveWireTrapOnewayMoveEnd::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveWireTrapOnewayMoveEnd, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	self->onHitFlag(HIT_FLAG_NO_COLLISION);

	bool done;
	self->mScaleRate -= 1.0f / self->getSaveParam2()->mScaleTimerMax.get();
	if (self->mScaleRate <= 0.0f) {
		self->mScaleRate = 0.0f;
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		done = true;
	} else {
		done = false;
	}

	if (done) {
		f32 pos = 0.0f < self->mDirSign ? 0.0f : 1.0f;
		self->getWireBinder()->getPoint(&self->mPosition,
		                                0.01f * self->mDirSign + pos);
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveWireTrapSearch, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();

	bool done;
	if (!SMS_IsMarioOnWire()) {
		done = false;
	} else {
		if (self->mCoolTimer > 0)
			self->mCoolTimer -= 1;

		JGeometry::TVec3<f32> diff = SMS_GetMarioPos();
		diff -= self->mPosition;
		f32 dot        = diff.dot(self->getWireBinder()->getDir());
		self->mDirSign = dot > 0.0f ? 1 : dot < 0.0f ? -1 : 0;

		self->mLinearVelocity = self->getVelocityAlongWireDir();
		done                  = false;
	}

	if (done) {
		spine->pushAfterCurrent(this);
		spine->pushAfterCurrent(&TNerveWireTrapWait::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveWireTrapWait, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	return self->mWaitTime < spine->getTime() ? TRUE : FALSE;
}

DEFINE_NERVE(TNerveWireTrapGoWait, TLiveActor)
{
	TWireTrap* self = (TWireTrap*)spine->getBody();
	return self->getSaveParam2()->mGoTimerMax.get() < spine->getTime() ? TRUE
	                                                                    : FALSE;
}
