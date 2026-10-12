#include <JSystem/JDrama/JDRFrmGXSet.hpp>

using namespace JDrama;

#ifdef TARGET_PC
// Retail reads the TGraphics render mode in TEfbCtrlDisp::perform (DRAW_INIT) before the first TFrmGXSet of the frame
// has run, relying on stale stack contents from the previous frame's TGraphics (same stack slot every call). The host
// cannot rely on that, so direct() seeds its TGraphics from the display with the same assignments.
void JDrama::SunshineApplyDisplay(TDisplay* display, TGraphics* graphics)
{
	graphics->mFrameBuffer = display->getCurrentFrameBuffer();
	graphics->mRenderMode  = display->getRenderMode();

	graphics->getUnkFC().setBit(0x1, display->unk64.check(0x1));
	graphics->getUnkFC().setBit(0x2, display->unk64.check(0x2));
	graphics->getUnkFC().setBit(0x4, display->unk64.check(0x4));
	graphics->getUnkFC().setBit(0x8, display->unk64.check(0x8));
	graphics->getUnkFC().setBit(0x10, display->unk64.check(0x10));
	graphics->getUnkFC().setBit(0x20, display->unk64.check(0x20));
	graphics->getUnkFC().setBit(0x40, display->unk64.check(0x40));

	graphics->mFBClamp    = display->getFBClamp();
	graphics->mClearColor = display->getClearColor();
	graphics->mClearZ     = display->getClearZ();
}

void TFrmGXSet::perform(u32 cue, TGraphics* graphics)
{
	if (cue & CUE_DRAW)
		SunshineApplyDisplay(unk10, graphics);
}
#else
void TFrmGXSet::perform(u32 cue, TGraphics* graphics)
{
	char trash[0x78];
	if (cue & CUE_DRAW) {
		graphics->mFrameBuffer = unk10->getCurrentFrameBuffer();
		graphics->mRenderMode  = unk10->getRenderMode();

		graphics->getUnkFC().setBit(0x1, unk10->unk64.check(0x1));
		graphics->getUnkFC().setBit(0x2, unk10->unk64.check(0x2));
		graphics->getUnkFC().setBit(0x4, unk10->unk64.check(0x4));
		graphics->getUnkFC().setBit(0x8, unk10->unk64.check(0x8));
		graphics->getUnkFC().setBit(0x10, unk10->unk64.check(0x10));
		graphics->getUnkFC().setBit(0x20, unk10->unk64.check(0x20));
		graphics->getUnkFC().setBit(0x40, unk10->unk64.check(0x40));

		graphics->mFBClamp    = unk10->getFBClamp();
		graphics->mClearColor = unk10->getClearColor();
		graphics->mClearZ     = unk10->getClearZ();
	}
}
#endif
