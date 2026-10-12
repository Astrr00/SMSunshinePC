#include <Animal/BeeHive.hpp>
#include <Animal/boid.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/MarioFlags.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <M3DUtil/InfectiousStrings.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

namespace {
f32 cAngleLimit = JGeometry::TUtil<f32>::PI() * 0.375f;

// Every state sets up the swarm the same way before choosing a goal.
inline void setBoidParams(TBeeHive* hive)
{
	hive->unk150->mBaseSpeed         = 25.0f;
	hive->unk150->mNeighborRadius    = 80.0f;
	hive->unk150->mYawSpeed          = 8.0f;
	hive->unk150->mPitchSpeed        = 8.0f;
	hive->unk150->mMaxPitch          = 85.0f;
	hive->unk150->mAlignmentStrength = 0.001f;
}

inline void chaseMario(TBeeHive* hive, f32 height)
{
	setBoidParams(hive);
	hive->unk150->setUnk38(TPathNode((THitActor*)gpMarioAddress));
	hive->unk150->mGoalOffset = JGeometry::TVec3<f32>(0.0f, height, 0.0f);
}

inline void stayAtHive(TBeeHive* hive)
{
	setBoidParams(hive);
	hive->unk150->setUnk38(hive->mPosition);
	hive->unk150->mGoalOffset = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
}

inline bool isMarioInWater()
{
	return (SMS_CheckMarioFlag(MARIO_FLAG_VISIBLE)
	        || SMS_CheckMarioFlag(MARIO_FLAG_IN_SHALLOW_WATER))
	       || (*gpMarioGroundPlane)->isWaterSurface()
	       || SMS_CheckMarioFlag(MARIO_FLAG_IN_WATER);
}

inline bool isMarioGivenUp(TBeeHive* hive)
{
	if (hive->mNumBees == 0)
		return false;

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff -= hive->getCenterOfGravity();
	f32 range = hive->getSaveParam2()->mGiveupRange.get();
	return range * range <= diff.squared();
}
} // namespace

#define DEFINE_THE_NERVE(Name)                                                 \
	inline const Name& Name::theNerve()                                        \
	{                                                                          \
		static Name instance;                                                  \
		return instance;                                                       \
	}

DEFINE_THE_NERVE(TNerveBeeHiveWait)
DEFINE_THE_NERVE(TNerveBeeHiveFall)
DEFINE_THE_NERVE(TNerveBeeHiveBreak)
DEFINE_THE_NERVE(TNerveBeeHiveAttack)
DEFINE_THE_NERVE(TNerveBeeHiveMarioWaterIn)
DEFINE_THE_NERVE(TNerveBeeHiveReset)

#undef DEFINE_THE_NERVE

void TBee::init()
{
	initHitActor(0x1000002F, 1, 0x80000000, 20.0f, 20.0f, 50.0f, 50.0f);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

BOOL TBee::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	switch (message) {
	case HIT_MESSAGE_TAKE:
		if (mHolder == nullptr) {
			mHolder = (TTakeActor*)sender;
			SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &mPosition, nullptr,
			                     JGeometry::TVec3<f32>(1.0f));
			return TRUE;
		}
		break;
	case HIT_MESSAGE_UNK8:
		if (mHolder != nullptr) {
			mHolder = nullptr;
			return TRUE;
		}
		break;
	case HIT_MESSAGE_UNKB:
		unkA8->receiveMessageFromChild(this);
		return TRUE;
	}
	return FALSE;
}

TBeeHive::TBeeHive(const char* name)
    : TRealoid(name)
{
	mItem = nullptr;
}

void TBeeHive::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	onLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK10);
	mSpine->initWith(&TNerveBeeHiveWait::theNerve());

	mInitialPosition = mPosition;
	mNumBees         = 0;
	mNumDead         = 0;
	initHitActor(0x10000031, 0, 0x80000000, 50.0f, 50.0f, 100.0f, 100.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
}

void TBeeHive::reset()
{
	mPosition = mInitialPosition;

	TRotation3f mtx;
	MsMtxSetRotRPH(mtx, mRotation.x, 0.0f, mRotation.z);
	mtx.getQuat(unk178);

	unk168.setEulerY(mRotation.y);
	unk158 = unk168;

	mAngle      = 0.0f;
	mAngularVel = 0.0f;
	mAngleLimit = 0.015707964f;
	mCollisionIdx = 0;
	onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);
	mMActor = mMActorKeeper->getMActor("bee_nest.bmd");
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

#pragma dont_inline on
void TBeeHive::receiveMessageFromChild(TBee* bee)
{
	char trash[0x10];
	if (bee->checkFlag(TRealoidActor::FLAG_UNK4))
		return;

	bee->onFlag(TRealoidActor::FLAG_UNK4);
	bee->onHitFlag(HIT_FLAG_NO_COLLISION);

	TMapObjBase* coin = mCoins[mNumDead];
	if (mNumDead != unk150->mNumBoids - 1)
		coin = gpItemManager->makeObjAppear(0x2000000E);

	if (coin != nullptr) {
		coin->appear();
		coin->mPosition = bee->mPosition;
		coin->setVelocityAndFlag10(0.0f, 15.0f, 0.0f);
	}

	++mNumDead;
}
#pragma dont_inline off

void TBeeHive::load(JSUMemoryInputStream& stream)
{
	loadDefault(stream, "bee_body.bmd", 2);

	u32 eventId;
	u32 lastEventId;
	stream >> eventId;
	stream >> lastEventId;

	mItem  = TMapObjBaseManager::newAndRegisterObjByEventID(eventId, "");
	mCoins = new TMapObjBase*[unk150->mNumBoids];

	TMapObjBase** coin = mCoins;
	TMapObjBase** last = mCoins + unk150->mNumBoids - 1;
	for (; coin != last; ++coin)
		*coin = TMapObjBaseManager::newAndRegisterObj("coin");

	*coin = TMapObjBaseManager::newAndRegisterObjByEventID(lastEventId, "");
	if (*coin == nullptr)
		*coin = TMapObjBaseManager::newAndRegisterObj("coin");

	mMActorKeeper->createMActor("bee_nest_break.bmd", 3);
	mMActor = mMActorKeeper->createMActor("bee_nest.bmd", 3);

	for (int i = 0; i < unk150->mNumBoids; ++i) {
		TRealoidActor* bee = unk154[i];
		bee->init();
		bee->onFlag(TRealoidActor::FLAG_UNK2);
	}

	reset();
}

TBee* TBeeHive::createRealoidActor(MActor* actor)
{
	return new TBee(actor, this);
}

// Retail calls TVec3::dot/scale out of line from the inlined normalize().
#pragma dont_inline on
static f32 dotOutOfLine(const JGeometry::TVec3<f32>& a,
                        const JGeometry::TVec3<f32>& b)
{
	return a.dot(b);
}

static void scaleOutOfLine(JGeometry::TVec3<f32>& dst, f32 s,
                           const JGeometry::TVec3<f32>& src)
{
	dst.scale(s, src);
}

static f32 invSqrtOutOfLine(f32 x) { return JGeometry::TUtil<f32>::inv_sqrt(x); }

static void subOutOfLine(JGeometry::TVec3<f32>& dst,
                         const JGeometry::TVec3<f32>& src)
{
	dst.sub(src);
}
#pragma dont_inline off

BOOL TBeeHive::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER: {
		JGeometry::TVec3<f32> dir = mPosition;
		dir.sub(*gpMarioPos);

		if (mSpine->getLatestNerve() != &TNerveBeeHiveFall::theNerve()) {
			JGeometry::TVec3<f32> flat = dir;
			flat.y                     = 0.0f;
			f32 lsq = dotOutOfLine(flat, flat);
			if (lsq <= JGeometry::TUtil<f32>::epsilon())
				flat.zero();
			else
				scaleOutOfLine(flat,
				               1.0f * invSqrtOutOfLine(lsq),
				               flat);

			f32 sign;
			if (fabsf(mAngularVel) < 0.0001f) {
				sign = 1.0f;
			} else {
				int s = mAngularVel > 0.0f ? 1 : (mAngularVel < 0.0f ? -1 : 0);
				sign  = (f32)s;
			}

			unk158.setRotate(JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f), flat);
			mAngularVel += sign * getSaveParam2()->mShakePower.get();
		}

		TRotation3f mtx;
		f32 x = sender->mPosition.x;
		f32 y = sender->mPosition.y;
		f32 z = sender->mPosition.z;
		mtx.identity33();
		mtx.ref(0, 3) = x;
		mtx.ref(1, 3) = y;
		mtx.ref(2, 3) = z;
		gpMarioParticleManager->emitAndBindToMtx(PARTICLE_MS_ENM_WATHIT, mtx,
		                                         0, nullptr);
		SMSGetMSound()->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK,
		                              &sender->mPosition, 0, 0.0f, 0, 0, 4);
		return TRUE;
	}
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
	case HIT_MESSAGE_PUNCH:
		if (mSpine->getLatestNerve() == &TNerveBeeHiveWait::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveBeeHiveFall::theNerve());
		}
		return TRUE;
	}
	return FALSE;
}

void TBeeHive::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TRealoid::perform(cue, graphics);
	TSpineEnemy::perform(cue, graphics);
}

void TBeeHive::control()
{
	controlCollision();
	TLiveActor::control();
	controlSound();
}

void TBeeHive::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	JGeometry::TVec3<f32> pos = mPosition;
	pos += mLinearVelocity;
	pos += mVelocity;

	mVelocity.y -= getGravityY();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	f32 offset = -60.0f * (-1.0f + JMASCos(182.04445f * (57.295776f * mAngle)));
	mGroundHeight
	    = gpMap->checkGround(pos.x, pos.y + mHeadHeight + offset, pos.z,
	                         &mGroundPlane);
	mGroundHeight += 1.0f;

	if (pos.y + offset <= mGroundHeight + 0.05f) {
		if (mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL))
			kill();

		offLiveFlag(LIVE_FLAG_AIRBORNE);
		mVelocity.set(0.0f, 0.0f, 0.0f);
		pos.y = mGroundHeight - offset;
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&pos.x, pos.y + mHeadHeight, &pos.z,
	                                 mBodyRadius);

	JGeometry::TVec3<f32> delta = pos;
	subOutOfLine(delta, mPosition);
	mLinearVelocity = delta;
}

#pragma dont_inline on
void TBeeHive::controlCollision()
{
	int idx = mCollisionIdx;
	int num = mNumBees;
	TRealoidActor* bee = unk154[idx];
	bee->checkHitActors();
	bee->onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	if (num <= ++mCollisionIdx)
		mCollisionIdx = 0;

	idx = mCollisionIdx;
	if (num <= idx)
		idx = 0;
	bee = unk154[idx];
	if (!bee->checkFlag(TRealoidActor::FLAG_UNK2_OR_UNK4))
		bee->offHitFlag(HIT_FLAG_CANNOT_ATTACK);
}
#pragma dont_inline off

void TBeeHive::controlSound()
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	if (mNumBees == 0)
		return;

	f32 x     = 0.0f;
	f32 y     = 0.0f;
	f32 z     = 0.0f;
	int count = 0;
	for (int i = 0; i < mNumBees; ++i) {
		TRealoidActor* bee = unk154[i];
		if (!bee->checkFlag(TRealoidActor::FLAG_UNK2_OR_UNK4)) {
			++count;
			x += bee->mPosition.x;
			y += bee->mPosition.y;
			z += bee->mPosition.z;
		}
	}

	if (count != 0) {
		f32 inv       = 1.0f / count;
		x *= inv;
		y *= inv;
		z *= inv;
		mBeesCenter.x = x;
		mBeesCenter.y = y;
		mBeesCenter.z = z;
		SMSGetMSound()->startBeeSe(&mBeesCenter, count);
	}
}

void TBeeHive::calcRootMatrix()
{
	JGeometry::TQuat4<f32> q = unk178;
	JGeometry::TQuat4<f32> swing;
	swing.setEulerX(mAngle);
	q.mul(q, unk168);
	q.mul(q, swing);

	TRotation3f mtx;
	mtx.setSQ(mScaling, q);
	mtx.ref(0, 3) = mPosition.x;
	mtx.ref(1, 3) = mPosition.y;
	mtx.ref(2, 3) = mPosition.z;
	mtx.ref(1, 3) += 120.0f;
	PSMTXCopy(mtx, getModel()->getBaseTRMtx());
}

bool TBeeHive::doWait()
{
	f32 prevAngularVel = mAngularVel;
	f32 spring = mAngle * -getSaveParam2()->mRebound.get();
	mAngularVel += spring;
	mAngularVel *= getSaveParam2()->mDecay.get();
	mAngle += mAngularVel;

	if (mAngle < -mAngleLimit || mAngleLimit < mAngle) {
		mAngleLimit += getSaveParam2()->mAngleMaxAdd.get();
		mAngleLimit = JGeometry::TUtil<f32>::clamp(mAngleLimit, 0.0f,
		                                            1.5707964f);
		mAngle      = JGeometry::TUtil<f32>::clamp(mAngle, -mAngleLimit,
		                                           mAngleLimit);
		mAngularVel = 0.0f;

		f32 swing = fabsf(mAngle);
		SMSGetMSound()->startSoundActorWithInfo(MSD_SE_EN_BEENEST_SWING,
		                                        &mPosition, nullptr, swing, 0,
		                                        0, nullptr, 0, 4);
	}

	if (!JGeometry::TUtil<f32>::epsilonEquals(0.0f, mAngularVel)
	    && prevAngularVel * mAngularVel <= 0.0f) {
		f32 swing = fabsf(mAngle);
		SMSGetMSound()->startSoundActorWithInfo(MSD_SE_EN_BEENEST_SWING,
		                                        &mPosition, nullptr, swing, 0,
		                                        0, nullptr, 0, 4);
	}

	unk168.slerp(unk158, 0.01f);
	unk168.normalize();

	JGeometry::TVec3<f32> diff = *gpMarioPos;
	diff.sub(mPosition);
	f32 range = getSaveParam2()->mSearchRange.get();
	if (diff.squared() <= range * range) {
		chaseMario(this, 200.0f);
	} else {
		stayAtHive(this);
	}

	return cAngleLimit <= fabsf(mAngle);
}

#pragma dont_inline on
void TBeeHive::appearBee(int idx)
{
	TRealoidActor* bee = unk154[idx];
	if (bee->checkFlag(TRealoidActor::FLAG_UNK4))
		return;
	if (!bee->checkFlag(TRealoidActor::FLAG_UNK2))
		return;

	bee->offFlag(TRealoidActor::FLAG_UNK2);
	bee->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk150->getBoid(idx)->mPosition = mPosition;
}
#pragma dont_inline off

JGeometry::TVec3<f32> TBeeHive::getCenterOfGravity() const
{
	int num = mNumBees;
	if (num == 0)
		return JGeometry::TVec3<f32>(0.0f);

	JGeometry::TVec3<f32> sum(0.0f);
	char trash[0x20];
	TBoidLeader* boids = unk150;
	for (int i = 0; i < num; ++i)
		sum += boids->getBoid(i)->mPosition;
	sum /= (f32)num;
	return sum;
}

TBeeHiveManager::TBeeHiveManager(const char* name)
    : TEnemyManager(name)
{
}

void TBeeHiveManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBeeHiveParams("/Animal/beehive.prm");
	TEnemyManager::load(stream);
}

void TBeeHiveManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bee_body.bmd", 0x10210000, 0 },
		{ "bee_nest.bmd", 0x10210000, 0 },
		{ "bee_nest_break.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

BOOL TNerveBeeHiveWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TBeeHive* self = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10 | LIVE_FLAG_AIRBORNE);
		if (self->mNumBees < 3) {
			for (int i = 0; i < 3; ++i)
				self->appearBee(i);
			self->mNumBees = 3;
		}

		stayAtHive(self);
		char trash[0x48];
	}

	if (self->doWait()) {
		spine->pushAfterCurrent(&TNerveBeeHiveFall::theNerve());
		return true;
	}

	if (self->getSaveParam2()->mDecrimentTimer.get() < spine->getTime()) {
		if (self->mNumBees > 3) {
			--self->mNumBees;
			TRealoidActor* bee = self->unk154[self->mNumBees];
			if (!bee->checkFlag(TRealoidActor::FLAG_UNK2)) {
				bee->onFlag(TRealoidActor::FLAG_UNK2);
				bee->onHitFlag(HIT_FLAG_NO_COLLISION);
			}
		}
		spine->pushAfterCurrent(this);
		return true;
	}
	return false;
}

BOOL TNerveBeeHiveFall::execute(TSpineBase<TLiveActor>* spine) const
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	TBeeHive* self = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_UNK10);
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		SMSGetMSound()->startSoundActor(MSD_SE_EN_BEENEST_OFF,
		                                &self->mPosition, 0, nullptr, 0, 4);

		JGeometry::TVec3<f32> dir;
		int pad4;
		self->unk168.getZDir(dir);
		dir.scale(1.0f);

		self->mAngularVel = self->getSaveParam2()->mFallAngularVel.get();
		self->mVelocity   = dir;

		TMapObjBase* item;
		if ((item = self->mItem) != nullptr) {
			if (item->isActorType(0x2000000E))
				item = gpItemManager->makeObjAppear(0x2000000E);

			if (item != nullptr) {
				item->appear();
				item->JSGSetTranslation(self->mPosition);
				item->setVelocityAndFlag10(0.0f, 0.0f, 0.0f);
				item->onLiveFlag(LIVE_FLAG_AIRBORNE);
				self->mItem = nullptr;
			}
		}
	}

	self->mAngle += self->mAngularVel;

	if (!self->checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
		spine->pushAfterCurrent(&TNerveBeeHiveBreak::theNerve());
		return true;
	}
	return false;
}

BOOL TNerveBeeHiveBreak::execute(TSpineBase<TLiveActor>* spine) const
{
	TBeeHive* self = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		self->mMActor = self->mMActorKeeper->getMActor("bee_nest_break.bmd");
		self->mMActor->setBck("bee_nest_break");
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_A, &self->mPosition, self,
		                     JGeometry::TVec3<f32>(2.0f));
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_DISAP_B, &self->mPosition, self,
		                     JGeometry::TVec3<f32>(2.0f));
		SMSGetMSound()->startSoundActor(MSD_SE_EN_BEENEST_LAND,
		                                &self->mPosition, 0, nullptr, 0, 4);

		for (int i = 0; i < self->unk150->mNumBoids; ++i)
			self->appearBee(i);
		self->mNumBees = self->unk150->mNumBoids;

		chaseMario(self, 200.0f);
		char trash[0x58];
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
		return true;
	}
	return false;
}

BOOL TNerveBeeHiveAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	TBeeHive* self = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		chaseMario(self, 200.0f);
		char trash[0x38];
	}

	if (isMarioInWater()) {
		spine->pushAfterCurrent(&TNerveBeeHiveMarioWaterIn::theNerve());
		return true;
	}

	if (isMarioGivenUp(self)) {
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
		return true;
	}
	return false;
}

BOOL TNerveBeeHiveMarioWaterIn::execute(TSpineBase<TLiveActor>* spine) const
{
	TBeeHive* self = (TBeeHive*)spine->getBody();
	if (spine->getTime() == 0) {
		chaseMario(self, 500.0f);
		char trash[0x18];
	}

	if (self->getSaveParam2()->mGiveupTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveBeeHiveReset::theNerve());
		return true;
	}

	if (!isMarioInWater()) {
		spine->pushAfterCurrent(&TNerveBeeHiveAttack::theNerve());
		return true;
	}
	char trash2[0x20];
	return false;
}

BOOL TNerveBeeHiveReset::execute(TSpineBase<TLiveActor>* spine) const
{
	TBeeHive* self = (TBeeHive*)spine->getBody();
	char trash[8];
	if (spine->getTime() == 0) {
		self->reset();
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->mScaling.setAll(0.0f);
	}

	bool done = false;
	self->mScaling.x += 0.01f;
	if (1.0f <= self->mScaling.x) {
		self->mScaling.setAll(1.0f);
		done = true;
	} else {
		self->mScaling.y += 0.01f;
		self->mScaling.z += 0.01f;
	}

	if (done) {
		spine->pushAfterCurrent(&TNerveBeeHiveWait::theNerve());
		return true;
	}
	return false;
}
