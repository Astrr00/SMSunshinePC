#include <Camera/cameralib.hpp>
#include <Enemy/BossHanachanSub.hpp>
#include <JSystem/JGeometry/JGVec3.hpp>
#include <JSystem/JMath.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <Player/ModelWaterManager.hpp>
#include <System/MarDirector.hpp>


TSphereLink::TSphereLink(u16 count, const JGeometry::TVec3<f32>& pos,
                         f32 linkLength, f32 radius, f32 damping, f32 gravityY,
                         f32 revisionRate, f32 angleY)
    : mNumPoints(count)
{
	mPoints       = new TSpherePoint[count];
	mDamping      = damping;
	mGravityY     = gravityY;
	mRadius       = radius;
	mRevisionRate = revisionRate;
	mAngleY       = angleY;

	// The chain hangs behind the head, along the direction angleY.
	s16 angle = (s16)DEG2SHORTANGLE(mAngleY);
	f32 dx    = linkLength * JMASSin(angle);
	f32 dz    = linkLength * JMASCos(angle);

	for (int i = 0; i < mNumPoints; ++i) {
		TSpherePoint* point = &mPoints[i];

		JGeometry::TVec3<f32> p;
		if (i == 0) {
			p = pos;
		} else {
			p = mPoints[i - 1].mPos;
			p.x -= dx;
			p.z -= dz;
		}

		point->mPos      = p;
		point->mLastPos  = point->mPos;
		point->mVelocity.x = 0.0f;
		point->mVelocity.y = 0.0f;
		point->mVelocity.z = 0.0f;
		point->mLinkLength = linkLength;
		point->mDegreeZ    = 0.0f;
	}
}

bool TSphereLink::setDegreeZAndRevisionPosXZ(int index, f32 degreeZ)
{
	TSpherePoint* point = &mPoints[index];
	f32 oldDegreeZ      = point->mDegreeZ;
	bool changed        = false;

	if (oldDegreeZ != degreeZ) {
		point->mDegreeZ = degreeZ;
		changed         = true;

		f32 angle;
		if (index == 0) {
			angle = mAngleY;
		} else {
			JGeometry::TVec3<f32> toPrev = mPoints[index - 1].mPos - point->mPos;
			angle = MsWrap(MsGetRotFromZaxisY(toPrev), 0.0f, 360.0f);
		}

		f32 dx, dz;
		BHSCalcRevisionDistXZByRotateZ(angle, degreeZ - oldDegreeZ,
		                               mRevisionRate, &dx, &dz);
		point->mPos.x += dx;
		point->mPos.z += dz;
	}

	return changed;
}

void TSphereLink::moveHead(const JGeometry::TVec3<f32>& head)
{
	for (int i = 0; i < mNumPoints; ++i) {
		mPoints[i].mPos.y += mGravityY;
		mPoints[i].mPos.add(mPoints[i].mVelocity);
	}

	mPoints[0].mPos = head;

	{
		JGeometry::TVec3<f32>* pos = &mPoints[0].mPos;
		gpMap->isTouchedOneWallAndMoveXZ(&pos->x, pos->y, &pos->z, mRadius);

		const TBGCheckData* ground;
		f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
		    pos->x, pos->y + mRadius, pos->z, &ground);
		if (ground && !ground->isIllegalData()) {
			if (pos->y < groundY)
				pos->y = groundY;
		}
	}

	for (int i = 1; i < mNumPoints; ++i) {
		TSpherePoint* point = &mPoints[i];
		TSpherePoint* prev  = &mPoints[i - 1];

		JGeometry::TVec3<f32> dir = point->mPos - prev->mPos;

		if (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z <= 0.0000038146973f) {
			dir.x = 0.0f;
			dir.y = 1.0f;
			dir.z = 0.0f;
		} else {
			PSVECNormalize(&dir, &dir);
		}

		dir.x *= point->mLinkLength;
		dir.y *= point->mLinkLength;
		dir.z *= point->mLinkLength;

		JGeometry::TVec3<f32> newPos = prev->mPos + dir;
		point->mPos                  = newPos;

		JGeometry::TVec3<f32>* pos = &point->mPos;
		gpMap->isTouchedOneWallAndMoveXZ(&pos->x, pos->y, &pos->z, mRadius);

		const TBGCheckData* ground;
		f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
		    pos->x, pos->y + mRadius, pos->z, &ground);
		if (ground && !ground->isIllegalData()) {
			if (pos->y < groundY)
				pos->y = groundY;
		}
	}

	for (int i = 0; i < mNumPoints; ++i) {
		TSpherePoint* point = &mPoints[i];

		JGeometry::TVec3<f32> delta = point->mPos - point->mLastPos;
		point->mVelocity            = delta * mDamping;
		point->mLastPos  = point->mPos;
	}
}

// Receives the water gun's particle message (0xF): the hit counter is armed
// unless the game is in a talk or demo state.
BOOL TWaterHitActor::receiveMessage(THitActor*, u32 message)
{
	BOOL result = FALSE;
	if (message == 0xF) {
		if (gpMarDirector->isThing())
			mWaterHitCounter = 0;
		else
			mWaterHitCounter = 0x3C;
		result = TRUE;
	}
	return result;
}

void TWaterHitActor::onWaterHitCounter() { mWaterHitCounter = 0x3C; }

void BHSCalcRevisionDistXZByRotateZ(f32 angle, f32 a, f32 b, f32* outX,
                                    f32* outZ)
{
	JGeometry::TVec2<f32> v(b * a, 0.0f);
	s16 ang = CLBRoundf<s16>(DEG2SHORTANGLE(angle));
	f32 sn  = JMASSin(ang);
	f32 cs  = JMASCos(ang);
	*outX   = v.x * cs + v.y * sn;
	*outZ   = -v.x * sn + v.y * cs;
}

f32 BHSCalcCentrifugalForce(const JGeometry::TVec3<f32>& a,
                            const JGeometry::TVec3<f32>& b,
                            const JGeometry::TVec3<f32>& c, f32 angle)
{
	f32 abX = a.x - b.x;
	f32 abZ = a.z - b.z;
	f32 abLenSq = abX * abX + abZ * abZ;
	if (abLenSq < 0.001f)
		return 0.0f;

	f32 bcX = b.x - c.x;
	f32 bcZ = b.z - c.z;
	if (bcX * bcX + bcZ * bcZ < 0.001f)
		return 0.0f;

	JGeometry::TVec3<f32> ab(abX, 0.0f, abZ);
	s16 abAngle = CLBRoundf<s16>(DEG2SHORTANGLE(MsGetRotFromZaxisY(ab)));
	JGeometry::TVec3<f32> bc(bcX, 0.0f, bcZ);
	s16 bcAngle = CLBRoundf<s16>(DEG2SHORTANGLE(MsGetRotFromZaxisY(bc)));

	if (abAngle == bcAngle)
		return 0.0f;

	int diff = abAngle - bcAngle;
	f32 turn = CLBAbs<int>((s16)diff) * 0.000030517578f;
	if (turn >= 0.5f)
		return 0.0f;

	abLenSq *= turn;
	if (diff < 0)
		abLenSq = -abLenSq;

	s16 ref = CLBRoundf<s16>(DEG2SHORTANGLE(angle));
	return abLenSq * JMASCos((s16)(abAngle - ref));
}
