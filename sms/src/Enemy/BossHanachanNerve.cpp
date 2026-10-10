#include <Enemy/BossHanachan.hpp>
#include <GC2D/GCConsole2.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSSetSound.hpp> // include order decides the sinit order
#include <MSound/MSoundBGM.hpp>
#include <MSound/MSModBgm.hpp>
#include <Strategic/Spine.hpp>
#include <System/MarDirector.hpp>

DEFINE_NERVE(TNerveBossHanachanGraphWander, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 0)
		boss->setHeadAndBodyAnm(BOSSHANACHAN_ANM_KIND_WALK,
		                        BOSSHANACHAN_BLEND_ON);

	boss->execWalk(true);
	if (boss->checkFallDecideAndSetup()) {
		spine->pushAfterCurrent(&TNerveBossHanachanTumble::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanTumble, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 0)
		boss->setTumbleAnm(BOSSHANACHAN_BLEND_ON);
	else
		boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_TUMBLE);

	boss->execSlip();
	if (0.0f == boss->mMarchSpeed && boss->isTumbleCompletelyAllBody()) {
		gpMarDirector->getConsole()->startAppearBalloon(0xE0007, true);
		spine->pushAfterCurrent(&TNerveBossHanachanDown::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanDown, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_DOWN);
	if (spine->getTime() >= boss->mChangeSaveParams->mSLDownFrames.get()) {
		boss->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanGetUp, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_GETUP);
	if (boss->isFinishedGetUp()) {
		boss->setRandomWeakBodyIndex();
		boss->setAnmTimerWhenSnort();
		spine->pushAfterCurrent(&TNerveBossHanachanSnort::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanDamage, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_DAMAGE);
	boss->execSlip();
	if (0.0f == boss->mMarchSpeed
	    && spine->getTime() >= boss->mChangeSaveParams->mSLDamageFrames.get()) {
		boss->setAnmTimerWhenGetUp();
		spine->pushAfterCurrent(&TNerveBossHanachanGetUp::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanSnort, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	if (spine->getTime() == 200 && boss->checkLiveFlag(LIVE_FLAG_UNK20000)) {
		boss->offLiveFlag(LIVE_FLAG_UNK20000);
		MSBgm::startBGM(MSD_BGM_BOSSGESO_2DN3RD); // id 0x80010029 per asm
		switch (boss->mHitPoints) {
		case 2:
			gpMSound->unk98->changeTempo(0, 1);
			break;
		case 1:
			gpMSound->unk98->changeTempo(1, 1);
			break;
		}
	}

	boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_SNORT);
	if (boss->isAllBckAlreadyEnd(BOSSHANACHAN_ANM_KIND_SNORT)) {
		boss->goToInitialRecoverGraphNode();
		spine->pushAfterCurrent(&TNerveBossHanachanGraphWander::theNerve());
		return true;
	}
	return false;
}

DEFINE_NERVE(TNerveBossHanachanDead, TLiveActor)
{
	TBossHanachan* boss = static_cast<TBossHanachan*>(spine->getBody());
	boss->considerSetAnm(BOSSHANACHAN_NERVE_ANM_DEAD);
	if (!boss->checkLiveFlag(LIVE_FLAG_UNK40000)
	    && boss->isAllBckAlreadyEnd(BOSSHANACHAN_ANM_KIND_DEAD)) {
		boss->onLiveFlag(LIVE_FLAG_UNK40000);
		boss->removeAllMapCollision();
	}
	return false;
}
