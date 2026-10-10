#ifndef ANIMAL_BIRD_HPP
#define ANIMAL_BIRD_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>

class TMapObjBase;
class TWireBinder;

class TAnimalBirdParams : public TSpineEnemyParams {
public:
	TAnimalBirdParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mMarchSpeed;
	/* 0xBC */ TParamRT<f32> mTurnSpeed;
	/* 0xD0 */ TParamRT<s32> mReturnTimer;
	/* 0xE4 */ TParamRT<f32> mSearchLength;
	/* 0xF8 */ TParamRT<f32> mSearchHeight;
	/* 0x10C */ TParamRT<f32> mSearchAware;
	/* 0x120 */ TParamRT<f32> mSearchAngle;
	/* 0x134 */ TParamRT<s32> mActionTimer;
	/* 0x148 */ TParamRT<s32> mWaterproofTimerMax;
	/* 0x15C */ TParamRT<s32> mFloatingTimerMax;
	/* 0x170 */ TParamRT<f32> mLandingGravityY;
	/* 0x184 */ TParamRT<f32> mLandingTorqueY;
	/* 0x198 */ TParamRT<f32> mWalkingTorqueY;
	/* 0x1AC */ TParamRT<f32> mWalkingSpeed;
	/* 0x1C0 */ TParamRT<s32> mWalkTimer;
	/* 0x1D4 */ TParamRT<f32> mLandingFric;
	/* 0x1E8 */ TParamRT<s32> mActionTimerAdd;
	/* 0x1FC */ TParamRT<f32> mWaterPowerY;
};

class TAnimalBird : public TSpineEnemy {
public:
	TAnimalBird(const char* name = "(幸せの青い)鳥");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;

	void initParams();
	bool doLanding(bool);
	void doFlyToCurPathNode();
	bool isFindMario() const;

	// fabricated
	TAnimalBirdParams* getBirdParams() const
	{
		return (TAnimalBirdParams*)getSaveParam();
	}

public:
	/* 0x150 */ TMapObjBase* mItem;
	/* 0x154 */ TWireBinder* mWireBinder;
	/* 0x158 */ JGeometry::TVec3<f32> mHomePos;
	/* 0x164 */ JGeometry::TVec3<f32> mHomeRot;
	/* 0x170 */ f32 mWalkDir;
	/* 0x174 */ f32 mSpeedScale;
	/* 0x178 */ s32 mWaterproofTimer;
	/* 0x17C */ s32 mFloatingTimer;
	/* 0x180 */ s32 mColorIdx;
};

class TAnimalBirdManager : public TEnemyManager {
public:
	TAnimalBirdManager(const char* name = "(幸せの青い)鳥マネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
};

DECLARE_NERVE(TNerveAnimalBirdLanding, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdPreLanding, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdComeback, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdChangeToCoin, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdGraphWander, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdTakeoff, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdWalkOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdActionOnGround, TLiveActor);
DECLARE_NERVE(TNerveAnimalBirdWaitOnGround, TLiveActor);

#endif
