# Shared description of Skerrow Rock: the height field and the key locations.
# Used by build_island_terrain.py (mesh) and build_skerrow_map.py (placing things on it).
# Units are centimetres; sea level is z = 0.

import math

SEED = 1974

# island ellipse semi-axes (about 180 x 130 m)
A = 9000.0
B = 6500.0

PLATEAU = 1500.0
SEABED = -1500.0

# the only gentle slope down to the water, for the landing and pier (degrees, 0 = +X)
LANDING_ANGLE = 215.0
LANDING_SPREAD = 32.0

# flattened building pads: name -> (x, y, radius)
PADS = {
    "lighthouse": (7100.0, 560.0, 800.0),
    "cottage": (-1750.0, 2450.0, 850.0),
    "shed": (1820.0, -1120.0, 550.0),
}

# dirt track from the landing to every building
PATH = [
    (-6510.0, -3290.0),
    (-4340.0, -1890.0),
    (-2660.0, 420.0),
    (-1750.0, 1820.0),
    (-840.0, 420.0),
    (840.0, -490.0),
    (1820.0, -770.0),
    (3640.0, -210.0),
    (5600.0, 350.0),
    (6790.0, 560.0),
]


# --- noise -------------------------------------------------------------------------

def _hash(ix, iy, seed):
    n = (ix * 374761393 + iy * 668265263 + seed * 982451653) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(x, y, seed=SEED):
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = _smooth(x - ix), _smooth(y - iy)
    a = _hash(ix, iy, seed)
    b = _hash(ix + 1, iy, seed)
    c = _hash(ix, iy + 1, seed)
    d = _hash(ix + 1, iy + 1, seed)
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy


def fbm(x, y, octaves=4, seed=SEED):
    """Fractal noise in roughly -1..1"""
    total, amplitude, frequency, norm = 0.0, 1.0, 1.0, 0.0
    for i in range(octaves):
        total += (value_noise(x * frequency, y * frequency, seed + i) * 2.0 - 1.0) * amplitude
        norm += amplitude
        amplitude *= 0.5
        frequency *= 2.0
    return total / norm


def smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 1.0 if x >= edge1 else 0.0
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


# --- shape ---------------------------------------------------------------------------

def angle_of(x, y):
    return math.degrees(math.atan2(y / B, x / A)) % 360.0


def landing_weight(x, y):
    """1 inside the landing cove, 0 elsewhere"""
    diff = abs((angle_of(x, y) - LANDING_ANGLE + 180.0) % 360.0 - 180.0)
    return smoothstep(LANDING_SPREAD, LANDING_SPREAD * 0.4, diff)


def coast_wobble(x, y):
    """Scales the coastline in/out so it is not a perfect ellipse"""
    theta = math.radians(angle_of(x, y))
    return 1.0 + 0.13 * fbm(math.cos(theta) * 3.5 + 7.0, math.sin(theta) * 3.5 + 7.0, 5, SEED + 11)


def radius(x, y):
    """Normalised distance from the centre; the coastline is at 1"""
    return math.sqrt((x / A) ** 2 + (y / B) ** 2) / coast_wobble(x, y)


def base_height(x, y):
    """Height before building pads are flattened"""
    r = radius(x, y)
    landing = landing_weight(x, y)

    top = PLATEAU + 450.0 * fbm(x / 3200.0, y / 3200.0, 4) + 500.0 * max(0.0, 1.0 - r * r)
    # small bumps so the plateau is not flat
    top += 45.0 * fbm(x / 700.0, y / 700.0, 3, SEED + 5)

    # everywhere but the cove: plateau ending in a sheer cliff
    cliff = lerp(SEABED, top, smoothstep(1.0, 0.9, r))

    # the cove: one long ramp from the plateau down to shallow water. Spread over 60% of the
    # radius it stays under ~35 degrees; the old version peaked at 45 and stopped the keeper
    ramp = smoothstep(0.32, 1.02, r)
    cove = lerp(top, -250.0, ramp)
    return lerp(cliff, cove, landing)


_pad_heights = {}


def pad_height(name):
    if name not in _pad_heights:
        x, y, _ = PADS[name]
        _pad_heights[name] = base_height(x, y)
    return _pad_heights[name]


def height(x, y):
    h = base_height(x, y)
    for name, (px, py, pr) in PADS.items():
        d = math.hypot(x - px, y - py)
        w = smoothstep(pr + 700.0, pr, d)
        if w > 0.0:
            h = lerp(h, pad_height(name), w)
    return h


def _segment_distance(px, py, ax, ay, bx, by):
    abx, aby = bx - ax, by - ay
    t = max(0.0, min(1.0, ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby)))
    return math.hypot(px - (ax + abx * t), py - (ay + aby * t))


def dirt(x, y):
    """0..1 amount of bare dirt (paths and around buildings)"""
    d = min(_segment_distance(x, y, *PATH[i], *PATH[i + 1]) for i in range(len(PATH) - 1))
    wobble = 90.0 * fbm(x / 700.0, y / 700.0, 3, SEED + 21)
    amount = smoothstep(260.0 + wobble, 90.0, d)
    for name, (px, py, pr) in PADS.items():
        amount = max(amount, smoothstep(pr + 150.0 + wobble, pr * 0.6, math.hypot(x - px, y - py)) * 0.8)
    return amount


def shore_point(angle_degrees):
    """World XY where the terrain meets sea level (z = 0) along an angle."""
    previous = coast_point(angle_degrees, 0.3)
    r = 0.3
    while r < 1.3:
        r += 0.005
        point = coast_point(angle_degrees, r)
        if height(*point) < 0.0:
            h0, h1 = height(*previous), height(*point)
            t = h0 / (h0 - h1)
            return previous[0] + (point[0] - previous[0]) * t, previous[1] + (point[1] - previous[1]) * t
        previous = point
    return coast_point(angle_degrees, 1.0)


def coast_point(angle_degrees, r=1.0):
    """World XY at normalised radius r along an angle (measured in ellipse space)"""
    theta = math.radians(angle_degrees)
    x, y = A * math.cos(theta), B * math.sin(theta)
    k = coast_wobble(x, y) * r
    return x * k, y * k
