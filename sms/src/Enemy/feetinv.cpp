#include <Enemy/FeetInv.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <dolphin/mtx.h>
#include <math.h>

// fabricated: acos(c) in degrees, built from matan like TSpineEnemy::calcTurnSpeedToReach
static inline f32 acosDeg(f32 c)
{
	if (c == 1.0f)
		return 0.0f;
	if (c == -1.0f)
		return 180.0f;

	f32 t = -(c * c - 1.0f);
	volatile f32 s = t * __frsqrte(t);
	return 90.0f - matan(s, c) * (360.0f / 65536.0f);
}

// fabricated: asin(x) in degrees
static inline f32 asinDeg(f32 x)
{
	if (x == 1.0f)
		return 90.0f;
	if (x == -1.0f)
		return -90.0f;

	f32 t = -(x * x - 1.0f);
	volatile f32 s = t * __frsqrte(t);
	return matan(s, x) * (360.0f / 65536.0f);
}

TMtxCalcFootInv::TMtxCalcFootInv(u16 hipA, u16 kneeA, u16 footA, u16 hipB,
                                 u16 kneeB, u16 footB, f32 offset)
    : J3DMtxCalcSoftimageAnm(nullptr)
    , unk68(hipA)
    , unk6A(kneeA)
    , unk6C(footA)
    , unk6E(hipB)
    , unk70(kneeB)
    , unk72(footB)
    , unk74(offset)
{
}

void TMtxCalcFootInv::calc(u16 jointIdx)
{
	J3DMtxCalcAnm::calc(jointIdx);

	if (unk6C == jointIdx)
		FeetInvCalc(j3dSys.mModel, unk68, unk6A, unk6C, unk74);

	if (unk72 == jointIdx)
		FeetInvCalc(j3dSys.mModel, unk6E, unk70, unk72, unk74);
}

// Two-bone IK: plants the end joint (c) on the ground below it and re-aims
// the root (a) / middle (b) / end (c) node matrices accordingly.
void FeetInvCalc(J3DModel* model, u16 a, u16 b, u16 c, f32 offset)
{
	MtxPtr mtxB = model->getAnmMtx(b);
	MtxPtr mtxC = model->getAnmMtx(c);

	JGeometry::TVec3<f32> posB;
	posB.x = mtxB[0][3];
	posB.y = mtxB[1][3];
	posB.z = mtxB[2][3];

	JGeometry::TVec3<f32> posC;
	posC.x = mtxC[0][3];
	posC.y = mtxC[1][3];
	posC.z = mtxC[2][3];

	JGeometry::TVec3<f32> lower = posC;
	lower -= posB;
	f32 lowerLen = lower.length();

	const TBGCheckData* ground;
	f32 groundY
	    = gpMap->checkGround(posC.x, posC.y + lowerLen, posC.z, &ground);

	f32 targetY = groundY + offset;
	if (targetY < posC.y)
		return;
	posC.y = targetY;

	MtxPtr mtxA = model->getAnmMtx(a);
	JGeometry::TVec3<f32> posA;
	posA.x = mtxA[0][3];
	posA.y = mtxA[1][3];
	posA.z = mtxA[2][3];

	JGeometry::TVec3<f32> upper = posB;
	upper -= posA;
	f32 upperLen = upper.length();

	JGeometry::TVec3<f32> reach = posC;
	reach -= posA;
	f32 reachLen = reach.length();

	JGeometry::TVec3<f32> axis;
	axis.cross(reach, upper);
	f32 dot = reach.dot(upper);
	f32 hipAngle = MsAtan2(dot, MsVECMag2(&axis));

	// law of cosines for the knee angle, law of sines for the hip angle
	f32 kneeCos = (upperLen * upperLen + lowerLen * lowerLen
	               - reachLen * reachLen)
	              / (2.0f * upperLen * lowerLen);
	f32 kneeAngle = acosDeg(kneeCos);
	f32 hipSin    = lowerLen * MsSin(kneeAngle) / reachLen;
	f32 hipTurn   = asinDeg(hipSin) - hipAngle;

	Mtx rot;
	MsMtxSetRotZ(rot, -hipTurn);
	PSMTXConcat(mtxA, rot, mtxA);

	// the middle joint follows the rotated root's x axis
	JGeometry::TVec3<f32> xAxisA(mtxA[0][0], mtxA[1][0], mtxA[2][0]);
	xAxisA.normalize();
	xAxisA.scale(upperLen);
	posB = posA + xAxisA;
	mtxB[0][3] = posB.x;
	mtxB[1][3] = posB.y;
	mtxB[2][3] = posB.z;

	// the middle joint's x axis points at the end joint; its scales are kept
	JGeometry::TVec3<f32> toEnd = posC;
	f32 lenZB = MsSqrtf(mtxB[0][2] * mtxB[0][2] + mtxB[1][2] * mtxB[1][2]
	                    + mtxB[2][2] * mtxB[2][2]);
	f32 lenXB = MsSqrtf(mtxB[0][0] * mtxB[0][0] + mtxB[1][0] * mtxB[1][0]
	                    + mtxB[2][0] * mtxB[2][0]);
	toEnd -= posB;
	toEnd.normalize();
	toEnd.scale(lenXB);
	mtxB[0][0] = toEnd.x;
	mtxB[1][0] = toEnd.y;
	mtxB[2][0] = toEnd.z;

	JGeometry::TVec3<f32> yAxisB(mtxB[0][1], mtxB[1][1], mtxB[2][1]);
	JGeometry::TVec3<f32> zAxisB;
	zAxisB.cross(toEnd, yAxisB);
	zAxisB.normalize();
	zAxisB.scale(lenZB);
	mtxB[0][2] = zAxisB.x;
	mtxB[1][2] = zAxisB.y;
	mtxB[2][2] = zAxisB.z;

	mtxC[0][3] = posC.x;
	mtxC[1][3] = posC.y;
	mtxC[2][3] = posC.z;

	// the end joint's y axis points into the ground, its x axis completes the frame
	JGeometry::TVec3<f32> yAxisC = ground->mNormal;
	f32 lenYC = MsSqrtf(mtxC[0][1] * mtxC[0][1] + mtxC[1][1] * mtxC[1][1]
	                    + mtxC[2][1] * mtxC[2][1]);
	f32 lenXC = MsSqrtf(mtxC[0][0] * mtxC[0][0] + mtxC[1][0] * mtxC[1][0]
	                    + mtxC[2][0] * mtxC[2][0]);
	yAxisC.negate();
	yAxisC.scale(lenYC);
	mtxC[0][1] = yAxisC.x;
	mtxC[1][1] = yAxisC.y;
	mtxC[2][1] = yAxisC.z;

	JGeometry::TVec3<f32> zAxisC;
	zAxisC.x = mtxC[0][2];
	zAxisC.y = mtxC[1][2];
	zAxisC.z = mtxC[2][2];
	JGeometry::TVec3<f32> xAxisC;
	xAxisC.cross(yAxisC, zAxisC);
	xAxisC.normalize();
	xAxisC.scale(lenXC);
	mtxC[0][0] = xAxisC.x;
	mtxC[1][0] = xAxisC.y;
	mtxC[2][0] = xAxisC.z;
}
