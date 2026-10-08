#!/usr/bin/env bash
# Project-local dependency acquisition; no root privileges or GPU needed.
set -euo pipefail
cordel_repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$cordel_repo"
export PATH="$cordel_repo/.venv/bin:$PATH"
exec .venv/bin/python native/physics/tools/gate.py "${1:-tmp/phase2_1-final}"
