# Phase 1.3 executed evidence

Linux SDL offscreen / Mesa llvmpipe software evidence recorded 2026-10-08.
No physical desktop, controller, hardware timing or GPU residency evidence.
[Full findings](../../PHASE_1_3_NARRATIVE_OWNERSHIP.md) explain scope and limitations.

- `manifest.json`: accepted base, executed source/binary/evidence hashes and commands.
- `protocol-schema.json`: exact shared field-contract table, protocol 0.1.
- `native-trace.jsonl`, `worker-trace.jsonl`: actual message/tick/process traces;
  worker traces carry run identity. Stdout is protocol-only in worker mode.
- `scenario-results.json`: 186 passing native ownership assertions.
- `continuity-results.json`: ticks/frames/drop/worst intervals during held waits,
  plus distinct explicit pause/resume samples.
- `latency-samples.json`, `latency-results.json`: 55 one-clock RTT samples and
  summaries; intentional wait and native queue-pump latency are labeled.
- `cancellation-results.json`, `checkpoint-results.json`, `failure-results.json`,
  `backpressure-results.json`, `worker-lifecycle.json`: session/process boundaries.
- `reference-continue.json`, `reference-stay.json`, `reference-comparison.json`:
  ordinary displayed Ren'Py fixture agrees with both transported branches.
- `tests.txt`, `native-tests.txt`, `worker-tests.txt`, `build.txt`: passing build/
  CTest/worker checks; no warnings in final native build.
- `baseline.txt`, native/viewport regression records and baseline transcripts:
  accepted 1.2/1.1/focused baseline remain green; known full-suite failures separate.
- `dependency-manifest.json`: unchanged accepted native dependency pins/licenses.
- `exploratory-findings.json`: menu/context, cleanup/diagnostic and test-review friction,
  with fixes distinguished from architectural conclusions.

Primary command:

```sh
bash narrative/phase1_adapter/tools/run_offscreen.sh tmp/phase1_3-gate
```

The final worker-context cleanup was executed in this run. The native binary is
from `5bc6596`; worker source is from `ea944d7`. Later files document those results.
JSON/JSONL data are retained as generated; trailing whitespace in console `.txt`
transcripts is normalized. Large build trees, SDKs, dependencies and duplicate
console/raw trace copies are omitted. All original accepted evidence is preserved.
