#ifndef ENEMY_KILLER_HPP
#define ENEMY_KILLER_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <Strategic/Nerve.hpp>
#include <System/ParamInst.hpp>
#include <JSystem/JGeometry.hpp>
#include <dolphin/gx/GXStruct.h>

class TLiveActor;

// Params shared by all flying enemies (Bullet Bills and friends)
class TFlyEnemyParams : public TWalkerEnemyParams {
public:
	TFlyEnemyParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLNormalFlyGravityY, 0.2f)
	    , PARAM_INIT(mSLNormalFlySpeed, 10.0f)
	    , PARAM_INIT(mSLChaseFlyGravityY, 0.1f)
	    , PARAM_INIT(mSLChaseDist, 2000.0f)
	    , PARAM_INIT(mSLForceGravityY, 0.1f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<f32> mSLNormalFlyGravityY;
	/* 0x340 */ TParamRT<f32> mSLNormalFlySpeed;
	/* 0x354 */ TParamRT<f32> mSLChaseFlyGravityY;
	/* 0x368 */ TParamRT<f32> mSLChaseDist;
	/* 0x37C */ TParamRT<f32> mSLForceGravityY;
};

class TKillerSaveLoadParams : public TFlyEnemyParams {
public:
	TKillerSaveLoadParams(const char* path)
	    : TFlyEnemyParams(path)
	    , PARAM_INIT(mSLWaterAddGravityY, 1.0f)
	    , PARAM_INIT(mSLChaseTimer, 1000)
	    , PARAM_INIT(mSLBombRange, 300.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x390 */ TParamRT<f32> mSLWaterAddGravityY;
	/* 0x3A4 */ TParamRT<s32> mSLChaseTimer;
	/* 0x3B8 */ TParamRT<f32> mSLBombRange;
};

class TKillerManager : public TSmallEnemyManager {
public:
	TKillerManager(const char* name = "キラーマネージャー");

	virtual TSpineEnemy* createEnemyInstance();
	virtual void createModelData();
	virtual void load(JSUMemoryInputStream&);
};

// Base of all flying enemies. Only TKiller derives from it.
class TFlyEnemy : public TWalkerEnemy {
public:
	TFlyEnemy(const char* name)
	    : TWalkerEnemy(name)
	    , mChaseState(0)
	    , mFlyParams(nullptr)
	    , mFlyTime(0)
	    , mIsHoming(true)
	    , mIsGolden(false)
	{
	}

	virtual void bind();
	virtual f32 getGravityY() const;
	virtual void reset();
	virtual void init(TLiveManager*);
	virtual void setAfterDeadEffect() { }
	virtual void flyBehavior() { }
	virtual void setChaseFlyAnm() { }
	virtual void setNormalFlyAnm() { }

	void calcChaseParam();
	void fly();

	static f32 mTestSp;
	static s32 mInvalidTime;
	static f32 mTestMarioSpMax;

public:
	/* 0x194 */ f32 mFlyGravityY;
	/* 0x198 */ s32 mChaseState;
	/* 0x19C */ TFlyEnemyParams* mFlyParams;
	/* 0x1A0 */ s32 mFlyTime;
	/* 0x1A4 */ u8 mIsTimedChase;
	/* 0x1A5 */ u8 mIsHoming;
	/* 0x1A6 */ u8 mIsGolden;
	/* 0x1A8 */ JGeometry::TVec3<f32> mChaseVelocity;
};

class TKiller : public TFlyEnemy {
public:
	TKiller(const char* name = "キラー");

	virtual bool isFindMario(f32);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void reset();
	virtual void changeOut();
	virtual void flyBehavior();
	virtual bool isCollidMove(THitActor*);
	virtual void setChaseFlyAnm();
	virtual void setNormalFlyAnm();
	virtual const char** getBasNameTable() const;
	virtual void attackToMario();
	virtual void setDeadAnm();
	virtual bool isHitValid(u32);
	virtual void genEventCoin();
	virtual void behaveToWater(THitActor*);
	virtual void setMActorAndKeeper();
	virtual void init(TLiveManager*);
	virtual void forceKill() { }

	void setColorType();
	bool isRollFly();

	// fabricated
	TKillerSaveLoadParams* getSaveParam2() const
	{
		return (TKillerSaveLoadParams*)getSaveParam();
	}

	static bool mSerialBomb;
	static bool mTrampleDie;
	static bool mRollSw;

public:
	/* 0x1B4 */ TKillerSaveLoadParams* mKillerParams;
	/* 0x1B8 */ f32 mRollAngle;
	/* 0x1BC */ TPosition3f mParticlePos;
	/* 0x1EC */ GXColorS10 mNoseColor;
	/* 0x1F4 */ GXColorS10 mEyesColor;
	/* 0x1FC */ GXColorS10 mBodyColor;
	/* 0x204 */ GXColorS10 mBaseColor;
	/* 0x20C */ f32 mBombScale;
};

class TNerveFlyEnemyNormalFly : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNerveFlyEnemyNormalFly& theNerve()
	{
		static TNerveFlyEnemyNormalFly instance;
		return instance;
	}
};

class TNerveFlyEnemyChaseFly : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNerveFlyEnemyChaseFly& theNerve()
	{
		static TNerveFlyEnemyChaseFly instance;
		return instance;
	}
};

class TNerveKillerExplosion : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNerveKillerExplosion& theNerve()
	{
		static TNerveKillerExplosion instance;
		return instance;
	}
};

#endif
