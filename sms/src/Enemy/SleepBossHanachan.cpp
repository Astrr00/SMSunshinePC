#include <Enemy/BossHanachan.hpp>
#include <Enemy/SleepBossHanachan.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MoveBG/Item.hpp>
#include <MoveBG/ItemManager.hpp>
#include <Strategic/MirrorActor.hpp>
#include <Strategic/Spine.hpp>
#include <System/FlagManager.hpp>

// rogue include needed for matching the string pool in .rodata
#include <M3DUtil/InfectiousStrings.hpp>

static const char* sleepBossHanachan_bastable[] = {
	"/scene/sleepBossHanachan/bas/demohanatyan_fall.bas",
	nullptr,
};

DEFINE_NERVE(TNerveSBH_Fall, TLiveActor)
{
	TSleepBossHanachan* self
	    = static_cast<TSleepBossHanachan*>(spine->getBody());

	if (self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr)) {
		JGeometry::TVec3<f32> pos = self->mShinePos;
		char trash[0xc];
		TShine* shine             = gpItemManager->makeShineAppearWithDemo(
            "シャイン（ボス用）", "ボスシャインカメラ", pos.x, pos.y, pos.z);
		shine->onMapObjFlag(0x20000000);

		self->onLiveFlag(LIVE_FLAG_DEAD);
		TMirrorActor* mirror = self->mMirrorActor;
		mirror->unk1A |= 1;
		SMS_HideAllShapePacket(mirror->unk14);

		spine->pushAfterCurrent(&TNerveSBH_SleepContinue::theNerve());
		return TRUE;
	}
	return FALSE;
}

DEFINE_NERVE(TNerveSBH_SleepContinue, TLiveActor) { return FALSE; }

void TSleepBossHanachan::startFall(f32 x, f32 y, f32 z)
{
	char trash[8];
	TFlagManager::getInstance()->setBool(true, 0x5000B);

	mShinePos.set(x, y, z);
	mMActor->setBckFromIndex(0);
	setCurAnmSound();
	mSpine->setNext(&TNerveSBH_Fall::theNerve());
}

const char** TSleepBossHanachan::getBasNameTable() const
{
	return sleepBossHanachan_bastable;
}

void TSleepBossHanachan::calcRootMatrix() { }

void TSleepBossHanachan::init(TLiveManager* manager)
{
	initBase(manager, 3);

	mSpine->initWith(&TNerveSBH_SleepContinue::theNerve());

	initHitActor(0x8000016, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
	onHitFlag(HIT_FLAG_NO_COLLISION);

	initAnmSound();
	mMActor->setBckFromIndex(1);
	setCurAnmSound();

	MsMtxSetXYZRPH(getModel()->getBaseTRMtx(), mPosition.x, mPosition.y,
	               mPosition.z, mRotation.x, mRotation.y, mRotation.z);

	mMirrorActor = new TMirrorActor("寝てるボスハナチャンin鏡");
	mMirrorActor->init(getModel(), 10);
}

void TSleepBossHanachanManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "demohanatyan_model.bmd",
		  J3DMLF_MaterialPEFull | (1 << J3DMLF_TevStageNumShift), 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}
