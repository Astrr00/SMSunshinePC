"""Recheck logs: python tools/world_summary.py <tag> [retry-tag ...] [--write-coverage].

Writing docs/world-load-coverage.tsv requires every present later-world scenario
to pass on the current Strict binary. Later tags replace earlier probes of the
same scenario, only on the same binary. No game data or screenshots are exported.
"""
import collections
import csv
import hashlib
import json
import sys

from world_sweep import INTRO_DESTINATIONS, ROOT, verdict

merged, digest = {}, None
for tag in (arg for arg in sys.argv[1:] if arg != "--write-coverage"):
    summary = json.loads((ROOT / "build/WorldSweep" / tag / "summary.json").read_text())
    digest = digest or summary["exe_sha256"]
    assert digest == summary["exe_sha256"], "Cannot combine different binaries"
    results = summary["results"]
    assert len({(r["stage"], r["scenario"]) for r in results}) == len(results), "Duplicate scenario in tag"
    for row in results:
        directory = ROOT / "build/Route" / row["run"]
        log = (directory / "run.log").read_text(errors="replace")
        crash = (directory / "crash.log").stat().st_size if (directory / "crash.log").exists() else 0
        row["crash_bytes"] = crash
        row["result"] = verdict(log, row["rc"], crash, row["stage"], row["scenario"])
        merged[row["stage"], row["scenario"]] = row
        if row["result"] != "pass":
            print(row["run"], row["result"], "rc=" + str(row["rc"]))
    print(tag, len(results), dict(collections.Counter(r["result"] for r in results)))
results = list(merged.values())
print("Coverage", len(results), dict(collections.Counter(r["result"] for r in results)))
if "--write-coverage" in sys.argv[2:]:
    with (ROOT / "docs/content-coverage.tsv").open(encoding="utf-8") as f:
        expected = {(int(r["raw_stage"]), int(r["scenario_index"]), r["archive"]) for r in csv.DictReader(f, delimiter="\t")
                    if r["kind"] == "scenario" and r["metadata_status"] == "decoded" and int(r["raw_stage"]) >= 3 and r["raw_stage"] != "15"}
    actual = {(r["stage"], r["scenario"], r["archive"]) for r in results}
    assert actual == expected and len(results) == len(expected), "Incomplete or duplicate scenario coverage"
    assert all(r["result"] == "pass" for r in results), "A probe did not pass"
    assert hashlib.sha256((ROOT / "build/Strict/sunshine.exe").read_bytes()).hexdigest() == digest, "Strict binary changed"
    output = ROOT / "docs/world-load-coverage.tsv"
    columns = ("stage", "scenario", "archive", "control_stage", "control_scenario", "result", "rc", "seconds", "crash_bytes", "run", "exe_sha256")
    with output.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=columns, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        for row in results:
            stage, scenario = INTRO_DESTINATIONS.get((row["stage"], row["scenario"]), (row["stage"], row["scenario"]))
            writer.writerow({**row, "control_stage": stage, "control_scenario": scenario, "exe_sha256": digest})
    print("Wrote", output)
