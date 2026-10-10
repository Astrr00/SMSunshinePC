#include <Enemy/LimitKoopaJr.hpp>
#include <Enemy/Koopa.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* koopajr_bastable[] = {
	"/scene/koopajr/bas/koopajr_damage.bas",
	"/scene/koopajr/bas/koopajr_shoot.bas",
	nullptr,
	"/scene/koopajr/bas/koopajr_yahoo.bas",
};

TLimitKoopaJrParams::TLimitKoopaJrParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLAcceleration, 1.0f)
    , PARAM_INIT(mSLRotationSpeed, 1.0f)
    , PARAM_INIT(mSLSpeedMax, 8.0f)
    , PARAM_INIT(mSLRoundAngleVelocity, 0.05f)
    , PARAM_INIT(mSLRoundRadius, 2000.0f)
    , PARAM_INIT(mSLRoundHeight, 0.0f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight, 4000.0f)
    , PARAM_INIT(mSLKoopaJrScale, 1.6f)
    , PARAM_INIT(mSLShotDoodlePeriod, 1200)
    , PARAM_INIT(mSLDamagePeriod, 360)
{
	TParams::load(mPrmPath);
	mSLAcceleration.set(1.0f);
	mSLRotationSpeed.set(1.0f);
	mSLSpeedMax.set(15.0f);
	mSLRoundAngleVelocity.set(0.08f);
	mSLRoundRadius.set(3300.0f);
	mSLRoundHeight.set(5400.0f);
	mSLDamageRadius.set(100.0f);
	mSLDamageHeight.set(300.0f);
	mSLKoopaJrScale.set(2.0f);
	mSLDamagePeriod.set(120);
	mSLShotDoodlePeriod.set(600);
}

TLimitKoopaJr::TLimitKoopaJr(const char* name)
    : TSpineEnemy(name)
    , mBathtub(nullptr)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}

void TLimitKoopaJr::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("koopajr_model.bmd", 0);
	mMActor->setLightType(1);
	initAnmSound();

	f32 damageHeight = getSaveParam2()->mSLDamageHeight.get();
	initHitActor(0x0800002E, 1, 0, 0.0f, 0.0f,
	             getSaveParam2()->mSLDamageRadius.get(), damageHeight);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	mSpine->initWith(&TNerveLimitKoopaJrRun::theNerve());

	f32 scale = getSaveParam2()->mSLKoopaJrScale.get();
	mScaling.set(scale, scale, scale);

	mSpine->reset();
	unk158           = 0;
	mShotDoodleTimer = 0;
	mShotDoodleTimer = getSaveParam2()->mSLShotDoodlePeriod.get();
	unk160.set(0.0f, 0.0f, 0.0f, 1.0f);
	mFaceDir.mDirection  = 0.0f;
	mRoundDir.mDirection = 0.0f;

	getModel()->getModelData()->onFlag1OnAllShapes();
}

void TLimitKoopaJr::reset()
{
	TSpineEnemy::reset();

	mSpine->reset();
	unk158           = 0;
	mShotDoodleTimer = 0;
	mShotDoodleTimer = getSaveParam2()->mSLShotDoodlePeriod.get();
	unk160.set(0.0f, 0.0f, 0.0f, 1.0f);
	mFaceDir.mDirection  = 0.0f;
	mRoundDir.mDirection = 0.0f;
}

void TLimitKoopaJr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!mKoopa) {
		TKoopaManager* manager
		    = (TKoopaManager*)JDrama::TNameRefGen::search("クッパマネージャー");
		mKoopa = (TKoopa*)manager->getObj(0);
	}

	if (!mBathtub)
		mBathtub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

	if (cue & CUE_MOVE) {
		if (unk158 > 0)
			unk158 -= 1;
		if (mShotDoodleTimer > 0)
			mShotDoodleTimer -= 1;

		if (mSpine->getCurrentNerve() != &TNerveLimitKoopaJrWait::theNerve()) {
			TNerveLimitKoopaJrRun::theNerve();
		}
	}

	TSpineEnemy::perform(cue, graphics);
}

void TLimitKoopaJr::bind() { }

void TLimitKoopaJr::calcRootMatrix()
{
	f32 halfAngle = 0.5f * mFaceDir.mDirection;
	f32 s         = sinf(halfAngle);
	f32 c         = cosf(halfAngle);

	JGeometry::TQuat4<f32> quat(0.0f, s, 0.0f, c);
	TPosition3f mtx;
	mtx.setQT(quat, mPosition);

	getModel()->setBaseTRMtx(mtx);
	getModel()->setBaseScale(mScaling);
}

const char** TLimitKoopaJr::getBasNameTable() const
{
	return koopajr_bastable;
}

BOOL TLimitKoopaJr::receiveMessage(THitActor*, u32 message)
{
	if (message == 0xF)
		return true;

	return false;
}

void TLimitKoopaJr::moveRun()
{
	f32 turnSpeed
	    = 0.017453294f * getSaveParam2()->mSLRoundAngleVelocity.get();

	TDirectionCalc tubDir;
	JGeometry::TVec3<f32> toTub;
	toTub.sub(mBathtub->mPosition, *gpMarioPos);
	toTub.y = 0.0f;
	tubDir.makeDirection(toTub);

	TDirectionCalc nextDir(
	    mRoundDir.calcTurnDirection(tubDir.mDirection, turnSpeed));
	f32 turned = nextDir.sub(mRoundDir.mDirection);
	mRoundDir.mDirection = nextDir.mDirection;
	mRoundRadius         = getSaveParam2()->mSLRoundRadius.get();

	JGeometry::TVec3<f32> offset = mRoundDir.calcDirectionVector();
	offset *= mRoundRadius;
	mPosition.add(mBathtub->mPosition, offset);
	mPosition.y = getSaveParam2()->mSLRoundHeight.get();

	offset.normalize();
	JGeometry::TVec3<f32> tangent;
	tangent.cross(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f), offset);
	tangent.normalize();
	if (turned < 0.0f)
		tangent.negate();

	JGeometry::TVec3<f32> tangent2 = tangent;
	tangent2.normalize();
	TDirectionCalc faceDir(tangent2);
	mFaceDir.mDirection = mFaceDir.calcTurnDirection(
	    faceDir.mDirection,
	    TDirectionCalc::d2r(getSaveParam2()->mSLRotationSpeed.get()));
}

BOOL TNerveLimitKoopaJrRun::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopaJr* self = (TLimitKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(2);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[2]);
	}

	TDirectionCalc tubDir;
	JGeometry::TVec3<f32> toTub;
	toTub.sub(self->mBathtub->mPosition, *gpMarioPos);
	toTub.y = 0.0f;
	tubDir.makeDirection(toTub);

	f32 tubAngle  = self->mRoundDir.absDirection(tubDir.mDirection);
	bool isOffTub = tubAngle <= 0.62831855f ? false : true;
	if (!isOffTub) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrWait::theNerve());
		return true;
	}

	self->moveRun();
	return false;
}

BOOL TNerveLimitKoopaJrWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopaJr* self = (TLimitKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(2);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[2]);
	}

	TDirectionCalc tubDir;
	JGeometry::TVec3<f32> toTub;
	toTub.sub(self->mBathtub->mPosition, *gpMarioPos);
	toTub.y = 0.0f;
	tubDir.makeDirection(toTub);

	f32 tubAngle  = self->mRoundDir.absDirection(tubDir.mDirection);
	bool isOffTub = tubAngle <= 0.62831855f ? false : true;
	if (isOffTub) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrRun::theNerve());
		return true;
	}

	if (SMS_IsMarioStatusTypeJumping()) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrYahoo::theNerve());
		return true;
	}

	if (self->mShotDoodleTimer <= 0) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrLaunch::theNerve());
		return true;
	}

	JGeometry::TVec3<f32> toMario;
	toMario.sub(*gpMarioPos, self->mPosition);
	toMario.y = 0.0f;
	JGeometry::TVec3<f32> dir = toMario;
	dir.normalize();
	TDirectionCalc marioDir(dir);
	self->mFaceDir.mDirection = self->mFaceDir.calcTurnDirection(
	    marioDir.mDirection,
	    TDirectionCalc::d2r(self->getSaveParam2()->mSLRotationSpeed.get()));
	return false;
}

BOOL TNerveLimitKoopaJrLaunch::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopaJr* self = (TLimitKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(1);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[1]);
		self->mShotDoodleTimer
		    = self->getSaveParam2()->mSLShotDoodlePeriod.get();
	}

	if (self->mMActor->curAnmEndsNext()) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrWait::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveLimitKoopaJrYahoo::execute(TSpineBase<TLiveActor>* spine) const
{
	TLimitKoopaJr* self = (TLimitKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(3);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[3]);
	}

	if (self->mMActor->curAnmEndsNext()) {
		spine->pushAfterCurrent(&TNerveLimitKoopaJrWait::theNerve());
		return true;
	}

	return false;
}

TLimitKoopaJrManager::TLimitKoopaJrManager(const char* name)
    : TEnemyManager(name)
{
}

void TLimitKoopaJrManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopajr_model.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TLimitKoopaJrManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TLimitKoopaJrParams("/enemy/limitkoopajr.prm");
}

void TLimitKoopaJrManager::loadAfter() { }

TSpineEnemy* TLimitKoopaJrManager::createEnemyInstance() { return nullptr; }
