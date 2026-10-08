#!/usr/bin/env python3
"""Bounded, same-machine physics correctness/lifetime and warmed-cost gate."""
import hashlib
import json
import os
import platform
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n")

def main():
    output = (ROOT / sys.argv[1]).resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit("Use a new/empty physics evidence directory")
    output.mkdir(parents=True, exist_ok=True)
    build = Path(os.environ.get("CORDEL_PHYSICS_BUILD_DIR", ROOT / "build/phase2-physics")).resolve()
    commands = []
    def run(args, log, timeout=600):
        commands.append(args)
        start = time.monotonic()
        with (output / log).open("w") as stream:
            subprocess.run(args, cwd=ROOT, check=True, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
        return time.monotonic() - start
    run(["cmake", "-S", "native/physics", "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"], "configure.txt")
    builds = {}
    for backend in ("jolt", "bullet"):
        clean = not (build / f"cordel-physics-{backend}").exists()
        seconds = run(["cmake", "--build", str(build), "--target", f"cordel-physics-{backend}", "-j", "4"], f"build-{backend}.txt")
        paths = ([build / "_deps/jolt-build/libJolt.a"] if backend == "jolt" else
                 [build / f"_deps/bullet-build/src/{name}/lib{name}.a" for name in ("BulletDynamics", "BulletCollision", "LinearMath")])
        builds[backend] = {"build_seconds": seconds, "clean_target_build": clean,
                           "binary_bytes": (build / f"cordel-physics-{backend}").stat().st_size,
                           "linked_dependency_archive_bytes": sum(p.stat().st_size for p in paths),
                           "dependency_source_bytes": sum(p.stat().st_size for p in (build / f"_deps/{backend}-src").rglob("*") if p.is_file())}
        print(f"{backend}: built in {seconds:.3f}s (clean={clean})", flush=True)
    run(["ctest", "--test-dir", str(build), "--output-on-failure", "-V"], "native-tests.txt")
    results, repetitions = {}, {}
    for backend in ("jolt", "bullet"):
        repetitions[backend] = []
        for repeat in range(5):
            result_path = output / (f"{backend}-results.json" if repeat == 0 else f"{backend}-temporary.json")
            wall = run([str(build / f"cordel-physics-{backend}"), str(result_path)], f"test-{backend}-{repeat}.txt", timeout=15)
            result = json.loads(result_path.read_text())
            assert result["passed"] and all(c["passed"] for c in result["checks"])
            repetitions[backend].append({"repeat": repeat, "process_wall_seconds": wall,
                "scene_initialization_ms": result["scene_initialization_ms"], "peak_rss_kib": result["peak_rss_kib"], "workloads": result["performance"]})
            if repeat == 0:
                results[backend] = result
            else:
                result_path.unlink()
        print(f"{backend}: {result['assertions']} assertions per run, 5 repeated runs passed", flush=True)
    assert results["jolt"]["fixture"] == results["bullet"]["fixture"]
    assert [c["name"] for c in results["jolt"]["checks"]] == [c["name"] for c in results["bullet"]["checks"]]
    aggregate = {}
    for backend, runs in repetitions.items():
        metrics = []
        for count in (100, 1000):
            for name in ("ray", "capsule_sweep", "overlap", "fixed_step_static"):
                samples = [m for r in runs for m in r["workloads"] if m["name"] == name and m["count"] == count]
                metrics.append({"name": name, "count_per_repeat": count, "repetitions": 5,
                    "mean_of_means_us": statistics.mean(s["mean_us"] for s in samples),
                    "median_of_medians_us": statistics.median(s["median_us"] for s in samples),
                    "worst_observed_us": max(s["worst_us"] for s in samples)})
        aggregate[backend] = {"metrics": metrics, "initialization_mean_ms": statistics.mean(r["scene_initialization_ms"] for r in runs),
            "initialization_median_ms": statistics.median(r["scene_initialization_ms"] for r in runs),
            "initialization_worst_ms": max(r["scene_initialization_ms"] for r in runs),
            "peak_rss_kib": max(r["peak_rss_kib"] for r in runs), "build": builds[backend]}
    write(output / "performance-results.json", {"units": "CPU wall-clock us per query; scheduling and timer overhead included", "repetitions": repetitions, "aggregate": aggregate,
        "limitations": "Tiny static convex scene, no hardware rendering required. Process RSS is lifetime high-water mark, not library allocator usage. Build time excludes configure/download; source footprint includes upstream tests/content not linked into executable. No 4ms production budget certification."})
    write(output / "query-results.json", {"shared_fixture_identical": True, "assertions_per_candidate": results["jolt"]["assertions"], "candidates": {b: d["queries"] for b, d in results.items()}})
    write(output / "lifecycle-results.json", {b: {"cycles": d["lifecycle"], "final_live": d["final_live"]} for b, d in results.items()})
    write(output / "fixture.json", results["jolt"]["fixture"])
    write(output / "comparison.json", {"selected_development_backend": "Jolt", "correctness": "Both candidates pass the same scenarios; no Bullet correctness failure found",
        "rationale": "Jolt's explicit penetration/subshape shape-query results and inspected character-query facilities fit the next motor experiment; warmed capsule/ray medians and fixed-step cost are measured, not an exclusive performance win. Larger executable and build are accepted. Bullet remains a tested comparison adapter.",
        "categories": {"capsule_sweeps": "both pass", "raycasts_overlaps": "both pass", "contact_normals": "both pass outward/unit and analytic checks", "step_slope_suitability": "both pass 5 slopes, 5 steps, blocked/clear doors and ceiling",
            "sensors": "both pass query-derived begin/persist/end", "lifetime": "both pass five cycles and stale/cross-world IDs", "fixed_step": "both 60Hz, three catch-up ticks", "linux": "both verified GCC14.2", "windows": "pending both", "adapter_complexity": "two small private adapters, Jolt additionally registers process-global types; Bullet uses synchronous callbacks", "production_motor": "not implemented or certified"},
        "measurements": aggregate, "decision_scope": "Phase2.2 development foundation; revisit on motor, dynamic/triangle scenes or platform evidence"})
    dependencies = json.loads((ROOT / "native/physics/third_party/dependency-manifest.json").read_text())
    write(output / "dependency-manifest.json", dependencies)
    sources = list((ROOT / "native/physics").rglob("*"))
    write(output / "manifest.json", {"success": True, "schema_version": 1, "accepted_base": "0baf08bbf478ba5540ee3b5da067df83fe5e2724",
        "source_revision_at_execution": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "source_sha256": {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources if p.is_file() and "__pycache__" not in p.parts},
        "environment": {"system": platform.system(), "machine": platform.machine(), "compiler": subprocess.check_output(["c++", "--version"], text=True).splitlines()[0], "cmake": subprocess.check_output(["cmake", "--version"], text=True).splitlines()[0], "build_type": "Release", "jobs": 4, "float_precision": "32-bit backend / double CORDEL boundary"},
        "commands": commands, "graphics_fixture_sha256": hashlib.sha256((ROOT / "examples/cordel_viewport/game/assets/cordel_scene.gltf").read_bytes()).hexdigest(), "hardware_certification": False})
    (output / "tests.txt").write_text("\n".join(" ".join(command) for command in commands) + "\nAll commands passed.\n")
    print(f"Phase2.1 physics gate passed: {output}")

if __name__ == "__main__":
    main()
