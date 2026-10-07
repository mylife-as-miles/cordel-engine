# Phase 1.1 — real-time 3D viewport reference spike

Recorded 2026-10-07 UTC. **The software reference gate passes**: the actual
Ren'Py-derived runtime renders an isolated static 3D scene continuously, with
time-based camera controls and measured interaction behavior. Desktop devices,
hardware performance, oversized offscreen window growth and GPU buffer release
remain open. Phase 1.2 is the next comparison; the Phase 1.4 architecture decision
has not been made.

The project starts from CORDEL root `3334167d44069604396eb1523a0374d475fa2117`
on `cordel/phase1-viewport`, preserving fresh CORDEL history. Upstream runtime,
samples and licenses are unchanged. Code revision
`816abef1bb8ba0c49a12873c0b6bd1982056dd2b` contains the exercised source; the
[manifest](evidence/phase1_1/manifest.json) records individual source hashes,
commands and pinned SDK identity. Subsequent report/metadata edits are documentation.

## Implementation and source locations

The independently launchable project is [examples/cordel_viewport](../../examples/cordel_viewport/README.md).
It contains generated glTF, perspective/depth rendering, held fly controls,
uncaptured drag-look, focus clearing, resize-aware projection, a diagnostics HUD,
structured logs, a developer stall and home/viewport/narrator/home flow.
No renderer replacement, native host, physics, ECS, animation or production asset
pipeline is introduced.

| Source | Responsibility |
| --- | --- |
| [game/script.rpy](../../examples/cordel_viewport/game/script.rpy) | Isolated configuration; screens; home/entry/exit; F7 real narrator interaction |
| [math3d.py](../../examples/cordel_viewport/game/python-packages/cordel_spike/math3d.py) | Camera, vectors, view/projection matrices and named import conversion |
| [input_state.py](../../examples/cordel_viewport/game/python-packages/cordel_spike/input_state.py) | Held actions and continuous analog magnitude/deadzone |
| [timing.py](../../examples/cordel_viewport/game/python-packages/cordel_spike/timing.py) | Monotonic interval sampling, 50 ms clamp and bounded diagnostics |
| [runtime.py](../../examples/cordel_viewport/game/python-packages/cordel_spike/runtime.py) | `Viewport` displayable, `SessionModel` pin, Render adapter, input, HUD, lifecycle and JSON trace |
| [selftest.py](../../examples/cordel_viewport/game/python-packages/cordel_spike/selftest.py) | Actual-runtime experiments, synthetic event queue, pixel controls, Say wait and five cycles |
| [testcases.rpy](../../examples/cordel_viewport/game/testcases.rpy) | Normal demo UI flow, distinct from the lower-level experiment harness |
| [test_logic.py](../../examples/cordel_viewport/tests/test_logic.py) | 31 standalone camera/input/timing/conversion tests |
| [generate_scene.py](../../examples/cordel_viewport/tools/generate_scene.py) | Deterministic, standard-library-only fixture generation/check |
| [run_offscreen.sh](../../examples/cordel_viewport/tools/run_offscreen.sh), [check_baseline.sh](../../examples/cordel_viewport/tools/check_baseline.sh) | Repeatable verification using BOOTSTRAP's existing environment |

## Scene and coordinate convention

The CC0 fixture is authored numerically in the repository. Seven box meshes,
168 authored face vertices, 84 triangles, no downloaded content/textures. A shallow
ground box supports red/blue overlapping occluders, a 5 m gold column, green/cream
blocks and distant purple geometry. Code/generator are MIT; generated scene data
is CC0-1.0. See [provenance](../../examples/cordel_viewport/game/assets/LICENSE.md).
The shader reads Assimp's diffuse color with simple ambient/directional shading;
glTF material fields do not imply a PBR implementation.

World/camera: **right-handed**, **+Y up**, **-Z forward**, **+X right**, **metre/unit**.
Yaw/pitch are degrees; zero yaw looks -Z, positive yaw turns toward +X about -Y,
positive pitch looks up about local right. No roll; yaw wraps to [-180°,180°),
pitch clamps to ±85°. Camera quaternion state is unused. glTF quaternion order
is (x,y,z,w), but this translation-only scene does not verify quaternion import.

Flat matrices are row-major, acting on column vectors, `P * V * M * p`.
glTF matrix serialization is column-major; Assimp's row fields are copied into
Ren'Py's Matrix. In [assimp.pyx](../../renpy/gl2/assimp.pyx), `Loader.load` uses
the realtime-quality preset plus FlipUVs and FlipWindingOrder, and initial matrix
`S(-1,-1,-1) * Ry(180°) * S(zoom)`. At zoom 1 this is approximately
`C = diag(1,-1,1,1)`, a Y reflection. `load_node` accumulates `C * node`.

`ASSIMP_TO_WORLD` is C's inverse. `Viewport._scene` cancels the initial virtual
screen projection and this reflection at one boundary:

```text
Render.reverse = screen(logical width,height)^-1 * P(drawable aspect) * V * C^-1
effective geometry = screen * Render.reverse * C * node * p = P * V * node * p
```

Normals use the same named conversion in the shader. Perspective is OpenGL NDC
Z [-1,+1], vertical FOV 65°, near 0.05 m, far 100 m. Actual imported node origins
match the seven authored translations within 1e-5. Native Matrix probes request
`components=3`; the API defaults to two components. Pure camera/input code never
uses Ren'Py coordinates. This is a reference convention, not a new serialized ABI.

## Rendering, resize and depth evidence

`SessionModel` subclasses the existing `GLTFModel`; it retains that class's importer
and mesh-render construction. The adapter uses native Render view/projection flags,
mesh uniforms and the existing shader system. Shader dialect is explicitly
`glsl=100`, translated by the runtime; the active renderer reports GLSL 450 on this
host. Opaque materials are double-sided, so winding/cull behavior is not a claim
about a production material pipeline.

[gl2draw.pyx](../../renpy/gl2/gl2draw.pyx), `GL2DrawingContext.draw_one`, clears/enables
depth at the shared depth subtree and uses **GL_LEQUAL**. No live object sorting
is performed. The pixel probe renders both mesh submission orders and a control
with depth disabled at both Render levels:

| Probe | Centre RGBA |
| --- | --- |
| Depth enabled, original order | 153, 20, 14, 255 (near red) |
| Depth enabled, reversed order | 153, 20, 14, 255 (near red) |
| Depth disabled, original order | 7, 27, 85, 255 (far blue) |
| Depth disabled, reversed order | 80, 11, 7, 255 (red back face) |

The unchanged depth-enabled result plus the changed control is functional depth
evidence, independent of manually sorting geometry.

The example's `config.adjust_view_size` uses the full drawable. Projection uses
actual drawable aspect, while UI keeps virtual 1280×720 coordinates. Pixel probes
read a normal renderer draw through `screenshot(None)`, avoiding the ordinary
screenshot helper's uniform virtual scaling.

| Actual SDL window/drawable | Cream cube front-face pixels | Width/height |
| --- | --- | --- |
| 850×480 | 84×84 | 1.000 |
| 500×500 | 88×88 | 1.000 |
| 320×500 | 88×88 | 1.000 |
| 800×400 | 70×70 | 1.000 |

At every size: valid framebuffer clear, matching drawable/readback dimensions,
square silhouette, near-red depth ordering, and subsequent W movement pass.
The test resizes the actual SDL window through the existing Window wrapper.
Ren'Py's public resize helper clamps requested dimensions to its virtual-aspect
desktop bounds (922×519 here). Growing the offscreen drawable beyond its initial
size yielded invalid pixels; that scope is excluded from the passing table and
requires a desktop or separately provisioned surface test. No desktop growth,
high-DPI or physically presented frame claim is made.

## Input and narrative ownership

WASD fly/strafe; Space/Left Ctrl world up/down; Left Shift 3× speed. Camera speed
is 3 m/s, with combined motion bounded to that speed. Event down/up updates held
actions; simulation samples them once per update using elapsed delta. There are
no per-key-repeat position increments. The project's story skip bindings are
disabled because Ren'Py otherwise intercepts Left Ctrl and restarts the interaction.

Left click focuses the viewport; left-button dragging uses ordinary SDL event
motion deltas, with 0.15° per motion unit. Escape releases viewport focus, actions
and dragging. The implementation **does not capture the OS mouse**. It never
warps or enables relative mode; dragging stops at window boundaries. The current
wrapper couples grab/relative mode to cursor visibility and lacks an independent
relative-mode setter/status query. This fallback is explicit in the HUD and README.

Focus loss, mouse leave, minimize/hide, Ren'Py viewport unfocus, interaction changes,
dialogue ownership and close clear held/analog state. Synthetic focus-loss events
while W is down leave no held actions. The normal script clears input immediately
after its `ui.interact` returns; `per_interact` also clears it on restart. A new click
is needed after dialogue. Physical keyboard/mouse alt-tab behavior is still untested.

The raw axis adapter retains signed 16-bit magnitude with a radial 0.15 deadzone.
A synthetic value 16384 yields approximately 0.500 normalized input; disconnect
clears it. That adapter check bypasses Ren'Py's controller translation and is
labeled accordingly. The synthetic end-to-end dispatch records axes (0,0) and
viewport focus false: the normal controller layer posts digital pad events and
hides the cursor/removes hover focus. No physical controllers were present.
Controller support remains unresolved, not silently certified by the math test.

F7 produces a real synchronous narrator Say interaction, with the viewport still
shown. World redraws continue while input belongs to the line. The software
harness's line waits 0.75 s before a test-only timer dismisses it; normal runs wait
for the user. The normal UI test independently enters, opens/dismisses the line,
leaves and re-enters through the actual script/screens.

## Scheduling, diagnostics and measurements

`Viewport.render` samples `perf_counter`, advances the camera, builds a scene
Render tree, and calls `renpy.redraw(self, 1/60)`. This is interaction/redraw-driven
simulation on the engine thread. The maximum delta is **0.05 s**, excess wall time
is discarded, and no backlog/fixed tick/interpolation is implemented. Invalid,
zero and negative samples yield zero movement. F6 sleeps that thread for 0.25 s.

The HUD/log separates raw wall interval, clamped simulation delta, update fragment
and scene-tree preparation durations. `perf_counter` CPU-code durations include
possible descheduling; they are not exclusive thread CPU or GPU times. Preparation
excludes HUD construction, shader compilation in the draw phase, GPU execution
and the remaining engine loop. Frame interval includes scheduling/waiting.
`process_time` includes the entire process's threads, including Mesa software work.
There are no GPU timer queries or hardware benchmark claims.

JSON initialization logs version, renderer, drawable/logical size, GL version,
asset/mesh counts and coordinates. Periodic records are spaced two seconds apart.
Close records imports, updates, engine frame-counter delta, mean/worst durations,
process time and observable cache ownership. Per-motion logging exists only in
the software harness mode. Logs default to the example's ignored `tmp/viewport`.

### Headless/software results

Debian 13.6 x86_64, glibc 2.41. Pinned SDK Python 3.12.8; host `.venv` Python 3.12.14.
SDL offscreen + dummy audio + `LIBGL_ALWAYS_SOFTWARE=1`; gl2; Mesa 25.0.7 OpenGL
4.5 compatibility; **llvmpipe LLVM 19.1.7**. Scene: seven meshes, 84 triangles.

Final steady interval, after warm-up, without snapshot readback during measurement:

| Measurement | Observed value |
| --- | --- |
| Window/drawable | 850×480 |
| Sample wall duration | 3.00946 s |
| Viewport update/render samples | 131 |
| Rolling update FPS | 42.746 |
| Mean / worst raw update interval | 23.130 / 54.873 ms |
| Mean / worst idle update CPU fragment | 0.00164 / 0.17631 ms |
| Mean / worst scene preparation CPU fragment | 0.20723 / 0.59962 ms |
| Active camera fragment, separate input probes | 68 samples; mean 0.02514 ms, worst 0.12153 ms |
| Ren'Py drawn-frame counter delta | 1199 (cached trees may be drawn between camera updates) |
| Whole-process CPU time | 6.02682 s, approximately 2.00 CPU-core equivalents over the wall interval |
| Discarded simulation time in steady sample | 0.01873 s |

The engine counter is counted draws, not monitor refresh or GPU timing. A 60 Hz
redraw request yielded about 43 updates/s here, with many cached draws between
updates. The software renderer and shared host make these numbers functional
instrumentation, not performance targets for hardware or AAA content.

Forced stall: raw interval **259.337 ms**, delta **50.000 ms**, **209.337 ms**
discarded; W at normal speed moved **0.150 m** in that update. Later intervals
returned below the clamp. Pure timing tests verify no accumulated catch-up debt.
Say wait: **33 viewport updates in 0.78070 s**; last interval 18.918 ms; actions
remained empty while input was disabled; updates continued after return.

### Hardware/interactive results

None. No visible desktop or physical controller was available. Mouse drag/escape,
keyboard focus, surface growth, DPI, hardware rasterization/presentation, audible
audio and GPU lifetime/performance require subsequent device verification. The
checked-in PNGs were actually rendered offscreen; they are not desktop screenshots.

## Lifecycle evidence

The native experiment enters/leaves **five sessions**. Every session imports the
asset once. After close plus a blank interaction: zero active sessions, importer
cache/prediction entries and viewport redraw requests; no additional viewport
updates; input cleared; ModelData weak reference gone. Shutdown callback count
stays one. A deliberate importer-cache eviction retains the identical session pin.

Observed global texture-cache `(bytes,count)` after the five exits:
`(3928232,13)`, `(1634472,3)`, `(1863848,4)`, `(1634472,3)`, `(1863848,4)`.
The first cycle includes diagnostic/Say/readback caches; subsequent counts are
bounded in this short run. This is not a long soak or total GPU residency proof.
The pinned SDK's mesh objects do not support Python weakrefs. Mesh buffers can
outlive importer data via Render caches, with deferred native deletion. No claim
that GPU buffers were freed is inferred from Python reference disappearance.

## Executed checks and reproducible commands

From `/workspace/cordel-engine`, after BOOTSTRAP setup:

```sh
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/phase1-accepted
bash examples/cordel_viewport/tools/check_baseline.sh tmp/phase1-accepted
```

Both exited **0**. Use a new directory when repeating the first command. Exact
expanded commands/selections are in those short scripts. The first executes the
deterministic fixture check, 31 Python logic tests, actual native-backed viewport
experiments and the normal UI test. The second replays BOOTSTRAP's focused checks
and lints the new project, using the same SDK and host-stdlib runner workaround.

| Check | Result |
| --- | --- |
| Generator `--check` | Fixture matches exactly |
| Camera/input/timing/conversion unit suite | 31 passed |
| Native-backed software experiment | 83 passing check records, including repeated resize/cycle checks |
| Normal viewport UI testcase | 1 passed; 8 assertions and 2 hooks passed |
| Original source-only unit selection | 29 passed |
| Original SDK-backed unit selection | 89 passed |
| Original `the_question` bad_end/good_end | 2 passed, 1 unselected/skipped; 2 assertions, 3 hooks passed |
| New project lint | Exit 0; only generic conflicting-properties advice |

The two baseline unit selections overlap. Known full-suite audio/SDK and history
click failures from BOOTSTRAP remain outside these selections and unchanged.
Native source compilation remains blocked by the original missing development
dependencies; no system packages or giant dependencies were introduced.

[Archived report](evidence/phase1_1/report.json), [trace](evidence/phase1_1/trace.jsonl),
[test transcripts](evidence/phase1_1/ui-tests.txt), and
[source manifest](evidence/phase1_1/manifest.json) make these claims inspectable.
The evidence directory also retains baseline transcripts, a HUD frame, portrait/
square frames and enabled/disabled depth controls.

## Problems discovered

1. **Scheduling ownership.** Simulation in `render` follows invalidation rather
   than each engine draw; default fast-redraw policy submits cached frames.
   There is no independent fixed world tick. Main-thread stalls block UI/render
   servicing; clamping avoids a camera jump by deliberately losing simulation time.
2. **Asset lifetime ownership.** An initial active ModelData weak reference expired
   during a repeated-cycle trial. Importer cache residency is prediction-owned.
   The isolated pin prevents re-importing; production assets need deliberate
   residency and renderer lifetime/fence accounting independent of story prediction.
3. **Input ownership.** Story skip intercepted LCtrl; Function actions restart
   interactions by default, clearing queued mouse motion; controller translation
   removes hover focus. The harness's injection Function uses `_update_screens=False`
   so it does not erase its own motion. These interactions are measurable coupling,
   not an analog controller solution or a production gameplay event loop.
4. **Mouse-mode coupling.** Grab and visibility influence relative mode, without
   independent exposed control/query. Drag-look is useful but bounded and unverified
   on devices. A future platform/input owner needs a reliable capture contract.
5. **Virtual-space assumptions.** Default letterbox/resize bounds and uniform
   screenshot scaling assume virtual aspect. World projection must use drawable
   aspect explicitly; UI can still distort. Oversized offscreen readbacks produced
   invalid pixels, leaving window growth/device recovery unverified on this host.
6. **Internal adapter dependence.** Render matrix flags, Assimp cache/prediction
   sets and native module details are internal APIs, not a stable rendering plugin
   contract. The projection constant is not Python-exported by the SDK; its source
   value 2 is isolated in the adapter. Pure Python modules use `renpy.exports` for
   script API functions. Shader dialect must be declared explicitly.
7. **Scale/profiling limits.** Static models have no world visibility culling in
   this path; all seven meshes are submitted. An 84-triangle test says little about
   animation/world scale. CPU fragments do not measure GPU time, full frame cost,
   resident buffers, device loss, streaming or a production asset pipeline.

[Exploratory failure records](evidence/phase1_1/exploratory-failures.json) retain
the relevant failed trials before adapter/harness corrections. They are distinct
from the final passing evidence. The coordinate probe's initial two-component
mistake was corrected; it is not reported as an importer Z-conversion defect.

## Recommendation and next task

The existing runtime is sufficient for this small continuous static viewport and
an ordinary narrative wait. Further production expansion inside this adapter would
depend increasingly on display-cache, prediction and UI/input internals. Proceed
to **Phase 1.2 — Native Host Comparison Spike**, while keeping this working
reference for identical scene/control/trace comparisons.

Exact next task: build one isolated desktop native host with an explicitly
experimental backend, load the same CC0 scene, use the documented camera convention,
implement a bounded fixed update plus presentation interpolation, and reproduce
input/focus, resize, forced stall, five enter/exit cycles and separately labeled
CPU/render diagnostics. Record its build/license/dependency footprint and compare
under the same environment; add real desktop/hardware evidence when available.
Select only one comparison backend for that spike, without committing CORDEL's
production API, physics, ECS or editor. Phase 1.2 has not been started. Narrative
ownership and the final runtime ADR remain Phase 1.3/1.4 work.
