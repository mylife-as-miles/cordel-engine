# Phase 1.3 — narrative ownership experiment

A fixture-scoped **Ren'Py script executor in a separate process**, connected to the
existing C++20 native host through bounded JSONL pipes. CORDEL owns its SDL window,
GL context, camera, fixed clock, world state, presentation completion and input.
This is an experiment, not the production hosting choice or a complete narrative API.

From the repository root, use the existing pinned SDK and `.venv` in
[BOOTSTRAP](../../docs/cordel/BOOTSTRAP.md):

```sh
bash narrative/phase1_adapter/tools/run_offscreen.sh tmp/my-narrative-run
```

Use a new output directory. The runner configures/builds the existing native host,
runs four CTests and Python protocol/real-worker tests, executes the native ownership
gate under a 90-second timeout, and compares both branches with ordinary displayed
Ren'Py execution of **the same script**. It collects JSON reports/traces and checks
process cleanup. Dependencies remain the accepted SDL 3.4.8/cgltf/native build and
SDK 8.6.0.26100601+nightly.dirty; no new third-party code or privileged installation.

For a visible desktop build/environment, the optional console presentation is:

```sh
build/phase1-host/cordel-native-host --narrative
```

The verified build is offscreen-only; this desktop command has not been physically
verified. The host prints dialogue/choices to its console. Enter acknowledges;
1/2 select Continue/Stay; F9 cancels. WASD, Space/Ctrl and Shift keep controlling the
camera in every narrative input mode. Click requests relative mouse mode; Escape
releases the cursor. Focus loss clears gameplay actions and pending presentation
keys. The gameplay wait resolves when the camera is within 2 m of the gold column
at (3,2.5,-5). The automated gate uses a controlled native event trigger instead.
There is no production dialogue UI, audio, save system or embedded Python.

## Source boundary

- `protocol/schema.json`: version `cordel.narrative/0.1`, required envelope and
  payload fields, response correlation. It is a small field-contract table used
  by both validators, not a general JSON Schema implementation.
- `worker/bootstrap.py`: SDK Python executes checkout `renpy.py`; dummy drivers,
  Python stdout redirected to stderr, protocol FD 1 kept separately.
- `fixtures/game/script.rpy`: real Label/Say/Menu/Jump/If/Python/Return statements,
  explicit text IDs, two branches and failure/pause fixtures. The ordinary
  reference uses Ren'Py Character/Menu screens; only the worker swaps presentation.
- `worker/service.py`: custom headless CLI command, real Ren'Py context execution,
  synchronous worker-only acknowledgements, cancellation, labelled checkpoint
  restore and explicit dynamic-context cleanup.
- `native/phase1_host/src/narrative`: strict JSON/protocol parser, nonblocking pipe
  I/O thread, bounded client, world-thread session service and ownership probes.
- `tests`: pure protocol tests and real SDK-worker subprocess tests. The finite
  adversarial peer is explicitly a test double, not the narrative executor.
- `tools/collect.py`: checks story equivalence, merges worker traces, derives
  one-process-clock RTT summaries and records source/binary hashes.

Each queue holds at most 64 messages, each JSON line at most 16 KiB, nesting at
most 32 levels. Session/reply/command ledgers also have finite bounds. Overflow
ends transport and requests termination without waiting in the world loop;
shutdown reaps the worker and joins I/O with a one-second grace limit. The worker
is not a sandbox for arbitrary Python. Transport is currently Linux/POSIX only.

See [executed findings](../../docs/cordel/PHASE_1_3_NARRATIVE_OWNERSHIP.md) for exact
measurements, regressions, cancellation semantics and unresolved production costs.
Original CORDEL adapter code/fixture prose use [MIT](LICENSE); upstream Ren'Py and
SDK/dependency notices remain applicable and are not replaced by this license.
