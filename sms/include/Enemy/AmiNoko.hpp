#ifndef ENEMY_AMI_NOKO_HPP
#define ENEMY_AMI_NOKO_HPP

#include <Enemy/WalkerEnemy.hpp>
#include <Strategic/HitActor.hpp>
#include <Strategic/Strategy.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>

class TAmiNoko;

class TAmiNokoParams : public TWalkerEnemyParams {
public:
	TAmiNokoParams(const char* path)
	    : TWalkerEnemyParams(path)
	    , PARAM_INIT(mSLElecRange, 200.0f)
	    , PARAM_INIT(mSLMtxRotSpeed, 0.05f)
	{
		TParams::load(mPrmPath);
	}

	/* 0x32C */ TParamRT<f32> mSLElecRange;
	/* 0x340 */ TParamRT<f32> mSLMtxRotSpeed;
};

/**
 * @brief Hit actor that rides on the fence side of a TAmiNoko. It follows the
 * owner, 100 units out along the owner's up vector, and makes the owner attack
 * Mario on contact.
 */
class TAmiHit : public THitActor {
public:
	TAmiHit(TAmiNoko* owner, const char* name)
	    : THitActor(name)
	    , mOwner(owner)
	{
		static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
		    ->getChildren()
		    .push_back(this);
		initHitActor(0x10000021, 1, 0x80000000, 120.0f, 240.0f, 120.0f, 240.0f);
		offHitFlag(HIT_FLAG_NO_COLLISION);
	}

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

public:
	/* 0x68 */ TAmiNoko* mOwner;
};

class TAmiNokoManager : public TSmallEnemyManager {
public:
	TAmiNokoManager(const char* name = "アミノコマネージャー");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

class TAmiNoko : public TWalkerEnemy {
public:
	enum PlaneType {
		PLANE_FLOOR = 0,
		PLANE_ROOF  = 1,
		PLANE_WALL  = 2,
	};

	TAmiNoko(const char* name = "アミノコ");

	virtual void load(JSUMemoryInputStream& stream);
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual void init(TLiveManager* manager);
	virtual void calcRootMatrix();
	virtual void bind();
	virtual f32 getGravityY() const;
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual void behaveToWater(THitActor*);
	virtual void setWalkAnm();
	virtual void attackToMario();
	virtual void setMActorAndKeeper();
	virtual bool isHitValid(u32 message);
	virtual bool isCollidMove(THitActor*) { return false; }

	void creepToCurPathNode(f32 speed);

public:
	/* 0x194 */ const TBGCheckData* mPlane;
	/* 0x198 */ int mPlaneType;
	/* 0x19C */ JGeometry::TVec3<f32> mUp;
	/* 0x1A8 */ JGeometry::TVec3<f32> mForward;
	/* 0x1B4 */ JGeometry::TVec3<f32> mPrevUp;
	/* 0x1C0 */ JGeometry::TVec3<f32> mPrevForward;
	/* 0x1CC */ Mtx mBaseMtx;
	/* 0x1FC */ JGeometry::TVec3<f32> mElecPos;
	/* 0x208 */ TAmiHit* mAmiHit;
	/* 0x20C */ TAmiNokoParams* mParams;
	/* 0x210 */ bool mIsFenceAnm;
};
#endif
