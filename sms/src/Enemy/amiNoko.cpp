#include <Enemy/AmiNoko.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Player/MarioAccess.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>

// theNerve() is a plain out-of-line member (retail: instance$NNNN statics).
#define AMINOKO_NERVE(Name)                                                    \
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

AMINOKO_NERVE(TNerveAmiNokoFreeze)
AMINOKO_NERVE(TNerveAmiNokoDie)
AMINOKO_NERVE(TNerveAmiNokoTurn)
AMINOKO_NERVE(TNerveAmiNokoWalkOnFence)

#undef AMINOKO_NERVE

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* amiNoko_bastable[] = {
	nullptr,
	"/scene/amiNoko/bas/aminoko_flying1_start.bas",
	"/scene/amiNoko/bas/aminoko_hit1.bas",
	nullptr,
	"/scene/amiNoko/bas/aminoko_run1_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_run2_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_turn1_loop.bas",
	nullptr,
	nullptr,
	"/scene/amiNoko/bas/aminoko_turn2_loop.bas",
	nullptr,
	nullptr,
};

// ms_dnk_shibire_b.jpa, ms_dnk_shibire_a.jpa, ms_dnk_hibana.jpa
enum {
	PARTICLE_MS_DNK_SHIBIRE_B = 0xCA,
	PARTICLE_MS_DNK_SHIBIRE_A = 0x17D,
	PARTICLE_MS_DNK_HIBANA    = 0x17E,
	PARTICLE_MS_AMN_BIRI      = 0x180,
	PARTICLE_MS_AMN_SPARK_L   = 0x181,
	PARTICLE_MS_AMN_SPARK_R   = 0x182,
	PARTICLE_MS_AMN_SPARK_M   = 0x183,
};

// Moves value towards target by at most step, without overshooting.
static inline f32 convergeTo(f32 value, f32 target, f32 step)
{
	if (value < target) {
		value += step;
		return value > target ? target : value;
	}
	value -= step;
	return value > target ? value : target;
}

TAmiNokoManager::TAmiNokoManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void TAmiNokoManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAmiNokoParams("/enemy/amiNoko.prm");
	TSmallEnemyManager::load(stream);
}

void TAmiNokoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "aminoko_model1.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (2 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TAmiNokoManager::createEnemyInstance() { return nullptr; }

BOOL TAmiHit::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TAmiHit::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		JGeometry::TVec3<f32> offset = mOwner->mUp;
		char trash[24];
		offset.normalize();
		offset.scale(100.0f);

		mPosition = mOwner->mPosition;
		mPosition += offset;
		mPosition.y -= 0.5f * mAttackHeight;

		if (!mOwner->checkLiveFlag(LIVE_FLAG_DEAD)) {
			for (int i = 0; i < mColCount; ++i) {
				if (mCollisions[i]->mActorType == 0x80000001)
					mOwner->attackToMario();
			}
		}
	}

	THitActor::perform(cue, graphics);
}

TAmiNoko::TAmiNoko(const char* name)
    : TWalkerEnemy(name)
    , mPlane(nullptr)
    , mPlaneType(0)
    , mIsFenceAnm(true)
{
	mUp.x = 0.0f;
	mUp.y = 1.0f;
	mUp.z = 0.0f;

	mForward.x = 0.0f;
	mForward.y = 0.0f;
	mForward.z = 1.0f;

	mPrevUp      = mUp;
	mPrevForward = mForward;
}

void TAmiNoko::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
	stream >> mCoinId;
}

void TAmiNoko::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000021;
	unk150     = 0x11;
	mSpine->initWith(&TNerveAmiNokoWalkOnFence::theNerve());
	mParams = (TAmiNokoParams*)getSaveParam();
	reset();
	setWalkAnm();
	onLiveFlag(LIVE_FLAG_UNK10);
	initialGraphNode();

	if (gpMarDirector->mMap == 8)
		mIsFenceAnm = false;

	mAmiHit = new TAmiHit(this, "アミノコ当り判定");
	char trash[12];
}

void TAmiNoko::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("aminoko_model1.bmd", 3);
}

void TAmiNoko::reset() { TWalkerEnemy::reset(); }

void TAmiNoko::behaveToWater(THitActor*)
{
	if (mSpine->getCurrentNerve() != &TNerveAmiNokoFreeze::theNerve()) {
		mSpine->pushNerve(&TNerveAmiNokoFreeze::theNerve());
		mSprayedByWaterCooldown = 0;
	}
}

void TAmiNoko::attackToMario()
{
	char trash[8];
	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyChange::theNerve()
	    && mPlane != nullptr) {
		BOOL attack = TRUE;
		switch (mPlaneType) {
		case PLANE_FLOOR:
			if (SMS_GetMarioRfPlane() != nullptr && gpMarioPos->y < mPosition.y)
				attack = false;
			break;

		case PLANE_ROOF:
			if (SMS_GetMarioGrPlane() != nullptr
			    && 5.0f + gpMarioPos->y > mPosition.y)
				attack = false;
			break;

		case PLANE_WALL:
			if (const TBGCheckData* wall = SMS_GetMarioWlPlane()) {
				if (wall->getNormal().dot(mPlane->getNormal()) < 0.0f)
					attack = false;
			}
			break;
		}

		if (attack && SMS_SendMessageToMario(this, 9)) {
			if (mSpine->getCurrentNerve() != &TNerveAmiNokoFreeze::theNerve())
				mSpine->pushNerve(&TNerveAmiNokoFreeze::theNerve());
		}
	}
}

void TAmiNoko::setWalkAnm()
{
	if (mIsFenceAnm)
		setBckAnm(5);
	else
		setBckAnm(8);
}

bool TAmiNoko::isHitValid(u32 message)
{
	char trash[40];
	if (message == HIT_MESSAGE_PUNCH || message == HIT_MESSAGE_HIP_DROP) {
		f32 dx = mPosition.x - gpMarioPos->x;
		f32 dy = 0.0f;
		f32 dz = mPosition.z - gpMarioPos->z;
		matan(mUp.z, mUp.x);
		if (dx * mUp.x + dy * mUp.y + dz * mUp.z > 0.0f
		    || message == HIT_MESSAGE_HIP_DROP)
			mSpine->pushNerve(&TNerveAmiNokoDie::theNerve());
	}
	if (message == HIT_MESSAGE_UNKB)
		return true;
	return false;
}

void TAmiNoko::calcRootMatrix()
{
	char trash0[8];
	if (SMSGetMSound()->gateCheck(MSD_SE_EN_AMINOKO_SPARK))
		MSoundSESystem::MSoundSE::startSoundActor(MSD_SE_EN_AMINOKO_SPARK,
		                                          &mPosition, 0, nullptr, 0, 4);

	// the actor pointers are only used to tell the emitters apart
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_BIRI, mMActor->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_BIRI, mMActor->getModel()->getAnmMtx(10), 1, this + 1);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_BIRI, mMActor->getModel()->getAnmMtx(9), 1, this + 2);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_SPARK_R, mMActor->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_SPARK_L, mMActor->getModel()->getAnmMtx(11), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_AMN_SPARK_M, mMActor->getModel()->getAnmMtx(11), 1, this);

	if (mSpine->getCurrentNerve() == &TNerveAmiNokoFreeze::theNerve()
	    && mSpine->getTime() < 46) {
		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_MS_DNK_SHIBIRE_A, mMActor->getModel()->getAnmMtx(0), 1,
		    this);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f));

		MtxPtr jointMtx = mMActor->getModel()->getAnmMtx(6);
		mElecPos.set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);
		emitter         = gpMarioParticleManager->emitAndBindToPosPtr(
            PARTICLE_MS_DNK_HIBANA, &mElecPos, 1, this);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f));
	}

	if (isBckAnm(0)) {
		mBaseMtx[0][3] = mPosition.x;
		mBaseMtx[1][3] = mPosition.y;
		mBaseMtx[2][3] = mPosition.z;
		MTXCopy(mBaseMtx, mMActor->getModel()->getBaseTRMtx());
		return;
	}

	MtxPtr mtx = getModel()->getBaseTRMtx();

	JGeometry::TVec3<f32> fwd = getUnkF4().getPoint();
	fwd -= mPosition;
	if (fwd.x == 0.0f && fwd.y == 0.0f && fwd.z == 0.0f)
		fwd.x = 1.0f;
	VECNormalize(&fwd, &fwd);

	// pick the closest of the touched walls, the floor and the roof
	TBGWallCheckRecord record(mPosition.x, mPosition.y, mPosition.z, 10.0f, 4,
	                          0);
	bool touched  = gpMap->isTouchedWallsAndMoveXZ(&record);
	f32 bestDist  = -1.0f;
	int bestIndex = -1;
	const TBGCheckData* plane;
	for (int i = 0; i < touched; ++i) {
		const TBGCheckData* wall = record.mResultWalls[i];
		plane                    = wall;
		f32 dist
		    = fabsf(wall->mNormal.dot(mPosition) + wall->mPlaneDistance);
		if (bestIndex < 0 || bestDist > dist || bestDist < 0.0f) {
			bestDist  = dist;
			mPlaneType = PLANE_WALL;
			bestIndex = i;
		}
	}

#ifdef TARGET_PC
	const TBGCheckData* bestPlane
	    = bestIndex >= 0 ? record.mResultWalls[bestIndex] : nullptr;
#else
	// bestIndex == -1 reads the (zero) flags member in front of the array
	const TBGCheckData* bestPlane = record.mResultWalls[bestIndex];
#endif

	gpMap->checkGround(mPosition.x, mPosition.y + getHeadHeight(), mPosition.z,
	                   &plane);
	mGroundPlane = plane;
	if (mGroundPlane != nullptr) {
		plane = mGroundPlane;
		f32 dist = plane->mNormal.dot(mPosition) + plane->mPlaneDistance;
		if (dist >= 0.0f && (bestDist > dist || bestDist < 0.0f)) {
			bestDist   = dist;
			bestPlane  = plane;
			mPlaneType = PLANE_FLOOR;
		}
	}

	gpMap->checkRoof(mPosition, &plane);
	if (plane != nullptr) {
		f32 dist = plane->mNormal.dot(mPosition) + plane->mPlaneDistance;
		if (dist >= 0.0f && (bestDist > dist || bestDist < 0.0f)) {
			bestPlane  = plane;
			mPlaneType = PLANE_ROOF;
		}
	}

	if (bestPlane != nullptr)
		mPlane = bestPlane;

	f32 rotSpeed = ((TAmiNokoParams*)getSaveParam())->mSLMtxRotSpeed.get();

	JGeometry::TVec3<f32> up;
	if (mPlane != nullptr) {
		up = mPlane->mNormal;
	} else {
		up.x = 0.0f;
		up.y = 1.0f;
		up.z = 0.0f;
	}

	mUp.x = convergeTo(mUp.x, up.x, rotSpeed);
	mUp.y = convergeTo(mUp.y, up.y, rotSpeed);
	mUp.z = convergeTo(mUp.z, up.z, rotSpeed);

	// don't turn around on the spot, go sideways instead
	if (mForward.dot(fwd) < -0.1f) {
		Mtx rotMtx;
		MTXRotAxisRad(rotMtx, &up, 1.5707964f);
		MTXMultVec(rotMtx, &fwd, &fwd);
	}

	mForward.x = convergeTo(mForward.x, fwd.x, rotSpeed);
	mForward.y = convergeTo(mForward.y, fwd.y, rotSpeed);
	mForward.z = convergeTo(mForward.z, fwd.z, rotSpeed);
	VECNormalize(&mForward, &mForward);

	up = mUp;
	fwd = mForward;
	JGeometry::TVec3<f32> side;
	side.cross(up, fwd);
	if (side.isZero()) {
		up  = mPrevUp;
		fwd = mPrevForward;
		side.cross(up, fwd);
		if (side.isZero()) {
			side.x = 1.0f;
			side.y = 0.0f;
			side.z = 0.0f;
		}
	} else {
		mPrevUp      = mUp;
		mPrevForward = mForward;
	}
	VECNormalize(&side, &side);

	fwd.cross(side, up);
	if (fwd.isZero()) {
		fwd.x = 0.0f;
		fwd.y = 0.0f;
		fwd.z = 1.0f;
	} else {
		VECNormalize(&fwd, &fwd);
	}

	up.cross(fwd, side);
	if (up.isZero()) {
		up.x = 0.0f;
		up.y = 1.0f;
		up.z = 0.0f;
	} else {
		VECNormalize(&up, &up);
	}

	mtx[0][0] = side.x;
	mtx[1][0] = side.y;
	mtx[2][0] = side.z;
	mtx[0][1] = up.x;
	mtx[1][1] = up.y;
	mtx[2][1] = up.z;
	mtx[0][2] = fwd.x;
	mtx[1][2] = fwd.y;
	mtx[2][2] = fwd.z;

	// the result of this is not used
	JGeometry::TVec3<f32> offset;
	offset.x = 0.0f;
	offset.y = -200.0f;
	offset.z = 0.0f;
	Mtx offsetMtx;
	char trash[232];
	MsMtxSetRotRPH(offsetMtx, up.x, up.y, up.z);
	MTXMultVec(offsetMtx, &offset, &offset);

	mtx[0][3] = mPosition.x - 30.0f * mUp.x;
	mtx[1][3] = mPosition.y - 30.0f * mUp.y;
	mtx[2][3] = mPosition.z - 30.0f * mUp.z;
	MTXCopy(mtx, mBaseMtx);

	getModel()->setBaseScale(mScaling);
}

void TAmiNoko::bind()
{
	if (isBckAnm(0)) {
		JGeometry::TVec3<f32> nextPos = mPosition;
		nextPos += mLinearVelocity;
		nextPos += mVelocity;

		mVelocity.y -= getGravityY();
		if (mVelocity.y < mVelocityMinY)
			mVelocity.y = mVelocityMinY;

		if (checkLiveFlag(LIVE_FLAG_UNK1000)) {
			mGroundHeight = gpMap->checkGroundIgnoreWaterSurface(
			    nextPos.x, nextPos.y + mHeadHeight, nextPos.z, &mGroundPlane);
		} else {
			mGroundHeight = gpMap->checkGround(
			    nextPos.x, nextPos.y + mHeadHeight, nextPos.z, &mGroundPlane);
		}
		mGroundHeight += 1.0f;

		if (nextPos.y <= mGroundHeight + 0.05f) {
			if (mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL))
				kill();
			offLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity.set(0.0f, 0.0f, 0.0f);
			nextPos.y = mGroundHeight;
		} else {
			onLiveFlag(LIVE_FLAG_AIRBORNE);
		}

		mLinearVelocity = nextPos - mPosition;
	} else {
		TLiveActor::bind();
	}
}

void TAmiNoko::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	mAmiHit->perform(cue, graphics);
}

f32 TAmiNoko::getGravityY() const
{
	if (mSpine->getCurrentNerve() == &TNerveAmiNokoDie::theNerve())
		return 0.0f;
	return mGravity;
}

void TAmiNoko::creepToCurPathNode(f32 speed)
{
	if (isBckAnm(4) || isBckAnm(7) || isBckAnm(10) || isBckAnm(13)) {
		JGeometry::TVec3<f32> dir = getUnkF4().getPoint();
		dir -= mPosition;
		if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
			dir.x = 1.0f;
		VECNormalize(&dir, &dir);
		dir.scale(speed);
		JGeometry::TVec3<f32> velocity = mLinearVelocity;
		velocity += dir;
		mLinearVelocity = velocity;
	}
}

const char** TAmiNoko::getBasNameTable() const { return amiNoko_bastable; }

BOOL TNerveAmiNokoWalkOnFence::execute(TSpineBase<TLiveActor>* spine) const
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();
	if (spine->getTime() == 0)
		self->setWalkAnm();

	if (self->isBckAnm(5) || self->isBckAnm(8)) {
		if (self->checkCurAnmEnd(0)) {
			if (self->mIsFenceAnm)
				self->setBckAnm(4);
			else
				self->setBckAnm(7);
		}
	}

	if ((self->unkF4.getPoint() - self->mPosition).length() < 1.5f
	    && self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(3) || self->isBckAnm(6)) {
			self->goToRandomNextGraphNode();
			spine->pushAfterCurrent(&TNerveAmiNokoTurn::theNerve());
			return true;
		} else if (self->isBckAnm(4)) {
			self->setBckAnm(3);
		} else if (self->isBckAnm(7)) {
			self->setBckAnm(6);
		}
	}

	self->creepToCurPathNode(3.0f);
	return false;
}

BOOL TNerveAmiNokoTurn::execute(TSpineBase<TLiveActor>* spine) const
{
	TAmiNoko* self = (TAmiNoko*)spine->getBody();
	if (spine->getTime() == 0) {
		if (self->mIsFenceAnm)
			self->setBckAnm(11);
		else
			self->setBckAnm(14);
	}

	if (self->isBckAnm(11) || self->isBckAnm(14)) {
		if (self->checkCurAnmEnd(0)) {
			if (self->mIsFenceAnm)
				self->setBckAnm(10);
			else
				self->setBckAnm(13);
		}
	}

	JGeometry::TVec3<f32> dir = self->getUnkF4().getPoint();
	dir -= self->mPosition;
	if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
		dir.x = 1.0f;
	VECNormalize(&dir, &dir);

	if (dir.dot(self->mForward) > 0.8f && self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(9) || self->isBckAnm(12)) {
			if (self->mIsFenceAnm)
				self->setBckAnm(15);
			else
				self->setBckAnm(15);
		} else if (self->isBckAnm(15)) {
			spine->pushAfterCurrent(&TNerveAmiNokoWalkOnFence::theNerve());
			return true;
		} else {
			if (self->mIsFenceAnm)
				self->setBckAnm(9);
			else
				self->setBckAnm(12);
		}
	}

	self->creepToCurPathNode(0.0f);
	return false;
}

BOOL TNerveAmiNokoDie::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TAmiNoko* self = (TAmiNoko*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(1);
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (self->checkCurAnmEnd(0) && self->isBckAnm(1)) {
		JGeometry::TVec3<f32> dir = self->mPosition - *gpMarioPos;
		if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
			dir.x = 1.0f;

		MtxPtr mtx = self->mMActor->getModel()->getBaseTRMtx();
		dir.x      = mtx[0][1];
		dir.y      = mtx[1][1];
		dir.z      = mtx[2][1];
		MsVECNormalize(&dir, &dir);
		dir.scale(20.0f);
		self->mPosition.y += 10.0f;
		self->mVelocity = dir;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->setBckAnm(0);
	}

	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_KIL_SMOKE, self->mMActor->getModel()->getBaseTRMtx(), 1,
	    self);

	if (self->isBckAnm(0) && spine->getTime() > 30) {
		JGeometry::TVec3<f32> toMario = self->mPosition - *gpMarioPos;

		bool hitWall;
		if (gpMap->isTouchedOneWallAndMoveXZ(
		        &self->mPosition.x, self->mPosition.y, &self->mPosition.z,
		        3.0f * self->mBodyRadius)) {
			self->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);

			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
			    PARTICLE_MS_ENM_WALLHIT, &self->mPosition, 0,
			    DEG2SHORTANGLE(self->mRotation.y), 0, 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(self->mScaling);

			emitter = gpMarioParticleManager->emitWithRotate(
			    PARTICLE_MS_ENM_WALLHIT_O, &self->mPosition, 0,
			    DEG2SHORTANGLE(self->mRotation.y), 0, 0, nullptr);
			if (emitter)
				SMSSetEmitterPolColor(emitter, 6);

			hitWall = true;
		} else {
			hitWall = false;
		}

		if (hitWall || !self->isAirborne()
		    || JGeometry::TUtil<f32>::sqrt(toMario.squared()) > 10000.0f) {
			self->onHitFlag(HIT_FLAG_NO_COLLISION);
			self->onLiveFlag(LIVE_FLAG_DEAD);
			self->onLiveFlag(LIVE_FLAG_UNK8);
			self->offLiveFlag(LIVE_FLAG_HIDDEN);
			self->offLiveFlag(LIVE_FLAG_UNK10000);
			self->mHolder = nullptr;
			self->stopAnmSound();
			spine->reset();
			spine->setNext(&TNerveSmallEnemyDie::theNerve());
			spine->pushAfterCurrent(spine->getDefault());
			self->genRandomItem();
			return true;
		}
	}
	return false;
}

BOOL TNerveAmiNokoFreeze::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[8];
	TAmiNoko* self = (TAmiNoko*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(2);
		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    PARTICLE_MS_DNK_SHIBIRE_B, self->mMActor->getModel()->getAnmMtx(0),
		    0, nullptr);
		if (emitter)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f));
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2))
			self->setBckAnm(15);
		else if (spine->getTime() > self->getSaveParams()->getSLFreezeWait())
			return true;
	}
	return false;
}
