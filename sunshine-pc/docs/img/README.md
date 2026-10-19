# Status graphics

Three checked-in SVGs serve different purposes:

- `status.svg` counts the rows in [playability-matrix.tsv](../playability-matrix.tsv) and lists open acceptance checks. A `passed-bounded` row certifies only its recorded evidence. The rows have unequal scope; their counts are not a completion percentage.
- `matching.svg` reads `sms/build/GMSJ01/report.json`. It shows byte-identical functions, matched code bytes and complete units. Each treemap cell represents a source unit, weighted by its function count. Green represents matched functions within that unit. Open the SVG directly to inspect cell tooltips; README image embeds may not expose them.
- `features.svg` counts the explicitly scoped systems in [gameplay-features.tsv](../gameplay-features.tsv). `observed-bounded` means the implementation has been exercised in normal native play within the recorded scope. `partial` means some behavior has evidence while explicit checks remain open. `unverified` means no accepted normal-play milestone is recorded for that system; it does not mean its code is absent. The list is curated, can grow and is not an exhaustive denominator for game completion. Evidence paths are relative to the catalogue's directory.

All three contain a title and text description for accessibility. Feature cells include evidence and scope in their tooltips. The root README also provides text summaries and links to the evidence.

## Regenerate

From the repository root, using an existing full GMSJ01 matching report:

```powershell
python sunshine-pc/tools/status_treemap.py
python sunshine-pc/tools/status_treemap_test.py
```

The generator uses only Python's standard library. It reads evidence; it does not build or run the game. Regeneration does not refresh the matching measurement or certify additional gameplay. Update the README's text summaries and measurement date when the underlying evidence changes.

An alternate report and output location can be supplied:

```powershell
python sunshine-pc/tools/status_treemap.py path/to/report.json path/to/status.svg
```

`matching.svg` and `features.svg` are written beside the requested status output. For new evidence, edit the feature catalogue's status, scope and source together, then regenerate; do not infer feature status from matching or linking success. Inspect the graphics at README display width before committing; keep generated raster previews in an ignored build directory.
