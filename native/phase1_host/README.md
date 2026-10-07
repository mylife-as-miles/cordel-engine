# CORDEL ENGINE — 0.1.0-dev: Phase 1.2 native host

An isolated C++20 / SDL3 / OpenGL core comparison experiment. CORDEL owns events,
held input, the fixed simulation clock, interpolation, draw submission and GL
object destruction. No Python or Ren'Py code runs in the executable's frame loop.
The unchanged [Phase 1.1 CC0 fixture](../../examples/cordel_viewport/game/assets/cordel_scene.gltf)
is loaded directly: seven meshes, 168 vertices, 84 triangles. This is not the
production renderer or a narrative integration.

## Build and run

CMake >=3.24, C/C++20 compiler and a build tool are needed. This host uses GCC 14.2,
Make and project-local CMake 3.31.6 installed into BOOTSTRAP's existing `.venv`:

```sh
uv pip install --python .venv/bin/python cmake==3.31.6
export PATH="$PWD/.venv/bin:$PATH"
cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON
cmake --build build/phase1-host -j 4
ctest --test-dir build/phase1-host --output-on-failure -V
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  build/phase1-host/cordel-native-host --self-test --output tmp/my-native-test
```

One repeatable software gate, including PNG conversion:

```sh
bash native/phase1_host/tools/run_offscreen.sh tmp/my-native-gate
```

Use a new output directory. The wrapper adds CMake if absent, configures/builds,
runs two CTest targets, and runs the graphical self-test under a 30-second timeout.
Python is test/build/image-conversion tooling only. Native logic tests execute
without a graphics context. `BUILD_TESTING=OFF` removes the configure-time Python
requirement; the host's compiled self-test remains available without Python.

SDL 3.4.8 is fetched from a commit URL plus archive SHA-256 into `build/_deps`.
The default CMake path keeps desktop drivers eligible; on this minimal Linux host,
`CORDEL_OFFSCREEN_ONLY=ON` disables X11/Wayland/KMSDRM and permits a console build.
It enables SDL's OpenGL feature using bundled declarations because system GL
headers are absent. SDL's bundled EGL declarations plus runtime loading suffice.
This flag does **not** produce a visible desktop build. On a provisioned desktop,
use a **separate** build directory without the flag:

```sh
cmake -S native/phase1_host -B build/phase1-host-desktop
cmake --build build/phase1-host-desktop -j 4
build/phase1-host-desktop/cordel-native-host
```

That desktop flow, other operating systems and physical devices are unverified.
No root or host-wide installation is performed. SDL audio/render/GPU/Vulkan paths
are disabled. See [dependency manifest](third_party/dependency-manifest.json) and
the retained SDL/HIDAPI/cgltf license texts. CORDEL code is MIT under [LICENSE](LICENSE).
The fixture remains CC0; upstream Ren'Py notices remain untouched.

## Controls and diagnostic modes

| Control/option | Behavior |
| --- | --- |
| W/S, A/D | Forward/backward fly, left/right strafe |
| Space / Left Ctrl | World up/down |
| Left Shift | 3× sprint; normal speed 3 m/s |
| Left click | Deliberately request SDL window relative mouse mode |
| Relative mouse motion | Yaw/pitch at 0.15° per event unit; pitch ±85° |
| Escape | Disable relative mode and clear held/analog/pending mouse input |
| F6 | Approximately 250 ms artificial main-thread stall |
| Close window | Clear controls, unload scene, destroy context/window and SDL |
| `--seconds 3` | Timed controlled run (~60 rendered frames/s) |
| `--uncapped --seconds 3` | Timed uncapped render run; same 60 Hz simulation |
| `--output DIR` | JSONL trace and measurement/probe output |
| `--scene FILE` | Override the existing fixture path |
| `--self-test` | Required assertions, five scene cycles and full host recreation |

Window focus loss, hiding and minimization clear held state immediately. Regaining
focus does not restore old keys or mouse capture. Native gamepad events retain raw
left-stick magnitude, use a radial 0.15 deadzone, and clear on device removal.
There is no digital conversion of analog axes. Physical controllers are untested.

`mouse_captured` in logs is the **SDL window relative-mode request flag** returned
by SDL's public getter. An offscreen window has no real keyboard focus: flag and
synthetic relative-event tests do not prove OS-level capture or physical unbounded
movement. Native desktop input uses SDL relative mode directly, with no drag-look
or pointer-warp imitation. Physical capture/release/alt-tab remains a device gate.

## Math, scheduling and rendering

RH, +Y up, +X right, -Z forward, metres. Yaw zero is -Z, positive yaw turns toward
+X about -Y; positive pitch looks up. Degrees, no roll/quaternion camera state,
pitch ±85°. glTF quaternion order is (x,y,z,w), but this loader explicitly rejects
rotation/scale/matrix/skin/hierarchy content: it is a translation-only fixture
loader. Matrix storage is row-major with column vectors (`P * V * M * p`). cgltf
node matrices arrive column-major and transpose once at import. There is no axis
reflection; Ren'Py's Assimp conversion is not copied. GL upload converts storage
to column-major once. Vertical FOV 65°, near .05 m, far 100 m, NDC Z [-1,+1].

Shared [goldens.json](tests/goldens.json) checks Python and C++ vectors, view and
perspective, angle wrapping/clamp, all movement axes/sprint/diagonal and node
translations. `tools/goldens.py --check` validates Python and the corresponding
compiled native header; regenerate deliberately after a convention change.

`FrameRunner::next` owns event polling → input snapshot → monotonic accumulator
→ at most three 1/60 s ticks → interpolated camera → GL render → completion → swap.
The renderer does not update simulation. Camera previous/current snapshots are
linearly interpolated, with shortest-arc yaw. This deliberately introduces up to
one tick of presentation latency. Mouse deltas accumulate until a tick and are
consumed once across catch-up ticks. After a stall, excess **whole ticks** are
dropped; a sub-tick remainder is retained for alpha. Authoritative W motion is at
most .150 m per stalled frame; rendered interpolation may show less immediately.

Controlled mode first tests swap pacing; otherwise it sleeps to a monotonic
deadline. Uncapped mode removes those sleeps. Every frame uses `glFinish` as
**diagnostic backpressure**: SDL offscreen swap does not reliably drain queued GL
work. This bounds outstanding work without building a production fence scheduler.
The completion wait is separately measured CPU wall time, not GPU profiling.
Changing this policy changes the throughput comparison; see the failed unbounded
trial in the report. Simulation still shares a thread with GL and can be blocked
by a driver; fixed clock ownership does not guarantee hard real-time execution.

Minimal GLSL 330 normal/color lighting matches Phase 1.1. Depth uses GL_LEQUAL,
culling/blending/sRGB conversion are disabled for the opaque double-sided fixture.
Drawable dimensions drive `glViewport` and projection. SDL 3.4.8 offscreen resizing
only changes metadata, so `Platform::resize` recreates that window/EGL surface
while retaining its context/resources. Desktop uses ordinary SDL resize. Window
and drawable sizes are logged separately; high-DPI behavior is untested.

GL names are noncopyable RAII owners with live/create/delete counters. Each scene
owns one linked program, seven VAOs, seven VBOs, seven EBOs and CPU mesh data.
Temporary shader stages are detached/deleted after linking. No textures or shared
host-global GL objects are created. Unload unbinds objects and destroys all scene
owners before context shutdown. Counters prove CORDEL deletion calls/ownership,
not physical driver memory reclamation.

Logs supply diagnostics instead of a font/UI dependency. The title shows identity,
FPS and exit/stall controls; JSONL provides camera, frame/tick/alpha, dimensions,
input, CPU fragments and resources. GPU timing is **unavailable**. CPU fragments
are monotonic durations of CPU code and may include preemption/driver waiting.

See [Phase 1.2 report](../../docs/cordel/PHASE_1_2_NATIVE_HOST.md) for measured
software results, baseline replay, direct comparison and remaining limits.
