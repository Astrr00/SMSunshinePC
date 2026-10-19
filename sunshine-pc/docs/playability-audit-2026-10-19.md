# Focused gameplay audit (2026-10-19)

The next ROADMAP step asks for a supported-data or production-code counterexample
before correcting shared game code. This bounded read-only pass found no new
confirmed defect. No speculative initialization, layout change or hang bypass is
applied. This does not close PC-CORE or certify the inspected subsystems.

## Rejected candidates

- **Player/FLUDD:** `MarioMove.cpp`'s nozzle `+0x714` write is already excluded
  from TARGET_PC. `TNozzleBase::unk374` is omitted from construction, but the first
  normal positive trigger pressure assigns it before spray sound reads it.
  `TMario::checkController()` quantizes input through `getLRLevel(u8)`; with the
  standard parameters the first positive `mAnalogR` is about 0.001666667 and the
  pressure accumulator is 64, so the artificial tiny-positive-pressure
  counterexample is not established on the controller path. `gunExec()` calls
  pressure movement before emission. Sources: `Player/WaterGun.cpp` constructor,
  `triggerPressureMovement()` and `emit()`; `Player/MarioMove.cpp`
  `checkController()`/`gunExec()`; `Player/MarioInit.cpp` trigger parameters.
- **Enemy graph:** the flagged minimum/distance locals in `Enemy/graph.cpp` are
  protected by the initial-result and first-candidate branches. The inspected
  byte arithmetic addresses serialized graph data, not host objects.
- **Hinokuri parameters:** `THinokuri2::setLevel()` would use an unset graph name
  with null parameters, but manager load constructs `THino2Params` first and
  initialization already requires them. No supported null-parameter caller was
  found. Source: `Enemy/hinokuri2.cpp` manager `load()`, actor `init()` and
  `setLevel()`.
- **Camera demo FOV:** `CameraDemo.cpp`/`CameraBck.cpp` permit a path where no
  BCK writes the FOV output. A supported missing-animation caller has not been
  demonstrated. This remains a question, not a proven gameplay failure.
- **Shared layout reads:** inspected collision data uses explicit serialized
  offsets; packet callbacks use typed host records. The board-NPC conductor
  clip-distance read and director event-table write already have PC-specific
  handling. No new fixed-PPC-offset host-object defect was established there.

Two agents independently inspected Player/FLUDD and Enemy/Camera; the lead read
the shared map, packet and NPC/director candidates and checked their existing PC
branches. Prior compiler inventories were consulted as leads, not fresh test
results. This is a limited source/caller audit, not an exhaustive static-analysis
pass, assembly-equivalence review or new runtime acceptance.

## Next normal content target

Pinna Episode4 is the next episode available from the earned thirteen-Shine save.
The original inventory maps raw stage5/scenario3 to `pinnaBeach2.szs`.
`scene.bin` supplies five TamaNoko enemies and the sunflower Shine33. Source
inspection confirms that normal water wakes a turtle, its jump must land on
collision sand, and Mario's ground-pound message1 can kill it while it is sunk.
An ordinary stomp is insufficient. Flower revival, reward/save and independent
cold reload must still be demonstrated before accepting that episode.

The earlier Gelato mirror deferral and all release gates remain unchanged.

## Controller-route observations

The existing Pinna telemetry already logs all five turtles. The driver now adds
read-only conditions with an actor index from 0 through 4:

- `tamaawake=N`: the first wake has completed (`unk1B9` is false); this remains
  true after a later sleep and does not mean the jump is ready.
- `tamachase=N`: the existing nerve is `TNerveTamaNokoAttack`.
- `tamaair=N`: the original jump-attack flag is set.
- `tamasunk=N`: the original sunk flag is set; late recovery can still reject a
  hit, so this is not a replacement for the game's hit-validity check.
- `tamadead=N`: the original dead flag is set.

Unavailable actors/scenes return false. Lookups require the joined current
stage5/scenario3 and do not retain scene pointers or create nerve singletons.
Only ordinary PAD commands act on the result. No game source is changed.

Strict reports zero unresolved symbols; 68 port checks and ten focused native
parser cases pass. The parser checks accepted indices, malformed/out-of-range
values and unready-scene behavior, with the existing observation syntax as a
regression. A second agent reviewed the final observation diff without findings.

Normal entry `p401_entry` exits 0 in 165.34 seconds on executable SHA256
`0b4ba455453051d8bfb1e677658e53d768ee7421f9b56849378873a04909b291`.
It reaches raw5/3 through normal menus and the cannon, observes five live turtles
and thirteen Shines, and leaves an empty crash report. All card checksums pass;
the earned GCI remains byte-identical with SHA256
`99f868abf960e0eba6d68b0e6fc4248947ba075f8d5b75dbaface967a96545e4`.

Preserved route controls on the new observation build are `p402_one` (stationary
contact before jump), `p403_close` (closing too far causes direct contact),
`p404_lure` (slow sideways movement is insufficient), `p405_run` (jump succeeds
but lands on hard collision), `p406_sand` (sand landing succeeds but the Mario
jump ends too early), and `p407_stomp` (the closer running jump reaches the
target, but the incorrect Z input opens the Guide). Z opens the Guide even in
the air; the original ground-pound input is L (`TMario::checkBackTrig()`).
Each exits 3 on its explicit unmet condition
with an empty crash report. These are controller-route failures, not crashes or
new game-code defects. The loaded original jump range is 800, not the constructor
default 200. The original director's four movement substeps also explain the
observed 120 spine updates per 30 input ticks; this is not duplicate execution.

`p402_one`/`p403_close` use executable SHA256
`3912ca0b6fd0f0433d84d307ee61a756eff092d8942611f738df06f232191b1e`.
The later chase-condition controls use
`ec7c1ce6fe9fe564556ad94647850c9aa40680dfc56c4cff7717b6fadd7d958e`.
