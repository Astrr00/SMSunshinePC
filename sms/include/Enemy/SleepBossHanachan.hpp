#ifndef ENEMY_SLEEP_BOSS_HANACHAN_HPP
#define ENEMY_SLEEP_BOSS_HANACHAN_HPP

#include <Enemy/DemoBossHanachan.hpp>
#include <Strategic/Nerve.hpp>

class TLiveActor;
class TLiveManager;
class TMirrorActor;

/**
 * @brief The sleeping Hanachan (wiggler) of the Pianta Village boss intro.
 * @details Plays the sleep animation until startFall() is called, then falls
 * and makes the boss shine appear at the position given to startFall().
 */
class TSleepBossHanachan : public TDemoBossHanachan {
public:
	TSleepBossHanachan(const char* name = "?")
	    : TDemoBossHanachan(name)
	    , mShinePos(0.0f, 0.0f, 0.0f)
	    , mMirrorActor(nullptr)
	{
	}

	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();
	virtual const char** getBasNameTable() const;

	void startFall(f32 x, f32 y, f32 z);

public:
	/* 0x150 */ JGeometry::TVec3<f32> mShinePos;
	/* 0x15C */ TMirrorActor* mMirrorActor;
};

class TSleepBossHanachanManager : public TDemoBossHanachanManager {
public:
	TSleepBossHanachanManager(const char* name = "?")
	    : TDemoBossHanachanManager(name)
	{
	}

	virtual void createModelData();
};

// TNerveSBH_Fall and TNerveSBH_SleepContinue are declared in BossHanachan.hpp.

#endif
