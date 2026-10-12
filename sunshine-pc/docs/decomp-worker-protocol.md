# Decomp worker protocol v3 (Super Mario Sunshine GMSJ01, repo root = the folder containing `sms/` and `sunshine-pc/`)

## Goal (user rule, latest)
Every non-100 % GAME function (sms/, mwcc decomp; also PC-valid with clang-cl, keep every `#ifdef TARGET_PC`) must have BEHAVIOUR equal to retail, proven from the retail asm and recorded as `logic-verified` in the ledger. Byte-identical is preferred but NOT required: where it is not reachable you may use your own code (non-retail helpers/wrappers, extra locals, restructured control flow, header edits) as long as behaviour equals retail. Codegen score is only a hint; spend NO effort on codegen-only residue beyond ~8 builds per function. Matching units stay 100 %, DOL SHA-1 stays `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`. Shared/JSystem headers may be edited when it helps (rebuild every includer via `--build`, no drop in Matching units; tell the header owner agent if one is named). No MSL/MetroTRK/THPPlayer/dolphin SDK edits.

## Time/budget discipline (most failures came from these)
- Do NOT stop early. Process units until your function list/claims are done or you have done >=4 units, then give the FULL final report (counts, fixes with asm addrs, ledger rows). A final answer that only promises a report is a failure.
- Never print raw objdiff JSON or whole .s files (`--diff` is compact). Read retail asm only by `read path:from-to`.
- After every source edit: `--build <unit>` and confirm no other function of the unit dropped; if one dropped and the edit is not asm-proven logic, undo YOUR edit (restore your saved function text; never whole files from git).
- Stale objects: `--diff/--check` read the LAST built object. Always `--build` first.

## Tools (python, from repo root, all subprocesses hidden: creationflags 0x08000000|0x4000)
- `python sunshine-pc/tools/decomp_queue.py claim <You>` next unit; `claim <You> --quick` unit with most >=99 % functions (fast wins); `claim <You> <unit>` specific unit (prints `CLAIMED_BY x` if taken: skip it); `hold <You> <unit>...` same check for several; `release <You> <unit> <note>`; `free <You>`; `status`.
- `python sunshine-pc/tools/decomp_queue.py ledger <unit> "<bare fn name>" <pct> <status> <You> "<evidence>"`. The tool downgrades `logic-verified` rows lacking `branches mapped` (or `no branches`) to partial and stores pct 100 as `matched`.
- `python sunshine-pc/tools/decomp_triage.py --build <unit>` (single-unit mwcc, per-function %), `--diff <unit> <sym|demangled-substring> [ctx]`, `--check <unit> <sym>` (call order, vtable slots, fused vs unfused FP, opcode histogram, pool constant VALUES), `--branchscan <unit>` (branch-sense mismatches after identical compares), `--immscan <unit>` (compare immediates / field displacements).
- `python -m m2c.main --target ppc-mwcc-c++ -f <mangled> sms/build/GMSJ01/asm/<Dir>/<Unit>.s` (retail draft). Upstream bodies: `../sms-upstream/src` (sibling folder). Permuter: `cd sunshine-pc && python tools/permute_fn.py <unit> <sym> --seconds 600 --jobs 2` (>=97 % reg-only functions).
- Progress: one line per finished function in `sunshine-pc/build/progress.log`: `HH:MM <You> <unit> <fn> <before%>-><after%>`.

## Loop per unit
1. Claim. `--build`. List non-100 functions. Do `partial (pending review)` + `branch check pending` rows first (fast promotions: map branches, `--check`, re-ledger).
2. Per function, lowest % first:
   a. `--diff`, `--check`, `--branchscan`, `--immscan`.
   b. REAL BUG CLASSES (fix in source with `// asm 0x<addr>`; semantics beat score): inverted/missing branch (mismatched bne/beq after identical compares; `b` landing in a different block = misplaced braces); wrong switch case set (differing binary-search pivots / jump-table order); wrong member / field offset (onHitFlag vs onLiveFlag, mVelocity vs mLinearVelocity); this-vs-param base register; wrong vtable slot; swapped/different call arguments or matrix index (mtx[3][0] vs mtx[0][3]); wrong constant or pool value; missing/extra call or store; wrong return value; wrong joint/material/name accessor; signed vs unsigned compare; unexplained frame-size gap (look for missing locals/calls, not just padding).
   c. If our code is structurally far from retail: rewrite from the m2c draft until behaviour is equal.
   d. Codegen-only residue (register/stack/schedule, header inline vs retail out-of-line, fused FP only where retail's choice is documented) -> promote.
   e. Near-100 % stack/reg-only functions: pad recipes close many (`char trash[N]` unused local, declaration order, temp locals; e.g. bosseel/bosswanwan/MapObjBianco 17+12 functions) — max 8 builds, then permuter, then move on.
3. `logic-verified` evidence MUST state: `check OK` (or each item explained), `branches mapped` (taken/fallthrough code traced against retail, sense equal), calls in order incl. vtable slots, constants/pool values, field offsets, loop shape, return value; for stack-only residue also `stack remap injective`.
4. Known limits (not worth effort): JGeometry TVec3/TUtil out-of-line (per-call-site inline choice; HeaderLab proved no header mechanism), pragmas inert for header-defined inline callees, static pool/string layout shifts (`addi` into rodata with a constant delta = pool layout, benign).
5. Release the unit; claim the next.

## Rules
- Hold a unit before editing it; edit only held units' files (+headers as above). No plain `ninja` runs (the lead runs the gates), no configure.py edits, no git writes (read-only `git show/diff/log` ok), no game runs (decomp agents), no files in sms/ root, delete your scratch files. Ledger key = bare function name (no class prefix).
- Final report: units, functions before->after %, bugs fixed (asm addr), rewritten functions, ledger rows (counts), still-unproven items with reason.

## Recipes found in rounds 2-3 (observed, 2026-10-12)
- `char trash[N]` pad: a function-level pad must be declared AFTER the local that has to sit higher in the same scope. A pad at the top of the function only grows the frame; a pad declared after a local places it lower. Nested-block pads often have no effect.
- String-pool / immediate-only diffs (`addi rX,r31,0x...`, 99.9x % loadAfter/__ct): `-inline deferred` emits functions and the string pool in REVERSE source order, retail's pool is forward. Reverse the top-level function order of the .cpp (keep `#pragma dont_inline on/off` pairs around their function), then check every function of the unit; revert if anything else drops (cannon.cpp: loadAfter and SaveLoadParams::__ct to 100 %).
- A static table that is const in our source but in `.data` in retail must be non-const (popo createModelData).
- A local `const TNerveBase<TLiveActor>*` for the `theNerve()` compare fixes the operand order (bosspakkun receiveMessage).
- `(mLiveFlag & X ? 1 : 0)` inline instead of `checkLiveFlag2` removes a `clrlwi.` vs `cmpwi` row.
- Split a float product into separate statements to avoid `fmadds` fusion; merge to get fusion.
- Known limits (do not spend builds): header-defined inline callees ignore `#pragma dont_inline` / `inline_depth` per function under `-inline deferred` (a file-wide `inline_depth` pragma at the end of the .cpp works but changes every function of the unit); TVec3/TUtil out-of-line vs inline per call site; out-of-line `TNerveBase` ctor in inlined `theNerve()`.
- `scaleAdd(scale, b, c)` is `this = b + c*scale`; the one-argument `TMtx34::concat(b)` is not `this*b`: check call sites against the retail operand order.
