#ifndef MOVE_BG_MAP_OBJ_BALL_HPP
#define MOVE_BG_MAP_OBJ_BALL_HPP

#include <MoveBG/MapObjGeneral.hpp>

class TWaterEmitInfo;

// A ball that bounces and rolls around when Mario or other objects touch it.
// The parameter fields are filled in by initMapObj depending on the actor type
// (0x400000D0 = ball, 0x40000390..0x40000395 = fruits).
class TMapObjBall : public TMapObjGeneral {
public:
	TMapObjBall(const char* name = "ボール");

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void control();
	virtual void makeObjAppeared();
	virtual void initMapObj();
	virtual void touchActor(THitActor*);
	virtual u32 touchWater(THitActor*);
	virtual void makeObjDefault();
	virtual f32 getDepthAtFloating() { return mFloatingDepth; }
	virtual void hold(TTakeActor*);
	virtual void put();
	virtual void touchGround(JGeometry::TVec3<f32>*);
	virtual void checkWallCollision(JGeometry::TVec3<f32>*);
	virtual void touchWall(JGeometry::TVec3<f32>*, TBGWallCheckRecord*);
	virtual void touchRoof(JGeometry::TVec3<f32>*);
	virtual void rebound(JGeometry::TVec3<f32>*);
	virtual void touchWaterSurface();
	virtual void touchPollution();
	virtual void kicked();
	virtual void calcCurrentMtx();

	void boundByActor(THitActor*);

public:
	// fabricated names, derived from how the fields are used
	/* 0x148 */ f32 mPlayerPushRate;
	/* 0x14C */ f32 mPlayerPushOutRate;
	/* 0x150 */ f32 mPlayerFastPushUpSpeed;
	/* 0x154 */ f32 mPlayerSlowPushUpSpeed;
	/* 0x158 */ f32 mPlayerBounceRate;
	/* 0x15C */ f32 mPlayerBounceRateY;
	/* 0x160 */ f32 mPlayerBounceKeep;
	/* 0x164 */ f32 mObjPushOutRate;
	/* 0x168 */ f32 mObjPushUpSpeed;
	/* 0x16C */ f32 mObjBounceRate;
	/* 0x170 */ f32 mKickRate;
	/* 0x174 */ f32 mKickRateY;
	/* 0x178 */ f32 mKickUpSpeed;
	/* 0x17C */ f32 mWaterPushRate;
	/* 0x180 */ f32 mSlopeRollRate;
	/* 0x184 */ f32 mWallRollUpRate;
	/* 0x188 */ f32 mMaxLandingSpeed;
	/* 0x18C */ f32 mFloatingDepth;
	/* 0x190 */ f32 mHoldHeightOffset;
	/* 0x194 */ int mHitCooldown;
};

// A fruit that reappears after being picked, thrown away or rotting.
class TResetFruit : public TMapObjBall {
public:
	TResetFruit(const char* name = "無限フルーツ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void control();
	virtual void makeObjAppeared();
	virtual void initMapObj();
	virtual void touchActor(THitActor*);
	virtual u32 touchWater(THitActor*);
	virtual u32 getLivingTime() const { return mFruitLivingTime; }
	virtual void appearing();
	virtual void breaking();
	virtual void waitingToAppear();
	virtual void hold(TTakeActor*);
	virtual void thrown();
	virtual void checkGroundCollision(JGeometry::TVec3<f32>*);
	virtual void touchGround(JGeometry::TVec3<f32>*);
	virtual void touchWaterSurface();
	virtual void touchPollution();
	virtual void kicked();

	void killByTimer(int);
	void makeObjLiving();
	void makeObjWaitingToAppear();

	static u32 mFruitLivingTime;
	static f32 mScaleUpSpeed;
	static f32 mBreakingScaleSpeed;
	static u32 mFruitWaitTimeToAppear;

public:
	/* 0x198 */ f32 mSandRiseRatio;
	/* 0x19C */ GXColorS10 mTevColor;
	/* 0x1A4 */ u8 unk1A4;
};

class TRandomFruit : public TResetFruit {
public:
	TRandomFruit(const char* name = "ランダムフルーツ");
	virtual void initMapObj();

public:
	/* 0x1A8 */ char mFruitName[0x20];
};

// The lid fruit of a fruit basket.
class TCoverFruit : public TMapObjBase {
public:
	TCoverFruit(const char* name = "フタのフルーツ")
	    : TMapObjBase(name)
	{
	}

	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void calcRootMatrix();
};

class TBigWatermelon : public TMapObjBall {
public:
	TBigWatermelon(const char* name = "お化けスイカ");

	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void control();
	virtual void kill();
	virtual void initMapObj();
	virtual void touchActor(THitActor*);
	virtual void appearing();
	virtual void touchGround(JGeometry::TVec3<f32>*);
	virtual void checkWallCollision(JGeometry::TVec3<f32>*);
	virtual void touchWall(JGeometry::TVec3<f32>*, TBGWallCheckRecord*);
	virtual void rebound(JGeometry::TVec3<f32>*);
	virtual void touchWaterSurface();

	void startEvent();

public:
	/* 0x198 */ TWaterEmitInfo* mWaterEmitInfo;
	/* 0x19C */ int mShineCoinNum;
	/* 0x1A0 */ f32 mSandRiseRatio;
};

#endif
