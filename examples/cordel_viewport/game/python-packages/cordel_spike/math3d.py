"""Right-handed glTF world space: +Y up, -Z forward, one unit = one metre.

Matrices are flat row-major tuples, applied to column vectors: P * V * M * p.
Only runtime.py knows about Ren'Py's imported Y reflection.
"""

from dataclasses import dataclass
import math


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def multiply(a, b):
    return tuple(sum(a[row * 4 + k] * b[k * 4 + col] for k in range(4))
                 for row in range(4) for col in range(4))


def transform(matrix, point):
    """Return a homogeneous vector, without silently dividing by W."""
    p = (*point, 1.0) if len(point) == 3 else point
    return tuple(dot(matrix[row * 4:row * 4 + 4], p) for row in range(4))


IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)
# Assimp.pyx applies S(-1,-1,-1) * Ry(180): C = C^-1.
ASSIMP_TO_WORLD = (1., 0., 0., 0., 0., -1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.)


def imported_to_world(point):
    """Ren'Py GLTFModel's imported space -> authored glTF/CORDEL world space."""
    return transform(ASSIMP_TO_WORLD, point)[:3]


def perspective(fov_y, aspect, near, far):
    if not (0 < fov_y < 180 and aspect > 0 and 0 < near < far):
        raise ValueError("Invalid perspective frustum")
    f = 1.0 / math.tan(math.radians(fov_y) / 2.0)
    return (f / aspect, 0., 0., 0.,
            0., f, 0., 0.,
            0., 0., (far + near) / (near - far), 2 * far * near / (near - far),
            0., 0., -1., 0.)


@dataclass
class Camera:
    position: tuple = (0.0, 2.5, 6.0)
    yaw: float = 0.0
    pitch: float = -8.0
    field_of_view: float = 65.0
    near_plane: float = 0.05
    far_plane: float = 100.0
    movement_speed: float = 3.0
    mouse_sensitivity: float = 0.15  # degrees per SDL motion pixel
    sprint_multiplier: float = 3.0
    pitch_limit: float = 85.0

    def __post_init__(self):
        self.look(0, 0)

    def forward(self):
        y, p = math.radians(self.yaw), math.radians(self.pitch)
        return (math.sin(y) * math.cos(p), math.sin(p), -math.cos(y) * math.cos(p))

    def right(self):
        y = math.radians(self.yaw)
        return (math.cos(y), 0.0, math.sin(y))

    def look(self, dx, dy):
        self.yaw = (self.yaw + dx * self.mouse_sensitivity + 180) % 360 - 180
        self.pitch = max(-self.pitch_limit, min(self.pitch_limit,
                         self.pitch - dy * self.mouse_sensitivity))

    def move(self, actions, delta, analog=(0.0, 0.0)):
        """Fly camera. Bound combined movement, retaining sub-unit analog magnitude."""
        if not math.isfinite(delta) or delta <= 0:
            return
        forward = float(actions.held("forward")) - actions.held("backward") - analog[1]
        right = float(actions.held("right")) - actions.held("left") + analog[0]
        up = float(actions.held("up")) - actions.held("down")
        f, r = self.forward(), self.right()
        direction = tuple(f[i] * forward + r[i] * right + (up if i == 1 else 0)
                          for i in range(3))
        length = math.sqrt(dot(direction, direction))
        scale = self.movement_speed * delta / max(1.0, length)
        if actions.held("sprint"):
            scale *= self.sprint_multiplier
        self.position = tuple(p + d * scale for p, d in zip(self.position, direction))

    def view(self):
        right, forward = self.right(), self.forward()
        up = cross(right, forward)
        back = tuple(-v for v in forward)
        return (*right, -dot(right, self.position),
                *up, -dot(up, self.position),
                *back, -dot(back, self.position),
                0., 0., 0., 1.)

    def projection(self, width, height):
        return perspective(self.field_of_view, max(1, width) / max(1, height),
                           self.near_plane, self.far_plane)
