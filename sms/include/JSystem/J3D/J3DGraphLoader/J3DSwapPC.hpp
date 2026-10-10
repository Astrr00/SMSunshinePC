#ifndef J3D_SWAP_PC_HPP
#define J3D_SWAP_PC_HPP

// Host byte order conversion shared by the J3D model (BMD/BMT) and animation (BCK/BTK/BRK/BPK/BTP/...) loaders.
// TARGET_PC only; the console build never includes this API.
//
// Every J3D file starts with the JUTDataFileHeader, followed by `mBlockNum` blocks that each start with a
// JUTDataBlockHeader {fourcc, size}. The first function that sees the raw bytes of a file calls J3DSwapFileHeaderPC();
// when that returns true it then converts the bodies of the blocks it understands (see J3DModelSwapPC.hpp,
// J3DMatSwapPC.hpp, J3DAnmSwapPC.hpp). Nested formats (name tables, BTI texture headers) swap themselves.
//
// Idempotency marker: the file magic word at offset 0 ('J3D1' / 'J3D2', four ASCII bytes). Raw it reads as the
// big-endian constant; after conversion the first word holds the same four bytes in host order, which is a different
// bit pattern. So one word tells converted from raw buffers, needs no extra storage, and a second call (re-mount, cache
// hit, a resource loaded twice) returns false without touching the buffer.

#ifdef TARGET_PC

#include <hostendian.h>
#include <JSystem/JUtility/JUTDataHeader.hpp>

static inline void J3DSwap16PC(void* p, u32 count)
{
	for (u16* q = (u16*)p; count != 0; --count, ++q)
		swapInPlace16(q);
}

static inline void J3DSwap32PC(void* p, u32 count)
{
	for (u32* q = (u32*)p; count != 0; --count, ++q)
		swapInPlace32(q);
}

// Converts the file header words (magic, type, file size, block count, SeAnmOffset) and every block header
// (fourcc, size) in place. Returns true if this call converted the file, false if it was converted before or is not a
// J3D file. The file header bytes 0x10..0x1B (sub-version tag and padding) are byte data and stay untouched.
// A block count that runs past the file size (4 shipped .bmt files declare 2 blocks but contain 1) is clamped to the
// blocks that fit, so the loaders never read a block header beyond the buffer.
static inline bool J3DSwapFileHeaderPC(void* data)
{
	u8* p     = (u8*)data;
	u32 magic = rdbe32(p);
	if (magic != 'J3D1' && magic != 'J3D2')
		return false;

	JUTDataFileHeader* header = (JUTDataFileHeader*)p;
	swapInPlace32(&header->mMagic);
	swapInPlace32(&header->mType);
	swapInPlace32(&header->mFileSize);
	swapInPlace32(&header->mBlockNum);
	swapInPlace32(&header->mSeAnmOffset);

	u32 off = sizeof(JUTDataFileHeader) - sizeof(JUTDataBlockHeader); // 0x20
	u32 num = 0;
	while (num < header->mBlockNum && off + sizeof(JUTDataBlockHeader) <= header->mFileSize) {
		JUTDataBlockHeader* block = (JUTDataBlockHeader*)(p + off);
		swapInPlace32(&block->mType);
		swapInPlace32(&block->mSize);
		++num;
		if (block->mSize < sizeof(JUTDataBlockHeader))
			break;
		off += block->mSize;
	}
	header->mBlockNum = num;
	return true;
}

#endif // TARGET_PC

#endif // J3D_SWAP_PC_HPP
