#include <Enemy/HanaSambo.hpp>
#include <Enemy/Conductor.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/HitActor.hpp>
#include <System/EmitterViewObj.hpp>
#include <System/Particles.hpp>
#include <Map/MapData.hpp>
#include <Map/Map.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Mario.hpp>
#include <M3DUtil/MActorData.hpp>
#include <M3DUtil/MActor.hpp>
#include <M3DUtil/SDLModel.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/LightUtil.hpp>
#include <MarioUtil/TexUtil.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/RumbleMgr.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <MoveBG/ItemManager.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <MoveBG/MapObjManager.hpp>
#include <MoveBG/MapObjBase.hpp>
#include <MoveBG/Item.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSe.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JParticle/JPAEmitter.hpp>
#include <string.h>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

u8 TSamboHead::mBodyJntIndex;
static TSamboHead* gpCurSamboHead;

u8 THanaSambo::mHeadJntIndex   = 3;
u8 THanaSambo::mPollenJntIndex = 6;

static const char* sambo_bastable[] = {
	"/scene/sambo/bas/sambo_down.bas",
	nullptr,
	nullptr,
	nullptr,
	"/scene/sambo/bas/sambo_Fhide.bas",
	nullptr,
	"/scene/sambo/bas/sambo_Fset.bas",
	"/scene/sambo/bas/sambo_Fwait.bas",
	"/scene/sambo/bas/sambo_hit.bas",
	nullptr,
	"/scene/sambo/bas/sambo_Ydown.bas",
};

static const char* sambohead_bastable[] = {
	"/scene/sambohead/bas/flower_shoot.bas",
	"/scene/sambohead/bas/samboHead_crash.bas",
	"/scene/sambohead/bas/samboHead_dance.bas",
	"/scene/sambohead/bas/samboHead_down.bas",
	"/scene/sambohead/bas/samboHead_Fhide.bas",
	"/scene/sambohead/bas/samboHead_hit.bas",
	"/scene/sambohead/bas/samboHead_hit_end.bas",
	"/scene/sambohead/bas/samboHead_jump_end.bas",
	"/scene/sambohead/bas/samboHead_jump_start.bas",
	nullptr,
	"/scene/sambohead/bas/samboHead_set.bas",
	"/scene/sambohead/bas/samboHead_turn.bas",
	nullptr,
};

// ============= TSamboFlowerCoinUnit =============

TSamboFlowerCoinUnit::TSamboFlowerCoinUnit(int param_1)
    : mFlowers(nullptr)
    , mNumFlowers(0)
    , mMaxFlowers(param_1)
    , mCoin(nullptr)
    , mCoinCount(param_1)
{
	mFlowers = new TSamboFlower*[param_1];
}

void TSamboFlowerCoinUnit::add(TSamboFlower* param_1)
{
	mFlowers[mNumFlowers] = param_1;
	param_1->unk164       = &mCoinCount;
	mNumFlowers += 1;
}

void TSamboFlowerCoinUnit::checkGenCoin()
{
	if (mCoin == nullptr)
		return;

	bool bloomed = true;
	for (int i = 0; i < mNumFlowers; i++) {
		bool fwait = mFlowers[i]->mMActor->checkCurAnm("flower_fwait", 0) ? true : false;
		if (!fwait)
			bloomed = false;
	}

	if (!bloomed)
		return;

	int total = 0;
	for (int i = 0; i < mNumFlowers; i++) {
		TSamboFlower* flower = mFlowers[i];
		if (flower->unk168 != nullptr)
			total++;

		flower->unk160 = 0;
	}

	if (total > 0) {
		Mtx mtx;
		int spawned = 0;
		for (int i = 0; i < mNumFlowers; i++) {
			if (mFlowers[i]->unk168 == nullptr)
				continue;

			f32 rate = (f32)spawned / (f32)total;

			JGeometry::TVec3<f32> offset(0.0f, 0.0f,
			                             mFlowers[i]->unk16C->mSLCoinCircleR.get());
			MsMtxSetRotY(mtx, 360.0f * rate);
			MTXMultVec(mtx, &offset, &offset);

			TCoin* coin = mFlowers[i]->unk168;
			if (coin->isActorType(0x2000000E))
				coin = (TCoin*)gpItemManager->makeObjAppear(0x2000000E);

			if (coin) {
				coin->appear();

				coin->mPosition = mCoinPos + offset;

				MsVECNormalize(&offset, &offset);

				TSamboFlowerSaveLoadParams* prm = mFlowers[i]->unk16C;
				coin->mVelocity.set(offset.x * prm->mSLCoinVelocityXZ.get(),
				                    8.0f * rate + prm->mSLCoinVelocityY.get(),
				                    offset.z * prm->mSLCoinVelocityXZ.get());
				coin->offLiveFlag(LIVE_FLAG_UNK10);
				spawned++;
			}
		}

		if (gpMSound->gateCheck(0x4813))
			MSoundSESystem::MSoundSE::startSoundSystemSE(0x4813, spawned,
			                                             nullptr, 0);
	}

	mCoin = nullptr;
}

// ============= TSamboLeaf =============

TSamboLeaf::TSamboLeaf(TSamboFlowerManager* manager,
                       SDLModelData* model_data, const char* name)
    : JDrama::TViewObj(name)
    , mModel(nullptr)
    , unk44(0)
    , mManager(manager)
{
	mModel = new SDLModel(model_data, 3, 1);
}

void TSamboLeaf::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (!unk44)
		return;

	if (cue & CUE_MOVE) {
		mPosition += mVelocity;

		if (mVelocity.y > -20.0f) {
			TSamboFlowerSaveLoadParams* params
			    = (TSamboFlowerSaveLoadParams*)mManager->unk38;
			mVelocity.y -= params->mSLLeafGravity.get();
		}

		const TBGCheckData* data;
		f32 ground = gpMap->checkGround(mPosition.x, mPosition.y + 20.0f,
		                                mPosition.z, &data);
		if (mPosition.y < ground)
			unk44 = 0;
	}

	if (cue & CUE_CALC_ANIM) {
		Mtx mtx;
		MtxPtr mtxP = mtx;
		MsMtxSetXYZRPH(mtxP, mPosition.x, mPosition.y, mPosition.z, 0,
		               (s16)(mRotation.y * (65536.0f / 360.0f)), 0);

		s16 angle = DEG2SHORTANGLE(-MsGetRotFromZaxis(mVelocity).x);

		Mtx rotMtx;
		f32 sin = JMASSin(angle);
		f32 cos = JMASCos(angle);
		rotMtx[0][0] = cos;
		rotMtx[0][1] = -sin;
		rotMtx[0][2] = 0.0f;
		rotMtx[0][3] = 0.0f;
		rotMtx[1][0] = sin;
		rotMtx[1][1] = cos;
		rotMtx[1][2] = 0.0f;
		rotMtx[1][3] = 0.0f;
		rotMtx[2][0] = 0.0f;
		rotMtx[2][1] = 0.0f;
		rotMtx[2][2] = 1.0f;
		rotMtx[2][3] = 0.0f;

		MTXConcat(mtxP, rotMtx, mtxP);

		MTXCopy(mtxP, mModel->getBaseTRMtx());
		mModel->setBaseScale(mScaling);
		mModel->calc();
	}

	if (cue & CUE_CALC_VIEW)
		mModel->viewCalc();

	if (cue & CUE_ENTRY)
		mModel->entry();

	char trash[4];
}


void TSamboLeaf::generate(JGeometry::TVec3<f32>& param_1)
{
	mRotation.x = 0.0f;
	mRotation.y = param_1.y;
	mRotation.z = 0.0f;
}

// ============= params =============

TSamboFlowerSaveLoadParams::TSamboFlowerSaveLoadParams(const char* path)
    : TSpineEnemyParams(path)
    , PARAM_INIT(mSLLeafVelocityXZ, 4.0f)
    , PARAM_INIT(mSLLeafVelocityY, 6.0f)
    , PARAM_INIT(mSLLeafGravity, 0.2f)
    , PARAM_INIT(mSLBudDist, 2000.0f)
    , PARAM_INIT(mSLBloomTimer, 300)
    , PARAM_INIT(mSLCoinCircleR, 100.0f)
    , PARAM_INIT(mSLCoinVelocityXZ, 12.0f)
    , PARAM_INIT(mSLCoinVelocityY, 10.0f)
    , PARAM_INIT(mSLSeedShootRange, 500.0f)
    , PARAM_INIT(mSLSeedShootInterval, 200)
    , PARAM_INIT(mSLSeedGravity, 0.1f)
    , PARAM_INIT(mSLSeedSpeedXZ, 10.0f)
    , PARAM_INIT(mSLSeedSpeedY, 10.0f)
{
	TParams::load(mPrmPath);
}

// ============= TSamboFlowerManager =============

void TSamboFlowerManager::load(JSUMemoryInputStream& stream)
{
	TEnemyManager::load(stream);
	unk38 = new TSamboFlowerSaveLoadParams("/enemy/samboflower.prm");
	mMatTable = J3DModelLoaderDataBase::loadMaterialTable(
	    JKRGetResource("/scene/samboflower/flower_orange.bmt"));
}

void TSamboFlowerManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "flower.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TSamboFlowerManager::createEnemyInstance()
{
	return new TSamboFlower;
}

void TSamboFlowerManager::loadAfter()
{
	void* rawModelData = JKRGetResource("/scene/samboflower/leaf.bmd");
	SDLModelData* modelData = new SDLModelData(
	    J3DModelLoaderDataBase::load(rawModelData, 0x10210000));

	mLeaves = new TSamboLeaf*[18];
	for (int i = 0; i < 18; ++i) {
		TSamboLeaf* leaf = new TSamboLeaf(this, modelData, "サンボリーフ");
		mLeaves[i]       = leaf;
	}

	mCoinUnitNum = 0;
	for (int i = 0; i < gpItemManager->mObjNum; ++i) {
		if (strstr(gpItemManager->unk18[i]->getName(),
		           "コイン（フラワー用）"))
			mCoinUnitNum += 1;
	}

	mCoinUnits   = new TSamboFlowerCoinUnit*[mCoinUnitNum];
	s32* counts  = new s32[mCoinUnitNum];
	for (int i = 0; i < mCoinUnitNum; ++i)
		counts[i] = 0;

	for (int i = 0; i < mObjNum; ++i) {
		if (strstr(unk18[i]->getName(), "フラワー（コイン用）")) {
			TSamboFlower* flower = (TSamboFlower*)unk18[i];
			if (flower->unk158 < mCoinUnitNum)
				counts[flower->unk158] += 1;
		}
	}

	for (int i = 0; i < mCoinUnitNum; ++i)
		mCoinUnits[i] = new TSamboFlowerCoinUnit(counts[i]);

	for (int i = 0; i < gpItemManager->mObjNum; ++i) {
		if (strstr(gpItemManager->unk18[i]->getName(), "コイン（フラワー用）")) {
			TFlowerCoin* coin = (TFlowerCoin*)gpItemManager->unk18[i];
			s32 id           = coin->unk158;
			if (id < mCoinUnitNum) {
				mCoinUnits[id]->mCoin    = coin;
				mCoinUnits[id]->mCoinPos = coin->mPosition;
				coin->kill();
			}
		}
	}

	for (int i = 0; i < mObjNum; ++i) {
		if (strstr(unk18[i]->getName(), "フラワー（コイン用）")) {
			TSamboFlower* flower = (TSamboFlower*)unk18[i];
			if (flower->unk158 < mCoinUnitNum) {
				TSamboFlowerCoinUnit* unit = mCoinUnits[flower->unk158];
				if (unit->mNumFlowers < unit->mMaxFlowers)
					unit->add(flower);
			}
		}
	}

	char trash[0x5c];
	JDrama::TNameRef::loadAfter();
}

void TSamboFlowerManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (mCoinUnitNum > 0 && (cue & CUE_CALC_ANIM)) {
		for (int i = 0; i < mCoinUnitNum; ++i)
			mCoinUnits[i]->checkGenCoin();
	}

	TEnemyManager::perform(cue, graphics);

	for (int i = 0; i < 18; ++i)
		mLeaves[i]->perform(cue, graphics);
}

void TSamboFlowerManager::dropLeaf(JGeometry::TVec3<f32>& param_1,
                                   JGeometry::TVec3<f32>& param_2)
{
	const f32 angleTable[] = { 0.0f, 120.0f, 240.0f };

	int generated = 0;
	for (int i = 0; generated < 3 && i < 18; ++i) {
		TSamboLeaf* leaf = mLeaves[i];
		if (!leaf->unk44) {
			leaf->mPosition = param_1;
			leaf->mPosition.y += 10.0f;
			leaf->unk44 = 1;

			TSamboFlowerSaveLoadParams* params
			    = (TSamboFlowerSaveLoadParams*)unk38;
			f32 leafVelXZ = params->mSLLeafVelocityXZ.get();
			f32 leafVelY  = params->mSLLeafVelocityY.get();
			TMsRange<f32> rangeXZ(leafVelXZ, leafVelXZ * 1.2f);
			TMsRange<f32> rangeY(leafVelY, leafVelY * 1.2f);
			f32 velZ = rangeXZ.rand();
			f32 velY = rangeY.rand();
			JGeometry::TVec3<f32> vel(0.0f, velY, velZ);

			Mtx mtx;
			MsMtxSetRotRPH(mtx, 0.0f, angleTable[i], 0.0f);
			MTXMultVec(mtx, &vel, &vel);

			JGeometry::TVec3<f32> leafRot(0.0f, angleTable[i] - 90.0f, 0.0f);
			mLeaves[i]->generate(leafRot);
			mLeaves[i]->mVelocity = vel;
			mLeaves[i]->mScaling  = param_2;

			++generated;
		}
	}
}

// ============= TSamboFlower =============

void TSamboFlower::load(JSUMemoryInputStream& stream)
{
	TSpineEnemy::load(stream);
#ifdef TARGET_PC
	unk15C = stream.readS32();
	unk158 = stream.readS32();
#else
	stream.read(&unk15C, 4);
	stream.read(&unk158, 4);
#endif
	unk160 = 1;
	reset();
	offLiveFlag(LIVE_FLAG_UNK800);
}

void TSamboFlower::loadAfter()
{
	JDrama::TNameRef::loadAfter();
	if (unk15C >= 0) {
		TMapObjBase* coin = TMapObjBaseManager::newAndRegisterObj(
		    "coin", JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
		    JGeometry::TVec3<f32>(0.0f, 0.0f, 0.0f),
		    JGeometry::TVec3<f32>(1.0f, 1.0f, 1.0f));
		if (coin != nullptr)
			unk168 = (TCoin*)coin;
	}
}

void TSamboFlower::init(TLiveManager* param_1)
{
	mManager = param_1;
	param_1->manageActor(this);
	setMActorAndKeeper();
	unk130 = 1;
	unk16C = (TSamboFlowerSaveLoadParams*)getSaveParam();
	if (unk16C != nullptr) {
		mBodyRadius       = unk16C->mSLBodyRadius.get();
		mWallRadius       = unk16C->mSLWallRadius.get();
		mHeadHeight       = unk16C->mSLHeadHeight.get();
		mScaledBodyRadius = mBodyScale * mBodyRadius;
	}
	initHitActor(0x10000027, 1, -0x80000000, mBodyRadius, mHeadHeight,
	             mBodyRadius, mHeadHeight);
	onHitFlag(HIT_FLAG_NO_COLLISION);
	offLiveFlag(LIVE_FLAG_UNK400);
	initAnmSound();
	mActorType = 0x10000027;
	unk150     = 0;
	onLiveFlag(LIVE_FLAG_DEAD);
}

void TSamboFlower::setMActorAndKeeper()
{
	char trash[8];
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("flower.bmd", 3);
	MActor* actor = mMActor;
	J3DMaterialTable* table = ((TSamboFlowerManager*)mManager)->mMatTable;
	actor->getModel()->getModelData()->setMaterialTable(table,
	                                                    J3DMatCopyFlag_All);
	actor->initDL();
	actor->getModel()->lock();
}

BOOL TSamboFlower::receiveMessage(THitActor* sender, u32 message)
{
	char trash[8];
	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		if (!unk150) {
			unk150 = 1;
			unk154 = 0;
			gpMarioParticleManager->emit(0xB2, &mPosition, 0, nullptr);
			mMActor->setBck("flower_hit");
			hide();
		}
		return true;
	}
	return false;
}

void TSamboFlower::reset()
{
	TSpineEnemy::reset();
	mMActor->setBck("flower_wait");
	offHitFlag(HIT_FLAG_NO_COLLISION);
	offLiveFlag(LIVE_FLAG_UNK800);
	offLiveFlag(LIVE_FLAG_DEAD);
	offLiveFlag(LIVE_FLAG_UNK10);
}

void TSamboFlower::moveObject()
{
	if (unk150) {
		if (checkCurAnmEnd(0) && mMActor->checkCurAnm("flower_hit", 0))
			mMActor->setBck("flower_fwait");

		if (unk160) {
			unk154 += 1;
			if (isBloomEnd())
				bloom();
		}
	}

	if (!checkLiveFlag(LIVE_FLAG_UNK10))
		mPosition.y = gpMap->checkGround(mPosition.x, mPosition.y + 100.0f,
		                                 mPosition.z, &mGroundPlane);
}

void TSamboFlower::drawObject(JDrama::TGraphics* graphics)
{
	if (!checkLiveFlag(LIVE_FLAG_DEAD)) {
		TCircleShadowRequest request;
		request.mPosition = mPosition;
		request.mRadiusX  = request.mRadiusZ = 120.0f;
		gpBindShadowManager->request(request, mActorType & 0xFFFF0000);
		mMActor->setLightData(mGroundPlane, mPosition);
		mMActor->entry();
	}
}

void TSamboFlower::bloom()
{
	if (mMActor->checkCurAnm("flower_fwait", 0)) {
		unk150 = 0;
		if (unk164 != nullptr)
			++*unk164;
		mMActor->setBck("flower_hit");
		J3DFrameCtrl* ctrl = mMActor->getFrameCtrl(ANM_TYPE_BCK);
		mMActor->getFrameCtrl(ANM_TYPE_BCK)->setFrame(ctrl->getEnd());
		mMActor->setFrameRate(-SMSGetAnmFrameRate(), ANM_TYPE_BCK);
	} else if (mMActor->getFrameCtrl(ANM_TYPE_BCK)->getFrame() < 1.0f) {
		unk154 = 0;
		mMActor->setBck("flower_wait");
	}
}

bool TSamboFlower::isBloomEnd()
{
	return unk154 > unk16C->mSLBloomTimer.get();
}

void TSamboFlower::hide()
{
	if (unk160 && unk164 != nullptr) {
		--*unk164;
		u32 snd = *unk164 + 0x89B9;
		if (gpMSound->gateCheck(snd))
			MSoundSESystem::MSoundSE::startSoundActor(snd, &mPosition, 0,
			                                          nullptr, 0, 4);
	}
}

// ============= THanaSamboManager =============

THanaSamboSaveLoadParams::THanaSamboSaveLoadParams(const char* path)
    : TSmallEnemyParams(path)
    , PARAM_INIT(mSLAttackDist, 200.0f)
    , PARAM_INIT(mSLAttackInterval, 200)
    , PARAM_INIT(mSLAppearDist, 800.0f)
    , PARAM_INIT(mSLHideDist, 1000.0f)
    , PARAM_INIT(mSLAttackingTime, 30)
    , PARAM_INIT(mSLHeadAttackRadius, 60.0f)
    , PARAM_INIT(mSLHeadAttackHeight, 20.0f)
    , PARAM_INIT(mSLHeadDamageRadius, 80.0f)
    , PARAM_INIT(mSLHeadDamageHeight, 40.0f)
{
	TParams::load(mPrmPath);
}

THanaSamboManager::THanaSamboManager(const char* name)
    : TSmallEnemyManager(name)
{
}

void THanaSamboManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new THanaSamboSaveLoadParams("/enemy/hanasambo.prm");
}

TSpineEnemy* THanaSamboManager::createEnemyInstance()
{
	return new THanaSambo;
}

void THanaSamboManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "sambo.bmd", 0x10220000, 0 },
		{ "samboD.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

// ============= params =============

// ============= THanaSamboHead =============

BOOL THanaSamboHead::receiveMessage(THitActor* sender, u32 message)
{
	if (message == HIT_MESSAGE_TRAMPLE || message == HIT_MESSAGE_HIP_DROP) {
		mOwner->kill();
		return true;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER) {
		mOwner->waterDamage();
		return true;
	}

	return false;
}

void THanaSamboHead::checkHit()
{
	for (int i = 0; i < mColCount; ++i) {
		if (mCollisions[i]->isActorType(0x80000001))
			SMS_SendMessageToMario(this, HIT_MESSAGE_ATTACK);
	}
}


void THanaSamboHead::kill() { onHitFlag(HIT_FLAG_NO_COLLISION); }


// ============= THanaSambo =============

THanaSambo::THanaSambo(const char* name)
    : TSmallEnemy(name)
    , unk194(nullptr)
    , unk198(nullptr)
    , unk1A8(nullptr)
    , unk1AC(nullptr)
{
}

void THanaSambo::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	TPathNode marioNode((THitActor*)gpMarioAddress);
	setGoalPath(marioNode);
	char trash[4];
}

void THanaSambo::init(TLiveManager* param_1)
{
	char trashTop[4];
	TSmallEnemy::init(param_1);
	mActorType = 0x1000001A;
	unk150     = 0x11;
	unk198     = (THanaSamboSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveHanaSamboHide::theNerve());

	unk1AC = new TMBindShadowBody(this, getModel(), 1.5f);

	TPathNode marioNode((THitActor*)gpMarioAddress);
	setGoalPath(marioNode);

	if (mInstanceIndex == 0) {
		for (u8 i = 0; i < mMActorKeeper->getMActor("samboD.bmd")
		                       ->getModel()
		                       ->getModelData()
		                       ->getJointNum();
		     ++i) {
			(void)0; // assert?
		}
	}

	unk194 = new THanaSamboHead;
	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(unk194);

	f32 attackRadius = unk198->mSLHeadAttackRadius.get() * mBodyScale;
	f32 attackHeight = unk198->mSLHeadAttackHeight.get() * mBodyScale;
	f32 damageRadius = unk198->mSLHeadDamageRadius.get() * mBodyScale;
	f32 damageHeight = unk198->mSLHeadDamageHeight.get() * mBodyScale;
	unk194->initHitActor(0x1000001B, 2, -0x80000000, attackRadius, attackHeight,
	                     damageRadius, damageHeight);
	unk194->mOwner = this;
	char trash[8];
}

void THanaSambo::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 2);
	mMActor       = mMActorKeeper->createMActor("sambo.bmd", 3);
	mMActorKeeper->createMActor("samboD.bmd", 3);
}

void THanaSambo::moveObject()
{
	TSmallEnemy::moveObject();

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		unk194->mPosition = mPosition;
	} else {
		MtxPtr headMtx = getModel()->getAnmMtx(mHeadJntIndex);
		unk194->mPosition.x = headMtx[0][3];
		unk194->mPosition.y = headMtx[1][3];
		unk194->mPosition.z = headMtx[2][3];
	}

	unk194->checkHit();

	mPosition.x = unk19C.x;
	mPosition.z = unk19C.z;
}

void THanaSambo::drawObject(JDrama::TGraphics* graphics)
{
	TLiveActor::drawObject(graphics);
	if (!checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8))
		unk1AC->entryDrawShadow();
}

void THanaSambo::kill()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD))
		return;

	mHitPoints = 1;

	if (isBckAnm(7))
		unk18C = 3;

	if (mSpine->getCurrentNerve() != &TNerveHanaSamboDie::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveHanaSamboDie::theNerve());
		mSpine->pushAfterCurrent(&TNerveHanaSamboDie::theNerve());
	}

	unk194->onHitFlag(HIT_FLAG_NO_COLLISION);
	onLiveFlag(LIVE_FLAG_UNK40);
}

void THanaSambo::reset()
{
	TSmallEnemy::reset();
	unk165 = 0;
	unk19C = mPosition;
}

void THanaSambo::setWaitAnm()
{
	setBckAnm(7);
	unk1B0 = 0;
}

void THanaSambo::setDeadAnm()
{
	mMActor = getActorKeeper()->getMActor("samboD.bmd");

	if (unk1B0)
		setBckAnm(0xA);
	else
		setBckAnm(0);

	unk194->kill();
	unk1B0 = 0;
	onLiveFlag(LIVE_FLAG_UNK8);
}

// fabricated: never called in emitted code
void THanaSambo::setAttackAnm()
{
	setBckAnm(3);
	unk1B0 = 1;
}

void THanaSambo::waterDamage()
{
	unk165 = true;

	if (changeByJuice())
		return;

	if (mSpine->getCurrentNerve() != &TNerveHanaSamboWait::theNerve())
		return;

	mSpine->pushNerve(&TNerveHanaSamboFreeze::theNerve());
}

const char** THanaSambo::getBasNameTable() const { return sambo_bastable; }

void THanaSambo::perform(u32 cue, JDrama::TGraphics* graphics)
{
	TSmallEnemy::perform(cue, graphics);
	if (!checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN
	                   | LIVE_FLAG_CLIPPED_OUT))
		unk194->THitActor::perform(cue, graphics);
}

void THanaSambo::createPollen()
{
	JGeometry::TVec3<f32> pos;
	Mtx mtx;
	MtxPtr pollenMtx = mMActor->getModel()->getAnmMtx(mPollenJntIndex);

	pos.x = pollenMtx[0][3];
	pos.y = pollenMtx[1][3];
	pos.z = pollenMtx[2][3];

	MsMtxSetRotRPH(mtx, 0.0f, 0.0f, -90.0f);
	MTXConcat(pollenMtx, mtx, mtx);

	JPABaseEmitter* emitter;
	if (mSpine->getCurrentNerve() == &TNerveHanaSamboWait::theNerve()) {
		emitter = gpMarioParticleManager->emit(0xB2, &pos, 0, nullptr);
		mtx[1][3] += 200.0f;
	} else {
		emitter = gpMarioParticleManager->emit(0xB3, &pos, 0, nullptr);

		JGeometry::TVec3<f32> dir(0.0f, 0.0f, 200.0f);
		Mtx rot;
		char trash[4];
		MsMtxSetRotRPH(rot, mRotation.x, mRotation.y, mRotation.z);
		MTXMultVec(rot, &dir, &dir);

		mtx[0][3] += dir.x;
		mtx[1][3] += dir.y;
		mtx[2][3] += dir.z;
	}

	if (emitter != nullptr)
		emitter->setGlobalSRTMatrix(mtx);
}

void THanaSambo::initFlower()
{
	if (unk1A8 == nullptr) {
		unk1A8 = (TSamboFlower*)gpConductor->makeOneEnemyAppear(
		    mPosition, "サンボフラワーマネージャー", 1);
		unk1A8->reset();
	}
	unk1A8->onLiveFlag(LIVE_FLAG_UNK10);
	unk1A8->offLiveFlag(LIVE_FLAG_DEAD);
	unk1A8->mPosition   = mPosition;
	unk1A8->mPosition.y = mGroundHeight;
}

// ============= nerves (THanaSambo) =============

DEFINE_NERVE(TNerveHanaSamboAppear, TLiveActor)
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(HIT_FLAG_NO_COLLISION);
		self->setBckAnm(6);
		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);

		TSamboFlower* flower = self->unk1A8;
		flower->onHitFlag(HIT_FLAG_NO_COLLISION);
		flower->mMActor->setBck("flower_fwait");
		flower->onLiveFlag(LIVE_FLAG_DEAD);
	}

	if (self->checkCurAnmEnd(0)) {
		spine->pushAfterCurrent(&TNerveHanaSamboWait::theNerve());
		self->setWaitAnm();

		if (gpMarioPos->y < self->mPosition.y + 100.0f) {
			self->updateSquareToMario();
			f32 attackDist = self->unk198->mSLAttackDist.get();
			if (self->mDistToMarioSquared < attackDist * attackDist)
				spine->pushAfterCurrent(&TNerveHanaSamboAttack::theNerve());
		}
		return true;
	}

	self->walkToCurPathNode(0.0f, 3.0f * self->mTurnSpeed, 0.0f);
	char trash[16];
	return false;
}

DEFINE_NERVE(TNerveHanaSamboWait, TLiveActor)
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setWaitAnm();

	if (!self->checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)
	    && self->getMActor()->getFrameCtrl(ANM_TYPE_BCK)->checkPass(16.0f))
		self->createPollen();

	if (spine->getTime() > self->unk198->mSLAttackInterval.get()
	    && gpMarioPos->y < self->mPosition.y + 100.0f) {
		self->updateSquareToMario();
		f32 attackDist = self->unk198->mSLAttackDist.get();
		if (self->mDistToMarioSquared < attackDist * attackDist) {
			spine->pushAfterCurrent(&TNerveHanaSamboAttack::theNerve());
			return true;
		}
	}

	self->updateSquareToMario();
	f32 hideDist = self->unk198->mSLHideDist.get();
	if (self->mDistToMarioSquared > hideDist * hideDist) {
		spine->pushAfterCurrent(&TNerveHanaSamboHide::theNerve());
		return true;
	}

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	char trash[40];
	return false;
}

DEFINE_NERVE(TNerveHanaSamboAttack, TLiveActor)
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(3);
		self->unk1B0 = 1;
	} else if (!self->checkCurAnmEnd(0)) {
	} else if (self->isBckAnm(3)) {
		self->createPollen();
		SMSGetMSound()->startSoundActor(0x291B, &self->mPosition, 0, nullptr,
		                                0, 4);
		self->setBckAnm(1);
	} else if (self->isBckAnm(1)) {
		if (spine->getTime() > self->unk198->mSLAttackingTime.get()
		    && !self->unsetUnk165()) {
			self->setBckAnm(2);
		} else {
			self->setBckAnm(1);
		}
	} else {
		spine->pushAfterCurrent(&TNerveHanaSamboWait::theNerve());
		return true;
	}

	char trash[8];
	return false;
}

DEFINE_NERVE(TNerveHanaSamboHide, TLiveActor)
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		gpMarioParticleManager->emit(0xB8, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB9, &self->mPosition, 0, nullptr);
	}

	if (spine->getTime() == 75)
		self->initFlower();

	if (self->checkCurAnmEnd(0)) {
		self->initFlower();
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->setBckAnm(6);
		self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
	}

	self->updateSquareToMario();
	f32 appearDist = self->unk198->mSLAppearDist.get();
	if (self->mDistToMarioSquared < appearDist * appearDist) {
		spine->pushAfterCurrent(&TNerveHanaSamboAppear::theNerve());
		return true;
	}

	char trash[16];
	return false;
}

DEFINE_NERVE(TNerveHanaSamboDie, TLiveActor)
{
	static int jIndexTable[] = { 1, 3, 4, 5 };

	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0) {
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		self->setDeadAnm();
	} else if (self->checkCurAnmEnd(0) || spine->getTime() > 300) {
		for (int i = 0; i < 4; ++i) {
			MtxPtr mtx
			    = self->getMActor()->getModel()->getAnmMtx(jIndexTable[i]);
			self->unk1B4[i].set(mtx[0][3], mtx[1][3], mtx[2][3]);

			JPABaseEmitter* emitter = gpMarioParticleManager->emit(
			    0xE4, &self->unk1B4[i], 0, nullptr);
			if (emitter != nullptr)
				emitter->setGlobalScale(self->mScaling);

			emitter = gpMarioParticleManager->emit(0xE6, &self->unk1B4[i], 0,
			                                       nullptr);
			if (emitter != nullptr)
				emitter->setGlobalScale(self->mScaling);
		}

		self->onLiveFlag(LIVE_FLAG_DEAD);
		self->onLiveFlag(LIVE_FLAG_UNK8);
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
		self->mHolder = nullptr;
		self->stopAnmSound();
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		spine->reset();
		spine->setNext(&TNerveSmallEnemyDie::theNerve());
		spine->pushAfterCurrent(spine->getDefault());
		self->genRandomItem();
	}

	return false;
}

DEFINE_NERVE(TNerveHanaSamboFreeze, TLiveActor)
{
	THanaSambo* self = (THanaSambo*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(5);

	if (self->checkCurAnmEnd(0)) {
		if (self->isBckAnm(5)) {
			if (self->unsetUnk165())
				self->setBckAnm(8);
		} else if (self->unsetUnk165()) {
			self->setBckAnm(8);
		} else {
			return true;
		}
	}

	return false;
}

// ============= SamboHeadRollCallback =============

static int SamboHeadRollCallback(J3DNode* node, int param_2)
{
	if (param_2 == 0) {
		if (gpCurSamboHead == nullptr || !gpCurSamboHead->isUseCallBack())
			return true;

		Mtx rotMtx;

		J3DJoint* joint = (J3DJoint*)node;
		MtxPtr anmMtx
		    = gpCurSamboHead->getModel()->getAnmMtx(joint->getJntNo());

		JGeometry::TVec3<f32> velocity = gpCurSamboHead->mVelocity;
		if (0.0f == velocity.x && 0.0f == velocity.z)
			velocity.x = 0.001f;

		JGeometry::TVec3<f32> side;
		JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
		PSVECCrossProduct(&up, &velocity, &side);

		JGeometry::TVec3<f32> zAxis(anmMtx[0][2], anmMtx[1][2], anmMtx[2][2]);
		JGeometry::TVec3<f32> yAxis(anmMtx[0][1], anmMtx[1][1], anmMtx[2][1]);
		JGeometry::TVec3<f32> xAxis(anmMtx[0][0], anmMtx[1][0], anmMtx[2][0]);

		f32 rateZ = 0.0f;
		if (0.0f != zAxis.squared())
			rateZ = side.dot(zAxis) / zAxis.squared();

		f32 rateY = 0.0f;
		if (0.0f != yAxis.squared())
			rateY = side.dot(yAxis) / yAxis.squared();

		f32 rateX = 0.0f;
		if (0.0f != xAxis.squared())
			rateX = side.dot(xAxis) / xAxis.squared();

		JGeometry::TVec3<f32> axis(rateX, rateY, rateZ);

		PSMTXRotAxisRad(rotMtx, &axis, 0.017453292f * gpCurSamboHead->unk1AC);
		PSMTXConcat(anmMtx, rotMtx, anmMtx);
		PSMTXConcat(j3dSys.mCurrentMtx, rotMtx, j3dSys.mCurrentMtx);
	}
	return true;
}

// ============= params =============

TSamboHeadSaveLoadParams::TSamboHeadSaveLoadParams(const char* path)
    : TWalkerEnemyParams(path)
    , PARAM_INIT(mSLAppearDist, 800.0f)
    , PARAM_INIT(mSLHideDist, 1000.0f)
    , PARAM_INIT(mSLMoveDist, 100.0f)
    , PARAM_INIT(mSLMoveGravity, 0.1f)
    , PARAM_INIT(mSLJumpSp, 10.0f)
    , PARAM_INIT(mSLJumpPrepareTime, 20)
    , PARAM_INIT(mSLHitJumpSpXZ, 12.0f)
    , PARAM_INIT(mSLHitJumpSpY, 10.0f)
    , PARAM_INIT(mSLHitJumpGravity, 1.0f)
    , PARAM_INIT(mSLHitJumpSpRateXZ, 0.3f)
    , PARAM_INIT(mSLHitJumpSpRateY, 0.3f)
    , PARAM_INIT(mSLJumpAngY, 30.0f)
{
	TParams::load(mPrmPath);
}

// ============= TSamboHeadManager =============

TSamboHeadManager::TSamboHeadManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurSamboHead = nullptr;
}

void TSamboHeadManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TSamboHeadSaveLoadParams("/enemy/sambohead.prm");
}

void TSamboHeadManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "samboHead.bmd", 0x10220000, 0 },
		{ nullptr, 0, 0 },
	};
	createModelDataArray(entry);
}

TSpineEnemy* TSamboHeadManager::createEnemyInstance()
{
	return new TSamboHead;
}

// ============= TSamboHead =============

TSamboHead::TSamboHead(const char* name)
    : TWalkerEnemy(name)
    , unk194(nullptr)
    , unk198(nullptr)
    , unk19C(0)
    , unk1AC(0.0f)
    , unk1B0(0)
{
}

void TSamboHead::load(JSUMemoryInputStream& stream)
{
	TSmallEnemy::load(stream);
	reset();
	TPathNode marioNode((THitActor*)gpMarioAddress);
	setGoalPath(marioNode);
	char trash[4];
}

void TSamboHead::init(TLiveManager* param_1)
{
	TWalkerEnemy::init(param_1);
	mActorType = 0x1000001B;
	unk150     = 0x11;
	unk194     = (TSamboHeadSaveLoadParams*)getSaveParam();
	mSpine->initWith(&TNerveSamboHeadHide::theNerve());
	TPathNode marioNode((THitActor*)gpMarioAddress);
	setGoalPath(marioNode);
	mMActor->setJointCallback(mBodyJntIndex, SamboHeadRollCallback);
	char trash[4];
}

void TSamboHead::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("samboHead.bmd", 3);
}

void TSamboHead::reset()
{
	gpCurSamboHead = this;
	TWalkerEnemy::reset();
	unk165 = 0;
	unk1B0 = 0;
	unk19C = 0;
	unk1AC = 0.0f;
	offLiveFlag(LIVE_FLAG_UNK8);
}

void TSamboHead::kill()
{
	mHitPoints = 1;

	if (mSpine->getCurrentNerve() != &TNerveSmallEnemyDie::theNerve()) {
		mSpine->reset();
		mSpine->setNext(&TNerveSmallEnemyDie::theNerve());
		mSpine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
	}

	onLiveFlag(LIVE_FLAG_UNK40);
}

void TSamboHead::behaveToWater(THitActor* param_1)
{
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadHide::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSamboHeadAppear::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSamboHeadHitWall::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSmallEnemyDie::theNerve())
		return;

	JGeometry::TVec3<f32> vel = mVelocity;
	vel.y                   = 0.0f;

	JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 0.0f,
	                          mPosition.z - gpMarioPos->z);
	MsVECNormalize(&dir, &dir);
	dir.scale(unk194->mSLHitJumpSpXZ.get());
	dir.y = unk194->mSLHitJumpSpY.get();

	if (mSpine->getCurrentNerve() != &TNerveSamboHeadHitWater::theNerve())
		mSpine->pushNerve(&TNerveSamboHeadHitWater::theNerve());
	else
		dir += vel;

	mVelocity = dir;
	mPosition.y += 2.0f;
	onLiveFlag(LIVE_FLAG_AIRBORNE);

	char trash[24];
}

void TSamboHead::attackToMario()
{
	char trashA[16];
	sendAttackMsgToMario();

	if (isAirborne()) {
		JGeometry::TVec3<f32> dir(mPosition.x - gpMarioPos->x, 10.0f,
		                          mPosition.z - gpMarioPos->z);
		MsVECNormalize(&dir, &dir);
		dir.scale(8.0f);
		mVelocity = dir;
	} else if (mSpine->getCurrentNerve()
	           == &TNerveSamboHeadAttack::theNerve()) {
		mSpine->pushNerve(&TNerveSmallEnemyFreeze::theNerve());
	}

	char trash[20];
}

f32 TSamboHead::getGravityY() const
{
	f32 gravity = mGravity;

	if (mSpine->getCurrentNerve() == &TNerveSamboHeadAttack::theNerve())
		gravity = unk194->mSLMoveGravity.get();

	if (mSpine->getCurrentNerve() == &TNerveSamboHeadHitWater::theNerve())
		gravity = unk194->mSLHitJumpGravity.get();

	return gravity;
}

void TSamboHead::initFlower()
{
	if (unk198 == nullptr) {
		unk198 = (TSamboFlower*)gpConductor->makeOneEnemyAppear(
		    mPosition, "サンボフラワーマネージャー", 1);
		unk198->reset();
	}
	unk198->offHitFlag(HIT_FLAG_NO_COLLISION);
	unk198->onLiveFlag(LIVE_FLAG_UNK10);
	unk198->offLiveFlag(LIVE_FLAG_DEAD);
	unk198->mPosition.y = mGroundHeight;
	unk198->mPosition   = mPosition;
}


void TSamboHead::setDeadAnm() { setBckAnm(3); }

void TSamboHead::setAfterDeadEffect()
{
	char trash[8];
	JPABaseEmitter* emitter;
	JGeometry::TVec3<f32> scale(1.5f, 1.5f, 1.5f);
	if (isBckAnm(1)) {
		emitter = gpMarioParticleManager->emit(0xE5, &mPosition, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(scale);
	} else {
		emitter = gpMarioParticleManager->emit(0xE4, &mPosition, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(scale);
	}

	emitter = gpMarioParticleManager->emit(0xE6, &mPosition, 0, nullptr);
	if (emitter != nullptr)
		emitter->setGlobalScale(scale);

	SMSGetMSound()->startSoundActor(0x295F, &mPosition, 0, nullptr, 0, 4);
}

// fabricated: never called in emitted code
void TSamboHead::setCrashAnm()
{
	setBckAnm(1);
	unk19C = 0;
	unk1AC = 0.0f;
	unk1B0 = 1;
}

void TSamboHead::calcRootMatrix()
{
	gpCurSamboHead = this;
	TSpineEnemy::calcRootMatrix();
}

void TSamboHead::genEventCoin()
{
	if (isBckAnm(1)) {
		Mtx mtx;
		MtxPtr matrix = mtx;
		for (int i = 0; i < 3; ++i) {
			MsMtxSetRotY(matrix, mRotation.y - 60.0f + 60.0f * i);
			JGeometry::TVec3<f32> offset(0.0f, 0.0f, 100.0f);
			MTXMultVec(matrix, &offset, &offset);

			TMapObjBase* coin;
			if (i == 1 && mCoin) {
				coin = mCoin;
				if (coin->isActorType(0x2000000E))
					coin = gpItemManager->makeObjAppear(0x2000000E);

				if (coin) {
					coin->appear();
					coin->mPosition = mPosition;
				}
			} else {
				coin = gpItemManager->makeObjAppear(
				    mPosition.x + offset.x, mPosition.y, mPosition.z + offset.z,
				    0x2000000E, true);
			}

			if (coin) {
				coin->mPosition.y = mPosition.y;
				MsVECNormalize(&offset, &offset);
				coin->mVelocity.set(offset.x * 4.0f,
				                    TMsRange<f32>(8.0f, 16.0f).rand(),
				                    offset.z * 4.0f);
				coin->offLiveFlag(LIVE_FLAG_UNK10);
			}
		}
	} else {
		TSmallEnemy::genEventCoin();
	}
}

bool TSamboHead::isUseCallBack()
{
	if (mSpine->getCurrentNerve() == &TNerveSamboHeadAttack::theNerve()
	    || mSpine->getCurrentNerve() == &TNerveSamboHeadHitWater::theNerve()
	    || mSpine->getCurrentNerve()
	           == &TNerveSamboHeadRecoverWater::theNerve())
		return true;
	return false;
}

const char** TSamboHead::getBasNameTable() const
{
	return sambohead_bastable;
}

// ============= nerves (TSamboHead) =============

DEFINE_NERVE(TNerveSamboHeadAppear, TLiveActor)
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->offLiveFlag(LIVE_FLAG_HIDDEN);
		self->offHitFlag(HIT_FLAG_NO_COLLISION);

		TSamboFlower* flower = self->unk198;
		if (flower->unk150) {
			flower->unk150 = 1;
			flower->unk154 = 0;
			gpMarioParticleManager->emit(0xB2, &flower->mPosition, 0,
			                             nullptr);
			flower->mMActor->setBck("flower_hit");
			flower->hide();
			self->setBckAnm(0xA);
		} else {
			self->setBckAnm(0xA);
		}

		gpMarioParticleManager->emit(0xB6, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB7, &self->mPosition, 0, nullptr);

		flower = self->unk198;
		flower->onHitFlag(HIT_FLAG_NO_COLLISION);
		flower->mMActor->setBck("flower_fwait");
		flower->onLiveFlag(LIVE_FLAG_DEAD);
	}

	if (spine->getTime() == 20) {
		TSamboFlowerManager* manager
		    = (TSamboFlowerManager*)self->unk198->mManager;
		manager->dropLeaf(self->mPosition, self->mScaling);
	}

	if (self->checkCurAnmEnd(0)) {
		self->setBckAnm(0xC);
		spine->pushAfterCurrent(&TNerveSamboHeadAttack::theNerve()); // asm: retail pushes SamboHeadAttack
		return true;
	}

	char trash[32];
	return false;
}

DEFINE_NERVE(TNerveSamboHeadAttack, TLiveActor)
{
	char trashA[16];
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (!self->isAirborne()) {
		if (self->unk19C > self->unk194->mSLJumpPrepareTime.get()
		    && self->checkCurAnmEnd(0)) {
			self->unk19C = 0;
			self->updateSquareToMario();

			JGeometry::TVec3<f32> dir = self->unk104.getPoint();
			dir.set(gpMarioPos->x - self->mPosition.x, 0.0f,
			        gpMarioPos->z - self->mPosition.z);

			if (dir.x == 0.0f && dir.y == 0.0f && dir.z == 0.0f)
				dir.x += 1.0f;

			MsVECNormalize(&dir, &dir);
			f32 moveDist = self->unk194->mSLMoveDist.get();
			dir.x = dir.x * moveDist + self->mPosition.x;
			dir.z = dir.z * moveDist + self->mPosition.z;
			dir.y = self->mPosition.y;

			f32 jumpSp  = self->unk194->mSLJumpSp.get();
			self->mVelocity = self->calcVelocityToJumpToY(
			    dir, jumpSp, self->getGravityY());

			self->mPosition.y += 2.0f;
			self->onLiveFlag(LIVE_FLAG_AIRBORNE);
			self->setBckAnm(8);
		} else {
			self->unk19C += 1;
		}

		if (self->checkCurAnmEnd(0) && self->isBckAnm(7))
			self->setBckAnm(0xC);

		MActor* actor = self->mMActor;
		actor->setFrameRate(SMSGetAnmFrameRate(), ANM_TYPE_BCK);
	} else {
		JGeometry::TVec3<f32> vel = self->mVelocity;
		if (vel.y < 0.0f && self->isBckAnm(8)) {
			self->setBckAnm(7);
			self->mMActor->setFrameRate(0.0f, ANM_TYPE_BCK);
		}
	}

	if (self->mPosition.y > self->mGroundHeight + 30.0f) {
		f32 jumpAngY                = self->unk194->mSLJumpAngY.get();
		JGeometry::TVec3<f32> vel   = self->mVelocity;
		self->unk1AC = MsClamp(MsGetRotFromZaxis(vel).x, -jumpAngY, jumpAngY);
	} else {
		self->unk1AC *= 0.8f;
	}

	f32 turnSpeed = self->mTurnSpeed;
	if (self->isAirborne())
		turnSpeed = 5.0f;

	self->walkToCurPathNode(0.0f, turnSpeed, 0.0f);
	char trash[92];
	return false;
}

DEFINE_NERVE(TNerveSamboHeadHide, TLiveActor)
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(4);
		self->onHitFlag(HIT_FLAG_NO_COLLISION);
		gpMarioParticleManager->emit(0xB8, &self->mPosition, 0, nullptr);
		gpMarioParticleManager->emit(0xB9, &self->mPosition, 0, nullptr);
	} else if (self->checkCurAnmEnd(0)) {
		self->onLiveFlag(LIVE_FLAG_HIDDEN);
		self->setBckAnm(0xC);
		self->initFlower();
	} else if (self->isFindMario(1.0f)) {
		self->updateSquareToMario();
		f32 appearDist = self->unk194->mSLAppearDist.get();
		if (self->mDistToMarioSquared < appearDist * appearDist) {
			spine->pushAfterCurrent(&TNerveSamboHeadAppear::theNerve());
			return true;
		}
	}

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	char trash[8];
	return false;
}

DEFINE_NERVE(TNerveSamboHeadHitWater, TLiveActor)
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(5);
		self->unk1A0 = self->mVelocity;
	}

	if (self->isHitWallInBound()) {
		self->unk1AC = 0.0f;
		if (self->unk1B0)
			self->mRotation.y += 180.0f;

		spine->reset();
		spine->setNext(&TNerveSamboHeadHitWall::theNerve());
		spine->pushAfterCurrent(&TNerveSamboHeadHitWall::theNerve());
		return true;
	}

	f32 jumpAngY = self->unk194->mSLJumpAngY.get();
	if (self->isBckAnm(6)) {
		if (spine->getTime() < 100)
			self->unk1AC
			    = MsClamp(self->unk1AC - 3.0f, -jumpAngY, jumpAngY);
		else
			self->unk1AC
			    = MsClamp(3.0f + self->unk1AC, -jumpAngY, 0.0f);
	} else {
		self->unk1AC = MsClamp(3.0f + self->unk1AC, -jumpAngY, jumpAngY);
	}

	if (self->isBckAnm(5) && !self->isAirborne())
		self->setBckAnm(7);

	if (self->isBckAnm(6)) {
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->mPosition.y = self->mGroundHeight + 1.0f;
		self->mVelocity   = self->unk1A0;
	}

	if (!self->isAirborne())
		self->setBckAnm(6);

	if (self->checkCurAnmEnd(0) && self->isBckAnm(6)) {
		f32 hitJumpSpRate = self->unk194->mSLHitJumpSpRateXZ.get();
		self->setBckAnm(6);
		self->unk1A0.x *= hitJumpSpRate;
		self->unk1A0.z *= hitJumpSpRate;
		self->unk1A0.y = 0.0f;
		self->mVelocity   = self->unk1A0;
		self->mPosition.y = self->mGroundHeight;
		self->offLiveFlag(LIVE_FLAG_AIRBORNE);
		self->setBckAnm(0xC);

		spine->setNext(&TNerveSamboHeadAttack::theNerve());
		spine->pushAfterCurrent(&TNerveSamboHeadRecoverWater::theNerve());
		return true;
	}

	self->walkToCurPathNode(0.0f, self->mTurnSpeed, 0.0f);
	char trash[48];
	return false;
}

DEFINE_NERVE(TNerveSamboHeadRecoverWater, TLiveActor)
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0)
		self->setBckAnm(0xC);

	self->unk1AC *= 0.99f;

	if (self->checkCurAnmEnd(0) && self->unk1AC < 1.0f)
		return true;

	return false;
}

DEFINE_NERVE(TNerveSamboHeadHitWall, TLiveActor)
{
	TSamboHead* self = (TSamboHead*)spine->getBody();

	if (spine->getTime() == 0) {
		self->setBckAnm(1);

		JPABaseEmitter* emitter = gpMarioParticleManager->emitWithRotate(
		    PARTICLE_MS_ENM_WALLHIT, &self->mPosition, 0,
		    DEG2SHORTANGLE(self->mRotation.y), 0, 0, nullptr);
		if (emitter != nullptr)
			emitter->setGlobalScale(
			    JGeometry::TVec3<f32>(1.5f, 1.5f, 1.5f));

		emitter = gpMarioParticleManager->emitWithRotate(
		    PARTICLE_MS_ENM_WALLHIT_O, &self->mPosition, 0,
		    DEG2SHORTANGLE(self->mRotation.y), 0, 0, nullptr);
		if (emitter != nullptr) {
			emitter->setGlobalScale(
			    JGeometry::TVec3<f32>(1.5f, 1.5f, 1.5f));
			SMSSetEmitterPolColor(emitter, 6);
		}
	}

	int waitTime = ((TSmallEnemyManager*)self->mManager)->unk5C;
	if (self->checkCurAnmEnd(0)) {
		J3DFrameCtrl* ctrl
		    = self->getMActor()->getFrameCtrl(ANM_TYPE_BCK);
		if (spine->getTime() > waitTime + ctrl->getEnd()) {
			self->onLiveFlag(LIVE_FLAG_DEAD);
			self->onLiveFlag(LIVE_FLAG_UNK8);
			self->onLiveFlag(LIVE_FLAG_UNK20000);
			self->offLiveFlag(TSmallEnemy::LIVE_FLAG_MELT_ON_DEATH);
			self->mHolder = nullptr;
			self->stopAnmSound();
			spine->reset();
			spine->setNext(&TNerveSmallEnemyDie::theNerve());
			spine->pushAfterCurrent(&TNerveSmallEnemyDie::theNerve());
			self->genRandomItem();
			return true;
		}
	}

	return false;
}
