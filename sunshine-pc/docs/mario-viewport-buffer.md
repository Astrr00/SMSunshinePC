# Mario viewport buffer bounds

## Proven defect and normal callers

`TMario::boxDrawPrepare` declared `wpsave[5]` and passed it to `GXGetViewportv`.
The API requires six floats (`GX_VIEWPORT_SZ` in the game header).
The active Aurora implementation in `lib/dolphin/gx/GXGet.cpp` writes elements 0 through 5 unconditionally: viewport origin, width/height, near Z and far Z.
Its `GXProject` implementation subsequently reads element 5 for projected depth.
The game method therefore supplied a 20-byte stack object for a 24-byte write and then an out-of-bounds read.

All three callers are ordinary render branches in `TMario::perform` in `MarioMain.cpp`.
`TMario`'s constructor enables the occlusion-probe and visibility flags used by those branches.
Original `PerformLists.bin` schedules player-group cues `0x02000000` and `0x00800000` in `PerformList GX` (node `0x2A4`), and `0x80000000` in `PerformList Silhouette` (node `0x8F2`).
The defect requires no malformed asset, cheat or unusual input.
No historical crash or visible rendering failure is attributed to it without fault-time evidence.

## Correction and PPC retention

The PC array now holds six floats; the PPC declaration remains unchanged.
The existing projection, position adjustment, matrix preparation and render-state operations are untouched.
Retail assembly at `0x8012339C` places the viewport at `SP+0x58` and the projection at `SP+0x70`, leaving all 24 viewport bytes available.
The five-element source declaration and adjacent padding are a matching artifact, not a five-value API contract.
No SDK or middleware sources change.

## Focused regression

From `sunshine-pc`, with the Strict compiler environment and its AddressSanitizer runtime available:

```powershell
.\tools\mario_viewport_test.cmd
# Optional old-source control:
.\tools\mario_viewport_test.cmd "<old MarioDraw.cpp>"
```

The test compiles the complete extracted `boxDrawPrepare` body at `/O2 /Ob1` with AddressSanitizer.
It extracts the unchanged active viewport getter, projection getter and projector bodies from Aurora, using real vector/matrix declarations and controlled GX state.
Subsequent render-state and matrix-setup calls are inert; this checks stack bounds and projection, not rendered cube or occlusion output.
The old source exits1 with `AddressSanitizer: stack-buffer-overflow`: a four-byte write at `vp[5]` reaches offset52 immediately after `wpsave` at `[32,52)` in the caller's instrumented stack frame.
The correction passes **60/60 checks**, covering perspective/orthographic projection, viewport origins/sizes, depth ranges (including reversed depth), model-view translation and Mario's existing +80 Y adjustment.
ASan is confined to this focused test; the native Strict executable is uninstrumented.

## Build retention

`ninja baseline` preceded edits, and `ninja changes_all` retains identical report measures.
All **736** pre-edit PPC object hashes remain identical; `boxDrawPrepare` remains **100% matching** (508 bytes, 127 instructions).
Baseline-aware symbol validation reports **0 new, 17 inherited** errors (16 missing symbols and an order discrepancy); strict symbol validation is not clean.

## Short native regression

Strict reports `STRICT: 0 unresolved symbols`; **68 port checks** pass.
Normal-menu cold load `pa14_mario_viewport` passes the unchanged `pc_pinna_mecha_reload.dev` route from earned profile `pn45_pinna_combat`: rc0 / **52.37 seconds**, done tick1363, health8, eleven Shines, no active cheats and an empty crash log.
The earned GCI remains byte-identical and all seven sector checksums pass.

- Strict executable SHA256: `f8f2841a9ae7fa803ce2870500bcc1b3624b222029579439def7288499915581`.
- Route SHA256: `5478364f5cdab6112a908f1bb8c29de17e388e3dc53e3cec99d836dae0d3dc30`.
- GCI SHA256: `f430dcb08a8b745c2e340de231510c920c53946216b68fe77232d61832e85213`.

Private evidence remains under `build/pa14-*` and `build/Route/pa14_mario_viewport/`.
This is bounded normal startup/save-load evidence, not full visual correctness or hardware acceptance.
PC-CORE stays open; matrix counts remain 17 passed-bounded, 8 open and 0 external checks.
The mirror route remains deferred and the release remains a draft.
