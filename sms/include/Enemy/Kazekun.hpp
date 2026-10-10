#ifndef ENEMY_KAZEKUN_HPP
#define ENEMY_KAZEKUN_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Nerve.hpp>

class TKazekunParams : public TSmallEnemyParams {
public:
	TKazekunParams(const char*);

	/* 0x2D4 */ TParamRT<f32> mAppearDist;
	/* 0x2E8 */ TParamRT<f32> mAroundDist;
	/* 0x2FC */ TParamRT<f32> mAroundSpeed;
	/* 0x310 */ TParamRT<s32> mAroundTime;
	/* 0x324 */ TParamRT<f32> mAttackSpeed;
	/* 0x338 */ TParamRT<f32> mAirFric;
	/* 0x34C */ TParamRT<s32> mResetTime;
	/* 0x360 */ TParamRT<s32> mResetTimeHitting;
	/* 0x374 */ TParamRT<s32> mPoseTime;
	/* 0x388 */ TParamRT<f32> mDicideTiming;
	/* 0x39C */ TParamRT<f32> mTurnOffsetY;
	/* 0x3B0 */ TParamRT<f32> mLostOffsetYUp;
	/* 0x3C4 */ TParamRT<f32> mLostOffsetYDown;
	/* 0x3D8 */ TParamRT<f32> mPoseSpeed;
	/* 0x3EC */ TParamRT<f32> mPoseOmegaRate;
};

class TKazekunManager : public TSmallEnemyManager {
public:
	TKazekunManager(const char* name);

	virtual void load(JSUMemoryInputStream& stream);
	virtual void createModelData();
};

class TKazekun : public TSmallEnemy {
public:
	TKazekun(const char* name);

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual bool isCollidMove(THitActor*);

	void doAttackPose(bool);
	void flyAroundMario();

	// fabricated
	TKazekunParams* getSaveParam2() const
	{
		return (TKazekunParams*)getSaveParam();
	}

public:
	/* 0x194 */ JGeometry::TVec3<f32> mInitPos;
	/* 0x1A0 */ JGeometry::TQuat4<f32> mQuat;
	/* 0x1B0 */ int mWaitTime;
	/* 0x1B4 */ u8 unk1B4[0x1D4 - 0x1B4]; // never touched by the unit
};

class TLiveActor;

DECLARE_NERVE(TNerveKazekunHitWater, TLiveActor);
DECLARE_NERVE(TNerveKazekunWait, TLiveActor);
DECLARE_NERVE(TNerveKazekunDisappear, TLiveActor);
DECLARE_NERVE(TNerveKazekunAttack, TLiveActor);
DECLARE_NERVE(TNerveKazekunPreAttack, TLiveActor);
DECLARE_NERVE(TNerveKazekunTurn, TLiveActor);
DECLARE_NERVE(TNerveKazekunAppear, TLiveActor);
DECLARE_NERVE(TNerveKazekunSearch, TLiveActor);

#endif
