#ifndef ENEMY_BOSS_HANACHAN_SUB_HPP
#define ENEMY_BOSS_HANACHAN_SUB_HPP

#include <JSystem/JGeometry/JGVec3.hpp>
#include <dolphin/types.h>

// One mass point of the chain a Wiggler (TBossHanachan) is built from.
class TSpherePoint {
public:
	TSpherePoint() { }

	/* 0x00 */ JGeometry::TVec3<f32> mLastPos;
	/* 0x0C */ JGeometry::TVec3<f32> mPos;
	/* 0x18 */ JGeometry::TVec3<f32> mVelocity;
	/* 0x24 */ f32 mLinkLength;
	/* 0x28 */ f32 mDegreeZ;
};

class TSphereLink {
public:
	TSphereLink(u16 count, const JGeometry::TVec3<f32>& pos, f32 linkLength,
	            f32 radius, f32 damping, f32 gravityY, f32 revisionRate,
	            f32 angleY);

	bool setDegreeZAndRevisionPosXZ(int index, f32 degreeZ);
	void moveHead(const JGeometry::TVec3<f32>& head);

	/* 0x00 */ u16 mNumPoints;
	/* 0x04 */ TSpherePoint* mPoints;
	/* 0x08 */ f32 mDamping;
	/* 0x0C */ f32 mGravityY;
	/* 0x10 */ f32 mRadius;
	/* 0x14 */ f32 mRevisionRate;
	/* 0x18 */ f32 mAngleY;
};

void BHSCalcRevisionDistXZByRotateZ(f32 angle, f32 a, f32 b, f32* outX,
                                    f32* outZ);
f32 BHSCalcCentrifugalForce(const JGeometry::TVec3<f32>&,
                            const JGeometry::TVec3<f32>&,
                            const JGeometry::TVec3<f32>&, f32);

#endif
