# GX destination inversion

Normal Pinna Episode3 electrical damage made Aurora abort with `unsupported logic op 10`.
Mario initializes `mBlendLogicOp` to10; `TMario::drawLogic` selects `GX_BM_LOGIC` and that operation during the electrical-damage effect. Operation10 is `GX_LO_INV`: complement the destination channels. Aurora previously implemented only CLEAR, COPY and NOOP.

Patch `0022-gx-logic-invert.patch` implements INV without changing the game or its collision/damage behavior. The fragment shader evaluates the original TEV/fog and alpha discard, then supplies white RGBA. Additive blending with source factor `OneMinusDst` and destination factor zero produces `1-destination`, independent of source color or alpha. Destination-alpha replacement and channel write masks still apply. A reserved shader-config bit distinguishes INV in shader and pipeline cache keys.

## Evidence (2026-10-19)

- Old control `p309_full`: normal controller entry and five red coins, then electrical-damage status `0x00020338` at tick7036, position(-596,2440,-7950). The next message is the unsupported-operation fatal; native exit `0xC0000409`. Its empty `crash.log` alone is not crash evidence.
- Corrected `p310_inv_control`: identical controller script, earned twelve-Shine seed `si25_pinna2_full` and matching tick7036 damage. Thirty trace samples record that damage state; rendering continues without the fatal. The unchanged route then exits3 at tick7229 because Mario fell off the net and did not collect red coin6. This is a renderer regression pass, **not** a full Episode3 pass.
- Both runs use route SHA-256 `1336bd45ddc139eefc2f924e7166416d8fc3980887d9b6c91d8ae8183b767a91`.
- Old executable SHA-256: `bf705a0ea6c143de7066c35b4b85a1fc43e201c24ef6f420d09b18e57e611b0a`.
- Corrected executable SHA-256: `7c8ec93285ee025a85f233570c996b0bda0fa15a494991cacc1d0c0308335e19`.
- `/O2 /Ob1` Strict reports **0 unresolved symbols**; all68 port checks and all four `GXLogicInvTest.*` cases pass. The cases exercise actual shader generation, pipeline configuration, blend conversion and masks. They do not perform pixel readback or establish complete visual accuracy.
- The 22-patch series matches52 files. All41 pre-existing unrelated Aurora diff blocks remain byte-identical; the dirty submodule is not staged.

## Focused verification

Run from `sunshine-pc` in the normal MSVC/LLVM build environment:

```powershell
python tools\locked.py cmd /c tools\_strict.cmd
tools\port_tests.cmd
cmake --build build\Strict --target gfx_recording_tests -j 3
build\Strict\extern\aurora\tests\gfx_recording_tests.exe --gtest_filter=GXLogicInvTest.*
python tools/apply-aurora-patches.py --check
```

Clang23 diagnoses an upstream Googletest `char8_t` conversion under `-Werror`. For the focused test-target build only, `CL=/clang:-Wno-error=character-conversion` permits that warning. The product Strict build passes without that setting; no vendor source or project compiler policy was changed.

No undefined-behavior attribution, damage bypass or invulnerability change is made. Full Pinna Episode3 reward/save acceptance is recorded separately in [normal Pinna evidence](pinna-normal-route.md).
