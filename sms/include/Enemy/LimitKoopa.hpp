#ifndef ENEMY_LIMIT_KOOPA_HPP
#define ENEMY_LIMIT_KOOPA_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Enemy/DirectionCalc.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Strategy.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>

class TLimitKoopa;

// ============= params =============

class TLimitKoopaParams : public TSpineEnemyParams {
public:
	TLimitKoopaParams(const char* path);

	/* 0xA8 */ TParamRT<f32> rotationSpeed;
	/* 0xBC */ TParamRT<f32> bodyScale;
	/* 0xD0 */ TParamRT<f32> hipDropInitialSpeedY;
	/* 0xE4 */ TParamRT<f32> hipDropGravityY;
	/* 0xF8 */ TParamRT<f32> turnSpeed;
	/* 0x10C */ TParamRT<f32> turnAnim;
	/* 0x120 */ TParamRT<f32> waitStep;
	/* 0x134 */ TParamRT<f32> attackRadius;
	/* 0x148 */ TParamRT<f32> attackHeight;
	/* 0x15C */ TParamRT<f32> focusRange;
	/* 0x170 */ TParamRT<f32> waitRange;
	/* 0x184 */ TParamRT<f32> fireSpeed;
	/* 0x198 */ TParamRT<f32> tumbleSpeed;
	/* 0x1AC */ TParamRT<f32> waitSpeed;
	/* 0x1C0 */ TParamRT<f32> staggerSpeed;
	/* 0x1D4 */ TParamRT<f32> downSpeed;
	/* 0x1E8 */ TParamRT<f32> tumbleWeight;
	/* 0x1FC */ TParamRT<f32> flameScale;
	/* 0x210 */ TParamRT<s32> flameCount;
	/* 0x224 */ TParamRT<s32> flameFocusStartStep;
	/* 0x238 */ TParamRT<s32> flameFocusEndStep;
	/* 0x24C */ TParamRT<f32> flameRadius;
	/* 0x260 */ TParamRT<f32> flameHeight;
	/* 0x274 */ TParamRT<f32> headRadius;
	/* 0x288 */ TParamRT<f32> waterhitSpeed;
	/* 0x29C */ TParamRT<f32> flameOverStart;
	/* 0x2B0 */ TParamRT<f32> flameNeckRange;
	/* 0x2C4 */ TParamRT<f32> flameNeckDownRate;
	/* 0x2D8 */ TParamRT<f32> marioEstimationFire;
	/* 0x2EC */ TParamRT<f32> marioEstimationWait;
};

// ============= koopa parts =============

class TLimitKoopaParts : public TLiveActor {
public:
	TLimitKoopaParts(const char* name, u32 actorType, TLimitKoopa* koopa,
	                 f32 radius)
	    : TLiveActor(name)
	    , mOwner(koopa)
	{
		TIdxGroupObj* group = static_cast<TIdxGroupObj*>(
		    JDrama::TNameRefGen::search("敵グループ"));
		group->getChildren().push_back(this);
		initHitActor(actorType, 5, 0x80000000, radius, radius, radius, radius);
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void attack_(THitActor*) = 0;

	void set(const JGeometry::TVec3<f32>& pos, f32 radius, f32 height)
	{
		mPosition.set(pos);
		offHitFlag(HIT_FLAG_CANNOT_ATTACK);
		offHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		offHitFlag(HIT_FLAG_NO_COLLISION);
		mAttackRadius = radius;
		mAttackHeight = height;
		mDamageRadius = radius;
		mDamageHeight = height;
		calcEntryRadius();
	}

	void remove()
	{
		onHitFlag(HIT_FLAG_CANNOT_ATTACK);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		onHitFlag(HIT_FLAG_NO_COLLISION);
	}

public:
	/* 0xF4 */ TLimitKoopa* mOwner;
};

class TLimitKoopaBody : public TLimitKoopaParts {
public:
	TLimitKoopaBody(TLimitKoopa* koopa)
	    : TLimitKoopaParts("クッパの体", 0x08000033, koopa, 100.0f)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TLimitKoopaHead : public TLimitKoopaParts {
public:
	TLimitKoopaHead(TLimitKoopa* koopa)
	    : TLimitKoopaParts("クッパの頭", 0x08000031, koopa, 100.0f)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TLimitKoopaHand : public TLimitKoopaParts {
public:
	TLimitKoopaHand(TLimitKoopa* koopa)
	    : TLimitKoopaParts("クッパの手", 0x08000032, koopa, 100.0f)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TLimitKoopaFlame : public TLimitKoopaParts {
public:
	TLimitKoopaFlame(TLimitKoopa* koopa)
	    : TLimitKoopaParts("クッパの吐く炎", 0x08000030, koopa, 100.0f)
	{
	}

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

// ============= koopa =============

class TLimitKoopa : public TSpineEnemy {
public:
	TLimitKoopa(const char* name = "クッパ");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual f32 getGravityY() const;
	virtual void reset();

	TLimitKoopaParams* getParam() const;
	void setUpHitActors();
	void startHipDrop();
	void changeAnm(int bck, f32 rate);
	bool isBreathing() const;

public:
	/* 0x150 */ s32 unk150;
	/* 0x154 */ s32 mHipDropTimer;
	/* 0x158 */ s32 unk158;
	/* 0x15C */ JGeometry::TVec3<f32> mAcceleration;
	/* 0x168 */ u8 mLanded;
	/* 0x16C */ TDirectionCalc mDirection;
	/* 0x170 */ f32 unk170;
	/* 0x174 */ u32 unk174;
	/* 0x178 */ TLimitKoopaFlame* mFlames[10];
	/* 0x1A0 */ TLimitKoopaHand* mHands[2];
	/* 0x1A8 */ TLimitKoopaHead* mHead;
	/* 0x1AC */ TLimitKoopaBody* mBody;
	/* 0x1B0 */ s32 mHeadJntIdx;
	/* 0x1B4 */ s32 mNeckJntIdx;
	/* 0x1B8 */ s32 mAgoJntIdx;
	/* 0x1BC */ u8 unk1BC[0xC];
};

class TLimitKoopaManager : public TEnemyManager {
public:
	TLimitKoopaManager(const char* name = "クッパマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

#endif
