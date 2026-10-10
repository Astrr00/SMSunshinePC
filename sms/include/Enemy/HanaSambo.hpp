#ifndef HANA_SAMBO_HPP
#define HANA_SAMBO_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <JSystem/JDrama/JDRViewObj.hpp>

class TSamboFlower;
class TSamboLeaf;
class TSamboFlowerCoinUnit;
class TSamboFlowerManager;
class THanaSambo;
class TCoin;
class SDLModel;
class SDLModelData;
class J3DMaterialTable;
class TMBindShadowBody;

// ============= params =============

class TSamboHeadSaveLoadParams : public TWalkerEnemyParams {
public:
	TSamboHeadSaveLoadParams(const char* path);

	/* 0x32C */ TParamRT<f32> mSLAppearDist;
	/* 0x340 */ TParamRT<f32> mSLHideDist;
	/* 0x354 */ TParamRT<f32> mSLMoveDist;
	/* 0x368 */ TParamRT<f32> mSLMoveGravity;
	/* 0x37C */ TParamRT<f32> mSLJumpSp;
	/* 0x390 */ TParamRT<s32> mSLJumpPrepareTime;
	/* 0x3A4 */ TParamRT<f32> mSLHitJumpSpXZ;
	/* 0x3B8 */ TParamRT<f32> mSLHitJumpSpY;
	/* 0x3CC */ TParamRT<f32> mSLHitJumpGravity;
	/* 0x3E0 */ TParamRT<f32> mSLHitJumpSpRateXZ;
	/* 0x3F4 */ TParamRT<f32> mSLHitJumpSpRateY;
	/* 0x408 */ TParamRT<f32> mSLJumpAngY;
};

class THanaSamboSaveLoadParams : public TSmallEnemyParams {
public:
	THanaSamboSaveLoadParams(const char* path);

	/* 0x2D4 */ TParamRT<f32> mSLAttackDist;
	/* 0x2E8 */ TParamRT<s32> mSLAttackInterval;
	/* 0x2FC */ TParamRT<f32> mSLAppearDist;
	/* 0x310 */ TParamRT<f32> mSLHideDist;
	/* 0x324 */ TParamRT<s32> mSLAttackingTime;
	/* 0x338 */ TParamRT<f32> mSLHeadAttackRadius;
	/* 0x34C */ TParamRT<f32> mSLHeadAttackHeight;
	/* 0x360 */ TParamRT<f32> mSLHeadDamageRadius;
	/* 0x374 */ TParamRT<f32> mSLHeadDamageHeight;
};

class TSamboFlowerSaveLoadParams : public TSpineEnemyParams {
public:
	TSamboFlowerSaveLoadParams(const char* path);

	/* 0xA8 */ TParamRT<f32> mSLLeafVelocityXZ;
	/* 0xBC */ TParamRT<f32> mSLLeafVelocityY;
	/* 0xD0 */ TParamRT<f32> mSLLeafGravity;
	/* 0xE4 */ TParamRT<f32> mSLBudDist;
	/* 0xF8 */ TParamRT<s32> mSLBloomTimer;
	/* 0x10C */ TParamRT<f32> mSLCoinCircleR;
	/* 0x120 */ TParamRT<f32> mSLCoinVelocityXZ;
	/* 0x134 */ TParamRT<f32> mSLCoinVelocityY;
	/* 0x148 */ TParamRT<f32> mSLSeedShootRange;
	/* 0x15C */ TParamRT<s32> mSLSeedShootInterval;
	/* 0x170 */ TParamRT<f32> mSLSeedGravity;
	/* 0x184 */ TParamRT<f32> mSLSeedSpeedXZ;
	/* 0x198 */ TParamRT<f32> mSLSeedSpeedY;
};

// ============= sambo head =============

class TSamboHead : public TWalkerEnemy {
public:
	TSamboHead(const char* name = "サンボヘッド");

	virtual void load(JSUMemoryInputStream&);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void genEventCoin();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual void setAfterDeadEffect();

	bool isUseCallBack();
	void setCrashAnm();
	void initFlower();

	static u8 mBodyJntIndex;

public:
	/* 0x194 */ TSamboHeadSaveLoadParams* unk194;
	/* 0x198 */ TSamboFlower* unk198;
	/* 0x19C */ int unk19C;
	/* 0x1A0 */ JGeometry::TVec3<f32> unk1A0;
	/* 0x1AC */ f32 unk1AC;
	/* 0x1B0 */ u8 unk1B0;
};

class TSamboHeadManager : public TSmallEnemyManager {
public:
	TSamboHeadManager(const char* name = "サンボヘッドマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= hana sambo =============

class THanaSamboHead : public THitActor {
public:
	THanaSamboHead(const char* name = "ハナサンボ頭あたり")
	    : THitActor(name)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void kill();
	void checkHit();

public:
	/* 0x68 */ THanaSambo* mOwner;
};

class THanaSambo : public TSmallEnemy {
public:
	THanaSambo(const char* name = "ハナサンボ");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual void moveObject();
	virtual void drawObject(JDrama::TGraphics*);
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*) { }
	virtual void setDeadAnm();
	virtual void setWaitAnm();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32 param_1)
	{
		if (param_1 == HIT_MESSAGE_UNKB)
			return true;
		return false;
	}
	virtual bool isCollidMove(THitActor*) { return false; }

	void createPollen();
	void initFlower();
	void waterDamage();
	void setAttackAnm();

	static u8 mHeadJntIndex;
	static u8 mPollenJntIndex;

public:
	/* 0x194 */ THanaSamboHead* unk194;
	/* 0x198 */ THanaSamboSaveLoadParams* unk198;
	/* 0x19C */ JGeometry::TVec3<f32> unk19C;
	/* 0x1A8 */ TSamboFlower* unk1A8;
	/* 0x1AC */ TMBindShadowBody* unk1AC;
	/* 0x1B0 */ u8 unk1B0;
	/* 0x1B4 */ JGeometry::TVec3<f32> unk1B4[4];
};

class THanaSamboManager : public TSmallEnemyManager {
public:
	THanaSamboManager(const char* name = "ハナサンボマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// ============= sambo flower =============

class TSamboFlower : public TSpineEnemy {
public:
	TSamboFlower(const char* name = "サンボフラワー")
	    : TSpineEnemy(name), unk150(0), unk154(0), unk158(-1), unk15C(-1), unk160(0), unk164(nullptr), unk168(nullptr) {}

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void control() { }
	virtual void moveObject();
	virtual void drawObject(JDrama::TGraphics*);
	virtual void reset();
	virtual void setMActorAndKeeper();

	void hide();
	bool isBloomEnd();
	void bloom();

public:
	/* 0x150 */ u8 unk150;
	/* 0x154 */ int unk154;
	/* 0x158 */ s32 unk158;
	/* 0x15C */ s32 unk15C;
	/* 0x160 */ u8 unk160;
	/* 0x164 */ s32* unk164;
	/* 0x168 */ TCoin* unk168;
	/* 0x16C */ TSamboFlowerSaveLoadParams* unk16C;
};

class TSamboLeaf : public JDrama::TViewObj {
public:
	TSamboLeaf(TSamboFlowerManager* manager, SDLModelData* model_data,
	           const char* name = "サンボリーフ");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void generate(JGeometry::TVec3<f32>&);

public:
	/* 0x10 */ SDLModel* mModel;
	/* 0x14 */ JGeometry::TVec3<f32> mPosition;
	/* 0x20 */ JGeometry::TVec3<f32> mRotation;
	/* 0x2C */ JGeometry::TVec3<f32> mScaling;
	/* 0x38 */ JGeometry::TVec3<f32> mVelocity;
	/* 0x44 */ u8 unk44;
	/* 0x48 */ TSamboFlowerManager* mManager;
};

class TSamboFlowerCoinUnit {
public:
	TSamboFlowerCoinUnit(int);

	void add(TSamboFlower*);
	void checkGenCoin();

public:
	/* 0x0 */ TSamboFlower** mFlowers;
	/* 0x4 */ JGeometry::TVec3<f32> mCoinPos;
	/* 0x10 */ s32 mNumFlowers;
	/* 0x14 */ s32 mMaxFlowers;
	/* 0x18 */ TCoin* mCoin;
	/* 0x1C */ s32 mCoinCount;
};

class TSamboFlowerManager : public TEnemyManager {
public:
	TSamboFlowerManager(const char* name = "サンボフラワーマネージャー")
	    : TEnemyManager(name), mCoinUnits(nullptr), mCoinUnitNum(0), unk5C(0), mLeaves(nullptr) {}

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void dropLeaf(JGeometry::TVec3<f32>&, JGeometry::TVec3<f32>&);

public:
	/* 0x54 */ TSamboFlowerCoinUnit** mCoinUnits;
	/* 0x58 */ s32 mCoinUnitNum;
	/* 0x5C */ u32 unk5C;
	/* 0x60 */ TSamboLeaf** mLeaves;
	/* 0x64 */ J3DMaterialTable* mMatTable;
};

// ============= nerves =============

DECLARE_NERVE(TNerveSamboHeadHide, TLiveActor);
DECLARE_NERVE(TNerveSamboHeadAppear, TLiveActor);
DECLARE_NERVE(TNerveSamboHeadAttack, TLiveActor);
DECLARE_NERVE(TNerveSamboHeadHitWater, TLiveActor);
DECLARE_NERVE(TNerveSamboHeadHitWall, TLiveActor);
DECLARE_NERVE(TNerveSamboHeadRecoverWater, TLiveActor);

DECLARE_NERVE(TNerveHanaSamboDie, TLiveActor);
DECLARE_NERVE(TNerveHanaSamboHide, TLiveActor);
DECLARE_NERVE(TNerveHanaSamboWait, TLiveActor);
DECLARE_NERVE(TNerveHanaSamboAppear, TLiveActor);
DECLARE_NERVE(TNerveHanaSamboAttack, TLiveActor);
DECLARE_NERVE(TNerveHanaSamboFreeze, TLiveActor);

#endif
