#ifndef GC2D_SELECT_SHINE_2_HPP
#define GC2D_SELECT_SHINE_2_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JParticle/JPAEmitterManager.hpp>

class J3DAnmColor;
class J3DDrawBuffer;
class J3DModel;
class J3DModelData;
class JPABaseEmitter;

/**
 * @brief A single shine sprite shown in the shine select menu: a J3D model
 * placed on a circle, bobbing along a quadratic curve and trailing particle
 * emitters.
 */
class TSelectShine {
public:
	TSelectShine(J3DModelData* pModelData, J3DAnmColor* pAnm,
	             JPAEmitterManager* pEmitterMgr, JGeometry::TVec3<f32>& rPos,
	             s16 yaw, u8 kind, f32 time, f32 timeSpeed, f32 amplitude);
	virtual ~TSelectShine() { }
	virtual void move();

	// Pauses the particle emission of all the emitters of this shine.
	void stopEmitters();
	// Resumes the particle emission of all the emitters of this shine.
	void playEmitters();

public:
	/* 0x04 */ J3DModel* mModel;
	/* 0x08 */ J3DAnmColor* mAnm;
	/* 0x0C */ JGeometry::TVec3<f32> mPos;
	/* 0x18 */ JGeometry::TVec3<f32> mOffset;
	/// Set for the currently selected shine, which makes it spin
	/* 0x24 */ bool mSelected;
	/// Progress along the 4 segment bobbing curve, in [0, 4]
	/* 0x28 */ f32 mTime;
	/* 0x2C */ f32 mAmplitude;
	/* 0x30 */ f32 mTimeSpeed;
	/* 0x34 */ s32 mSpinAngle;
	/* 0x38 */ s8 mSpinSpeed;
	/* 0x3A */ s16 mYaw;
	/// Frame of the color animation (alpha fade)
	/* 0x3C */ s16 mFrame;
	/* 0x3E */ u8 unk3E;
	/* 0x40 */ f32 unk40;
	/* 0x44 */ f32 unk44;
	/* 0x48 */ u8 mFrameDown;
	/* 0x49 */ u8 mFrameUp;
	/// 0: three emitters, 1: one emitter, 2: no emitter
	/* 0x4A */ u8 mKind;
	/* 0x4C */ JPAEmitterManager* mEmitterMgr;
	/* 0x50 */ JPABaseEmitter* mEmitters[3];
};

class TSelectShineManager : public JDrama::TViewObj {
public:
	TSelectShineManager(const char*);
	virtual ~TSelectShineManager() { }
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void initData(u8*, u8, u8, JPAEmitterManager*);
	void startClose();
	void startIncrease(int);
	void startDecrease(int);

public:
	/* 0x10 */ TSelectShine* mShines[8];
	/* 0x30 */ u8 unk30[0x20];
	/* 0x50 */ J3DDrawBuffer* mDrawBufferOpa;
	/* 0x54 */ J3DDrawBuffer* mDrawBufferXlu;
	/* 0x58 */ u8 unk58[0x20];
	/* 0x78 */ f32 unk78;
	/* 0x7C */ f32 unk7C;
	/* 0x80 */ u8 unk80[0x8];
	/* 0x88 */ s32 mNumShines;
	/* 0x8C */ s32 mCurrent;
	/* 0x90 */ f32 unk90;
	/* 0x94 */ f32 unk94;
	/* 0x98 */ s32 unk98;
	/// Scroll position of the circle, 0x28 per shine
	/* 0x9C */ s32 mScroll;
	/* 0xA0 */ f32 mScrollSpeed;
	/* 0xA4 */ u8 mIncreasing;
	/* 0xA5 */ u8 mDecreasing;
	/* 0xA6 */ u8 unkA6;
	/* 0xA7 */ u8 mClosing;
	/* 0xA8 */ JGeometry::TVec3<f32> unkA8[8];
	/* 0x108 */ u8 unk108[0x18];

	static JGeometry::TVec3<f32> cCenter;
};

#endif // GC2D_SELECT_SHINE_2_HPP
