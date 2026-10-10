#ifndef JASBNKPARSER_HPP
#define JASBNKPARSER_HPP

#include <JSystem/JAudio/JASystem/JASBasicBank.hpp>
#include <JSystem/JAudio/JASystem/JASOscillator.hpp>
#include <JSystem/JSupport.hpp>
#include <JSystem/JAudio/JASystem/JASHostFmt.hpp>
#include <dolphin/types.h>

namespace JASystem {

namespace BNKParser {

	template <typename T> struct TOffset {
		T* ptr(const void* base) const
		{
			return JSUConvertOffsetToPtr<T>(base, mOffset);
		}

		/* 0x00 */ DiscU32 mOffset;
	};

	struct TVmap;
	struct TRand;
	struct TSense;
	struct TKeymap;
	struct TOsc;
	struct TInst;
	struct TPmap;
	struct TPerc;

	struct THeader {
		/* 0x000 */ u8 unk0[0x24];
		/* 0x024 */ TOffset<TInst> mInstOffsets[0x80];
		/* 0x224 */ u8 unk224[0x190];
		/* 0x3B4 */ TOffset<TPerc> mPercOffsets[12];
	};
	struct TInst {
		/* 0x00 */ u8 unk0[8];
		/* 0x08 */ DiscF32 unk8;
		/* 0x0C */ DiscF32 unkC;
		/* 0x10 */ TOffset<TOsc> mOscOffsets[2];
		/* 0x18 */ TOffset<TRand> mRandOffsets[2];
		/* 0x20 */ TOffset<TSense> mSenseOffsets[2];
		/* 0x28 */ DiscU32 mKeyRegionCount;
		/* 0x2C */ TOffset<TKeymap> mKeymapOffsets[2];
	};
	struct TKeymap {
		/* 0x00 */ u8 unk0;
		/* 0x04 */ DiscU32 unk4;
		/* 0x08 */ TOffset<TVmap> mVmapOffsets[1];
	};
	struct TOsc {
		/* 0x00 */ u8 unk0;
		/* 0x04 */ DiscF32 unk4;
		/* 0x08 */ TOffset<s16> mAdsTableOffset;
		/* 0x0C */ TOffset<s16> mRelTableOffset;
		/* 0x10 */ DiscF32 unk10;
		/* 0x14 */ DiscF32 unk14;
	};
	struct TPerc {
		/* 0x000 */ DiscU32 mMagic;
		/* 0x004 */ u8 unk4[0x84];
		/* 0x088 */ TOffset<TPmap> mPmapOffsets[0x80];
		/* 0x288 */ s8 mPan[0x80];
		/* 0x308 */ DiscU16 mRelease[0x80];
	};
	struct TPmap {
		/* 0x00 */ DiscF32 unk0;
		/* 0x04 */ DiscF32 unk4;
		/* 0x08 */ TOffset<TRand> mRandOffsets[2];
		/* 0x10 */ DiscU32 mVeloRegionCount;
		/* 0x14 */ TOffset<TVmap> mVeloRegionOffsets[1];
	};
	struct TRand {
		/* 0x00 */ u8 unk0;
		/* 0x04 */ DiscF32 unk4;
		/* 0x08 */ DiscF32 unk8;
	};
	struct TSense {
		/* 0x00 */ u8 unk0;
		/* 0x01 */ u8 unk1;
		/* 0x02 */ u8 unk2;
		/* 0x04 */ DiscF32 unk4;
		/* 0x08 */ DiscF32 unk8;
	};
	struct TVmap {
		/* 0x00 */ u8 unk0;
		/* 0x04 */ DiscU32 unk4;
		/* 0x08 */ DiscF32 unk8;
		/* 0x0C */ DiscF32 unkC;
	};

	TBasicBank* createBasicBank(void* data);
	u32 getUsedHeapSize();

	extern u32 sUsedHeapSize;

} // namespace BNKParser

} // namespace JASystem

#endif // JASBNKPARSER_HPP
