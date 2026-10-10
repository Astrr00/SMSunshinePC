#ifndef ENEMY_TIN_KOOPA_HPP
#define ENEMY_TIN_KOOPA_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <M3DUtil/M3UJoint.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/Spine.hpp>

class MActor;
class TGraphWeb;
class TMapCollisionMove;
class TTinKoopa;
class TTinKoopaPartsBase;
class TTinKoopaFlame;
class TCoasterKillerManager;

class TTinKoopaParams : public TSpineEnemyParams {
public:
	TTinKoopaParams(const char* path);

	/* 0xA8 */ TParamRT<s32> mSLPartsHP;
	/* 0xBC */ TParamRT<s32> mSLFlameHP;
	/* 0xD0 */ TParamRT<s32> mSLFlameRevivalTime;
	/* 0xE4 */ TParamRT<f32> mSLFlameDamageRadius0;
	/* 0xF8 */ TParamRT<f32> mSLFlameDamageHeight0;
	/* 0x10C */ TParamRT<f32> mSLFlameDamageRadius1;
	/* 0x120 */ TParamRT<f32> mSLFlameDamageHeight1;
	/* 0x134 */ TParamRT<f32> mSLDamageRadius;
	/* 0x148 */ TParamRT<f32> mSLDamageHeight0;
	/* 0x15C */ TParamRT<f32> mSLDamageHeight1;
	/* 0x170 */ TParamRT<s32> mSLKillerInterval;
	/* 0x184 */ TParamRT<s32> mSLDefeatWaitTime;
	/* 0x198 */ TParamRT<f32> mSLKillerApproachingDistance;
};

// Never instantiated in this unit; only its vtable and destructor are emitted.
class TTinKoopaMtxCalc : public M3UMtxCalcSIAnmBlendQuat {
public:
	virtual void calc(u16);

public:
	/* 0x64 */ TTinKoopa* mOwner;
};

/**
 * One of the six solid pieces of the mecha Koopa: head, breast, stomach, right
 * arm, left arm, leg. Index 1..4 can be shot off.
 */
class TTinKoopaPartsBase : public TLiveActor {
public:
	TTinKoopaPartsBase(TTinKoopa* koopa, int kind);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void reset();

	void emitPartsDisappearEffects(const char** names, int count, f32 scale);
	void emitPartsDisappearEffects();
	void emitPartsTrackEffects(const char** names, int count);
	void startBreaking();
	void initTinKoopaPartsBase();

public:
	/* 0xF4 */ TMapCollisionMove* mCollision;
	/* 0xF8 */ bool mIsBreaking;
	/* 0xFC */ int mKind;
	/* 0x100 */ TTinKoopa* mOwner;
	/* 0x104 */ MActor* mBreakMActor;
	/* 0x108 */ JGeometry::TVec3<f32> mTrackPos[6];
};

class TTinKoopaFlame : public THitActor {
public:
	TTinKoopaFlame(TTinKoopa* koopa);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void emitFlameEffects();
	void setHitParamsByState();
	void resetFlame();

public:
	/* 0x68 */ TTinKoopa* mOwner;
	/* 0x6C */ f32 mScale;
	/* 0x70 */ s16 mHitPoints;
	/* 0x72 */ u8 mHasEmittedSpray;
};

// One scheduled volley of killers, see TTinKoopa::makeLaunchSchedule.
class TTinKoopaLaunchOrder {
public:
	TTinKoopaLaunchOrder(TTinKoopa* koopa)
	    : mOwner(koopa)
	    , mWaitCycle(0)
	    , mFrame(0)
	    , mCount(0)
	    , mKind(0)
	{
	}

	void set(s8 waitCycle, int frame, s8 count, bool kind)
	{
		mWaitCycle = waitCycle;
		mFrame     = frame;
		mCount     = count;
		mKind      = kind;
	}

	void checkOrder();

public:
	/* 0x0 */ TTinKoopa* mOwner;
	/* 0x4 */ s8 mWaitCycle;
	/* 0x8 */ int mFrame;
	/* 0xC */ s8 mCount;
	/* 0xD */ bool mKind;
};

class TTinKoopaLaunchOrderAry {
public:
	TTinKoopaLaunchOrderAry(TTinKoopa* koopa, u8 num)
	    : mNum(num)
	    , mOwner(koopa)
	{
		mOrders = new TTinKoopaLaunchOrder*[mNum];
		for (int i = 0; i < mNum; ++i)
			mOrders[i] = new TTinKoopaLaunchOrder(mOwner);
	}

	// fabricated
	TTinKoopaLaunchOrder* getOrder(int index) const { return mOrders[index]; }

public:
	/* 0x0 */ u8 mNum;
	/* 0x4 */ TTinKoopa* mOwner;
	/* 0x8 */ TTinKoopaLaunchOrder** mOrders;
};

class TTinKoopa : public TSpineEnemy {
public:
	TTinKoopa(const char* name = "メカクッパ");
	virtual ~TTinKoopa() { }

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual BOOL hasMapCollision() const { return TRUE; }
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void emitTinKoopaEffects();
	void checkTinKoopaFirstFlameMessage();
	void checkTinKoopaKillerApproachingMessage();
	void launchKiller(int direction);
	void hitParts();
	void resetTinKoopa();
	f32 calcCoasterDistance(int from, int to);
	void makeLaunchSchedule();
	void setHitParamsByState();

	// fabricated
	void setLaunchSchedule(int count, bool kind)
	{
		int num = count;
		if (num > 4)
			num = 4;
		mLaunchIndex = 0;
		mLaunchNum   = num;
		for (int i = 0; i < mLaunchNum; ++i)
			mLaunchSchedule[i] = kind;
		mTimers[TIMER_LAUNCH] = 0;
	}

	TTinKoopaParams* getSaveParam2() const
	{
		return (TTinKoopaParams*)getSaveParam();
	}

	enum {
		TIMER_LAUNCH,
		TIMER_FLAME_REVIVAL,
		TIMER_DEFEAT_WAIT,
	};

public:
	/* 0x150 */ int mState;
	/* 0x154 */ int unk154;
	/* 0x158 */ int unk158;
	/* 0x15C */ int mWaitCycle;
	/* 0x160 */ TTinKoopaFlame* mFlame;
	/* 0x164 */ MActor* mKoopaRail;
	/* 0x168 */ u8 mHasShownFirstFlame;
	/* 0x169 */ s8 mLaunchSchedule[4];
	/* 0x170 */ int mLaunchNum;
	/* 0x174 */ int mLaunchIndex;
	/* 0x178 */ int mTimers[3];
	/* 0x184 */ JGeometry::TVec3<f32> mHeadPos;
	/* 0x190 */ JGeometry::TVec3<f32> mBreastPos;
	/* 0x19C */ JGeometry::TVec3<f32> mRightArmPos;
	/* 0x1A8 */ JGeometry::TVec3<f32> mLeftArmPos;
	/* 0x1B4 */ f32 unk1B4[4];
	/* 0x1C4 */ int unk1C4;
	/* 0x1C8 */ int mPartsHP;
	/* 0x1CC */ TTinKoopaPartsBase* mParts[6];
	/* 0x1E4 */ TTinKoopaPartsBase* mBreakingParts;
	/* 0x1E8 */ f32* mCoasterDistances;
	/* 0x1EC */ TGraphWeb* mGraph;
	/* 0x1F0 */ TCoasterKillerManager* mKillerManager;
	/* 0x1F4 */ TTinKoopaLaunchOrderAry* mLaunchOrders;
	/* 0x1F8 */ int unk1F8;
};

class TTinKoopaManager : public TEnemyManager {
public:
	TTinKoopaManager(const char* name = "メカクッパマネージャ");
	virtual ~TTinKoopaManager() { }

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual BOOL hasMapCollision() const { return TRUE; }
	virtual TSpineEnemy* createEnemyInstance();
};

class TNerveTinKoopaWait : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

};

class TNerveTinKoopaDamage : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

};

class TNerveTinKoopaBreak : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

};

// fabricated: the two blocks are identical in TNerveTinKoopaBreak::execute and
// TTinKoopa::reset, so they were most likely inline functions.
inline void TTinKoopaFlame::resetFlame()
{
	mHitPoints       = mOwner->getSaveParam2()->mSLFlameHP.get();
	mScale           = 1.0f;
	mHasEmittedSpray = 0;
}

inline void TTinKoopaFlame::setHitParamsByState()
{
	if (mOwner->mState == 0) {
		setHitParams(0.0f, 0.0f,
		             mOwner->getSaveParam2()->mSLFlameDamageRadius0.get(),
		             mOwner->getSaveParam2()->mSLFlameDamageHeight0.get());
	} else if (mOwner->mState == 1) {
		setHitParams(0.0f, 0.0f,
		             mOwner->getSaveParam2()->mSLFlameDamageRadius1.get(),
		             mOwner->getSaveParam2()->mSLFlameDamageHeight1.get());
	}
}

inline void TTinKoopa::setHitParamsByState()
{
	if (mState == 0) {
		setHitParams(0.0f, 0.0f, getSaveParam2()->mSLDamageRadius.get(),
		             getSaveParam2()->mSLDamageHeight0.get());
	} else if (mState == 1) {
		setHitParams(0.0f, 0.0f, getSaveParam2()->mSLDamageRadius.get(),
		             getSaveParam2()->mSLDamageHeight1.get());
	} else if (mState == 2) {
		(void)mState;
	}

	mFlame->setHitParamsByState();
}

#endif
