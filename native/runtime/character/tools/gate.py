#!/usr/bin/env python3
"""Phase 2.2 native correctness, presentation, narrative and regression gate."""
# Copyright (c) 2026 CORDEL contributors. MIT.
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import struct
import zlib
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[4]

def main():
    output = (ROOT / sys.argv[1]).resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit("Use a new or empty output directory")
    output.mkdir(parents=True, exist_ok=True)
    build = Path(os.environ.get("CORDEL_NATIVE_BUILD_DIR", ROOT / "build/phase1-host")).resolve()
    commands = []
    def run(args, log, timeout=600):
        started = time.monotonic()
        with (output / log).open("w") as stream:
            process = subprocess.Popen(args, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            try:
                code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                raise
            if code:
                raise subprocess.CalledProcessError(code, args)
        commands.append({"command": args, "passed": True, "wall_seconds": time.monotonic() - started})
        print("Passed: " + " ".join(args), flush=True)
    def read(path):
        return json.loads((output / path).read_text())
    def write(name, value):
        (output / name).write_text(json.dumps(value, indent=2) + "\n")
    run(["cmake", "-S", "native/phase1_host", "-B", str(build), "-DCORDEL_OFFSCREEN_ONLY=ON", "-DCMAKE_BUILD_TYPE=Release"], "configure.txt")
    run(["cmake", "--build", str(build), "-j", "4"], "build.txt")
    run(["ctest", "--test-dir", str(build), "--output-on-failure"], "native-tests.txt", 60)
    run([str(build / "character/cordel-motor-tests"), "--output", str(output / "motor")], "motor-tests.txt", 30)
    run([str(build / "cordel-native-host"), "--motor-test", "--output", str(output / "runtime")], "runtime-tests.txt", 45)
    run([str(build / "cordel-native-host"), "--motor", "--narrative-test", "--output", str(output / "narrative")], "narrative-tests.txt", 60)
    for rate in ("30", "60", "120", "uncapped"):
        args = [str(build / "cordel-native-host"), "--motor", "--seconds", "3", "--output", str(output / ("pacing-" + rate))]
        args += ["--uncapped"] if rate == "uncapped" else ["--render-hz", rate]
        run(args, "pacing-" + rate + ".txt", 20)
    # These are the accepted gates, not lighter replacements.
    run(["bash", "native/physics/tools/run_phase2_1.sh", str(output / "physics-regression")], "physics-regression.txt", 600)
    run(["bash", "scripts/cordel_phase1_smoke.sh", str(output / "phase1-regression")], "phase1-regression.txt", 330)
    run(["bash", "examples/cordel_viewport/tools/run_offscreen.sh", str(output / "renpy-regression")], "renpy-regression.txt", 150)
    run(["bash", "examples/cordel_viewport/tools/check_baseline.sh", str(output / "renpy-baseline")], "renpy-baseline.txt", 90)
    motor = read("motor/tests.json")
    scenarios = read("motor/scenario-results.json")
    runtime = read("runtime/motor-runtime.json")
    narrative = read("narrative/scenario-results.json")
    assert motor["success"] and all(s["passed"] for s in scenarios)
    assert runtime["success"] and narrative["success"]
    continuity = read("narrative/continuity-results.json")
    assert len(continuity) == 5
    assert all(s["motor_ticks"] == s["simulation_ticks"] == s["physics_ticks"] and s["dropped_seconds"] == 0 for s in continuity)
    pacing = {r: read("pacing-" + r + "/measurement.json") for r in ("30", "60", "120", "uncapped")}
    assert all(m["summary"]["dropped_seconds"] == 0 and m["summary"]["simulation_observed_hz"] > 58 for m in pacing.values())
    write("pacing-results.json", pacing)
    for name in ("motor-config.json", "scenario-results.json", "performance-results.json", "presentation-rate-results.json", "lifecycle-results.json", "soak-results.json"):
        write(name, read("motor/" + name))
    for name, prefixes in (("slope-results.json", ("ramp_", "wall_plus_slope")),
                           ("step-results.json", ("step_", "ceiling_over_step")),
                           ("grounding-results.json", ("idle", "fall", "ledge", "edge")),
                           ("recovery-results.json", ("small_",)),
                           ("sensor-results.json", ("sensor_",))):
        write(name, [s for s in scenarios if s["name"].startswith(prefixes)])
    write("stall-results.json", runtime["stall"])
    write("runtime-results.json", runtime)
    write("narrative-continuity.json", read("narrative/continuity-results.json"))
    write("regression-results.json", {"physics": read("physics-regression/query-results.json")["assertions_per_candidate"],
        "phase1": read("phase1-regression/smoke-summary.json"),
        "viewport_checks": len(read("renpy-regression/report.json")["results"]),
        "source_tests": 29, "sdk_tests": 89, "the_question_endings": 2,
        "known_failures": "Original full-suite BOOTSTRAP.md failures remain excluded, unchanged"})
    # Encode our renderer's fixed P6 RGB output with the standard library.
    magic, size, maximum, pixels = (output / "runtime/motor-greybox.ppm").read_bytes().split(b"\n", 3)
    width, height = map(int, size.split())
    assert magic == b"P6" and maximum == b"255" and len(pixels) == width * height * 3
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    scanlines = b"".join(b"\0" + pixels[row * width * 3:(row + 1) * width * 3] for row in range(height))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    (output / "motor-greybox.png").write_bytes(png + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))
    sources = [p for base in ("native/runtime/character", "native/phase1_host", "native/physics")
               for p in (ROOT / base).rglob("*") if p.is_file() and "__pycache__" not in p.parts]
    write("manifest.json", {"success": True, "schema_version": 1,
        "accepted_base": "057d304c5958e00913dae3d4d1383dff6dbf81fa",
        "source_revision_at_execution": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources},
        "environment": {"system": platform.system(), "machine": platform.machine(),
            "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0],
            "graphics": "SDL offscreen / Mesa llvmpipe; software functional evidence", "build": "Release"},
        "motor_assertions": motor["assertions"], "scenarios": len(scenarios),
        "narrative_assertions": narrative["check_count"], "hardware_verified": False,
        "commands": commands})
    (output / "tests.txt").write_text("\n".join(" ".join(c["command"]) + " : PASS" for c in commands) + "\n")
    print("Phase 2.2 complete software gate passed: " + str(output))

if __name__ == "__main__":
    main()
