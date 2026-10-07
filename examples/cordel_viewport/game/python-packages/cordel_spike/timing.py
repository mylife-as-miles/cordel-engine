"""Wall-clock redraw interval measurement. No fixed-update scheduler or GPU timer."""

from collections import deque
from dataclasses import dataclass
import math


@dataclass(frozen=True)
class Step:
    raw: float
    delta: float
    discarded: float


def clamp_delta(raw, maximum=0.05):
    if not math.isfinite(maximum) or maximum <= 0:
        raise ValueError("maximum delta must be finite and positive")
    raw = raw if math.isfinite(raw) and raw > 0 else 0.0
    delta = min(raw, maximum)
    return Step(raw, delta, raw - delta)


class FrameClock:
    def __init__(self, maximum=0.05):
        self.maximum = maximum
        clamp_delta(0, maximum)
        self.previous = None

    def tick(self, now):
        if not math.isfinite(now):
            return clamp_delta(0, self.maximum)
        raw = 0.0 if self.previous is None else now - self.previous
        # A backwards clock sample cannot create a large following delta.
        self.previous = max(now, self.previous) if self.previous is not None else now
        return clamp_delta(raw, self.maximum)


class Metrics:
    def __init__(self):
        self.frames = 0
        self.intervals = deque(maxlen=120)
        self.interval_count = 0
        self.interval_sum = self.interval_worst = 0.0
        self.update_sum = self.update_worst = 0.0
        self.prep_sum = self.prep_worst = 0.0
        self.discarded = 0.0
        self.last = Step(0, 0, 0)
        self.update_ms = self.prep_ms = 0.0

    def record(self, step, update_seconds, prep_seconds):
        self.frames += 1
        self.last = step
        if step.raw > 0:
            self.intervals.append(step.raw)
            self.interval_count += 1
            self.interval_sum += step.raw
            self.interval_worst = max(self.interval_worst, step.raw)
        self.update_ms, self.prep_ms = update_seconds * 1000, prep_seconds * 1000
        self.update_sum += self.update_ms
        self.update_worst = max(self.update_worst, self.update_ms)
        self.prep_sum += self.prep_ms
        self.prep_worst = max(self.prep_worst, self.prep_ms)
        self.discarded += step.discarded

    def summary(self):
        frames = max(1, self.frames)
        return dict(frames=self.frames,
                    fps=len(self.intervals) / sum(self.intervals) if self.intervals else 0,
                    interval_mean_ms=1000 * self.interval_sum / max(1, self.interval_count),
                    interval_worst_ms=1000 * self.interval_worst,
                    update_mean_cpu_ms=self.update_sum / frames,
                    update_worst_cpu_ms=self.update_worst,
                    prep_mean_cpu_ms=self.prep_sum / frames,
                    prep_worst_cpu_ms=self.prep_worst,
                    discarded_seconds=self.discarded)
