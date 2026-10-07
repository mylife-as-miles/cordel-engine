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
3. **1.3 Narrative ownership experiment.** Run one line/choice/wait while the
   viewport continues updating. Compare adapter feasibility, UI/input/audio
   ownership, exception/restart and pause behavior. Exit: measurable continuous
   frames during story wait and correct cancellation; keep Ren'Py reference output.
4. **1.4 Runtime/backend ADR.** Choose ownership and next prototype backend based
   on 1.1–1.3 evidence, maintenance effort and platform constraints. Set hardware
   and performance test targets. Exit: reviewed decision with alternatives and
   known gaps, and one automated smoke path on the selected development target.

**Exact next task:** **1.2 — Native Host Comparison Spike**. Create one isolated
native desktop host with an explicitly experimental backend, load the same CC0
scene and camera convention, and compare fixed update plus interpolation against
the Ren'Py redraw-driven reference. Repeat focus/input, resize, forced stall and
five enter/exit probes with comparable CPU diagnostics, documented dependencies/
licenses and a reproducible build. Add desktop/hardware evidence when available.
No production API decision, physics, ECS or editor yet. Keep Phase 1.1's open
device/GPU checks visible; Phase 1.2 has not started.

## Phase 2 — Third-person character controller

1. **2.1 Collision/query adapter:** choose one physics candidate after capsule
   sweep/raycast/contact experiments. Exit: blocked walls, ground, slopes and
   steps with documented limits and reproducible collision tests.
2. **2.2 Character motor:** movement, gravity, ground state and a capsule player;
   one fixed update owns motion. Exit: consistent travel at different presentation
   rates, no wall penetration in test scenes, focus loss stops input.
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

Require full native-build evidence before native ABI changes; measured hosting
evidence before replacing the main loop; coordinated save tests before migrating
production story state; runtime schemas before editor technology choice. Update
known baseline failures whenever upstream changes.

Console/mobile support, ray tracing, large open worlds, multiplayer, bespoke
animation middleware and AAA content scale are deferred. Each would need separate
requirements, resources, licensing and test hardware. No schedule is inferred from
these phase numbers.
