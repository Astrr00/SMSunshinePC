#include <Enemy/EffectEnemy.hpp>
#include <Map/Map.hpp>
#include <Map/MapData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <Player/MarioAccess.hpp>
#include <Strategic/ObjModel.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

void TEffectEnemy::setDeadAnm()
{
	char trash[8];
	gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_OFF,
	                                            &mPosition, 0, nullptr);
	SMSGetMSound()->startSoundActor(MSD_SE_BS_WANWAN_TO_COOL, &mPosition, 0,
	                                nullptr, 0, 4);
	onLiveFlag(LIVE_FLAG_UNK20000);
}

void TEffectEnemy::sendAttackMsgToMario()
{
	switch (unk194) {
	case 0:
		SMS_SendMessageToMario(this, HIT_MESSAGE_UNKA);
		kill();
		break;
	case 1:
		SMS_SendMessageToMario(this, 9);
		break;
	default:
		SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
		break;
	}
}

void TEffectEnemy::behaveToWater(THitActor* param_1)
{
	if (mHitPoints > 1)
		TSmallEnemy::behaveToWater(param_1);
	else
		kill();
}

void TEffectEnemy::reset() { TWalkerEnemy::reset(); }

void TEffectEnemy::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	if (cue & 1)
		TWalkerEnemy::moveObject();

	if ((cue & 2) && !checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		Vec local_1c;
		VECScale(&mScaling, &local_1c,
		         mHitPoints
		             / (u8)(getSaveParam() ? getSaveParam()->mSLHitPointMax.get()
		                                   : 1));

		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_C,
		                                            &mPosition, 3, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_A,
		                                            &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_B,
		                                            &mPosition, 1, this);
		gpMarioParticleManager->emitAndBindToPosPtr(PARTICLE_MS_MOE_FIRE_D,
		                                            &mPosition, 1, this);
	}

	THitActor::perform(cue, graphics);
}

void TEffectEnemy::forceKill()
{
	if (!(mGroundPlane->checkFlag(BG_CHECK_FLAG_ILLEGAL)
	      || (!mGroundPlane->isDeathPlane() && !mGroundPlane->isPool()
	          && !mGroundPlane->isWaterSurface())
	      || isAirborne() || checkLiveFlag(LIVE_FLAG_UNK10))
	    || !gpMap->isInArea(mPosition.x, mPosition.z))
		kill();
}

void TEffectEnemy::kill()
{
	setDeadAnm();
	onLiveFlag(LIVE_FLAG_DEAD);
	onHitFlag(HIT_FLAG_NO_COLLISION);
}

void TEffectEnemy::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("default.bmd", 3);
}

void TEffectEnemy::init(TLiveManager* manager)
{
	TWalkerEnemy::init(manager);
	mActorType = 0x10000005;
}

TEffectEnemy::TEffectEnemy(const char* name)
    : TWalkerEnemy(name)
    , unk194(0)
{
}

void TEffectEnemyManager::initSetEnemies() { }

TSmallEnemy* TEffectEnemyManager::createEnemyInstance()
{
	return new TEffectEnemy;
}

void TEffectEnemyManager::loadAfter() { JDrama::TNameRef::loadAfter(); }

void TEffectEnemyManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TWalkerEnemyParams("/enemy/moveFireEffect.prm");
}
