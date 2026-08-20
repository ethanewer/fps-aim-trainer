#!/usr/bin/env python3
"""Default wall-task catalog.

Edit this file, then from the repo root:

    python scripts/default-tasks.py

or click Reset tasks in the menu. That writes data/default-tasks.json and
src/default_tasks.inc. Rebuild C++ from the JSON with --from-json.

`tasks(...)` takes the cartesian product of every list argument. A two-number
list is a min/max range, not an axis. Other arguments are shared: a scalar, a
dict keyed by wall/movement/size/mode, or a function of those. `keep` / `skip`
drop combinations. A task can still override name, radius, speeds, accel,
dir_change, and bounce_* when the named defaults are not enough.
"""

from default_tasks import (
    TRACKING,
    Task,
    bounce_defaults,
    main,
    movement,
    size,
    tasks,
    wall,
)

# Sizes: radius in meters. Suffix is the token in the generated name (T / TS / TES).
EXTRA_LARGE = size("extra_large", 0.32, "EL")
LARGE = size("large", 0.16, "L")
NORMAL = size("normal", 0.08, "T")
SMALL = size("small", 0.04, "TS")
EXTRA_SMALL = size("extra_small", 0.02, "TES")

# Walls: spawn-distance range in meters. Mid is omitted from the preset name.
CLOSE = wall("close", 4, 5)
MID = wall("mid", 8, 10, named=False)
FAR = wall("far", 16, 20)

# Movement: h/v speed (m/s), accel (m/s²), dir-change interval (s).
STATIC = movement("static", h=0, v=0, accel=0, dir_change=0)
STRAFE = movement("strafing", h=(1, 1.5), v=0, accel=8, dir_change=(1, 4), label="STRAFE")
DYNAMIC = movement("dynamic", h=(1, 1.5), v=(0, 0.75), accel=8, dir_change=(1, 2))
BOUNCE = movement("bounce", h=(1, 1.5), v=(0, 0.75), accel=0, dir_change=0, bounce=True)

# Bounce physics used unless a task overrides bounce_*.
bounce_defaults(angle=(30, 75), speed=(4, 6), camera=0.25, gravity=6, dir_change=0)

TARGETS = {MID: 3, CLOSE: 3, FAR: 3}

WALLS = [MID, CLOSE]
SIZES = [NORMAL, SMALL, EXTRA_SMALL]


def keep_size(wall, movement, size):
    """Extra-small is mid-static, plus close/far static and strafe — never dynamic."""
    if size is not EXTRA_SMALL:
        return True
    if movement is DYNAMIC:
        return False
    return wall is not MID or movement is STATIC


DEFAULT_TASKS = [
    # Clicking: one-shot.
    *tasks(
        wall=WALLS,
        movement=[DYNAMIC, STRAFE, STATIC],
        size=SIZES,
        targets=TARGETS,
        keep=keep_size,
    ),
    # Switching: tracking copies of dynamic/strafe clicking.
    *tasks(
        wall=WALLS,
        movement=[DYNAMIC, STRAFE],
        size=SIZES,
        targets=TARGETS,
        mode=TRACKING,
        health={DYNAMIC: 20, STRAFE: 10},
        keep=keep_size,
    ),
    # Tracking: one small dynamic target, infinite health.
    *tasks(
        wall=WALLS,
        size=SMALL,
        targets=1,
        movement=DYNAMIC,
        mode=TRACKING,
        health=0,
    ),
    Task(name="BOUNCE 180", size=NORMAL, targets=6, movement=BOUNCE, wall=MID),
    Task(size=EXTRA_LARGE, targets=3, movement=STATIC, wall=FAR),
]


if __name__ == "__main__":
    raise SystemExit(main(DEFAULT_TASKS))
