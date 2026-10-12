#include <Enemy/HauntLeg.hpp>
#include <Enemy/Walker.hpp>
#include <Enemy/Graph.hpp>
#include <Enemy/Spider.hpp>
#include <Enemy/Conductor.hpp>
#include <Map/MapCollisionData.hpp>
#include <M3DUtil/MActor.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/PacketUtil.hpp>
#include <MarioUtil/RandomUtil.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/Strategy.hpp>
#include <JSystem/JMath.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>

const TNerveHauntLegHaunt& TNerveHauntLegHaunt::theNerve()
{
	static TNerveHauntLegHaunt instance;
	return instance;
}

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

static THauntLeg* gpCurHauntLeg;

static const char* hauntleg_bastable[] = {
	nullptr,
	nullptr,
	nullptr,
};

static BOOL HauntLegCallback(J3DNode* node, int param_2)
{
	char trashA[8];
	if (param_2 == 0) {
		if (gpCurHauntLeg == nullptr || !gpCurHauntLeg->isHaunting())
			return true;

		MtxPtr jointMtx = gpCurHauntLeg->mMActor->getModel()->getAnmMtx(
		    ((J3DJoint*)node)->getJntNo());

		Mtx rotation;
		char trashB[8];
		MsMtxSetRotZ(rotation, gpCurHauntLeg->mSpinAngle);

		MTXConcat(jointMtx, rotation, jointMtx);
		MTXConcat(J3DSys::mCurrentMtx, rotation, J3DSys::mCurrentMtx);
	}

	return true;
}

THauntLegManager::THauntLegManager(const char* name)
    : TSmallEnemyManager(name)
{
	gpCurHauntLeg = nullptr;
}

void THauntLegManager::load(JSUMemoryInputStream& stream)
{
	TSmallEnemyManager::load(stream);
	unk38 = new TWalkerEnemyParams("/enemy/hauntLeg.prm");
}

TSmallEnemy* THauntLegManager::createEnemyInstance() { return new THauntLeg; }

void THauntLegManager::initSetEnemies()
{
	char trash[8];
	static const GXColorS10 tevColorData1[8] = {
		{ 0x0, 0x0, 0x78, 0xFF },  { 0x78, 0x0, 0x0, 0xFF },
		{ 0x0, 0x78, 0x0, 0xFF },  { 0x78, 0x78, 0x0, 0xFF },
		{ 0x78, 0x0, 0x78, 0xFF }, { 0x64, 0xC8, 0x0, 0xFF },
		{ 0x0, 0x64, 0xC8, 0xFF }, { 0xC8, 0x64, 0x96, 0xFF },
	};

	static const GXColorS10 tevColorData2[8] = {
		{ 0x0, 0x0, 0xFA, 0xFF },  { 0xFA, 0x0, 0x0, 0xFF },
		{ 0x0, 0xFA, 0x0, 0xFF },  { 0xFA, 0xFA, 0x0, 0xFF },
		{ 0xFA, 0x0, 0xFA, 0xFF }, { 0x96, 0xFA, 0x0, 0xFF },
		{ 0x0, 0x96, 0xFA, 0xFF }, { 0xFA, 0x96, 0xC8, 0xFF },
	};

	int colorIdx = 0;
	for (int i = 0; i < mObjNum; ++i) {
		TGraphWeb* graph = gpConductor->getGraphByName("main");
		THauntLeg* leg   = (THauntLeg*)getObj(i);

		TMsRange<s32> nodeRange(0, graph->getNodeNum());
		JGeometry::TVec3<f32> point;
		graph->getGraphNode(nodeRange.rand()).getPoint(point);
		leg->mPosition = point;
		leg->mPosition.y += 5.0f;
		leg->onLiveFlag(LIVE_FLAG_AIRBORNE);
		leg->reset();

		for (u16 j = 0;
		     j < leg->mMActor->getModel()->getModelData()->getMaterialNum();
		     ++j) {
			SMS_InitPacket_TwoTevColor(
			    ((THauntLeg*)getObj(i))->mMActor->getModel(), j, GX_TEVREG0,
			    &tevColorData1[colorIdx], GX_TEVREG1, &tevColorData2[colorIdx]);
		}

		++colorIdx;
		if (colorIdx >= 8)
			colorIdx = 0;
	}
}

void THauntLegManager::createModelData()
{
	static TModelDataLoadEntry entry[] = {
		{ "hauntleg.bmd", 0x10220000, 0 },
#ifdef TARGET_PC
		// The original array has no terminator and only stops at the first
		// word of the following .data object (a zero vtable word); that layout
		// does not exist on the host.
		{ nullptr, 0, 0 },
#endif
	};
	createModelDataArray(entry);
}

BOOL THauntedObject::receiveMessage(THitActor* sender, u32 message)
{
	if (message <= HIT_MESSAGE_HIP_DROP) {
		mOwner->kill();
		return true;
	}

	if (message == HIT_MESSAGE_SPRAYED_BY_WATER)
		return true;

	return false;
}

void THauntLeg::init(TLiveManager* manager)
{
	char trash[8];
	TWalkerEnemy::init(manager);

	mActorType = 0x10000025;
	unk150     = 0x3A;
	onHitFlag(0x60000000);
	getWalker()->setMode(1);
	unk130 = 2;
	mMActor->setJointCallback(1, &HauntLegCallback);

	mHauntedObject = new THauntedObject("ハントオブジェクト");

	TIdxGroupObj* group
	    = static_cast<TIdxGroupObj*>(JDrama::TNameRefGen::search("敵グループ"));
	group->getChildren().push_back(mHauntedObject);

	f32 radius = 30.0f * mBodyScale;
	mHauntedObject->initHitActor(0x10000025, 2, -0x80000000, radius, radius,
	                             radius, radius);
	mHauntedObject->mOwner = this;
}

void THauntLeg::setMActorAndKeeper()
{
	mMActorKeeper = new TMActorKeeper(mManager, 1);
	mMActor       = mMActorKeeper->createMActor("hauntleg.bmd", 3);
}

void THauntLeg::reset()
{
	mTarget      = nullptr;
	mHasCaught   = false;
	mIsFirstJump = true;
	TWalkerEnemy::reset();
}

static inline void checkHit(THauntedObject* obj)
{
	for (int i = obj->getColNum(); i > 0; --i) { }
}

void THauntLeg::calcRootMatrix()
{
	gpCurHauntLeg = this;

	if (isEaten())
		return;

	getModel()->setBaseScale(mScaling);

	MtxPtr anmMtx = getModel()->getBaseTRMtx();

	if (getWalker()->unk2C->unk10 > 0.0f && unk138 != nullptr) {
		JGeometry::TVec3<f32> local_30(0.0f, 1.0f, 0.0f);

		const JGeometry::TVec3<f32> normal = unk138->getNormal();

		JGeometry::TVec3<f32> local_a0;
		local_a0.cross(normal, local_30);
		MsVECNormalize(&local_a0, &local_a0);

		local_30.cross(local_a0, normal);
		MsVECNormalize(&local_30, &local_30);

		anmMtx[0][0] = local_a0.x;
		anmMtx[1][0] = local_a0.y;
		anmMtx[2][0] = local_a0.z;

		anmMtx[0][1] = normal.x;
		anmMtx[1][1] = normal.y;
		anmMtx[2][1] = normal.z;

		anmMtx[0][2] = local_30.x;
		anmMtx[1][2] = local_30.y;
		anmMtx[2][2] = local_30.z;

		anmMtx[0][3] = 0.0f;
		anmMtx[1][3] = 0.0f;
		anmMtx[2][3] = 0.0f;

		f32 angle = (1.0f - getWalker()->unk2C->unk10) * 90.0f;

		Mtx rotation;
		MsMtxSetRotX(rotation, angle);

		MTXConcat(anmMtx, rotation, anmMtx);
	} else {
		JGeometry::TVec3<f32> local_f0(MsSin(mRotation.y), 0.0f,
		                               MsCos(mRotation.y));

		const JGeometry::TVec3<f32> normal = mGroundPlane->getNormal();

		JGeometry::TVec3<f32> local_d8;
		local_d8.cross(normal, local_f0);
		MsVECNormalize(&local_d8, &local_d8);

		local_f0.cross(local_d8, normal);
		MsVECNormalize(&local_f0, &local_f0);

		anmMtx[0][0] = local_d8.x;
		anmMtx[1][0] = local_d8.y;
		anmMtx[2][0] = local_d8.z;

		anmMtx[0][1] = normal.x;
		anmMtx[1][1] = normal.y;
		anmMtx[2][1] = normal.z;

		anmMtx[0][2] = local_f0.x;
		anmMtx[1][2] = local_f0.y;
		anmMtx[2][2] = local_f0.z;
	}

	anmMtx[0][3] = mPosition.x;
	anmMtx[1][3] = mPosition.y;
	anmMtx[2][3] = mPosition.z;

	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		mHauntedObject->mPosition = mPosition;
	} else {
		MtxPtr jointMtx = getModel()->getAnmMtx(2);
		mHauntedObject->mPosition.x = jointMtx[0][3];
		mHauntedObject->mPosition.y = jointMtx[1][3];
		mHauntedObject->mPosition.z = jointMtx[2][3];
	}

	if (0) {
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
		(void)(GXColor){1, 2, 3, 4};
	}
	checkHit(mHauntedObject);
}

void THauntLeg::setGenerateAnm() { setBckAnm(0); }

void THauntLeg::setWaitAnm() { setBckAnm(2); }

void THauntLeg::setWalkAnm() { setBckAnm(1); }

void THauntLeg::setRunAnm() { setBckAnm(1); }

void THauntLeg::setDeadAnm()
{
	if (mTarget != nullptr) {
		mTarget->receiveMessage(this, HIT_MESSAGE_PUT);
		mHolder     = nullptr;
		mHeldObject = nullptr;
	}

	mHauntedObject->onHitFlag(HIT_FLAG_NO_COLLISION);
}

void THauntLeg::attackToMario()
{
	char trash[8];
	updateSquareToMario();
	if (mDistToMarioSquared < 10000.0f)
		sendAttackMsgToMario();
}

bool THauntLeg::isCollidMove(THitActor* other)
{
	char trash[8];
	if (mSpine->getCurrentNerve() != &TNerveHauntLegHaunt::theNerve()
	    && !mHasCaught && !checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		u32 type = other->mActorType & ACTOR_TYPE_MASK;
		if (type == ACTOR_TYPE_UNK20000000 || type == ACTOR_TYPE_UNK40000000) {
			TTakeActor* actor = static_cast<TTakeActor*>(other);
			if (actor->mHolder == nullptr || actor != mTarget) {
				mTarget = actor;
				mSpine->setNext(&TNerveHauntLegHaunt::theNerve());
			}
			return false;
		}
	}

	return false;
}

const char** THauntLeg::getBasNameTable() const { return hauntleg_bastable; }

MtxPtr THauntLeg::getTakingMtx()
{
	char trash[8];
	if (checkLiveFlag(LIVE_FLAG_CLIPPED_OUT)) {
		TPosition3f mat;
		mat.translation(mPosition.x, mPosition.y, mPosition.z);
		MTXCopy(mat, mMActor->getModel()->getBaseTRMtx());
		return mMActor->getModel()->getBaseTRMtx();
	}

	return mMActor->getModel()->getAnmMtx(2);
}

BOOL TNerveHauntLegHaunt::execute(TSpineBase<TLiveActor>* spine) const
{
	char trash[16];
	THauntLeg* self = (THauntLeg*)spine->getBody();

	if (spine->getTime() == 0) {
		f32 speed = 10.0f;
		self->mJumpVelocity = self->calcVelocityToJumpToY(
		    self->mTarget->mPosition, speed, self->getGravityY());
		self->mVelocity = self->mJumpVelocity;
		self->mPosition.y += 10.0f;
		self->onLiveFlag(LIVE_FLAG_AIRBORNE);
		self->mIsFirstJump = true;
	} else if (!self->isAirborne()) {
		if (self->mIsFirstJump) {
			self->mVelocity = self->mJumpVelocity;
			self->mPosition.y += 10.0f;
			self->mIsFirstJump = false;

			JGeometry::TVec3<f32> delta
			    = self->mPosition - self->mTarget->mPosition;
			f32 dist = delta.length();
			if (dist < 200.0f) {
				if (self->mTarget->mHolder == nullptr) {
					if (self->mTarget->receiveMessage(self, HIT_MESSAGE_TAKE)) {
						self->mHeldObject = self->mTarget;
						self->mHasCaught  = true;
					}
				}
			}
		} else {
			self->mSpinAngle = 0.0f;
			spine->pushAfterCurrent(&TNerveWalkerGraphWander::theNerve());
			return true;
		}
	}

	if (self->isAirborne()) {
		if (self->mIsFirstJump) {
			f32 angle = 2.0f + self->mSpinAngle;
			if (angle > 180.0f)
				angle = 180.0f;
			else if (angle < 0.0f)
				angle = 0.0f;
			self->mSpinAngle = angle;
		} else {
			f32 angle = 2.0f + self->mSpinAngle;
			if (angle > 360.0f)
				angle = 360.0f;
			else if (angle < 0.0f)
				angle = 0.0f;
			self->mSpinAngle = angle;
		}
	}

	return false;
}
