# ADR 0001 — Native runtime and narrative ownership

## Status

**Accepted.** Architectural direction for CORDEL ENGINE 0.1.0-dev. This accepts
ownership rules and a development reference; it does not certify a production
engine, permanent narrative deployment model or production graphics backend.

## Date

2026-10-08. Based on accepted Phase 1.1–1.3 source and Linux software/offscreen
evidence. Phase 1.3 was fast-forwarded into `main` at
`8d685666ab0fe76ce90ce6046efc7d4d78a6a14b` before this decision branch; the
previous main was `810df7e2d8587a5289cd84873b14467562bef92f`. No squash,
force-push, history rewrite or restoration of upstream ancestry occurred.

## Context

CORDEL targets cinematic third-person games with a continuously simulated world
and asynchronous narrative. Ren'Py contributes mature parsing, AST execution,
labels, Say/Menu semantics, branching, Python scripting, translation infrastructure
and persistence concepts. Its ordinary application owns nested script/display
interactions, prediction, UI input and rendering. Those ownership assumptions
need a deliberate boundary rather than an expanding viewport adapter.

Three experiments now coexist: a Ren'Py-hosted viewport, a native host of the
same seven-mesh/84-triangle scene, and a real Ren'Py AST worker with the native
world still authoritative. This milestone records their implications. It adds
no physics, renderer rewrite, embedded interpreter, production UI or audio engine.

## Evidence

The review inspected actual implementations and raw reports/traces, including
failed exploratory trials. Recorded source hashes were checked against each
manifest's code revision; Phase 1.2/1.3 recorded artifact hashes also matched.
Earlier reports remain historical records, with their original next-gate wording.

| Experiment and inspectable evidence | Observation | Decision relevance / limit |
| --- | --- | --- |
| [1.1 report](../PHASE_1_1_VIEWPORT.md), [raw report](../evidence/phase1_1/report.json), [failures](../evidence/phase1_1/exploratory-failures.json) | Static glTF, perspective, real depth, time-based movement, five sessions and real Say wait passed. About 42.746 viewport updates/s; 33 updates during a 0.78070 s Say wait | Ren'Py is capable of this viewport. It does not provide the required independent world clock or ownership separation |
| [1.1 runtime source](../../../examples/cordel_viewport/game/python-packages/cordel_spike/runtime.py) | `Viewport.render` advances simulation and requests redraw; `SessionModel` pins prediction-owned import data; skip/hover/keymap interactions affect input; mouse is uncaptured drag | Redraw/cache coupling, input contention, virtual-screen assumptions and internal renderer dependence are structural maintenance costs |
| [1.2 report](../PHASE_1_2_NATIVE_HOST.md), [controlled sample](../evidence/phase1_2/controlled.json), [stall](../evidence/phase1_2/stall-probe.json), [lifecycle](../evidence/phase1_2/lifecycle.json) | Independent 60 Hz accumulator/interpolation; 59.998 native render iterations/s controlled; 250.143 ms stall executes three ticks/50 ms and moves 0.150 m; five explicit zero-owner teardown cycles | Native clock, input and GL ownership are executable, observable and independent of Python in the frame loop |
| [1.2 exploratory record](../evidence/phase1_2/exploratory-findings.json) and native renderer/platform sources | Unbounded offscreen GL submission stalled the world and grew peak RSS to 1,564,700 KiB; diagnostic `glFinish` bounds work. Offscreen resize recreates its EGL surface | Native ownership exposes driver/surface policy; it does not make a single-threaded loop immune to GL stalls or prove production synchronization |
| [1.3 continuity](../evidence/phase1_3/continuity-results.json), [failures](../evidence/phase1_3/failure-results.json), [backpressure](../evidence/phase1_3/backpressure-results.json) | Dialogue/event/choice waits each render 24 frames and execute 24–25 ticks over about 0.4 s with no dropped time. Worker death detected in 34.631 ms; subsequent frames/ticks continue. Finite flood hits the 64-message bound | Blocking/failing narrative can be isolated without transferring world ownership. Short software runs do not establish arbitrary-script compatibility, sandboxing or long-soak memory bounds |
| [1.3 reference comparison](../evidence/phase1_3/reference-comparison.json), [checkpoint](../evidence/phase1_3/checkpoint-results.json), [latency](../evidence/phase1_3/latency-results.json) | Both branches match ordinary Ren'Py; fixture world+narrative restore succeeds without command replay. Native ACK-to-resume observation means about 17 ms | Preserved fixture semantics and feasible coordination; RTT includes next frame pump, not bare transport latency. Full Character/localization/save behavior remains open |

Native render iterations and Ren'Py viewport invalidations are different metrics;
no speedup ratio follows from these numbers. All graphics evidence uses Mesa
llvmpipe, SDL offscreen and the tiny scene. GPU timing and visible hardware/device
presentation are unverified. [Phase 1.4 validation](../PHASE_1_ARCHITECTURE_DECISION.md)
records fresh regressions and the canonical smoke gate separately from historical
measurements.

## Decision

> CORDEL is a native real-time engine with an independent narrative service
> boundary. Ren'Py contributes narrative technology to CORDEL; Ren'Py does not
> own CORDEL.

Three decisions have distinct lifetimes:

1. **Runtime ownership: decided.** Native CORDEL permanently owns the real-time
   world. Ren'Py must not regain the main world loop.
2. **Narrative interface: decided.** Stable IDs and typed, versioned semantic
   requests/events cross an explicit transport-independent service boundary.
3. **Narrative deployment transport: replaceable.** Native CORDEL plus an isolated
   Ren'Py worker is the verified default/reference for subsequent development.
   Two-process shipping is not permanently required. Embedded hosting remains
   a compatible candidate pending dedicated evidence.

OpenGL Core is the **development/reference renderer**. The **production renderer
architecture is undecided**. SDL3/C++20 remain the executed native reference
stack, without introducing a public platform/plugin ABI.

## Decision Details

### Authority and subsystem contracts

| Owner | Authority and boundary |
| --- | --- |
| CORDEL core/platform | Application lifecycle, window, platform events, real-time input, clock, fixed accumulator, startup/shutdown and service lifetime |
| CORDEL world/gameplay | World/scene state, transforms, camera, gameplay, stable world identities; future physics, animation, AI and cinematics run under native authority |
| CORDEL renderer | GPU buffers, programs, textures, targets, synchronization/fences and command submission. Gameplay never owns GL IDs or device objects |
| CORDEL presentation/input | Production HUD, subtitles/dialogue/choice presentation, accessibility, cinematic overlays and completion acknowledgements. Narrative supplies speaker, text/text ID, stable option IDs and hints |
| CORDEL audio (future) | One real-time device and world mix, listeners/emitters, voice, SFX, music and cinematic timing. Narrative emits semantic voice/music/dialogue/cinematic cues; no competing Ren'Py production mixer |
| Narrative subsystem | Script execution, labels/branches, narrative variables, semantic dialogue/choices/waits, localization-compatible identity and narrative checkpoint data |
| CORDEL checkpoint coordinator (future) | Consistent world+narrative capture/restore, versions, IDs and event watermarks; narrative contributes its portion |

Production UI/audio and the checkpoint coordinator are ownership decisions,
**not implemented systems**. Preserve the ordinary Ren'Py application and its
upstream source/notices as the behavioral reference for AST/dialogue, localization,
adapter regressions and upstream comparisons. Its screen language may inform
tools/authoring surfaces; it must not take over the native gameplay window.

### Transport-independent narrative service

Logical operations include start/cancel session, `DialogueRequest`, `ChoiceRequest`,
`NarrativeCommand`/`CommandResult`, `GameplayEvent`, immutable `WorldFact`, checkpoint
capture/restore, failure and shutdown. `cordel.narrative/0.1` establishes request,
session, message, sequence and correlation semantics; its JSON serialization and
POSIX pipe/process mechanics are replaceable implementation details.

Conceptual service operations: `start_session`, bounded `poll_events`,
`acknowledge_dialogue`, `select_choice`, `submit_command_result`, `publish_fact`,
`publish_event`, `request_checkpoint`, `restore_checkpoint`, `cancel`, and
`shutdown`. Replies are queued completions/errors; no gameplay-thread blocking
call waits for narrative execution. Protocol incompatibility fails explicitly.

`ProcessNarrativeTransport`, `InProcessNarrativeTransport` and
`TestNarrativeTransport` could implement the same semantics. They must preserve
validation, correlation, cancellation, queue limits and thread confinement.
Higher-level gameplay must not know file descriptors, PIDs, JSONL framing,
CPython addresses or interpreter calls. No interchangeable transport classes
have been implemented in this ADR.

The current `Session` holds a concrete `Client&`; `Client` combines protocol,
queues and Linux child lifetime. Before broader gameplay integrates it, extract
the smallest logical service/transport seam and prove equivalent fake/process
behavior. Retain process health/cleanup diagnostics behind that seam. The fixture's
hardcoded labels, two choices and named beacon are not a generalized API.

No narrative state may retain entity/physics pointers, GL handles, renderer
objects, memory addresses or direct mutable scene references. Use typed stable
IDs externally and generation-safe handles internally; validate target existence
and generation at application time. The exact registry/ECS/scene design is deferred.
Do not treat fixture mesh-name strings as a complete identity system.

### Frame and thread ownership

Expected order:

```text
Platform events → Input snapshot → Bounded narrative message pump
→ Fixed simulation ticks (gameplay / future physics / future AI / commands)
→ Animation and cinematic state → Interpolated render extraction
→ Renderer submission → Presentation → Bounded background/service work
```

Narrative I/O never mutates the world. Validate incoming commands, enqueue them,
and apply world changes on the authoritative fixed-tick boundary. Completion
acks identify the applied tick; duplicate/stale requests must not repeat effects.
Explicit pause/resume/cancel and checkpoint control use a documented world-thread
safe boundary even while ticks are paused; they cannot wait for a tick that cannot
occur. Arbitrary mutation from transport callbacks is forbidden.

Initial development clock: **60 Hz**, fixed `1/60 s`, maximum **three catch-up
ticks/frame** (50 ms). Drop excess whole backlog ticks with diagnostics; retain
the sub-tick remainder for alpha. Interpolate previous/current position and pitch
and shortest-arc yaw; do not extrapolate. Rendering can lag by up to one tick.
Future physics uses this clock, never presentation delta. Tick rate is not yet a
permanent content ABI. Narrative wait leaves simulation running; explicit pause
freezes simulation only, keeps events/render/service communication alive, and
resets accumulator debt on pause/resume.

| Execution location | Affinity / allowed work |
| --- | --- |
| Main/world thread | SDL initialization/window/events, input routing, fixed world state, command application, render extraction, current GL context/use/deletion and presentation |
| Narrative I/O thread | Framing/validation and bounded queues only; no world mutation, SDL UI, GL or Ren'Py execution |
| Narrative worker process | Ren'Py/Python AST, stores, waits and context cleanup; never the native window, GL context or world clock |

Renderer owners must die before context/window teardown. Worker shutdown is
bounded and reaped; process/thread joins belong to teardown rather than waiting
inside active world updates. Do not introduce render/physics threads, a job
system, task graph or worker pool without profiling. Future in-process execution
must explicitly prove GIL, interpreter and thread/reentrancy rules.

Input mode and focus are native-owned. Current modes are Gameplay,
NarrativeAcknowledge and NarrativeChoice: WASD stays gameplay-owned; Enter and
1/2 route presentation separately. Focus loss clears held/analog/pending input;
Escape releases capture. Completion, cancellation and failure release narrative
ownership. Future modal/cinematic policies must be explicit rather than inheriting
Ren'Py keymap contention. Physical relative-mouse behavior remains unqualified.

### Renderer boundary and prototype debt

World/render extraction should eventually submit immutable `RenderFrame`,
`ViewData`, `VisibleMesh`, generation-safe `MaterialHandle`, `LightData` and
`DebugDraw` data. Only a backend maps these to GPU objects. Drawable dimensions
drive projection and viewport; presentation changes cannot redefine world units.
Use the verified RH +Y-up, -Z-forward, metre reference convention for subsequent
experiments; storage/import conversions remain explicit, without committing a
permanent serialized asset ABI.

Current `Renderer` owns imported CPU scene plus GL resources; its header exposes
`GLuint`/SDL GL types. `Session::apply` calls `Renderer::set_visible` directly,
`FrameBoundary` takes `Renderer&`, and checkpoint restore directly changes render
visibility. These are prototype dependencies. Phase 2 world collision/state must
not adopt GL IDs or GPU-owned scene state. Introduce only a small authoritative
world state/render extraction seam when needed; keep existing reference probes
working. A huge renderer abstraction/refactor is not an ADR deliverable.

Per-frame `glFinish` remains diagnostic backpressure, not the production fence
policy or GPU profiling. Resource counts prove CORDEL owners and deletion calls,
not driver physical memory release. Device failure/recovery needs its own policy.

### Save, rollback and failure rules

Future `CORDEL Checkpoint`: world schema/version, world state, scene state,
stable entity IDs, narrative checkpoint/schema, event watermark, cinematic state
where needed and asset manifest/version. Capture at coordinated safe boundaries,
validate compatibility before restore, and define atomic durable writes and failed
restore handling before claiming saves. Never serialize raw pointers, GL/physics
handles or process-local IDs. Phase 1.3 restores only named fixture labels and
primitive state in a current session; it does not certify legacy Ren'Py saves.

Default rollback is confined to explicitly rollback-safe narrative regions.
Irreversible world effects establish a boundary: block crossing it, restore a
coordinated world checkpoint, or implement explicit compensation. Do not infer
automatic reverse execution of gameplay or replay commands after restore.

| Failure | Required behavior / evidence limit |
| --- | --- |
| Ordinary narrative-script error | Fail the session, surface diagnostics, cancel waits and release input; world continues where isolation permits. Verified for fixture Python exception |
| Worker death | Detect EOF/process death, fail session/outstanding requests and release input; world may continue. Restart policy/automatic recovery is not yet certified |
| Invalid command/target | Return explicit failure without partial state corruption; apply side effects once at authoritative boundary |
| Queue/framing overflow | Enforce finite bounds, report cause and terminate/cancel safely; never permit unbounded adapter growth |
| Renderer/device failure | Separate future recovery policy; narrative isolation cannot protect against native/driver failure |

Reference bounds are 16 KiB/line, depth 32, 64 incoming and 64 outgoing messages,
eight incoming messages pumped/frame; pending/ID/latency ledgers are also finite.
This is adapter backpressure, not a security sandbox or total Python memory bound.
Cancellation invalidates outstanding correlations and retired session IDs;
late replies cannot resurrect effects. General exactly-once delivery across
process restart/durable saves remains deferred.

### Development platforms and budgets

Primary engineering environment: **Linux x86_64**, currently verified only as
Debian 13.6 SDL offscreen/Mesa 25.0.7 llvmpipe. Next qualification: **Windows
x86_64** (not currently compatible/certified by evidence: POSIX transport needs
a Windows implementation and packaging validation). macOS is planned after a
viable renderer/backend path. CORDEL is not intended to be Linux-only.

Provisional Phase 2 budgets on a future documented reference development machine:
60 Hz simulation; world/physics simulation **<4 ms**; total main-thread CPU frame
**<16.67 ms** at 60 fps; narrative handling bounded/nonblocking; **zero dropped
simulation time in normal operation**. Forced-stall drop is deliberate and logged.
These are development targets, not achieved production measurements or minimum
hardware specifications. Profile before tightening budgets; record CPU wall
fragments, process CPU and GPU timing separately.

## Alternatives Considered

| Architecture | Decision | Reason |
| --- | --- | --- |
| Ren'Py owns runtime | **Reject for production real-time 3D gameplay** | Phase 1.1 proves useful 3D rendering but couples simulation to redraw/cache, assets to prediction, gameplay to UI input and projection to virtual-screen internals; no independent fixed clock |
| Native runtime + isolated Ren'Py worker | **Accept as current default/reference implementation** | Phase 1.3 actually proves asynchronous waits, branching, failure isolation, bounded queues, cancellation and checkpoint feasibility with native clock/resources retained; deployment remains revisable |
| Native runtime + embedded Ren'Py/Python | **Defer: candidate optimization/deployment architecture** | No executed embedding evidence. GIL/interpreter lifecycle, globals, restart, extension ABI, thread restrictions, exceptions, window/audio suppression, shutdown/reinitialization, packaging and crash propagation remain unknown |
| Replace Ren'Py narrative completely | **Reject for current scope; defer reconsideration** | Rebuilding parser/AST/dialogue/menus/translation/persistence/authoring creates risk without a demonstrated need. Revisit if measured adapter maintenance exceeds replacing the required supported subset |
| Native runtime + transport-independent narrative boundary | **Accept as architectural rule** | Explicit authority, stable IDs, validation, acknowledgement and failure semantics survive process, in-process or test implementations |

## Consequences

Phase 2 can evolve gameplay/collision under one clock and resource owner without
waiting for narrative execution. Original Ren'Py remains a comparison application;
its upstream directories and license notices stay intact. The current worker
preserves useful real AST behavior, while presentation/audio/save integration
become explicit CORDEL responsibilities with a declared supported subset.

There are two development processes, SDK/native-module packaging costs and a
frame-pump reply latency. The logical transport and render extraction seams need
small future extractions; this ADR does not rename prototype directories, rewrite
the native host or promise drop-in compatibility with arbitrary Ren'Py projects.

## Risks

Ren'Py context/global assumptions and SDK ABI changes can break the adapter.
Fixture support may underestimate Character, localization, voice and large-choice
semantics. Worker isolation is not sandboxing arbitrary Python. Linux pipes and
SDK-relative build paths are not a Windows distribution. Single-thread GL stalls,
software-only graphics evidence, narrow checkpoints and unsafe rollback effects
remain material risks. Untested embedding could propagate Python/native crashes.

## Mitigations

Keep pinned dependencies/source provenance, notices and original reference
execution. Preserve raw exploratory failures, known-full-suite exclusions and
cross-language camera goldens. Run the [canonical smoke gate](../../../scripts/cordel_phase1_smoke.sh)
and milestone regressions. Require fresh session IDs, bounded queues, correlated
acknowledgements, explicit cleanup and supported-subset tests. Profile representative
content/hardware and qualify transports before changing defaults.

## Deferred Decisions

Production graphics API/abstraction and GPU in-flight scheduling; permanent
narrative deployment/embedding; physics library; entity/scene registry/ECS;
animation middleware; audio/UI implementations; durable checkpoint transactions,
legacy saves and rollback compensation; editor technology and content ABI.
No production library is chosen solely from a feature/license list.

## Validation Gates

Phase 1 completion requires passing fresh narrative/native/viewport/baseline
regressions and the canonical smoke command, recorded in the Phase 1 conclusion.
Known full-suite failures remain separate; Linux offscreen is a functional gate.

Before production readiness, qualify:

- **Graphics:** visible accelerated GPU/backend, hardware frame pacing and GPU
  queries, buffer/resource lifetime, resize, fullscreen/window modes, DPI and
  context/device failure behavior where feasible. Replace diagnostic synchronization
  only with evidence that in-flight work stays bounded and simulation responsive.
- **Input:** physical keyboard, true relative mouse/capture/Escape, alt-tab,
  focus loss, multiple controllers and hotplug. Synthetic SDL events do not close
  these gates.
- **Narrative:** longer scripts, supported Character behaviors, localization,
  voice/cue acknowledgement, large choices, worker restart, durable checkpoints
  and supported save/rollback policy; compare the original Ren'Py application.
- **Platform/deployment:** visible Linux desktop, Windows x86_64 transport/build/
  package/shutdown, eventual macOS with viable backend; clean machine dependencies,
  distribution licenses and bounded process teardown.
- **Alternative embedding:** dedicated GIL/interpreter/ABI/thread/restart/failure/
  device suppression and packaging tests preserving all ownership/continuity gates.

These production gates do not all block Phase 2. **Next: Phase 2.1 — Collision /
Query Adapter.** Compare Jolt and Bullet before selecting a physics provider;
a minimal no-full-physics query baseline is optional. Test capsule sweep, raycasts,
static collision, slopes/steps, triggers, layers, contacts, ordered native events,
fixed-clock costs, integration/build footprint, exact licenses and Linux/Windows
viability. Keep world IDs/queries independent of GL and library pointers. Do not
implement that comparison during this ADR.

## Supersession Policy

Supersede through a new numbered ADR with concrete code/evidence, alternatives,
migration costs and regression gates. A transport/backend replacement that
preserves ownership can be recorded independently; it does not reopen native
authority automatically. Reopening runtime ownership requires evidence resolving
the Phase 1.1 limitations and an explicitly reviewed successor decision.
Never silently change this accepted record or relabel software evidence as hardware.
