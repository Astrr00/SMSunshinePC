# SPC native handles in comparisons and conditions

## Proven defect

Native event callbacks such as `getNameRefHandle` pass object addresses through `TSpcInterp::push(void*)`.
The PC slice already retains the complete address in a pointer-wide integer payload.
However, comparisons and Boolean opcodes previously called `getDataInt()`, narrowing it back to 32 bits.
A valid address with zero low32 bits therefore compared equal to null and took the null branch.
Two distinct addresses sharing their low32 bits also compared equal.
This is a deterministic production-interpreter counterexample, independent of the earlier `/O2` sweep or an `/Od` comparison.

## Correction and compatibility

The host slice records whether its integer payload originated from a native pointer push.
The Boolean field occupies existing padding: `sizeof(TSpcSlice)` stays 16 bytes and the payload remains at offset 8 on Windows x64.
All constructors initialize it; scalar/string setters clear it, and ordinary stack/storage copies preserve it.

Pointer conditions and comparisons use the full payload; ordinary numeric values still use the existing 32-bit conversion.
That distinction preserves signed comparisons after numeric increment/negation rather than widening all script integer behavior.
String/string and float comparisons retain their existing paths; mixed non-float values retain their prior numeric conversion rules, with full-width native handles.
All six comparison operators and NOT, AND, OR and the conditional branch share the corrected conversion.
Numeric arithmetic, file formats and native callback identifiers are unchanged.
The PowerPC path has no new field or changed executable code.

## Production regression

From `sunshine-pc`, with `LLVM_BIN` set as for Strict:

```powershell
.\tools\spc_pointer_conditions_test.cmd
# Optional negative control with an old header directory and old translation unit:
.\tools\spc_pointer_conditions_test.cmd "<old include directory>" "<old spcinterp.cpp>"
```

The test compiles the actual interpreter translation unit and real headers at `/O2 /Ob1`.
It constructs a small synthetic SPCB header/text immediate and executes real opcode methods and stacks.
There are no mocked comparisons or fabricated pointer values.
Two actual `VirtualAlloc` reservations at `0x100000000` and `0x200000000` hold distinct readable/writable values in the observed run.
The test tries other address pairs if those reservations are unavailable and fails explicitly if no pair can be allocated.
It frees both allocations afterward and embeds no game data.

The unchanged header/TU fail **17 of 51 checks**; the correction passes **51/51**.
Coverage includes native push/pop, equal/different/null handles, ordering, all affected Boolean opcodes, numeric boundaries and increment conversion, floats, strings and replacement of a pointer slot by each scalar type.

## Build and retention

- Strict reports `STRICT: 0 unresolved symbols`; **68 port checks** pass.
- `ninja baseline` preceded the change; `ninja changes_all` completes with identical report measures.
- SHA256 comparison retains all **736** pre-edit rebuilt PowerPC objects byte-for-byte.
- The inspected PowerPC `execeq` and `execjne` comparisons remain 100% matching; symbol validation passes with six inherited UNUSED-size warnings.
- Reference DOL SHA1 remains `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.

## Normal native regression

`pa12_spc_pointer` passes the unchanged public `pc_gelato_sandcastle_reload.dev` route from `nw91_gelato_early_takeoff`: rc0 / 51 seconds, done at tick1363, health8, no active cheats and an empty crash log.
Normal file menus/movies reach controllable Delfino with six earned Shines; all route flag assertions and GCI checks pass.
The original earned save is byte-for-byte unchanged.

- Strict executable SHA256: `65435b3c5badc4fbf53a894ebc0fd1d5b24334ab495e4026aee0b0110b2b5c03`.
- Public route SHA256: `ec6802d19ce1b8998ef2611ee710d49e09c2f6da1fa61b3b4ebb2310b2a43770`.
- Source/result GCI SHA256: `f7a1826bd9098a4c2d588ce6d65c4fba1e248afeb2344427673ac823228f954a`.
- Private logs and summary: `build/Route/pa12_spc_pointer/run.log`, `build/pa12-native-evidence.json`.

This is a normal-input load/scene regression of the rebuilt interpreter, not a new mission completion or native-Close certificate.
The zero-low32 and colliding-address conditions are reproduced in the actual isolated interpreter test, not by forcing allocations or actor state inside the running game.
No previous native failure is attributed to this defect without the missing fault-time addresses.
Full later-world progression, hardware/audio acceptance and the deferred mirror route remain open.
