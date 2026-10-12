# Exit teardown: 0xC0000409 after a devscript TIMEOUT

Status: analysis only. No source edits, no builds, no game runs. Fix A below is proposed text, not applied. Hunk headers in the diff are approximate and must be regenerated against the current files.

Symptom: after a devscript TIMEOUT the process returns 3221226505 (0xC0000409) instead of 3. Recorded in `platform-notes.md` ("Exit after a devscript TIMEOUT").

## Observed (Beobachtet)

### Exit paths

| Path | Exit call | Runs atexit and static dtors | Calls `aurora_shutdown()` first |
|---|---|---|---|
| devscript TIMEOUT, failed, hold | `std::exit(3)` at `src/devscript/devscript.cpp:760,870,901,918,947,971` | yes | no |
| devscript done, `SUNSHINE_DEV_EXIT_ON_END` | `std::_Exit(0)` at `devscript.cpp:734` | no | no |
| AURORA_EXIT in game loop | `std::exit(0)` at `src/main.cpp:75` | yes | no |
| OSResetSystem | `std::exit(0)` at `src/sdk/os_misc.cpp:49` | yes | no |
| OSPanic | `std::abort()` at `src/sdk/os_misc.cpp:37` | no | no |
| Launcher quit | `aurora_shutdown(); return 0;` at `src/main.cpp:127-128` | yes | yes |
| DVD open failure | `aurora_shutdown(); return 1;` at `src/main.cpp:132-133` | yes | yes |
| End of game main | `aurora_shutdown(); return 0;` at `src/main.cpp:156-157` | yes | yes |
| Dev card test hook | `aurora_shutdown(); std::exit(rc);` at `src/saves/save_roundtrip.cpp:122-124` (only when `SUNSHINE_DEV_CARD_TEST` is set, per the hook) | yes | yes |

The code comment at `devscript.cpp:734` already records the same symptom: "no static teardown: it aborts with 0xC0000409 after a script exit".

### Build wiring

- `sunshine-pc/CMakeLists.txt:80-82`: target `sunshine` (the game EXE) compiles `src/main.cpp` and `src/saves/save_roundtrip.cpp`. `settings_test` (`:85`) does not.
- `save_roundtrip.cpp` has no `main`. The only `int main` in `sunshine-pc/src/` is `src/main.cpp:91`.
- `savesRoundTripTest()` is called only from `save_roundtrip.cpp:122`. `sunshineBootAfterHook()` is called from the sms `Application.cpp:633` build copies (`build/Strict/...`, `build/UiDev/...`, under `#ifdef TARGET_PC`). The sms source file was not found at the expected path.

### Pipeline threads (joinable `std::thread` objects with static storage)

- `extern/aurora/lib/gfx/pipeline_cache.cpp:83` `static std::thread g_pipelineThread;`. Started at `:1147` for every backend except WebGPU (`:1143-1147`). Joined only in `shutdown_pipeline_cache` (`:1161-1166`).
- `pipeline_cache.cpp:99` `static std::thread g_pipelineCacheWriterThread;`. Started at `:1072` by `start_pipeline_cache_writer` (`:1066-1073`) when `prepare_pipeline_cache_db()` succeeds (`:1067`). Joined in `stop_pipeline_cache_writer` (`:1075-1088`), which runs only from `shutdown_pipeline_cache` (`:1170`).
- `initialize_pipeline_cache` is called from `gfx/frame.cpp:527`. `gfx/frame.cpp:530-542` runs render-worker shutdown, then `shutdown_pipeline_cache()`.
- `aurora_shutdown()` is `extern/aurora/lib/aurora.cpp:461`, which calls `aurora::shutdown()`. The link from `aurora::shutdown` to `gfx/frame.cpp:530` was not read [INFERENCE].
- Backend on the test tree: `platform-notes.md:607` reads "Observed (own tree `build/PixelMetric`, RTX 3070 Ti, D3D12; scratch test in `devscript.cpp`, since removed)". So D3D12, which is not WebGPU, so `g_pipelineThread` is started (`:1147`). Scope: this is a scratch test tree, not confirmed for the live run.

### Other threads and handlers

- `render_worker.cpp:199`, `gx/fifo.cpp`, `gfx/texture_replacement.cpp`: `thread::Thread` (jthread-based per the handoff). Static dtor requests stop and joins; it does not call `terminate`. `render_worker::shutdown` returns early when not running (`render_worker.cpp:206-209`), so a second call is safe.
- OS threads (`src/sdk/os_thread.cpp:296-306`): `_beginthreadex`, handle closed immediately. `Rec` is never freed (`:40-41`, `:71-72`). No `std::thread` object, so OS threads are not on the terminate path.
- Detached threads with no join: `src/sdk/av.cpp:41`, `src/sdk/dsp_glue.cpp:231`, `src/sdk/av_win.cpp:176-188`.
- `dolphin/dvd/dvd.cpp:708` `s_worker.start()` on successful disc open. Dtor (`:346`) calls `stop()`, which joins.
- Crash handler: `src/crash.cpp:44` `isFatal` includes 0xC0000409. `:232-234` adds a back trace for 0xC0000374/0xC0000409. VEH at `:333`, UEF at `:336`. `crash.log` created with `CREATE_ALWAYS` at install (`:332`).
- `[av] audio:` summary is registered at runtime (`av_win.cpp:195`), so it runs before the startup-registered static dtors. Whether that line prints on the TIMEOUT path comes from the handoff and was not re-observed.

## Ranked causes (Vermutet unless noted)

1. **High.** `g_pipelineThread` is still joinable when static destructors run. Its `~thread()` calls `std::terminate`, which calls `abort`, which under the MSVC/UCRT runtime ends in `__fastfail` (0xC0000409). [External runtime knowledge, not verified in this repo.] Every path that skips `aurora_shutdown()` hits this. Paths that call it, or use `_Exit`, do not. This matches the table.
2. **Medium.** The same mechanism through `g_pipelineCacheWriterThread`. It needs `prepare_pipeline_cache_db()` to succeed on this PC, which is not verified. Cause 1 is sufficient alone.
3. **Low.** A fatal log or OOM abort during teardown (`src/main.cpp:39`, `extern/aurora/lib/logging.hpp:51-52`, `src/sdk/jkr_new_delete.cpp:151`). These print a message first. None is recorded in the notes.
4. **Low.** Detached threads touching statics that are being destroyed. This is undefined behaviour. It usually shows as a different exception, such as 0xC0000005 or heap 0xC0000374.
5. **Excluded in principle.** A join hang at teardown. With the watchdog active (default 120 s, `crash.cpp:171-200`), a hang would show as `0xDEAD0001`, not 0xC0000409.

**Empty `crash.log` (Vermutet).** `__fastfail` (`int 0x29` / `RaiseFailFastException`) is probably not dispatched to VEH or UEF, so the handler at `crash.cpp:333,336` never runs. The 0xC0000409 branch at `:232-234` only runs when that exception record is dispatched. This fits the empty log, but it is not verified. Check with V6.

**Premise correction.** The crash handler does list 0xC0000409 as fatal (`crash.cpp:44`). It would report if the record were dispatched. The empty log suggests it is not dispatched here.

## Fix A (recommended, proposed text, not applied)

One ordered teardown. `std::atexit` is registered once after `aurora_initialize`, and the explicit calls go, including the one in the dev card test hook. Atexit handlers run in LIFO order, so this handler runs before the static destructors of the pipeline thread objects, which are registered during static init before `main`.

```diff
--- a/sunshine-pc/src/main.cpp
+++ b/sunshine-pc/src/main.cpp
@@ -120,3 +120,4 @@
 	aurora_initialize(argc, argv, &config);
 	sunshineApplyRenderSettings();
+	std::atexit([] { aurora_shutdown(); }); // one teardown for every return and std::exit: pipeline threads join before static dtors
 
 	const char* disc = findDvdArg(argc, argv);
@@ -126,9 +127,7 @@
 		if (!launcher::run(settingsFile)) {
-			aurora_shutdown();
 			return 0;
 		}
 	} else if (!aurora_dvd_open(disc)) {
 		std::fprintf(stderr, "usage: sunshine --dvd <path to disc image (iso/rvz/wia/...)>\n");
-		aurora_shutdown();
 		return 1;
 	}
@@ -154,5 +153,4 @@
 	modsShutdown(); // mods/mods_loader.cpp: calls sunshine_mod_shutdown and frees the DLLs
 
-	aurora_shutdown();
 	return 0;
 }
--- a/sunshine-pc/src/saves/save_roundtrip.cpp
+++ b/sunshine-pc/src/saves/save_roundtrip.cpp
@@ -120,5 +120,4 @@
 	const int rc = savesRoundTripTest();
-	aurora_shutdown();
 	std::exit(rc);
 }
```

Notes:
- `std::exit` is already used in `main.cpp` (`:75`), so `<cstdlib>` is available [INFERENCE].
- `save_roundtrip.cpp:123` is removed so that it does not double-call `aurora_shutdown()` through the new atexit handler. Without that removal, the dev card test path (`SUNSHINE_DEV_CARD_TEST`) would run shutdown twice.
- Open point: if `sunshineBootAfterHook()` can run before `main` reaches the `std::atexit` line, the hook's `std::exit` would skip shutdown. The scout did not establish the call order. Check before applying.
- `aurora_shutdown()` idempotence: `render_worker::shutdown` early-returns. `shutdown_pipeline_cache` guards the join with `g_hasPipelineThread`. `stop_pipeline_cache_writer` guards with `joinable()` (`:1076`). `pipeline_cache_abort()` and the `aurora::shutdown` middle link were not read.
- Risks: on the devscript path, shutdown may run mid-frame. `render_worker::shutdown` enqueues Shutdown and joins. If the worker waits on an open frame, the join hangs, and the watchdog then gives `0xDEAD0001` after 120 s. `gM` (`os_thread.cpp:462-474`) is held only in interrupt-disabled sections [INFERENCE]. `modsShutdown` and `gpApplication.finalize` still do not run on `std::exit` paths, as before.

## Fallback B (devscript only, not preferred)

Replace `std::exit(3)` with `std::_Exit(3)` at the six devscript sites. Add `fflush(stderr)` next to the existing `fflush(stdout)`, following the precedent at `devscript.cpp:734`.

Costs: loses the `[av] audio:` summary (`av_win.cpp:195`) and `closeAtExit` (`av_dump.cpp:79`). Does not fix OSResetSystem (`os_misc.cpp:49`) or AURORA_EXIT (`main.cpp:75`). Use only if Fix A fails V2 or V3.

## Fix C (not recommended)

Leak the two pipeline thread objects (`static std::thread& g_pipelineThread = *new std::thread;`). This hides the lifecycle bug. `extern/aurora` is patch-managed (`sunshine-pc/patches/aurora/README.md`), so any change there must become a patch file.

## Verification plan (not run here; for Main)

- **V1** standalone repro outside the repo: `static std::thread t; int main(){ t = std::thread([]{ Sleep(INFINITE); }); std::exit(3); }`. Expect 0xC0000409. With `t.detach()` before the exit, expect 3.
- **V2** game, Fix A applied: a devscript with a never-true `waitfor` and `timeout=30`. The exact condition syntax was not read; check the parser in `devscript.cpp`. Expect `[devscript] TIMEOUT`, rc 3, no `0xDEAD000x`, empty `crash.log`.
- **V3** game: AURORA_EXIT (window close) and OSResetSystem. Predicted 0xC0000409 before the fix (not observed). Expect 0 after the fix.
- **V4** game: script `done` with `SUNSHINE_DEV_EXIT_ON_END=1` still exits 0 (`_Exit` path unchanged).
- **V5** regression: launcher quit (no `--dvd`) still returns 0. The DVD-open failure path returns 1. With `SUNSHINE_DEV_CARD_TEST` set, the hook exits with `rc` and shutdown runs once.
- **V6** if 0xC0000409 persists after Fix A: run under cdb, break on exception `c0000409`, and print the stack. Expect `ucrtbase!abort` and `std::terminate` from the static dtor phase, or a join in `aurora::shutdown`.
- Before any game run: `tools/runlock.py`. Watch `SUNSHINE_WATCHDOG_SECONDS` for hangs.

## Unchecked

- `aurora::shutdown` middle link and `pipeline_cache_abort()`.
- Whether the writer thread starts on the live PC (DB prep not verified). The backend on the test tree is D3D12 (`platform-notes.md:607`), but that is a scratch tree.
- Call order of `sunshineBootAfterHook()` relative to `main`'s atexit registration. The sms source file was not found; only build copies were read.
- `fifo` / `texture_replacement` stop call sites during shutdown.
- `thread.hpp` jthread claim (taken from the handoff, not re-read).
- `launcher::run` return value on quit.
- devscript `waitfor` / `timeout=` condition syntax (not read).
