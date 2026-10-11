#ifndef MOVE_BG_MAP_OBJ_MONTE_HPP
#define MOVE_BG_MAP_OBJ_MONTE_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>

class JAISound;
class TFluffManager;

class TMapObjMonteRoot : public TMapObjBase {
public:
	TMapObjMonteRoot(const char* name = "根っこ")
	    : TMapObjBase(name)
	{
	}

	void initMapObj();
};

class TJumpMushroom : public TMapObjBase {
public:
	TJumpMushroom(const char* name = "ジャンプきのこ")
	    : TMapObjBase(name)
	{
	}

	BOOL receiveMessage(THitActor*, u32);
	void load(JSUMemoryInputStream&);
};

class THangingBridge;

class THangingBridgeBoard : public TLeanBlock {
public:
	THangingBridgeBoard(const char*);

	void drawOneRope(const JGeometry::TVec3<f32>&) const;
	void drawRopes() const;
	void pushNeighbor(f32);
	void control();
	void calcDefaultMtx();
	void setGroundCollision();
	void initMapObj();

	static f32 mMarioAccelY;
	static f32 mMarioHipDropAccelY;
	static f32 mReturnAccelRate;
	static f32 mSpeedDownRate;
	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;

public:
	/* 0x194 */ THangingBridgeBoard* mPrev;
	/* 0x198 */ THangingBridgeBoard* mNext;
	/* 0x19C */ THangingBridgeBoard* mPrev2;
	/* 0x1A0 */ THangingBridgeBoard* mNext2;
	/// Rope attachment points on both sides of the board.
	/* 0x1A4 */ JGeometry::TVec3<f32> mRopePos[2];
	/* 0x1BC */ THangingBridge* mBridge;
};

class THangingBridge : public JDrama::TViewObj {
public:
	THangingBridge(const char* name = "つり橋");

	void drawLowerMinus(const JGeometry::TVec3<f32>&,
	                    const JGeometry::TVec3<f32>&,
	                    const JGeometry::TVec2<f32>&, int) const;
	void drawLowerPlus(const JGeometry::TVec3<f32>&,
	                   const JGeometry::TVec3<f32>&,
	                   const JGeometry::TVec2<f32>&, int) const;
	void drawUpper(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&,
	               const JGeometry::TVec2<f32>&, int) const;
	void drawRopeBetweenBoards(f32, int) const;
	void initDraw() const;
	void perform(u32, JDrama::TGraphics*);
	void loadAfter();
	inline void initMonte();

	static f32 mRopeWidthBetweenBoards;
	static f32 mRopeWidthBetweenBoardsY;
	static int mPointNumBetweenBoards;
	static f32 mBetweenBoardsTexPosRate;
	static f32 mRopeHeight;

public:
	/* 0x10 */ int mBoardNum;
	/* 0x14 */ THangingBridgeBoard** mBoards;
	/* 0x18 */ JGeometry::TVec3<f32> mStartPos;
	/* 0x24 */ JGeometry::TVec3<f32> mEndPos;
	/// Direction of the bridge in the XZ plane, rotated by 90 degrees.
	/* 0x30 */ JGeometry::TVec2<f32> mSideDir;
	/// Vertical offsets of the rope between two boards.
	/* 0x38 */ f32* mSagTable;
	/* 0x3C */ f32 mBoardHalfWidth;
	/* 0x40 */ f32 mNeighborPush1;
	/* 0x44 */ f32 mNeighborPush2;
};

class TSwingBoard : public TMapObjBase {
public:
	TSwingBoard(const char* name = "つり橋");

	void drawOneRope(const JGeometry::TVec3<f32>&,
	                 const JGeometry::TVec3<f32>&) const;
	void initDraw() const;
	void draw() const;
	void control();
	void load(JSUMemoryInputStream&);

	static f32 mBoardWidth;
	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;
	static f32 mReturnAccelRate;
	static f32 mSpeedDownRate;

public:
	/* 0x138 */ f32 mRopeLength;
	/* 0x13C */ f32 mSwingAngle;
	/* 0x140 */ f32 mPushAccel;
	/* 0x144 */ f32 mAngularSpeed;
	/* 0x148 */ f32 mSlowDownSpeed;
	/* 0x14C */ TMtx34f mYawMtx;
	/* 0x17C */ JGeometry::TVec3<f32> mPivot;
	/* 0x188 */ JAISound* mSound;
};

class TGoalFlag : public TMapObjBase {
public:
	TGoalFlag(const char* name = "ゴールフラグ")
	    : TMapObjBase(name)
	{
	}

	f32 getRadiusAtY(f32) const { return 20.0f; }
	void touchActor(THitActor*);
	void initMapObj();
};

class TFluff : public TMapObjBase {
public:
	TFluff(const char*);

	f32 getRadiusAtY(f32) const { return 20.0f; }
	u32 touchWater(THitActor*);
	void move();
	void kill();
	void control();
	void appear();
	void initMapObj();

	static f32 mScaleUpSpeed;
	static f32 mScaleDownSpeed;

public:
	/* 0x138 */ f32 mSwayRadius;
	/* 0x13C */ f32 mFallSpeed;
	/* 0x140 */ f32 mHeadingSin;
	/* 0x144 */ f32 mHeadingCos;
	/* 0x148 */ f32 mSwayAngle;
	/* 0x14C */ f32 mSwaySpeed;
	/* 0x150 */ f32 mWindRatio;
	/* 0x154 */ JGeometry::TVec3<f32> mWindOffset;
	/* 0x160 */ f32 mWaterPushRate;
	/* 0x164 */ f32 mVelocityDamping;
	/* 0x168 */ TFluffManager* mManager;
	/* 0x16C */ bool mIsLeader;
};

class TFluffManager : public TMapObjBase {
public:
	TFluffManager(const char* name = "特別な綿毛");

	void findNextFluff();
	void control();
	void registerNextFluff(TFluff*);
	void setUpNextFluff();
	TFluff* newFluff(const char*);
	f32 getRandomX() const;
	f32 getRandomZ() const;
	void loadAfter();
	void load(JSUMemoryInputStream&);

	static f32 mWindMin;

public:
	/* 0x138 */ f32 mRangeX;
	/* 0x13C */ f32 mRangeZ;
	/* 0x140 */ f32 mDropHeight;
	/* 0x144 */ s32 mWindFrames;
	/* 0x148 */ JGeometry::TVec3<f32> mWind;
	/* 0x154 */ f32 mWindDecay;
	/* 0x158 */ TFluff* mCurrentFluff;
	/* 0x15C */ TFluff* mNextFluff;
	/* 0x160 */ int mFluffNum;
	/* 0x164 */ int mFluffCapacity;
	/* 0x168 */ TFluff** mFluffs;
};

#endif
