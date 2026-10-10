#ifndef ENEMY_EFFECT_ENEMY_HPP
#define ENEMY_EFFECT_ENEMY_HPP

#include <Enemy/WalkerEnemy.hpp>

class TEffectEnemyManager : public TSmallEnemyManager {
public:
	TEffectEnemyManager(const char* name)
	    : TSmallEnemyManager(name)
	{
	}

	virtual void load(JSUMemoryInputStream&);
	virtual void loadAfter();
	virtual TSmallEnemy* createEnemyInstance();
	virtual void initSetEnemies();
};

class TEffectEnemy : public TWalkerEnemy {
public:
	TEffectEnemy(const char* name = "エフェクト敵");

	virtual void perform(u32, JDrama::TGraphics*);
	virtual void init(TLiveManager*);
	virtual void reset();
	virtual void kill();
	virtual void behaveToWater(THitActor*);
	virtual void setDeadAnm();
	virtual void forceKill();
	virtual void setMActorAndKeeper();
	virtual void sendAttackMsgToMario();

public:
	/* 0x194 */ int unk194;
};

#endif
