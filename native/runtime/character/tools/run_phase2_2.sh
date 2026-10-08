#!/usr/bin/env bash
# Copyright (c) 2026 CORDEL contributors. MIT.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)
cd "$cordel_repo"
export PATH="$cordel_repo/.venv/bin:$PATH"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
exec .venv/bin/python native/runtime/character/tools/gate.py "${1:-tmp/phase2_2-$(date -u +%Y%m%dT%H%M%S)}"
