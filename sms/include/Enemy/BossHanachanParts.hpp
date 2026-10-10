#ifndef ENEMY_BOSS_HANACHAN_PARTS_HPP
#define ENEMY_BOSS_HANACHAN_PARTS_HPP

#include <JSystem/JGeometry/JGVec3.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/LiveActor.hpp>
#include <dolphin/mtx.h>
#include <dolphin/types.h>

class TBossHanachan;
class TIdxGroupObj;
class TMapCollisionMove;
class TNpcInbetween;

// Indexes sBodyBckIndex / sHeadBckIndex / sHeadBtpIndex / sHeadBtkIndex and
// the bas name table (18 entries, NONE == "no anm yet").
enum EnumBossHanachanAnmKind {
	BOSSHANACHAN_ANM_KIND_WALK             = 0,  // bck hanabody_walk
	BOSSHANACHAN_ANM_KIND_RUN              = 1,  // bck hanabody_run
	BOSSHANACHAN_ANM_KIND_JITABATA         = 2,  // flail on its back
	BOSSHANACHAN_ANM_KIND_PIKUPIKU         = 3,  // twitch on its back
	BOSSHANACHAN_ANM_KIND_DAMAGE           = 4,  // bck hanabody_damage
	BOSSHANACHAN_ANM_KIND_JUMP_REACTION    = 5,  // stomped by Mario
	BOSSHANACHAN_ANM_KIND_HIPDROP_REACTION = 6,  // weak body ground-pounded
	BOSSHANACHAN_ANM_KIND_GETUP_L1         = 7,
	BOSSHANACHAN_ANM_KIND_GETUP_L2         = 8,
	BOSSHANACHAN_ANM_KIND_GETUP_L3         = 9,
	BOSSHANACHAN_ANM_KIND_GETUP_R1         = 10,
	BOSSHANACHAN_ANM_KIND_GETUP_R2         = 11,
	BOSSHANACHAN_ANM_KIND_GETUP_R3         = 12,
	BOSSHANACHAN_ANM_KIND_HIPDROP_LONG     = 13, // other bodies ground-pounded
	BOSSHANACHAN_ANM_KIND_SNORT            = 14, // bck hanabody_start
	BOSSHANACHAN_ANM_KIND_DEAD             = 15,
	BOSSHANACHAN_ANM_KIND_TUMBLE_L         = 16,
	BOSSHANACHAN_ANM_KIND_TUMBLE_R         = 17,
	BOSSHANACHAN_ANM_KIND_NONE             = 18,
};

// Argument of TBossHanachan::considerSetAnm: the nerve asking for an anm.
enum EnumBossHanachanNerveAnm {
	BOSSHANACHAN_NERVE_ANM_TUMBLE = 0,
	BOSSHANACHAN_NERVE_ANM_DOWN   = 1,
	BOSSHANACHAN_NERVE_ANM_GETUP  = 2,
	BOSSHANACHAN_NERVE_ANM_DAMAGE = 3,
	BOSSHANACHAN_NERVE_ANM_SNORT  = 4,
	BOSSHANACHAN_NERVE_ANM_DEAD   = 5,
};

enum EnumBossHanachanStopMotionBlendOnOff {
	BOSSHANACHAN_BLEND_OFF = 0,
	BOSSHANACHAN_BLEND_ON  = 1,
};

// Foot hit actor of a body part (the water hit counter at 0x68 is inherited).
class TFootHitActor : public TWaterHitActor {
public:
	TFootHitActor(const char* name)
	    : TWaterHitActor(name)
	{
	}

	/* 0x6C */ MtxPtr mJointMtx; // "foot_L" / "foot_R" joint
};

class TBossHanachanPartsBase : public TLiveActor {
public:
	TBossHanachanPartsBase(TBossHanachan*, u32, int, const char*);

	virtual const char** getBasNameTable() const;
	virtual bool setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff)
	    = 0;

	void considerSetAnm_(EnumBossHanachanNerveAnm);
	void calcRotateZWhenGetUp_();
	const TLiveActor* getSandActor_() const;
	void copyFrameFromOldAnmToNewAnm_();
	bool isCurBckAlreadyEnd_() const;
	void setDamageFog_(JDrama::TGraphics*);
	void entryCircleShadow_();
	void moveMapCollision_();
	void changeTumbleAnmRate_();
	void initMapCollisionAndHitActor_(TIdxGroupObj*);

public:
	/* 0xF4 */ EnumBossHanachanAnmKind mAnmKind;
	/* 0xF8 */ EnumBossHanachanAnmKind mPrevAnmKind;
	/* 0xFC */ TBossHanachan* mOwner;
	/* 0x100 */ TWaterHitActor* mHitActor;
	/* 0x104 */ TMapCollisionMove* mMapCollision;
	/* 0x108 */ MtxPtr mJointMtx; // "center" joint
	/* 0x10C */ s32 mAnmTimer;
	/* 0x110 */ TNpcInbetween* mInbetween;
};

class TBossHanachanPartsHead : public TBossHanachanPartsBase {
public:
	TBossHanachanPartsHead(TBossHanachan*, const char*);

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual bool setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff);

public:
	/* 0x114 */ MtxPtr mNoseHallMtx[2]; // "L_hall", "R_hall"
};

class TBossHanachanPartsBody : public TBossHanachanPartsBase {
public:
	TBossHanachanPartsBody(TBossHanachan*, const char*);

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual bool setAnm_(EnumBossHanachanAnmKind,
	                     EnumBossHanachanStopMotionBlendOnOff);

	void initFootHitActor_(TIdxGroupObj*);

public:
	/* 0x114 */ s32 mIndex; // compared with TBossHanachan::mWeakBodyIndex
	/* 0x118 */ TFootHitActor* mFootHitActors[2]; // "foot_L", "foot_R"
	/* 0x120 */ f32 mSandTiltRotZ; // target of mRotation.z
	/* 0x124 */ JGeometry::TVec3<f32> mLastPos;
	/* 0x130 */ JGeometry::TVec3<f32> mLastLastPos;
	/* 0x13C */ f32 mLastRotZ;
	/* 0x140 */ f32 mLastLastRotZ;
	/* 0x144 */ f32 mRotZSpeed;
	/* 0x148 */ f32 mLastRotZSpeed;
	/* 0x14C */ MtxPtr mLegMtx[2]; // "leg_L3", "leg_R3"
	/* 0x154 */ JGeometry::TVec3<f32> mJointPos;
};

#endif
