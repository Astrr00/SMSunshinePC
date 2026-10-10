#ifndef MOVE_BG_MAP_OBJ_CORONA_HPP
#define MOVE_BG_MAP_OBJ_CORONA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <Map/BathWaterManager.hpp>
#include <JSystem/JGeometry.hpp>
#include <new>

class MActorAnmData;
class TBathtub;
class TBathtubGrip;
class TBathtubGripParts;
class MActor;

// The retail ctor/load/calcBathtubData treat TBathtubData::unk18 as a row-major
// SMatrix33R (the ctor calls SMatrix33R::SMatrix33R and identity()/setXDir()
// store a direction as three contiguous floats). The shared header declares it
// as the SMatrix33C view that Map/BathWaterManager.cpp indexes, so go through
// this layout-identical view here instead of changing that type under it.
typedef JGeometry::TRotation3<JGeometry::TMatrix33<JGeometry::SMatrix33R<f32> > >
    TBathtubRot;

// TBathtubData whose rotation member is constructed as the row-major view.
class TBathtubDataR : public TBathtubData {
public:
	TBathtubDataR() { ::new (&unk18) TBathtubRot; }
};

class TBathtubParams : public TParams {
public:
	TBathtubParams();

	/* 0x008 */ TParamRT<u8> resetGrip;
	/* 0x01C */ TParamRT<s32> trampleRelease;
	/* 0x030 */ TParamRT<s32> trampleRecover;
	/* 0x044 */ TParamRT<s32> quakeRelease;
	/* 0x058 */ TParamRT<s32> quakeRecover;
	/* 0x06C */ TParamRT<s32> hipdropRelease;
	/* 0x080 */ TParamRT<s32> hipdropRecover;
	/* 0x094 */ TParamRT<s32> breakCount0;
	/* 0x0A8 */ TParamRT<s32> breakCount1;
	/* 0x0BC */ TParamRT<s32> breakCount2;
	/* 0x0D0 */ TParamRT<s32> breakCount3;
	/* 0x0E4 */ TParamRT<s32> launchStopCount;
	/* 0x0F8 */ TParamRT<f32> animSpeed0;
	/* 0x10C */ TParamRT<f32> animSpeed1;
	/* 0x120 */ TParamRT<f32> animSpeed2;
	/* 0x134 */ TParamRT<f32> animSpeed3;
	/* 0x148 */ TParamRT<f32> animSpeed4;
	/* 0x15C */ TParamRT<f32> shake;
	/* 0x170 */ TParamRT<f32> watermark;
	/* 0x184 */ TParamRT<f32> maxAngle;
	/* 0x198 */ TParamRT<f32> angleVelDamp;
	/* 0x1AC */ TParamRT<f32> rebound;
	/* 0x1C0 */ TParamRT<f32> shakeDamp;
	/* 0x1D4 */ TParamRT<f32> marioWeight;
	/* 0x1E8 */ TParamRT<f32> marioDropWeight;
	/* 0x1FC */ TParamRT<f32> outerHeight;
};

class TBathtub : public TMapObjBase {
public:
	TBathtub(const char* name = "バスタブ");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual MtxPtr getTakingMtx();
	virtual Mtx* getRootJointMtx() const;
	virtual void calcRootMatrix();
	virtual void control();

	void hipdrop(const JGeometry::TVec3<f32>&);
	void quake(const JGeometry::TVec3<f32>&);
	int getNumGripsDead() const;
	void tumble(f32, f32);
	MtxPtr getSubmarineMtxInDemo();
	MtxPtr getPeachMtxInDemo();
	MtxPtr getKoopaJrMtxInDemo();
	void calcBathtubData();
	void setupCollisions_();
	void removeCollisions_();
	void startDemo();
	bool allowsTumble() const;
	bool getNearGrip(const JGeometry::TVec3<f32>&, f32, f32*) const;
	f32 getNextJuncture(const JGeometry::TVec3<f32>&,
	                    const JGeometry::TVec3<f32>&) const;
	bool getNextGrip(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&,
	                 f32, f32*) const;
	void updatePosture_();
	int getNumKillerLaunchable() const;
	bool isKillerAttackable() const;
	u8 getNumKillerBurstable() const;
	bool isKillerLaunchable() const;
	MtxPtr getKoopaMtxInDemo();

	const TBathtubData& getBathtubData() const { return mBathtubData; }

public:
	/* 0x138 */ MActorAnmData* unk138;
	/* 0x13C */ f32 unk13C[5]; // grip junction angles
	/* 0x150 */ f32 unk150[5]; // grip angles
	/* 0x164 */ TMapCollisionMove** unk164;
	/* 0x168 */ TBathtubGrip** unk168;
	/* 0x16C */ TBathtubParams* unk16C;
	/* 0x170 */ TBathtubDataR mBathtubData;
	/* 0x1D8 */ JGeometry::TQuat4<f32> mQuat; // tilt
	/* 0x1E8 */ JGeometry::TVec3<f32> mAngularVelocity;
	/* 0x1F4 */ JGeometry::TVec3<f32> unk1F4; // inner bottom, steam position
	/* 0x200 */ JGeometry::TVec3<f32> unk200;
	/* 0x20C */ u8 unk20C[0x30];
	/* 0x23C */ f32 unk23C;
	/* 0x240 */ f32 unk240;
	/* 0x244 */ f32 unk244;
	/* 0x248 */ int unk248; // killers cannot be attacked while > 0
	/* 0x24C */ int unk24C; // quake timer
	/* 0x250 */ int unk250; // wobble release timer
	/* 0x254 */ int unk254;
	/* 0x258 */ int unk258; // recover counter
	/* 0x25C */ int unk25C; // recover counter start
	/* 0x260 */ int mMarioJntIdx;
	/* 0x264 */ int mStarJntIdx;
	/* 0x268 */ int mShineBodyJntIdx;
	/* 0x26C */ int mSubmarineJntIdx;
	/* 0x270 */ int mDuckJntIdx;
	/* 0x274 */ int mJuniorJntIdx;
	/* 0x278 */ int mKoopaJntIdx;
	/* 0x27C */ int mWater4JntIdx;
	/* 0x280 */ int mWater5JntIdx;
	/* 0x284 */ int mWater1JntIdx;
	/* 0x288 */ int mWater2JntIdx;
	/* 0x28C */ int mWater3JntIdx;
	/* 0x290 */ int unk290;
	/* 0x294 */ int unk294;
	/* 0x298 */ u8 unk298;
	/* 0x299 */ u8 unk299;
	/* 0x29A */ u8 unk29A;
	/* 0x29C */ MActor* unk29C;
	/* 0x2A0 */ u32 unk2A0;
};

class TBathtubGrip : public TMapObjBase {
public:
	TBathtubGrip(TBathtub*, f32, MActorAnmData*, const char*);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual Mtx* getRootJointMtx() const;
	virtual void calcRootMatrix();
	virtual void control();
	virtual void kill();
	void removeCollisions_();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

public:
	/* 0x138 */ JGeometry::TVec3<f32> unk138[2]; // rumble pos, sound pos
	/* 0x150 */ TMapCollisionMove* mColsA[5];    // "b%d" breakable pieces
	/* 0x164 */ TMapCollisionMove* mColsB[17];   // "c%d" pieces
	/* 0x1A8 */ TBathtubGripParts* mPartsA[5];   // Fragile
	/* 0x1BC */ TBathtubGripParts* mPartsB[17];  // Hard
	/* 0x200 */ int mJointIdx[17];               // joint "c%d"
	/* 0x244 */ TBathtub* mOwner;
	/* 0x248 */ u8 unk248; // breaking
	/* 0x249 */ u8 unk249; // intact (0 once broken)
	/* 0x24A */ u8 unk24A; // killed
	/* 0x24B */ u8 unk24B; // collision pieces follow the model
	/* 0x24C */ f32 mAngle;
	/* 0x250 */ f32 unk250; // break animation speed
	/* 0x254 */ int unk254; // hit timer, 0 = not hit
	/* 0x258 */ int unk258; // hit timer limit
	/* 0x25C */ MActor* unk25C; // stand_effect model
	/* 0x260 */ u8 unk260;      // effect finished
};

class TBathtubGripParts : public TLiveActor {
public:
	TBathtubGripParts(TBathtubGrip* grip, int index, const char* name)
	    : TLiveActor(name)
	    , mGrip(grip)
	    , mIndex(index)
	{
	}

	virtual Mtx* getRootJointMtx() const;

public:
	/* 0xF4 */ TBathtubGrip* mGrip;
	/* 0xF8 */ int mIndex;
};

class TBathtubGripPartsHard : public TBathtubGripParts {
public:
	TBathtubGripPartsHard(TBathtubGrip* grip, int index)
	    : TBathtubGripParts(grip, index, "バスタブの足場の一部（壊れない）")
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
};

class TBathtubGripPartsFragile : public TBathtubGripParts {
public:
	TBathtubGripPartsFragile(TBathtubGrip* grip, int index)
	    : TBathtubGripParts(grip, index, "バスタブの足場の一部（弱点）")
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
};

#endif
