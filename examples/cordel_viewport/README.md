# CORDEL ENGINE — 0.1.0-dev: Phase 1.1 viewport reference spike

A standalone Ren'Py project with a continuously redrawing static glTF scene,
perspective fly camera, held actions, drag-look, depth testing, resize-aware
projection and CPU diagnostics. This is a disposable runtime experiment.

## Run

Configure the checkout's `.venv` and pinned nightly native SDK using
[BOOTSTRAP](../../docs/cordel/BOOTSTRAP.md). No additional dependencies are needed.
Run from the repository root, using the checkout entry point explicitly:

```sh
lib/py3-linux-x86_64/python -X utf8 renpy.py examples/cordel_viewport
```

Choose **Enter viewport**. F9 returns to the home screen; enter again to exercise
lifecycle. F7 opens one ordinary Ren'Py narrator line over the running viewport.
Click or Enter dismisses it. Click the viewport again to regain input ownership.
Save/load and rollback are outside this experiment; rollback and story skipping
are disabled in this project's configuration. Existing samples are unaffected.

| Control | Behavior |
| --- | --- |
| Left click | Focus the viewport |
| Hold left button and move mouse | Drag-look: yaw right with rightward motion; pitch up with upward motion |
| W / S | Fly forward / backward along camera forward |
| A / D | Strafe left / right, without roll |
| Space / Left Ctrl | World up / down |
| Left Shift | 3× speed (normal speed 3 metres/second) |
| Escape | Clear actions and release viewport focus/dragging |
| F6 | Developer-only 250 ms main-thread stall |
| F7 / F9 | Dialogue probe / leave viewport |

**Mouse policy:** uncaptured drag-look, bounded by the window. No pointer warp,
OS grab or SDL relative mode is enabled. The wrapper exposes grab and `get_rel`,
but visibility changes also alter relative mode and it has no independent
relative-mode setter/status query. Event `rel` during dragging is ordinary cursor
motion, not unbounded captured input. The HUD explicitly says **uncaptured**.
Desktop cursor/device behavior still needs physical verification.

Movement integrates a monotonic elapsed delta, capped at **50 ms**. Excess time
is discarded, without accumulating catch-up ticks. Combined fly/strafe/vertical
movement is bounded to normal speed; sprint multiplies it. Pitch is clamped to
±85°. Application focus loss, mouse leave, minimize/hide, viewport unfocus,
interaction changes, dialogue ownership and shutdown clear held state.
Keyboard repeat never advances the camera directly.

Raw left-stick signed 16-bit axis values have a separate analog state and radial
deadzone (0.15); sub-unit magnitude is retained. The raw adapter was checked with
synthetic data. Ren'Py's existing pad translation can remove viewport hover focus,
so end-to-end controller ownership remains unresolved; no physical device was
available. Controller support is not an acceptance claim.

## Coordinates and rendering boundary

The authored scene and camera share **right-handed space**, **+Y up**, **-Z
forward**, **+X right**, **1 unit = 1 metre**. Camera angles are degrees. Yaw zero
looks -Z; positive yaw turns toward +X (rotation about world **-Y**). Positive
pitch looks up, rotating about camera right. There is no roll. Yaw wraps to
[-180°, 180°); pitch clamps independently. Camera code uses yaw/pitch vectors,
not a quaternion. glTF defines quaternion order **(x, y, z, w)**, but the generated
nodes use translation only; quaternion import has not been experimentally tested.

Matrices are stored row-major and applied to column vectors. `A * B` applies B
first; the clip transform is `P * V * node * position`. glTF serialized matrices
are column-major; Assimp exposes `aiMatrix4x4` rows which Ren'Py reads as rows.
The camera constructs a conventional OpenGL perspective frustum, vertical FOV
65°, near 0.05 m, far 100 m, clip NDC Z from -1 to +1.

The inspected importer (`renpy/gl2/assimp.pyx`, `Loader.load`/`load_node`) uses
Assimp's realtime-quality preset, **FlipUVs** and **FlipWindingOrder**, then applies:

```text
C = scale(-1,-1,-1) * rotateY(180°) * scale(zoom)
zoom = 1: C ≈ diag(1,-1,1,1), so imported Y points down
imported node = C * authored node
```

`math3d.ASSIMP_TO_WORLD` is that reflection's inverse. `Viewport._scene` is the
single spatial conversion boundary. Its projection-tagged Render has
`reverse = screen_projection(logical_size)^-1 * P(drawable_aspect) * V * C^-1`.
Ren'Py supplies the initial screen projection and the imported child supplies
`C * node`, leaving `P * V * node`. The shader also uses this named conversion
for imported normals. Native imported node origins and screenshot occlusion are
checked by the software harness. No conversion is scattered through input or
movement code.

The live scene uses `GLTFModel` meshes and one small ambient/directional color
shader, with opaque double-sided materials. A shared Render depth group invokes
Ren'Py's GL depth clear/test (`LEQUAL`); there is no object depth sorting. The
fixture deliberately submits the near red object before its blue occludee.
Tests reverse submission and disable depth as a control, using actual pixels.

`config.adjust_view_size` fills the drawable instead of preserving the virtual
16:9 letterbox. P uses drawable aspect; UI remains in Ren'Py's virtual 1280×720
coordinates and may scale nonuniformly. World projection is verified at multiple
aspects. Production UI layout and DPI behavior need later work.

## Scene and ownership

`tools/generate_scene.py` deterministically generates `game/assets/cordel_scene.gltf`:
seven box meshes, 168 authored face vertices, 84 triangles, no external textures.
There is a shallow ground box, near red/far blue occluders, a 5 m gold column,
green and cream blocks and distant purple geometry. Scene data is **CC0-1.0**;
see [asset provenance](game/assets/LICENSE.md). Prototype code is MIT under
[LICENSE](LICENSE). Upstream engine/dependency licenses remain applicable.

`SessionModel` subclasses GLTFModel solely to pin ModelData until viewport close.
Ren'Py's prediction-owned importer cache otherwise can evict a live displayable's
data and cause another import. Close removes only this model's cache/prediction
ownership and releases the pin. Subsequent inactive renders do not schedule
updates. Five cycles verify one import/session, no active sessions or pending
viewport redraw requests, cleared actions and bounded texture-cache counts.
Mesh weakrefs are unavailable in the pinned SDK; GPU buffer deletion/residency
has **not** been proven. Texture cache counters include engine/UI caching.

## Scheduling and evidence

`Viewport.render` measures elapsed wall time, updates the fly camera, prepares the
scene tree, then requests `renpy.redraw(self, 1/60)`. It runs during ordinary
interactions and Say waits, on the engine thread. This couples simulation to
render-cache invalidation. A 60 Hz request does not guarantee 60 updates/second.
Ren'Py can draw cached trees several times between camera updates.

The HUD shows update FPS, raw interval, clamped delta, camera/angles, drawable,
renderer, mouse policy, update CPU and tree-preparation CPU durations. These
durations measure host code with `perf_counter`, including possible preemption;
frame interval is wall time. Preparation excludes GPU execution, shader compile
during draw, HUD preparation and the remainder of Ren'Py's drawing loop.
There are no GPU queries. Two-second JSON diagnostics, initialization and teardown
records go to `tmp/viewport/trace.jsonl` by default. Override the destination with
`CORDEL_VIEWPORT_OUTPUT`; shutdown records counters and observable ownership.

From the repository root:

```sh
.venv/bin/python examples/cordel_viewport/tools/generate_scene.py --check
.venv/bin/python -m unittest discover -s examples/cordel_viewport/tests -v
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/my-viewport-run
bash examples/cordel_viewport/tools/check_baseline.sh tmp/my-viewport-run
```

Use a new output directory for each run. The shell script sets
`SDL_VIDEODRIVER=offscreen`, `SDL_AUDIODRIVER=dummy`, `LIBGL_ALWAYS_SOFTWARE=1`,
runs the Python logic tests, real runtime experiments, then the actual home/
viewport/dialogue UI test. It writes `report.json`, `trace.jsonl`, test transcripts
and PNG pixel probes. The second script replays BOOTSTRAP's original focused
baseline commands and lints the new project; it does not certify the known failing
full suites.

The software harness generates events through the real event queue after
interaction initialization; it does not simulate physical devices. Its narrator
line advances after 0.75 s only with `CORDEL_VIEWPORT_SELFTEST=1`. The normal
project waits for user input. Tests use smaller window sizes within the initial
offscreen drawable because larger readbacks returned invalid pixels on this
SDL offscreen host. This limitation is recorded rather than treated as verified
desktop resize growth. Hardware/desktop execution remains pending.

See [the Phase 1.1 report](../../docs/cordel/PHASE_1_1_VIEWPORT.md) for actual
measurements, failures discovered and the Phase 1.2 comparison recommendation.
