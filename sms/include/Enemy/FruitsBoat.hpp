#ifndef ENEMY_FRUITS_BOAT_HPP
#define ENEMY_FRUITS_BOAT_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>

class J3DAnmTransformKey;
class J3DFrameCtrl;

class TFruitsBoatParams : public TSpineEnemyParams {
public:
	TFruitsBoatParams(const char* path)
	    : TSpineEnemyParams(path)
	    , PARAM_INIT(mSLMoveSpeed, 4.0f)
	    , PARAM_INIT(mSLRotSpeed, 0.1f)
	    , PARAM_INIT(mSLBckMoveSpeed, 0.2f)
	{
		TParams::load(mPrmPath);
	}

	/* 0xA8 */ TParamRT<f32> mSLMoveSpeed;
	/* 0xBC */ TParamRT<f32> mSLRotSpeed;
	/* 0xD0 */ TParamRT<f32> mSLBckMoveSpeed;
};

class TFruitsBoatManager : public TEnemyManager {
public:
	TFruitsBoatManager(int model_type, const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();

public:
	/* 0x54 */ int mModelType;
};

class TFruitsBoat : public TSpineEnemy {
public:
	TFruitsBoat(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual BOOL receiveMessage(THitActor*, u32);
	virtual Mtx* getRootJointMtx() const;
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void setGroundCollision();
	virtual void moveObject();
	virtual void requestShadow();

	int setBckTrack(const char*);

	// fabricated
	TFruitsBoatParams* getSaveParam2() const
	{
		return (TFruitsBoatParams*)getSaveParam();
	}
	int getModelType() const
	{
		return static_cast<TFruitsBoatManager*>(mManager)->mModelType;
	}

public:
	/* 0x150 */ s16 mDirFlip;
	/* 0x154 */ f32 mShadowRadiusX;
	/* 0x158 */ f32 mShadowRadiusZ;
	/* 0x15C */ J3DAnmTransformKey* mBckTrack;
	/* 0x160 */ J3DFrameCtrl* mBckFrameCtrl;
	/* 0x164 */ JGeometry::TVec3<f32> mTiltAxis;
	/* 0x170 */ f32 mTiltAngle;
	/* 0x174 */ f32 mTiltSpeed;
};

#endif
