"""Ren'Py-specific adapter. Uses GLTFModel/render trees, never issues GL calls.

This module intentionally depends on inspected internal APIs. It is a reference
spike, not a supported renderer interface. Pure logic lives in adjacent modules.
"""

import gc
import json
import os
from pathlib import Path
import time
import threading
import weakref

import renpy
import renpy.exports as api
import renpy.pygame as pygame
from renpy.display.displayable import Displayable
from renpy.display.imagelike import Solid
from renpy.display.matrix import Matrix
from renpy.display.render import Render, render, redraw
from renpy.gl2 import assimp
from renpy.text.text import Text

from . import VERSION
from .input_state import ActionState, analog_pair
from .math3d import ASSIMP_TO_WORLD, Camera, multiply
from .timing import FrameClock, Metrics

ASSET = "assets/cordel_scene.gltf"
# Cython compile-time constant (render.pyx/.pxd), not exported by the pinned SDK.
MATRIX_PROJECTION = 2
CONVENTION = "RH; +Y up; -Z forward; metre/unit; row-major, column vectors; import Y reflection cancelled"
ACTIVE = set()
_session_number = 0


def output_directory():
    path = Path(os.environ.get("CORDEL_VIEWPORT_OUTPUT",
                               str(Path(renpy.config.basedir) / "tmp/viewport")))
    path.mkdir(parents=True, exist_ok=True)
    return path


def log_event(event, **fields):
    record = dict(event=event, monotonic=time.perf_counter(), **fields)
    line = json.dumps(record, sort_keys=True, allow_nan=False)
    api.log("CORDEL_VIEWPORT " + line)
    with (output_directory() / "trace.jsonl").open("a", encoding="utf-8") as stream:
        stream.write(line + "\n")


def register_shader():
    api.register_shader("cordel.spike", glsl=100, variables="""
        uniform mat4 u_model__inverse_transpose;
        uniform mat4 u_cordel_import_to_world;
        uniform vec4 u_color_diffuse;
        attribute vec3 a_normal;
        varying float v_cordel_light;
    """, vertex_300="""
        vec3 n = normalize((u_cordel_import_to_world * u_model__inverse_transpose * vec4(a_normal, 0.0)).xyz);
        v_cordel_light = 0.35 + 0.65 * max(dot(n, normalize(vec3(-0.4, 0.8, 0.5))), 0.0);
    """, fragment_300="""
        gl_FragColor = vec4(u_color_diffuse.rgb * v_cordel_light, 1.0);
    """)


def resource_snapshot():
    """Observable renderer cache counters; these are NOT GPU allocation counts."""
    draw = renpy.display.draw
    textures = draw.get_texture_size() if draw is not None else (0, 0)
    return dict(active_sessions=len(ACTIVE), assimp_cache_entries=len(assimp.cache),
                assimp_predicted=len(assimp.predicted or ()),
                assimp_new_predicted=len(assimp.new_predicted),
                texture_cache_size=list(textures))


def shutdown_all():
    for viewport in tuple(ACTIVE):
        viewport.close("process_exit")


class SessionModel(assimp.GLTFModel):
    """Pin importer data for an active session, independently of prediction GC.

    GLTFModel remains responsible for import and mesh Render construction.
    Its global cache is prediction-owned, and can evict a live CDD's model.
    Keep the same ModelData through those evictions rather than re-importing it.
    The lock also serializes Ren'Py's optional background preloader with release.
    """

    def __init__(self):
        super().__init__(ASSET, shader="cordel.spike",
                         u_cordel_import_to_world=Matrix(ASSIMP_TO_WORLD))
        self._data = None
        self._released = False
        self._pin_lock = threading.RLock()
        self.imports = 0

    def load(self):
        with self._pin_lock:
            if self._released:
                raise RuntimeError("Attempt to load a released CORDEL viewport model")
            if self._data is None:
                self._data = super().load()
                self.imports += 1
            assimp.cache[self] = self._data
            return self._data

    def release(self):
        with self._pin_lock:
            with assimp.loader_lock:
                assimp.cache.pop(self, None)
                # Replace sets instead of mutating a background iterator's set.
                assimp.new_predicted = assimp.new_predicted - {self}
                if assimp.predicted is not None:
                    assimp.predicted = assimp.predicted - {self}
            self._released = True
            self._data = None


class Viewport(Displayable):
    REDRAW_SECONDS = 1 / 60

    def __init__(self):
        super().__init__()
        global _session_number
        _session_number += 1
        self.session = _session_number
        self.camera = Camera()
        self.actions = ActionState()
        self.clock = FrameClock(maximum=.05)
        self.metrics = Metrics()
        self.model = SessionModel()
        self.model_data_ref = None
        self.imports_at_close = 0
        self.mesh_refs = []
        self.active = True
        self.focused = False
        self.dragging = False
        self.input_enabled = True
        self.axes = (0.0, 0.0)
        self.controller_id = None
        self.stall_pending = False
        self.last_stall = None
        self.drawable = (1, 1)
        self.logical = (1, 1)
        self._last_log = 0.0
        self._hud_until = 0.0
        self._hud = None
        self._background = Solid("#18212b")
        self._panel = Solid("#101722dd")
        self._cpu_started = time.process_time()
        self._wall_started = time.perf_counter()
        self._engine_frame_started = renpy.config.frames
        self._started_logged = False
        self.keys = {pygame.K_w: "forward", pygame.K_s: "backward",
                     pygame.K_a: "left", pygame.K_d: "right", pygame.K_SPACE: "up",
                     pygame.K_LCTRL: "down", pygame.K_LSHIFT: "sprint"}
        ACTIVE.add(self)
        log_event("viewport_created", session=self.session, version=VERSION, asset=ASSET,
                  convention=CONVENTION, mouse_policy="uncaptured_drag_look")

    def per_interact(self):
        # Ren'Py starts a new interaction for pause, say, menus and screen returns.
        self.release_input("interaction_started")
        if self.active:
            redraw(self, 0)

    def unfocus(self, default=False):
        self.release_input("viewport_unfocused")

    def release_input(self, reason):
        had_input = self.focused or self.dragging or self.actions.active() or self.axes != (0, 0)
        self.actions.clear()
        self.axes = (0.0, 0.0)
        self.focused = self.dragging = False
        if had_input:
            log_event("input_released", session=self.session, reason=reason)

    def set_input_enabled(self, enabled):
        self.release_input("input_ownership_changed")
        self.input_enabled = enabled
        redraw(self, 0)

    def visit(self):
        return [self.model] if self.active and self.model is not None else []

    def _drawable_size(self):
        viewport = renpy.display.draw.drawable_viewport
        return (max(1, round(viewport[2])), max(1, round(viewport[3])))

    def _scene(self, width, height, st, at, *, depth=True, reverse_submission=False):
        """Single conversion boundary: screen^-1 * P * V * C^-1.

        The imported child contributes C * node, leaving P * V * node.
        P uses drawable aspect; screen^-1 uses Ren'Py's logical render size.
        reverse_submission/depth=False are pixel-test controls, never live mode.
        """
        scene = Render(width, height)
        projection_view = multiply(self.camera.projection(*self.drawable), self.camera.view())
        scene.reverse = Matrix.screen_projection(width, height).inverse() * Matrix(
            multiply(projection_view, ASSIMP_TO_WORLD))
        scene.forward = scene.reverse.inverse()
        scene.matrix_kind = MATRIX_PROJECTION
        scene.add_property("depth", depth)
        child = self.model.render(width, height, st, at)
        # Fresh Render, not an engine-cached render; do not mutate shared geometry.
        child.add_property("depth", depth)
        if reverse_submission:
            child.children.reverse()
        scene.blit(child, (0, 0))
        return scene

    def scene_snapshot(self, *, depth=True, reverse_submission=False):
        """Build without advancing simulation/timing or scheduling another redraw."""
        width, height = self.logical
        root = Render(width, height)
        root.blit(render(self._background, width, height, 0, 0), (0, 0))
        root.blit(self._scene(width, height, 0, 0, depth=depth,
                              reverse_submission=reverse_submission), (0, 0))
        # Use the normal drawable viewport, then read it back without re-rendering.
        # Ren'Py's screenshot(render_tree) instead scales both axes by draw_per_virt;
        # that assumes the original virtual aspect and would invalidate resize probes.
        renpy.display.draw.draw_screen(root, flip=False)
        return renpy.display.draw.screenshot(None)

    def _hud_text(self):
        info = renpy.display.draw.info
        metrics = self.metrics.summary()
        x, y, z = self.camera.position
        status = "dragging (uncaptured)" if self.dragging else "uncaptured — drag-look fallback"
        return ("CORDEL ENGINE 0.1.0-dev | Phase 1.1 Viewport Spike\n"
                f"Update FPS ~{metrics['fps']:.1f} | interval {self.metrics.last.raw*1000:.2f} ms | "
                f"delta {self.metrics.last.delta*1000:.2f} ms | frames {self.metrics.frames}\n"
                f"Camera {x:+.2f} / {y:+.2f} / {z:+.2f} m | yaw {self.camera.yaw:+.1f} | pitch {self.camera.pitch:+.1f}\n"
                f"Drawable {self.drawable[0]} × {self.drawable[1]} | {info.get('renderer', '?')} | mouse: {status}\n"
                f"Update CPU {self.metrics.update_ms:.3f} ms | tree prep CPU {self.metrics.prep_ms:.3f} ms\n"
                "Click to focus; drag LMB to look. WASD / Space / LCtrl; LShift sprint.\n"
                "Esc releases focus | F6: 250 ms stall | F7: dialogue | F9: leave viewport\n"
                f"Input: {'viewport' if self.input_enabled and self.focused else 'UI / click to focus'}")

    def render(self, width, height, st, at):
        width, height = max(1, width), max(1, height)
        if not self.active:
            return Render(width, height)
        old_drawable = self.drawable
        self.drawable = self._drawable_size()
        self.logical = (width, height)
        if self._started_logged and self.drawable != old_drawable:
            log_event("resized", session=self.session, drawable=self.drawable,
                      logical=self.logical, aspect=self.drawable[0] / self.drawable[1])
        before_stall = self.camera.position
        injected = self.stall_pending
        if injected:
            self.stall_pending = False
            time.sleep(.25)  # Deliberate developer experiment, blocks the main thread.
        now = time.perf_counter()
        step = self.clock.tick(now)
        update_started = time.perf_counter()
        active_update = self.input_enabled and self.focused
        if active_update:
            self.camera.move(self.actions, step.delta, analog_pair(*self.axes))
        update_seconds = time.perf_counter() - update_started
        prep_started = time.perf_counter()
        rv = Render(width, height)
        rv.blit(render(self._background, width, height, st, at), (0, 0))
        rv.blit(self._scene(width, height, st, at), (0, 0))
        prep_seconds = time.perf_counter() - prep_started
        self.metrics.record(step, update_seconds, prep_seconds, active=active_update)
        if not self._started_logged:
            data = self.model.load()
            self.model_data_ref = weakref.ref(data)
            for mesh_info in data.mesh_info:
                try:
                    self.mesh_refs.append(weakref.ref(mesh_info.mesh))
                except TypeError:
                    break
            info = renpy.display.draw.info
            log_event("viewport_initialized", session=self.session, version=VERSION,
                      renderer=info.get("renderer"), drawable=self.drawable, logical=self.logical,
                      gl_version=info.get("gpu_driver_version"), gpu=info.get("gpu_name"),
                      asset=ASSET, imported_meshes=len(data.mesh_info),
                      imported_blits=len(data.blit_info), asset_imports=self.model.imports, convention=CONVENTION)
            self._started_logged = True
        if injected:
            distance = sum((a-b)**2 for a,b in zip(self.camera.position, before_stall)) ** .5
            self.last_stall = dict(raw_seconds=step.raw, delta_seconds=step.delta,
                                   discarded_seconds=step.discarded, movement_metres=distance)
            log_event("stall", session=self.session, **self.last_stall)
        # HUD generation is separately outside tree-preparation measurement.
        if self._hud is None or now >= self._hud_until:
            self._hud = Text(self._hud_text(), size=18, color="#eef4ff", substitute=False)
            self._hud_until = now + .25
        rv.blit(render(self._panel, min(width, 1100), 202, st, at), (0, 0))
        rv.blit(render(self._hud, width, height, st, at), (14, 10))
        rv.add_focus(self, None, 0, 0, width, height)
        if now - self._last_log >= 2:
            self._last_log = now
            log_event("periodic", session=self.session, camera=self.camera.position,
                      yaw=self.camera.yaw, pitch=self.camera.pitch, drawable=self.drawable,
                      input_enabled=self.input_enabled, actions=self.actions.active(),
                      mouse_captured=False, **self.metrics.summary())
        redraw(self, self.REDRAW_SECONDS)
        return rv

    def event(self, ev, x, y, st):
        if not self.active:
            return None
        if os.environ.get("CORDEL_VIEWPORT_SELFTEST") == "1" and ev.type in (
                pygame.MOUSEMOTION, pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP):
            log_event("synthetic_mouse_dispatch", type=pygame.event.event_name(ev.type),
                      focused=self.focused, dragging=self.dragging, xy=(x, y),
                      rel=getattr(ev, "rel", None), buttons=getattr(ev, "buttons", None))
        lost_events = (pygame.WINDOWFOCUSLOST, pygame.WINDOWMOUSELEAVE,
                       pygame.WINDOWMINIMIZED, pygame.WINDOWHIDDEN)
        if ev.type in lost_events:
            self.release_input(pygame.event.event_name(ev.type))
            return None
        if ev.type == pygame.CONTROLLERDEVICEREMOVED:
            if getattr(ev, "which", None) == self.controller_id:
                self.axes = (0.0, 0.0)
                self.controller_id = None
            return None
        if ev.type == pygame.KEYUP:
            self.actions.key_up(ev.key)
            if ev.key in self.keys:
                raise api.IgnoreEvent()
        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_ESCAPE:
            self.release_input("escape")
            redraw(self, 0)
            raise api.IgnoreEvent()
        if not self.input_enabled:
            return None
        if ev.type == pygame.MOUSEBUTTONDOWN and ev.button == 1 and x >= 0 and y >= 0:
            self.focused = self.dragging = True
            redraw(self, 0)
            raise api.IgnoreEvent()
        if ev.type == pygame.MOUSEBUTTONUP and ev.button == 1:
            self.dragging = False
            raise api.IgnoreEvent()
        if ev.type == pygame.MOUSEMOTION and self.focused and self.dragging:
            # Relative event delta during ordinary dragging; NOT SDL relative mode.
            if getattr(ev, "buttons", (0, 0, 0))[0]:
                self.camera.look(*ev.rel)
                redraw(self, 0)
            else:
                self.dragging = False
            raise api.IgnoreEvent()
        if ev.type == pygame.KEYDOWN and ev.key == pygame.K_F6 and not getattr(ev, "repeat", False):
            self.stall_pending = True
            redraw(self, 0)
            raise api.IgnoreEvent()
        if self.focused and ev.type == pygame.KEYDOWN and ev.key in self.keys:
            self.actions.key_down(ev.key, self.keys[ev.key])
            raise api.IgnoreEvent()
        if self.focused and ev.type == pygame.CONTROLLERAXISMOTION:
            # Raw SDL signed 16-bit axis magnitude; keep analog state distinct.
            if self.controller_id is None:
                self.controller_id = ev.which
            if self.controller_id == ev.which and ev.axis in (0, 1):
                values = list(self.axes)
                values[ev.axis] = max(-1., min(1., ev.value / 32767.))
                self.axes = tuple(values)
                raise api.IgnoreEvent()
        return None

    def close(self, reason="leave"):
        if not self.active:
            return
        self.release_input(reason)
        self.active = False
        ACTIVE.discard(self)
        if self.model is not None:
            self.imports_at_close = self.model.imports
            self.model.release()
            self.model = None
        self._hud = None
        # This removes scheduled redraw requests on the next engine cache sweep;
        # the inactive render path never reschedules itself.
        redraw(self, 0)
        wall_seconds = time.perf_counter() - self._wall_started
        log_event("viewport_destroyed", session=self.session, reason=reason,
                  engine_frames=renpy.config.frames - self._engine_frame_started,
                  process_cpu_seconds=time.process_time() - self._cpu_started,
                  wall_seconds=wall_seconds, resources=resource_snapshot(),
                  asset_imports=self.imports_at_close,
                  release="own importer entries removed; renderer references/deferred GL deletion unproven",
                  **self.metrics.summary())


def collect_resources():
    gc.collect()
    return resource_snapshot()
