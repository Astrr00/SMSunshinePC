#include <Enemy/Yunbo.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/Map.hpp>
#include <Player/MarioAccess.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* sambohead_bastable[] = {
	"/scene/sambohead/bas/flower_shoot.bas",
	"/scene/sambohead/bas/samboHead_crash.bas",
	"/scene/sambohead/bas/samboHead_dance.bas",
	"/scene/sambohead/bas/samboHead_down.bas",
	"/scene/sambohead/bas/samboHead_Fhide.bas",
	"/scene/sambohead/bas/samboHead_hit.bas",
	"/scene/sambohead/bas/samboHead_hit_end.bas",
	"/scene/sambohead/bas/samboHead_jump_end.bas",
	"/scene/sambohead/bas/samboHead_jump_start.bas",
	nullptr,
	"/scene/sambohead/bas/samboHead_set.bas",
	"/scene/sambohead/bas/samboHead_turn.bas",
	nullptr,
};

void TYumboSeed::init()
{
	initHitActor(0x1000002A, 1, ACTOR_TYPE_PLAYER, 30.0f, 30.0f, 0.0f, 0.0f);
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);
}

void TYumboSeed::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (mFlags & SEED_FLAG_INACTIVE)
		return;

	if (cue & CUE_CALC_ANIM) {
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

	if (cue & CUE_MOVE) {
		mVelocity.y -= mOwner->getSaveParam2()->mSeedGravityY.get();
		mPosition.add(mVelocity);
		mVelocity.scale(mOwner->getSaveParam2()->mSeedAirFric.get());

		THitActor** end = mCollisions + mColCount;
		for (THitActor** it = mCollisions; it != end; ++it) {
			switch ((*it)->mActorType) {
			case 0x80000001:
				SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
				mFlags |= SEED_FLAG_INACTIVE;
				break;
			}
		}

		if (--mLife == 0) {
			mFlags |= SEED_FLAG_INACTIVE;
			onHitFlag(HIT_FLAG_NO_COLLISION);
		}
	}

	if (!(mFlags & SEED_FLAG_HIDDEN))
		mMActor->perform(cue, graphics);
}

TYumbo::TYumbo(const char* name)
    : TSmallEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TYumbo::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 18);

	MActor* yumbo  = mMActorKeeper->createMActor("yumbo.bmd", 0);
	MActor* flower = mMActorKeeper->createMActor("flower.bmd", 0);
	flower->getModel()->getModelData()->setMaterialTable(
	    ((TYumboManager*)mManager)->mFlowerMatTable, J3DMatCopyFlag_All);
	flower->initDL();
	flower->getModel()->lock();
	mMActor = yumbo;

	mSpine->initWith(&TNerveYumboDancing::theNerve());

	TYumboSeed** end = mSeeds + 16;
	for (TYumboSeed** it = mSeeds; it != end; ++it) {
		*it = new TYumboSeed(
		    this, mMActorKeeper->createMActor("samboSeed.bmd", 3));
		(*it)->init();
	}

	initHitActor(0x1000002A, 1, ACTOR_TYPE_PLAYER, 97.5f, 225.0f, 90.0f,
	             225.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mGroundHeight = gpMap->checkGround(
	    mPosition.x, mPosition.y + mHeadHeight, mPosition.z, &mGroundPlane);
	mScaledBodyRadius = 75.0f;
	mScaling.set(1.5f, 1.5f, 1.5f);
	initAnmSound();
	mCenterJointIdx
	    = getModel()->getModelData()->getJointName()->getIndex("center");
}

void TYumbo::reset() { }

BOOL TYumbo::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return false;

	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		if (mSpine->getLatestNerve() != &TNerveYumboFreeze::theNerve())
			return false;

		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		return true;
	}

	return TSmallEnemy::receiveMessage(sender, message);
}

void TYumbo::moveObject()
{
	if (mSpine->getLatestNerve() == &TNerveYumboFreeze::theNerve()) {
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToPosPtr(
		        PARTICLE_MS_POI_KIZETSU, &mPosition, 1, this))
			emitter->setGlobalScale(mScaling);
	}

	if (mMActor->checkCurAnm("sambohead_dance", 0)) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_MS_YNB_ONPU, getModel()->getAnmMtx(mCenterJointIdx), 1,
		    this);
	}

	if (mSpine->getLatestNerve() == &TNerveYumboFreeze::theNerve()
	    || mSpine->getLatestNerve() == &TNerveSmallEnemyDie::theNerve())
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	else
		offHitFlag(HIT_FLAG_CANNOT_ATTACK);

	updateSquareToMario();
	TSmallEnemy::moveObject();
}

void TYumbo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	TYumboSeed** end = mSeeds + 16;
	for (TYumboSeed** it = mSeeds; it != end; ++it)
		(*it)->perform(cue, graphics);
}

// fabricated: the original inlined this predicate (a helper is needed for the
// theNerve() inlining to come out the same)
static inline bool isUnaffectedByWater(const TNerveBase<TLiveActor>* nerve)
{
	return nerve == &TNerveYumboFreeze::theNerve()
	       || nerve == &TNerveSmallEnemyDie::theNerve()
	       || nerve == &TNerveYumboHiding::theNerve()
	       || nerve == &TNerveYumboAttack::theNerve();
}

void TYumbo::behaveToWater(THitActor* sender)
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	if (!isUnaffectedByWater(nerve)) {
		mSpine->reset();
		mSpine->setNext(&TNerveYumboFreeze::theNerve());
	}
}

// fabricated: finds the first seed in [it, end) which isn't flying around.
static inline TYumboSeed* findInactiveSeed(TYumboSeed** it, TYumboSeed** end)
{
	for (; it != end; ++it) {
		if ((*it)->mFlags & TYumboSeed::SEED_FLAG_INACTIVE)
			return *it;
	}
	return nullptr;
}

void TYumbo::shotSeeds()
{
	TYumboSeed* seed = findInactiveSeed(mSeeds, mSeeds + 16);

	if (seed == nullptr)
		return;

	mMActor->setBck(0);
	setCurAnmSound();

	JGeometry::TVec3<f32> vel = SMS_GetMarioPos();
	vel -= mPosition;
	vel.y += 200.0f * (0.5f + MsRandF());

	vel.setLength(getSaveParam2()->mShootSpeed.get());

	f32 yaw = MsGetRotFromZaxisY(vel);
	JGeometry::TQuat4<f32> quat;
	quat.setEulerY(-(0.017453294f * yaw));
	quat.rotate(vel, vel);

	JGeometry::TQuat4<f32> tilt;
	quat.setEulerY(2.0f * M_PI * MsRandF());
	tilt.setEulerX(-M_PI * getSaveParam2()->mShootAngleX.get());
	quat.mul(quat, tilt);
	quat.rotate(vel, vel);

	s32 life = getSaveParam2()->mSeedLife.get();
	seed->mFlags &= ~TYumboSeed::SEED_FLAG_INACTIVE;
	seed->mPosition = mPosition;
	seed->mVelocity = vel;
	seed->mLife     = life;
	seed->mScaling.set(2.0f, 2.0f, 2.0f);
	seed->offHitFlag(HIT_FLAG_NO_COLLISION);
	seed->onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
}

void TYumbo::setDeadAnm() { setBckAnm(3); }

bool TYumbo::doKeepDistance()
{
	return mSpine->getLatestNerve() == &TNerveYumboFreeze::theNerve();
}

void TYumbo::attackToMario()
{
	if (mSpine->getLatestNerve() != &TNerveYumboFreeze::theNerve())
		sendAttackMsgToMario();
}

TYumboManager::TYumboManager(const char* name)
    : TSmallEnemyManager(name)
    , mFlowerMatTable(nullptr)
{
}

void TYumboManager::load(JSUMemoryInputStream& stream)
{
	TYumboParams* params = new TYumboParams("/enemy/Yumbo.prm");

	unk38 = params;

	TSmallEnemyManager::load(stream);

	params->mSLAttackRadius.set(97);
	params->mSLAttackHeight.set(225);
	params->mSLDamageRadius.set(90);
	params->mSLDamageHeight.set(225);

	mFlowerMatTable = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRGetResource("/scene/samboHead/flower_blue.bmt"));
}

void TYumboManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "yumbo.bmd", 0x10210000, 0 },
		{ "flower.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

const char** TYumbo::getBasNameTable() const { return sambohead_bastable; }

// fabricated: like TSmallEnemy::isFindMarioFromParam, but the view cone is
// flattened to the Yumbo's own height.
static inline bool isMarioInSight(TYumbo* self)
{
	f32 searchHeight = self->getSaveParams()->mSLSearchHeight.get();
	if (abs(SMS_GetMarioPos().y - self->mPosition.y) < searchHeight) {
		JGeometry::TVec3<f32> pos(SMS_GetMarioPos().x, self->mPosition.y,
		                          SMS_GetMarioPos().z);
		f32 searchLength = self->getSaveParams()->mSLSearchLength.get();
		f32 searchAngle  = self->getSaveParams()->mSLSearchAngle.get();
		f32 searchAware  = self->getSaveParams()->mSLSearchAware.get();
		if (self->isInSight(pos, searchLength, searchAngle, searchAware))
			return true;
		else
			return false;
	}
	return false;
}

DEFINE_NERVE(TNerveYumboDancing, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(2);

	JGeometry::TVec3<f32> dir = SMS_GetMarioPos();
	dir -= self->mPosition;
	self->mRotation.y = MsGetRotFromZaxisY(dir);
	char trash[8];

	if (isMarioInSight(self)) {
		spine->pushAfterCurrent(&TNerveYumboHiding::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveYumboHiding, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	char trash[8];
	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->mMActor->getFrameCtrl(ANM_TYPE_BCK)
		    ->setRate(3.0f * SMSGetAnmFrameRate());
		self->mParticlesEmitted = false;
		if (gpMSound->gateCheck(MSD_SE_EN_YUMBO_SINK))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_YUMBO_SINK, &self->mPosition, 0, nullptr, 0, 4);
	}

	if (!self->mParticlesEmitted
	    && self->mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(34.0f)) {
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_ROCK,
		                             &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_SMOKE,
		                             &self->mPosition, 0, nullptr);
		self->mParticlesEmitted = true;
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveYumboAttack::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveYumboAppearing, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor = self->mMActorKeeper->getMActor("yumbo.bmd");
		self->setBckAnm(10);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_ROCK,
		                             &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(PARTICLE_MS_SMB_AP_SMOKE,
		                             &self->mPosition, 0, nullptr);
	}

	JGeometry::TVec3<f32> dir = SMS_GetMarioPos();
	dir -= self->mPosition;
	self->mRotation.y = MsGetRotFromZaxisY(dir);
	char trash[8];

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveYumboDancing::theNerve());
		return true;
	}

	return false;
}

// fabricated: whether Mario got too far away (vertically or horizontally) to
// keep shooting at him.
static inline bool isMarioTooFar(TYumbo* self)
{
	f32 giveUpHeight = self->getSaveParams()->mSLGiveUpHeight.get();
	if (giveUpHeight <= abs(SMS_GetMarioPos().y - self->mPosition.y))
		return true;

	JGeometry::TVec3<f32> toMario = SMS_GetMarioPos();
	toMario -= self->mPosition;
	toMario.y = 0.0f;
	f32 giveUpLength = self->getSaveParams()->mSLGiveUpLength.get();
	return giveUpLength * giveUpLength < toMario.squared();
}

DEFINE_NERVE(TNerveYumboAttack, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	if (spine->getTime() == 0) {
		self->mMActor = self->mMActorKeeper->getMActor("flower.bmd");
		self->shotSeeds();
	}

	if (isMarioTooFar(self)) {
		spine->pushAfterCurrent(&TNerveYumboAppearing::theNerve());
		return true;
	}

	if (self->getSaveParam2()->mSeedLife.get() / 16 < spine->getTime()) {
		spine->pushAfterCurrent(this);
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveYumboFreeze, TLiveActor)
{
	TYumbo* self = (TYumbo*)spine->getBody();
	char trash[8];
	if (spine->getTime() == 0) {
		self->setBckAnm(11);
		if (gpMSound->gateCheck(MSD_SE_EN_COMMON_TWINKLE))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_COMMON_TWINKLE, &self->mPosition, 0, nullptr, 0, 4);
	}

	if (self->getSaveParam2()->mRecoverTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveYumboDancing::theNerve());
		return true;
	}

	return false;
}
