#include <Enemy/Cannon.hpp>
#include <Enemy/BombHei.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/EffectObj.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Igaiga.hpp>
#include <Enemy/Killer.hpp>
#include <Enemy/Popo.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JMath.hpp>
#include <System/Application.hpp>
#include <System/FlagManager.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/DrawUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const char* cannon_bastable[] = {
	nullptr,
	nullptr,
	"/scene/cannon/bas/CannonDom_break.bas",
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/cannon/bas/tyorobe_appear1.bas",
	"/scene/cannon/bas/tyorobe_damage1.bas",
	"/scene/cannon/bas/tyorobe_down1.bas",
	"/scene/cannon/bas/tyorobe_hyde1.bas",
	"/scene/cannon/bas/tyorobe_throw1.bas",
	nullptr,
	nullptr,
	nullptr,
};

u8 TCannon::mChorobeiJntIdx     = 4;
u8 TCannon::mChorobeiHandJntIdx = 4;
f32 TCannon::mVelocityRate      = 0.62f;
f32 TCannon::mSearchRate        = 0.02f;

BOOL TNerveCannonObject::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		if (!self->isBckAnm(4)) {
			self->setBckAnm(4);
			J3DFrameCtrl* ctrl = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
			ctrl->setFrame(ctrl->getEnd());
		}

		if (self->mChorobei != nullptr)
			self->mChorobei->unk70 = 1.0f;
	}

	if (self->isBckAnm(4)
	    && self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(60.0f)) {
		SMSGetMSound()->startSoundActor(MSD_SE_EN_CANNON_LAND,
		                                &self->mPosition, 0, nullptr, 0, 4);
	}

	if (spine->getTime() > 150 && self->mMareGate != nullptr) {
		if (self->mMareGate->checkLiveFlag(LIVE_FLAG_DEAD)) {
			self->mMareGate->appear();
			self->mMareGate->mPosition = self->mPosition;
			self->mMareGate->mScaling.set(0.27f, 0.02f, 0.27f);
			self->mMareGate->getMActor()->setBtk("maregate");
		}

		f32 scaleY = 1.01f * self->mMareGate->mScaling.y;
		if (scaleY > 0.22f)
			scaleY = 0.22f;
		else if (scaleY < 0.0f)
			scaleY = 0.0f;
		self->mMareGate->mScaling.y = scaleY;
	}

	return false;
}

BOOL TNerveCannonDamageDemo::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0)
		self->mChorobei->setBckAnm(14);

	if (spine->getTime() > 120
	    && self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()
	    && !self->isBckAnm(4)) {
		self->setBckAnm(4);
		SMSGetMSound()->startSoundActor(MSD_SE_EN_CANNON_DOWN,
		                                &self->mPosition, 0, nullptr, 0, 4);

		TEffectExplosion* effect
		    = (TEffectExplosion*)gpConductor->makeOneEnemyAppear(
		        self->mPosition, "エフェクト爆発マネージャー", 1);
		if (effect != nullptr) {
			JGeometry::TVec3<f32> scale(2.5f, 2.5f, 2.5f);
			effect->generate(self->mPosition, scale);
		}

		TFlagManager::smInstance->setBool(true, 0x5000C);
		spine->pushAfterCurrent(&TNerveCannonObject::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveCannonDamage::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
		    EXPLOSION_JPA_MS_BOMB_SMOKE, &self->mPosition, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));

		gpMarioParticleManager->emitAndBindToPosPtr(
		    EXPLOSION_JPA_MS_BOMB_BOMB, &self->mPosition, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));

		gpMarioParticleManager->emitAndBindToPosPtr(
		    EXPLOSION_JPA_MS_BOMB_HAHEN, &self->mPosition, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(2.0f, 2.0f, 2.0f));

		self->decHitPoints();
		if (self->getHitPoints() == 0) {
			if (self->unk1A4 != nullptr)
				self->unk1A4->kill();

			self->mChorobei->setBckAnm(13);

			if (gpApplication.mCurrArea.unk0 == 5)
				SMSRumbleMgr->start(0x18, (f32*)nullptr);
			else
				SMSRumbleMgr->start(0x17, (f32*)nullptr);

			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0xC8,
			    self->mChorobei->mSharedParts->getMActor()->getModel()->getAnmMtx(
			        12),
			    0, nullptr);
			if (emitter != nullptr)
				emitter->setGlobalScale(self->mChorobei->mScaling);

			emitter = gpMarioParticleManager->emitAndBindToPosPtr(
			    0xC7, &self->unk294, 0, nullptr);
			if (emitter != nullptr)
				emitter->setGlobalScale(self->mChorobei->mScaling);

			if (gpApplication.mCurrArea.unk0 == 5) {
				self->unk2A0 = self->mPosition;
				self->unk2A0.y = 0.0f;
				gpMarDirector->fireStartDemoCamera(
				    "tyorocam_pinna", &self->unk2A0, -1, self->mRotation.y,
				    true, nullptr, 0, nullptr, JDrama::TFlagT<u16>(0));
			} else {
				gpMarDirector->fireStartDemoCamera(
				    "tyorocam_mare", nullptr, -1, 0.0f, true, nullptr, 0,
				    nullptr, JDrama::TFlagT<u16>(0));
			}

			self->onHitFlag(HIT_FLAG_NO_COLLISION);
			self->mChorobei->onHitFlag(HIT_FLAG_NO_COLLISION);
		} else {
			self->setFreezeAnm();
			self->mChorobei->setBckAnm(13);

			MtxPtr jointMtx
			    = self->mChorobei->mSharedParts->getMActor()->getModel()->getAnmMtx(
			        0);
			self->unk294.set(jointMtx[0][3], jointMtx[1][3], jointMtx[2][3]);
			emitter = gpMarioParticleManager->emitAndBindToPosPtr(
			    0xC7, &self->unk294, 0, nullptr);
			if (emitter != nullptr)
				emitter->setGlobalScale(self->mChorobei->mScaling);
		}

		self->setVelocity(JGeometry::TVec3<f32>(0.0f, 4.0f, 0.0f));
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->mPosition.y += 10.0f;
	}

	if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()) {
		SMS_ResetDamageFogEffect(self->mChorobei->mSharedParts->getMActor()
		                             ->getModel()
		                             ->getModelData());
		if (self->getHitPoints() == 0) {
			spine->pushAfterCurrent(&TNerveCannonDamageDemo::theNerve());
			return true;
		}

		spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
		return true;
	}

	return false;
}

BOOL TNerveCannonClose::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() < 2) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mChorobei->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->mChorobei->setBckAnm(15);
		self->unk294 = self->mPosition;
		self->unk294.y += 300.0f;
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_CHO_ASE,
		                                            &self->unk294, 0, nullptr);
	}

	if (self->mChorobei->checkHideEnd() && !self->isBckAnm(0))
		self->setBckAnm(0);

	if (self->isBckAnm(0)
	    && self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(10.0f)) {
		self->unk294 = self->mPosition;
		self->unk294.y += 290.0f;
		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToPosPtr(
		    PARTICLE_MS_JUMP_ED_B, &self->unk294, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(JGeometry::TVec3<f32>(1.5f, 1.5f, 1.5f));
	}

	self->updateSquareToMario();

	f32 hideDist = 3.0f * self->mParams->mSLHideDist.get();
	if (self->mDistToMarioSquared > hideDist * hideDist) {
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->mChorobei->offHitFlag(HIT_FLAG_NO_COLLISION);
		spine->pushAfterCurrent(&TNerveCannonOpen::theNerve());
		return true;
	}

	self->unk2B0->moveMtx(self->getMActor()->getModel()->getAnmMtx(4));
	return false;
}

BOOL TNerveCannonForceBombShoot::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		f32 bombDist = self->mParams->mSLBombDist.get();
		if (self->mDistToMarioSquared < 2.0f * (bombDist * bombDist))
			self->mChorobei->setBckAnm(17);
		else
			return true;
	}

	if (self->mChorobei->mSharedParts->getMActor()->checkCurBckFromIndex(17)) {
		if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()) {
			self->mChorobei->setBckAnm(16);
			self->bombSet();
		}
		self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	} else if (self->mChorobei->mSharedParts->getMActor()->checkCurBckFromIndex(
	               16)) {
		if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext())
			return true;

		if (self->mChorobei->mSharedParts->getMActor()
		        ->getFrameCtrl(ANM_TYPE_BCK)
		        ->checkPass(38.0f))
			self->bombShoot();

		if (self->mChorobei->mSharedParts->getMActor()
		        ->getFrameCtrl(ANM_TYPE_BCK)
		        ->getFrame()
		    > 26.0f) {
			if (self->unk1A4 != nullptr) {
				f32 maxScale = self->unk220;
				f32 step     = 0.2f * maxScale;
				f32 scale    = self->unk1A4->mScaling.x + step;
				if (scale > maxScale)
					scale = maxScale;
				else if (scale < 0.0f)
					scale = 0.0f;
				self->unk1A4->mScaling.x = scale;
				self->unk1A4->mScaling.set(self->mScaling.x, self->mScaling.x,
				                           self->mScaling.x);
			}
		}
	}

	return false;
}

BOOL TNerveCannonShoot::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		if (!self->unk290)
			self->setKillerGoalPoint();
		else
			self->mChorobei->setBckAnm(17);
	}

	if (self->unk290) {
		if (self->mChorobei->mSharedParts->getMActor()->checkCurBckFromIndex(
		        17)) {
			if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()) {
				self->mChorobei->setBckAnm(16);
				self->bombSet();
			}
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
		} else if (self->mChorobei->mSharedParts->getMActor()
		               ->checkCurBckFromIndex(16)) {
			if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()) {
				spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
				return true;
			}

			if (self->mChorobei->mSharedParts->getMActor()
			        ->getFrameCtrl(ANM_TYPE_BCK)
			        ->checkPass(38.0f))
				self->bombShoot();

			if (self->mChorobei->mSharedParts->getMActor()
			        ->getFrameCtrl(ANM_TYPE_BCK)
			        ->getFrame()
			    > 26.0f) {
				if (self->unk1A4 != nullptr) {
					f32 maxScale = self->unk220;
					f32 step     = 0.2f * maxScale;
					f32 scale    = self->unk1A4->mScaling.x + step;
					if (scale > maxScale)
						scale = maxScale;
					else if (scale < 0.0f)
						scale = 0.0f;
					self->unk1A4->mScaling.x = scale;
					self->unk1A4->mScaling.set(
					    self->mScaling.x, self->mScaling.x, self->mScaling.x);
				}
			}
		}
	} else {
		if (spine->getTime() < 40)
			self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);

		if (spine->getTime() == 40)
			self->killerShoot();

		if (spine->getTime() > self->mParams->mSLKillerInterval.get()) {
			self->mDomIdx++;
			if (self->mDomIdx >= 3)
				self->mDomIdx = 0;
			return true;
		}
	}

	return false;
}

BOOL TNerveCannonSearch::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	self->updateSquareToMario();
	f32 distSq      = self->mDistToMarioSquared;
	f32 bombDistSq  = self->mParams->mSLBombDist.get()
	                 * self->mParams->mSLBombDist.get();

	if (spine->getTime() == 0) {
		if (distSq < bombDistSq)
			self->setGoalPath(TPathNode((THitActor*)gpMarioAddress));

		self->mChorobei->setBckAnm(19);
	}

	if (self->mChorobei->mSharedParts->getMActor()->curAnmEndsNext()
	    && self->mChorobei->mSharedParts->getMActor()->checkCurBckFromIndex(
	        19)) {
		self->mChorobei->setBckAnm(18);
	}

	if (distSq < self->mParams->mSLHideDist.get()
	                 * self->mParams->mSLHideDist.get()) {
		spine->pushAfterCurrent(&TNerveCannonClose::theNerve());
		return true;
	}

	if (distSq < self->mParams->mSLBombDist.get()
	                 * self->mParams->mSLBombDist.get()) {
		if (spine->getTime() > self->mParams->mSLBombInterval.get()) {
			self->unk290 = 1;
			spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
			return true;
		}
	} else if (self->mAreaType == 5 || self->mAreaType == 9) {
		if (distSq < self->mParams->mSLKillerDist.get()
		                 * self->mParams->mSLKillerDist.get()) {
			if (spine->getTime() > self->mParams->mSLShootInterval.get()) {
				self->unk290 = 0;
				spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
				spine->pushAfterCurrent(&TNerveCannonForceBombShoot::theNerve());
				spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
				spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
				spine->pushAfterCurrent(&TNerveCannonShoot::theNerve());
				return true;
			}
		}
	}

	if (self->unk2AC != self->mRotation.y) {
		self->unk2AC = self->mRotation.y;
		SMSGetMSound()->startSoundActor(MSD_SE_EN_CANNON_MOVE,
		                                &self->mPosition, 0, nullptr, 0, 4);
	}

	if (gpApplication.mCurrArea.unk0 == 5 && gpMarDirector->mState == 1) {
		JGeometry::TVec3<f32> diff = *gpMarioPos - self->mPosition;
		self->mRotation.y = MsGetRotFromZaxis(diff).y;
	} else {
		self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	}

	return false;
}

BOOL TNerveCannonOpen::execute(TSpineBase<TLiveActor>* spine) const
{
	TCannon* self = (TCannon*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mChorobei->setBckAnm(12);
		self->setBckAnm(3);
	}

	TChorobei* chorobei = self->mChorobei;
	bool endOpen;
	if (chorobei->mSharedParts->getMActor()->curAnmEndsNext()
	    && chorobei->mSharedParts->getMActor()->checkCurBckFromIndex(12)) {
		endOpen = true;
	} else {
		chorobei->unk70 = 0.0f;
		endOpen         = false;
	}

	if (endOpen && self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveCannonSearch::theNerve());
		return true;
	}

	return false;
}

void TCannon::startChorobeiShout() { }

bool TCannon::isObject()
{
	if (isBckAnm(4) && checkCurAnmEnd(0))
		return true;
	return false;
}

void TCannon::setKillerGoalPoint()
{
	if (unk239) {
		TMsRange<f32> range(0.0f, 360000.0f);
		f32 angle = range.rand();
		JGeometry::TVec3<f32> target = *gpMarioPos;
		target.x += 500.0f * JMASCos(angle);
		target.z += 500.0f * JMASSin(angle);
		setGoalPath(TPathNode(target));
	} else {
		setGoalPath(TPathNode(unk248));
	}

	mDoms[mDomIdx]->setBckAnm(1);
}

void TCannon::killerShoot()
{
	if (unk239) {
		TKiller* killer = (TKiller*)gpConductor->makeOneEnemyAppear(
		    mPosition, "キラーマネージャー", 0);
		if (killer != nullptr) {
			killer->reset();

			unk1E0 = mDoms[mDomIdx]->getMActor()->getModel()->getBaseTRMtx();
			unk1E0 = mDoms[mDomIdx]->getMActor()->getModel()->getAnmMtx(1);

			TPosition3f mtx;
			mtx.translation(0.0f, -60.0f, 150.0f);
			MTXConcat(unk1E0, mtx, mtx);
			killer->mPosition.set(mtx.ref(0, 3), mtx.ref(1, 3), mtx.ref(2, 3));

			f32 marioSpeedX = *gpMarioSpeedX;
			f32 marioSpeedZ = *gpMarioSpeedZ;

			JGeometry::TVec3<f32> target = *gpMarioPos;
			TMsRange<f32> range(-300.0f, 300.0f);
			target.x += range.rand();
			switch (mDomIdx) {
			case 0:
				target.z -= 2.0f * fabsf(range.rand());
				break;
			case 1:
				break;
			case 2:
				target.z += 2.0f * fabsf(range.rand());
				break;
			}

			JGeometry::TVec3<f32> velocity = killer->calcVelocityToJumpToY(
			    target, 5.0f, killer->getGravityY());

			JGeometry::TVec3<f32> toTarget = target - mPosition;
			f32 flyTime = fabsf(MsVECMag2(toTarget)
			                    / (velocity.x * mVelocityRate));

			killer->mIsHoming = 0;
			f32 rate          = mVelocityRate;
			TMsRange<s32> chance(0, 100);
			if (chance.rand() % 5 == 0) {
				killer->mIsHoming = 1;
			} else {
				if (*gpMarioSpeedX > 2.0f)
					rate = 0.55f;
				if (*gpMarioSpeedX < -2.0f)
					rate = 0.68f;
			}

			JGeometry::TVec3<f32> predicted(
			    target.x + mSearchRate * (marioSpeedX * flyTime), target.y,
			    target.z + mSearchRate * (marioSpeedZ * flyTime));
			velocity = killer->calcVelocityToJumpToY(predicted, 5.0f,
			                                         killer->getGravityY());
			velocity.x *= rate;
			velocity.y *= rate;
			velocity.z *= rate;

			f32 rotY = MsAngleWrap(MsGetRotFromZaxisY(velocity));
			killer->mRotation.set(0.0f, rotY, 0.0f);
			killer->mScaling.set(0.1f, 0.1f, 0.1f);
			if (gpMarDirector->mState == 1) {
				velocity.x *= 0.2f;
				velocity.y *= 0.4f;
				velocity.z *= 0.2f;
				killer->mScaling.set(0.6f, 0.6f, 0.6f);
			}

			killer->setColorType();
			killer->mChaseVelocity = velocity;
			killer->mVelocity      = velocity;
			killer->onLiveFlag(LIVE_FLAG_AIRBORNE);

			JGeometry::TVec3<f32> marioDiff = *gpMarioPos - mPosition;
			target.x += marioDiff.x;
			target.z += marioDiff.z;
			killer->setGoalPath(TPathNode(target));

			SMSGetMSound()->startSoundActor(MSD_SE_EN_KILLER_FIRE,
			                                &killer->mPosition, 0, nullptr, 0,
			                                4);
			SMSGetMSound()->startSoundActor(MSD_SE_EN_KILLER_FLY,
			                                &killer->mPosition, 0, nullptr, 0,
			                                4);
		}
	} else {
		TIgaiga* igaiga = (TIgaiga*)gpConductor->makeOneEnemyAppear(
		    mPosition, "イガイガマネージャー", 1);
		if (igaiga != nullptr) {
			igaiga->reset();

			unk1E0 = mDoms[mDomIdx]->getMActor()->getModel()->getAnmMtx(1);

			TPosition3f mtx;
			mtx.translation(0.0f, -60.0f, 150.0f);
			MTXConcat(unk1E0, mtx, mtx);
			igaiga->mPosition.set(mtx.ref(0, 3), mtx.ref(1, 3), mtx.ref(2, 3));

			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
			    0xCB, &igaiga->mPosition, 0,
			    (s16)(igaiga->mRotation.y * (65536.0f / 360.0f)), 0, 0, nullptr);
			if (emitter != nullptr) {
				emitter->setGlobalScale(JGeometry::TVec3<f32>(
				    1.5f * mScaling.x, 1.5f * mScaling.y, 1.5f * mScaling.z));
			}

			JGeometry::TVec3<f32> point;
			igaiga->getTracer()->getGraph()->getGraphNode(0).getPoint(
			    (Vec*)&point);
			unk248 = point;

			JGeometry::TVec3<f32> velocity = igaiga->calcVelocityToJumpToY(
			    point, 10.0f, igaiga->getGravityY());
			igaiga->mRotation = mRotation;
			igaiga->shoot(velocity);
		}
	}
}

void TCannon::bombShoot()
{
	if (unk1A4 != nullptr) {
		JGeometry::TVec3<f32> dir(gpMarioPos->x - mPosition.x, 0.0f,
		                          gpMarioPos->z - mPosition.z);
		if (dir.x == 0.0f && dir.z == 0.0f)
			dir.x = 1.0f;

		MsVECNormalize((Vec*)&dir, (Vec*)&dir);

		TMsRange<f32> range(-30.0f, 30.0f);
		Mtx mtx;
		MsMtxSetRotRPH(mtx, 0.0f, mRotation.y + range.rand(), 0.0f);

		f32 throwSpeed = mParams->mSLThrowXZSpeed.get();
		dir.y          = throwSpeed;
		dir.x *= throwSpeed;
		dir.z *= throwSpeed;

		if (unk21C) {
			unk1A4->setVelocityAndFlag10(dir.x, dir.y, dir.z);
		} else {
			unk1A4->mVelocity = dir;
			unk1A4->onLiveFlag(LIVE_FLAG_AIRBORNE);
			unk1A4->getMActor()->setFrameRate(SMSGetAnmFrameRate(),
			                                  ANM_TYPE_BCK);
		}

		unk1A4->mPosition.y += 2.0f;
		unk1A4->receiveMessage(this, HIT_MESSAGE_PUT);
	}
}

void TCannon::bombSet()
{
	TMsRange<f32> range(0.0f, 1.0f);
	f32 chance = range.rand();
	char trash[0x8];

	unk21C = false;
	unk1A4 = nullptr;

	TSpineEnemy* enemy;
	if (chance < mParams->mSLBombHeiGenerateRate.get()) {
		enemy = gpConductor->makeOneEnemyAppear(mPosition,
		                                        "ボム兵マネージャー", 1);
	} else {
		TMsRange<s32> kind(0, 100);
		if (kind.rand() % 2 == 1) {
			TPopo* popo = (TPopo*)gpConductor->makeOneEnemyAppear(
			    mPosition, "ポポマネージャー", 1);
			if (popo != nullptr)
				popo->thrownByChorobei();
			enemy = popo;
		} else {
			enemy = gpConductor->makeOneEnemyAppear(
			    mPosition, "ハムクリマネージャー", 1);
		}
	}

	if (unk1A4 == nullptr) {
		unk1A4 = enemy;
		if (enemy != nullptr) {
			enemy->reset();
			enemy->getMActor()->setFrameRate(0.0f, ANM_TYPE_BCK);
		}
	}

	if (unk1A4 != nullptr) {
		unk1A4->mPosition = mChorobei->mPosition;
		unk220            = unk1A4->mScaling.x;
		unk1A4->mScaling.set(0.0f, 0.0f, 0.0f);
		unk1A4->mRotation = mRotation;
		if (unk1A4->receiveMessage(this, HIT_MESSAGE_TAKE))
			mHeldObject = unk1A4;
	}
}

MtxPtr TCannon::getTakingMtx()
{
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		return mChorobei->mSharedParts->getMActor()->getModel()->getBaseTRMtx();
	}

	f32 x = mChorobei->mPosition.x;
	f32 y = mChorobei->mSharedParts->getMActor()
	            ->getModel()
	            ->getAnmMtx(mChorobeiHandJntIdx)[1][3];
	f32 z = mChorobei->mPosition.z;
	mTakingMtx.translation(x, y, z);
	return mTakingMtx;
}

void TCannon::calcRootMatrix()
{
	if (mSpine->getCurrentNerve() != &TNerveCannonObject::theNerve()) {
		static const f32 xzTable[4][2] = {
			{ 1.0f, -1.0f },
			{ 1.0f, 1.0f },
			{ -1.0f, 1.0f },
			{ -1.0f, -1.0f },
		};

		mHeadHeight = 50.0f;

		JGeometry::TVec3<f32> base = mPosition;
		base.y = getMActor()->getModel()->getAnmMtx(4)[1][3];

		for (int i = 0; i < 4; i++) {
			mCollisionVerts[i] = base;
			mCollisionVerts[i].x += 200.0f * xzTable[i][0];
			mCollisionVerts[i].z += 200.0f * xzTable[i][1];
		}

		unk258->setVertexData(0, mCollisionVerts[2], mCollisionVerts[1],
		                      mCollisionVerts[0]);
		unk258->setVertexData(1, mCollisionVerts[0], mCollisionVerts[3],
		                      mCollisionVerts[2]);
	}

	if (mHolder != nullptr) {
		MtxPtr holderMtx = mHolder->getTakingMtx();
		if (mSpine->getCurrentNerve() == &TNerveCannonObject::theNerve()) {
			MTXCopy(holderMtx, getModel()->getBaseTRMtx());
			mPosition.set(holderMtx[0][3], holderMtx[1][3], holderMtx[2][3]);
		} else {
			bool lookingAway = true;
			if (gpMarDirector->unk124 != 3 && gpMarDirector->unk124 != 4)
				lookingAway = false;
			if (lookingAway)
				mRotation.y = -80.0f;

			mPosition.set(holderMtx[0][3], holderMtx[1][3], holderMtx[2][3]);
			MsMtxSetXYZRPH(getMActor()->getModel()->getBaseTRMtx(),
			               mPosition.x, mPosition.y, mPosition.z, mRotation.x,
			               mRotation.y, mRotation.z);
		}
	} else {
		TSpineEnemy::calcRootMatrix();
	}

	if (mChorobei != nullptr
	    && mChorobei->mSharedParts->getMActor()->checkCurBckFromIndex(14)
	    && mChorobei->mSharedParts->getMActor()
	           ->getFrameCtrl(ANM_TYPE_BCK)
	           ->checkPass(2.0f)) {
		for (int i = 0; i < 3; i++) {
			MtxPtr connected = mDoms[i]->getConnectedMtx();
			unk294.set(connected[0][3], connected[1][3], connected[2][3]);
			mDoms[i]->setBckAnm(2);
			gpMarioParticleManager->emit(PARTICLE_MS_HIPDROP_C, &unk294, 0,
			                             nullptr);
			gpMarioParticleManager->emit(PARTICLE_MS_HIPDROP_B, &unk294, 0,
			                             nullptr);
			gpMarioParticleManager->emit(PARTICLE_MS_HIPDROP_A, &unk294, 0,
			                             nullptr);
		}
	}
}

void TCannon::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);

	if (unk238) {
		mHodai->getMActor()->perform(cue, graphics);
		if (cue & 1) {
			if (mHodai->getMActor()->curAnmEndsNext())
				unk238 = 0;
		}

		if ((cue & 2) && mAreaType == 1) {
			if (mHodai->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(
			        174.0f)) {
				MtxPtr mtx = unk1B8->getMActor()->getModel()->getAnmMtx(0);
				gpMarioParticleManager->emitAndBindToMtxPtr(0xE8, mtx, 0,
				                                            nullptr);
				gpMarioParticleManager->emitAndBindToMtxPtr(0xE9, mtx, 0,
				                                            nullptr);
				gpMarioParticleManager->emitAndBindToMtxPtr(0xEA, mtx, 0,
				                                            nullptr);
				gpMarioParticleManager->emitAndBindToMtxPtr(0xEB, mtx, 0,
				                                            nullptr);
				gpMarioParticleManager->emitAndBindToMtxPtr(0xEC, mtx, 0,
				                                            nullptr);
			}

			if (mHodai->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			    > 175.0f) {
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0x166, mHodai->getMActor()->getModel()->getAnmMtx(0), 1,
				    this);
			}
		}
	}

	if (mAreaType == 5 || mAreaType == 9) {
		mChorobei->perform(cue, graphics);

		if (cue & 0x200) {
			if (mSpine->getCurrentNerve() == &TNerveCannonDamage::theNerve()) {
				mChorobei->mSharedParts->getMActor()->offMakeDL();
				SMS_AddDamageFogEffect(mChorobei->mSharedParts->getMActor()
				                           ->getModel()
				                           ->getModelData(),
				                       mPosition, graphics);
			}
		}

		for (int i = 0; i < 3; i++) {
			if (!mDoms[i]->unk24) {
				if (cue == 1) {
					if (mParams->mSLBombDist.get() * mParams->mSLBombDist.get()
					        < mDistToMarioSquared
					    && mAreaType == 5) {
						mDoms[i]->unk2C = unk224[i];
						if (i != mDomIdx
						    && mSpine->getCurrentNerve()
						           != &TNerveCannonObject::theNerve()) {
							mDoms[i]->unk30 += 1.0f;
							if (mDoms[i]->unk30 > 360.0f)
								mDoms[i]->unk30 -= 360.0f;

							if (i == 2) {
								mDoms[i]->unk28
								    = -10.0f
								      * JMASSin(0.5f * mDoms[i]->unk30
								                * (65536.0f / 360.0f));
							} else {
								mDoms[i]->unk28
								    = -20.0f
								      * JMASSin(0.5f * mDoms[i]->unk30
								                * (65536.0f / 360.0f));
							}
						}
					} else {
						mDoms[i]->unk30 = 0.0f;
						mDoms[i]->unk28 *= 0.99f;
						mDomCollisions[i]->moveMtx(
						    mDoms[i]->getConnectedMtx());
					}
				}

				mDoms[i]->perform(cue, graphics);
			}
		}
	} else {
		if (!unk1B8->unk24)
			unk1B8->perform(cue, graphics);
	}
}

const char** TCannon::getBasNameTable() const { return cannon_bastable; }

BOOL TCannon::receiveMessage(THitActor* sender, u32 message)
{
	if (sender->getActorType() == 0x40000235 && message == HIT_MESSAGE_TAKE
	    && mHolder == nullptr) {
		mHolder = (TTakeActor*)sender;
		return TRUE;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return TRUE;

	return FALSE;
}

void TCannon::moveObject()
{
	TSmallEnemy::moveObject();

	if (mAreaType == 5 || mAreaType == 9) {
		if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
			mChorobei->mPosition = mPosition;
		} else {
			MtxPtr jointMtx = getModel()->getAnmMtx(mChorobeiJntIdx);
			mChorobei->mPosition.x = jointMtx[0][3];
			mChorobei->mPosition.y = jointMtx[1][3] - 100.0f;
			mChorobei->mPosition.z = jointMtx[2][3];
		}

		mChorobei->checkHit();

		JGeometry::TVec3<f32> velocity = mVelocity;
		mPosition.y += velocity.y;
		mVelocity.y -= getGravityY();
		if (mPosition.y < unk23C.y) {
			mVelocity = JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f);
			mPosition.y = unk23C.y;
		}

		if (unk1A0 != nullptr) {
			unk1A0->mPosition = unk194 + mChorobei->mPosition;
			unk1A0->offLiveFlag(LIVE_FLAG_AIRBORNE);

			if (mSpine->getCurrentNerve() == &TNerveCannonClose::theNerve()) {
				unk1A0->kill();
				unk1A0 = nullptr;
			} else if (!unk1A0->doKeepDistance()) {
				unk1A0 = nullptr;
				mSpine->pushNerve(&TNerveCannonDamage::theNerve());
			}
		}
	}
}

void TCannon::reset()
{
	TSmallEnemy::reset();

	mHitPoints = getMaxHitPoints();
	onHitFlag(HIT_FLAG_NO_COLLISION);
	mDomIdx     = 1;
	mHeadHeight = 40.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
	unk2AC = mRotation.y;

	if (mAreaType == 9) {
		unk239 = 0;
		setGoalPath(TPathNode(JGeometry::TVec3<f32>(-565.0f, 8500.0f, 7675.0f)));
	}
}

void TCannon::init(TLiveManager* manager)
{
	TSmallEnemy::init(manager);

	mActorType = 0x1000001C;
	unk150     = 0x11;
	mParams    = (TCannonSaveLoadParams*)getSaveParam();
	setBckAnm(3);

	void* res = JKRFileLoader::getGlbResource("/scene/cannon/cannon_Dom.bmd");
	SDLModelData* modelData
	    = new SDLModelData(J3DModelLoaderDataBase::load(res, 0x10050000));

	unk234     = mRotation.y;
	mAreaType = gpApplication.mCurrArea.unk0;

	if (mAreaType == 5 || mAreaType == 9) {
		mSpine->initWith(&TNerveCannonSearch::theNerve());

		if (mAreaType == 9) {
			unk239 = 0;
			setGoalPath(
			    TPathNode(JGeometry::TVec3<f32>(-565.0f, 8500.0f, 7675.0f)));
		} else {
			setGoalPath(TPathNode((THitActor*)gpMarioAddress));
		}

		mChorobei = new TChorobei(this, 0, "チョロベー");
		for (u8 i = 0;
		     i < mChorobei->mSharedParts->getMActor()->getModel()
		                 ->getModelData()
		                 ->getJointNum();
		     i++) { }

		mChorobei->initHitActor(0x1000001D, 3, (int)0x90000000,
		                        mParams->mSLChorobeiAttackRadius.get(),
		                        mParams->mSLChorobeiAttackHeight.get(),
		                        mParams->mSLChorobeiDamageRadius.get(),
		                        mParams->mSLChorobeiDamageHeight.get());

		TIdxGroupObj* group
		    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
		group->getChildren().push_back(mChorobei);

		static const char* sCannonDomPartsJointTable[] = { "nullC", "nullB",
			                                               "nullA" };
		JUTNameTab* jointNames = getMActor()->getModel()->getModelData()->getJointName();
		for (int i = 0; i < 3; i++) {
			int jointIndex = jointNames->getIndex(sCannonDomPartsJointTable[i]);
			mDoms[i] = new TCannonDom(this, jointIndex, modelData, 3, "砲身");
			mDomCollisions[i] = new TMapCollisionMove();
			mDomCollisions[i]->init("/cannon/CannonDom", 2, this);
			mDomCollisions[i]->setUpTrans(mPosition);
		}

		unk2B0 = new TMapCollisionMove();
		unk2B0->init("/cannon/CannonFuta", 2, this);
		unk2B0->setUpTrans(mPosition);
	} else {
		mSpine->initWith(&TNerveCannonObject::theNerve());

		int jointIndex
		    = getMActor()->getModel()->getModelData()->getJointName()->getIndex(
		        "nullA");
		unk1B8       = new TCannonDom(this, jointIndex, modelData, 3, "砲身");
		unk1B8->unk24 = true;
	}

	void* hodaiRes = JKRFileLoader::getGlbResource("/scene/cannon/hodai_mario.bmd");
	SDLModelData* hodaiModel
	    = new SDLModelData(J3DModelLoaderDataBase::load(hodaiRes, 0x10010000));
	mHodai = new TSharedParts(this, 0, hodaiModel, 3, "<TSharedParts>");
	for (u8 i = 0; i < getModel()->getModelData()->getJointNum(); i++) { }

	unk258 = new TMapCollisionMove();
	unk258->init(2, 0, 0, nullptr);
}

void TCannon::loadAfter()
{
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_a.jpa", 0xE8);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_b.jpa", 0xE9);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_c.jpa", 0xEA);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_d.jpa", 0xEB);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_e.jpa", 0xEC);
	SMS_LoadParticle("/scene/cannon/jpa/ms_cannon_smoke.jpa", 0x166);

	if (mAreaType == 5) {
		mMareGate = (TMapObjBase*)JDrama::TNameRefGen::search("efMareGate");
		mMareGate->kill();
	}
}

void TCannon::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	mHitPoints = getMaxHitPoints();
	mAreaType  = gpApplication.mCurrArea.unk0;
	unk23C     = mPosition;
}

TCannon::TCannon(const char* name)
    : TSmallEnemy(name)
    , unk1A0(nullptr)
    , mChorobei(nullptr)
    , unk1B8(nullptr)
    , unk1E0(nullptr)
    , mDomIdx(0)
    , unk218(0)
    , unk21C(0)
    , unk220(0.0f)
    , mAreaType(1)
    , unk238(0)
    , unk239(1)
    , mMareGate(nullptr)
    , unk258(nullptr)
{
	unk290 = 0;
	unk2AC = 0.0f;
	unk224[0] = 0.0f;
	unk224[1] = 0.0f;
	unk224[2] = 0.0f;
}

void TCannonDom::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!unk10->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN
	                          | LIVE_FLAG_CLIPPED_OUT)) {
		if (cue == 2) {
			if (mAnmSound != nullptr && mAnmSoundPath != nullptr) {
				J3DFrameCtrl* ctrl = getMActor()->getFrameCtrl(ANM_TYPE_BCK);
				mAnmSound->animeLoop((Vec*)&unk10->mPosition, ctrl->getFrame(),
				                     ctrl->getRate(), 0, 4);
			}

			MtxPtr connected = getConnectedMtx();
			Mtx rot;
			MsMtxSetRotRPH(rot, unk28, unk2C, 0.0f);
			MTXConcat(connected, rot, connected);
			MTXCopy(connected, getMActor()->getModel()->getBaseTRMtx());
		}

		getMActor()->perform(cue, graphics);
	}
}

#pragma dont_inline on
TCannonDom::TCannonDom(TLiveActor* owner, int jointIndex, SDLModelData* model,
                       u32 flags, const char* name)
    : TSharedParts(owner, jointIndex, model, flags, name)
    , mAnmSound(nullptr)
    , mAnmSoundPath(nullptr)
    , unk24(false)
    , unk28(0.0f)
    , unk2C(0.0f)
    , unk30(0.0f)
{
	TMsRange<f32> range(0.0f, 360.0f);
	unk30 = range.rand();
	if (mAnmSound != nullptr)
		return;
	mAnmSound = new MAnmSound(SMSGetMSound());
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}
#pragma dont_inline off

BOOL TChorobei::receiveMessage(THitActor*, u32) { return FALSE; }

void TChorobei::checkHit()
{
	for (int i = 0; i < mColCount; i++) {
		THitActor* hit = mCollisions[i];
		if (hit->isActorType(0x80000001))
			SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);

		if (hit->isActorType(0x1000001E)) {
			TLiveActor* cannon = mCannon;
			if (cannon->mSpine->getCurrentNerve()
			    != &TNerveCannonDamage::theNerve()) {
				if (((TBombHei*)hit)->isDamageToCannon()) {
					cannon->mSpine->pushNerve(&TNerveCannonDamage::theNerve());
					((TLiveActor*)hit)->kill();
				}
			}
		}

		if (hit->isActorType(0x1000001F)) {
			if (((TKiller*)hit)->isRollFly()) {
				mCannon->mSpine->pushNerve(&TNerveCannonDamage::theNerve());
				((TLiveActor*)hit)->kill();
			}
		}
	}
}

void TChorobei::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!mCannon->checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN
	                            | LIVE_FLAG_CLIPPED_OUT)
	    && unk70 == 0.0f) {
		if (cue & 2) {
			if (mAnmSound != nullptr && mAnmSoundPath != nullptr) {
				J3DFrameCtrl* ctrl
				    = mSharedParts->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
				mAnmSound->animeLoop((Vec*)&mPosition, ctrl->getFrame(),
				                     ctrl->getRate(), 0, 4);
			}

			Mtx mtx;
			MTXCopy(mSharedParts->getConnectedMtx(), mtx);
			mtx[1][3] += unk7C;
			MTXCopy(mtx, mSharedParts->getMActor()->getModel()->getBaseTRMtx());
			mPosition.x = mtx[0][3];
			mPosition.y = mtx[1][3] - 150.0f;
			mPosition.z = mtx[2][3];
		}

		THitActor::perform(cue, graphics);
		mSharedParts->getMActor()->perform(cue, graphics);
	}
}

#pragma dont_inline on
TChorobei::TChorobei(TCannon* cannon, int jointIndex, const char* name)
    : THitActor(name)
    , mCannon(cannon)
    , mSharedParts(nullptr)
    , unk70(0.0f)
    , mAnmSound(nullptr)
    , mAnmSoundPath(nullptr)
    , unk7C(300.0f)
{
	char trash[0x8];
	mSharedParts = new TSharedParts(mCannon, jointIndex,
	                                "/scene/cannon/tyorobe_model1.bmd",
	                                0x10020000, 3, "<TSharedParts>");

	if (mAnmSound != nullptr)
		return;
	mAnmSound = new MAnmSound(SMSGetMSound());
	mAnmSound->initAnmSound(nullptr, 1, 0.0f);
}
#pragma dont_inline off

TSpineEnemy* TCannonManager::createEnemyInstance() { return new TCannon("砲台"); }

void TCannonManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TCannonSaveLoadParams("/enemy/cannon.prm");
}

TCannonManager::TCannonManager(const char* name)
    : TSmallEnemyManager(name)
{
}

TCannonSaveLoadParams::TCannonSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLHideDist, 300.0f)
    , PARAM_INIT(mSLBombDist, 2000.0f)
    , PARAM_INIT(mSLKillerDist, 10000.0f)
    , PARAM_INIT(mSLBombInterval, 100)
    , PARAM_INIT(mSLKillerInterval, 50)
    , PARAM_INIT(mSLShootInterval, 100)
    , PARAM_INIT(mSLChorobeiAttackRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiAttackHeight, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageRadius, 100.0f)
    , PARAM_INIT(mSLChorobeiDamageHeight, 100.0f)
    , PARAM_INIT(mSLKillerTransYOffset, -50.0f)
    , PARAM_INIT(mSLBombHeiGenerateRate, 0.7f)
    , PARAM_INIT(mSLThrowXZSpeed, 12.0f)
{
	TParams::load(mPrmPath);
}
