#include <JSystem/JAudio/JASystem/JASTrack.hpp>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

namespace JASystem {

#ifdef TARGET_PC
// TSeqParser::cmdEXTSet overlays a TOuterParam on a block the sequence reserves in its own data, sized for the console
// layout (no pointers, so the host layout is identical). A bigger host struct would overwrite the following sequence bytes.
static_assert(sizeof(TTrack::TOuterParam) == 0x2C, "TOuterParam must keep the console size");
#endif

TTrack::TOuterParam::TOuterParam()
{
	mSwitch = 0;
	mUpdate = 0;
	mVolume = 0.0f;
	mPitch  = 0.0f;
	mFxmix  = 0.0f;
	mDolby  = 0.0f;
	mPan    = 0.0f;
	mTempo  = 0.0f;
	for (int i = 0; i < 8; i++)
		mFirFilter[i] = 0;
}

void TTrack::TOuterParam::initExtBuffer()
{
	mSwitch = 0;
	mUpdate = 0;
}

void TTrack::TOuterParam::setOuterSwitch(u16 param) { mSwitch = param; }

u16 TTrack::TOuterParam::getSwitch() { return mSwitch; }

bool TTrack::TOuterParam::checkOuterSwitch(u16 param)
{
	return mSwitch & param;
}

void TTrack::TOuterParam::setOuterUpdate(u16 param) { mUpdate = param; }

u16 TTrack::TOuterParam::getOuterUpdate() { return mUpdate; }

void TTrack::TOuterParam::setIntFirFilter(s16 param1, u8 param2)
{
	mFirFilter[param2] = param1;
}

s16 TTrack::TOuterParam::getIntFirFilter(u8 param) { return mFirFilter[param]; }

void TTrack::TOuterParam::setParam(u8 param, f32 value)
{
	f32* var1;
	switch (param) {
	case TTrack::UPDATE_Volume:
		var1 = &mVolume;
		break;
	case TTrack::UPDATE_Pitch:
		var1 = &mPitch;
		break;
	case TTrack::UPDATE_Fxmix:
		var1 = &mFxmix;
		break;
	case TTrack::UPDATE_Dolby:
		var1 = &mDolby;
		break;
	case TTrack::UPDATE_Pan:
		var1 = &mPan;
		break;
	case TTrack::UPDATE_Tempo:
		var1 = &mTempo;
		break;
	default:
		return;
	}
	*var1 = value;
	mUpdate |= param;
}

void TTrack::TOuterParam::onSwitch(u16 param)
{
	mSwitch |= param;
	mUpdate |= param;
}

void TTrack::TOuterParam::setFirFilter(s16* param)
{
	mUpdate |= TTrack::UPDATE_FirFilter;
	mSwitch |= TTrack::UPDATE_FirFilter;
#ifdef TARGET_PC
	// The only caller (TSeqParser::cmdFIRSet via TTrack::setExtFirFilterD) passes the 8 big-endian coefficients that
	// live in the sequence data; mFirFilter is consumed in host order.
	for (u8 i = 0; i < 8; i++)
		mFirFilter[i] = (s16)rdbe16(&param[i]);
#else
	for (u8 i = 0; i < 8; i++)
		mFirFilter[i] = param[i];
#endif
}

} // namespace JASystem
