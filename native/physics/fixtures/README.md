# Deterministic CPU collision fixture

The authoritative generator is `src/common/fixture.cpp::collision_fixture()`.
Both candidates execute those same 25 box descriptors; neither imports renderer
meshes. Executed descriptors are emitted to `fixture.json` by the gate and compared
for exact equality between candidates. This evidence copy is not an editable level.

Ground y=0 spans x/z ±8m. A wall's near face is z=-3.75m; the side wall creates
an inside corner. Five ramps at x=20+8*i, z=0 rotate about +Z by 10/25/35/45/55°.
Five step boxes at x=20+8*i, z=15 have 10/20/30/45/60cm heights and separate floor
pads. Other isolated zones contain a ceiling with underside y=1.5m, a .60m door
(test temporarily widens to .80m then restores), a finite ledge with missing ground
beyond z=22m, a sensor, and two overlapping obstacles. Separation between test
zones prevents unrelated geometry from contaminating the expected nearest hit.

RH +Y up +X right -Z forward, metres; XYZW quaternions. Boxes are unrounded
(convex radius / box margin zero) for analytic comparisons. Capsule dimensions
and query clearance are defined in the parent README. This is authored procedural
geometry covered by the local CORDEL MIT license, with no external assets.
