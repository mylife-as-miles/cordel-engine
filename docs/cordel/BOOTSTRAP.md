# Development bootstrap and executed baseline

Recorded 2026-10-07 UTC against foundation
`68fdaef917919e330c90765dd69668fee1cb9654`. All baseline runs preceded CORDEL
file additions. No privileged installation script or package installation was run.

## Host and requirements

The host is Debian GNU/Linux 13.6, Linux x86_64, glibc 2.41. Git 2.52.0,
GCC/G++, make, pkg-config, curl, tar, and uv 0.12.19 are available. The selected Python is
3.12.14. No desktop display or Xvfb session is configured. Mesa 25.0.7 with
llvmpipe (LLVM 19.1.7) provides software OpenGL 4.5 through SDL's offscreen driver.
This proves functional headless drawing, not hardware performance or visible GUI use.

Upstream `pyproject.toml` requires Python `>=3.12.8,<3.13`; choosing Debian's default
Python without checking its version is insufficient. `uv sync` creates `.venv`
and installs Python packages; it does not produce the engine's native extensions.

Native build requirements are determined by `setup.py`, not just the README:
`sdl3`, `sdl3-image`, `libpng`, `libavformat`, `libavcodec`, `libavutil`,
`libswresample`, `libswscale`, `freetype2`, `harfbuzz`, `fribidi`, and `assimp`
must be discoverable through pkg-config, along with matching Python headers,
C/C++ compiler, Cython, and setuptools. None of those pkg-config packages was
available on this host. Optional Live2D also needs a separately licensed Cubism SDK.

**Setup drift:** `README.rst` still recommends SDL2 development packages, while
`setup.py`, `src/core.c`, `renpy/pygame/`, and `.devcontainer/Dockerfile` use SDL3.
The devcontainer targets Ubuntu 26.04. Its create/start scripts run privileged
commands and modify device permissions; they were inspected but not executed.
`scripts/run-headless.sh` is another upstream workflow: it starts Weston or Xvfb
and calls `run.sh`, so it still requires native compilation. Executing it here
fails with "No Wayland (weston) or X11 (xvfb-run/Xvfb) virtual display server found".
The verified offscreen workflow below needs neither virtual display server.

## Repository and Python setup

The original clone and Git operations were:

```sh
git clone https://github.com/renpy/renpy.git cordel-engine
cd cordel-engine
git remote rename origin upstream
git switch -c cordel/bootstrap
git rev-parse HEAD
uv sync
```

These commands record the original Phase 0 bootstrap. The owner subsequently
requested fresh CORDEL history; the published project now uses `main` and
`origin` at `https://github.com/mylife-as-miles/cordel-engine.git`. To obtain the
current project, use:

```sh
git clone https://github.com/mylife-as-miles/cordel-engine.git cordel-engine
cd cordel-engine
git remote add upstream https://github.com/renpy/renpy.git
```

The original foundation hash is source provenance, not local Git ancestry. Recorded
baseline logs still contain the old bootstrap branch name; no runtime source was
changed by Git reinitialization. See [UPSTREAM](UPSTREAM.md) for synchronization.

For an existing checkout, inspect `git status`, branch, remotes, and HEAD first;
do not rerun branch creation or overwrite a directory. To reproduce this Phase 0
Python resolution on a fresh CORDEL checkout:

```sh
cp docs/cordel/python-dependencies.lock uv.lock
uv sync --locked --python 3.12
mkdir -p tmp
.venv/bin/python -m unittest unittests.test_setuplib unittests.test_asynctask unittests.test_ecsign -v
```

The checked-in lock snapshot contains public registry URLs and artifact hashes.
Upstream ignores the root `uv.lock`; the snapshot is deliberately kept in CORDEL
documentation. Regenerate it explicitly when upstream requirements change.
The three-module command above executed **29 tests, all passing**. The build-lock
tests need `tmp` to exist.

## Verified nightly-native workflow

This follows upstream's supported prebuilt-native approach. SDK files stay outside
the repository; only ignored development symlinks point to them. The pinned SDK's
Python is **3.12.8** and its native modules are compiled into `librenpython.so`.

```sh
mkdir -p ../scratch/cordel-runtime
curl -fL 'https://nightly.renpy.org/8.6.0.26100601+nightly.dirty/renpy-8.6.0.26100601+nightly.dirty-sdk.tar.bz2' \
  -o ../scratch/cordel-runtime/renpy-sdk.tar.bz2
printf '%s\n' '75c926cfe3c7f9b6e6ff56adcc7adc79d207f035c019f5724e752bdc347a8583  ../scratch/cordel-runtime/renpy-sdk.tar.bz2' \
  | sha256sum -c -
tar -xjf ../scratch/cordel-runtime/renpy-sdk.tar.bz2 -C ../scratch/cordel-runtime
./after_checkout.sh "$(realpath ../scratch/cordel-runtime/renpy-8.6.0.26100601+nightly.dirty-sdk)"
lib/py3-linux-x86_64/python -X utf8 renpy.py the_question lint
```

`after_checkout.sh` exits 0 but emits a warning about absent
`templates/english/README.html`. It also creates dangling sample README links
to absent `help.html`; only those two newly created untracked links were removed
after the baseline. This warning does not prevent runtime setup. Do not rerun the
script blindly when its links already exist.

**Use the explicit bundled Python command** to execute checkout source. The
nightly's `renpy.sh` resolves its symlink to the SDK directory; invoking that link
can select the SDK's own entry point. Passing `renpy.py` to bundled Python avoids
claiming that SDK source is checkout source. Never copy SDK source over the clone.

Headless smoke test, executed successfully (exit 0):

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  timeout 30s lib/py3-linux-x86_64/python -X utf8 renpy.py \
  the_question test bad_end good_end --report-detailed
```

Result: **2 passed, 1 intentionally unselected/skipped**, 3 hooks passed, 2
assertions passed. Initialization, drawing, dialogue progression, both branching
endings, and return to the main menu were exercised. Audio uses a dummy output;
audible playback and save/load round trips have not been verified. The log records
the checkout as `Ren'Py 8.6.0.26100603+unofficial.dirty.cordel/bootstrap`, with
Mesa llvmpipe, a 1280×720 drawable, and GLSL 330.

On a desktop with display and audio configured, the corresponding normal commands
are `lib/py3-linux-x86_64/python -X utf8 renpy.py the_question` and
`lib/py3-linux-x86_64/python -X utf8 renpy.py launcher`. These interactive desktop
commands have not been verified in this headless session.

## Unit tests with bundled native modules

The SDK omits `unittest` and uses launcher-controlled module paths; `PYTHONPATH`
did not supply the missing standard library here. The following uses the host's
matching Python 3.12 standard-library sources for the test runner only, without
editing the SDK or upstream code:

```sh
CORDEL_TEST_STDLIB=$(.venv/bin/python -c 'import sysconfig; print(sysconfig.get_path("stdlib"))')
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  lib/py3-linux-x86_64/python -c '
import sys
sys.path.append(sys.argv.pop(1))
import runpy
sys.argv = ["unittest", "-v",
    "unittests.test_asynctask", "unittests.test_ecsign",
    "unittests.test_particle", "unittests.test_persistent_tasks",
    "unittests.test_prediction", "unittests.test_savelocation_background",
    "unittests.test_surface_deallocation", "unittests.test_test_input",
    "unittests.test_test_window", "unittests.test_warp"]
runpy.run_module("unittest", run_name="__main__")
' "$CORDEL_TEST_STDLIB"
```

Result: **89 tests passed**. This selection excludes SDK-incompatible audio-native
tests, build tooling tests (already covered in `.venv`), and the initialization
module whose child process lacks `unittest`. It is not full-suite success. The
host and SDK patch versions differ; do not use this test-runner workaround as
shipping Python packaging. The 29-test and 89-test sets overlap, so their counts
must not be added as unique coverage.

## Failures and limits retained

| Executed check | Exit/result | Cause or limitation |
| --- | --- | --- |
| `uv sync` | 0 | Python dependency setup succeeded |
| `./run.sh --build` | 1 | Cython generation completed, then missing pkg-config `sdl3`; other native development packages also absent |
| `scripts/run-headless.sh the_question test` | 1 | Neither Weston nor Xvfb wrapper/server is installed; native source build would still be required |
| `.venv/bin/python -X utf8 renpy.py the_question` | 1 | Missing `renpy.pygame.error` extension; exception reporting also raises `NameError: PyExpr` during partial initialization |
| `.venv/bin/python -m unittest discover -s unittests -v` | 1; 40 reported, 1 failure, 9 errors | Native imports unavailable, including child-process initialization; failed imports count as synthetic tests |
| Nightly-backed sample lint | 0 | Script loads; this is static validation, not gameplay coverage |
| Dummy-video sample `test` under 45-second timeout | 124 | Dummy driver cannot create GL; software fallback starts but no cases execute before termination. Its printed PASSED status is not success |
| Offscreen sample full `test --report-detailed` | 1; 2 passed, 1 failed | History-screen click passes a tuple state as mouse button to `press_mouse`; comparison with integer fails |
| SDK `python -m unittest` with `PYTHONPATH` | 1 | SDK strips unittest and ignores that path injection |
| SDK full discovery with explicit host stdlib/site paths | 1; 110 reported, 1 failure, 19 errors | 18 audio tests try `ctypes.CDLL('built-in')`; setup import needs unavailable `sysconfig.get_platform`; child interpreter lacks unittest |
| Selected SDK-backed unit suite | 0; 89 passed | Selection and runner workaround described above |
| Offscreen ending smoke | 0; 2 passed | Clean focused runtime baseline |

The full sample failure is in checkout Python code:
`renpy/test/testast.py:896` forwards selector state to `click_mouse`, and
`renpy/test/testmouse.py:73` expects an integer button. This is pre-existing in the
unmodified foundation. It was not patched as part of identity/bootstrap work.
Its exact correction and regression test belong in a separate reviewed change.

## Phase 1.1 baseline regression replay

On 2026-10-07 the isolated viewport feature replayed the same selected workflows,
using the existing `.venv`, pinned SDK symlink and host-stdlib runner workaround.
No new native dependencies, privileged operations or upstream runtime changes:

```sh
bash examples/cordel_viewport/tools/check_baseline.sh tmp/phase1-accepted
```

Exit **0**: original source selection **29 passed**, SDK selection **89 passed**,
The Question's two selected endings **2 passed** (history case unselected), and
viewport lint exited 0 with generic conflicting-properties advice. The script
contains the exact commands above, plus the new-project lint. Known full-suite
failures remain unchanged and excluded; the unit selections still overlap.
Transcripts are in [Phase 1.1 evidence](evidence/phase1_1/README.md).

The new viewport runs on this same baseline. Its verification wrapper executes
31 pure helper tests, 83 software runtime checks and the normal viewport UI test:

```sh
bash examples/cordel_viewport/tools/run_offscreen.sh tmp/my-viewport-run
```

Use a new output directory. See [the Phase 1.1 report](PHASE_1_1_VIEWPORT.md) for
offscreen limits, real measurements and open desktop/device/resource checks.

Captured execution records (trailing whitespace normalized) are in
[evidence/](evidence/), with commands indexed in
[evidence/README.md](evidence/README.md). Original session logs also remain under
`/workspace/scratch/cordel-*.log`. The large SDK and `.venv` are development
artifacts and are not committed. Future nightly availability is not guaranteed;
retain the checksum-verified archive externally for repeatability.

## Native compilation remediation (not executed)

The [Phase 1.2 native host](../../native/phase1_host/README.md) now builds and tests
independently using project-local CMake 3.31.6, pinned SDL 3.4.8 and vendored cgltf.
Its repeatable software command is:

```sh
bash native/phase1_host/tools/run_offscreen.sh tmp/my-native-run
```

That smaller experiment uses bundled GL/EGL declarations and an offscreen-only
SDL build. It does not compile Ren'Py's native modules or supply the missing
Assimp/FFmpeg/font/image development dependencies listed below. Phase 1.2 also
replays the existing Phase 1.1 and baseline wrappers successfully; original
full-suite failures remain unchanged. No root or host-wide installation was used.

Use an isolated Linux development container/workstation with Python 3.12 and SDL3
development packages. Compare `.devcontainer/Dockerfile` and the source requirements
above; the existing container configuration assumes desktop GPU/display mounts
and is not a verified portable headless image.

On an appropriate Ubuntu release, an administrator can provision:

```sh
sudo apt install build-essential pkg-config python3-dev libassimp-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev \
  libharfbuzz-dev libfreetype6-dev libfribidi-dev libsdl3-dev libsdl3-image-dev \
  libpng-dev libjpeg-dev
```

This is remediation guidance, not a command executed here. Ensure Python headers
match the 3.12 interpreter used by uv; a default 3.13 header package is insufficient.
If the distribution does not provide SDL3/SDL3_image, build pinned versions into
a user-owned prefix and set `PKG_CONFIG_PATH`, compiler/linker paths, and runtime
library paths for that prefix. Do not substitute SDL2 for SDL3.

Then check each pkg-config dependency, restore the pinned Python lock, and run:

```sh
uv sync --locked --python 3.12
./run.sh --build
.venv/bin/python -m unittest discover -s unittests -v
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  ./run.sh the_question test --report-detailed
```

`run.sh` uses `nproc` for build parallelism; inspect resource limits before using
it on constrained hosts. Reconcile known test failures separately. A full native
build, hardware graphics run, audible audio, interactive launcher, all sample
projects, mobile/web packaging, and full game save/load remain unverified.
