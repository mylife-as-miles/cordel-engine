# Phase 1.4 validation evidence

Recorded 2026-10-08 on `cordel/phase1-runtime-adr`, exercised revision
`361b410`. These are fresh **Linux software/offscreen** regressions and the
canonical architecture smoke. No hardware, physical input, Windows or embedded
Python qualification is inferred. Decisions are in
[ADR 0001](../../adr/0001-runtime-narrative-ownership.md); the
[Phase 1 conclusion](../../PHASE_1_ARCHITECTURE_DECISION.md) explains the results.

- `manifest.json`: accepted/fetched Git base, exact executed commands/exit codes,
  source/binary hashes, output hashes and unchanged dependency scope.
- `review-integrity.json`: actual historical source/artifact hash verification and
  raw trace parsing for all three accepted experiments. Phase 1.1's manifest only
  supplies source hashes; no absent artifact hash claim is made.
- `narrative-scenario.json`, `continuity.json`, `cancellation.json`, `checkpoint.json`,
  `failures.json`, `backpressure.json`, `latency.json`, `worker-lifecycle.json`:
  fresh 186-check gate and its measured ownership/failure/restore results.
- `reference-comparison.json`: both branches match ordinary displayed Ren'Py.
  `worker-unwinds.json`: 15 actual unwinds, outer context depth one, zero dynamic roots.
- `native-self-test.json`, `native-lifecycle.json`, `context-recreation.json`,
  `depth.json`, `resize.json`, `stall.json`: 112 graphics assertions, five cycles,
  full host recreation and numeric rendering/input/timing probes.
- `viewport-report.json` and viewport transcripts: 31 helpers, 83 runtime checks,
  one normal UI testcase (8 assertions / 2 hooks).
- `native-tests.txt`, `worker-tests.txt`, baseline transcripts and lint: exact
  selected tests. Counts overlap; known full-suite failures remain excluded.
- `smoke-summary.json`, `smoke-narrative.txt`, `smoke-native.txt`: a second,
  separately executed selected architecture path on the same source/binary.
  `smoke-refusal.json`: populated directory and bad argument count return 2;
  existing evidence's 54 files are unchanged. The first `tmp/phase1-final` refusal
  is also recorded in the manifest.
- `host-boundaries.json`: explicitly labeled startup/shutdown/recreation excerpts
  from fresh native JSONL, including zero final resource counts. This is not a
  complete frame trace.

Replay the five commands in the manifest with fresh directories. Full native /
worker / viewport trace outputs remain locally under ignored `tmp/phase1_4-*`;
this archive keeps compact reports/excerpts and useful transcripts (~150 KiB),
without duplicating prior PNGs, SDKs, dependency trees, binaries or all frame logs.
JSON is retained as generated; console transcript trailing whitespace/BOM and
extra terminal blank lines are normalized. Hashes describe the archived bytes.
The manifest does not hash itself. Later commits add documentation/metadata only.
