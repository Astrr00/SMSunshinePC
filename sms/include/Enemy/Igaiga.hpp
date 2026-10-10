#ifndef ENEMY_IGAIGA_HPP
#define ENEMY_IGAIGA_HPP

#include <Enemy/EnemyAttachment.hpp>
#include <Enemy/AreaCylinder.hpp>
#include <Enemy/WalkerEnemy.hpp>
#include <JSystem/JGeometry.hpp>
#include <dolphin/gx/GXStruct.h>

class TAreaCylinderManager;
class TMapEventSink;
class TWaterEmitInfo;

// Shared by TIgaigaManager (/enemy/igaiga.prm) and TGorogoroManager
// (/enemy/gorogoro.prm); the constructor is always inlined into `load`.
class TRollEnemySaveLoadParams : public TWalkerEnemyParams {
public:
	TRollEnemySaveLoadParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLGenerateInterval, 300)
	    , PARAM_INIT(mSLExpandRate, 1.0f)
	    , PARAM_INIT(mSLExpandMax, 1.5f)
	    , PARAM_INIT(mSLBoundVYMax, 15.0f)
	    , PARAM_INIT(mSLGroundOffsetY, 150.0f)
	{
		TParams::load(mPrmPath);
	}

	s32 getSLGenerateInterval() const { return mSLGenerateInterval.get(); }
	f32 getSLExpandRate() const { return mSLExpandRate.get(); }
	f32 getSLExpandMax() const { return mSLExpandMax.get(); }
	f32 getSLBoundVYMax() const { return mSLBoundVYMax.get(); }
	f32 getSLGroundOffsetY() const { return mSLGroundOffsetY.get(); }

	/* 0x32C */ TParamRT<s32> mSLGenerateInterval;
	/* 0x340 */ TParamRT<f32> mSLExpandRate;
	/* 0x354 */ TParamRT<f32> mSLExpandMax;
	/* 0x368 */ TParamRT<f32> mSLBoundVYMax;
	/* 0x37C */ TParamRT<f32> mSLGroundOffsetY;
};

// Base of the two rolling enemies. Size 0x1B4.
class TRollEnemy : public TWalkerEnemy {
public:
	TRollEnemy(const char*);

	virtual void reset();
	virtual void setBehavior();
	virtual void behaveToWater(THitActor*);
	virtual void attackToMario();
	virtual bool isCollidMove(THitActor*);
	virtual void setAfterDeadEffect() { }
	virtual void walkBehavior(int, f32);
	virtual void flagJump();
	virtual bool isReachedToGoalXZ();
	virtual void bound() { }
	virtual bool isRolling() { return false; }
	virtual void rollSE() { }
	virtual void boundSE() { }

	static f32 mBoundVal;
	static f32 mTransYOffset;

public:
	/* 0x194 */ f32 unk194; // roll angle (degrees), advanced in walkBehavior
	/* 0x198 */ f32 unk198;
	/* 0x19C */ f32 unk19C;
	/* 0x1A0 */ f32 unk1A0; // vertical speed used for bound detection
	/* 0x1A4 */ TRollEnemySaveLoadParams* unk1A4;
	/* 0x1A8 */ bool unk1A8; // airborne after flagJump
	/* 0x1AC */ f32 unk1AC;
	/* 0x1B0 */ f32 unk1B0;
};

class TIgaigaPolluteModelManager : public TEnemyPolluteModelManager {
public:
	TIgaigaPolluteModelManager(const char* name)
	    : TEnemyPolluteModelManager(name)
	{
	}

	virtual void init(TLiveActor*);
};

class TIgaigaPolluteModel : public TEnemyPolluteModel {
public:
	TIgaigaPolluteModel(TLiveActor* actor, int index, SDLModelData* model,
	                    const char* name)
	    : TEnemyPolluteModel(actor, index, model, name)
	{
	}

	virtual void setAnm();
};

// Size 0x6C.
class TIgaigaManager : public TSmallEnemyManager {
public:
	TIgaigaManager(const char* name = "イガイガマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void initSetEnemies();

	// fabricated (inlined into TIgaiga::setDeadAnm)
	void requestPolluteModel(JGeometry::TVec3<f32>& pos,
	                         JGeometry::TVec3<f32>& scale)
	{
		unk60->generatePolluteModel(pos, scale);
	}

public:
	/* 0x60 */ TEnemyPolluteModelManager* unk60;
	/* 0x64 */ u32 unk64;
	/* 0x68 */ TWaterEmitInfo* unk68;
};

// Size 0x1EC.
class TIgaiga : public TRollEnemy {
public:
	TIgaiga(const char*);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setWalkAnm();
	virtual void setDeadAnm();
	virtual void setMeltAnm();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32);
	virtual void walkBehavior(int, f32);
	virtual bool isReachedToGoalXZ();
	virtual void bound();
	virtual bool isRolling();
	virtual void rollSE();
	virtual void boundSE();

	void shoot(JGeometry::TVec3<f32>&);

	static f32 mReachNodeDist;

public:
	/* 0x1B4 */ s32 unk1B4; // frame counter, wraps at unk1B8
	/* 0x1B8 */ s32 unk1B8;
	/* 0x1BC */ bool unk1BC;
	/* 0x1C0 */ JGeometry::TVec3<f32> unk1C0; // effect position
	/* 0x1CC */ f32 unk1CC;
	/* 0x1D0 */ s32 unk1D0;
	/* 0x1D4 */ u32 unk1D4;
	/* 0x1D8 */ JGeometry::TVec3<f32> unk1D8; // velocity set by shoot()
	/* 0x1E4 */ f32 unk1E4;
	/* 0x1E8 */ s32 unk1E8; // water hit counter
};

DECLARE_NERVE(TNerveIgaigaRollOnGraph, TLiveActor);
DECLARE_NERVE(TNerveIgaigaWaterHit, TLiveActor);
DECLARE_NERVE(TNerveIgaigaShootFromCannon, TLiveActor);

class TGorogoroPolluteModelManager : public TEnemyPolluteModelManager {
public:
	TGorogoroPolluteModelManager(const char* name)
	    : TEnemyPolluteModelManager(name)
	{
	}

	virtual void init(TLiveActor*);
};

class TGorogoroPolluteModel : public TEnemyPolluteModel {
public:
	TGorogoroPolluteModel(TLiveActor* actor, int index, SDLModelData* model,
	                      const char* name)
	    : TEnemyPolluteModel(actor, index, model, name)
	{
	}

	virtual void setAnm();
};

// Size 0x74.
class TGorogoroManager : public TSmallEnemyManager {
public:
	TGorogoroManager(const char* name = "ゴロゴロマネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
	virtual void initSetEnemies();

	// fabricated (defined inline in igaiga.cpp, inlined into perform)
	BOOL inArea();

	// fabricated (inlined into TGorogoro::bound / TNerveGorogoroDie::execute)
	void requestPolluteModel(JGeometry::TVec3<f32>& pos,
	                         JGeometry::TVec3<f32>& scale)
	{
		unk6C->generatePolluteModel(pos, scale);
	}

public:
	/* 0x60 */ s32 unk60; // generate interval counter
	/* 0x64 */ TMapEventSink* unk64;
	/* 0x68 */ bool unk68;
	/* 0x6C */ TEnemyPolluteModelManager* unk6C;
	/* 0x70 */ TAreaCylinderManager* unk70;
};

// Size 0x1F4. The constructor is inlined into
// TGorogoroManager::createEnemyInstance.
class TGorogoro : public TRollEnemy {
public:
	TGorogoro(const char*);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void kill();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void setMeltAnm();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual void walkBehavior(int, f32);
	virtual void flagJump();
	virtual void bound();
	virtual bool isRolling();
	virtual void rollSE();
	virtual void boundSE();

	void generateByGateKeeper(const JGeometry::TVec3<f32>&,
	                          const JGeometry::TVec3<f32>&);

	// fabricated (inlined into TGorogoroManager::initSetEnemies)
	void setGenerateGraphIdx(int idx) { unk1E8 = idx; }

public:
	/* 0x1B4 */ TPosition3f unk1B4;
	/* 0x1E4 */ u8 unk1E4;
	/* 0x1E8 */ s32 unk1E8; // generate graph node index
	/* 0x1EC */ u8 unk1EC;
	/* 0x1ED */ GXColor unk1ED; // TEV K color, alpha (0x1F0) = remaining HP
};

DECLARE_NERVE(TNerveGorogoroRollOnGraph, TLiveActor);
DECLARE_NERVE(TNerveGorogoroDie, TLiveActor);

#endif
