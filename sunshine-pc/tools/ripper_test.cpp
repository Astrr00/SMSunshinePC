// The three stream decoders for Yaz0 (JKRDvdRipper, JKRAram, JKRDvdAramRipper) are file-static in their translation
// units, so this file includes the real source and calls them on an in-memory .szs. Compiled three times by
// rarc_test.cmd with -DRIPPER=1|2|3; rarc_test.cpp calls the entry points and prints their CRCs.
#include <cstdlib>
#include <cstring>
#include <vector>

#if RIPPER == 1
#include <JSystem/JKernel/JKRDvdRipper.cpp>
extern "C" int ripperDvd(u8* szs, u8* dst, u32 size)
{
	fileOffset = 0;
	maxDest    = size;
	transLeft  = 0; // the whole file is in memory, nextSrcData() is never needed
	srcLimit   = (u8*)~(uintptr_t)0;
	refBuf     = nullptr;
	return decompSZS_subroutine(szs, dst);
}
#elif RIPPER == 2
#include <JSystem/JKernel/JKRAram.cpp>
extern "C" int ripperAram(u8* szs, u8* dst, u32 size)
{
	fileOffset = 0;
	maxDest    = size;
	transLeft  = 0;
	srcLimit   = (u8*)~(uintptr_t)0;
	refBuf     = nullptr;
	return decompSZS_subroutine(szs, dst);
}
#elif RIPPER == 3
// The decoder streams its output to ARAM through JKRAramPiece::orderSync; point that at a plain buffer.
#define JKRAramPiece FakeAramPiece
#include <JSystem/JKernel/JKRDvdAramRipper.cpp>
static u8* fakeAram;
bool FakeAramPiece::orderSync(int direction, JKRAramAddr source, JKRAramAddr destination, u32 length, JKRAramBlock*)
{
	std::memcpy(fakeAram + destination, (u8*)source, length);
	return true;
}
extern "C" int ripperDvdAram(u8* szs, u8* dst, u32 size)
{
	std::vector<u8> aram(size + 0x100, 0);
	fakeAram   = aram.data();
	fileOffset = 0;
	maxDest    = size;
	transLeft  = 0;
	srcLimit   = (u8*)~(uintptr_t)0;
	readCount  = 0;
	refBuf     = (u8*)std::malloc(0x1120);
	refEnd     = refBuf + 0x1120;
	refCurrent = refBuf;
	dmaBuf     = (u8*)std::malloc(0x100);
	dmaEnd     = dmaBuf + 0x100;
	dmaCurrent = dmaBuf;
	int r      = decompSZS_subroutine(szs, 0);
	std::memcpy(dst, aram.data(), size);
	return r;
}
#endif
