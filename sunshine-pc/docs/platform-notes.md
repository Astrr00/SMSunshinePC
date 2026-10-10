# Platform notes for the port

Decomp `sms/` @ `56161c69`, version GMSJ01. Everything here is read from source, not verified at runtime unless stated.

## Decomp state (`sms/report.json`)

| Measure | Value |
| --- | ---: |
| Functions matched | 9,160 / 12,881 (71.1%) |
| Code matched | 47.44% |
| Code complete (linked) | 20.25% |
| Units complete | 415 / 736 |

Largest gaps (code bytes, matched share): Enemy 1.08 MB (26.8%), MoveBG 340 KB (37.0%), Player 296 KB (36.4%), GC2D 292 KB (17.6%), System 222 KB (43.5%), Animal 50 KB (19.3%).
SDK and JSystem are nearly complete (dolphin/* 100%, J3D 96.5%, JAudio 91.2%).
For the port the relevant measure is "exists as C/C++", not "matches". That number is still to be measured (`NonMatching` functions with a body vs. assembly fallback).

## Timing

- `TApplication::gameLoop()` (`src/System/Application.cpp`) calls `TDisplay::startRendering()`, `mDirector->direct()` (logic and drawing in one pass), then `TDisplay::endRendering()` once per iteration.
- `TDisplay` is constructed with `2` in `TApplication::initialize()` (`unk4C`). `endRendering()` calls `TVideo::waitForRetrace(unk4C)`. At 60 Hz VI that is 30 logic frames per second. To confirm: the value may change per scene, and PAL (GMSP01) runs at 50 Hz.
- `TVideo::waitForRetrace` blocks on `VIGetRetraceCount()` / `VIWaitForRetrace()`. Replacing this loop is the hook for decoupled rendering.
- Other VI wait loops: `JUTConsole`, `JUTException` (debug only, irrelevant for the port).
- Logic and rendering are not separated. Interpolation therefore needs per-logic-frame snapshots of object matrices and the camera, not a faster game loop.

## SDK layer

- `sms/include/dolphin/*` is the decomp's own header set; Aurora ships its own. Which one wins when linking game code is an open decision for phase 2.
- `sms/src/main.cpp`: `gpApplication.initialize(); proc(); finalize();`. This is the entry point `sunshine-pc/src/main.cpp` will call later.
- `initialize()` uses `OSProtectRange` with fixed addresses (`0x80000000`, `0x83000000`). Those calls cannot work on PC and must go under `TARGET_PC`.

## Feasibility: decomp sources under clang-cl / MSVC (`tools/probe-units.cmd`)

Syntax-only check (`/Zs`, C++20; C11 for plain C units, `/TP` for `THPPlayer/` which the decomp builds with `-lang=c++`) over the 733 source files listed in `configure.py`. No MSL include paths (CRT instead of MSL), `include/pc_compat.h` force-included, `/DTARGET_PC /DNDEBUG`.
`CLANG_CL=<path to clang-cl.exe>` switches the probe from `cl` to clang-cl. clang-cl is needed because the decomp uses C99 compound literals in C++ (`(Vec){...}`), which MSVC rejects and clang accepts; Dusklight also ships clang-cl presets.

Units listed in `excluded-units.txt` (151, each with a reason: MSL, CodeWarrior runtime, MetroTRK, SDK parts Aurora provides, `main.cpp`, JAudio DSP glue) are not probed. The denominator below is the 582 probed units.

| Step | Compiler | Units without errors |
| --- | --- | ---: |
| Unmodified, 733 units | cl | 73 / 733 |
| MSVC branch in `dolphin/types.h`, `nullptr` not a macro, `pc_compat.h` intrinsics | cl | 326 / 733 |
| `EOF` un-macroized, MSL float constants | cl | 404 / 733 |
| `printf.h` shim, exclusion list applied | cl | 432 / 582 |
| nullptr-as-integer sites, `<cmath>` | cl | 464 / 582 |
| clang-cl, 32-bit-only flag bits in `J3DMaterial`/`J3DPacket` | clang-cl | 542 / 582 |
| return-type conflicts, remaining nullptr sites, include fixes, `NDEBUG` | clang-cl | 566 / 582 |
| asm rewrites in `J3DTransform.cpp` and `MathUtil.cpp`, last site fixes | clang-cl | 579 / 582 |

Only syntax is checked, not linking or behavior.

Still failing (3), all in files that carry uncommitted edits that are not part of this port and were left untouched:
`Enemy/Koopa.cpp` (`TUtil<f32>::mod` missing), `Enemy/bossManta.cpp` (jump past initialization in a `case`), `Enemy/hanasambo.cpp` (non-const reference to a temporary).

### Open items (each fails visibly instead of being faked)

- **`__fres`** (reciprocal estimate): declared without a definition in `pc_compat.h`, so `JPAField`, `JPAMath`, `MathUtil` and `J3DPSCalcInverseTranspose` fail at link time. Needs the bit-exact Gekko estimate table. Aurora has none; Dolphin's is GPL-2.0 and is not copied into this CC0/MIT-licensed port. Options: derive it from hardware test vectors, or contribute it to Aurora next to its `frsqrte`.
- **Global `operator new/delete`**: `JKRHeap.cpp` defines only the placement overloads under `TARGET_PC`. Plain `new`/`delete` in game code goes to the CRT heap and a `delete` of a JKR-allocated object would be wrong. Goal-Port B.4 (`JKR_NEW`/`JKR_DELETE`, as Dusklight) must convert every call site.
- **Paired-single math**: the `ps_madd` family is modeled with `fmaf` (one rounding). Not verified against hardware or Dolphin. `fmuls`/`ps_muls` with a double operand (frsqrte result) round that operand to 25 bits first, as on the console.
- **Float contraction**: MWCC builds the game with `-fp_contract on` and fuses multiply-adds at optimizer level. clang-cl's `-ffp-contract=on` only fuses inside one expression and needs `-mfma` to emit FMA at all. Results can differ in the last bit. Decide the flag policy before validating gameplay math.
- **Pointer truncation**: `JSUConvertOffsetToPtr` (`JSupport.hpp`) computes `(s32)ptr + offset`, and `J3DMaterial.hpp` compares `(uintptr_t)mMaterialAnm < 0xC0000000`. Both assume 32-bit pointers and are part of Goal-Port B.3 (file formats).
- **`OSf32tos8`**: saturating truncation in `pc_compat.h`; NaN behavior unverified.
- **JAudio DSP glue** (`JSystem/dsp*.c`, `osdsp*.c`): excluded, needs a host DSP implementation (Dusklight: `DuskDsp`).

52 source files are empty (621,444 code bytes, 2,068 functions, 17.3% of all code; mostly Enemy: BossHanachan*, Kukku, TabePuku, BathtubPeach, ...). They still have to be written. Without them the game cannot be completed.

## Compile and link (`build.cmd`, preset `windows-clang-relwithdebinfo`)

`sms_game.lib` is built from `files.cmake` (generated by `tools/gen-files.py` from `sms/configure.py`): 579 units compile with clang-cl, 0 errors. Excluded: 151 units (`excluded-units.txt`), blocked: 3 (`blocked-units.txt`).
Linking `sunshine.exe` against Aurora showed two classes of problem:

1. **Duplicate symbols (5,925 in 21 names, all fixed).** Headers define globals in every includer: the memory-mapped register arrays in `hw_regs.h`, the fixed-address OS globals in `os.h`, `cSunVolumeName`/`cSunsetVolumeName`, `gpMapWireManager`, and `cNpcPartsNameRootJoint` (defined in two sources). MWCC tolerated this. Under `TARGET_PC` the register/OS globals are declared `extern` and never defined (a built unit that uses one fails at link time), the header globals are C++17 `inline` variables, and `NpcAnm.cpp` declares the symbol that `NpcParts.cpp` defines.
2. **Unresolved symbols (265 distinct).** Sorted by cause in [`unresolved.md`](unresolved.md), regenerated with `tools/classify-unresolved.py`:

| Cause | Symbols |
| --- | ---: |
| Game/JSystem method or static with no definition (empty or unfinished decomp source) | 157 |
| OS layer (threads, mutex, cond, message queue, stopwatch, interrupts, font, reset) | 51 |
| GX/GD leftovers Aurora does not provide | 17 |
| AI / ARAM / DSP hardware | 14 |
| Game global with no definition | 10 |
| `Hx_` wiper library (not in the decomp sources) | 8 |
| VI (retrace, framebuffer hooks) | 7 |
| `__fres` | 1 |

The decomp calls the paired-single `PS*` matrix/vector routines directly in about 23 files, bypassing its `MTXConcat`-style macros. `pc_compat.h` renames `PS*` to Aurora's `C_*` versions; this also covers the decomp's own prototypes. Only the register and OS-global duplicates and the four `PS`/`C_` header issues needed changes in `sms/include`; everything is behind `TARGET_PC`.

Matching build check: after the `sms/` header edits, `ninja` links `mario.dol` with SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`, equal to `config/GMSJ01/build.sha1`. `Enemy/Koopa.o` does not build in the working tree (uncommitted work in progress that is not part of this port, same error under MWCC and clang), so `REPORT`/`PROGRESS` steps were not reached.
