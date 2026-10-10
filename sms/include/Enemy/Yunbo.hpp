#ifndef ENEMY_YUNBO_HPP
#define ENEMY_YUNBO_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Nerve.hpp>

class J3DMaterialTable;
class MActor;
class TYumbo;

class TYumboParams : public TSmallEnemyParams {
public:
	TYumboParams(const char* path)
	    : TSmallEnemyParams(path)
	    , PARAM_INIT(mRecoverTimer, 600)
	    , PARAM_INIT(mShootSpeed, 55.0f)
	    , PARAM_INIT(mShootAngleX, 0.2f)
	    , PARAM_INIT(mSeedLife, 80)
	    , PARAM_INIT(mSeedAirFric, 0.94f)
	    , PARAM_INIT(mSeedGravityY, 0.9f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x2D4 */ TParamRT<s32> mRecoverTimer;
	/* 0x2E8 */ TParamRT<f32> mShootSpeed;
	/* 0x2FC */ TParamRT<f32> mShootAngleX;
	/* 0x310 */ TParamRT<s32> mSeedLife;
	/* 0x324 */ TParamRT<f32> mSeedAirFric;
	/* 0x338 */ TParamRT<f32> mSeedGravityY;
};

class TYumboManager : public TSmallEnemyManager {
public:
	TYumboManager(const char* name = "ユンボマネージャー");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void createModelData();

public:
	/* 0x60 */ J3DMaterialTable* mFlowerMatTable;
};

/**
 * @brief A seed spat out by the Yumbo's flower. Not a manager-owned enemy: the
 * Yumbo allocates a pool of these in init() and drives them from perform().
 */
class TYumboSeed : public THitActor {
public:
	enum {
		SEED_FLAG_INACTIVE = 0x1,
		SEED_FLAG_HIDDEN   = 0x4,
	};

	TYumboSeed(TYumbo* owner, MActor* actor, const char* name = "ユンボ種")
	    : THitActor(name)
	    , mOwner(owner)
	    , mMActor(actor)
	    , mFlags(SEED_FLAG_INACTIVE)
	{
	}

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init();

public:
	/* 0x68 */ TYumbo* mOwner;
	/* 0x6C */ MActor* mMActor;
	/* 0x70 */ u32 mFlags;
	/* 0x74 */ s32 mLife;
	/* 0x78 */ JGeometry::TVec3<f32> mVelocity;
};

class TYumbo : public TSmallEnemy {
public:
	TYumbo(const char* name = "ユンボ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager* manager);
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor* sender);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual bool doKeepDistance();

	void shotSeeds();

	// fabricated
	TYumboParams* getSaveParam2() const
	{
		return (TYumboParams*)getSaveParam();
	}

public:
	/* 0x194 */ TYumboSeed* mSeeds[16];
	/* 0x1D4 */ int mCenterJointIdx;
	/* 0x1D8 */ bool mParticlesEmitted;
};

class TLiveActor;

DECLARE_NERVE(TNerveYumboDancing, TLiveActor);
DECLARE_NERVE(TNerveYumboHiding, TLiveActor);
DECLARE_NERVE(TNerveYumboAppearing, TLiveActor);
DECLARE_NERVE(TNerveYumboAttack, TLiveActor);
DECLARE_NERVE(TNerveYumboFreeze, TLiveActor);

#endif
