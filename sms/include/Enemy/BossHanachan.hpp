#ifndef ENEMY_BOSS_HANACHAN_HPP
#define ENEMY_BOSS_HANACHAN_HPP

#include <Enemy/BossHanachanChangeSaveParams.hpp>
#include <Enemy/BossHanachanParts.hpp>
#include <Enemy/BossHanachanSub.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>

class TLiveActor;

// The Wiggler boss: a head part plus eight body parts that hang off a chain of
// mass points (TSphereLink). Total size 0x1C4.
class TBossHanachan : public TSpineEnemy {
public:
	TBossHanachan(const char*);

	virtual void perform(u32, JDrama::TGraphics*);
	virtual void init(TLiveManager*);
	virtual void bind();
	virtual void moveObject();
	virtual void kill();
	virtual BOOL hasMapCollision() const;

	// BossHanachanMain.cpp
	void removeAllMapCollision();
	void execDamage();
	void goToInitialRecoverGraphNode();
	void execSlip();
	void execWalk(bool);
	f32 getBodyMaxRotateZ() const;
	bool checkFallDecideAndSetup();
	bool isTumbleCompletelyAllBody() const;
	void throwMario_(THitActor*);
	void setRandomWeakBodyIndex();

	// BossHanachanAnm.cpp
	void changeAnmRateAndFrameUpdate_();
	bool isAllBckAlreadyEnd(EnumBossHanachanAnmKind) const;
	bool isFinishedGetUp() const;
	void considerSetAnm(EnumBossHanachanNerveAnm);
	void setAnmTimerWhenDead();
	void setAnmTimerWhenDamage();
	void setAnmTimerWhenSnort();
	void setAnmTimerWhenGetUp();
	void setTumbleAnm(EnumBossHanachanStopMotionBlendOnOff);
	void setHeadAndBodyAnm(EnumBossHanachanAnmKind,
	                       EnumBossHanachanStopMotionBlendOnOff);

	// BossHanachanEffect.cpp
	void emitCamShake_();
	void emitOneTimeSandPillar_(TBossHanachanPartsBody*);
	void emitParticle_();
	static void staticLoadParticle();

public:
	/* 0x150 */ TBossHanachanPartsBody* mBodyParts[8];
	/* 0x170 */ TBossHanachanPartsHead* mHeadPart;
	/* 0x174 */ s32 mWeakBodyIndex;
	/* 0x178 */ TSphereLink* mSphereLink;
	/* 0x17C */ JGeometry::TVec3<f32> mHeadPos;
	/* 0x188 */ JGeometry::TVec3<f32> mLastLinearVelocity;
	/* 0x194 */ f32 mTumbleTargetAngleZ; // +-179, sign of the body-0 tilt
	/* 0x198 */ f32 mTumbleAngularSpeedZ;
	/* 0x19C */ MActor* mSandPillarActor;
	/* 0x1A0 */ JGeometry::TVec3<f32> mSandPillarPos;
	/* 0x1AC */ JGeometry::TVec3<f32> mDeadMarioPos;
	/* 0x1B8 */ s32 mHintBalloonTimer;
	/* 0x1BC */ TBossHanachanCommonSaveParams* mCommonSaveParams;
	/* 0x1C0 */ TBossHanachanChangeSaveParams* mChangeSaveParams;
};

class TBossHanachanManager : public TEnemyManager {
public:
	TBossHanachanManager(const char*);

	virtual void loadAfter();
	virtual void createModelData();
	virtual BOOL hasMapCollision() const;
	virtual void clipEnemies(JDrama::TGraphics*);

public:
	/* 0x54 */ TBossHanachanCommonSaveParams* mCommonSaveParams;
	/* 0x58 */ TBossHanachanChangeSaveParams* mChangeSaveParams[3];
};

DECLARE_NERVE(TNerveSBH_Fall, TLiveActor);
DECLARE_NERVE(TNerveSBH_SleepContinue, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDead, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanSnort, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDamage, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGetUp, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanDown, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanTumble, TLiveActor);
DECLARE_NERVE(TNerveBossHanachanGraphWander, TLiveActor);

#endif
