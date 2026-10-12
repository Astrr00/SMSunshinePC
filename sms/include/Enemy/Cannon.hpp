#ifndef ENEMY_CANNON_HPP
#define ENEMY_CANNON_HPP

#include <Enemy/SmallEnemy.hpp>
#include <Strategic/SharedParts.hpp>
#include <Strategic/Nerve.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MAnmSound.hpp>
#include <Strategic/Spine.hpp>

class TCannon;
class TChorobei;
class TCannonDom;
class SDLModelData;
class TMapCollisionMove;
class TMapObjBase;

class TCannonSaveLoadParams : public TSmallEnemyParams {
public:
	TCannonSaveLoadParams(const char*);

	/* 0x2D4 */ TParamRT<f32> mSLHideDist;
	/* 0x2E8 */ TParamRT<f32> mSLBombDist;
	/* 0x2FC */ TParamRT<f32> mSLKillerDist;
	/* 0x310 */ TParamRT<s32> mSLBombInterval;
	/* 0x324 */ TParamRT<s32> mSLKillerInterval;
	/* 0x338 */ TParamRT<s32> mSLShootInterval;
	/* 0x34C */ TParamRT<f32> mSLChorobeiAttackRadius;
	/* 0x360 */ TParamRT<f32> mSLChorobeiAttackHeight;
	/* 0x374 */ TParamRT<f32> mSLChorobeiDamageRadius;
	/* 0x388 */ TParamRT<f32> mSLChorobeiDamageHeight;
	/* 0x39C */ TParamRT<f32> mSLKillerTransYOffset;
	/* 0x3B0 */ TParamRT<f32> mSLBombHeiGenerateRate;
	/* 0x3C4 */ TParamRT<f32> mSLThrowXZSpeed;
};

class TCannonManager : public TSmallEnemyManager {
public:
	TCannonManager(const char* name = "砲台マネージャー");

	virtual void load(JSUMemoryInputStream&);
	virtual TSpineEnemy* createEnemyInstance();
	virtual void clipEnemies(JDrama::TGraphics*) { }
};

// A part of the cannon barrel; one of these exists per barrel joint.
class TCannonDom : public TSharedParts {
public:
	TCannonDom(TLiveActor*, int, SDLModelData*, u32, const char*);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);

	void setBckAnm(int index);

public:
	/* 0x1C */ MAnmSound* mAnmSound;
	/* 0x20 */ const char* mAnmSoundPath;
	/* 0x24 */ bool unk24;
	/* 0x28 */ f32 unk28;
	/* 0x2C */ f32 unk2C;
	/* 0x30 */ f32 unk30;
};

// The little guy operating the cannon.
class TChorobei : public THitActor {
public:
	TChorobei(TCannon*, int, const char*);

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);

	void checkHit();
	void setBckAnm(int index);

	bool checkHideEnd()
	{
		if (mSharedParts->getMActor()->curAnmEndsNext()
		    && mSharedParts->getMActor()->checkCurBckFromIndex(15)) {
			unk70 = 1.0f;
			return true;
		}
		return false;
	}

public:
	/* 0x68 */ TCannon* mCannon;
	/* 0x6C */ TSharedParts* mSharedParts;
	/* 0x70 */ f32 unk70;
	/* 0x74 */ MAnmSound* mAnmSound;
	/* 0x78 */ const char* mAnmSoundPath;
	/* 0x7C */ f32 unk7C;
};

class TCannon : public TSmallEnemy {
public:
	TCannon(const char* name = "砲台");

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual MtxPtr getTakingMtx();
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void moveObject();
	virtual const char** getBasNameTable() const;
	virtual void reset();
	virtual bool isCollidMove(THitActor*) { return false; }
	virtual BOOL isInhibitedForceMove() { return TRUE; }
	// sic: the original misspells isHitValid, so this is a new virtual that
	// overrides nothing
	virtual bool isHitVallid(u32) { return false; }

	void startChorobeiShout();
	bool isObject();
	void setKillerGoalPoint();
	void killerShoot();
	void bombShoot();
	void bombSet();

	static u8 mChorobeiJntIdx;
	static u8 mChorobeiHandJntIdx;
	static f32 mVelocityRate;
	static f32 mSearchRate;

public:
	/* 0x194 */ JGeometry::TVec3<f32> unk194;
	/* 0x1A0 */ TSmallEnemy* unk1A0;
	/* 0x1A4 */ TSpineEnemy* unk1A4;
	/* 0x1A8 */ TChorobei* mChorobei;
	/* 0x1AC */ TCannonDom* mDoms[3];
	/* 0x1B8 */ TCannonDom* unk1B8;
	/* 0x1BC */ TSharedParts* mHodai;
	/* 0x1C0 */ TMapCollisionMove* mDomCollisions[3];
	/* 0x1CC */ u8 unk1CC[0x14];
	/* 0x1E0 */ MtxPtr unk1E0;
	/* 0x1E4 */ TPosition3f mTakingMtx;
	/* 0x214 */ int mDomIdx;
	/* 0x218 */ int unk218;
	/* 0x21C */ u8 unk21C;
	/* 0x220 */ f32 unk220;
	/* 0x224 */ f32 unk224[3];
	/* 0x230 */ u8 mAreaType;
	/* 0x234 */ f32 unk234;
	/* 0x238 */ u8 unk238;
	/* 0x239 */ u8 unk239;
	/* 0x23C */ JGeometry::TVec3<f32> unk23C;
	/* 0x248 */ JGeometry::TVec3<f32> unk248;
	/* 0x254 */ TMapObjBase* mMareGate;
	/* 0x258 */ TMapCollisionMove* unk258;
	/* 0x25C */ JGeometry::TVec3<f32> mCollisionVerts[4];
	/* 0x28C */ TCannonSaveLoadParams* mParams;
	/* 0x290 */ u8 unk290;
	/* 0x294 */ JGeometry::TVec3<f32> unk294;
	/* 0x2A0 */ JGeometry::TVec3<f32> unk2A0;
	/* 0x2AC */ f32 unk2AC;
	/* 0x2B0 */ TMapCollisionMove* unk2B0;
};

// Plays the bck's sound (if the bas table has one) along with the model anm.
inline void TChorobei::setBckAnm(int index)
{
	mSharedParts->getMActor()->setBckFromIndex(index);
	const char** table = mCannon->getBasNameTable();
	mAnmSoundPath      = !table ? nullptr : table[index];
	if (mAnmSoundPath != nullptr) {
		void* res = JKRFileLoader::getGlbResource(mAnmSoundPath);
		mAnmSound->initAnmSound(res, 1, 0.0f);
	} else {
		mAnmSound->initAnmSound(nullptr, 1, 0.0f);
	}
}

inline void TCannonDom::setBckAnm(int index)
{
	getMActor()->setBckFromIndex(index);
	const char** table = unk10->getBasNameTable();
	mAnmSoundPath      = !table ? nullptr : table[index];
	if (mAnmSoundPath != nullptr) {
		void* res = JKRFileLoader::getGlbResource(mAnmSoundPath);
		mAnmSound->initAnmSound(res, 1, 0.0f);
	} else {
		mAnmSound->initAnmSound(nullptr, 1, 0.0f);
	}
}

#endif
