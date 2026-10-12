#include <Enemy/Rocket.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/WaterGun.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* rocket_bastable[] = { nullptr, nullptr, nullptr, nullptr };

f32 TRocket::mTestAng_x     = 0.0f;
f32 TRocket::mTestAng_y     = 90.0f;
f32 TRocket::mTestAng_z     = 0.0f;
f32 TRocket::mNozzleOffsetZ = 25.0f;
f32 TRocket::mColOffsetY    = 20.0f;

TRocketManager::TRocketManager(const char* name)
    : TSmallEnemyManager(name)
    , mIsFree(true)
    , unk64(0)
    , mExplodeEmitInfo(nullptr)
{
}

void TRocketManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38            = new TRocketSaveLoadParams("/enemy/rocket.prm");
	mExplodeEmitInfo = new TWaterEmitInfo("/enemy/rocketexpwater.prm");
}

void TRocketManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

TSpineEnemy* TRocketManager::createEnemyInstance() { return new TRocket; }

void TRocketManager::initSetEnemies()
{
	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph    = gpConductor->getGraphByName("main");
		TSpineEnemy* rocket = getObj(i);
		if (rocket->checkLiveFlag(LIVE_FLAG_DEAD) && !graph->isDummy()) {
			TMsRange<s32> range(0, graph->getNodeNum());
			JGeometry::TVec3<f32> point;
			graph->getGraphNode(range.rand()).getPoint(&point);
			rocket->mPosition = point;
			rocket->mPosition.y += 5.0f;
			rocket->onLiveFlag(LIVE_FLAG_AIRBORNE);
			rocket->reset();
		}
	}
}

void TRocketManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "rocket.bmd", 0x10040000, 0 },
		{ nullptr, 0, 0 },
	};

	createModelDataArray(entry);
}

void TRocketManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		for (int i = 0; i < getActiveObjNum(); ++i) {
			TSpineEnemy* rocket = getObj(i);
			if (rocket->checkLiveFlag(LIVE_FLAG_DEAD))
				rocket->reset();
		}
	}

	TEnemyManager::perform(cue, graphics);
}

TRocket::TRocket(const char* name)
    : TSmallEnemy(name)
    , mIsPossessed(false)
    , mHasInitialPosition(false)
{
}

void TRocket::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	mInitialPosition    = mPosition;
	mHasInitialPosition = true;
	reset();
}

void TRocket::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);
	mActorType = 0x1000002B;
	unk150     = 17;
	mParams    = (TRocketSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveRocketWait::theNerve());
	onHitFlag(HIT_FLAG_UNK8000000);
}

void TRocket::calcRootMatrix()
{
	if (mIsPossessed) {
		getModel()->setBaseScale(mScaling);

		TPosition3f mtx;
		if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
			mtx.translation(mPosition.x, mPosition.y, mPosition.z);
		} else {
			MTXCopy(SMS_GetMarioWaterGun()->getEmitMtx(0), mtx);

			// normalize the three axes of the nozzle's emit matrix. Note that
			// the original guards each axis with the length of the next one
			// (z, x, y) while dividing by its own length (x, y, z).
			JGeometry::TVec3<f32> axis[3];
			axis[0].x = mtx.at(0, 0);
			axis[0].y = mtx.at(1, 0);
			axis[0].z = mtx.at(2, 0);
			f32 xLength = axis[0].length();

			axis[1].x = mtx.at(0, 1);
			axis[1].y = mtx.at(1, 1);
			axis[1].z = mtx.at(2, 1);
			f32 yLength = axis[1].length();

			axis[2].x = mtx.at(0, 2);
			axis[2].y = mtx.at(1, 2);
			axis[2].z = mtx.at(2, 2);
			f32 zLength = axis[2].length();

			if (zLength != 0.0f) {
				mtx.ref(0, 0) /= xLength;
				mtx.ref(1, 0) /= xLength;
				mtx.ref(2, 0) /= xLength;
			}
			if (xLength != 0.0f) {
				mtx.ref(0, 1) /= yLength;
				mtx.ref(1, 1) /= yLength;
				mtx.ref(2, 1) /= yLength;
			}
			if (yLength != 0.0f) {
				mtx.ref(0, 2) /= zLength;
				mtx.ref(1, 2) /= zLength;
				mtx.ref(2, 2) /= zLength;
			}

			TPosition3f offset;
			offset.translation(mNozzleOffsetZ, 0.0f, 0.0f);
			MTXConcat(mtx, offset, mtx);

			mPosition.x = mtx.at(0, 3);
			mPosition.y = mtx.at(1, 3) - mColOffsetY;
			mPosition.z = mtx.at(2, 3);
		}

		Mtx rot;
		MsMtxSetRotRPH(rot, mTestAng_x, mTestAng_y, mTestAng_z);
		MTXConcat(mtx, rot, mtx);
		getModel()->setBaseTRMtx(mtx);
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (isBckAnm(1)) {
		gpMSound->startSoundActor(MSD_SE_PO_PETBOTTLE_FLY, &mPosition, 0,
		                          nullptr, 0, 4);
	}
}

void TRocket::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("rocket.bmd", 3);
}

void TRocket::reset()
{
	mIsPossessed = false;
	TSmallEnemy::reset();
	if (mHasInitialPosition)
		mPosition = mInitialPosition;
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK800);
	onLiveFlag(LIVE_FLAG_UNK8);
	mSpine->initWith(&TNerveRocketWait::theNerve());
}

void TRocket::attackToMario()
{
	if (mSpine->getCurrentNerve() == &TNerveRocketWait::theNerve()
	    && static_cast<TRocketManager*>(mManager)->mIsFree) {
		mSpine->pushNerve(&TNerveRocketPossessedNozzle::theNerve());
	}
}

void TRocket::behaveToWater(THitActor*) { attackToMario(); }

void TRocket::bind()
{
	if (!checkLiveFlag(LIVE_FLAG_UNK10)) {
		if (mSpine->getCurrentNerve()
		        == &TNerveRocketPossessedNozzle::theNerve()
		    || mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
			TBGWallCheckRecord record(mPosition.x, mPosition.y, mPosition.z,
			                          getWallRadius(), 1, 0);
			char trash[0x20];
			if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
				THitActor* wallActor = const_cast<TLiveActor*>(
				    record.mResultWalls[0]->getActor());
				if (wallActor != nullptr)
					wallActor->receiveMessage(this, HIT_MESSAGE_ATTACK);
				kill();
			} else if (mSpine->getCurrentNerve()
			           == &TNerveRocketFly::theNerve()) {
				TLiveActor::bind();
				if (!isAirborne()) {
					THitActor* groundActor
					    = const_cast<TLiveActor*>(mGroundPlane->getActor());
					if (groundActor != nullptr)
						groundActor->receiveMessage(this, HIT_MESSAGE_ATTACK);
					kill();
				}
			}
		} else {
			TLiveActor::bind();
		}
	}
}

void TRocket::setDeadAnm()
{
	TRocketManager* manager = static_cast<TRocketManager*>(mManager);
	JGeometry::TVec3<f32> position = mPosition;
	char trash[0x28];
	// the original reads the emit direction here and drops the result
	JGeometry::TVec3<f32> direction;
	direction.set(manager->mExplodeEmitInfo->mDir.value);
	manager->mExplodeEmitInfo->mPos.value = position;
	gpModelWaterManager->emitRequest(*manager->mExplodeEmitInfo);

	if (mIsPossessed) {
		static_cast<TRocketManager*>(mManager)->mIsFree = true;
		mIsPossessed                                   = false;
	}

	onLiveFlag(LIVE_FLAG_UNK20000);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_PETBTL_BOMB_A, mMActor->getModel()->getBaseTRMtx(), 0,
	    nullptr);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    PARTICLE_MS_PETBTL_BOMB_B, mMActor->getModel()->getBaseTRMtx(), 0,
	    nullptr);
}

f32 TRocket::getGravityY() const
{
	f32 gravity = mGravity;
	if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve())
		gravity = mParams->mSLFlyGravity.get();
	return gravity;
}

bool TRocket::isCollidMove(THitActor* other)
{
	if (mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve()) {
		if (other->receiveMessage(this, HIT_MESSAGE_TRAMPLE))
			kill();
	}
	return false;
}

bool TRocket::isAttack()
{
	return mSpine->getCurrentNerve() == &TNerveRocketFly::theNerve() ? true
	                                                                 : false;
}

const char** TRocket::getBasNameTable() const { return rocket_bastable; }

DEFINE_NERVE(TNerveRocketPossessedNozzle, TLiveActor)
{
	char trash[0x68];
	TRocket* self = static_cast<TRocket*>(spine->getBody());

	if (spine->getTime() == 0) {
		SMSRumbleMgr->start(21, 10, static_cast<f32*>(nullptr));
		gpMSound->startSoundActor(MSD_SE_MA_GET_ITEM, &self->mPosition, 0,
		                          nullptr, 0, 4);
		gpMSound->startSoundActor(MSD_SE_PO_GET_PETBOTTLE, &self->mPosition, 0,
		                          nullptr, 0, 4);
		static_cast<TRocketManager*>(self->mManager)->mIsFree = false;
		self->offLiveFlag(LIVE_FLAG_UNK10);
		self->mIsPossessed = true;
		self->setBckAnm(0);
	}

	SMS_SendMessageToMario(self, HIT_MESSAGE_UNK5);

	if ((u8)gpMarDirector->unk18[0]->mCompSPos[3] > 20
	    && self->getHitPoints() > 1) {
		self->mHitPoints -= 1;
	}

	if (!self->isBckAnm(2)) {
		gpMSound->startSoundSystemSE(MSD_SE_SY_PO_WATER_FULL, 0, nullptr, 0);
		self->setBckAnm(2);
	}

	bool released;
	if (gpMarDirector->unk18[0]->checkFrameMeaning(TMarioGamePad::MEANING_R)) {
		self->unk190 = 2.0f;
		self->expandCollision();
		gpMSound->startSoundActor(MSD_SE_PO_PETBOTTLE_FLY, &self->mPosition, 0,
		                          nullptr, 0, 4);
		SMSRumbleMgr->start(21, 5, static_cast<f32*>(nullptr));
		released = true;
	} else {
		released = false;
	}

	if (released) {
		spine->pushAfterCurrent(&TNerveRocketFly::theNerve());
		return true;
	}

	return false;
}

DEFINE_NERVE(TNerveRocketFly, TLiveActor)
{
	char trash[0x48];
	TRocket* self = static_cast<TRocket*>(spine->getBody());

	if (spine->getTime() == 0) {
		self->setBckAnm(1);

		MtxPtr emitMtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 speed      = self->mParams->mSLReleaseSpeed.get();
		JGeometry::TVec3<f32> velocity;
		velocity.x = speed * emitMtx[0][0];
		velocity.y = speed * emitMtx[1][0];
		velocity.z = speed * emitMtx[2][0];
		self->mVelocity = velocity;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		static_cast<TRocketManager*>(self->mManager)->mIsFree = true;
		self->mIsPossessed                                    = false;
		f32 yaw = MsGetRotFromZaxisY(velocity);
		self->mRotation.set(0.0f, MsWrap(yaw, 0.0f, 360.0f), 0.0f);
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	if (!self->isBckAnm(1))
		self->setBckAnm(1);

	JGeometry::TVec3<f32> direction = self->mVelocity;
	self->mRotation.x               = MsGetRotFromZaxis(direction).x;
	gpMarioParticleManager->emitAndBindToPosPtr(
	    PARTICLE_MS_PETBTL_SMOKE, &self->mPosition, 1, self);

	if (self->mSpine->getTime() > self->mParams->mSLFlyLimitTime.get())
		self->kill();

	// the original calls getModel() here and drops the result
	if (!self->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT))
		self->getModel();

	return false;
}

DEFINE_NERVE(TNerveRocketWait, TLiveActor)
{
	TRocket* self = static_cast<TRocket*>(spine->getBody());

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10);
		self->setBckAnm(3);
	}

	return false;
}
