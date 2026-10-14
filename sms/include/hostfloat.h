#ifndef HOSTFLOAT_H
#define HOSTFLOAT_H

// Host float conversion helpers for TARGET_PC. MWCC on the console lowers a floating-point to unsigned-integer
// conversion to a call of the runtime's __cvt_fp2unsigned; x86-64 clang instead converts through a signed 64-bit
// integer and keeps the low 32 bits, so a negative value wraps (-22.5 -> 0xFFFFFFEA) where the console gives 0.
// Everything here exists only under TARGET_PC; the console build never includes this API.

#ifdef TARGET_PC

#include <types.h>

// asm 0x80082BBC __cvt_fp2unsigned: d < 0 -> 0, d >= 2^32 or NaN -> 0xFFFFFFFF, else truncate toward zero.
static inline u32 SMSCvtFp2Unsigned(double d)
{
	if (d < 0.0)
		return 0;
	if (!(d < 4294967296.0))
		return 0xFFFFFFFFu;
	return (u32)d;
}

#endif // TARGET_PC

#endif // HOSTFLOAT_H
