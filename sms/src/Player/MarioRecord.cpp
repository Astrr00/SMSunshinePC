#include <Player/MarioRecord.hpp>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

template <typename T> bool TRecordValueManager<T>::get(T* outValue)
{
	*outValue = *mCurrentValuePtr;
	mElapsedFrames++;

	if (mElapsedFrames >= *mCurrentDurationPtr) {
		mCurrentDurationPtr++;
		mCurrentValuePtr++;
		mElapsedFrames = 0;
		return true;
	}
	return false;
}

template <typename T> void TRecordValueManager<T>::reset()
{
	mCurrentDurationPtr = mFirstDurationPtr;
	mCurrentValuePtr    = mFirstValuePtr;
	mElapsedFrames      = 0;
	mUnk14              = 0;
}

#ifdef TARGET_PC
// .pad replay files are big-endian on disc: 16-byte magic "MARIO RECORDv0.2", s32 length @0x10, five {durations,
// values} offset pairs @0x14 (mag f32, yaw s16, btnMask u16, unk64 u8, unk80 u8), each channel holding
// n = (valuesOfs - durationsOfs) / 4 durations (u32) followed by n values. The replay objects keep pointers into the
// file image, so the image itself is swapped in place (it is plain u8/u16/u32/f32 data, same layout on the host).
// Idempotency marker: the loader never reads the magic, so after the swap its first byte is cleared and a buffer that
// no longer starts with "MARIO RECORD" is left alone. chk_pad.py replays this routine on the real files.
static void swapReplayData(u8* d)
{
	if (memcmp(d, "MARIO RECORD", 12) != 0)
		return;

	static const u8 sizes[5] = { 4, 2, 2, 1, 1 };

	swapInPlace32(d + 0x10);
	for (int i = 0; i < 10; ++i)
		swapInPlace32(d + 0x14 + i * 4);

	for (int c = 0; c < 5; ++c) {
		s32 durOfs = *(s32*)(d + 0x14 + c * 8);
		s32 valOfs = *(s32*)(d + 0x18 + c * 8);
		for (s32 i = 0; i < (valOfs - durOfs) / 4; ++i) {
			swapInPlace32(d + durOfs + i * 4);
			if (sizes[c] == 4)
				swapInPlace32(d + valOfs + i * 4);
			else if (sizes[c] == 2)
				swapInPlace16(d + valOfs + i * 2);
		}
	}
	d[0] = 0;
}
#endif

void TMarioInputReplay::init(u8* iData)
{
#ifdef TARGET_PC
	swapReplayData(iData);
#endif
	mUnk0        = 0;
	mCanPlay     = 0;
	mPrevBtnMask = 0;
	mReplayPos   = 0;

	s32 offset = 0x10;

	mReplayLength = *reinterpret_cast<s32*>(iData + offset);

	offset += 4;
	mMag.mFirstDurationPtr = (u32*)(iData + *(s32*)(iData + offset));
	offset += 4;
	mMag.mFirstValuePtr = (f32*)(iData + *(s32*)(iData + offset));
	mMag.reset();

	offset += 4;
	mYaw.mFirstDurationPtr = (u32*)(iData + *(s32*)(iData + offset));
	offset += 4;
	mYaw.mFirstValuePtr = (s16*)(iData + *(s32*)(iData + offset));
	mYaw.reset();

	offset += 4;
	mBtnMask.mFirstDurationPtr = (u32*)(iData + *(s32*)(iData + offset));
	offset += 4;
	mBtnMask.mFirstValuePtr = (u16*)(iData + *(s32*)(iData + offset));
	mBtnMask.reset();

	offset += 4;
	mUnk64.mFirstDurationPtr = (u32*)(iData + *(s32*)(iData + offset));
	offset += 4;
	mUnk64.mFirstValuePtr = (u8*)(iData + *(s32*)(iData + offset));
	mUnk64.reset();

	offset += 4;
	mUnk80.mFirstDurationPtr = (u32*)(iData + *(s32*)(iData + offset));
	offset += 4;
	mUnk80.mFirstValuePtr = (u8*)(iData + *(s32*)(iData + offset));
	mUnk80.reset();
}

bool TMarioInputReplay::play(f32* outIntendedMag, s16* outIntendedYaw,
                             u32* outBtnMask, u32* outJustPressedBtnMask,
                             u8* outParam5, u8* outParam6)
{
	if (mCanPlay == 1) {
		if (mReplayPos < mReplayLength) {
			mMag.get(outIntendedMag);
			mYaw.get(outIntendedYaw);

			u16 btnMask;
			mBtnMask.get(&btnMask);

			u16 justPressedBtns = btnMask & (btnMask ^ mPrevBtnMask);
			mPrevBtnMask        = btnMask;

			*outBtnMask = (*outBtnMask & 0xFFFF0000) | btnMask;
			*outJustPressedBtnMask
			    = justPressedBtns | (*outJustPressedBtnMask & 0xFFFF0000);

			mUnk64.get(outParam5);
			mUnk80.get(outParam6);

			mReplayPos++;
			return true;
		}
		mCanPlay = 0;
	}
	return false;
}

void TMarioInputReplay::reset()
{
	mCanPlay     = 0;
	mPrevBtnMask = 0;
	mReplayPos   = 0;

	mMag.reset();
	mYaw.reset();
	mBtnMask.reset();
	mUnk64.reset();
	mUnk80.reset();
}
