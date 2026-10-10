#ifndef MOVE_BG_MAP_OBJ_PINNA_HPP
#define MOVE_BG_MAP_OBJ_PINNA_HPP

#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/MapObjGeneral.hpp>
#include <MoveBG/MapObjTown.hpp>

class TCoin;

class TFerrisWheel : public TMapObjBase {
public:
	TFerrisWheel(const char* name = "観覧車");

	virtual void control();
	virtual void initMapObj();

#ifdef TARGET_PC
	static s32 becomeCalmlyCallback(uintptr_t, u32);
#else
	static s32 becomeCalmlyCallback(u32, u32);
#endif

public:
	/* 0x138 */ s32 unk138;
	/* 0x13C */ TMapObjBase** unk13C;
	/* 0x140 */ f32 unk140;
};

class THorizontalViking : public TMapObjBase {
public:
	THorizontalViking(const char* name)
	    : TMapObjBase(name)
	    , unk138(0.0f)
	    , unk13C(0.0f)
	    , unk140(0.0f)
	    , unk144(0.0f)
	    , unk148(0.0f)
	{
	}

	virtual void control();
	virtual void initMapObj();
	virtual void reset();

public:
	/* 0x138 */ f32 unk138;
	/* 0x13C */ f32 unk13C;
	/* 0x140 */ f32 unk140;
	/* 0x144 */ f32 unk144;
	/* 0x148 */ f32 unk148;
};

class TViking : public THorizontalViking {
public:
	TViking(const char* name = "バイキング");

	virtual void control();
	virtual void initMapObj();
	virtual void loadAfter();
	virtual void reset();

	void roll();

public:
	/* 0x14C */ s32 unk14C;
	/* 0x150 */ f32 unk150;
	/* 0x154 */ f32 unk154;
	/* 0x158 */ f32 unk158;
};

class TPinnaShell : public THitActor {
public:
	TPinnaShell()
	    : THitActor("シェル")
	    , unk68(0)
	    , unk6C(0.0f)
	    , unk70(0.0f)
	    , unk74(nullptr)
	    , unk78(nullptr)
	    , unk7C(0)
	    , unk80(nullptr)
	    , unk84(nullptr)
	    , unk88(nullptr)
	    , unk8C(nullptr)
	{
		initHitActor(0x4000013A, 1, 0x80000000, 250.0f, 400.0f, 250.0f, 200.0f);
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void control();

public:
	/* 0x68 */ s32 unk68;
	/* 0x6C */ f32 unk6C;
	/* 0x70 */ f32 unk70;
	/* 0x74 */ MtxPtr unk74;
	/* 0x78 */ J3DJoint* unk78;
	/* 0x7C */ s32 unk7C;
	/* 0x80 */ TMapObjBase* unk80;
	/* 0x84 */ TMapCollisionMove* unk84;
	/* 0x88 */ TDamageObj* unk88;
	/* 0x8C */ TMapObjBase* unk8C;
};

class TShellCup : public TMapObjBase {
public:
	TShellCup(const char* name = "シェルカップ");

	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void control();
	virtual void initMapObj();

	static f32 mOpenRotMax;
	static f32 mShellDamageRot;
	static f32 mWaterOpenAccel;
	static f32 mCloseAccel;

public:
	/* 0x138 */ TPinnaShell unk138[6];
	/* 0x498 */ TMapObjBase* unk498;
	/* 0x49C */ TCoin* unk49C;
	/* 0x4A0 */ TCoin* unk4A0;
};

class TMerryPole;
class TChangeStageMerrygoround;

class TMerrygoround : public TMapObjBase {
public:
	TMerrygoround(const char* name = "メリーゴーランド");

	virtual void control();
	virtual void draw() const;
	virtual void initMapObj();

	static f32 mRotSpeed;

public:
	/* 0x138 */ TMapObjBase* unk138[2];
	/* 0x140 */ u16 unk140[2];
	/* 0x144 */ TMerryPole* unk144[9];
	/* 0x168 */ TMapCollisionMove* unk168[9];
	/* 0x18C */ u16 unk18C[9];
	/* 0x1A0 */ TChangeStageMerrygoround* unk1A0;
	/* 0x1A4 */ u16 unk1A4;
};

class TChangeStageMerrygoround : public TMapObjChangeStage {
public:
	TChangeStageMerrygoround()
	    : TMapObjChangeStage("ステージ切り替え（メリーゴーランド用）")
	    , unk13C(0)
	{
	}

	virtual void calc();
	virtual void touchPlayer(THitActor*);

public:
	/* 0x13C */ u8 unk13C;
};

class TBalloonKoopaJr : public TMapObjGeneral {
public:
	TBalloonKoopaJr(const char* name = "風船（クッパＪｒ）")
	    : TMapObjGeneral(name)
	{
		unk148.zero();
	}

	virtual void load(JSUMemoryInputStream&);
	virtual void kill();
	virtual void touchActor(THitActor*);

public:
	/* 0x148 */ JGeometry::TVec3<f32> unk148;
};

class TPinnaEntrance : public TMapObjBase {
public:
	TPinnaEntrance(const char* name = "ピンナ入り口")
	    : TMapObjBase(name)
	{
	}

	virtual void loadAfter();
};

class TWaterRecoverObj : public TMapObjBase {
public:
	TWaterRecoverObj(const char* name = "水回復オブジェ")
	    : TMapObjBase(name)
	{
	}

	virtual void touchPlayer(THitActor*);
};

class TAmiKing : public TMapObjBase {
public:
	TAmiKing(const char* name = "アミキング")
	    : TMapObjBase(name)
	    , unk138(0)
	{
	}

	virtual void loadAfter();
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void moveObject();
	virtual void initMapObj();
	virtual void touchPlayer(THitActor*);
	virtual u32 touchWater(THitActor*) { return 1; }

public:
	/* 0x138 */ u8 unk138;
	/* 0x13C */ JGeometry::TVec3<f32> unk13C;
};

class TPinnaCoaster : public TMapObjBase {
public:
	TPinnaCoaster(const char* name = "コースター");

	virtual void control();
	virtual void initMapObj();

public:
	/* 0x138 */ MActor* unk138;
	/* 0x13C */ u32 unk13C; // unused, never written by the original code
	/* 0x140 */ JGeometry::TVec3<f32> unk140;
};

class TMerryPole : public TMapObjBase {
public:
	virtual Mtx* getRootJointMtx() const { return (Mtx*)unk138.mMtx; }

	TMerryPole()
	    : TMapObjBase("メリーゴーランド用ポール")
	{
		unk138.identity();
	}

public:
	/* 0x138 */ TMtx34f unk138;
};

#endif
