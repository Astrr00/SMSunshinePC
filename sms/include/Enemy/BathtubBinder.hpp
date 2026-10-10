#ifndef ENEMY_BATHTUB_BINDER_HPP
#define ENEMY_BATHTUB_BINDER_HPP

#include <types.h>
#include <Strategic/Binder.hpp>

class TBathtub;
class TBathWaterManager;
class TLiveActor;

/**
 * @brief Binder that keeps an actor inside the bathtub and floating on the
 * bathtub water.
 */
class TBathtubBinder : public TBinder {
public:
	TBathtubBinder();
	virtual ~TBathtubBinder();
	virtual void bind(TLiveActor*);

	bool init(f32, f32, f32, f32, f32);
	void float_(TLiveActor*);

public:
	/* 0x04 */ TBathtub* mBathtub;
	/* 0x08 */ TBathWaterManager* mWater;
	/* 0x0C */ f32 unkC;
	/* 0x10 */ f32 unk10;
	/* 0x14 */ f32 unk14;
	/* 0x18 */ f32 unk18;
	/* 0x1C */ f32 unk1C;
	/* 0x20 */ f32 unk20;
};

#endif
