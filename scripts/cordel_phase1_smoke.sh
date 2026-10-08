#!/usr/bin/env bash
# Copyright (c) 2026 CORDEL contributors. MIT.
# Selected Linux software architecture gate; full milestone regressions are separate.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$cordel_repo"
if (( $# > 1 )); then
    printf 'Usage: bash scripts/cordel_phase1_smoke.sh [new-output-dir]\n' >&2
    exit 2
fi
cordel_output=${1:-"tmp/phase1-smoke-$(date -u +%Y%m%dT%H%M%S)"}
if [[ -e "$cordel_output" ]] && { [[ ! -d "$cordel_output" ]] ||
    [[ -n "$(find "$cordel_output" -mindepth 1 -maxdepth 1 -print -quit)" ]]; }; then
    printf 'Use a new or empty output directory: %s\n' "$cordel_output" >&2
    exit 2
fi
if [[ ! -x .venv/bin/cmake || ! -x .venv/bin/python || ! -x lib/py3-linux-x86_64/python ]]; then
    printf 'Configure the pinned Python/SDK/CMake environment in docs/cordel/BOOTSTRAP.md first.\n' >&2
    exit 1
fi
command -v timeout >/dev/null
mkdir -p "$cordel_output"
cordel_output=$(cd "$cordel_output" && pwd)
export PATH="$cordel_repo/.venv/bin:$PATH"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1

# One build/test wrapper proves real worker ownership plus both ordinary displayed
# Ren'Py branches. Outer timeout covers configure/build and nested test timeouts.
# GNU timeout also terminates the command process group; kill-after bounds hangs.
timeout --kill-after=5s 300s bash narrative/phase1_adapter/tools/run_offscreen.sh \
    "$cordel_output/narrative" 2>&1 | tee "$cordel_output/narrative-gate.txt"

# Reuse that binary, avoiding another configure/build/unit-suite invocation.
timeout --kill-after=5s 30s build/phase1-host/cordel-native-host --self-test \
    --output "$cordel_output/native" 2>&1 | tee "$cordel_output/native-gate.txt"

.venv/bin/python - "$cordel_output" <<'PY'
import hashlib
import json
from pathlib import Path
import subprocess
import sys

output = Path(sys.argv[1])
def read(relative):
    return json.loads((output / relative).read_text())

story = read("narrative/scenario-results.json")
native = read("native/self-test.json")
reference = read("narrative/reference-comparison.json")
lifecycle = read("native/lifecycle.json")
if not (story["success"] and all(c["passed"] for c in story["checks"]) and
        native["success"] and len(reference) == 2 and
        all(r["matches_reference"] for r in reference) and
        len(lifecycle["cycles"]) == 5 and
        all(v == 0 for v in lifecycle["final_live_resources"].values())):
    raise SystemExit("Architecture smoke failed; inspect stage reports")
summary = {
    "schema_version": 1,
    "success": True,
    "evidence_class": "Linux SDL offscreen / Mesa software; no hardware certification",
    "revision": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
    "smoke_script_sha256": hashlib.sha256(Path("scripts/cordel_phase1_smoke.sh").read_bytes()).hexdigest(),
    "narrative_checks": story["check_count"],
    "native_checks": native["graphics_runtime_checks"],
    "ordinary_renpy_reference_branches": len(reference),
    "scene_cycles": len(lifecycle["cycles"]),
    "final_live_resources": lifecycle["final_live_resources"],
    "full_milestone_regressions": "Run separately; see Phase 1 conclusion",
}
(output / "smoke-summary.json").write_text(json.dumps(summary, indent=2) + "\n")
print("CORDEL Phase 1 architecture smoke passed: " + str(output))
PY
