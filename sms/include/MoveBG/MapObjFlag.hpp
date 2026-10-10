#ifndef MOVE_BG_MAP_OBJ_FLAG_HPP
#define MOVE_BG_MAP_OBJ_FLAG_HPP

#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JGeometry.hpp>
#include <Strategic/HitActor.hpp>

struct ResTIMG;

class TMapObjFlagManager;

extern TMapObjFlagManager* gpMapObjFlagManager;

class TMapObjFlag : public THitActor {
public:
	TMapObjFlag(const char* name = "旗");

	virtual ~TMapObjFlag() { }
	virtual void load(JSUMemoryInputStream&);
	virtual void updateVertex();

	void init(const char*);
	void draw();

public:
	/// Flutter speed of the cloth wave, set per map by TMapObjFlagManager::load
	static f32 mFlutterSpeed;

public:
	/* 0x68 */ f32 mWidth;
	/* 0x6C */ f32 mHeight;
	/* 0x70 */ s32 mVtxNumX;
	/* 0x74 */ s32 mVtxNumY;
	/* 0x78 */ JGeometry::TVec3<f32>** mVertices;
	/* 0x7C */ f32 mWaveFreqX;
	/* 0x80 */ f32 mWaveFreqY;
	/* 0x84 */ f32 mWaveAmp;
	/* 0x88 */ f32 mWavePhase;
	/* 0x8C */ TMtx34f mModelMtx;
	/* 0xBC */ s32 mVtxStep;
};

class TMapObjFlagManager : public JDrama::TViewObj {
public:
	class TMapObjFlagInfo {
	public:
		TMapObjFlagInfo()
		    : mFlagNum(0)
		    , mTexture(nullptr)
		{
		}

	public:
		/* 0x0 */ s32 mFlagNum;
		/* 0x4 */ TMapObjFlag* mFlags[20];
		/* 0x54 */ ResTIMG* mTexture;
	};

	TMapObjFlagManager(const char* name = "旗管理");

	virtual ~TMapObjFlagManager() { }
	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void registerObj(TMapObjFlag*, const char*);
	void initDraw();

public:
	/* 0x10 */ TMapObjFlagInfo mInfos[15];
};

#endif
