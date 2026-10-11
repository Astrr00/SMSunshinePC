# Restore the Sunshine project from a handoff bundle on a new Windows PC.
# Run from the bundle root:  powershell -ExecutionPolicy Bypass -File .\handoff-setup.ps1 [-Target D:\Projekte] [-SkipGates]
# Bundle layout: repo.bundle, wip-uncommitted.patch, toolchain\, wheelhouse\, game-data\, extra-repos\, state\, aurora-worktree\
param(
  [string]$Target = "$env:USERPROFILE\Documents\Projekte",
  [switch]$SkipGates
)
$ErrorActionPreference = "Stop"
$bundle = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Join-Path $Target "Sunshine Decomp"
New-Item -ItemType Directory -Force -Path $Target | Out-Null

function Need($cmd, $hint) { if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { Write-Warning "missing: $cmd ($hint)"; return $false }; return $true }
$ok = $true
$ok = (Need git "install Git") -and $ok
$ok = (Need python "install Python 3.12+") -and $ok
$ok = (Need cmake "install CMake >= 3.28") -and $ok
if (-not (Test-Path "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat") -and
    -not (Test-Path "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat")) {
  Write-Warning "Visual Studio 2022 Build Tools (C++ workload) not found: winget install Microsoft.VisualStudio.2022.BuildTools --override `"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended`""
  $ok = $false
}
if (-not $ok) { Write-Warning "Install the missing tools, then re-run." }

# 1. repo
if (-not (Test-Path $repo)) {
  git clone (Join-Path $bundle "repo.bundle") $repo; git -C $repo checkout main
  git -C $repo remote set-url origin https://github.com/Astrr00/SMSunshinePC.git   # the bundle path is not a real remote
  try { git -C $repo fetch origin } catch { Write-Warning "fetch origin failed (offline or no access)" }
  git -C $repo branch --set-upstream-to=origin/main main 2>$null
}
else { Write-Host "repo exists, skipping clone" }

# 2. toolchain: LLVM
$llvmDir = Join-Path $Target "llvm"
if (-not (Test-Path $llvmDir)) {
  New-Item -ItemType Directory -Force -Path $llvmDir | Out-Null
  tar -xf (Join-Path $bundle "toolchain\llvm.tar.xz") -C $llvmDir
}
$llvmBin = (Get-ChildItem $llvmDir -Directory | Select-Object -First 1).FullName + "\bin"
[Environment]::SetEnvironmentVariable("LLVM_BIN", $llvmBin, "User")
$env:LLVM_BIN = $llvmBin
Write-Host "LLVM_BIN=$llvmBin"

# 3. python deps (offline wheels)
$req = Join-Path $repo "sunshine-pc\tools\requirements.txt"
if (Test-Path (Join-Path $bundle "wheelhouse\*.whl")) { python -m pip install --no-index --find-links (Join-Path $bundle "wheelhouse") -r $req }
else { python -m pip install -r $req }   # new PC has internet access

# 4. decomp build tools + compilers
New-Item -ItemType Directory -Force -Path (Join-Path $repo "sms\build") | Out-Null
Copy-Item -Recurse -Force (Join-Path $bundle "toolchain\sms-build-tools") (Join-Path $repo "sms\build\tools")
Copy-Item -Recurse -Force (Join-Path $bundle "toolchain\sms-build-compilers") (Join-Path $repo "sms\build\compilers")

# 5. game data (owner's own copy; never commit)
if (Test-Path (Join-Path $bundle "game-data")) {
  New-Item -ItemType Directory -Force -Path (Join-Path $repo "sms\orig") | Out-Null
  Copy-Item -Recurse -Force (Join-Path $bundle "game-data\GMSJ01") (Join-Path $repo "sms\orig\GMSJ01")
}

# 6. sibling repos (permuter, upstream reference)
foreach ($d in "decomp-permuter", "sms-upstream") {
  $src = Join-Path $bundle "extra-repos\$d"
  if ((Test-Path $src) -and -not (Test-Path (Join-Path $Target $d))) { Copy-Item -Recurse $src (Join-Path $Target $d) }
}

# 7. aurora submodule
$aur = Join-Path $repo "sunshine-pc\extern\aurora"
if (-not (Test-Path (Join-Path $aur "CMakeLists.txt"))) {
  try { git -C $repo submodule update --init } catch { Write-Warning "submodule fetch failed; using bundled aurora-worktree" }
}
if (-not (Test-Path (Join-Path $aur "CMakeLists.txt")) -and (Test-Path (Join-Path $bundle "aurora-worktree"))) {
  Copy-Item -Recurse -Force (Join-Path $bundle "aurora-worktree\*") $aur
}

# 8. WIP patch (optional)
$wip = Join-Path $bundle "wip-uncommitted.patch"
if (Test-Path $wip) { git -C $repo apply --check $wip 2>$null; if ($LASTEXITCODE -eq 0) { Write-Host "WIP patch applies cleanly; apply with: git -C `"$repo`" apply `"$wip`"" } }

# 8b. queue state + triage (decomp_queue.py needs sms\build\triage.json; regenerate with: python sunshine-pc\tools\decomp_triage.py after ninja)
foreach ($f in "decomp_queue.json","triage.json") { $src = Join-Path $bundle "state\$f"; if (Test-Path $src) { Copy-Item -Force $src (Join-Path $repo "sms\build\$f") } }
# selected evidence logs/scripts referenced by the docs
if (Test-Path (Join-Path $bundle "state\build-evidence")) { New-Item -ItemType Directory -Force -Path (Join-Path $repo "sunshine-pc\build") | Out-Null; Copy-Item -Recurse -Force (Join-Path $bundle "state\build-evidence\*") (Join-Path $repo "sunshine-pc\build") }

# 9. stateroot memory (optional)
if (Test-Path (Join-Path $bundle "state\stateroot")) { Write-Host "StateRoot: in the repo run  stateroot resume  (handoff written by the CLI, see HANDOFF.md); raw snapshot in state\stateroot is for reference only (never edit .stateroot by hand)" }
if (Test-Path (Join-Path $bundle "state\progress.log")) { Copy-Item -Force (Join-Path $bundle "state\progress.log") (Join-Path $repo "sunshine-pc\build\progress.log") -ErrorAction SilentlyContinue }

if ($SkipGates) { Write-Host "Done (gates skipped). See HANDOFF.md section 4."; exit 0 }

# 10. gates
Push-Location (Join-Path $repo "sms")
python configure.py
ninja -k 0 -j4
$sha = (certutil -hashfile "build\GMSJ01\mario.dol" SHA1)[1].Trim()
Write-Host "DOL SHA-1: $sha  (expected 9f5a8caf56f5356aeac9d3ed28bf8de976a03625)"
Pop-Location
Push-Location (Join-Path $repo "sunshine-pc")
python tools\apply-aurora-patches.py --check
Pop-Location
Write-Host "Next: strict link (see HANDOFF.md section 4)."
