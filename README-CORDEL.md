# CORDEL ENGINE — 0.1.0-dev

CORDEL is a cinematic, narrative-driven 3D engine project founded on the
[Ren'Py repository](https://github.com/renpy/renpy). Phase 0 establishes an audited
codebase, a reproducible development baseline, and an engineering direction.
Phase 1.1 adds an isolated Ren'Py-backed 3D viewport reference experiment. Phase
1.2 adds a C++20 native host for the same scene, with executed software evidence
for fixed updates, interpolation, input and deterministic GL ownership. A
production CORDEL world/narrative runtime remains unimplemented. Phase 1.3 adds a
fixture-scoped isolated Ren'Py worker; executed software evidence confirms story
waits and failures leave the native world clock authoritative. Phase 1 is now
architecturally complete: [ADR 0001](docs/cordel/adr/0001-runtime-narrative-ownership.md)
accepts a native real-time runtime and independent narrative service boundary.
The isolated worker is the development default; deployment remains replaceable.
Phase 2.1 adds a native collision/query interface and executed Jolt/Bullet
comparison. [ADR 0002](docs/cordel/adr/0002-physics-query-backend.md) selects Jolt
for development; the next milestone is the third-person character motor.

Our long-term focus is third-person gameplay, expressive character animation,
interactive environments, branching dialogue, and seamless cinematic transitions.
The Last of Us and Uncharted inspire the categories of experiences we want to
support; CORDEL uses independently developed architecture and no proprietary
Naughty Dog technology. This is a specialized narrative engine project.

## Current foundation

- CORDEL uses fresh Git history on `main` in
  [mylife-as-miles/cordel-engine](https://github.com/mylife-as-miles/cordel-engine).
  The source foundation remains `68fdaef917919e330c90765dd69668fee1cb9654`.
  Upstream history was removed at the project owner's request; source and notices
  were retained, with Ren'Py available through the `upstream` reference remote.
- Original Python dependencies install in an isolated environment. Native source
  compilation is blocked by missing SDL3 and other development packages.
- Unmodified checkout code runs using an upstream nightly's native modules.
  Mesa offscreen OpenGL executes both ending tests for **The Question** successfully.
- 29 source-only unit tests and 89 selected SDK-backed unit tests pass. These
  sets overlap. Full suites have documented failures and are not green.
- No renderer, scripting, save-format, launcher, package-name, or license changes
  are part of Phase 0. The upstream runtime still identifies itself as Ren'Py.
- [Viewport spike](examples/cordel_viewport/README.md): generated static glTF,
  depth/perspective, fly camera, held input, uncaptured drag-look, resize and frame
  diagnostics. Headless checks and lifecycle/Say probes pass; desktop devices,
  hardware timing and GPU buffer release remain unverified.
- [Native comparison](native/phase1_host/README.md): experimental SDL3/OpenGL core
  host, 60 Hz fixed ticks/interpolation, seven-mesh fixture, depth/resize/stall
  probes, five explicit GL teardown cycles and complete host recreation. Software
  gate passes; physical relative input, controllers, desktop/DPI and driver memory
  remain unverified. Phase 1.1 and 1.2 are accepted into `main`.
- [Narrative ownership](narrative/phase1_adapter/README.md): real Ren'Py AST worker,
  bounded JSONL, native-owned presentation/input, beacon commands/events/facts,
  cancellation/failure isolation and one fixture checkpoint. Software gate passes;
  production compatibility and permanent deployment remain open. Phase 1.3
  is accepted into `main`; Phase 1.4 decision work stays on its review branch.

CORDEL owns the native world loop, input, simulation and GPU resources. Ren'Py
contributes narrative technology behind a transport-independent interface.
OpenGL Core is the development/reference renderer; the production backend is
undecided. Native production UI/future audio and coordinated checkpoint/rollback
ownership are accepted rules, not implemented production systems.

Linux x86_64 offscreen software is verified; Windows x86_64 is the next platform
qualification target. Hardware/devices, full narrative compatibility, in-process
hosting and durable saves remain open. **Next: Phase 2.1 — Collision / Query Adapter**,
comparing Jolt and Bullet; Phase 2 has not begun. Version remains `0.1.0-dev`.

## Development and design

Start with [BOOTSTRAP](docs/cordel/BOOTSTRAP.md) for verified commands, SDK checksum,
environment limits, and test results. Other project documents:

- [Architecture audit](docs/cordel/ARCHITECTURE_AUDIT.md): source evidence and reuse decisions.
- [Accepted architecture](docs/cordel/ARCHITECTURE.md): ownership, interfaces, deferred technologies and qualification gates.
- [Roadmap](docs/cordel/ROADMAP.md): completed Phase 1/2.1 gates and next character motor.
- [Upstream policy](docs/cordel/UPSTREAM.md): provenance, remotes, licenses, and synchronization.
- [Project metadata](cordel/project.toml): machine-readable identity and foundation commit.
- [Phase 1.1 findings](docs/cordel/PHASE_1_1_VIEWPORT.md): implementation, measurements and runtime limits.
- [Phase 1.2 findings](docs/cordel/PHASE_1_2_NATIVE_HOST.md): native host, comparison measurements and open device gates.
- [Phase 2.1 findings](docs/cordel/PHASE_2_1_COLLISION_QUERY.md): shared collision queries, measured backend selection and fixed-clock integration.
- [Phase 1.3 findings](docs/cordel/PHASE_1_3_NARRATIVE_OWNERSHIP.md): narrative boundary, continuity/failure evidence and compatibility limits.

Canonical software smoke after BOOTSTRAP setup (use a new or empty directory):

```sh
bash scripts/cordel_phase1_smoke.sh tmp/new-phase1-smoke
```

See [Phase 1 conclusion](docs/cordel/PHASE_1_ARCHITECTURE_DECISION.md) for exact
executed regressions, evidence and limits. Original Ren'Py remains the behavioral
reference; no physics, ECS, audio/UI engine or production renderer is added by ADR 0001.

## Identity and attribution

Use **CORDEL ENGINE** in CORDEL-facing documentation and future tools, with
**CORDEL ENGINE — 0.1.0-dev** for the full development identity. CORDEL versions use
`MAJOR.MINOR.PATCH` with `-dev` during development, `-alpha.N`, `-beta.N`, and `-rc.N`
for prereleases. These do not replace Ren'Py's independent engine or SDK versions.
Before 1.0, document breaking changes at each minor release; save and asset formats
will have explicit schema versions independent of product versions.

Keep future CORDEL additions in identifiable directories and use `cordel` for new
namespaces. Preserve `renpy` imports, internal identifiers, original copyright
headers, third-party notices, and the original [README](README.rst).

Ren'Py is primarily MIT licensed, with LGPL-covered portions and dependencies
under additional licenses. See [the upstream license text](sphinx/source/license.rst)
and [distribution obligations](docs/cordel/UPSTREAM.md). Existing attribution and
licenses remain in force; the project name does not change them.
