# PC animation-disable index safety

## Demonstrated failure

`MActorAnmDataEach<T>::getAnmPtr()` previously checked only `idx < mAnmNum`.
The valid animation-disable sentinel `-1` therefore indexed immediately before the animation-pointer array.
This is a production call path, not a malformed asset or an arbitrary invalid API call:

- `TNpcParts` construction enables simple motion blending for Peach's LOD 0 parasol, part 4.
- `TBaseNPC::setNpcAnm_()` selects `-1` for that part in `NPC_ANM_KIND_UNK5`, reached by the ordinary wet reaction.
  Other animations select index 17, so switching from an active parasol animation can issue the disable call.
- `MActor::setBckFromIndex(-1)` calls `MActorAnmBck::setAnmFromIndex()`.
  Its frame-controller setter records the negative index and skips frame initialization, but its final motion-blend lookup still calls `getAnmPtr(-1)`.
- `TMotionBlendCtrl::setNewAnm()` stores the returned pointer.
  Disabled animation channels are excluded from `MActor::updateIn()` and `updateOut()`.

The original `NPCPeach` records are present in `airport0.szs`, `dolpic0.szs` and `dolpic1.szs`.
The Delfino records serialize parts mask `0x18`, enabling part 4; the airport record's mask is `0x1` and does not enable the parasol.
The original parasol model definition is `sPeach_ParasolData` (`peach_parasol.bmd`), assigned to part 4 by `sPeach_InitData`.
No original data is modified or committed.

The complete original PPC `MActorAnmBck::setAnmFromIndex()` routine, `0x800D6AFC–0x800D6C08`, confirms the negative-index setter branch and subsequent signed upper-bound-only lookup.
This does not establish an observed crash in the retail game.

## Correction and focused regression

On `TARGET_PC`, negative indices now return `nullptr` before indexing the array.
Nonnegative lookups and the original PPC branch retain their existing behavior.
The caller still records the disable state, retains the old animation/frame for blending and passes a null new-animation pointer to the blend controller.

Run from `sunshine-pc`:

```text
tools\mactor_animation_index_test.cmd
tools\mactor_animation_index_test.cmd build\mi01-before\MActorData.hpp
```

The optional second command uses a private pre-fix header snapshot.
At `/O2 /Ob1` with AddressSanitizer, that control reports a heap-buffer-overflow: an eight-byte read immediately before the animation list, through the production blend-stop caller.
The corrected header passes all 16 checks with ASan enabled.
These cover active-to-disabled and repeated blend stops, ordinary non-blended stops, retained old animation/frame, unchanged frame initialization, valid endpoints, positive bounds, negative indices and an empty table.

The fixture compiles the actual animation-data header and constructor, and extracts the complete production blend caller and frame-controller setter without rewriting their logic.
Frame/blend bookkeeping is stubbed.
Animation pointers are identity tokens and are never dereferenced on the tested negative setter branch.
This isolates the invalid table access; it is not a rendered-model or complete NPC-behavior test.

Strict reports `STRICT: 0 unresolved symbols`; all 74 port checks pass.
`ninja baseline` precedes the header correction and `ninja changes_all` exits 0 with identical before/after report totals.
The rebuilt `MActorData.o`, `MActorAnm.o`, `MActor.o` and `NpcAnm.o` are also byte-identical to their pre-change PPC objects.
The final source and fixture review reports no findings.

## Bounded native diagnostic and limits

`mi02_peach` uses the corrected Strict executable SHA256 `162732b021e6d6fed4c6f6376dcaa227bd098dadec34c6b12a0cfb11a822792a`.
One diagnostic start in raw stage 1/scenario 1, a teleport to `(5352,305,3650)` and ordinary L/R input finish with exit 0 after 32.29 seconds, `done at tick=859`, health 8 and an empty crash log.
Original collision inspection gives floor height 300, nearest wall 648 units away and no overhead triangle at the target; Mario settles at y300.
The cheat registry remains inactive.

The screenshots show Toad near Mario and an ordinary water spray; they do not establish a visible Peach encounter or execution of her parasol-disable branch.
That branch's memory-safety proof remains the isolated ASan control above.
No second calibration attempt or normal entry/boss/Shine/cold route is run.
The normal owner playthrough and broad PC-CORE acceptance remain open.

The copied GCI and its earned eighteen-Shine source remain byte-identical, SHA256 `fda013ce271f8c28ecc09784259cf3fa9656958bb259f5458b0bb6a54854dde6`.
The diagnostic's fresh in-memory progress is not a saved-Shine result.
Private inputs, logs, screenshots and summaries remain under `build/`; matrix states stay 17 passed-bounded / 8 open / 0 external, and release v0.1.0 remains draft.
