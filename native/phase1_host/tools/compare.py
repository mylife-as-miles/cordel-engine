#!/usr/bin/env python3
"""Compare unmodified Ren'Py reports and native measurements without equalizing
different frame metrics or concealing unfavorable/variable runs.
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def read(path):
    return json.loads(Path(path).read_text())


def renpy_results(report):
    if not report["success"] or not all(r["passed"] for r in report["results"]):
        raise ValueError("Ren'Py gate did not pass")
    m = report["measurements"]
    return dict(check_records=len(report["results"]), steady=m["steady_software"],
                active_camera=m["active_camera_cpu"], stall=m["stall"], narrative=m["narrative"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renpy", action="append", required=True, help="include every comparable replay report")
    parser.add_argument("--native", required=True, help="native self-test output directory")
    parser.add_argument("--controlled", required=True)
    parser.add_argument("--uncapped", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    native = Path(args.native)
    test = read(native / "self-test.json")
    if not test["success"]:
        raise SystemExit("Native self-test did not pass")
    controlled, uncapped = read(args.controlled), read(args.uncapped)
    for measurement in (controlled, uncapped):
        if measurement["drawable_size"] != [850, 480] or "llvmpipe" not in measurement["gpu"]:
            raise SystemExit("This archived comparison requires matching 850x480 llvmpipe runs")
    replay_results = [renpy_results(read(p)) for p in args.renpy]
    for replay in replay_results:
        if replay["steady"]["drawable"] != [850, 480]:
            raise SystemExit("Ren'Py comparison drawable differs")
    dimensions = [
        ("main_loop_owner", "Ren'Py", "CORDEL"),
        ("simulation", "render invalidation/redraw driven", "bounded 60 Hz fixed accumulator"),
        ("interpolation", "none", "previous/current poses, shortest yaw arc"),
        ("stall", "50ms dt clamp", "3 fixed ticks, whole backlog dropped, fraction retained"),
        ("input_owner", "shared UI/story/hover/keymaps", "native held state, focus clear"),
        ("relative_mouse", "uncaptured drag", "SDL window relative-mode request; physical behavior unverified"),
        ("asset_lifetime", "prediction/cache coupled; session pin workaround", "deterministic scene/GL RAII"),
        ("scene_import", "Ren'Py/Assimp plus Y reflection cancellation", "cgltf, storage transpose only"),
        ("depth", "GL_LEQUAL", "GL_LEQUAL; identical numeric controls"),
        ("resize", "virtual/display coupling; oversized pbuffer unverified", "drawable-owned; offscreen surface recreation"),
        ("python_in_frame_loop", True, False),
        ("narrative", "full Ren'Py Say/Menu", "none in Phase 1.2"),
        ("teardown", "importer/caches observable; GPU deletion unproven", "GL delete calls / counters explicit; driver residency unmeasured"),
    ]
    result = dict(
        schema=1, evidence_kind="functional software/offscreen; not hardware performance",
        environment=dict(os="Debian 13.6 x86_64", renderer=controlled["gpu"],
                         native_gl=controlled["gl_version"], sdl_native="3.4.8 stock pinned release",
                         sdl_renpy="3.4.8 queried from loaded SDK SDL_GetVersion",
                         drawable=[850, 480], camera=[0, 2.5, 6], yaw=0, pitch=-8,
                         same_scene="examples/cordel_viewport/game/assets/cordel_scene.gltf"),
        accepted_phase1_1=renpy_results(read(ROOT / "docs/cordel/evidence/phase1_1/report.json")),
        phase1_1_replays=replay_results,
        native_self_test=test, native_stall=read(native / "stall-probe.json"),
        native_input=read(native / "input-probe.json"),
        native_controlled=controlled, native_uncapped=uncapped,
        architecture=[dict(dimension=k, renpy=a, native=b) for k, a, b in dimensions],
        comparability_limits=[
            "Ren'Py viewport invalidations/updates, native completed render iterations, and Ren'Py cached draw counter are different metrics; no speedup ratio is valid.",
            "Ren'Py requested 60Hz redraw, but retains fast cached draws. Native controlled mode sleeps to ~60Hz; native uncapped mode is a separate throughput sample.",
            "Native glFinish bounds outstanding work. Ren'Py uses its existing compositing/cache/presentation policy; neither implementation was altered for favorable results.",
            "Same scene/camera/light/llvmpipe/SDL version number; core versus compatibility contexts, SDL build options, importer, UI/HUD and shader plumbing still differ.",
            "Both use monotonic CPU-code wall fragments, not exclusive thread CPU or GPU timing. Preparation and active camera scopes/samples are not identical.",
            "The first render interval is near-zero after warm-up; final presentation/pacing tail is included in wall time but not a subsequent clock sample. 179 ticks over ~3s is a one-tick sample-boundary effect.",
            "Ren'Py replay stall can clear held input before movement. Its existing assertion checks an upper bound and can pass at zero metres; include both replay results.",
            "Short samples on a shared software-rendering host do not establish world-scale performance, hardware behavior or long-soak memory safety.",
        ])
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Comparison saved: {output}; {len(replay_results)} Ren'Py replays retained")
