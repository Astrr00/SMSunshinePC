#ifndef TYPES_H
#define TYPES_H

#include <dolphin/types.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

// TARGET_PC: nullptr is a real keyword in MSVC; macroizing it is a hard error there.
#if !defined(nullptr) && !defined(TARGET_PC)
#define nullptr 0
#endif

// Address arithmetic: on the console a pointer fits in 32 bits, so the decomp writes (u32)ptr. PTR_U32/PTR_S32 are
// the same cast there and pointer-sized on the PC, where (u32)ptr would truncate. Use them wherever the operand is
// a pointer (clang -Wpointer-to-int-cast finds the rest: sunshine-pc/tools/probe-units.cmd --ptrtrunc).
#ifdef TARGET_PC
#define PTR_U32(p) ((uintptr_t)(p))
#define PTR_S32(p) ((intptr_t)(p))
#else
#define PTR_U32(p) ((u32)(p))
#define PTR_S32(p) ((s32)(p))
#endif

#endif
