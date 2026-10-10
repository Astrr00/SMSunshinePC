#ifndef ROCKET_H
#define ROCKET_H

#include <Enemy/EnemyManager.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Strategic/Strategy.hpp>
#include <Map/MapData.hpp>

class TWaterEmitInfo;

class TRocketSaveLoadParams : public TSmallEnemyParams {
public:
	TRocketSaveLoadParams(const char* path)
	    : TSmallEnemyParams(path)
	    , PARAM_INIT(mSLReleaseSpeed, 10.0f)
	    , PARAM_INIT(mSLFlyGravity, 0.0f)
	    , PARAM_INIT(mSLFlyLimitTime, 300)
	{
		TParams::load(mPrmPath);
	}

	/* 0x2D4 */ TParamRT<f32> mSLReleaseSpeed;
	/* 0x2E8 */ TParamRT<f32> mSLFlyGravity;
	/* 0x2FC */ TParamRT<s32> mSLFlyLimitTime;
};

class TRocketManager : public TSmallEnemyManager {
public:
	TRocketManager(const char* name = "ロケットマネージャー");
	virtual ~TRocketManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
	virtual void initSetEnemies();

	/* 0x60 */ bool mIsFree; // no rocket is currently attached to the nozzle
	/* 0x64 */ u32 unk64;
	/* 0x68 */ TWaterEmitInfo* mExplodeEmitInfo;
};

class TRocket : public TSmallEnemy {
public:
	TRocket(const char* name = "ロケット");
	virtual ~TRocket() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual bool isCollidMove(THitActor*);

	bool isAttack();

	static f32 mTestAng_x;
	static f32 mTestAng_y;
	static f32 mTestAng_z;
	static f32 mNozzleOffsetZ;
	static f32 mColOffsetY;

	/* 0x194 */ JGeometry::TVec3<f32> mInitialPosition;
	/* 0x1A0 */ bool mIsPossessed; // attached to Mario's nozzle
	/* 0x1A1 */ bool mHasInitialPosition;
	/* 0x1A4 */ TRocketSaveLoadParams* mParams;
};

DECLARE_NERVE(TNerveRocketWait, TLiveActor);
DECLARE_NERVE(TNerveRocketFly, TLiveActor);
DECLARE_NERVE(TNerveRocketPossessedNozzle, TLiveActor);

#endif
