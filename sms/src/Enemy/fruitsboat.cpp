#include <Enemy/FruitsBoat.hpp>
#include <Enemy/Conductor.hpp>
#include <Enemy/Graph.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DAnimation.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JMath.hpp>
#include <M3DUtil/MActor.hpp>
#include <MSound/MSound.hpp>
#include <MSound/MSoundSE.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MarioUtil/ShadowUtil.hpp>
#include <Map/Map.hpp>
#include <Map/MapCollisionManager.hpp>
#include <Map/MapData.hpp>
#include <MoveBG/MapObjWave.hpp>
#include <Player/MarioAccess.hpp>
#include <Player/Yoshi.hpp>
#include <Strategic/ObjModel.hpp>
#include <Strategic/Spine.hpp>
#include <Strategic/question.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

class TNerveFruitsBoatBckTrace : public TNerveBase<TLiveActor> {
public:
	TNerveFruitsBoatBckTrace() { }
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
};

class TNerveFruitsBoatGraphWander : public TNerveBase<TLiveActor> {
public:
	TNerveFruitsBoatGraphWander() { }
	virtual BOOL execute(TSpineBase<TLiveActor>*) const;
};

// The retail object keeps the singleton of each nerve as a plain local static
// (instance$NNNN) and never emits a theNerve() body, so these accessors have
// internal linkage instead of being static members. They are defined further
// down so that their statics are created after those of moveObject.
static inline const TNerveFruitsBoatBckTrace& theNerveBckTrace();
static inline const TNerveFruitsBoatGraphWander& theNerveGraphWander();

TFruitsBoat::TFruitsBoat(const char* name)
    : TSpineEnemy(name)
{
	mDirFlip       = 0;
	mShadowRadiusX = 800.0f;
	mShadowRadiusZ = 800.0f;
	mBckTrack      = nullptr;
	mBckFrameCtrl  = nullptr;
	mTiltAxis.x    = 1.0f;
	mTiltAxis.y    = 0.0f;
	mTiltAxis.z    = 0.0f;
	mTiltAngle     = 0.0f;
	mTiltSpeed     = 0.0f;
	onLiveFlag(LIVE_FLAG_UNK10);
}

int TFruitsBoat::setBckTrack(const char* name)
{
	MActorAnmDataEach<J3DAnmTransformKey>* anms
	    = mManager->getMActorAnmData()->getUnk2C();

	for (int i = 0; i < anms->getAnmNum(); ++i) {
		if (strcmp(name, anms->getName(i)) != 0)
			continue;

		mBckTrack     = anms->getAnmPtr(i);
		mBckFrameCtrl = new J3DFrameCtrl;
		mBckFrameCtrl->init(mBckTrack->getFrameMax());
		mBckFrameCtrl->setAttribute(mBckTrack->getAttribute());
		f32 speed = getSaveParam2()->mSLBckMoveSpeed.get();
		mBckFrameCtrl->setRate(speed * SMSGetAnmFrameRate());
		return 0;
	}

	return -1;
}

Mtx* TFruitsBoat::getRootJointMtx() const
{
	return (Mtx*)mMActor->getModel()->getAnmMtx(0);
}

void TFruitsBoat::load(JSUMemoryInputStream& stream)
{
	JDrama::TActor::load(stream);

	char buffer[256];
	stream.readString(buffer, 256);
	TLiveManager* mgr
	    = static_cast<TLiveManager*>(JDrama::TNameRefGen::search(buffer));

	char buffer2[256];
	stream.readString(buffer2, 256);
	TGraphWeb* graph = gpConductor->getGraphByName(buffer2);
	unk124->init(graph);

	mGroundPlane = TMap::getIllegalCheckData();

	init(mgr);

	if (graph == nullptr || graph->isDummy()) {
		if (setBckTrack(buffer2) == 0)
			mSpine->initWith(&theNerveBckTrace());
	}
}

void TFruitsBoat::init(TLiveManager* manager)
{
	mManager = manager;
	mManager->manageActor(this);
	mMActorKeeper = new TMActorKeeper(mManager, 1);

	initHitActor(0x4000007b, 1, 0xc0000000, 0.0f, 0.0f, 0.0f, 0.0f);
	offHitFlag(HIT_FLAG_NO_COLLISION);

	unk124->reset();
	goToShortestNextGraphNode();
	if (unk124->unk0->unk14) {
		mPosition = unk124->getCurrentPos();
		unk124->moveToShortestNext();
	}

	switch (getModelType()) {
	case 0:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboat", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic.bmd", 0);
		mMapCollisionManager->init("ShipDolpic.col", 1, nullptr);
		mAttackRadius = 850.0f;
		calcEntryRadius();
		mAttackHeight = 600.0f;
		calcEntryRadius();
		break;
	case 1:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatb", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic2.bmd", 0);
		mMapCollisionManager->init("ShipDolpic2.col", 1, nullptr);
		mAttackRadius = 750.0f;
		calcEntryRadius();
		mAttackHeight = 480.0f;
		calcEntryRadius();
		break;
	case 2:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatc", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic3.bmd", 0);
		mMapCollisionManager->init("ShipDolpic3.col", 1, nullptr);
		mAttackRadius = 1000.0f;
		calcEntryRadius();
		mAttackHeight = 300.0f;
		calcEntryRadius();
		break;
	case 3:
	default:
		mMapCollisionManager
		    = new TMapCollisionManager(1, "/scene/fruitsboatd", this);
		mMActor = mMActorKeeper->createMActor("ShipDolpic4.bmd", 0);
		mMapCollisionManager->init("ShipDolpic4.col", 1, nullptr);
		mAttackRadius = 760.0f;
		calcEntryRadius();
		mAttackHeight = 270.0f;
		calcEntryRadius();
		break;
	}

	mMapCollisionManager->setUpUnk8TRS(mPosition, mRotation, mScaling);

	mSpine->initWith(&theNerveGraphWander());

	if (unk124->getGraph() == nullptr)
		onLiveFlag(LIVE_FLAG_UNK10000);
	else if (unk124->getCurrent().checkFlag(0x80))
		onLiveFlag(LIVE_FLAG_UNK10000);
	else
		offLiveFlag(LIVE_FLAG_UNK10000);

	mMarchSpeed = getSaveParam2()->mSLMoveSpeed.get();
	mTurnSpeed  = getSaveParam2()->mSLRotSpeed.get();
	offLiveFlag(LIVE_FLAG_CLIPPED_OUT);
	onLiveFlag(LIVE_FLAG_UNK20);
	offLiveFlag(LIVE_FLAG_UNK100);
	mMActor->setLightType(2);
	calcRootMatrix();
	mMActor->calc();
}

BOOL TFruitsBoat::receiveMessage(THitActor*, u32) { return FALSE; }

void TFruitsBoat::setGroundCollision()
{
	JGeometry::TVec3<f32> toMario = mPosition;
	toMario -= *gpMarioPos;
	char trash[8];

	if (mColCount != 0 || toMario.length() < 1000.0f
	    || (SMS_GetYoshi()->isHatched()
	        && mPosition.x - 1000.0f < SMS_GetYoshi()->getTranslation().x
	        && 1000.0f + mPosition.x > SMS_GetYoshi()->getTranslation().x
	        && mPosition.z - 1000.0f < SMS_GetYoshi()->getTranslation().z
	        && 1000.0f + mPosition.z > SMS_GetYoshi()->getTranslation().z)) {
		MtxPtr mtx = getModel()->getAnmMtx(0);
		if (mMapCollisionManager->unk8)
			mMapCollisionManager->unk8->moveMtx(mtx);
	}
}

void TFruitsBoat::calcRootMatrix()
{
	J3DModel* model = getModel();
	MtxPtr mtx      = model->getBaseTRMtx();
	MsMtxSetRotRPH(mtx, mRotation.x, mRotation.y, mRotation.z);

	Mtx tilt;
	MTXRotAxisRad(tilt, &mTiltAxis, 0.017453292f * mTiltAngle);
	MTXConcat(tilt, mtx, mtx);
	MTXTransApply(mtx, mtx, mPosition.x, mPosition.y, mPosition.z);
	model->setBaseScale(mScaling);
}

// fabricated: min/max taking their operands by reference. A literal passed this
// way is materialised in .sdata, like the retail object's clamp limits.
template <class T> static inline T minRef(const T& a, const T& b)
{
	return a > b ? b : a;
}

template <class T> static inline T maxRef(const T& a, const T& b)
{
	return a > b ? a : b;
}

// fabricated: horizontal vector of the given length along the yaw (degrees)
static inline JGeometry::TVec3<f32> polarXZ(f32 theta, f32 radius)
{
	f32 c = radius * MsCos(theta);
	f32 s = radius * MsSin(theta);
	return JGeometry::TVec3<f32>(s, 0.0f, c);
}

// fabricated: plays the idle bck for this boat variant while Mario rides it
static inline void setRideBck(TFruitsBoat* self)
{
	const char* name;
	switch (self->getModelType()) {
	case 0:
		name = "shipdolpic";
		break;
	case 1:
		name = "shipdolpic2";
		break;
	case 2:
		name = "shipdolpic3";
		break;
	case 3:
	default:
		return;
	}

	if (!self->mMActor->checkCurAnm(name, ANM_TYPE_BCK)
	    || self->mMActor->curAnmEndsNext(ANM_TYPE_BCK, nullptr))
		self->mMActor->setBck(name);
}

void TFruitsBoat::moveObject()
{
	JGeometry::TVec3<f32> dir = polarXZ(mRotation.y, 300.0f);

	// sample the waves at the bow and the stern to pitch the boat
	JGeometry::TVec3<f32> pos = mPosition;
	JGeometry::TVec3<f32> front = pos;
	front += dir;
	JGeometry::TVec3<f32> back = pos;
	back -= dir;
	front.y = gpMapObjWave->getWaveHeight(front.x, front.z);
	back.y  = gpMapObjWave->getWaveHeight(back.x, back.z);

	JGeometry::TVec3<f32> diff  = front - back;
	JGeometry::TVec3<f32> pitch;
	pitch = MsGetRotFromZaxis(diff);
	pitch.x *= 0.5f;

	f32 delta = MsAngleDiff(pitch.x, mRotation.x);
	if (delta >= 0.0f)
		delta = minRef(delta, 1.0f);
	else
		delta = maxRef(delta, -1.0f);
	mRotation.x += delta;

	const TBGCheckData* plane = SMS_GetMarioGrPlane();

	if (!(mLiveFlag & LIVE_FLAG_UNK20000)) {
		if (plane != nullptr && plane->getActor() == this
		    && SMS_IsMarioTouchGround4cm()) {
			JGeometry::TVec3<f32> toMario = *gpMarioPos;
			toMario -= mPosition;
			toMario.y = 0.0f;

			f32 dist = toMario.length();
			if (0.0f != dist) {
				static JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
				toMario.normalize();
				mTiltAxis.cross(up, toMario);
				mTiltAxis.normalize();
				mTiltSpeed += 0.0003f * dist;
			}

			onLiveFlag(LIVE_FLAG_UNK20000);
			offLiveFlag(LIVE_FLAG_UNK10000);

			if (plane->isBounceOnLanding()) {
				setRideBck(this);
				offLiveFlag(LIVE_FLAG_UNK20000);
			}
		}
	} else {
		if (!(plane != nullptr && plane->getActor() == this
		      && SMS_IsMarioTouchGround4cm()))
			offLiveFlag(LIVE_FLAG_UNK20000);
	}

	if (mLiveFlag & LIVE_FLAG_UNK20000) {
		JGeometry::TVec3<f32> toMario = *gpMarioPos;
		toMario -= mPosition;
		toMario.y = 0.0f;

		if (0.0f != toMario.length()) {
			toMario.normalize();
			static JGeometry::TVec3<f32> up(0.0f, 1.0f, 0.0f);
			JGeometry::TVec3<f32> axis;
			axis.cross(up, toMario);
			axis.normalize();
			f32 dx = 0.1f * (axis.x - mTiltAxis.x);
			mTiltAxis.x += dx;
			f32 dy = 0.1f * (axis.y - mTiltAxis.y);
			mTiltAxis.y += dy;
			f32 dz = 0.1f * (axis.z - mTiltAxis.z);
			mTiltAxis.z += dz;
		}
	}

	f32 sinVal = -MsSin(mTiltAngle);
	mTiltSpeed = 0.01f * sinVal + mTiltSpeed;
	mTiltAngle = mTiltAngle + mTiltSpeed;
	if (mTiltAngle < -10.0f || mTiltAngle > 10.0f)
		mTiltSpeed = -mTiltSpeed;
	mTiltSpeed *= 0.99f;

	TLiveActor::moveObject();
}

void TFruitsBoat::requestShadow()
{
	if (checkLiveFlag(LIVE_FLAG_DEAD | LIVE_FLAG_HIDDEN | LIVE_FLAG_UNK8))
		return;

	if (!checkLiveFlag(LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT)
	    || checkLiveFlag(LIVE_FLAG_UNK400)) {
		TCircleShadowRequest request;
		request.mPosition    = mPosition;
		request.mRadiusX     = mShadowRadiusX;
		request.mRadiusZ     = mShadowRadiusZ;
		request.mShadowType  = SHADOW_TYPE_SHIP;
		request.mRotationY   = (s16)mRotation.y;

		if (checkLiveFlag(LIVE_FLAG_UNK400))
			gpBindShadowManager->forceRequest(request, getActorType());
		else
			gpBindShadowManager->request(request, getActorType());
	}

	if (!checkLiveFlag(LIVE_FLAG_UNK200 | LIVE_FLAG_CLIPPED_OUT)
	    && !checkActorType(ACTOR_TYPE_UNK40000000))
		gpQuestionManager->request(mPosition, mScaledBodyRadius);
}

TFruitsBoatManager::TFruitsBoatManager(int model_type, const char* name)
    : TEnemyManager(name)
{
	mModelType = model_type;
}

void TFruitsBoatManager::createModelData()
{
	switch (mModelType) {
	case 0: {
		static const TModelDataLoadEntry entry[] = {
			{ "ShipDolpic.bmd", 0x10210000, 0 },
			{ nullptr, 0, 0 },
		};
		createModelDataArray(entry);
		break;
	}
	case 1: {
		static const TModelDataLoadEntry entry[] = {
			{ "ShipDolpic2.bmd", 0x10210000, 0 },
			{ nullptr, 0, 0 },
		};
		createModelDataArray(entry);
		break;
	}
	case 2: {
		static const TModelDataLoadEntry entry[] = {
			{ "ShipDolpic3.bmd", 0x10210000, 0 },
			{ nullptr, 0, 0 },
		};
		createModelDataArray(entry);
		break;
	}
	case 3:
	default: {
		static const TModelDataLoadEntry entry[] = {
			{ "ShipDolpic4.bmd", 0x10210000, 0 },
			{ nullptr, 0, 0 },
		};
		createModelDataArray(entry);
		break;
	}
	}
}

void TFruitsBoatManager::load(JSUMemoryInputStream& stream)
{
	unk38 = new TFruitsBoatParams("/enemy/fruitsBoat.prm");
	TEnemyManager::load(stream);
}

TSpineEnemy* TFruitsBoatManager::createEnemyInstance() { return nullptr; }

static inline const TNerveFruitsBoatGraphWander& theNerveGraphWander()
{
	static TNerveFruitsBoatGraphWander instance;
	return instance;
}

static inline const TNerveFruitsBoatBckTrace& theNerveBckTrace()
{
	static TNerveFruitsBoatBckTrace instance;
	return instance;
}

// fabricated: the original duplicates this block in both branches of
// TNerveFruitsBoatGraphWander::execute
static inline void sailAlongGraph(TFruitsBoat* self)
{
	f32 march = self->mMarchSpeed;
	if (self->unk124->unk0->unk14 ? TRUE : FALSE) {
		f32 speed = self->unk124->calcSplineSpeed(march);
		self->unk124->traceSpline(speed);

		JGeometry::TVec3<f32> pos;
		JGeometry::TVec3<f32> rot;
		self->unk124->unk0->unk14->getPosAndRot(self->unk124->unk14, &pos,
		                                        &rot);
		pos -= self->mPosition;
		self->mLinearVelocity += pos;
		self->mRotation.y = rot.y;
		if (speed < 0.0f)
			self->mRotation.y = MsAngleWrap(180.0f + self->mRotation.y);
	} else {
		self->walkToCurPathNode(march, self->mTurnSpeed, 0.0f);
	}

	if (gpMSound->gateCheck(MSD_SE_OBJ_PONPONSEN))
		MSoundSESystem::MSoundSE::startSoundActor(
		    MSD_SE_OBJ_PONPONSEN, &self->mPosition, 0, nullptr, 0, 4);
}

BOOL TNerveFruitsBoatGraphWander::execute(TSpineBase<TLiveActor>* spine) const
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();

	if (self->unk124->getGraph() == nullptr
	    || self->unk124->getGraph()->isDummy())
		return FALSE;

	if (self->isReachedToGoal()) {
		TGraphNode& node = self->unk124->getCurrent();
		if (node.checkFlag(0x100))
			self->onLiveFlag(LIVE_FLAG_UNK10000);
		if (node.checkFlag(0x400))
			self->mDirFlip ^= 1;

		JGeometry::TVec3<f32> dir;
		f32 radius = 1.0f;
		f32 theta  = self->mRotation.y;
		dir.x      = radius * MsSin(theta);
		dir.y      = 0.0f;
		dir.z      = radius * MsCos(theta);
		self->goToDirectedNextGraphNode(dir);

		if (!self->checkLiveFlag(LIVE_FLAG_UNK10000))
			sailAlongGraph(self);

		spine->pushAfterCurrent(&theNerveGraphWander());
		return TRUE;
	}

	if (self->checkLiveFlag(LIVE_FLAG_UNK10000))
		return FALSE;

	sailAlongGraph(self);
	return FALSE;
}

// Follows the baked bck animation of the boat's two joints: joint 0 is the
// path, joint 1 the local offset on top of it.
BOOL TNerveFruitsBoatBckTrace::execute(TSpineBase<TLiveActor>* spine) const
{
	TFruitsBoat* self = (TFruitsBoat*)spine->getBody();
	char trash[8];

	self->mBckFrameCtrl->update();
	self->mBckTrack->setFrame(self->mBckFrameCtrl->getFrame());

	J3DTransformInfo local;
	J3DTransformInfo path;
	self->mBckTrack->getTransform(0, &path);
	self->mBckTrack->getTransform(1, &local);

	self->mPosition.x = path.mTranslate.x + local.mTranslate.x;
	self->mPosition.y = path.mTranslate.y + local.mTranslate.y;
	self->mPosition.z = path.mTranslate.z + local.mTranslate.z;

	self->mRotation.x = (360.0f / 65536.0f)
	                    * (path.mRotation.x + local.mRotation.x);
	self->mRotation.y = (360.0f / 65536.0f)
	                    * (path.mRotation.y + local.mRotation.y);
	self->mRotation.z = (360.0f / 65536.0f)
	                    * (path.mRotation.z + local.mRotation.z);

	self->mScaling.x = path.mScale.x * local.mScale.x;
	self->mScaling.y = path.mScale.y * local.mScale.y;
	self->mScaling.z = path.mScale.z * local.mScale.z;

	return FALSE;
}
