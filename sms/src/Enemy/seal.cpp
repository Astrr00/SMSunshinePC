#include <Enemy/Seal.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <M3DUtil/MActor.hpp>
#include <Map/MapCollisionManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MSound/SoundEffects.hpp>
#include <Player/ModelWaterManager.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

TSeal::TSeal(const char* name)
    : TSpineEnemy(name)
{
	unk150 = 0;
	onLiveFlag(LIVE_FLAG_UNK10);
}

void TSeal::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("gene_orange_model1.bmd", 0);
	mMActor->offMakeDL();

	f32 radius = 100.0f * mScaling.x;
	initHitActor(0x10000024, 1, 0x81000000, radius, radius, radius, radius);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"))
	    ->getChildren()
	    .push_back(this);

	mRotation.x = MsWrap(270.0f + mRotation.x, 0.0f, 360.0f);

	mMapCollisionManager
	    = new TMapCollisionManager(1, "/scene/seal", this);
	mMapCollisionManager->init("gene_orange_col1.col", 2, nullptr);
	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	mHitPoints = getSaveParam() ? getSaveParam()->mSLHitPointMax.value : 1;
	mSpine->initWith(&TNerveSealSleep::theNerve());
}

BOOL TSeal::receiveMessage(THitActor* sender, u32 message)
{
	char trash[0x10];
	if (sender->getActorType() == 0x1000001
	    && message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		gpMarioParticleManager->emit(PARTICLE_MS_ENM_WATHIT, &sender->mPosition,
		                             0, nullptr);
		gpMSound->startSoundSet(MSD_SE_EN_COMMON_W_HIT_OK, &sender->mPosition,
		                        0, 0.0f, 0, 0, 4);

		if (gpModelWaterManager->unk5D5F != 0) {
			gpMSound->startSoundSet(MSD_SE_ERASE_SCRAWL, &sender->mPosition, 0,
			                        0.0f, 0, 0, 4);

			const TNerveBase<TLiveActor>* die = &TNerveSealDie::theNerve();
			if (mSpine->getLatestNerve() != die) {
				if (mMapCollisionManager->unk8)
					mMapCollisionManager->unk8->remove();
				const TNerveBase<TLiveActor>* nerve = &TNerveSealDie::theNerve();
				mSpine->pushNerve(nerve);
			}
			++unk150;
			return true;
		}
		return true;
	}
	return false;
}

void TSeal::calcRootMatrix()
{
	J3DModel* model = getModel();
	MsMtxSetXYZRPH(model->getBaseTRMtx(), mPosition.x, mPosition.y, mPosition.z,
	               mRotation.x, mRotation.y, mRotation.z);
	model->setBaseScale(mScaling);
}

void TSeal::perform(u32 cue, JDrama::TGraphics* graphics)
{
	char trash[8];
	if (!(mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN)) && (cue & 1)) {
		for (int i = 0; i < mColCount; ++i) {
			THitActor* col = mCollisions[i];
			if (col->isActorType(0x80000001))
				col->receiveMessage(this, HIT_MESSAGE_ATTACK);
		}
	}

	TSpineEnemy::perform(cue, graphics);

	if (cue & 1) {
		updateSquareToMario();
		unk150 = 0;
	}

	if ((cue & 2) && !(mLiveFlag & (LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN))
	    && mDistToMarioSquared < 2250000.0f) {
		if (gpMSound->gateCheck(MSD_SE_EN_ORANGESEAL_WAIT))
			MSoundSESystem::MSoundSE::startSoundActor(
			    MSD_SE_EN_ORANGESEAL_WAIT, &mPosition, 0, nullptr, 0, 4);
	}
}

TSealManager::TSealManager(const char* name)
    : TEnemyManager(name)
{
}

void TSealManager::createModelData()
{
	static const TModelDataLoadEntry entry[] = {
		{ "gene_orange_model1.bmd", 0x11210000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

void TSealManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
}

DEFINE_NERVE(TNerveSealSleep, TLiveActor)
{
	char trash[8];
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0) {
		if (!self->mMActor->checkCurBckFromIndex(1))
			self->mMActor->setBckFromIndex(-1);
	}

	if (self->mDistToMarioSquared < 1000000.0f) {
		spine->pushAfterCurrent(&TNerveSealWait::theNerve());
		return true;
	}

	if (self->mMActor->curAnmEndsNext(0, nullptr)) {
		if (self->mMActor->checkCurBckFromIndex(1))
			self->mMActor->setBckFromIndex(-1);
	}
	return false;
}

DEFINE_NERVE(TNerveSealWait, TLiveActor)
{
	char trash[0x10];
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0)
		self->mMActor->setBckFromIndex(3);

	if (self->mMActor->curAnmEndsNext(0, nullptr)) {
		if (self->mMActor->checkCurBckFromIndex(3))
			self->mMActor->setBckFromIndex(2);
	}

	if (self->mDistToMarioSquared > 2250000.0f) {
		if (self->mMActor->curAnmEndsNext(0, nullptr)) {
			self->mMActor->setBckFromIndex(1);
			spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
			return true;
		}
	}
	return false;
}

DEFINE_NERVE(TNerveSealDie, TLiveActor)
{
	char trash[0x10];
	TSeal* self = (TSeal*)spine->getBody();

	if (spine->getTime() == 0) {
		self->mMActor->setBckFromIndex(0);

		MtxPtr mtx = self->mMActor->getModel()->getBaseTRMtx();
		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(0xD1, mtx, 0,
		                                                  self))
			emitter->setGlobalScale(self->mScaling);

		if (JPABaseEmitter* emitter
		    = gpMarioParticleManager->emitAndBindToMtxPtr(
		        0xD2, mtx, 0, (const u8*)self + 1))
			emitter->setGlobalScale(self->mScaling);
	}

	if (gpMSound->gateCheck(MSD_SE_WT_BOSS_FADEAWAY))
		MSoundSESystem::MSoundSE::startSoundActor(MSD_SE_WT_BOSS_FADEAWAY,
		                                          &self->mPosition, 0, nullptr,
		                                          0, 4);

	if (self->mMActor->curAnmEndsNext(0, nullptr)) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->kill();
		spine->pushAfterCurrent(&TNerveSealSleep::theNerve());
		return true;
	}
	return false;
}
