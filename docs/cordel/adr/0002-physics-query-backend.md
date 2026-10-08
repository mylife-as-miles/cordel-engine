# ADR 0002 — Native collision/query backend

- Status: Accepted for Phase 2 development after executed Linux comparison gate
- Date: 2026-10-08
- Depends on: [ADR 0001](0001-runtime-narrative-ownership.md)
- Scope: static convex collision/query foundation; Phase 2.2 motor development

## Decision

Use **Jolt Physics v5.3.0**, revision
`0373ec0dd762e4bc2f6acdb08371ee84fa23c6db`, as the native development backend.
Retain **Bullet 3.25**, revision
`2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5`, as the separately linked comparison
adapter and regression executable. The selected host links one backend, never both.
CORDEL owns world/shape/body handles, query semantics, filtering, sensor events,
fixed stepping and destruction. No library pointers or graphics objects cross
`cordel::physics::PhysicsWorld`.

This decision selects a foundation for the next motor experiment. It does not
certify a production motor, dynamic simulation, shipped platforms, allocator
budgets, or world scale. Ren'Py retains no physics ownership.

## Evidence and alternatives

Both candidates pass **207 identical assertions**, including 25 generated boxes,
5 slopes, 5 steps, capsule sweeps, normals, penetration, narrow/clear doorway,
ceiling, ground loss, four layers, sensor begin/persist/end, stale and cross-world
IDs, and five teardown cycles. Each candidate also passes CTest and five separately
launched repeated runs. Both execute the existing 60 Hz clock with three catch-up
ticks; Jolt additionally passes actual native graphics/narrative integration.
No Bullet correctness failure was found. See
[query and comparison evidence](../evidence/phase2_1/comparison.json) and
[full report](../PHASE_2_1_COLLISION_QUERY.md).

Jolt's explicit shape-cast penetration axis/depth and subshape tokens, synchronous
single-thread query/update path, and inspected character-query facilities provide
a useful basis for a CORDEL-owned motor. The actual tests establish slope normals,
clearance/recovery inputs and initial-overlap handling; they do not test Jolt's
CharacterVirtual motor. Its source demonstrates explicit padding, penetration
recovery and step/slope operations that can inform later engineering without
requiring those types to escape our boundary.

Measured warmed 1,000-query workload medians favor Jolt here: approximately
0.411µs ray, 0.891µs sweep, 4.356µs overlap versus Bullet 0.571/1.703/6.490µs.
These include common normalization/prechecks and timer overhead. Both are viable
for this small scene; latency tails and static fixed-step measurements are not
production performance predictions.

Bullet offers a smaller test executable (1,195,320 versus 2,492,304 bytes), a
slightly shorter single clean build measurement (47.376 versus 51.066 seconds),
and familiar sequential collision callbacks. Jolt adds global type-registration
lifetime coordination. These costs are accepted for the tested query behavior and
future character work; no performance metric overrides correctness.

Both dependency licenses permit this use: Jolt MIT, Bullet zlib. Exact source URLs,
archive SHA256, build features, licenses and notices are retained in the
[dependency manifest](../../../native/physics/third_party/dependency-manifest.json).
No upstream source tree or build artifacts are committed.

## Ownership and constraints

- Native FrameRunner owns a PhysicsRuntime/world; one physics step and diagnostic
  queries execute at each fixed tick, before free-camera state updates.
- No additional physics thread. Jolt's per-world JobSystemSingleThreaded executes
  synchronously. Bullet receives one external fixed dt and disables its accumulator.
- Collision descriptors are CPU world state. The 25-box fixture is independent of
  the unchanged seven-mesh/84-triangle renderer scene. The reference camera still
  flies freely and does not collide; this is not a motor implementation.
- Capsule is upright Y, total height 1.8m, radius .35m, cylinder length 1.1m;
  center-based queries use explicit .02m ground clearance where appropriate.
- CORDEL query filters use WorldStatic/Character/Sensor/GameplayQuery layers.
  Character-layer capsules are repositionable sensor probes, not dynamic actors.
- Sensor events are query-derived capsule/sensor pairs on the simulation thread,
  not general rigid-body contact callbacks. Read events each tick; undrained events
  reject another tick. Bodies/shapes have 4096-slot limits; sensor pairs cap at 4096.
- Handles carry world token, slot and generation. Teardown removes bodies before
  shapes, clears event state, and destroys the backend world. Counters prove owned
  handle balance, not physical allocator or GPU driver memory return.

## Open gates and revisit criteria

Linux x86_64/GCC14.2 and software/offscreen native integration pass. Windows,
macOS, accelerated presentation and device input remain unverified. Linux CPU
query tests do not require graphics; llvmpipe only qualifies the integration run.

Revisit selection if Phase 2.2 reveals unstable grounding/sliding/step recovery,
mesh-edge artifacts, moving-platform/dynamic contact problems, platform build
failures, unacceptable footprint, or measured budget violations. Add those cases
to both adapters before drawing stronger conclusions. Triangle meshes, compounds,
large scenes, continuous dynamic-body simulation, motor gravity/friction and
network determinism are outside the current evidence.

Next: **Phase 2.2 — Third-Person Character Motor** on this query foundation.
