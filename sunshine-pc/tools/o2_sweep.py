"""Rerun the unassisted regression routes on the optimised Strict build; one PASS/FAIL line each (dev-only).

  python tools/o2_sweep.py [tag] [route-substring ...]    results: build/Route/o2s_<tag>_<route>/run.log, summary on stdout
PASS = exit code 0, devscript printed `done at tick`, non-empty crash.log absent (route.py always creates an empty one).
"""
import os, subprocess, sys, time
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
tag = sys.argv[1] if len(sys.argv) > 1 else time.strftime("%m%d")
NAV = os.path.join(ROOT, "build", "PortGoal_20261016", "dolpic1-corrected.nav")
ADAPT = "SUNSHINE_ALLOW_CPU_ADAPTER=0"
RUNS = [  # (script, seconds, extra args)
    ("pc_ricco_scene", 150, ["SUNSHINE_DEV_START=ricco/0", ADAPT]),
    ("pc_airport_unassisted", 700, [ADAPT]),
    ("pc_gatekeeper_unassisted", 500, ["--appdata-from", "pg1_airport_reload_20261016", ADAPT]),
    ("pc_shadow_unassisted", 400, ["SUNSHINE_DEV_START=dolpic/1", "SUNSHINE_DEV_NAV=" + NAV, ADAPT]),
]
for script, secs, extra in RUNS:
    if len(sys.argv) > 2 and not any(k in script for k in sys.argv[2:]):
        continue
    name = f"o2s_{tag}_{script}"
    t = time.time()
    r = subprocess.run([sys.executable, "tools/route.py", "Strict", f"tools/routes/{script}.dev", "--name", name,
                        "--seconds", str(secs)] + extra, cwd=ROOT, capture_output=True, text=True)
    d = os.path.join(ROOT, "build", "Route", name)
    log = open(os.path.join(d, "run.log"), errors="replace").read() if os.path.exists(os.path.join(d, "run.log")) else ""
    ok = r.returncode == 0 and "done at tick" in log and os.path.getsize(os.path.join(d, "crash.log")) <= 2 if os.path.exists(os.path.join(d, "crash.log")) else True
    print(f"{'PASS' if ok else 'FAIL'} {script} rc={r.returncode} {time.time()-t:.0f}s", flush=True)
