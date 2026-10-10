#ifndef ENEMY_SEAL_HPP
#define ENEMY_SEAL_HPP

#include <Enemy/Enemy.hpp>
#include <Enemy/EnemyManager.hpp>

class TSeal : public TSpineEnemy {
public:
	TSeal(const char* name = "シール");

	virtual void perform(u32 cue, JDrama::TGraphics* graphics);
	virtual BOOL receiveMessage(THitActor* sender, u32 message);
	virtual void init(TLiveManager*);
	virtual void calcRootMatrix();

public:
	/* 0x150 */ s32 unk150;
};

class TSealManager : public TEnemyManager {
public:
	TSealManager(const char* name = "シールマネージャ");

	virtual void load(JSUMemoryInputStream&);
	virtual void createModelData();
};

DECLARE_NERVE(TNerveSealDie, TLiveActor);
DECLARE_NERVE(TNerveSealWait, TLiveActor);
DECLARE_NERVE(TNerveSealSleep, TLiveActor);

#endif
