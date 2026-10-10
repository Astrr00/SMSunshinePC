#include <Enemy/TabePuku.hpp>
#include <Enemy/Graph.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapData.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* tabepuku_bastable[] = {
	"/scene/tabepuku/bas/pukupuku_chase.bas",
	"/scene/tabepuku/bas/pukupuku_search.bas",
	"/scene/tabepuku/bas/pukupuku_swim.bas",
};

namespace {
f32 cAngleMax = JGeometry::TUtil<f32>::PI() / 8.0f;
}

void TTPHitActor::init()
{
	initHitActor(0x10000035, 1, ACTOR_TYPE_PLAYER, 10.0f, 10.0f, 10.0f, 10.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);
}

BOOL TTPHitActor::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TTPHitActor::updateTerrainCollsion()
{
	JGeometry::TQuat4<f32> quat = mOwner->mQuat;
	JGeometry::TVec3<f32> dir;
	quat.getYDir(dir);

	mHeadHeight = mAttackHeight;
	mWallRadius = mAttackRadius;

	f32 height = 0.6666667f * mHeadHeight;
	if ((BOOL)mOwner->isHolding()) {
		f32 damageHeight = mOwner->mHeldObject->mDamageHeight;
		height += damageHeight;
		mHeadHeight += damageHeight;
		mWallRadius += mOwner->mHeldObject->mDamageRadius;
	}

	f32 reach = 0.5f * mAttackHeight;
	JGeometry::TVec3<f32> down(0.0f, -1.0f, 0.0f);
	const JGeometry::TVec3<f32>& ownerPos = mOwner->mPosition;
	JGeometry::TVec3<f32> pos;
	pos.x = ownerPos.x + dir.x * reach + down.x * height;
	pos.y = ownerPos.y + dir.y * reach + down.y * height;
	pos.z = ownerPos.z + dir.z * reach + down.z * height;

	JGeometry::TVec3<f32> diff = pos;
	diff -= mPosition;
	mLinearVelocity = diff;
	mPosition       = pos;
}

void TTPHitActor::bind()
{
	JGeometry::TVec3<f32> pos = mPosition;
	pos += mLinearVelocity;
	JGeometry::TVec3<f32> velocity = mOwner->mVelocity;
	pos += velocity;
	pos += mOwner->mLinearVelocity;

	mGroundHeight = gpMap->checkGroundIgnoreWaterSurface(
	    pos.x, pos.y + mHeadHeight, pos.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (pos.y <= mGroundHeight + 0.05f) {
		mIsAirborne = false;

		const JGeometry::TVec3<f32>& normal = mGroundPlane->getNormal();
		f32 depth = 1.0f
		            - (normal.dot(pos)
		               - normal.dot(JGeometry::TVec3<f32>(pos.x, mGroundHeight,
		                                                  pos.z)));
		if (depth > 0.0f) {
			pos.x += normal.x * depth;
			pos.y += normal.y * depth;
			pos.z += normal.z * depth;
		}
		pos.y = mGroundHeight;
	} else {
		mIsAirborne = true;
	}

	if (0.0f <= pos.y + mHeadHeight)
		pos.y = -mHeadHeight;

	TBGWallCheckRecord record(pos.x, pos.y, pos.z, mWallRadius, 1, 0);
	bool touched = gpMap->isTouchedWallsAndMoveXZ(&record);
	pos.x        = record.mCenter.x;
	pos.z        = record.mCenter.z;

	JGeometry::TVec3<f32> diff = pos;
	diff -= mPosition;
	mLinearVelocity = diff;
	mIsTouchedWall  = touched;
	mPosition       = pos;
}

TTabePuku::TTabePuku(const char* name)
    : TSmallEnemy(name)
{
	onLiveFlag(LIVE_FLAG_UNK1000);
}

void TTabePuku::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	setMActorAndKeeper();
	mSpine->initWith(&TNerveTabePukuGraphWander::theNerve());

	initHitActor(0x10000035, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	mHitActor = new TTPHitActor("タベプク用当たり", this);
	mHitActor->init();
	mHitActor->mPosition = mPosition;

	mQuat.set(SMS_Eular2Quat(mRotation));
	mMouthJointIdx
	    = getModel()->getModelData()->getJointName()->getIndex("jnt_mouth_up");
	initAnmSound();
}

void TTabePuku::reset() { mScaledBodyRadius = 130.0f; }

void TTabePuku::perform(u32 cue, JDrama::TGraphics* graphics)
{
	mHitActor->perform(cue, graphics);
	TSmallEnemy::perform(cue, graphics);
}

void TTabePuku::control()
{
	TLiveActor::control();

	mHitActor->checkMarioContact();

	if (isCarryingMario())
		SMSGetMSound()->startSoundActor(MSD_SE_EN_TOBIPUKU_CHEW, &mPosition, 0,
		                                nullptr, 0, 4);
}

void TTabePuku::bind()
{
	mHitActor->updateHitParam();
	mHitActor->updateTerrainCollsion();
	mHitActor->bind();

	mLinearVelocity = mHitActor->mLinearVelocity;
	mIsTouchedWall  = mHitActor->mIsTouchedWall;
	if (mHitActor->mIsAirborne)
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	else
		offLiveFlag(LIVE_FLAG_AIRBORNE);
	mGroundPlane  = mHitActor->mGroundPlane;
	mGroundHeight = mHitActor->mGroundHeight;
}

void TTabePuku::calcRootMatrix()
{
	if (isTaken()) {
		TSmallEnemy::calcRootMatrix();
		return;
	}

	TPosition3f mtx;
	mtx.setQT(mQuat, mPosition);
	getModel()->setBaseScale(mScaling);
	getModel()->setBaseTRMtx(mtx);

	JGeometry::TVec3<f32> scale(1.0f, 1.0f, 1.0f);
	JPABaseEmitter* emitter = SMS_EasyEmitParticle(
	    PARTICLE_MS_PUKU_AWA, getModel()->getAnmMtx(mMouthJointIdx), this,
	    scale);
	if (emitter) {
		f32 depth = -mPosition.y / 100.0f;
		if (depth <= 0.0f)
			depth = 0.0f;
		int lifetime = (int)depth * 20 + 2;
		if (lifetime > 200)
			lifetime = 200;
		emitter->mBaseLifetime = lifetime;
		if (isAttack())
			emitter->mChildSpawnRate = 0.1f;
	}
}

BOOL TTabePuku::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_TRAMPLE:
	case HIT_MESSAGE_HIP_DROP:
		return false;
	default:
		return TSmallEnemy::receiveMessage(sender, message);
	}
}

MtxPtr TTabePuku::getTakingMtx()
{
	mTakingMtx.setQuat(mQuat);

	JGeometry::TVec3<f32> zDir;
	JGeometry::TVec3<f32> yDir;
	mTakingMtx.getZDir(zDir);
	mTakingMtx.getYDir(yDir);

	f32 correctZ = getSaveParam2()->mCorrectZ.get();
	f32 x        = zDir.x * correctZ + mPosition.x;
	f32 y        = zDir.y * correctZ + mPosition.y;
	f32 z        = zDir.z * correctZ + mPosition.z;

	f32 correctY = getSaveParam2()->mCorrectY.get();
	mTakingMtx.ref(0, 3) = yDir.x * correctY + x;
	mTakingMtx.ref(1, 3) = yDir.y * correctY + y;
	mTakingMtx.ref(2, 3) = yDir.z * correctY + z;
	MtxPtr result = mTakingMtx;
	return result;
}

const char** TTabePuku::getBasNameTable() const { return tabepuku_bastable; }

void TTabePuku::attackToMario()
{
	if (isCarryingMario())
		return;

	if (isPatrolling())
		return;

	if (SMS_SendMessageToMario(this, HIT_MESSAGE_TAKE)) {
		mHeldObject = (TTakeActor*)SMS_GetMarioHitActor();
		mSpine->reset();
		mSpine->setNext(&TNerveTabePukuBite::theNerve());
	}
}

void TTabePuku::behaveToWater(THitActor*) { }

void TTabePuku::forceKill() { }

bool TTabePuku::isFindMario(f32 param_1)
{
	return isFindMarioFromParam(param_1);
}

bool TTabePuku::doKeepDistance()
{
	return !(isAttack() || isCarryingMario());
}

#define TABEPUKU_MOVE_FORWARD(self)                                            \
	{                                                                          \
		JGeometry::TVec3<f32> forward;                                         \
		(self)->mQuat.getZDir(forward);                                        \
		forward.scale((self)->mMarchSpeed);                                    \
		JGeometry::TVec3<f32> vel = (self)->mVelocity;                         \
		vel.scale((self)->getSaveParam2()->mWaterFric.get());                  \
		vel += forward;                                                        \
		(self)->mVelocity   = vel;                                             \
		(self)->mRotation.y = MsGetRotFromZaxisY(vel);                         \
	}

void TTabePuku::swimTo(const JGeometry::TVec3<f32>& target)
{
	JGeometry::TVec3<f32> dir = target;
	if (JGeometry::TUtil<f32>::epsilonEquals(0.0f, dir.squared())) {
		TABEPUKU_MOVE_FORWARD(this)
	} else {
		dir.normalize();

		JGeometry::TQuat4<f32> rot;
		if (JGeometry::TUtil<f32>::epsilonEquals(
		        -1.0f, dir.dot(JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f))))
			rot.setEulerY(M_PI);
		else
			rot.setRotate(JGeometry::TVec3<f32>(0.0f, 0.0f, 1.0f), dir, 1.0f);

		mQuat.slerp(rot, getSaveParam2()->mTurnSlerpRate.get());
		mQuat.normalize();
		TABEPUKU_MOVE_FORWARD(this)
	}
}

TTabePukuManager::TTabePukuManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TTabePukuManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TTabePukuParams("/enemy/tabepuku.prm");
	TSmallEnemyManager::load(stream);
}

void TTabePukuManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "tabepuku.bmd", 0x10210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveTabePukuGraphWander, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getTracer()->reset();
		self->goToShortestNextGraphNode();
		self->setBckAnm(2);
		self->mMarchSpeed = self->getSaveParam2()->mMarchSpeed.get();
	}

	if (self->isReachedToGoal())
		self->goToRandomNextGraphNode();

	if (self->isFindMario(1.0f)) {
		spine->pushAfterCurrent(&TNerveTabePukuFound::theNerve());
		return true;
	}

	JGeometry::TVec3<f32> diff = self->getUnk104().getPoint();
	diff -= self->mPosition;
	JGeometry::TVec3<f32> offset(0.0f, 0.0f, 0.0f);
	diff += offset;
	self->swimTo(diff);
	return false;
}

DEFINE_NERVE(TNerveTabePukuFound, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(1);
		self->mMarchSpeed = 0.0f;
	}

	TABEPUKU_MOVE_FORWARD(self)

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveTabePukuAttack::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveTabePukuRecoverGraph, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->getTracer()->reset();
		self->getTracer()->reset2();
		self->goToShortestNextGraphNode();
		self->mMarchSpeed = self->getSaveParam2()->mMarchSpeed.get();
	}

	if (self->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveTabePukuGraphWander::theNerve());
		return true;
	}

	bool lifted = true;
	if (self->isAirborne()) {
		if (!self->mIsTouchedWall)
			lifted = false;
	}

	JGeometry::TVec3<f32> offset;
	if (lifted)
		offset.set(0.0f, 10000.0f, 0.0f);
	else
		offset.set(0.0f, 0.0f, 0.0f);

	JGeometry::TVec3<f32> diff = self->getUnk104().getPoint();
	diff -= self->mPosition;
	diff += offset;
	self->swimTo(diff);
	return false;
}

DEFINE_NERVE(TNerveTabePukuAttack, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(0);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		self->mMarchSpeed = self->getSaveParam2()->mAttackSpeed.get();
	}

	bool giveUp;
	if (abs(SMS_GetMarioPos().y - self->mPosition.y)
	    > self->getSaveParams()->getSLGiveUpHeight()) {
		giveUp = true;
	} else {
		f32 giveUpLength = self->getSaveParams()->getSLGiveUpLength();
		JGeometry::TVec3<f32> toGoal = self->getUnk104().getPoint();
		toGoal -= self->mPosition;
		if (toGoal.length() > giveUpLength) {
			giveUp = true;
		} else {
			JGeometry::TVec3<f32> toLink
			    = self->getTracer()->getGraph()->getNearestPosOnGraphLink(
			        self->mPosition);
			toLink -= self->mPosition;
			f32 range = self->getSaveParam2()->mTerritoryRange.get();
			giveUp    = range * range <= toLink.squared();
		}
	}

	if (giveUp || self->mIsTouchedWall) {
		spine->pushAfterCurrent(&TNerveTabePukuRecoverGraph::theNerve());
		return true;
	}

	JGeometry::TVec3<f32> diff = self->getUnk104().getPoint();
	diff -= self->mPosition;
	diff += JGeometry::TVec3<f32>(0.0f, 150.0f, 0.0f);
	self->swimTo(diff);
	return false;
}

DEFINE_NERVE(TNerveTabePukuBite, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	self->setBckAnm(2);
	SMSGetMSound()->startSoundActor(MSD_SE_EN_TOBIPUKU_BITE, &self->mPosition,
	                                0, nullptr, 0, 4);
	spine->pushAfterCurrent(&TNerveTabePukuDive::theNerve());
	return true;
}

DEFINE_NERVE(TNerveTabePukuDive, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mDiveStartY = self->mPosition.y;
		self->setBckAnm(2);
		self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setRate(
		    2.0f * SMSGetAnmFrameRate());
		self->mMarchSpeed = self->getSaveParam2()->mDiveSpeed.get();
	}

	self->swimTo(JGeometry::TVec3<f32>(
	    0.0f, self->mGroundHeight - self->mPosition.y, 0.0f));

	if (self->mPosition.y - self->mDiveStartY
	        < -self->getSaveParam2()->mApartHeight.get()
	    || self->mPosition.y - self->mGroundHeight < 200.0f
	    || !self->isAirborne()) {
		spine->pushAfterCurrent(&TNerveTabePukuDrag::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveTabePukuDrag, TLiveActor)
{
	TTabePuku* self = (TTabePuku*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mDragDir.x = 0.0f;
		self->mDragDir.y = 0.0f;
		self->mDragDir.z = 1.0f;
		JGeometry::TQuat4<f32> rot;
		rot.setEulerY(MsRandF() * (2.0f * M_PI));
		rot.rotate(self->mDragDir, self->mDragDir);
		self->setGoalPath(TPathNode(self->mPosition));
		self->mMarchSpeed = self->getSaveParam2()->mDiveSpeed.get();
	}

	self->swimTo(self->mDragDir);

	bool done = true;
	if (!self->mIsTouchedWall && self->isAirborne()) {
		JGeometry::TVec3<f32> toGoal = self->getUnk104().getPoint();
		toGoal -= self->mPosition;
		f32 dist = toGoal.length();
		if (!(self->getSaveParam2()->mDragLength.get() < dist))
			done = false;
	}
	if (done) {
		SMS_SendMessageToMario(self, HIT_MESSAGE_UNK8);
		self->mHeldObject = nullptr;
	}

	if (done) {
		spine->pushAfterCurrent(&TNerveTabePukuRecoverGraph::theNerve());
		return true;
	}
	return false;
}
