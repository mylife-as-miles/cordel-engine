#!/usr/bin/env bash
# Repeatable native software gate; no root or host-wide configuration required.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$cordel_repo"
cordel_output=${1:-"tmp/native-$(date -u +%Y%m%dT%H%M%S)"}
mkdir -p "$cordel_output"
cordel_output=$(cd "$cordel_output" && pwd)
if [[ -e "$cordel_output/self-test.json" ]]; then
    printf 'Use a new evidence directory: %s\n' "$cordel_output" >&2
    exit 1
fi
# BOOTSTRAP's .venv is retained; add the pinned build tool there if absent.
if [[ ! -x .venv/bin/cmake ]]; then
    uv pip install --python .venv/bin/python cmake==3.31.6
fi
export PATH="$cordel_repo/.venv/bin:$PATH"
cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON \
    > "$cordel_output/configure.txt" 2>&1
cmake --build build/phase1-host -j "${CORDEL_BUILD_JOBS:-4}" \
    > "$cordel_output/build.txt" 2>&1
ctest --test-dir build/phase1-host --output-on-failure -V 2>&1 | tee "$cordel_output/native-tests.txt"
export SDL_VIDEODRIVER=offscreen LIBGL_ALWAYS_SOFTWARE=1
timeout 30s build/phase1-host/cordel-native-host --self-test --output "$cordel_output" \
    2>&1 | tee "$cordel_output/self-test.txt"
.venv/bin/python native/phase1_host/tools/convert_images.py "$cordel_output"
printf 'Native offscreen gate passed: %s\n' "$cordel_output"
