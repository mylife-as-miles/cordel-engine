# CORDEL ENGINE — 0.1.0-dev

CORDEL is a cinematic, narrative-driven 3D engine project founded on the
[Ren'Py repository](https://github.com/renpy/renpy). Phase 0 establishes an audited
codebase, a reproducible development baseline, and an engineering direction.
Phase 1.1 adds an isolated Ren'Py-backed 3D viewport reference experiment with
executed software-rendering evidence. A production CORDEL world runtime remains
proposed.

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

The recommended direction is a native real-time runtime hosting a carefully
isolated narrative adapter. That is a proposal to validate, not an implemented
integration or a final choice of renderer, physics library, ECS, or editor.

## Development and design

Start with [BOOTSTRAP](docs/cordel/BOOTSTRAP.md) for verified commands, SDK checksum,
environment limits, and test results. Other project documents:

- [Architecture audit](docs/cordel/ARCHITECTURE_AUDIT.md): source evidence and reuse decisions.
- [Architecture proposal](docs/cordel/ARCHITECTURE.md): ownership, interfaces, and technology trade-offs.
- [Roadmap](docs/cordel/ROADMAP.md): independently testable milestones and Phase 1 task.
- [Upstream policy](docs/cordel/UPSTREAM.md): provenance, remotes, licenses, and synchronization.
- [Project metadata](cordel/project.toml): machine-readable identity and foundation commit.
- [Phase 1.1 findings](docs/cordel/PHASE_1_1_VIEWPORT.md): implementation, measurements and runtime limits.

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
