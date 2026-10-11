#include <Enemy/ChuuHana.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Conductor.hpp>
#include <Player/MarioAccess.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapMirror.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/MirrorActor.hpp>
#include <System/Particles.hpp>
#include <System/EmitterViewObj.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DNode.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

#include <Map/MapCollisionEntry.hpp>

// theNerve() is fully inlined at every use in the original build, so no
// out-of-line symbol exists for it.
#define DECLARE_INLINE_NERVE(Name, T)                                          \
	class Name : public TNerveBase<T> {                                        \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<T>*) const;                            \
		static const Name& theNerve()                                          \
		{                                                                      \
			static Name instance;                                              \
			return instance;                                                   \
		}                                                                      \
	};

DECLARE_INLINE_NERVE(TNerveChuuHanaWalkOnPanel, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaForceJumped, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaKeepBalance, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaStick, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaRoll, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaFall, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaFall2, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaObject, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaAttack, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaJumpPrepare, TLiveActor)
DECLARE_INLINE_NERVE(TNerveChuuHanaWait, TLiveActor)

s32 TChuuHana::mCheckOnPanelTimeRoll = 20;
s32 TChuuHana::mCheckOnPanelTime     = 400;
u8 TChuuHana::mBodyJntIndex          = 1;
u8 TChuuHana::mEyeJntIndex           = 12;
u8 TChuuHana::mFootJntIndex          = 5;
bool TChuuHana::mNewSw               = true;
bool TChuuHana::mCompareHeight       = true;
f32 TChuuHana::mSmallMirrorR         = 650.0f;
f32 TChuuHana::mMediumMirrorR        = 900.0f;
f32 TChuuHana::mLargeMirrorR         = 1100.0f;
bool TChuuHana::mAttackVersion       = true;
bool TChuuHana::mDamageSw            = true;

static TChuuHana* gpCurChuuHana;

static const char* tyuhana_bastable[] = {
	"/scene/tyuhana/bas/tyuhana_chance_end.bas",
	nullptr,
	"/scene/tyuhana/bas/tyuhana_chance_start.bas",
	"/scene/tyuhana/bas/tyuhana_jump.bas",
	"/scene/tyuhana/bas/tyuhana_push.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/tyuhana/bas/tyuhana_walk.bas",
};

TChuuHanaSaveLoadParams::TChuuHanaSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLGetWaterPow, 1.0f)
    , PARAM_INIT(mSLGetGroundPow, 1.0f)
    , PARAM_INIT(mSLKeepBalanceTime, 200)
    , PARAM_INIT(mSLCheckFrame, 5)
    , PARAM_INIT(mSLReverseHeightS, 15.0f)
    , PARAM_INIT(mSLStretchHeightS, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightS, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightS, 3.0f)
    , PARAM_INIT(mSLReverseHeightM, 15.0f)
    , PARAM_INIT(mSLStretchHeightM, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightM, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightM, 3.0f)
    , PARAM_INIT(mSLReverseHeightL, 15.0f)
    , PARAM_INIT(mSLStretchHeightL, 10.0f)
    , PARAM_INIT(mSLMediumStretchHeightL, 7.0f)
    , PARAM_INIT(mSLSmallStretchHeightL, 3.0f)
    , PARAM_INIT(mSLWalkGravity, 4.0f)
    , PARAM_INIT(mSLWaterHitGravity, 0.2f)
    , PARAM_INIT(mSLJumpGravity, 0.2f)
    , PARAM_INIT(mSLJumpSp, 12.0f)
    , PARAM_INIT(mSLJumpHeight, 300.0f)
    , PARAM_INIT(mSLGetWaterPow2, 1.0f)
    , PARAM_INIT(mSLTacklePow, 100.0f)
    , PARAM_INIT(mSLDashRate, 2.0f)
    , PARAM_INIT(mSLAttackTimer, 300)
    , PARAM_INIT(mSLHitWaterTimer, 60)
{
	TParams::load(mPrmPath);
}

TChuuHanaManager::TChuuHanaManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurChuuHana = nullptr;
	unk60         = 0;
	unk61         = 0;
	unk62         = 0;
}

void TChuuHanaManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TChuuHanaSaveLoadParams("/enemy/chuuhana.prm");
}

void TChuuHanaManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TEnemyManager::perform(cue, graphics);
}

TSmallEnemy* TChuuHanaManager::createEnemyInstance() { return new TChuuHana; }

void TChuuHanaManager::initSetEnemies()
{
	static const char* graphlist[] = {
		"kohana0", "kohana1", "kohana1", "kohana2", "kohana2", "kohana2",
	};

	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph = gpConductor->getGraphByName(graphlist[i]);
		TChuuHana* enemy = (TChuuHana*)unk18[i];

		if (i == 0)
			enemy->unk21C = &unk60;
		else if (i < 3)
			enemy->unk21C = &unk61;
		else
			enemy->unk21C = &unk62;

		JGeometry::TVec3<f32> pos;
		graph->getGraphNode(TMsRange<s32>(0, graph->getNodeNum()).rand())
		    .getPoint(&pos);
		enemy->mPosition = pos;
		enemy->mPosition.y += 50.0f;
		enemy->onLiveFlag(LIVE_FLAG_AIRBORNE);
		enemy->getTracer()->init(graph);
		enemy->reset();
	}
}

static BOOL ChuuHanaBodyCallback(J3DNode* node, BOOL calcTiming)
{
	if (calcTiming == 0) {
		TChuuHana* self = gpCurChuuHana;
		if (self == nullptr
		    || !(self->mSpine->getCurrentNerve()
		         == &TNerveChuuHanaRoll::theNerve()))
			return TRUE;

		u16 jointNo = ((J3DJoint*)node)->getJntNo();
		MtxPtr mtx  = gpCurChuuHana->getModel()->getAnmMtx(jointNo);

		Mtx identity;
		identity[0][3] = 0.0f;
		identity[1][3] = 0.0f;
		identity[2][3] = 0.0f;
		identity[0][0] = 1.0f;
		identity[0][1] = 0.0f;
		identity[0][2] = 0.0f;
		identity[1][0] = 0.0f;
		identity[1][1] = 1.0f;
		identity[1][2] = 0.0f;
		identity[2][0] = 0.0f;
		identity[2][1] = 0.0f;
		identity[2][2] = 1.0f;

		JGeometry::TVec3<f32> rollDir(gpCurChuuHana->unk204.x, 0.0f,
		                              gpCurChuuHana->unk204.z);
		if (rollDir.x == 0.0f && rollDir.z == 0.0f)
			rollDir.x = 0.001f;

		JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
		JGeometry::TVec3<f32> cross;
		VECCrossProduct(&up, &rollDir, &cross);

		// express the rotation axis in the joint's local frame
		JGeometry::TVec3<f32> axisX(mtx[0][0], mtx[1][0], mtx[2][0]);
		JGeometry::TVec3<f32> axisY(mtx[0][1], mtx[1][1], mtx[2][1]);
		JGeometry::TVec3<f32> axisZ(mtx[0][2], mtx[1][2], mtx[2][2]);

		f32 projZ = 0.0f;
		if (axisZ.squared() != 0.0f)
			projZ = cross.dot(axisZ) / axisZ.squared();

		f32 projY = 0.0f;
		if (axisY.squared() != 0.0f)
			projY = cross.dot(axisY) / axisY.squared();

		f32 projX = 0.0f;
		if (axisX.squared() != 0.0f)
			projX = cross.dot(axisX) / axisX.squared();

		JGeometry::TVec3<f32> axis(projX, projY, projZ);

		Mtx rot;
		MTXRotAxisRad(rot, &axis, (JGeometry::TUtil<f32>::PI() / 180.0f) * gpCurChuuHana->unk210);
		MTXConcat(mtx, rot, mtx);
		MTXConcat(mtx, identity, mtx);
		MTXConcat(J3DSys::mCurrentMtx, rot, J3DSys::mCurrentMtx);
		MTXConcat(J3DSys::mCurrentMtx, identity, J3DSys::mCurrentMtx);
	}
	return TRUE;
}

void TChuuHanaAseParCallback::execute(JPABaseEmitter* emitter,
                                      JPABaseParticle* particle)
{
	TChuuHana* owner = mOwner;
	if (!owner->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		emitter->setGlobalRTMatrix(
		    owner->mMActor->getModel()->getAnmMtx(TChuuHana::mEyeJntIndex));
		emitter->setGlobalScale(JGeometry::TVec3<f32>(2.5f, 2.5f, 2.5f));
	}
}

void TChuuHanaAseParCallback::draw(JPABaseEmitter*, JPABaseParticle*) { }

TChuuHana::TChuuHana(const char* name)
    : TWalkerEnemy(name)
    , unk194(0.0f)
    , unk198(0.0f)
    , unk19C(0.0f)
    , unk1A0(0)
    , unk1A4(0)
    , unk1A8(0.0f)
    , unk1AC(0)
    , unk1B0(1)
    , unk1B1(0)
    , unk1B2(0)
    , unk1B8(0.0f)
    , unk210(0.0f)
    , unk214(0)
    , unk215(0)
    , unk218(nullptr)
    , unk21C(nullptr)
    , unk220(0.0f)
    , unk224(0)
    , unk228(this)
{
}

void TChuuHana::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000016;
	unk150     = 0x11;
	offHitFlag(HIT_FLAG_UNK40000000);
	mSpine->initWith(&TNerveChuuHanaWalkOnPanel::theNerve());
	mMActor->setJointCallback(mBodyJntIndex, ChuuHanaBodyCallback);
	unk130 = 1;
	mMActor->initNormalMotionBlend();
	unk1B4 = (TChuuHanaSaveLoadParams*)getSaveParam();
	mMActor->getModel()->calc();

	TMirrorActor* mirror = new TMirrorActor("チュウハナin鏡");
	mirror->init(getModel(), 0);
}

void TChuuHana::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("default.bmd", 3);
}

void TChuuHana::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		if (gpMirrorModelManager->isInMirror(mPosition)
		    || gpMirrorModelManager->isInMirror(marioPos)) {
			if (cue & 2) {
				calcRootMatrix();
				mMActor->calc();
			}
			if (cue & 4)
				mMActor->viewCalc();
		}
	}
}

void TChuuHana::reset()
{
	gpCurChuuHana = this;
	TWalkerEnemy::reset();
	unk215      = 0;
	unk1A4      = 30;
	mHeadHeight = 200.0f;
	unk1F8      = mPosition;
	unk19C      = 0.0f;
	unk198      = 0.0f;
	unk1A0      = 0;
	unk224      = 0;
	unk1A4      = mCheckOnPanelTime;

	JGeometry::TVec3<f32> pt;
	unk124->getGraph()
	    ->getGraphNode(
	        TMsRange<s32>(0, unk124->getGraph()->getNodeNum()).rand())
	    .getPoint(&pt);
	setGoalPath(TPathNode(pt));
	unk1B2 = 1;
}

void TChuuHana::setBckAnm(int index)
{
	unk194 = 1.0f;
	mMActor->setMotionBlendRatioForBck(unk194);
	mMActor->setBckOldMotionBlendAnmPtr(mMActor->getBckAnm());
	TSmallEnemy::setBckAnm(index);
}

void TChuuHana::behaveToWater(THitActor* water)
{
	unk165 = true;
	unk224 = 0;

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaRoll::theNerve()) {
		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 0.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		f32 pow = unk1B4->mSLGetWaterPow.get();
		dir.x *= pow;
		dir.y *= pow;
		dir.z *= pow;

		JGeometry::TVec3<f32> vel = mVelocity;
		VECAdd(vel, dir, vel);
		vel.y     = 0.0f;
		mVelocity = vel;
		onLiveFlag(LIVE_FLAG_AIRBORNE);
		mPosition.y += 10.0f;
	} else if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
	           || mSpine->getCurrentNerve() == &TNerveChuuHanaAttack::theNerve()
	           || mSpine->getCurrentNerve() == &TNerveChuuHanaWait::theNerve()
	           || (mNewSw
	               && mSpine->getCurrentNerve()
	                      == &TNerveChuuHanaStick::theNerve())) {
		unk165 = true;
		if (mAttackVersion)
			*unk21C = 1;

		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 0.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		f32 pow = unk1B4->mSLGetWaterPow2.get();
		dir.x *= pow;
		dir.y *= pow;
		dir.z *= pow;

		if (!isAirborne()) {
			if (mCompareHeight)
				mPosition.y += 2.0f;
			else
				mPosition.y += 1.0f;
		}
		mVelocity = dir;
		onLiveFlag(LIVE_FLAG_AIRBORNE);

		if (mSpine->getCurrentNerve() != &TNerveChuuHanaStick::theNerve())
			mSpine->pushNerve(&TNerveChuuHanaStick::theNerve());
		mSprayedByWaterCooldown = 0;
	} else if (!mNewSw) {
		if (mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()) {
			JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 10.0f,
			                          mPosition.z - gpMarioPos->z);
			MsVECNormalize(&dir, &dir);
			f32 pow = 2.0f * unk1B4->mSLGetWaterPow.get();
			dir.x *= pow;
			dir.y *= pow;
			dir.z *= pow;
			mVelocity = dir;
			onLiveFlag(LIVE_FLAG_AIRBORNE);
			mPosition.y += 20.0f;
		}
	}
}

void TChuuHana::attackToMario()
{
	if (mSpine->getCurrentNerve() != &TNerveChuuHanaObject::theNerve()) {
		if (mDamageSw) {
			SMS_SendMessageToMario(this, 14);
		} else if (mSpine->getCurrentNerve()
		           == &TNerveChuuHanaAttack::theNerve()) {
			if (!SMS_IsMarioTouchGround4cm())
				return;

			SMS_SendMessageToMario(this, 7);

			JGeometry::TVec3<f32> diff = mPosition - *gpMarioPos;
			f32 yaw                    = MsGetRotFromZaxisY(diff);

			Mtx mtx;
			MsMtxSetRotRPH(mtx, 0.0f, yaw, 0.0f);
			JGeometry::TVec3<f32> dir(0.0f, 1.0f, -1.0f);
			MTXMultVec(mtx, (Vec*)&dir, (Vec*)&dir);
			SMS_ThrowMario(dir, unk1B4->mSLTacklePow.get());
			*unk21C = 0;
		} else {
			SMS_SendMessageToMario(this, 14);
		}
	} else {
		unk215 = 1;
	}
}

void TChuuHana::moveObject()
{
	TWalkerEnemy::moveObject();

	if (unk1A0 == 0) {
		unk198 = unk19C = mPosition.y;
		unk1A8          = 0.0f;
	} else {
		++unk1A0;
		if (unk198 > mPosition.y)
			unk198 = mPosition.y;
		if (unk19C < mPosition.y)
			unk19C = mPosition.y;

		if (unk1A0 > unk1B4->mSLCheckFrame.get()) {
			f32 stretch = unk19C - unk198;
			if (unk1A8 < stretch)
				unk1A8 = stretch;

			if (mPosition.y < unk19C) {
				unk1A8 = unk1A8 / (f32)unk1A0;
				checkStretchType();
			} else {
				if (mSpine->getCurrentNerve()
				    == &TNerveChuuHanaKeepBalance::theNerve())
					setBckAnm(7);
				unk1A0 = 1;
				unk198 = unk19C = mPosition.y;
			}
		}
	}

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaRoll::theNerve()
	    && !isAirborne()) {
		const JGeometry::TVec3<f32>& n = mGroundPlane->mNormal;
		f32 pow                        = unk1B4->mSLGetGroundPow.get();
		JGeometry::TVec3<f32> push(pow * n.x, 0.0f, pow * n.z);

		JGeometry::TVec3<f32> vel = mVelocity;
		VECAdd(vel, push, vel);
		vel.y     = 0.0f;
		mVelocity = vel;
		mPosition.y += 5.0f;
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	f32 blend = unk194 - 0.1f;
	if (blend > 1.0f)
		blend = 1.0f;
	else if (blend < 0.0f)
		blend = 0.0f;
	unk194 = blend;
	mMActor->setMotionBlendRatioForBck(unk194);

	unk1EC = mLinearVelocity;

	if (!isAirborne()
	    && (mGroundPlane->mActor == nullptr || mGroundPlane->mActor != unk218))
		kill();
}

bool TChuuHana::isCollidMove(THitActor* other)
{
	if (other->isActorType(0x10000016)) {
		if (((TLiveActor*)other)->mSpine->getCurrentNerve()
		    == &TNerveChuuHanaRoll::theNerve()) {
			if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve())
				mSpine->pushNerve(&TNerveChuuHanaRoll::theNerve());
		} else if (!unk1B2) {
			if (mSpine->getCurrentNerve() != &TNerveChuuHanaAttack::theNerve()) {
				if (((TLiveActor*)other)->mInstanceIndex > mInstanceIndex) {
					int chance = TMsRange<s32>(0, 100).rand();
					if (chance % 4 == 0) {
						unk1A4 = mCheckOnPanelTime;

						JGeometry::TVec3<f32> pt;
						unk124->getGraph()
						    ->getGraphNode(TMsRange<s32>(
						                       0, unk124->getGraph()->getNodeNum())
						                       .rand())
						    .getPoint(&pt);
						setGoalPath(TPathNode(pt));
						unk1B2 = 1;
					}
				}
			}
		}
	}

	return mSpine->getCurrentNerve() == &TNerveChuuHanaObject::theNerve() ? false
	                                                                       : true;
}

void TChuuHana::calcRootMatrix()
{
	gpCurChuuHana = this;

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaJumpPrepare::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaFall2::theNerve()) {
		J3DModel* model = mMActor->getModel();
		MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x,
		               mPosition.y + unk220, mPosition.z, mRotation.x,
		               mRotation.y, mRotation.z);
		model->setBaseScale(mScaling);
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()) {
		gpMarioParticleManager->emitParticleCallBack(0x130, &mPosition, 1,
		                                             &unk228, this);
	}

	if (isBckAnm(6)) {
		if (mMActor->getFrameCtrl(0)->checkPass(2.0f)) {
			JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x54, mMActor->getModel()->getAnmMtx(mBodyJntIndex), 0, nullptr);
			if (emitter)
				emitter->setGlobalScale(mScaling);
		}
	}
}

void TChuuHana::bind()
{
	if (mSpine->getCurrentNerve() != &TNerveChuuHanaRoll::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveChuuHanaFall2::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveChuuHanaJumpPrepare::theNerve()) {
		TWalkerEnemy::bind();
		return;
	}

	JGeometry::TVec3<f32> nextPos = mPosition;
	nextPos += mLinearVelocity;
	nextPos += mVelocity;

	mVelocity.y -= getGravityY();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	mGroundHeight = gpMap->checkGround(nextPos.x, nextPos.y + mHeadHeight,
	                                   nextPos.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (nextPos.y <= 0.05f + mGroundHeight && mGroundPlane->mActor == nullptr
	    && mPosition.y < unk1F8.y - 200.0f) {
		offLiveFlag(LIVE_FLAG_AIRBORNE);
		nextPos.y = mGroundHeight;
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&nextPos.x, nextPos.y + mHeadHeight,
	                                 &nextPos.z, mBodyRadius);
	mLinearVelocity = nextPos - mPosition;
}

BOOL TChuuHana::receiveMessage(THitActor* sender, u32 message)
{
	if (unk1A0 == 0) {
		if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
		    || mSpine->getCurrentNerve()
		           == &TNerveChuuHanaKeepBalance::theNerve()) {
			if (message == 1)
				unk1A0 = 1;
		}
	}

	if (message == 15) {
		if (mSprayedByWaterCooldown == 0) {
			mSprayedByWaterCooldown = 1;
			behaveToWater(sender);
		}
		unk165 = true;
		gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                             0, nullptr);
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0, 0.0f,
		                        0, 0, 4);
		return TRUE;
	}
	return FALSE;
}

void TChuuHana::setWalkAnm()
{
	bool first = false;
	if (mCurrentBckAnm < 0)
		first = true;
	setBckAnm(12);
	if (first)
		mMActor->getFrameCtrl(0)->setFrame(10.0f * mInstanceIndex);
}

void TChuuHana::kill()
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		onLiveFlag(LIVE_FLAG_HIDDEN);
		if (unk218) {
			((TLiveActor*)unk218)->receiveMessage(this, 8);
			unk218 = nullptr;
		}
		TSmallEnemy::kill();
	}
}

void TChuuHana::forceKill()
{
	if (!(mGroundPlane->isIllegalData()
	      || (!mGroundPlane->isDeathPlane() && !mGroundPlane->isPool()
	          && !mGroundPlane->isWaterSurface()))
	    || (gpMap->isInArea(mPosition.x, mPosition.z)
	        && mGroundPlane->isIllegalData()))
		kill();
}

f32 TChuuHana::getGravityY() const
{
	f32 gravity = TWalkerEnemy::getGravityY();

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaWalkOnPanel::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveChuuHanaForceJumped::theNerve()) {
		gravity = unk1B4->mSLWalkGravity.get();
	} else if (mSpine->getCurrentNerve() == &TNerveChuuHanaStick::theNerve()) {
		gravity = unk1B4->mSLWaterHitGravity.get();
	} else if (mSpine->getCurrentNerve() == &TNerveChuuHanaFall2::theNerve()
	           || mSpine->getCurrentNerve()
	                  == &TNerveChuuHanaJumpPrepare::theNerve()) {
		gravity = unk1B4->mSLJumpGravity.get();
	}
	return gravity;
}

bool TChuuHana::willFall(s32 checkTime)
{
	f32 limit = mSmallMirrorR;
	if (mInstanceIndex > 0)
		limit = mMediumMirrorR;
	if (mInstanceIndex > 2)
		limit = mLargeMirrorR;
	if (checkTime == mCheckOnPanelTimeRoll)
		limit += 250.0f;

	if (unk218 != nullptr) {
		if (mPosition.distance(unk218->mPosition) > limit) {
			unk1A4 = mCheckOnPanelTime;

			JGeometry::TVec3<f32> pt;
			unk124->getGraph()
			    ->getGraphNode(
			        TMsRange<s32>(0, unk124->getGraph()->getNodeNum()).rand())
			    .getPoint(&pt);
			setGoalPath(TPathNode(pt));
			unk1B2 = 1;
			return true;
		}
	}

	unk1B2 = 0;
	return false;
}

#pragma dont_inline on
void TChuuHana::setGoal()
{
	JGeometry::TVec3<f32> goal(mPosition.x, mPosition.y, mPosition.z);

	f32 yaw = mRotation.y + TMsRange<f32>(-30.0f, 30.0f).rand();
	JGeometry::TVec3<f32> dir(0.0f, 0.0f, 1.0f);

	Mtx mtx;
	MsMtxSetRotRPH(mtx, mRotation.x, yaw, mRotation.z);
	MTXMultVec(mtx, (Vec*)&dir, (Vec*)&dir);
	goal.x += 1000.0f * dir.x;
	goal.z += 1000.0f * dir.z;

	setGoalPath(TPathNode(goal));
	unk1A4 = mCheckOnPanelTime;
	unk1B2 = 0;
}
#pragma dont_inline off

void TChuuHana::checkStretchType()
{
	unk1A0 = 0;
	f32 stretch = unk1A8;

	if (mSpine->getCurrentNerve() == &TNerveChuuHanaKeepBalance::theNerve()) {
		f32 limit = unk1B4->mSLReverseHeightS.get();
		if (mInstanceIndex > 0)
			limit = unk1B4->mSLReverseHeightM.get();
		if (mInstanceIndex > 2)
			limit = unk1B4->mSLReverseHeightL.get();

		if (stretch > limit) {
			unk1B1 = 1;
			unk214 = 1;
			mSpine->pushNerve(&TNerveChuuHanaFall2::theNerve());
			mSpine->pushNerve(&TNerveChuuHanaJumpPrepare::theNerve());
		}
	} else {
		f32 limit = unk1B4->mSLStretchHeightS.get();
		if (mInstanceIndex > 0)
			limit = unk1B4->mSLStretchHeightM.get();
		if (mInstanceIndex > 2)
			limit = unk1B4->mSLStretchHeightL.get();

		if (stretch > limit) {
			unk1B1 = 0;
			unk214 = 0;
			setBckAnm(8);
			mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
		} else {
			limit = unk1B4->mSLMediumStretchHeightS.get();
			if (mInstanceIndex > 0)
				limit = unk1B4->mSLMediumStretchHeightM.get();
			if (mInstanceIndex > 2)
				limit = unk1B4->mSLMediumStretchHeightL.get();

			if (stretch > limit) {
				unk1B1 = 0;
				unk214 = 0;
				setBckAnm(9);
				mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
			} else {
				limit = unk1B4->mSLSmallStretchHeightS.get();
				if (mInstanceIndex > 0)
					limit = unk1B4->mSLSmallStretchHeightM.get();
				if (mInstanceIndex > 2)
					limit = unk1B4->mSLSmallStretchHeightL.get();

				if (stretch > limit) {
					unk1B1 = 0;
					unk214 = 0;
					setBckAnm(10);
					mSpine->pushNerve(&TNerveChuuHanaForceJumped::theNerve());
				}
			}
		}
	}
}

const char** TChuuHana::getBasNameTable() const { return tyuhana_bastable; }

BOOL TNerveChuuHanaWalkOnPanel::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setWalkAnm();
		*self->unk21C = 0;
	}

	if (self->unk218 == nullptr) {
		if (self->mGroundPlane->mActor != nullptr) {
			self->unk1F8  = self->mGroundPlane->mActor->mPosition;
			self->unk218 = self->mGroundPlane->mActor;
		}
	} else {
		self->walkBehavior(2, 1.0f);
	}

	++self->unk1A4;
	if (self->unk1A4 > 20) {
		self->unk1A4 = 0;
		if (self->willFall(TChuuHana::mCheckOnPanelTime))
			self->unk1A4 = -100;

		if (!self->isAirborne() && self->mGroundPlane->mActor == nullptr
		    && 200.0f + self->mPosition.y < self->unk1F8.y)
			self->mSpine->pushNerve(&TNerveChuuHanaFall2::theNerve());
	}

	if (self->isReachedToGoalXZ())
		self->setGoal();

	if (*self->unk21C) {
		spine->pushAfterCurrent(&TNerveChuuHanaAttack::theNerve());
		return true;
	}
	return false;
}

BOOL TNerveChuuHanaForceJumped::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0) {
		self->unk1A4 = TChuuHana::mCheckOnPanelTime;

		JGeometry::TVec3<f32> pt;
		self->unk124->getGraph()
		    ->getGraphNode(
		        TMsRange<s32>(0, self->unk124->getGraph()->getNodeNum()).rand())
		    .getPoint(&pt);
		self->setGoalPath(TPathNode(pt));
		self->unk1B2 = 1;
	}

	if (self->unk214) {
		if (self->getCurAnmFrameNo(0) > 80.0f) {
			if (self->mGroundPlane->mActor)
				((TLiveActor*)self->mGroundPlane->mActor)
				    ->receiveMessage(self, 3);
			self->unk214 = 0;
		}
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->unk1B1) {
			spine->pushAfterCurrent(&TNerveChuuHanaRoll::theNerve());
		} else {
			spine->reset();
			spine->setDefaultNext();
			spine->pushAfterCurrent(spine->getDefault());
		}
		return true;
	}
	return false;
}

BOOL TNerveChuuHanaKeepBalance::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(2);
		self->mPosition.x -= 10.0f * self->unk1EC.x;
		self->mPosition.z -= 10.0f * self->unk1EC.z;
	} else if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(2)) {
			self->setBckAnm(1);
		} else if (self->isBckAnm(1)) {
			if (spine->getTime() > self->unk1B4->mSLKeepBalanceTime.get())
				self->setBckAnm(0);
			else
				self->setBckAnm(1);
		} else if (self->isBckAnm(0) || self->isBckAnm(7)) {
			spine->reset();
			spine->setNext(&TNerveChuuHanaKeepBalance::theNerve());
			spine->pushAfterCurrent(spine->getDefault());
			self->unk1A4 = TChuuHana::mCheckOnPanelTime;

			JGeometry::TVec3<f32> pt;
			self->unk124->getGraph()
			    ->getGraphNode(TMsRange<s32>(
			                       0, self->unk124->getGraph()->getNodeNum())
			                       .rand())
			    .getPoint(&pt);
			self->setGoalPath(TPathNode(pt));
			self->unk1B2 = 1;
			return true;
		}
	}

	if (TChuuHana::mAttackVersion)
		*self->unk21C = 1;

	if (!TChuuHana::mNewSw && self->mGroundPlane->mActor == nullptr) {
		spine->pushAfterCurrent(&TNerveChuuHanaFall::theNerve());
		return true;
	}
	return false;
}

BOOL TNerveChuuHanaStick::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0 || !self->isBckAnm(4)) {
		self->setBckAnm(4);
		self->setGoalPath(TPathNode(*gpMarioPos));
		if (TChuuHana::mAttackVersion)
			*self->unk21C = 1;
	} else {
		++self->unk224;
		if (self->unk224 > self->unk1B4->mSLHitWaterTimer.get()) {
			if (self->checkCurAnmEnd(0)) {
				*self->unk21C = 0;
				spine->pushAfterCurrent(&TNerveChuuHanaWait::theNerve());
				return true;
			}
		} else {
			if (self->checkCurAnmEnd(0))
				self->setBckAnm(4);
		}
	}

	if (TChuuHana::mNewSw && self->willFall(TChuuHana::mCheckOnPanelTimeRoll)) {
		spine->pushAfterCurrent(&TNerveChuuHanaKeepBalance::theNerve());
		return true;
	}

	self->walkBehavior(3, 0.2f);
	return false;
}

BOOL TNerveChuuHanaRoll::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0) {
		self->unk210 = 0.0f;
		self->unk204.set(0.0f, 0.0f, 0.0f);
	}

	if (self->unk1B0) {
		if (self->willFall(TChuuHana::mCheckOnPanelTimeRoll)) {
			spine->pushAfterCurrent(&TNerveChuuHanaKeepBalance::theNerve());
			return true;
		}
	} else if (spine->getTime() > 5000) {
		return true;
	}

	JGeometry::TVec3<f32> vel = self->mVelocity;
	self->unk204.x += 0.2f * (vel.x - self->unk204.x);
	self->unk204.z += 0.2f * (vel.z - self->unk204.z);

	f32 speed = JGeometry::TUtil<f32>::sqrt(self->unk204.x * self->unk204.x
	                                        + self->unk204.z * self->unk204.z);
	self->unk1B8 = 2.0f * (speed / self->mBodyRadius);
	self->unk210 += self->unk1B8;
	if (self->unk210 > 360.0f)
		self->unk210 -= 360.0f;
	if (self->unk210 < 0.0f)
		self->unk210 += 360.0f;
	return false;
}

BOOL TNerveChuuHanaFall::execute(TSpineBase<TLiveActor>* spine) const { return false; }

BOOL TNerveChuuHanaFall2::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(6);

	self->unk220 *= 0.98f;

	if (!self->isAirborne() || spine->getTime() > 800) {
		spine->pushAfterCurrent(&TNerveChuuHanaObject::theNerve());
		self->kill();
		return true;
	}
	return false;
}

BOOL TNerveChuuHanaObject::execute(TSpineBase<TLiveActor>* spine) const { return false; }

BOOL TNerveChuuHanaAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0) {
		self->setBckAnm(12);
		self->mMActor->setFrameRate(2.0f * SMSGetAnmFrameRate(), 0);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	if (SMS_GetMarioGroundPlane()->mActor != self->unk218)
		*self->unk21C = 0;

	if (spine->getTime() > self->unk1B4->mSLAttackTimer.get()) {
		spine->pushAfterCurrent(&TNerveChuuHanaWalkOnPanel::theNerve());
		spine->pushAfterCurrent(&TNerveChuuHanaWait::theNerve());
		return true;
	}

	++self->unk1A4;
	if (self->unk1A4 > 20) {
		self->unk1A4 = 0;
		if (self->willFall(TChuuHana::mCheckOnPanelTime))
			self->unk1A4 = -100;

		if (!self->isAirborne() && self->mGroundPlane->mActor == nullptr
		    && 200.0f + self->mPosition.y < self->unk1F8.y)
			self->mSpine->pushNerve(&TNerveChuuHanaFall2::theNerve());
	}

	if (self->isReachedToGoalXZ())
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	self->walkBehavior(2, self->unk1B4->mSLDashRate.get());
	return false;
}

BOOL TNerveChuuHanaJumpPrepare::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(3);

	self->unk220 = self->mPosition.y
	               - self->mMActor->getModel()
	                     ->getAnmMtx(TChuuHana::mFootJntIndex)[1][3];

	if (self->mMActor->getFrameCtrl(0)->checkPass(10.0f)) {
		JGeometry::TVec3<f32> target;
		target.x = 2.0f * self->unk1F8.x - self->mPosition.x;
		target.y = 2.0f * self->unk1F8.y - self->mPosition.y;
		target.z = 2.0f * self->unk1F8.z - self->mPosition.z;
		*self->unk21C = 0;
		target.y += self->unk1B4->mSLJumpHeight.get();

		f32 jumpSp                = self->unk1B4->mSLJumpSp.get();
		JGeometry::TVec3<f32> vel = self->calcVelocityToJumpToY(
		    target, jumpSp, self->getGravityY());
		vel.y += 10.0f;
		self->mPosition.y += 10.0f;
		self->mVelocity = vel;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->unk130 = 0;
	}

	return self->checkCurAnmEnd(0) ? TRUE : FALSE;
}

BOOL TNerveChuuHanaWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TChuuHana* self = (TChuuHana*)spine->getBody();
	if (spine->getTime() == 0)
		self->setBckAnm(11);

	return self->checkCurAnmEnd(0) ? TRUE : FALSE;
}
