#include <Enemy/Koopa.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/HitActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/MapData.hpp>
#include <Map/Map.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/LightUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <MoveBG/Item.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSe.hpp>
#include <MSound/SoundEffects.hpp>
#include <Camera/CameraShake.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JGeometry.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static const Vec koopa_zero = { 0.0f, 0.0f, 0.0f };
static const Vec koopa_one  = { 1.0f, 1.0f, 1.0f };

static const char* koopa_bastable[] = {
	"/scene/koopa/bas/koopa_down.bas",
	nullptr,
	"/scene/koopa/bas/koopa_fall.bas",
	"/scene/koopa/bas/koopa_fire_end.bas",
	"/scene/koopa/bas/koopa_fire_loop.bas",
	"/scene/koopa/bas/koopa_fire_start.bas",
	"/scene/koopa/bas/koopa_first.bas",
	"/scene/koopa/bas/koopa_getup.bas",
	"/scene/koopa/bas/koopa_hipdrop.bas",
	"/scene/koopa/bas/koopa_stagger.bas",
	"/scene/koopa/bas/koopa_turn_l.bas",
	"/scene/koopa/bas/koopa_turn_r.bas",
	"/scene/koopa/bas/koopa_wait.bas",
	nullptr,
	"/scene/koopa/bas/koopa_waterhit.bas",
};

// ============= nerves =============

#pragma dont_inline on
static f32 fmodDeg(f32 x, f32 y) { return std::fmodf(x, y); }
static f32 modDeg(f32 x, f32 y) { return JGeometry::TUtil<f32>::mod(x, y); }
// Retail calls TEnemyManager::getSaveParam / TKoopa::changeAnm out of line here.
static TKoopaSaveLoadParams* saveParamCall(TEnemyManager* mgr)
{
	return (TKoopaSaveLoadParams*)mgr->getSaveParam();
}
static void changeAnmCall(TKoopa* self, int bck, int btp, f32 rate)
{
	self->changeAnm(bck, btp, rate);
}
#pragma dont_inline off

#define KOOPA_PARAMS(self) \
	((TKoopaSaveLoadParams*)((TEnemyManager*)(self)->mManager)->unk38)
#define KOOPA_SAVE(self) saveParamCall((TEnemyManager*)(self)->mManager)

// asm 0x8033A128 (retail: first params read inline, the rest out of line)
BOOL TNerveKoopaTurnL::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x160];
	TKoopa* self = (TKoopa*)spine->getBody();
	bool turning;

	f32 diff = self->mTargetDir - self->mRotation.y;
	diff = -180.0f + fmodDeg(360.0f + (diff - -180.0f), 360.0f);

	if (diff < -((TKoopaSaveLoadParams*)((TEnemyManager*)self->mManager)->unk38)
	               ->turnSpeed.get()) {
		f32 turnSpeed = -KOOPA_SAVE(self)->turnSpeed.get();
		if (turnSpeed > 0.0f)
			changeAnmCall(self, 0xB, 0, turnSpeed * KOOPA_SAVE(self)->turnAnim.get());
		else
			changeAnmCall(self, 0xA, 0, -turnSpeed * KOOPA_SAVE(self)->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + modDeg(360.0f + ((self->mRotation.y + turnSpeed) - -180.0f), 360.0f);
		turning = true;
	} else if (diff < 0.0f) {
		if (diff > 0.0f)
			changeAnmCall(self, 0xB, 0, diff * KOOPA_SAVE(self)->turnAnim.get());
		else
			changeAnmCall(self, 0xA, 0, -diff * KOOPA_SAVE(self)->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + modDeg(360.0f + ((self->mRotation.y + diff) - -180.0f), 360.0f);
		turning = true;
	} else {
		turning = false;
	}

	return turning ? false : true;
}

BOOL TNerveKoopaTurnR::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x158];
	TKoopa* self = (TKoopa*)spine->getBody();
	bool turning;

	f32 diff = self->mTargetDir - self->mRotation.y;
	diff = -180.0f + fmodDeg(360.0f + (diff - -180.0f), 360.0f);

	if (diff > ((TKoopaSaveLoadParams*)((TEnemyManager*)self->mManager)->unk38)
	               ->turnSpeed.get()) {
		f32 turnSpeed = KOOPA_SAVE(self)->turnSpeed.get();
		if (turnSpeed > 0.0f)
			changeAnmCall(self, 0xB, 0, turnSpeed * KOOPA_SAVE(self)->turnAnim.get());
		else
			changeAnmCall(self, 0xA, 0, -turnSpeed * KOOPA_SAVE(self)->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + modDeg(360.0f + ((self->mRotation.y + turnSpeed) - -180.0f), 360.0f);
		turning = true;
	} else if (diff > 0.0f) {
		if (diff > 0.0f)
			changeAnmCall(self, 0xB, 0, diff * KOOPA_SAVE(self)->turnAnim.get());
		else
			changeAnmCall(self, 0xA, 0, -diff * KOOPA_SAVE(self)->turnAnim.get());
		self->mRotation.y
		    = -180.0f
		      + modDeg(360.0f + ((self->mRotation.y + diff) - -180.0f), 360.0f);
		turning = true;
	} else {
		turning = false;
	}

	return turning ? false : true;
}

BOOL TNerveKoopaWait::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[0x90];
	TKoopa* self               = (TKoopa*)spine->getBody();

	if (self->unk19C > 0) {
		self->changeAnm(0xC, 1, KOOPA_PARAMS(self)->waitSpeed.get());
		return false;
	}

	TBathtub* bathtub
	    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

	JGeometry::TVec3<f32> marioSpeed;
	marioSpeed.set(*gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);

	JGeometry::TVec3<f32> speed;
	TKoopaSaveLoadParams* estWait = KOOPA_SAVE(self);
	speed.x = marioSpeed.x * estWait->marioEstimationWait.get();
	speed.y = marioSpeed.y * estWait->marioEstimationWait.get();
	speed.z = marioSpeed.z * estWait->marioEstimationWait.get();

	u8 grip = bathtub->getNextGrip(*gpMarioPos, speed,
	                               KOOPA_PARAMS(self)->waitRange.get(), &self->mTargetDir);
	if (!grip) {
		JGeometry::TVec3<f32> speed2;
		TKoopaSaveLoadParams* estFire = KOOPA_SAVE(self);
		speed2.x = marioSpeed.x * estFire->marioEstimationFire.get();
		speed2.y = marioSpeed.y * estFire->marioEstimationFire.get();
		speed2.z = marioSpeed.z * estFire->marioEstimationFire.get();
		self->mTargetDir = bathtub->getNextJuncture(*gpMarioPos, speed2);
	}

	f32 diff = self->mTargetDir - self->mRotation.y;
	diff = -180.0f + fmodDeg(360.0f + (diff - -180.0f), 360.0f);

	int side;
	if (diff < -KOOPA_PARAMS(self)->focusRange.get())
		side = -1;
	else if (diff > KOOPA_PARAMS(self)->focusRange.get())
		side = 1;
	else
		side = 0;

	if (grip) {
		switch (side) {
		case -1:
			spine->pushNerve(&TNerveKoopaTurnL::theNerve());
			break;
		case 1:
			spine->pushNerve(&TNerveKoopaTurnR::theNerve());
			break;
		case 0:
		default:
			self->changeAnm(0xC, 1, KOOPA_PARAMS(self)->waitSpeed.get());
			bool tumble = self->getAnmIndex() == 0xC
			              && (self->endsAnm() || self->passesAnmFrame(2.0f)
			                  || self->passesAnmFrame(400.0f)
			                  || self->passesAnmFrame(700.0f));
			if (tumble) {
				bathtub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
				if (bathtub->allowsTumble())
					spine->pushNerve(&TNerveKoopaTumble::theNerve());
			}
			break;
		}
	} else {
		switch (side) {
		case -1:
			spine->pushNerve(&TNerveKoopaTurnL::theNerve());
			break;
		case 1:
			spine->pushNerve(&TNerveKoopaTurnR::theNerve());
			break;
		case 0:
		default:
			self->mTargetDir = self->getTargetDir(*gpMarioPos);
			f32 newDiff     = self->mTargetDir - self->mRotation.y;
			newDiff = -180.0f + fmodDeg(360.0f + (newDiff - -180.0f), 360.0f);
			self->mIgnoreMario = newDiff < 0.0f;
			spine->setNext(&TNerveKoopaFlame::theNerve());
			break;
		}
	}

	return false;
}

BOOL TNerveKoopaTumble::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();

	self->changeAnm(0x8, 0, self->getParam()->tumbleSpeed.get());

	(void)self->mMActor->getFrameCtrl(ANM_TYPE_BCK); // dead call in target

	if (spine->getTime() == 0xBE) {
		gpCameraShake->startShake((EnumCamShakeMode)0x27, 1.0f);

		static TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xF5, *bathtub->getRootJointMtx(), 0, this);

		if (SMS_IsMarioTouchGround4cm())
			SMSRumbleMgr->start(1, static_cast<f32*>(nullptr));
	}

	return self->endsAnm();
}

BOOL TNerveKoopaFall::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[8];
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(0x2, 0, self->getParam()->fallSpeed.get());
	return false;
}

BOOL TNerveKoopaFlame::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self               = (TKoopa*)spine->getBody();

	switch (self->getAnmIndex()) {
	case 5:
		if (self->endsAnm()) {
			self->changeAnm(0x4, 0, 2.0f); // asm 0x80336208: constant, not fireSpeed
			spine->setNext(&TNerveKoopaFlame::theNerve());
		} else {
			self->unk155 = 0;
		}
		break;

	case 3:
		if (self->endsAnm()) {
			if (self->unk155) {
				self->laugh();
				self->unk155 = 0;
			}

			if (self->unk19C > 0) {
				spine->setNext(&TNerveKoopaWait::theNerve());
				return false;
			}

			TBathtub* bathtub
			    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

			JGeometry::TVec3<f32> marioSpeed;
			marioSpeed.set(*gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);

			JGeometry::TVec3<f32> speed;
			TKoopaSaveLoadParams* estWait = KOOPA_SAVE(self);
			speed.x = marioSpeed.x * estWait->marioEstimationWait.get();
			speed.y = marioSpeed.y * estWait->marioEstimationWait.get();
			speed.z = marioSpeed.z * estWait->marioEstimationWait.get();

			u8 grip = bathtub->getNextGrip(*gpMarioPos, speed,
			                               KOOPA_PARAMS(self)->waitRange.get(),
			                               &self->mTargetDir);
			if (!grip) {
				JGeometry::TVec3<f32> speed2;
				TKoopaSaveLoadParams* estFire = KOOPA_SAVE(self);
				speed2.x = marioSpeed.x * estFire->marioEstimationFire.get();
				speed2.y = marioSpeed.y * estFire->marioEstimationFire.get();
				speed2.z = marioSpeed.z * estFire->marioEstimationFire.get();
				self->mTargetDir
				    = bathtub->getNextJuncture(*gpMarioPos, speed2);
			}

			f32 diff = self->mTargetDir - self->mRotation.y;
			diff = -180.0f + fmodDeg(360.0f + (diff - -180.0f), 360.0f);

			int side;
			if (diff < -KOOPA_PARAMS(self)->focusRange.get())
				side = -1;
			else if (diff > KOOPA_PARAMS(self)->focusRange.get())
				side = 1;
			else
				side = 0;

			if (grip) {
				spine->setNext(&TNerveKoopaWait::theNerve());
			} else {
				switch (side) {
				case -1:
					spine->pushNerve(&TNerveKoopaTurnL::theNerve());
					break;
				case 1:
					spine->pushNerve(&TNerveKoopaTurnR::theNerve());
					break;
				case 0:
					self->mTargetDir = self->getTargetDir(*gpMarioPos);
					f32 newDiff     = self->mTargetDir - self->mRotation.y;
					newDiff         = -180.0f
					          + fmodDeg(360.0f + (newDiff - -180.0f), 360.0f);
					self->mIgnoreMario = newDiff < 0.0f;
					self->changeAnm(0x5, 0, KOOPA_PARAMS(self)->fireSpeed.get());
					spine->setNext(&TNerveKoopaFlame::theNerve());
					break;
				}
			}
		}
		break;

	case 4:
		if (spine->getTime() >= KOOPA_PARAMS(self)->flameCount.get()) {
			if ((spine->getTime() & 7) == 0) {
				TBathtub* bathtub
				    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

				JGeometry::TVec3<f32> marioSpeed;
				marioSpeed.set(*gpMarioSpeedX, *gpMarioSpeedY, *gpMarioSpeedZ);

				JGeometry::TVec3<f32> speed;
				TKoopaSaveLoadParams* estWait = KOOPA_SAVE(self);
				speed.x = marioSpeed.x * estWait->marioEstimationWait.get();
				speed.y = marioSpeed.y * estWait->marioEstimationWait.get();
				speed.z = marioSpeed.z * estWait->marioEstimationWait.get();

				u8 grip = bathtub->getNextGrip(*gpMarioPos, speed,
				                               KOOPA_PARAMS(self)->waitRange.get(),
				                               &self->mTargetDir);
				if (!grip) {
					JGeometry::TVec3<f32> speed2;
					TKoopaSaveLoadParams* estFire = KOOPA_SAVE(self);
					speed2.x = marioSpeed.x * estFire->marioEstimationFire.get();
					speed2.y = marioSpeed.y * estFire->marioEstimationFire.get();
					speed2.z = marioSpeed.z * estFire->marioEstimationFire.get();
					self->mTargetDir
					    = bathtub->getNextJuncture(*gpMarioPos, speed2);
				}

				f32 diff = self->mTargetDir - self->mRotation.y;
				diff = -180.0f + fmodDeg(360.0f + (diff - -180.0f), 360.0f);

				int side;
				if (diff < -KOOPA_PARAMS(self)->focusRange.get())
					side = -1;
				else if (diff > KOOPA_PARAMS(self)->focusRange.get())
					side = 1;
				else
					side = 0;

				if (grip || side != 0)
					self->changeAnm(0x3, 0, KOOPA_PARAMS(self)->fireSpeed.get());
			} else if (self->endsAnm()
			           && spine->getTime() >= KOOPA_PARAMS(self)->flameFocusEndStep.get()) {
				self->changeAnm(0x3, 0, KOOPA_PARAMS(self)->fireSpeed.get());
			}
		}
		break;

	default:
		self->changeAnm(0x5, 0, KOOPA_PARAMS(self)->fireSpeed.get());
		break;
	}

	return false;
}

BOOL TNerveKoopaProvoke::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();

	self->changeAnm(0x6, 0, 2.0f);
	if (self->endsAnm())
		spine->setNext(&TNerveKoopaWait::theNerve());

	return false;
}

BOOL TNerveKoopaStagger::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(0x9, 0, self->getParam()->staggerSpeed.get());
	return self->endsAnm();
}

BOOL TNerveKoopaGetShowered::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();
	self->changeAnm(0xE, 0, self->getParam()->waterhitSpeed.get());
	return self->endsAnm();
}

BOOL TNerveKoopaGetDown::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopa* self = (TKoopa*)spine->getBody();

	switch (self->getAnmIndex()) {
	case 0:
		if (self->endsAnm())
			self->changeAnm(0x1, 0, self->getParam()->downSpeed.get());
		break;
	case 1: {
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		if (spine->getTime() * (bathtub->getNumGripsDead() + 2)
		        >= self->getParam()->downStep.get()
		    && self->endsAnm())
			self->changeAnm(0x7, 0, self->getParam()->downSpeed.get());
		break;
	}
	case 7:
		if (self->endsAnm())
			return true;
		break;
	default: {
		self->changeAnm(0x0, 0, self->getParam()->downSpeed.get());
		static TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		gpMarioParticleManager->emitAndBindToMtx(
		    0xF5, *bathtub->getRootJointMtx(), 0, this);
		break;
	}
	}

	return false;
}

// ============= parts =============

TKoopaParts::TKoopaParts(const char* name, u32 actorType, TKoopa* koopa,
                         f32 radius)
    : THitActor(name)
    , mOwner(koopa)
{
	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(this);
	initHitActor(actorType, 5, -0x78000000, radius, radius, radius, radius);
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_UNK10000000);
	onHitFlag(HIT_FLAG_UNK8000000);
}

void TKoopaParts::perform(u32 cue, JDrama::TGraphics* graphics)
{
	THitActor::perform(cue, graphics);

	if (cue & CUE_MOVE) {
		control();
		for (int i = 0; i < mColCount; ++i)
			attack_(mCollisions[i]);
	}
}

void TKoopaParts::set(const JGeometry::TVec3<f32>& pos, f32 radius, f32 height)
{
	mPosition.set(pos);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mAttackRadius = radius;
	mAttackHeight = height;
	mDamageRadius = radius;
	mDamageHeight = height;
	calcEntryRadius();
}

void TKoopaParts::remove()
{
	onHitFlag(HIT_FLAG_CANNOT_ATTACK);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

// ============= TKoopaFlame =============

TKoopaFlame::TKoopaFlame(TKoopa* koopa)
    // asm 0x80334FCC: retail actorType is 0x08000029 (lis 0x800 + addi 0x29)
    : TKoopaParts("クッパの吐く炎", 0x08000029, koopa, 100.0f)
    // asm 0x80334FDC: retail ctor stores only mMaxLength (0x88) and mCurLength (0x8c), no mSpeed (0x84) store
    , mMaxLength(0.0f)
    , mCurLength(1.0f)
{
}

bool TKoopaFlame::isAlive() const { return mCurLength < mMaxLength; }

f32 TKoopaFlame::getLength() const { return mCurLength; }

void TKoopaFlame::resetFlame()
{
	mMaxLength = 0.0f;
	mCurLength = 1.0f;
}

void TKoopaFlame::fire(const JGeometry::TVec3<f32>& pos,
                       const JGeometry::TVec3<f32>& dir, f32 speed, f32 maxLen,
                       f32 radius, f32 height)
{
	mPosition.set(pos);
	mDirection.set(dir);
	mBase.set(pos);
	mSpeed     = speed;
	mMaxLength = maxLen;
	mCurLength = 0.0f;
	mRadius    = radius;
	mHeight    = height;
}

void TKoopaFlame::control()
{
	if (!(mCurLength < mMaxLength)) {
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		return;
	}

	mCurLength += mSpeed;
	f32 x = mDirection.x * mCurLength + mBase.x;
	f32 y = mDirection.y * mCurLength + mBase.y;
	f32 z = mDirection.z * mCurLength + mBase.z;

	f32 height = mHeight;
	if (height <= 0.0f)
		height = 2.0f * mRadius;

	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);
	offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_NO_COLLISION);
	mAttackRadius = mRadius;
	mAttackHeight = height;
	mDamageRadius = mRadius;
	mDamageHeight = height;
	calcEntryRadius();
}

BOOL TKoopaFlame::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		return false;
	default:
		return true;
	}
}

void TKoopaFlame::attack_(THitActor* other)
{
	if (other->receiveMessage(this, HIT_MESSAGE_UNKA)
	    && other == gpMarioAddress) {
		char trash[0x18];
		JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);
		SMS_ThrowMario(dir, mOwner->getParam()->flameJump.get());
		mOwner->unk155 = 1;
		changeAnmCall(mOwner, 0x3, 0, mOwner->getParam()->fireSpeed.get());
		mOwner->unk19C = 0xF0;
	}
}

// ============= TKoopaHand =============

TKoopaHand::TKoopaHand(TKoopa* koopa)
    // asm 0x80335034: retail actorType is 0x0800002B (lis 0x800 + addi 0x2b)
    : TKoopaParts("クッパの手", 0x0800002B, koopa, 100.0f)
{
}

BOOL TKoopaHand::receiveMessage(THitActor* sender, u32 message) { return true; }

void TKoopaHand::attack_(THitActor* other)
{
	other->receiveMessage(this, HIT_MESSAGE_ATTACK);
}

// ============= TKoopaHead =============

TKoopaHead::TKoopaHead(TKoopa* koopa)
    // asm 0x8033506C: retail actorType is 0x0800002A (lis 0x800 + addi 0x2a)
    : TKoopaParts("クッパの頭", 0x0800002A, koopa, 100.0f)
{
}

BOOL TKoopaHead::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x10];
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		if (mOwner->getShowered()) {
			gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
			gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK,
			                        &mOwner->mPosition, 0, 0.0f, 0, 0, 4);
		}
		break;
	case HIT_MESSAGE_ATTACK:
		if (!sender->isActorType(0x08000024))
			break;

		TKoopa* koopa = mOwner;
		if (&TNerveKoopaFall::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaProvoke::theNerve() != koopa->mSpine->getCurrentNerve()
		    && koopa->mSpine->getCurrentNerve() != &TNerveKoopaFlame::theNerve()
		    && &TNerveKoopaTumble::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaGetDown::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaGetShowered::theNerve()
		           != koopa->mSpine->getCurrentNerve())
			koopa->mSpine->pushNerve(&TNerveKoopaStagger::theNerve());
		break;
	}
	return true;
}

void TKoopaHead::attack_(THitActor* other)
{
	if (other->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && other == SMS_GetMarioLiveActor()) {
		JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);
		SMS_ThrowMario(dir, 60.0f);
	}
}

// ============= TKoopaBody =============

TKoopaBody::TKoopaBody(TKoopa* koopa)
    // asm 0x803350B0: retail actorType is 0x0800002A (lis 0x800 + addi 0x2a)
    : TKoopaParts("クッパの体", 0x0800002A, koopa, 100.0f)
{
}

BOOL TKoopaBody::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x10];
	switch (message) {
	case HIT_MESSAGE_SPRAYED_BY_WATER:
		break;
	case HIT_MESSAGE_ATTACK:
		if (!sender->isActorType(0x08000024))
			break;

		TKoopa* koopa = mOwner;
		if (&TNerveKoopaFall::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaProvoke::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaFlame::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaTumble::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaGetDown::theNerve() != koopa->mSpine->getCurrentNerve()
		    && &TNerveKoopaGetShowered::theNerve()
		           != koopa->mSpine->getCurrentNerve())
			koopa->mSpine->pushNerve(&TNerveKoopaStagger::theNerve());
		break;
	}
	return true;
}

void TKoopaBody::attack_(THitActor* other)
{
	if (other->receiveMessage(this, HIT_MESSAGE_ATTACK)
	    && other == SMS_GetMarioLiveActor()) {
		JGeometry::TVec3<f32> dir(0.0f, 1.0f, 0.0f);
		SMS_ThrowMario(dir, 60.0f);
	}
}

// ============= TKoopa =============

f32 TKoopa::getFlameDirDegree() const
{
	char trash[8];
	f32 degree = getFlameDirRate() * getParam()->flameNeckRange.get();
	return mRotation.y + (mIgnoreMario ? -degree : degree);
}

namespace {

int KoopaNeckCallBack(J3DNode* node, int timing)
{
	if (timing != 0)
		return 1;

	TKoopa* koopa   = (TKoopa*)node->getCallBackUserData();
	J3DJoint* joint = (J3DJoint*)node;
	TMtx34f* jointMtx
	    = (TMtx34f*)j3dSys.mModel->getAnmMtx(joint->getJntNo());

	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	marioPos.y += 85.0f;
	marioPos.x -= jointMtx->at(0, 3);
	marioPos.y -= jointMtx->at(1, 3);
	marioPos.z -= jointMtx->at(2, 3);

	if (koopa->isFlaming()) {
		f32 angle = koopa->getFlameDirRate() * 6.2831855f
		            * koopa->getParam()->flameNeckRange.get() / 360.0f;
		f32 downAngle = angle * koopa->getParam()->flameNeckDownRate.get();
		if (downAngle > 0.0f)
			downAngle = -downAngle;
		if (koopa->mIgnoreMario)
			angle = -angle;

		TRotation3f rot;
		rot.setEularY(angle);
		jointMtx->concat(rot, *jointMtx);

		rot.setEularZ(downAngle);
		jointMtx->concat(*jointMtx, rot); // asm 0x80336F00-CC: jm * rotZ
	}

	f32 focus = koopa->getNeckFocus();

	JGeometry::TVec3<f32> aim(jointMtx->at(0, 1) * marioPos.x,
	                          jointMtx->at(1, 1) * marioPos.y,
	                          jointMtx->at(2, 1) * marioPos.z);
	f32 dot = aim.x + aim.y + aim.z;
	aim.x = jointMtx->at(0, 1) * -dot + marioPos.x;
	aim.y = jointMtx->at(1, 1) * -dot + marioPos.y;
	aim.z = jointMtx->at(2, 1) * -dot + marioPos.z;

	JGeometry::TVec3<f32> aimNorm;
	aimNorm.normalize(aim);

	marioPos.normalize();

	f32 proj = aimNorm.x * jointMtx->at(0, 0) + aimNorm.y * jointMtx->at(1, 0)
	           + aimNorm.z * jointMtx->at(2, 0);
	if (proj < 0.5)
		focus *= (1.0f + proj) / 1.5f; // asm 0x803370DC (1.0f, not 0.0f)

	JGeometry::TQuat4<f32> quat;
	JGeometry::TVec3<f32> axis(jointMtx->at(1, 0) * aimNorm.z
	                               - jointMtx->at(2, 0) * aimNorm.y,
	                           jointMtx->at(2, 0) * aimNorm.x
	                               - jointMtx->at(0, 0) * aimNorm.z,
	                           jointMtx->at(0, 0) * aimNorm.y
	                               - jointMtx->at(1, 0) * aimNorm.x);

	f32 axisLenSq = axis.squared();
	f32 axisLen = JGeometry::TUtil<f32>::sqrt(axisLenSq);

	if (axisLen <= JGeometry::TUtil<f32>::epsilon()) { // asm 0x8033714C compares sqrt'd length
		quat.x = 0.0f;
		quat.y = 0.0f;
		quat.z = 0.0f;
		quat.w = 1.0f; // asm 0x80337160-70
	} else {
		f32 angle = atan2f(axisLen,
		                   jointMtx->at(0, 0) * aimNorm.x
		                       + jointMtx->at(1, 0) * aimNorm.y
		                       + jointMtx->at(2, 0) * aimNorm.z);
		f32 halfAngle = focus * (0.5f * angle);
		f32 sinHalf   = sinf(halfAngle) / axisLen;
		quat.x        = axis.x * sinHalf;
		quat.y        = axis.y * sinHalf;
		quat.z        = axis.z * sinHalf;
		quat.w        = cosf(halfAngle);
	}

	// normalize the quaternion
	f32 lenSq = quat.x * quat.x + quat.y * quat.y + quat.z * quat.z
	            + quat.w * quat.w;
	if (lenSq <= JGeometry::TUtil<f32>::epsilon()) {
		quat.x = 0.0f; // asm 0x80337204: all four zeroed
		quat.y = 0.0f;
		quat.z = 0.0f;
		quat.w = 0.0f;
	} else {
		f32 inv = 1.0f * JGeometry::TUtil<f32>::inv_sqrt(lenSq);
		quat.x *= inv;
		quat.y *= inv;
		quat.z *= inv;
		quat.w *= inv;
	}

	if (!koopa->isFlaming()) {
		// asm 0x8033728C: aimNorm x marioPos (not quat)
		JGeometry::TVec3<f32> cross(aimNorm.y * marioPos.z - aimNorm.z * marioPos.y,
		                            aimNorm.z * marioPos.x - aimNorm.x * marioPos.z,
		                            aimNorm.x * marioPos.y - aimNorm.y * marioPos.x);

		f32 angle2 = atan2f(JGeometry::TUtil<f32>::sqrt(cross.squared()),
		                    aimNorm.x * marioPos.x + aimNorm.y * marioPos.y
		                        + aimNorm.z * marioPos.z);
		f32 dot2 = marioPos.x * jointMtx->at(0, 1)
		           + marioPos.y * jointMtx->at(1, 1)
		           + marioPos.z * jointMtx->at(2, 1);
		f32 signedAngle = fabsf(angle2);
		if (dot2 < 0.0f)
			signedAngle = -signedAngle;

		f32 halfAngle2 = 0.5f * (focus * signedAngle);

		JGeometry::TQuat4<f32> quat2;
		f32 sinHalf2 = sinf(halfAngle2);
		quat2.x      = 2.0f * (quat.x * quat.z + quat.w * quat.y) * sinHalf2;
		quat2.y      = 2.0f * (quat.y * quat.z - quat.w * quat.x) * sinHalf2; // asm 0x80337354 yz - wx
		quat2.z = (1.0f - 2.0f * quat.x * quat.x - 2.0f * quat.y * quat.y)
		          * sinHalf2;
		quat2.w = cosf(halfAngle2);

		f32 lenSq2 = quat2.x * quat2.x + quat2.y * quat2.y
		             + quat2.z * quat2.z + quat2.w * quat2.w;
		if (lenSq2 <= JGeometry::TUtil<f32>::epsilon()) {
			quat2.x = 0.0f; // asm 0x803373BC: all four zeroed
			quat2.y = 0.0f;
			quat2.z = 0.0f;
			quat2.w = 0.0f;
		} else {
			f32 inv = 1.0f * JGeometry::TUtil<f32>::inv_sqrt(lenSq2);
			quat2.x *= inv;
			quat2.y *= inv;
			quat2.z *= inv;
			quat2.w *= inv;
		}

		// asm 0x80337410-7C: all four results computed from the old quat before any store
		f32 nx = quat2.w * quat.x - quat2.z * quat.y + quat2.y * quat.z
		         + quat2.x * quat.w;
		f32 ny = quat2.w * quat.y + quat2.z * quat.x - quat2.x * quat.z
		         + quat2.y * quat.w;
		f32 nz = quat2.w * quat.z + quat2.x * quat.y - quat2.y * quat.x
		         + quat2.z * quat.w;
		f32 nw = quat2.w * quat.w - quat2.x * quat.x - quat2.y * quat.y
		         - quat2.z * quat.z;
		quat.x = nx;
		quat.y = ny;
		quat.z = nz;
		quat.w = nw;
	}

	TRotation3f rot2;
	rot2.setQuat(quat);
	jointMtx->concat(*jointMtx, rot2); // asm 0x803374B0-DC: jm * rot2
	PSMTXCopy(*jointMtx, j3dSys.mCurrentMtx);

	return 1;
}

} // namespace

inline void TKoopa::breathFlame()
{
	int freeIdx   = -1;
	bool anyDying = false;
	for (int i = 0; i < 10; ++i) {
		if (!mFlames[i]->isAlive())
			freeIdx = i;
		else if (mFlames[i]->getLength() < 2.0f * getParam()->flameRadius.get())
			anyDying = true;
	}

	if (!anyDying && freeIdx >= 0) {
		MtxPtr headMtx = getHeadMtx();
		JGeometry::TVec3<f32> dir(headMtx[0][0], 0.0f, headMtx[2][0]);
		JGeometry::TVec3<f32> pos(headMtx[0][3], headMtx[1][3] - 500.0f, headMtx[2][3]);
		// asm 0x80336A10-70: retail inlines setLength(); written out because the header pragma blocks that
		f32 lsq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
		if (lsq <= JGeometry::TUtil<f32>::epsilon()) {
			dir.x = dir.y = dir.z = 0.0f;
		} else {
			f32 inv = 1.0f * JGeometry::TUtil<f32>::inv_sqrt(lsq);
			dir.x *= inv;
			dir.y *= inv;
			dir.z *= inv;
		}
		mFlames[freeIdx]->fire(pos, dir, getParam()->flameVelocity.get(),
		                       4000.0f, getParam()->flameRadius.get(),
		                       getParam()->flameHeight.get());
	}
}

inline void TKoopa::resetFlame_()
{
	for (int i = 0; i < 10; ++i)
		mFlames[i]->resetFlame();
}

#pragma dont_inline on
void TKoopa::setUpHitActors()
{
	if (isBreathing())
		breathFlame();
	else
		resetFlame_();

	MtxPtr agoMtx = mMActor->getModel()->getAnmMtx(mAgoJntIdx);
	JGeometry::TVec3<f32> headPos(agoMtx[0][3], agoMtx[1][3] - 200.0f,
	                              agoMtx[2][3]);
	mHead->set(headPos, getParam()->headRadius.get(),
	           2.0f * getParam()->headRadius.get());
	mBody->set(mPosition, 800.0f, 2000.0f);
}
#pragma dont_inline off

void TKoopa::changeAnm(int bck, int btp, f32 rate)
{
	if (!mMActor->checkCurBckFromIndex(bck)) {
		mMActor->setBckFromIndex(bck);
		const char** table = getBasNameTable();
		setAnmSound(table == nullptr ? nullptr : table[bck]);
	}

	if (mMActor->getCurAnmIdx(ANM_TYPE_BTP) != btp)
		mMActor->setBtpFromIndex(btp);

	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setRate(0.5f * (rate * SMSGetAnmFrameRate()));
}

int TKoopa::getAnmIndex() const
{
	return mMActor->getCurAnmIdx(ANM_TYPE_BCK);
}

bool TKoopa::endsAnm() const
{
	return mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr) ? true : false;
}

void TKoopa::laugh()
{
	if (gpMSound->gateCheck(MSD_SE_BS_KOOPA_VO_LAUGH))
		MSoundSESystem::MSoundSE::startSoundActor(MSD_SE_BS_KOOPA_VO_LAUGH,
		                                          &mSoundPos, 0, nullptr, 0, 4);
}

bool TKoopa::isBreathing() const
{
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 4)
		return true;
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 5
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() >= 85.0f)
		return true;
	return false;
}

bool TKoopa::ignoresMario() const { return mIgnoreMario ? true : false; }

void TKoopa::setIgnoreMario(bool ignore) { mIgnoreMario = ignore; }

f32 TKoopa::getFlameDirRate() const
{
	f32 frame          = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
	f32 end            = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	f32 overStart      = getParam()->flameOverStart.get();
	s32 focusStart     = getParam()->flameFocusStartStep.get();
	s32 focusEnd       = getParam()->flameFocusEndStep.get();
	s32 time           = mSpine->getTime();

	f32 rate;
	switch (mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 5:
		rate = -(frame * overStart / end);
		break;
	case 4:
	case 3:
		if (time <= focusStart)
			rate = -overStart;
		else if (time <= focusEnd)
			rate = (1.0f + overStart) * (time - focusStart)
			       / (focusEnd - focusStart) - overStart;
		else
			rate = 1.0f;
		if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 3)
			rate *= 1.0f - frame / end;
		break;
	default:
		rate = 1.0f;
		break;
	}

	return rate;
}

bool TKoopa::isFlaming() const
{
	switch (mMActor->getCurAnmIdx(ANM_TYPE_BCK)) {
	case 3:
	case 4:
	case 5:
		return true;
	default:
		return false;
	}
}

f32 TKoopa::getNeckFocus() const
{
	char trash[0x8];
	int anmIdx           = mMActor->getCurAnmIdx(ANM_TYPE_BCK);
	J3DFrameCtrl* ctrl   = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	f32 frame            = ctrl->getFrame();
	f32 end              = ctrl->getEnd();

	f32 focus = 1.0f;
	switch (anmIdx) {
	case 6:
		focus = 0.0f;
		if (frame >= 164.0f)
			focus = (frame - 164.0f) / (end - 164.0f);
		break;
	case 1:
		focus = 0.0f;
		break;
	case 0:
		if (frame <= 40.0f)
			focus = 1.0f - frame / 40.0f;
		else
			focus = 0.0f;
		break;
	case 2:
		focus = 0.0f;
		break;
	case 7:
		if (frame <= 125.0f)
			focus = 0.0f;
		else
			focus = (frame - 125.0f) / (end - 125.0f);
		break;
	case 9:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 65.0f)
			focus = 0.0f;
		else
			focus = (frame - 65.0f) / (end - 65.0f);
		break;
	case 8:
		if (frame <= 30.0f)
			focus = 1.0f - frame / 30.0f;
		else if (frame <= 170.0f)
			focus = 0.0f;
		else
			focus = (frame - 170.0f) / (end - 170.0f);
		break;
	case 14:
		if (frame <= 20.0f)
			focus = 1.0f - frame / 20.0f;
		else if (frame <= 40.0f)
			focus = 0.0f;
		else
			focus = (frame - 40.0f) / (end - 40.0f);
		break;
	case 4:
		focus = 0.0f;
		break;
	case 5:
		if (frame <= 103.0f)
			focus = 1.0f - frame / 103.0f;
		else
			focus = 0.0f;
		break;
	case 3:
		focus = frame / end;
		break;
	case 12:
		if (frame <= 200.0f)
			focus = 1.0f;
		else if (frame <= 255.0f)
			focus = 1.0f - (frame - 200.0f) / 55.0f;
		else if (frame <= 330.0f)
			focus = 0.0f;
		else if (frame <= 390.0f)
			focus = (frame - 330.0f) / 60.0f;
		else if (frame <= 440.0f)
			focus = 1.0f;
		else if (frame <= 480.0f)
			focus = 1.0f - (frame - 440.0f) / 40.0f;
		else if (frame <= 555.0f)
			focus = 0.0f;
		else if (frame <= 615.0f)
			focus = (frame - 555.0f) / 60.0f;
		else
			focus = 1.0f;
		break;
	case 10:
	case 11:
	case 13:
	default:
		break;
	}

	return focus;
}

bool TKoopa::allowsLaunch() const
{
	return &TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve() ? false : true;
}

void TKoopa::getDown()
{
	char trash[0x10];
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		mSpine->setNext(&TNerveKoopaGetDown::theNerve());
	mSpine->pushNerve(&TNerveKoopaGetDown::theNerve());
}

bool TKoopa::effectsTumble() const
{
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve()) {
		int t = mSpine->getTime();
		if (t < 900 && t > 190)
			return true;
	}
	return false;
}

bool TKoopa::getShowered()
{
	char trash[0x20];
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetDown::theNerve() == mSpine->getCurrentNerve())
		return false;
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		return true;
	if (&TNerveKoopaStagger::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaGetShowered::theNerve());
		return true;
	}
	if (&TNerveKoopaFlame::theNerve() == mSpine->getCurrentNerve()) {
		mSpine->setNext(&TNerveKoopaWait::theNerve());
		return false;
	}
	mSpine->pushNerve(&TNerveKoopaGetShowered::theNerve());
	return true;
}

void TKoopa::stagger(bool force)
{
	char trash[0x18];
	if (&TNerveKoopaFall::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaProvoke::theNerve() == mSpine->getCurrentNerve())
		return;
	if (!force && &TNerveKoopaFlame::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaTumble::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaGetDown::theNerve() == mSpine->getCurrentNerve())
		return;
	if (&TNerveKoopaGetShowered::theNerve() == mSpine->getCurrentNerve())
		return;
	mSpine->pushNerve(&TNerveKoopaStagger::theNerve());
}

#pragma dont_inline on
f32 TKoopa::getTargetDir(const JGeometry::TVec3<f32>& target) const
{
	char trash[0xA8];
	TBathtub* bathtub
	    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	MtxPtr bathMtx = *bathtub->getRootJointMtx();

	f32 dx = target.x - bathMtx[0][3];
	f32 dy = target.y - bathMtx[1][3];
	f32 dz = target.z - bathMtx[2][3];

	f32 localZ = bathMtx[0][2] * dx + bathMtx[1][2] * dy + bathMtx[2][2] * dz;
	f32 localX = bathMtx[0][0] * dx + bathMtx[1][0] * dy + bathMtx[2][0] * dz;

	return matan(localZ, localX) * (360.0f / 65536.0f);
}
#pragma dont_inline off

f32 TKoopa::getAnmFrame() const
{
	return mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
}

f32 TKoopa::getAnmFrameNext() const
{
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	return ctrl->getFrame() + ctrl->getRate();
}

bool TKoopa::passesAnmFrame(f32 frame) const
{
	return getAnmFrame() <= frame && frame <= 0.005f + getAnmFrameNext();
}

void TKoopa::fall()
{
	mRotation.y = 180.0f;
	mSpine->setNext(&TNerveKoopaFall::theNerve());
}

void TKoopa::updateAnmSound()
{
	char trash[0x18];
	if (mMActor->getCurAnmIdx(ANM_TYPE_BCK) == 8) {
		mSoundPos.set(mPosition);
	} else {
		MtxPtr headMtx = mMActor->getModel()->getAnmMtx(mHeadJntIdx);
		f32 z = headMtx[2][3];
		f32 y = headMtx[1][3];
		f32 x = headMtx[0][3];
		mSoundPos.set(x, y, z);
	}

	if (mAnmSound != nullptr && mAnmSoundPath != nullptr) {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		mAnmSound->animeLoop(&mSoundPos, ctrl->getFrame(), ctrl->getRate(), 0,
		                     4);
	}
}

TKoopa::TKoopa(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TKoopa::load(JSUMemoryInputStream& stream) { TSpineEnemy::load(stream); }

void TKoopa::loadAfter()
{
	JDrama::TNameRef::loadAfter();

	for (int i = 0; i < 10; ++i)
		mFlames[i] = new TKoopaFlame(this);

	for (int i = 0; i < 2; ++i)
		mHands[i] = new TKoopaHand(this);

	mHead = new TKoopaHead(this);
	mBody = new TKoopaBody(this);
}

void TKoopa::init(TLiveManager* manager)
{
	char trash[0x30];
	mBodyRadius = 800.0f;
	mHeadHeight = 2000.0f;

	TSpineEnemy::init(manager);

	onHitFlag(HIT_FLAG_NO_COLLISION);
	onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
	offHitFlag(HIT_FLAG_CANNOT_ATTACK);

	mSpine->initWith(&TNerveKoopaProvoke::theNerve());

	changeAnm(0xC, 1, 2.0f);
	changeAnm(0x6, 0, 2.0f);

	if (mMActor->getAnmBck() != nullptr)
		mMActor->getAnmBck()->initSimpleMotionBlend(0x10);

	// initial facing: angle to mario in the bathtub's frame
	{
		JGeometry::TVec3<f32>* marioPos = gpMarioPos;
		TBathtub* bathtub
		    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
		MtxPtr bathMtx = *bathtub->getRootJointMtx();
		JGeometry::TVec3<f32> col0(bathMtx[0][0], bathMtx[1][0],
		                           bathMtx[2][0]);
		JGeometry::TVec3<f32> col2(bathMtx[0][2], bathMtx[1][2],
		                           bathMtx[2][2]);
		JGeometry::TVec3<f32> trans(bathMtx[0][3], bathMtx[1][3],
		                            bathMtx[2][3]);
		JGeometry::TVec3<f32> diff;
		diff.sub(*marioPos, trans);
		mTargetDir = matan(col2.dot(diff), col0.dot(diff)) * (360.0f / 65536.0f);
	}

	initAnmSound();
	reset();

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	for (u16 i = 0; i < jointNames->getNameNum(); ++i) {
	} // empty loop body in the retail build

	mAgoJntIdx  = jointNames->getIndex("ago");
	mHeadJntIdx = jointNames->getIndex("head");
	mNeckJntIdx = jointNames->getIndex("neck");

	J3DJoint* headJoint
	    = getModel()->getModelData()->getJointNodePointer(mHeadJntIdx);
	headJoint->setCallBack(KoopaNeckCallBack);
	headJoint->setCallBackUserData(this);

	unk1B8 = 0.0f;
	unk155 = 0;
}

const char** TKoopa::getBasNameTable() const { return koopa_bastable; }

MtxPtr TKoopa::getHeadMtx() const
{
	return mMActor->getModel()->getAnmMtx(mHeadJntIdx);
}

void TKoopa::reset()
{
	char trash[8];
	TSpineEnemy::reset();
	changeAnm(0xC, 1, getParam()->waitSpeed.get());
	unk19C = 0x258;
}

void TKoopa::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0x58];
	if (cue & CUE_MOVE) {
		unk1B8 = getNeckFocus();
		if (unk19C > 0)
			--unk19C;
	}

	TSpineEnemy::perform(cue, graphics);

	for (int i = 0; i < 10; ++i)
		mFlames[i]->perform(cue, graphics);
	mHead->perform(cue, graphics);
	mHands[0]->perform(cue, graphics);
	mHands[1]->perform(cue, graphics);
	mBody->perform(cue, graphics);

	if (cue & CUE_MOVE) {
		f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		if (mSpine->getCurrentNerve() == &TNerveKoopaTumble::theNerve()
		    && frame >= getParam()->tumbleStartFrame.get()
		    && frame <= getParam()->tumbleEndFrame.get()) {
			TBathtub* bathtub
			    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
			bathtub->tumble(mRotation.y, getParam()->tumbleWeight.get());
		}
		setUpHitActors();
	}

	if (cue & CUE_CALC_ANIM) {
		if (isBreathing()
		    || (getAnmIndex() == 6 && getAnmFrame() >= 68.0f
		        && getAnmFrame() <= 164.0f)) {
			mMActor->calc();
			f32 flameScale = getParam()->flameScale.get();
			JGeometry::TVec3<f32> scale(flameScale, flameScale, flameScale);
			JPABaseEmitter* emitter;
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1F3, getHeadMtx(), 3, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C3, getHeadMtx(), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C2, getHeadMtx(), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C1, getHeadMtx(), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
			emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
			    0x1C0, getHeadMtx(), 1, this);
			if (emitter != nullptr)
				emitter->setGlobalScale(scale);
		}
	}
}

BOOL TKoopa::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

void TKoopa::calcRootMatrix()
{
	TBathtub* bathtub
	    = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");
	MtxPtr bathMtx = *bathtub->getRootJointMtx();

	JGeometry::TVec3<f32> offset(bathMtx[0][1] * -1500.0f,
	                             bathMtx[1][1] * -1500.0f,
	                             bathMtx[2][1] * -1500.0f);

	TPosition3f mtx;
	MsMtxSetRotRPH(mtx, 0.0f, mRotation.y, 0.0f);
	mtx.ref(0, 3) = 0.0f;
	mtx.ref(1, 3) = 0.0f;
	mtx.ref(2, 3) = 0.0f;
	mtx.concat((TSMtx34f&)*bathMtx, mtx);

	mPosition.set(mtx.at(0, 3), mtx.at(1, 3), mtx.at(2, 3));
	mPosition += offset;
	mtx.setTrans(mPosition);

	J3DModel* model = getModel();
	PSMTXCopy(mtx, model->getBaseTRMtx());
	model->setBaseScale(JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	mScaling.set(1.0f, 1.0f, 1.0f);
}

TKoopaSaveLoadParams* TKoopa::getParam() const
{
	return (TKoopaSaveLoadParams*)((TEnemyManager*)mManager)->getSaveParam();
}

// ============= TKoopaManager =============

TKoopaManager::TKoopaManager(const char* name)
    : TEnemyManager(name)
{
}

TSpineEnemy* TKoopaManager::createEnemyInstance() { return nullptr; }

void TKoopaManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopa_model.bmd", 0x14240000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TKoopaSaveLoadParams("/enemy/koopa.prm");
}

void TKoopaManager::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_a.jpa", 0x1C0);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_b.jpa", 0x1C1);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_c.jpa", 0x1C2);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_d.jpa", 0x1C3);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_hipdrop.jpa", 0xF5);
	SMS_LoadParticle("/scene/koopa/jpa/ms_kp_fire_e.jpa", 0x1F3);
}
