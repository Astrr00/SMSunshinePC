#ifndef ENEMY_POPO_HPP
#define ENEMY_POPO_HPP

#include <Strategic/Nerve.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>

class TLiveActor;
class TPopo;
class TWaterEmitInfo;

class TPopoSaveLoadParams : public TWalkerEnemyParams {
public:
	TPopoSaveLoadParams(const char* path);

	/* 0x32C */ TParamRT<f32> mSLMoveDist;
	/* 0x340 */ TParamRT<f32> mSLMoveGravity;
	/* 0x354 */ TParamRT<f32> mSLMoveJumpSp;
	/* 0x368 */ TParamRT<f32> mSLAttackDist;
	/* 0x37C */ TParamRT<f32> mSLAttackGravity;
	/* 0x390 */ TParamRT<f32> mSLAttackJumpSp;
	/* 0x3A4 */ TParamRT<f32> mSLReleaseSpeed;
	/* 0x3B8 */ TParamRT<f32> mSLFlyGravity;
	/* 0x3CC */ TParamRT<s32> mSLFlyLimitTime;
	/* 0x3E0 */ TParamRT<s32> mSLExplosionEmitTime;
	/* 0x3F4 */ TParamRT<f32> mSLWaterScaleMax;
	/* 0x408 */ TParamRT<f32> mSLThrownGravity;
	/* 0x41C */ TParamRT<f32> mSLPumpRate;
	/* 0x430 */ TParamRT<f32> mSLLevelLimit;
	/* 0x444 */ TParamRT<f32> mSLScaleRate;
};

class TPopoManager : public TSmallEnemyManager {
public:
	TPopoManager(const char* name = "ポポマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSmallEnemy* createEnemyInstance();
	virtual void initSetEnemies();

public:
	/* 0x60 */ bool mIsNozzleFree;
	/* 0x64 */ TWaterEmitInfo* mFlyWaterEmitInfo;
	/* 0x68 */ TWaterEmitInfo* mExplosionWaterEmitInfo;
};

// The hit collision of a Popo. It forwards every message to the Popo, except
// while the Popo is flying.
class TPopoCollision : public THitActor {
public:
	TPopoCollision(const char* name)
	    : THitActor(name)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ TPopo* mOwner;
};

class TPopo : public TWalkerEnemy {
public:
	TPopo(const char* name = "ポポ");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void attackToMario();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32);
	virtual bool isCollidMove(THitActor*);
	virtual bool isFindMario(float);
	virtual void behaveToFindMario();
	virtual void walkBehavior(int, float);

	void thrownByChorobei();
	void possessedIn();
	void explosion();
	void flyBehavior();
	bool checkTrigger();

	static bool mRollSw;
	static bool mTriggerSw;
	static f32 mTestAng_x;
	static f32 mTestAng_y;
	static f32 mTestAng_z;
	static f32 mNozzleOffsetZ;
	static u8 mCenterJntIndex;
	static u8 mMouthJntIndex;
	static u8 mRLegJntIndex;
	static u8 mLLegJntIndex;
	static u8 mRHandJntIndex;
	static u8 mLHandJntIndex;
	static f32 mTestBodyScale;
	static bool mBrkFlag;
	static f32 mColOffsetY;
	static f32 mColMinVal;
	static bool mLevelShootSw;
	static bool mExplosionSw;

public:
	/* 0x194 */ TPopoSaveLoadParams* mParams;
	/* 0x198 */ f32 mWaterScale;
	/* 0x19C */ int mFlyTimer;
	/* 0x1A0 */ f32 mBrkFrameMax;
	/* 0x1A4 */ bool mHasInitialPos;
	/* 0x1A8 */ JGeometry::TVec3<f32> mInitialPos;
	/* 0x1B4 */ bool mIsPossessed;
	/* 0x1B8 */ f32 mRollAngle;
	/* 0x1BC */ bool mIsPumping;
	/* 0x1C0 */ JGeometry::TVec3<f32> mExplosionPos;
	/* 0x1CC */ bool mShootRequested;
	/* 0x1CD */ u8 unk1CD;
	/* 0x1D0 */ Mtx mPossessedMtx;
	/* 0x200 */ Mtx mNozzleMtx;
	/* 0x230 */ JGeometry::TVec3<f32> mNozzleScale;
	/* 0x23C */ TPopoCollision* mCollision;
};

class TLiveActor;

// The Popo nerves are only used inside popo.cpp and their theNerve() is
// defined inline (the retail object has no out-of-line theNerve() symbols).
#define POPO_INLINE_NERVE(Name)                                                \
	class Name : public TNerveBase<TLiveActor> {                               \
	public:                                                                    \
		virtual BOOL execute(TSpineBase<TLiveActor>*) const;                   \
		static const Name& theNerve()                                          \
		{                                                                      \
			static Name instance;                                              \
			return instance;                                                   \
		}                                                                      \
	};

POPO_INLINE_NERVE(TNervePopoPossessedNozzle)
POPO_INLINE_NERVE(TNervePopoAttack)
POPO_INLINE_NERVE(TNervePopoFly)
POPO_INLINE_NERVE(TNervePopoExplosion)
POPO_INLINE_NERVE(TNervePopoWait)
POPO_INLINE_NERVE(TNervePopoThrown)

#undef POPO_INLINE_NERVE

#endif
