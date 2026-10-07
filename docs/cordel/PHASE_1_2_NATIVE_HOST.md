# Phase 1.2 — Native Host Comparison Spike

Recorded 2026-10-07 UTC. **The software/offscreen comparison gate passes.** An
isolated native host owns its event loop, fixed simulation clock, interpolation,
input state and graphics object lifetimes while rendering the same Phase 1.1
fixture. Ren'Py remains intact. This is evidence for the next narrative experiment,
not the Phase 1.4 production architecture/backend decision.

## Git acceptance and scope

Before modification, local/fetched `main` both matched
`3334167d44069604396eb1523a0374d475fa2117`; local/fetched `cordel/phase1-viewport`
both matched accepted `74ff4155f6a831cf86b26823743ea7fa869ddb09`. The tree was clean.
`main` was fast-forwarded to the accepted branch and pushed to `origin`; no squash,
new merge commit, ancestry restoration or history rewriting occurred.
`cordel/phase1-native-host` was created there. Phase 1.2 is published on that feature
branch and is **not automatically merged into main**.

Implementation commits: `589106b` (logic/goldens/fixture loader), `948ccf1` (native
host and probes), `cc8f0b9` (repeatable tools). Subsequent edits document evidence.
The [manifest](evidence/phase1_2/manifest.json) records full revisions, source and
artifact hashes, exact commands and the accepted base. Original Ren'Py code,
Phase 1.1 code/assets/tests, existing samples and original notices are unchanged.

## Implementation and source locations

| Location | Responsibility |
| --- | --- |
| [native/phase1_host/CMakeLists.txt](../../native/phase1_host/CMakeLists.txt) | C++20, pinned SDL FetchContent, static SDL, native and shared-golden CTest targets |
| [logic.hpp](../../native/phase1_host/include/cordel/logic.hpp) | Camera/vector/matrix math, held actions, analog deadzone, fixed accumulator, interpolation, ownership counters |
| [scene.cpp](../../native/phase1_host/src/scene/scene.cpp) | cgltf parsing/validation, explicit supported fixture subset and one import boundary |
| [platform.cpp](../../native/phase1_host/src/platform/platform.cpp) | SDL window/context, keyboard/mouse/gamepad queue, focus clearing, resize surface policy |
| [runtime.cpp](../../native/phase1_host/src/runtime.cpp) | `FrameRunner::next`, shared by normal execution and self-test; simulation before rendering |
| [renderer.cpp](../../native/phase1_host/src/render/renderer.cpp) | Minimal GL loader, GLSL 330 shader, deterministic GL owners, depth, drawable projection/readback |
| [diagnostics.cpp](../../native/phase1_host/src/diagnostics/diagnostics.cpp), [main.cpp](../../native/phase1_host/src/main.cpp) | JSONL/statistics, paced/uncapped CLI, ordered host teardown/recreation |
| [self_test.cpp](../../native/phase1_host/src/self_test.cpp) | Actual context, pixel, resize, queued input/stall and five-cycle assertions |
| [tests](../../native/phase1_host/tests/logic_suite.hpp) | 304 context-independent assertions; common machine-readable golden vectors |
| [tools](../../native/phase1_host/tools/run_offscreen.sh) | Repeatable native gate, PNG conversion and direct report comparison |

No Python, Ren'Py, IPC, narrative runtime, physics, character controller, ECS,
animation, PBR, audio, editor or new production backend abstraction was added.
`ldd` lists only system C/C++/math libraries; GL/EGL load dynamically through SDL.
Python is used for build/test/golden/image/comparison tooling, outside the host.

## Backend, environment and dependencies

Debian 13.6 x86_64; GCC/G++ 14.2.0, CMake 3.31.6, Release (`-O3 -DNDEBUG`), C++20.
SDL offscreen and `LIBGL_ALWAYS_SOFTWARE=1`, **llvmpipe LLVM 19.1.7**,
Mesa 25.0.7-2+deb13u1. Requested OpenGL 3.3 core; obtained **4.5 core**, GLSL 330
source and a 24-bit default framebuffer depth attachment. No GPU timing queries.

| Dependency | Pin/license | Role |
| --- | --- | --- |
| SDL 3.4.8 | `d9d5536704d585616d4db3c8ba3c4ff6fc2757e1`; Zlib | Statically linked platform/input; fetched into ignored build tree |
| Embedded HIDAPI | Same SDL source pin, `src/hidapi`; BSD-style alternative | SDL HID/gamepad path; retained binary-notice text |
| cgltf 1.15 | `360db1a95480fe102ae9c69b27c5d101167ff5ba`; MIT | Unmodified single header compiled into fixture loader |
| CMake 3.31.6 | Project-local Python wheel; BSD-3-Clause / packaging Apache-2.0 | Build tool only, absent from executable |

Archive/header SHA-256, URLs, purpose/linkage and notices are in the
[dependency manifest](../../native/phase1_host/third_party/dependency-manifest.json).
The loaded Ren'Py SDK's `SDL_GetVersion()` returned **3004008**, so the native pin
matches its 3.4.8 release number. SDK build options/patch provenance are still
distinct from stock native SDL; equal version numbers do not prove identical builds.
An earlier development trial used 3.2.20 and is labeled separately.

The host lacked CMake and SDL/GL development headers. CMake was installed into
the existing `.venv`; no privileged or host-wide package changes. SDL's bundled
GL/EGL declarations plus dynamic loading support this minimal build. Its console
option avoids refusing a build without X11/Wayland development packages. SDL
audio, SDL renderers, SDL GPU and Vulkan are disabled. This is an offscreen-only
build; a desktop build needs an independently configured directory/development host.

## Scene, coordinates and rendering

The loader consumes the original
[cordel_scene.gltf](../../examples/cordel_viewport/game/assets/cordel_scene.gltf)
directly. No duplicate scene is maintained. Seven meshes, 168 face vertices,
**84 triangles**, CC0 numerical fixture: ground, near red/far blue overlapping
boxes, a 5 m gold column, green/cream blocks and far purple box. Code is MIT.
Solid material color and ambient/directional illumination match Phase 1.1's
equation; there are no textures, PBR or shadows. All meshes are submitted.

Convention: **RH, +Y up, +X right, -Z forward, one metre/unit**. Yaw/pitch degrees;
yaw 0 looks -Z, positive yaw turns toward +X about -Y, positive pitch looks up;
pitch ±85°, wrapped yaw, no roll. Camera has no quaternion state; glTF ordering
is (x,y,z,w), but rotations/scales/matrix nodes/skins/hierarchies are explicitly
rejected by this fixture-only loader. They are not silently unsupported features.

CPU matrices are row-major acting on column vectors (`P * V * M * p`). cgltf
returns column-major node storage, transposed once at import; coordinates already
match CORDEL. **No Assimp reflection** is copied. Matrix upload converts storage
to GL column-major once. Vertical FOV 65°, near .05 m, far 100 m, NDC Z [-1,+1].
All seven imported node origins agree with the authored golden translations within
1e-6. Shared JSON and its generated native header cover six angle/view/projection
cases, nine movement cases and seven node origins; Python validates against the
unchanged Phase 1.1 helpers and C++ validates the compiled values.

Real API depth uses `GL_DEPTH_TEST` / `GL_LEQUAL`, with no mesh sorting. Numeric
readback at 850×480 (and all resize sizes) matches Phase 1.1:

| Submission control | Centre RGBA |
| --- | --- |
| Depth on, normal / reversed order | **153,20,14,255** in both: near red |
| Depth off, normal order | **7,27,85,255**: far blue |
| Depth off, reversed order | **80,11,7,255**: red back face |

Depth-on order invariance and changed depth-off controls prove raster depth
ordering. Core framebuffer attachment queries report 24 depth bits; a legacy
`GL_DEPTH_BITS` query failed during development and was replaced.

## Native loop, timing, interpolation and pacing

`FrameRunner::next` executes SDL polling → input state snapshot → monotonic
accumulator → fixed simulation ticks → interpolated camera → render preparation
→ GL submission → completion wait → swap. Simulation never runs inside Renderer.
One thread owns events/simulation/GL; this is not a separate simulation thread.

Fixed tick **1/60 s**, maximum **three ticks per rendered frame**. Excess whole
backlog ticks are dropped and measured; a sub-tick remainder is retained for alpha.
Invalid/nonpositive time does not advance simulation. Previous/current camera
states interpolate position and pitch linearly, yaw along the shortest wrapped
arc. Rendering therefore lags authoritative state by up to one fixed tick; it
does not extrapolate. Pending relative motion is consumed once when a tick occurs,
not once for every catch-up tick, and is cleared on focus loss.

F6 dispatch uses the same native loop as normal execution and sleeps approximately
250 ms. Final probe: **250.143 ms raw**, **3 ticks / 50.000 ms simulation**,
**200.000 ms dropped**, alpha **0.008562**, W displacement **0.150 m**. The remaining
0.143 ms is carried as interpolation remainder. Recovery executed bounded ticks
with zero dropped debt. This differs slightly from Ren'Py's discard-everything-
above-50-ms clamp while matching its authoritative movement bound.

Controlled mode attempts VSync and measures swap blocking during warm-up. Here
it selected **monotonic sleep fallback** (~60 render frames/s), without busy-spin.
Uncapped mode removes pacing sleeps; fixed ticking remains independent of render
frequency. GPU timing is **unavailable**. Logs label CPU-code wall fragments,
which include possible preemption/driver waiting, separately from process CPU.

Offscreen EGL swap did not sufficiently bound queued GL work in an uncapped
development trial. Its final submission blocked the single-threaded world loop,
and peak RSS reached **1,564,700 KiB**. That trial remains in
[exploratory findings](evidence/phase1_2/exploratory-findings.json), not the passing
throughput measurements. The final diagnostic path calls **glFinish per frame**,
separately timing its CPU wait. It bounds outstanding work and makes completed
frame measurements repeatable; it is not a production fence/in-flight scheduler
or GPU timer. Native ownership alone does not solve driver stalls.

## Input evidence

WASD, Space, Left Ctrl and Left Shift drive held native actions. Queued down/up
events, all axes, 9 m/s sprint and W-down → focus-loss clearing pass through SDL's
actual event queue and the same native dispatch/tick path. Repeats do not move
the camera directly. Hiding/minimizing/focus loss clears held/analog/pending motion;
Escape disables relative mode and clears controls. Focus gain does not restore
old held keys or automatically recapture the cursor.

Left click uses SDL's independent relative-window-mode setter/getter. Its request
flag is true in the software probe; synthetic (100,-40) relative motion gives
yaw 15°, pitch 6°, and Escape releases the flag. **The offscreen SDL window has
no real keyboard focus.** The public getter reports a request flag, not proof
that a physical mouse is producing unbounded relative motion. That semantics is
explicit in the trace. No warp or drag fallback is represented as relative mode.
Desktop capture/release/alt-tab remains unverified.

Gamepad hotplug/raw left-stick events retain magnitude, normalize signed 16-bit
values and use the same radial .15 deadzone as Phase 1.1. Synthetic unit tests
verify sub-unit magnitude and diagonal bounds; device removal clears axes. Zero
physical gamepads were present. No physical controller/driver behavior is certified.

## Resize and resource lifetime

Window size and drawable size are recorded separately. Drawable size drives
`glViewport` and projection aspect. This host reports a 1:1 scale:

| Window / drawable | Cream front-face width × height |
| --- | --- |
| 850×480 | 84×84 px |
| 500×500 | 88×88 px |
| 320×500 | 88×88 px |
| 1280×720 | 126×126 px |

Every size passes square proportion, valid top-left background, four depth
controls and subsequent W simulation. The larger size has valid pixels here.
SDL 3.4.8's offscreen `OFFSCREEN_SetWindowSize` only updates metadata, so the
native resize test recreates its window/EGL pbuffer and rebinds the existing GL
context, preserving scene resources. This is an explicit surface-lifetime
workaround, **not** proof that ordinary offscreen SDL resize grows a pbuffer.
Desktop resize uses SDL's normal path and remains untested; high-DPI is untested.

Each loaded scene owns **one program, seven VAOs, seven VBOs, seven EBOs and one
scene object**. Temporary shader stages detach/delete after linking, no textures
exist, and no host-global GL objects remain outside a scene. Noncopyable/movable
RAII owners issue real GL deletion calls, with separate live/create/delete counts.

**Five cycles** load, render/move through six frames, release input and unload.
At every unload: all live counts zero, create/delete totals balanced, held/analog
state empty and mouse request released. Renderer is destroyed before its SDL
context/window. An optional extra test fully destroys renderer/context/window/SDL,
then creates a second host, loads/renders/unloads and destroys it. Final live
counts are all zero. This proves CORDEL ownership/deletion calls, not physical
driver-memory reclamation or a long memory soak.

## Measurements and direct comparison

All measurements are **software/offscreen**, same machine, same tiny scene and
850×480 drawable, default camera (0,2.5,6), yaw 0/pitch -8, same lighting equation.
Native samples exclude shader/context warm-up. Native emits periodic JSONL at
0.5 s intervals; no per-mouse-event flood. Ren'Py is completely unchanged.

| Metric | Phase 1.1 accepted | Phase 1.1 replay 1 / 2 | Native controlled | Native uncapped |
| --- | --- | --- | --- | --- |
| Sample wall seconds | 3.009 | 3.007 / 3.007 | 3.000 | 3.007 |
| Updates or completed render iterations | 131 updates | 127 / 124 updates | 180 frames | 3525 frames |
| Reported rate | 42.746 update/s | 41.957 / 40.592 update/s | 59.998 render/s | 1172.111 render/s |
| Mean raw interval ms | 23.130 | 23.712 / 24.419 | 16.575 | 0.847 |
| Worst raw interval ms | 54.873 | 57.756 / 54.942 | 17.309 | 28.533 |
| Simulation ticks / observed Hz | No fixed tick | No fixed tick | 179 / 59.665 | 179 / 59.520 |
| Preparation CPU fragment ms | .20723 | .19371 / .23413 | .01062 | .00242 |
| GL submission CPU ms | Not separated | Not separated | .12472 | .04732 |
| GL completion CPU wait ms | Not separated | Not separated | .78128 | .79831 |
| Process CPU seconds | 6.027 | 6.023 / 6.016 | .389 | 5.997 |
| Dropped simulation seconds | .01873 | .02011 / .01778 | 0 | 0 |
| Peak process RSS KiB | Not instrumented | Not instrumented | 67,732 | 68,692 |

These rates are **not equivalent frame metrics** and do not justify a speedup
ratio. Ren'Py redraw invalidations advance the viewport while many cached trees
are drawn (1199 accepted; 1232/1098 replay counters). Native counts completed render
loop iterations including swap and explicit glFinish. Neither measures physical
display refresh. Both request/control approximately 60 Hz in controlled paths,
but Ren'Py's fast cached redraw policy differs. Native has no diagnostic text/HUD
rendering, core versus compatibility GL, distinct importer/SDL build settings and
different pipeline structure. The uncapped column is a separate diagnostic.

The first native interval after warm-up is near zero, while the final pacing/
completion tail is included in wall duration but not a subsequent clock sample.
179 ticks over the ~3 s finite sample is a one-tick boundary effect, not a 59 Hz
simulation setting. Units and raw data are retained rather than rounded internally.

Active camera CPU-code fragment: Phase 1.1 accepted .02514 ms (68 updates), replay
1 .02540 ms; native .00138 ms/tick over only six queued direction samples.
These scopes and sample counts differ; they are instrumentation examples rather
than credible optimization/production budget claims. Idle simulation timings in
the native summaries include snapshot/tick overhead without held movement.

Native artifact **3,379,448 bytes**, fixture **16,216 bytes**. SDK's librenpython
**42,280,504 bytes**, SDK lib-tree logical bytes **184,395,369** (multiple features/
architectures); SDK launcher **4,760 bytes**. These are not equivalent distributable
packages. Native still depends on system C++/EGL/GL libraries and external scene
data; no packaged installer was built. Native context/SDL initialization in the
self-test measured 35.76 ms, excluding scene and first draw; reference startup/RSS
were not instrumented and are explicitly null in the footprint report.

The first Ren'Py replay's forced stall recorded zero movement although all 83
checks passed; its accepted assertion tests `<= .150001`, without requiring W to
remain held. The second replay recorded .150 m at 265.319 ms raw / 50 ms clamped.
Both reports/traces are retained. This is input/interaction variability to inspect,
not a silently fixed Phase 1.1 change. Native's assertion requires actual .150 m.

### Architectural comparison

| Dimension | Ren'Py reference | Native reference |
| --- | --- | --- |
| Loop / world update | Interaction/cache invalidation owner | Explicit CORDEL accumulator before render |
| Fixed ticks / interpolation | None | 60 Hz / previous-current interpolation |
| Input | UI/story/hover/keymap sharing | One native held-state owner; narrative contention not yet tested |
| Mouse | Uncaptured drag | SDL relative-mode API; physical behavior pending |
| Asset/GL lifetime | Prediction/cache pin, partial observations | Scene-scoped deterministic owners and deletion counters |
| Import coordinates | Assimp reflection cancellation | Native glTF convention; storage conversion only |
| Resize | Virtual/display/pbuffer coupling | Drawable projection and explicit offscreen surface lifetime |
| Narrative | Full real Say/Menu | None in this milestone |
| Python per frame | Yes | No |

## Executed commands and acceptance

From repository root after BOOTSTRAP's existing Python/SDK setup:

```sh
uv pip install --python .venv/bin/python cmake==3.31.6
export PATH="$PWD/.venv/bin:$PATH"
bash native/phase1_host/tools/run_offscreen.sh tmp/phase1_2-sdl348-final
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  build/phase1-host/cordel-native-host --seconds 3 --output tmp/phase1_2-sdl348-controlled
SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
  build/phase1-host/cordel-native-host --uncapped --seconds 3 --output tmp/phase1_2-sdl348-uncapped
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/phase1_2-renpy-regression
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/phase1_2-renpy-repeat
bash examples/cordel_viewport/tools/check_baseline.sh tmp/phase1_2-renpy-baseline
```

All exit **0**. The native wrapper expands to configure with
`-DCORDEL_OFFSCREEN_ONLY=ON`, build `-j 4`, `ctest --output-on-failure -V`, and
offscreen `--self-test` under timeout. The accepted source tests remain:

A second, fresh `build/phase1-host-clean` directory independently configured and
built the same pinned source, passed both CTest targets and repeated all 112
graphical assertions/five cycles/full host recreation. It used the same commands
with that build path and `tmp/phase1_2-clean-selftest`; clean transcripts are
archived. No compiler warnings were reported by either final native build.

| Check | Executed result |
| --- | --- |
| Native CTest | 2/2 targets passed: 304 native assertions + Python/shared-golden validation |
| Native context/pixels/input/timing/resources | 112 assertions passed; five cycles, extra full host recreation |
| Phase 1.1 helper suite | 31 passed, both replays |
| Phase 1.1 runtime | 83 check records passed, both replays |
| Phase 1.1 UI | 1 testcase, 8 assertions, 2 hooks passed, both replays |
| Existing source selection | 29 passed |
| Existing selected SDK suite | 89 passed (overlaps source selection) |
| The Question endings | 2 passed, history case unselected; 2 assertions, 3 hooks |
| Viewport lint | Exit 0; same generic conflicting-properties advice |

Known full-suite audio/SDK/history-click failures remain separate, unchanged and
unclaimed. Ren'Py native source compilation is still blocked by its wider missing
development dependencies; building this smaller host does not solve that baseline.

## Problems discovered and limits

1. **Ownership is materially clearer, but still one thread.** Fixed simulation no
   longer depends on cached displayables, yet GL/event stalls still delay ticks.
   Bounded catch-up loses time deliberately. No hard-real-time/threaded scheduler.
2. **Offscreen platform assumptions persist.** SDL resize does not reallocate its
   pbuffer and swap is insufficient backpressure. Owning surface/frame policy made
   those constraints explicit and testable; it did not remove them automatically.
3. **Mouse flags are not device evidence.** Request/query/event dispatch work, but
   no real offscreen keyboard focus or physical relative-motion/alt-tab verification.
4. **Native input has only one consumer.** Cleaner action ownership here has not
   proved coexistence with Ren'Py's synchronous UI/rollback/restart/audio ownership.
5. **Tiny scene and synchronous GL.** No world-scale culling, streaming, animation,
   shader/material pipeline or production in-flight queue. glFinish is a comparison
   policy with explicit CPU waiting; short runs are not hardware/AAA benchmarks.
6. **Resource counters have a precise boundary.** Actual CORDEL glDelete calls and
   zero live owners are proven, driver residency/reclamation and long soaks are not.
7. **Deployment is a source experiment.** One Linux offscreen build verified;
   visible desktop, high-DPI, other platforms, packaging and controller devices open.

## Recommendation and next gate

The experiment supports a measured **yes** for clearer runtime ownership at this
scale: simulation timing is independent of render frequency, input state has an
explicit owner, and scene teardown is deterministic and observable. The principal
gain is architectural predictability, not the non-equivalent FPS comparison.
Driver blocking and forthcoming narrative contention remain material constraints.

Ready for **Phase 1.3 — Narrative Ownership Experiment**: exercise one actual
Ren'Py line/choice/wait while the native fixed world keeps ticking; measure input
handoff, cancellation, exceptions/restart and packaging cost against the reference.
Evaluate the adapter/transport rather than silently committing to one. Phase 1.3
has not started. Make the final runtime/backend ADR only at **Phase 1.4**, informed
by all three experiments and later desktop/device evidence.
