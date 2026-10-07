#!/usr/bin/env bash
# Linux functional verification using the pinned BOOTSTRAP.md runtime.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$cordel_repo"
cordel_output=${1:-"$cordel_repo/tmp/cordel-viewport-$(date -u +%Y%m%dT%H%M%S)"}
mkdir -p "$cordel_output"
cordel_output=$(cd "$cordel_output" && pwd)
if [[ -e "$cordel_output/report.json" ]]; then
    printf 'Use a new output directory; report already exists: %s\n' "$cordel_output" >&2
    exit 1
fi
if [[ ! -x lib/py3-linux-x86_64/python || ! -x .venv/bin/python ]]; then
    printf 'Configure the Python environment and pinned nightly SDK from docs/cordel/BOOTSTRAP.md first.\n' >&2
    exit 1
fi
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
export CORDEL_VIEWPORT_OUTPUT="$cordel_output"
.venv/bin/python examples/cordel_viewport/tools/generate_scene.py --check
.venv/bin/python -m unittest discover -s examples/cordel_viewport/tests -v 2>&1 | tee "$cordel_output/logic-tests.txt"
CORDEL_VIEWPORT_SELFTEST=1 timeout 60s lib/py3-linux-x86_64/python -X utf8 renpy.py examples/cordel_viewport \
    2>&1 | tee "$cordel_output/runtime-console.txt"
CORDEL_VIEWPORT_SELFTEST=0 timeout 30s lib/py3-linux-x86_64/python -X utf8 renpy.py \
    examples/cordel_viewport test viewport_ui --report-detailed 2>&1 | tee "$cordel_output/ui-tests.txt"
.venv/bin/python - "$cordel_output/report.json" <<'PY'
import json
import sys
with open(sys.argv[1]) as stream:
    report = json.load(stream)
if not report["success"] or not all(result["passed"] for result in report["results"]):
    raise SystemExit("Viewport experiments failed; inspect report.json and trace.jsonl")
print(f"Viewport experiments: {len(report['results'])} checks passed; {sys.argv[1]}")
PY
