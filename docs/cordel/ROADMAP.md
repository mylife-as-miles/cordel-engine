# Engineering roadmap

These are sequenced planning targets, not delivery-date or staffing commitments.
Every milestone produces reviewable source, reproducible commands, test evidence,
and a short decision record where ownership or technology changes. Preserve the
Ren'Py baseline as a regression/reference path.

## Phase 0 — Foundation

| Milestone | Acceptance evidence/status |
| --- | --- |
| 0.1 Repository provenance | Full official clone, `upstream` remote, `cordel/bootstrap`, foundation hash recorded |
| 0.2 Development baseline | Isolated Python setup; sample lint and focused offscreen ending smoke pass; source-build and full-suite limits precisely recorded |
| 0.3 Architecture investigation | Source-based lifecycle/render/input/narrative/tooling audit completed |
| 0.4 Identity and planning | `cordel/project.toml`, project overview, setup/audit/proposal/roadmap/upstream documentation |
| 0.5 Review and local commit | Validate metadata/links/diff; commit locally, no remote publishing |

The table records the original Phase 0 acceptance. The owner subsequently
authorized publication with fresh Git history: current development uses `main`
in `mylife-as-miles/cordel-engine`. Source, evidence and license notices remain;
the original branch and commits are historical provenance rather than ancestry.

Phase 0 satisfies the documented-blocker allowance; it does not close native
compilation, full tests, or hardware execution. Before a native host depends on
the source build, provision matching dependencies and establish that build in CI.
Keep the known history-click failure separate from CORDEL identity changes.

## Phase 1 — Real-time 3D proof of concept

1. **1.1 Ren'Py viewport reference spike.** Add an isolated
   `examples/cordel_viewport` project with a tiny, license-clear static glTF scene,
   depth testing, and a controllable perspective camera. Use current model/shader
   facilities with minimal opaque materials. Establish coordinate conversion,
   held-key actions, analog input experiment, relative mouse policy and focus-loss
   clearing. Do not modify existing samples or imply a production backend choice.
   Software reference gate **passed 2026-10-07**; see
   [executed Phase 1.1 findings](PHASE_1_1_VIEWPORT.md). Static glTF, perspective,
   actual depth controls, held movement/drag-look/focus clearing, four drawable
   aspects, forced stall, real Say idle and five enter/exit cycles pass on the
   pinned offscreen baseline. CPU fragments/intervals and cache ownership are
   measured. One import/session is retained across prediction eviction.
   **Open:** desktop input/cursor testing, physical controller ownership, hardware
   timing, window growth beyond the initial offscreen drawable, high-DPI behavior
   and actual GPU buffer destruction/residency. Mouse is explicitly uncaptured;
   raw-axis adapter verification does not certify end-to-end controller support.
2. **1.2 Native host comparison spike.** Build a small isolated desktop host using
   one explicitly experimental backend. Show the same scene/camera, fixed update
   plus render interpolation, resize/shutdown, and diagnostics. Exit: repeatable
   build on one desktop target, no Python hot-loop dependency, comparable trace
   and documented library/license footprint. One backend suffices for this spike.
   Software reference gate **passed 2026-10-07**; see
   [executed Phase 1.2 findings](PHASE_1_2_NATIVE_HOST.md). Isolated C++20,
   pinned SDL3/OpenGL core, same seven-mesh/84-triangle fixture, 60 Hz accumulator,
   interpolation, three-tick catch-up, queued controls/focus, numeric depth,
   four drawable sizes including 1280×720, five zero-owner teardown cycles and
   full host recreation pass. 304 native assertions, shared Python/C++ goldens,
   112 graphics/runtime assertions and Phase 1.1/baseline replays pass.
   **Open:** physical relative mouse/alt-tab/controllers, visible desktop/high-DPI,
   hardware/GPU timing, driver residency, long soaks and production frame queue.
   Offscreen resize recreates its EGL surface; glFinish is diagnostic backpressure.
   Phase 1.1 and Phase 1.2 were fast-forwarded into `main`; Phase 1.2 acceptance
   into main was verified/pushed on 2026-10-08.
3. **1.3 Narrative ownership experiment.** Run one line/choice/wait while the
   viewport continues updating. Compare adapter feasibility, UI/input/audio
   ownership, exception/restart and pause behavior. Exit: measurable continuous
   frames during story wait and correct cancellation; keep Ren'Py reference output.
   Linux software ownership gate **passed 2026-10-08**; see
   [executed Phase 1.3 findings](PHASE_1_3_NARRATIVE_OWNERSHIP.md). Real headless
   Ren'Py AST worker, strict versioned JSONL, bounded asynchronous native queues,
   dialogue/choice/event waits, explicit input routing, actual beacon command/fact,
   cancellation in three wait states, exception/death/malformed/overflow survival,
   fixture checkpoint and declared rollback boundary pass. 186 native gate checks,
   14 Python tests, 23 native protocol assertions and both ordinary reference
   branches pass; accepted 1.2/1.1/baseline regressions pass. Native GL counts end
   at zero. Phase 1.3 was fast-forwarded into `main` at accepted `8d685666`
   and pushed on 2026-10-08 before ADR work; no squash or history rewrite.
   **Open:** production script/Character/UI/audio/localization/save compatibility,
   in-process Python/GIL evidence, other-OS transport, device/hardware/DPI,
   long soaks, general rollback and durable coordinated saves. Separate-process
   hosting is an experimental isolation boundary, not the production decision.
4. **1.4 Runtime / Narrative Architecture Decision Record.** **Complete at the
   Linux software/reference gate, 2026-10-08.** [ADR 0001](adr/0001-runtime-narrative-ownership.md)
   accepts native runtime authority and an explicit transport-independent narrative
   service. Isolated Ren'Py worker is the current development default; embedded
   hosting is deferred, not certified. OpenGL Core is the reference backend, not
   the permanent production renderer. Native input/UI/future audio, thread and
   checkpoint/rollback ownership, typed IDs, provisional budgets and platform/device
   gates are documented. All four accepted regressions and the canonical smoke
   command pass; see [Phase 1 conclusion](PHASE_1_ARCHITECTURE_DECISION.md) and
   [fresh evidence](evidence/phase1_4/README.md). Only software/offscreen and
   synthetic-input behavior is verified. Hardware/devices, Windows transport,
   full narrative compatibility, packaging and durable saves remain open.

**Phase 1 architectural foundation is complete.** Production runtime qualification
is not. Preserve all three references; do not turn passing software probes into
hardware, arbitrary-script compatibility or AAA performance claims.

**Latest completed gate:** Phase 2.2 — Third-Person Character Motor, Linux
software/native gate passed 2026-10-08. CORDEL query-owned capsule movement,
slopes/steps/recovery, fixed-tick rate independence and real narrative continuity
are verified; see [report](PHASE_2_2_CHARACTER_MOTOR.md) and
[evidence](evidence/phase2_2/manifest.json). Jolt remains the development provider;
Bullet's complete Phase 2.1 comparison gate remains green.
**Exact next task: Phase 2.3 — Follow / Orbit Camera.** It has not begun.

Canonical selected architecture smoke, using BOOTSTRAP's existing environment:

```sh
bash scripts/cordel_phase1_smoke.sh tmp/new-phase1-smoke
```

Use a new or empty directory. This composes the narrative gate and both ordinary
Ren'Py reference branches, then runs native graphics/lifecycle on the same binary
without another build. Full 1.1 and baseline checks stay separate regression gates.

## Phase 2 — Third-person character controller

1. **2.1 Collision / Query Adapter: passed software gate 2026-10-08.**
   Both Jolt and Bullet pass 207 identical assertions, five teardown cycles and
   repeated warmed workloads. Jolt is the selected development backend; native
   60 Hz/narrative continuity/GL regressions pass. See
   [report](PHASE_2_1_COLLISION_QUERY.md) and [ADR 0002](adr/0002-physics-query-backend.md).
   **Open:** Windows/macOS, accelerated/device evidence, dynamic/mesh contacts,
   production character qualification and production timing budgets. Completed engineering steps:

   - **Candidate/build inventory:** pin exact revisions/URLs, enabled features,
     transitive licenses and build/runtime footprint; assess Linux/Windows viability.
     Do not claim Windows success without executing its build/tests.
   - **Query boundary:** stable typed body/shape/world IDs, validated lifetimes,
     collision layers; raycast and capsule-sweep probes against a static scene.
     Gameplay does not retain library pointers or OpenGL IDs.
   - **Traversal/contact probes:** reproducible walls/ground/slopes/stairs/steps,
     triggers and contacts, including expected limitations and penetration margins;
     ordered events delivered on the authoritative 60 Hz world thread.
   - **Comparison/selection gate:** identical query vectors and workload for both
     candidates, measured fixed-tick costs/bounded catch-up, integration effort,
     footprint and licensing; decision report and accepted Phase 1 regressions.
     Target world/physics <4 ms on a documented development machine, total main
     thread <16.67 ms at 60 fps, zero normal dropped time; targets require profiling.
     Exit: select a provider with evidence and explicit open platform/device gates.
     Do not fold animation, combat or a full character motor into the comparison.
2. **2.2 Character motor: passed software gate 2026-10-08.** CORDEL-owned
   upright query capsule, fixed 60 Hz movement/gravity, bounded sweep/slide/recovery,
   10–45° walkable ramps/55° downhill slide, 0.10–0.30 m steps/0.45–0.60 m blockers,
   ceilings/doors/ledges/sensors. 43 scenarios, 10,202 assertions, 25 synthetic
   presentation comparisons (zero state difference), real 30/60/120/uncapped
   offscreen renders, five zero-owned-resource cycles and 60 simulated seconds
   of deterministic soak pass. Motor/world/physics continue during real Ren'Py
   dialogue/choice/event waits; explicit pause stops ticks while rendering continues.
   Both Phase 2.1 query candidates, Phase 1 smoke and full viewport/baseline pass.
   **Open:** desktop/device/hardware, Windows/macOS, dynamic/moving-platform/mesh
   contacts, production tuning, durable motor checkpoints and production soak.
   Report: [Phase 2.2](PHASE_2_2_CHARACTER_MOTOR.md).
3. **2.3 Follow/orbit camera:** obstruction handling, rotation and camera distance.
   Exit: no clipping through tested walls; gameplay/UI camera ownership is explicit.
4. **2.4 Basic locomotion:** import one licensed skeleton and idle/walk/run clips,
   blend with motor state. Exit: correct pose sampling and transitions; choose and
   test root-motion policy. No claim of advanced character performance yet.

## Phase 3 — Narrative integration

1. **3.1 Command/event protocol:** typed IDs, ordered requests/acks, waits,
   failures/cancellation and bounded queues. Exit: duplicate/stale commands rejected
   or idempotent; invalid targets and session shutdown produce defined results.
2. **3.2 Dialogue/choice slice:** gameplay trigger starts story, localized line
   renders, choice changes world state. Exit: world simulation continues through
   story wait; compare branching/localization behavior against the Ren'Py reference.
3. **3.3 Coordinated checkpoint:** versioned world and narrative restore, asset
   IDs and event watermarks. Exit: save/load at a dialogue boundary restores both
   states; interrupted writes and incompatible schemas fail without corrupting state.
4. **3.4 Rollback/side-effect policy:** define checkpoint/compensation boundaries.
   Exit: rollback does not duplicate irreversible world effects or orphan objects;
   supported and unsupported legacy Ren'Py behavior is documented.

## Phase 4 — Character and world systems

1. **4.1 Scene lifecycle/serialization:** hierarchy, stable IDs, components and
   scene transitions. Exit: cyclic parents rejected; destroyed references detected;
   scene round trips and unloading have verified resource behavior.
2. **4.2 Asset cooking/residency:** mesh/material/texture/animation imports,
   dependency hashes and cache invalidation. Exit: deterministic artifacts;
   missing assets diagnosed; measured memory budget in one representative scene.
3. **4.3 Rendering slice:** PBR material convention, lights, shadows and a measured
   post-processing chain. Exit: reference images, correct color spaces and target
   hardware timings; advance features only within measured budgets.
4. **4.4 Animation and interaction:** blend graph, layers, root motion and one IK
   interaction with an environment object. Exit: synchronization and interruption
   work, no stale pose/target references, performance trace at agreed character count.
5. **4.5 World behavior:** triggers/traversal, navigation/perception and one scripted
   encounter; positional sound. Exit: interactions/nav replanning/events are
   repeatable and audio listener/emitter ownership is verified.

## Phase 5 — Cinematic systems

1. **5.1 Sequencer and bindings:** camera/animation/dialogue/event tracks with
   stable scene IDs. Exit: missing bindings diagnosed; deterministic cue ordering.
2. **5.2 Virtual camera/character handoff:** blends between gameplay and a sequence.
   Exit: same player instance returns with correct pose/input/camera ownership.
3. **5.3 Performance and audio synchronization:** dialogue/animation timing and
   spatial/cinematic mix transitions. Exit: measured synchronization and drift
   limits, with defined pause behavior.
4. **5.4 Seek/skip/interruption:** cancel, skip, load mid-sequence and missing assets.
   Exit: no repeated one-shot side effects or locked controls; checkpoint compatibility.

## Phase 6 — Engine editor

1. **6.1 Project/asset hub:** build/launch/import diagnostics, retain useful launcher
   workflows. Exit: new project and packaged runtime smoke from a clean environment.
2. **6.2 Scene composition:** viewport, transforms, inspector, hierarchy, lighting
   and undo/redo. Exit: edit/save/reopen matches runtime scene serialization.
3. **6.3 Character/gameplay authoring:** placements, interaction bindings and debug
   overlays. Exit: invalid content fails validation with actionable object references.
4. **6.4 Cinematic/narrative authoring:** timeline, camera tracks, dialogue preview
   and localization validation. Exit: author the Phase 5 slice without hand-editing
   internal files and verify runtime output.
5. **6.5 Play/build isolation:** preview world does not mutate the edit world;
   versioned project migrations and packaging/license manifest. Exit: packaged
   slice runs on the selected desktop support matrix from clean installs.

## Progress gates and deferred scope

Require matching Ren'Py source-native build evidence before changing its extension
ABI; preserve the accepted native ownership ADR and measured hosting boundary; coordinated save tests before migrating
production story state; runtime schemas before editor technology choice. Update
known baseline failures whenever upstream changes.

Console/mobile support, ray tracing, large open worlds, multiplayer, bespoke
animation middleware and AAA content scale are deferred. Each would need separate
requirements, resources, licensing and test hardware. No schedule is inferred from
these phase numbers.
