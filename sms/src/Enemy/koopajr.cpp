#include <Enemy/KoopaJr.hpp>
#include <Enemy/Koopa.hpp>
#include <Enemy/BathtubBinder.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/HitActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/FlagManager.hpp>
#include <System/Particles.hpp>
#include <Map/MapData.hpp>
#include <Map/Map.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <Player/WaterGun.hpp>
#include <Player/NozzleBase.hpp>
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
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JGeometry.hpp>
#include <math.h>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// Retail calls the larger member functions of this unit out of line (e.g.
// appearShineKiller, calcNearerDirection, the TKoopaJrSubmarine constructor),
// whereas MWCC would inline them here, so automatic inlining is off; only the
// tiny TDirectionCalc helpers below turn it back on, as retail inlines those.
// Functions are defined in reverse order of the retail .text, because MWCC
// emits them in reverse definition order (-inline deferred).
#pragma auto_inline off

#pragma auto_inline on
#define DEFINE_THE_NERVE(Name)                                                 \
	const Name& Name::theNerve()                                               \
	{                                                                          \
		static Name instance;                                                  \
		return instance;                                                       \
	}

DEFINE_THE_NERVE(TNerveKoopaJrWait)
DEFINE_THE_NERVE(TNerveKoopaJrDemo)
DEFINE_THE_NERVE(TNerveKoopaJrLaunch)
DEFINE_THE_NERVE(TNerveKoopaJrYahoo)
DEFINE_THE_NERVE(TNerveKoopaJrSubmarineWait)
DEFINE_THE_NERVE(TNerveKoopaJrSubmarineCannonOpenClose)
DEFINE_THE_NERVE(TNerveKoopaJrSubmarineLaunchKiller)

#undef DEFINE_THE_NERVE
#pragma auto_inline off

static const char* koopajr_bastable[] = {
	"/scene/koopajr/bas/koopajr_damage.bas",
	"/scene/koopajr/bas/koopajr_shoot.bas",
	nullptr,
	"/scene/koopajr/bas/koopajr_yahoo.bas",
};

static const char* TKoopaJr_jointNameTable[] = {
	"KoopaJr_null",   "killer_null00", "killer_null03",
	"killer_null01", "killer_null04",
};

static int TKoopaJr_jointIndexTable[5];

static const char* koopajrsubmarine_bastable[] = { nullptr };

#pragma auto_inline on
TDirectionCalc::TDirectionCalc()
    : mDirection(0.0f)
{
}

TDirectionCalc::TDirectionCalc(f32 direction)
    : mDirection(direction)
{
}

TDirectionCalc::TDirectionCalc(JGeometry::TVec3<f32> vec) { makeDirection(vec); }

#pragma auto_inline off

// Retail calls mod/fmodf out of line; mwcc inlines the header versions otherwise.
#pragma dont_inline on
static f32 modOutOfLine(f32 value, f32 modulus)
{
	return JGeometry::TUtil<f32>::mod(value, modulus);
}
static f32 fmodfOutOfLine(f32 value, f32 modulus)
{
	return std::fmodf(value, modulus);
}
// Retail calls TRotation3::setQuat out of line.
static void setQuatOutOfLine(TPosition3f& mtx, const JGeometry::TQuat4<f32>& q)
{
	mtx.setQuat(q);
}
static void identity33OutOfLine(TPosition3f& mtx) { mtx.identity33(); }
// Retail calls TVec3::dot and TVec3::scale(f32, TVec3) out of line.
static f32 dotOutOfLine(const JGeometry::TVec3<f32>& a,
                        const JGeometry::TVec3<f32>& b)
{
	return a.dot(b);
}
static void scaleOutOfLine(JGeometry::TVec3<f32>& dst, f32 scale,
                           const JGeometry::TVec3<f32>& src)
{
	dst.scale(scale, src);
}
static void setOutOfLine(JGeometry::TVec3<f32>& v, f32 x, f32 y, f32 z)
{
	v.set(x, y, z);
}
#pragma dont_inline off

f32 TDirectionCalc::calcNearerDirection(f32 target)
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	f32 cur = mDirection;
	f32 lo  = 0.0f;
	mDirection = lo + fmodfOutOfLine(6.2831855f + (cur - lo), 6.2831855f);

	if (target >= mDirection) {
		f32 diff = target - mDirection;
		if (6.2831855f - diff < diff)
			target -= 6.2831855f;
	} else {
		f32 diff = mDirection - target;
		if (6.2831855f - diff < diff)
			target += 6.2831855f;
	}

	return target;
}

f32 TDirectionCalc::sub(f32 target)
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	f32 cur = mDirection;
	f32 lo  = 0.0f;
	mDirection = lo + modOutOfLine(6.2831855f + (cur - lo), 6.2831855f);

	if (target >= mDirection) {
		f32 diff = target - mDirection;
		if (6.2831855f - diff < diff)
			target -= 6.2831855f;
	} else {
		f32 diff = mDirection - target;
		if (6.2831855f - diff < diff)
			target += 6.2831855f;
	}

	return mDirection - target;
}

f32 TDirectionCalc::calcTurnDirection(f32 target, f32 step)
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	f32 lo = 0.0f;
	mDirection = lo + fmodfOutOfLine(6.2831855f + (mDirection - lo), 6.2831855f);
	mDirection = lo + modOutOfLine(6.2831855f + (mDirection - lo), 6.2831855f);

	if (target >= mDirection) {
		f32 diff = target - mDirection;
		if (6.2831855f - diff < diff)
			target -= 6.2831855f;
	} else {
		f32 diff = mDirection - target;
		if (6.2831855f - diff < diff)
			target += 6.2831855f;
	}

	if (target > mDirection) {
		f32 diff = target - mDirection;
		if (diff < step)
			step = diff;
		return mDirection + step;
	} else {
		f32 diff = mDirection - target;
		if (diff < step)
			step = diff;
		return mDirection - step;
	}
}


#pragma auto_inline on
void TDirectionCalc::makeDirection(JGeometry::TVec3<f32> vec)
{
	mDirection = atan2(vec.x, vec.z);
}

JGeometry::TVec3<f32> TDirectionCalc::calcDirectionVector()
{
	return JGeometry::TVec3<f32>(sin(mDirection), 0.0f, cos(mDirection));
}

#pragma auto_inline off

f32 TDirectionCalc::absDirection(f32 target)
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	f32 cur = mDirection;
	f32 lo  = 0.0f;
	mDirection = lo + modOutOfLine(6.2831855f + (cur - lo), 6.2831855f);

	if (target >= mDirection) {
		f32 diff = target - mDirection;
		if (6.2831855f - diff < diff)
			target -= 6.2831855f;
	} else {
		f32 diff = mDirection - target;
		if (6.2831855f - diff < diff)
			target += 6.2831855f;
	}

	return fabsf(mDirection - target);
}


#pragma auto_inline on
f32 TDirectionCalc::d2r(f32 degrees)
{
	f32 pi = 3.1415927f;
	return degrees * pi / 180.0f;
}

f32 TDirectionCalc::r2d(f32 radians) { return 180.0f * radians / 3.1415927f; }
#pragma auto_inline off


TKoopaJrParams::TKoopaJrParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLLaunchKillerLimit, 10000.0f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight, 4000.0f)
    , PARAM_INIT(mSLKoopaJrScale, 1.6f)
    , PARAM_INIT(mSLFastLaunchDistance, 10000.0f)
    , PARAM_INIT(mSLDamagePeriod, 360)
    , PARAM_INIT(mSLLaunchKillerPeriod, 1200)
    , PARAM_INIT(mSLLaunchKillerPeriodFast, 360)
{
	TParams::load(mPrmPath);

	mSLLaunchKillerLimit.set(4200.0f);
	mSLDamageRadius.set(100.0f);
	mSLDamageHeight.set(300.0f);
	mSLKoopaJrScale.set(2.0f);
	mSLFastLaunchDistance.set(4600.0f);
	mSLDamagePeriod.set(240);
	mSLLaunchKillerPeriodFast.set(360);
	mSLLaunchKillerPeriod.set(840);
}

TKoopaJrSubmarineParams::TKoopaJrSubmarineParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(killerTargetDistanceMin, 500.0f)
    , PARAM_INIT(killerTargetDistance, 500.0f)
    , PARAM_INIT(bottomHeight, 0.0f)
    , PARAM_INIT(centerZ, 0.0f)
    , PARAM_INIT(aboidKoopaFlameAngle, 0.31415927f)
    , PARAM_INIT(traceMarioAngle, 0.31415927f)
    , PARAM_INIT(mSLWavePhaseVelocity, 0.31415927f)
    , PARAM_INIT(mSLWaveAmplitudeMin, 0.37699112f)
    , PARAM_INIT(mSLWaveAmplitudeMaxLaunch, 0.37699112f)
    , PARAM_INIT(mSLWaveAmplitudeMax, 0.37699112f)
    , PARAM_INIT(mSLSwingPhaseVelocity, 0.31415927f)
    , PARAM_INIT(mSLSwingAmplitudeMin, 0.37699112f)
    , PARAM_INIT(mSLSwingAmplitudeMax, 0.37699112f)
    , PARAM_INIT(mSLRoundAngleVelocity, 0.05f)
    , PARAM_INIT(mSLRoundDistance, 1.0f)
    , PARAM_INIT(mSLAcceleration, 1.0f)
    , PARAM_INIT(mSLRotationSpeed, 1.0f)
    , PARAM_INIT(mSLSpeedMax, 8.0f)
    , PARAM_INIT(mSLKoopaJrSubmarineScale, 1.6f)
    , PARAM_INIT(mSLDamageRadius, 1000.0f)
    , PARAM_INIT(mSLDamageHeight, 4000.0f)
    , PARAM_INIT(shineKillerProbability0, 0.0f)
    , PARAM_INIT(shineKillerProbability1, 0.0f)
    , PARAM_INIT(mSLKillerIntervalFast, 30)
    , PARAM_INIT(mSLKillerInterval, 90)
{
	TParams::load(mPrmPath);

	killerTargetDistanceMin.set(500.0f);
	killerTargetDistance.set(700.0f);
	bottomHeight.set(0.0f);
	centerZ.set(200.0f);
	aboidKoopaFlameAngle.set(0.62831855f);
	traceMarioAngle.set(0.31415927f);
	mSLWavePhaseVelocity.set(0.09424778f);
	mSLWaveAmplitudeMin.set(0.12566371f);
	mSLWaveAmplitudeMaxLaunch.set(0.37699112f);
	mSLWaveAmplitudeMax.set(0.31415927f);
	mSLSwingPhaseVelocity.set(0.18849556f);
	mSLSwingAmplitudeMin.set(0.12566371f);
	mSLSwingAmplitudeMax.set(0.5654867f);
	mSLRoundAngleVelocity.set(0.12f);
	mSLRoundDistance.set(2000.0f);
	mSLAcceleration.set(1.0f);
	mSLRotationSpeed.set(1.0f);
	mSLSpeedMax.set(5.0f);
	mSLKoopaJrSubmarineScale.set(2.0f);
	mSLDamageRadius.set(240.0f);
	mSLDamageHeight.set(120.0f);
	shineKillerProbability0.set(0.5f);
	shineKillerProbability1.set(0.125f);
	mSLKillerInterval.set(90);
	mSLKillerIntervalFast.set(30);
}

BOOL TCallbackHitActor::receiveMessage(THitActor* sender, u32 message)
{
	return mOwner->receiveMessage(sender, message);
}

TKoopaJr::TKoopaJr(const char* name)
    : TSpineEnemy(name)
    , mBathtub(nullptr)
    , mKoopa(nullptr)
    , mSubmarine(nullptr)
    , mSubmarineManager(nullptr)
    , mKillerManager(nullptr)
{
	onLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}

void TKoopaJr::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);

	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("koopajr_model.bmd", 0);
	mMActor->setLightType(1);
	initAnmSound();

	f32 height = ((TKoopaJrParams*)getSaveParam())->mSLDamageHeight.value;
	initHitActor(0x8000028, 1, 0, 0.0f, 0.0f,
	             ((TKoopaJrParams*)getSaveParam())->mSLDamageRadius.value,
	             height);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	mSpine->initWith(&TNerveKoopaJrWait::theNerve());

	// Retail also reads the managers' unk38 right after each lookup and drops
	// the result.
	mKillerManager = (TBathtubKillerManager*)JDrama::TNameRefGen::search(
	    "バスタブキラーマネージャー");
	(void)(*(volatile u32*)&mKillerManager->unk38 == 0);

	if (!mSubmarineManager) {
		mSubmarineManager
		    = (TKoopaJrSubmarineManager*)JDrama::TNameRefGen::search(
		        "クッパジュニアサブマリンマネージャー");
		(void)(*(volatile u32*)&mSubmarineManager->unk38 == 0);
	}

	f32 scale = ((TKoopaJrParams*)getSaveParam())->mSLKoopaJrScale.value;
	mScaling.set(scale, scale, scale);

	mSpine->reset();
	unk150 = 0;
	unk154 = 0;
	unk158 = 0;
	unk154 = ((TKoopaJrParams*)getSaveParam())->mSLLaunchKillerPeriod.value;
	unk158 = 0;
}

void TKoopaJr::reset()
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	TSpineEnemy::reset();

	mSpine->reset();
	unk150 = 0;
	unk154 = 0;
	unk158 = 0;
	unk154 = ((TKoopaJrParams*)getSaveParam())->mSLLaunchKillerPeriod.value;
	unk158 = 0;
}

void TKoopaJr::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!mSubmarine) {
		mSubmarine = (TKoopaJrSubmarine*)mSubmarineManager->getObj(0);
		mSubmarine->mKoopaJr = this;
	}

	if (!mKoopa) {
		mKoopa = (TKoopa*)((TKoopaManager*)JDrama::TNameRefGen::search(
		                       "クッパマネージャー"))
		             ->getObj(0);
	}

	if (!mBathtub)
		mBathtub = (TBathtub*)JDrama::TNameRefGen::search("バスタブ");

	if (cue & CUE_CALC_ANIM) {
		TKoopaJrSubmarineParams* subParams
		    = (TKoopaJrSubmarineParams*)mSubmarine->getSaveParam();
		if (!(mSubmarine->mSwingAmplitude
		      < 0.5f * subParams->mSLSwingAmplitudeMax.value)) {
			unk150
			    = ((TKoopaJrParams*)getSaveParam())->mSLDamagePeriod.value;

			if (mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve())
				mSpine->pushNerve(&TNerveKoopaJrDamage::theNerve());

			if (mSpine->getCurrentNerve()
			        == &TNerveKoopaJrLaunch::theNerve()
			    || mSpine->getCurrentNerve()
			           == &TNerveKoopaJrYahoo::theNerve()) {
				mSpine->setNext(&TNerveKoopaJrDamage::theNerve());
			}
		}
	}

	if (cue & CUE_MOVE) {
		if (unk150 > 0)
			unk150 -= 1;
		if (unk154 > 0)
			unk154 -= 1;
		if (unk158 > 0)
			unk158 -= 1;

		if (!mBathtub->unk29A) {
			if (mBathtub->unk29A) {
				if (mSpine->getCurrentNerve() != &TNerveKoopaJrDemo::theNerve())
					mSpine->pushNerve(&TNerveKoopaJrDemo::theNerve());
			}

			if (mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve()) {
				checkNerveKillerLaunchNormal();
				checkNerveKillerLaunchFast();
				checkNerveKillerHit();
			}

			JGeometry::TVec3<f32> toMario;
			toMario.x = gpMarioPos->x - mPosition.x;
			toMario.y = gpMarioPos->y - mPosition.y;
			toMario.z = gpMarioPos->z - mPosition.z;
			toMario.y = 0.0f;
			TDirectionCalc dir;
			dir.makeDirection(toMario);
			mRotation.y = TDirectionCalc::r2d(dir.mDirection);
		}
	}

	TSpineEnemy::perform(cue, graphics);
}

void TKoopaJr::calcRootMatrix()
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	J3DModel* model = getModel();

	if (mBathtub->unk29A) {
		getModel()->setBaseTRMtx(mBathtub->getKoopaJrMtxInDemo());
	} else {
		mSubmarine->getJointTransByIndex(TKoopaJr_jointIndexTable[0],
		                                 &mPosition);
		MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x, mPosition.y,
		               mPosition.z, mRotation.x, mRotation.y, mRotation.z);
	}

	model->setBaseScale(mScaling);
}

const char** TKoopaJr::getBasNameTable() const { return koopajr_bastable; }

BOOL TKoopaJr::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0, 0.0f,
		                        0, 0, 4);
		unk150 = ((TKoopaJrParams*)getSaveParam())->mSLDamagePeriod.get();

		if (mSpine->getCurrentNerve() == &TNerveKoopaJrWait::theNerve())
			mSpine->pushNerve(&TNerveKoopaJrDamage::theNerve());

		if (mSpine->getCurrentNerve() == &TNerveKoopaJrLaunch::theNerve()
		    || mSpine->getCurrentNerve() == &TNerveKoopaJrYahoo::theNerve()) {
			mSpine->setNext(&TNerveKoopaJrDamage::theNerve());
		}
		return TRUE;
	}
	return FALSE;
}

void TKoopaJr::checkNerveKillerLaunchNormal()
{
	if (unk154 > 0)
		return;

	int num = mBathtub->getNumKillerLaunchable();
	if (num == 0)
		return;

	TKoopaJrSubmarine* submarine = mSubmarine;
	if (num > 8)
		num = 8;

	submarine->mKillerIndex = 0;
	submarine->mKillerNum   = num;
	for (int i = 0; i < submarine->mKillerNum; ++i)
		submarine->mKillerKinds[i] = 0;

	if (submarine->appearShineKiller(submarine->mKillerNum))
		submarine->mKillerKinds[submarine->mKillerNum - 1] = 1;

	mSpine->pushNerve(&TNerveKoopaJrLaunch::theNerve());
	mSubmarine->mSpine->pushNerve(
	    &TNerveKoopaJrSubmarineCannonOpenClose::theNerve());
}

void TKoopaJr::checkNerveKillerLaunchFast()
{
	if (unk158 > 0)
		return;

	int num = mBathtub->getNumKillerBurstable();
	if (num == 0)
		return;

	TKoopaJrSubmarine* submarine = mSubmarine;
	if (num > 8)
		num = 8;

	submarine->mKillerIndex = 0;
	submarine->mKillerNum   = num;
	for (int i = 0; i < submarine->mKillerNum; ++i)
		submarine->mKillerKinds[i] = 2;

	if (submarine->appearShineKiller(submarine->mKillerNum))
		submarine->mKillerKinds[submarine->mKillerNum - 1] = 1;

	mSpine->pushNerve(&TNerveKoopaJrLaunch::theNerve());
	mSubmarine->mSpine->pushNerve(
	    &TNerveKoopaJrSubmarineCannonOpenClose::theNerve());
}

void TKoopaJr::checkNerveKillerHit()
{
	for (int i = 0; i < mKillerManager->getActiveObjNum(); ++i) {
		TBathtubKiller* killer = (TBathtubKiller*)mKillerManager->getObj(i);
		if (killer->unk21C == 1) {
			const TNerveBase<TLiveActor>* nerve = &TNerveKoopaJrYahoo::theNerve();
			mSpine->pushNerve(nerve);
			break;
		}
	}
}

BOOL TNerveKoopaJrWait::execute(TSpineBase<TLiveActor>* spine) const
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(2);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[2]);
		self->unk154
		    = ((TKoopaJrParams*)self->getSaveParam())->mSLLaunchKillerPeriod
		          .get();
		self->unk158 = ((TKoopaJrParams*)self->getSaveParam())
		                   ->mSLLaunchKillerPeriodFast.get();
	}

	return FALSE;
}

const TNerveKoopaJrDamage& TNerveKoopaJrDamage::theNerve()
{
	static TNerveKoopaJrDamage instance;
	return instance;
}

BOOL TNerveKoopaJrDamage::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(0);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[0]);
	}

	if (self->unk150 <= 0 && self->mMActor->isCurAnmAlreadyEnd(ANM_TYPE_BCK))
		return TRUE;
	return FALSE;
}

BOOL TNerveKoopaJrDemo::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(0);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[0]);
	}

	return FALSE;
}

BOOL TNerveKoopaJrLaunch::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(1);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[1]);
	}

	return self->mMActor->isCurAnmAlreadyEnd(ANM_TYPE_BCK) ? TRUE : FALSE;
}

BOOL TNerveKoopaJrYahoo::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopaJr* self = (TKoopaJr*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(3);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[3]);
	}

	return self->mMActor->isCurAnmAlreadyEnd(ANM_TYPE_BCK) ? TRUE : FALSE;
}

TKoopaJrManager::TKoopaJrManager(const char* name)
    : TEnemyManager(name)
{
}

void TKoopaJrManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "koopajr_model.bmd", 0x54220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TKoopaJrManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TKoopaJrParams("/enemy/koopajr.prm");
}

void TKoopaJrManager::loadAfter()
{
	static const char* onetimeFilenames[1] = {
		"/scene/koopajr/jpa/ms_koopajr_killer.jpa",
	};
	for (int i = 0; i < 1; ++i)
		SMS_LoadParticle(onetimeFilenames[i], 0xEF + i);
}

TSpineEnemy* TKoopaJrManager::createEnemyInstance() { return nullptr; }

TKoopaJrSubmarine::TKoopaJrSubmarine(const char* name)
    : TSpineEnemy(name)
    , mRoundAngle()
    , mHeading()
    , mAnmRate(0.0f)
    , mKoopaJr(nullptr)
{
	offLiveFlag(LIVE_FLAG_UNK10);
	offLiveFlag(LIVE_FLAG_UNK100);
}

void TKoopaJrSubmarine::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	initAnmSound();

	f32 damageHeight
	    = ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageHeight.value;
	initHitActor(
	    0x8000020, 0, 0, 0.0f, 0.0f,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageRadius.value,
	    damageHeight);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	mRearBodyHit = new TCallbackHitActor(
	    "サブマリンリアボディ", this,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageRadius.value,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageHeight.value);
	mFrontBodyHit = new TCallbackHitActor(
	    "サブマリンフロントボディ", this,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageRadius.value,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLDamageHeight.value);

	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("LastKoopaJrSubmarine.bmd", 0);
	mMActor->setLightType(1);

	mSpine->initWith(&TNerveKoopaJrSubmarineWait::theNerve());

	JUTNameTab* jointNames = getModel()->getModelData()->getJointName();
	for (int i = 0; i < 5; ++i) {
		TKoopaJr_jointIndexTable[i]
		    = jointNames->getIndex(TKoopaJr_jointNameTable[i]);
	}

	f32 scale = ((TKoopaJrSubmarineParams*)getSaveParam())
	                ->mSLKoopaJrSubmarineScale.value;
	mScaling.set(scale, scale, scale);

	mBathtubBinder = new TBathtubBinder;
	mBinder        = mBathtubBinder;

	resetKoopaJrSubmarine();
}

void TKoopaJrSubmarine::reset()
{
	TSpineEnemy::reset();
	resetKoopaJrSubmarine();
}

void TKoopaJrSubmarine::resetKoopaJrSubmarine()
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	mSpine->reset();
	unk150 = 0;

	mMActor->setBckFromIndex(0);
	const char** table = getBasNameTable();
	setAnmSound(!table ? nullptr : table[0]);

	mAnmRate = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getRate();

	mKillerIndex = 0;
	mKillerNum   = 0;
	for (int i = 0; i < 8; ++i)
		mKillerKinds[i] = 0;

	mQuat.set(0.0f, 0.0f, 0.0f, 1.0f);
	mHeading.mDirection    = 0.0f;
	mRoundAngle.mDirection = 0.0f;
	unk170                 = 0;
	mIsHit                 = 0;
	mSwingAmplitude        = 0.0f;
	mSwingPhase            = 0.0f;
	mWaveAmplitude         = 0.0f;
	mWavePhase             = 0.0f;

	mBathtubBinder->init(
	    150.0f, 100.0f, 150.0f, 100.0f,
	    ((TKoopaJrSubmarineParams*)getSaveParam())->bottomHeight.value);
}

void TKoopaJrSubmarine::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	if (cue & CUE_CALC_ANIM) {
		if (mSpine->getCurrentNerve()
		        == &TNerveKoopaJrSubmarineLaunchKiller::theNerve()
		    && mKillerIndex < mKillerNum && unk150 <= 0) {
			launchKiller();

			if (mKillerKinds[mKillerIndex] == 2) {
				unk150 = ((TKoopaJrSubmarineParams*)getSaveParam())
				             ->mSLKillerIntervalFast.get();
			} else {
				unk150 = ((TKoopaJrSubmarineParams*)getSaveParam())
				             ->mSLKillerInterval.get();
			}
			mKillerIndex += 1;
		}

		makeCollisionPositions();
		moveSwing();
	}

	if (cue & CUE_MOVE) {
		if (unk150 > 0)
			unk150 -= 1;
		checkNerve();
	}

	TSpineEnemy::perform(cue, graphics);
	mRearBodyHit->perform(cue, graphics);
	mFrontBodyHit->perform(cue, graphics);
}

void TKoopaJrSubmarine::makeCollisionPositions()
{
	char trash[0x38];
	f32 x = 0.0f;
	f32 y = 0.0f;
	f32 z = 0.0f;
	for (int i = 0; i < 2; ++i) {
		MtxPtr mtx = getModel()->getAnmMtx(TKoopaJr_jointIndexTable[i + 3]);
		x += mtx[0][3];
		y += mtx[1][3];
		z += mtx[2][3];
	}
	x *= 0.5f;
	y *= 0.5f;
	z *= 0.5f;
	mRearBodyHit->mPosition.set(x, y, z);

	getJointTransByIndex(TKoopaJr_jointIndexTable[0],
	                     &mFrontBodyHit->mPosition);
}

void TKoopaJrSubmarine::moveSwing()
{
	if (mIsHit) {
		mIsHit = 0;
		mSwingAmplitude += 0.06283186f;
		f32 amplitude = mSwingAmplitude;
		f32 max       = ((TKoopaJrSubmarineParams*)getSaveParam())
		                  ->mSLSwingAmplitudeMax.get();
		amplitude       = max >= amplitude ? amplitude : max;
		mSwingAmplitude = amplitude;
	}

	mSwingAmplitude -= 0.009424779f;
	f32 amplitude = mSwingAmplitude;
	f32 min       = ((TKoopaJrSubmarineParams*)getSaveParam())
	                  ->mSLSwingAmplitudeMin.get();
	mSwingAmplitude = min >= amplitude ? min : amplitude;
	if (mSwingAmplitude <= 0.0f)
		mSwingPhase = 0.0f;

	f32 lo = 0.0f;
	mSwingPhase
	    = lo
	      + fmodfOutOfLine(
	          6.2831855f
	              + (mSwingPhase
	                 + ((TKoopaJrSubmarineParams*)getSaveParam())
	                       ->mSLSwingPhaseVelocity.get()
	                 - lo),
	          6.2831855f);

	f32 speedRate = mVelocity.length()
	                / ((TKoopaJrSubmarineParams*)getSaveParam())
	                      ->mSLSpeedMax.get();

	if (unk150 > 0) {
		mWaveAmplitude += 0.03141593f;
		f32 waveAmplitude = mWaveAmplitude;
		f32 waveMax       = ((TKoopaJrSubmarineParams*)getSaveParam())
		                    ->mSLWaveAmplitudeMaxLaunch.get();
		waveAmplitude  = waveMax >= waveAmplitude ? waveAmplitude : waveMax;
		mWaveAmplitude = waveAmplitude;
	}

	if (speedRate > 0.5f) {
		mWaveAmplitude += 0.03141593f;
		f32 waveAmplitude = mWaveAmplitude;
		f32 waveMax       = ((TKoopaJrSubmarineParams*)getSaveParam())
		                    ->mSLWaveAmplitudeMax.get();
		waveAmplitude  = waveMax >= waveAmplitude ? waveAmplitude : waveMax;
		mWaveAmplitude = waveAmplitude;
	}

	mWaveAmplitude -= 0.018849557f;
	f32 waveAmplitude = mWaveAmplitude;
	f32 waveMin       = ((TKoopaJrSubmarineParams*)getSaveParam())
	                    ->mSLWaveAmplitudeMin.get();
	mWaveAmplitude = waveMin >= waveAmplitude ? waveMin : waveAmplitude;
	if (mWaveAmplitude <= 0.0f)
		mWavePhase = 0.0f;

	mWavePhase
	    = lo
	      + fmodfOutOfLine(
	          6.2831855f
	              + (mWavePhase
	                 + ((TKoopaJrSubmarineParams*)getSaveParam())
	                       ->mSLWavePhaseVelocity.get()
	                 - lo),
	          6.2831855f);
}

void TKoopaJrSubmarine::bind()
{
	JGeometry::TVec3<f32> nextPosition = mPosition;
	nextPosition.add(mLinearVelocity);
	nextPosition.add(mVelocity);
	mLinearVelocity = nextPosition - mPosition;

	mBathtubBinder->bind(this);
}

void TKoopaJrSubmarine::calcRootMatrix()
{
	if (mKoopaJr->mBathtub->unk29A) {
		getModel()->setBaseTRMtx(mKoopaJr->mBathtub->getSubmarineMtxInDemo());
	} else {
		JGeometry::TQuat4<f32> rollQuat;
		rollQuat.setEulerZ(mSwingAmplitude * sinf(mSwingPhase));
		JGeometry::TQuat4<f32> pitchQuat;
		pitchQuat.setEulerX(mWaveAmplitude * sinf(mWavePhase));
		JGeometry::TQuat4<f32> yawQuat;
		yawQuat.setEulerY(mHeading.mDirection);

		JGeometry::TQuat4<f32> rotation;
		rotation.mul(yawQuat, rollQuat);
		rotation.mul(rotation, pitchQuat);

		JGeometry::TVec3<f32> center;
		center.set(0.0f, 0.0f,
		           ((TKoopaJrSubmarineParams*)getSaveParam())->centerZ.value);

		TPosition3f centerMtx;
		identity33OutOfLine(centerMtx);
		centerMtx.setTrans(center);

		JGeometry::TVec3<f32> trans = center;
		trans.negate();
		trans.add(mPosition);

		TPosition3f mtx;
		setQuatOutOfLine(mtx, rotation);
		mtx.setTrans(trans);

		mtx.concat(mtx, centerMtx); // asm 0x8032A7C4-D0: standard mtx * centerMtx
		getModel()->setBaseTRMtx(mtx);
	}

	getModel()->setBaseScale(mScaling);
}

BOOL TKoopaJrSubmarine::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(0xE7, &sender->mPosition, 0, nullptr);
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &mPosition, 0, 0.0f,
		                        0, 0, 4);
		mIsHit = 1;
		return TRUE;
	}
	return FALSE;
}

bool TKoopaJrSubmarine::appearShineKiller(int)
{
	f32 probability;

	if (SMS_GetMarioWaterGun()->mCurrentWater == 0) {
		probability = 0.5f;
	} else if (mKoopaJr->mKillerManager->unk60
	           == TFlagManager::getInstance()->getFlag(0x20001)) {
		probability = 0.5f;
	} else {
		s32 maxWater
		    = ((const TWaterGun*)SMS_GetMarioWaterGun())
		          ->getCurrentNozzle()
		          ->mEmitParams.mAmountMax.value;
		s32 water = SMS_GetMarioWaterGun()->mCurrentWater;
		f32 p0    = ((TKoopaJrSubmarineParams*)getSaveParam())
		              ->shineKillerProbability0.value;
		f32 p1 = ((TKoopaJrSubmarineParams*)getSaveParam())
		             ->shineKillerProbability1.value;
		probability = p0 + (f32)water / (f32)maxWater * (p1 - p0);
	}

	bool appear = false;
	if (rand() * (1.0f / 32768.0f) < probability)
		appear = true;
	return appear;
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
}

void TKoopaJrSubmarine::launchKiller()
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 }; }
	int index = mKillerIndex % 4;

	TBathtubKiller* killer
	    = (TBathtubKiller*)mKoopaJr->mKillerManager->getDeadEnemy();
	if (killer != nullptr) {
		killer->unk194 = mKillerKinds[mKillerIndex];
		killer->reset();

		int jointIndex = index + 1;
		MtxPtr mtx = getModel()->getAnmMtx(TKoopaJr_jointIndexTable[jointIndex]);
		killer->mPosition.set(mtx[0][3], mtx[1][3], mtx[2][3]);

		JGeometry::TVec3<f32> velocity(mtx[0][2], mtx[1][2], mtx[2][2]);
		makeKillerVelocity(killer, velocity);

		if (gpMSound->gateCheck(MSD_SE_EN_KILLER_FIRE)) {
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_KILLER_FIRE, &killer->mPosition, 0, nullptr, 0, 4);
		}
	}
}

void TKoopaJrSubmarine::makeKillerVelocity(TBathtubKiller* killer,
                                           JGeometry::TVec3<f32> velocity)
{
	if (mKillerKinds[mKillerIndex] == 2) {
		velocity.set(0.0f, 1.0f, 0.0f);

		JGeometry::TVec3<f32> marioPos = *gpMarioPos;
		JGeometry::TVec3<f32> toMario;
		toMario.x = marioPos.x - killer->mPosition.x;
		toMario.y = marioPos.y - killer->mPosition.y;
		toMario.z = marioPos.z - killer->mPosition.z;
		toMario.y = 0.0f;
		toMario.normalize();

		JGeometry::TVec3<f32> axis;
		axis.cross(velocity, toMario);
		axis.normalize();

		JGeometry::TQuat4<f32> rotation;
		rotation.setRotate(axis, 0.62831855f);
		rotation.rotate(velocity, velocity);

		axis = toMario;
		int phase = mKillerIndex % 4;
		f32 angle = 0.62831855f;
		if (phase == 0)
			angle = -0.15707964f;
		else if (phase == 1)
			angle = 0.15707964f;
		else if (phase == 2)
			angle = -0.31415927f;
		else if (phase == 3)
			angle = 0.31415927f;
		rotation.setRotate(axis, angle);
		rotation.rotate(velocity, velocity);

		velocity.normalize();
		velocity.scale(killer->unk198.unkC);
	} else {
		velocity.normalize();
		velocity.scale(killer->unk198.unkC);

		JGeometry::TVec3<f32> marioPos = *gpMarioPos;
		marioPos.y = (*mKoopaJr->mBathtub->getRootJointMtx())[1][3];

		JGeometry::TVec3<f32> toMario(marioPos.x - killer->mPosition.x, 0.0f,
		                              marioPos.z - killer->mPosition.z);

		f32 distance = toMario.length()
		               - ((TKoopaJrSubmarineParams*)getSaveParam())
		                     ->killerTargetDistance.value;
		f32 minDistance = ((TKoopaJrSubmarineParams*)getSaveParam())
		                      ->killerTargetDistanceMin.value;
		if (distance >= minDistance)
			minDistance = distance;

		toMario.normalize();
		toMario.scale(minDistance);

		JGeometry::TVec3<f32> target;
		target.add(killer->mPosition, toMario);
		target.y = marioPos.y;

		velocity = calcVelocityToJumpToY(
		    target, velocity.y,
		    killer->getSaveParam2()->mSLFlyingGravityY.value);
	}

	killer->makeInitialVelocity(velocity);
}

const char** TKoopaJrSubmarine::getBasNameTable() const
{
	return koopajrsubmarine_bastable;
}

void TKoopaJrSubmarine::makeRelativeAngle()
{
	f32 flameDirection
	    = TDirectionCalc::d2r(mKoopaJr->mKoopa->getFlameDirDegree());
	f32 flameNearer = mRoundAngle.calcNearerDirection(flameDirection);
	f32 flameDiff   = fabsf(mRoundAngle.mDirection - flameNearer);

	JGeometry::TVec3<f32> toMario;
	const JGeometry::TVec3<f32>& tubPos = mKoopaJr->mBathtub->mPosition;
	toMario.x = gpMarioPos->x - tubPos.x;
	toMario.y = gpMarioPos->y - tubPos.y;
	toMario.z = gpMarioPos->z - tubPos.z;
	toMario.y = 0.0f;

	TDirectionCalc marioDir;
	marioDir.makeDirection(toMario);
	f32 marioDirection = marioDir.mDirection;
	f32 marioNearer    = mRoundAngle.calcNearerDirection(marioDirection);
	f32 target         = mRoundAngle.mDirection;
	f32 marioDiff      = fabsf(target - marioNearer);

	if (mKoopaJr->mKoopa->isFlaming()
	    && flameDiff <= ((TKoopaJrSubmarineParams*)getSaveParam())
	                        ->aboidKoopaFlameAngle.get()) {
		target = flameDirection + 3.1415927f;
	} else if (marioDiff > ((TKoopaJrSubmarineParams*)getSaveParam())
	                           ->traceMarioAngle.get()) {
		target = marioDirection;
	}

	f32 step = 0.017453294f
	           * ((TKoopaJrSubmarineParams*)getSaveParam())
	                 ->mSLRoundAngleVelocity.get();

	mRoundAngle.mDirection
	    = 0.0f
	      + JGeometry::TUtil<f32>::mod(
	          6.2831855f + (mRoundAngle.mDirection - 0.0f), 6.2831855f);

	f32 nearer  = mRoundAngle.calcNearerDirection(target);
	f32 current = mRoundAngle.mDirection;
	if (nearer > current) {
		f32 diff = nearer - current;
		if (diff < step)
			step = diff;
		mRoundAngle.mDirection = current + step;
	} else {
		f32 diff = current - nearer;
		if (diff < step)
			step = diff;
		mRoundAngle.mDirection = current - step;
	}
}

void TKoopaJrSubmarine::makeRoundVelocity()
{
	JGeometry::TVec3<f32> offsetTmp;
	setOutOfLine(offsetTmp, sinf(mRoundAngle.mDirection), 0.0f,
	             cosf(mRoundAngle.mDirection));
	JGeometry::TVec3<f32> offset = offsetTmp;
	offset.scale(mRoundDistance);

	JGeometry::TVec3<f32> toTarget(
	    mKoopaJr->mBathtub->mPosition.x + offset.x - mPosition.x, 0.0f,
	    mKoopaJr->mBathtub->mPosition.z + offset.z - mPosition.z);

	if (toTarget.length() < 100.0f) {
		unk170 = 1;
		return;
	}

	unk170 = 0;
	toTarget.normalize();
	toTarget.scale(
	    ((TKoopaJrSubmarineParams*)getSaveParam())->mSLAcceleration.value);
	mVelocity.add(toTarget);

	f32 speed    = mVelocity.length();
	f32 speedMax = ((TKoopaJrSubmarineParams*)getSaveParam())
	                   ->mSLSpeedMax.value;
	if (speed > speedMax) {
		mVelocity.normalize();
		mVelocity.scale(speedMax);
	}
}

void TKoopaJrSubmarine::checkNerve()
{
	if (mKoopaJr->mSpine->getCurrentNerve()
	    == &TNerveKoopaJrWait::theNerve()) {
		makeRelativeAngle();
		mRoundDistance = ((TKoopaJrSubmarineParams*)getSaveParam())
		                     ->mSLRoundDistance.value;
		makeRoundVelocity();
	}

	mVelocity.x *= 0.95f;
	mVelocity.y *= 0.95f;
	mVelocity.z *= 0.95f;

	if (!unk170) {
		JGeometry::TVec3<f32> direction = mVelocity;
		f32 lsq = dotOutOfLine(direction, direction);
		if (lsq <= JGeometry::TUtil<f32>::epsilon())
			direction.zero();
		else
			scaleOutOfLine(direction,
			               JGeometry::TUtil<f32>::one()
			                   * JGeometry::TUtil<f32>::inv_sqrt(lsq),
			               direction);
		TDirectionCalc dirCalc;
		dirCalc.makeDirection(direction);
		f32 target = dirCalc.mDirection;
		mHeading.mDirection = mHeading.calcTurnDirection(
		    target,
		    TDirectionCalc::d2r(((TKoopaJrSubmarineParams*)getSaveParam())
		                            ->mSLRotationSpeed.value));
	}

	if (mSpine->getCurrentNerve()
	    != &TNerveKoopaJrSubmarineWait::theNerve()) {
		if (mSpine->getCurrentNerve()
		    == &TNerveKoopaJrSubmarineCannonOpenClose::theNerve()) {
			if (mKillerIndex == 0) {
				J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
				if (ctrl->checkPass(30.0f)) {
					ctrl->setRate(0.0f);
					mSpine->pushNerve(
					    &TNerveKoopaJrSubmarineLaunchKiller::theNerve());
				}
			}
		} else if (mSpine->getCurrentNerve()
		           == &TNerveKoopaJrSubmarineLaunchKiller::theNerve()) {
		}
	}
}

BOOL TNerveKoopaJrSubmarineWait::execute(TSpineBase<TLiveActor>* spine) const
{
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(0);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[0]);
		self->mMActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(0.0f);
	}

	return FALSE;
}

BOOL TNerveKoopaJrSubmarineCannonOpenClose::execute(
    TSpineBase<TLiveActor>* spine) const
{
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(0);
		const char** table = self->getBasNameTable();
		self->setAnmSound(!table ? nullptr : table[0]);
		J3DFrameCtrl* ctrl = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(self->mAnmRate);
	}

	return self->mMActor->isCurAnmAlreadyEnd(ANM_TYPE_BCK) ? TRUE : FALSE;
}

BOOL TNerveKoopaJrSubmarineLaunchKiller::execute(
    TSpineBase<TLiveActor>* spine) const
{
	if (0) { (void)(GXColor) { 1, 2, 3, 4 }; }
	TKoopaJrSubmarine* self = (TKoopaJrSubmarine*)spine->getBody();

	if (self->mKillerIndex == self->mKillerNum && self->unk150 <= 0) {
		J3DFrameCtrl* ctrl = self->mMActor->getFrameCtrl(ANM_TYPE_BCK);
		ctrl->setRate(self->mAnmRate);
		return TRUE;
	}
	return FALSE;
}

TKoopaJrSubmarineManager::TKoopaJrSubmarineManager(const char* name)
    : TEnemyManager(name)
{
}

void TKoopaJrSubmarineManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "LastKoopaJrSubmarine.bmd", 0x54220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// Retail load()/loadAfter() read unk38 around the base call and drop the
// result; volatile reads keep those loads (the dead compares fold away).
void TKoopaJrSubmarineManager::load(JSUMemoryInputStream& stream)
{
	if (0) {
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
	}
	(void)(*(volatile u32*)&unk38 == 0);
	TEnemyManager::load(stream);
	unk38 = new TKoopaJrSubmarineParams("/enemy/koopajrsubmarine.prm");
	(void)(*(volatile u32*)&unk38 == 0);
}

void TKoopaJrSubmarineManager::loadAfter()
{
	if (0) {
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
		(void)(GXColor) { 1, 2, 3, 4 }; (void)(GXColor) { 1, 2, 3, 4 };
	}
	TEnemyManager::loadAfter();
	(void)(*(volatile u32*)&unk38 == 0);
}

TSpineEnemy* TKoopaJrSubmarineManager::createEnemyInstance()
{
	return new TKoopaJrSubmarine("クッパジュニアサブマリン");
}
