#include <Enemy/BossHanachan.hpp>
#include <Enemy/BossHanachanParts.hpp>
#include <Camera/cameralib.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorData.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <NPC/NpcInbetween.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/MarDirector.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

const char* cMapCollisionJointName     = "center";
const char* cBodyMapCollisionFileName  = "/scene/bosshanachan/hanabody_col.col";
const char* cHeadMapCollisionFileName  = "/scene/bosshanachan/hanahead_col.col";
const char* cLegJointName_L3           = "leg_L3";
const char* cLegJointName_R3           = "leg_R3";
const char* cNoseHallJointName_L       = "L_hall";
const char* cNoseHallJointName_R       = "R_hall";

TBossHanachanPartsBase::TBossHanachanPartsBase(TBossHanachan* owner, u32 type,
                                               int mode, const char* name)
    : TLiveActor(name)
    , mAnmKind(BOSSHANACHAN_ANM_KIND_NONE)
    , mPrevAnmKind(BOSSHANACHAN_ANM_KIND_NONE)
    , mOwner(owner)
    , mHitActor(nullptr)
    , mMapCollision(nullptr)
    , mJointMtx(nullptr)
    , mAnmTimer(0)
    , mInbetween(nullptr)
{
	mMActorKeeper = mOwner->getActorKeeper();
	mMActor       = mMActorKeeper->createMActorFromNthData(mode, 0);
	mMActor->initNormalMotionBlend();
	initHitActor(type, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	switch (type) {
	case 0x8000015:
		mScaledBodyRadius = mOwner->mCommonSaveParams->mSLBodyShadowSize.get();
		break;
	case 0x8000014:
		mScaledBodyRadius = mOwner->mCommonSaveParams->mSLHeadShadowSize.get();
		break;
	}

	onLiveFlag(LIVE_FLAG_UNK8);
	initAnmSound();
	mMActor->setLightType(1);
	mInbetween = new TNpcInbetween(
	    1, CLBPalFrame<s16>(mOwner->mCommonSaveParams->mSLMotionBlendFrames.get()));
}

TBossHanachanPartsBody::TBossHanachanPartsBody(TBossHanachan* owner,
                                               const char* name)
    : TBossHanachanPartsBase(owner, 0x8000015, 0, name)
    , mIndex(0)
    , mSandTiltRotZ(0.0f)
    , mLastPos(0.0f, 0.0f, 0.0f)
    , mLastLastPos(0.0f, 0.0f, 0.0f)
    , mLastRotZ(0.0f)
    , mLastLastRotZ(0.0f)
    , mRotZSpeed(0.0f)
    , mLastRotZSpeed(0.0f)
    , mJointPos(0.0f, 0.0f, 0.0f)
{
	J3DModel* model        = getModel();
	JUTNameTab* jointNames = model->getModelData()->getJointName();
	mLegMtx[0] = model->getAnmMtx((u16)jointNames->getIndex(cLegJointName_L3));
	mLegMtx[1] = model->getAnmMtx((u16)jointNames->getIndex(cLegJointName_R3));
}

TBossHanachanPartsHead::TBossHanachanPartsHead(TBossHanachan* owner,
                                               const char* name)
    : TBossHanachanPartsBase(owner, 0x8000014, 1, name)
{
	J3DModel* model        = getModel();
	JUTNameTab* jointNames = model->getModelData()->getJointName();
	mNoseHallMtx[0]
	    = model->getAnmMtx((u16)jointNames->getIndex(cNoseHallJointName_L));
	mNoseHallMtx[1]
	    = model->getAnmMtx((u16)jointNames->getIndex(cNoseHallJointName_R));
}

void TBossHanachanPartsBase::initMapCollisionAndHitActor_(TIdxGroupObj* group)
{
	TBossHanachanCommonSaveParams* params = mOwner->mCommonSaveParams;

	const char* colFile = cBodyMapCollisionFileName;
	f32 attackRadius    = params->mSLBodyAttackRadius.get();
	f32 attackHeight    = params->mSLBodyAttackHeight.get();
	f32 damageRadius    = params->mSLBodyDamageRadius.get();
	f32 damageHeight    = params->mSLBodyDamageHeight.get();
	f32 hitOffsetY      = params->mSLBodyHitOffsetY.get();
	switch (mActorType) {
	case 0x8000015:
		break;
	case 0x8000014:
		colFile      = cHeadMapCollisionFileName;
		attackRadius = params->mSLHeadAttackRadius.get();
		attackHeight = params->mSLHeadAttackHeight.get();
		damageRadius = params->mSLHeadDamageRadius.get();
		damageHeight = params->mSLHeadDamageHeight.get();
		hitOffsetY   = params->mSLHeadHitOffsetY.get();
		break;
	}

	u16 jointIndex = mMActor->getModel()
	                     ->getModelData()
	                     ->getJointName()
	                     ->getIndex(cMapCollisionJointName);
	mJointMtx = mMActor->getModel()->getAnmMtx(jointIndex);

	mMapCollision = new TMapCollisionMove;
	mMapCollision->init(colFile, 0x8000, this);

	mHitActor = new TWaterHitActor("ボスハナチャンのパーツ");
	mHitActor->initHitActor(mActorType, 1, 0x80000000, attackRadius,
	                        attackHeight, damageRadius, damageHeight);
	group->getChildren().push_back(mHitActor);
	mHitActor->offHitFlag(HIT_FLAG_NO_COLLISION);
	mHitActor->mPosition.set(mJointMtx[0][3], mJointMtx[1][3] - hitOffsetY,
	                         mJointMtx[2][3]);
}

void TBossHanachanPartsBody::initFootHitActor_(TIdxGroupObj* group)
{
	static const char* sFootJointName[2] = { "foot_L", "foot_R" };

	JUTNameTab* jointNames = mMActor->getModel()->getModelData()->getJointName();
	TBossHanachanCommonSaveParams* params = mOwner->mCommonSaveParams;

	int jointIndex[2];
	for (int i = 0; i < 2; ++i) {
		jointIndex[i]    = jointNames->getIndex(sFootJointName[i]);
		mFootHitActors[i] = new TFootHitActor("ボスハナチャンの足");
		mFootHitActors[i]->initHitActor(
		    mActorType, 1, 0x80000000, params->mSLFootAttackRadius.get(),
		    params->mSLFootAttackHeight.get(),
		    params->mSLFootDamageRadius.get(),
		    params->mSLFootDamageHeight.get());
		group->getChildren().push_back(mFootHitActors[i]);
		mFootHitActors[i]->offHitFlag(HIT_FLAG_NO_COLLISION);

		MtxPtr mtx = mMActor->getModel()->getAnmMtx((u16)jointIndex[i]);
		mFootHitActors[i]->mJointMtx = mtx;
		mFootHitActors[i]->mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);
	}
}

void TBossHanachanPartsBase::changeTumbleAnmRate_()
{
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	switch (mAnmKind) {
	case BOSSHANACHAN_ANM_KIND_TUMBLE_L:
	case BOSSHANACHAN_ANM_KIND_TUMBLE_R: {
		f32 frame = ctrl->getFrame();
		if (frame <= 40.0f)
			break;
		f32 remain = ctrl->getEnd() - frame;
		f32 rate   = ctrl->getRate();
		char trash[8]; // unused stack slot (present in the retail frame)
		CLBChaseConstantSpecifyFrame(&rate, SMSGetAnmFrameRate(), remain);
		ctrl->setRate(rate);
		break;
	}
	default:
		ctrl->setRate(SMSGetAnmFrameRate());
		break;
	}
}

void TBossHanachanPartsBase::moveMapCollision_()
{
	JGeometry::TVec3<f32> pos(mJointMtx[0][3], mJointMtx[1][3],
	                          mJointMtx[2][3]);
	mMapCollision->moveTrans(pos);
}

void TBossHanachanPartsBase::entryCircleShadow_()
{
	if (mOwner->mSpine->getCurrentNerve() != &TNerveBossHanachanDead::theNerve()
	    || mOwner->mSpine->getTime() <= 200) {
		TCircleShadowRequest request;
		request.mPosition.set(mJointMtx[0][3], mJointMtx[1][3],
		                      mJointMtx[2][3]);
		request.mRadiusX = request.mRadiusZ = mScaledBodyRadius;
		gpBindShadowManager->forceRequest(request, mActorType);
	}
}

void TBossHanachanPartsBase::setDamageFog_(JDrama::TGraphics* graphics)
{
	bool notHead = true;
	if (mActorType == 0x8000014)
		notHead = false;

	J3DModelData* modelData = mMActor->getModel()->getModelData();
	JGeometry::TVec3<f32> pos(mJointMtx[0][3], mJointMtx[1][3],
	                          mJointMtx[2][3]);
	u16 materialNum = modelData->getMaterialNum();

	if (mOwner->mSpine->getLatestNerve()
	    == &TNerveBossHanachanDamage::theNerve()) {
		SMS_AddDamageFogEffect(modelData, pos, graphics);
		if (notHead) {
			for (u16 i = 0; i < materialNum; ++i)
				modelData->getMaterialNodePointer(i)->change();
		}
		if (mAnmTimer == 0)
			mMActor->getModel()->unlock();
	} else {
		SMS_ResetDamageFogEffect(modelData);
	}
}

bool TBossHanachanPartsBase::isCurBckAlreadyEnd_() const
{
	char trash[8]; // unused stack slot (present in the retail frame)
	bool result = true;
	if (mMActor) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		if (ctrl) {
			result = ctrl->checkState(J3DFrameCtrl::STATE_COMPLETED_ONCE)
			         || ctrl->checkState(J3DFrameCtrl::STATE_LOOPED_ONCE)
			         || ctrl->getFrame() + 0.1f >= ctrl->getEnd();
		}
	}
	return result;
}

void TBossHanachanPartsBase::copyFrameFromOldAnmToNewAnm_()
{
	J3DAnmTransformKey* anm = mMActor->getBckAnm();
	J3DFrameCtrl* ctrl      = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	if (anm && ctrl) {
		f32 frame = mMActor->getBckOldMotionBlendFrame();
		anm->setFrame(frame);
		ctrl->setFrame(frame);
	}
}

const TLiveActor* TBossHanachanPartsBase::getSandActor_() const
{
	const TLiveActor* actor = SMS_GetGroundActor(mGroundPlane, 0x400000CD);
	if (!actor)
		actor = SMS_GetGroundActor(mGroundPlane, 0x400000CB);
	return actor;
}

void TBossHanachanPartsBase::calcRotateZWhenGetUp_()
{
	if (mAnmTimer == 0) {
		switch (mAnmKind) {
		case BOSSHANACHAN_ANM_KIND_GETUP_L2:
		case BOSSHANACHAN_ANM_KIND_GETUP_R2: {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
			f32 frames = 2.0f * (ctrl->getEnd() - ctrl->getFrame());
			if (frames < 0.001f)
				mRotation.z = 0.0f;
			else
				CLBChaseConstantSpecifyFrame(&mRotation.z, 0.0f, frames);
			break;
		}
		}
	}
}

void TBossHanachanPartsBase::considerSetAnm_(EnumBossHanachanNerveAnm nerveAnm)
{
	if (nerveAnm == BOSSHANACHAN_NERVE_ANM_TUMBLE) {
		switch (mAnmKind) {
		case BOSSHANACHAN_ANM_KIND_JUMP_REACTION:
		case BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION:
		case BOSSHANACHAN_ANM_KIND_HIPDROP_LONG:
		case BOSSHANACHAN_ANM_KIND_TUMBLE_L:
		case BOSSHANACHAN_ANM_KIND_TUMBLE_R:
			if (isCurBckAlreadyEnd_())
				setAnm_(BOSSHANACHAN_ANM_KIND_PIKUPIKU,
				        BOSSHANACHAN_BLEND_OFF);
			break;
		}
	} else if (nerveAnm == BOSSHANACHAN_NERVE_ANM_DOWN) {
		bool marioOn = false;
		if (SMS_IsMarioTouchGround4cm()) {
			const TBGCheckData* plane = *gpMarioGroundPlane;
			if (plane && plane->mActor == this)
				marioOn = true;
		}

		switch (mAnmKind) {
		case BOSSHANACHAN_ANM_KIND_JUMP_REACTION:
		case BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION:
		case BOSSHANACHAN_ANM_KIND_HIPDROP_LONG:
		case BOSSHANACHAN_ANM_KIND_TUMBLE_L:
		case BOSSHANACHAN_ANM_KIND_TUMBLE_R:
			if (isCurBckAlreadyEnd_()) {
				if (mActorType == 0x8000015 && marioOn)
					setAnm_(BOSSHANACHAN_ANM_KIND_JITABATA,
					        BOSSHANACHAN_BLEND_ON);
				else
					setAnm_(BOSSHANACHAN_ANM_KIND_PIKUPIKU,
					        BOSSHANACHAN_BLEND_OFF);
			}
			break;
		default:
			if (mActorType == 0x8000015) {
				bool blending = true;
				if (!mInbetween->isMotionBlending()) {
					if (!mInbetween->isForcedBlendRatio())
						blending = false;
				}
				if (!blending) {
					if (mAnmKind == BOSSHANACHAN_ANM_KIND_JITABATA) {
						if (!marioOn)
							setAnm_(BOSSHANACHAN_ANM_KIND_PIKUPIKU,
							        BOSSHANACHAN_BLEND_ON);
					} else if (marioOn) {
						setAnm_(BOSSHANACHAN_ANM_KIND_JITABATA,
						        BOSSHANACHAN_BLEND_ON);
					}
				}
			}
			break;
		}
	} else if (nerveAnm == BOSSHANACHAN_NERVE_ANM_GETUP) {
		if (mAnmTimer > 0)
			mAnmTimer--;
		if (mAnmTimer == 0) {
			bool end = isCurBckAlreadyEnd_();
			switch (mAnmKind) {
			case BOSSHANACHAN_ANM_KIND_GETUP_L1:
				if (end)
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_L2,
					        BOSSHANACHAN_BLEND_OFF);
				break;
			case BOSSHANACHAN_ANM_KIND_GETUP_L2:
				if (end)
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_L3,
					        BOSSHANACHAN_BLEND_OFF);
				break;
			case BOSSHANACHAN_ANM_KIND_GETUP_R1:
				if (end)
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_R2,
					        BOSSHANACHAN_BLEND_OFF);
				break;
			case BOSSHANACHAN_ANM_KIND_GETUP_R2:
				if (end)
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_R3,
					        BOSSHANACHAN_BLEND_OFF);
				break;
			case BOSSHANACHAN_ANM_KIND_GETUP_L3:
			case BOSSHANACHAN_ANM_KIND_GETUP_R3:
				break;
			default:
				if (mRotation.z < 0.0f)
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_L1,
					        BOSSHANACHAN_BLEND_ON);
				else
					setAnm_(BOSSHANACHAN_ANM_KIND_GETUP_R1,
					        BOSSHANACHAN_BLEND_ON);
				break;
			}
		}
	} else if (nerveAnm == BOSSHANACHAN_NERVE_ANM_DAMAGE) {
		if (mAnmTimer > 0) {
			mAnmTimer--;
			if (mAnmTimer == 0)
				setAnm_(BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION,
				        BOSSHANACHAN_BLEND_OFF);
		} else if (mAnmKind == BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION) {
			if (isCurBckAlreadyEnd_())
				setAnm_(BOSSHANACHAN_ANM_KIND_DAMAGE, BOSSHANACHAN_BLEND_OFF);
		}
	} else if (nerveAnm == BOSSHANACHAN_NERVE_ANM_SNORT) {
		if (mAnmTimer > 0)
			mAnmTimer--;
		if (mAnmTimer == 0) {
			if (mAnmKind != BOSSHANACHAN_ANM_KIND_SNORT)
				setAnm_(BOSSHANACHAN_ANM_KIND_SNORT, BOSSHANACHAN_BLEND_ON);
		}
	} else if (nerveAnm == BOSSHANACHAN_NERVE_ANM_DEAD) {
		if (mAnmTimer > 0)
			mAnmTimer--;
		if (mAnmTimer == 0)
			setAnm_(BOSSHANACHAN_ANM_KIND_DEAD, BOSSHANACHAN_BLEND_OFF);
	}
}

bool TBossHanachanPartsBody::setAnm_(
    EnumBossHanachanAnmKind kind, EnumBossHanachanStopMotionBlendOnOff blend)
{
	static const int sBodyBckIndex[18] = {
		0x13, 0xF, 0xA, 0xD, 0, 0xC, 9, 2, 3, 4, 5, 6, 7, 8, 0x10, 1, 0x11, 0x12,
	};

	bool changed = false;
	if (mAnmKind != kind) {
		mPrevAnmKind = mAnmKind;
		mAnmKind     = kind;

		if (sBodyBckIndex[kind] != mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
			int bck = sBodyBckIndex[kind];
			if (mIndex == mOwner->mWeakBodyIndex) {
				switch (kind) {
				case BOSSHANACHAN_ANM_KIND_JITABATA:
					bck = 0xB;
					break;
				case BOSSHANACHAN_ANM_KIND_PIKUPIKU:
					bck = 0xE;
					break;
				}
			}
			mMActor->setBckFromIndex(bck);
			changed = true;
			if (blend == BOSSHANACHAN_BLEND_ON)
				mInbetween->startMotionBlend();
			else
				mInbetween->stopMotionBlend();
			setCurAnmSound();
		}

		if (kind == BOSSHANACHAN_ANM_KIND_DEAD) {
			mMActor->setBrkFromIndex(0);
			mMActor->getFrameCtrl(ANM_TYPE_BRK)->setAttribute(0);
			mMActor->getModel()->unlock();
		}
	}
	return changed;
}

bool TBossHanachanPartsHead::setAnm_(
    EnumBossHanachanAnmKind kind, EnumBossHanachanStopMotionBlendOnOff blend)
{
	static const int sHeadBckIndex[18] = {
		0x24, 0x20, 0x1D, 0x1F, 0x14, 0x1E, 0x1C, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x21, 0x15, 0x22, 0x23,
	};
	static const int sHeadBtpIndex[18] = {
		0, 0, 1, 2, 1, 2, 2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 1, 1,
	};
	static const int sHeadBtkIndex[18] = {
		0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	};

	bool changed = false;
	if (mAnmKind != kind) {
		mPrevAnmKind = mAnmKind;
		mAnmKind     = kind;

		if (sHeadBckIndex[kind] != mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
			mMActor->setBckFromIndex(sHeadBckIndex[kind]);
			changed = true;
			if (blend == BOSSHANACHAN_BLEND_ON)
				mInbetween->startMotionBlend();
			else
				mInbetween->stopMotionBlend();
			setCurAnmSound();
		}

		if (sHeadBtpIndex[kind] != mMActor->getCurAnmIdx(ANM_TYPE_BTP))
			mMActor->setBtpFromIndex(sHeadBtpIndex[kind]);

		if (sHeadBtkIndex[kind] != mMActor->getCurAnmIdx(ANM_TYPE_BTK))
			mMActor->setBtkFromIndex(sHeadBtkIndex[kind]);

		if (kind == BOSSHANACHAN_ANM_KIND_DEAD) {
			mMActor->setBrkFromIndex(1);
			mMActor->getFrameCtrl(ANM_TYPE_BRK)->setAttribute(0);
		}
	}
	return changed;
}

// Shared by both receiveMessage functions.
static inline bool isRotZFallen(const TBossHanachanPartsBase* self)
{
	return self->mRotation.z == -179.0f || self->mRotation.z == 179.0f;
}
static inline bool isRotZFallenChecked(const TBossHanachanPartsBase* self)
{
	if (isRotZFallen(self))
		return true;
	return false;
}

static inline bool isReceivable(const TBossHanachanPartsBase* self)
{
	bool enabled = false;
	const TNerveBase<TLiveActor>* nerve
	    = self->mOwner->mSpine->getLatestNerve();
	if (nerve == &TNerveBossHanachanTumble::theNerve()
	    || nerve == &TNerveBossHanachanDown::theNerve()) {
		if (self->mActorType == 0x8000015) {
			if (isRotZFallenChecked(self))
				enabled = true;
		} else {
			enabled = true;
		}
	}
	return enabled;
}

BOOL TBossHanachanPartsBody::receiveMessage(THitActor* sender, u32 message)
{
	if (gpMarDirector->isThing())
		return FALSE;

	bool result = false;

	if (isReceivable(this)) {
		switch (message) {
		case HIT_MESSAGE_TRAMPLE:
			if (mAnmKind == BOSSHANACHAN_ANM_KIND_JUMP_REACTION)
				mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
			else
				setAnm_(BOSSHANACHAN_ANM_KIND_JUMP_REACTION,
				        BOSSHANACHAN_BLEND_OFF);
			result = true;
			break;
		case HIT_MESSAGE_HIP_DROP: {
			bool isWeak = mIndex == mOwner->mWeakBodyIndex ? true : false;
			switch (mAnmKind) {
			case BOSSHANACHAN_ANM_KIND_JITABATA:
			case BOSSHANACHAN_ANM_KIND_PIKUPIKU:
			case BOSSHANACHAN_ANM_KIND_JUMP_REACTION:
			case BOSSHANACHAN_ANM_KIND_HIPDROP_LONG:
			case BOSSHANACHAN_ANM_KIND_TUMBLE_L:
			case BOSSHANACHAN_ANM_KIND_TUMBLE_R:
				if (isWeak) {
					setAnm_(BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION,
					        BOSSHANACHAN_BLEND_OFF);
					mOwner->execDamage();
				} else if (mAnmKind == BOSSHANACHAN_ANM_KIND_HIPDROP_LONG) {
					mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
				} else {
					setAnm_(BOSSHANACHAN_ANM_KIND_HIPDROP_LONG,
					        BOSSHANACHAN_BLEND_OFF);
				}
				result = true;
				break;
			}
			break;
		}
		}
	}

	return result;
}

BOOL TBossHanachanPartsHead::receiveMessage(THitActor* sender, u32 message)
{
	if (gpMarDirector->isThing())
		return FALSE;

	bool result = false;

	if (isReceivable(this)) {
		switch (message) {
		case HIT_MESSAGE_TRAMPLE:
			if (mAnmKind == BOSSHANACHAN_ANM_KIND_JUMP_REACTION)
				mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(0.0f);
			else
				setAnm_(BOSSHANACHAN_ANM_KIND_JUMP_REACTION,
				        BOSSHANACHAN_BLEND_OFF);
			result = true;
			break;
		case HIT_MESSAGE_HIP_DROP:
			setAnm_(BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION,
			        BOSSHANACHAN_BLEND_OFF);
			mHitActor->onWaterHitCounter();
			result = true;
			break;
		}
	}

	return result;
}
