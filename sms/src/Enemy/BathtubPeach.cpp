#include <Enemy/BathtubPeach.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjManager.hpp>
#include <Strategic/Spine.hpp>
#include <System/Application.hpp>
#include <math.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>
#include <Map/MapCollisionEntry.hpp>

// Only index 21 (the bathtub peach wait animation) has an animation sound.
static const char* bathtubpeach_bastable[23] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	"/scene/bathtubpeach/bas/peach_wait.bas",
	nullptr,
};

static inline TBathtubPeachParams* getPeachParams(TBathtubPeach* peach)
{
	return static_cast<TBathtubPeachParams*>(
	    static_cast<TEnemyManager*>(peach->mManager)->unk38);
}

void TBathtubPeachManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TBathtubPeachParams("/enemy/bathtubpeach.prm");
}

void TBathtubPeachManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "ahiru_peach.bmd",
		  J3DMLF_MaterialPEFull | J3DMLF_MaterialTexGenFull
		      | J3DMLF_UseUniqueMaterials | (4 << J3DMLF_TevStageNumShift),
		  0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TBathtubPeachManager::createEnemyInstance() { return nullptr; }

TBathtubPeachManager::TBathtubPeachManager(const char* name)
    : TEnemyManager(name)
{
}

void TBathtubPeach::calcRootMatrix()
{
	TBathtub* bathtub
	    = static_cast<TBathtub*>(JDrama::TNameRefGen::search("バスタブ"));

	if (bathtub && bathtub->unk29A) {
		MTXCopy(bathtub->getPeachMtxInDemo(), getModel()->getBaseTRMtx());
	} else {
		TLiveActor::calcRootMatrix();
	}
}

BOOL TBathtubPeach::receiveMessage(THitActor* sender, u32 message)
{
	return TSpineEnemy::receiveMessage(sender, message);
}

Mtx* TBathtubPeach::getRootJointMtx() const
{
	return reinterpret_cast<Mtx*>(getModel()->getBaseTRMtx());
}

void TBathtubPeach::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSpineEnemy::perform(cue, graphics);
}

inline void TBathtubPeach::changeAnm(int bck, int btp, f32 rate)
{
	if (!mMActor->checkCurBckFromIndex(bck)) {
		mMActor->setBckFromIndex(bck);
		const char** table = getBasNameTable();
		setAnmSound(!table ? nullptr : table[bck]);
	}

	if (mMActor->getCurAnmIdx(ANM_TYPE_BTP) != btp)
		mMActor->setBtpFromIndex(btp);

	J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
	ctrl->setRate(0.5f * (rate * SMSGetAnmFrameRate()));
}

void TBathtubPeach::reset()
{
	mPosition.x *= 0.21f;
	mPosition.y *= 0.21f;
	mPosition.z *= 0.21f;

	mBathtubBinder.init(50.0f, 50.0f, 50.0f, 50.0f, 0.0f);
	unk130 = 0;
	mScaling.setAll(1.5f);
	mBinder = &mBathtubBinder;

	TSpineEnemy::reset();

	changeAnm(1, 1, 2.0f);
}

void TBathtubPeach::init(TLiveManager* manager)
{
	TSpineEnemy::init(manager);

	mSpine->initWith(&TNervePeachEscape::theNerve());

	initAnmSound();
	reset();
	mScaling.setAll(2.0f);
}

const char** TBathtubPeach::getBasNameTable() const
{
	return bathtubpeach_bastable;
}

TBathtubPeach::TBathtubPeach(const char* name)
    : TSpineEnemy(name)
{
	onLiveFlag(LIVE_FLAG_AIRBORNE);
	offLiveFlag(LIVE_FLAG_UNK100);
	offLiveFlag(LIVE_FLAG_UNK10);
}

BOOL TNervePeachEscape::execute(TSpineBase<TLiveActor>* spine) const
{
	TBathtubPeach* peach = static_cast<TBathtubPeach*>(spine->getBody());
	TBathtub* bathtub
	    = static_cast<TBathtub*>(JDrama::TNameRefGen::search("バスタブ"));

	if (bathtub->unk29A)
		return FALSE;

	if ((spine->getTime() & 4) == 0) {
		if (bathtub->getBathtubData().unk64)
			spine->pushNerve(&TNervePeachStagger::theNerve());
		return FALSE;
	}

	peach->changeAnm(1, 1, 2.0f);

	MtxPtr tubMtx = reinterpret_cast<MtxPtr>(bathtub->getRootJointMtx());
	f32 tubX      = tubMtx[0][3];
	f32 tubZ      = tubMtx[2][3];

	JGeometry::TVec3<f32> marioPos = *gpMarioPos;
	f32 marioAngle
	    = SHORTANGLE2DEG(matan(marioPos.z - tubZ, marioPos.x - tubX));
	f32 peachDZ    = peach->mPosition.z - tubZ;
	f32 peachDX    = peach->mPosition.x - tubX;
	f32 peachAngle = SHORTANGLE2DEG(matan(peachDZ, peachDX));

	// Keep the target angle on the side of Mario the peach is already on.
	// The original's std::fmodf is the same truncating float mod as
	// JGeometry::TUtil<f32>::mod.
	f32 target = -180.0f
	             + JGeometry::TUtil<f32>::mod(
	                 360.0f + ((peachAngle - marioAngle) - -180.0f), 360.0f);
	if (target < 0.0f) {
		target = -180.0f
		         + JGeometry::TUtil<f32>::mod(
		             360.0f
		                 + ((marioAngle - getPeachParams(peach)->angle.get())
		                    - -180.0f),
		             360.0f);
	} else {
		target = -180.0f
		         + JGeometry::TUtil<f32>::mod(
		             360.0f
		                 + ((marioAngle + getPeachParams(peach)->angle.get())
		                    - -180.0f),
		             360.0f);
	}

	s16 targetShort = static_cast<s16>(DEG2SHORTANGLE(target));
	f32 radius      = getPeachParams(peach)->radius.get();

	JGeometry::TVec2<f32> step(
	    (radius * JMASSin(targetShort) + tubX) - peach->mPosition.x,
	    (radius * JMASCos(targetShort) + tubZ) - peach->mPosition.z);

	f32 speed = getPeachParams(peach)->speed.get();
	if (step.x * step.x + step.y * step.y >= speed * speed)
		step.setLength(static_cast<TBathtubPeachParams*>(
		                   static_cast<TEnemyManager*>(peach->mManager)
		                       ->getSaveParam())
		                   ->speed.get());

	peach->mPosition.x += step.x;
	peach->mPosition.z += step.y;

	f32 dz        = gpMarioPos->z - peach->mPosition.z;
	f32 dx        = gpMarioPos->x - peach->mPosition.x;
	f32 turnSpeed = getPeachParams(peach)->turnSpeed2.get();

	if (dz * dz + dx * dx <= JGeometry::TUtil<f32>::epsilon())
		return FALSE;

	// Turn the peach towards Mario.
	f32 faceAngle = SHORTANGLE2DEG(matan(dz, dx));
	f32 face      = faceAngle - 90.0f;
	f32 diff = -180.0f
	           + JGeometry::TUtil<f32>::mod(
	               360.0f + ((face - peach->mRotation.y) - -180.0f), 360.0f);
	if (diff < -turnSpeed) {
		peach->mRotation.y
		    = -180.0f
		      + JGeometry::TUtil<f32>::mod(
		          360.0f + ((peach->mRotation.y - turnSpeed) - -180.0f),
		          360.0f);
	} else if (diff > turnSpeed) {
		peach->mRotation.y
		    = -180.0f
		      + JGeometry::TUtil<f32>::mod(
		          360.0f + ((peach->mRotation.y + turnSpeed) - -180.0f),
		          360.0f);
	} else {
		peach->mRotation.y = face;
	}

	return FALSE;
}

BOOL TNervePeachStagger::execute(TSpineBase<TLiveActor>* spine) const
{
	TBathtubPeach* peach = static_cast<TBathtubPeach*>(spine->getBody());

	peach->changeAnm(0, 0, 2.0f);

	return peach->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr) ? TRUE
	                                                             : FALSE;
}
