#!/usr/bin/env bash
# Replay BOOTSTRAP.md's focused baseline; excludes its known pre-existing failures.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$cordel_repo"
cordel_output=${1:-"$cordel_repo/tmp/cordel-baseline-$(date -u +%Y%m%dT%H%M%S)"}
mkdir -p "$cordel_output" tmp
cordel_output=$(cd "$cordel_output" && pwd)
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
.venv/bin/python -m unittest unittests.test_setuplib unittests.test_asynctask unittests.test_ecsign -v \
    2>&1 | tee "$cordel_output/baseline-source-tests.txt"
cordel_test_stdlib=$(.venv/bin/python -c 'import sysconfig; print(sysconfig.get_path("stdlib"))')
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
' "$cordel_test_stdlib" 2>&1 | tee "$cordel_output/baseline-sdk-tests.txt"
timeout 30s lib/py3-linux-x86_64/python -X utf8 renpy.py \
    the_question test bad_end good_end --report-detailed 2>&1 | tee "$cordel_output/baseline-question-tests.txt"
lib/py3-linux-x86_64/python -X utf8 renpy.py examples/cordel_viewport lint \
    2>&1 | tee "$cordel_output/viewport-lint.txt"
