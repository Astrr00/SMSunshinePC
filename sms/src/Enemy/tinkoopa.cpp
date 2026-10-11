#include <Enemy/TinKoopa.hpp>
#include <Enemy/CoasterKiller.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Enemy/Graph.hpp>
#include <Camera/CameraShake.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Strategy.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* tinkoopa_bastable[] = {
	"/scene/tinkoopa/bas/tinkoopa_break1.bas",
	"/scene/tinkoopa/bas/tinkoopa_break2.bas",
	"/scene/tinkoopa/bas/tinkoopa_break3.bas",
	"/scene/tinkoopa/bas/tinkoopa_break4.bas",
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_damage1.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage2.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage3.bas",
	"/scene/tinkoopa/bas/tinkoopa_damage4.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/tinkoopa/bas/tinkoopa_wait1.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait2.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait3.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait4.bas",
	"/scene/tinkoopa/bas/tinkoopa_wait5.bas",
};

TTinKoopaParams::TTinKoopaParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLPartsHP, 2)
    , PARAM_INIT(mSLFlameHP, 10)
    , PARAM_INIT(mSLFlameRevivalTime, 10)
    , PARAM_INIT(mSLFlameDamageRadius0, 100.0f)
    , PARAM_INIT(mSLFlameDamageHeight0, 400.0f)
    , PARAM_INIT(mSLFlameDamageRadius1, 100.0f)
    , PARAM_INIT(mSLFlameDamageHeight1, 400.0f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight0, 4000.0f)
    , PARAM_INIT(mSLDamageHeight1, 4000.0f)
    , PARAM_INIT(mSLKillerInterval, 30)
    , PARAM_INIT(mSLDefeatWaitTime, 600)
    , PARAM_INIT(mSLKillerApproachingDistance, 6000.0f)
{
	TParams::load(mPrmPath);

	mSLPartsHP.set(1);
	mSLFlameHP.set(50);
	mSLFlameRevivalTime.set(1200);
	mSLFlameDamageRadius0.set(500.0f);
	mSLFlameDamageHeight0.set(300.0f);
	mSLFlameDamageRadius1.set(700.0f);
	mSLFlameDamageHeight1.set(400.0f);
	mSLKillerInterval.set(60);
	mSLDamageRadius.set(800.0f);
	mSLDamageHeight0.set(4000.0f);
	mSLDamageHeight1.set(3400.0f);
	mSLDefeatWaitTime.set(160);
	mSLKillerApproachingDistance.set(2000.0f);
}

static const char* TTinKoopa_jointNameTable[] = {
	"jnt_head",  "jnt_breast",   "jnt_stomach",  "jnt_rarm",
	"jnt_larm",  "jnt_leg",      "jnt_leye",     "jnt_reye",
	"fire_null", "fire_col_null", "jnt_femur",   "killer_null1",
	"killer_null2", "killer_null3", "killer_null4",
};

static int TTinKoopa_jointIndexTable[15];

static const TNerveTinKoopaWait& theNerveWait()
{
	static TNerveTinKoopaWait instance;
	return instance;
}

static const TNerveTinKoopaDamage& theNerveDamage()
{
	static TNerveTinKoopaDamage instance;
	return instance;
}

static const TNerveTinKoopaBreak& theNerveBreak()
{
	static TNerveTinKoopaBreak instance;
	return instance;
}

// Index 0 is the head, 1..4 are the parts that can be shot off and 5 is the
// leg (which stays).
static inline const char* getPartsColFile(int kind)
{
	static const char* table[] = {
		"/scene/tinkoopa/head_col.col",   "/scene/tinkoopa/breast_col.col",
		"/scene/tinkoopa/stomach_col.col", "/scene/tinkoopa/rarm_col.col",
		"/scene/tinkoopa/larm_col.col",   "/scene/tinkoopa/leg_col.col",
	};
	return table[kind];
}

static inline const char* getPartsBmd(int kind)
{
	static const char* table[] = {
		nullptr,
		"tinkoopa_breast.bmd",
		"tinkoopa_stomach.bmd",
		"tinkoopa_rarm.bmd",
		"tinkoopa_larm.bmd",
		nullptr,
	};
	return table[kind];
}

static inline int getPartsBreakBck(int kind)
{
	static int table[] = { 0, 4, 0xB, 9, 0xA, 0 };
	return table[kind];
}

static inline u32 getPartsActorType(int kind)
{
	static u32 table[] = { 0x08000019, 0x0800001A, 0x0800001B,
		                         0x0800001D, 0x0800001C, 0x0800001E };
	return table[kind];
}

// Everything below is indexed by TTinKoopa::mState.
static inline int getWaitBck(int state)
{
	static int table[] = { 0xC, 0xD, 0xE, 0xF, 0x10 };
	return table[state];
}

static inline int getBreakBck(int state)
{
	static int table[] = { 0, 1, 2, 3, 0x10 };
	return table[state];
}

static inline int getDamageBck(int state)
{
	static int table[] = { 5, 6, 7, 8, 0x10 };
	return table[state];
}

static inline int getBreakFrame(int state)
{
	static int table[] = { 100, 134, 134, 100, 0 };
	return table[state];
}

static inline int getBreakPartIndex(int state)
{
	static int table[] = { 2, 3, 4, 1, 2 };
	return table[state];
}

static const char* breastTrackJointNameTable[] = {
	"breast_1", "breast_2", "breast_3", "breast_4", "breast_5", "breast_6",
};

static const char* bellyTrackJointNameTable[] = {
	"stomach_1", "stomach_2", "stomach_3",
	"stomach_4", "stomach_5", "stomach_6",
};

static const char* rightArmTrackJointNameTable[] = {
	"rarm_1",
	"rarm_2",
	"rarm_3",
	"rarm_4",
};

static const char* leftArmTrackJointNameTable[] = {
	"larm_1",
	"larm_2",
	"larm_3",
	"larm_4",
};

static inline bool checkRailPass(MActor* rail, int frame)
{
	if (rail == nullptr)
		return false;
	return rail->getFrameCtrl(0)->checkPass(frame) ? true : false;
}

inline TTinKoopaFlame::TTinKoopaFlame(TTinKoopa* koopa)
    : THitActor("flame")
    , mOwner(koopa)
{
	f32 damageHeight = mOwner->getSaveParam2()->mSLFlameDamageHeight0.get();
	f32 damageRadius = mOwner->getSaveParam2()->mSLFlameDamageRadius0.get();
	initHitActor(0x8000027, 0, 0, 0.0f, 0.0f, damageRadius, damageHeight);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);

	resetFlame();
}

inline TTinKoopaPartsBase::TTinKoopaPartsBase(TTinKoopa* koopa, int kind)
    : TLiveActor(getPartsColFile(kind))
    , mIsBreaking(false)
    , mKind(kind)
    , mOwner(koopa)
    , mBreakMActor(nullptr)
{
}

#pragma dont_inline on

void TTinKoopa::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mSpine->initWith(&theNerveWait());

	mMActorKeeper = new TMActorKeeper(mManager, 7);
	mMActor       = mMActorKeeper->createMActor("tinkoopa_body.bmd", 0);

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	for (int i = 0; i < 15; ++i)
		TTinKoopa_jointIndexTable[i]
		    = jointNames->getIndex(TTinKoopa_jointNameTable[i]);

	mFlame = new TTinKoopaFlame(this);

	for (int i = 0; i < 6; ++i) {
		mParts[i] = new TTinKoopaPartsBase(this, i);
		mParts[i]->initTinKoopaPartsBase();
	}

	f32 damageHeight = getSaveParam2()->mSLDamageHeight0.get();
	f32 damageRadius = getSaveParam2()->mSLDamageRadius.get();
	initHitActor(0x8000018, 1, 0, 0.0f, 0.0f, damageRadius, damageHeight);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	mMActor->setLightType(1);
	J3DModel* model = mMActor->getModel();
	if (!model->getSkinDeform())
		model->setSkinDeform(new J3DSkinDeform, (J3DDeformAttachFlag)1);

	calcRootMatrix();
	mMActor->calc();

	mGraph            = gpConductor->getGraphByName("killer");
	mCoasterDistances = new f32[mGraph->getNodeNum() - 1];

	JGeometry::TVec3<f32> prev = mGraph->indexToPoint(0);
	f32 totalDistance          = 0.0f;
	for (int i = 0; i < mGraph->getNodeNum() - 1; ++i) {
		JGeometry::TVec3<f32> start = prev;
		prev                        = mGraph->indexToPoint(i + 1);
		mCoasterDistances[i]        = start.distance(prev);
		totalDistance += mCoasterDistances[i];
	}

	mLaunchOrders = new TTinKoopaLaunchOrderAry(this, 11);
	makeLaunchSchedule();
	initAnmSound();
	resetTinKoopa();
}

void TTinKoopa::makeLaunchSchedule()
{
	mLaunchOrders->mOrders[0]->set(0, 600, -1, 0);
	mLaunchOrders->mOrders[1]->set(0, 1230, -1, 1);
	mLaunchOrders->mOrders[2]->set(0, 2450, -1, 1);
	mLaunchOrders->mOrders[3]->set(1, 240, -1, 0);
	mLaunchOrders->mOrders[4]->set(1, 280, -1, 1);
	mLaunchOrders->mOrders[5]->set(1, 630, -1, 0);
	mLaunchOrders->mOrders[6]->set(1, 900, -1, 0);
	mLaunchOrders->mOrders[7]->set(1, 1200, -1, 1);
	mLaunchOrders->mOrders[8]->set(2, 565, -1, 0);
	mLaunchOrders->mOrders[9]->set(2, 700, -1, 0);
	mLaunchOrders->mOrders[10]->set(2, 2220, -1, 1);
}

BOOL TTinKoopaFlame::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL] <= 0) {
			if (mHitPoints > 0)
				mHitPoints -= 1;

			if (mHitPoints <= 0) {
				mHitPoints = mOwner->getSaveParam2()->mSLFlameHP.get();
				mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL]
				    = (s16)mOwner->getSaveParam2()->mSLFlameRevivalTime.get();
				onHitFlag(HIT_FLAG_NO_COLLISION);
			}

			if (mHasEmittedSpray == 0) {
				mHasEmittedSpray = 1;
				gpMarioParticleManager->emitAndBindToPosPtr(0xF3, &mPosition,
				                                            0, this);
			}
		}
		return TRUE;
	}
	return FALSE;
}

void TTinKoopaFlame::perform(u32 cue, JDrama::TGraphics* graphics)
{
	THitActor::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		if (mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL] <= 0 && mOwner->mState != 4) {
			int frame   = mOwner->mState == 0 ? 2750 : 3400;
			bool passed = checkRailPass(mOwner->mKoopaRail, frame);
			if (passed)
				SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
		}
	}

	if (cue & CUE_CALC_ANIM) {
		MtxPtr mtx = mOwner->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[9]);
		mPosition.x = mtx[0][3];
		mPosition.y = mtx[1][3];
		mPosition.z = mtx[2][3];
		emitFlameEffects();

		if (mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL] <= 0)
			offHitFlag(HIT_FLAG_NO_COLLISION);
		mHasEmittedSpray = 0;
	}
}

void TTinKoopaFlame::emitFlameEffects()
{
	if (mOwner->mSpine->getCurrentNerve()
	    == &theNerveWait()) {
		MtxPtr mtx = mOwner->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[8]);

		f32 sizeRate = 1.0f;
		if (mOwner->mState != 0)
			sizeRate = 1.6f;

		if (mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL] > 0) {
			mScale -= 0.05f;
			if (mScale < 0.3f)
				mScale = 0.3f;
		} else {
			mScale += 0.05f;
			if (mScale > 1.0f)
				mScale = 1.0f;
		}

		f32 scaleXZ = mScale * sizeRate;
		f32 scaleY  = scaleXZ;
		if (mOwner->mTimers[TTinKoopa::TIMER_FLAME_REVIVAL] > 0)
			scaleY *= 0.5f;

		JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(0x1BB, mtx, 1, this);
		if (emitter)
			emitter->setGlobalScale(
			    JGeometry::TVec3<f32>(scaleXZ, scaleY, scaleXZ));

		emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(0x1BC, mtx, 1, this);
		if (emitter)
			emitter->setGlobalScale(
			    JGeometry::TVec3<f32>(scaleXZ, scaleY, scaleXZ));

		emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(0x1F2, mtx, 3, this);
		if (emitter)
			emitter->setGlobalScale(
			    JGeometry::TVec3<f32>(scaleXZ, scaleY, scaleXZ));

		SMSGetMSound()->startSoundActorWithInfo(MSD_SE_BS_MKP_FIRE, &mPosition,
		                                        nullptr, scaleXZ, 0, 0, nullptr,
		                                        0, 4);
	}
}

void TTinKoopaLaunchOrder::checkOrder()
{
	if (mOwner->mWaitCycle == mWaitCycle) {
		bool passed = checkRailPass(mOwner->mKoopaRail, mFrame);
		if (passed) {
			int count = 1;
			if (mCount == -1) {
				if (mOwner->mState == 0)
					count = 1;
				else if (mOwner->mState == 1)
					count = 1;
				else if (mOwner->mState == 2)
					count = 2;
				else if (mOwner->mState == 3)
					count = 3;
			} else {
				count = mCount;
			}

			if ((int)mKind == 1)
				count = count <= 2 ? count : 2;

			mOwner->setLaunchSchedule(count, mKind);
		}
	}
}

void TTinKoopaPartsBase::initTinKoopaPartsBase()
{
	initHitActor(getPartsActorType(mKind), 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);

	mCollision = new TMapCollisionMove;
	mCollision->init(getPartsColFile(mKind), 0, this);
	mCollision->setUpTrans(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));

	const char* bmdName = getPartsBmd(mKind);
	if (bmdName != nullptr) {
		mBreakMActor = mOwner->getActorKeeper()->createMActor(bmdName, 0);
		mBreakMActor->setLightType(1);
	}

	mIsBreaking = false;
	mCollision->setUpTrans(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
}

void TTinKoopaPartsBase::reset()
{
	mIsBreaking = false;
	mCollision->setUpTrans(JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f));
}

void TTinKoopaPartsBase::startBreaking()
{
	mIsBreaking = true;

	MtxPtr jointMtx
	    = mOwner->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[mKind]);
	mPosition.x = jointMtx[0][3];
	mPosition.y = jointMtx[1][3];
	mPosition.z = jointMtx[2][3];

	if (mBreakMActor != nullptr) {
		mBreakMActor->setBckFromIndex(getPartsBreakBck(mKind));

		MtxPtr baseMtx = mBreakMActor->getModel()->getBaseTRMtx();
		baseMtx[0][3]  = mPosition.x;
		baseMtx[1][3]  = mPosition.y;
		baseMtx[2][3]  = mPosition.z;
		MTXCopy(baseMtx, mBreakMActor->getModel()->getBaseTRMtx());
	}

	if (mBreakMActor != nullptr) {
		if (mKind == 1)
			emitPartsTrackEffects(breastTrackJointNameTable, 6);
		else if (mKind == 2)
			emitPartsTrackEffects(bellyTrackJointNameTable, 6);
		else if (mKind == 3)
			emitPartsTrackEffects(rightArmTrackJointNameTable, 4);
		else if (mKind == 4)
			emitPartsTrackEffects(leftArmTrackJointNameTable, 4);
	}
}

void TTinKoopaPartsBase::emitPartsTrackEffects(const char** names, int count)
{
	mBreakMActor->getModel()->calc();

	JUTNameTab* jointNames = mBreakMActor->getModel()->getModelData()->getJointName();
	for (int i = 0; i < count; ++i) {
		int jointIndex = jointNames->getIndex(names[i]);
		if (jointIndex < 0)
			return;

		MtxPtr jointMtx = mBreakMActor->getModel()->getAnmMtx(jointIndex);
		mTrackPos[i].set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);
		gpMarioParticleManager->emitAndBindToPosPtr(0xF4, &mTrackPos[i], 0,
		                                            mOwner);
	}
}

void TTinKoopaPartsBase::emitPartsDisappearEffects()
{
	if (mBreakMActor != nullptr
	    && mBreakMActor->checkCurBckFromIndex(getPartsBreakBck(mKind))) {
		J3DFrameCtrl* ctrl = mBreakMActor->getFrameCtrl(0);
		int endFrame       = 60;
		if (ctrl->checkPass(endFrame)) {
			if (mKind == 1)
				emitPartsDisappearEffects(breastTrackJointNameTable, 6, 4.0f);
			else if (mKind == 2)
				emitPartsDisappearEffects(bellyTrackJointNameTable, 6, 4.0f);
			else if (mKind == 3)
				emitPartsDisappearEffects(rightArmTrackJointNameTable, 4, 3.0f);
			else if (mKind == 4)
				emitPartsDisappearEffects(leftArmTrackJointNameTable, 4, 3.0f);
			mOwner->mBreakingParts = nullptr;
		}
	}
}

void TTinKoopaPartsBase::emitPartsDisappearEffects(const char** names,
                                                   int count, f32 scale)
{
	JGeometry::TVec3<f32> effectScale = mScaling;
	char trash[0x4];
	effectScale.x *= scale;
	effectScale.y *= scale;
	effectScale.z *= scale;

	JUTNameTab* jointNames = mBreakMActor->getModel()->getModelData()->getJointName();
	for (int i = 0; i < count; ++i) {
		int jointIndex = jointNames->getIndex(names[i]);
		if (jointIndex < 0)
			return;

		MtxPtr jointMtx = mBreakMActor->getModel()->getAnmMtx(jointIndex);
		mTrackPos[i].set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);

		TSpineEnemy* effect = gpConductor->makeOneEnemyAppear(
		    mTrackPos[i], "エフェクト爆発マネージャー", 1);
		if (effect == nullptr)
			break;
		((TEffectExplosion*)effect)->generate(mTrackPos[i], effectScale);
	}
}

BOOL TTinKoopaPartsBase::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x1000002B) {
		mOwner->hitParts();
		return TRUE;
	}
	return FALSE;
}

void TTinKoopaPartsBase::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TLiveActor::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		MtxPtr jointMtx
		    = mOwner->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[mKind]);
		mCollision->moveMtx(jointMtx);
	}

	if (cue & CUE_CALC_ANIM) {
		if (mIsBreaking && mBreakMActor != nullptr
		    && mBreakMActor->curAnmEndsNext(0, nullptr))
			mIsBreaking = false;
	}

	if (mIsBreaking && mBreakMActor != nullptr)
		mBreakMActor->perform(cue, graphics);
}

void TTinKoopaMtxCalc::calc(u16 jointIndex)
{
	M3UMtxCalcSIAnmBlendQuat::calc(jointIndex);
}

TTinKoopa::TTinKoopa(const char* name)
    : TSpineEnemy(name)
    , mKillerManager(nullptr)
    , unk1F8(0)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
	mScaledBodyRadius = 2000.0f;
}

f32 TTinKoopa::calcCoasterDistance(int from, int to)
{
	f32 total = 0.0f;
	for (int i = from; i < to; ++i)
		total += mCoasterDistances[i];
	return total;
}

void TTinKoopa::resetTinKoopa()
{
	if (mKillerManager == nullptr)
		mKillerManager = (TCoasterKillerManager*)JDrama::TNameRefGen::search(
		    "コースターキラーマネージャー");
	mKillerManager->getActiveObjNum();

	mKoopaRail = gpMarioOriginal->mKoopaRail;

	mState              = 0;
	unk154              = 0;
	unk158              = 0;
	mWaitCycle          = 0;
	mHasShownFirstFlame = 0;

	mPartsHP       = getSaveParam2()->mSLPartsHP.get();
	mBreakingParts = nullptr;

	mLaunchNum          = 0;
	mLaunchIndex        = 0;
	mLaunchSchedule[0]  = 0;
	mLaunchSchedule[1]  = 0;
	mLaunchSchedule[2]  = 0;
	mLaunchSchedule[3]  = 0;
	mTimers[TIMER_LAUNCH]        = 0;
	mTimers[TIMER_FLAME_REVIVAL]  = 0;
	mTimers[TIMER_DEFEAT_WAIT]    = 0;
	mTimers[TIMER_LAUNCH]        = 0;

	unk1B4[0] = 0.0f;
	unk1B4[1] = 30.0f;
	unk1B4[2] = 15.0f;
	unk1B4[3] = 45.0f;
}

void TTinKoopa::reset()
{
	TSpineEnemy::reset();

	mFlame->resetFlame();

	for (int i = 0; i < 6; ++i)
		mParts[i]->reset();

	setHitParamsByState();
	resetTinKoopa();

	int bck = getWaitBck(mState);
	mMActor->setBckFromIndex(bck);
	const char** table = getBasNameTable();
	setAnmSound(table == nullptr ? nullptr : table[bck]);
}

void TTinKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_CALC_ANIM) {
		emitTinKoopaEffects();

		if (mSpine->getCurrentNerve() == &theNerveWait()) {
			if (mLaunchIndex < mLaunchNum && mTimers[TIMER_LAUNCH] <= 0) {
				launchKiller(mLaunchSchedule[mLaunchIndex]);
				mLaunchIndex += 1;
				mTimers[TIMER_LAUNCH] = getSaveParam2()->mSLKillerInterval.get();
			}
		}
	}

	if (cue & CUE_MOVE) {
		for (int i = 0; i < 3; ++i) {
			if (mTimers[i] > 0)
				mTimers[i] -= 1;
		}

		if (mKoopaRail != nullptr) {
			J3DFrameCtrl* railCtrl = mKoopaRail->getFrameCtrl(0);
			if (railCtrl->checkPass(railCtrl->getEnd() - 1)) {
				mWaitCycle += 1;
				if (mWaitCycle > 2)
					mWaitCycle = 0;
			}
		}

		TTinKoopaLaunchOrderAry* orders = mLaunchOrders;
		for (int i = 0; i < orders->mNum; ++i)
			orders->mOrders[i]->checkOrder();

		checkTinKoopaKillerApproachingMessage();

		if (mKoopaRail != nullptr) {
			J3DFrameCtrl* ctrl = mKoopaRail->getFrameCtrl(0);
			if (mWaitCycle == 0 && ctrl->checkPass(300.0f))
				gpMarDirector->getConsole()->startAppearBalloon(0xE000A, true);
		}

		checkTinKoopaFirstFlameMessage();
	}

	TSpineEnemy::perform(cue, graphics);
	mFlame->perform(cue, graphics);
	for (int i = 0; i < 6; ++i)
		mParts[i]->perform(cue, graphics);
}

const char** TTinKoopa::getBasNameTable() const { return tinkoopa_bastable; }

BOOL TTinKoopa::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x1000002B) {
		hitParts();
		return TRUE;
	}
	return FALSE;
}

void TTinKoopa::hitParts()
{
	if (mSpine->getCurrentNerve() == &theNerveBreak())
		return;
	if (mSpine->getCurrentNerve() == &theNerveDamage())
		return;
	if (mState == 4)
		return;

	gpMarDirector->getConsole()->startAppearBalloon(0xE0024, true);

	mPartsHP -= 1;
	if (mPartsHP <= 0) {
		const TNerveBase<TLiveActor>* nerve = &theNerveBreak();
		mSpine->pushNerve(nerve);
	} else {
		const TNerveBase<TLiveActor>* nerve = &theNerveDamage();
		mSpine->pushNerve(nerve);
	}
}

void TTinKoopa::launchKiller(int direction)
{
	TCoasterKiller* killer = (TCoasterKiller*)mKillerManager->getDeadEnemy();
	if (killer != nullptr) {
		killer->reset();

		int jointIndex = TTinKoopa_jointIndexTable[direction == 1
		                                               ? mLaunchIndex + 11
		                                               : 14 - mLaunchIndex];
		getJointTransByIndex(jointIndex, &killer->mPosition);
		killer->mPathDir = direction;

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEF, getModel()->getAnmMtx(jointIndex), 0, this);
		SMSGetMSound()->startSoundActor(MSD_SE_EN_KILLER_FIRE,
		                                &killer->mPosition, 0, nullptr, 0, 4);
	}
}

void TTinKoopa::checkTinKoopaKillerApproachingMessage()
{
	u32 messageId = 0xE0009;
	for (int i = 0; i < mKillerManager->getActiveObjNum(); ++i) {
		TCoasterKiller* killer = (TCoasterKiller*)mKillerManager->getObj(i);
		if (!killer->checkLiveFlag(LIVE_FLAG_DEAD)) {
			f32 approachDistance
			    = getSaveParam2()->mSLKillerApproachingDistance.get();
			JGeometry::TVec3<f32> marioPos = *gpMarioPos;

			bool isApproaching;
			if (killer->checkLiveFlag(LIVE_FLAG_DEAD)) {
				isApproaching = false;
			} else if (killer->mPathDir != 0) {
				isApproaching = false;
			} else {
				int marioNode = mGraph->findNearestNodeIndex(marioPos, -1);
				f32 distance;
				if (marioNode >= killer->mPathIdx) {
					distance = calcCoasterDistance(killer->mPathIdx, marioNode);
				} else {
					distance
					    = calcCoasterDistance(0, marioNode)
					      + calcCoasterDistance(killer->mPathIdx,
					                            mGraph->getNodeNum() - 1);
				}
				isApproaching = distance <= approachDistance;
			}

			if (isApproaching)
				gpMarDirector->getConsole()->startAppearBalloon(messageId, true);
		}
	}
}

void TTinKoopa::checkTinKoopaFirstFlameMessage()
{
	if (mKoopaRail != nullptr && !mHasShownFirstFlame) {
		if (mSpine->getCurrentNerve() == &theNerveWait()) {
			J3DFrameCtrl* ctrl = mKoopaRail->getFrameCtrl(0);
			if (mState == 0) {
				if (ctrl->checkPass(2600.0f)) {
					gpMarDirector->getConsole()->startAppearBalloon(0xE000B,
					                                                true);
					mHasShownFirstFlame = 1;
				}
			} else if (ctrl->checkPass(3100.0f)) {
				gpMarDirector->getConsole()->startAppearBalloon(0xE000B, true);
				mHasShownFirstFlame = 1;
			}
		}
	}
}

void TTinKoopa::emitTinKoopaEffects()
{
	MtxPtr mtx = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]);
	mHeadPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);

	mtx = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[1]);
	mBreastPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);

	mtx = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[3]);
	mRightArmPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);

	mtx = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[4]);
	mLeftArmPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);

	gpMarioParticleManager->emitAndBindToPosPtr(0x1AC, &mBreastPos, 1, this);

	if (mState > 1)
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1AD, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[3]), 1,
		    this);
	if (mState > 2)
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1AE, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[4]), 1,
		    this);

	if (mState <= 0
	    && mSpine->getCurrentNerve() == &theNerveWait())
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1AF, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[2]), 1,
		    this);

	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x1B0, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[3]), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x1B0, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[4]), 1,
	    this + 1);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x1B1, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[10]), 1, this);
	gpMarioParticleManager->emitAndBindToMtxPtr(
	    0x1B2, getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]), 1, this);

	if (mSpine->getCurrentNerve() == &theNerveDamage()
	    || mSpine->getCurrentNerve() == &theNerveBreak()) {
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B3, &mBreastPos, 1,
		                                            this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B3, &mBreastPos, 1,
		                                            this + 1);
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B4, &mBreastPos, 1,
		                                            this);
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B4, &mBreastPos, 1,
		                                            this + 1);
	}

	if ((mSpine->getCurrentNerve() == &theNerveWait()
	     && mState > 1)
	    || mSpine->getCurrentNerve() == &theNerveDamage()
	    || mSpine->getCurrentNerve() == &theNerveBreak())
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B5, &mRightArmPos, 1,
		                                            this);

	if ((mSpine->getCurrentNerve() == &theNerveWait()
	     && mState > 2)
	    || mSpine->getCurrentNerve() == &theNerveDamage()
	    || mSpine->getCurrentNerve() == &theNerveBreak())
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B5, &mLeftArmPos, 1,
		                                            this + 1);

	if ((mSpine->getCurrentNerve() == &theNerveWait()
	     && mState > 0)
	    || (mSpine->getCurrentNerve() == &theNerveDamage()
	        && mState > 0)
	    || (mSpine->getCurrentNerve() == &theNerveBreak()
	        && (mState == 1 || mState == 2))) {
		MtxPtr breastMtx
		    = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[1]);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x1B6, breastMtx, 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x1B7, breastMtx, 1, this);
	}

	if ((mSpine->getCurrentNerve() == &theNerveWait()
	     && mState > 2)
	    || mSpine->getCurrentNerve() == &theNerveDamage()
	    || mSpine->getCurrentNerve() == &theNerveBreak())
		gpMarioParticleManager->emitAndBindToPosPtr(0x1B8, &mHeadPos, 1, this);

	if (mSpine->getCurrentNerve() == &theNerveDamage()
	    || mSpine->getCurrentNerve() == &theNerveBreak()) {
		MtxPtr femurMtx = getModel()->getAnmMtx(TTinKoopa_jointIndexTable[10]);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x1BA, femurMtx, 1, this);
		gpMarioParticleManager->emitAndBindToMtxPtr(0x1B9, femurMtx, 1, this);
	}

	if (mSpine->getCurrentNerve() == &theNerveBreak()) {
		if (mState == 0 || mState == 3) {
			if (mMActor->getFrameCtrl(0)->checkPass(100.0f))
				gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
		}

		if (mState == 1 || mState == 2) {
			if (mMActor->getFrameCtrl(0)->checkPass(104.0f))
				gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
		}

		if (mState == 0 || mState == 3) {
			if (mMActor->getFrameCtrl(0)->checkPass(108.0f))
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF0,
				    getModel()->getAnmMtx(TTinKoopa_jointIndexTable[2]), 0,
				    this);
		}

		if (mState == 0) {
			if (mMActor->getFrameCtrl(0)->checkPass(100.0f))
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF1,
				    getModel()->getAnmMtx(TTinKoopa_jointIndexTable[2]), 0,
				    this);
		}

		if (mState == 3) {
			if (mMActor->getFrameCtrl(0)->checkPass(100.0f))
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF1,
				    getModel()->getAnmMtx(TTinKoopa_jointIndexTable[1]), 0,
				    this);
		}

		if (mState == 1) {
			if (mMActor->getFrameCtrl(0)->checkPass(106.0f))
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF2,
				    getModel()->getAnmMtx(TTinKoopa_jointIndexTable[3]), 0,
				    this);
		}

		if (mState == 2) {
			if (mMActor->getFrameCtrl(0)->checkPass(106.0f))
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF2,
				    getModel()->getAnmMtx(TTinKoopa_jointIndexTable[4]), 0,
				    this);
		}
	}

	if (mBreakingParts != nullptr)
		mBreakingParts->emitPartsDisappearEffects();
}

TTinKoopaManager::TTinKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

void TTinKoopaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "tinkoopa_body.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TTinKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TTinKoopaParams("/enemy/tinkoopa.prm");
}

void TTinKoopaManager::loadAfter()
{
	static const char* onetimeFilenames[7] = {
		"/scene/tinkoopa/jpa/ms_mkp_hibana_d1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_killer.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_smoke1.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_parge_b14.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_parge_b23.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_flame_yuge.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_parts.jpa",
	};
	for (int i = 0; i < 7; ++i)
		SMS_LoadParticle(onetimeFilenames[i], 0xEE + i);

	static const char* loopFilenames[17] = {
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w1br.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w3ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_hibana_w4ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1st.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1fe.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_w1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_d1br_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_biri_d1br_b.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1ar.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_w2br_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_w2br_b.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1he.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1fe_l.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_kemu_b1fe_r.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_fire_a.jpa",
		"/scene/tinkoopa/jpa/ms_mkp_fire_b.jpa",
	};
	for (int i = 0; i < 17; ++i)
		SMS_LoadParticle(loopFilenames[i], 0x1AC + i);

	static const char* loopIndirectFilenames[1] = {
		"/scene/tinkoopa/jpa/ms_mkp_fire_c.jpa",
	};
	for (int i = 0; i < 1; ++i)
		SMS_LoadParticle(loopIndirectFilenames[i], 0x1F2 + i);
}

TSpineEnemy* TTinKoopaManager::createEnemyInstance() { return nullptr; }

BOOL TNerveTinKoopaWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		int bck = getWaitBck(self->mState);
		self->mMActor->setBckFromIndex(bck);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[bck]);
		self->mTimers[TTinKoopa::TIMER_DEFEAT_WAIT]
		    = self->getSaveParam2()->mSLDefeatWaitTime.get();
	}

	if (self->mState == 4 && self->mTimers[TTinKoopa::TIMER_DEFEAT_WAIT] <= 0)
		TFlagManager::smInstance->setBool(true, 0x5000A);

	return FALSE;
}

BOOL TNerveTinKoopaDamage::execute(TSpineBase<TLiveActor>* spine) const
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		int bck = getDamageBck(self->mState);
		self->mMActor->setBckFromIndex(bck);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[bck]);

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEE, self->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]), 0,
		    this);
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);
	}

	if (self->mMActor->checkCurBckFromIndex(getDamageBck(self->mState))) {
		if (self->mMActor->curAnmEndsNext(0, nullptr)) {
			int bck = getWaitBck(self->mState);
			self->mMActor->setBckFromIndex(bck);
			const char** table = self->getBasNameTable();
			self->setAnmSound(table == nullptr ? nullptr : table[bck]);
			return TRUE;
		}
	}
	return FALSE;
}

BOOL TNerveTinKoopaBreak::execute(TSpineBase<TLiveActor>* spine) const
{
	TTinKoopa* self = (TTinKoopa*)spine->getBody();
	if (spine->getTime() == 0) {
		int bck = getBreakBck(self->mState);
		self->mMActor->setBckFromIndex(bck);
		const char** table = self->getBasNameTable();
		self->setAnmSound(table == nullptr ? nullptr : table[bck]);

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xEE, self->getModel()->getAnmMtx(TTinKoopa_jointIndexTable[0]), 0,
		    this);
		gpCameraShake->startShake(CAM_SHAKE_MODE_UNK6, 1.0f);

		if (self->mState == 3) {
			self->mKillerManager->unk60 = 1;
			MSBgm::stopTrackBGMs(7, 10);
		}
	}

	if (self->mMActor->checkCurBckFromIndex(getBreakBck(self->mState))) {
		TTinKoopaPartsBase* parts = self->mParts[getBreakPartIndex(self->mState)];
		if (self->mMActor->curAnmEndsNext(0, nullptr)) {
			parts->mCollision->remove();
			self->mState += 1;
			self->setHitParamsByState();

			int bck = getWaitBck(self->mState);
			self->mMActor->setBckFromIndex(bck);
			const char** table = self->getBasNameTable();
			self->setAnmSound(table == nullptr ? nullptr : table[bck]);
			self->mPartsHP = self->getSaveParam2()->mSLPartsHP.get();
			return TRUE;
		} else if (!parts->mIsBreaking) {
			if (self->mMActor->getFrameCtrl(0)->checkPass(
			        getBreakFrame(self->mState))) {
				self->mBreakingParts
				    = self->mParts[getBreakPartIndex(self->mState)];
				self->mBreakingParts->startBreaking();
			}
		}
	}
	return FALSE;
}

#pragma dont_inline off
