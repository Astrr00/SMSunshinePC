#ifndef MOVE_BG_MODEL_GATE
#define MOVE_BG_MODEL_GATE

#include <Strategic/TakeActor.hpp>
#include <M3DUtil/MActor.hpp>

class SampleCtrlModelData;

/**
 * @brief The jelly gates of Delfino Plaza (the "JellyGate" map object): a gate
 * model whose texture is replaced by the THP video frame while it is open and
 * which grabs Mario once he jumps into the portal.
 */
class TModelGate : public TTakeActor {
public:
	TModelGate(const char* name = "<TModelGate>")
	    : TTakeActor(name)
	{
	}

	virtual MtxPtr getTakingMtx() { return nullptr; }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void loadAfter();

	void screenBlur(JDrama::TGraphics* graphics);
	void startOpen();

public:
	// Size 0x118 (console layout)
	/* 0x70 */ u8 mFlags;       // bit 0: gate is open, bit 1: still opening
	/* 0x71 */ u8 mDestination; // gate index: Ricco, Mamma, Monte, Mare, ...
	/* 0x72 */ u16 mCenterJointIdx;
	/* 0x74 */ s16 mCenterAngle; // yaw of the center joint
	/* 0x78 */ MActor* mActor;
	/* 0x7C */ Mtx mInvCenterMtx;
	/* 0xAC */ JGeometry::TVec3<f32> mFrontPos; // 250 units in front of the
	                                            // center joint
	/* 0xB8 */ u8 mTevAnimMode;                 // 1: animate the TEV stages
	/* 0xB9 */ u8 mTevAnimStep;
	/* 0xBA */ u8 mPrevTevAnimStep;
	/* 0xBC */ u16 mTevAnimTimer;
	/* 0xBE */ u16 mTevAnimInterval;
	/* 0xC0 */ SampleCtrlModelData* mSampleCtrlData;
	/* 0xC4 */ u8
	    mState; // 0: waiting for the open animation, 1: open, 2: closing
	/* 0xC5 */ u8 unkC5;
	/* 0xC6 */ u8 unkC6;
	/* 0xC8 */ s16 mOpenDuration;
	/* 0xCA */ s16 mOpenTimer;
	/* 0xCC */ s16 mOpenElapsed;
	/* 0xCE */ s16 unkCE;
	/* 0xD0 */ f32 mOpenRatio;
	/* 0xD4 */ f32 mOpenRatioInc;
	/* 0xD8 */ f32 mOpenRatioDecOpen;
	/* 0xDC */ f32 mOpenRatioDecClose;
	/* 0xE0 */ u8 mBlurMaxAlpha;
	/* 0xE4 */ f32 mBlurAlpha;
	/* 0xE8 */ f32 mBlurAlphaRate;
	/* 0xEC */ f32 mBlurStrength;
	/* 0xF0 */ f32 mBlurNearDist;
	/* 0xF4 */ f32 mBlurFarDist;
	/* 0xF8 */ f32 mHitEffectChance;
	/* 0xFC */ f32 mHitMaxZ;
	/* 0x100 */ f32 mPushRadius;
	/* 0x104 */ f32 mEnterHalfWidth;
	/* 0x108 */ f32 mEnterMinY;
	/* 0x10C */ f32 mEnterMaxY;
	/* 0x110 */ f32 mEnterMinZ;
	/* 0x114 */ f32 mEnterMaxZ;
};

#endif
