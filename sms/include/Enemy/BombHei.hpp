#ifndef ENEMY_BOMB_HEI_HPP
#define ENEMY_BOMB_HEI_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <Strategic/Nerve.hpp>

class TBombHeiSaveLoadParams : public TWalkerEnemyParams {
public:
	TBombHeiSaveLoadParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLBombTime, 1000)
	    , PARAM_INIT(mSLBombRange, 300.0f)
	    , PARAM_INIT(mSLThrownVY, 50.0f)
	    , PARAM_INIT(mSLThrownRateXZ, 0.5f)
	    , PARAM_INIT(mSLThrownGravityY, 1.5f)
	    , PARAM_INIT(mSLShootVelocity, 12.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<s32> mSLBombTime;
	/* 0x340 */ TParamRT<f32> mSLBombRange;
	/* 0x354 */ TParamRT<f32> mSLThrownVY;
	/* 0x368 */ TParamRT<f32> mSLThrownRateXZ;
	/* 0x37C */ TParamRT<f32> mSLThrownGravityY;
	/* 0x390 */ TParamRT<f32> mSLShootVelocity;
};

class TBombHeiManager : public TSmallEnemyManager {
public:
	TBombHeiManager(const char* name = "ボム兵マネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

	// fabricated: only a limited number of coins per manager may be dropped
	bool tryCountCoin()
	{
		if (unk60 < 20) {
			unk60 += 1;
			return true;
		}
		return false;
	}

	/* 0x60 */ int unk60;
};

class TBombHei : public TWalkerEnemy {
public:
	TBombHei(const char* name = "ボム兵");

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void genEventCoin();
	virtual void behaveToWater(THitActor*);
	virtual void changeOut();
	virtual void behaveToTaken(THitActor*);
	virtual void behaveToRelease();
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setFreezeAnm();
	virtual void attackToMario();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32);
	virtual bool isCollidMove(THitActor*);
	virtual void setAfterDeadEffect() { }
	virtual bool doKeepDistance() { return unk19C; }
	virtual void walkBehavior(int, f32);

	// called by TCannon
	bool isDamageToCannon();

	// fabricated
	TBombHeiSaveLoadParams* getBombHeiParams() const
	{
		return (TBombHeiSaveLoadParams*)getSaveParam();
	}

	static bool mSerialBomb;

public:
	/* 0x194 */ TBombHeiSaveLoadParams* unk194;
	/* 0x198 */ int unk198;
	/* 0x19C */ u8 unk19C;
	/* 0x1A0 */ f32 unk1A0;
	/* 0x1A4 */ u8 unk1A4;
};

#endif
