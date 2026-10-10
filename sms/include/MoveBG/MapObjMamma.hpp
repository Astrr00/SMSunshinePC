#ifndef MOVE_BG_MAP_OBJ_MAMMA_HPP
#define MOVE_BG_MAP_OBJ_MAMMA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjEx.hpp>

class J3DJoint;
class JPABaseEmitter;
class TJointModel;
class MActor;
class TSandBase;
class TShiningStone;

/// Sandcastle egg, only a model.
class TSandEgg : public TMapObjBase {
public:
	TSandEgg(const char* name = "すなのたまご")
	    : TMapObjBase(name)
	{
	}

	virtual u32 getSDLModelFlag() const;
};

/// Hides the parts of the terrain that are inside the mirror's view.
class TMammaMirrorMapOperator : public JDrama::TViewObj {
public:
	TMammaMirrorMapOperator(const char* name = "鏡内地形操作");

	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

public:
	/* 0x10 */ J3DJoint* mJoints[8];
	/* 0x30 */ JGeometry::TVec3<f32> mJointPos[8];
	/* 0x90 */ f32 mJointRadius[8];
	/* 0xB0 */ bool mJointHidden[8];
	/* 0xB8 */ JGeometry::TVec3<f32> mMirrorPos[3];
};

/// Goal of the watermelon event. Shrinks the watermelon that gets touched and
/// then makes the ghost watermelon's Shine appear.
class TGoalWatermelon : public TMapObjBase {
public:
	TGoalWatermelon(const char* name = "スイカゴール");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void control();
	virtual void touchActor(THitActor*);

public:
	/* 0x138 */ TMapObjBase* mShine;
	/* 0x13C */ TMapObjBase* mWatermelon;
	/* 0x140 */ JGeometry::TVec3<f32> mShinePos;
};

/// The big sand bird. A joint coin whose joints that aren't named "none" turn
/// into blocks.
class TSandBird : public TJointCoin {
public:
	TSandBird(const char* name = "おおすな鳥");

	virtual void control();
	virtual void initMapObj();
	virtual bool nameIsObj(const char*);
	virtual TMapObjBase* makeObjFromJointName(const char*, u16);

public:
	/* 0x148 */ u32 unk148;
	/* 0x14C */ u32 unk14C;
	/* 0x150 */ bool mBalloonShown;
	/* 0x151 */ bool mBalloonHidden;
};

class TMapObjFlag;

/// Yacht that bobs on the waves, carries a flag.
class TMammaYacht : public TMapObjBase {
public:
	TMammaYacht(const char* name = "砂の城")
	    : TMapObjBase(name)
	{
	}

	virtual void control();
	virtual void initMapObj();

public:
	/* 0x138 */ TMapObjFlag* mFlag;
};

/// Block of the sun tower that turns when it is sprayed.
class TMammaBlockRotate : public TMapObjBase {
public:
	TMammaBlockRotate(const char* name = "太陽の塔ブロック");

	virtual void load(JSUMemoryInputStream&);
	virtual void control();
	virtual void initMapObj();
	virtual u32 touchWater(THitActor*);

	enum {
		STATE_GO   = 2,
		STATE_WAIT = 3,
		STATE_BACK = 4,
	};

public:
	/* 0x138 */ TJointModel* mMapModel;
	/* 0x13C */ TJointObj* mJointDown;
	/* 0x140 */ TJointObj* mJointUp;
	/* 0x144 */ TMapCollisionMove* mColDown;
	/* 0x148 */ TMapCollisionMove* mColUp;

	static s32 mWaitTime;
	static f32 mRotSpeed;
	static f32 mRotReturnSpeed;
	static f32 mRotEnd;
	static f32 mMapGoSpeed;
	static f32 mMapBackSpeed;
};

/// One of the four stones of the sun stone. They get lit by the lean mirrors.
class TShiningStone : public THitActor {
public:
	TShiningStone(const char* name = "太陽石");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void putOnLight(TLiveActor*);

public:
	/* 0x68 */ MActor** mStoneActors;
	/* 0x6C */ MActor* mLightActor;
	/* 0x70 */ bool mStoneLit[4];
	/* 0x74 */ s32 mLightNum;
	/* 0x78 */ JPABaseEmitter* mEmitter;
	/* 0x7C */ f32 mEmitterRate;
};

/// Mirror that leans over when Mario or an enemy touches it. Once leaned
/// completely it reflects the light onto the sun stone.
class TLeanMirror : public TMapObjBase {
public:
	TLeanMirror(const char* name = "ぐらぐら鏡");

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void loadAfter();
	virtual void control();
	virtual void initMapObj();
	virtual u32 getSDLModelFlag() const;
	virtual void draw() const;
	virtual void touchPlayer(THitActor*);
	virtual void touchEnemy(THitActor*);
	virtual void load(JSUMemoryInputStream&);

	void addShake(THitActor*, f32);
	void controlShake();
	void controlGoTarget();
	void release();

	enum {
		STATE_SHAKE     = 1,
		STATE_GO_TARGET = 2,
		STATE_DEMO      = 3,
		STATE_DONE      = 4,
	};

	static s32 mGoTargetTime;
	static s32 mDemoWaitTime;
	static s32 mDemoLightTime;

public:
	/* 0x138 */ f32 mHalfSize;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ JGeometry::TVec3<f32> unk140;
	/* 0x14C */ JGeometry::TVec3<f32> mShake;
	/* 0x158 */ f32 mShakeSoundRate;
	/* 0x15C */ f32 mShakeDamping;
	/* 0x160 */ f32 mPlayerRate;
	/* 0x164 */ f32 mTrampleRate;
	/* 0x168 */ f32 mHipDropRate;
	/* 0x16C */ f32 mEnemyRate;
	/* 0x170 */ f32 mSuperHipDropRate;
	/* 0x174 */ f32 mTiltLimit;
	/* 0x178 */ f32 mBounceRate;
	/* 0x17C */ TShiningStone* mStone;
	/* 0x180 */ JGeometry::TVec3<f32> mStoneDir;
	/* 0x18C */ JGeometry::TVec3<f32> mFallAxis;
	/* 0x198 */ f32 mFallSpeed;
	/* 0x19C */ s32 mHitsLeft;
	/* 0x1A0 */ JGeometry::TVec3<f32> mMarioMovePos;
	/* 0x1AC */ bool mBgmStarted;
	/* 0x1AE */ u16 unk1AE;
};

/// Common code of the sand piles: a leaf object (TSandLeaf) that sits on top
/// of the pile, which grows and withers.
class TSandLeaf : public TMapObjBase {
public:
	TSandLeaf(const char* name = "すなやまの芽")
	    : TMapObjBase(name)
	    , mBase(nullptr)
	{
	}

	virtual void control();
	virtual u32 touchWater(THitActor*);

public:
	/* 0x138 */ TSandBase* mBase;
};

class TSandBase : public TMapObjBase {
public:
	TSandBase(const char* name);

	virtual void grow() = 0;
	virtual bool withering();

	enum {
		STATE_WITHER = 2,
		STATE_WAIT   = 3,
		STATE_GROWN  = 4,
		STATE_GROWN2 = 5,
	};

	static s32 mWitherTime;
	static f32 mScaleMin;

public:
	/* 0x138 */ f32 mGrowSpeed;
	/* 0x13C */ f32 mWitherSpeed;
	/* 0x140 */ s32 mWaitTime;
	/* 0x144 */ TSandLeaf* mLeaf;
};

class TSandLeafBase : public TSandBase {
public:
	TSandLeafBase(const char* name = "すなやまの芽の土台")
	    : TSandBase(name)
	{
	}

	virtual void grow();
	virtual void control();
	virtual void initMapObj();
};

class TSandBomb : public TSandLeaf {
public:
	TSandBomb()
	    : TSandLeaf("すなやま爆弾")
	    , unk13C(0)
	    , mTouched(false)
	{
	}

	virtual void makeObjAppeared();
	virtual u32 touchWater(THitActor*);
	virtual u32 getSDLModelFlag() const;
	virtual void initMapObj();

public:
	/* 0x13C */ u32 unk13C;
	/* 0x140 */ bool mTouched;
};

class TSandBombBase : public TSandBase {
public:
	TSandBombBase(const char* name = "すなやま爆弾の土台");

	virtual void loadAfter();
	virtual void control();
	virtual void initMapObj();
	virtual void grow();
	virtual void waitBeforeExplode();
	virtual void explode();
	virtual void exploding();
	virtual void expanded();
	virtual void withered();
	virtual TSandBomb* findTriggerActor();

	enum {
		STATE_FIRING             = 5,
		STATE_WAIT_BEFORE_EXPLODE = 6,
		STATE_EXPLODING          = 7,
		STATE_EXPANDED           = 8,
	};

	static f32 mFiringFrameSpeed;
	static f32 mFiringFrameDownSpeed;
	static f32 mExplodeFrameSpeed;
	static f32 mMarioJumpRate;
	static s32 mExlodingRumbleTime;

public:
	/* 0x148 */ s32 mExplodeWaitTime;
	/* 0x14C */ f32 mExplodeScale;
	/* 0x150 */ f32 mExpandFrameSpeed;
	/* 0x154 */ f32 mExplodeRadius;
};

class TSandCastle : public TSandBombBase {
public:
	TSandCastle(const char* name = "砂の城");

	virtual bool withering();
	virtual void calcRootMatrix();
	virtual void loadAfter();
	virtual void initMapObj();
	virtual void waitBeforeExplode();
	virtual void explode();
	virtual void expanded();
	virtual TSandBomb* findTriggerActor();

	static f32 mCollisionRate;

public:
	/* 0x158 */ TMapObjBase* mStageSwitch;
	/* 0x15C */ bool mCameraDemo;
};

#endif
