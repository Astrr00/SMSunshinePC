#include <Enemy/BossHanachan.hpp>
#include <Camera/CameraShake.hpp>
#include <Camera/SunMgr.hpp>
#include <Camera/cameralib.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/Spine.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const f32 sSnortStepFrames[3]   = { 21.0f, 36.0f, 55.0f };
static const f32 sEmitSandFrameFoot[2] = { 14.0f, 34.0f };

// MathUtil.hpp's MsSqrtf is marked dont_inline, but this unit has it expanded.
static inline f32 sqrtf1(f32 x)
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

// Same for MsClamp(t, 0.0f, 1.0f).
static inline f32 clamp01(f32 t)
{
	if (t > 1.0f)
		t = 1.0f;
	else if (t < 0.0f)
		t = 0.0f;
	return t;
}

void TBossHanachan::emitCamShake_()
{
	if (gpMarDirector->mState == TMarDirector::STATE_UNK1)
		return;

	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	J3DFrameCtrl* frameCtrl
	    = mBodyParts[0]->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
	bool onGround = SMS_IsMarioTouchGround4cm();

	if (nerve == &TNerveBossHanachanGraphWander::theNerve()) {
		f32 dist = sqrtf1(mDistToMarioSquared);

		f32 shakeScale;
		if (dist <= mCommonSaveParams->mSLCamShakeMaxDist.get()) {
			shakeScale = 1.0f;
		} else if (dist >= mCommonSaveParams->mSLCamShakeZeroDist.get()) {
			shakeScale = 0.0f;
		} else {
			shakeScale = clamp01(CLBCalcRatio<f32>(
			    mCommonSaveParams->mSLCamShakeZeroDist.get(),
			    mCommonSaveParams->mSLCamShakeMaxDist.get(), dist));
		}

		for (int i = 0; i < 2; ++i) {
			if (frameCtrl->checkPass(sEmitSandFrameFoot[i])) {
				gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0xb),
				                          shakeScale);
				if (onGround) {
					for (int j = 0; j < 8; ++j)
						SMSRumbleMgr->start(8, &mBodyParts[j]->mJointPos);
				}
			}
		}
	} else if (nerve == &TNerveBossHanachanSnort::theNerve()) {
		for (int i = 0; i < 3; ++i) {
			if (frameCtrl->checkPass(sSnortStepFrames[i])) {
				gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0xa),
				                          1.0f);
				if (onGround)
					SMSRumbleMgr->start(8, static_cast<f32*>(nullptr));
			}
		}
	} else if (nerve == &TNerveBossHanachanGetUp::theNerve()) {
		switch (mBodyParts[0]->mAnmKind) {
		case BOSSHANACHAN_ANM_KIND_GETUP_R3:
		case BOSSHANACHAN_ANM_KIND_GETUP_L3:
			if (frameCtrl->checkPass(9.0f)) {
				gpCameraShake->startShake(static_cast<EnumCamShakeMode>(0xc),
				                          1.0f);
				if (onGround)
					SMSRumbleMgr->start(8, static_cast<f32*>(nullptr));
				if (checkLiveFlag(LIVE_FLAG_UNK20000))
					MSBgm::stopTrackBGM(1, 30);
			}
			break;
		}
	}
}

void TBossHanachan::emitOneTimeSandPillar_(TBossHanachanPartsBody* body)
{
	onLiveFlag(LIVE_FLAG_UNK10000);

	MtxPtr bodyMtx = body->mJointMtx;
	mSandPillarPos.set(bodyMtx[0][3], mPosition.y, bodyMtx[2][3]);

	const TLiveActor* sand = body->getSandActor_();
	if (sand) {
		mSandPillarPos.x = 0.5f * (mSandPillarPos.x + sand->mPosition.x);
		mSandPillarPos.z = 0.5f * (mSandPillarPos.z + sand->mPosition.z);
		mSandPillarPos.y = sand->mPosition.y;
	}

	gpMarioParticleManager->emit(COLUMSAND_JPA_MS_BOHA_CRASH_A, &mSandPillarPos,
	                             0, nullptr);

	MtxPtr pillarMtx = mSandPillarActor->getModel()->getBaseTRMtx();
	pillarMtx[0][3]  = mSandPillarPos.x;
	pillarMtx[1][3]  = mSandPillarPos.y;
	pillarMtx[2][3]  = mSandPillarPos.z;
	mSandPillarActor->setBckFromIndex(0x25);
	mSandPillarActor->setBtkFromIndex(2);
	mSandPillarActor->setBrkFromIndex(2);

	gpCameraShake->startShake(static_cast<EnumCamShakeMode>(8), 1.0f);
	gpMSound->startSoundActor(MSD_SE_BS_HANA_TUMBLE, &mSandPillarPos, 0,
	                          nullptr, 0, 4);
}

void TBossHanachan::emitParticle_()
{
	const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
	if (nerve == &TNerveBossHanachanDead::theNerve())
		return;

	f32 waterHeight = gpSunMgr->unk20;

	// nose steam
	if (nerve != &TNerveBossHanachanSnort::theNerve()
	    && mHeadPart->mHitActor->mWaterHitCounter > 0) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x169, mHeadPart->mNoseHallMtx[1], 1, mHeadPart);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16b, mHeadPart->mNoseHallMtx[0], 1, mHeadPart);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16a, mHeadPart->mNoseHallMtx[1], 1, mHeadPart);
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16c, mHeadPart->mNoseHallMtx[0], 1, mHeadPart);
	}

	// splash around the sphere link points above the water
	if (nerve == &TNerveBossHanachanTumble::theNerve()
	    || (nerve == &TNerveBossHanachanDamage::theNerve()
	        && mMarchSpeed > 0.001f)) {
		for (int i = 0; i < 8; ++i) {
			JGeometry::TVec3<f32> pos = mSphereLink->mPoints[i].mPos;
			if (pos.y > waterHeight)
				gpMarioParticleManager->emitAndBindToPosPtr(
				    0x16e, &mBodyParts[i]->mJointPos, 1, mBodyParts[i]);
		}
		for (int i = 0; i < 8; ++i) {
			JGeometry::TVec3<f32> pos = mSphereLink->mPoints[i].mPos;
			if (pos.y > waterHeight)
				gpMarioParticleManager->emitAndBindToPosPtr(
				    0x16d, &mBodyParts[i]->mJointPos, 1, mBodyParts[i]);
		}
	}

	if (nerve == &TNerveBossHanachanGraphWander::theNerve()) {
		// sand/water kicked up by the feet
		for (int i = 0; i < 8; ++i) {
			J3DFrameCtrl* frameCtrl
			    = mBodyParts[i]->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
			for (int j = 0; j < 2; ++j) {
				if (!frameCtrl->checkPass(sEmitSandFrameFoot[j]))
					continue;

				MtxPtr footMtx = mBodyParts[i]->mFootHitActors[j]->mJointMtx;
				if (MsRandF()
				    < mChangeSaveParams->mSLParticleProbability.get()) {
					if (footMtx[1][3] < waterHeight) {
						MtxPtr legMtx = mBodyParts[i]->mLegMtx[j];
						JGeometry::TVec3<f32> pos(legMtx[0][3], waterHeight,
						                          legMtx[2][3]);
						gpMarioParticleManager->emit(0x7c, &pos, 0, nullptr);
						gpMarioParticleManager->emit(0x7d, &pos, 0, nullptr);
					} else {
						JGeometry::TVec3<f32> pos(footMtx[0][3], footMtx[1][3],
						                          footMtx[2][3]);
						gpMarioParticleManager->emit(0x77, &pos, 0, nullptr);
						gpMarioParticleManager->emit(0x76, &pos, 0, nullptr);
					}
				}
			}
		}
	} else if (nerve == &TNerveBossHanachanSnort::theNerve()) {
		if (mHeadPart->getMActor()
		        ->getFrameCtrl(ANM_TYPE_BCK)
		        ->checkPass(134.0f)) {
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x78, mHeadPart->mNoseHallMtx[1], 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x7a, mHeadPart->mNoseHallMtx[0], 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x79, mHeadPart->mNoseHallMtx[1], 0, nullptr);
			gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x7b, mHeadPart->mNoseHallMtx[0], 0, nullptr);
		}
	} else if (nerve == &TNerveBossHanachanDown::theNerve()) {
		gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x16f, mHeadPart->mJointMtx, 1, mHeadPart);
	}
}

void TBossHanachan::staticLoadParticle()
{
	SMS_LoadParticle("ms_boha_sandsmo.jpa", 0x76);
	SMS_LoadParticle("ms_boha_sand.jpa", 0x77);
	SMS_LoadParticle("ms_boha_jouki_r_a.jpa", 0x78);
	SMS_LoadParticle("ms_boha_jouki_r_b.jpa", 0x79);
	SMS_LoadParticle("ms_boha_jouki_l_a.jpa", 0x7a);
	SMS_LoadParticle("ms_boha_jouki_l_b.jpa", 0x7b);
	SMS_LoadParticle("ms_boha_hamon_a.jpa", 0x7c);
	SMS_LoadParticle("ms_boha_hamon_b.jpa", 0x7d);
	SMS_LoadParticle("ms_boha_crash_a.jpa", COLUMSAND_JPA_MS_BOHA_CRASH_A);
	SMS_LoadParticle("ms_boha_jouki2_r_a.jpa", 0x169);
	SMS_LoadParticle("ms_boha_jouki2_r_b.jpa", 0x16a);
	SMS_LoadParticle("ms_boha_jouki2_l_a.jpa", 0x16b);
	SMS_LoadParticle("ms_boha_jouki2_l_b.jpa", 0x16c);
	SMS_LoadParticle("ms_boha_sandsmo_sl.jpa", 0x16d);
	SMS_LoadParticle("ms_boha_sand_sl.jpa", 0x16e);
	SMS_LoadParticle("ms_boha_kizetsu.jpa", 0x16f);
}
