#include <Enemy/BossHanachan.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <Camera/CameraShake.hpp>
#include <Camera/cameralib.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DMaterialAttach.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/MapData.hpp>
#include <MarioUtil/MapUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSModBgm.hpp>
#include <NPC/NpcInbetween.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Binder.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/MarDirector.hpp>
#include <System/TargetArrow.hpp>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

const char* cSandPillarModelName = "sunabashira.bmd";
const char* cHitPoint1_RailName  = "bosshanachan2";
const char* cHitPoint2_RailName  = "bosshanachan1";
const char* cSandTextureName     = "suna";
const char* cDummyTextureName    = "M_dummy";

static void CalcRevisionPosByRotateZ(const JGeometry::TVec3<f32>& rotation,
                                     f32 plusYRate, Vec* position);

// MsWrap is never inlined into this unit's callers (nested inline), so the
// wrap to [-180, 180) is its own inline helper like MsAngleWrap.
static inline f32 MsWrapAngle180(f32 angle)
{
	return MsWrap(angle, -180.0f, 180.0f);
}

// Largest-magnitude mRotation.z over the body parts. Static inline so
// execSlip folds it in; getBodyMaxRotateZ stays out of line for perform.
static inline f32 bodyMaxRotateZ(TBossHanachanPartsBody* const* body)
{
	f32 maxRotateZ = 0.0f;
	for (int i = 0; i < 8; ++i, ++body) {
		f32 rotateZ = (*body)->mRotation.z;
		if (fabsf(rotateZ) > fabsf(maxRotateZ))
			maxRotateZ = rotateZ;
	}
	return maxRotateZ;
}

BOOL TBossHanachan::hasMapCollision() const { return TRUE; }

void TBossHanachan::removeAllMapCollision()
{
	mHeadPart->mMapCollision->remove();
	for (int i = 0; i < 8; ++i)
		mBodyParts[i]->mMapCollision->remove();
}

void TBossHanachan::execDamage()
{
	mSpine->reset();
	decHitPoints();

	if (mHitPoints == 0) {
		mHeadPart->mHitActor->onHitFlag(HIT_FLAG_NO_COLLISION);
		for (int i = 0; i < 8; ++i) {
			mBodyParts[i]->mHitActor->onHitFlag(HIT_FLAG_NO_COLLISION);
			mBodyParts[i]->mFootHitActors[0]->onHitFlag(HIT_FLAG_NO_COLLISION);
			mBodyParts[i]->mFootHitActors[1]->onHitFlag(HIT_FLAG_NO_COLLISION);
		}

		mSpine->setNext(&TNerveBossHanachanDead::theNerve());
		setAnmTimerWhenDead();
		mDeadMarioPos = *gpMarioPos;
		gpMSound->startSoundActor(0x28E6, &mDeadMarioPos, 0, nullptr, 0, 4);
	} else {
		mSpine->setNext(&TNerveBossHanachanDamage::theNerve());
		setAnmTimerWhenDamage();

		TBossHanachanChangeSaveParams** params
		    = static_cast<TBossHanachanManager*>(mManager)->mChangeSaveParams;
		mChangeSaveParams = params[3 - mHitPoints];

		const char* railName = cHitPoint1_RailName;
		switch (mHitPoints) {
		case 1:
			railName = cHitPoint1_RailName;
			break;
		case 2:
			railName = cHitPoint2_RailName;
			break;
		}
		getTracer()->setGraph(gpConductor->getGraphByName(railName));
		onLiveFlag(LIVE_FLAG_UNK20000);

		gpMSound->startSoundActor(
		    0x280F, &mBodyParts[mWeakBodyIndex]->mJointPos, 0, nullptr, 0, 4);
	}
}

void TBossHanachan::goToInitialRecoverGraphNode()
{
	getTracer()->reset();
	getTracer()->reset2();

	int index = getTracer()->getGraph()->findNearestVisibleIndex(
	    mPosition, mRotation.y, mCommonSaveParams->mSLRecoverSearchDist.get(),
	    mCommonSaveParams->mSLRecoverSearchDegree.get(), 0xFFFFFFFF);
	if (index < 0) {
		goToShortestNextGraphNode();
	} else {
		getTracer()->setTo(index);
		setGoalPathFromGraph();
		unk128 = 0;
		unk12C = 0.0f;
	}
}

void TBossHanachan::execSlip()
{
	CLBChaseGeneralConstantSpecifySpeed<f32>(
	    &mMarchSpeed, 0.0f, mChangeSaveParams->mSLMarchDecrease.get());
	mTurnSpeed = 0.1f;

	if (mMarchSpeed > 0.001f) {
		JGeometry::TVec3<f32> velocity = mLastLinearVelocity;

		if (mMarchSpeed > 4.0f) {
			// Push along the boss' own Z axis (towards the side it is leaning
			// to), rotated by its yaw.
			f32 maxRotateZ = bodyMaxRotateZ(mBodyParts);
			f32 dirY       = 0.0f;
			f32 dirX       = dirY;
			f32 dirZ       = 1.0f;
			if (maxRotateZ > 0.0f) {
				dirY = -dirY;
				dirZ = -dirZ;
				dirX = dirY;
			}

			s16 angle = CLBRoundf<s16>(DEG2SHORTANGLE(mRotation.y));
			f32 sinY  = JMASSin(angle);
			f32 cosY  = JMASCos(angle);
			f32 speed = 0.005f * mMarchSpeed;
			f32 rotX  = dirZ * cosY + dirX * sinY;
			f32 rotZ  = -dirZ * sinY + dirX * cosY;
			JGeometry::TVec3<f32> push(rotX, dirY, rotZ);
			push.scale(speed);
			velocity.add(push);
		}

		if (!velocity.isZero()) {
			MsVECNormalize(&velocity, &velocity);
			velocity.scale(500.0f);

			JGeometry::TVec3<f32> goal = mPosition;
			goal.add(velocity);
			setGoalPath(TPathNode(goal));
			walkToCurPathNode(mMarchSpeed, mTurnSpeed, 0.0f);
		}

		gpCameraShake->keepShake(static_cast<EnumCamShakeMode>(9), 1.0f);
		if (SMS_IsMarioTouchGround4cm() && mSpine->getTime() < 120)
			SMSRumbleMgr->start(0x16, static_cast<f32*>(nullptr));
	}
}

void TBossHanachan::execWalk(bool run)
{
	if (run) {
		CLBChaseGeneralConstantSpecifySpeed<f32>(
		    &mMarchSpeed, mChangeSaveParams->mSLMaxMarchSpeed.get(),
		    mChangeSaveParams->mSLMarchAccel.get());
	} else {
		CLBChaseGeneralConstantSpecifySpeed<f32>(
		    &mMarchSpeed, 0.0f, mChangeSaveParams->mSLMarchDecrease.get());
	}
	mTurnSpeed = mChangeSaveParams->mSLWalkTurnSpeed.get();

	bool shouldWalk = true;
	{
		const JGeometry::TVec3<f32>& goal
		    = unkF4.unk0 ? unkF4.unk0->mPosition : unkF4.unk4;
		JGeometry::TVec3<f32> pathNode = goal;
		JGeometry::TVec3<f32> diff;
		diff.set(pathNode.x - mPosition.x, 0.0f, pathNode.z - mPosition.z);
		if (diff.squared() < CLBSquared<f32>(10.0f))
			shouldWalk = false;
	}

	if (shouldWalk)
		walkToCurPathNode(mMarchSpeed, mTurnSpeed, 0.0f);

	const JGeometry::TVec3<f32>& goal
	    = unkF4.unk0 ? unkF4.unk0->mPosition : unkF4.unk4;
	JGeometry::TVec3<f32> pathNode = goal;
	JGeometry::TVec3<f32> diff(pathNode.x - mPosition.x, 0.0f,
	                           pathNode.z - mPosition.z);
	if (diff.squared() < CLBSquared<f32>(100.0f)) {
		if (!unk114.empty())
			switchNextGoalPath();
		else
			goToDirLimitedNextGraphNode(90.0f);
	}
}

#pragma dont_inline on
f32 TBossHanachan::getBodyMaxRotateZ() const
{
	return bodyMaxRotateZ(mBodyParts);
}
#pragma dont_inline off

bool TBossHanachan::checkFallDecideAndSetup()
{
	bool result = false;
	for (int i = 0; i < 8; ++i) {
		TBossHanachanPartsBody* body = mBodyParts[i];
		if (CLBAbs<f32>(body->mRotation.z)
		    > mChangeSaveParams->mSLFallDecideRotateZ.get()) {
			emitOneTimeSandPillar_(body);
			if (body->mRotation.z > 0.0f)
				mTumbleTargetAngleZ = 179.0f;
			else
				mTumbleTargetAngleZ = -179.0f;

			mTumbleAngularSpeedZ
			    = mChangeSaveParams->mSLWaveFallDownSpeed.get()
			      * CLBAbs<f32>(body->mLastRotZ - body->mRotation.z);
			if (mTumbleAngularSpeedZ
			    < mChangeSaveParams->mSLFallDecideMinSpeed.get())
				mTumbleAngularSpeedZ
				    = mChangeSaveParams->mSLFallDecideMinSpeed.get();
			result = true;
			break;
		}
	}
	return result;
}

bool TBossHanachan::isTumbleCompletelyAllBody() const
{
	bool result  = true;
	f32 rotateZ  = mBodyParts[0]->mRotation.z;
	bool tumbled = (rotateZ == -179.0f || rotateZ == 179.0f) ? true : false;
	if (!tumbled) {
		result = false;
	} else {
		for (int i = 1; i < 8; ++i) {
			if (mBodyParts[i]->mRotation.z != rotateZ) {
				result = false;
				break;
			}
		}
	}
	return result;
}

void TBossHanachan::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK200))
		return;

	if (checkLiveFlag(LIVE_FLAG_UNK40000)) {
		if (!(cue & CUE_MOVE))
			return;
		if (!(graphics->unk0 & 2))
			return;

		gpMSound->startSoundActor(0x6010, &mPosition, 0, nullptr, 0, 4);
		if (gpMarDirector->isThing())
			return;

		if (!checkLiveFlag(LIVE_FLAG_UNK100000)) {
			if (gpMSound->unk98->modBgm(1, 1) != nullptr)
				return;
		}

		onLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_UNK40);
		gpItemManager->makeShineAppearWithDemo(
		    "シャイン（ボス用）", "ボスシャインカメラ", mHeadPos.x,
		    mHeadPos.y + mCommonSaveParams->mSLShineAppearOffsetY.get(),
		    mHeadPos.z);
		return;
	}

	if (cue & CUE_MOVE) {
		if (gpMarDirector->isThing()) {
			mLinearVelocity.zero();
			mAngularVelocity.zero();
			if (graphics->unk0 & 2) {
				if (mSpine->getLatestNerve()
				    == &TNerveBossHanachanDead::theNerve()) {
					if (!checkLiveFlag(LIVE_FLAG_UNK100000)) {
						onLiveFlag(LIVE_FLAG_UNK100000);
						MSBgm::stopTrackBGM(1, 30);
					}
				}
			}
		} else {
			if (!checkLiveFlag(LIVE_FLAG_UNK80000)) {
				if (mHitPoints == 3 && mMarchSpeed != 0.0f) {
					const TNerveBase<TLiveActor>* nerve
					    = mSpine->getLatestNerve();
					if (nerve != &TNerveBossHanachanTumble::theNerve()) {
						if (mHintBalloonTimer == -1
						    && gpMarDirector->mState == 4) {
							mHintBalloonTimer = 7200;
						} else if (mHintBalloonTimer > 0) {
							mHintBalloonTimer--;
							if (mHintBalloonTimer == 0) {
								mHintBalloonTimer = 7200;
								gpMarDirector->mConsole->startAppearBalloon(
								    0xE0006, true);
							}
						}
					}
				}
			}

			moveObject();

			const TNerveBase<TLiveActor>* curNerve = mSpine->getLatestNerve();

			for (int i = 0; i < 8; ++i) {
				TBossHanachanPartsBody* body = mBodyParts[i];
				body->mLastLastPos           = body->mLastPos;
				body->mLastPos               = body->mPosition;
				body->mLastLastRotZ          = body->mLastRotZ;
				body->mLastRotZ              = body->mRotation.z;
				body->mLastRotZSpeed         = body->mRotZSpeed;
			}

			s16 headAngle
			    = CLBRoundf<s16>(DEG2SHORTANGLE(mBodyParts[0]->mRotation.y));
			CLBChaseAngleDecrease(
			    &headAngle, CLBRoundf<s16>(DEG2SHORTANGLE(mRotation.y)), 20);
			mBodyParts[0]->mRotation.y = headAngle * (360.0f / 65536.0f);

			for (int i = 1; i < 8; ++i) {
				JGeometry::TVec3<f32> diff = mSphereLink->mPoints[i - 1].mPos
				                             - mSphereLink->mPoints[i].mPos;
				mBodyParts[i]->mRotation.y
				    = MsAngleWrap(MsGetRotFromZaxisY(diff));
			}

			mSphereLink->mAngleY = mBodyParts[0]->mRotation.y;
			for (int i = 0; i < 8; ++i)
				mSphereLink->setDegreeZAndRevisionPosXZ(
				    i, mBodyParts[i]->mRotation.z);

			JGeometry::TVec3<f32> headPos = mPosition;
			f32 revisionX;
			f32 revisionZ;
			headPos.x -= JMASSin(DEG2SHORTANGLE(mRotation.y))
			             * mCommonSaveParams->mSLHeadLength.get();
			headPos.z -= JMASCos(DEG2SHORTANGLE(mRotation.y))
			             * mCommonSaveParams->mSLHeadLength.get();
			BHSCalcRevisionDistXZByRotateZ(mRotation.y, mRotation.z,
			                               mSphereLink->mRevisionRate,
			                               &revisionX, &revisionZ);
			headPos.x += revisionX;
			headPos.z += revisionZ;
			mSphereLink->moveHead(headPos);

			for (int i = 0; i < 8; ++i) {
				TBossHanachanPartsBody* body = mBodyParts[i];
				BHSCalcRevisionDistXZByRotateZ(
				    body->mRotation.y, body->mRotation.z,
				    mSphereLink->mRevisionRate, &revisionX, &revisionZ);
				body->mPosition = mSphereLink->mPoints[i].mPos;
				body->mPosition.x -= revisionX;
				body->mPosition.z -= revisionZ;
			}

			// While tumbling every body is pulled towards the most tilted
			// one, otherwise towards the centrifugal force of its own bend.
			bool tumbling       = false;
			f32 targetRotZSpeed = 0.0f;
			if (mSpine->getLatestNerve()
			    == &TNerveBossHanachanTumble::theNerve()) {
				tumbling        = true;
				targetRotZSpeed = getBodyMaxRotateZ();
			}

			for (int i = 0; i < 8; ++i) {
				TBossHanachanPartsBody* body = mBodyParts[i];
				if (!tumbling) {
					targetRotZSpeed
					    = BHSCalcCentrifugalForce(
					          body->mPosition, body->mLastPos,
					          body->mLastLastPos, body->mRotation.y)
					      * mChangeSaveParams->mSLCentrifugalForce.get();
				}
				CLBChaseGeneralConstantSpecifySpeed<f32>(
				    &body->mRotZSpeed, targetRotZSpeed,
				    mChangeSaveParams->mSLCentrifugalSpeed.get());

				f32 speedZ = body->mRotZSpeed;
				if (speedZ > 179.0f)
					speedZ = 179.0f;
				else if (speedZ < -179.0f)
					speedZ = -179.0f;
				body->mRotZSpeed = speedZ;
			}

			if (curNerve != &TNerveBossHanachanDown::theNerve()) {
				for (int i = 0; i < 8; ++i) {
					TBossHanachanPartsBody* body = mBodyParts[i];
					f32 groundY = gpMap->checkGroundIgnoreWaterSurface(
					    body->mPosition.x, 500.0f + body->mPosition.y,
					    body->mPosition.z, &body->mGroundPlane);
					const TLiveActor* sand = body->getSandActor_();
					if (sand != nullptr) {
						JGeometry::TVec3<f32> toSand = sand->mPosition - mPosition;
						JGeometry::TVec3<f32> diff = toSand;
						if (diff.x * diff.x + diff.z * diff.z
						    <= CLBSquared<f32>(50.0f)) {
							body->mSandTiltRotZ = 0.0f;
						} else {
							f32 angleToSand
							    = MsWrapAngle180(MsGetRotFromZaxisY(diff));
							f32 angleDiff = MsWrapAngle180(
							    angleToSand - MsWrapAngle180(mRotation.y));
							f32 absAngleDiff = CLBAbs<f32>(angleDiff);
							if (absAngleDiff <= 15.0f
							    || absAngleDiff >= 165.0f) {
								body->mSandTiltRotZ = 0.0f;
							} else {
								f32 riseUp = SMS_GetSandRiseUpRatio(sand);
								if (angleDiff > 15.0f)
									body->mSandTiltRotZ = 70.0f * riseUp;
								else
									body->mSandTiltRotZ = -70.0f * riseUp;
							}
						}
					} else {
						s16 angle
						    = CLBRoundf<s16>(DEG2SHORTANGLE(body->mRotation.y));
						f32 sinY    = JMASSin(angle);
						f32 cosY    = JMASCos(angle);
						f32 offsetZ = -200.0f * sinY + 0.0f * cosY;
						f32 offsetX = 200.0f * cosY + 0.0f * sinY;
						const TBGCheckData* plane;
						f32 heightA = gpMap->checkGroundIgnoreWaterSurface(
						    offsetX + body->mPosition.x,
						    500.0f + body->mPosition.y,
						    offsetZ + body->mPosition.z, &plane);
						f32 heightB = gpMap->checkGroundIgnoreWaterSurface(
						    -offsetX + body->mPosition.x,
						    500.0f + body->mPosition.y,
						    -offsetZ + body->mPosition.z, &plane);
						f32 diffA = heightA - groundY;
						f32 diffB = heightB - groundY;
						if (fabsf(diffA) < 0.001f && fabsf(diffB) < 0.001f) {
							body->mSandTiltRotZ = 0.0f;
						} else if (fabsf(diffA) > fabsf(diffB)) {
							body->mSandTiltRotZ
							    = (360.0f / 65536.0f) * matan(200.0f, diffA);
						} else {
							body->mSandTiltRotZ
							    = -((360.0f / 65536.0f) * matan(200.0f, diffB));
						}
					}
				}

				if (curNerve == &TNerveBossHanachanGetUp::theNerve()) {
					mHeadPart->calcRotateZWhenGetUp_();
					mRotation.z = mHeadPart->mRotation.z;
					for (int i = 0; i < 8; ++i)
						mBodyParts[i]->calcRotateZWhenGetUp_();
				} else {
					f32 bodyMaxRotateZ = getBodyMaxRotateZ();
					const TNerveBase<TLiveActor>* nerve
					    = mSpine->getLatestNerve();

					// constants of the discretised wave equation
					f32 bodyLength = mCommonSaveParams->mSLBodyLength.get();
					f32 decrease   = mChangeSaveParams->mSLWaveDecrease.get()
					                 * (1.0f / 120.0f);
					f32 invLength2 = 1.0f / (bodyLength * bodyLength);
					f32 waveVel    = mChangeSaveParams->mSLWaveVelocity.get();
					f32 coef1      = 1.0f / (1.0f + decrease);
					f32 coef2      = 1.0f - decrease;
					f32 coef3      = (1.0f / 120.0f)
					                 * ((1.0f / 120.0f) * (waveVel * waveVel));

					for (int i = 0; i < 8; ++i) {
						TBossHanachanPartsBody* body = mBodyParts[i];
						bool atLimit = (body->mRotation.z == -179.0f
						                || body->mRotation.z == 179.0f)
						                   ? true
						                   : false;
						if (atLimit && body->mRotation.z == bodyMaxRotateZ)
							continue;

						f32 prevRotZ  = (i == 0) ? mBodyParts[1]->mLastRotZ
						                         : mBodyParts[i - 1]->mLastRotZ;
						f32 nextRotZ  = (i == 7) ? mBodyParts[6]->mLastRotZ
						                         : mBodyParts[i + 1]->mLastRotZ;
						f32 lastRotZ2 = 2.0f * body->mLastRotZ;
						f32 target
						    = coef3
						      * (invLength2
						             * ((nextRotZ + prevRotZ) - lastRotZ2)
						         + body->mLastRotZSpeed);
						target = coef1 * target;
						target = lastRotZ2 * coef1 + target;
						target = target - coef1 * (body->mLastLastRotZ * coef2);
						if (target > 179.0f)
							target = 179.0f;
						else if (target < -179.0f)
							target = -179.0f;
						CLBChaseGeneralConstantSpecifySpeed(
						    &body->mRotation.z, target,
						    mChangeSaveParams->mSLRotateZLeanSpeed.get());

						bool hasSand = false;
						if (nerve
						    == &TNerveBossHanachanGraphWander::theNerve()) {
							if (body->getSandActor_() != nullptr) {
								hasSand = true;
								if (body->mSandTiltRotZ != 0.0f) {
									f32 slope = body->mSandTiltRotZ
									            * mMarchSpeed
									            * mChangeSaveParams
									                  ->mSLSandSlopeForce.get();
									f32 limit = 179.0f;
									if (body->mSandTiltRotZ < 0.0f)
										limit = -179.0f;
									CLBChaseGeneralConstantSpecifySpeed(
									    &body->mRotation.z, limit, slope);
								}
							} else {
								CLBChaseGeneralConstantSpecifySpeed(
								    &body->mRotation.z, body->mSandTiltRotZ,
								    mChangeSaveParams
								        ->mSLRotateZRestorationSpeed.get());
							}
						} else if (nerve
						           == &TNerveBossHanachanTumble::theNerve()) {
							CLBChaseGeneralConstantSpecifySpeed(
							    &body->mRotation.z, mTumbleTargetAngleZ,
							    mTumbleAngularSpeedZ);
						}

						f32 rotZ = body->mRotation.z;
						if (rotZ > 179.0f)
							rotZ = 179.0f;
						else if (rotZ < -179.0f)
							rotZ = -179.0f;
						body->mRotation.z = rotZ;

						if (nerve
						    == &TNerveBossHanachanGraphWander::theNerve()) {
							if (mSpine->getTime()
							        < mChangeSaveParams->mSLNotFallDownFrames
							              .get()
							    || !hasSand) {
								f32 maxRotateNotSand
								    = mChangeSaveParams->mSLMaxRotateZNotSand
								          .get();
								f32 curRotZ = body->mRotation.z;
								if (curRotZ < -maxRotateNotSand) {
									CLBChaseGeneralConstantSpecifySpeed(
									    &body->mRotation.z, -maxRotateNotSand,
									    15.0f);
								} else if (curRotZ > maxRotateNotSand) {
									CLBChaseGeneralConstantSpecifySpeed(
									    &body->mRotation.z, maxRotateNotSand,
									    15.0f);
								}
							}
						}
					}

					f32 diffMax = mChangeSaveParams->mSLDiffMaxRotateZ.get();
					for (int i = 1; i < 8; ++i) {
						f32 prevRotZ = mBodyParts[i - 1]->mRotation.z;
						f32 rotZ     = mBodyParts[i]->mRotation.z;
						if (fabsf(prevRotZ - rotZ) > diffMax) {
							if (rotZ < prevRotZ)
								mBodyParts[i]->mRotation.z = prevRotZ - diffMax;
							else
								mBodyParts[i]->mRotation.z = prevRotZ + diffMax;
						}
					}
				}
			}

			if (mHeadPart->mHitActor->mWaterHitCounter >= 1)
				mHeadPart->mHitActor->mWaterHitCounter -= 1;

			bool thrown = false;
			{
				TWaterHitActor* hit = mHeadPart->mHitActor;
				for (int j = 0; j < hit->getColNum(); ++j) {
					if (hit->getCollision(j)->getActorType() == 0x80000001) {
						throwMario_(hit);
						thrown = true;
						break;
					}
				}
			}

			for (int i = 0; i < 8; ++i) {
				TBossHanachanPartsBody* body = mBodyParts[i];
				if (body->mHitActor->mWaterHitCounter >= 1)
					body->mHitActor->mWaterHitCounter -= 1;
				if (!thrown) {
					TWaterHitActor* hit = body->mHitActor;
					for (int j = 0; j < hit->getColNum(); ++j) {
						if (hit->getCollision(j)->getActorType() == 0x80000001) {
							throwMario_(hit);
							thrown = true;
							break;
						}
					}
				}

				for (int k = 0; k < 2; ++k) {
					TFootHitActor* foot = body->mFootHitActors[k];
					if (foot->mWaterHitCounter >= 1)
						foot->mWaterHitCounter -= 1;
					if (!thrown) {
						for (int j = 0; j < foot->getColNum(); ++j) {
							if (foot->getCollision(j)->getActorType()
							    == 0x80000001) {
								throwMario_(foot);
								thrown = true;
								break;
							}
						}
					}
				}
			}

			if (graphics->unk0 & 2) {
				if (curNerve == &TNerveBossHanachanDead::theNerve()) {
					gpMSound->startSoundActor(0x6010, &mPosition, 0, nullptr, 0,
					                          4);
					if (!checkLiveFlag(LIVE_FLAG_UNK100000))
						gpMSound->unk98->modBgm(1, 1);
				}
			}
		}

		mHeadPart->moveMapCollision_();
		for (int i = 0; i < 8; ++i)
			mBodyParts[i]->moveMapCollision_();
	}

	if (cue & CUE_CALC_ANIM) {
		gpTargetArrow->unk14 = 0;
		if (!gpMarDirector->isThing()) {
			bool walking = false;
			if (mSpine->getLatestNerve()
			        == &TNerveBossHanachanGraphWander::theNerve()
			    && mMarchSpeed > 0.001f)
				walking = true;

			MtxPtr headMtx = mHeadPart->mJointMtx;
			JGeometry::TVec3<f32> headJoint(headMtx[0][3], headMtx[1][3],
			                                headMtx[2][3]);
			TWaterHitActor* headHit = mHeadPart->mHitActor;
			headHit->mPosition.x    = headJoint.x;
			headHit->mPosition.y
			    = headJoint.y - mCommonSaveParams->mSLHeadHitOffsetY.get();
			headHit->mPosition.z = headJoint.z;
			if (walking) {
				if (!headHit->checkHitFlag(0x80000000)) {
					headHit->onHitFlag(0x80000000);
					mHeadPart->mMapCollision->remove();
				}
			} else {
				if (headHit->checkHitFlag(0x80000000)) {
					headHit->offHitFlag(0x80000000);
					mHeadPart->mMapCollision->setUpTrans(headJoint);
				}
			}

			f32 footOffsetY = mCommonSaveParams->mSLFootHitOffsetY.get();
			f32 bodyOffsetY = mCommonSaveParams->mSLBodyHitOffsetY.get();
			for (int i = 0; i < 8; ++i) {
				TBossHanachanPartsBody* body = mBodyParts[i];
				body->mJointPos.set(body->mJointMtx[0][3],
				                    body->mJointMtx[1][3],
				                    body->mJointMtx[2][3]);
				TWaterHitActor* bodyHit = body->mHitActor;
				bodyHit->mPosition.x    = body->mJointPos.x;
				bodyHit->mPosition.y    = body->mJointPos.y - bodyOffsetY;
				bodyHit->mPosition.z    = body->mJointPos.z;
				if (walking) {
					if (!bodyHit->checkHitFlag(0x80000000)) {
						bodyHit->onHitFlag(0x80000000);
						body->mMapCollision->remove();
					}
				} else {
					if (bodyHit->checkHitFlag(0x80000000)) {
						bodyHit->offHitFlag(0x80000000);
						body->mMapCollision->setUpTrans(body->mJointPos);
					}
				}

				for (int k = 0; k < 2; ++k) {
					TFootHitActor* foot = body->mFootHitActors[k];
					MtxPtr footMtx      = foot->mJointMtx;
					foot->mPosition.x   = footMtx[0][3];
					foot->mPosition.y   = footMtx[1][3] - footOffsetY;
					foot->mPosition.z   = footMtx[2][3];
					if (walking)
						foot->onHitFlag(0x80000000);
					else
						foot->offHitFlag(0x80000000);
				}
			}

			emitParticle_();
			emitCamShake_();
		}

		if (checkLiveFlag(LIVE_FLAG_UNK10000)) {
			if (mSandPillarActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
				offLiveFlag(LIVE_FLAG_UNK10000);
		}

		if (!gpMarDirector->isThing())
			changeAnmRateAndFrameUpdate_();

		mHeadPart->mInbetween->execMotionBlend(mHeadPart->mMActor);
		for (int i = 0; i < 8; ++i)
			mBodyParts[i]->mInbetween->execMotionBlend(mBodyParts[i]->mMActor);
	}

	if (cue & CUE_CALC_ANIM) {
		JGeometry::TVec3<f32> headPos = mPosition;
		CalcRevisionPosByRotateZ(mRotation,
		                         mCommonSaveParams->mSLHeadPlusYByRotateZ.get(),
		                         &headPos);
		CLBCalcRotateZXYTranslateMatrix(
		    mHeadPart->mMActor->getModel()->getBaseTRMtx(), mRotation, headPos);
		mHeadPart->mMActor->calc();

		for (int i = 0; i < 8; ++i) {
			TBossHanachanPartsBody* body  = mBodyParts[i];
			JGeometry::TVec3<f32> bodyPos = body->mPosition;
			CalcRevisionPosByRotateZ(
			    body->mRotation, mCommonSaveParams->mSLBodyPlusYByRotateZ.get(),
			    &bodyPos);
			Mtx mtx;
			CLBCalcRotateZXYTranslateMatrix(mtx, body->mRotation, bodyPos);
			MTXCopy(mtx, body->mMActor->getModel()->getBaseTRMtx());
			body->mMActor->calc();
		}

		if (!gpMarDirector->isThing()) {
			if (mSpine->getLatestNerve()
			        == &TNerveBossHanachanTumble::theNerve()
			    || mSpine->getLatestNerve()
			           == &TNerveBossHanachanDown::theNerve()) {
				MtxPtr weakMtx = mBodyParts[mWeakBodyIndex]->mJointMtx;
				JGeometry::TVec3<f32> arrowPos(
				    weakMtx[0][3], 400.0f + weakMtx[1][3], weakMtx[2][3]);
				gpTargetArrow->setPos(arrowPos);
				gpTargetArrow->unk14 = 1;
			}
		}
	}

	if (cue & CUE_ENTRY) {
		mHeadPart->entryCircleShadow_();
		mHeadPart->setDamageFog_(graphics);
		mHeadPart->drawObject(graphics);
		for (int i = 0; i < 8; ++i) {
			TBossHanachanPartsBody* body = mBodyParts[i];
			body->entryCircleShadow_();
			body->setDamageFog_(graphics);
			body->drawObject(graphics);
		}
	}

	if (cue & CUE_CALC_VIEW) {
		mHeadPart->mMActor->viewCalc();
		for (int i = 0; i < 8; ++i)
			mBodyParts[i]->mMActor->viewCalc();
	}

	if (checkLiveFlag(LIVE_FLAG_UNK10000))
		mSandPillarActor->perform(cue, graphics);
}

void TBossHanachan::moveObject()
{
	updateSquareToMario();
	mLastLinearVelocity.x = mLinearVelocity.x;
	mLastLinearVelocity.y = mLinearVelocity.y;
	mLastLinearVelocity.z = mLinearVelocity.z;
	TLiveActor::moveObject();

	if (mSpine->getLatestNerve() != &TNerveBossHanachanGetUp::theNerve())
		CLBChaseDecrease(&mRotation.z, mBodyParts[0]->mRotation.z, 0.04f, 0.0f);

	mHeadPart->mPosition    = mPosition;
	mHeadPart->mRotation    = mRotation;
	mHeadPart->mGroundPlane = mGroundPlane;
}

void TBossHanachan::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	if (mBinder != nullptr) {
		mBinder->bind(this);
		return;
	}

	JGeometry::TVec3<f32> nextPos = mPosition;
	nextPos.add(mLinearVelocity);
	nextPos.add(mVelocity);

	mVelocity.y -= getGravityY();
	if (mVelocity.y < mVelocityMinY)
		mVelocity.y = mVelocityMinY;

	mHeadPos.x = nextPos.x;
	mHeadPos.y = nextPos.y;
	mHeadPos.z = nextPos.z;

	f32 revisionX;
	f32 revisionZ;
	BHSCalcRevisionDistXZByRotateZ(mRotation.y, mRotation.z,
	                               mSphereLink->mRevisionRate, &revisionX,
	                               &revisionZ);
	mHeadPos.x += revisionX;
	mHeadPos.z += revisionZ;

	JGeometry::TVec3<f32> before = mHeadPos;
	mGroundHeight                = gpMap->checkGroundIgnoreWaterSurface(
	    mHeadPos.x, mHeadPos.y + mHeadHeight, mHeadPos.z, &mGroundPlane);
	mGroundHeight += 1.0f;

	if (mHeadPos.y <= 0.05f + mGroundHeight) {
		if (mGroundPlane != nullptr && mGroundPlane->isLegal()) {
			offLiveFlag(LIVE_FLAG_AIRBORNE);
			mVelocity.set(0.0f, 0.0f, 0.0f);
			mHeadPos.y = mGroundHeight;
		}
	} else {
		onLiveFlag(LIVE_FLAG_AIRBORNE);
	}

	gpMap->isTouchedOneWallAndMoveXZ(&mHeadPos.x, mHeadPos.y + mHeadHeight,
	                                 &mHeadPos.z, mBodyRadius);

	JGeometry::TVec3<f32> wallPush = mHeadPos - before;
	JGeometry::TVec3<f32> toNext   = nextPos - mPosition;
	mLinearVelocity                = toNext + wallPush;
}

void TBossHanachan::kill() { }

#pragma dont_inline on
static void CalcRevisionPosByRotateZ(const JGeometry::TVec3<f32>& rotation,
                                     f32 plusYRate, Vec* position)
{
	f32 absRotateZ = fabsf(rotation.z);
	position->y += plusYRate * absRotateZ;
	if (absRotateZ > 90.0f) {
		f32 shift = 7.0f * (absRotateZ - 90.0f);
		if (rotation.z > 0.0f)
			shift = -shift;

		s16 angle = CLBRoundf<s16>(DEG2SHORTANGLE(rotation.y));
		f32 sinY  = JMASSin(angle);
		f32 cosY  = JMASCos(angle);
		f32 x     = shift;
		f32 z     = 0.0f;
		position->x += x * cosY + z * sinY;
		position->z += -x * sinY + z * cosY;
	}
}
#pragma dont_inline off

void TBossHanachan::throwMario_(THitActor* thrower)
{
	JGeometry::TVec3<f32> toMario = *gpMarioPos;
	toMario -= thrower->mPosition;
	JGeometry::TVec3<f32> dir = toMario;

	f32 speed;
	if (dir.squared() <= 0.0000038146973f) {
		dir.set(0.0f, 1.0f, 0.0f);
		speed = mMarchSpeed * mChangeSaveParams->mSLThrowTotalPower.get();
	} else {
		s16 moveAngle = CLBRoundf<s16>(
		    DEG2SHORTANGLE(MsGetRotFromZaxisY(mLastLinearVelocity)));
		s16 dirAngle  = CLBRoundf<s16>(DEG2SHORTANGLE(MsGetRotFromZaxisY(dir)));
		s16 angleDiff    = moveAngle - dirAngle;
		int absAngleDiff = CLBAbs<int>(angleDiff);

		f32 power = mMarchSpeed * mChangeSaveParams->mSLThrowTotalPower.get();
		f32 ratio = 1.0f - (1.0f / 32768.0f) * absAngleDiff;
		JGeometry::TVec3<f32> push = mLastLinearVelocity;
		f32 pushScale = ratio * mChangeSaveParams->mSLThrowMoveDirPower.get();
		speed         = ratio * power;
		push.x *= pushScale;
		push.y *= pushScale;
		push.z *= pushScale;
		dir.x += push.x;
		dir.y += push.y;
		dir.z += push.z;
		dir.y = mChangeSaveParams->mSLThrowVecY.get();
	}

	f32 maxSpeed   = mChangeSaveParams->mSLThrowSpeedMax.get();
	f32 minSpeed   = mChangeSaveParams->mSLThrowSpeedMin.get();
	f32 throwSpeed = speed;
	if (speed > maxSpeed)
		throwSpeed = maxSpeed;
	else if (speed < minSpeed)
		throwSpeed = minSpeed;

	SMS_SendMessageToMario(mHeadPart, 0xE);
	SMS_SendMessageToMario(mHeadPart, 7);
	SMS_ThrowMario(dir, throwSpeed);
	mHeadPart->mHitActor->onWaterHitCounter();
}

void TBossHanachan::init(TLiveManager* manager)
{
	mManager = manager;
	manager->manageActor(this);
	mMActorKeeper    = new TMActorKeeper(manager, 10);
	mSandPillarActor = mMActorKeeper->createMActor(cSandPillarModelName, 0);

	TBossHanachanManager* hanaManager
	    = static_cast<TBossHanachanManager*>(manager);
	mCommonSaveParams = hanaManager->mCommonSaveParams;
	mChangeSaveParams = hanaManager->mChangeSaveParams[0];

	mBodyScale        = 1.0f;
	mBodyRadius       = 350.0f;
	mWallRadius       = mBodyRadius;
	mHeadHeight       = 500.0f;
	mMarchSpeed       = 0.0f;
	mGravity          = 2.0f;
	mHitPoints        = 3;
	mScaledBodyRadius = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK8 | LIVE_FLAG_UNK1000);

	mSpine->initWith(&TNerveBossHanachanGraphWander::theNerve());
	getTracer()->reset();
	goToShortestNextGraphNode();
	initHitActor(0x08000014, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	for (int i = 0; i < 8; ++i) {
		mBodyParts[i] = new TBossHanachanPartsBody(this, "ボスハナチャンの体");
		mBodyParts[i]->mIndex = i;
	}
	mHeadPart = new TBossHanachanPartsHead(this, "ボスハナチャンの頭");

	mMActor  = mHeadPart->mMActor;
	mHeadPos = mPosition;

	JGeometry::TVec3<f32> linkPos = mHeadPos;
	linkPos.x -= JMASSin(DEG2SHORTANGLE(mRotation.y))
	             * mCommonSaveParams->mSLHeadLength.get();
	linkPos.z -= JMASCos(DEG2SHORTANGLE(mRotation.y))
	             * mCommonSaveParams->mSLHeadLength.get();
	mSphereLink = new TSphereLink(
	    8, JGeometry::TVec3<f32>(linkPos.x, linkPos.y, linkPos.z),
	    mCommonSaveParams->mSLBodyLength.get(),
	    mCommonSaveParams->mSLBodyAttackRadius.get(), 0.2f, -2.0f, -3.5f,
	    mRotation.y);

	mHeadPart->mPosition    = mPosition;
	mHeadPart->mRotation    = mRotation;
	mHeadPart->mGroundPlane = mGroundPlane;

	for (int i = 0; i < 8; ++i) {
		TBossHanachanPartsBody* body = mBodyParts[i];
		body->mPosition              = mSphereLink->mPoints[i].mPos;
		body->mLastPos               = body->mPosition;
		body->mLastLastPos           = body->mLastPos;
		body->mRotation              = mRotation;
	}

	setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK, BOSSHANACHAN_BLEND_OFF);

	JGeometry::TVec3<f32> headPos = mPosition;
	CalcRevisionPosByRotateZ(
	    mRotation, mCommonSaveParams->mSLHeadPlusYByRotateZ.get(), &headPos);
	CLBCalcRotateZXYTranslateMatrix(
	    mHeadPart->mMActor->getModel()->getBaseTRMtx(), mRotation, headPos);
	mHeadPart->mMActor->calc();

	for (int i = 0; i < 8; ++i) {
		TBossHanachanPartsBody* body  = mBodyParts[i];
		JGeometry::TVec3<f32> bodyPos = body->mPosition;
		CalcRevisionPosByRotateZ(body->mRotation,
		                         mCommonSaveParams->mSLBodyPlusYByRotateZ.get(),
		                         &bodyPos);
		Mtx mtx;
		CLBCalcRotateZXYTranslateMatrix(mtx, body->mRotation, bodyPos);
		MTXCopy(mtx, body->mMActor->getModel()->getBaseTRMtx());
		body->mMActor->calc();
	}

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	mHeadPart->initMapCollisionAndHitActor_(group);
	for (int i = 0; i < 8; ++i) {
		mBodyParts[i]->initMapCollisionAndHitActor_(group);
		mBodyParts[i]->initFootHitActor_(group);
	}
}

void TBossHanachan::setRandomWeakBodyIndex()
{
	mWeakBodyIndex = 8.0f * MsRandF();
}

TBossHanachan::TBossHanachan(const char* name)
    : TSpineEnemy(name)
    , mWeakBodyIndex(0)
    , mSphereLink(nullptr)
    , mHeadPos(0.0f, 0.0f, 0.0f)
    , mLastLinearVelocity(0.0f, 0.0f, 0.0f)
    , mTumbleTargetAngleZ(0.0f)
    , mTumbleAngularSpeedZ(0.0f)
    , mSandPillarActor(nullptr)
    , mSandPillarPos(0.0f, 0.0f, 0.0f)
    , mDeadMarioPos(0.0f, 0.0f, 0.0f)
    , mHintBalloonTimer(-1)
    , mCommonSaveParams(nullptr)
    , mChangeSaveParams(nullptr)
{
	mWeakBodyIndex = 8.0f * MsRandF();
}

BOOL TBossHanachanManager::hasMapCollision() const { return TRUE; }

void TBossHanachanManager::clipEnemies(JDrama::TGraphics* graphics)
{
	clipActorsAux(graphics, mCommonSaveParams->mSLViewClipFar.get(),
	              mCommonSaveParams->mSLViewClipRadius.get());
}

void TBossHanachanManager::loadAfter()
{
	J3DMaterialTable* table = gpMapObjManager->unkC0;
	s32 textureIndex = table->getTextureName()->getIndex(cSandTextureName);
	const ResTIMG* sandTexture = table->getTexture()->getResTIMG(textureIndex);
	for (int i = 0; i < 2; ++i) {
		SMS_ChangeTextureAll(
		    getModelDataKeeper()->getNthData(i)->getModelData(),
		    cDummyTextureName, *sandTexture);
	}
}

void TBossHanachanManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "hanabody_model.bmd", 0x10300000, 0 },
		{ "hanahead_model.bmd", 0x10100000, 0 },
		{ cSandPillarModelName, 0x10010000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TBossHanachanManager::TBossHanachanManager(const char* name)
    : TEnemyManager(name)
{
	static const char* sCommonSaveFileName = "/enemy/bosshanachanCommon.prm";
	mCommonSaveParams = new TBossHanachanCommonSaveParams(sCommonSaveFileName);

	static const char* sChangeSaveFileName[3] = {
		"/enemy/bosshanachan0.prm",
		"/enemy/bosshanachan1.prm",
		"/enemy/bosshanachan2.prm",
	};
	for (int i = 0; i < 3; ++i)
		mChangeSaveParams[i]
		    = new TBossHanachanChangeSaveParams(sChangeSaveFileName[i]);
}
