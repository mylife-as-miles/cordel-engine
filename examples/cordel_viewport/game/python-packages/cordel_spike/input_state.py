"""Held actions, independent of event repeats and Ren'Py keymap dispatch."""

import math


class ActionState:
    ACTIONS = frozenset(("forward", "backward", "left", "right", "up", "down", "sprint"))

    def __init__(self):
        self._keys = {}

    def key_down(self, key, action):
        if action not in self.ACTIONS:
            raise ValueError(action)
        self._keys[key] = action

    def key_up(self, key):
        self._keys.pop(key, None)

    def held(self, action):
        return action in self._keys.values()

    def clear(self):
        self._keys.clear()

    def active(self):
        return sorted(set(self._keys.values()))


def analog_pair(x, y, deadzone=0.15):
    """Radial deadzone with continuous rescaled magnitude, not digital actions."""
    if not 0 <= deadzone < 1:
        raise ValueError("deadzone must be in [0, 1)")
    if not (math.isfinite(x) and math.isfinite(y)):
        return (0.0, 0.0)
    length = math.hypot(x, y)
    if length <= deadzone:
        return (0.0, 0.0)
    magnitude = (min(length, 1.0) - deadzone) / (1.0 - deadzone)
    return (x * magnitude / length, y * magnitude / length)
