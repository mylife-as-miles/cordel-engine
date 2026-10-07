import math
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "game/python-packages"))

from cordel_spike.input_state import ActionState, analog_pair
from cordel_spike.math3d import (ASSIMP_TO_WORLD, IDENTITY, Camera, dot,
                               imported_to_world, multiply, perspective, transform)
from cordel_spike.timing import FrameClock, Metrics, clamp_delta


class CameraTests(unittest.TestCase):
    def assertVector(self, actual, expected):
        for a, e in zip(actual, expected):
            self.assertAlmostEqual(a, e, places=6)

    def test_forward(self):
        self.assertVector(Camera(pitch=0).forward(), (0, 0, -1))

    def test_right(self):
        self.assertVector(Camera(pitch=0).right(), (1, 0, 0))

    def test_yaw_right(self):
        c = Camera(yaw=90, pitch=0)
        self.assertVector(c.forward(), (1, 0, 0))
        self.assertVector(c.right(), (0, 0, 1))

    def test_pitch_and_mouse_sign(self):
        c = Camera(pitch=0)
        c.look(100, -100)
        self.assertEqual((c.yaw, c.pitch), (15, 15))
        self.assertGreater(c.forward()[1], 0)

    def test_pitch_clamp(self):
        c = Camera(pitch=999)
        self.assertEqual(c.pitch, 85)
        c.look(0, 9999)
        self.assertEqual(c.pitch, -85)

    def test_yaw_wrap(self):
        self.assertEqual(Camera(yaw=450).yaw, 90)

    def test_dt_consistency(self):
        actions = ActionState()
        actions.key_down("w", "forward")
        a, b = Camera(pitch=0), Camera(pitch=0)
        for _ in range(30):
            a.move(actions, 1/30)
        for _ in range(120):
            b.move(actions, 1/120)
        self.assertVector(a.position, b.position)
        self.assertVector(a.position, (0, 2.5, 3))

    def test_diagonal_normalization(self):
        actions = ActionState()
        actions.key_down("w", "forward")
        actions.key_down("d", "right")
        c = Camera(position=(0, 0, 0), pitch=0)
        c.move(actions, .1)
        self.assertAlmostEqual(math.sqrt(dot(c.position, c.position)), .3)

    def test_vertical_and_sprint(self):
        actions = ActionState()
        actions.key_down("space", "up")
        actions.key_down("shift", "sprint")
        c = Camera(position=(0, 0, 0))
        c.move(actions, .05)
        self.assertVector(c.position, (0, .45, 0))
        actions.clear()
        actions.key_down("ctrl", "down")
        c.move(actions, .05)
        self.assertVector(c.position, (0, .3, 0))

    def test_opposed_actions_cancel(self):
        actions = ActionState()
        actions.key_down("w", "forward")
        actions.key_down("s", "backward")
        c = Camera()
        original = c.position
        c.move(actions, .1)
        self.assertEqual(c.position, original)

    def test_invalid_delta_cannot_move(self):
        c, actions = Camera(), ActionState()
        actions.key_down("w", "forward")
        original = c.position
        for delta in (0, -1, float("nan"), float("inf")):
            c.move(actions, delta)
        self.assertEqual(c.position, original)

    def test_analog_magnitude_preserved(self):
        c = Camera(position=(0, 0, 0), pitch=0)
        c.move(ActionState(), .1, analog=(.5, 0))
        self.assertVector(c.position, (.15, 0, 0))

    def test_view_maps_camera_to_origin(self):
        c = Camera(position=(2, 3, 4), yaw=32, pitch=27)
        self.assertVector(transform(c.view(), c.position), (0, 0, 0, 1))
        ahead = tuple(p + f for p, f in zip(c.position, c.forward()))
        self.assertVector(transform(c.view(), ahead), (0, 0, -1, 1))


class InputTests(unittest.TestCase):
    def test_down_up_repeat(self):
        s = ActionState()
        s.key_down("w", "forward")
        s.key_down("w", "forward")
        self.assertTrue(s.held("forward"))
        s.key_up("w")
        self.assertFalse(s.held("forward"))
        s.key_up("w")

    def test_multiple_actions(self):
        s = ActionState()
        s.key_down("w", "forward")
        s.key_down("d", "right")
        s.key_up("w")
        self.assertEqual(s.active(), ["right"])

    def test_two_keys_for_same_action(self):
        s = ActionState()
        s.key_down("w", "forward")
        s.key_down("up", "forward")
        s.key_up("w")
        self.assertTrue(s.held("forward"))

    def test_focus_loss_clear(self):
        s = ActionState()
        for key, action in enumerate(s.ACTIONS):
            s.key_down(key, action)
        s.clear()
        self.assertEqual(s.active(), [])

    def test_unknown_action_rejected(self):
        with self.assertRaises(ValueError):
            ActionState().key_down("key", "typo")

    def test_analog_deadzone_and_fraction(self):
        self.assertEqual(analog_pair(.1, 0), (0, 0))
        self.assertAlmostEqual(analog_pair(.575, 0)[0], .5)
        self.assertEqual(analog_pair(1, 0), (1, 0))

    def test_analog_diagonal_bounded(self):
        a = analog_pair(1, 1)
        self.assertAlmostEqual(math.hypot(*a), 1)
        self.assertEqual(analog_pair(float("nan"), 0), (0, 0))


class TimingTests(unittest.TestCase):
    def test_delta_clamp_and_stall(self):
        step = clamp_delta(.25)
        self.assertEqual(step.delta, .05)
        self.assertEqual(step.discarded, .2)

    def test_nonpositive_nonfinite(self):
        for raw in (0, -1, float("nan"), float("inf")):
            self.assertEqual(clamp_delta(raw).delta, 0)

    def test_invalid_maximum(self):
        for limit in (0, -1, float("nan")):
            with self.assertRaises(ValueError):
                clamp_delta(1, limit)

    def test_clock_first_and_backwards(self):
        clock = FrameClock()
        self.assertEqual(clock.tick(10).delta, 0)
        self.assertEqual(clock.tick(9).delta, 0)
        self.assertAlmostEqual(clock.tick(10.01).delta, .01)

    def test_no_catchup_after_stall(self):
        clock = FrameClock()
        clock.tick(0)
        self.assertEqual(clock.tick(.25).delta, .05)
        self.assertAlmostEqual(clock.tick(.26).delta, .01)

    def test_metrics(self):
        metrics = Metrics()
        metrics.record(clamp_delta(.01), .001, .002)
        metrics.record(clamp_delta(.25), .003, .004)
        result = metrics.summary()
        self.assertEqual(result["frames"], 2)
        self.assertAlmostEqual(result["interval_mean_ms"], 130)
        self.assertEqual(result["interval_worst_ms"], 250)
        self.assertEqual(result["update_mean_cpu_ms"], 2)
        self.assertEqual(result["prep_mean_cpu_ms"], 3)


class CoordinateTests(unittest.TestCase):
    def test_reflection_boundary(self):
        self.assertEqual(imported_to_world((2, -3, -4)), (2, 3, -4))
        self.assertEqual(multiply(ASSIMP_TO_WORLD, ASSIMP_TO_WORLD), IDENTITY)

    def test_projection_near_far_and_front(self):
        p = perspective(65, 16/9, .05, 100)
        for depth, expected in ((.05, -1), (100, 1)):
            result = transform(p, (0, 0, -depth))
            self.assertGreater(result[3], 0)
            self.assertAlmostEqual(result[2] / result[3], expected)

    def test_aspect_preserves_pixel_proportions(self):
        for width, height in ((1280, 720), (800, 800), (600, 900)):
            p = perspective(65, width / height, .05, 100)
            self.assertAlmostEqual(p[0] * width, p[5] * height)

    def test_matrix_multiplication_order(self):
        translation = (1.,0.,0.,2., 0.,1.,0.,3., 0.,0.,1.,4., 0.,0.,0.,1.)
        result = transform(multiply(ASSIMP_TO_WORLD, translation), (0, 0, 0))
        self.assertEqual(result, (2, -3, 4, 1))

    def test_invalid_projection(self):
        for arguments in ((0, 1, .1, 10), (65, 0, .1, 10), (65, 1, 10, .1)):
            with self.assertRaises(ValueError):
                perspective(*arguments)


if __name__ == "__main__":
    unittest.main()
