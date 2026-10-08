# CORDEL Phase 2.1 collision/query foundation

A C++20, CPU-only collision boundary with two separately linked candidates.
The native host selects **Jolt** for development; Bullet remains the tested
comparison adapter. Neither dependency's public types escape the CORDEL API.
There is no character motor, dynamics gameplay, ECS or renderer integration here.

## Reproduce

From the repository root, using BOOTSTRAP's existing project-local CMake:

```sh
export PATH="$PWD/.venv/bin:$PATH"
cmake -S native/physics -B build/physics -DCMAKE_BUILD_TYPE=Release
cmake --build build/physics --target cordel-physics-jolt cordel-physics-bullet -j 4
ctest --test-dir build/physics --output-on-failure -V
bash native/physics/tools/run_phase2_1.sh tmp/new-physics-gate
bash scripts/cordel_phase1_smoke.sh tmp/new-native-regression
```

The gate requires a new/empty output directory. Each build/subprocess has a
bounded timeout, CTest failures fail the runner, and both executables must pass
the identical fixture/assertion names. Five separate process runs per candidate
collect warmed 100/1000-query workloads. Build targets avoid unused Bullet demos,
soft-body libraries and extras. Normal native-host CMake builds link **one** adapter:

```sh
cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON
cmake --build build/phase1-host -j 4
ctest --test-dir build/phase1-host --output-on-failure
```

`CORDEL_PHYSICS_BACKEND=Jolt` is the native default. `Bullet` is an explicit
comparison build option; any other value fails configuration. The physics-only
project defaults to both candidates. No Python runs inside either native query
or simulation loop. Python only orchestrates tests/measurement output.

For a clean timing run, choose a fresh ignored build directory with
`CORDEL_PHYSICS_BUILD_DIR="$PWD/build/my-fresh-physics"`. Configure/download time is
excluded from candidate target-build timing. Incremental runs are labeled
`clean_target_build=false`; never interpret these as clean builds. Source override
CMake variables are debugging conveniences, not the pinned acquisition gate.
Sources are fetched by immutable revision URL and verified archive SHA256.
See [dependency manifest](third_party/dependency-manifest.json), retained licenses,
[report](../../docs/cordel/PHASE_2_1_COLLISION_QUERY.md), and
[ADR 0002](../../docs/cordel/adr/0002-physics-query-backend.md).

## API contract

- RH, +Y up, +X right, -Z forward; metres. Rotations are active XYZW unit
  quaternions; no axis reflection or renderer-derived transforms.
- Boxes use half extents, capsules total height/radius. The test capsule is
  1.8m tall, radius .35m, with a 1.1m cylinder and .35m hemispheres, upright about Y.
  Capsule center is the geometric center; bottom = center.y - .9m. Helpers use
  center.y=.92m above y=0 ground, giving .02m query clearance.
- Ray/cast displacement gives direction **and** maximum length. Fraction is [0,1],
  distance is fraction times displacement length. Queries reject zero/non-finite
  vectors, invalid handles, unknown layers and invalid shape dimensions.
- `normal` points from target toward query: a separation direction, not a velocity.
  Overlap depth is nonnegative; contacts are deduplicated by body at deepest depth.
  Exact symmetric penetration and sharp edges can have nonunique recovery normals.
- Rays inside convex bodies report distance/fraction zero, `started_inside=true`,
  and an undefined zero normal. A 10µm narrowphase sphere supplies the common
  interior precheck; behavior within that surface tolerance is not guaranteed.
  Sweeps precheck penetration >.1mm and report initial overlap explicitly.
- Default queries hit only WorldStatic; Character, Sensor and GameplayQuery are
  explicit masks. Ignore-one-body IDs are validated, never native pointers.
- Body/shape IDs include world token, slot and generation. Deleted and cross-world
  IDs fail; shapes cannot be destroyed while referenced by bodies. 4096 slots per
  kind; wrapped generations retire slots. Objects and queries have one authoritative
  thread; worlds are noncopyable/nonmovable. Global live counters are atomic observations.
- Static geometry can be repositioned for probes without creating dynamic bodies.
  Character-layer bodies are upright capsule **sensor probes**, not character motors.
- `step` accepts exactly 1/60s. Jolt uses its single-thread job implementation;
  Bullet receives one externally fixed tick with no internal accumulator.
- Sensor events derive from Character-capsule versus Sensor overlap pairs. Begin,
  persist and end are emitted on the caller's fixed-update thread. This is not
  general rigid-body contact listening. Drain `take_contact_events()` every tick;
  another step with unread events throws instead of losing transitions. Pair
  capacity is 4096, at most 8192 current/end records. `clear()` discards session
  events. End events may contain retired IDs; consumers must validate before use.
- `clear` removes bodies before releasing shapes; destructor destroys the backend
  world/global registration when the last Jolt world leaves. Zero CORDEL counters
  and native remove/delete calls prove ownership balance, not allocator/RSS return.

The fixture is authored CORDEL geometry licensed with this directory under MIT.
No downloaded level/model or renderer vertex buffer supplies collision state.
