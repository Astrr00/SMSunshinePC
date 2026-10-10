#include <Map/PollutionManager.hpp>
#include <Map/PollutionLayer.hpp>
#include <Map/MapEventSink.hpp>
#include <System/MarDirector.hpp>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

int TPollutionManager::mFlushTime = 200;
u8 TPollutionManager::mEdgeAlpha  = 50;

TPollutionManager* gpPollution;

void TPollutionManager::stampModel(J3DModel* model)
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->getPlaneType() == 0)
			getLayer(i)->stampModel(model);
}

void TPollutionManager::stamp(u16 stamp_type, f32 x, f32 y, f32 z, f32 size)
{
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		layer->stamp(stamp_type, x, y, z, size);
	}
}

void TPollutionManager::clean(f32 x, f32 y, f32 z, f32 size)
{
	char trash[8];
	if (gpMarDirector->getCurrentMap() == 1 && y < -10.0f)
		return;

	stamp(0, x, y, z, size);
}

void TPollutionManager::stampGround(u16 stamp_type, f32 x, f32 y, f32 z,
                                    f32 size)
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->getPlaneType() == 0)
			getLayer(i)->stamp(stamp_type, x, y, z, size);
}

int TPollutionManager::getPollutionType(f32 x, f32 y, f32 z) const
{
	for (int i = 0; i < getJointModelNum(); ++i)
		if (getLayer(i)->isInArea(x, y, z))
			return getLayer(i)->getPollutionType();

	return POLLUTION_TYPE_UNK10;
}

u32 TPollutionManager::getPollutionDegree() const
{
	u32 totalDegree = 0;
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		totalDegree += layer->getPollutionDegree();
	}
	return totalDegree;
}

void TPollutionManager::isProhibit(f32 x, f32 y, f32 z) const { }

bool TPollutionManager::isPolluted(f32 x, f32 y, f32 z) const
{
	for (int i = 0; i < getJointModelNum(); ++i) {
		TPollutionLayer* layer = getLayer(i);
		if (layer->isPolluted(x, y, z))
			return true;
	}

	return false;
}

void TPollutionManager::subtractFromYMap(f32, f32, f32) const { }

static void dummy()
{
	(Vec) { 0.0f, 0.0f, 0.0f };
	(Vec) { 1.0f, 1.0f, 1.0f };
}

bool TPollutionManager::cleanedAll() const
{
	return getPollutionDegree() < TMapEventSink::mCleanedDegree ? true : false;
}

void TPollutionManager::draw() { }

void TPollutionManager::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (cue & CUE_UNK1000000) {
		getCounterObj().countObjDegree();
	} else if (cue & CUE_SEMITRANSPARENT_PRIO_2) {
		u8 layer = cue >> CUE_OFFSET_POLLUTION_LAYER;
		if (layer == 0)
			getCounterLayer().calcViewMtx();

		getCounterLayer().countTexDegree(layer);

		int last = getJointModelNum() - 1;
		if (layer == last)
			getCounterLayer().resetTask();
	} else {
		TJointModelManager::perform(cue, graphics);
	}
}

TJointModel* TPollutionManager::newJointModel(int param_1) const
{
	switch (getLayerInfo(param_1)->mPlaneType) {
	case 0:
		return new TPollutionLayer;
		break;
	case 1:
		return new TPollutionLayer;
		break;
	case 4:
		return new TPollutionLayerWallPlusZ;
		break;
	case 5:
		return new TPollutionLayerWallMinusZ;
		break;
	case 2:
		return new TPollutionLayerWallPlusX;
		break;
	case 3:
		return new TPollutionLayerWallMinusX;
		break;
	case 6:
		return new TPollutionLayerWave;
		break;
	default:
		return nullptr;
	}
}

#ifdef TARGET_PC
// ymap.ymp: the layer records are 0x2C bytes on disc but TPollutionLayerInfo is 0x30 bytes on the host (the
// mHeightMap pointer), so the disc image is neither swapped nor relocated in place. A host array is built from the
// big-endian records instead (no marker needed: the source is never modified, calling this twice just builds two
// arrays). chk_ymp.py proves the 0x2C layout and that the height maps lie inside the file.
static f32 rdbeF32(const u8* p)
{
	u32 u = rdbe32(p);
	f32 f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

void TPollutionManager::setDataAddress(TPollutionManager::TPollutionInfo* info)
{
	const u8* base = (const u8*)info;
	const u8* rec  = base + rdbe32(base + 4);
	mLayerInfos    = new TPollutionLayerInfo[mJointModelNum];
	for (int i = 0; i < mJointModelNum; ++i, rec += 0x2C) {
		TPollutionLayerInfo& dst = mLayerInfos[i];
		dst.mPollutionType       = rdbe16(rec + 0x0);
		dst.mFlags               = rdbe16(rec + 0x2);
		dst.mPlaneType           = rdbe16(rec + 0x4);
		dst.mVerticalOffset      = rdbeF32(rec + 0x8);
		dst.mTexelSize           = rdbeF32(rec + 0xC);
		dst.mLeft                = rdbeF32(rec + 0x10);
		dst.mTop                 = rdbeF32(rec + 0x14);
		dst.mRight               = rdbeF32(rec + 0x18);
		dst.mBottom              = rdbeF32(rec + 0x1C);
		dst.mLog2Width           = rdbe16(rec + 0x20);
		dst.mLog2Height          = rdbe16(rec + 0x22);
		dst.unk24                = rdbe32(rec + 0x24);
		dst.mHeightMap           = (u8*)base + rdbe32(rec + 0x28);
	}
}
#else
void TPollutionManager::setDataAddress(TPollutionManager::TPollutionInfo* info)
{
	(void)0;
	// pointer patching ewwww
	info->mLayerInfos
	    = (TPollutionLayerInfo*)((u8*)info->mLayerInfos + (u32)info);
	mLayerInfos = info->mLayerInfos;
	for (int i = 0; i < mJointModelNum; ++i)
		mLayerInfos[i].mHeightMap += (u32)info;
}
#endif

void TPollutionManager::initPollutionInfo()
{
	if (TPollutionInfo* info
	    = (TPollutionInfo*)JKRGetResource("/scene/map/ymap.ymp")) {
#ifdef TARGET_PC
		mJointModelNum = rdbe16(info); // the disc image stays untouched, see setDataAddress
#else
		mJointModelNum = info->mLayerCount;
#endif
		setDataAddress(info);

		if (gpMarDirector->getCurrentMap() == 0x9
		    && gpMarDirector->getCurrentStage() != 0x7) {
			static const char* mare_name_table[] = {
				"pollution00", "pollution01", "pollution02", "pollution03",
				"pollution04", "pollution05", "pollution06", "pollutionA",
				"pollutionB",  nullptr,
			};
			initJointModel("scene/map/pollution", mare_name_table);
		} else {
			static const char* name_table[] = {
				"pollution00", "pollution01", "pollution02", "pollution03",
				"pollution04", "pollution05", "pollution06", "pollution07",
				"pollution08", "pollution09", "pollution10", "pollution11",
				"pollution12", "pollution13", "pollution14", "pollution15",
				"pollution16", "pollution17", "pollution18", "pollution19",
				nullptr,
			};
			initJointModel("scene/map/pollution", name_table);
		}
	}
}

void TPollutionManager::load(JSUMemoryInputStream& stream)
{
	char trash[0x20];
	TJointModelManager::load(stream);

	initPollutionInfo();

	if (mJointModelNum != 0) {
		mDefaultPolluteStampTex
		    = (ResTIMG*)JKRGetResource("/common/map/pollute.bti");
		mDefaultCleanStampTex
		    = (ResTIMG*)JKRGetResource("/common/map/clean.bti");

		getCounterLayer().init(getJointModelNum(), 15, 5);

		for (int i = 0; i < getJointModelNum(); ++i)
			getCounterLayer().registerLayer(getLayer(i),
			                                &getLayer(i)->mCounter);

		gpPollution->getCounterObj().init(30);

		getCounterLayer().registerTexStamp(0, 0xff, mDefaultCleanStampTex);
		getCounterLayer().registerTexStamp(1, 0xff, mDefaultPolluteStampTex);
	}
}

TPollutionManager::TPollutionManager(const char* name)
    : TJointModelManager(name)
    , mLayerInfos(0)
    , mDefaultPolluteStampTex(0)
    , mDefaultCleanStampTex(0)
    , unk20C(0)
{
	gpPollution = this;
}
