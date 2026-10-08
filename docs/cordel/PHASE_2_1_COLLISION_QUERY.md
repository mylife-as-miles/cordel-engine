# Phase 2.1 — Native collision / physics query foundation

Software gate passed 2026-10-08. Phase 1.4 was fast-forwarded into main at
`0baf08bbf478ba5540ee3b5da067df83fe5e2724`; implementation remains on
`cordel/phase2-collision-query`. [ADR 0002](adr/0002-physics-query-backend.md)
selects Jolt for development after comparing both candidates. Phase 2.2 has not begun.

## Implementation and locations

| Source | Responsibility |
| --- | --- |
| `native/physics/include/cordel/physics/{types,queries,world}.hpp` | Backend-independent world, generation-safe IDs, layers, shapes, queries, contacts |
| `native/physics/src/common/world.cpp` | Validation, handle registry, normalized results, authoritative-thread checks, event/lifetime policy |
| `native/physics/src/common/backend.hpp` | Private adapter interface; exactly one factory per linked executable |
| `native/physics/src/{jolt,bullet}/backend.cpp` | Real library ray/sweep/overlap/update calls and deterministic resource ownership |
| `native/physics/src/common/fixture.cpp` | Same deterministic CPU geometry generator for both candidates |
| `native/physics/include/cordel/physics/helpers.hpp` | Slope/walkability, clearance, raised-forward/downward landing feasibility |
| `native/physics/tests/test_queries.cpp` | Shared assertions, five lifecycle cycles, fixed-clock and warmed workloads |
| `native/physics/tools/{run_phase2_1.sh,gate.py}` | Bounded configure/build/CTest/repeated measurement/evidence gate |
| `native/phase1_host/include/cordel/physics_runtime.hpp` | Native-owned fixture/probe/query state; no renderer references |
| `native/phase1_host/src/runtime.cpp` | Physics step/query before each fixed world/camera update |
| Native diagnostics/self-test/narrative experiment | Physics timing, tick continuity, pause/stall and explicit teardown checks |

Only additive CORDEL code and the existing native experiment change. Ren'Py core,
its ancestry/attribution/licenses, existing example sources and original graphics
fixture remain intact. No character motor, physics gameplay/dynamic actors, ECS,
animation, production renderer or extra scheduler is implemented.

## Reproduction and dependencies

From `/workspace/cordel-engine`, use BOOTSTRAP's existing `.venv`:

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake -S native/physics -B build/physics -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics --target cordel-physics-jolt cordel-physics-bullet -j 4
ctest --test-dir build/physics --output-on-failure -V
bash native/physics/tools/run_phase2_1.sh tmp/new-physics-gate

cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON
cmake --build build/phase1-host -j 4
ctest --test-dir build/phase1-host --output-on-failure
bash scripts/cordel_phase1_smoke.sh tmp/new-phase1-regression
```

Actual final physics measurement command:

```sh
CORDEL_PHYSICS_BUILD_DIR="$PWD/build/phase2-physics-final" \
  bash native/physics/tools/run_phase2_1.sh tmp/phase2_1-verified
```

The fresh ignored build directory establishes clean candidate target builds.
Default subsequent gate runs may be incremental and are labeled accordingly.
The gate uses immutable FetchContent URLs plus verified SHA256; acquisition and
configure time are excluded from target build timings. No privileged installations.
Builds target needed collision/dynamics/math libraries, not Bullet demos/extras.

| Dependency | Revision | License | Native default executable |
| --- | --- | --- | --- |
| Jolt v5.3.0 | `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db` | MIT | Statically linked |
| Bullet 3.25 | `2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5` | zlib | Comparison executable only |

Pinned notices and acquisition hashes are in
[dependency-manifest.json](evidence/phase2_1/dependency-manifest.json).
Boundary/host C++20, Release GCC14.2/CMake3.31.6 on Debian13.6 x86_64; libraries
retain their upstream language standards. Single-precision backend geometry,
double boundary values; Jolt SSE2 baseline, AVX/LTO/profiling/debug-renderer off,
exceptions/RTTI enabled. No new threads: Jolt has a per-world synchronous job
implementation; Bullet sequential dispatcher/solver. Flags are archived separately.
The native OpenGL reference remains SDL3/Mesa25.0.7/llvmpipe, not hardware evidence.

## Scene, coordinates and shape semantics

Both adapters consume **25 authored boxes**, generated once by common C++ source:
ground, front wall and side corner; five ramps 10/25/35/45/55°; five steps
.10/.20/.30/.45/.60m plus five floor pads; low ceiling; two doorway jambs;
ledge/pit; sensor; two overlapping obstacles. Test zones are spatially separated.
Descriptors from each executed candidate are compared exactly and archived in
[fixture.json](evidence/phase2_1/fixture.json). Geometry is CORDEL-authored under
MIT; no external level or renderer mesh import. The renderer's original glTF
remains seven meshes/84 triangles and has unchanged asset hashes.

RH +Y up, +X right, -Z forward, metres. XYZW active local-to-world unit quaternions.
Ramp rotation is about +Z; expected upward normal is (-sin(angle),cos(angle),0).
There is no Assimp reflection or graphics conversion in the physics boundary.
Boxes use half extents and zero box margin/convex radius for analytic comparisons.
The capsule is Y-oriented, **1.8m total height, .35m radius, 1.1m cylinder**;
hemisphere centers are at center.y±.55m, extremes center.y±.9m.
A standing query center at .92m has .02m floor clearance. This is query clearance,
not an implemented character skin/contact solver. Capsule height must exceed diameter.

Cast displacement supplies direction and maximum length; fractions are [0,1] and
hit distance = displacement length × fraction. Normals are unit target-to-query
separation directions; nonnegative overlap penetration is deduplicated per body
at deepest contact. Jolt returns an opaque primitive subshape token (all-ones for
these whole primitive shapes), not a face index; Bullet returns unavailable/null.

Start-inside rays use a common 10µm sphere narrowphase precheck: strict interior
reports fraction/distance zero and an undefined zero normal. Sweeps precheck
initial overlap >.1mm and expose `started_inside` plus penetration instead of
relying on inconsistent backend start-penetrating sweep behavior. Exact surface
and sharp-edge behavior remains tolerance-sensitive. Deep symmetric penetration
can have several valid recovery normals; no universal depenetration solver is claimed.

## Query, slope, step and filtering results

**207 identical assertions per candidate pass**, including:

- Ground ray distance 3m/up normal; wall distance 3.75m/+Z normal; miss, inside,
  explicit sensor and ignored-body filters, empty mask, invalid zero/NaN inputs.
- Wall capsule clearance distance 3.4m; diagonal path length, corner side normal
  and two-body corner overlap; ground/ceiling sweeps; five ramp landing casts.
- Empty/ground/wall/multiple/sensor overlaps; .1m ground and .2m wall penetration;
  using wall recovery direction/depth plus .005m clears the obstacle.
- Actual normals classify 10/25/35/45° walkable and 55° unwalkable. Boundary probes
  include 44.999/45/45.01/90°; classification tolerance is .001°.
- Raising .35m, then forward cast and downward landing permits .10/.20/.30m steps
  and rejects .45/.60m steps. This is feasibility only, not motion or a motor.
- 1.5m overhead opening rejects the 1.8m capsule; .60m doorway blocks .70m width;
  widening to .80m passes; ledge ground exists and pit ground does not.
- WorldStatic/Character/Sensor/GameplayQuery all have explicit positive/negative
  query tests. Default mask includes only WorldStatic.
- Invalid/stale/cross-world IDs, shape-in-use destruction, duplicate destroy,
  reused slot/new generation, invalid shapes, foreign-thread queries and tilted
  Character sensor probes are rejected.

Analytic distance tolerance is generally .003m, sweep .006m; normals .01–.02
vector error, ramp ray .003° and sweep .05°. Exact expected conditions and values
are recorded with each check. Tolerances allow float differences without hiding
wrong hit IDs, missed obstacles, wrong classification or flipped normals.

Sensor begin/persist/end derive from upright Character-capsule versus Sensor
queries on the authoritative fixed thread. These are not general rigid-body
callbacks. Drain events every tick; stepping with unread events throws. Registry
capacity is 4096 body/shape slots; pair capacity is 4096, event snapshot at most
8192 current/end records. Tests cover drain rejection and pair transitions; the
maximum-capacity stress case is not executed. End IDs are historical and must
be validated before dereferencing. Clear/unload cancels pending event state.

## Native fixed-clock ownership and resource lifetime

`FrameRunner` owns `PhysicsRuntime`, which owns its world, fixture and one capsule
sensor probe. SDL/narrative pumping precedes the existing bounded fixed clock;
each tick steps physics/queries, updates free-camera state, then applies narrative
world commands. Rendering only extracts/interpolates camera state. No presentation
dt, Python, Ren'Py, GL object or renderer reference enters the physics API.

The native diagnostic fixture is intentionally invisible and independent of the
reference graphics scene. Ground/overlap query results are logged; camera motion
remains unconstrained. Render caches or GPU unload do not own collision state.

Both candidate clock tests prove 120 presentation intervals at 120Hz schedule
60 physics ticks and a .25s input interval schedules three ticks/.05s, dropping
.20s. The native SDL F6 test additionally exercises the real loop with physics.
Native and candidate lifecycle tests each run five cycles; bodies are removed
before shapes, stale IDs remain invalid, and final owned worlds/shapes/bodies
return to zero. Native GL creation/deletion balance and context recreation remain
intact. Actual library remove/destroy/ref-release calls are used, not counters alone.
Allocator committed pages, RSS reclamation and GPU driver release are not measured.

## Measured costs

Warmed identical 100/1000 workloads, 200 warmup calls each, five fresh process
runs per candidate. Measurements below use the 1000-call groups; means average
per-run sample means, medians are the median of per-run medians, worst is maximum
observed latency across runs. They include common prechecks/normalization and
clock/OS scheduling overhead. Queries are wall-clock CPU costs, not GPU timings.

| 1000-call groups: mean / median / worst µs | Jolt | Bullet |
| --- | --- | --- |
| Raycast | 0.420 / 0.411 / 19.790 | 0.638 / 0.571 / 64.076 |
| Capsule sweep | 0.926 / 0.891 / 57.647 | 1.767 / 1.703 / 74.973 |
| Overlap | 4.612 / 4.356 / 213.603 | 7.530 / 6.490 / 413.815 |
| Static fixed update | 0.205 / 0.200 / 19.309 | 4.719 / 3.996 / 583.232 |

| Initialization / footprint | Jolt | Bullet |
| --- | --- | --- |
| Initialization mean ms | 0.251 | 1.264 |
| Initialization median ms | 0.264 | 1.311 |
| Initialization worst ms | 0.316 | 1.450 |
| Clean target build s | 51.066 | 47.376 |
| Test executable bytes | 2492304 | 1195320 |
| Linked dependency archive bytes | 4509818 | 4285950 |
| Fetched source tree bytes | 34234883 | 336664963 |

Scene initialization includes backend/type setup and fixture creation. RSS is
process lifetime high-water mark; both measured 13,440KiB under this launcher,
which does not distinguish allocator use or prove equal memory footprint.
Static fixed-step workloads contain no dynamic actors or Character sensor probes;
the integrated host additionally has one probe and per-tick ground/overlap queries.
No hardware <4ms world-budget claim is made. Clean builds are single samples on
this host; source footprint includes upstream tests/content, not shipping data.

## Regression evidence and limitations

Executed commands (all pass):

```sh
bash scripts/cordel_phase1_smoke.sh tmp/phase2_1-phase1-verified
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/phase2_1-renpy-regression
bash examples/cordel_viewport/tools/check_baseline.sh tmp/phase2_1-renpy-baseline
```

Native graphics has **129 checks**, narrative **191 checks**, native logic **304**,
protocol **23**, four host CTest targets and 14 adapter Python tests; both ordinary
Ren'Py narrative reference branches agree. Phase 1.1 has **31 helper tests**,
**83 runtime checks**, UI **8 assertions/2 hooks**. Existing source tests **29**,
selected SDK tests **89**, and both The Question ending tests pass; sets overlap.
Lint reports no errors; known full-suite failures were not changed.

| Native narrative sample | Physics/world ticks | Render frames | Wall s | Dropped s |
| --- | --- | --- | --- | --- |
| dialogue | 25/25 | 24 | 0.4077 | 0 |
| wait_for_event | 24/24 | 24 | 0.4087 | 0 |
| choice | 24/24 | 24 | 0.4107 | 0 |
| dialogue (explicit pause) | 0/0 | 24 | 0.4099 | 0 |
| dialogue | 24/24 | 24 | 0.4138 | 0 |

Actual F6 raw interval **250.140ms**; three fixed physics/world ticks,
**50ms** simulated, **200ms** dropped, camera travels
**0.150m** at 3m/s. Physics step/query fragment totals
**0.07117ms** across those three ticks; this is one CPU sample.

Known full SDK/source-suite audio/history failures remain separate; those full
suites are not claimed green. Linux CPU query correctness and software/offscreen
native integration are verified. Windows/macOS, hardware performance, high DPI,
physical input devices and desktop capture remain pending. Existing Ren'Py source
native-build dependency blockers are unchanged.

Other limits: no triangle/compound collision shapes, moving platforms, dynamic
actors, material friction, continuous body collision, motor velocity/gravity/
ground adhesion, general contact listener, serialization, animation or production
physics debug renderer. Box zero margins are an analytic test choice; rounded
edges and mesh seams need later tests. Static debug probes are explicitly moved,
not solved. Generation handles are runtime identities, not persistent save IDs.
Jolt factory registration assumes worlds live on one engine-authoritative thread.
A sensor-capacity error fails the step and should retire that test world; no
transactional rollback of an already-stepped backend is implemented.

## Architecture implication and next gate

CORDEL can own native collision geometry, query semantics, fixed scheduling and
resource lifetime while narrative waits and rendering remain independent. Both
libraries meet the present static convex requirements. Jolt is the selected
**development** provider because of the tested query behavior, inspected character
query facilities and measured warmed costs; Bullet's smaller footprint is retained
as a documented tradeoff and a working alternative. This extends ADR 0001 without
handing world authority to middleware or Ren'Py.

Ready for **Phase 2.2 — Third-Person Character Motor**: implement a CORDEL-owned
capsule motor with gravity, grounded state, bounded slide/penetration recovery and
step/slope handling, then verify presentation-rate independence and difficult
contacts against the retained fixture. That work has not been started.
