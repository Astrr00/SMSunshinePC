#include <JSystem/JParticle/JPADataBlock.hpp>
#include <dolphin/os.h>
#include <macros.h>

JPADataBlock::JPADataBlock(const u8* param_1, JKRHeap* param_2)
{
	u32 size = JPA_U32(param_1 + 4);
	mRawData = JKRHeap::alloc(size, 0x20, param_2);
	memcpy(mRawData, param_1, size);
#ifdef TARGET_PC
	// KFA1: u8 key count at 0x10, then count * 4 f32 (time, value, tangents) at 0x20, read as host floats by
	// JPAGetKeyFrameInterpolation. Swapped here, once, in the private copy (the file image is never touched).
	if (rdbe32(mRawData) == 'KFA1') {
		u8* block    = (u8*)mRawData;
		u32 keyCount = block[0x10];
		for (u32 i = 0; i < keyCount * 4; ++i)
			swapInPlace32(block + 0x20 + i * 4);
	}
#endif
	DCStoreRange(mRawData, ALIGN_NEXT(size, 0x20));
}
