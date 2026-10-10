#ifndef ENEMY_TABEPUKU_HPP
#define ENEMY_TABEPUKU_HPP

#include <Enemy/SmallEnemy.hpp>
#include <JSystem/JGeometry.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/Spine.hpp>

class JSUMemoryInputStream;
class TBGCheckData;
class TLiveManager;
class TTabePuku;
class TTPHitActor;

class TTabePukuParams : public TSmallEnemyParams {
public:
	TTabePukuParams(const char* path)
	    : TSmallEnemyParams(path)
	    , PARAM_INIT(mMarchSpeed, 0.15f)
	    , PARAM_INIT(mAttackSpeed, 0.22f)
	    , PARAM_INIT(mDiveSpeed, 0.4f)
	    , PARAM_INIT(mWaterFric, 0.95f)
	    , PARAM_INIT(mTurnSlerpRate, 0.05f)
	    , PARAM_INIT(mApartHeight, 500.0f)
	    , PARAM_INIT(mCorrectY, -40.0f)
	    , PARAM_INIT(mCorrectZ, 150.0f)
	    , PARAM_INIT(mTerritoryRange, 1000.0f)
	    , PARAM_INIT(mDragLength, 2500.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x2D4 */ TParamRT<f32> mMarchSpeed;
	/* 0x2E8 */ TParamRT<f32> mAttackSpeed;
	/* 0x2FC */ TParamRT<f32> mDiveSpeed;
	/* 0x310 */ TParamRT<f32> mWaterFric;
	/* 0x324 */ TParamRT<f32> mTurnSlerpRate;
	/* 0x338 */ TParamRT<f32> mApartHeight;
	/* 0x34C */ TParamRT<f32> mCorrectY;
	/* 0x360 */ TParamRT<f32> mCorrectZ;
	/* 0x374 */ TParamRT<f32> mTerritoryRange;
	/* 0x388 */ TParamRT<f32> mDragLength;
};

class TTabePukuManager : public TSmallEnemyManager {
public:
	TTabePukuManager(const char* name = "プクプク(レール巡回)マネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

/**
 * @brief Collision body of the Tabepuku: follows the fish's mouth and holds
 * Mario while he is being swallowed.
 */
class TTPHitActor : public THitActor {
public:
	TTPHitActor(const char* name, TTabePuku* owner)
	    : THitActor(name)
	    , mOwner(owner)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init();

	void bind();
	void updateTerrainCollsion();

	// fabricated
	void updateHitParam();
	void checkMarioContact();

public:
	/* 0x68 */ TTabePuku* mOwner;
	/* 0x6C */ JGeometry::TVec3<f32> mLinearVelocity;
	/* 0x78 */ f32 mHeadHeight;
	/* 0x7C */ f32 mWallRadius;
	/* 0x80 */ f32 mGroundHeight;
	/* 0x84 */ const TBGCheckData* mGroundPlane;
	/* 0x88 */ u8 mIsAirborne;
	/* 0x89 */ u8 mIsTouchedWall;
};

DECLARE_NERVE(TNerveTabePukuDrag, TLiveActor);
DECLARE_NERVE(TNerveTabePukuDive, TLiveActor);
DECLARE_NERVE(TNerveTabePukuBite, TLiveActor);
DECLARE_NERVE(TNerveTabePukuAttack, TLiveActor);
DECLARE_NERVE(TNerveTabePukuRecoverGraph, TLiveActor);
DECLARE_NERVE(TNerveTabePukuFound, TLiveActor);
DECLARE_NERVE(TNerveTabePukuGraphWander, TLiveActor);

class TTabePuku : public TSmallEnemy {
public:
	TTabePuku(const char* name = "プクプク(レール巡回)");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void attackToMario();
	virtual void forceKill();
	virtual bool doKeepDistance();
	virtual bool isFindMario(f32);

	void swimTo(const JGeometry::TVec3<f32>& target);

	// fabricated
	bool isAttack() const
	{
		return mSpine->getLatestNerve() == &TNerveTabePukuAttack::theNerve();
	}

	// fabricated, Mario is being bitten, dived with or dragged away
	bool isCarryingMario() const
	{
		const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
		return nerve == &TNerveTabePukuBite::theNerve()
		       || nerve == &TNerveTabePukuDive::theNerve()
		       || nerve == &TNerveTabePukuDrag::theNerve();
	}

	// fabricated, swimming along the graph
	bool isPatrolling() const
	{
		const TNerveBase<TLiveActor>* nerve = mSpine->getLatestNerve();
		return nerve == &TNerveTabePukuGraphWander::theNerve()
		       || nerve == &TNerveTabePukuRecoverGraph::theNerve();
	}

	// fabricated
	TTabePukuParams* getSaveParam2() const
	{
		return (TTabePukuParams*)getSaveParam();
	}

public:
	/* 0x194 */ TTPHitActor* mHitActor;
	/* 0x198 */ JGeometry::TQuat4<f32> mQuat;
	/* 0x1A8 */ TPosition3f mTakingMtx;
	/* 0x1D8 */ int mMouthJointIdx;
	/* 0x1DC */ u8 mIsTouchedWall;
	/* 0x1E0 */ f32 mDiveStartY;
	/* 0x1E4 */ JGeometry::TVec3<f32> mDragDir;
};

inline void TTPHitActor::updateHitParam()
{
	f32 damageHeight = mOwner->getSaveParams()->getSLDamageHeight();
	f32 damageRadius = mOwner->getSaveParams()->getSLDamageRadius();
	f32 attackHeight = mOwner->getSaveParams()->getSLAttackHeight();
	f32 attackRadius = mOwner->getSaveParams()->getSLAttackRadius();
	setHitParams(attackRadius, attackHeight, damageRadius, damageHeight);
}

inline void TTPHitActor::checkMarioContact()
{
	THitActor** it  = mCollisions;
	THitActor** end = mCollisions + mColCount;
	for (; it != end; ++it) {
		switch ((*it)->getActorType()) {
		case 0x80000001:
			mOwner->attackToMario();
			break;
		}
	}
}

#endif // ENEMY_TABEPUKU_HPP
