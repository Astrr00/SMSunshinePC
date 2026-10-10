#ifndef ANIMAL_BEE_HIVE_HPP
#define ANIMAL_BEE_HIVE_HPP

#include <Animal/fishoid.hpp>
#include <Enemy/EnemyManager.hpp>
#include <JSystem/JGeometry.hpp>
#include <Strategic/Nerve.hpp>

class TBeeHive;
class TMapObjBase;

class TBeeHiveParams : public TSpineEnemyParams {
public:
	TBeeHiveParams(const char* path)
	    : TSpineEnemyParams(path)
	    , PARAM_INIT(mGiveupTimer, 600)
	    , PARAM_INIT(mGiveupRange, 1750.0f)
	    , PARAM_INIT(mDecrimentTimer, 60)
	    , PARAM_INIT(mRebound, 0.007f)
	    , PARAM_INIT(mDecay, 0.98f)
	    , PARAM_INIT(mAngleMaxAdd, 0.08f)
	    , PARAM_INIT(mShakePower, 0.003f)
	    , PARAM_INIT(mFallAngularVel, 0.01f)
	    , PARAM_INIT(mSearchRange, 800.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0xA8 */ TParamRT<s32> mGiveupTimer;
	/* 0xBC */ TParamRT<f32> mGiveupRange;
	/* 0xD0 */ TParamRT<s32> mDecrimentTimer;
	/* 0xE4 */ TParamRT<f32> mRebound;
	/* 0xF8 */ TParamRT<f32> mDecay;
	/* 0x10C */ TParamRT<f32> mAngleMaxAdd;
	/* 0x120 */ TParamRT<f32> mShakePower;
	/* 0x134 */ TParamRT<f32> mFallAngularVel;
	/* 0x148 */ TParamRT<f32> mSearchRange;
};

class TBee : public TRealoidActor {
public:
	TBee(MActor* actor, TBeeHive* hive)
	    : TRealoidActor(actor)
	{
		unkA8 = hive;
	}

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init();

public:
	/* 0xA8 */ TBeeHive* unkA8;
};

class TBeeHive : public TRealoid {
public:
	TBeeHive(const char* name = "ハチの巣とハチ");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual void reset();
	virtual TBee* createRealoidActor(MActor*);

	void receiveMessageFromChild(TBee*);
	void controlSound();
	void controlCollision();
	void appearBee(int);
	bool doWait();
	JGeometry::TVec3<f32> getCenterOfGravity() const;

	// fabricated
	TBeeHiveParams* getSaveParam2() const
	{
		return (TBeeHiveParams*)getSaveParam();
	}

public:
	/* 0x158 */ JGeometry::TQuat4<f32> unk158;
	/* 0x168 */ JGeometry::TQuat4<f32> unk168;
	/* 0x178 */ JGeometry::TQuat4<f32> unk178;
	/* 0x188 */ f32 mAngle;
	/* 0x18C */ f32 mAngularVel;
	/* 0x190 */ f32 mAngleLimit;
	/* 0x194 */ JGeometry::TVec3<f32> mInitialPosition;
	/* 0x1A0 */ JGeometry::TVec3<f32> mBeesCenter;
	/* 0x1AC */ TMapObjBase* mItem;
	/* 0x1B0 */ int mCollisionIdx;
	/* 0x1B4 */ int mNumBees;
	/* 0x1B8 */ TMapObjBase** mCoins;
	/* 0x1BC */ int mNumDead;
};

class TBeeHiveManager : public TEnemyManager {
public:
	TBeeHiveManager(const char* name = "ハチの巣マネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

// theNerve() is defined (inline) in BeeHive.cpp: the original inlines it into
// every execute() and never emits it out of line.
class TNerveBeeHiveReset : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveReset& theNerve();
};

class TNerveBeeHiveWait : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveWait& theNerve();
};

class TNerveBeeHiveAttack : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveAttack& theNerve();
};

class TNerveBeeHiveMarioWaterIn : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveMarioWaterIn& theNerve();
};

class TNerveBeeHiveBreak : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveBreak& theNerve();
};

class TNerveBeeHiveFall : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
	static const TNerveBeeHiveFall& theNerve();
};

#endif
