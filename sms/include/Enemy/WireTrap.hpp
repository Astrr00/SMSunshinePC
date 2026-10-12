#ifndef ENEMY_WIRE_TRAP_HPP
#define ENEMY_WIRE_TRAP_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/WireBinder.hpp>
#include <Strategic/Nerve.hpp>

class TWireTrapParams : public TSpineEnemyParams {
public:
	TWireTrapParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mInWaterPowerRate;
	/* 0xBC */ TParamRT<s32> mScaleTimerMax;
	/* 0xD0 */ TParamRT<s32> mGoTimerMax;
};

class TWireTrapManager : public TEnemyManager {
public:
	TWireTrapManager(const char* name = "電線トラップマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

class TWireTrap : public TSpineEnemy {
public:
	TWireTrap(const char* name = "電線トラップ");

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();

	static const TNerveBase<TLiveActor>* getNerveFromMode(int);

	TWireBinder* getWireBinder() const;
	const JGeometry::TVec3<f32>& getWireDir() const;
	void checkHitActors();

	// fabricated
	TWireTrapParams* getSaveParam2() const
	{
		return (TWireTrapParams*)getSaveParam();
	}

	// fabricated
	const JGeometry::TVec3<f32>& getBinderDir() const
	{
		return getWireBinder()->getDir();
	}

	// fabricated
	void scaleByWireSpeed(JGeometry::TVec3<f32>& vel) const
	{
		f32 rate = 1.0f;
		if (mInWaterTimer > 0)
			rate = 1.0f + mInWaterPower * mInWaterTimer / 30.0f;
		vel.scale(mDirSign * rate);
		vel.scale(mSpeedScale);
	}

	// fabricated
	JGeometry::TVec3<f32> getVelocityAlongWireDir() const
	{
		JGeometry::TVec3<f32> vel;
		vel = getWireDir();
		scaleByWireSpeed(vel);
		return vel;
	}

	// fabricated
	JGeometry::TVec3<f32> getVelocityOnWire() const
	{
		JGeometry::TVec3<f32> vel = getBinderDir();
		scaleByWireSpeed(vel);
		return vel;
	}

	enum {
		MODE_RETURN_MOVE = 0,
		MODE_ONEWAY_MOVE = 1,
		MODE_SEARCH      = 2,
	};

public:
	/* 0x150 */ char unk150[0x10];
	/* 0x160 */ s32 unk160;
	/* 0x164 */ s32 mInWaterTimer;
	/* 0x168 */ f32 mInWaterPower;
	/* 0x16C */ s32 mCoolTimer;
	/* 0x170 */ f32 mDirSign;
	/* 0x174 */ s32 mWaitTime;
	/* 0x178 */ f32 mScaleRate;
	/* 0x17C */ f32 mSpeedScale;
	/* 0x180 */ s16 mMode;
};

DECLARE_NERVE(TNerveWireTrapGoWait, TLiveActor);
DECLARE_NERVE(TNerveWireTrapWait, TLiveActor);
DECLARE_NERVE(TNerveWireTrapSearch, TLiveActor);
DECLARE_NERVE(TNerveWireTrapOnewayMoveEnd, TLiveActor);
DECLARE_NERVE(TNerveWireTrapOnewayMove, TLiveActor);
DECLARE_NERVE(TNerveWireTrapOnewayMoveStart, TLiveActor);
DECLARE_NERVE(TNerveWireTrapReturnMove, TLiveActor);

#endif
