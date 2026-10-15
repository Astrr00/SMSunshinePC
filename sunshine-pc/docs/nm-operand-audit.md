# NonMatching operand audit

Tool: `sunshine-pc/tools/nm_operand_audit.py` (Python 3 stdlib only, issue #2).

It runs one-shot `objdiff-cli diff` for every NonMatching unit of the decomp
(target = `build/GMSJ01/obj`, base = `build/GMSJ01/src`, paths from `sms/objdiff.json`)
and counts instructions whose operands differ in an immediate, an offset, a
relocation or a branch target. Register-only differences, opcode changes
(`DIFF_REPLACE`), inserts and deletes are not counted. Units are ranked by hits.

## Run

```
python sunshine-pc/tools/nm_operand_audit.py --sms-dir "<decomp root>/sms" [--top 10] [--jobs 4] [--tsv <file>]
```

`--tsv` writes every function with hits as `unit, function, hits`. Keep such a file
outside the repo. The tool needs `sms/build/tools/objdiff-cli.exe` and
`sms/build/GMSJ01/report.json`. It only reads the decomp tree; objdiff output goes to a
temp directory that is removed afterwards. A run takes about 10 seconds.

## Result (report.json of 2026-10-14, GMSJ01)

- NonMatching units: 267 (all diffed, 0 failed, 0 skipped)
- Units with operand hits: 246
- Operand hits (instructions): 35633, of which immediate/offset 34815, relocation 712, branch target 119
- Register-only mismatches (ignored): 23678
- Opcode changes (ignored): 1284

Top 10 NonMatching units by operand hits:

| # | Unit | Top function | Fn hits | Unit hits |
|---|---|---|---|---|
| 1 | mario/GC2D/GCConsole2 | TGCConsole2::perform(unsigned long, JDrama::TGraphics*) | 604 | 1394 |
| 2 | mario/Enemy/fireWanwan | TTailRubber::adjust() | 125 | 1027 |
| 3 | mario/GC2D/CardLoad | TCardLoad::selectFunction() | 322 | 966 |
| 4 | mario/Enemy/bossgesso | TBossGesso::doAttackSingle() | 93 | 785 |
| 5 | mario/Map/BathWaterManager | TBathWaterManager::perform(unsigned long, JDrama::TGraphics*) | 209 | 767 |
| 6 | mario/Enemy/bosswanwan | TBWBinder::bind(TLiveActor*) | 232 | 704 |
| 7 | mario/Enemy/bgtentacle | TBGTentacleMtxCalc::calc(unsigned short) | 193 | 694 |
| 8 | mario/Enemy/chuuhana | TChuuHana::behaveToWater(THitActor*) | 95 | 637 |
| 9 | mario/Enemy/Kumokun | TKumokun::bind() | 195 | 561 |
| 10 | mario/Enemy/hamukuri | TNerveHaneHamuKuriMoveOnGraph::execute(TSpineBase<TLiveActor>... | 58 | 560 |

## Caveats

- Hits are per instruction, so one wrong value repeats. In the top unit, the sampled
  immediate hits were all stack-frame offsets (for example `stfd f31, 0x12a0(r1)`
  against `0xe10(r1)`). A single local-size difference shifts every frame access, so
  the hit count can overstate the number of separate bugs. Check the frame size first.
- A row that mixes a register and an offset difference counts as an operand hit.
- The ranking follows objdiff's current matching. It is only as fresh as
  `build/GMSJ01/report.json` and the objdiff.json in the decomp tree.
