# Ren'Py architecture audit

Scope: actual source at `68fdaef917919e330c90765dd69668fee1cb9654`, inspected
2026-10-07. Paths below are relative to repository root. Line references apply
to this foundation. Findings describe source structure; runtime evidence and
coverage limits are in [BOOTSTRAP](BOOTSTRAP.md).

## Initialization and lifecycle

The desktop source entry point is `renpy.py:284` (`main`), also exposed through
the `main.py` symlink. It determines the engine base, imports `renpy.bootstrap`,
sets `renpy.__main__`, and calls `bootstrap`. It does not embed/initialize CPython
itself. `run.sh` executes a virtual-environment Python after compiling Cython/C
extensions; the SDK uses renpy-build's native launcher and `librenpython.so`.
The inspected external `runtime/librenpython.c` at renpy-build commit
`16129650549f6ccef43d9eab9292701691844ca6` establishes interpreter paths,
`PyConfig`, UTF-8 mode, calls `Py_InitializeFromConfig`, and runs Python. This
external packaging source is distinct from the cloned engine repository.

`renpy/bootstrap.py:198` configures the engine base, optional `environment.txt`,
arguments and project directories, imports `renpy.pygame` and `_renpy`, calls
`renpy.import_all()` (`renpy/__init__.py:366`), installs the game importer, then
enters `renpy.main.main()` with restart/error/exit handling. Native extension
lookup support appears in `renpy/__init__.py:33`; packaged built-in modules and
source-built extensions are different deployment modes.

`renpy.py:36` selects the project game directory; `renpy/arguments.py` parses
commands. `renpy/main.py:311` initializes config and screen parsing, indexes
files, loads extensions and `.rpy` scripts, creates stores and styles, executes
ordered init code, prepares preferences/persistence/save locations, compiles ATL,
initializes translation, and constructs the display `Interface`. Game
`options.rpy`, screens, other init blocks, `renpy/config.py`, platform variants,
CLI flags and environment settings participate in startup configuration.

`renpy/main.py:57` resets stores and selects `_start` or `start`, then
`renpy/execution.py:1043` runs a script context. `Context.run` around line 590
walks AST nodes and executes statements, handling jumps/calls and rollback
boundaries. Say/menu/pause statements enter the display interaction loop.
`Interface.interact` at `renpy/display/core.py:2122` delegates to `interact_core`
at line 2349. That loop processes events, redraws, transitions, predictions,
audio, and cooperative tasks until an interaction result or restart occurs.

**Implication:** there are nested script and display execution loops, global
`renpy.game` state, restart restoration, and Python store backups. This is not a
standalone externally pumped narrative coroutine. A host must account for these
ownership assumptions before treating Ren'Py as a library.

## Rendering and GPU resources

| Area | Source evidence | Consequence |
| --- | --- | --- |
| Renderer selection | `renpy/display/core.py:1086`, `get_draw_constructors` | `gl2`, `gles2`, Windows `angle2`, desktop `sw` fallback; all GPU choices share GL2Draw |
| Graphics contexts | `renpy/gl2/gl2draw.pyx:333`, `gl_context_versions` | Requests GL 3.3 compatibility/core then GL 2.0; GLES requests ES 3.0. Names like `gles2` do not indicate exact minimum context version |
| Renderer surface | `renpy/display/core.py:302`, `Renderer` | Texture, draw-screen, screenshot, resizing and coordinate mapping operations; useful seam but typed to Ren'Py render trees rather than arbitrary world draw packets |
| Displayables | `renpy/display/displayable.py:130,405,420,465` | `render(width,height,st,at)`, `event`, `visit` form a retained UI/display tree with prediction and interaction semantics |
| Render trees/cache | `renpy/display/render.pyx`; `core.py:1274` | Displayables produce cached Render trees; per-frame screens, compositing and redraw invalidation determine work |
| Scheduling | `core.py:2835,2851,3024,3081`; `gl2draw.pyx:1113,1126` | Demand-driven redraw, vsync, event polling/waiting and power saving; can force redraw but no general fixed simulation clock |
| Shaders | `renpy/gl2/gl2shadercache.py:350,840,956`; `gl2shader.pyx`; `gl2uniform.pyx` | Shader parts, dialect translation, linked shader caching, predictions, GL compilation/uniform binding; custom shaders are real functionality |
| Textures | `renpy/gl2/gl2texture.pyx:79,101,235,505` | Lazy uploads, allocation tracking, weak load queue, per-frame GL deletion free list, mipmaps and filtering |
| Render targets | `gl2draw.pyx:974,1395`; `gl2texture.pyx:225,421` | FBOs, color/depth renderbuffers, render-to-texture and compositing; lifetimes tied to GL context/display |
| Image memory | `renpy/display/im.py:173`; `renpy/config.py:100` | Prediction/preloading and image cache budget (400 MB default), not a unified world streaming/residency manager |
| Software fallback | `renpy/display/swdraw.py:337,649` | Surface blits and supported transitions; not an equivalent custom-shader or mesh renderer |

### Existing 3D support must be credited

`renpy/display/transform.py` and `renpy/display/accelerator.pyx` implement ATL
transforms, perspective, 3D matrices, camera/layer operations and depth-related
properties. `renpy/display/model.py:52` supplies the `Model` displayable with
meshes, textures, shader parts and uniforms. `renpy/gl2/gl2mesh3.pyx:31` and
`gl2mesh.pyx` support 3D vertex data and triangle meshes. This is more than flat
sprite drawing.

`renpy/gl2/assimp.pyx:351` uses Assimp with Ren'Py IO to load node transforms,
static meshes, normals/tangents, material uniforms and embedded/external textures.
`GLTFModel` at line 722 renders glTF models as displayables; it requires a shader,
has zero size for 2D layout, and documents `gl_depth True` for multiple models.
Its docstring explicitly says **no model culling**. Importing metallic/roughness
texture data is not proof of a complete physically based renderer. The example
in `sphinx/source/model.rst:673` is a single-light diffuse/specular shader, described
at line 711 as experimental.

The inspected loader creates mesh/blit records and releases the Assimp scene.
It does not import a skeletal animation runtime or process bone/animation data
in that load path. ATL animates display properties; optional Live2D code animates
2D character rigs. `renpy/gl2/gl2physics.pyx:382` contains `PendulumPhysics` for
rig parameters/strands, not a world rigid-body collision engine.

**Reuse:** UI/text composition, shader experimentation, glTF/static geometry
prototype, texture and resource-lifetime patterns, display resizing and narrative
overlays. **Extend only after measurement:** a viewport displayable and continuous
frame scheduling. **Replace/isolate for production 3D:** world visibility/culling,
scene render extraction, PBR/lighting/shadow passes, animation skinning, scalable
resource streaming and backend abstraction. Existing FBOs are not a render graph,
and the GL2 renderer seam does not make Vulkan/D3D12 drop-in replacements.

## Input, events and continuous updates

`renpy/pygame/` is an upstream Cython SDL3 wrapper exposing a pygame-compatible
API; this revision does not depend on installing generic pygame as a substitute.
`event.pyx` translates/polls SDL events, `key.pyx:68` offers held-key state,
`mouse.pyx` provides buttons/position, and `event.pyx:619` controls input grab.
`core.py:1585-1657` implements poll/peek/wait and coordinate translation; around
line 3291 it dispatches to `root_widget.event`. Focus and keymap interpretation
live in `renpy/display/focus.py` and `renpy/display/behavior.py`.

`renpy/display/controller.py` loads mappings, manages hotplug/repeats and translates
gamepad controls. Its axis handler at line 254 uses thresholds to emit positive,
negative and zero control transitions. `renpy/pygame/controller.pyx:187` can read
raw axes, and `config.pass_controller_events` can preserve raw events. Default
UI-style digital mapping loses analog magnitude needed for third-person movement.

`core.py:3081` drives periodic callbacks, audio and controller repeat events;
`draw_screen` handles screen per-frame work. `renpy/asynctask.py` is a cooperative,
budgeted generator runner; its docstring explicitly excludes timers/futures/event
loop support. It is useful for incremental background work, not deterministic
physics scheduling. `renpy/display/minigame.py` raises "Minigame is no longer
implemented" and is not a usable gameplay extension point.

**Required future work:** held-state action snapshots per simulation tick, raw
analog normalization/deadzones, relative mouse/cursor capture, input ownership
between UI/camera/gameplay/cinematics, focus-loss clearing, fixed updates separate
from render rate, and bounded catch-up. Disabling power saving alone is insufficient.

## Narrative, state and serialization

- `renpy/lexer.py`, `renpy/parser.py`, and `renpy/script.py:96` load `.rpy`/compiled
  scripts, generate AST, maintain label lookup and bytecode caches. `renpy/ast.py`
  implements Say (876), Label (1135), Python (1194), Call (1650), Menu (1769),
  Jump (1957), While (2043), If (2097), Define (2468), and Default (2566).
- `renpy/character.py:1018` (`ADVCharacter`) manages dialogue presentation and
  callbacks; `display_say` at 561 interacts with screens, reveal timing, voice,
  text tags and user input. Narrative Character state is not a physical character
  controller or skeletal character component.
- `renpy/python.py` creates store namespaces, compiles Python and executes
  bytecode. `renpy/revertable.py` and `renpy/rollback.py:457` track mutable narrative
  state/checkpoints. `renpy/execution.py` couples AST execution to rollback and
  contexts. Arbitrary game Python can have external side effects.
- `renpy/loadsave.py:135` freezes the rollback log/store roots and serializes
  `(roots, log)`, screenshot and metadata to a SaveRecord. `load` at 631 validates
  save tokens and unfreezes into `_after_load`. `renpy/savelocation.py`,
  `renpy/savetoken.py`, and `renpy/persistent.py` handle storage, signature/token
  checks and persistent preferences/progress.
- `renpy/translation/__init__.py` tracks dialogue identifiers and string
  translations; `renpy/translation/dialogue.py` and launcher translation tools
  support authoring workflows. Preserve translation IDs when migrating scripts.
- `renpy/atl.py`, interaction timeouts, screen timers and narrative statements
  schedule presentation/events; `renpy/audio/audio.py` maintains channels and
  queues with periodic work and `renpy/audio/music.py:550` supplies stereo pan.
  These do not supply world-space acoustic propagation or a world timeline.

**Integration opportunity:** labels and custom statements can emit typed gameplay
commands; character callbacks can synchronize voice/dialogue cues; gameplay events
can resolve narrative waits. **Hard boundary:** saves contain Python objects,
execution contexts and rollback history, not a native scene snapshot. GPU handles,
physics pointers and native objects must stay out of revertable stores. Define
explicit scene checkpoints and stable IDs; choose compensation or disable rollback
across irreversible gameplay effects. Narrative extraction requires adaptation,
not copying a parser and claiming Ren'Py compatibility.

## Tools, assets and distribution

`launcher/` is itself a Ren'Py application. `launcher/game/project.rpy:61,564`
defines Project/ProjectManager, scanning and launch actions; `new_project.rpy`,
`front_page.rpy`, `navigation.rpy`, and `editor.rpy` provide project creation,
lint/recompile/test actions and external script editor integration. The main engine
also has `renpy/editor.py`, tracebacks and logging, developer screens/profiling,
`renpy/common/00console.rpy` (console), and `00director.rpy` (visual staging tools).
These offer project management and narrative authoring patterns, not a 3D editor.

`renpy/loader.py` indexes filesystem/RPA assets and manages project/common search
paths. Font rendering uses FreeType, HarfBuzz and FriBidi (`renpy/text/`); audio/video
decoding uses FFmpeg (`src/ffmedia.c`, `renpy/audio/renpysound.pyx`). `GLTFModel`
is a runtime importer; there is no complete CORDEL offline mesh/animation cooker.

`setup.py` and `scripts/setuplib.py` generate/build native modules.
`renpy/common/00build.rpy`, `launcher/game/distribute.rpy:505`, `package_formats.rpy`,
`archiver.rpy`, `distribute.py` and `scripts/distribute_all.sh` build game/SDK packages.
`runtime/` contains platform bindings; the separate renpy-build repository owns
cross-platform native toolchains and launchers. `scripts/README.rst` still mentions
`autobuild.sh`, which is absent from this revision. Mobile/web packaging carries its
own runtime/deployment assumptions. Desktop reuse does not establish console support.

`unittests/` uses unittest; many modules call `unittests/renpy_test_support.py` to
import the compiled engine. `renpy/test/` implements the in-engine testcase DSL;
sample suites exist in `the_question/`, `tutorial/`, `launcher/`, `gui/`, and
`testcases/`. See `sphinx/source/testcases.rst` for CLI execution.
`scripts/run-headless.sh` invokes the source-building `run.sh` after setting up
Weston or Xvfb; neither virtual-display dependency is present here. The inspected
`.github/` contains metadata/templates/dependabot configuration, not a complete
automated engine build/test workflow. CORDEL needs its own measured CI baseline.

## Disposition and confidence

| Subsystem | Phase 0 disposition | Long-term recommendation |
| --- | --- | --- |
| Script/dialogue/branching/localization | Preserve unchanged | Adapt behind narrative commands, retain tested behavior and IDs |
| Saves/rollback/persistence | Preserve for existing games | Isolate from native world; coordinate explicit versioned checkpoints |
| UI/fonts/2D presentation | Preserve | Reuse as overlay where integration cost permits |
| Audio queues/music/voice | Preserve | Bridge timing and ownership; add separate spatial-audio capability |
| SDL input/window/platform code | Preserve | Evaluate reuse with action snapshots and host ownership |
| GL renderer/static glTF | Preserve | Use for comparison spike; replace/extend through a deliberate boundary |
| ATL/Live2D/pendulum rigs | Preserve | Reuse narrative presentation; use dedicated skeletal/world systems |
| Launcher/build tools | Preserve | Reuse project/packaging knowledge; new scene/cinematic authoring tools |

The audit supports a host-runtime proposal but does not establish its integration
cost through implementation. Baseline execution covers narrative rendering and
branching, not glTF rendering, scene physics, character locomotion, hardware
performance, spatial audio, editor tooling or complete save/load compatibility.
Those are future verification gates, not current engine features.
