#ifndef ENEMY_KUKKU_HPP
#define ENEMY_KUKKU_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Nerve.hpp>

class TMapObjBase;

class TKukkuParams : public TSmallEnemyParams {
public:
	TKukkuParams(const char* path)
	    : TSmallEnemyParams(path)
	    , PARAM_INIT(mMarchSpeed, 3.0f)
	    , PARAM_INIT(mTurnSpeed, 0.2f)
	    , PARAM_INIT(mWaterPowerY, 12.0f)
	    , PARAM_INIT(mShootSpeed, 1.0f)
	    , PARAM_INIT(mShootInterval, 60)
	    , PARAM_INIT(mSearchRange, 800.0f)
	    , PARAM_INIT(mHabatakiTimer, 45)
	    , PARAM_INIT(mAirFric, 0.97f)
	    , PARAM_INIT(mUpperVelocityY, 0.0f)
	    , PARAM_INIT(mDropSpeed, 5.0f)
	    , PARAM_INIT(mDropAngleX, -0.25f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x2D4 */ TParamRT<f32> mMarchSpeed;
	/* 0x2E8 */ TParamRT<f32> mTurnSpeed;
	/* 0x2FC */ TParamRT<f32> mWaterPowerY;
	/* 0x310 */ TParamRT<f32> mShootSpeed;
	/* 0x324 */ TParamRT<s32> mShootInterval;
	/* 0x338 */ TParamRT<f32> mSearchRange;
	/* 0x34C */ TParamRT<s32> mHabatakiTimer;
	/* 0x360 */ TParamRT<f32> mAirFric;
	/* 0x374 */ TParamRT<f32> mUpperVelocityY;
	/* 0x388 */ TParamRT<f32> mDropSpeed;
	/* 0x39C */ TParamRT<f32> mDropAngleX;
};

class TKukkuManager : public TSmallEnemyManager {
public:
	TKukkuManager(const char* name = "クックマネージャー");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void createModelData();

public:
	/* 0x60 */ u32 unk60; // never touched in this unit, but the object is 0x64
	                      // bytes
};

// Droppings (fecal bombs) shot by the Kukku
class TKukkuBall : public THitActor {
public:
	TKukkuBall(MActor* actor)
	    : THitActor("クック玉")
	    , mMActor(actor)
	    , mFlags(1)
	    , unk7C(0)
	{
	}

	virtual void perform(u32, JDrama::TGraphics*);
	virtual void init();

	// fabricated: dead and not owned by anything
	bool isIdle() const { return (mFlags & 1) && unk7C == 0; }

public:
	/* 0x68 */ MActor* mMActor;
	/* 0x6C */ u32 mFlags;
	/* 0x70 */ JGeometry::TVec3<f32> mVelocity;
	/* 0x7C */ u32 unk7C;
};

class TKukku : public TSmallEnemy {
public:
	TKukku(const char* name = "クック");

	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void setAfterDeadEffect();

	void dropCoins();
	JGeometry::TVec3<f32> calcMomentum(f32);
	void updateRotation();

	// fabricated
	TKukkuParams* getSaveParam2() const
	{
		return (TKukkuParams*)getSaveParam();
	}

	// fabricated
	bool isFalling() const;

public:
	/* 0x194 */ TKukkuBall* mBalls[3];
	/* 0x1A0 */ TMapObjBase* mMushroom;
	/* 0x1A4 */ int unk1A4;
	/* 0x1A8 */ int mCenterJointIndex;
	/* 0x1AC */ int mShootTimer;
	/* 0x1B0 */ int mDropCount;
};

class TLiveActor;

DECLARE_NERVE(TNerveKukkuGraphWander, TLiveActor);
DECLARE_NERVE(TNerveKukkuFall, TLiveActor);
DECLARE_NERVE(TNerveKukkuPostFall, TLiveActor);
DECLARE_NERVE(TNerveKukkuRecoverGraph, TLiveActor);

#endif
