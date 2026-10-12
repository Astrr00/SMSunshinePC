#include <Camera/Camera.hpp>
#include <Camera/CameraKindParam.hpp>
#include <Camera/CameraMarioData.hpp>
#include <Camera/camerasave.hpp>
#include <Camera/cameralib.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/LiveManager.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/WaterGun.hpp>
#include <System/MarioGamePad.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <Enemy/Enemy.hpp>

static const char* dummyMactorStringValue1 = "\0\0\0\0\0\0\0\0\0\0\0";
static const char* SMS_NO_MEMORY_MESSAGE   = "メモリが足りません\n";

static const char* sNoticeActorManagerName[] = {
	"ヒノクリマネージャー",
	"ヒノクリ２マネージャー",
	"ボスパックンマネージャー",
	nullptr,
};

const char* bossGesoViewObjName = "ボスゲッソー";

void CPolarSubCamera::setNoticeInfo()
{
	unk2A0       = new TLiveActor*[0x10];
	mNoticeActor = nullptr;
	unk29C       = 0;

	for (int i = 0; sNoticeActorManagerName[i] != nullptr; ++i) {
		TLiveManager* mgr
		    = gpConductor->getManagerByName(sNoticeActorManagerName[i]);

		if (mgr != nullptr) {
			s32 e = mgr->getObjNum();
			for (int j = 0; j < e; ++unk29C, ++j)
				unk2A0[unk29C] = mgr->getObj(j);
		}
	}

	unk2A8 = static_cast<TLiveActor*>(
	    JDrama::TNameRefGen::search(bossGesoViewObjName));
}

TLiveActor* CPolarSubCamera::getNoticeActor_()
{
	char trash[0x28];
	if (mNoticeActor != nullptr && !mNoticeActor->checkLiveFlag(LIVE_FLAG_DEAD)
	    && !mNoticeActor->checkLiveFlag(LIVE_FLAG_HIDDEN)) {

		f32 dxA = mNoticeActor->mPosition.x - gpMarioPos->x;
		f32 dyA = mNoticeActor->mPosition.y - gpMarioPos->y;
		f32 dzA = mNoticeActor->mPosition.z - gpMarioPos->z;
		f32 sxA = dxA * dxA;
		f32 syA = dyA * dyA;
		f32 szA = dzA * dzA;
		f32 dist2 = sxA + syA + szA;
		if (dist2 < CLBSquared<f32>(mSaveNotice->mOffDist.get())) {
			f32 clipMax = mSaveNotice->mOffClipRatio.get();
			JGeometry::TVec2<f32> clipPos;
			CLBCalc2DFPos(&clipPos, unk16C, unk1EC, mNoticeActor->mPosition,
			              nullptr, false);

			// asm 0x80359438: retail keeps three bools plus a ternary result
			f32 clipMin   = -clipMax;
			bool inClipY2 = false;
			bool inClipY1 = false;
			bool inClipX  = false;
			if (clipMin <= clipPos.x && clipPos.x <= clipMax)
				inClipX = true;

			if (inClipX && clipMin <= clipPos.y)
				inClipY1 = true;

			if (inClipY1 && clipPos.y <= clipMax)
				inClipY2 = true;

			bool inClip = inClipY2 ? true : false;
			if (inClip)
				return mNoticeActor;
		}
	}

	TLiveActor* noticeActor = nullptr;
	f32 closestDist2        = CLBSquared<f32>(mSaveNotice->mOnDist.get());

	for (int i = 0; i < unk29C; i++) {
		if (unk2A0[i]->checkLiveFlag(LIVE_FLAG_DEAD)
		    || unk2A0[i]->checkLiveFlag(LIVE_FLAG_HIDDEN))
			continue;

		if (mNoticeActor != nullptr && mNoticeActor == unk2A0[i])
			continue;

		f32 dxB = unk2A0[i]->mPosition.x - gpMarioPos->x;
		f32 dyB = unk2A0[i]->mPosition.y - gpMarioPos->y;
		f32 dzB = unk2A0[i]->mPosition.z - gpMarioPos->z;
		f32 sxB = dxB * dxB;
		f32 syB = dyB * dyB;
		f32 szB = dzB * dzB;
		f32 dist2 = sxB + syB + szB;
		if (!(dist2 < closestDist2))
			continue;

		f32 clipMax = mSaveNotice->mOnClipRatio.get();
		JGeometry::TVec2<f32> clipPos;
		CLBCalc2DFPos(&clipPos, unk16C, unk1EC, unk2A0[i]->mPosition, nullptr,
		              false);

		// asm 0x803595E0: retail keeps three bools plus a ternary result
		f32 clipMin   = -clipMax;
		bool inClipX  = false;
		bool inClipY1 = false;
		bool inClipY2 = false;
		if (clipMin <= clipPos.x && clipPos.x <= clipMax) {
			inClipX = true;
		}
		if (inClipX && clipMin <= clipPos.y) {
			inClipY1 = true;
		}
		if (inClipY1 && clipPos.y <= clipMax) {
			inClipY2 = true;
		}
		bool inClip = inClipY2 ? true : false;
		if (!inClip)
			continue;

		if (!MsIsInSight(*gpMarioPos, SHORTANGLE2DEG(*gpMarioAngleY),
		                 unk2A0[i]->mPosition, dist2,
		                 mSaveNotice->mOnDegree.get(), -1.0f))
			continue;

		closestDist2 = dist2;
		noticeActor  = unk2A0[i];
	}

	return noticeActor;
}

void CPolarSubCamera::execNoticeOnOffProc_(EnumNoticeOnOffMode mode)
{
	switch (mode) {
	case NOTICE_MODE_UNK0:
		mNoticeActor = nullptr;
		unk64 &= ~CAMERA_FLAG_NOTICE_ACTIVE;
		break;
	case NOTICE_MODE_UNK1: {
		TLiveActor* actor = getNoticeActor_();
		if (actor != mNoticeActor && actor == nullptr) {
			mNoticeActor = nullptr;
			unk64 &= ~CAMERA_FLAG_NOTICE_ACTIVE;
		}
		break;
	}
	case NOTICE_MODE_UNK2: {
		TLiveActor* actor = getNoticeActor_();
		if (actor != mNoticeActor && actor != nullptr) {
			mNoticeActor = actor;
			unk64 |= CAMERA_FLAG_NOTICE_ACTIVE;
		}
		break;
	}
	}
}

void CPolarSubCamera::calcNoticeTargetYrot_(const Vec& target)
{
	Vec mPos     = gpCameraMario->unk0;
	char trash[0x20];
	f32 dz2      = CLBSquared<f32>(mPos.z - target.z);
	f32 dx2      = CLBSquared<f32>(mPos.x - target.x);
	f32 dist2    = dx2 + dz2;
	f32 farClip2 = CLBSquared<f32>(mSaveNotice->mRotateMinDistXZ.get());
	f32 near2    = CLBSquared<f32>(mSaveNotice->mRotateFastMinDistXZ.get());

	if (dist2 > farClip2) {
		JGeometry::TVec3<f32> diff(mPos.x - target.x, mPos.y - target.y,
		                           mPos.z - target.z);
		MsVECNormalize(&diff, &diff);
		// TODO: many inlines from cameralib maybe?
		f32 dx       = diff.x * 500.0f + mPos.x;
		f32 dz       = diff.z * 500.0f + mPos.z;
		s16 ang      = matan(dz - mCurrentTarget.mTarget.z,
		                     dx - mCurrentTarget.mTarget.x);
		s16 yawDiff  = mCurrentTarget.mYaw - ang;
		int absAngle = yawDiff >= 0 ? yawDiff : -yawDiff;
		f32 ratio    = (f32)absAngle * (1.0f / 32768.0f);

		f32 chase;
		if (dist2 > near2) {
			chase = 1.0f;
		} else {
			chase = CLBCalcRatio<f32>(farClip2, near2, dist2);
		}
		f32 base = CLBLinearInbetween<f32>(
		    1.0f, mSaveNotice->mRotateMagnifXmax.get(), mCurrentTarget.unk28);
		f32 speed
		    = unk288
		      * (base
		         * (chase * (ratio * (f32)mSaveNotice->mRotateYSpeed.get())));
		if (speed > 32766.998f)
			speed = 32766.998f;
		s16 delta = CLBRoundf<s16>(speed);
		CLBChaseGeneralConstantSpecifySpeed<s16>(&mCurrentTarget.mYaw, ang,
		                                         delta);
	}
}

void CPolarSubCamera::getNozzleTopPos_(JGeometry::TVec3<f32>* out) const
{
	char trash[0x18];
	if (SMS_GetMarioWaterGun() == nullptr) {
		out->set(gpCameraMario->unk0);
	} else {
		MtxPtr mtx = SMS_GetMarioWaterGun()->getNozzleMtx();
		out->x     = mtx[0][3];
		out->y     = mtx[1][3];
		out->z     = mtx[2][3];

		JGeometry::TVec3<f32> dir(mtx[0][1], mtx[1][1], mtx[2][1]);
		dir.normalize();
		dir *= 30.0f;
		*out += dir;
	}
}

void CPolarSubCamera::ctrlLButtonCamera_()
{
	f32 stickX = -unk120->mCompSPos[4];
	f32 stickY = -unk120->mCompSPos[5];
	char trash[0x30];

	if (mTargetFreezeFrames == 0) {
		if (!SMS_CheckMarioFlag(MARIO_FLAG_HAS_FLUDD))
			mCurrentTarget.mTarget.set(gpCameraMario->unk0);
		else
			getNozzleTopPos_(&mCurrentTarget.mTarget);
	}

	if (mPosFreezeFrames == 0) {
		if (unk64 & CAMERA_FLAG_NOTICE_ACTIVE) {
			if (stickX != 0.0f) {
				rotateY_ByStickX_(stickX);
			} else {
				calcNoticeTargetYrot_(mNoticeActor->mPosition);
			}
		} else {
			rotateY_ByStickX_(stickX);
		}
		rotateX_ByStickY_(stickY);
	}

	calcPosAndAt_();
}
