"""Diagnostic WARP load/input probes, not normal unlocking or mission completion.

python tools/world_sweep.py <unique-tag> [raw-stage or stage/scenario ...]
Private scripts, results and binary identity: build/WorldSweep/<tag>/summary.json.
"""
import csv
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parent.parent


def selects(stage, scenario, selected):
    return not selected or (stage,) in selected or (stage, scenario) in selected


def verdict(log, code, crash, stage, scenario):
    if crash:
        return "crash"
    if code != 0:
        return "timeout" if code == 124 or "TIMEOUT" in log else "failed-exit"
    if "done at tick=" not in log:
        return "incomplete"
    if "API: D3D12" not in log or "screenshot written: scene_ready.png" not in log:
        return "no-render-proof"
    snapshots = re.findall(r"\[devscript\] tick=\d+[^\n]*?stage=(\d+) scen=(\d+) mario=\(([^)]+)\)[^\n]*?camDemo=(\d+)[^\n]*?dir.state=(\d+)", log)
    if len(snapshots) < 3 or any((int(s[0]), int(s[1])) != (stage, scenario) for s in snapshots):
        return "wrong-scene"
    if snapshots[-1][3:] != ("0", "4"):
        return "not-controllable"
    points = [tuple(map(float, s[2].split(","))) for s in snapshots]
    if not all(math.isfinite(v) for p in points for v in p):
        return "invalid-position"
    if re.search(r"\[(?:status|devscript)-mario3\] health=0\b", log):
        return "death"
    return "pass" if any(math.dist(points[0], p) > 1 for p in points[1:]) else "no-motion"


def main():
    tag, *selected = sys.argv[1:]
    if not re.fullmatch(r"[A-Za-z0-9_-]+", tag):
        raise SystemExit("Use a simple unique tag (letters, digits, underscore, hyphen).")
    selected = {tuple(map(int, item.split("/"))) for item in selected}
    with (ROOT / "docs/content-coverage.tsv").open(encoding="utf-8") as f:
        rows = [r for r in csv.DictReader(f, delimiter="\t") if r["kind"] == "scenario"]
    names = {int(r["raw_stage"]): r["archive"].removesuffix(".arc") for r in rows if r["scenario_index"] == "0"}
    rows = [r for r in rows if r["metadata_status"] == "decoded" and int(r["raw_stage"]) >= 3
            and r["raw_stage"] != "15" and selects(int(r["raw_stage"]), int(r["scenario_index"]), selected)]
    rows.sort(key=lambda r: (int(r["scenario_index"]), int(r["raw_stage"])))
    if not rows:
        raise SystemExit("No present later-world scenarios selected.")
    directory = ROOT / "build/WorldSweep" / tag
    directory.mkdir(parents=True, exist_ok=False)
    exe = ROOT / "build/Strict/sunshine.exe"
    digest = lambda: hashlib.sha256(exe.read_bytes()).hexdigest()
    summary = {"exe_sha256": digest(), "results": []}
    for row in rows:
        if shutil.disk_usage(ROOT).free < 4 * 2**30:
            raise SystemExit("C: below 4 GiB free; stopped without deleting anything.")
        if digest() != summary["exe_sha256"]:
            raise SystemExit("Strict binary changed during sweep; stopped.")
        stage, scenario = int(row["raw_stage"]), int(row["scenario_index"])
        name = f"world_{tag}_{stage:02}_{scenario:02}"
        if (ROOT / "build/Route" / name).exists():
            raise SystemExit(f"Refusing to overwrite existing run {name}")
        script = directory / f"{stage:02}_{scenario:02}.dev"
        script.write_text(f"""# Diagnostic dev start; ordinary pad input, no progress or resource writes.
waitfor stage={stage} scen={scenario} director mario dirstate=4 camDemo=0 timeout=1200
wait frames=90
mash A period=30 until talkbusy=0 camDemo=0 timeout=1200
wait frames=30
waitfor stage={stage} scen={scenario} director mario dirstate=4 camDemo=0 talkbusy=0 timeout=1200
log
hold A frames=2
log
wait frames=45
hold stick=0,80 frames=15
wait frames=30
log
shot scene_ready.png
wait frames=3
end
""")
        started = time.monotonic()
        run = subprocess.run([sys.executable, "tools/route.py", "Strict", str(script), "--name", name,
                              "--seconds", "90", f"SUNSHINE_DEV_START={names[stage]}/{scenario}",
                              "SUNSHINE_DEV_SKIP_MOVIES=1", "SUNSHINE_ALLOW_CPU_ADAPTER=1"],
                             cwd=ROOT, capture_output=True, text=True)
        output = ROOT / "build/Route" / name
        log = (output / "run.log").read_text(errors="replace") if (output / "run.log").exists() else ""
        crash = (output / "crash.log").stat().st_size if (output / "crash.log").exists() else 0
        result = {"stage": stage, "scenario": scenario, "archive": row["archive"], "run": name,
                  "rc": run.returncode, "seconds": round(time.monotonic() - started, 1), "crash_bytes": crash,
                  "result": verdict(log, run.returncode, crash, stage, scenario)}
        summary["results"].append(result)
        (directory / "summary.json").write_text(json.dumps(summary, indent=2))
        print(f"{len(summary['results'])}/{len(rows)} {result['result'].upper():16} {stage:02}/{scenario:02} {row['archive']} rc={run.returncode} {result['seconds']}s", flush=True)
    return int(any(r["result"] != "pass" for r in summary["results"]))


if __name__ == "__main__":
    sys.exit(main())
