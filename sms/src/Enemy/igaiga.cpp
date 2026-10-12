#include <Enemy/Igaiga.hpp>
#include <Enemy/AreaCylinder.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/NameKuri.hpp>
#include <Enemy/Walker.hpp>
#include <Camera/cameralib.hpp>
#include <Camera/CameraShake.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JMath.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp>
#include <Map/PollutionManager.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSModBgm.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <System/TargetArrow.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <stdlib.h>
#include <Enemy/GateKeeper.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapMirror.hpp>
#include <MoveBG/MapObjBianco.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <Map/MapEventSink.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Strategic/MirrorActor.hpp>
#include <Strategic/SharedParts.hpp>

// file-level data
f32 TRollEnemy::mBoundVal = 80.0f;
f32 TIgaiga::mReachNodeDist = 300.0f;
f32 TRollEnemy::mTransYOffset;

static TRollEnemy* gpCurRollEnemy;

static const char* igaiga_bastable[] = {
	"/scene/igaiga/bas/igaiga_down1.bas",
	"/scene/igaiga/bas/igaiga_down2.bas",
	nullptr,
	nullptr,
	"/scene/igaiga/bas/igaiga_shoot1.bas",
	"/scene/igaiga/bas/igaiga_waterdown1.bas",
	"/scene/igaiga/bas/igaiga_waterhit1.bas",
	nullptr,
};

static const char* gorogoro_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
};

static BOOL RollEnemyBodyCallback(J3DNode*, int);

static inline BOOL containsOrNone(TAreaCylinderManager* area,
                                  const JGeometry::TVec3<f32>& pos)
{
	return area == nullptr ? FALSE : area->contain(pos);
}

inline BOOL TGorogoroManager::inArea()
{
	return unk70 ? (containsOrNone(unk70, *gpMarioPos) ? 1 : 0) : 1;
}

inline TGorogoro::TGorogoro(const char* name)
    : TRollEnemy(name)
    , unk1E4(0)
    , unk1E8(0)
    , unk1EC(0)
{
	gpCurRollEnemy = nullptr;
}

// fabricated: shared tail of the WaterHit and RollOnGraph nerves
static inline void rollAlongGraph(TIgaiga* self)
{
	if (self->isReachedToGoalXZ()) {
		if (self->jumpToNextGraphNode() >= 0)
			self->flagJump();

		if (self->unk124->getCurrent().checkFlag(0x40))
			return;

		self->goToRandomNextGraphNode();
	}

	self->walkBehavior(2, 1.0f);
}

// fabricated: the original clamps are inlined (MsClamp is not)
template <class T> static inline T igaClamp(T t, T l, T r)
{
	if (t > r)
		t = r;
	else if (t < l)
		t = l;
	return t;
}

static BOOL RollEnemyBodyCallback(J3DNode* node, BOOL param_2);

TRollEnemy::TRollEnemy(const char* name)
    : TWalkerEnemy(name)
    , unk194(0.0f)
    , unk198(0.0f)
    , unk19C(0.0f)
    , unk1A0(0.0f)
    , unk1A4(nullptr)
    , unk1A8(false)
    , unk1AC(0.0f)
    , unk1B0(1.0f)
{
}

void TRollEnemy::reset()
{
	JGeometry::TVec3<f32> point;
	JGeometry::TVec3<f32> dir;
	// stack padding to match the retail frame size
	char trash[0x30];
	gpCurRollEnemy = this;
	TWalkerEnemy::reset();

	unk194 = TMsRange<f32>(0.0f, 360.0f).rand();
	unk158 = 1.0f;

	unk124->getGraph()->getGraphNode(0).getPoint(&point);
	mPosition = point;
	mPosition.y += 10.0f;

	unk124->getGraph()->getGraphNode(1).getPoint(&point);
	dir.x = point.x - mPosition.x;
	dir.y = 0.0f;
	dir.z = point.z - mPosition.z;
	mRotation.y = MsWrap(MsGetRotFromZaxisY(dir), 0.0f, 360.0f);

	unk198 = 1.5f * mMarchSpeed;
	unk19C = mMarchSpeed;
	unk1A0 = 0.0f;
	unk124->mCurrIdx = 0;
}

void TRollEnemy::walkBehavior(int param_1, f32 param_2)
{
	if (!unk1A8)
		TWalkerEnemy::walkBehavior(param_1, param_2);

	if (isAirborne() && mPosition.y > 20.0f + mGroundHeight) {
		f32 t = (mPosition.y - mGroundHeight) / mBoundVal;
		f32 w = MsWrap(t, 0.0f, unk1A4->getSLBoundVYMax());
		if (unk1A0 < w)
			unk1A0 = w;
	} else if (!mGroundPlane->isWaterSurface()) {
		unk1A8 = false;
		if (unk1A0 > unk1B0) {
			bound();
			JGeometry::TVec3<f32> vel;
			// stack padding to match the retail frame size
			char trash[8];
			vel.x = 0.0f;
			vel.y = unk1A0;
			vel.z = 0.0f;
			mVelocity = vel;
			onLiveFlag(LIVE_FLAG_AIRBORNE);
			mPosition.y += 5.0f;
			unk1A0 = 0.0f;
			boundSE();
		}
	}

	if (mPosition.y < 30.0f + mGroundHeight)
		rollSE();

	if (unk128 > 300) {
		onLiveFlag(LIVE_FLAG_MELT_ON_DEATH);
		kill();
	}
}

void TRollEnemy::behaveToWater(THitActor*)
{
	mSprayedByWaterCooldown = 0;
	if (unk158 < unk1A4->getSLExpandMax()) {
		f32 rate = unk1A4->getSLExpandRate();
		mBodyScale *= rate;
		unk158 *= rate;
		mScaledBodyRadius *= rate;
		mScaling.x = mScaling.y = mScaling.z = mScaling.z * rate;

		f32 attackRadius = getSaveParams()->getSLAttackRadius();
		f32 attackHeight = getSaveParams()->getSLAttackHeight();
		f32 damageRadius = getSaveParams()->getSLDamageRadius();
		f32 damageHeight = getSaveParams()->getSLDamageHeight();

		attackRadius *= mBodyScale / unk154;
		attackHeight *= mBodyScale / unk154;
		damageRadius *= mBodyScale / unk154;
		damageHeight *= mBodyScale / unk154;

		setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
	}
}

void TRollEnemy::attackToMario()
{
	SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
}

void TRollEnemy::flagJump()
{
	JGeometry::TVec3<f32> point;
	// stack padding to match the retail frame size
	char trash[0x20];
	unk124->getCurrent().getPoint(&point);
	mPosition.y += 30.0f;
	f32 speed = unk124->unkC;
	JGeometry::TVec3<f32> vel
	    = calcVelocityToJumpToY(point, speed, getGravityY());
	unk1A8 = true;
	mVelocity = vel;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

bool TRollEnemy::isCollidMove(THitActor* param_1)
{
	if (param_1->isActorType(0x4000022B)) {
		kill();
		return true;
	}

	param_1->receiveMessage(this, HIT_MESSAGE_ATTACK);
	return false;
}

bool TRollEnemy::isReachedToGoalXZ()
{
	JGeometry::TVec3<f32> tmp;
	tmp = getUnk104().getPoint();
	tmp -= mPosition;
	if (!unk1A8)
		tmp.y = 0.0f;
	if (MsVECMag2(&tmp) < 200.0f)
		return true;
	else
		return false;
}

void TRollEnemy::setBehavior()
{
	if (mPosition.y > 50.0f + mGroundHeight)
		return;

	if (mSpine->getTime() % getSaveParams()->getSLPolluteInterval() == 0
	    && !checkLiveFlag(LIVE_FLAG_DEAD)
	    && mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()
	    && mIsPolluter) {
		f32 size = 2.0f;
		if (!checkLiveFlag(LIVE_FLAG_HIDDEN)) {
			if (!mIsAmpPolluter) {
				size = getSaveParams()->getSLPolluteRange();
			} else {
				s32 rMin  = getSaveParams()->getSLPolluteRMin();
				s32 rMax  = getSaveParams()->getSLPolluteRMax();
				s32 cycle = getSaveParams()->getSLPolluteCycle();
				s32 time  = mSpine->getTime() % cycle;
				f32 angle = 180.0f * (f32)time / (f32)cycle;
				s16 idx   = (65536.0f / 360.0f) * angle;
				f32 s     = jmaSinTable[static_cast<u16>(idx) >> jmaSinShift];
				size      = mBodyScale * ((f32)(rMax - rMin) * s) + (f32)rMin;
			}
		}
		gpPollution->stampGround(1, mPosition.x + unk1AC * mLinearVelocity.x,
		                         mPosition.y,
		                         mPosition.z + unk1AC * mLinearVelocity.z,
		                         32.0f * size);
	}
}

void TIgaigaPolluteModelManager::init(TLiveActor* param_1)
{
	TEnemyPolluteModelManager::init(param_1);

	void* res = JKRFileLoader::getGlbResource(
	    "/scene/igaiga/stamp_igaiga_model1.bmd");
	SDLModelData* modelData = new SDLModelData(J3DModelLoaderDataBase::load(
	    res, J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
	             | (1 << J3DMLF_TevStageNumShift)));

	for (int i = 0; i < unk14; ++i)
		unk18[i] = new TIgaigaPolluteModel(param_1, 0, modelData,
		                                   "イガイガ汚染モデル");
}

void TIgaigaPolluteModel::setAnm()
{
	unk10->getMActor()->setBckFromIndex(7);
	unk10->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
}

TIgaigaManager::TIgaigaManager(const char* name)
    : TSmallEnemyManager(name)
    , unk64(0)
    , unk68(nullptr)
{
	gpCurRollEnemy = nullptr;
}

void TIgaigaManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TRollEnemySaveLoadParams("/enemy/igaiga.prm");
	unk68 = new TWaterEmitInfo("/enemy/igaigawater.prm");
}

void TIgaigaManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "igaiga_model1.bmd", 0x11240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TIgaigaManager::createEnemyInstance()
{
	return new TIgaiga("イガイガ");
}

void TIgaigaManager::initSetEnemies()
{
	char trash[0x78];
	unk60 = new TIgaigaPolluteModelManager("イガイガモデル汚染");
	unk60->init((TLiveActor*)unk18[0]);
}

void TIgaigaManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);
	unk60->perform(cue, graphics);
}

static BOOL RollEnemyBodyCallback(J3DNode* node, BOOL param_2)
{
	Mtx rot;
	char trash[8];
	MtxPtr rotp = rot;
	if (param_2 == 0) {
		if (gpCurRollEnemy == nullptr || !gpCurRollEnemy->isRolling())
			return true;

		u16 jointIndex = ((J3DJoint*)node)->getJntNo();
		MtxPtr jointMtx
		    = gpCurRollEnemy->getModel()->getAnmMtx(jointIndex);

		MsMtxSetRotX(rotp, gpCurRollEnemy->unk194);

		jointMtx[1][3] += TRollEnemy::mTransYOffset;
		MTXConcat(jointMtx, rotp, jointMtx);
		MTXConcat(J3DSys::mCurrentMtx, rotp, J3DSys::mCurrentMtx);
	}
	return true;
}

#pragma dont_inline on
TIgaiga::TIgaiga(const char* name)
    : TRollEnemy(name)
    , unk1B4(0)
    , unk1B8(0)
    , unk1BC(true)
    , unk1CC(1.0f)
    , unk1D0(0)
    , unk1E4(1.0f)
    , unk1E8(0)
{
}
#pragma dont_inline off

void TIgaiga::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000017;
	unk150     = 0x11;
	offHitFlag(HIT_FLAG_UNK8000000 | HIT_FLAG_UNK10000000);
	onHitFlag(HIT_FLAG_UNK40000000);
	mSpine->initWith(&TNerveIgaigaRollOnGraph::theNerve());
	unk1A4 = (TRollEnemySaveLoadParams*)getSaveParam();
	mMActor->setJointCallback(1, RollEnemyBodyCallback);
	mMActor->setBtkFromIndex(0);
	unk124->setGraph(gpConductor->getGraphByName("igaiga"));
}

void TIgaiga::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("igaiga_model1.bmd", 0);
}

void TIgaiga::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
}

void TIgaiga::calcRootMatrix()
{
	gpCurRollEnemy = this;
	TSpineEnemy::calcRootMatrix();
}

bool TIgaiga::isRolling()
{
	if (mSpine->getCurrentNerve() == &TNerveIgaigaRollOnGraph::theNerve()
	    || mSpine->getCurrentNerve()
	           == &TNerveIgaigaShootFromCannon::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveIgaigaWaterHit::theNerve())
		return true;
	return false;
}

void TIgaiga::behaveToWater(THitActor*)
{
	// stack padding to match the retail frame size
	char trash[8];
	mSprayedByWaterCooldown = 0;
	if (unk1E4 < unk1A4->mSLExpandMax.get())
		unk1E4 = unk1E4 * unk1A4->mSLExpandRate.get();

	unk165 = true;
	if (mSpine->getCurrentNerve() != &TNerveIgaigaWaterHit::theNerve())
		mSpine->pushNerve(&TNerveIgaigaWaterHit::theNerve());
}

void TIgaiga::reset()
{
	TRollEnemy::reset();
	initialGraphNode();
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1B4 = 0;

	TMsRange<s32> interval(50, 100);
	unk1B8 = interval.rand() * 0x78;
	unk1BC = true;

	mPosition.y += 20.0f;
	gpMap->checkGround(mPosition.x, mPosition.y + mHeadHeight, mPosition.z,
	                   &mGroundPlane);
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	unk1E4 = 1.0f;
	unk1CC = 1.0f;
	unk1AC = -30.0f;
	unk1B0 = 2.0f;
	unk1E8 = 0;
}

void TIgaiga::kill()
{
	unk194 = 0.0f;
	TSmallEnemy::kill();
}

void TIgaiga::moveObject()
{
	// stack padding to match the retail frame size
	char trash[0x18];
	TWalkerEnemy::moveObject();

	unk1CC = igaClamp(unk1CC - 0.0002f, 0.5f, 1.0f);

	f32 attackRadius = getSaveParams()->getSLAttackRadius();
	f32 attackHeight = getSaveParams()->getSLAttackHeight();
	f32 damageRadius = getSaveParams()->getSLDamageRadius();
	f32 damageHeight = getSaveParams()->getSLDamageHeight();

	f32 baseScale = unk154 * unk1CC;
	mBodyScale    = igaClamp(unk1E4 * baseScale, baseScale, 3.0f * mBodyScale);

	f32 ratio       = mBodyScale / unk154;
	f32 radiusScale = igaClamp(unk1CC * unk1E4, 1.0f, 1.2f);

	mScaledBodyRadius = 8.0f * (mBodyScale * mBodyRadius) * radiusScale;
	mScaling.x = mScaling.y = mScaling.z = mBodyScale;
	setHitParams(attackRadius * ratio, attackHeight * ratio,
	             damageRadius * ratio, damageHeight * ratio);

	mMarchSpeed = unk1A4->mSLMarchSpeedLow.get();
	mTurnSpeed  = unk1A4->getSLTurnSpeedLow();

	if (unk124->getCurrent().checkFlag(0x40)) {
		JGeometry::TVec3<f32> pos;
		unk124->getCurrent().getPoint(&pos);
		if (mPosition.y < 50.0f + pos.y) {
			kill();
			unk1BC = true;
		}
	}
}

void TIgaiga::rollSE()
{
	SMSGetMSound()->startSoundActorSpecial(MSD_SE_EN_IGAIGA_ROLL, &mPosition,
	                                       mScaling.x, mMarchSpeed, 0, nullptr,
	                                       0, 4);
}

void TIgaiga::boundSE()
{
	char trash[8];
	SMSGetMSound()->startSoundActorWithInfo(MSD_SE_EN_IGAIGA_BOUND, &mPosition,
	                                        nullptr,
	                                        abs(mGroundPlane->getNormal().y), 0,
	                                        0, nullptr, 0, 4);
}

void TIgaiga::walkBehavior(int param_1, f32 param_2)
{
	TRollEnemy::walkBehavior(param_1, param_2);

	f32 x = mLinearVelocity.x;
	f32 z = mLinearVelocity.z;
	if (unk1A8) {
		// TODO: fake, the original keeps two struct copies of the velocity
		JGeometry::TVec3<f32> velocity = mVelocity;
		JGeometry::TVec3<f32> velocityB = velocity;
		JGeometry::TVec3<f32> velocityC = velocity;
		x = velocityC.x;
		z = velocityB.z;
	}

	f32 speed = JGeometry::TUtil<f32>::sqrt(x * x + z * z);
	unk194 += 4.0f * (speed / (unk1E4 * (mBodyRadius * unk1CC)));

	if (unk1B4 != 0) {
		unk1B4++;
		if (unk1B4 > unk1B8)
			unk1B4 = 0;
	}

	if (!isAirborne() && mGroundPlane != nullptr
	    && mGroundPlane->getActor() != nullptr) {
		const_cast<TLiveActor*>(mGroundPlane->getActor())
		    ->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}

	if (unk138 != nullptr && unk138->getActor() != nullptr) {
		const_cast<TLiveActor*>(unk138->getActor())
		    ->receiveMessage(this, HIT_MESSAGE_ATTACK);
	}
}

bool TIgaiga::isReachedToGoalXZ()
{
	JGeometry::TVec3<f32> tmp = getUnk104().getPoint();
	tmp -= mPosition;
	if (!unk1A8)
		tmp.y = 0.0f;
	tmp.y = 0.0f;

	if (MsVECMag2(&tmp) < mReachNodeDist)
		return true;
	else
		return false;
}

void TIgaiga::setWalkAnm() { setBckAnm(3); }

void TIgaiga::setDeadAnm()
{
	// stack padding to match the retail frame size
	char trash[0x18];
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk1C0 = mPosition;
	} else {
		MtxPtr mtx = mMActor->getModel()->getAnmMtx(0);
		unk1C0.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}

	gpMarioParticleManager->emit(0xCB, &unk1C0, 0, nullptr);

	if (unk1BC)
		setBckAnm(0);
	else
		setBckAnm(1);

	TIgaigaManager* manager = (TIgaigaManager*)mManager;
	JGeometry::TVec3<f32> scale = mScaling;
	f32 factor                  = unk1CC * unk1E4;
	scale.x *= factor;
	scale.y *= factor;
	scale.z *= factor;
	mPosition.y = mGroundHeight;

	scale.x = igaClamp(scale.x, 0.8f, 1.5f);
	scale.y = scale.z = scale.x;
	manager->requestPolluteModel(mPosition, scale);
}

void TIgaiga::setMeltAnm()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk1C0 = mPosition;
	} else {
		MtxPtr mtx = mMActor->getModel()->getAnmMtx(0);
		unk1C0.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}

	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT) && SMS_IsMarioTouchGround4cm()) {
		if (mScaling.x > mBodyScale)
			SMSRumbleMgr->start(0x15, 10, &mPosition);
		else
			SMSRumbleMgr->start(0x14, 10, &mPosition);
	}

	TIgaigaManager* manager = (TIgaigaManager*)mManager;
	manager->unk68->mPos.value = mPosition;
	gpModelWaterManager->emitRequest(*manager->unk68);

	JGeometry::TVec3<f32> scale = mScaling * 0.5f;

	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emit(0xA1, &unk1C0, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(scale);

	emitter = gpMarioParticleManager->emit(0xA2, &unk1C0, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(scale);

	setBckAnm(5);

	TMsRange<f32> range(0.0f, 1.0f);
	if (range.rand() < 0.2f) {
		gpItemManager->makeObjAppear(mPosition.x, 20.0f + mPosition.y,
		                             mPosition.z, 0x20000002, true);
	}
}

const char** TIgaiga::getBasNameTable() const { return igaiga_bastable; }

bool TIgaiga::isHitValid(u32 param_1)
{
	unk1BC = true;
	if (param_1 == 1)
		unk1BC = false;
	return true;
}

void TIgaiga::bound()
{
	if (unk1A0 > 5.0f) {
		setBckAnm(2);

		if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT) && SMS_IsMarioTouchGround4cm()) {
			if (mScaling.x > mBodyScale)
				SMSRumbleMgr->start(0x15, 10, &mPosition);
			else
				SMSRumbleMgr->start(0x14, 10, &mPosition);
		}
	}
}

void TIgaiga::shoot(JGeometry::TVec3<f32>& velocity)
{
	mSpine->setNext(&TNerveIgaigaShootFromCannon::theNerve());
	unk1D8 = velocity;
	offLiveFlag(LIVE_FLAG_UNK10);
	unk1A8 = true;
}

DEFINE_NERVE(TNerveIgaigaRollOnGraph, TLiveActor)
{
	char trash[0x18];
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0)
		self->setWalkAnm();

	if (self->checkCurAnmEnd(0) && self->isBckAnm(2))
		self->setBckAnm(3);

	rollAlongGraph(self);
	return false;
}

DEFINE_NERVE(TNerveIgaigaWaterHit, TLiveActor)
{
	char trash[0x18];
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(6);

	if (self->unk1E4 >= self->unk1A4->mSLExpandMax.get()) {
		if (self->unk1E8 > 20) {
			self->onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
			return true;
		}
		self->unk1E8++;
	} else if (self->checkCurAnmEnd(0)) {
		if (!self->unsetUnk165()) {
			self->setBckAnm(3);
			spine->pushAfterCurrent(&TNerveIgaigaRollOnGraph::theNerve());
			return true;
		}
	}

	if (self->checkCurAnmEnd(0) && self->isBckAnm(2))
		self->setBckAnm(3);

	rollAlongGraph(self);
	return false;
}

DEFINE_NERVE(TNerveIgaigaShootFromCannon, TLiveActor)
{
	TIgaiga* self = (TIgaiga*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->mPosition.y += 10.0f;
		self->mVelocity = self->unk1D8;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
	} else if (self->checkCurAnmEnd(0) && !self->isAirborne()) {
		self->bound();
		spine->pushAfterCurrent(&TNerveIgaigaRollOnGraph::theNerve());
		return true;
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

void TGorogoroPolluteModelManager::init(TLiveActor* actor)
{
	TEnemyPolluteModelManager::init(actor);

	void* res = JKRFileLoader::getGlbResource(
	    "/scene/gorogoro/bosspaku_head_stamp.bmd");
	SDLModelData* model
	    = new SDLModelData(J3DModelLoaderDataBase::load(res, 0x10210000));

	for (int i = 0; i < unk14; ++i)
		unk18[i] = new TGorogoroPolluteModel(actor, 0, model, "汚染モデル");
}

void TGorogoroPolluteModel::setAnm()
{
	unk10->getMActor()->setBckFromIndex(3);
	unk10->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
}

TGorogoroManager::TGorogoroManager(const char* name)
    : TSmallEnemyManager(name)
    , unk60(0)
    , unk64(nullptr)
    , unk68(true)
    , unk6C(nullptr)
    , unk70(nullptr)
{
}

void TGorogoroManager::load(JSUMemoryInputStream& stream)
{
	static const char* anmlist[] = { "bosspaku_head_move", nullptr };

	TSmallEnemyManager::load(stream);
	unk38 = new TRollEnemySaveLoadParams("/enemy/gorogoro.prm");
	createSharedMActorSet(anmlist);
}

void TGorogoroManager::loadAfter()
{
	unk64 = (TMapEventSink*)JDrama::TNameRefGen::search(
	    "イベント（地形沈むビアンコ）");
	unk70 = (TAreaCylinderManager*)gpConductor->search("ゴロゴロ発生マネージャー");
}

TSpineEnemy* TGorogoroManager::createEnemyInstance()
{
	return new TGorogoro("ゴロゴロ");
}

void TGorogoroManager::initSetEnemies()
{
	static const char* graphlist[] = { "gorogoro0", "gorogoro1" };

	unk6C = new TGorogoroPolluteModelManager("ゴロゴロモデル汚染");
	unk6C->init(getObj(0));

	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph = gpConductor->getGraphByName(graphlist[i % 2]);
		if (graph->isDummy())
			graph = gpConductor->getGraphByName(graphlist[0]);

		if (!graph->isDummy()) {
			TGorogoro* gorogoro = (TGorogoro*)getObj(i);
			JGeometry::TVec3<f32> point;
			graph->getGraphNode(0).getPoint(&point);
			gorogoro->unk124->setGraph(graph);
			gorogoro->mPosition = point;
			gorogoro->setGenerateGraphIdx(graph->getNodeNum() - 1);
		}
	}
}

void TGorogoroManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "bosspaku_head.bmd", 0x10300000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TGorogoroManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_MOVE) {
		unk60++;
		int interval = ((TRollEnemySaveLoadParams*)unk38)->getSLGenerateInterval();
		if (unk60 > interval) {
			unk60 = 0;
			if (inArea()) {
				for (int i = 0; i < getActiveObjNum(); ++i) {
					if (!getObj(i)->checkLiveFlag(LIVE_FLAG_DEAD))
						continue;

					TSpineEnemy* enemy = getObj(i);
					if (unk64) {
						if (!unk64->isBuried(1)) {
							if (unk68) {
								unk68 = false;
								enemy->reset();
								JGeometry::TVec3<f32> start;
								enemy->unk124->getGraph()
								    ->getGraphNode(10)
								    .getPoint(&start);
								enemy->mPosition        = start;
								enemy->unk124->mCurrIdx = 10;
								enemy->unk124->mPrevIdx = 9;
								JGeometry::TVec3<f32> next;
								enemy->unk124->getGraph()
								    ->getGraphNode(11)
								    .getPoint(&next);
								JGeometry::TVec3<f32> dir(
								    next.x - enemy->mPosition.x, 0.0f,
								    next.z - enemy->mPosition.z);
								enemy->mRotation.y
								    = MsAngleWrap(MsGetRotFromZaxisY(dir));
								enemy->setGoalPath(TPathNode(next));

								TSpineEnemy* second = getObj(1);
								second->reset();
								JGeometry::TVec3<f32> start2;
								second->unk124->getGraph()
								    ->getGraphNode(16)
								    .getPoint(&start2);
								second->mPosition        = start2;
								second->unk124->mCurrIdx = 16;
								second->unk124->mPrevIdx = 15;
								JGeometry::TVec3<f32> next2;
								second->unk124->getGraph()
								    ->getGraphNode(17)
								    .getPoint(&next2);
								JGeometry::TVec3<f32> dir2(
								    next2.x - second->mPosition.x, 0.0f,
								    next2.z - second->mPosition.z);
								second->mRotation.y
								    = MsAngleWrap(MsGetRotFromZaxisY(dir2));
								second->setGoalPath(TPathNode(next2));
							} else {
								enemy->reset();
							}
						} else {
							unk60 = interval;
						}
					} else {
						enemy->reset();
					}
					break;
				}
			}
		}
	}

	TEnemyManager::perform(cue, graphics);
	unk6C->perform(cue, graphics);
}

void TGorogoro::init(TLiveManager* manager)
{
	// stack padding to match the retail frame size
	char trash[0x10];
	TWalkerEnemy::init(manager);
	mActorType = 0x10000019;
	unk150     = 0x31;
	offHitFlag(HIT_FLAG_UNK40000000 | HIT_FLAG_UNK10000000
	           | HIT_FLAG_UNK8000000);
	mSpine->initWith(&TNerveGorogoroRollOnGraph::theNerve());
	unk1A4 = (TRollEnemySaveLoadParams*)getSaveParam();

	TMirrorActor* mirror = new TMirrorActor("ゴロゴロin鏡");
	mirror->init(mMActor->getModel(), 0x18);
	unk1ED.a = 0xFF;

	ResTIMG* image = (ResTIMG*)JKRFileLoader::getGlbResource(
	    "/scene/map/pollution/H_ma_rak.bti");
	if (image) {
		SMS_ChangeTextureAll(mMActor->getModel()->getModelData(), "M_dummy",
		                     *image);
		SMS_ChangeTextureAll(mirror->unk14->getModelData(), "M_dummy", *image);
	}

	for (u16 i = 0; i < mMActor->getModel()->getModelData()->getMaterialNum();
	     ++i) {
		SMS_InitPacket_OneTevKColor(mMActor->getModel(), i, GX_KCOLOR0,
		                            &unk1ED);
		SMS_InitPacket_OneTevKColor(mirror->unk14, i, GX_KCOLOR0, &unk1ED);
	}

	mMActor->setJointCallback(1, RollEnemyBodyCallback);
	unk130 = 1;
}

void TGorogoro::perform(u32 cue, JDrama::TGraphics* graphics)
{
	// stack padding to match the retail frame size
	char trash[0x10];
	TSmallEnemy::perform(cue, graphics);

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if (gpMirrorModelManager->isInMirror(mPosition)) {
			if (cue & CUE_CALC_ANIM) {
				calcRootMatrix();
				mMActor->calc();
			}
			if (cue & CUE_CALC_VIEW)
				mMActor->viewCalc();
		}
	}
}

void TGorogoro::calcRootMatrix()
{
	gpCurRollEnemy = this;
	if (mSpine->getCurrentNerve() == &TNerveGorogoroDie::theNerve()) {
		TSpineEnemy::calcRootMatrix();
		if (checkLiveFlag(LIVE_FLAG_MELT_ON_DEATH)) {
			unk1B4.ref(0, 3) = mPosition.x;
			unk1B4.ref(2, 3) = mPosition.z;
		}
	} else if (!isEaten()) {
		f32 groundOffsetY = unk1A4->getSLGroundOffsetY();
		if (mPosition.y < 30.0f + mGroundHeight) {
			JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToMtxPtr(
			        0x175, mMActor->getModel()->getAnmMtx(0), 1, this);
			if (emitter)
				emitter->setGlobalScale(mScaling);
		}
		MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
		               groundOffsetY * unk158 + mPosition.y, mPosition.z,
		               mRotation.x, mRotation.y, mRotation.z);
		getModel()->setBaseScale(mScaling);
	}
}

void TGorogoro::reset()
{
	unk130 = 1;
	TRollEnemy::reset();
	offLiveFlag(LIVE_FLAG_UNK1000);
	unk1ED.a = 0xFF;
	unk1AC   = -10.0f;
	unk1B0   = 1.0f;
}

void TGorogoro::kill()
{
	unk194 = 0.0f;

	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveGorogoroDie::theNerve());
		mSpine->pushAfterCurrent(mSpine->getDefault());

		onLiveFlag(LIVE_FLAG_UNK8);
	}
}

void TGorogoro::forceKill()
{
	if (!mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL)
	    && (mGroundPlane->isPool() || mGroundPlane->isWaterSurface())
	    && !isAirborne()) {
		if (mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
			mSpine->reset();
			mSpine->setNext(&TNerveGorogoroDie::theNerve());
			mSpine->pushAfterCurrent(mSpine->getDefault());

			onLiveFlag(LIVE_FLAG_UNK20000);
			onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		}
	}
}

void TGorogoro::behaveToWater(THitActor*)
{
	mSprayedByWaterCooldown = 0;

	if (unk158 < unk1A4->getSLExpandMax()) {
		f32 rate = unk1A4->getSLExpandRate();
		mBodyScale *= rate;
		unk158 *= rate;
		mScaledBodyRadius *= rate;
		mScaling.x = mScaling.y = mScaling.z *= rate;

		f32 attackRadius = getSaveParams()->getSLAttackRadius();
		f32 attackHeight = getSaveParams()->getSLAttackHeight();
		f32 damageRadius = getSaveParams()->getSLDamageRadius();
		f32 damageHeight = getSaveParams()->getSLDamageHeight();

		f32 ratio = mBodyScale / unk154;
		attackRadius *= ratio;
		attackHeight *= ratio;
		damageRadius *= ratio;
		damageHeight *= ratio;

		setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
	}

	u8 maxHp = getMaxHitPoints();
	unk1ED.a = mHitPoints * 255 / maxHp;
	if (mHitPoints < 2)
		mHitPoints = 1;

	if (JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	        0x176, mMActor->getModel()->getAnmMtx(0), 1, this))
		emitter->setGlobalScale(mScaling);
}

void TGorogoro::rollSE()
{
	// stack padding to match the retail frame size
	char trash[8];
	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_BS_KRPAKU_ROLL, &mPosition, nullptr,
	    abs(mGroundPlane->getNormal().y), 0, 0, nullptr, 0, 4);
}

void TGorogoro::boundSE()
{
	// stack padding to match the retail frame size
	char trash[8];
	SMSGetMSound()->startSoundActorWithInfo(
	    MSD_SE_BS_KRPAKU_GND, &mPosition, nullptr,
	    abs(mGroundPlane->getNormal().y), 0, 0, nullptr, 0, 4);
}

void TGorogoro::walkBehavior(int param_1, f32 param_2)
{
	if (mPosition.y > mGroundHeight)
		onLiveFlag(LIVE_FLAG_AIRBORNE);

	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if (getGroundPlane() && getGroundPlane()->mActor
		    && getGroundPlane()->mActor->getActorType() == 0x4000009A) {
			TBiancoWatermill* mill
			    = (TBiancoWatermill*)getGroundPlane()->mActor;
			mill->turnByEnemy(this, getGroundPlane());
			TRollEnemy::walkBehavior(param_1, 0.2f * param_2);
			return;
		}

		const TBGCheckData* roof;
		gpMap->checkRoof(mPosition.x, mPosition.y + mHeadHeight, mPosition.z,
		                 &roof);
		if (roof && roof->mActor
		    && roof->mActor->getActorType() == 0x4000009A) {
			TBiancoWatermill* mill = (TBiancoWatermill*)roof->mActor;
			mill->turnByEnemy(this, roof);
			TRollEnemy::walkBehavior(param_1, 0.3f * param_2);
			return;
		}

		if (unk138 && unk138->mActor) {
			TBGWallCheckRecord record(mPosition.x, mPosition.y + mHeadHeight,
			                          mPosition.z, getWallRadius(), 4,
			                          0);
			if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
				for (int i = 0; i < record.mResultWallsNum; ++i) {
					if (record.mResultWalls[i]->mActor
					    && record.mResultWalls[i]->mActor->getActorType()
					           == 0x4000009A) {
						TBiancoWatermill* mill
						    = (TBiancoWatermill*)record.mResultWalls[i]->mActor;
						mill->turnByEnemy(this, record.mResultWalls[i]);
					}
				}
				TRollEnemy::walkBehavior(param_1, 0.2f * param_2);
				return;
			}
		}
	}

	mTurnSpeed = unk1A4->getSLTurnSpeedLow();
	TRollEnemy::walkBehavior(param_1, param_2);
	unk194 += 0.4f * mMarchSpeed;

	if (mSpine->getCurrentNerve() != &TNerveGorogoroDie::theNerve()) {
		if (mPosition.y < 10.0f + mGroundHeight
		    && getGroundPlane()->isWaterSurface()) {
			onLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			onLiveFlag(LIVE_FLAG_UNK20000);
			kill();
		}
	}
}

void TGorogoro::flagJump()
{
	JGeometry::TVec3<f32> goal;
	// stack padding to match the retail frame size
	char trash[0x20];
	unk124->getCurrent().getPoint(&goal);
	mPosition.y += 30.0f;
	f32 speed = unk124->unkC;
	JGeometry::TVec3<f32> velocity
	    = calcVelocityToJumpToY(goal, speed, getGravityY());
	unk1A8 = true;
	mVelocity = velocity;
	onLiveFlag(LIVE_FLAG_AIRBORNE);
}

void TGorogoro::setDeadAnm()
{
	char trash[8];
	JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
	    0xBF, mMActor->getModel()->getAnmMtx(1), 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(mScaling);

	setBckAnm(0);
	SMSGetMSound()->startSoundActor(MSD_SE_BS_KRPAKU_DOWN, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TGorogoro::setMeltAnm()
{
	char trash[8];
	setBckAnm(1);
	unk130 = 0;
	onLiveFlag(LIVE_FLAG_UNK1000);
	PSMTXCopy(mMActor->getModel()->getBaseTRMtx(), unk1B4);
	unk1B4.ref(1, 3) = mGroundHeight;
	JPABaseEmitter* emitter
	    = gpMarioParticleManager->emitAndBindToMtxPtr(0xBE, unk1B4, 0, nullptr);
	if (emitter)
		emitter->setGlobalScale(mScaling);

	SMSGetMSound()->startSoundActor(MSD_SE_BS_KRPAKU_SINK, &mPosition, 0,
	                                nullptr, 0, 4);
}

void TGorogoro::bound()
{
	((TGorogoroManager*)mManager)->requestPolluteModel(mPosition, mScaling);

	if (!checkLiveFlag(LIVE_FLAG_CLIPPED_OUT) && SMS_IsMarioTouchGround4cm())
		SMSRumbleMgr->start(0x15, 10, &mPosition);
}

const char** TGorogoro::getBasNameTable() const { return gorogoro_bastable; }

bool TGorogoro::isRolling()
{
	if (mSpine->getCurrentNerve() == &TNerveGorogoroRollOnGraph::theNerve()
	    || isBckAnm(1))
		return true;
	return false;
}

void TGorogoro::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("bosspaku_head.bmd", 3);
}

void TGorogoro::generateByGateKeeper(const JGeometry::TVec3<f32>& pos,
                                     const JGeometry::TVec3<f32>& rot)
{
	reset();

	TGraphWeb* graph = unk124->getGraph();
	int idx          = graph->findNearestNodeIndex(pos, 0xffffffff);
	unk124->mCurrIdx = idx;
	unk124->mPrevIdx = idx - 1;
	TGraphNode& node = graph->getGraphNode(idx);

	JGeometry::TVec3<f32> goal;
	BOOL seen;
	if (MsIsInSight(pos, rot.y, *gpMarioPos, 2000.0f, 360.0f, -1.0f)) {
		goal = *gpMarioPos;
		seen = true;
	} else {
		node.getPoint(&goal);
		seen = false;
	}

	goal.sub(pos);
	mPosition = pos;

	if (!goal.isZero()) {
		VECNormalize(&goal, &goal);

		Mtx rotMtx;
		MsMtxSetRotY(rotMtx, 30.0f * MsRandF() - 15.0f);
		MTXMultVec(rotMtx, &goal, &goal);

		mRotation.y = MsWrap(MsGetRotFromZaxisY(goal), 0.0f, 360.0f);

		goal.scale(MsRandF() * 1500.0f);
		goal += pos;

		mVelocity = calcVelocityToJumpToY(goal, 15.0f, getGravityY());
	} else {
		mVelocity = calcVelocityToJumpToY(pos, 15.0f, getGravityY());

		mRotation.y = MsWrap(MsGetRotFromZaxisY(goal), 0.0f, 360.0f);
	}

	if (seen) {
		TPathNode path(*gpMarioPos);
		unk114.push(unkF4);
		unkF4 = path;
	}

	unk1A8 = true;
}

DEFINE_NERVE(TNerveGorogoroRollOnGraph, TLiveActor)
{
	TGorogoro* self = (TGorogoro*)spine->getBody();

	if (spine->getTime() == 0) {
		self->goToShortestNextGraphNode();
		self->setBckAnm(2);
	}

	if (self->isReachedToGoalXZ()) {
		if (self->jumpToNextGraphNode() >= 0)
			self->flagJump();
		else
			self->goToShortestNextGraphNode();
	}

	self->walkBehavior(2, 1.0f);
	return false;
}

DEFINE_NERVE(TNerveGorogoroDie, TLiveActor)
{
	TGorogoro* self = (TGorogoro*)spine->getBody();

	if (spine->getTime() < 2) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		if (self->getGroundPlane()->isWaterSurface() && !self->isAirborne())
			self->generateEffectColumWater();

		if (self->checkLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH)) {
			self->setMeltAnm();
		} else {
			((TGorogoroManager*)self->getManager())
			    ->requestPolluteModel(self->mPosition, self->mScaling);
			self->setDeadAnm();
			self->setDeadEffect();
		}
	} else if (self->checkCurAnmEnd(0) || spine->getTime() > 360) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();

		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(spine->getDefault());

		self->genRandomItem();
		return true;
	}

	if (self->checkLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH))
		self->walkBehavior(2, 0.5f);

	return false;
}

