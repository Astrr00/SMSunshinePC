// Force-included (/FI) into every decomp translation unit of the PC port.
// Host replacements for Gekko/CodeWarrior intrinsics. Never included by the matching build.
#ifndef SUNSHINE_PC_COMPAT_H
#define SUNSHINE_PC_COMPAT_H

#include <math.h>
#include <stdint.h>

#include <dolphin/ppc_math.h> // Aurora: hardware-accurate frsqrte (verified against a real 750CL)

#ifdef __cplusplus
#include <cmath>   // MSL exposed std::sqrtf/fmodf/fabsf; decomp code uses them
#include <cctype>  // std::tolower
#include <cstdlib>
#endif
#include <stdarg.h> // va_start in files that never include it themselves under MSL

// The decomp calls the paired-single matrix/vector routines (PS*) directly in many places, bypassing its own
// MTXConcat-style macros. Those do not exist on PC; Aurora provides the C_ versions. Renaming here also renames the
// decomp's own PS* prototypes, so they declare the C_ functions instead.
#define PSMTXConcat C_MTXConcat
#define PSMTXCopy C_MTXCopy
#define PSMTXIdentity C_MTXIdentity
#define PSMTXInverse C_MTXInverse
#define PSMTXTranspose C_MTXTranspose
#define PSMTXMultVec C_MTXMultVec
#define PSMTXMultVecArray C_MTXMultVecArray
#define PSMTXMultVecSR C_MTXMultVecSR
#define PSMTXScale C_MTXScale
#define PSMTXScaleApply C_MTXScaleApply
#define PSMTXTrans C_MTXTrans
#define PSMTXTransApply C_MTXTransApply
#define PSMTXRotRad C_MTXRotRad
#define PSMTXRotTrig C_MTXRotTrig
#define PSMTXRotAxisRad C_MTXRotAxisRad
#define PSMTXQuat C_MTXQuat
#define PSVECAdd C_VECAdd
#define PSVECSubtract C_VECSubtract
#define PSVECScale C_VECScale
#define PSVECNormalize C_VECNormalize
#define PSVECMag C_VECMag
#define PSVECDotProduct C_VECDotProduct
#define PSVECCrossProduct C_VECCrossProduct
#define PSVECSquareMag C_VECSquareMag
#define PSVECDistance C_VECDistance
#define PSVECSquareDistance C_VECSquareDistance

// __fres: reciprocal estimate (Gekko fres). OPEN: declared on purpose without a definition so every user fails at
// link time until a bit-exact implementation exists. Aurora has none and Dolphin's table is GPL-2.0, so it is not
// copied; see sunshine-pc/docs/platform-notes.md.
#ifdef __cplusplus
extern "C"
#endif
float __fres(float);

// OSf32tos8 (psq_st, s8 quantization): truncate toward zero and saturate to [-128, 127], like the hardware.
// Aurora's OSFastCast.h uses a plain (s8) cast, which is undefined for out-of-range values. NaN behavior is unverified.
static inline void OSf32tos8(float* f, signed char* out) {
    float v = *f;
    *out = v >= 127.0f ? 127 : v <= -128.0f ? -128 : (signed char)v;
}
#ifdef _MSC_VER
#include <intrin.h>
#endif
// Constants from MSL math.h. They are float-typed on purpose: the CRT's M_PI is a double and would
// change float results.
#undef M_PI
#define LONG_TAU   6.2831854820251465
#define TAU        6.2831855f
#define HALF_PI    1.5707964f
#define THIRD_PI   1.0471976f
#define QUARTER_PI 0.7853982f
#define SIN_2_5    0.43633234f
#define M_SQRT3    1.73205f
#define M_PI       3.14159265358979323846f
#define DEG_TO_RAD(degrees) (degrees * (M_PI / 180.0f))
#define RAD_TO_DEG(radians) (radians * (180.0f / M_PI + 0.000005f))


// frsqrte: reciprocal square root estimate. Callers refine it with Newton-Raphson, so the estimate
// must match the console bit for bit to keep float results identical.
static inline double __frsqrte(double x) { return frsqrte(x); }

// cntlzw: count leading zeros, 32 for 0.
static inline int __cntlzw(unsigned int v) {
#ifdef _MSC_VER
    unsigned long idx;
    return _BitScanReverse(&idx, v) ? 31 - (int)idx : 32;
#else
    return v ? __builtin_clz(v) : 32;
#endif
}

#endif
