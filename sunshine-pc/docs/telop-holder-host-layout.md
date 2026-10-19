# Delfino news selection: host holder layout

## Confirmed defect

`TGCConsole2::checkChangeTelopArray` selects the Delfino scenario1 news array according to whether Peach is being carried.
`loadAfter` obtains Peach by name (`ピーチ姫`) and stores its pointer in `unkC4`.
The original instructions at `0x802082AC..0x802082C8` load that pointer, read its word at `0x68`, and choose `scDolpicNewsDolpic1` only when the word is nonzero.
On PowerPC this is `TTakeActor::mHolder`.

The same fixed byte offset addresses different members in the Windows x64 layout:

| Member | PowerPC offset | Windows x64 offset |
| --- | --- | --- |
| `THitActor::mColCount` | `0x48` | `0x68` |
| `THitActor::mColCapacity` | `0x4A` | `0x6A` |
| `TTakeActor::mHolder` | `0x68` | `0x88` |

The previous host read therefore combined collision count and capacity into one word.
`TBaseNPC::initNpcObjCollision_` normally assigns capacity2 to managed NPCs, while the standalone initialization path in `NpcInitPrg.cpp` uses capacity0.
These supported initialization paths provide counterexamples without arbitrary pointer contents: a managed, unheld actor still enables the news; an actor with zero collision metadata cannot enable it when held.
The message condition follows collision metadata instead of the carry state.
This is a wrong-field defect, not an inference from an `/O2` route failure or an `/Od` comparison.

## Correction

On PC, read the existing `TTakeActor::getHolder()` and compare it with null.
The original PowerPC expression and all other selection/reset behavior remain unchanged.
There is no new null guard, forced news state, gameplay bypass or controller assistance.

## Isolated production regression

Run from `sunshine-pc` with the same `LLVM_BIN` as Strict:

```powershell
.\tools\telop_holder_test.cmd
# Optional negative control using an unchanged source file:
.\tools\telop_holder_test.cmd "<old GCConsole2.cpp>"
```

The test compiles the complete production selector, its actual news arrays and the existing getter body with `/O2 /Ob1`.
The real actor headers determine holder and collision offsets; a layout-backed storage fixture avoids actor constructors, vtables and scene dependencies.
The fixture is not a full NPC or a rendered message system.

Four collision count/capacity pairs (`0/0`, `0/2`, `1/2`, `2/2`) are combined with held/unheld states.
Checks also cover resetting the news index only on array changes, preserving other scenario arrays, scenario5 flag combinations, the scenario8 Shine threshold and suppression outside Delfino.
The unchanged source fails four of **37** checks; the corrected source passes **37/37**.
No game data is embedded in the fixture.

## Build and PowerPC retention

- Strict reports `STRICT: 0 unresolved symbols`; all **68** port checks pass.
- `ninja baseline` preceded the edit; `ninja changes_all` completes afterward with identical report measures.
- The entire rebuilt `GCConsole2.o` is byte-identical to the pre-edit object: SHA256 `f1ff463d3716dd0a1f32b80886fe2c95ffb77196ecaf9bfbdff6b00651241f9e`.
- `decomp-diff.py -u mario/GC2D/GCConsole2 -d checkChangeTelopArray --no-collapse` reports a **100% match**, 740 bytes / 185 instructions; the complete comparison was reviewed.
- `validate-symbol-order.py` passes with the four inherited UNUSED-size warnings.
- Reference DOL SHA1 remains `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`.

## Normal native regression

`pa11_telop_holder` runs the unchanged public `pc_shadow_connected_unassisted.dev` route from the earned airport profile `pg1_airport_reload_20261016`.
Ordinary controller inputs complete Gatekeeper defeat, Shadow Mario defeat and the dialogue: rc0 / 512 seconds, `done at tick=14427`, health8 at the end / minimum1, lives3, no active cheats and an empty crash log.
The original one-Shine GCI is byte-for-byte unchanged and its checksum validation passes; no new saved reward is claimed.

- Strict executable SHA256: `02ac66155194f93deee2a19103524e40396923a95b70a71cbf4bfe563e609609`.
- Public route SHA256: `1cbd376a82f13cf2638d0f5e9efbc41bd72aa59730af43cf261ab64913529876`.
- Source and resulting GCI SHA256: `ee79a201327664876bf0833ff689356a0437e48d58c4f36c9a8a361a719fe528`.
- Private evidence: `build/pa11-native-evidence.json`, `build/pa11-observer.jsonl` and `build/Route/pa11_telop_holder/run.log`.

Gatekeeper and Shadow dialogue PNGs are written.
The last requested `shadow_transition.png` is absent at the immediate script exit; the final state is supported by logs, not that missing image.
This is one normal connected regression using D3D12's CPU adapter and quarter render scale, not a new consecutive-repeatability, native-Close or hardware/visual certificate.

Read-only observation of that process uses the executable's PDB symbol for `gpMarDirector`, its actual module base and header-compiled host offsets.
The private observer requests only process-query and memory-read access; it writes no game state and supplies no input.
In actual Delfino1/1, it observes Peach first unheld and then held, with collision count0/capacity2 in both states.
The legacy word is `131072` in both states, so its nonzero test incorrectly enables the carry-dependent news for the observed unheld state.
This reproduces the wrong-field counterexample in supported native scene data as well as in the isolated test.

The complete selector's branching/reset behavior is checked in isolation; the observer does not instrument individual selector calls.
The native run does not certify exact scrolling-text timing, rendered glyphs, audible behavior or hardware-GPU output.
Broad gameplay acceptance and the deferred mirror route remain open.
