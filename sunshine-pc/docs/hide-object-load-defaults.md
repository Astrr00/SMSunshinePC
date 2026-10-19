# PC hidden-object load defaults

## Supported-data defect

`TMapObjBase::loadHideObjInfo` used uninitialized float and integer locals after failed reads.
The original GMSJ01 `data/scene/coro_ex6.szs` contains eight `MapObjSteam` nodes in `/map/scene.bin` at offsets `0x1744`, `0x17A3`, `0x1802`, `0x1861`, `0x18C0`, `0x191F`, `0x197E` and `0x19DD`, ending at `0x1A3C`.
Every node ends immediately after the `no_data` model name, before all four hide-info fields.
The production path is `TMapObjSteam::load` → `THideObjBase::load` → `TMapObjBase::loadHideObjInfo`, using a node-bounded memory stream.
The existing PC caller initializes event ID to `-1`, but the helper's speed and timer locals had no initial value.

The real memory stream copies zero bytes at EOF.
Its PC typed reads swap the destination before and after reading, preserving a preset when no bytes arrive.
Consequently the helper previously evaluated uninitialized values on this supported-data path.
This is source-and-data proof of an invalid read; no historical crash or visible steam malfunction is attributed to it.
These eight steam actors have no hidden reward object with event ID `-1`, so the audit does not establish an observable reward failure.

Input SHA256 values, without redistributing game data:

- `coro_ex6.szs`: `dfb32b3e74fafc0e6a793b8ae2572d2fe31f55a11d005bf62e7e42c16fd35519`.
- `/map/scene.bin`: `34a1603360614ed42b08c5dfd988142e6d0c9b5aa75766c5ad670b7a1b975343`.

## Minimal correction

Only the PC path initializes the helper's float and integer locals to zero.
The omitted fields therefore yield speeds `0/0`, consistent with the caller's constructor, and timer `1200` through the existing nonpositive fallback.
These are deterministic PC defaults, not a claim about retail's uninitialized stack contents.
Complete records still overwrite the initializers and retain the original scaling and fallback branches.
The reused float remains reused: if the first float exists and the second is absent, Y speed retains the first float, subject to the existing negative-value fallback.
The other caller, `THideObjInfo::load`, retains complete-record behavior.
No middleware code changes.

## Focused regression

From `sunshine-pc`, with the Strict compiler environment:

```powershell
.\tools\hide_obj_info_test.cmd
# Optional old-source control:
.\tools\hide_obj_info_test.cmd "<old MapObjLib.cpp>"
```

The test extracts the complete production helper and compiles the unchanged real JSU stream implementations and headers at `/O2 /Ob1`.
`-ftrivial-auto-var-init=pattern` makes the old uninitialized float result deterministic; this instrumentation is confined to the test and is absent from the native Strict build.
The old source fails **4 of 55 checks** with NaN speed values; the fix passes **55/55**.
Cases cover empty records, EOF at whole-field boundaries, retained second-float behavior, complete big-endian records, speed scaling, negative Y fallback, timer scaling/fallback, EOF state and an untouched following word.
No test input contains original game bytes.
The isolated test does not instantiate actors or reproduce native steam gameplay.

## Build and short native check (2026-10-19)

- Strict reports `STRICT: 0 unresolved symbols`; **68 port checks** pass.
- `ninja baseline` preceded edits; `ninja changes_all` completes with identical report measures.
- All **736** pre-edit PPC object hashes remain identical; the changed helper remains **100% matching** (220 bytes).
- Baseline-aware symbol validation reports **0 new, 2 inherited** missing UNUSED symbols and 27 existing UNUSED-size warnings; strict validation is not clean.
- Normal-menu cold load `pa13_hide_info` passes `pc_pinna_mecha_reload.dev` from the earned `pn45_pinna_combat` save: rc0 / **52.4 seconds**, done tick1363, health8, eleven saved Shines, no active cheats and empty `crash.log`.
- All seven GCI sector checksums pass; the earned save is unchanged, SHA256 `f430dcb08a8b745c2e340de231510c920c53946216b68fe77232d61832e85213`.
- Strict executable SHA256: `57faf586440d0734111843b94d289e78cb87ddb0b721ca7d3cd575b50c908503`.
- Route SHA256: `5478364f5cdab6112a908f1bb8c29de17e388e3dc53e3cec99d836dae0d3dc30`.

Private evidence remains under `build/pa13-*` and `build/Route/pa13_hide_info/`.
The native check covers ordinary save loading and scene startup, not normal entry to `coro_ex6` or its steam effects.
PC-CORE and PC-CONTENT remain open; matrix counts remain 17 passed-bounded, 8 open and 0 external checks.
The mirror route remains deferred; no completed sweep or boss route was repeated.
