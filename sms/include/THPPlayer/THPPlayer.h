#ifndef _THP_THPPLAYER_H
#define _THP_THPPLAYER_H

#include <dolphin/os.h>
#include <dolphin/dvd.h>
#include <dolphin/gx.h>

#include <THPPlayer/THPBuffer.h>
#include <THPPlayer/THPFile.h>
#include <THPPlayer/THPInfo.h>

#ifdef TARGET_PC
#include <hostendian.h>
// THP files are big-endian: the per-frame size words are read through these. On the console they are the plain loads.
#define THP_RD_S32(p) ((s32)rdbe32(p))
#define THP_RD_U32(p) rdbe32(p)
#else
#define THP_RD_S32(p) (*(s32*)(p))
#define THP_RD_U32(p) (*(u32*)(p))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define THP_AUDIO_BUFFER_COUNT 3
#ifdef TARGET_PC
// A video frame can take hundreds of ms on the host (software rasterizer, loaded machine). Read buffers are only freed
// by the video decoder and the audio decoder shares them, so 10 buffers starved the movie audio for ~0.4 s every few
// seconds. 40 buffers (1.3 s) keep the audio decoder fed while the video catches up.
#define THP_READ_BUFFER_COUNT  40
#else
#define THP_READ_BUFFER_COUNT  10
#endif
#define THP_TEXTURE_SET_COUNT  3

typedef struct THPPlayer {
	DVDFileInfo fileInfo;
	THPHeader header;
	THPFrameCompInfo compInfo;
	THPVideoInfo videoInfo;
	THPAudioInfo audioInfo;
	void* thpWork;
	BOOL open;
	u8 state;
	u8 internalState;
	u8 playFlag;
	u8 audioExist;
	s32 dvdError;
	s32 videoError;
	BOOL onMemory;
	u8* movieData;
	s32 initOffset;
	s32 initReadSize;
	s32 initReadFrame;
	u32 curField;
	s64 retaceCount;
	s32 prevCount;
	s32 curCount;
	s32 videoDecodeCount;
	f32 curVolume;
	f32 targetVolume;
	f32 deltaVolume;
	s32 rampCount;
	s32 curAudioTrack;
	s32 curVideoNumber;
	s32 curAudioNumber;
	THPTextureSet* dispTextureSet;
	THPAudioBuffer* playAudioBuffer;
	THPReadBuffer readBuffer[THP_READ_BUFFER_COUNT];
	THPTextureSet textureSet[THP_TEXTURE_SET_COUNT];
	THPAudioBuffer audioBuffer[THP_AUDIO_BUFFER_COUNT];
} THPPlayer; // Size: 0x1d0

extern THPPlayer ActivePlayer;

BOOL THPPlayerInit(s32);
void THPPlayerQuit();
BOOL THPPlayerOpen(const char* fileName, BOOL onMemory);
BOOL THPPlayerClose();
BOOL THPPlayerPlay();
void THPPlayerStop();
BOOL THPPlayerPause();
BOOL THPPlayerPrepare(s32 offset, u8 flag, s32 audioTrack);

BOOL THPPlayerSetBuffer(u8* data);

u32 THPPlayerCalcNeedMemory();

BOOL THPPlayerGetVideoInfo(THPVideoInfo* videoInfo);
BOOL THPPlayerGetAudioInfo(THPAudioInfo* audioInfo);
// f32 THPPlayerGetFrameRate();
BOOL THPPlayerSetVolume(s32 vol, s32 duration);

s32 THPPlayerDrawCurrentFrame(GXRenderModeObj* rmode, u32 x, u32 y,
                              u32 polygonW, u32 polygonH);
u32 THPPlayerGetTotalFrame();
s32 THPPlayerGetState();
void THPPlayerDrawDone();

void THPPlayerPostDrawDone();

#ifdef __cplusplus
}
#endif

void PrepareReady(int msg);

#endif
