# PC-CONTENT: what the 44 + 144 unresolved rows are

Source: `docs/content-coverage.tsv` (4714 rows, all `runtime_status=open`). This page groups the two metadata-unresolved sets.
Metadata presence is not collectibility; nothing here is a runtime or completion claim.

## 44 unresolved scenario resources (`metadata_status=unresolved-resource`)

| Group | Rows | Stages (raw) | Reading |
|---|---|---|---|
| `absent-referenced-resource` | 36 | 11 `scale0-9`, 12 `test10-19`, 13 `pinnaDemo0-2`, 25-27 `dolpic_ex5-7`, 28 `bia_ex0`, 34-39 `pinna_ex0-5`, 43 `monte_ex1`, 45 `mare_ex1`, 49 `coro_ex3` | `stageArc.bin` names an archive that is not in the disc file system (`FST-absent`). Test stages (`scale`, `test`) and demo stages are leftovers; the `*_ex*` ones are extra-slot stages whose archive is missing in this release. Probably not reachable in a normal game: verify by checking that no `setSystemFlag`/scene change names them. |
| `placeholder-reference` | 8 | 1 (scenarios 3, 4), 10, 17, 18, 19, 53, 54 | The archive is `none.arc`: an empty slot, nothing to load. |

Evidence from `exShineTable` (`sms/src/System/StageUtil.cpp`, index = raw stage - 21): all 13 absent extra stages (25-28, 34-39, 43, 45, 49) have the entry `0xFF`, i.e. no Shine reward. So no Shine depends on them. The other 23 (`scale`, `test`, `pinnaDemo`) are test and demo stages with no entry in that table. Still unproven: that no scene change enters them; that needs the scene-exit data (`scene.bin` exits, `manievent.sb`), not done.

## 144 unresolved script/object records (`kind=unresolved`)

| Group | Rows | Where | Reading |
|---|---|---|---|
| `Kazekun` in `scene.bin` | 92 | ricco1/2/4/5, bianco3, monte2 and more (Ricco Harbor and Bianco/Monte scenes) | Retail node ends after the manager; the source loader still requests graph and drop fields. No event ID can be inferred. Question: does this object ever drop a Shine/blue coin? Decide from the retail loader asm. |
| `Amenbo` in `scene.bin` | 31 | same scenes | Same pattern as Kazekun (water striders). |
| `Kukku`, `Cannon` | 6 + 2 | scene.bin | The source small-enemy loader requests `mCoinId`, but the retail node has no drop field. No event ID. |
| `MapObjSteam` | 8 | scene.bin | `field out of bounds ... size=4`: the node is shorter than the source parser reads. Parser vs. data question. |
| `dynamic-event-assignment` | 5 | `manievent.sb` at text+0x37c in dolpic5, 6, 10 and two more | `setEventID` with a dynamic argument; needs interprocedural analysis of the script variable or branch. |

Reading: 139 of 144 are one question, "does the retail loader read fewer fields than the source loader", for four object types. One decode of those loaders would settle most rows at once. The 5 script rows need real script analysis.

## Suggested order

1. Retail asm check of the Kazekun/Amenbo/Kukku/Cannon node readers (131 rows).
2. MapObjSteam parser bounds (8 rows).
3. manievent.sb dynamic `setEventID` (5 rows).
4. Reachability proof for the 36 absent archives (no 36-way work: group by test/demo/extra).
