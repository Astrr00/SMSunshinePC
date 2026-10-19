# Clean Windows PC test: PASS (2026-10-19)

The owner ran the private `sunshine-0.1.0.20261019.zip` candidate on a second Windows PC without development tools, independently of the development checkout. The owner returned `sunshine-logs.zip` and explicitly confirmed correct picture, sound and controls, and that the earned, saved Shine remained after restarting the game. This closes **PC-CLEANPC** with bounded evidence for this package and machine.

## Candidate and provenance

- Package: `sunshine-0.1.0.20261019.zip`, 21,648,842 bytes; SHA-256 `e36f0d9fa55241198ea2acebe59cb4754943d3c32d684bda804bdf4b36539589`.
- Packaged executable SHA-256: `50e1601bf7b29e222ddf80c5c75b4e22afd924ac9b4f554367dc9e14bc143127`.
- Release `/O2`, `SUNSHINE_ALLOW_UNRESOLVED=OFF`; source base `99dca5dd55ec6d948943d9040967b332ce1f7901` plus pending read-only devscript/parser diagnostics and the existing Aurora changes. This was a sealed working-tree snapshot, not a clean tagged release. No `sms/` game-source changes relative to that base.
- Aurora base `50e5be30ec12cc72768bc564bc016b55e82be868`; tracked working-tree patch SHA-256 `c372dd29337e23206a9058b38a121af84f093254278dc870f6eb90cd2adfa0b0`. Main working-tree patch SHA-256 `7cae67684ecc64a8fce476e24e0a16ff73f484226203e1148150779d926a6cf6`. Private build seals are retained under `build/CleanPC_20261019/`.
- Returned log archive SHA-256: `c069cb63a12f69fc3d0b6f400beaa49de3d583026b1f3a6de6ce90b6497a04cc`. ZIP integrity passes. Raw logs remain private because they contain user paths; this document records the relevant observations without those paths.

The package and executable hashes identify the supplied artifact from the development-side seal; the returned logs do not independently hash the target executable. Their package-directory version matches the supplied candidate.

## External observations

- Windows build `26300`, AMD Ryzen 7 5800X3D, NVIDIA GeForce RTX 3070 Ti, driver `32.0.16.1692`; D3D12 initializes on the discrete GPU.
- Three controller devices are enumerated: DualSense Wireless Controller, XInput Controller and Xbox One S Controller. Enumeration does not identify which device the owner used.
- After the startup sample, all 36 recorded one-second logic samples report `30/s`. At the configured 30 FPS limit, `window presents 0/s` counts extra replay presents, not all rendered frames.
- Audio opens successfully; shutdown reports zero underruns, overflow drops and device failures. One block was dropped before the device was ready.
- `crash.log` is empty; `[exit] AURORA_EXIT` records normal window exit. Buffer-map cancellation and device-destroyed warnings follow that exit during teardown.
- Owner confirmation supplies the gameplay observations absent from the logs: picture, sound and controls worked; a normally earned Shine was saved and remained after a fresh game start. The returned log loads a memory-card image, but does not record the Shine identity or its save transaction.

## Scope and remaining gates

PASS covers startup without development tools, ordinary play, owner-observed audiovisual/input behavior, saved-Shine persistence across restart and normal quit on this machine. It does not certify all controllers, other GPUs or Windows builds, later worlds, the full playthrough, long-session stability or a future executable/package.

The separate development-host smoke of the same ZIP also exited normally with code 0 and an empty crash log; it is not the external-machine evidence. Packaging checks found 138 files, 11 DLLs, 20 license texts, zero missing imports, matching staged/ZIP contents and checksum sidecar, and no game data, saves or PDBs.

PC-PACKAGE and PC-PUBLISH remain open. Microsoft runtime entitlement/compliance and exact signed-DXIL provenance, accepted final source and other release gates still require resolution. **v0.1.0 remains a draft.** The owner's confirmation approves this test result, not release publication.
