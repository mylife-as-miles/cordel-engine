# Phase 0 execution records

These are captured stdout/stderr from the unmodified engine baseline on
2026-10-07 UTC. Initial baseline runs preceded CORDEL additions; the headless
wrapper probe and environment capture were added during documentation review.
Engine source remained unchanged throughout. Trailing whitespace is normalized
in these committed text copies; original session logs remain in `/workspace/scratch`.
Logs are retained
including failures; console strings such as PASSED do not override a timeout or
the absence of executed test cases. Read [BOOTSTRAP](../BOOTSTRAP.md) for interpretation.

All commands used `/workspace/cordel-engine` as working directory unless stated.
The bundled interpreter came from SDK 8.6.0.26100601+nightly.dirty via the ignored
`lib` symlink. Host stdlib path was
`/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12`.

| Record | Command/check | Actual exit |
| --- | --- | --- |
| [uv-sync.txt](uv-sync.txt) | `uv sync` | 0 |
| [source-build.txt](source-build.txt) | `./run.sh --build` | 1 |
| [headless-wrapper.txt](headless-wrapper.txt) | `scripts/run-headless.sh the_question test` | 1 |
| [source-launch.txt](source-launch.txt) | `.venv/bin/python -X utf8 renpy.py the_question` | 1 |
| [source-tests.txt](source-tests.txt) | `.venv/bin/python -m unittest discover -s unittests -v` | 1 |
| [pure-tests.txt](pure-tests.txt) | `.venv/bin/python -m unittest unittests.test_setuplib unittests.test_asynctask unittests.test_ecsign -v` | 0 |
| [after-checkout.txt](after-checkout.txt) | `./after_checkout.sh /workspace/scratch/cordel-runtime/renpy-8.6.0.26100601+nightly.dirty-sdk` | 0 with warning |
| [nightly-lint.txt](nightly-lint.txt) | `lib/py3-linux-x86_64/python -X utf8 renpy.py the_question lint` | 0 |
| [nightly-sample-test.txt](nightly-sample-test.txt) | `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy RENPY_RENDERER=sw timeout 45s lib/py3-linux-x86_64/python -X utf8 renpy.py the_question test` | 124 |
| [offscreen-test.txt](offscreen-test.txt) | `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 timeout 55s lib/py3-linux-x86_64/python -X utf8 renpy.py the_question test --report-detailed` | 1 |
| [offscreen-smoke.txt](offscreen-smoke.txt) | Same offscreen environment, `timeout 30s lib/py3-linux-x86_64/python -X utf8 renpy.py the_question test bad_end good_end --report-detailed` | 0 |
| [offscreen-renderer.txt](offscreen-renderer.txt) | `the_question/log.txt` from the passing focused smoke, including GL capabilities | Runtime log |
| [nightly-unit-tests.txt](nightly-unit-tests.txt) | Bundled Python full discovery with explicit host stdlib and `.venv` site paths (below) | 1 |
| [nightly-supported-tests.txt](nightly-supported-tests.txt) | Bundled Python selected suite with explicit host stdlib, 10 modules listed in BOOTSTRAP | 0 |
| [environment.txt](environment.txt) | OS, toolchain, provenance and individual pkg-config probes | Mixed probes; missing packages explicitly recorded |

Full discovery invocation used for `nightly-unit-tests.txt`:

```sh
lib/py3-linux-x86_64/python -c '
import sys
sys.path.extend([
    "/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12",
    "/workspace/cordel-engine/.venv/lib/python3.12/site-packages"])
import runpy
sys.argv = ["unittest", "discover", "-s", "unittests", "-v"]
runpy.run_module("unittest", run_name="__main__")
'
```

An earlier bare `python -m unittest` SDK attempt failed with `No module named
unittest`, including when `PYTHONPATH` supplied host stdlib/site paths. The explicit
`sys.path` test-runner workaround above then exposed the documented built-in audio,
sysconfig and child-process limitations. It does not patch or mock engine behavior.

No complete native build, desktop-visible launcher, hardware GPU performance,
audible audio, glTF scene rendering, or end-to-end save/load was verified. A passing
unit test for save-location or prediction behavior is narrower than full runtime
system verification. Only the selected sample ending smoke is green; the history
test and unselected suites remain open.
