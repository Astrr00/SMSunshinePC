#include <Enemy/BossWanwan.hpp>
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
#include <MarioUtil/MtxUtil.hpp>
#include <Enemy/EffectObj.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DCluster.hpp>

// file-level data
const JGeometry::TVec3<f32> BW_BATH_POS(-1000.0f, 4.5f, -6217.2f);
const JGeometry::TVec3<f32> BW_PICKET_START(6012.84f, 0.0f, 7323.15f);
const JGeometry::TVec3<f32> BW_HEAD_START(5741.72f, -100.0f, 6311.62f);

static const char* bwanwan_bastable[] = {
	"/scene/bwanwan/bas/bwanwan_bark.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_shake.bas",
	nullptr,
	"/scene/bwanwan/bas/bwanwan_wait.bas",
	"/scene/bwanwan/bas/bwanwan_wait2.bas",
	nullptr,
};

// inline helpers shared by the nerves (no standalone symbol in the original)
inline void TBossWanwanMtxCalc::joinAnm(int i)
{
	M3UMtxCalcSIAnmBlendQuat::joinAnm(mOwner->getActorKeeper()->getMActorAnmData()->getUnk2C()->getAnmPtr(i));
}

inline void TBossWanwan::changeBck(int i)
{
	mMtxCalc->joinAnm(i);
	mMActor->setFrameCtrlForBck(i);
	unk178 = 10.0f / mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	setAnmSound(bwanwan_bastable[i]);
}

// fabricated: horizontal vector of the given length along the yaw (degrees)
static inline JGeometry::TVec3<f32> polarXZ(f32 theta, f32 radius)
{
	f32 c = radius * MsCos(theta);
	f32 s = radius * MsSin(theta);
	return JGeometry::TVec3<f32>(s, 0.0f, c);
}

static inline BOOL isPulledOverLimit(TBossWanwan* boss)
{
	if (boss->unk15C.squared() < boss->getBossWanwanParams()->mSLPullLimit.get())
		return FALSE;
	return TRUE;
}

// MsSqrtf, but inlined here
static inline f32 bwSqrtf(f32 x)
{
	volatile float y;
	if (x > 0.0f) {
		double guess = __frsqrte((double)x);
		guess        = .5 * guess * (3.0 - guess * guess * x);
		y            = (float)(x * guess);
		return y;
	}
	return x;
}

inline TBWHit::TBWHit(TBossWanwan* owner, int jointIndex, const char* name)
    : THitActor(name)
    , mOwner(owner)
    , mJointIndex(jointIndex)
{
}

inline TBWPicket::TBWPicket(TBossWanwan* owner, const char* name)
    : TTakeActor(name)
    , mOwner(owner)
    , mMActor(nullptr)
{
	MTXIdentity(unk74);

	initHitActor(0x800000D, 1, 0x80000000,
	             mOwner->getBossWanwanParams()->mSLPicketRadius.get(),
	             mOwner->getBossWanwanParams()->mSLPicketHeight.get(),
	             mOwner->getBossWanwanParams()->mSLPicketRadius.get(),
	             mOwner->getBossWanwanParams()->mSLPicketHeight.get());

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	mMActor = mOwner->mMActorKeeper->createMActor("bwanwan_picket.bmd", 0);
}

inline TBossWanwanMtxCalc::TBossWanwanMtxCalc(TBossWanwan* owner)
    : M3UMtxCalcSIAnmBlendQuat(false)
    , mOwner(owner)
{
	mMotionBlendRatio = 0.0f;
}

// fabricated
static inline JGeometry::TVec3<f32> forwardFromYaw(s16 angle, f32 radius)
{
	return JGeometry::TVec3<f32>(radius * JMASSin(angle), 0.0f,
	                             radius * JMASCos(angle));
}

TBWLeashNode::TBWLeashNode(TBWLeash* leash, int index, const char* name)
    : THitActor(name)
    , mOwner(leash)
    , mMActor(nullptr)
    , mTemperature(0.0f)
    , mIndex(index)
{
	TBossWanwan* boss = mOwner->mOwner;
	mMActor           = boss->mMActorKeeper->createMActor("bwanwan_chain.bmd", 0);
	mMActor->setBrkFromIndex(2);
	f32 radius = boss->getBossWanwanParams()->mSLChainHitRadius.get();
	f32 height = boss->getBossWanwanParams()->mSLChainHitHeight.get();
	initHitActor(0x800000C, 1, 0x80000000, 1.2f * radius, 1.2f * height, radius,
	             height);
}

TBWParams::TBWParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLMarchSpeed, 6.0f)
    , PARAM_INIT(mSLTurnSpeed, 1.0f)
    , PARAM_INIT(mSLLeashNodeLen, 120.0f)
    , PARAM_INIT(mSLPicketHeight, 100.0f)
    , PARAM_INIT(mSLPicketRadius, 100.0f)
    , PARAM_INIT(mSLChainHitHeight, 100.0f)
    , PARAM_INIT(mSLChainHitRadius, 100.0f)
    , PARAM_INIT(mSLChainGroundRadius, 60.0f)
    , PARAM_INIT(mSLPullLimit, 1.0f)
    , PARAM_INIT(mSLAttackSpeed, 10.0f)
    , PARAM_INIT(mSLStunTimer, 4000)
    , PARAM_INIT(mSLSearchLength, 10000.0f)
    , PARAM_INIT(mSLSearchAngle, 60.0f)
    , PARAM_INIT(mSLBWHitPointMax, 255)
    , PARAM_INIT(mSLHeadGap, 150.0f)
    , PARAM_INIT(mSLShakeLengthMax, 3000.0f)
    , PARAM_INIT(mSLShakeLengthMaxHP0, 2000.0f)
{
	TParams::load(mPrmPath);
}

void TBWLeashNode::calcTemperature()
{
	if (mIndex == 0)
		return;

	int prev = mIndex - 1;
	f32 diff = mOwner->mNodes[prev]->mTemperature - mTemperature;
	f32 step;
	if (diff < 0.0f) {
		if (diff < -0.1f)
			step = -0.02f;
		else
			step = -0.005f;
	} else {
		if (diff > 0.1f)
			step = 0.02f;
		else
			step = 0.005f;
	}

	mTemperature += step;
	if (mTemperature < 0.0f)
		mTemperature = 0.0f;
	if (mTemperature > 1.0f)
		mTemperature = 1.0f;
}

void TBWLeashNode::calcMatrix()
{
	TRope* rope          = mOwner->mRope;
	TRopePoint* points   = rope->mPoints;
	MtxPtr mtx           = mMActor->getModel()->getBaseTRMtx();
	int index            = mIndex;
	TRopePoint* point    = &points[index];
	JGeometry::TVec3<f32> pos = point->unkC;

	JGeometry::TVec3<f32> dir;
	if (index < rope->mNumPoints - 1) {
		dir = points[index + 1].unkC;
		dir -= pos;
	} else {
		dir = points[index - 1].unkC;
		dir -= pos;
		dir.negate();
	}
	PSVECNormalize(&dir, &dir);

	JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
	JGeometry::TVec3<f32> side;
	side.cross(up, dir);
	PSVECNormalize(&side, &side);
	up.cross(dir, side);
	PSVECNormalize(&up, &up);

	mtx[0][2] = dir.x;
	mtx[1][2] = dir.y;
	mtx[2][2] = dir.z;
	if (mIndex & 1) {
		mtx[0][0] = side.x;
		mtx[1][0] = side.y;
		mtx[2][0] = side.z;
		mtx[0][1] = up.x;
		mtx[1][1] = up.y;
		mtx[2][1] = up.z;
	} else {
		mtx[0][0] = up.x;
		mtx[1][0] = up.y;
		mtx[2][0] = up.z;
		mtx[0][1] = side.x;
		mtx[1][1] = side.y;
		mtx[2][1] = side.z;
	}
	mtx[0][3] = pos.x;
	mtx[1][3] = 30.0f + pos.y;
	mtx[2][3] = pos.z;

	mPosition = pos;
}

void TBWLeashNode::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		calcTemperature();
		calcMatrix();
		if (mOwner->mOwner->getHitPoints() != 0 && mIndex < 8) {
			for (int i = 0; i < mColCount; ++i) {
				THitActor* hit = mCollisions[i];
				if (hit->getActorType() == 0x80000001)
					hit->receiveMessage(this, HIT_MESSAGE_UNKA);
			}
		}
	}

	if (cue & 2) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BRK);
		if (ctrl != nullptr) {
			f32 frame = mTemperature * (ctrl->getEnd() - 1);
			f32 scale;
			if (mIndex < 5) {
				scale = (f32)mOwner->mOwner->getHitPoints()
				            / (f32)mOwner->mOwner->getBossWanwanParams()
				                  ->mSLBWHitPointMax.get()
				        + 0.25f * mIndex;
			} else if (mIndex >= mOwner->mRope->mNumPoints - 10) {
				scale = (mOwner->mRope->mNumPoints - mIndex) / 10.0f;
			} else {
				scale = 1.0f;
			}

			if (scale > 1.0f)
				scale = 1.0f;
			else if (scale < 0.0f)
				scale = 0.0f;

			frame *= scale;
			ctrl->setFrame(frame);
			ctrl->setRate(0.0f);
		}
	}

	if (mIndex < mOwner->mRope->mNumPoints - 1)
		mMActor->perform(cue, graphics);
	char trash[0x60];
}

TBWLeash::TBWLeash(TBossWanwan* owner, int numNodes, const char* name)
    : JDrama::TViewObj(name)
    , mOwner(owner)
    , mRope(nullptr)
    , mNodes(nullptr)
{
	f32 nodeLength = mOwner->getBossWanwanParams()->mSLLeashNodeLen.get();
	f32 radius = mOwner->getBossWanwanParams()->mSLChainGroundRadius.get();
	mRope      = new TRope(numNodes, mOwner->mPosition, nodeLength, radius, 0.7f,
	                       -2.0f);
	mNodes     = new TBWLeashNode*[numNodes];
	for (int i = 0; i < numNodes; ++i)
		mNodes[i] = new TBWLeashNode(this, i);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	for (int i = 0; i < numNodes; ++i) {
		group->getChildren().push_back(mNodes[i]);
		if (i < numNodes - 2)
			mNodes[i]->offHitFlag(HIT_FLAG_NO_COLLISION);
		else
			mNodes[i]->onHitFlag(HIT_FLAG_NO_COLLISION);

		if (i < 2)
			mRope->mPoints[i].unk28 |= 1;
		else
			mRope->mPoints[i].unk28 &= ~1;
	}
}

void TBWLeash::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & 1) {
		JGeometry::TVec3<f32> headPos;
		mOwner->getJointTransByIndex(5, &headPos);
		mRope->moveHead(headPos);
		mOwner->unk188 = 0;

		if (mOwner->unk17C != 0) {
			const JGeometry::TVec3<f32>& stakePos = mOwner->mPicket->mPosition;
			JGeometry::TVec3<f32> pull = mRope->mPoints[0].unkC;
			mRope->constraintTail(stakePos);
			pull -= mRope->mPoints[0].unkC;
			pull.negate();
			mOwner->unk15C = pull;

			JGeometry::TVec3<f32> toHead = mRope->mPoints[0].unkC;
			JGeometry::TVec3<f32> bossTop = mOwner->mPosition;
			bossTop.y += 500.0f;
			toHead -= bossTop;
			if (PSVECMag(&toHead) > 650.0f) {
				PSVECNormalize(&toHead, &toHead);
				toHead *= 20.0f;
				toHead.y = 0.0f;
				mOwner->mPosition += toHead;
				mOwner->unk188 = 1;
			}

			f32 target = MsWrap(180.0f + MsGetRotFromZaxisY(toHead), 0.0f, 360.0f);
			f32 delta  = MsAngleDiff(target, mOwner->mRotation.y);
			f32 turn;
			if (delta > 0.0f) {
				f32 limit = 1.5f * mOwner->mTurnSpeed;
				turn      = delta > limit ? limit : delta;
			} else {
				f32 limit = 1.5f * -mOwner->mTurnSpeed;
				turn      = delta > limit ? delta : limit;
			}
			mOwner->mRotation.y = MsWrap(mOwner->mRotation.y + turn, 0.0f, 360.0f);
		}
	}

	// keep the chain out of the boss' body
	if (cue & 2) {
		JGeometry::TVec3<f32> bodyPos;
		mOwner->getJointTransByIndex(1, &bodyPos);
		for (int i = 0; i < 15; ++i) {
			TRopePoint* point = &mRope->mPoints[i];
			JGeometry::TVec3<f32> offset = point->unkC;
			offset -= bodyPos;
			f32 dist = PSVECMag(&offset);
			if (dist < 500.0f) {
				offset *= 500.0f / dist;
				offset += bodyPos;
				point->unk18.zero();
				point->unkC = offset;
				point->unk0 = point->unkC;
			}
		}
	}

	for (int i = 0; i < mRope->mNumPoints; ++i)
		mNodes[i]->testPerform(cue, graphics);
	char trash[0x80];
}

BOOL TBWPicket::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x80000001) {
		if (message == HIT_MESSAGE_HIP_DROP) {
			mOwner->unk17C = 1;
			mOwner->unk184 = 0;
			gpMSound->startSoundActor(0x28C0, &mPosition, 0, nullptr, 0, 4);
			return true;
		}

		if (message == HIT_MESSAGE_TAKE) {
			TBossWanwan* boss = mOwner;
			if (boss->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xAE, &boss->mPicket->mPosition, 0, nullptr);
				if (emitter) {
					emitter->setGlobalScale(JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
				}
			}
			boss->unk194 = 0;
			boss->unk17C = 0;
			mHolder      = static_cast<TTakeActor*>(sender);
			return true;
		}

		if (message == HIT_MESSAGE_THROWN || message == HIT_MESSAGE_UNK8) {
			mHolder = nullptr;
			return true;
		}
	}
	return false;
}

BOOL TBWPicket::moveRequest(const JGeometry::TVec3<f32>& target)
{
	char trash[0x30]; // stack padding to match the retail frame size
	if (&TNerveBWJumpToBath::theNerve() == mOwner->mSpine->getLatestNerve()
	    || &TNerveBWDie::theNerve() == mOwner->mSpine->getLatestNerve())
		return false;

	if (mOwner->mHitPoints != 0)
		return false;

	TBWLeash* leash = mOwner->mLeash;
	JGeometry::TVec3<f32> pull = leash->mRope->mPoints[0].unkC;
	leash->mRope->constraintTail(target);
	pull -= leash->mRope->mPoints[0].unkC;
	pull.negate();
	leash->mOwner->unk15C = pull;
	return true;
}

MtxPtr TBWPicket::getTakingMtx() { return unk74; }

void TBWPicket::perform(u32 flags, JDrama::TGraphics* graphics)
{
	if (flags & CUE_MOVE) {
		TRope* rope = mOwner->mLeash->mRope;
		mPosition   = rope->mPoints[rope->mNumPoints - 1].unkC;
		ensureTakeSituation();

		if (mHolder != nullptr) {
			TBWLeash* leash = mOwner->mLeash;
			JGeometry::TVec3<f32> toHolder(
			    static_cast<const Vec&>(mHolder->mPosition));
			toHolder -= leash->mRope->mPoints[0].unkC;

			JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
			JGeometry::TVec3<f32> right;
			right.cross(up, toHolder);
			PSVECNormalize(&right, &right);
			up.cross(toHolder, right);
			PSVECNormalize(&up, &up);
			toHolder.cross(right, up);
			PSVECNormalize(&toHolder, &toHolder);

			MtxPtr takingMtx = unk74;
			takingMtx[0][0] = right.x;
			takingMtx[1][0] = right.y;
			takingMtx[2][0] = right.z;
			takingMtx[0][1] = up.x;
			takingMtx[1][1] = up.y;
			takingMtx[2][1] = up.z;
			takingMtx[0][2] = toHolder.x;
			takingMtx[1][2] = toHolder.y;
			takingMtx[2][2] = toHolder.z;
			takingMtx[0][3] = mPosition.x;
			takingMtx[1][3] = mPosition.y;
			takingMtx[2][3] = mPosition.z;
			mHolder->moveRequest(mPosition);
		}
	}

	char trash0[4];
	if (flags & CUE_CALC_ANIM) {
		MtxPtr mtx = mMActor->getModel()->getBaseTRMtx();
		MTXIdentity(mtx);

		TBWLeash* leash = mOwner->mLeash;
		JGeometry::TVec3<f32> dir(static_cast<const Vec&>(mPosition));
		dir -= leash->mRope->mPoints[leash->mRope->mNumPoints - 3].unkC;
		if (dir.isZero())
			dir.set(0.0f, 0.0f, 1.0f);

		JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
		JGeometry::TVec3<f32> right;
		right.cross(up, dir);
		PSVECNormalize(&right, &right);
		up.cross(dir, right);
		PSVECNormalize(&up, &up);
		dir.cross(right, up);
		PSVECNormalize(&dir, &dir);

		mtx[0][0] = right.x;
		mtx[1][0] = right.y;
		mtx[2][0] = right.z;
		mtx[0][1] = up.x;
		mtx[1][1] = up.y;
		mtx[2][1] = up.z;
		mtx[0][2] = dir.x;
		mtx[1][2] = dir.y;
		mtx[2][2] = dir.z;
		mtx[0][3] = mPosition.x;
		mtx[1][3] = mPosition.y;
		mtx[2][3] = mPosition.z;

		if (mOwner->unk17C == 0)
			mtx[1][3] += 70.0f;

		if (isTaken()) {
			mtx[0][3] -= 50.0f * dir.x;
			mtx[2][3] -= 50.0f * dir.z;
		} else {
			mtx[0][3] -= 60.0f * dir.x;
			mtx[2][3] -= 60.0f * dir.z;
		}
	}

	char trash[0xC4];
	mMActor->perform(flags, graphics);
	THitActor::perform(flags, graphics);
}

BOOL TBWHit::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

void TBWHit::perform(u32 flags, JDrama::TGraphics* graphics)
{
	char trash[8];
	if (flags & CUE_MOVE) {
		if (mJointIndex >= 0)
			mOwner->getJointTransByIndex(mJointIndex, &mPosition);

		for (int i = 0; i < mColCount; ++i) {
			THitActor* actor = mCollisions[i];
			if (mOwner->mHitPoints != 0 && actor->getActorType() == 0x80000001)
				actor->receiveMessage(mOwner, HIT_MESSAGE_UNKA);
		}
	}

	THitActor::perform(flags, graphics);
}

void TBWBinder::bind(TLiveActor* actor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(actor);
	char trash[0xa8]; // stack padding to match the retail frame size

	JGeometry::TVec3<f32> velocity = boss->mLinearVelocity;
	JGeometry::TVec3<f32> nextPos  = boss->mPosition;
	nextPos += velocity;

	if (boss->isAirborne()) {
		JGeometry::TVec3<f32> fall = boss->mVelocity;
		nextPos += fall;
		fall.y -= boss->getGravityY();
		if (fall.y < TLiveActor::mVelocityMinY)
			fall.y = TLiveActor::mVelocityMinY;
		boss->mVelocity = fall;
	}

	if (&TNerveBWJumpToBath::theNerve() == boss->mSpine->getLatestNerve()
	    || &TNerveBWDie::theNerve() == boss->mSpine->getLatestNerve()) {
		boss->mLinearVelocity = nextPos - boss->mPosition;
		return;
	}

	if (boss->isAirborne()) {
		const TBGCheckData* plane;
		f32 nextY = nextPos.y;
		f32 nextZ = nextPos.z;
		f32 groundY = gpMap->checkGround(
		    nextPos.x, nextY + boss->getHeadHeight(), nextZ, &plane);
		groundY += 1.0f;

		if (boss->mPosition.y > nextY && !plane->isEnemyThrough()) {
			const TBGCheckData* plane2;
			f32 groundY2 = gpMap->checkGround(
			    nextPos.x, boss->mPosition.y + boss->getHeadHeight(), nextZ,
			    &plane2);
			groundY2 += 1.0f;
			if (groundY2 > groundY) {
				plane   = plane2;
				groundY = groundY2;
			}
		}

		if (nextY <= groundY && !plane->checkFlag(BG_CHECK_FLAG_ILLEGAL)
		    && !plane->isEnemyThrough()) {
			nextPos.y        = groundY;
			boss->mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
			boss->offLiveFlag(LIVE_FLAG_AIRBORNE);
			boss->offLiveFlag(LIVE_FLAG_UNK8000);
		} else {
			boss->onLiveFlag(LIVE_FLAG_AIRBORNE);
		}

		boss->mGroundHeight = groundY;
		boss->mGroundPlane  = plane;
	}

	velocity = nextPos - boss->mPosition;

	// on the ground the movement follows the current graph link
	if (!boss->isAirborne()) {
		TGraphTracer* tracer = boss->unk124;
		if (tracer->unk0 != nullptr) {
			int cur  = tracer->mCurrIdx;
			int prev = tracer->mPrevIdx;
			if (cur >= 0 && prev >= 0 && cur != prev) {
				TGraphNode* nodes = tracer->unk0->unk0;
				TGraphNode* nodeCur  = &nodes[cur];
				TGraphNode* nodePrev = &nodes[prev];
				JGeometry::TVec3<f32> curPoint;
				JGeometry::TVec3<f32> prevPoint;
				nodeCur->getPoint(curPoint);
				nodePrev->getPoint(prevPoint);
				curPoint -= prevPoint;
				PSVECNormalize(&curPoint, &curPoint);

				f32 lenSq = curPoint.squared();
				f32 scale = lenSq == 0.0f ? 0.0f : velocity.dot(curPoint) / lenSq;

				f32 speed = scale;
				if (scale < 0.0f) {
					if (scale > -3.0f)
						speed = -3.0f;
				} else if (scale > 0.0f) {
					if (scale < 3.0f)
						speed = 3.0f;
				}

				velocity = curPoint;
				velocity *= speed;
			}
		}
	}

	// spin the ball according to the distance rolled
	if (!boss->isAirborne()) {
		f32 mag = PSVECMag(JGeometry::TVec3<f32>(velocity));
		if (mag != 0.0f) {
			f32 roll = 360.0f * (mag / 3141.5928f);
			JGeometry::TVec3<f32> forward
			    = forwardFromYaw(182.04445f * boss->mRotation.y, 1.0f);
			if (forward.dot(velocity) < 0.0f)
				roll = -roll;
			roll *= 2.0f;

			if (boss->unk16C != 0) {
				boss->unk168 = MsAngleWrap(roll + boss->unk168);
			} else if (0.0f != boss->unk168) {
				f32 angle = boss->unk168 + roll;
				if (angle > 360.0f)
					angle = 0.0f;
				boss->unk168 = angle;
			}
		}
	}

	// keep the boss within reach of the chain
	if (boss->unk17C != 0) {
		TBWLeash* leash = boss->mLeash;
		JGeometry::TVec3<f32> pos = boss->mPosition;
		JGeometry::TVec3<f32> target = pos;
		JGeometry::TVec3<f32> origin = pos;
		target += velocity;

		JGeometry::TVec3<f32> anchor = leash->mRope->mPoints[3].unkC;
		target -= anchor;
		if (PSVECMag(&target) > 860.0f) {
			PSVECNormalize(&target, &target);
			target *= 860.0f;
			target += anchor;
			target -= origin;
			velocity = target;
		}
	}

	if (!boss->isAirborne()) {
		TGraphTracer* tracer = boss->unk124;
		TGraphNode* nodeCur  = &tracer->getCurrent();
		TGraphNode* nodePrev = &tracer->getPrevious();

		JGeometry::TVec3<f32> curPoint;
		JGeometry::TVec3<f32> prevPoint;
		nodeCur->getPoint(curPoint);
		nodePrev->getPoint(prevPoint);

		JGeometry::TVec3<f32> foot
		    = MsPerpendicFootToLineR(prevPoint, curPoint, boss->mPosition);
		curPoint -= prevPoint;

		JGeometry::TVec3<f32> toFoot = foot;
		toFoot -= boss->mPosition;
		f32 dist = PSVECMag(&toFoot);
		if (dist > 10.0f)
			dist = 10.0f;
		if (dist < 0.0001f) {
			toFoot.zero();
		} else {
			PSVECNormalize(&toFoot, &toFoot);
			toFoot *= dist;
		}
		velocity += toFoot;
	}

	boss->mLinearVelocity = velocity;
}

void TBossWanwanMtxCalc::calc(u16 jointIndex)
{
	if (jointIndex == 0 && mOwner->isAirborne()) {
		j3dSys.setCurrentMtxCalc(this);

		J3DTransformInfo info;
		if (mNewAnm != nullptr) {
			mNewAnm->getTransform(jointIndex, &info);
		} else {
			info = j3dSys.getModel()
			           ->getModelData()
			           ->getJointNodePointer(jointIndex)
			           ->getTransformInfo();
		}
		info.mTranslate.x = 0.0f;
		info.mTranslate.y = 0.0f;
		info.mTranslate.z = 0.0f;
		calcTransform(jointIndex, info);
	} else {
		M3UMtxCalcSIAnmBlendQuat::calc(jointIndex);
		if (jointIndex == 1) {
			TPosition3f rotation;
			rotation.zero();
			MtxPtr rotMtx = rotation;
			MsMtxSetRotX(rotMtx, mOwner->unk168);

			MtxPtr jointMtx = mOwner->getModel()->getAnmMtx(jointIndex);
			MTXConcat(jointMtx, rotMtx, jointMtx);
			MTXCopy(jointMtx, J3DSys::mCurrentMtx);
		}
	}
	char trash[0x10];
}

TBossWanwan::TBossWanwan(const char* name)
    : TSpineEnemy(name)
    , mMtxCalc(nullptr)
    , mLeash(nullptr)
    , mPicket(nullptr)
    , unk168(0.0f)
    , unk16C(0)
    , unk17C(0)
    , unk180(0)
    , unk184(0)
    , unk188(0)
    , unk18C(0)
    , unk18D(0)
    , unk190(0)
    , unk194(1)
    , unk195(0)
    , unk198(0)
    , unk19C(0)
    , unk1A0(0)
    , unk1A4(0.0f, 0.0f, 0.0f)
    , unk1B0(0)
    , unk1B4(0)
{
	mBinder = new TBWBinder;
}

void TBossWanwan::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 0x11);
	mMActor       = mMActorKeeper->createMActor("bwanwan_body.bmd", 0);

	TGraphWeb* graph = gpConductor->getGraphByName("bwanwan");
	graph->initGoalIndex(BW_BATH_POS);
	unk124->setGraph(graph);

	mSpine->initWith(&TNerveBWGraphWander::theNerve());

	mMarchSpeed = getBossWanwanParams()->mSLMarchSpeed.get();
	mTurnSpeed  = getBossWanwanParams()->mSLTurnSpeed.get();
	mPosition   = BW_HEAD_START;
	reset();

	mLeash  = new TBWLeash(this, 15);
	mPicket = new TBWPicket(this);
	mPicket->mPosition = BW_PICKET_START;
	unk17C             = 1;
	unk184             = 0;

	goToRandomNextGraphNode();
	initHitActor(0x800000B, 1, 0x80000000, 0.0f, 0.0f, 0.0f, 0.0f);

	mHit[0] = new TBWHit(this, 3);
	mHit[0]->initHitActor(0x800000B, 3, 0xA0000000, 500.0f, 500.0f, 450.0f,
	                      500.0f);
	mHit[1] = new TBWHit(this, -1);
	mHit[1]->initHitActor(0x800000B, 3, 0xA0000000, 300.0f, 500.0f, 270.0f,
	                      500.0f);

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	for (int i = 0; i < 2; ++i) {
		group->getChildren().push_back(mHit[i]);
		mHit[i]->offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	initAnmSound();
	mScaledBodyRadius = 500.0f;

	mMtxCalc = new TBossWanwanMtxCalc(this);
	mMActor->setCalcForBck(mMtxCalc);
	mMActor->calc();
	offLiveFlag(LIVE_FLAG_UNK100);

	changeBck(4);
	mMActor->setBrkFromIndex(0);
	mMActor->setLightType(1);

	J3DModel* model = mMActor->getModel();
	if (model->getSkinDeform() == nullptr)
		model->setSkinDeform(new J3DSkinDeform, J3D_DEFORM_ATTACH_FLAG_UNK_1);

	mHitPoints = getBossWanwanParams()->mSLBWHitPointMax.get();
	unk15C.zero();

	mMapCollisionManager = new TMapCollisionManager(1, "/scene/bwanwan", this);
	mMapCollisionManager->init("bwanwan_ofuro_col.col", 2, nullptr);
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);
	if (mMapCollisionManager->getUnk8() != nullptr)
		mMapCollisionManager->getUnk8()->remove();
}

void TBossWanwan::shakeCamera(int mode)
{
	if (!SMS_IsMarioTouchGround4cm())
		return;

	f32 distance = bwSqrtf(mDistToMarioSquared);
	f32 lengthMax    = getBossWanwanParams()->mSLShakeLengthMax.get();
	f32 lengthMaxHP0 = getBossWanwanParams()->mSLShakeLengthMaxHP0.get();

	f32 ratio;
	if (mMActor->checkCurBckFromIndex(0)) {
		ratio = 1.0f;
	} else {
		ratio = static_cast<f32>(mHitPoints)
		        / static_cast<f32>(
		            getBossWanwanParams()->mSLBWHitPointMax.get());
	}

	f32 length    = lengthMax * ratio + lengthMaxHP0 * (1.0f - ratio);
	f32 remaining = length - distance;
	if (remaining < 0.0f)
		return;

	length = remaining / length;
	if (length > 1.0f)
		length = 1.0f;
	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(mode),
	                          length * ratio);
	SMSRumbleMgr->start(8, &mPosition);
}

BOOL TBossWanwan::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x20];
	if (sender->getActorType() == 0x80000001)
		return false;

	if (sender->getActorType() == 0x1000001) {
		if (unk18C)
			return true;

		gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                             0, nullptr);

		if (mHitPoints == 0) {
			SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_COOL_MORE,
			                                &mPosition, 0, nullptr, 0, 4);
		} else if (mHitPoints == 1) {
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xB0, getModel()->getAnmMtx(1), 0, nullptr);
			SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_TO_COOL,
			                                &mPosition, 0, nullptr, 0, 4);
		} else {
			SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_COOL, &mPosition,
			                                0, nullptr, 0, 4);
		}

		decHitPoints();
		unk190++;
		return true;
	}

	if (sender->isActorType(0x4000005A)) {
		sender->receiveMessage(this, 1);
		mHitPoints = 0;
		unk190++;
		if (unk1A0 == 0)
			unk1A0++;

		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xB0, getModel()->getAnmMtx(1), 0, nullptr);
		SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_TO_COOL, &mPosition, 0,
		                                nullptr, 0, 4);
	}

	return TSpineEnemy::receiveMessage(sender, message);
}

void TBossWanwan::calcRootMatrix()
{
	getModel()->setBaseScale(mScaling);
	MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x,
	               500.0f + mPosition.y, mPosition.z, mRotation.x, mRotation.y,
	               mRotation.z);
}

void TBossWanwan::slideToCurPathNode(f32 slideSpeed, f32 turnSpeed)
{
	JGeometry::TVec3<f32> toNode = getUnkF4().getPoint();
	toNode -= mPosition;

	f32 dist  = PSVECMag(&toNode);
	f32 angle = MsWrap(MsGetRotFromZaxisY(toNode), 0.0f, 360.0f);
	f32 diff  = MsAngleDiff(angle, mRotation.y);

	if (diff > 0.0f)
		diff = diff > turnSpeed ? turnSpeed : diff;
	else
		diff = diff > -turnSpeed ? diff : -turnSpeed;

	mRotation.y = MsWrap(mRotation.y + diff, 0.0f, 360.0f);

	JGeometry::TVec3<f32> vel = mLinearVelocity;
	if (dist > 0.0f)
		toNode.scale(slideSpeed / dist);
	vel += toNode;
	mLinearVelocity = vel;
	char trash[0x10];
}

void TBossWanwan::control()
{
	TLiveActor::control();

	// The chain pulls the boss while it is stretched or Mario holds the stake
	// and the pull vector is at least the pull limit.
	if (unk17C != 0
	    || (mPicket->isTaken()
	        && (unk15C.squared() < getBossWanwanParams()->mSLPullLimit.get()
	                ? FALSE
	                : TRUE))) {
		mLinearVelocity += unk15C;

		JGeometry::TVec3<f32> toHead = mPosition;
		toHead -= mLeash->mRope->mPoints[3].unkC;
		f32 angle = MsWrap(MsGetRotFromZaxisY(toHead), 0.0f, 360.0f);
		f32 diff  = MsAngleDiff(angle, mRotation.y);

		f32 turn;
		if (diff > 0.0f)
			turn = diff > 4.0f * mTurnSpeed ? 4.0f * mTurnSpeed : diff;
		else
			turn = diff > 4.0f * -mTurnSpeed ? diff : 4.0f * -mTurnSpeed;

		mRotation.y = MsWrap(mRotation.y + turn, 0.0f, 360.0f);
	}

	unk15C.zero();
	updateSquareToMario();
	char trash[0x40];
}

void TBossWanwan::emitEffects()
{
	char trash[0x68];
	BOOL hit = FALSE;
	if (mMActor->checkCurBckFromIndex(4) || mMActor->checkCurBckFromIndex(5)) {
		if (mMActor->checkBckPass(8.0f))
			hit = TRUE;
	} else if (mMActor->checkCurBckFromIndex(2)) {
		if (mMActor->checkBckPass(38.0f))
			hit = TRUE;
	}

	if (hit) {
		gpMarioParticleManager->emit(0xAD, &mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xAE, &mPosition, 0, nullptr);
		if (mHitPoints == 0) {
			SMSGetMSound()->startSoundActor(MSD_SE_M_BOSW_CHAIN_CAL_F,
			                                &mLeash->mRope->mPoints[0].unkC, 0,
			                                nullptr, 0, 4);
			SMSGetMSound()->startSoundActor(MSD_SE_M_BOSW_CHAIN_CAL_R,
			                                &mPicket->mPosition, 0, nullptr, 0,
			                                4);
		} else {
			SMSGetMSound()->startSoundActor(MSD_SE_M_BOSW_CHAIN_ANG_F,
			                                &mLeash->mRope->mPoints[0].unkC, 0,
			                                nullptr, 0, 4);
			SMSGetMSound()->startSoundActor(MSD_SE_M_BOSW_CHAIN_ANG_R,
			                                &mPicket->mPosition, 0, nullptr, 0,
			                                4);
		}
	}

	hit = FALSE;
	if (mMActor->checkCurBckFromIndex(0)) {
		if (mMActor->checkBckPass(72.0f))
			hit = TRUE;
	} else if (mMActor->checkCurBckFromIndex(4)
	           || mMActor->checkCurBckFromIndex(5)) {
		if (mMActor->checkBckPass(6.0f) || mMActor->checkBckPass(12.0f))
			hit = TRUE;
	} else if (mMActor->checkCurBckFromIndex(2)) {
		if (mMActor->checkBckPass(4.0f))
			hit = TRUE;
	}

	if (hit) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0xAF, getModel()->getAnmMtx(1), 0, this);
	}

	if (mMActor->checkCurBckFromIndex(4) || mMActor->checkCurBckFromIndex(5)) {
		if (mMActor->checkBckPass(10.0f))
			shakeCamera(0x16);
	}

	if (mMActor->checkCurBckFromIndex(0)) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
		if (ctrl->checkPass(60.0f) || ctrl->checkPass(127.0f)) {
			shakeCamera(0x16);
			gpMarioParticleManager->emit(0xAD, &mPosition, 0, nullptr);
			gpMarioParticleManager->emit(0xAE, &mPosition, 0, nullptr);
		}
		if (ctrl->checkPass(202.0f)) {
			shakeCamera(0x17);
			gpMarioParticleManager->emit(0xAD, &mPosition, 0, nullptr);
			gpMarioParticleManager->emit(0xAE, &mPosition, 0, nullptr);
		}
	}

	if (mMActor->checkCurBckFromIndex(2)) {
		if (mMActor->checkBckPass(40.0f))
			shakeCamera(0x16);
	}

	if (mHitPoints != 0) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x1EE, getModel()->getAnmMtx(1), 3, this);
	}

	if (unk190 != 0 && mHitPoints != 0) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x167, getModel()->getAnmMtx(1), 1, this);
		unk190 = 0;
	}
}

void TBossWanwan::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x98];
	if (unk18C) {
		mHit[0]->perform(cue, graphics);
		TSpineEnemy::perform(cue, graphics);

		if (cue & CUE_MOVE)
			mMtxCalc->advanceMotionBlend(-unk178);

		if (cue & CUE_ENTRY) {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(5);
			if (ctrl != nullptr && ctrl->getFrame() > 0.5f * ctrl->getEnd()) {
				unk1A4 = mPosition;
				unk1A4.y += 500.0f;
				gpMarioParticleManager->emitAndBindToPosPtr(
				    BWANWAN_JPA_MS_BWAN_KIRA, &unk1A4, 1, this);
			}
		}
		return;
	}

	if (cue & CUE_CALC_ANIM) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(5);
		f32 ratio = (f32)mHitPoints
		            / (f32)getBossWanwanParams()->mSLBWHitPointMax.get();
		ctrl->setFrame(ratio * (ctrl->getEnd() - 1));
		ctrl->setRate(0.0f);

		if (mHitPoints == 0
		    || mHitPoints == getBossWanwanParams()->mSLBWHitPointMax.get())
			mLeash->mNodes[0]->mTemperature = ratio;

		emitEffects();
	}

	mHit[0]->perform(cue, graphics);

	if (cue & CUE_MOVE) {
		mHit[1]->mPosition.set(mHit[0]->mPosition.x,
		                       mHit[0]->mPosition.y - 500.0f,
		                       mHit[0]->mPosition.z);
	}
	mHit[1]->perform(cue, graphics);

	if ((cue & CUE_MOVE) && unk194 == 0) {
		unk19C++;
		if (unk19C < 0)
			unk19C = 1;
		if (unk19C > 0x258) {
			if (!(unk198 & 1))
				gpMarDirector->mConsole->startAppearBalloon(0xE001A, true);
			unk198 |= 1;
		}
	}

	if ((cue & CUE_MOVE) && unk1A0 == 0
	    && gpMarDirector->mMoveTickCount >= 0x7080) {
		if (!(unk198 & 8))
			gpMarDirector->mConsole->startAppearBalloon(0xE001D, true);
		unk198 |= 8;
	}

	if (cue & CUE_MOVE) {
		const TNerveBase<TLiveActor>* dieNerve = &TNerveBWDie::theNerve();
		if (mSpine->getLatestNerve() != dieNerve) {
			const TNerveBase<TLiveActor>* bathNerve
			    = &TNerveBWJumpToBath::theNerve();
			if (mSpine->getLatestNerve() != bathNerve) {
				if (mHitPoints != 0) {
					if (unk17C != 0 && unk194 == 0) {
						unk184++;
						if (unk184 > 0x258) {
							const TNerveBase<TLiveActor>* shake
							    = &TNerveBWShake::theNerve();
							mSpine->pushNerve(shake);
							unk184 = 0;
						}
					}
				} else {
					unk184 = 0;
				}

				if (mHitPoints == 0
				    && mSpine->getLatestNerve() != &TNerveBWBark::theNerve()) {
					unk180++;
					if (unk180 > 0x960) {
						if (mSpine->getLatestNerve()
						    != &TNerveBWBark::theNerve())
							mSpine->setNext(&TNerveBWBark::theNerve());
					}
				} else {
					if (gpMarDirector->mMoveTickCount % 20 == 0) {
						if (mHitPoints
						    < getBossWanwanParams()->mSLBWHitPointMax.get())
							mHitPoints++;
					}
					unk180 = 0;
				}
			}
		}
	}

	if (cue & CUE_MOVE) {
		const TNerveBase<TLiveActor>* wanderNerve
		    = &TNerveBWGraphWander::theNerve();
		if (mSpine->getLatestNerve() == wanderNerve && mPicket->isTaken()) {
			f32 dragLength = unk15C.length();
			SMSGetMSound()->startSoundActorWithInfo(
			    MSD_SE_BS_WANWAN_M_DRAG, &mLeash->mRope->mPoints[6].unkC,
			    nullptr, dragLength, 0, 0, nullptr, 0, 4);
		}
	}

	TSpineEnemy::perform(cue, graphics);
	mLeash->testPerform(cue, graphics);
	mPicket->testPerform(cue, graphics);

	if (cue & CUE_MOVE)
		mMtxCalc->advanceMotionBlend(-unk178);
}

TBossWanwanManager::TBossWanwanManager(const char* name)
    : TEnemyManager(name)
{
}

TSpineEnemy* TBossWanwanManager::createEnemyInstance()
{
	return new TBossWanwan("ボスワンワン");
}

void TBossWanwanManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "bwanwan_body.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (2 << J3DMLF_TevStageNumShift),
		  0 },
		{ "bwanwan_chain.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (2 << J3DMLF_TevStageNumShift),
		  0 },
		{ "bwanwan_picket.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_UseUniqueMaterials
		      | (2 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TBossWanwanManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TBWParams("/enemy/bosswanwan.prm");
	TEnemyManager::load(stream);

	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_rock.jpa", 0xAD);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_jump_smoke.jpa", 0xAE);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_downyuge.jpa", 0xB0);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hibana.jpa", 0xAF);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_deadyuge.jpa", 0xB1);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_yugami.jpa", 0x1EE);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_hityuge.jpa", 0x167);
	SMS_LoadParticle("/scene/bwanwan/jpa/ms_bwan_kira.jpa",
	                 BWANWAN_JPA_MS_BWAN_KIRA);
}

DEFINE_NERVE(TNerveBWGraphWander, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	if (spine->getTime() == 0) {
		boss->unk16C = 0;
		if (!boss->mMActor->checkCurBckFromIndex(4)
		    && !boss->mMActor->checkCurBckFromIndex(4)) {
			if (boss->getHitPoints() == 0)
				boss->changeBck(5);
			else
				boss->changeBck(4);
			J3DFrameCtrl* ctrl = boss->mMActor->getFrameCtrl(ANM_TYPE_BCK);
			ctrl->setFrame(0.0f);
			ctrl->setRate(SMSGetAnmFrameRate());
		}
		boss->mMActor->setBtpFromIndex(0);
		J3DFrameCtrl* ctrl = boss->mMActor->getFrameCtrl(ANM_TYPE_BTP);
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}

	if (boss->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)
	    && boss->getHitPoints() == 0
	    && !boss->mMActor->checkCurBckFromIndex(5)) {
		boss->changeBck(5);
		J3DFrameCtrl* ctrl = boss->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setFrame(0.0f);
		ctrl->setRate(SMSGetAnmFrameRate());
	}

	if (boss->unk17C == 0 && boss->mPicket->isTaken()
	    && isPulledOverLimit(boss)) {
		TGraphWeb* graph = boss->unk124->unk0;
		int prevIdx      = boss->unk124->mPrevIdx;
		JGeometry::TVec3<f32> toPrev;
		graph->getGraphNode(prevIdx).getPoint(&toPrev);
		toPrev -= boss->mPosition;
		if (VECMag(&toPrev) < 400.0f && prevIdx == graph->unk10) {
			spine->pushAfterCurrent(&TNerveBWJumpToBath::theNerve());
			return true;
		}

		if (VECMag(&toPrev) < 400.0f) {
			JGeometry::TVec3<f32> toMario = *gpMarioPos;
			toMario -= boss->mPosition;
			int prev = boss->unk124->mPrevIdx;
			int next = boss->unk124->unk0->getAimToDirNextIndex(
			    prev, boss->unk124->mCurrIdx, toMario, boss->mPosition, -1);
			boss->unk124->mPrevIdx = next;
			boss->unk124->mCurrIdx = prev;
			boss->setGoalPathFromGraph();
			boss->unk128 = 0;
			boss->unk12C = 0.0f;
		}

		boss->slideToCurPathNode(
		    3.0f
		        * ((f32)boss->getHitPoints()
		           / (f32)boss->getBossWanwanParams()->mSLBWHitPointMax.get()
		           * boss->getMarchSpeed()),
		    boss->getTurnSpeed());
		return false;
	}

	if (boss->isReachedToGoal()) {
		if (boss->jumpToNextGraphNode() >= 0) {
			spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
			spine->pushAfterCurrent(&TNerveBWJump::theNerve());
		} else {
			spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
			TGraphWeb* graph = boss->unk124->unk0;
			int prevIdx      = boss->unk124->mPrevIdx;
			int curIdx       = boss->unk124->mCurrIdx;
			if (prevIdx >= 0
			    && graph->getGraphNode(prevIdx).getRailNode()->mConnectionNum
			           >= 2
			    && gpMarDirector->mMoveTickCount >= 0x3840) {
				if ((boss->unk198 & 4) == 0)
					gpMarDirector->getConsole()->startAppearBalloon(0xE001C,
					                                               true);
				boss->unk198 |= 4;
			}

			boss->unk124->moveTo(graph->getEscapeDirLimited(
			    curIdx, prevIdx, polarXZ(boss->mRotation.y, 1.0f),
			    boss->mPosition, 100.0f, -1));
			boss->setGoalPathFromGraph();
			boss->unk128 = 0;
			boss->unk12C = 0.0f;
		}
		return true;
	}

	boss->unk16C = 0;
	f32 ratio = (f32)boss->getHitPoints()
	            / (f32)boss->getBossWanwanParams()->mSLBWHitPointMax.get();
	if (boss->mPicket->isTaken()) {
		JGeometry::TVec3<f32> dir
		    = polarXZ(gpMarioOriginal->mIntendedYaw * 0.005493164f, 1.0f);
		JGeometry::TVec3<f32> toBoss = boss->mPosition;
		toBoss -= gpMarioOriginal->mPosition;
		VECNormalize(&toBoss, &toBoss);
		f32 along = gpMarioOriginal->mIntendedMag * 0.03125f * 0.75;
		f32 pulled = along * -toBoss.dot(dir);
		f32 scale  = 1.0f - pulled;
		if (scale < 0.0f)
			scale = 0.0f;
		else if (1.5f < scale)
			scale = 1.5f;
		boss->slideToCurPathNode(scale * (ratio * boss->getMarchSpeed()),
		                         boss->getTurnSpeed());
	} else {
		boss->slideToCurPathNode(ratio * boss->getMarchSpeed() + 0.2f,
		                         boss->getTurnSpeed());
	}
	return false;
}

DEFINE_NERVE(TNerveBWRoll, TLiveActor)
{
	char trash[0xd0];
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());

	if (spine->getTime() == 0) {
		J3DFrameCtrl* ctrl = boss->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
		boss->unk16C = 1;
	}

	if (boss->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		J3DFrameCtrl* ctrl = boss->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(SMSGetAnmFrameRate());
		return true;
	}

	boss->walkToCurPathNode(
	    boss->getBossWanwanParams()->mSLAttackSpeed.get(), boss->mTurnSpeed,
	    0.0f);
	return false;
}

DEFINE_NERVE(TNerveBWBark, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());

	if (spine->getTime() == 0) {
		boss->changeBck(0);
		boss->unk16C = 0;
		boss->unk168 = 0.0f;
		if (boss->unk194 == 0) {
			if (boss->unk17C) {
				JPABaseEmitter* emitter = gpMarioParticleManager->emit(
				    0xAE, &boss->mPicket->mPosition, 0, nullptr);
				if (emitter) {
					emitter->setGlobalScale(JGeometry::TVec3<f32>(0.3f, 0.5f, 0.3f));
				}
			}
			boss->unk194 = 0;
			boss->unk17C = 0;
			gpMSound->startSoundActor(0x2966, &boss->mPicket->mPosition, 0, nullptr, 0, 4);
		}
	}

	if (spine->getTime() == 0x118)
		boss->mHitPoints = boss->getBossWanwanParams()->mSLBWHitPointMax.get();

	if (boss->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		if (!(boss->unk198 & 2))
			gpMarDirector->mConsole->startAppearBalloon(0xE001B, true);
		boss->unk198 |= 2;
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBWJump, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& target = boss->getUnk104().getPoint();
		f32 speed                           = boss->unk124->unkC;
		boss->mVelocity
		    = boss->calcVelocityToJumpToY(target, speed, boss->getGravityY());
		boss->onLiveFlag(LIVE_FLAG_AIRBORNE);
		boss->unk16C = 0;
	}

	if (boss->isReachedToGoal()) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return true;
	}

	boss->walkToCurPathNode(0.0f, boss->getTurnSpeed(), 0.0f);
	return false;
}

DEFINE_NERVE(TNerveBWStun, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());

	if (boss->mPicket->isTaken() && isPulledOverLimit(boss)) {
		TGraphTracer* tracer = boss->unk124;
		int prevIdx          = tracer->mPrevIdx;
		TGraphWeb* graph     = tracer->unk0;

		JGeometry::TVec3<f32> nodePos;
		graph->getGraphNode(prevIdx).getPoint(&nodePos);
		nodePos -= boss->mPosition;
		if (PSVECMag(&nodePos) < 100.0f) {
			if (prevIdx == graph->unk10) {
				spine->pushAfterCurrent(&TNerveBWJumpToBath::theNerve());
				return true;
			}

			TGraphTracer* tracer2 = boss->unk124;
			int prevIdx2          = tracer2->mPrevIdx;
			int idx = tracer2->unk0->getAimToDirNextIndex(
			    prevIdx2, tracer2->mCurrIdx, *gpMarioPos - boss->mPosition,
			    boss->mPosition, -1);
			boss->unk124->mPrevIdx = idx;
			boss->unk124->mCurrIdx = prevIdx2;
			boss->setGoalPathFromGraph();
			boss->unk128 = 0;
			boss->unk12C = 0.0f;
		}
		return false;
	}

	if (spine->getTime() > boss->getBossWanwanParams()->mSLStunTimer.get()) {
		boss->mHitPoints
		    = boss->getBossWanwanParams()->mSLBWHitPointMax.get();
		spine->pushAfterCurrent(&TNerveBWWakeup::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBWWakeup, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());

	if (spine->getTime() == 0) {
		boss->changeBck(6);
		boss->mMActor->setBtpFromIndex(2);
		boss->unk16C = 0;
		boss->unk168 = 0.0f;
	}

	if (boss->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBWJumpToBath, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> vel
		    = boss->calcVelocityToJumpToY(BW_BATH_POS, 10.0f, boss->getGravityY());
		boss->setGoalPath(TPathNode(BW_BATH_POS));
		boss->mVelocity = vel;
		boss->onLiveFlag(LIVE_FLAG_AIRBORNE);
		boss->unk16C = 0;
	}

	if (spine->getTime() > 120 && !boss->unk195
	    && boss->mPosition.y < BW_BATH_POS.y + 10.0f) {
		TEffectColumWater* eff = static_cast<TEffectColumWater*>(
		    gpConductor->makeOneEnemyAppear(
		        boss->mPosition, "エフェクト水柱マネージャー", 1));
		if (eff) {
			JGeometry::TVec3<f32> scale;
			scale.x = 5.0f;
			scale.y = 5.0f;
			scale.z = 5.0f;
			JGeometry::TVec3<f32> pos;
			pos.x = boss->mPosition.x;
			pos.y = boss->mPosition.y + 500.0f - 30.0f;
			pos.z = boss->mPosition.z;
			eff->generate(pos, scale);
			boss->unk195 = 1;
		}
		SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_DIVE,
		                                &boss->mPosition, 0, nullptr, 0, 4);
	}

	JGeometry::TVec3<f32> goal = BW_BATH_POS;
	if ((goal - boss->mPosition).squared() < 10000.0f
	    && boss->mPosition.y <= BW_BATH_POS.y) {
		boss->mPosition = goal;
		spine->pushAfterCurrent(&TNerveBWDie::theNerve());
		return true;
	}

	boss->walkToCurPathNode(0.0f, boss->getTurnSpeed(), 0.0f);
	return false;
}

DEFINE_NERVE(TNerveBWDie, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	MActor* actor     = boss->mMActor;
	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> zero;
		zero.zero();
		boss->mVelocity       = zero;
		boss->mLinearVelocity = zero;
		boss->onLiveFlag(LIVE_FLAG_UNK10);
		boss->offLiveFlag(LIVE_FLAG_AIRBORNE);
		gpMarioParticleManager->emit(0xB1, &boss->mPosition, 0, nullptr);
	}

	if (boss->getHitPoints() != 0) {
		boss->decHitPoints();
		J3DFrameCtrl* ctrl = actor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
		spine->pushAfterCurrent(&TNerveBWDie::theNerve());
		return true;
	}

	if (spine->getTime() == 0) {
		SMSGetMarDirector()->fireStartDemoCamera(
		    "bwanwan_down_camera", nullptr, -1, 0.0f, true, nullptr, 0, nullptr,
		    JDrama::TFlagT<u16>(0));
		boss->unk16C = 0;
		boss->unk168 = 0.0f;
		boss->unk18C = 1;
		boss->mPosition = BW_BATH_POS;

		JGeometry::TVec3<f32> pos = boss->mPosition;
		pos.y += 500.0f;
		JGeometry::TVec3<f32> scale = boss->mScaling;
		scale.scale(1.1f);
		boss->mMapCollisionManager->setUpUnk8TRS(pos, boss->mRotation, scale);

		boss->mHit[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
		boss->mHit[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
		TBWLeash* leash = boss->mLeash;
		int i           = 0;
		while (i < leash->mRope->mNumPoints) {
			leash->mNodes[i]->onHitFlag(HIT_FLAG_NO_COLLISION);
			i++;
		}
		boss->mPicket->onHitFlag(HIT_FLAG_NO_COLLISION);

		boss->changeBck(1);
		actor->setBtpFromIndex(0);
		actor->setBrkFromIndex(1);
	}

	if (spine->getTime() > 60 && !boss->unk18D
	    && gpMarDirector->unk124 != 3) {
		gpItemManager->makeShineAppearWithDemo("シャイン（ボス用）",
		                                       "ボスシャインカメラ",
		                                       boss->mPosition.x,
		                                       boss->mPosition.y,
		                                       boss->mPosition.z);
		boss->unk18D = 1;
	}

	if (actor->curAnmEndsNext(ANM_TYPE_BRK, nullptr))
		actor->getFrameCtrl(ANM_TYPE_BRK)->setRate(0.0f);
	return false;
}

DEFINE_NERVE(TNerveBWJumpAway, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());

	if (spine->getTime() == 0) {
		JGeometry::TVec3<f32> velocity = boss->calcVelocityToJumpToY(
		    BW_HEAD_START, 40.0f, boss->getGravityY());
		boss->setGoalPath(TPathNode(BW_HEAD_START));
		boss->mVelocity = velocity;
		boss->onLiveFlag(LIVE_FLAG_AIRBORNE);
		boss->unk16C = 0;
	}

	if (boss->unk188) {
		spine->pushAfterCurrent(&TNerveBWFall::theNerve());
		return true;
	}

	if (boss->isReachedToGoal()) {
		boss->mPosition = BW_HEAD_START;
		boss->unk124->reset();
		boss->unk124->reset2();
		boss->goToShortestNextGraphNode();
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return true;
	}

	boss->walkToCurPathNode(0.0f, boss->mTurnSpeed, 0.0f);
	return false;
}

DEFINE_NERVE(TNerveBWShake, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	MActor* actor     = boss->mMActor;

	if (spine->getTime() == 0)
		boss->changeBck(2);

	if (actor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		if (boss->unk17C) {
			JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			    0xAE, &boss->mPicket->mPosition, 0, nullptr);
			if (emitter) {
				JGeometry::TVec3<f32> scale(0.3f, 0.5f, 0.3f);
				emitter->setGlobalScale(scale);
			}
		}
		boss->unk194 = 0;
		boss->unk17C = 0;
		gpMSound->startSoundActor(0x2967, &boss->mPicket->mPosition, 0, nullptr, 0, 4);
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBWFall, TLiveActor)
{
	TBossWanwan* boss = static_cast<TBossWanwan*>(spine->getBody());
	if (spine->getTime() == 0) {
		const JGeometry::TVec3<f32>& target = boss->mPicket->mPosition;
		JGeometry::TVec3<f32> vel
		    = boss->calcVelocityToJumpToY(target, 5.0f, boss->getGravityY());
		boss->setGoalPath(TPathNode(boss->mPicket->mPosition));
		boss->mVelocity = vel;
		boss->onLiveFlag(LIVE_FLAG_AIRBORNE);
		boss->unk16C = 0;
	}

	if (boss->isReachedToGoal()) {
		boss->mPosition = boss->mPicket->mPosition;
		boss->getTracer()->reset();
		boss->getTracer()->reset2();
		boss->goToShortestNextGraphNode();
		spine->pushAfterCurrent(&TNerveBWGraphWander::theNerve());
		return true;
	}
	return false;
}

