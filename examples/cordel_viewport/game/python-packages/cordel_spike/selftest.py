"""Executed functional experiments; generated events are not device verification.

KEY/MOUSE/WINDOW/CONTROLLER events are posted through Ren'Py's SDL event queue
after interaction initialization. Rendering and Say interactions use the real
runtime. Pixel probes use the real renderer, with depth disabled as a control.
"""

import json
import math
import time

import renpy
import renpy.exports as api
import renpy.pygame as pygame

from .math3d import Camera, imported_to_world
from .runtime import ACTIVE, ASSET, Viewport, collect_resources, log_event, output_directory
from renpy.gl2 import assimp


class Experiment:
    def __init__(self):
        self.results = []
        self.measurements = {}
        self.viewport = None

    def check(self, name, condition, **evidence):
        record = dict(name=name, passed=bool(condition), **evidence)
        self.results.append(record)
        log_event("check", **record)
        if not condition:
            raise AssertionError(name + ": " + repr(evidence))

    def post(self, events):
        for event_type, fields in events:
            pygame.event.post(pygame.event.Event(event_type, **fields))

    def wait(self, seconds, events=()):
        if events:
            api.show_screen("cordel_test_input", callback=lambda: self.post(events))
        api.pause(seconds, hard=True)
        api.hide_screen("cordel_test_input")

    def key(self, key, down=True):
        return (pygame.KEYDOWN if down else pygame.KEYUP,
                dict(key=key, mod=0, unicode="", repeat=False))

    def focus(self):
        w, h = self.viewport.drawable
        return [(pygame.WINDOWMOUSEENTER, {}), (pygame.WINDOWFOCUSGAINED, {}),
                (pygame.MOUSEBUTTONDOWN, dict(button=1, pos=(w//2, h//2))),
                (pygame.MOUSEBUTTONUP, dict(button=1, pos=(w//2, h//2)))]

    def enter(self):
        self.viewport = Viewport()
        api.show_screen("cordel_viewport", viewport=self.viewport)
        self.wait(.25)
        self.check("asset_loaded", self.viewport.model_data_ref() is not None,
                   meshes=len(self.viewport.model.load().mesh_info))

    def leave(self):
        viewport = self.viewport
        viewport.release_input("interaction_exited")
        api.hide_screen("cordel_viewport")
        viewport.close("selftest_leave")
        samples = viewport.metrics.frames
        self.wait(.2)
        snapshot = collect_resources()
        self.check("lifecycle_no_active_importer", snapshot["active_sessions"] == 0 and
                   snapshot["assimp_cache_entries"] == 0, resources=snapshot)
        self.check("lifecycle_no_updates_after_exit", viewport.metrics.frames == samples,
                   frames_before=samples, frames_after=viewport.metrics.frames)
        self.check("lifecycle_input_cleared", not viewport.actions.active() and not viewport.focused
                   and not viewport.dragging and viewport.axes == (0, 0))
        self.check("single_import_per_session", viewport.imports_at_close == 1,
                   imports=viewport.imports_at_close)
        self.check("importer_data_reference_released", viewport.model_data_ref() is None)
        snapshot["mesh_weakrefs_available"] = len(viewport.mesh_refs)
        snapshot["mesh_weakrefs_alive"] = (sum(ref() is not None for ref in viewport.mesh_refs)
                                          if viewport.mesh_refs else None)
        snapshot["viewport_redraw_requests"] = sum(d is viewport for _, d in renpy.display.render.redraw_queue)
        self.check("lifecycle_no_pending_viewport_redraw", snapshot["viewport_redraw_requests"] == 0)
        return snapshot

    def depth_pixels(self):
        v = self.viewport
        v.camera = Camera(position=(-1.8, 1.2, 4), pitch=0)
        results = {}
        for depth, reverse in ((True, False), (True, True), (False, False), (False, True)):
            surface = v.scene_snapshot(depth=depth, reverse_submission=reverse)
            w, h = surface.get_size()
            pixel = tuple(surface.get_at((w//2, h//2)))
            key = f"depth_{depth}_reverse_{reverse}"
            results[key] = dict(pixel=pixel, size=(w, h))
            pygame.image.save(surface, str(output_directory() / (key + ".png")))
        a, b = (results[key]["pixel"] for key in ("depth_True_reverse_False", "depth_True_reverse_True"))
        self.check("depth_near_red_in_both_orders", a[0] > 2*a[2] and b[0] > 2*b[2], pixels=results)
        c, d = (results[key]["pixel"] for key in ("depth_False_reverse_False", "depth_False_reverse_True"))
        self.check("disabled_depth_control_changes_occlusion", c != d and (c[2] > c[0] or d[2] > d[0]),
                   pixels=results)
        self.measurements["depth"] = results

    def coordinates(self):
        data = self.viewport.model.load()
        positions = [imported_to_world(blit.reverse.transform(0, 0, 0, components=3))
                     for blit in data.blit_info]
        with api.file(ASSET) as stream:
            nodes = json.load(stream)["nodes"]
        expected = [node["translation"] for node in nodes]
        self.check("actual_imported_node_conversion", len(positions) == len(expected) and
                   all(any(math.dist(point, authored) < 1e-5 for point in positions) for authored in expected),
                   imported_to_world_origins=positions, authored_origins=expected)
        self.measurements["coordinates"] = dict(converted_origins=positions, authored_origins=expected)
        # Deterministic regression for the live importer eviction observed in an
        # earlier cycle run. Remove only our entry, then demand the identical pin.
        with assimp.loader_lock:
            assimp.cache.pop(self.viewport.model, None)
        pinned = self.viewport.model.load()
        self.check("session_pin_survives_cache_eviction", pinned is data and self.viewport.model.imports == 1,
                   imports=self.viewport.model.imports)

    def resize_pixels(self):
        v = self.viewport
        v.camera = Camera(position=(1.2, .5, 4), pitch=0)
        results = []
        # SDL's offscreen window starts with a fixed-size drawable on this host.
        # Stay within it for actual framebuffer verification; oversized readbacks
        # yielded invalid pixels. Desktop growth beyond it is a separate check.
        for width, height in ((850, 480), (500, 500), (320, 500), (800, 400)):
            # The exported set_physical_size clamps to a virtual-aspect desktop
            # bound (922x519 here). Simulate an OS resize through the SDL wrapper.
            pygame.display.get_window().resize((width, height), opengl=True,
                                               fullscreen=False, maximized=False)
            self.wait(.15)
            surface = v.scene_snapshot()
            sw, sh = surface.get_size()
            pygame.image.save(surface, str(output_directory() / f"resize_{width}x{height}.png"))
            self.check("resize_framebuffer_clear_valid", tuple(surface.get_at((0, 0)))[:3] == (24, 33, 43),
                       corner=tuple(surface.get_at((0, 0))), requested=(width, height))
            xs, ys = [], []
            for y in range(sh):
                for x in range(sw):
                    r, g, b, _ = surface.get_at((x, y))
                    if r > 100 and .90*r < g < 1.05*r and .60*r < b < .75*r:
                        xs.append(x)
                        ys.append(y)
            self.check("resize_cream_cube_visible", bool(xs), requested=(width, height))
            box = (min(xs), min(ys), max(xs)+1, max(ys)+1)
            ratio = (box[2]-box[0]) / (box[3]-box[1])
            centre = tuple(surface.get_at((sw//2, sh//2)))
            result = dict(requested=(width, height), drawable=v.drawable, screenshot=(sw, sh),
                          cube_box=box, cube_pixel_aspect=ratio, centre_pixel=centre)
            results.append(result)
            self.check("resize_drawable_and_projection", v.drawable == (width, height) and
                       (sw, sh) == (width, height) and abs(ratio-1) < .035, **result)
            pygame.image.save(surface, str(output_directory() / f"resize_{width}x{height}.png"))
            v.camera.position = (-1.8, 1.2, 4)
            surface = v.scene_snapshot()
            pixel = tuple(surface.get_at((sw//2, sh//2)))
            self.check("resize_depth_preserved", pixel[0] > 2*pixel[2], drawable=v.drawable, pixel=pixel)
            v.camera.position = (1.2, .5, 4)
            self.wait(.12, self.focus() + [self.key(pygame.K_w)])
            self.check("camera_moves_after_resize", v.camera.position[2] < 3.9,
                       drawable=v.drawable, position=v.camera.position)
            v.camera.position = (1.2, .5, 4)
        self.measurements["resize"] = results
        pygame.display.get_window().resize((850, 480), opengl=True,
                                           fullscreen=False, maximized=False)
        self.wait(.1)

    def inputs(self):
        v = self.viewport
        v.camera = Camera(position=(0, 2.5, 6), pitch=0)
        start = v.camera.position
        self.wait(.35, self.focus() + [self.key(pygame.K_w), self.key(pygame.K_d)])
        travelled = math.dist(start, v.camera.position)
        self.check("held_w_d_movement", v.actions.held("forward") and v.actions.held("right") and
                   v.camera.position[0] > start[0] and v.camera.position[2] < start[2],
                   position=v.camera.position, distance=travelled, actions=v.actions.active())
        self.check("movement_bounded_diagonal", .3 < travelled < 1.2, distance=travelled)
        self.wait(.10, [self.key(pygame.K_w, False), self.key(pygame.K_d, False)])
        self.check("key_up_and_interaction_clear", not v.actions.active())
        for key, axis, sign in ((pygame.K_s, 2, 1), (pygame.K_a, 0, -1),
                                (pygame.K_SPACE, 1, 1), (pygame.K_LCTRL, 1, -1)):
            start = v.camera.position
            self.wait(.15, self.focus() + [self.key(key)])
            displacement = v.camera.position[axis] - start[axis]
            self.check("direction_" + str(key), sign*displacement > .1, displacement=displacement)
        start = v.camera.position
        self.wait(.15, self.focus() + [self.key(pygame.K_SPACE), self.key(pygame.K_LSHIFT)])
        self.check("sprint_vertical", v.camera.position[1] - start[1] > .5,
                   displacement=v.camera.position[1]-start[1])
        w, h = v.drawable
        drag = [(pygame.WINDOWMOUSEENTER, {}),
                (pygame.MOUSEBUTTONDOWN, dict(button=1, pos=(w//2, h//2))),
                (pygame.MOUSEMOTION, dict(pos=(w//2+100, h//2-40), rel=(100, -40), buttons=(1,0,0))),
                (pygame.MOUSEBUTTONUP, dict(button=1, pos=(w//2+100, h//2-40)))]
        self.wait(.15, drag)
        self.check("drag_mouse_yaw_pitch", v.camera.yaw == 15 and v.camera.pitch == 6,
                   yaw=v.camera.yaw, pitch=v.camera.pitch, sdl_mouse_grab=pygame.event.get_grab())
        self.wait(.1, [self.key(pygame.K_ESCAPE)])
        self.check("escape_releases_focus", not v.focused and not v.dragging and not v.actions.active())
        loss_results = []
        for loss in (pygame.WINDOWFOCUSLOST, pygame.WINDOWMOUSELEAVE, pygame.WINDOWMINIMIZED):
            self.wait(.12, self.focus() + [self.key(pygame.K_w), (loss, {})])
            loss_results.append(dict(event=pygame.event.event_name(loss), actions=v.actions.active(), focused=v.focused))
            self.check("focus_loss_clears_held", not v.actions.active() and not v.focused and not v.dragging,
                       loss=pygame.event.event_name(loss))
        self.measurements["focus_loss"] = loss_results
        self.wait(.2, self.focus() + [(pygame.CONTROLLERAXISMOTION, dict(which=9876, axis=0, value=16384))])
        # Ren'Py's controller layer also posts digital pad events/hides the cursor,
        # which can remove hover focus. Record that behavior rather than call it
        # verified controller support. Test the raw adapter in isolation as well.
        self.measurements["controller_dispatch"] = dict(axes_after_engine_dispatch=v.axes,
            viewport_focus_after_dispatch=v.focused,
            physical_controllers=len(renpy.display.controller.controllers),
            note="synthetic axis through engine; default controller layer can take input focus")
        for event in (pygame.event.Event(pygame.MOUSEBUTTONDOWN, button=1),
                      pygame.event.Event(pygame.CONTROLLERAXISMOTION, which=9876, axis=0, value=16384)):
            try:
                v.event(event, 100, 100, 0)
            except api.IgnoreEvent:
                pass
        self.check("analog_raw_adapter_software_path", .49 < v.axes[0] < .51,
                   axes=v.axes, scope="direct event-handler check; controller ownership remains unresolved",
                   physical_controllers=len(renpy.display.controller.controllers))
        try:
            v.event(pygame.event.Event(pygame.CONTROLLERDEVICEREMOVED, which=9876), 0, 0, 0)
        except api.IgnoreEvent:
            pass
        self.check("controller_disconnect_adapter_clear", v.axes == (0, 0))
        self.wait(.1, [(pygame.CONTROLLERDEVICEREMOVED, dict(which=9876))])
        self.check("controller_disconnect_clear", v.axes == (0, 0))

    def stall(self):
        v = self.viewport
        v.camera = Camera(pitch=0)
        self.wait(.45, self.focus() + [self.key(pygame.K_w), self.key(pygame.K_F6)])
        result = v.last_stall
        self.check("forced_stall_clamped", result is not None and result["raw_seconds"] >= .25 and
                   result["delta_seconds"] == .05 and result["movement_metres"] <= .150001,
                   stall=result)
        self.measurements["stall"] = result
        self.wait(.2)
        following = list(v.metrics.intervals)[-5:]
        self.check("stall_recovers_update_intervals", any(interval < .05 for interval in following),
                   following_intervals=following, following_delta=v.metrics.last.delta)

    def narrative(self):
        v = self.viewport
        v.set_input_enabled(False)
        before = v.metrics.frames
        started = time.perf_counter()
        api.show_screen("cordel_test_input", callback=lambda: self.post(self.focus() + [self.key(pygame.K_w)]))
        api.say(None, "Narrative idle probe: this is a real Ren'Py Say interaction. The viewport keeps redrawing.")
        api.hide_screen("cordel_test_input")
        result = dict(update_frames=v.metrics.frames-before, wall_seconds=time.perf_counter()-started,
                      held_actions=v.actions.active(), input_enabled=v.input_enabled,
                      interval_ms=v.metrics.last.raw*1000)
        self.check("real_say_continuous_redraw", result["update_frames"] >= 10 and
                   result["wall_seconds"] >= .65, **result)
        self.check("say_owns_input", not v.actions.active() and not v.focused)
        self.measurements["narrative"] = result
        v.set_input_enabled(True)
        before = v.metrics.frames
        self.wait(.2)
        self.check("return_from_say_redraw", v.metrics.frames > before)

    def steady(self):
        v = self.viewport
        v.camera = Camera()
        self.wait(.3)
        v.metrics = type(v.metrics)()
        v.clock.previous = None
        started, cpu_started, presentations = time.perf_counter(), time.process_time(), renpy.config.frames
        self.wait(3)
        result = dict(**v.metrics.summary(), wall_seconds=time.perf_counter()-started,
                      process_cpu_seconds=time.process_time()-cpu_started,
                      engine_drawn_frames=renpy.config.frames-presentations,
                      renderer=dict(renpy.display.draw.info), drawable=v.drawable)
        self.check("continuous_idle_updates", result["frames"] >= 60, frames=result["frames"])
        self.measurements["steady_software"] = result
        renpy.display.draw.draw_screen(renpy.display.interface.surftree, flip=False)
        pygame.image.save(renpy.display.draw.screenshot(None), str(output_directory() / "viewport.png"))

    def run(self):
        self.enter()
        self.coordinates()
        self.depth_pixels()
        self.resize_pixels()
        self.inputs()
        self.measurements["active_camera_cpu"] = {k: val for k, val in self.viewport.metrics.summary().items()
                                                   if k.startswith("active_")}
        self.stall()
        self.narrative()
        self.steady()
        cycles = [self.leave()]
        callback_count = len(renpy.config.at_exit_callbacks)
        for _ in range(4):
            self.enter()
            cycles.append(self.leave())
        self.check("no_duplicate_shutdown_callbacks", len(renpy.config.at_exit_callbacks) == callback_count,
                   callback_count=callback_count)
        texture_counts = [snapshot["texture_cache_size"][1] for snapshot in cycles]
        self.check("texture_cache_bounded_over_cycles", max(texture_counts[1:]) <= texture_counts[0]+2,
                   texture_counts=texture_counts)
        self.measurements["five_cycles"] = cycles


def run():
    experiment = Experiment()
    success = False
    try:
        experiment.run()
        success = True
    finally:
        for viewport in tuple(ACTIVE):
            api.hide_screen("cordel_viewport")
            viewport.close("selftest_finally")
        report = dict(success=success, evidence_kind="offscreen software rendering and synthetic events",
                      results=experiment.results, measurements=experiment.measurements)
        (output_directory() / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        log_event("selftest_finished", success=success, passed=sum(r["passed"] for r in experiment.results),
                  checks=len(experiment.results))
