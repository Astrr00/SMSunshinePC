#ifndef ENEMY_BOSS_TELESA_HPP
#define ENEMY_BOSS_TELESA_HPP

#include <JSystem/JGeometry.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/SmallEnemy.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <MoveBG/MapObjSirena.hpp>
#include <Strategic/Nerve.hpp>

class TBossTelesa;
class TSharedParts;
class TTelesaManager;

class TBossTelesaSaveLoadParams : public TSpineEnemyParams {
public:
	TBossTelesaSaveLoadParams(const char* path);

	/* 0x0A8 */ TParamRT<s32> mSLDamageRadius;
	/* 0x0BC */ TParamRT<s32> mSLDamageHeight;
	/* 0x0D0 */ TParamRT<s32> mSLAttackRadius;
	/* 0x0E4 */ TParamRT<s32> mSLAttackHeight;
	/* 0x0F8 */ TParamRT<s32> mSLGenAttackerTime;
	/* 0x10C */ TParamRT<s32> mSLGenBubbleTime;
	/* 0x120 */ TParamRT<f32> mSLHitAngle;
	/* 0x134 */ TParamRT<s32> mSLNumGenBubble;
	/* 0x148 */ TParamRT<f32> mSL1stBubbleSp;
	/* 0x15C */ TParamRT<f32> mSLHideAreaRadius;
	/* 0x170 */ TParamRT<s32> mSLSlotItemNum;
	/* 0x184 */ TParamRT<s32> mSLSlotFruitNum;
	/* 0x198 */ TParamRT<f32> mSLSlotFirstHitCollectRate;
	/* 0x1AC */ TParamRT<f32> mSLSlotHitCollectRate;
	/* 0x1C0 */ TParamRT<f32> mSLTransYOffset;
	/* 0x1D4 */ TParamRT<s32> mSLStopSlotTime0;
	/* 0x1E8 */ TParamRT<s32> mSLStopSlotTime1;
	/* 0x1FC */ TParamRT<s32> mSLStopSlotTime2;
	/* 0x210 */ TParamRT<s32> mSLSpicyTime;
};

class TBossTelesaManager : public TEnemyManager {
public:
	TBossTelesaManager(const char* name = "ボステレサマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32, JDrama::TGraphics*);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
};

// Collision actors owned by TBossTelesa. Their constructors are inlined into
// TBossTelesa::init in the original binary.

// Boss body ("ボステレサ体コリジョン"). Size 0x70.
class TBossTelesaBody : public THitActor {
public:
	TBossTelesaBody(const char* name = "ボステレサ体コリジョン")
	    : THitActor(name)
	    , unk6C(0)
	{
	}

	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TBossTelesa* mOwner; // not initialized by the ctor
	/* 0x6C */ u8 unk6C;
};

// Boss tongue ("ボステレサ舌コリジョン"). Size 0x6C.
class TBossTelesaTongue : public THitActor {
public:
	TBossTelesaTongue(const char* name = "ボステレサ舌コリジョン")
	    : THitActor(name)
	{
	}

	virtual BOOL receiveMessage(THitActor*, u32);

public:
	/* 0x68 */ TBossTelesa* mOwner; // not initialized by the ctor
};

// Kills small enemies that touch it ("ボステレサ雑魚敵死コリジョン").
// Size 0x70.
class TBossTelesaKillSmallEnemy : public THitActor {
public:
	TBossTelesaKillSmallEnemy(const char* name = "ボステレサ雑魚敵死コリジョン")
	    : THitActor(name)
	    , mOwner(nullptr)
	    , unk6C(0)
	{
	}

	void checkHit();

public:
	/* 0x68 */ TBossTelesa* mOwner;
	/* 0x6C */ u8 unk6C;
};

// Slot machine map object (actor type 0x400001A6). Size 0x1F0.
class TTelesaSlot : public TSlotDrum {
public:
	TTelesaSlot(const char* name = "btelesaSlot")
	    : TSlotDrum(name)
	    , unk19B(false)
	    , unk19C(true)
	    , unk1A4(2)
	    , unk1E0(false)
	{
		char trash[16]; // unused stack slot (present in the retail frame)
		unk1A8[0] = false;
		unk1A8[1] = false;
		unk1A8[2] = false;
	}

	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void initMapObj();
	virtual u32 touchWater(THitActor*);
	virtual void initNeonMatColor() { }

	int getResultFromAng(f32);
	int getForcastResult(int);
	int getSlotResult();
	bool isRollDrum();
	void forceStopSlot(int);
	void moveStart();
	void randomReset();

public:
	/* 0x198 */ bool unk198[3]; // per drum: still rolling (not ctor-initialized)
	/* 0x19B */ bool unk19B;
	/* 0x19C */ bool unk19C;
	/* 0x1A0 */ TBossTelesa* mBoss;
	/* 0x1A4 */ s32 unk1A4; // forced result
	/* 0x1A8 */ bool unk1A8[3]; // per drum: stop requested
	/* 0x1AC */ JGeometry::TVec3<f32> unk1AC[4]; // constructed, not used in this unit
	/* 0x1DC */ TMapCollisionMove* mMapCollisionMove;
	/* 0x1E0 */ u8 unk1E0;
	/* 0x1E4 */ f32 unk1E4[3]; // written by TBossTelesa::rouletteStart
};

// Boss Telesa (Hotel Delfino). Size 0x38C.
class TBossTelesa : public TSpineEnemy {
public:
	TBossTelesa(const char* name = "ボステレサ");

	virtual void loadAfter();
	virtual void perform(u32, JDrama::TGraphics*);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	void forceHide();
	void forceAllItemKill();
	void generateSlotItem();
	void rouletteStart();
	void genAttacker();
	void flashItem(int);
	bool slotFall();
	bool rouletteFall();
	void damageRecover();
	void setSpicy(TLiveActor*);
	void checkHitObject(THitActor*);

	static f32 mEnemyGenRate;
	static f32 mItemGenRate;
	static u8 mNormalAlpha;
	static f32 mBaseHoseiPosY;
	static f32 mRouletteUpRate;
	static s32 mTelesaGenerateInterval;
	static f32 mCameraMoveLimit;
	static f32 mCameraMoveSp;

public:
	/* 0x150 */ u8 mGenTelesa; // genAttacker: spawn a Telesa instead of bubbles
	/* 0x154 */ u32 unk154;
	/* 0x158 */ u32 unk158;
	/* 0x15C */ TBossTelesaSaveLoadParams* mBossParams;
	// 0x160..0x168: state of the repeated inline "start BCK" helper
	/* 0x160 */ s32 mCurBck;
	/* 0x164 */ s32 mPrevBck;
	/* 0x168 */ f32 mBckBlendRatio;
	/* 0x16C */ TBossTelesaBody* mBody;
	/* 0x170 */ TBossTelesaTongue* mTongue;
	/* 0x174 */ TBossTelesaKillSmallEnemy* mKillSmallEnemy;
	/* 0x178 */ TRoulette* mRoulettes[3]; // actor type 0x4000019A
	/* 0x184 */ TTelesaSlot* mSlot;
	/* 0x188 */ TSharedParts* mSlotFrame;
	/* 0x18C */ u8 unk18C;
	/* 0x190 */ JGeometry::TVec3<f32> unk190;
	/* 0x19C */ JGeometry::TVec3<f32> mRoulettePos;
	/* 0x1A8 */ s32 mSlotResult;
	/* 0x1AC */ TMapObjBase* mItems[50];
	/* 0x274 */ s32 mItemNum;
	/* 0x278 */ Mtx mTakingMtx;
	/* 0x2A8 */ TMapObjBase* mFruits[20];
	/* 0x2F8 */ TMapObjBase* mChilis[10];
	/* 0x320 */ TMapObjBase* mCoins[10];
	/* 0x348 */ GXColor mKColor1;
	/* 0x34C */ GXColor mKColor0; // alpha (+0x34F) fades the body
	/* 0x350 */ bool mIsSpicy;
	/* 0x354 */ TTelesaManager* mTelesaManager;
	/* 0x358 */ s16 mMarioHP;
	/* 0x35A */ u8 unk35A;
	/* 0x35B */ u8 unk35B;
	/* 0x35C */ s32 mHitCount;
	/* 0x360 */ f32 mCameraMove;
	/* 0x364 */ f32 mHoseiPosY;
	/* 0x368 */ s32 mSlotTimer;
	/* 0x36C */ s32 mSpicyTimer;
	/* 0x370 */ u8 unk370;
	/* 0x374 */ JGeometry::TVec3<f32> mHitPos;
	/* 0x380 */ s32 mHitParticleId;
	/* 0x384 */ u8 mBgmStarted;
	/* 0x388 */ s32 mDieTimer;
};

class TBubbleSaveLoadParams : public TWalkerEnemyParams {
public:
	// Inlined into TBubbleManager::load in the original binary.
	TBubbleSaveLoadParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLLiveTime, 200)
	    , PARAM_INIT(mSLNumDivision, 5)
	    , PARAM_INIT(mSLMaxScale, 1.5f)
	    , PARAM_INIT(mSLAddPosBase, 50.0f)
	    , PARAM_INIT(mSLRateExpand, 1.001f)
	    , PARAM_INIT(mSLDeadHeight, 300.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<s32> mSLLiveTime;
	/* 0x340 */ TParamRT<s32> mSLNumDivision;
	/* 0x354 */ TParamRT<f32> mSLMaxScale;
	/* 0x368 */ TParamRT<f32> mSLAddPosBase;
	/* 0x37C */ TParamRT<f32> mSLRateExpand;
	/* 0x390 */ TParamRT<f32> mSLDeadHeight;
};

class TBubbleManager : public TSmallEnemyManager {
public:
	TBubbleManager(const char* name = "バブルマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// Bubble spit by the Boss Telesa. Size 0x1D4.
class TBubble : public TWalkerEnemy {
public:
	// Inlined into TBubbleManager::createEnemyInstance in the original binary.
	TBubble(const char* name = "バブル")
	    : TWalkerEnemy(name)
	    , mAppended(nullptr)
	    , unk1CC(0.0f)
	    , unk1D0(false)
	    , unk1D1(false)
	{
	}

	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void kill();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void attackToMario();
	virtual void setAfterDeadEffect() { }

	void appendEnemy();
	void split();

public:
	/* 0x194 */ TBubbleSaveLoadParams* mBubbleParams;
	/* 0x198 */ TSpineEnemy* mAppended; // enemy carried inside the bubble
	/* 0x19C */ TPosition3f unk19C; // type chosen to match the ctor call
	/* 0x1CC */ f32 unk1CC; // height above the ground
	/* 0x1D0 */ bool unk1D0; // split-off child
	/* 0x1D1 */ bool unk1D1;
	/* 0x1D2 */ bool unk1D2;
};

DECLARE_NERVE(TNerveBossTelesaFallDemo, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaHideWait, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaFreeze, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaPrepareSlot, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaHide, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaSpitSlotItem, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaSlotStart, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaAppear, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaSpit, TLiveActor);
DECLARE_NERVE(TNerveBossTelesaDie, TLiveActor);

DECLARE_NERVE(TNerveBubbleSplit, TLiveActor);
DECLARE_NERVE(TNerveBubbleLive, TLiveActor);

#endif
