# Phase 2.2 — Third-Person Character Motor

**Software/native gate passed 2026-10-08. CORDEL ENGINE 0.1.0-dev.**
Phase 2.1 at `057d304c5958e00913dae3d4d1383dff6dbf81fa` was fast-forwarded
into `main` and pushed before creating `cordel/phase2-character-motor`.
No squashing, ancestry restoration or force-push occurred. Phase 2.2 stays on its
review branch. [Evidence](evidence/phase2_2/manifest.json) identifies the tested
implementation revision and file hashes; the subsequent commit adds documentation
and archived results only.

## Motor architecture and ownership

`native/runtime/character/include/cordel/character/motor.hpp` declares the config,
input, authoritative state, fixed-size diagnostics and `CharacterMotor` API.
`src/motor.cpp` owns reset, motion, gravity, grounding, recovery, sweep-slide and
step semantics. It calls only CORDEL `PhysicsWorld` queries; no Jolt header,
CharacterVirtual/Character or backend pointer appears in gameplay code.
`src/demo_fixture.cpp` is separate greybox data, not movement logic.

The noncopyable motor owns its capsule shape and Character-layer query/sensor
proxy. Destruction releases body before shape. World/generation IDs validate every
simulation call. A new opaque weak lifetime token lets a motor reject an expired
world and destruct safely after its world has already reclaimed the proxy. This
is an expiration guard, not concurrent ownership: all operations/destruction stay
on the authoritative world thread. Clearing a live world also invalidates its
handles; further motor simulation fails explicitly.

`FrameRunner` optionally owns a motor, replacing the old camera sensor probe.
Each authoritative tick snapshots native held actions, calls motor simulation,
then steps the CPU physics world and drains sensor events, then dispatches the
existing narrative fixed boundary. The default Phase 1 free-camera path remains
available. Python never executes in the world loop.

Renderer integration lives in `native/phase1_host/src/character/debug.cpp` and
`experiment.cpp`. Renderer accepts CPU scene data and named transform/color updates.
The motor emits a bounded six-normal packet and positions, with no OpenGL calls.

## Capsule and development config

Position is the upright capsule **centre**, not the foot. Coordinates remain
right-handed, +Y up, +X right, -Z forward, one unit/metre. Capsule hemispheres have
radius 0.35 m and centres 1.10 m apart; total height is 1.80 m. Physics quaternion
ordering and original fixture rotations remain unchanged.

| Parameter | Value |
|---|---:|
| Height / radius / cylinder | 1.80 / 0.35 / 1.10 m |
| Walk / sprint planar speed | 4.5 / 7.0 m/s |
| Gravity | 9.81 m/s² |
| Walkable slope | ≤45°, existing query helper's 0.001° tolerance |
| Step height | 0.35 m |
| Contact skin | 0.003 m |
| Ground probe | 0.025 m |
| Maximum downward snap | 0.15 m |
| Recovery displacement budget | 0.20 m total per tick |
| Recovery penetration tolerance | 0.00005 m |
| Slide / recovery iteration caps | 6 / 6 |
| Simulation | 60 Hz, max 3 catch-up ticks |

[Config JSON](evidence/phase2_2/motor-config.json) is emitted from the actual config.
Finite positive dimensions/limits, planar finite input, fixed dt and valid handles
are checked before queries. NaN position/velocity/input/dt, invalid dimensions,
slope/step/skin config, stale handles/world and deep overlap are rejection tests.

## Movement, gravity and grounding

Native held input maps W/S to -Z/+Z, A/D to -X/+X and Left Shift to sprint.
Combined planar magnitude is capped at one; diagonals do not gain speed. The
existing radial 0.15 controller deadzone preserves analog magnitude. No camera
basis enters motor motion. `horizontal_velocity` records the **desired** planar
velocity; actual constrained displacement is in diagnostics.

Every move derives from velocity × 1/60. Airborne gravity uses semi-implicit Euler:
first update vertical velocity, then its displacement. Walkable contact clears
negative vertical velocity. Ceiling impact clears positive velocity; gravity can
resume on the next tick. Reset's optional vertical velocity is for controlled
state/test setup, not a jump action.

Grounding casts the capsule downward and records body, separation, normal,
walkability and support. Capsule separation normals at box corners can identify
an edge rather than its supporting top face. Five tiny rays (centre and four
2 mm horizontal offsets; origin 20 mm above contact, 40 mm down) select a surface
normal on the **same body**. A top face is eligible only within the configured
step-height envelope above the foot. This fixed import-independent query rule
contains no fixture-name branches. A steep ramp still reports its steep face.

Snap uses the actual capsule sweep, max 0.15 m, only when current/previous motion
has grounded support and vertical velocity is nonpositive. It cannot attach to
unwalkable walls or cross the tested large drops. Each tick's downward adjustment
is bounded; this is development adhesion, not final cinematic locomotion tuning.
The 0.10 and 0.30 m descents stay supported while the capsule travels around the
edge; the 0.60 m descent becomes airborne (6/30 ticks at the sampled endpoint).
The ledge walkoff loses support and falls rather than snapping to distant ground.

## Wall sliding and corners

Sweep to the safe contact distance (3 mm skin along motion), project remaining
motion off its inward collision component, and repeat up to six iterations.
When a second plane blocks that slide, keep only the intersection crease; reject
remaining motion entering an already collected plane. Tangential magnitude is
preserved, not renormalized to full walk speed. This stopped the initial opposing
ceiling/door-corner iteration churn. The cap remains observable and bounded.

Straight, diagonal and shallow wall approaches, inner/outer corners, wall+ground,
wall+slope, step beside wall, low ceiling over a step, narrow doorway and edge
landing have explicit region/penetration checks. Outside-corner approach starts
within capsule reach of the wall; it glances around it, so its final X changes.
The motor has no general optimal multi-contact solver or pathological-geometry
claim.

## Slopes and steps

10/25/35/45° ramps pass 20-tick uphill and downhill tests with all ticks grounded.
Walking preserves the requested planar speed and derives ramp Y motion from the
surface plane; total surface path speed therefore increases on a ramp. This is
an explicit initial policy, not an animation-ready constant surface-speed rule.

55° is **unwalkable**. It may be supported by a contact, but `grounded` and
`walkable_ground` stay false. Remove uphill planar input and project gravity along
the steep surface. The tested attempt moves downhill (X decreases, Y drops),
with no ordinary uphill climbing or indefinite standing.

On a grounded horizontal obstruction, validate upward clearance, raised forward
sweep, downward landing, walkable top, total tread rise from the foot and final
capsule overlap. Rise ≤0.35 m; every phase is a real query. Measuring rise from the
foot prevents oversized steps from being climbed in repeated increments.
Small 0.10 m geometry can be traversed by ordinary capsule sweep/projection;
0.20/0.30 m tests exercise accepted step candidates. 0.45/0.60 m remain blocked.
No upward teleport based on obstacle name/height is used.

## Recovery, ceiling, doors and sensors

Recovery selects deepest overlaps and moves along their outward normals, within
six iterations/0.20 m total. Small floor/wall/corner overlaps recover in 1/1/3
iterations. Deep invalid penetration throws with the original authoritative
position preserved and `InvalidPenetration`; it does not silently apply huge
corrections. Idle ground has no repeated recovery and no creep.

A standing capsule cannot enter the 1.50 m ceiling clearance. A ceiling over the
0.30 m step rejects the step; the upward-velocity probe hits its separate ceiling
and clamps velocity. Independent post-tick overlaps stay below 0.0001 m.
Door gap 0.60 m blocks the 0.70 m diameter capsule; 0.80 m passes, including glancing
entry. Exact contact positions reflect rounded capsule/door corners, not a box
approximation.

The motor's actual Character-layer proxy participates in the accepted query-derived
sensor system. Its traversal produces **1 begin, 34 persist, 1 end**. Events are
drained every fixed tick, outside motor logic. There are no gameplay trigger effects.

## Input, narrative, pause and interpolation

Input focus loss goes through the existing SDL event handler: held actions clear,
subsequent horizontal displacement is zero. An airborne motor keeps falling.
Mouse controls the stationary inspection camera through the existing native
relative-mode path; Escape releases capture. Physical desktop input remains
unverified. There is no orbit/follow camera or motor Space/Ctrl movement.

Real Ren'Py worker waits preserve the existing explicit policy: WASD remains
owned by gameplay during dialogue, choice and event waits; Enter/1/2 route narrative
presentation. Each motor wait starts airborne, making movement and gravity
observable. The command still changes actual beacon visibility and the native
event still resumes the story. See [continuity](evidence/phase2_2/narrative-continuity.json).

| Wait | Motor/world/physics ticks | Render frames | ΔZ / ΔY m | Dropped time |
|---|---:|---:|---:|---:|
| Dialogue | 25 | 23 | -1.875 / -0.885625 | 0 |
| Gameplay event | 25 | 23 | -1.875 / -0.885625 | 0 |
| Choice | 24 | 23 | -1.800 / -0.817500 | 0 |
| Explicit pause | 0 | 23 | 0 / 0 | 0 |
| Resume dialogue | 24 | 23 | -1.800 / -0.817500 | 0 |

Pause resets the accumulator; rendering/event pumping continue. Resume carries no
catch-up debt. The separate native pause test also records zero motor ticks and
21 frames including its resume frame. Narrative checkpoints remain the earlier
fixture/world-fact/free-camera experiment: durable motor saves/rollback are not
implemented or claimed.

Render position is `previous_position*(1-alpha)+position*alpha`. This accessor
clamps finite alpha, does not query physics, and never mutates authoritative state.
The greybox capsule changes green/red with grounded/airborne state. CPU packets
also draw ground normal/probe, desired/actual displacements, collision normals and
accepted step marker. The same fixture descriptors build collision and greybox
geometry; extra pads make separated ceiling/door/sensor cases playable. No editor.

## Executed scenarios, rate tests and soak

43 scenario records describe initial centre, tick input, duration, expected region,
grounded state and actual outcomes; [scenario definitions](evidence/phase2_2/scenario-definitions.json)
also record checked expected sensor-event predicates. C++ cases remain the execution
source; JSON is the machine-readable specification/result archive. [Full results](evidence/phase2_2/scenario-results.json).
Flat travel over 300 ticks is 22.5 m; sprint is 35 m. Diagonal travel is 4.5 m total
in 60 ticks. Ground/wall contact stays stable, all post-tick overlap probes pass.

Five fixed-tick sequences (flat, corner, ramp up/down, step out/back, ledge fall)
run under synthetic 30/60/120/variable/1000 Hz presentation schedules. All 25
results have **0 m** final position difference (assertion tolerance 1e-9 m),
300 authoritative ticks and no clock drops. Interpolation is evaluated per
synthetic presentation; these are scheduler/math tests, not physical monitor rates.

A 3,600-tick/60-simulated-second soak repeats six 120-tick segments with deterministic
reset boundaries: wall turns, ramp ascent/descent, ledges, sensor travel, steps and
idle. It records 180 sensor events, zero maximum measured penetration, finite
states, unchanged live body/shape counts and a fixed-capacity diagnostic packet.
It runs faster than wall time. Its direct-tick schedule has no backlog; normal
wall-clock drop behavior is measured separately below.

Five independent CPU world/motor cycles end with motor/world/body/shape counts
zero. Five native renderer/world/motor cycles also end with all GL counts zero;
198 frames completed in that test. This proves explicit owned-handle teardown,
not driver-memory or allocator page reclamation.

## Timing and pacing measurements

Debian 13.6 x86_64, GCC 14.2 Release, pinned Jolt v5.3.0, SDL 3.4.8,
Mesa 25.0.7 llvmpipe LLVM 19.1.7 OpenGL 4.5 core, offscreen 850×480.
No hardware/performance certification. No GPU timer queries.

F6 sleeps 250 ms; measured full raw frame interval includes the normal preceding
17 ms test pacing. Grounded interval **268.189 ms** runs three ticks/**50 ms**,
drops **216.667 ms** of whole-tick backlog, retains fractional accumulator time,
and moves **0.225 m** at 4.5 m/s. Airborne interval 268.601 ms runs three ticks and
moves Y **-0.01635 m** from rest. No unbounded catch-up. See [stall trace](evidence/phase2_2/stall-results.json).

Representative 300-reset-sample CPU fragments, including motor collision queries
but excluding fixture/reset/test, world.step and rendering:

| Workload | Mean ms | Median ms | Worst ms |
|---|---:|---:|---:|
| Idle grounded | 0.020409 | 0.013521 | 0.774741 |
| Flat movement | 0.026434 | 0.015042 | 0.728321 |
| Wall slide | 0.024032 | 0.018908 | 0.258371 |
| Ramp | 0.024579 | 0.018007 | 0.623522 |
| Step negotiation | 0.037957 | 0.025048 | 0.673719 |
| Recovery | 0.017921 | 0.014271 | 0.337570 |

These wall durations include descheduling. The ADR 0001 world+physics **<4 ms**
provisional budget remains visible; this tiny static motor fragment cannot certify
that full production budget. Raw query costs are measured separately by the
unchanged Phase 2.1 gate. Native traces separate motor CPU from world query/step CPU.

Actual three-second idle-motor rendering runs (same 850×480 greybox, synchronous
`glFinish` diagnostic completion):

| Requested presentation | Observed renders/s | World ticks/s | Worst interval ms | Dropped s |
|---|---:|---:|---:|---:|
| 30 Hz sleep | 29.999 | 59.331 | 33.419 | 0 |
| 60 Hz sleep | 59.998 | 59.665 | 16.728 | 0 |
| 120 Hz sleep | 119.994 | 59.664 | 11.006 | 0 |
| Uncapped | 820.330 | 59.213 | 24.696 | 0 |

Short-run boundary/warm-up/final-sleep effects account for observed tick rates
slightly below 60. Fixed dt remains 1/60; the world is not tied to render frequency.
Process CPU/RSS and CPU render fragments are in [pacing JSON](evidence/phase2_2/pacing-results.json).
Resize while motor runs covers 500×500, 320×500, 1200×700 and 850×480; projection
uses drawable size. Original numeric depth/aspect tests still pass in regression.

## Verification and reproducibility

After [BOOTSTRAP](BOOTSTRAP.md), executed complete command:

```sh
bash native/runtime/character/tools/run_phase2_2.sh tmp/phase2_2-verified
```

It configures/builds the selected Jolt host, runs all five CTest targets, the motor
binary, native SDL/GL probes, real-worker continuity, paced/uncapped runs, and:

```sh
CORDEL_PHYSICS_BUILD_DIR="$PWD/build/phase2-physics" \
  bash native/physics/tools/run_phase2_1.sh <new-output>/physics-regression
bash scripts/cordel_phase1_smoke.sh <new-output>/phase1-regression
bash examples/cordel_viewport/tools/run_offscreen.sh <new-output>/renpy-regression
bash examples/cordel_viewport/tools/check_baseline.sh <new-output>/renpy-baseline
```

All passed: **10,202 motor assertions / 43 scenarios / 25 rate comparisons**, 70
motor narrative checks, both adapters 207 assertions × five repeated runs each,
Phase 1 smoke 191 narrative/129 native checks and both ordinary reference branches,
31 Python viewport helpers/83 runtime checks/UI assertions, 29 source tests,
89 selected SDK-backed tests and both The Question endings. The original documented
full-suite failures are still excluded and unchanged. Commands/results and tested
source hashes are archived; builds, SDKs and dependencies are not committed.

Bounded `--motor-zone ramp35 --seconds 1` and `--motor --narrative --seconds 1`
offscreen CLI runs additionally pass, including grounded ramp ticks, story-wait
world ticks and clean shutdown. These are not physical interactive input evidence.

The first gate completed its runtime/regression stages but failed screenshot
packaging on an absent Pillow module. PNG encoding now uses Python's standard
library; the entire final gate was rerun successfully. No dependency was added.

## Limitations and next milestone

- Linux software and synthetic SDL input only. Physical keyboard/mouse/controller,
  accelerated GPU, Windows/macOS and desktop/DPI are unverified.
- Upright capsule against static convex boxes; no moving platforms, dynamic pushing,
  triangle-mesh seam qualification, crouch, jump, animation or third-person camera.
- Small fixed contact-neighbourhood rays address tested box edges. Arbitrary concave,
  pathological multi-contact and very thin geometry need later tests. Bounded
  blocking/iteration caps are explicit, not a universal solver guarantee.
- Gravity-projected steep sliding has development tuning; desired planar velocity
  changes instantly. Ramp path speed and edge adhesion are explicit provisional rules.
- Sampled regions/tolerances and deterministic resets do not constitute a production
  soak, persistent world or arbitrary level traversal qualification.
- A stationary inspection camera can lose sight of the capsule. Debug zones are
  launch-time test placement; no level editor, camera follow or cinematic behavior.
- Native narrative presentation remains console/debug, fixture-scoped and asynchronous.
  Full motor checkpoint/save/rollback and cinematic side-effect policy are deferred.

CORDEL now owns movement semantics over a backend-independent query boundary,
independently of rendering and Ren'Py waits. Jolt supplies collision results; it
does not dictate the gameplay motor API. The accepted ownership architecture
remains intact. **Next: Phase 2.3 — Follow / Orbit Camera**, using interpolated motor
presentation with explicit obstruction/input ownership tests. It has not begun.
