#include <Animal/AnimalBase.hpp>
#include <Animal/Bird.hpp>
#include <Camera/cameralib.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/WireBinder.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Map/Map.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* bird_bastable[] = {
	nullptr,
	"/scene/bird/bas/bird_fly.bas",
	"/scene/bird/bas/bird_open.bas",
	nullptr,
	nullptr,
	"/scene/bird/bas/bird_start.bas",
	"/scene/bird/bas/bird_stop.bas",
	nullptr,
	nullptr,
};

namespace {
const int cRandomAnims[] = { 7, 4, 0, 2, 8 };

const char* const cMatName = "_mat_body1";

const GXColorS10 cColorTable[] = {
	{ 0, 100, 255, 0 },
	{ 0, 200, 0, 0 },
	{ 255, 200, 0, 0 },
	{ 255, 0, 0, 0 },
};
} // namespace

TAnimalBird::TAnimalBird(const char* name)
    : TSpineEnemy(name)
{
	mItem       = nullptr;
	mWireBinder = nullptr;
}

void TAnimalBird::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("bird_man.bmd", 0);

	mSpine->initWith(&TNerveAnimalBirdWaitOnGround::theNerve());
	initParams();

	initHitActor(0x10000032, 0, 0, 50.0f, 50.0f, 70.0f, 80.0f);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mScaledBodyRadius = 35.0f;
	initAnmSound();
}

void TAnimalBird::initParams()
{
	mHomePos.x = mPosition.x;
	mHomePos.y = mPosition.y;
	mHomePos.z = mPosition.z;
	mHomePos.y += 90.0f;

	mHomeRot.x = mRotation.x;
	mHomeRot.y = mRotation.y;
	mHomeRot.z = mRotation.z;

	mHitPoints       = getMaxHitPoints();
	mWaterproofTimer = 0;
	mFloatingTimer   = 0;
	mWalkDir         = 1.0f;
	offLiveFlag(LIVE_FLAG_AIRBORNE);
	mSpeedScale = 1.0f - 0.1f * (MsRandF() - 0.5f);

	if (TWireBinder::isOnWire(mPosition)) {
		mWireBinder = new TWireBinder;
		mWireBinder->init(mPosition);
	}
}

void TAnimalBird::load(JSUMemoryInputStream& stream)
{
	char trash[8];
	TSpineEnemy::load(stream);

	s32 eventId;
	stream >> eventId;

	if (eventId >= 0)
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(eventId, "鳥用");
	else
		mItem = TMapObjBaseManager::newAndRegisterObjByEventID(100, "");

	switch (mItem->getActorType()) {
	default:
		mColorIdx = 1;
		break;
	case 0x20000013:
		mColorIdx = 2;
		break;
	case 0x2000000F:
		mColorIdx = 3;
		break;
	case 0x20000010:
		mColorIdx = 0;
		if (TFlagManager::smInstance->getBlueCoinFlag(
		        gpMarDirector->getCurrentMap(), eventId))
			onLiveFlag(LIVE_FLAG_DEAD);
		break;
	}

	const GXColorS10* color = &cColorTable[mColorIdx];
	u32 matIdx
	    = getModel()->getModelData()->getMaterialName()->getIndex(cMatName);
	SMS_InitPacket_OneTevColor(getModel(), matIdx, GX_TEVREG1, color);
}

void TAnimalBird::loadAfter()
{
	char trash[0x18];
	TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_FLYING1,
	                                          &mPosition);
	MSoundSESystem::MSRandPlay::registerTrans(MSD_SE_OBJ_BIRD_DOL_CHUN,
	                                          &mPosition);
}

BOOL TAnimalBird::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return FALSE;

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f));
		SMSGetMSound()->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK,
		                              &sender->mPosition, 0, 0.0f, 0, 0, 4);
		if (mWaterproofTimer <= 0) {
			mWaterproofTimer = getBirdParams()->mWaterproofTimerMax.get();
			if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
				decHitPoints();
		}
		return TRUE;
	}

	if (message == HIT_MESSAGE_TAKE && mHolder == nullptr) {
		onHitFlag(HIT_FLAG_NO_COLLISION);
		mHolder = (TTakeActor*)sender;
		SMS_EasyEmitParticle(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                     nullptr, JGeometry::TVec3<f32>(1.0f));
		return TRUE;
	}

	if ((message == HIT_MESSAGE_PUT || message == HIT_MESSAGE_THROWN)
	    && mHolder == sender) {
		mHolder = nullptr;
		offHitFlag(HIT_FLAG_NO_COLLISION);
		return TRUE;
	}

	if (message == HIT_MESSAGE_UNKB) {
		mHolder = nullptr;
		if (mSpine->getLatestNerve()
		    != &TNerveAnimalBirdChangeToCoin::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else {
			kill();
		}
		return TRUE;
	}

	if (message == HIT_MESSAGE_TRAMPLE && sender->isActorType(0x1000000D)) {
		if (mSpine->getLatestNerve()
		    != &TNerveAnimalBirdChangeToCoin::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
		} else {
			receiveMessage(this, HIT_MESSAGE_SPRAYED_BY_WATER);
		}
		return TRUE;
	}

	return TSpineEnemy::receiveMessage(sender, message);
}

void TAnimalBird::calcRootMatrix()
{
	if (mHolder != nullptr) {
		MtxPtr holderMtx = mHolder->getTakingMtx();
		MTXCopy(holderMtx, getModel()->getBaseTRMtx());
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	getModel()->getBaseTRMtx()[1][3] += 35.0f;
}

void TAnimalBird::moveObject()
{
	if (mWaterproofTimer > 0)
		--mWaterproofTimer;

	TSpineBase<TLiveActor>* spine = mSpine;
	bool onGround
	    = spine->getLatestNerve() == &TNerveAnimalBirdWaitOnGround::theNerve()
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdActionOnGround::theNerve()
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdWalkOnGround::theNerve();
	if (onGround) {
		if (checkLiveFlag(LIVE_FLAG_AIRBORNE)) {
			if (getBirdParams()->mFloatingTimerMax.get() < ++mFloatingTimer) {
				mSpine->reset();
				mSpine->setNext(&TNerveAnimalBirdTakeoff::theNerve());
			}
		} else {
			mFloatingTimer = 0;
		}
	}

	bool isCoin
	    = &TNerveAnimalBirdChangeToCoin::theNerve() == mSpine->getLatestNerve();
	if (!isCoin && mHitPoints == 0) {
		mSpine->reset();
		mSpine->setNext(&TNerveAnimalBirdChangeToCoin::theNerve());
	}

	spine = mSpine;
	bool flying
	    = spine->getLatestNerve() == &TNerveAnimalBirdGraphWander::theNerve()
	      || spine->getLatestNerve() == &TNerveAnimalBirdComeback::theNerve();
	if (flying) {
		SMSGetMSound()->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_FLYING1,
		                                mInstanceIndex);
	}

	spine = mSpine;
	bool walking
	    = spine->getLatestNerve() == &TNerveAnimalBirdWaitOnGround::theNerve()
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdActionOnGround::theNerve()
	      || spine->getLatestNerve()
	             == &TNerveAnimalBirdWalkOnGround::theNerve();
	if (walking) {
		SMSGetMSound()->startSeRandPlay(MSD_SE_OBJ_BIRD_DOL_CHUN,
		                                mInstanceIndex);
	}

	TLiveActor::moveObject();
}

void TAnimalBird::bind()
{
	bool wired                    = false;
	TSpineBase<TLiveActor>* spine = mSpine;
	wired = mWireBinder != nullptr
	        && (spine->getLatestNerve()
	                == &TNerveAnimalBirdWaitOnGround::theNerve()
	            || spine->getLatestNerve()
	                   == &TNerveAnimalBirdActionOnGround::theNerve()
	            || spine->getLatestNerve()
	                   == &TNerveAnimalBirdWalkOnGround::theNerve()
	            || mSpine->getLatestNerve()
	                   == &TNerveAnimalBirdPreLanding::theNerve());

	if (!wired)
		TLiveActor::bind();
	else
		mWireBinder->bind(this);
}

const char** TAnimalBird::getBasNameTable() const { return bird_bastable; }

bool TAnimalBird::isFindMario() const
{
	f32 height = fabsf(gpMarioPos->y - mPosition.y);
	if (getBirdParams()->mSearchHeight.get() < height)
		return false;

	return isInSight(*gpMarioPos,
	                 mSpeedScale * getBirdParams()->mSearchLength.get(),
	                 mSpeedScale * getBirdParams()->mSearchAngle.get(),
	                 mSpeedScale * getBirdParams()->mSearchAware.get());
}

void TAnimalBird::doFlyToCurPathNode()
{
	JGeometry::TVec3<f32> diff = unkF4.getPoint();
	diff -= mPosition;

	f32 dist = diff.length();
	if (dist < 100.0f)
		return;

	f32 marchSpeed = mSpeedScale * getBirdParams()->mMarchSpeed.get();
	marchSpeed *= SMSGetAnmFrameRate();
	f32 turnSpeed = getBirdParams()->mTurnSpeed.get();
	turnSpeed *= SMSGetAnmFrameRate();

	if (dist <= 2.0f * calcMinimumTurnRadius(marchSpeed, turnSpeed))
		turnSpeed = calcTurnSpeedToReach(marchSpeed, 0.5f * dist);

	TAnimalBase::getRotationFlyToDir(&mRotation, diff, marchSpeed, turnSpeed);

	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	JGeometry::TVec3<f32> vel(0.0f, 0.0f, marchSpeed);
	quat.rotate(vel, vel);

	vel.scale(1.0f
	          - (f32)mWaterproofTimer
	                / (f32)getBirdParams()->mWaterproofTimerMax.get());
	f32 ratio = (f32)mWaterproofTimer
	            / (f32)getBirdParams()->mWaterproofTimerMax.get();
	vel.y -= getBirdParams()->mWaterPowerY.get() * ratio;
	mLinearVelocity = vel;
}

bool TAnimalBird::doLanding(bool start)
{
	if (start) {
		f32 speed = mSpeedScale * getBirdParams()->mMarchSpeed.get();
		speed *= SMSGetAnmFrameRate();

		JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
		JGeometry::TVec3<f32> vel(0.0f, 0.0f, speed);
		quat.rotate(vel, vel);
		mVelocity.set(vel.x, 0.0f, vel.z);
	}

	JGeometry::TVec3<f32> fall(0.0f, 0.0f, 0.0f);
	bool landed = false;
	if (mWireBinder != nullptr)
		mWireBinder->getPoint(&fall, mHomePos);
	else
		gpMap->checkGround(mPosition, &mGroundPlane);

	if (checkLiveFlag(LIVE_FLAG_AIRBORNE))
		fall.y = -getBirdParams()->mLandingGravityY.get();
	else
		landed = true;

	mRotation.x = mHomeRot.x;
	mRotation.z = mHomeRot.z;

	f32 torqueParam = getBirdParams()->mLandingTorqueY.get();
	f32 torque      = torqueParam * SMSGetAnmFrameRate();
	f32 turn        = JGeometry::TUtil<f32>::clamp(
	    MsAngleDiff(mHomeRot.y, mRotation.y), -torque, torque);
	mRotation.y = MsWrap(mRotation.y + turn, 0.0f, 360.0f);

	mLinearVelocity = fall;

	JGeometry::TVec3<f32> velocity = mVelocity;
	f32 speed = JGeometry::TUtil<f32>::sqrt(velocity.squared());
	JGeometry::TVec3<f32> vel(0.0f, 0.0f, speed);
	vel.scale(getBirdParams()->mLandingFric.get());

	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(mRotation);
	quat.rotate(vel, vel);
	mVelocity = vel;

	return landed && fabsf(turn) < 0.01f;
}

TAnimalBirdParams::TAnimalBirdParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mMarchSpeed, 5.0f)
    , PARAM_INIT(mTurnSpeed, 0.1f)
    , PARAM_INIT(mReturnTimer, 1800)
    , PARAM_INIT(mSearchLength, 800.0f)
    , PARAM_INIT(mSearchHeight, 600.0f)
    , PARAM_INIT(mSearchAware, 400.0f)
    , PARAM_INIT(mSearchAngle, 90.0f)
    , PARAM_INIT(mActionTimer, 100)
    , PARAM_INIT(mWaterproofTimerMax, 45)
    , PARAM_INIT(mFloatingTimerMax, 30)
    , PARAM_INIT(mLandingGravityY, 1.0f)
    , PARAM_INIT(mLandingTorqueY, 2.0f)
    , PARAM_INIT(mWalkingTorqueY, 0.5f)
    , PARAM_INIT(mWalkingSpeed, 2.0f)
    , PARAM_INIT(mWalkTimer, 100)
    , PARAM_INIT(mLandingFric, 0.95f)
    , PARAM_INIT(mActionTimerAdd, 300)
    , PARAM_INIT(mWaterPowerY, 15.0f)
{
	TParams::load(mPrmPath);
}

TAnimalBirdManager::TAnimalBirdManager(const char* name)
    : TEnemyManager(name)
{
}

void TAnimalBirdManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TAnimalBirdParams("/Animal/bird.prm");
	TEnemyManager::load(stream);
}

void TAnimalBirdManager::loadAfter()
{
	char trash[0x20];
	TNameRef::loadAfter();
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_FLYING1,
	                                              mObjNum);
	MSoundSESystem::MSRandPlay::createRandPlayVec(MSD_SE_OBJ_BIRD_DOL_CHUN,
	                                              mObjNum);
}

void TAnimalBirdManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bird_man.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (1 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

DEFINE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(7);
		bird->setCurAnmSound();
	}

	bool flee = (bird->mWaterproofTimer > 0 || bird->isFindMario())
	            && MsRandF() < 0.5f;
	if (flee) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	if (bird->checkCurAnmEnd(ANM_TYPE_BCK)) {
		int over = bird->mSpine->getTime()
		           - bird->getBirdParams()->mActionTimer.get();
		bool doAction;
		if (over < 0) {
			doAction = false;
		} else {
			f32 prob
			    = (f32)over / (f32)bird->getBirdParams()->mActionTimerAdd.get();
			doAction = MsRandF() < prob;
		}

		if (doAction) {
			spine->pushAfterCurrent(
			    &TNerveAnimalBirdActionOnGround::theNerve());
			return TRUE;
		}
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		int anm = cRandomAnims[(int)(5.0f * MsRandF())];
		if (anm == 8) {
			spine->pushAfterCurrent(&TNerveAnimalBirdWalkOnGround::theNerve());
			return TRUE;
		}
		bird->mMActor->setBck(anm);
	}

	bool flee = (bird->mWaterproofTimer > 0 || bird->isFindMario())
	            && MsRandF() < 0.5f;
	if (flee) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	if (bird->checkCurAnmEnd(ANM_TYPE_BCK)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mWalkDir *= -1.0f;
		bird->mMActor->setBckFromIndex(8);
		bird->setCurAnmSound();
	}

	bool flee = (bird->mWaterproofTimer > 0 || bird->isFindMario())
	            && MsRandF() < 0.5f;
	if (flee) {
		spine->pushAfterCurrent(&TNerveAnimalBirdTakeoff::theNerve());
		return TRUE;
	}

	bird->mGravity = 0.15f;

	f32 walkTorque = bird->getBirdParams()->mWalkingTorqueY.get();
	bird->mRotation.y
	    = MsWrap<f32>(bird->mWalkDir * (walkTorque * SMSGetAnmFrameRate())
	                      + bird->mRotation.y,
	                  0.0f, 360.0f);

	JGeometry::TQuat4<f32> quat = SMS_Eular2Quat(bird->mRotation);
	JGeometry::TVec3<f32> vel(0.0f, 0.0f,
	                          bird->getBirdParams()->mWalkingSpeed.get());
	quat.rotate(vel, vel);
	bird->mLinearVelocity = vel;

	if (bird->getBirdParams()->mWalkTimer.get() < spine->getTime()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(5);
		bird->setCurAnmSound();
		bird->onLiveFlag(LIVE_FLAG_AIRBORNE);
		bird->mGravity       = 0.0f;
		bird->mFloatingTimer = 0;

		J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(3.0f * ctrl->getRate());
		SMSGetMSound()->startSoundActor(MSD_SE_OBJ_BIRD_DOL_TO_FLY1,
		                                &bird->mPosition, 0, nullptr, 0, 4);
	}

	if (bird->checkCurAnmEnd(ANM_TYPE_BCK)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		bird->onLiveFlag(LIVE_FLAG_AIRBORNE);
		bird->mGravity       = 0.0f;
		bird->mFloatingTimer = 0;
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
		bird->unk124->reset();
		bird->goToShortestNextGraphNode();
	}

	if (spine->getTime() == 0 || bird->isReachedToGoal()) {
		bird->goToRandomNextGraphNode();

		JGeometry::TVec3<f32> pos = bird->unk104.getPoint();
		pos.x += 200.0f * (MsRandF() - 0.5f);
		pos.y += 200.0f * (MsRandF() - 0.5f);
		pos.z += 200.0f * (MsRandF() - 0.5f);
		bird->setGoalPath(pos);

		if (bird->mPosition.y <= bird->getUnkF4().getPoint().y) {
			bird->mMActor->setBckFromIndex(1);
			bird->setCurAnmSound();
		} else {
			bird->mMActor->setBckFromIndex(3);
			bird->setCurAnmSound();
		}
	}

	bird->checkCurAnmEnd(ANM_TYPE_BCK);

	if (bird->getBirdParams()->mReturnTimer.get() < bird->mSpine->getTime()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdComeback::theNerve());
		return TRUE;
	}

	bird->doFlyToCurPathNode();
	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor)
{
	char trash[8];
	TMapObjBase* obj;
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->onLiveFlag(LIVE_FLAG_DEAD);

		if (bird->mItem->isActorType(0x20000013)) {
			bird->mItem->JSGSetTranslation(bird->mPosition);
			((TShine*)bird->mItem)->appearWithDemo("鳥シャインカメラ");
		} else {
			if (bird->mItem->isActorType(0x2000000E))
				obj = gpItemManager->makeObjAppear(0x2000000E);
			else
				obj = bird->mItem;

			if (obj != nullptr) {
				obj->appear();
				obj->JSGSetTranslation(bird->mPosition);
				obj->setVelocityAndFlag10(0.0f, -10.0f, 0.0f);
				obj->onLiveFlag(LIVE_FLAG_AIRBORNE);
			}
		}
	}

	return TRUE;
}

DEFINE_NERVE(TNerveAnimalBirdComeback, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->setGoalPath(TPathNode(bird->mHomePos));
		bird->mMActor->setBckFromIndex(3);
		bird->setCurAnmSound();
	}

	bird->doFlyToCurPathNode();

	if (bird->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (bird->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdPreLanding::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor)
{
	TAnimalBird* bird = (TAnimalBird*)spine->getBody();

	if (spine->getTime() == 0) {
		bird->mMActor->setBckFromIndex(1);
		bird->setCurAnmSound();
		J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(1.5f * ctrl->getRate());
		bird->doLanding(true);
	}

	if (bird->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (bird->doLanding(false)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdLanding::theNerve());
		return TRUE;
	}

	return FALSE;
}

DEFINE_NERVE(TNerveAnimalBirdLanding, TLiveActor)
{
	TAnimalBird* bird  = (TAnimalBird*)spine->getBody();
	J3DFrameCtrl* ctrl = bird->mMActor->getFrameCtrl(ANM_TYPE_BCK);

	if (spine->getTime() == 0) {
		bird->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
		bird->mMActor->setBckFromIndex(5);
		bird->setCurAnmSound();
		ctrl->setAttribute(J3DFrameCtrl::ATTR_ONCE_AND_RESET);
		ctrl->setFrame(ctrl->getEnd());
		ctrl->setRate(-1.0f * ctrl->getRate());
	}

	if (bird->isFindMario()) {
		spine->pushAfterCurrent(&TNerveAnimalBirdGraphWander::theNerve());
		return TRUE;
	}

	if (ctrl->checkState(J3DFrameCtrl::STATE_COMPLETED_ONCE)) {
		spine->pushAfterCurrent(&TNerveAnimalBirdWaitOnGround::theNerve());
		return TRUE;
	}

	return FALSE;
}
