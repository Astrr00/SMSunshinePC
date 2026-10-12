#ifndef KOOPA_HPP
#define KOOPA_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Spine.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JGeometry/JGMatrix34.hpp>
#include <JSystem/JDrama/JDRGraphics.hpp>
#include <MarioUtil/RandomUtil.hpp>

class TBathtub;
class J3DModel;
class J3DJoint;

class TKoopa;

// ============= params =============

class TKoopaSaveLoadParams : public TSpineEnemyParams {
public:
	TKoopaSaveLoadParams(const char* path)
	    : TSpineEnemyParams(path)
	    , PARAM_INIT(turnSpeed, 1.6f)
	    , PARAM_INIT(turnAnim, 3.7f)
	    , PARAM_INIT(waitStep, 600.0f)
	    , PARAM_INIT(downStep, 1000.0f)
	    , PARAM_INIT(attackRadius, 800.0f)
	    , PARAM_INIT(attackHeight, 1000.0f)
	    , PARAM_INIT(focusRange, 2.0f)
	    , PARAM_INIT(waitRange, 12.0f)
	    , PARAM_INIT(fireSpeed, 3.5f)
	    , PARAM_INIT(tumbleWeight, 8.3f)
	    , PARAM_INIT(tumbleSpeed, 2.0f)
	    , PARAM_INIT(tumbleStartFrame, 95.0f)
	    , PARAM_INIT(tumbleEndFrame, 160.0f)
	    , PARAM_INIT(waitSpeed, 2.0f)
	    , PARAM_INIT(staggerSpeed, 2.0f)
	    , PARAM_INIT(downSpeed, 2.0f)
	    , PARAM_INIT(flameVelocity, 35.0f)
	    , PARAM_INIT(flameScale, 1.0f)
	    , PARAM_INIT(flameCount, 500)
	    , PARAM_INIT(flameFocusStartStep, 25)
	    , PARAM_INIT(flameFocusEndStep, 500)
	    , PARAM_INIT(flameRadius, 300.0f)
	    , PARAM_INIT(flameHeight, 1000.0f)
	    , PARAM_INIT(headRadius, 400.0f)
	    , PARAM_INIT(waterhitSpeed, 2.0f)
	    , PARAM_INIT(flameOverStart, 1.0f)
	    , PARAM_INIT(flameNeckRange, 17.0f)
	    , PARAM_INIT(flameNeckDownRate, 0.3f)
	    , PARAM_INIT(flameJump, 80.0f)
	    , PARAM_INIT(fallSpeed, 2.0f)
	    , PARAM_INIT(marioEstimationFire, 20.0f)
	    , PARAM_INIT(marioEstimationWait, 10.0f)
	{
		TParams::load(mPrmPath);
	}

	/* 0xA8 */ TParamRT<f32> turnSpeed;
	/* 0xBC */ TParamRT<f32> turnAnim;
	/* 0xD0 */ TParamRT<f32> waitStep;
	/* 0xE4 */ TParamRT<f32> downStep;
	/* 0xF8 */ TParamRT<f32> attackRadius;
	/* 0x10C */ TParamRT<f32> attackHeight;
	/* 0x120 */ TParamRT<f32> focusRange;
	/* 0x134 */ TParamRT<f32> waitRange;
	/* 0x148 */ TParamRT<f32> fireSpeed;
	/* 0x15C */ TParamRT<f32> tumbleWeight;
	/* 0x170 */ TParamRT<f32> tumbleSpeed;
	/* 0x184 */ TParamRT<f32> tumbleStartFrame;
	/* 0x198 */ TParamRT<f32> tumbleEndFrame;
	/* 0x1AC */ TParamRT<f32> waitSpeed;
	/* 0x1C0 */ TParamRT<f32> staggerSpeed;
	/* 0x1D4 */ TParamRT<f32> downSpeed;
	/* 0x1E8 */ TParamRT<f32> flameVelocity;
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
	/* 0x2D8 */ TParamRT<f32> flameJump;
	/* 0x2EC */ TParamRT<f32> fallSpeed;
	/* 0x300 */ TParamRT<f32> marioEstimationFire;
	/* 0x314 */ TParamRT<f32> marioEstimationWait;
};

// ============= koopa parts =============

class TKoopaParts : public THitActor {
public:
	TKoopaParts(const char* name, u32 actorType, TKoopa* koopa, f32 radius);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void control() { }
	virtual void attack_(THitActor*) = 0;

	void remove();
	void set(const JGeometry::TVec3<f32>& pos, f32 radius, f32 height);

public:
	/* 0x68 */ TKoopa* mOwner;
};

class TKoopaBody : public TKoopaParts {
public:
	TKoopaBody(TKoopa* koopa);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TKoopaHead : public TKoopaParts {
public:
	TKoopaHead(TKoopa* koopa);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TKoopaHand : public TKoopaParts {
public:
	TKoopaHand(TKoopa* koopa);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void attack_(THitActor*);
};

class TKoopaFlame : public TKoopaParts {
public:
	TKoopaFlame(TKoopa* koopa);

	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void control();
	virtual void attack_(THitActor*);

	void fire(const JGeometry::TVec3<f32>&, const JGeometry::TVec3<f32>&, f32,
	          f32, f32, f32);
	void resetFlame();
	f32 getLength() const;
	bool isAlive() const;

public:
	/* 0x6C */ JGeometry::TVec3<f32> mBase;
	/* 0x78 */ JGeometry::TVec3<f32> mDirection;
	/* 0x84 */ f32 mSpeed;
	/* 0x88 */ f32 mMaxLength;
	/* 0x8C */ f32 mCurLength;
	/* 0x90 */ f32 mRadius;
	/* 0x94 */ f32 mHeight;
};

// ============= koopa =============

class TKoopa : public TSpineEnemy {
public:
	TKoopa(const char* name = "クッパ");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void updateAnmSound();
	virtual const char** getBasNameTable() const;
	virtual void reset();

	TKoopaSaveLoadParams* getParam() const;
	void changeAnm(int bck, int btp, f32 rate);
	void setUpHitActors();
	void fall();
	void stagger(bool force);
	bool getShowered();
	void getDown();
	bool allowsLaunch() const;
	bool effectsTumble() const;
	bool isFlaming() const;
	bool isBreathing() const;
	f32 getNeckFocus() const;
	f32 getFlameDirRate() const;
	f32 getFlameDirDegree() const;
	f32 getTargetDir(const JGeometry::TVec3<f32>&) const;
	MtxPtr getHeadMtx() const;
	int getAnmIndex() const;
	f32 getAnmFrame() const;
	f32 getAnmFrameNext() const;
	bool endsAnm() const;
	bool passesAnmFrame(f32) const;
	void laugh();
	void setIgnoreMario(bool);
	bool ignoresMario() const;
	void resetFlame_();
	void breathFlame();

public:
	/* 0x150 */ f32 mTargetDir;
	/* 0x154 */ u8 mIgnoreMario;
	/* 0x155 */ u8 unk155;
	/* 0x158 */ JGeometry::TVec3<f32> mSoundPos;
	/* 0x164 */ TKoopaFlame* mFlames[10];
	/* 0x18C */ TKoopaHand* mHands[2];
	/* 0x194 */ TKoopaHead* mHead;
	/* 0x198 */ TKoopaBody* mBody;
	/* 0x19C */ s32 unk19C;
	/* 0x1A0 */ s32 mHeadJntIdx;
	/* 0x1A4 */ s32 mNeckJntIdx;
	/* 0x1A8 */ s32 mAgoJntIdx;
	/* 0x1AC */ char unk1AC[0xC];
	/* 0x1B8 */ f32 unk1B8;
};

class TKoopaManager : public TEnemyManager {
public:
	TKoopaManager(const char* name = "クッパマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

#endif
