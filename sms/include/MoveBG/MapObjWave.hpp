#ifndef MOVE_BG_MAP_OBJ_WAVE_HPP
#define MOVE_BG_MAP_OBJ_WAVE_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <dolphin/gx/GXStruct.h>

struct ResTIMG;

class TMapObjWave;

extern TMapObjWave* gpMapObjWave;

class TMapObjWave : public JDrama::TViewObj {
public:
	TMapObjWave(const char* name = "波の表現");

	virtual ~TMapObjWave() { }
	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void updateTime();
	void updateHeightAndAlpha();
	void draw();
	void noWave();
	f32 getHeight(f32 x, f32 y, f32 z) const;
	f32 getWaveHeight(f32 x, f32 z) const;
	void initDraw();

	// inlined by draw()
	s32 getAlpha(f32 x, f32 z) const;
	f32 getStaticTexPos0(f32 pos) const;
	f32 getStaticTexPos1(f32 pos) const;
	f32 getMoveTexPos0(f32 pos) const;
	f32 getMoveTexPos1(f32 pos) const;

public:
	/* 0x10 */ f32 mAreaSize;
	/* 0x14 */ f32 mHalfAreaSize;
	/* 0x18 */ f32 mInvHalfAreaSize;
	/* 0x1C */ f32 mCellSize;
	/* 0x20 */ s32 mCellNum;
	/* 0x24 */ f32 mFreqX;
	/* 0x28 */ f32 mFreqZ;
	/* 0x2C */ f32 mDeepAmpX;
	/* 0x30 */ f32 mDeepAmpZ;
	/* 0x34 */ f32 mShallowAmpX;
	/* 0x38 */ f32 mShallowAmpZ;
	/* 0x3C */ f32 mAmpX;
	/* 0x40 */ f32 mAmpZ;
	/* 0x44 */ f32 mExtraAmp;
	/* 0x48 */ f32 mExtraAmpSpeed;
	/* 0x4C */ f32 mAmpDepth;
	/* 0x50 */ f32 mAlphaDepth;
	/* 0x54 */ f32 mAlpha;
	/* 0x58 */ f32 mDeepAlpha;
	/* 0x5C */ f32 mShallowAlpha;
	/* 0x60 */ f32 mTexScrollSpeed;
	/* 0x64 */ f32 mPhaseX;
	/* 0x68 */ f32 mPhaseZ;
	/* 0x6C */ f32 mTexScrollU;
	/* 0x70 */ f32 mTexScrollV;
	/* 0x74 */ f32 mTexScale0;
	/* 0x78 */ f32 mTexScale1;
	/* 0x7C */ GXColorS10 mTevColor0;
	/* 0x84 */ GXColorS10 mTevColor1;
	/* 0x8C */ GXColorS10 mTevColor2;
	/* 0x94 */ ResTIMG* mTexture;
	/* 0x98 */ s16 unk98;
};

#endif
