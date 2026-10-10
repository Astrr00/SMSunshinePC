#include <JSystem/JDrama/JDRVideo.hpp>
#include <JSystem/JDrama/JDRRenderMode.hpp>
#include <dolphin/vi.h>
#include <dolphin/os.h>
#include <types.h>

using namespace JDrama;

#ifdef TARGET_PC
void sunshinePresentFrame(); // sunshine-pc/src/main.cpp
void sunshinePaceWindow(u32 nextRetraceIndex); // sunshine-pc/src/framerate/framerate.cpp
#endif

void InitRenderModeVIParams(GXRenderModeObj* obj)
{
	obj->viTVmode  = (VITVMode)-1;
	obj->fbWidth   = 0;
	obj->efbHeight = 0;
	obj->xfbHeight = 0;
	obj->viXOrigin = 0;
	obj->viYOrigin = 0;
	obj->viWidth   = 0;
	obj->viHeight  = 0;
	obj->xFBmode   = (VIXFBMode)-1;
}

TVideo::TVideo()
{
	mCurFrameBuffer   = nullptr;
	mNextFrameBuffer  = nullptr;
	mLastRetraceTime  = OSGetTick();
	mNextRetraceIndex = VIGetRetraceCount() + 1;

	InitRenderModeVIParams(&mNextRenderMode);

	mCurRenderMode = mNextRenderMode;
}

void TVideo::setNextXFB(const void* fb) { mNextFrameBuffer = fb; }

void TVideo::waitForRetrace(u16 param_1)
{
#ifdef TARGET_PC
	sunshinePaceWindow(mNextRetraceIndex); // presents between logic ticks when fpsLimit > 30
#endif
	while (mNextRetraceIndex - (int)VIGetRetraceCount() > 1)
		VIWaitForRetrace();

	if (!IsEqualRenderModeVIParams(mCurRenderMode, mNextRenderMode)) {
		VIConfigure(&mNextRenderMode);
		if (mCurRenderMode.viTVmode != mNextRenderMode.viTVmode) {
			VISetBlack(1);
			mCurFrameBuffer = 0;
			VIFlush();
			VIWaitForRetrace();
			s32 uVar11 = mCurRenderMode.viTVmode & 3;
			if (((uVar11 == 2) && ((mNextRenderMode.viTVmode & 3) != 2))
			    || ((uVar11 != 2 && ((mNextRenderMode.viTVmode & 3) == 2)))) {

				for (int i = 0; i < 0x3C; ++i)
					VIWaitForRetrace();
			}
		}
	}

	if (mCurFrameBuffer != mNextFrameBuffer) {
		if (mNextFrameBuffer != nullptr) {
			VISetNextFrameBuffer((void*)mNextFrameBuffer);
			VISetBlack(0);
		} else {
			VISetBlack(1);
		}
	}

	mCurRenderMode  = mNextRenderMode;
	mCurFrameBuffer = mNextFrameBuffer;
#ifdef TARGET_PC
	// Aurora has no XFB: the frame reaches the window when the port ends it. The XFB pointer only changes when
	// the game flips buffers (never with a single XFB), so present here, once per game frame, before VIFlush.
	sunshinePresentFrame();
#endif
	VIFlush();
	VIWaitForRetrace();
	mLastRetraceTime  = OSGetTick();
	mNextRetraceIndex = param_1 + VIGetRetraceCount();
}
