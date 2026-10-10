#ifndef JSYSTEM_DSPPROC_H
#define JSYSTEM_DSPPROC_H

#include <dolphin/types.h>

void DSPSendCommands(u32*, u32);
void DSPReleaseHalt();
void DSPWaitFinish();
void Dswap(u32, u32, u32);
void Dmix(u32, u32, u32, s16);
void Dcopy(u32, u32, u32);
void DloadBuffer1(u32, u32);
void DloadBuffer(u32, u32, u32);
void DsaveBuffer(u16, u32, u32);
void DsetLoopState(u32);
void DadpcmDec(u32, u32, u32, u32);
void DloadFilter(u32, u32);
void DcopyfromARAM(u32, u32, u32);
void DoscStart(unsigned char, u16, u16);
void DoscStop(unsigned char);
void DoscStore(u32);
void Dmixer(u32, u32);
void Dresampletest();
void Dadpcmtest(u32);
#ifdef TARGET_PC
// main-RAM buffer addresses handed to the DSP: pointer-sized on the host
void DsetupTable(u32, uintptr_t, uintptr_t, uintptr_t, uintptr_t);
void DsetMixerLevel(float);
void DsyncFrame(u32, uintptr_t, uintptr_t);
#else
void DsetupTable(u32, u32, u32, u32, u32);
void DsetMixerLevel(float);
void DsyncFrame(u32, u32, u32);
#endif
void wait_callback(u16);
void DwaitFrame();
void DiplSec(u32, void (*)(u16));
void DagbSec(u32, void (*)(u16));
void dummy_callback(u16);

#endif
