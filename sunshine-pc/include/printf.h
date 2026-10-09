// Shim for MSL's <printf.h>. Decomp code includes it directly; the port uses the MSVC CRT instead.
// Reachable only because sunshine-pc/include is on the include path and MSL's include dirs are not.
#ifndef SUNSHINE_PC_PRINTF_H
#define SUNSHINE_PC_PRINTF_H

#include <stdarg.h>
#include <stdio.h>

#endif
