#ifndef ENEMY_KOOPA_JR_HPP
#define ENEMY_KOOPA_JR_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/DirectionCalc.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Spine.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JGeometry/JGQuat4.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <Strategic/Strategy.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>

class TBathtub;
class TBathtubBinder;
class TBathtubKiller;
class TBathtubKillerManager;
class TKoopa;
class TKoopaJr;
class TKoopaJrSubmarine;
class TKoopaJrSubmarineManager;

// ============= params =============

class TKoopaJrParams : public TSpineEnemyParams {
public:
	TKoopaJrParams(const char* path);

	/* 0x0A8 */ TParamRT<f32> mSLLaunchKillerLimit;
	/* 0x0BC */ TParamRT<f32> mSLDamageRadius;
	/* 0x0D0 */ TParamRT<f32> mSLDamageHeight;
	/* 0x0E4 */ TParamRT<f32> mSLKoopaJrScale;
	/* 0x0F8 */ TParamRT<f32> mSLFastLaunchDistance;
	/* 0x10C */ TParamRT<s32> mSLDamagePeriod;
	/* 0x120 */ TParamRT<s32> mSLLaunchKillerPeriod;
	/* 0x134 */ TParamRT<s32> mSLLaunchKillerPeriodFast;
};

class TKoopaJrSubmarineParams : public TSpineEnemyParams {
public:
	TKoopaJrSubmarineParams(const char* path);

	/* 0x0A8 */ TParamRT<f32> killerTargetDistanceMin;
	/* 0x0BC */ TParamRT<f32> killerTargetDistance;
	/* 0x0D0 */ TParamRT<f32> bottomHeight;
	/* 0x0E4 */ TParamRT<f32> centerZ;
	/* 0x0F8 */ TParamRT<f32> aboidKoopaFlameAngle;
	/* 0x10C */ TParamRT<f32> traceMarioAngle;
	/* 0x120 */ TParamRT<f32> mSLWavePhaseVelocity;
	/* 0x134 */ TParamRT<f32> mSLWaveAmplitudeMin;
	/* 0x148 */ TParamRT<f32> mSLWaveAmplitudeMaxLaunch;
	/* 0x15C */ TParamRT<f32> mSLWaveAmplitudeMax;
	/* 0x170 */ TParamRT<f32> mSLSwingPhaseVelocity;
	/* 0x184 */ TParamRT<f32> mSLSwingAmplitudeMin;
	/* 0x198 */ TParamRT<f32> mSLSwingAmplitudeMax;
	/* 0x1AC */ TParamRT<f32> mSLRoundAngleVelocity;
	/* 0x1C0 */ TParamRT<f32> mSLRoundDistance;
	/* 0x1D4 */ TParamRT<f32> mSLAcceleration;
	/* 0x1E8 */ TParamRT<f32> mSLRotationSpeed;
	/* 0x1FC */ TParamRT<f32> mSLSpeedMax;
	/* 0x210 */ TParamRT<f32> mSLKoopaJrSubmarineScale;
	/* 0x224 */ TParamRT<f32> mSLDamageRadius;
	/* 0x238 */ TParamRT<f32> mSLDamageHeight;
	/* 0x24C */ TParamRT<f32> shineKillerProbability0;
	/* 0x260 */ TParamRT<f32> shineKillerProbability1;
	/* 0x274 */ TParamRT<s32> mSLKillerIntervalFast;
	/* 0x288 */ TParamRT<s32> mSLKillerInterval;
};

// ============= hit actors =============

// Hit actor that forwards every hit message to its owner.
class TCallbackHitActor : public THitActor {
public:
	// inlined into TKoopaJrSubmarine::init
	TCallbackHitActor(const char* name, TSpineEnemy* owner, f32 radius,
	                  f32 height)
	    : THitActor(name)
	    , mOwner(owner)
	{
		initHitActor(0x800002D, 0, 0, 0.0f, 0.0f, radius, height);
		offHitFlag(HIT_FLAG_NO_COLLISION);
		static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
		    ->getChildren()
		    .push_back(this);
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ THitActor* mOwner;
};

// ============= koopa jr =============

class TKoopaJr : public TSpineEnemy {
public:
	TKoopaJr(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void checkNerveKillerHit();
	void checkNerveKillerLaunchFast();
	void checkNerveKillerLaunchNormal();

public:
	/* 0x150 */ s32 unk150; // damage timer
	/* 0x154 */ s32 unk154; // normal killer launch timer
	/* 0x158 */ s32 unk158; // fast killer launch timer
	/* 0x15C */ TBathtub* mBathtub;
	/* 0x160 */ TKoopa* mKoopa;
	/* 0x164 */ TKoopaJrSubmarine* mSubmarine;
	/* 0x168 */ TKoopaJrSubmarineManager* mSubmarineManager;
	/* 0x16C */ TBathtubKillerManager* mKillerManager;
};

class TKoopaJrManager : public TEnemyManager {
public:
	TKoopaJrManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= koopa jr submarine =============

class TKoopaJrSubmarine : public TSpineEnemy {
public:
	TKoopaJrSubmarine(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void checkNerve();
	void makeRoundVelocity();
	void makeRelativeAngle();
	void makeKillerVelocity(TBathtubKiller*, JGeometry::TVec3<f32>);
	void launchKiller();
	bool appearShineKiller(int);
	void moveSwing();
	void makeCollisionPositions();
	void resetKoopaJrSubmarine();

public:
	/* 0x150 */ s32 unk150; // killer launch interval timer
	/* 0x154 */ JGeometry::TQuat4<f32> mQuat;
	/* 0x164 */ TDirectionCalc mRoundAngle;
	/* 0x168 */ f32 mRoundDistance;
	/* 0x16C */ TDirectionCalc mHeading;
	/* 0x170 */ u8 unk170;
	/* 0x174 */ TBathtubBinder* mBathtubBinder;
	/* 0x178 */ s8 mKillerKinds[8]; // 0 = normal, 1 = shine, 2 = fast
	/* 0x180 */ s32 mKillerIndex;
	/* 0x184 */ s32 mKillerNum;
	/* 0x188 */ f32 mAnmRate;
	/* 0x18C */ u8 mIsHit;
	/* 0x190 */ f32 mSwingAmplitude;
	/* 0x194 */ f32 mSwingPhase;
	/* 0x198 */ f32 mWaveAmplitude;
	/* 0x19C */ f32 mWavePhase;
	/* 0x1A0 */ TKoopaJr* mKoopaJr;
	/* 0x1A4 */ TCallbackHitActor* mRearBodyHit;
	/* 0x1A8 */ TCallbackHitActor* mFrontBodyHit;
};

class TKoopaJrSubmarineManager : public TEnemyManager {
public:
	TKoopaJrSubmarineManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= nerves =============

// All theNerve() but TNerveKoopaJrDamage's are defined in the class bodies.
// Declaration order is relevant for the order of the function statics.

class TNerveKoopaJrWait : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrWait& theNerve()
	{
		static TNerveKoopaJrWait instance;
		return instance;
	}
};

class TNerveKoopaJrDamage : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrDamage& theNerve();
};

class TNerveKoopaJrDemo : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrDemo& theNerve()
	{
		static TNerveKoopaJrDemo instance;
		return instance;
	}
};

class TNerveKoopaJrLaunch : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrLaunch& theNerve()
	{
		static TNerveKoopaJrLaunch instance;
		return instance;
	}
};

class TNerveKoopaJrYahoo : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrYahoo& theNerve()
	{
		static TNerveKoopaJrYahoo instance;
		return instance;
	}
};

class TNerveKoopaJrSubmarineWait : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrSubmarineWait& theNerve()
	{
		static TNerveKoopaJrSubmarineWait instance;
		return instance;
	}
};

class TNerveKoopaJrSubmarineCannonOpenClose : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrSubmarineCannonOpenClose& theNerve()
	{
		static TNerveKoopaJrSubmarineCannonOpenClose instance;
		return instance;
	}
};

class TNerveKoopaJrSubmarineLaunchKiller : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveKoopaJrSubmarineLaunchKiller& theNerve()
	{
		static TNerveKoopaJrSubmarineLaunchKiller instance;
		return instance;
	}
};

#endif
