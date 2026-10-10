#ifndef ENEMY_ELEC_NOKONOKO_HPP
#define ENEMY_ELEC_NOKONOKO_HPP

#include <Enemy/EnemyAttachment.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <Strategic/Nerve.hpp>

class J3DMaterialTable;
class TElecCarapace;
class TElecNokonoko;

class TElecNokonokoSaveLoadParams : public TWalkerEnemyParams {
public:
	TElecNokonokoSaveLoadParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLReadyTime, 300)
	    , PARAM_INIT(mSLCarapaceGravity, 0.01f)
	    , PARAM_INIT(mSLCarapaceSpeed, 5.0f)
	    , PARAM_INIT(mSLCarapaceTurnSpeed, 2.0f)
	    , PARAM_INIT(mSLCarapaceSpinSpeed, 2.0f)
	    , PARAM_INIT(mSLCarapaceShootRange, 500.0f)
	    , PARAM_INIT(mSLCarapaceFlyDist, 100.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<s32> mSLReadyTime;
	/* 0x340 */ TParamRT<f32> mSLCarapaceGravity;
	/* 0x354 */ TParamRT<f32> mSLCarapaceSpeed;
	/* 0x368 */ TParamRT<f32> mSLCarapaceTurnSpeed;
	/* 0x37C */ TParamRT<f32> mSLCarapaceSpinSpeed;
	/* 0x390 */ TParamRT<f32> mSLCarapaceShootRange;
	/* 0x3A4 */ TParamRT<f32> mSLCarapaceFlyDist;
};

class TElecNokonokoManager : public TSmallEnemyManager {
public:
	TElecNokonokoManager(const char* name = "電気ノコノコマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSmallEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*);
	virtual void initSetEnemies();

public:
	/* 0x60 */ J3DMaterialTable* mMatTable;
};

// The shell of the electric Nokonoko. It is thrown at Mario with shoot() and
// returns to its owner afterwards.
class TElecCarapace : public TEnemyAttachment {
public:
	TElecCarapace(const char* name = "ノコノコ甲羅");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual void kill();
	virtual void loadInit(TSpineEnemy*, const char*);
	virtual void appear();
	virtual void rebirth() { }
	virtual void sendMessage();
	virtual void behaveToHitGround();
	virtual void behaveToHitWall(const TBGCheckData*);
	virtual void setBehavior();
	virtual void recoverScale() { }
	virtual f32 getNowGravity();
	virtual f32 getPhaseShift() const { return unk174 ? 0.0f : 180.0f; }
	virtual void shoot();

	void reflect(THitActor*);

public:
	/* 0x16C */ TElecNokonoko* mOwner;
	/* 0x170 */ THitActor* mReflectedBy;
	/* 0x174 */ u8 unk174;
	/* 0x175 */ u8 unk175;
	/* 0x176 */ u8 unk176;
	/* 0x178 */ f32 unk178;
	/* 0x17C */ f32 unk17C;
	/* 0x180 */ int unk180;
	/* 0x184 */ u8 unk184;
	/* 0x188 */ f32 mSpinAngle;
	/* 0x18C */ JGeometry::TVec3<f32> mReturnSpeed;
	/* 0x198 */ f32 unk198;
};

class TElecNokonoko : public TWalkerEnemy {
public:
	TElecNokonoko(const char* name = "電気ノコノコ");

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void genRandomItem();
	virtual void behaveToWater(THitActor*);
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setMeltAnm();
	virtual void setWaitAnm();
	virtual void setRunAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual void sendAttackMsgToMario();
	virtual void behaveToFindMario();
	virtual bool isResignationAttack();
	virtual void rest();

	// fabricated
	bool hasCarapace() const { return mCarapaceShot == 0 ? true : false; }

	static u8 mReflectSw;

public:
	/* 0x194 */ TElecCarapace* mCarapace;
	/* 0x198 */ int unk198;
	/* 0x19C */ int unk19C;
	/* 0x1A0 */ TElecNokonokoSaveLoadParams* mParams;
	/* 0x1A4 */ int mCarapaceShot; // 0 = still carries the shell, 1 = shell thrown
	/* 0x1A8 */ JGeometry::TVec3<f32> unk1A8;
};

#endif
