#include <Enemy/Popo.hpp>
#include <Enemy/Graph.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/Application.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarioGamePad.hpp>
#include <System/Particles.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DNode.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static TPopo* gpCurPopo;

static const char* popo_bastable[] = {
	"/scene/popo/bas/popo_chase.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/popo/bas/popo_jump.bas",
	"/scene/popo/bas/popo_wait.bas",
};

bool TPopo::mRollSw         = true;
bool TPopo::mTriggerSw      = true;
f32 TPopo::mTestAng_x       = 90.0f;
f32 TPopo::mTestAng_y       = 90.0f;
f32 TPopo::mNozzleOffsetZ   = -15.0f;
u8 TPopo::mCenterJntIndex   = 1;
u8 TPopo::mMouthJntIndex    = 2;
u8 TPopo::mRLegJntIndex     = 5;
u8 TPopo::mLLegJntIndex     = 11;
u8 TPopo::mRHandJntIndex    = 7;
u8 TPopo::mLHandJntIndex    = 9;
f32 TPopo::mTestBodyScale   = 35.0f;
bool TPopo::mBrkFlag        = true;
f32 TPopo::mColOffsetY      = 20.0f;
f32 TPopo::mColMinVal       = 0.6f;
bool TPopo::mLevelShootSw   = true;
f32 TPopo::mTestAng_z;
bool TPopo::mExplosionSw;

typedef JGeometry::TPosition3<
    JGeometry::TMatrix34<JGeometry::SMatrix34C<f32> > >
    TPopoMtx;

TPopoSaveLoadParams::TPopoSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLMoveDist, 100.0f)
    , PARAM_INIT(mSLMoveGravity, 0.1f)
    , PARAM_INIT(mSLMoveJumpSp, 10.0f)
    , PARAM_INIT(mSLAttackDist, 100.0f)
    , PARAM_INIT(mSLAttackGravity, 0.1f)
    , PARAM_INIT(mSLAttackJumpSp, 10.0f)
    , PARAM_INIT(mSLReleaseSpeed, 10.0f)
    , PARAM_INIT(mSLFlyGravity, 0.0f)
    , PARAM_INIT(mSLFlyLimitTime, 300)
    , PARAM_INIT(mSLExplosionEmitTime, 60)
    , PARAM_INIT(mSLWaterScaleMax, 2.0f)
    , PARAM_INIT(mSLThrownGravity, 0.5f)
    , PARAM_INIT(mSLPumpRate, 0.0001f)
    , PARAM_INIT(mSLLevelLimit, 1.2f)
    , PARAM_INIT(mSLScaleRate, 0.99f)
{
	TParams::load(mPrmPath);
}

TPopoManager::TPopoManager(const char* name)
    : TSmallEnemyManager(name)
    , mIsNozzleFree(true)
    , mFlyWaterEmitInfo(nullptr)
    , mExplosionWaterEmitInfo(nullptr)
{
	gpCurPopo = nullptr;
	unk5C     = 0;
}

void TPopoManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38                   = new TPopoSaveLoadParams("/enemy/popo.prm");
	mFlyWaterEmitInfo       = new TWaterEmitInfo("/enemy/popowater.prm");
	mExplosionWaterEmitInfo = new TWaterEmitInfo("/enemy/popoexpwater.prm");
}

TSmallEnemy* TPopoManager::createEnemyInstance() { return new TPopo; }

void TPopoManager::initSetEnemies()
{
	TGraphWeb* graph = getObj(0)->getTracer()->getGraph();
	if (graph && graph->isDummy())
		return;
}

void TPopoManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "popoH.bmd", 0x10020000, 0 },
		{ "popoL.bmd", 0x10020000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TPopoManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[8];
	if (cue & 1) {
		for (int i = 0; i < getActiveObjNum(); ++i) {
			TPopo* popo = (TPopo*)unk18[i];
			if (popo->mHasInitialPos && popo->checkLiveFlag(LIVE_FLAG_DEAD))
				popo->reset();
		}
	}
	TEnemyManager::perform(cue, graphics);
}

BOOL TPopoCollision::receiveMessage(THitActor* sender, u32 message)
{
	if (mOwner->isRollJump())
		return mOwner->receiveMessage(sender, message);
	return FALSE;
}

static BOOL PopoRollCallback(J3DNode* node, int param_2)
{
	if (param_2 == 0) {
		if (gpCurPopo == nullptr)
			return true;

		u16 jntNo  = ((J3DJoint*)node)->getJntNo();
		MtxPtr mtx = gpCurPopo->getModel()->getAnmMtx(jntNo);

		Mtx scaleMtx;
		scaleMtx[0][3] = 0.0f;
		scaleMtx[1][3] = 0.0f;
		scaleMtx[2][3] = 0.0f;
		TPopo* popo    = gpCurPopo;
		f32 bodyScale  = popo->mBodyScale;
		scaleMtx[0][0] = bodyScale;
		scaleMtx[0][1] = 0.0f;
		scaleMtx[0][2] = 0.0f;
		scaleMtx[1][0] = 0.0f;
		scaleMtx[1][1] = bodyScale;
		scaleMtx[1][2] = 0.0f;
		scaleMtx[2][0] = 0.0f;
		scaleMtx[2][1] = 0.0f;
		scaleMtx[2][2] = bodyScale;

		Mtx rotMtx;
		if (popo->mSpine->getCurrentNerve() != &TNervePopoFly::theNerve()) {
			MsMtxSetRotX(rotMtx, gpCurPopo->mRollAngle);
		} else {
			MsMtxSetRotY(rotMtx, 180.0f);
		}

		MTXConcat(mtx, rotMtx, mtx);
		MTXConcat(mtx, scaleMtx, mtx);
		MTXConcat(J3DSys::mCurrentMtx, rotMtx, J3DSys::mCurrentMtx);
		MTXConcat(J3DSys::mCurrentMtx, scaleMtx, J3DSys::mCurrentMtx);
	}
	return true;
}

static BOOL PopoPossessedCallback(J3DNode* node, int param_2)
{
	if (param_2 == 0) {
		TPopo* popo = gpCurPopo;
		if (popo == nullptr
		    || !(popo->mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()
		         || popo->mSpine->getCurrentNerve()
		                == &TNervePopoExplosion::theNerve()
		         || popo->mIsPossessed))
			return true;

		f32 scale = gpCurPopo->mWaterScale;
		if (scale < 1.1f)
			return true;

		u16 jntNo  = ((J3DJoint*)node)->getJntNo();
		MtxPtr mtx = gpCurPopo->getModel()->getAnmMtx(jntNo);

		Mtx scaleMtx;
		scaleMtx[0][3] = 0.0f;
		scaleMtx[1][3] = 0.0f;
		scaleMtx[2][3] = 0.0f;
		scaleMtx[0][0] = scale;
		scaleMtx[0][1] = 0.0f;
		scaleMtx[0][2] = 0.0f;
		scaleMtx[1][0] = 0.0f;
		scaleMtx[1][1] = scale;
		scaleMtx[1][2] = 0.0f;
		scaleMtx[2][0] = 0.0f;
		scaleMtx[2][1] = 0.0f;
		scaleMtx[2][2] = scale;

		MTXConcat(mtx, scaleMtx, mtx);
		MTXConcat(J3DSys::mCurrentMtx, scaleMtx, J3DSys::mCurrentMtx);

		if (gpCurPopo->mIsPumping) {
			MTXCopy(mtx, gpCurPopo->mPossessedMtx);

			Mtx rotMtx;
			MsMtxSetRotRPH(rotMtx, 0.0f, 270.0f, 0.0f);
			MTXConcat(gpCurPopo->mPossessedMtx, rotMtx,
			          gpCurPopo->mPossessedMtx);

			JGeometry::TVec3<f32> axis[3];
			axis[0].x = mtx[0][0];
			axis[0].y = mtx[1][0];
			axis[0].z = mtx[2][0];
			gpCurPopo->mNozzleScale.y = axis[0].length();
			axis[1].x = mtx[0][1];
			axis[1].y = mtx[1][1];
			axis[1].z = mtx[2][1];
			gpCurPopo->mNozzleScale.z = axis[1].length();
			axis[2].x = mtx[0][2];
			axis[2].y = mtx[1][2];
			axis[2].z = mtx[2][2];
			gpCurPopo->mNozzleScale.x = axis[2].length();

			// TODO: particle 0x13C has no named enumerator yet
			JPABaseEmitter* emitter
			    = gpMarioParticleManager->emitAndBindToMtxPtr(
			        0x13C, gpCurPopo->mPossessedMtx, 1, gpCurPopo);
			if (emitter)
				emitter->setGlobalScale(gpCurPopo->mNozzleScale);
		}
	}
	return true;
}

static BOOL PopoNonScaleCallback(J3DNode* node, int param_2)
{
	if (param_2 == 0) {
		TPopo* popo = gpCurPopo;
		if (popo == nullptr
		    || !(popo->mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()
		         || popo->mSpine->getCurrentNerve()
		                == &TNervePopoExplosion::theNerve()
		         || popo->mIsPossessed))
			return true;

		u16 jntNo  = ((J3DJoint*)node)->getJntNo();
		MtxPtr mtx = gpCurPopo->getModel()->getAnmMtx(jntNo);

		Mtx scaleMtx;
		scaleMtx[1][3] = 0.0f;
		scaleMtx[0][3] = 0.0f;
		scaleMtx[2][3] = 0.0f;
		f32 scale      = 0.9f * gpCurPopo->mBodyScale;
		scaleMtx[0][0] = scale;
		scaleMtx[0][1] = 0.0f;
		scaleMtx[0][2] = 0.0f;
		scaleMtx[1][0] = 0.0f;
		scaleMtx[1][1] = scale;
		scaleMtx[1][2] = 0.0f;
		scaleMtx[2][0] = 0.0f;
		scaleMtx[2][1] = 0.0f;
		scaleMtx[2][2] = scale;

		MTXConcat(mtx, scaleMtx, mtx);
		MTXConcat(J3DSys::mCurrentMtx, scaleMtx, J3DSys::mCurrentMtx);
	}
	return true;
}

TPopo::TPopo(const char* name)
    : TWalkerEnemy(name)
    , mParams(nullptr)
    , mWaterScale(1.0f)
    , mFlyTimer(0)
    , mBrkFrameMax(30.0f)
    , mHasInitialPos(false)
    , mIsPossessed(false)
    , mRollAngle(0.0f)
    , mShootRequested(false)
    , unk1CD(0)
    , mCollision(nullptr)
{
}

void TPopo::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	mInitialPos    = mPosition;
	mHasInitialPos = true;
	reset();
}

void TPopo::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x1000000D;

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); ++i) {
			;
		}
	}

	unk150  = 0x11;
	mParams = (TPopoSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveWalkerGraphWander::theNerve());
	onHitFlag(HIT_FLAG_UNK8000000);

	mMActor->setJointCallback(mCenterJntIndex, &PopoRollCallback);
	mMActorKeeper->getMActor("popoL.bmd")
	    ->setJointCallback(mCenterJntIndex, &PopoRollCallback);
	mMActor->setJointCallback(mMouthJntIndex, &PopoPossessedCallback);
	mMActor->setJointCallback(mRLegJntIndex, &PopoNonScaleCallback);
	mMActor->setJointCallback(mLLegJntIndex, &PopoNonScaleCallback);
	mMActor->setJointCallback(mRHandJntIndex, &PopoNonScaleCallback);
	mMActor->setJointCallback(mLHandJntIndex, &PopoNonScaleCallback);
	unk188 = 0.0f;

	mCollision = new TPopoCollision("ポポコリジョン");
	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(mCollision);
	mCollision->initHitActor(0x1000000D, 2, 0x98000000, 80.0f, 80.0f, 80.0f,
	                         80.0f);
	mCollision->onHitFlag(HIT_FLAG_NO_COLLISION);
	mCollision->mOwner = this;
}

void TPopo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	mCollision->THitActor::perform(cue, graphics);
}

void TPopo::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("popoH.bmd", 3);
	mMActorKeeper->createMActor("popoL.bmd", 3);
}

void TPopo::reset()
{
	gpCurPopo = this;
	TWalkerEnemy::reset();
	unk165            = false;
	mIsPossessed      = false;
	mWaterScale       = 1.0f;
	mRollAngle        = 0.0f;
	mFlyTimer         = 0;
	mScaledBodyRadius = 15.0f * (mBodyScale * mBodyRadius);
	unk190            = 0.2f;
	expandCollision();
	mMActor = mMActorKeeper->getMActor("popoL.bmd");

	if (mHasInitialPos) {
		onLiveFlag(LIVE_FLAG_UNK10);
		mSpine->initWith(&TNervePopoWait::theNerve());
		mPosition = mInitialPos;
		offLiveFlag(LIVE_FLAG_UNK800);
	}

	mCollision->onHitFlag(HIT_FLAG_NO_COLLISION);
	unk18C = 0;
}

bool TPopo::isRollJump()
{
	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		return false;
	return true;
}

bool TPopo::checkTrigger()
{
	char trash[0x40];
	mIsPumping = false;

	if (gpMarioOriginal->onYoshi()
	    || (int)SMS_GetMarioWaterGun()->mCurrentNozzle != 0) {
		kill();
		return false;
	}

	SMS_SendMessageToMario(this, HIT_MESSAGE_UNK5);

	f32 scaleMax = mParams->mSLWaterScaleMax.get();
	int level    = (int)gpMarioOriginal->mGamePad->mCompSPos[1 * 2 + 1];
	if ((u8)level > 20) {
		mIsPumping = true;
		SMSGetMSound()->startSoundActorWithInfo(MSD_SE_EN_POPO_GROW,
		                                        &mPosition, nullptr,
		                                        mWaterScale, 0, 0, nullptr, 0,
		                                        4);
		mSprayedByWaterCooldown = 0;
		unk165                  = true;
		f32 pump = (u8)level * mParams->mSLPumpRate.get();
		mWaterScale += pump;
		if (mWaterScale > scaleMax) {
			mWaterScale = scaleMax;
			if (!mBrkFlag) {
				MActor* actor = mMActor;
				actor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BRK);
			}
		}
		f32 maxScale = mParams->mSLWaterScaleMax.get();
		if (mBrkFlag) {
			f32 frame = mBrkFrameMax * mWaterScale / maxScale;
			mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(frame);
		}
	}

	if (gpMarioOriginal->mGamePad->checkFrameMeaning(
	        TMarioGamePad::MEANING_R)
	    || !mTriggerSw) {
		if (mLevelShootSw)
			mShootRequested = true;
		else if (mWaterScale >= scaleMax)
			mShootRequested = true;
	}

	if (mLevelShootSw) {
		if (mWaterScale < scaleMax - 0.1f && mWaterScale > 1.0f)
			mWaterScale *= mParams->mSLScaleRate.get();
	}

	f32 levelLimit = mParams->mSLLevelLimit.get();
	if ((u8)level < 20) {
		if (mShootRequested || mWaterScale > levelLimit) {
			SMSGetMSound()->startSoundActor(MSD_SE_EN_POPO_POP, &mPosition, 0,
			                                nullptr, 0, 4);
			onHitFlag(HIT_FLAG_NO_COLLISION);
			mCollision->offHitFlag(HIT_FLAG_NO_COLLISION);
			return true;
		}
	}

	mScaledBodyRadius = (8.0f * mWaterScale + 8.0f) * (mBodyScale * mBodyRadius);
	if (mWaterScale >= scaleMax)
		mMActor->getFrameCtrl(ANM_TYPE_BTP)->setFrame(5.0f);
	return false;
}

void TPopo::behaveToWater(THitActor* param_1)
{
	if (mSpine->getCurrentNerve() != &TNervePopoFly::theNerve()
	    && mSpine->getCurrentNerve() != &TNervePopoExplosion::theNerve()
	    && mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		if (mSpine->getCurrentNerve() == &TNervePopoPossessedNozzle::theNerve()) {
			mSprayedByWaterCooldown = 0;
		} else if (isAirborne()) {
			JGeometry::TVec3<f32> velocity = mVelocity;
			JGeometry::TVec3<f32> away(mPosition.x - SMS_GetMarioX(), 0.0f,
			                           mPosition.z - SMS_GetMarioZ());
			MsVECNormalize(&away, &away);
			away.x *= 12.0f;
			away.y *= 12.0f;
			away.z *= 12.0f;
			away.y = -1.0f;
			away.x += velocity.x;
			away.y += velocity.y;
			away.z += velocity.z;
			mVelocity = away;
		} else if (mSpine->getCurrentNerve()
		           != &TNerveSmallEnemyFreeze::theNerve()) {
			mSpine->pushNerve(&TNerveSmallEnemyFreeze::theNerve());
		}
	}
}

f32 TPopo::getGravityY() const
{
	f32 gravity = mGravity;
	if (mSpine->getCurrentNerve() == &TNerveWalkerGraphWander::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveWalkerAttack::theNerve())
		return mParams->mSLMoveGravity.get();

	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve())
		gravity = mParams->mSLAttackGravity.get();
	else if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
		gravity = mParams->mSLFlyGravity.get();
	else if (mSpine->getCurrentNerve() == &TNervePopoThrown::theNerve())
		gravity = mParams->mSLThrownGravity.get();
	return gravity;
}

void TPopo::behaveToFindMario()
{
	TPopoManager* manager = (TPopoManager*)mManager;
	if (0) {
		(void)TPathNode((THitActor*)gpMarioAddress);
	}
	if (SMS_CheckMarioFlag(MARIO_FLAG_HAS_FLUDD) && manager->mIsNozzleFree
	    && (int)SMS_GetMarioWaterGun()->mCurrentNozzle == 0
	    && !gpMarioOriginal->onYoshi()) {
		setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		mSpine->pushAfterCurrent(&TNervePopoAttack::theNerve());
	} else {
		mSpine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
	}
}

void TPopo::walkBehavior(int, f32)
{
	if (!isAirborne()) {
		JGeometry::TVec3<f32> dir = unk104.getPoint();
		dir.set(unk104.getPoint().x - mPosition.x, 0.0f,
		        unk104.getPoint().z - mPosition.z);
		if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
			dir.x += 1.0f;
		MsVECNormalize(&dir, &dir);

		f32 dist      = mParams->mSLMoveDist.get();
		f32 jumpSpeed = mParams->mSLMoveJumpSp.get();
		TMsRange<f32> range(-20.0f, 20.0f);
		f32 randScale = 1.0f;
		if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve()) {
			jumpSpeed = mParams->mSLAttackJumpSp.get();
			dist      = mParams->mSLAttackDist.get();
			randScale = 10.0f;
			setBckAnm(0);
		}

		JGeometry::TVec3<f32> target;
		target.x = mPosition.x + dir.x * dist + randScale * range.rand();
		target.z = mPosition.z + dir.z * dist + randScale * range.rand();
		target.y = mPosition.y;

		f32 speedScale = 1.0f;
		if (mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve()) {
			speedScale = 1.2f;
			setBckAnm(5);
		}

		setVelocity(calcVelocityToJumpToY(target, jumpSpeed * speedScale,
		                                  getGravityY()));
		mPosition.y += 2.0f;
		onLiveFlag(LIVE_FLAG_AIRBORNE);

		if (mSpine->getCurrentNerve() == &TNerveWalkerGraphWander::theNerve()) {
			setGoalPath(TPathNode(target));
			setBckAnm(5);
		}
	} else {
		if (mVelocity.y > 1.5f)
			mPosition.y += 0.5f * mVelocity.y;
		if (mVelocity.y < -1.0f)
			mPosition.y += 0.2f * mVelocity.y;
	}

	JGeometry::TVec3<f32> velocity = mVelocity;
	mRollAngle += 1.0f;
	if (mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve())
		mRollAngle += 2.0f;
	if (mRollAngle > 360.0f)
		mRollAngle -= 360.0f;
	if (!mRollSw)
		mRollAngle = 0.0f;

	JGeometry::TVec3<f32> velocity2 = mVelocity;
	if (velocity2.y > 0.0f)
		walkToCurPathNode(0.0f, mTurnSpeed, 0.0f);
}

void TPopo::attackToMario()
{
	char trash[0x18];
	TPopoManager* manager = (TPopoManager*)mManager;
	if ((mSpine->getCurrentNerve() == &TNervePopoAttack::theNerve()
	     || mSpine->getCurrentNerve() == &TNervePopoWait::theNerve())
	    && manager->mIsNozzleFree) {
		mSpine->pushNerve(&TNervePopoPossessedNozzle::theNerve());
	} else if (mSpine->getCurrentNerve() == &TNerveWalkerEscape::theNerve()
	           || mSpine->getCurrentNerve()
	                  == &TNerveWalkerGraphWander::theNerve()) {
		sendAttackMsgToMario();

		JGeometry::TVec3<f32> push(0.0f, 0.0f, 0.0f);
		JGeometry::TVec3<f32> dir(mPosition.x - SMS_GetMarioX(),
		                          mPosition.y - SMS_GetMarioY(),
		                          mPosition.z - SMS_GetMarioZ());
		MsVECNormalize(&dir, &dir);
		mVelocity.x = dir.x;
		mVelocity.z = dir.z;
		f32 scale   = mBodyScale * mBodyRadius;
		dir.x *= scale;
		dir.y *= scale;
		dir.z *= scale;
		push.x += dir.x;
		push.y += dir.y;
		push.z += dir.z;
		mLinearVelocity = push;
	}
}

void TPopo::calcRootMatrix()
{
	u8 centerJnt = mCenterJntIndex;
	gpCurPopo    = this;
	MtxPtr jointMtx = getModel()->getAnmMtx(centerJnt);
	mCollision->mPosition.set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);

	if (mIsPossessed) {
		unk190 = 0.8f * mWaterScale / mParams->mSLWaterScaleMax.get();
		if (unk190 < mColMinVal)
			unk190 = mColMinVal;
		expandCollision();
		getModel()->setBaseScale(mScaling);

		TPopoMtx mtx;
		if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
			mtx.translation(mPosition.x, mPosition.y, mPosition.z);
		} else {
			MTXCopy(SMS_GetMarioWaterGun()->getEmitMtx(0), mtx);

			JGeometry::TVec3<f32> axis[3];
			axis[0].x = mtx.ref(0, 0);
			axis[0].y = mtx.ref(1, 0);
			axis[0].z = mtx.ref(2, 0);
			f32 lenX = axis[0].length();
			axis[1].x = mtx.ref(0, 1);
			axis[1].y = mtx.ref(1, 1);
			axis[1].z = mtx.ref(2, 1);
			f32 lenY = axis[1].length();
			axis[2].x = mtx.ref(0, 2);
			axis[2].y = mtx.ref(1, 2);
			axis[2].z = mtx.ref(2, 2);
			f32 lenZ = axis[2].length();
			if (lenZ != 0.0f) {
				mtx.ref(0, 0) /= lenX;
				mtx.ref(1, 0) /= lenX;
				mtx.ref(2, 0) /= lenX;
			}
			if (lenX != 0.0f) {
				mtx.ref(0, 1) /= lenY;
				mtx.ref(1, 1) /= lenY;
				mtx.ref(2, 1) /= lenY;
			}
			if (lenY != 0.0f) {
				mtx.ref(0, 2) /= lenZ;
				mtx.ref(1, 2) /= lenZ;
				mtx.ref(2, 2) /= lenZ;
			}

			TPopoMtx noseOffset(7.0f * mWaterScale + mNozzleOffsetZ, 0.0f,
			                    0.0f);
			MTXConcat(mtx, noseOffset, mtx);

			TPopoMtx bodyOffset(mTestBodyScale * mWaterScale, 0.0f, 0.0f);
			MTXConcat(mtx, bodyOffset, bodyOffset);
			mPosition.x = bodyOffset.ref(0, 3);
			mPosition.y = bodyOffset.ref(1, 3) - mColOffsetY * mWaterScale;
			mPosition.z = bodyOffset.ref(2, 3);

			if (mIsPumping) {
				MTXCopy(mMActor->getModel()->getAnmMtx(mCenterJntIndex), mNozzleMtx);
				mNozzleMtx[0][3] = bodyOffset.ref(0, 3);
				mNozzleMtx[1][3] = bodyOffset.ref(1, 3);
				mNozzleMtx[2][3] = bodyOffset.ref(2, 3);
				// TODO: particle 0x13D has no named enumerator yet
				JPABaseEmitter* emitter
				    = gpMarioParticleManager->emitAndBindToMtxPtr(
				        0x13D, mNozzleMtx, 1, this);
				if (emitter)
					emitter->setGlobalScale(mNozzleScale);
			}
		}

		Mtx rotMtx;
		MsMtxSetRotRPH(rotMtx, mTestAng_x, mTestAng_y, mTestAng_z);
		MTXConcat(mtx, rotMtx, mtx);
		MTXCopy(mtx, getModel()->getBaseTRMtx());
	} else {
		TWalkerEnemy::calcRootMatrix();
	}
}

void TPopo::kill()
{
	if (mIsPossessed) {
		((TPopoManager*)mManager)->mIsNozzleFree = true;
		mIsPossessed                             = false;
	}
	TSmallEnemy::kill();
}

void TPopo::forceKill()
{
	if (mGroundPlane->isIllegalData()
	    || !(mGroundPlane->isDeathPlane() || mGroundPlane->isPool()
	         || mGroundPlane->isWaterSurface())
	    || isAirborne() || checkLiveFlag(LIVE_FLAG_UNK10)) {
		if (gpMap->isInArea(mPosition.x, mPosition.z))
			return;
	}

	if (mSpine->getCurrentNerve() != &TNervePopoExplosion::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNervePopoExplosion::theNerve());
		mSpine->pushAfterCurrent(mSpine->getDefault());
		onLiveFlag(LIVE_FLAG_UNK20000);
		mHitPoints = 1;
	}
}

void TPopo::bind()
{
	TPopoCollision* collision = mCollision;
	for (int i = 0; i < collision->getColNum(); ++i) {
		THitActor* other = collision->getCollision(i);
		if (other->isActorType(0x80000001))
			collision->mOwner->attackToMario();
		else
			collision->mOwner->behaveToHitOthers(other);
	}

	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	if ((mSpine->getCurrentNerve() == &TNervePopoPossessedNozzle::theNerve()
	     && mWaterScale > 1.2f && mExplosionSw)
	    || mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
		mGroundHeight = gpMap->checkGround(
		    mPosition.x, mPosition.y + mHeadHeight, mPosition.z, &mGroundPlane);
		if (mPosition.y <= mGroundHeight + 30.0f
		    || (abs(JGeometry::TVec3<f32>(mVelocity).x) < 1.0f
		        && abs(JGeometry::TVec3<f32>(mVelocity).z) < 1.0f))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());

		TBGWallCheckRecord record(mPosition.x, mPosition.y, mPosition.z,
		                          mWaterScale * (mBodyScale * mWallRadius), 1,
		                          0);
		if (gpMap->isTouchedWallsAndMoveXZ(&record))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
		else if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve())
			TLiveActor::bind();
	} else {
		TLiveActor::bind();
	}
}

bool TPopo::isHitValid(u32 message)
{
	if (message == 0xB)
		return true;
	if (message <= 1)
		mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	return false;
}

bool TPopo::isFindMario(f32 scale)
{
	if (mSpine->getTime() > 100
	    && !gpMarioOriginal->checkFlag(MARIO_FLAG_VISIBLE)) {
		TSmallEnemyParams* params = getSaveParams();
		JGeometry::TVec3<f32> marioPos(SMS_GetMarioX(), SMS_GetMarioY(),
		                               SMS_GetMarioZ());
		f32 searchLength = params->mSLSearchLength.get();
		f32 searchAngle  = params->mSLSearchAngle.get();
		f32 searchAware  = params->mSLSearchAware.get();
		if (isInSight(marioPos, searchLength * scale, searchAngle * scale,
		              searchAware * scale))
			return true;
	}
	return false;
}

bool TPopo::isCollidMove(THitActor* other)
{
	if (mSpine->getCurrentNerve() == &TNervePopoFly::theNerve()) {
		if (other->receiveMessage(this, 0))
			mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}
	return false;
}

void TPopo::flyBehavior()
{
	char trash0[4];
	JGeometry::TVec3<f32> pos;
	char trash[0x14];
	++mFlyTimer;
	if (mFlyTimer > mParams->mSLFlyLimitTime.get()) {
		mFlyTimer = 0;
		mSpine->pushNerve(&TNervePopoExplosion::theNerve());
	}

	if (mWaterScale > 1.0f)
		mWaterScale *= 0.999f;

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		pos = mPosition;
	} else {
		MtxPtr mtx = getModel()->getAnmMtx(mMouthJntIndex);
		pos.x      = mtx[0][3];
		pos.y      = mtx[1][3];
		pos.z      = mtx[2][3];
	}

	TPopoManager* manager = (TPopoManager*)mManager;
	manager->mFlyWaterEmitInfo->mPos.value = pos;
	gpModelWaterManager->emitRequest(*manager->mFlyWaterEmitInfo);

	SMSGetMSound()->startSoundActor(MSD_SE_EN_POPO_FLY, &mPosition, 0, nullptr,
	                                0, 4);
}

void TPopo::explosion()
{
	if (mWaterScale > 1.0f)
		mWaterScale *= 0.9f;

	TPopoManager* manager     = (TPopoManager*)mManager;
	JGeometry::TVec3<f32> pos = mPosition;
	pos.y += 100.0f;

	if (mSpine->getTime() % 2 == 0) {
		JGeometry::TVec3<f32>& dirRef
		    = manager->mExplosionWaterEmitInfo->mDir.value;
		JGeometry::TVec3<f32> dir = dirRef;
		dir.y *= -1.0f;
		dirRef = dir;
	}

	f32 num = manager->mExplosionWaterEmitInfo->mNum.get()
	          * (mWaterScale / mParams->mSLWaterScaleMax.get());
	if (num < 2.0f)
		num = 2.0f;
	manager->mExplosionWaterEmitInfo->mNum.set((s32)num);

	manager->mExplosionWaterEmitInfo->mPos.value = pos;
	gpModelWaterManager->emitRequest(*manager->mExplosionWaterEmitInfo);
}

void TPopo::possessedIn()
{
	char trash[0x10];
	mMActor = mMActorKeeper->getMActor("popoH.bmd");
	setBckAnm(3);
	mMActor->setBtpFromIndex(0);
	mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
	if (!mExplosionSw)
		onHitFlag(HIT_FLAG_NO_COLLISION);
	mMActor->setBrkFromIndex(0);
	mMActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(0.0f);
	mBrkFrameMax = 30.0f;
	mMActor->setFrameRate(0.0f, ANM_TYPE_BRK);
	offLiveFlag(LIVE_FLAG_UNK10);
	mRollAngle   = 90.0f;
	mIsPossessed = true;
	SMSGetMSound()->startSoundActor(MSD_SE_EN_POPO_STUCK, &mPosition, 0,
	                                nullptr, 0, 4);
	mShootRequested = false;
	unk1CD          = 0;
}

void TPopo::thrownByChorobei()
{
	mSpine->initWith(&TNervePopoThrown::theNerve());
}

const char** TPopo::getBasNameTable() const { return popo_bastable; }

BOOL TNervePopoPossessedNozzle::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		TPopoManager* manager = (TPopoManager*)self->mManager;
		if (!manager->mIsNozzleFree) {
			spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
			return true;
		}
		manager->mIsNozzleFree = false;
		self->possessedIn();
	}

	if (self->checkCurAnmEnd(0)) {
		if (self->unsetUnk165()) {
			self->setBckAnm(3);
			MActor* actor = self->mMActor;
			actor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BTP);
		} else {
			self->setBckAnm(4);
			self->mMActor->getFrameCtrl(ANM_TYPE_BTP)->setFrame(0.0f);
			self->mMActor->setFrameRate(0.0f, ANM_TYPE_BTP);
		}
	}

	if (self->checkTrigger()) {
		spine->pushAfterCurrent(&TNervePopoFly::theNerve());
		return true;
	}

	return false;
}

BOOL TNervePopoAttack::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x10];
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

	if (!self->isAirborne()) {
		if (!((TPopoManager*)self->mManager)->mIsNozzleFree)
			return true;
		if (gpMarioOriginal->checkFlag(MARIO_FLAG_VISIBLE))
			return true;
		f32 giveUpHeight = self->getSaveParams()->mSLGiveUpHeight.get();
		if (abs(SMS_GetMarioY() - self->mPosition.y) > giveUpHeight)
			return true;
		if (self->isResignationAttack())
			return true;
	}

	self->walkBehavior(0, 1.0f);
	return false;
}

BOOL TNervePopoFly::execute(TSpineBase<TLiveActor>* spine) const
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(2);

		MtxPtr emitMtx = SMS_GetMarioWaterGun()->getEmitMtx(0);
		f32 speed      = self->mParams->mSLReleaseSpeed.get()
		            * (self->mWaterScale
		               / self->mParams->mSLWaterScaleMax.get());
		JGeometry::TVec3<f32> velocity;
		velocity.x = speed * emitMtx[0][0];
		velocity.y = speed * emitMtx[1][0];
		velocity.z = speed * emitMtx[2][0];
		self->setVelocity(velocity);
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);

		if (self->mIsPossessed) {
			((TPopoManager*)self->mManager)->mIsNozzleFree = true;
			self->mIsPossessed                             = false;
		}

		f32 yaw = MsGetRotFromZaxisY(velocity);
		self->mRotation.set(0.0f, MsWrap(yaw, 0.0f, 360.0f), 0.0f);

		if (TPopo::mExplosionSw)
			self->offHitFlag(HIT_FLAG_NO_COLLISION);
	} else {
		if (!self->isAirborne()) {
			spine->pushAfterCurrent(&TNervePopoExplosion::theNerve());
			return true;
		}
	}

	if (spine->getTime() > 5) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mCollision->offHitFlag(HIT_FLAG_NO_COLLISION);
	}
	self->flyBehavior();
	return false;
}

BOOL TNervePopoExplosion::execute(TSpineBase<TLiveActor>* spine) const
{
	JGeometry::TVec3<f32> zero;
	char trash[0x1c];
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		zero.set(0.0f, 0.0f, 0.0f);
		self->setVelocity(zero);
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);

		if (self->mIsPossessed) {
			((TPopoManager*)self->mManager)->mIsNozzleFree = true;
			self->mIsPossessed                             = false;
		}

		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->onLiveFlag(LIVE_FLAG_UNK8);

		MtxPtr mtx = self->mMActor->getModel()->getAnmMtx(TPopo::mCenterJntIndex);
		self->mExplosionPos.set(mtx[0][3], mtx[1][3], mtx[2][3]);
		gpMarioParticleManager->emit(PARTICLE_MS_POPO_BOMB_A,
		                             &self->mExplosionPos, 0, nullptr);
		gpMarioParticleManager->emit(PARTICLE_MS_POPO_BOMB_B,
		                             &self->mExplosionPos, 0, nullptr);
	}

	if (spine->getTime() > self->mParams->mSLExplosionEmitTime.get()) {
		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();
		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(spine->getDefault());
		return true;
	}

	self->explosion();
	return false;
}

BOOL TNervePopoWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onLiveFlag(LIVE_FLAG_UNK10);
		self->setBckAnm(6);
		self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));
	}

	self->walkToCurPathNode(0.0f, self->getTurnSpeed(), 0.0f);
	return false;
}

BOOL TNervePopoThrown::execute(TSpineBase<TLiveActor>* spine) const
{
	TPopo* self = (TPopo*)spine->getBody();

	if (spine->getTime() > 30 && !self->isAirborne()) {
		spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
		return true;
	}

	return false;
}
