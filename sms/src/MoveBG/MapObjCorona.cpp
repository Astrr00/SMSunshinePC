#include <MoveBG/MapObjCorona.hpp>

#include <stdio.h>
#include <math.h>

#include <Camera/CameraShake.hpp>
#include <Enemy/BathtubKiller.hpp>
#include <Enemy/Koopa.hpp>
#include <GC2D/GCConsole2.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/Mario.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/NozzleTrigger.hpp>
#include <Player/WaterGun.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
TBathtubParams::TBathtubParams()
    : TParams("/MapObj/bathtub.prm")
    , PARAM_INIT(resetGrip, 0)
    , PARAM_INIT(trampleRelease, 10)
    , PARAM_INIT(trampleRecover, 10)
    , PARAM_INIT(quakeRelease, 500)
    , PARAM_INIT(quakeRecover, 500)
    , PARAM_INIT(hipdropRelease, 35)
    , PARAM_INIT(hipdropRecover, 35)
    , PARAM_INIT(breakCount0, 750)
    , PARAM_INIT(breakCount1, 710)
    , PARAM_INIT(breakCount2, 685)
    , PARAM_INIT(breakCount3, 655)
    , PARAM_INIT(launchStopCount, 1000)
    , PARAM_INIT(animSpeed0, 0.15f)
    , PARAM_INIT(animSpeed1, 0.15f)
    , PARAM_INIT(animSpeed2, 0.15f)
    , PARAM_INIT(animSpeed3, 0.15f)
    , PARAM_INIT(animSpeed4, 0.22f)
    , PARAM_INIT(shake, 0.0f)
    , PARAM_INIT(watermark, 0.3f)
    , PARAM_INIT(maxAngle, 35.0f)
    , PARAM_INIT(angleVelDamp, 0.93f)
    , PARAM_INIT(rebound, 0.0005f)
    , PARAM_INIT(shakeDamp, 0.93f)
    , PARAM_INIT(marioWeight, 0.01f)
    , PARAM_INIT(marioDropWeight, 5.0f)
    , PARAM_INIT(outerHeight, 20.0f)
{
	TParams::load(mPrmPath);
}

static inline TBathtubRot& bathRot(TBathtubData& data)
{
	return (TBathtubRot&)data.unk18;
}

u8 TBathtub::getNumKillerBurstable() const
{
	if (!isKillerLaunchable())
		return 0;
	int n = getNumGripsDead();
	if (n >= 4)
		return 8;
	if (!allowsTumble() && unk250 == 0 && unk258 == 0) {
		switch (n) {
		case 1:
			return 4;
		case 2:
			return 6;
		case 3:
			return 8;
		case 4:
			return 8;
		}
	}
	return 0;
}

bool TBathtub::isKillerAttackable() const
{
	return unk248 <= 0;
}

bool TBathtub::isKillerLaunchable() const
{
	if (unk29A)
		return false;
	TKoopa* koopa = (TKoopa*)JDrama::TNameRefGen::search("クッパ");
	if (!koopa->allowsLaunch())
		return false;
	return isKillerAttackable();
}

int TBathtub::getNumKillerLaunchable() const
{
	if (!isKillerLaunchable())
		return 0;

	int num = getNumGripsDead() + 1;
	if (num < 2)
		num = 2;
	if (num > 4)
		num = 4;
	return num;
}

TBathtub::TBathtub(const char* name)
    : TMapObjBase(name)
    , unk164(nullptr)
    , unk290(0)
{
	unk16C = new TBathtubParams;
	mQuat.z = mQuat.y = mQuat.x = 0.0f;
	mQuat.w                     = 1.0f;
	mPosition.zero();
	mAngularVelocity.zero();
	unk250 = 0;
	unk254 = 1;
	unk258 = 0;
	unk25C = 1;
	unk248 = 0;
	unk298 = 0;
	unk23C = unk240 = unk244 = 0.0f;
	unk299                   = 0;
	unk29A                   = 0;
	unk2A0                   = 0;
	unk294                   = 0;
}

void TBathtub::load(JSUMemoryInputStream& stream)
{
	char trash[8];
	unk24C = 0;
	TMapObjBase::load(stream);
	mPosition.set(mInitialPosition);

	unk164 = new TMapCollisionMove*[30];
	for (int i = 0; i < 30; ++i) {
		unk164[i]        = new TMapCollisionMove;
		const char* path = nullptr;
		switch (i % 6) {
		case 0:
			path = "/scene/mapObj/bath_col_inside3.col";
			break;
		case 1:
			path = "/scene/mapObj/bath_col_inside2.col";
			break;
		case 2:
			path = "/scene/mapObj/bath_col_inside1.col";
			break;
		case 3:
			path = "/scene/mapObj/bath_col_inside6.col";
			break;
		case 4:
			path = "/scene/mapObj/bath_col_inside5.col";
			break;
		case 5:
			path = "/scene/mapObj/bath_col_inside4.col";
			break;
		}
		unk164[i]->init(path, 0, this);
		unk164[i]->setUp();
	}

	mBathtubData.mPos.set(mInitialPosition);
	bathRot(mBathtubData).identity();
	mBathtubData.unk3C = 3000.0f;
	mBathtubData.unk40 = 3600.0f;
	mBathtubData.unk44 = mBathtubData.unk3C * sinf(0.27925268f);
	mBathtubData.unk58.zero();
	mBathtubData.unk4C = 0.0f;
	mBathtubData.unk50 = 0.0f;
	mBathtubData.unk54 = 0.0f;
	mBathtubData.unk48 = 100.0f;
	mBathtubData.unk64 = 0;
	mBathtubData.unk0C.set(0.0f, 1.0f, 0.0f);

	unk168 = new TBathtubGrip*[5];
	unk138 = new MActorAnmData;
	unk138->init("scene/map/map/stand_effect", nullptr);
	for (int i = 0; i < 5; ++i) {
		f32 angle = 360.0f * (0.5f + i) / 5.0f;
		f32 ctorAngle = angle - 180.0f;
		unk168[i % 5] = new TBathtubGrip(this, ctorAngle, unk138, "壊れかけのバスタブの取っ手");
		unk168[i % 5]->appear();
		unk13C[i] = -180.0f + angle;
		unk150[i] = -180.0f + 360.0f * i / 5.0f;
	}

	JUTNameTab* names = getModel()->getModelData()->getJointName();
	mMarioJntIdx      = names->getIndex("mario");
	mStarJntIdx       = names->getIndex("star");
	mWater4JntIdx     = names->getIndex("water4");
	mWater5JntIdx     = names->getIndex("water5");
	mWater1JntIdx     = names->getIndex("water1");
	mWater2JntIdx     = names->getIndex("water2");
	mWater3JntIdx     = names->getIndex("water3");
	mDuckJntIdx       = names->getIndex("ahiru");
	mSubmarineJntIdx  = names->getIndex("submarin");
	mJuniorJntIdx     = names->getIndex("Jr");
	mKoopaJntIdx      = names->getIndex("koopa");

	MActorAnmData* shineAnm = new MActorAnmData;
	shineAnm->init("/scene/map/map/shine", nullptr);
	unk29C = new MActor(shineAnm);
	void* res = JKRFileLoader::getGlbResource("/scene/map/map/shine/shine_3bai.bmd");
	J3DModel* model = new J3DModel(J3DModelLoaderDataBase::load(res, 0x10000000), 0, 1);
	unk29C->setModel(model, 0x10000000);
	mShineBodyJntIdx = unk29C->getModel()->getModelData()->getJointName()->getIndex("body");
	unk298 = 1;
}

static inline void liftMario(JGeometry::TVec3<f32>& angVel, const JGeometry::TVec3<f32>& marioPos,
                             const JGeometry::TVec3<f32>& pos, f32 weight)
{
	static JGeometry::TVec3<f32> yDown(0.0f, -1.0f, 0.0f);
	JGeometry::TVec3<f32> lever;
	lever.sub(marioPos, pos);
	JGeometry::TVec3<f32> torque;
	torque.cross(lever, yDown);
	torque.scale(0.00000001f * weight);
	angVel.add(torque);
}

void TBathtub::updatePosture_()
{
	static JGeometry::TVec3<f32> y(0.0f, 1.0f, 0.0f);

	if (unk250 == 0) {
		f32 rate;
		if (unk258 == 0) {
			rate = 1.0f;
		} else {
			unk258--;
			rate = 1.0f - (f32)unk258 / (f32)unk25C;
		}
		JGeometry::TVec3<f32> up;
		mQuat.getYDir(up);
		JGeometry::TVec3<f32> axis;
		axis.cross(y, up);
		axis.normalize();
		axis.scale(unk16C->rebound.get() * (rate * -acosf(y.dot(up))));
		mAngularVelocity.scaleAdd(unk16C->angleVelDamp.get(), axis,
		                          mAngularVelocity);
	} else {
		unk250--;
	}

	JGeometry::TQuat4<f32> dq;
	dq.xyz().scale(0.5f, mAngularVelocity);
	dq.w = 0.0f;
	dq.mul(dq, mQuat);
	mQuat.x += dq.x;
	mQuat.y += dq.y;
	mQuat.z += dq.z;
	mQuat.w += dq.w;
	mQuat.normalize();

	JGeometry::TVec3<f32> up;
	mQuat.getYDir(up);
	f32 angle    = acosf(y.dot(up));
	f32 maxAngle = DEG_TO_RAD(unk16C->maxAngle.get());
	f32 excess   = angle - maxAngle;
	if (excess > 0.0f) {
		JGeometry::TQuat4<f32> limit;
		limit.setRotate(up, y, excess / angle);
		mQuat.mul(limit, mQuat);
	}
	mQuat.normalize();
}

static f32 getDir(MtxPtr mtx, const JGeometry::TVec3<f32>& pos)
{
	JGeometry::TVec3<f32> xDir(mtx[0][0], mtx[1][0], mtx[2][0]);
	JGeometry::TVec3<f32> zDir(mtx[0][2], mtx[1][2], mtx[2][2]);
	JGeometry::TVec3<f32> trans(mtx[0][3], mtx[1][3], mtx[2][3]);
	JGeometry::TVec3<f32> delta;
	delta.sub(pos, trans);
	f32 localZ = zDir.dot(delta);
	f32 localX = xDir.dot(delta);
	return (360.0f / 65536.0f) * matan(localZ, localX);
}

static f32 getDir(MtxPtr mtx, const JGeometry::TVec3<f32>& pos,
                  const JGeometry::TVec3<f32>& direction)
{
	JGeometry::TVec3<f32> xDir(mtx[0][0], mtx[1][0], mtx[2][0]);
	JGeometry::TVec3<f32> zDir(mtx[0][2], mtx[1][2], mtx[2][2]);
	JGeometry::TVec3<f32> trans(mtx[0][3], mtx[1][3], mtx[2][3]);
	JGeometry::TVec3<f32> delta;
	delta.sub(pos, trans);
	JGeometry::TVec3<f32> normal;
	normal.normalize(delta);
	JGeometry::TVec3<f32> tangent;
	tangent.scaleAdd(-normal.dot(direction), direction, normal);
	delta += tangent;
	f32 localZ = zDir.dot(delta);
	f32 localX = xDir.dot(delta);
	return (360.0f / 65536.0f) * matan(localZ, localX);
}

// Retail calls fmodf out of line; keep the MSL inline version out of the grip
// loops so mwcc emits the same bl.
#pragma dont_inline on
static f32 fmodDeg(f32 x, f32 y) { return std::fmodf(x, y); }
#pragma dont_inline off

#pragma dont_inline on
bool TBathtub::getNearGrip(const JGeometry::TVec3<f32>& pos, f32 maxDiff,
                           f32* out) const
{
	char trash[0x30];
	f32 angle = getDir(*getRootJointMtx(), pos);

	f32 best  = 360.0f;
	int index = 0;
	for (int i = 0; i < 5; ++i) {
		f32 diff = fabsf(
		    -180.0f
		    + fmodDeg(360.0f + (unk150[i] - angle - -180.0f), 360.0f));
		if (diff < best) {
			index = i;
			best  = diff;
		}
	}

	if (best < maxDiff) {
		*out = unk150[index];
		return true;
	}
	return false;
}
#pragma dont_inline off

f32 TBathtub::getNextJuncture(const JGeometry::TVec3<f32>& pos,
                              const JGeometry::TVec3<f32>& direction) const
{
	char trash[0x28];
	f32 angle = getDir(*getRootJointMtx(), pos, direction);
	f32 best  = 360.0f;
	int index = 0;
	for (int i = 0; i < 5; ++i) {
		f32 diff = fabsf(
		    -180.0f
		    + fmodDeg(360.0f + (unk13C[i] - angle - -180.0f), 360.0f));
		if (diff < best) {
			index = i;
			best  = diff;
		}
	}
	return unk13C[index];
}

bool TBathtub::getNextGrip(const JGeometry::TVec3<f32>& pos,
                           const JGeometry::TVec3<f32>& direction, f32 maxDiff,
                           f32* out) const
{
	char trash[0x28];
	f32 angle = getDir(*getRootJointMtx(), pos, direction);
	f32 best  = 360.0f;
	int index = 0;
	for (int i = 0; i < 5; ++i) {
		f32 diff = fabsf(
		    -180.0f
		    + fmodDeg(360.0f + (unk150[i] - angle - -180.0f), 360.0f));
		if (diff < best) {
			index = i;
			best  = diff;
		}
	}
	if (best < maxDiff) {
		*out = unk150[index];
		return true;
	}
	return false;
}

void TBathtub::calcRootMatrix()
{
	if (unk29A) {
		MtxPtr mtx = getModel()->getBaseTRMtx();
		MsMtxSetRotRPH(mtx, 0.0f, mRotation.y, 0.0f);
		mtx[0][3] = mPosition.x;
		mtx[1][3] = mPosition.y;
		mtx[2][3] = mPosition.z;
	} else {
		TPosition3f* mtx = (TPosition3f*)getModel()->getBaseTRMtx();
		mtx->setQuat(mQuat);
		mtx->setTrans(mPosition);
	}
}

bool TBathtub::allowsTumble() const
{
	JGeometry::TVec3<f32> pos = *gpMarioPos;
	f32 angle;
	if (!getNearGrip(pos, 18.0f, &angle))
		return false;

	JGeometry::TVec3<f32> d(pos.x - mBathtubData.mPos.x,
	                        pos.y - mBathtubData.mPos.y,
	                        pos.z - mBathtubData.mPos.z);
	JGeometry::TVec3<f32> local;
	mBathtubData.unk18.mult33(d, local);
	JGeometry::TVec3<f32> flat = local;
	flat.y                     = 0.0f;
	f32 dist                   = flat.length();
	if (dist < 4200.0f)
		return false;
	if (!(4700.0f < dist))
		return true;
	if (!(mBathtubData.unk18.at(1, 1) > 0.99f))
		return false;

	TBathtubKillerManager* killers
	    = (TBathtubKillerManager*)JDrama::TNameRefGen::search(
	        "バスタブキラーマネージャー");
	u32 status = SMS_GetMarioStatus();
	if (status == MARIO_STATUS_HIP_DROP)
		return false;
	if (status == MARIO_STATUS_ROCKET)
		return false;
	if (status == MARIO_STATUS_ROCKET_LANDING)
		return false;

	const TWaterGun* gun = gpMarioOriginal->mWaterGun;
	if (gun) {
		TNozzleBase* nozzle = gun->getCurrentNozzle();
		if (nozzle) {
			if (nozzle->getNozzleKind() == 1
			    && ((TNozzleTrigger*)nozzle)->unk388 > 0.0f)
				return false;
		}
	}
	return killers->countActiveKillers() == 0;
}

void TBathtub::startDemo()
{
	char trash[0x40];
	char trash2[8];
	if (unk29A)
		return;

	MSBgm::stopTrackBGMs(7, 10);
	if (!(unk2A0 & 0x20))
		gpMarDirector->mConsole->startAppearBalloon(0xE0023, true);
	unk2A0 |= 0x20;
	unk290 = 10;

	for (int i = 0; i < 5; ++i)
		unk168[i]->kill();

	TBathtubGrip* grip = unk168[2];
	grip->offLiveFlag(LIVE_FLAG_DEAD);
	grip->unk248 = 0;
	grip->unk24A = 0;
	grip->unk249 = 1;
	grip->unk24B = 0;
	grip->startAnim(0);
	J3DFrameCtrl* ctrl = grip->mMActor->getFrameCtrl(0);
	if (ctrl) {
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}
	grip->unk250 = 1.0f;
	grip->unk258 = 100;
	grip->unk260 = 0;

	grip->unk258 = 2;
	grip->unk250 = unk16C->animSpeed1.get();
	grip->unk254 = 1;
	grip->unk25C->setBck("stand_effect");
	grip->unk25C->setBtk("stand_effect");
	grip->unk25C->setBrk("stand_effect");
	ctrl = grip->unk25C->getFrameCtrl(0);
	if (ctrl)
		ctrl->setRate(0.5f * (12.0f * SMSGetAnmFrameRate()));
	grip->unk248 = 1;
	grip->unk249 = 0;
	grip->startAnim(1);

	onMapObjFlag(MAP_OBJ_FLAG_UNK8);
	offMapObjFlag(MAP_OBJ_FLAG_UNK100);
	startBck("bath_overturn1");

	TLiveActor* mario = SMS_GetMarioLiveActor();
	if (mario->receiveMessage(this, HIT_MESSAGE_TAKE))
		mHeldObject = mario;

	gpMarioOriginal->mFaceAngle.y = 0x7FFF;
	gpMarDirector->fireStartDemoCamera("koopa_last2", &mPosition, -1,
	                                   mRotation.y, false, nullptr, 0, nullptr,
	                                   JDrama::TFlagT<u16>(0));
	gpMarDirector->fireStreamingMovie(0xE);
	((TKoopa*)JDrama::TNameRefGen::search("クッパ"))->fall();
	unk29A = 1;
}

void TBathtub::setupCollisions_()
{
	JGeometry::TVec3<f32> d(gpMarioPos->x - mBathtubData.mPos.x,
	                        gpMarioPos->y - mBathtubData.mPos.y,
	                        gpMarioPos->z - mBathtubData.mPos.z);
	JGeometry::TVec3<f32> local;
	mBathtubData.unk18.mult33(d, local);

	if (gpMarioPos->y < 300.0f
	    || local.x * local.x + local.z * local.z < 0.01f) {
		removeCollisions_();
		for (int i = 0; i < 5; ++i)
			unk168[i]->unk24B = 0;
		return;
	}

	f32 angle = atan2f(local.x, local.z);
	if (angle < 0.0f)
		angle += 6.2831855f;

	f32 seg = angle * 4.774648f;
	if (seg < 0.0f)
		seg += 30.0f;

	int base = ((int)(seg + 0.5f - 1.0f) + 30) % 30;
	int i    = 0;
	for (; i < 2; ++i) {
		int j = (i + base) % 30;
		f32 a = (f32)(j + 1) * 6.2831855f / 30.0f - 3.1415927f;
		TPosition3f rotY;
		rotY.identity();
		rotY.setEularY(a);
		TMtx34f result;
		result.concat(*(TSMtx34f*)getModel()->getBaseTRMtx(),
		              rotY);
		unk164[j]->moveMtx(result);
		unk164[j]->setUp();
	}
	for (; i < 30; ++i)
		unk164[(i + base) % 30]->remove();

	for (int k = 0; k < 5; ++k)
		unk168[k]->unk24B = 0;
	unk168[((int)(5.0f * angle / 6.2831855f) + 10) % 5]->unk24B = 1;
}

void TBathtub::removeCollisions_()
{
	for (int i = 0; i < 30; ++i)
		unk164[i]->remove();
}

void TBathtub::calcBathtubData()
{
	Mtx* m = getRootJointMtx();
	TBathtubRot& rot = bathRot(mBathtubData);
	rot.setXDir((*m)[0][0], (*m)[1][0], (*m)[2][0]);
	rot.setYDir((*m)[0][1], (*m)[1][1], (*m)[2][1]);
	rot.setZDir((*m)[0][2], (*m)[1][2], (*m)[2][2]);
	mBathtubData.mPos.set((*m)[0][3], (*m)[1][3], (*m)[2][3]);

	f32 y = rot.at(1, 1);
	f32 s = JGeometry::TUtil<f32>::sqrt(1.0f - y * y);
	mBathtubData.unk44
	    = mBathtubData.unk3C
	      * JGeometry::max(unk16C->watermark.get(), s);
	mBathtubData.unk48 = unk16C->outerHeight.get();
	rot.getYDir(mBathtubData.unk0C);
	mBathtubData.unk58.zero();

	TKoopa* koopa = (TKoopa*)JDrama::TNameRefGen::search("クッパ");
	if (koopa->effectsTumble() || unk24C > 0) {
		JGeometry::TVec3<f32> axis;
		axis.cross(JGeometry::TVec3<f32>(0.0f, 1.0f, 0.0f),
		           mBathtubData.unk0C);
		axis.normalize();
		if (!axis.isZero()) {
			JGeometry::TQuat4<f32> q;
			q.setRotate(axis, unk16C->maxAngle.get() * 6.2831855f / 360.0f);
			q.rotate(mBathtubData.unk0C, mBathtubData.unk0C);
			mBathtubData.unk58.set(0.0f, unk16C->shake.get(), 0.0f);
		}
	} else {
		mBathtubData.unk0C.set(0.0f, 1.0f, 0.0f);
	}

	mBathtubData.unk64 = unk250 < unk254 / 2 && unk258 > 0;
	mBathtubData.unk65 = unk29A;
	unk1F4             = mBathtubData.getThing();
}

void TBathtub::control()
{
	char trash[0x40];
	if (unk29A) {
		if (mMActor->curAnmEndsNext(0, nullptr)) {
			switch (unk294++) {
			case 0:
				startBck("bath_overturn2");
				break;
			case 1:
				startBck("bath_overturn3");
				break;
			}
		}

		JPABaseEmitter* emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x128, unk29C->getModel()->getAnmMtx(mShineBodyJntIdx), 1, this);
		if (emitter) {
			emitter->mGlobalDynamicsScale.setAll(3.0f);
			emitter->mGlobalParticleScale.setAll(3.0f);
		}

		emitter = gpMarioParticleManager->emitAndBindToMtxPtr(
		    0x129, unk29C->getModel()->getAnmMtx(mShineBodyJntIdx), 1, this);
		if (emitter) {
			emitter->mGlobalDynamicsScale.setAll(3.0f);
			emitter->mGlobalParticleScale.setAll(3.0f);
		}

		MtxPtr star = mMActor->getModel()->getAnmMtx(mStarJntIdx);
		unk200.set(star[0][3], star[1][3], star[2][3]);
		gpMSound->startSoundActor(0x81C1, &unk200, 0, nullptr, 0, 4);

		calcRootMatrix();
		calcBathtubData();
		removeCollisions_();

		TLiveActor* mario = SMS_GetMarioLiveActor();
		if (mario->receiveMessage(this, HIT_MESSAGE_TAKE))
			mHeldObject = mario;
		gpMarioOriginal->mFaceAngle.y = 0x7FFF;

		if (unk290 > 0)
			unk290--;
	} else {
		if (unk24C > 0)
			unk24C--;
		if (unk248 > 0)
			unk248--;

		if (unk250 > unk16C->hipdropRelease.get()
		    || (marioIsOn() && !mHeldObject)) {
			f32 weight = unk16C->marioWeight.get();
			if (unk250 > 0)
				weight += unk16C->marioDropWeight.get();
			if (!(weight <= 0.0000000001f)) {
				liftMario(mAngularVelocity, *gpMarioPos, mPosition, weight);
			}
		}

		updatePosture_();
		calcRootMatrix();
		TMapObjBase::control();
		calcBathtubData();
		getModel()->calc();

		JGeometry::TVec3<f32> upDir(mBathtubData.unk18.at(1, 0),
		                            mBathtubData.unk18.at(1, 1),
		                            mBathtubData.unk18.at(1, 2));
		if (mBathtubData.unk0C.dot(upDir) > 0.995f)
			gpMarioParticleManager->emit(0x1BE, &unk1F4, 1, this);

		for (int i = 0; i < 5; ++i) {
			if (unk168[i]->unk248) {
				MtxPtr mtx = mMActor->getModel()->getAnmMtx(
				    (&mWater4JntIdx)[i]);
				gpMarioParticleManager->emitAndBindToMtxPtr(0x1BF, mtx, 1,
				                                            mtx);
			}
		}

		if (!mHeldObject)
			setupCollisions_();
		else
			removeCollisions_();
	}
}

void TBathtub::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[0xc8];
	TMapObjBase::perform(cue, graphics);

	if (cue & 1) {
		MTXCopy(mMActor->getModel()->getAnmMtx(mStarJntIdx),
		        unk29C->getModel()->getBaseTRMtx());
		JGeometry::TVec3<f32> scale(3.0f, 3.0f, 3.0f);
		unk29C->getModel()->setBaseScale(scale);
	}

	if (cue & 1) {
		int ticks = gpMarDirector->mMoveTickCount;
		int dead = getNumGripsDead();

		switch (dead) {
		case 0:
			if (ticks >= 7200) {
				if (!(unk2A0 & 2))
					gpMarDirector->mConsole->startAppearBalloon(0xE001F, true);
				unk2A0 |= 2;
			} else if (ticks >= 3600) {
				if (!(unk2A0 & 1))
					gpMarDirector->mConsole->startAppearBalloon(0xE001E, true);
				unk2A0 |= 1;
			}
			break;
		case 1:
			if (!(unk2A0 & 4))
				gpMarDirector->mConsole->startAppearBalloon(0xE0020, true);
			unk2A0 |= 4;
			break;
		case 2:
			if (!(unk2A0 & 8))
				gpMarDirector->mConsole->startAppearBalloon(0xE0021, true);
			unk2A0 |= 8;
			break;
		case 3:
			if (!(unk2A0 & 0x10))
				gpMarDirector->mConsole->startAppearBalloon(0xE0022, true);
			unk2A0 |= 0x10;
			break;
		case 4:
			if (!(unk2A0 & 0x40000))
				gpMarDirector->mConsole->startAppearBalloon(0xE0030, true);
			unk2A0 |= 0x40000;
			break;
		case 5:
			if (!(unk2A0 & 0x20))
				gpMarDirector->mConsole->startAppearBalloon(0xE0023, true);
			unk2A0 |= 0x20;
			break;
		}
	}

	if (cue & 2)
		unk29C->calc();

	if (cue & 4)
		unk29C->viewCalc();

	if (cue & 0x200) {
		MtxPtr mtx = unk29C->getModel()->getBaseTRMtx();
		JGeometry::TVec3<f32> pos(mtx[0][3], mtx[1][3], mtx[2][3]);
		unk29C->setLightData(mGroundPlane, pos);
		unk29C->entry();
	}
}

Mtx* TBathtub::getRootJointMtx() const
{
	if (unk29A)
		return (Mtx*)getModel()->getAnmMtx(0);
	return (Mtx*)getModel()->getBaseTRMtx();
}

BOOL TBathtub::receiveMessage(THitActor* sender, u32 message)
{
	switch (message) {
	case HIT_MESSAGE_HIP_DROP:
		hipdrop(*gpMarioPos);
		return true;
	case HIT_MESSAGE_SUPER_HIP_DROP:
		hipdrop(*gpMarioPos);
		return true;
	case HIT_MESSAGE_TRAMPLE:
		if (!unk29A && unk250 <= unk16C->trampleRelease.get()) {
			unk250 = unk16C->trampleRelease.get();
			unk258 = unk16C->trampleRecover.get();
			unk25C = unk16C->trampleRecover.get();
			unk254 = unk16C->hipdropRelease.get();
		}
		return true;
	}
	return false;
}

MtxPtr TBathtub::getKoopaJrMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mJuniorJntIdx);
}

MtxPtr TBathtub::getPeachMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mDuckJntIdx);
}

MtxPtr TBathtub::getSubmarineMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mSubmarineJntIdx);
}

MtxPtr TBathtub::getTakingMtx()
{
	return mMActor->getModel()->getAnmMtx(mMarioJntIdx);
}

MtxPtr TBathtub::getKoopaMtxInDemo()
{
	return mMActor->getModel()->getAnmMtx(mKoopaJntIdx);
}

void TBathtub::tumble(f32 angle, f32 weight)
{
	if (unk29A != 0)
		return;

	s16 a = DEG2SHORTANGLE(angle);
	weight *= 0.0001f;

	JGeometry::TVec3<f32> push;
	push.x = weight * JMASCos(a);
	push.y = 0.0f;
	push.z = weight * -JMASSin(a);
	mAngularVelocity.add(push);
}

int TBathtub::getNumGripsDead() const
{
	int n = 0;
	for (int i = 0; i < 5; ++i) {
		if (!unk168[i]->unk249)
			n++;
	}
	return n;
}

void TBathtub::quake(const JGeometry::TVec3<f32>& pos)
{
	if (unk29A)
		return;

	JGeometry::TVec3<f32> dir(pos.x - mInitialPosition.x, 0.0f,
	                          pos.z - mInitialPosition.z);
	dir.normalize();

	unk24C = 300;
	unk250 = unk16C->quakeRelease.get();
	unk258 = unk16C->quakeRecover.get();
	unk25C = unk16C->quakeRecover.get();
	unk254 = unk16C->hipdropRelease.get();
	unk248 = unk16C->launchStopCount.get();

	TKoopa* koopa = (TKoopa*)JDrama::TNameRefGen::search("クッパ");
	gpCameraShake->startShake((EnumCamShakeMode)0x25, 1.0f);
	gpCameraShake->startShake((EnumCamShakeMode)0x26, 1.0f);
	SMSRumbleMgr->start(4, (f32*)nullptr);
	JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
	SMS_ThrowMario(up, 10.0f);
	koopa->getDown();
}

#pragma dont_inline on
void TBathtub::hipdrop(const JGeometry::TVec3<f32>& pos)
{
	if (unk29A)
		return;
	if (unk250 > unk16C->hipdropRelease.get())
		return;

	JGeometry::TVec3<f32> dir(pos.x - mInitialPosition.x, 0.0f,
	                          pos.z - mInitialPosition.z);
	dir.normalize();

	unk250 = unk16C->hipdropRelease.get();
	unk258 = unk16C->hipdropRecover.get();
	unk25C = unk16C->hipdropRecover.get();
	unk254 = unk16C->hipdropRelease.get();
	((TKoopa*)JDrama::TNameRefGen::search("クッパ"))->stagger(false);
}
#pragma dont_inline off

void TBathtub::loadAfter()
{
	SMS_LoadParticle("/scene/map/map/ms_lkp_yuge1.jpa", 0x1BE);
	SMS_LoadParticle("/scene/map/map/ms_kp_funsui.jpa", 0x1BF);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_a.jpa", 0xF6);
	SMS_LoadParticle("/scene/map/map/ms_kp_break_b.jpa", 0xF7);
}

void TBathtubGrip::control()
{
	char trash[0x20];
	calcRootMatrix();
	TMapObjBase::control();

	if (unk24A) {
		for (int i = 0; i < 17; ++i)
			mColsB[i]->remove();
		for (int i = 0; i < 5; ++i)
			mColsA[i]->remove();
		return;
	}

	mMActor->calcAnm();
	if (unk24B) {
		for (int i = 0; i < 17; ++i) {
			mColsB[i]->moveMtx(*mPartsB[i]->getRootJointMtx());
			mColsB[i]->setUp();
		}
		for (int i = 0; i < 5; ++i) {
			mColsA[i]->moveMtx(*mPartsA[i]->getRootJointMtx());
			mColsA[i]->setUp();
		}
	}

	if (unk248) {
		if (animIsFinished()) {
			if (mOwner->unk16C->resetGrip.get()) {
				offLiveFlag(LIVE_FLAG_DEAD);
				unk248 = 0;
				unk24A = 0;
				unk249 = 1;
				unk24B = 0;
				startAnim(0);
				J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
				if (ctrl) {
					ctrl->setFrame(0.0f);
					ctrl->setRate(0.0f);
				}
				unk250 = 1.0f;
				unk258 = 100;
				unk260 = 0;
			} else {
				kill();
			}
		} else {
			J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
			if (ctrl)
				ctrl->setRate(0.5f * (unk250 * SMSGetAnmFrameRate()));
			MtxPtr mtx = *mPartsA[0]->getRootJointMtx();
			unk138[1].set(mtx[0][3], mtx[1][3], mtx[2][3]);
			gpMSound->startSoundActor(0x300D, unk138 + 1, 0, nullptr, 0, 4);
			SMSRumbleMgr->start(8, (Vec*)unk138);
		}
	} else {
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
		ctrl->setRate(0.0f);
		if (unk254 > 0) {
			ctrl->setFrame(1.0f);
			if (++unk254 > unk258) {
				unk248 = 1;
				unk254 = 0;
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF6, *getRootJointMtx(), 0, this);
				gpMarioParticleManager->emitAndBindToMtxPtr(
				    0xF7, *getRootJointMtx(), 0, this);
			}
		} else {
			ctrl->setFrame(0.0f);
		}
	}
}

void TBathtubGrip::calcRootMatrix()
{
	TMtx34f* base = (TMtx34f*)getModel()->getBaseTRMtx();
	TMtx34f local;
	MsMtxSetRotRPH(local, 0.0f, mAngle, 0.0f);
	local.ref(0, 3) = 0.0f;
	local.ref(1, 3) = 0.0f;
	local.ref(2, 3) = 0.0f;
	base->concat(*(TSMtx34f*)mOwner->getRootJointMtx(), local);
}

Mtx* TBathtubGrip::getRootJointMtx() const
{
	return (Mtx*)getModel()->getBaseTRMtx();
}

BOOL TBathtubGrip::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x98];
	switch (message) {
	case HIT_MESSAGE_SUPER_HIP_DROP: {
		if (unk248)
			return false;
		if (!unk249)
			return false;

		unk138[0].set(*gpMarioPos);
		gpMSound->startSoundActor(0x3821, unk138, 0, nullptr, 0, 4);

		u16 n = mOwner->getNumGripsDead();
		if (n == 4) {
			mOwner->startDemo();
			return true;
		}

		mOwner->quake(sender->mPosition);
		int breakCount;
		f32 animSpeed;
		switch (n) {
		case 1:
			breakCount = mOwner->unk16C->breakCount0.get();
			animSpeed  = mOwner->unk16C->animSpeed1.get();
			break;
		case 2:
			breakCount = mOwner->unk16C->breakCount1.get();
			animSpeed  = mOwner->unk16C->animSpeed2.get();
			break;
		case 3:
			breakCount = mOwner->unk16C->breakCount2.get();
			animSpeed  = mOwner->unk16C->animSpeed3.get();
			break;
		case 4:
			breakCount = mOwner->unk16C->breakCount3.get();
			animSpeed  = mOwner->unk16C->animSpeed4.get();
			break;
		default:
			breakCount = mOwner->unk16C->breakCount0.get();
			animSpeed  = mOwner->unk16C->animSpeed0.get();
			break;
		}
		unk258 = breakCount;
		unk250 = animSpeed;
		unk254 = 1;
		unk25C->setBck("stand_effect");
		unk25C->setBtk("stand_effect");
		unk25C->setBrk("stand_effect");
		J3DFrameCtrl* ctrl = unk25C->getFrameCtrl(0);
		if (ctrl)
			ctrl->setRate(0.5f * (12.0f * SMSGetAnmFrameRate()));
		unk248 = 1;
		unk249 = 0;
		int anim = n;
		if (anim == 0)
			anim = 1;
		else if (anim == 1)
			anim = 0;
		startAnim(anim);
		return true;
	}
	case HIT_MESSAGE_HIP_DROP:
		mOwner->hipdrop(sender->mPosition);
		return true;
	case HIT_MESSAGE_TRAMPLE:
		if (!mOwner->unk29A
		    && mOwner->unk250 <= mOwner->unk16C->trampleRelease.get()) {
			mOwner->unk250 = mOwner->unk16C->trampleRelease.get();
			mOwner->unk258 = mOwner->unk16C->trampleRecover.get();
			mOwner->unk25C = mOwner->unk16C->trampleRecover.get();
			mOwner->unk254 = mOwner->unk16C->hipdropRelease.get();
		}
		return true;
	}
	return false;
}

void TBathtubGrip::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TMapObjBase::perform(cue, graphics);
	if (!unk260) {
		if (cue & 1) {
			MTXCopy(*getRootJointMtx(), unk25C->getModel()->getBaseTRMtx());
			if (unk254 > 0 || unk248) {
				if (unk25C->curAnmEndsNext(0, nullptr))
					unk260 = 1;
			}
		}
		unk25C->perform(cue, graphics);
	}
}

TBathtubGrip::TBathtubGrip(TBathtub* owner, f32 angle, MActorAnmData* anmData,
                           const char* name)
    : TMapObjBase(name)
{
	unk25C = new MActor(anmData);
	void* res = JKRFileLoader::getGlbResource(
	    "/scene/map/map/stand_effect/stand_effect.bmd");
	J3DModel* model
	    = new J3DModel(J3DModelLoaderDataBase::load(res, 0x50050000), 0, 1);
	unk25C->setModel(model, 0x50050000);
	unk254 = 0;
	mOwner = owner;
	mAngle = angle;
	initAndRegister("stand_break");
	calcRootMatrix();
	getModel()->calc();

	JUTNameTab* names = getModel()->getModelData()->getJointName();
	for (int i = 0; i < 17; ++i) {
		char partName[0x10];
		char colPath[0x100];
		sprintf(partName, "c%d", i + 1);
		sprintf(colPath, "/scene/mapObj/stand_break_%s.col", partName);
		mJointIdx[i] = names->getIndex(partName);
		mColsB[i]    = new TMapCollisionMove;
		mPartsB[i]   = new TBathtubGripPartsHard(this, i);
		mColsB[i]->init(colPath, 0, mPartsB[i]);

		if (i < 5) {
			sprintf(partName, "b%d", i + 1);
			sprintf(colPath, "/scene/mapObj/stand_break_%s.col", partName);
			mColsA[i]  = new TMapCollisionMove;
			mPartsA[i] = new TBathtubGripPartsFragile(this, i);
			mColsA[i]->init(colPath, 0, mPartsA[i]);
		}
	}

	offLiveFlag(LIVE_FLAG_DEAD);
	unk248 = 0;
	unk24A = 0;
	unk249 = 1;
	unk24B = 0;
	startAnim(0);
	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(0);
	if (ctrl) {
		ctrl->setFrame(0.0f);
		ctrl->setRate(0.0f);
	}
	unk250 = 1.0f;
	unk258 = 100;
	unk260 = 0;
}

void TBathtubGrip::kill()
{
	unk24A = 1;
	makeObjDead();
	removeCollisions_();
}

void TBathtubGrip::removeCollisions_()
{
	for (int i = 0; i < 17; ++i)
		mColsB[i]->remove();
	for (int i = 0; i < 5; ++i)
		mColsA[i]->remove();
}

BOOL TBathtubGripPartsHard::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SUPER_HIP_DROP)
		message = HIT_MESSAGE_HIP_DROP;
	return mGrip->receiveMessage(sender, message);
}

BOOL TBathtubGripPartsFragile::receiveMessage(THitActor* sender, u32 message)
{
	return mGrip->receiveMessage(sender, message);
}

Mtx* TBathtubGripParts::getRootJointMtx() const
{
	return (Mtx*)mGrip->getModel()->getAnmMtx(mGrip->mJointIdx[mIndex]);
}

