# Publishing runbook

For the lead. Nothing here is run automatically. Every step that writes to a remote needs explicit approval.

Targets:
- Decomp: `https://github.com/Astrr00/sms.git` (branch `main`).
- Port: `https://github.com/Astrr00/SMSunshinePC.git` (branch `main`).

Observed state at writing time (`git -C sms rev-parse HEAD` = `b20d8938a9563b1e224b19639c803e7f8977ca91`, branch `main`; `sms` has 96 entries in `git status --short`, all uncommitted; root has no remote).

## 0. Pre-push checks

- No game data tracked. Root tree: 89 tracked entries, none above 2 MiB. `sms` tracks no `.szs`, `.arc`, `.rvz`, `.iso`, `.dol`, `.bin`, `.bti`, `.bmd`, `.bdl` files. `sms/orig/*` tracks only `.gitkeep`. Largest tracked `sms` file: `config/GMSJ01/symbols.txt`, 3.26 MiB text.
- Matching build still passes: in `sms`, `ninja` links `mario.dol` with SHA-1 `9f5a8caf56f5356aeac9d3ed28bf8de976a03625`. Run it yourself before pushing `sms`.
- Aurora patch series is consistent: `python sunshine-pc/tools/apply-aurora-patches.py --check` exits 0 (state `applied`) on the working tree.

## 1. Push the decomp (`sms`)

`sms` is a nested repository with its own history. It has 96 uncommitted entries (`git -C sms status --short`), including `Koopa.cpp`, `hanasambo.cpp` and `PROGRESS.md`, which are foreign work in progress. The `TARGET_PC` edits are not confirmed to be committed or not; check each file. Decide which of them belong in the push. Commit only those, in `sms`, with the lead's own identity.

    git -C sms status --short
    git -C sms add <only the files to publish>
    git -C sms commit -m "<message>"
    git -C sms push origin main

Do not push foreign work-in-progress you have not reviewed. Changes that also benefit the decomp belong in separate commits (Port.md §3).

## 2. Swap `sms/` for a submodule

The root repository does not track `sms/` yet (it shows as `?? sms/`). Before `git submodule add` can run, the directory must not exist. The working tree must not be lost: it holds the uncommitted changes and the ignored `orig/` and `build/` directories.

1. Move the whole working tree out of the repo, keeping everything:

       mv sms "C:/Users/niemc/AppData/Local/Temp/sms-backup"

2. Add the submodule. It clones the pushed commit from step 1:

       git submodule add https://github.com/Astrr00/sms.git sms

3. Put back the ignored and game-data directories (never tracked, never in the clone):

       mv "C:/Users/niemc/AppData/Local/Temp/sms-backup/orig" sms/orig
       mv "C:/Users/niemc/AppData/Local/Temp/sms-backup/build" sms/build

4. Carry over the uncommitted changes that were not committed in step 1. Copy only files you reviewed. Do not copy `.git`:

       # per file, from the backup, after reviewing it
       cp "C:/Users/niemc/AppData/Local/Temp/sms-backup/<path>" sms/<path>

5. Verify. The submodule HEAD must equal the commit pushed in step 1, and the status must list only the carried-over files:

       git -C sms rev-parse HEAD
       git -C sms status --short
       git diff --cached --stat        # shows the new gitlink and .gitmodules only

Do not run `git add` on `sms` content (the submodule entry is staged by `git submodule add`; the lead commits it).

## 3. Aurora submodule

`.gitmodules` maps `sunshine-pc/extern/aurora` to `https://github.com/encounter/aurora.git`. The root gitlink records upstream `50e5be3`, not the port's working tree. A fresh clone therefore gets pristine Aurora, and the port's changes come from the patch series:

    git submodule update --init sunshine-pc/extern/aurora
    python sunshine-pc/tools/apply-aurora-patches.py

The tool reports `applied` or `pristine`/exit 0 after success. `mixed` (exit 1) means files were edited by hand. Resolve them first. The aurora working tree itself is not pushed anywhere by this runbook.

## 4. Root remote and push

Root has no remote yet. Add it and push only after the steps above are reviewed:

    git remote add origin https://github.com/Astrr00/SMSunshinePC.git
    git push -u origin main

Before the first push, review `git status --short` in the root. Lines beginning `??` are not in any commit. Decide per entry (commit, ignore via `.gitignore`, or leave out). Harness state (`.claude/`, `.cursor/`, `.stateroot/`) is ignored by the root `.gitignore`.

## 5. After publishing

- Fresh clone test in a scratch directory: `git clone --recurse-submodules <root url>`, then the aurora step in §3, then `sunshine-pc\build.cmd`.
- Confirm the submodule commits exist on the remotes: `git -C sms ls-remote origin main` must print the HEAD recorded in the root.
