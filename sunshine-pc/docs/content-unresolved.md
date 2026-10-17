# PC-CONTENT: what the 44 + 144 unresolved rows are

Source: `docs/content-coverage.tsv` (4714 rows, all `runtime_status=open`). This page groups the two metadata-unresolved sets.
Metadata presence is not collectibility; nothing here is a runtime or completion claim.

## 44 unresolved scenario resources (`metadata_status=unresolved-resource`)

| Group | Rows | Stages (raw) | Reading |
|---|---|---|---|
| `absent-referenced-resource` | 36 | 11 `scale0-9`, 12 `test10-19`, 13 `pinnaDemo0-2`, 25-27 `dolpic_ex5-7`, 28 `bia_ex0`, 34-39 `pinna_ex0-5`, 43 `monte_ex1`, 45 `mare_ex1`, 49 `coro_ex3` | `stageArc.bin` names an archive that is not in the disc file system (`FST-absent`). Test stages (`scale`, `test`) and demo stages are leftovers; the `*_ex*` ones are extra-slot stages whose archive is missing in this release. Probably not reachable in a normal game: verify by checking that no `setSystemFlag`/scene change names them. |
| `placeholder-reference` | 8 | 1 (scenarios 3, 4), 10, 17, 18, 19, 53, 54 | The archive is `none.arc`: an empty slot, nothing to load. |

Evidence from `exShineTable` (`sms/src/System/StageUtil.cpp`, index = raw stage - 21): all 13 absent extra stages (25-28, 34-39, 43, 45, 49) have the entry `0xFF`, i.e. no Shine reward. So no Shine depends on them. The other 23 (`scale`, `test`, `pinnaDemo`) are test and demo stages with no entry in that table. Still unproven: that no scene change enters them; that needs the scene-exit data (`scene.bin` exits, `manievent.sb`), not done.

## 144 script/object records: resolved 2026-10-17 (0 left)

| Group | Rows | Resolution |
|---|---|---|
| `Kazekun` 92, `Amenbo` 31, `Kukku` 6, `Cannon` 2 | 131 | The node ends before the graph name and/or the drop field. Each node has its own bounded stream (`TNameRef::getType`), so the reads fail: graph `""`, and `mCoinId` keeps the ctor value -1 (`smallEnemy.cpp:169`; the PC `read(s32&)` keeps a failed read's preset). `newAndRegisterCoin(0xFFFFFFFF)` returns nullptr: no coin. `TSmallEnemy::load`, `TSpineEnemy::load`, `newAndRegisterCoin` and the ctor match retail 100 % (objdiff), so this is retail behaviour. No reward. |
| `MapObjSteam` in `coro_ex6` (Corona, stage 52) | 8 | The node ends before the event ID. Retail `THideObjBase::load` reads it into an uninitialised local, so on the host that stack garbage could spawn a coin, fruit or 1-up. Fixed in the PC build: the local is preset to -1 (`MapObjHide.cpp`, `TARGET_PC` only), no hidden object [INFERENCE: retail outcome assumed, not observed]. |
| `setEventID` in `manievent.sb` (dolpic5, 6, 7, 8, 10) | 5 | Bytecode text 0x322-0x37c: `local5 = 0x10000 + 70 + i`, plus 22 when `i > 15` (`i` = exchanges done); `setEventID` narrows to u16, so `i` 0-23 give Shines 70-85 and 108-115. These are the 24 blue-coin exchange Shines (`GCConsole2.cpp:2035-2044`), already listed as `blue-coin-exchange-reward`. |

`tools/content_inventory.py` now classifies these cases instead of raising; self-check passes; coverage 4570 rows, 0 unresolved records, 240 blue and 120 Shine identities unchanged. Runtime collectibility stays open.
