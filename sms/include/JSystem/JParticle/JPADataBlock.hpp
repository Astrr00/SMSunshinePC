#ifndef JPA_DATA_BLOCK_HPP
#define JPA_DATA_BLOCK_HPP

#include <JSystem/JKernel/JKRHeap.hpp>

// The .jpa image is big-endian and is never modified. Every parser that reads the file image goes through these three
// (JPABinaryHeader, JPABaseShape/ExtraShape/SweepShape/ExTexShape, the block copy in JPADataBlock): on the host they
// convert, on the console they are the original raw loads. Blocks that outlive the load are private copies
// (JPADataBlock): BEM1/FLD1 stay big-endian and are read with the typed JSUInputStream reads, the KFA1 key frames
// (read as host f32) are swapped once in the copy, and the TEX1 BTI header is swapped by JUTTexture::storeTIMG.
#ifdef TARGET_PC
#include <hostendian.h>

static inline f32 JPAReadBEF32(const void* p)
{
	u32 u = rdbe32(p);
	f32 f;
	memcpy(&f, &u, sizeof(f));
	return f;
}

#define JPA_U32(p) rdbe32(p)
#define JPA_S16(p) ((s16)rdbe16(p))
#define JPA_F32(p) JPAReadBEF32(p)
#else
#define JPA_U32(p) (*(u32*)(p))
#define JPA_S16(p) (*(s16*)(p))
#define JPA_F32(p) (*(f32*)(p))
#endif

class JPADataBlock {
public:
	JPADataBlock() { }
	JPADataBlock(const u8*, JKRHeap*);
	virtual ~JPADataBlock() { }

	// fabricated
	u8* getRawData() { return (u8*)mRawData; }

public:
	/* 0x4 */ void* mRawData;
};

#endif
