# Phase 1.2 executed evidence

Functional **software/offscreen** evidence, 2026-10-07, Debian 13.6 x86_64,
SDL 3.4.8, Mesa llvmpipe/OpenGL 4.5 core. No accelerated desktop/device evidence.
The [report](../../PHASE_1_2_NATIVE_HOST.md) explains scope and comparison limits;
the [manifest](manifest.json) records source hashes, commands and acceptance base.

- `self-test.json`, `self-test.txt`, `native-tests.txt`: 304 native logic assertions,
  shared Python/native goldens and 112 graphics/runtime assertions. Counts are
  assertions inside two CTest targets and one graphical run, not hundreds of
  separately registered CTest cases.
- `native-trace.jsonl`: startup, five scene cycles, stall and host shutdown.
- `depth-probe.json`, `resize-probe.json`, `input-probe.json`, `stall-probe.json`,
  `lifecycle.json`, `context-recreation.json`: numeric controls and ownership probes.
- `controlled.json`, `uncapped.json` plus their traces: three-second steady
  measurements after warm-up, separate CPU submission/completion/pacing scopes.
- `comparison.json`: accepted Phase 1.1 plus **both** unmodified replay runs,
  native measurements and non-equivalent-metric cautions. Regenerate with:

```sh
.venv/bin/python native/phase1_host/tools/compare.py \
  --renpy docs/cordel/evidence/phase1_2/renpy-replay-1.json \
  --renpy docs/cordel/evidence/phase1_2/renpy-replay-2.json \
  --native docs/cordel/evidence/phase1_2 \
  --controlled docs/cordel/evidence/phase1_2/controlled.json \
  --uncapped docs/cordel/evidence/phase1_2/uncapped.json \
  --output docs/cordel/evidence/phase1_2/comparison.json
```

- `renpy-replay-{1,2}.json` and traces: all 83 checks passed; first stall movement
  zero, second .150 m. The existing assertion permits zero; no reference code was
  altered or unfavorable report removed.
- `renpy-logic-tests.txt`, `renpy-ui-tests.txt`, `baseline-*.txt`, `viewport-lint.txt`:
  31 helper, 8 UI assertions, 29 source, 89 selected SDK and two ending replays.
  Original unit selections overlap; known full-suite failures are not included.
- `clean-tests.txt`, `clean-self-test.json`, `clean-self-test.txt`: independent
  fresh build directory reproduced the same CTest and graphical gate.
- `build-info.txt`, `dependency-manifest.json`, `footprint.json`: toolchain, pinned
  dependencies/licenses, actual ELF linkage/size and explicitly unavailable metrics.
- `exploratory-findings.json`: configure/query/readback mistakes and the earlier
  SDL 3.2.20 unbounded uncapped queue failure, distinct from final 3.4.8 results.
- PNGs are actual `glReadPixels` frames (converted losslessly from native PPMs):
  default viewport, depth controls, square/portrait and 1280×720 surface probes.

Frame-loop counts are not physical presentation or equivalent to Ren'Py viewport
invalidations. CPU fragments include possible preemption/driver waiting; no GPU
timers. `mouse_captured` describes SDL's window relative-mode request flag, not
physical cursor evidence. Zero live counters prove CORDEL ownership/deletion calls,
not driver memory reclamation. Large build/dependency trees and PPMs stay ignored.
Archived console trailing whitespace is normalized; vendored sources/license
texts retain their original bytes and are excluded from whitespace-only diff checks.
