#ifndef ENEMY_DIRECTION_CALC_HPP
#define ENEMY_DIRECTION_CALC_HPP

#include <dolphin/types.h>
#include <JSystem/JGeometry/JGVec3.hpp>

// Heading helper of the Koopa family (TKoopaJr, TLimitKoopa, TLimitKoopaJr).
// Implemented in Enemy/koopajr.cpp.
class TDirectionCalc {
public:
	TDirectionCalc();
	TDirectionCalc(f32 direction);
	TDirectionCalc(JGeometry::TVec3<f32> vec);

	static f32 r2d(f32);
	static f32 d2r(f32);

	f32 absDirection(f32);
	JGeometry::TVec3<f32> calcDirectionVector();
	void makeDirection(JGeometry::TVec3<f32>);
	f32 calcTurnDirection(f32, f32);
	f32 sub(f32);
	f32 calcNearerDirection(f32);

	/* 0x0 */ f32 mDirection;
};

#endif
