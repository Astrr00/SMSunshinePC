#include <Enemy/BathtubBinder.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/JGeometry.hpp>
#include <Map/BathWaterManager.hpp>
#include <MarioUtil/MathUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Strategic/LiveActor.hpp>

// rogue includes needed for matching rodata
#include <M3DUtil/InfectiousStrings.hpp>

// The original reaches the sqrt through three nested inline calls, which is
// deeper than the compiler inlines, so the sqrt stays an out-of-line call.
static inline f32 getCapRadius(const TBathtubData& data)
{
	return JGeometry::TUtil<f32>::sqrt(data.unk3C * data.unk3C
	                                   - data.unk44 * data.unk44);
}

static inline f32 getRadius(const TBathtubData& data)
{
	return getCapRadius(data);
}

static inline f32 getInnerRadius(const TBathtubData& data, f32 margin)
{
	return getRadius(data) - margin;
}

void TBathtubBinder::float_(TLiveActor* actor)
{
	if (mWater == nullptr)
		return;

	Mtx mtx;
	MsMtxSetRotRPH(mtx, actor->mRotation.x, actor->mRotation.y,
	               actor->mRotation.z);
	f32 axisZ = mtx[2][2];
	f32 axisX = mtx[0][2];

	// front point: pushed back inside the tub, then lifted onto the water
	f32 frontX = axisX * unkC + actor->mPosition.x;
	f32 frontZ = axisZ * unkC + actor->mPosition.z;
	f32 frontR = unk10;
	if (mBathtub) {
		const TBathtubData& data = mBathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data.getThing();
		f32 inner = getInnerRadius(data, frontR);
		f32 dz     = frontZ - center.z;
		f32 dx     = frontX - center.x;
		if (dx * dx + dz * dz > inner * inner) {
			f32 s  = inner * JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz);
			frontX = s * dx + center.x;
			frontZ = s * dz + center.z;
		}
	}
	f32 frontH = unk20 + mWater->getWaterHeight(frontX, frontZ);

	// back point
	f32 backX = axisX * -unk14 + actor->mPosition.x;
	f32 backZ = axisZ * -unk14 + actor->mPosition.z;
	f32 backR = unk18;
	if (mBathtub) {
		const TBathtubData& data = mBathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data.getThing();
		f32 inner = getInnerRadius(data, backR);
		f32 dz     = backZ - center.z;
		f32 dx     = backX - center.x;
		if (dx * dx + dz * dz > inner * inner) {
			f32 s = inner * JGeometry::TUtil<f32>::inv_sqrt(dx * dx + dz * dz);
			backX = s * dx + center.x;
			backZ = s * dz + center.z;
		}
	}
	f32 backH = unk20 + mWater->getWaterHeight(backX, backZ);

	f32 dx = frontX - backX;
	f32 dz = frontZ - backZ;
	f32 dh = frontH - backH;
	actor->mPosition.y += 0.2f * ((unk1C * dh + backH) - actor->mPosition.y);
	if (dx * dx + dh * dh + dz * dz <= 3.8146973e-6f)
		return;

	f32 pitch = (360.0f / 65536.0f)
	            * matan(JGeometry::TUtil<f32>::sqrt(dx * dx + dz * dz), dh);
	if (pitch < -15.0f)
		pitch = -15.0f;
	else if (pitch > 15.0f)
		pitch = 15.0f;
	actor->mRotation.x += 0.1f * (pitch - actor->mRotation.x);
	actor->mRotation.z = 0.0f;

	f32 midR = 0.5f * (unk10 + unk18);
	if (mBathtub) {
		const TBathtubData& data = mBathtub->getBathtubData();
		JGeometry::TVec3<f32> center = data.getThing();
		f32 inner = getInnerRadius(data, midR);
		f32 ddz    = actor->mPosition.z - center.z;
		f32 ddx    = actor->mPosition.x - center.x;
		if (ddx * ddx + ddz * ddz > inner * inner) {
			f32 s = inner * JGeometry::TUtil<f32>::inv_sqrt(ddx * ddx + ddz * ddz);
			actor->mPosition.x = s * ddx + center.x;
			actor->mPosition.z = s * ddz + center.z;
		}
		if (actor->mPosition.y < center.y)
			actor->mPosition.y = center.y;
	}
}

void TBathtubBinder::bind(TLiveActor* actor)
{
	if (mBathtub == nullptr || mBathtub->unk29A == 0)
		float_(actor);
}

bool TBathtubBinder::init(f32 a, f32 b, f32 c, f32 d, f32 e)
{
	mBathtub = static_cast<TBathtub*>(JDrama::TNameRefGen::search("バスタブ"));
	mWater   = static_cast<TBathWaterManager*>(
        JDrama::TNameRefGen::search("バスタブの水"));
	unk20 = e;
	unkC  = a;
	unk10 = b;
	unk14 = c;
	unk18 = d;
	unk1C = unk18 / (unk10 + unk18);
	if (mBathtub == nullptr)
		mWater = nullptr;
	return mBathtub != nullptr;
}

TBathtubBinder::~TBathtubBinder() { }

TBathtubBinder::TBathtubBinder()
    : mBathtub(nullptr)
    , mWater(nullptr)
{
}
