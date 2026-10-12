#ifndef ENEMY_LIMIT_KOOPA_JR_HPP
#define ENEMY_LIMIT_KOOPA_JR_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/DirectionCalc.hpp>
#include <JSystem/JGeometry/JGQuat4.hpp>

class TBathtub;
class TKoopa;

// ============= params =============

class TLimitKoopaJrParams : public TSpineEnemyParams {
public:
	TLimitKoopaJrParams(const char*);

	/* 0xA8 */ TParamRT<f32> mSLAcceleration;
	/* 0xBC */ TParamRT<f32> mSLRotationSpeed;
	/* 0xD0 */ TParamRT<f32> mSLSpeedMax;
	/* 0xE4 */ TParamRT<f32> mSLRoundAngleVelocity;
	/* 0xF8 */ TParamRT<f32> mSLRoundRadius;
	/* 0x10C */ TParamRT<f32> mSLRoundHeight;
	/* 0x120 */ TParamRT<f32> mSLDamageRadius;
	/* 0x134 */ TParamRT<f32> mSLDamageHeight;
	/* 0x148 */ TParamRT<f32> mSLKoopaJrScale;
	/* 0x15C */ TParamRT<s32> mSLShotDoodlePeriod;
	/* 0x170 */ TParamRT<s32> mSLDamagePeriod;
};

// ============= manager =============

class TLimitKoopaJrManager : public TEnemyManager {
public:
	TLimitKoopaJrManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= koopa jr (the one circling the bathtub) =============

class TLimitKoopaJr : public TSpineEnemy {
public:
	TLimitKoopaJr(const char* name);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void moveRun();

	// fabricated
	TLimitKoopaJrParams* getSaveParam2() const
	{
		return (TLimitKoopaJrParams*)getSaveParam();
	}

public:
	/* 0x150 */ TBathtub* mBathtub;
	/* 0x154 */ TKoopa* mKoopa;
	// counts down every move tick; only ever reset to 0 in this unit
	/* 0x158 */ s32 unk158;
	/* 0x15C */ s32 mShotDoodleTimer;
	// reset to the identity quaternion; never read in this unit
	/* 0x160 */ JGeometry::TQuat4<f32> unk160;
	/* 0x170 */ TDirectionCalc mRoundDir;
	/* 0x174 */ f32 mRoundRadius;
	/* 0x178 */ TDirectionCalc mFaceDir;
};


#endif
