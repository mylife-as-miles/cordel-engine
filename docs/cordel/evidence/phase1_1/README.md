# Executed Phase 1.1 evidence

Captured 2026-10-07 UTC from the actual checkout using the pinned nightly-native
workflow. See [the report](../../PHASE_1_1_VIEWPORT.md) for interpretation and limits.

- `manifest.json`: source revision/hashes, baseline identity and executed commands.
- `report.json`: final successful software experiments, 83 passing check records.
- `trace.jsonl`: actual initialization, input/stall/resize/check/cycle logs; includes
  the separate normal UI test process (session numbering restarts per process).
- `logic-tests.txt`: 31 tests passed.
- `ui-tests.txt`: actual home/viewport/Say/home/re-entry testcase, eight assertions.
- `baseline-source-tests.txt`, `baseline-sdk-tests.txt`, `baseline-question-tests.txt`:
  focused original baseline replay (29, 89, two ending cases; unit sets overlap).
- `viewport-lint.txt`: native-backed project lint, exit 0.
- `viewport.png`: actual software-rendered HUD/scene after the steady sample.
- `resize_500x500.png`, `resize_320x500.png`: actual square/portrait drawable probes.
- `depth_True_reverse_False.png`, `depth_False_reverse_False.png`: depth-enabled
  near-red versus disabled-depth far-blue control using identical geometry/order.
- `exploratory-failures.json`: failed checks retained from development trials before
  corrections. It is deliberately separate from final acceptance evidence.

CPU fragment timings use wall clocks; llvmpipe executes GPU work on CPU threads.
Synthetic SDL events and direct raw-controller-adapter checks are labeled in the
report. No desktop/physical-device verification, precise GPU timing or proof of
GPU buffer deletion is represented here. Resize verification stays within the
initial offscreen drawable bounds.

To reproduce, configure [BOOTSTRAP](../../BOOTSTRAP.md), then from repository root:

```sh
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/new-viewport-run
bash examples/cordel_viewport/tools/check_baseline.sh tmp/new-viewport-run
```

The first script fails when `report.json` already exists, to keep runs separate.
Review new output in `tmp` before deliberately replacing this recorded snapshot.
No repeated measurement is required just to regenerate the documentation.
