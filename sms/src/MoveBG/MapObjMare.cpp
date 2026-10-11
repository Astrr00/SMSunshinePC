#include <MoveBG/MapObjMare.hpp>
#include <MoveBG/ItemManager.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <System/MarDirector.hpp>
#include <System/Particles.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Player/WaterGun.hpp>
#include <Camera/CubeManagerBase.hpp>
#include <Camera/CubeMapTool.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <Map/MapCollisionData.hpp>
#include <Map/MapEventMare.hpp>
#include <Map/MapWireManager.hpp>
#include <M3DUtil/MActor.hpp>
#include <Enemy/Cannon.hpp>
#include <MSound/MSound.hpp>
#include <MSound/SoundEffects.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <dolphin/gx.h>
#ifdef TARGET_PC
#include <string.h> // the MSL header is only here to match sinit/bss order and clashes with the CRT
#else
#include <PowerPC_EABI_Support/Msl/MSL_C/MSL_Common/string.h>
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

#include <Map/MapCollisionEntry.hpp>

f32 TCogwheelScale::mWaterLeakSpeed = 0.01f;
static f32 sRadius                  = 800.0f;
f32 TCogwheel::mRopeWidthX          = 10.0f;
f32 TCogwheel::mRopeWidthZ          = 7.0f;
f32 TCogwheel::mTexPosRate          = 0.01f;
f32 TCogwheel::mMinSpeed            = 3.0f;
static f32 mGrowStartFrame          = 90.0f;
static f32 mGrowEndFrame            = 175.0f;

static JGeometry::TVec3<f32> fall_upper_pos(2827.0f, 8604.0f, 7202.0f);

void TMareEventPoint::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);
	initHitActor(0x40000236, 0, 0, 0.0f, 0.0f, 300.0f, 600.0f);
}

BOOL TMareEventPoint::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		int id = TMapObjBase::getWaterID(sender);
		if (!gpModelWaterManager->checkFlagBottom4Bits(id, 0x1)
		    && TMapObjBase::getWaterPlane(sender) != nullptr
		    && TMapObjBase::getWaterPlane(sender)->mNormal.y < 0.1f) {
			if (unk68->startEvent()) {
				gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT,
				                             &sender->getPosition(), 0,
				                             nullptr);
				gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &mPosition,
				                        0, 0.0f, 0, 0, 4);
			}
			return true;
		}
	}
	return false;
	char trash[0x10];
}

void TMareCork::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (unk154) {
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 250.0f) {
			unk148.set(2773.0f, 8618.0f, 7006.0f);
			gpMSound->startSoundActor(MSD_SE_ENV_FALL_JET_LEVEL, &unk148, 0,
			                          nullptr, 0, 4);
			gpMarioParticleManager->emitAndBindToPosPtr(0x14C, &unk13C, 1,
			                                            this);
			gpMarioParticleManager->emitAndBindToPosPtr(0x14D, &unk13C, 1,
			                                            this);
			gpMarioParticleManager->emitAndBindToPosPtr(0x14E, &unk13C, 1,
			                                            this);
		}
	}
	char trash[8];
}

MtxPtr TMareCork::getTakingMtx() { return mMActor->getModel()->getAnmMtx(2); }

void TMareCork::calcRootMatrix()
{
	if (unk154) {
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(350.0f);
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->checkPass(250.0f)) {
			unk138->startChorobeiShout();
			gpItemManager->makeShineAppearWithDemo(
			    "シャイン（ボス用）", "ボスシャインカメラ", mPosition.x,
			    mPosition.y, mPosition.z);
			unk148.set(2773.0f, 8618.0f, 7006.0f);
			JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
			    PARTICLE_MS_M_SPHIPD_HIT_B, &unk148, 0x4000, 0xD82, 0, 0,
			    nullptr);
			if (emitter)
				emitter->setGlobalScale(JGeometry::TVec3<f32>(2.5f));
		}
	}
	TMapObjBase::calcRootMatrix();
}

void TMareCork::moveObject()
{
	if (unk138->isObject() && !unk154) {
		mMActor->setBck("marecork");
		setAnmSound("/scene/mapObj/marecork.bas");
		removeMapCollision();
		unk154 = true;
	}
}

void TMareCork::loadAfter()
{
	unk138 = (TCannon*)JDrama::TNameRefGen::getInstance()
	             ->getRootNameRef()
	             ->search("砲台");
	if (unk138->receiveMessage(this, HIT_MESSAGE_TAKE))
		mHeldObject = unk138;

	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_a.jpa", 0x14C);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_b.jpa", 0x14D);
	SMS_LoadParticle("/scene/map/map/ms_mare_gunwat_c.jpa", 0x14E);

	TMapObjBase::loadAfter();
	unk13C.set(0.0f, 0.0f, 0.0f);
	initAnmSound();
	char trash[8];
}

void TMareFall::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	SMS_LoadParticle("/scene/mapObj/mareFallSplash.jpa", 0x149);
	SMS_LoadParticle("/scene/mapObj/mareFallSmoke.jpa", 0x14A);
}

void TMareFall::calc()
{
	SMSGetMSound()->startSoundActor(MSD_SE_GE_FALL, &mPosition, 0, nullptr, 0,
	                                4);
	gpMSound->startSoundActor(MSD_SE_GE_FALL_UPPER, &fall_upper_pos, 0, nullptr,
	                          0, 4);
	gpMarioParticleManager->emit(0x149, &mPosition, 1, this);
	gpMarioParticleManager->emit(0x14A, &mPosition, 1, this);
	char trash[0x10];
}

TMuddyBoat::TMuddyBoat(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0.0f)
    , unk148(0.0f)
    , unk14C(0.0f)
    , unk150(0.0f)
    , unk154(0.0f)
    , unk158(0.0f)
    , unk15C(0.0f)
    , unk160(0.0f)
    , unk164(0.0f)
    , unk168(0)
    , unk16C(0)
{
	unk170.zero();
	unk17C.zero();
}

void TMuddyBoat::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 0.04f;
	unk144 = 0.998f;
	unk148 = 0.002f;
	unk150 = 0.997f;
	unk13C = 0.01f;
	unk168 = 600;
	if (gpMarDirector->mMap == 0x34) {
		unk158 = 126.0f;
		unk154 = 185.0f;
		unk15C = 150.0f;
		unk160 = 170.0f;
		unk164 = 185.0f;
	} else {
		unk158 = 100.0f;
		unk154 = 170.0f;
		unk15C = 150.0f;
		unk160 = 180.0f;
		unk164 = 100.0f;
	}
	unk17C.x = 3.0f;
	unk17C.y = 2.0f;
	unk17C.z = 5.0f;
	char trash[0x10];
}

u32 TMuddyBoat::getSDLModelFlag() const { return 0; }

void TMuddyBoat::calc()
{
	f32 z = mPosition.z;
	f32 y = mPosition.y - mYOffset;
	f32 h = y + gpMapObjWave->getWaveHeight(mPosition.x, z);
	MsMtxSetXYZRPH(getModel()->getAnmMtx(0), mPosition.x, h, z, 0.0f,
	               mRotation.y, 0.0f);

	TPosition3f scale;
	scale.identity();
	MTXScale(scale, mInitialScaling.x, mInitialScaling.y, mInitialScaling.z);
	MtxPtr anm = getModel()->getAnmMtx(0);
	MTXConcat(getModel()->getAnmMtx(0), scale, anm);

	if (unk140 != 0.0f) {
		unk170.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
		JGeometry::TVec3<f32> particleScale(3.0f * mScaling.x, 2.0f * mScaling.y,
		                                    3.0f * mScaling.z);
		emitAndBindScale(0x1E8, 3, &unk170, particleScale);
		emitAndBindScale(0x107, 1, &unk170, particleScale);
		unk16C = 0;
	}
	char trash[0x38];
}

void TMuddyBoat::control()
{
	TMapObjBase::control();
	if (marioIsOn())
		moveByWater();

	switch (mState) {
	case 1: {
		unk140 *= unk144;
		gpMSound->startSoundActorWithInfo(MSD_SE_OBJ_DORO_FLOAT, &mPosition,
		                                  nullptr, abs(unk140), 0, 0, nullptr,
		                                  0, 4);
		if (unk14C != 0.0f) {
			mRotation.y += unk14C;
			mRotation.y = MsWrap(mRotation.y, 0.0f, 360.0f);
			unk14C *= unk150;
			if (abs(unk14C) < 0.0001f)
				unk14C = 0.0f;
		}
		break;
	}
	case 2:
		if (animIsFinished()) {
			mStateTimer = unk168;
			onMapObjFlag(0x100);
			sleep();
			mState = 3;
		}
		break;
	case 3:
		if (!isStateTimerEngaged()) {
			awake();
			makeObjDead();
			makeObjDefault();
			makeObjAppeared();
			JGeometry::TVec3<f32> scale(2.0f * mScaling.x, 2.0f * mScaling.y,
			                            3.0f * mScaling.z);
			unk170.set(mPosition.x, mPosition.y - mYOffset, mPosition.z);
			emitAndSRT(PARTICLE_MS_ENM_DISAP_A, 0, &unk170, mRotation, scale);
			emitAndSRT(PARTICLE_MS_ENM_DISAP_B, 0, &unk170, mRotation, scale);
			gpMSound->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0,
			                          nullptr, 0, 4);
			mState = 1;
		}
		break;
	}
	char trash[0x20];
}

void TMuddyBoat::bind()
{
	if (checkLiveFlag(LIVE_FLAG_UNK10))
		return;

	JGeometry::TVec3<f32> pos = mPosition;
	MtxPtr mtx                = getModel()->getAnmMtx(0);
	pos.x += mtx[0][2] * unk140;
	pos.z += mtx[2][2] * unk140;

	int cubeNo = gpCubeStream->getInCubeNo(*gpMarioPos);
	if (cubeNo != -1) {
		TCubeStreamInfo* info
		    = (TCubeStreamInfo*)gpCubeStream->unk14->begin()[cubeNo];
		Mtx cubeMtx;
		MsMtxSetXYZRPH(cubeMtx, 0.0f, 0.0f, 0.0f, info->unk18.x, info->unk18.y,
		               info->unk18.z);
		f32 dot = 0.0f;
		dot += mtx[0][2] * cubeMtx[0][2];
		dot += mtx[2][2] * cubeMtx[2][2];
		unk140 += 0.0001f * (dot * info->unk40);
	}

	const TBGCheckData* ground;
	f32 groundY = gpMap->checkGroundIgnoreWaterSurface(pos, &ground);
	if (groundY > mPosition.y - mYOffset - 100.0f
	    || ground->checkFlag(BG_CHECK_FLAG_ILLEGAL)) {
		pos = mPosition;
		kill();
		mLinearVelocity.x = mLinearVelocity.y = mLinearVelocity.z = 0.0f;
		return;
	}

	if (!bindToWall(JGeometry::TVec3<f32>(pos.x + mtx[0][2] * unk160,
	                                      mPosition.y - mYOffset,
	                                      pos.z + mtx[2][2] * unk160),
	                unk158, &pos)) {
		if (!bindToWall(JGeometry::TVec3<f32>(pos.x - mtx[0][2] * unk164,
		                                      mPosition.y - mYOffset,
		                                      pos.z - mtx[2][2] * unk164),
		                unk15C, &pos)) {
			if (!bindToWall(JGeometry::TVec3<f32>(pos.x, mPosition.y - mYOffset,
			                                      pos.z),
			                unk154, &pos)) {
				mLinearVelocity = pos - mPosition;
			}
		}
	}
	char trash[0xF0];
}

inline bool TMuddyBoat::bindToWall(const JGeometry::TVec3<f32>& center,
                                   f32 radius, JGeometry::TVec3<f32>* pos)
{
	TBGWallCheckRecord record(center, radius, 4,
	                          TBGWallCheckRecord::DONT_MOVE_XZ);
	if (gpMap->isTouchedWallsAndMoveXZ(&record)) {
		touchWall(pos, record);
		return true;
	}
	return false;
}

inline void TMuddyBoat::touchWall(JGeometry::TVec3<f32>* pos,
                                  const TBGWallCheckRecord& record)
{
	const TBGCheckData* wall = record.mResultWalls[0];
	unk170.set(record.mCenter.x - wall->mNormal.x * (50.0f + record.mRadius),
	           100.0f + (mPosition.y - getObjCollisionHeightOffset()),
	           record.mCenter.z - wall->mNormal.z * (50.0f + record.mRadius));
	*pos = mPosition;
	kill();
	mLinearVelocity.x = mLinearVelocity.y = mLinearVelocity.z = 0.0f;
}

void TMuddyBoat::kill()
{
	unk140 = 0.0f;
	unk14C = 0.0f;
	SMS_EasyEmitParticle(PARTICLE_MS_M_AMIATTACK, &unk170, nullptr,
	                     JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
	gpMSound->startSoundActor(MSD_SE_OBJ_DORO_BROKEN, &mPosition, 0, nullptr, 0,
	                          4);
	MtxPtr base = getModel()->getBaseTRMtx();
	MTXCopy(getModel()->getAnmMtx(0), base);
	offMapObjFlag(0x100);
	onLiveFlag(LIVE_FLAG_UNK10);
	startAnim(1);
	startAnim(2);
	mState = 2;
	char trash[8];
}

void TMuddyBoat::calcRootMatrix() { }

void TMuddyBoat::moveByWater()
{
	if ((s32)SMS_GetMarioWaterGun()->mIsEmitWater != 0) {
		MtxPtr nozzle = SMS_GetMarioWaterGun()->getEmitMtx(0);
		JGeometry::TVec3<f32> dir(-nozzle[0][0], 0.0f, -nozzle[2][0]);
		MsVECNormalize(&dir, &dir);

		MtxPtr mtx = getModel()->getAnmMtx(0);
		JGeometry::TVec3<f32> boat(mtx[0][2], 0.0f, mtx[2][2]);
		f32 dot = dir.dot(boat);

		JGeometry::TVec3<f32> toMario;
		getNormalVecFromTargetXZ(gpMarioPos->x, gpMarioPos->z, &toMario);
		if (toMario.x != 0.0f || toMario.z != 0.0f)
			MsVECNormalize(&toMario, &toMario);

		f32 cross = boat.z * (dir.x - boat.x) - boat.x * (dir.z - boat.z);

		if (cross * toMario.dot(boat) > 0.0f)
			unk14C += unk148 * (1.0f - abs(dot));
		else
			unk14C -= unk148 * (1.0f - abs(dot));

		if (dot > 0.0f)
			unk140 += dot * unk138;
		else
			unk140 += dot * unk13C;

		offLiveFlag(LIVE_FLAG_UNK10);
	}
	char trash[0x10];
}

void TMapObjPuncher::load(JSUMemoryInputStream& stream)
{
	TMapObjBase::load(stream);
	s32 value;
	stream.read(value);
	unk138 = value;
	sleep();
	offHitFlag(HIT_FLAG_NO_COLLISION);
}

void TMapObjPuncher::control()
{
	TMapObjBase::control();
	switch (mState) {
	case 2:
		soundBas(MSD_SE_OBJ_PUNCHER_RETURN, 101.0f,
		         mMActor->getFrameCtrl(ANM_TYPE_BCK)->getRate());
		if (animIsFinished()) {
			JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
			emitAndScale(PARTICLE_MS_ENM_DISAP_A_W, 0, &mPosition, scale);
			emitAndScale(PARTICLE_MS_ENM_DISAP_B, 0, &mPosition, scale);
			gpMSound->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0,
			                          nullptr, 0, 4);
			kill();
		}
		break;
	}
	char trash[0x10];
}

void TMapObjPuncher::touchPlayer(THitActor*)
{
	awake();
	startAnim(1);
	JGeometry::TVec3<f32> dir;
	makeVecToLocalZ(1.0f, &dir);
	JGeometry::TVec3<f32> target = *gpMarioPos;
	target += dir * 100.0f;
	SMS_MarioMoveRequest(target);
	SMS_SendMessageToMario(this, HIT_MESSAGE_THROWN);
	SMS_ThrowMario(dir, unk138);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	JGeometry::TVec3<f32> scale(2.0f, 2.0f, 2.0f);
	emitAndScale(PARTICLE_MS_ENM_DISAP_A_W, 0, &mPosition, scale);
	emitAndScale(PARTICLE_MS_ENM_DISAP_B, 0, &mPosition, scale);
	gpMSound->startSoundActor(MSD_SE_SMOKE_EFFECT, &mPosition, 0, nullptr, 0,
	                          4);
	mState = 2;
	char trash[0x10];
}

TWireBell::TWireBell(const char* name)
    : TMapObjBase(name)
    , unk138(-1)
    , unk13C(200.0f)
    , unk140(10.0f)
    , unk144(5.0f)
    , unk148(0.01f)
{
	unk14C.zero();
}

void TWireBell::loadAfter()
{
	TMapObjBase::loadAfter();
	unk138 = gpMapWireManager->getWireNo(mPosition);
}

void TWireBell::control()
{
	gpMapWireManager->getPointPosInNthWire(unk138, mPosition, &unk14C);
	mPosition.x = unk14C.x;
	mPosition.y = unk14C.y - unk13C;
	mPosition.z = unk14C.z;
	Mtx mtx;
	MsMtxSetTRS(mtx, mPosition, mRotation, mScaling);
	MTXCopy(mtx, getModel()->getAnmMtx(0));
}

void TWireBell::draw() const
{
	initDraw();
	f32 y0 = mPosition.y;
	f32 y1 = unk14C.y;
	f32 v1   = unk148 * (y1 - y0);
	f32 v0   = unk148 * (y0 - y0);
	f32 xMax = unk14C.x + unk140;
	f32 xMin = unk14C.x - unk140;
	f32 zMax = unk14C.z + unk144;
	f32 zMin = unk14C.z - unk144;
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(xMin, y1, zMin);
	GXTexCoord2f32(0.0f, v1);
	GXPosition3f32(xMin, y0, zMin);
	GXTexCoord2f32(0.0f, v0);
	GXPosition3f32(xMax, y1, zMax);
	GXTexCoord2f32(1.0f, v1);
	GXPosition3f32(xMax, y0, zMax);
	GXTexCoord2f32(1.0f, v0);
	GXPosition3f32(xMax, y1, zMin);
	GXTexCoord2f32(0.0f, v1);
	GXPosition3f32(xMax, y0, zMin);
	GXTexCoord2f32(0.0f, v0);
	GXPosition3f32(xMin, y1, zMax);
	GXTexCoord2f32(1.0f, v1);
	GXPosition3f32(xMin, y0, zMax);
	GXTexCoord2f32(1.0f, v0);
}

void TWireBell::initDraw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0x00, 0x00, 0x64, 0xFF });

	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	JUTTexture texture(gpMapObjManager->unkC8);
	texture.load(GX_TEXMAP0);

	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
	char trash[8];
}

TMapObjGrowTree::TMapObjGrowTree(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0)
    , unk148(0.0f)
{
}

void TMapObjGrowTree::initMapObj()
{
	TMapObjBase::initMapObj();
	unk138 = 1000.0f;
	unk13C = 0.5f;
	unk140 = 0.1f;
	unk144 = 0x168;
	unk148 = mDamageHeight;
	mMActor->setBtp("moyasi_wink");
}

void TMapObjGrowTree::loadAfter()
{
	TMapObjBase::loadAfter();
	removeMapCollision();
}

void TMapObjGrowTree::control()
{
	TMapObjBase::control();
	if (isState(2) && mColCount == 0 && !isStateTimerEngaged()) {
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > 0.0f) {
			if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < mGrowEndFrame)
				removeMapCollision();

			f32 speed = -unk140;
			mMActor->getFrameCtrl(ANM_TYPE_BCK)
			    ->setFrame(speed
			               + mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame());
			if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 0.0f) {
				startAnim(0);
				mState = 1;
				return;
			}

			f32 frame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
			if (67.0f <= frame && frame <= 240.0f)
				SMSGetMSound()->startSoundActor(MSD_SE_OBJ_SAMDBOMB_REVERSE,
				                                &mPosition, 0, nullptr, 0, 4);

			updateHeight();
			if (mHeldObject) {
				JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
				pos.y -= getGrowHeightFromRate(unk140);
				mHeldObject->moveRequest(pos);
			}
		}
	}
	char trash[0x60];
}

inline f32 TMapObjGrowTree::getGrowHeightFromRate(f32 rate) const
{
	if (mGrowStartFrame < mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
	    && mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < mGrowEndFrame)
		return rate * unk138 / (mGrowEndFrame - mGrowStartFrame);
	return 0.0f;
}

inline void TMapObjGrowTree::updateHeight()
{
	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > mGrowStartFrame) {
		if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > mGrowEndFrame) {
			setDamageHeight(unk138);
		} else {
			setDamageHeight(
			    unk148
			    + (unk138 - unk148)
			          * (getMActor()->getFrameCtrl(ANM_TYPE_BCK)->getFrame()
			             - mGrowStartFrame)
			          / (mGrowEndFrame - mGrowStartFrame));
		}
	} else {
		setDamageHeight(unk148);
	}
}

u32 TMapObjGrowTree::touchWater(THitActor* sender)
{
	if (sender->mPosition.y > mPosition.y + unk148)
		return 0;

	if (isState(1)) {
		startAnim(1);
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setRate(0.0f);
		mState = 2;
	}

	s16 endFrame = mMActor->getFrameCtrl(ANM_TYPE_BCK)->getEnd();
	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < endFrame) {
		soundBas(MSD_SE_OBJ_SANDBOMB_WATER_1, 3.0f, unk13C);
		soundBas(MSD_SE_OBJ_SANDBOMB_WATER_2, 67.0f, unk13C);
		soundBas(MSD_SE_OBJ_SANDBOMB_WATER_3, 103.0f, unk13C);
		soundBas(MSD_SE_OBJ_SANDBOMB_WATER_4, 137.0f, unk13C);

		f32 speed = unk13C;
		mMActor->getFrameCtrl(ANM_TYPE_BCK)
		    ->setFrame(speed + mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame());
		updateHeight();
		if (mHeldObject) {
			JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
			pos.y += getGrowHeightFromRate(unk13C);
			mHeldObject->moveRequest(pos);
		}
	}

	if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() > mGrowEndFrame) {
		setUpMapCollision(0);
		mStateTimer = unk144;
	}
	return 1;
	char trash[0x50];
}

void TMapObjElasticCode::initMapObj()
{
	TMapObjBase::initMapObj();
	unk140   = 0.997f;
	mGravity = 0.01f;
	unk138   = 2.0f;
	unk13C   = 0.0005f;
}

void TMapObjElasticCode::control()
{
	TMapObjBase::control();
	mVelocity.y *= unk140;
	mVelocity.y += unk13C * (mInitialPosition.y - mPosition.y) - getGravityY();
	if (mHeldObject) {
		mVelocity.y -= unk138;
		JGeometry::TVec3<f32> pos = mHeldObject->mPosition;
		JGeometry::TVec3<f32> vel = mVelocity;
		pos.y += vel.y;
		mHeldObject->moveRequest(pos);
	}
	JGeometry::TVec3<f32> vel = mVelocity;
	mPosition.y += vel.y;
	char trash[0x18];
}

void TMapObjElasticCode::draw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0x00, 0x00, 0x64, 0xFF });

	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
	              GX_COLOR0A0);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
	GXSetCullMode(GX_CULL_NONE);
	GXSetLineWidth(0x18, GX_TO_ZERO);

	GXBegin(GX_LINES, GX_VTXFMT0, 2);
	GXPosition3f32(mInitialPosition.x, mInitialPosition.y + 1000.0f,
	               mInitialPosition.z);
	GXPosition3f32(mPosition.x, mPosition.y, mPosition.z);
	char trash[0x10];
}

TCogwheel::TCogwheel(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0.0f)
    , unk148(0.0f)
    , unk14C(0.0f)
    , unk150(nullptr)
    , unk160(0.0f)
    , unk164(nullptr)
    , unk174(0.0f)
{
	unk154.zero();
	unk168.zero();
}

void TCogwheel::initMapObj()
{
	TMapObjBase::initMapObj();
	f32 angle  = 0.017453294f * mRotation.y;
	f32 radius = sRadius;
	f32 cs    = cosf(angle);
	f32 sn    = sinf(angle);
	f32 zero  = 0.0f;
	f32 dx    = radius * cs - zero * sn;
	f32 dz    = radius * sn + zero * cs;

	JGeometry::TVec3<f32> pos(mPosition.x + dx, mPosition.y, mPosition.z - dz);
	unk150 = (TCogwheelScale*)TMapObjBaseManager::newAndRegisterObj(
	    "cogwheel_plate", pos, mRotation);
	unk150->unk154 = true;
	unk150->unk158 = this;
	unk150->appear();
	unk154.set(pos.x, mPosition.y, pos.z);

	pos.set(mPosition.x - dx, mPosition.y, mPosition.z + dz);
	unk164 = (TCogwheelScale*)TMapObjBaseManager::newAndRegisterObj(
	    "cogwheel_pot", pos, mRotation);
	unk164->unk154 = false;
	unk164->unk158 = this;
	unk164->appear();
	unk168.set(pos.x, mPosition.y, pos.z);

	if (strcmp(getName(), "天秤上") == 0) {
		unk140 = 0.003f;
		unk144 = 0.99f;
		unk148 = 0.8f;
		unk14C = 3800.0f;
		unk160 = 1000.0f;
		unk174 = 1800.0f;
		unk164->unk138 = 0.0f;
		unk164->unk13C = 0.0f;
		unk164->unk144 = 14.0f;
		unk150->unk138 = 10.0f;
		unk150->unk13C = 0.0f;
		unk150->unk144 = 0.0f;
	} else {
		unk140 = 0.008f;
		unk144 = 0.98f;
		unk148 = 0.8f;
		unk14C = 3950.0f;
		unk160 = 1000.0f;
		unk174 = 1900.0f;
		unk164->unk138 = 0.0f;
		unk164->unk13C = 0.0f;
		unk164->unk144 = 14.0f;
		unk150->unk138 = 10.0f;
		unk150->unk13C = 0.0f;
		unk150->unk144 = 0.0f;
	}
	unk13C = unk14C * 0.5f;
	char trash[0x20];
}

inline void TCogwheel::rebound()
{
	unk138 = unk138 * -unk148;
	if (abs(unk138) < mMinSpeed)
		unk138 = 0.0f;
}

void TCogwheel::control()
{
	TMapObjBase::control();
	unk13C += unk138;
	unk138 += unk140
	          * ((unk150->unk138 + unk150->unk140 + unk150->unk148)
	             - (unk164->unk138 + unk164->unk140 + unk164->unk148));
	unk138 *= unk144;
	if (unk13C < unk160 && unk138 < 0.0f)
		rebound();
	if (unk13C > unk14C - unk174 && unk138 > 0.0f)
		rebound();

	unk150->mPosition.y = (mPosition.y - unk13C) + unk150->mYOffset;
	unk164->mPosition.y = mPosition.y - (unk14C - unk13C);

	f32 speed = abs(unk138);
	if (speed > mMinSpeed)
		gpMSound->startSoundActorWithInfo(MSD_SE_OBJ_MR_TSUBO_PULL, &mPosition,
		                                  nullptr, speed, 0, 0, nullptr, 0, 4);
	char trash[0x28];
}

void TCogwheel::calc()
{
	Mtx rotZ;
	Mtx rotY;
	mRotation.z = 360.0f * (-unk13C / (3.14f * (2.0f * sRadius)));
	makeRootMtxRotZ(rotZ);
	rotZ[0][3] = 0.0f;
	rotZ[1][3] = 0.0f;
	rotZ[2][3] = 0.0f;
	makeRootMtxRotY(rotY);
	rotY[0][3] = 0.0f;
	rotY[1][3] = 0.0f;
	rotY[2][3] = 0.0f;
	MtxPtr anm = getModel()->getAnmMtx(0);
	MTXConcat(rotY, rotZ, anm);
	anm[0][3] = mPosition.x;
	anm[1][3] = mPosition.y;
	anm[2][3] = mPosition.z;
	char trash[8];
}

void TCogwheel::draw() const
{
	initDraw();

	f32 plateY  = unk150->mPosition.y - unk150->mYOffset;
	f32 topY    = 600.0f + plateY;
	f32 ropeY   = unk154.y;
	f32 x0      = unk154.x - mRopeWidthX;
	f32 x1      = unk154.x + mRopeWidthX;
	f32 z0      = unk154.z - mRopeWidthZ;
	f32 z1      = unk154.z + mRopeWidthZ;
	f32 vTop    = mTexPosRate * (topY - plateY);
	f32 vBottom = mTexPosRate * (ropeY - plateY);
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(x0, topY, z0);
	GXTexCoord2f32(0.0f, vTop);
	GXPosition3f32(x0, ropeY, z0);
	GXTexCoord2f32(0.0f, vBottom);
	GXPosition3f32(x1, topY, z1);
	GXTexCoord2f32(1.0f, vTop);
	GXPosition3f32(x1, ropeY, z1);
	GXTexCoord2f32(1.0f, vBottom);
	GXPosition3f32(x1, topY, z0);
	GXTexCoord2f32(2.0f, vTop);
	GXPosition3f32(x1, ropeY, z0);
	GXTexCoord2f32(2.0f, vBottom);
	GXPosition3f32(x0, topY, z1);
	GXTexCoord2f32(3.0f, vTop);
	GXPosition3f32(x0, ropeY, z1);
	GXTexCoord2f32(3.0f, vBottom);

	f32 potY    = unk164->mPosition.y - unk164->mYOffset;
	f32 x2      = unk168.x + mRopeWidthX;
	f32 x3      = unk168.x - mRopeWidthX;
	f32 z2      = unk168.z + mRopeWidthZ;
	f32 z3      = unk168.z - mRopeWidthZ;
	f32 potTopY = 1200.0f + potY;
	f32 vPot0   = mTexPosRate * (ropeY - potY);
	f32 vPot1   = mTexPosRate * (potTopY - potY);
	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 8);
	GXPosition3f32(x3, potTopY, z3);
	GXTexCoord2f32(0.0f, vPot1);
	GXPosition3f32(x3, ropeY, z3);
	GXTexCoord2f32(0.0f, vPot0);
	GXPosition3f32(x2, potTopY, z2);
	GXTexCoord2f32(1.0f, vPot1);
	GXPosition3f32(x2, ropeY, z2);
	GXTexCoord2f32(1.0f, vPot0);
	GXPosition3f32(x2, potTopY, z3);
	GXTexCoord2f32(0.0f, vPot1);
	GXPosition3f32(x2, ropeY, z3);
	GXTexCoord2f32(0.0f, vPot0);
	GXPosition3f32(x3, potTopY, z2);
	GXTexCoord2f32(1.0f, vPot1);
	GXPosition3f32(x3, ropeY, z2);
	GXTexCoord2f32(1.0f, vPot0);
	char trash[0x58];
}

void TCogwheel::initDraw() const
{
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GXLoadPosMtxImm(j3dSys.getViewMtx(), GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);

	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0x00, 0x00, 0x64, 0xFF });

	GXSetNumTexGens(1);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	JUTTexture texture(gpMapObjManager->unkC8);
	texture.load(GX_TEXMAP0);

	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);

	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	GXSetCullMode(GX_CULL_BACK);
	char trash[8];
}

TCogwheelScale::TCogwheelScale(const char* name)
    : TMapObjBase(name)
    , unk138(0.0f)
    , unk13C(0.0f)
    , unk140(0.0f)
    , unk144(0.0f)
    , unk148(0.0f)
    , unk14C(0.01f)
    , unk150(5.0f)
    , unk154(false)
    , unk158(nullptr)
{
}

void TCogwheelScale::control()
{
	unk148 = 0.0f;
	TMapObjBase::control();
	if (unk140 > 0.0f) {
		unk140 -= mWaterLeakSpeed;
		SMSGetMSound()->startSoundActorWithInfo(
		    MSD_SE_OBJ_MR_TSUBO_WATER, &mPosition, nullptr, abs(unk140), 0, 0,
		    nullptr, 0, 4);
		if (unk140 < 0.0f)
			unk140 = 0.0f;
	}
}

void TCogwheelScale::touchPlayer(THitActor*)
{
	if (marioIsOn())
		unk148 = unk13C;

	if (mPosition.y - mYOffset > 150.0f + gpMarioPos->y) {
		if ((unk154 && unk158->unk138 > 0.0f)
		    || (!unk154 && unk158->unk138 < 0.0f)) {
			unk158->rebound();
			if (marioHeadAttack())
				unk158->unk138 = unk14C * (unk158->unk138 * *gpMarioSpeedY);
		}
	}
	unk140 = 0.0f;
	char trash[0x38];
}

BOOL TCogwheelScale::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_HIP_DROP) {
		unk158->unk138 = unk158->unk138 + unk150;
		return true;
	}
	return TMapObjBase::receiveMessage(sender, message);
}

u32 TCogwheelScale::touchWater(THitActor*)
{
	if (unk140 < unk144)
		unk140 += 1.0f;
	return 1;
}
