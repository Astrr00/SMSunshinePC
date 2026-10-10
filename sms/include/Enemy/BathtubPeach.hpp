#ifndef ENEMY_BATHTUB_PEACH_HPP
#define ENEMY_BATHTUB_PEACH_HPP

#include <Enemy/BathtubBinder.hpp>
#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>
#include <Strategic/Nerve.hpp>
#include <System/ParamInst.hpp>

class TLiveManager;

/**
 * @brief Save params of the bathtub Peach (/enemy/bathtubpeach.prm).
 * @details The ctor is inlined into TBathtubPeachManager::load in the
 * retail build.
 */
class TBathtubPeachParams : public TSpineEnemyParams {
public:
	TBathtubPeachParams(const char* prm)
	    : TSpineEnemyParams(prm)
	    , PARAM_INIT(turnSpeed, 8.0f)
	    , PARAM_INIT(turnSpeed2, 1.0f)
	    , PARAM_INIT(speed, 16.0f)
	    , PARAM_INIT(angle, 72.0f)
	    , PARAM_INIT(range, 100.0f)
	    , PARAM_INIT(radius, 2200.0f)
	{
		load(mPrmPath);
	}

public:
	/* 0xA8 */ TParamRT<f32> turnSpeed;
	/* 0xBC */ TParamRT<f32> turnSpeed2;
	/* 0xD0 */ TParamRT<f32> speed;
	/* 0xE4 */ TParamRT<f32> angle;
	/* 0xF8 */ TParamRT<f32> range;
	/* 0x10C */ TParamRT<f32> radius;
};

/**
 * @brief The Peach sitting in the bathtub during the Bowser Jr. boss fight.
 * @details Her escape nerve keeps her at the given angle away from Mario on a
 * circle around the bathtub, and she staggers when the bathtub tumbles.
 */
class TBathtubPeach : public TSpineEnemy {
public:
	TBathtubPeach(const char* name);

	virtual BOOL receiveMessage(THitActor*, u32);
	virtual Mtx* getRootJointMtx() const;
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual void perform(u32, JDrama::TGraphics*);
	virtual void reset();
	virtual const char** getBasNameTable() const;

	void changeAnm(int bck, int btp, f32 rate);

public:
	/* 0x150 */ TBathtubBinder mBathtubBinder;
};

class TBathtubPeachManager : public TEnemyManager {
public:
	TBathtubPeachManager(const char* name);

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
	virtual TSpineEnemy* createEnemyInstance();
};

// The theNerve() bodies are inline in the retail build (weak, with the local
// static named "nerve"), the execute() bodies are defined in BathtubPeach.cpp.
class TNervePeachEscape : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNervePeachEscape& theNerve()
	{
		static TNervePeachEscape nerve;
		return nerve;
	}
};

class TNervePeachStagger : public TNerveBase<TLiveActor> {
public:
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;

	static const TNervePeachStagger& theNerve()
	{
		static TNervePeachStagger nerve;
		return nerve;
	}
};

#endif
