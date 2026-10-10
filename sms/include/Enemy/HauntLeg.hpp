#ifndef ENEMY_HAUNT_LEG_HPP
#define ENEMY_HAUNT_LEG_HPP

#include <Strategic/Nerve.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/TakeActor.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>

class TLiveActor;
class THauntLeg;

class TNerveHauntLegHaunt : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNerveHauntLegHaunt& theNerve()
	{
		static TNerveHauntLegHaunt instance;
		return instance;
	}
};

// A collision-only proxy that hangs off THauntLeg. Stomping or hip-dropping on
// it kills the leg.
class THauntedObject : public THitActor {
public:
	THauntedObject(const char* name)
	    : THitActor(name)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ THauntLeg* mOwner;
};

class THauntLegManager : public TSmallEnemyManager {
public:
	THauntLegManager(const char* name = "ハントレッグマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSmallEnemy* createEnemyInstance();
	virtual void initSetEnemies();
};

class THauntLeg : public TWalkerEnemy {
public:
	THauntLeg(const char* name = "ハントレッグ")
	    : TWalkerEnemy(name)
	    , mHauntedObject(nullptr)
	    , mHasCaught(false)
	    , mIsFirstJump(true)
	    , mTarget(nullptr)
	{
	}

	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void setGenerateAnm();
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setWaitAnm();
	virtual void setRunAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual bool isCollidMove(THitActor*);

	bool isHaunting() const
	{
		return mSpine->getCurrentNerve() == &TNerveHauntLegHaunt::theNerve()
		           ? true
		           : false;
	}

public:
	/* 0x194 */ THauntedObject* mHauntedObject;
	/* 0x198 */ bool mHasCaught;  // set once mTarget has been taken
	/* 0x199 */ bool mIsFirstJump; // jump toward mTarget not finished yet
	/* 0x19C */ TTakeActor* mTarget;
	/* 0x1A0 */ JGeometry::TVec3<f32> mJumpVelocity;
	/* 0x1AC */ f32 mSpinAngle; // joint 1 roll applied by HauntLegCallback
};

#endif
