#ifndef JSYSTEM_OSDSP_TASK_H
#define JSYSTEM_OSDSP_TASK_H

#include <dolphin/types.h>

#ifdef TARGET_PC
void DsyncFrame2(u32, uintptr_t, uintptr_t);
#else
void DsyncFrame2(u32, u32, u32);
#endif
BOOL Dsp_Running_Check();
void Dsp_Running_Start();

#endif
