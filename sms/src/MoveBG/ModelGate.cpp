#include <MoveBG/ModelGate.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <Camera/Camera.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JDrama/JDRViewObjPtrList.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/MActorUtil.hpp>
#include <M3DUtil/SampleCtrlModel.hpp>
#include <M3DUtil/SampleCtrlNode.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/LiveActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <THPPlayer/THPPlayer.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>

static const char* gateMActorNames[] = {
	"05_gate01",      "05_gate02rico", "05_gate03manma",
	"05_gate04monte", "05_gate05mare",
};

void TModelGate::perform(u32 param_1, JDrama::TGraphics* param_2)
{
	char trash[156];
	if (!(mFlags & 1))
		return;

	if ((param_1 & 8) && ActivePlayer.open != 0
	    && ActivePlayer.dispTextureSet != nullptr) {
		THPTextureSet* frame = ActivePlayer.dispTextureSet;
		J3DTexture* texture  = mActor->getModel()->getModelData()->getTexture();
		ResTIMG* timg        = texture->getResTIMG(0);
		timg->imageDataOffset = (u32)(PTR_U32(frame->ytexture) - PTR_U32(timg));
		timg                  = texture->getResTIMG(1);
		timg->imageDataOffset = (u32)(PTR_U32(frame->utexture) - PTR_U32(timg));
		timg                  = texture->getResTIMG(2);
		timg->imageDataOffset = (u32)(PTR_U32(frame->vtexture) - PTR_U32(timg));
	}

	mActor->perform(param_1, param_2);

	if (param_1 & 1) {
		if (!(mFlags & 2)) {
			if (JGeometry::TVec3<f32>(*gpMarioPos - mPosition).length()
			    < 1000.0f) {
				mOpenRatio += 0.01f;
				if (mOpenRatio > 1.0f) {
					mOpenRatio = 1.0f;
					mOpenTimer = mOpenDuration;
				}
			} else {
				mOpenTimer -= 1;
				if (mOpenTimer <= 0)
					mOpenTimer = 0;
				mOpenRatio = (f32)mOpenTimer / 1000.0f;
			}
		}

		JGeometry::TVec3<f32> local;
		MTXMultVec(mInvCenterMtx, gpMarioPos, &local);

		if (-mEnterHalfWidth < local.x && local.x < mEnterHalfWidth
		    && mEnterMinY < local.y && local.y < mEnterMaxY
		    && mEnterMinZ < local.z && local.z < mEnterMaxZ) {
			if (mOpenTimer > 0) {
				bool jumping = false;
				if (SMS_IsMarioStatusTypeJumping())
					jumping = true;
				if (jumping == true) {
					if (SMS_GetMarioLiveActor()->receiveMessage(
					        this, HIT_MESSAGE_TAKE)
					    == 1)
						mHeldObject = SMS_GetMarioLiveActor();
				}
			} else {
				f32 dz   = gpMarioPos->z - mPosition.z;
				f32 dx   = gpMarioPos->x - mPosition.x;
				f32 dist = std::sqrtf(dx * dx + dz * dz);
				if (dist < mPushRadius) {
					JGeometry::TVec3<f32> request = *gpMarioPos;
					request.x += 10.0f * (dx / dist);
					request.z += 10.0f * (dz / dist);
					SMS_MarioMoveRequest(request);
				}
			}
		}
	}

	if (param_1 & 2) {
		switch (mState) {
		case 0:
			if (mActor->getFrameCtrl(ANM_TYPE_BPK)->checkState(3))
				mState = 1;
			break;
		case 1:
			if (mOpenTimer > 0) {
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x131, mActor->getModel()->getAnmMtx(mCenterJointIdx), 1,
				    this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x132, mActor->getModel()->getAnmMtx(mCenterJointIdx), 1,
				    this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x133, mActor->getModel()->getAnmMtx(mCenterJointIdx), 1,
				    this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x134, mActor->getModel()->getAnmMtx(mCenterJointIdx), 1,
				    this);
				mOpenTimer -= 1;
				mOpenElapsed += 1;
				gpMSound->startSoundActor(MSD_SE_OBJ_GATE_LIGHT, &mPosition, 0,
				                          nullptr, 0, 4);
				gpMSound->startSoundActor(MSD_SE_OBJ_GATE_WAVE, &mPosition, 0,
				                          nullptr, 0, 4);
			} else {
				mOpenTimer   = 0;
				mOpenElapsed = 0;
				mOpenRatio -= mOpenRatioDecOpen;
				if (mOpenRatio < 0.0f)
					mOpenRatio = 0.0f;
			}
			break;
		default:
			mOpenRatio -= mOpenRatioDecClose;
			break;
		}

		if (mOpenRatio > 1.0f)
			mOpenRatio = 1.0f;
		if (mOpenRatio < 0.0f)
			mOpenRatio = 0.0f;

		J3DTevBlock* tevBlock = mActor->getModel()
		                            ->getModelData()
		                            ->getMaterialNodePointer(0)
		                            ->getTevBlock();
		if (mTevAnimMode == 1) {
			mPrevTevAnimStep = mTevAnimStep;
			mTevAnimTimer -= 1;
			if (mTevAnimTimer == 0) {
				mTevAnimStep += 1;
				mTevAnimTimer = mTevAnimInterval;
				if (mTevAnimStep >= 8)
					mTevAnimStep = 0;
			}

			SampleCtrlMaterial* mat = mSampleCtrlData->mMaterials[0];
			J3DTevStageInfo* stage2 = &mat->unk3C[2];
			J3DTevStageInfo* stage3 = &mat->unk3C[3];
			J3DTevStageInfo* stage5 = &mat->unk3C[5];
			u8& s0f5  = mat->unk3C[0].field_0x5;
			u8& s2f5  = stage2->field_0x5;
			u8& s3f5  = stage3->field_0x5;
			u8& s3f6  = stage3->field_0x6;
			u8& s3f7  = stage3->field_0x7;
			u8& s3f8  = stage3->field_0x8;
			u8& s5f11 = stage5->field_0x11;
			mat->unk3C[0].field_0x5  = 0;
			mat->unk3C[2].field_0x5  = 0;
			mat->unk3C[3].field_0x5  = 0;
			mat->unk3C[3].field_0x6  = 0;
			mat->unk3C[3].field_0x7  = 0;
			mat->unk3C[3].field_0x8  = 1;
			mat->unk3C[5].field_0x11 = 1;

			switch (mTevAnimStep) {
			case 1:
				s3f5 = 1;
				s3f8 = 0;
				break;
			case 2:
				s0f5 = 8;
				break;
			case 3:
				s3f5 = 8;
				break;
			case 4:
				s3f7 = 1;
				break;
			case 5:
				s5f11 = 0;
				break;
			case 6:
				s2f5 = 1;
				break;
			case 7:
				s3f5 = 0;
				s3f6 = 1;
				s3f8 = 0;
				break;
			}

			tevBlock->getTevStage(0)->setTevStageInfo(mat->unk3C[0]);
			tevBlock->getTevStage(2)->setTevStageInfo(*stage2);
			tevBlock->getTevStage(3)->setTevStageInfo(*stage3);
			tevBlock->getTevStage(5)->setTevStageInfo(*stage5);
		}

		f32 frame = mOpenRatio * mActor->getFrameCtrl(ANM_TYPE_BRK)->getEnd();
		mActor->getFrameCtrl(ANM_TYPE_BRK)->setFrame(frame);
	}

	if (param_1 & 4)
		screenBlur(param_2);

	THitActor::perform(param_1, param_2);
}

BOOL TModelGate::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x80000001 && message == HIT_MESSAGE_ATTACK) {
		mOpenDuration = 0;
		mState        = 2;
		return TRUE;
	}

	if (sender->getActorType() == 0x1000001) {
		JGeometry::TVec3<f32> local;
		MTXMultVec(mInvCenterMtx, &sender->mPosition, &local);

		// the copy of the center joint matrix is never read (the retail code
		// still does it), `trash` pads the frame like the retail one
		Mtx centerMtx;
		MTXCopy(mActor->getModel()->getAnmMtx(mCenterJointIdx), centerMtx);
		char trash[4];

		if (local.x * local.x + local.y * local.y < 40000.0f
		    && -100.0f < local.z && local.z < mHitMaxZ) {
			if (mFlags & 2) {
				mOpenRatio += mOpenRatioInc;
				if (mOpenRatio > 1.0f) {
					mOpenTimer = mOpenDuration;
					mOpenRatio = 1.0f;
					mFlags &= ~2;
				}
			}

			if (0.000030517578f * rand() < mHitEffectChance) {
				gpMarioParticleManager->emitWithRotate(
				    0x1DD, &sender->mPosition, 0, mCenterAngle, 0, 2, nullptr);
				gpMarioParticleManager->emitWithRotate(
				    0x1DE, &sender->mPosition, 0, mCenterAngle, 0, 2, nullptr);
			}
			return TRUE;
		}
	}

	return FALSE;
}

void TModelGate::screenBlur(JDrama::TGraphics* graphics)
{
	JGeometry::TVec3<f32> viewDir;
	JGeometry::TVec3<f32> dir;
	dir.x = gpMarioPos->x - mPosition.x;
	dir.y = 0.0f;
	dir.z = gpMarioPos->z - mPosition.z;
	VECNormalize(&dir, &dir);
	MTXMultVecSR(graphics->mViewMtx.mMtx, &dir, &viewDir);

	JGeometry::TVec3<f32> local;
	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	MTXMultVec(mInvCenterMtx, &marioPos, &local);

	f32 factor = 0.0f;
	if (-250.0f <= local.x && local.x <= 250.0f && -250.0f <= local.y
	    && local.y <= 250.0f && 0.0f <= local.z && local.z <= mBlurFarDist) {
		if (local.z < mBlurNearDist)
			factor = 1.0f;
		if (mBlurNearDist <= local.z && local.z <= mBlurFarDist)
			factor
			    = 1.0f
			      - (local.z - mBlurNearDist) / (mBlurFarDist - mBlurNearDist);
		if (mBlurFarDist < local.z)
			factor = 0.0f;
	}

	f32 alpha = mBlurMaxAlpha * factor;
	s16 angleS16 = (s16)(182.04445f * mRotation.y - gpCamera->unk258);
	if (angleS16 < -0x2AAA || angleS16 > 0x2AAA)
		alpha = 0.0f;

	mBlurAlpha += mBlurAlphaRate * (alpha - mBlurAlpha);

	u8 blur = (u8)(mBlurAlpha * (1.0f - gpCamera->unk270));
	f32 strength = mBlurStrength;
	JGeometry::TVec3<f32> offset(viewDir.x, viewDir.y, viewDir.z);
	char trash[16];
	TAfterEffect* effect                    = gpAfterEffect;
	effect->unk15                           = 2;
	effect->unk1C                           = blur;
	effect->unk50                           = strength;
	*(JGeometry::TVec3<f32>*)&effect->unk5C = offset;
}

void TModelGate::startOpen()
{
	mFlags |= 1;
	mState = 0;
	mActor->setBpk(gateMActorNames[mDestination]);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mFlags |= 2;
}

void TModelGate::loadAfter()
{
	static const char* gateNames[] = {
		"Gate",        "GateToRicco", "GateToMamma",
		"GateToMonte", "GateToMare",  nullptr,
	};

	initHitActor(0x080000C0, 5, -0x80000000, 300.0f, 400.0f, 300.0f, 400.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mFlags       = 0;
	mDestination = 0;

	for (u8 i = 0; i < 5; ++i) {
		if (strcmp(gateNames[i], mName) == 0) {
			mDestination = i;
			break;
		}
	}

	char path[256];
	u32 loaderFlags = 0x11100000;
	snprintf(path, 256, "/scene/map/map/gate/%s.bmd",
	         gateMActorNames[mDestination]);
	mActor = SMS_MakeMActor("/scene/map/map/gate", path, 0, loaderFlags);
	mCenterJointIdx
	    = mActor->getModel()->getModelData()->getJointName()->getIndex(
	        "center");

	if (ActivePlayer.open != 0) {
		THPVideoInfo info;
		THPPlayerGetVideoInfo(&info);

		J3DTexture* texture = mActor->getModel()->getModelData()->getTexture();
		u16 width           = info.xSize;
		u16 height          = info.ySize;
		ResTIMG* timg       = texture->getResTIMG(0);
		timg->format        = 1;
		timg->width         = width;
		timg->height        = height;
		timg                = texture->getResTIMG(1);
		timg->format        = 1;
		timg->width         = width >> 1;
		timg->height        = height >> 1;
		timg                = texture->getResTIMG(2);
		timg->format        = 1;
		timg->width         = width >> 1;
		timg->height        = height >> 1;
	}

	mTevAnimMode     = 0;
	mTevAnimStep     = 0;
	mPrevTevAnimStep = 0;
	mTevAnimInterval = 240;
	mTevAnimTimer    = mTevAnimInterval;

	mActor->setBtk(gateMActorNames[mDestination]);
	mActor->setBrk(gateMActorNames[mDestination]);
	mActor->getFrameCtrl(ANM_TYPE_BRK)->setRate(0.0f);

	mSampleCtrlData
	    = new SampleCtrlModelData(mActor->getModel()->getModelData());
	unkC5              = 0x20;
	unkC6              = 0xFF;
	mState             = 1;
	mOpenDuration      = 360;
	mOpenTimer         = 0;
	mOpenElapsed       = 0;
	unkCE              = 60;
	mOpenRatio         = 0.0f;
	mOpenRatioInc      = 0.1f;
	mOpenRatioDecOpen  = 0.02f;
	mOpenRatioDecClose = 0.025f;
	mScaling.set(1.0f, 1.0f, 1.0f);

	static_cast<JDrama::TViewObjPtrListT<JDrama::TViewObj>*>(
	    JDrama::TNameRefGen::search("マップグループ"))
	    ->getChildren()
	    .push_back(this);

	char trash[64];
	Mtx mtx;
	SMS_GetActorMtx(*this, mtx);
	mActor->getModel()->setBaseTRMtx(mtx);
	mActor->getModel()->calc();
	MTXTrans(mtx, 0.0f, 0.0f, 250.0f);
	MTXConcat(mActor->getModel()->getAnmMtx(mCenterJointIdx), mtx, mtx);
	mFrontPos.set(0.0f, 0.0f, 0.0f);
	MTXMultVec(mtx, &mFrontPos, &mFrontPos);
	MTXInverse(mActor->getModel()->getAnmMtx(mCenterJointIdx), mInvCenterMtx);
	MtxPtr center = mActor->getModel()->getAnmMtx(mCenterJointIdx);
	mCenterAngle  = matan(center[2][2], center[0][2]);

	mBlurMaxAlpha    = 0;
	mBlurAlpha       = 0.0f;
	mBlurAlphaRate   = 0.01f;
	mBlurStrength    = 0.02f;
	mBlurNearDist    = 500.0f;
	mBlurFarDist     = 1000.0f;
	mHitEffectChance = 0.7f;
	mHitMaxZ         = 0.0f;
	mPushRadius      = 200.0f;
	mEnterHalfWidth  = 150.0f;
	mEnterMinY       = -1000.0f;
	mEnterMaxY       = 150.0f;
	mEnterMinZ       = 0.0f;
	mEnterMaxZ       = 300.0f;

	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_body.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_BODY);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_head.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_HEAD);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_cap.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_CAP);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rhand.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_RHAND);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lhand.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_LHAND);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rleg.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_RLEG);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_rfoot.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_RFOOT);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lleg.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_LLEG);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_lfoot.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_LFOOT);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_watgun.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_WATGUN);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_dust.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_DUST);
	SMS_LoadParticle("/scene/map/map/gate/ms_mariowp_senko.jpa",
	                 MAP_MAP_GATE_MS_MARIOWP_SENKO);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a.jpa", 0x131);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a2.jpa", 0x132);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_a3.jpa", 0x133);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatewind_b.jpa", 0x134);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatehit_a.jpa", 0x1DD);
	SMS_LoadParticle("/scene/map/map/gate/ms_gatehit_b.jpa", 0x1DE);

	bool opened = false;
	switch (mDestination) {
	case 0:
		opened = TFlagManager::smInstance->getBool(0x10385);
		break;
	case 1:
		opened = TFlagManager::smInstance->getBool(0x10386);
		break;
	case 2:
		opened = TFlagManager::smInstance->getBool(0x10387);
		break;
	case 3:
		opened = TFlagManager::smInstance->getBool(0x10387);
		break;
	case 4:
		opened = TFlagManager::smInstance->getBool(0x10387);
		break;
	}

	if (opened == true) {
		mFlags |= 1;
		offHitFlag(HIT_FLAG_NO_COLLISION);
	} else {
		mFlags &= ~1;
		mFlags |= 2;
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}
}
