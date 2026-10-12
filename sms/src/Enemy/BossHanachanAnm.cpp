#include <Enemy/BossHanachan.hpp>
#include <Camera/cameralib.hpp>
#include <M3DUtil/MActor.hpp>
#include <NPC/NpcInbetween.hpp>
#include <Strategic/Spine.hpp>

static inline void setForcedBlendRatioAll_(TBossHanachan* boss, f32 ratio)
{
	boss->mHeadPart->mInbetween->mForcedBlendRatio = ratio;
	for (int i = 0; i < 8; i++)
		boss->mBodyParts[i]->mInbetween->mForcedBlendRatio = ratio;
}

static inline void copyFrameAll_(TBossHanachan* boss)
{
	boss->mHeadPart->copyFrameFromOldAnmToNewAnm_();
	for (int i = 0; i < 8; i++)
		boss->mBodyParts[i]->copyFrameFromOldAnmToNewAnm_();
}

void TBossHanachan::changeAnmRateAndFrameUpdate_()
{
	char trash[0x58];
	bool applyRate = true;
	f32 rate       = SMSGetAnmFrameRate();

	if (mSpine->getLatestNerve() == &TNerveBossHanachanTumble::theNerve()) {
		setForcedBlendRatioAll_(this, 0.0f);

		mHeadPart->changeTumbleAnmRate_();
		for (int i = 0; i < 8; i++)
			mBodyParts[i]->changeTumbleAnmRate_();
		applyRate = false;
	} else {
		switch (mHeadPart->mAnmKind) {
		case BOSSHANACHAN_ANM_KIND_WALK:
		case BOSSHANACHAN_ANM_KIND_RUN: {
			if (mMarchSpeed <= mChangeSaveParams->mSLWalkAnmMarchSpeed.get()) {
				setForcedBlendRatioAll_(this, 0.0f);

				switch (mHeadPart->mAnmKind) {
				case BOSSHANACHAN_ANM_KIND_RUN:
					setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK,
					                  BOSSHANACHAN_BLEND_OFF);
					copyFrameAll_(this);
					break;
				case BOSSHANACHAN_ANM_KIND_WALK:
					break;
				default:
					setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK,
					                  BOSSHANACHAN_BLEND_ON);
					break;
				}
			} else if (mMarchSpeed
			           >= mChangeSaveParams->mSLRunAnmMarchSpeed.get()) {
				setForcedBlendRatioAll_(this, 0.0f);

				switch (mHeadPart->mAnmKind) {
				case BOSSHANACHAN_ANM_KIND_RUN:
					break;
				case BOSSHANACHAN_ANM_KIND_WALK:
					setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_RUN,
					                  BOSSHANACHAN_BLEND_OFF);
					copyFrameAll_(this);
					break;
				default:
					setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_RUN,
					                  BOSSHANACHAN_BLEND_ON);
					break;
				}
			} else {
				f32 blend = CLBCalcRatio<f32>(
				    mChangeSaveParams->mSLWalkAnmMarchSpeed.get(),
				    mChangeSaveParams->mSLRunAnmMarchSpeed.get(), mMarchSpeed);
				switch (mHeadPart->mAnmKind) {
				case BOSSHANACHAN_ANM_KIND_WALK:
					if (mHeadPart->mPrevAnmKind != BOSSHANACHAN_ANM_KIND_RUN) {
						setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_RUN,
						                  BOSSHANACHAN_BLEND_OFF);
						copyFrameAll_(this);
						blend = 1.0f - blend;
					}
					setForcedBlendRatioAll_(this, blend);
					break;
				case BOSSHANACHAN_ANM_KIND_RUN:
					if (mHeadPart->mPrevAnmKind == BOSSHANACHAN_ANM_KIND_WALK) {
						blend = 1.0f - blend;
					} else {
						setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK,
						                  BOSSHANACHAN_BLEND_OFF);
						copyFrameAll_(this);
					}
					setForcedBlendRatioAll_(this, blend);
					break;
				default:
					setForcedBlendRatioAll_(this, 0.0f);
					setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK,
					                  BOSSHANACHAN_BLEND_ON);
					break;
				}
			}

			rate = mMarchSpeed * SMSGetAnmFrameRate()
			       * mChangeSaveParams->mSLWalkBckRateMagnif.get();
			if (rate < mChangeSaveParams->mSLWalkBckRateMin.get())
				rate = mChangeSaveParams->mSLWalkBckRateMin.get();
			break;
		}
		default:
			setForcedBlendRatioAll_(this, 0.0f);
			rate = SMSGetAnmFrameRate();
			break;
		}
	}

	MActor* headActor = mHeadPart->mMActor;
	if (applyRate)
		headActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(rate);
	mHeadPart->updateAnmSound();
	headActor->frameUpdate();

	for (int i = 0; i < 8; i++) {
		MActor* bodyActor = mBodyParts[i]->mMActor;
		if (applyRate)
			bodyActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(rate);
		mBodyParts[i]->updateAnmSound();
		bodyActor->frameUpdate();
	}
}

static inline bool isPartAnmEnd_(const TBossHanachanPartsBase* part,
                                 EnumBossHanachanAnmKind kind)
{
	bool end = false;
	if (part->mAnmKind == kind && part->isCurBckAlreadyEnd_())
		end = true;
	return end;
}

bool TBossHanachan::isAllBckAlreadyEnd(EnumBossHanachanAnmKind kind) const
{
	bool result = true;
	if (!isPartAnmEnd_(mHeadPart, kind)) {
		result = false;
	} else {
		for (int i = 0; i < 8; i++) {
			if (!isPartAnmEnd_(mBodyParts[i], kind)) {
				result = false;
				break;
			}
		}
	}
	return result;
}

bool TBossHanachan::isFinishedGetUp() const
{
	bool result = false;
	switch (mHeadPart->mAnmKind) {
	case BOSSHANACHAN_ANM_KIND_GETUP_L3:
	case BOSSHANACHAN_ANM_KIND_GETUP_R3:
		if (mHeadPart->isCurBckAlreadyEnd_())
			result = true;
		break;
	}
	return result;
}

void TBossHanachan::considerSetAnm(EnumBossHanachanNerveAnm nerveAnm)
{
	mHeadPart->considerSetAnm_(nerveAnm);
	for (int i = 0; i < 8; i++)
		mBodyParts[i]->considerSetAnm_(nerveAnm);
}

void TBossHanachan::setAnmTimerWhenDead()
{
	u8 diff = mChangeSaveParams->mSLDeadFrameDiff.get();
	for (int i = 0; i < 8; i++)
		mBodyParts[i]->mAnmTimer = diff * CLBAbs(mWeakBodyIndex - i);
	mHeadPart->mAnmTimer = diff * CLBAbs(mWeakBodyIndex - -1);
}

void TBossHanachan::setAnmTimerWhenDamage()
{
	u8 diff = mChangeSaveParams->mSLDamageFrameDiff.get();
	for (int i = 0; i < 8; i++)
		mBodyParts[i]->mAnmTimer = diff * CLBAbs(mWeakBodyIndex - i);
	mHeadPart->mAnmTimer = diff * CLBAbs(mWeakBodyIndex - -1);
}

void TBossHanachan::setAnmTimerWhenSnort()
{
	u8 diff              = mChangeSaveParams->mSLSnortFrameDiff.get();
	mHeadPart->mAnmTimer = 0;
	for (int i = 0; i < 8; i++)
		mBodyParts[i]->mAnmTimer = diff * (i + 1);
}

void TBossHanachan::setAnmTimerWhenGetUp()
{
	u8 diff = mChangeSaveParams->mSLGetUpFrameDiff.get();
	for (int i = 0; i < 8; i++)
		mBodyParts[7 - i]->mAnmTimer = diff * i;
	mHeadPart->mAnmTimer = diff * 8;
}

static inline void setTumbleAnmRate_(const TBossHanachan* boss,
                                     TBossHanachanPartsBase* part)
{
	J3DFrameCtrl* ctrl = part->mMActor->getFrameCtrl(ANM_TYPE_BCK);
	f32 frames         = (1.0f / boss->mTumbleAngularSpeedZ)
	             * CLBAbs(boss->mTumbleTargetAngleZ - part->mRotation.z);
	ctrl->setRate((1.0f / frames) * (2.0f * (40.0f * SMSGetAnmFrameRate())));
}

void TBossHanachan::setTumbleAnm(EnumBossHanachanStopMotionBlendOnOff blend)
{
	EnumBossHanachanAnmKind kind;
	if (179.0f == mTumbleTargetAngleZ) {
		kind = BOSSHANACHAN_ANM_KIND_TUMBLE_R;
	} else if (-179.0f == mTumbleTargetAngleZ) {
		kind = BOSSHANACHAN_ANM_KIND_TUMBLE_L;
	} else {
		return;
	}

	mHeadPart->setAnm_(kind, blend);
	setTumbleAnmRate_(this, mHeadPart);

	for (int i = 0; i < 8; i++) {
		mBodyParts[i]->setAnm_(kind, blend);
		setTumbleAnmRate_(this, mBodyParts[i]);
	}
}

void TBossHanachan::setHeadAndBodyAnm(EnumBossHanachanAnmKind kind,
                                      EnumBossHanachanStopMotionBlendOnOff blend)
{
	mHeadPart->setAnm_(kind, blend);
	for (int i = 0; i < 8; i++) {
		TBossHanachanPartsBody* body = mBodyParts[i];
		if (body->setAnm_(kind, blend)) {
			J3DFrameCtrl* ctrl = body->mMActor->getFrameCtrl(ANM_TYPE_BCK);
			f32 frame
			    = (i * mChangeSaveParams->mSLNormalBckFrameDiff.get())
			      % ctrl->getEnd();
			ctrl->setFrame(frame);

			ctrl = body->mMActor->getFrameCtrl(ANM_TYPE_BTP);
			if (ctrl)
				ctrl->setFrame(frame);

			ctrl = body->mMActor->getFrameCtrl(ANM_TYPE_BTK);
			if (ctrl)
				ctrl->setFrame(frame);
		}
	}
}
