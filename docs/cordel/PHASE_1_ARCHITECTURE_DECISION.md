# Phase 1 conclusion — runtime / narrative architecture

Recorded 2026-10-08. **Phase 1.4 is complete at the Linux software/reference gate.**
[ADR 0001](adr/0001-runtime-narrative-ownership.md) is the canonical accepted
decision. Phase 2.1 is the next implementation milestone; it has not begun.
CORDEL remains **0.1.0-dev**, without a production-readiness or hardware claim.

## What Phase 1.1 proved

Ren'Py can render a meaningful continuously updated static 3D viewport. The
generated CC0 scene has seven meshes and 84 triangles; perspective/depth, elapsed
movement, focus clearing, resize probes, a forced stall, real Say wait and five
session cycles passed. The accepted reference observed about 42.746 viewport
updates/s and 33 viewport updates during a 0.78070 s Say wait.

The actual [displayable](../../examples/cordel_viewport/game/python-packages/cordel_spike/runtime.py)
advances the camera in `render` and requests redraw. Cache invalidation drives
updates; assets are prediction-owned; UI/keymaps/hover contend with gameplay;
mouse uses uncaptured drag; virtual-screen assumptions require a projection
adapter. These ownership/maintenance costs are the reason to reject Ren'Py as
CORDEL's production real-time world owner, rather than claiming it cannot draw 3D.
See [1.1 findings](PHASE_1_1_VIEWPORT.md) and original raw evidence.

## What Phase 1.2 proved

The [C++20 native host](../../native/phase1_host/README.md) loads the same asset
directly and owns SDL, action state, clock, camera, interpolation and GL lifetime.
Its 60 Hz accumulator is independent of render rate, bounded to three catch-up
ticks; the accepted ~250 ms stall moved only 0.150 m. Depth controls, drawable
resize through 1280×720, five zero-owner teardown cycles and full host recreation
passed. The accepted controlled sample measured 59.998 native render iterations/s.

These are different frame metrics from Ren'Py invalidations; no speedup ratio is
justified. Native ownership also exposes GL queue/surface policy: an exploratory
unbounded software submission run stalled simulation and grew RSS. Per-frame
diagnostic glFinish now bounds work; offscreen resize recreates its EGL surface.
One world/GL thread remains susceptible to host/driver stalls. See
[1.2 findings](PHASE_1_2_NATIVE_HOST.md), including retained failed trials.

## What Phase 1.3 proved

A [real Ren'Py AST worker](../../narrative/phase1_adapter/README.md), using the
existing SDK and checkout, executes dialogue, choices, commands, event waits,
branches and variables without acquiring the native window/world. Synchronous
waits stay inside the worker; the native I/O thread supplies bounded queues to the
world thread. Both choice branches match ordinary Ren'Py execution of the same
fixture. Exceptions, worker death, cancellation and finite overflow leave the
world running. One label-scoped primitive checkpoint restores both sides without
replaying its world command; rollback across that effect is rejected.

The fresh Phase 1.4 replay recorded the following held waits at 850×480, Mesa
25.0.7 OpenGL 4.5 core / llvmpipe, SDL offscreen:

| Wait | Wall s | Fixed ticks | Render frames | Dropped s | Worst interval ms |
| --- | --- | --- | --- | --- | --- |
| Dialogue | 0.40871 | 24 | 24 | 0 | 17.370 |
| Gameplay event | 0.41044 | 24 | 24 | 0 | 17.513 |
| Choice | 0.40968 | 25 | 24 | 0 | 17.742 |
| Dialogue, explicit world pause | 0.40887 | 0 | 24 | 0 | 17.729 |
| Dialogue, after resume | 0.41006 | 24 | 24 | 0 | 17.932 |

Native ACK-to-worker-resume observation averages were 17.330 ms for choices,
17.460 ms for dialogue, 17.203 ms for commands and 17.907 ms for event waits,
across 55 samples. These one-clock RTTs include the next native frame pump;
they are not bare pipe latency or cross-process timestamp subtraction. Only
the 27-dialogue group has a reported p95 (19.297 ms).

Fresh overflow high-water remains 64 messages, followed by 20 frames/20 ticks;
native peak RSS remains 71,564 KiB before/after this short finite stress.
That is not a worker sandbox, long soak or total memory bound. Full worker/native
traces remain in ignored local output; compact results are archived in
[Phase 1.4 evidence](evidence/phase1_4/README.md). Earlier evidence remains intact.

## What Phase 1.4 decided

**CORDEL is a native real-time engine with an independent narrative service
boundary. Ren'Py contributes narrative technology; Ren'Py does not own CORDEL.**

Runtime authority is decided: lifecycle, window, real-time input, fixed world
clock, state/camera, GPU resources, gameplay and future physics/animation/AI/
cinematics remain native. Production in-game presentation and future audio device/
world mixing are native-owned. Narrative supplies semantic text IDs, options and
cues, and contributes versioned story checkpoint data to a coordinated CORDEL
checkpoint. No production UI/audio/save system was implemented here.

The narrative contract is decided; deployment is replaceable. Stable typed
commands/results, events, immutable facts, dialogue/choice requests, checkpoints
and cancellation cross a transport-independent service. JSONL/POSIX pipes are
the current implementation. The isolated Ren'Py worker is the default/reference
for subsequent development; shipping two processes is not permanently required.
Embedded Ren'Py/Python is a deferred candidate needing GIL/lifecycle/global/ABI/
reentrancy/restart/exception/device-suppression/shutdown/packaging evidence.

Ren'Py-primary world ownership is rejected using the Phase 1.1 limitations.
A narrative replacement/rewrite is rejected for current scope: useful parser,
AST, menus, translation and authoring semantics already exist; revisit if measured
adapter cost exceeds replacing the supported subset. Preserve the original Ren'Py
application, source and notices for behavioral/upstream comparisons.

OpenGL Core is the development/reference backend. **Production renderer
architecture remains undecided.** Gameplay must submit extracted state rather
than own GPU IDs. Current concrete Client/Session/Renderer dependencies, GPU-owned
visibility and exposed GL headers are documented prototype debt, not the Phase 2
interface. Future scene/physics/editor boundaries use typed IDs/generation-safe
handles, never pointers or GL objects. Small extractions can precede integration;
this ADR does not perform a large refactor.

Native frame order is events → input → bounded narrative pump → fixed world ticks
and command application → animation/cinematic state → interpolated render state
→ renderer/presentation → bounded service work. Initial clock is 60 Hz, max three
catch-up ticks with explicit dropping; physics must not use presentation delta.
Main/world thread owns SDL/simulation/GL; I/O thread owns transport only; worker
owns Ren'Py/Python. Parallel systems require profiling before introduction.

Default rollback stays inside declared narrative-safe regions. Irreversible world
effects block rollback unless coordinated checkpoint restore or explicit compensation
is supported. Future saves include schema/version, world/scene/stable IDs,
narrative state, event watermark, cinematic state where needed and asset version;
never process-local/native handles. Fixture checkpoints do not establish legacy
save compatibility, atomic durable transactions or arbitrary rollback.

## Executed validation and canonical smoke

From repository root with [BOOTSTRAP](BOOTSTRAP.md)'s pinned environment, all five
commands below exited **0** at source revision
`361b410` (full revision and artifact hashes in the evidence manifest):

```sh
bash narrative/phase1_adapter/tools/run_offscreen.sh tmp/phase1_4-narrative
bash native/phase1_host/tools/run_offscreen.sh tmp/phase1_4-native
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/phase1_4-renpy
bash examples/cordel_viewport/tools/check_baseline.sh tmp/phase1_4-baseline
bash scripts/cordel_phase1_smoke.sh tmp/phase1_4-smoke
```

| Gate | Executed result |
| --- | --- |
| Narrative | 186 native ownership checks, 14 Python tests, both ordinary Ren'Py branches match |
| CTest | 4/4 targets: 304 native logic assertions; shared Python/C++ goldens; 23 native protocol/JSON assertions; 5 Python protocol tests |
| Native graphics/runtime | 112 assertions, five zero-owner scene cycles, full host/context recreation; seven meshes/84 triangles unchanged |
| Viewport | 31 helper tests, 83 runtime checks, UI testcase: 8 assertions and 2 hooks |
| Original focused baseline | 29 source tests, 89 selected SDK tests, two The Question endings, lint exit 0 with existing generic advice |
| Canonical architecture smoke | Same narrative build/gate + two displayed Ren'Py references, followed by native self-test on the built binary; aggregate summary passes |

Counts overlap: CTest's five Python protocol tests also appear in the 14-test
worker suite; source/SDK selections overlap. Do not add them as unique coverage.
No known full-suite audio/SDK/history-click failures were patched or rerun as green.
Ren'Py source-native compilation remains blocked by its wider development dependencies;
worker/reference execution uses the existing pinned nightly modules.

The [smoke wrapper](../../scripts/cordel_phase1_smoke.sh) is the canonical selected
architecture command. Use a **new or empty** directory. It reserves separate
stage outputs, fails on command/assertion failure, uses a 300 s outer narrative
timeout and 30 s native timeout with 5 s kill-after, and writes `smoke-summary.json`
only after validated stage success. It avoids a duplicate configure/build/CTest
for native graphics, and does not rerun exploratory throughput measurements.
Full viewport/baseline regressions remain the separate commands above.

An initial invocation at `tmp/phase1-final` exited **2**, correctly refusing to
overwrite existing Phase 1.1 evidence. Fresh `tmp/phase1_4-smoke` passed. Reusing
that populated smoke directory and invalid argument count were also checked as
controlled refusals. This is runner protection, not a graphics or architecture
failure. Evidence includes the refusal results.

## Open qualification and development targets

Linux x86_64 is the primary environment; actual evidence is Debian 13.6 offscreen
software. Windows x86_64 is the next qualification target, not certified support;
the current Linux/POSIX transport must be replaced/ported and packaged. macOS
follows a viable backend route. No hardware/device gate was closed by this ADR.

Before production readiness: visible accelerated GPU, frame pacing/GPU timing,
resource lifetime, resize/fullscreen/DPI/context failure; physical keyboard,
relative mouse/Escape/alt-tab/focus loss, multiple controllers/hotplug; longer
stories, localization/voice/Character/large choices/restart/durable save;
platform packaging/licenses/clean-machine shutdown. Single-thread GL blocking,
SDK footprint/internal-context maintenance, process latency/deployment and
rollback-safe side effects remain open risks.

Phase 2 budgets are provisional: fixed 60 Hz, world/physics <4 ms on a future
documented reference development machine, main-thread CPU frame <16.67 ms at
60 fps, bounded/nonblocking narrative handling and zero normal-operation dropped
simulation. They are targets requiring profiling, not promised minimum specs.

## Phase 2 entry

Ready for **Phase 2.1 — Collision / Query Adapter**. Compare Jolt and Bullet
before selecting a provider, with an optional minimal query-only baseline.
Measure capsule sweeps/raycasts/static collision, slopes/steps/triggers/layers/
contacts, native event ordering, fixed-clock cost, integration/build footprint,
exact licenses and Linux/Windows viability. Keep queries/IDs independent of GL
and physics pointers. This task stops at the ADR and gate; Phase 2 has not begun.
