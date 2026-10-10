#ifndef MOVE_BG_MAP_OBJ_RICCO_HPP
#define MOVE_BG_MAP_OBJ_RICCO_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjBlock.hpp>
#include <MoveBG/Item.hpp>

class JAISound;
class TFruitLauncher;

// Crane arm that swings around the Y axis between two angles.
class TCraneRotY : public TMapObjBase {
public:
	TCraneRotY(const char* name = "Ｙ軸回転クレーン")
	    : TMapObjBase(name)
	    , unk138(0.0f)
	    , unk13C(0.0f)
	    , unk140(0.0f)
	    , unk144(0.0f)
	    , unk148(0)
	{
	}

	virtual void load(JSUMemoryInputStream&);
	virtual void control();
	virtual void calc();

	static u32 mWaitTime;

public:
	/* 0x138 */ f32 unk138; // initial Y rotation
	/* 0x13C */ f32 unk13C; // lower swing offset (never loaded, stays 0)
	/* 0x140 */ f32 unk140; // upper swing offset (loaded from the scene)
	/* 0x144 */ f32 unk144; // rotation speed
	/* 0x148 */ u32 unk148; // sound id
};

// Crane arm that swings up and down and carries a cargo object.
class TCraneUpDown : public TMapObjBase {
public:
	TCraneUpDown(const char* name = "上下クレーン")
	    : TMapObjBase(name)
	    , unk138(nullptr)
	    , unk13C(0)
	{
	}

	virtual void control();
	virtual void initMapObj();

	static f32 mRotSpeed;
	static u32 mWaitTime;

public:
	/* 0x138 */ TMapObjBase* unk138; // cargo
	/* 0x13C */ u32 unk13C;          // sound id
	/* 0x140 */ f32 unk140;          // max angle
	/* 0x144 */ f32 unk144;          // min angle
};

class TCraneCargo : public TLeanBlock {
public:
	TCraneCargo()
	    : TLeanBlock("クレーン積み荷")
	{
	}

	virtual void control();
	virtual void calc();
};

class TRiccoWatermill : public TMapObjBase {
public:
	TRiccoWatermill(const char* name = "リコ水車");

	virtual void loadAfter();
	virtual void control();
	virtual void calc();
	virtual u32 touchWater(THitActor*);

	static f32 mRotAccel;
	static f32 mRotSpeedMaxUp;
	static f32 mRotSpeedMaxDown;
	static f32 mRotDown;
	static f32 mSubmarineMoveRate;
	static f32 mSubmarineMaxTransY;
	static f32 mSubmarineBottomTransY;
	static u32 mWaitTime;
	static f32 mSubmarineSurfaceTransY;

public:
	/* 0x138 */ f32 unk138;           // wheel rotation speed
	/* 0x13C */ TMapObjBase* unk13C;  // submarine
	/* 0x140 */ int unk140;           // water hit grace timer
	/* 0x144 */ bool unk144;          // submarine already threw the coin
	/* 0x148 */ TMapObjBase* unk148;  // blue coin
	/* 0x14C */ JAISound* unk14C;     // wheel sound handle
	/* 0x150 */ JAISound* unk150;     // crane sound handle
	/* 0x154 */ JAISound* unk154;     // sinking sound handle
};

class TSurfGesoObj : public TItem {
public:
	TSurfGesoObj(const char* name = "イカサーフィン")
	    : TItem(name)
	{
	}

	virtual void initMapObj();

public:
	/* 0x154 */ GXColorS10 unk154;
};

class TFruitSwitch : public TMapObjBase {
public:
	TFruitSwitch(const char* name = "フルーツスイッチ")
	    : TMapObjBase(name)
	    , unk138(nullptr)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void pullUp();
	void pushDown();

public:
	/* 0x138 */ TFruitLauncher* unk138;
};

class TFruitLauncher : public TMapObjBase {
public:
	TFruitLauncher(const char* name = "フルーツ発射口")
	    : TMapObjBase(name)
	{
	}

	virtual void loadAfter();

	TMapObjBase* appearFruit() const;
	void fireObj();

	static f32 mObjSpeedXZ;
	static f32 mObjSpeedY;
	static u32 mFruitLiveTime;

public:
	/* 0x138 */ TFruitSwitch* unk138[2]; // the two switches (A, B)
	/* 0x140 */ int unk140;              // index of the switch that is up
};

#endif
