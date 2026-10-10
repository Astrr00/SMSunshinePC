#ifndef MOVE_BG_MAP_OBJ_MARE_HPP
#define MOVE_BG_MAP_OBJ_MARE_HPP

#include <MoveBG/MapObjBase.hpp>

struct TBGWallCheckRecord;
class TCannon;
class TCogwheel;
class TCogwheelScale;
class TMareEventDepressWall;

class TCogwheelScale : public TMapObjBase {
public:
	TCogwheelScale(const char*);

	virtual u32 touchWater(THitActor*);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void touchPlayer(THitActor*);
	virtual void control();

	static f32 mWaterLeakSpeed;

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ bool unk154;
	/* 0x158 */ TCogwheel* unk158;
};

class TCogwheel : public TMapObjBase {
public:
	TCogwheel(const char* name = "天秤");

	virtual void draw() const;
	virtual void calc();
	virtual void control();
	virtual void initMapObj();

	void initDraw() const;
	// inlined into control() and TCogwheelScale::touchPlayer()
	void rebound();

	static f32 mRopeWidthX;
	static f32 mRopeWidthZ;
	static f32 mTexPosRate;
	static f32 mMinSpeed;

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ TCogwheelScale* unk150;
	/* 0x154 */ JGeometry::TVec3<f32> unk154;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ TCogwheelScale* unk164;
	/* 0x168 */ JGeometry::TVec3<f32> unk168;
	/* 0x174 */ f32 unk174;
};

class TMapObjElasticCode : public TMapObjBase {
public:
	TMapObjElasticCode(const char* name = "ゴムひも")
	    : TMapObjBase(name)
	    , unk138(0.0f)
	    , unk13C(0.0f)
	    , unk140(0.0f)
	{
	}

	virtual void draw() const;
	virtual void control();
	virtual void initMapObj();

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
};

class TMapObjGrowTree : public TMapObjBase {
public:
	TMapObjGrowTree(const char* name = "もやしの木");

	virtual u32 touchWater(THitActor*);
	virtual void control();
	virtual void loadAfter();
	virtual void initMapObj();

	// both inlined into control() and touchWater()
	f32 getGrowHeightFromRate(f32) const;
	void updateHeight();

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ u32 unk144;
	/* 0x148 */ f32 unk148;
};

class TWireBell : public TMapObjBase {
public:
	TWireBell(const char* name = "ワイヤー鈴（紫）");

	virtual void draw() const;
	virtual void control();
	virtual void loadAfter();

	void initDraw() const;

public:
	/* 0x138 */ s32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ JGeometry::TVec3<f32> unk14C;
};

class TMapObjPuncher : public TMapObjBase {
public:
	TMapObjPuncher(const char* name = "パンチャー")
	    : TMapObjBase(name)
	    , unk138(0.0f)
	{
	}

	virtual void touchPlayer(THitActor*);
	virtual void control();
	virtual void load(JSUMemoryInputStream&);

public:
	/* 0x138 */ f32 unk138;
};

class TMuddyBoat : public TMapObjBase {
public:
	TMuddyBoat(const char* name = "どろの船");

	virtual void calcRootMatrix();
	virtual void control();
	virtual void bind();
	virtual void kill();
	virtual void initMapObj();
	virtual u32 getSDLModelFlag() const;
	virtual void calc();

	void moveByWater();
	// both inlined into bind()
	void touchWall(JGeometry::TVec3<f32>* pos, const TBGWallCheckRecord& record);
	bool bindToWall(const JGeometry::TVec3<f32>& center, f32 radius,
	                JGeometry::TVec3<f32>* pos);

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
	/* 0x14C */ f32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
	/* 0x15C */ f32 unk15C;
	/* 0x160 */ f32 unk160;
	/* 0x164 */ f32 unk164;
	/* 0x168 */ s32 unk168;
	/* 0x16C */ s32 unk16C;
	/* 0x170 */ JGeometry::TVec3<f32> unk170;
	/* 0x17C */ JGeometry::TVec3<f32> unk17C;
};

class TMareFall : public TMapObjBase {
public:
	TMareFall(const char* name = "マーレ滝")
	    : TMapObjBase(name)
	{
	}

	virtual void calc();
	virtual void load(JSUMemoryInputStream&);
};

class TMareCork : public TMapObjBase {
public:
	TMareCork(const char* name = "マーレコルク")
	    : TMapObjBase(name)
	    , unk138(nullptr)
	    , unk154(false)
	{
	}

	virtual void loadAfter();
	virtual void moveObject();
	virtual void calcRootMatrix();
	virtual MtxPtr getTakingMtx();
	virtual void drawObject(JDrama::TGraphics*);

public:
	/* 0x138 */ TCannon* unk138;
	/* 0x13C */ JGeometry::TVec3<f32> unk13C;
	/* 0x148 */ JGeometry::TVec3<f32> unk148;
	/* 0x154 */ bool unk154;
};

class TMareEventPoint : public THitActor {
public:
	TMareEventPoint(const char* name = "イベントポイント")
	    : THitActor(name)
	    , unk68(nullptr)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void load(JSUMemoryInputStream&);

public:
	/* 0x68 */ TMareEventDepressWall* unk68;
};

#endif
