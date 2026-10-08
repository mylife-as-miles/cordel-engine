#!/usr/bin/env bash
# Copyright (c) 2026 CORDEL contributors. MIT.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$cordel_repo"
cordel_output=${1:-"tmp/phase1_3-$(date -u +%Y%m%dT%H%M%S)"}
mkdir -p "$cordel_output"
cordel_output=$(cd "$cordel_output" && pwd)
if [[ -e "$cordel_output/scenario-results.json" ]]; then
    printf 'Use a new output directory: %s\n' "$cordel_output" >&2
    exit 1
fi
if [[ ! -x .venv/bin/cmake || ! -x lib/py3-linux-x86_64/python ]]; then
    printf 'Use the pinned Python/SDK/CMake environment in docs/cordel/BOOTSTRAP.md.\n' >&2
    exit 1
fi
export PATH="$cordel_repo/.venv/bin:$PATH"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
cmake -S native/phase1_host -B build/phase1-host -DCORDEL_OFFSCREEN_ONLY=ON > "$cordel_output/configure.txt" 2>&1
cmake --build build/phase1-host -j 4 > "$cordel_output/build.txt" 2>&1
ctest --test-dir build/phase1-host --output-on-failure -V 2>&1 | tee "$cordel_output/native-tests.txt"
timeout 45s .venv/bin/python -m unittest discover -s narrative/phase1_adapter/tests -v \
    2>&1 | tee "$cordel_output/worker-tests.txt"
timeout 90s build/phase1-host/cordel-native-host --narrative-test --output "$cordel_output" \
    > "$cordel_output/native-console.txt" 2>&1
for cordel_branch in continue stay; do
    CORDEL_NARRATIVE_REFERENCE=1 CORDEL_REFERENCE_CHOICE="$cordel_branch" \
        CORDEL_REFERENCE_OUTPUT="$cordel_output/reference-$cordel_branch.json" \
        timeout 30s lib/py3-linux-x86_64/python renpy.py narrative/phase1_adapter/fixtures \
        > "$cordel_output/reference-$cordel_branch.txt" 2>&1
done
.venv/bin/python narrative/phase1_adapter/tools/collect.py "$cordel_output"
