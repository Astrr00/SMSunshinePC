#include <JSystem/JDrama/JDRViewport.hpp>
#include <dolphin/gx/GXCull.h>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

using namespace JDrama;

TViewport::TViewport(const TRect& param_1, const char* name)
    : TViewObj(name)
    , unk10(param_1)
    , unk20(0)
{
}

void TViewport::perform(u32 cue, TGraphics* graphics)
{
	if (!(cue & (CUE_DRAW | CUE_DRAW_INIT)))
		return;

	graphics->setViewport(unk10, 0.0f, 1.0f);

	if (unk20 & 1)
		return;

	TRect rect(graphics->getDisplayRect());
	rect.intersect(unk10);
	graphics->setScissor(rect);
}

void TViewport::load(JSUMemoryInputStream& stream)
{
	TNameRef::load(stream);
#ifdef TARGET_PC
	// presets into disc order first: a failed raw read keeps the buffer (retail 0x8000E590)
	swapInPlace32(&unk10.x1);
	swapInPlace32(&unk10.y1);
	swapInPlace32(&unk10.x2);
	swapInPlace32(&unk10.y2);
#endif
	stream.read(&unk10.x1, sizeof(int));
	stream.read(&unk10.y1, sizeof(int));
	stream.read(&unk10.x2, sizeof(int));
	stream.read(&unk10.y2, sizeof(int));
#ifdef TARGET_PC
	// raw reads: the four ints are big-endian on disc
	swapInPlace32(&unk10.x1);
	swapInPlace32(&unk10.y1);
	swapInPlace32(&unk10.x2);
	swapInPlace32(&unk10.y2);
#endif
}
