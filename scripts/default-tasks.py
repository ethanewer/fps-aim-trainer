#!/usr/bin/env python3
"""Generate the default wall-task table.

A default task is:

    size        target radius:  normal | small | extra_small
    wall        close | mid | far  (default mid; close/far are appended to the name)
    targets     how many targets are alive at once (fixed count)
    movement    static | strafing | dynamic | bounce
    mode        clicking | tracking
    health      1 = one shot, N = N hits, 0 = infinite

Clicking defaults are one-shot. Target-switching defaults are tracking with health
20 on dynamic and 10 on strafe. Tracking defaults are one small dynamic target with
infinite health. Bounce 180 uses a separate in-game editor (spawn radius, jump
angle, takeoff speed, camera height, gravity).

Edit DEFAULT_TASKS, then from the repo root:

    python scripts/default-tasks.py

That writes data/default-tasks.json and src/default_tasks.inc.

To rebuild C++ after editing the JSON by hand:

    python scripts/default-tasks.py --from-json
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass
from enum import Enum
from pathlib import Path
from typing import Iterable, Sequence, Union

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "data" / "default-tasks.json"
INC_PATH = ROOT / "src" / "default_tasks.inc"

Number = Union[int, float]


class Size(Enum):
    NORMAL = "normal"
    SMALL = "small"
    EXTRA_SMALL = "extra_small"

    @property
    def radius_m(self) -> float:
        return {
            Size.NORMAL: 0.08,
            Size.SMALL: 0.04,
            Size.EXTRA_SMALL: 0.02,
        }[self]

    @property
    def name_suffix(self) -> str:
        return {
            Size.NORMAL: "T",
            Size.SMALL: "TS",
            Size.EXTRA_SMALL: "TES",
        }[self]


class Movement(Enum):
    STATIC = "static"
    STRAFING = "strafing"
    DYNAMIC = "dynamic"
    BOUNCE = "bounce"

    @property
    def preset_label(self) -> str:
        return {
            Movement.STATIC: "STATIC",
            Movement.STRAFING: "STRAFE",
            Movement.DYNAMIC: "DYNAMIC",
            Movement.BOUNCE: "BOUNCE",
        }[self]

    @property
    def h_speed(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (1.0, 1.5),
            Movement.DYNAMIC: (1.0, 1.5),
            Movement.BOUNCE: (1.0, 1.5),
        }[self]

    @property
    def v_speed(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (0.0, 0.0),
            Movement.DYNAMIC: (0.0, 0.75),
            Movement.BOUNCE: (0.0, 0.75),
        }[self]

    @property
    def accel(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (8.0, 8.0),
            Movement.DYNAMIC: (8.0, 8.0),
            Movement.BOUNCE: (0.0, 0.0),
        }[self]

    @property
    def dir_change(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (1.0, 4.0),
            Movement.DYNAMIC: (1.0, 2.0),
            Movement.BOUNCE: (0.0, 0.0),
        }[self]


class Mode(Enum):
    CLICKING = "clicking"
    TRACKING = "tracking"


class Wall(Enum):
    CLOSE = "close"
    MID = "mid"
    FAR = "far"

    @property
    def distance_m(self) -> tuple[float, float]:
        return {
            Wall.CLOSE: (4.0, 5.0),
            Wall.MID: (8.0, 10.0),
            Wall.FAR: (12.0, 15.0),
        }[self]


def _wall_from_json(value) -> Wall:
    if value is None:
        return Wall.MID
    if isinstance(value, Wall):
        return value
    if isinstance(value, str):
        return Wall(value.strip().lower())
    if isinstance(value, (list, tuple)) and len(value) == 2:
        lo, hi = float(value[0]), float(value[1])
        for wall in Wall:
            if wall.distance_m == (lo, hi):
                return wall
        raise ValueError(f"wall range {value!r} is not close, mid, or far")
    raise ValueError(f"wall must be close, mid, or far, got {value!r}")


def _targets_from_json(value) -> int:
    if isinstance(value, (list, tuple)):
        if not value:
            raise ValueError("targets list is empty")
        return int(value[0])
    return int(value)


@dataclass(frozen=True)
class Task:
    size: Size
    targets: int
    movement: Movement
    mode: Mode = Mode.CLICKING
    health: int = 1
    wall: Wall = Wall.MID
    name: str | None = None

    @property
    def target_count(self) -> int:
        count = int(self.targets)
        if count < 1:
            raise ValueError(f"targets must be at least 1, got {count}")
        return count

    @property
    def wall_m(self) -> tuple[float, float]:
        return self.wall.distance_m

    @property
    def preset_name(self) -> str:
        if self.name:
            return self.name
        if self.movement is Movement.BOUNCE:
            return "THE BOUNCE 180"
        parts = [
            f"1W{self.target_count}{self.size.name_suffix}",
            self.movement.preset_label,
        ]
        if self.mode is Mode.TRACKING:
            if int(self.health) > 0:
                parts.append("SWITCHING")
                parts.append(str(int(self.health)))
            else:
                parts.append("TRACKING")
        if self.wall is not Wall.MID:
            parts.append(self.wall.name)
        return " ".join(parts)

    def to_json(self) -> dict:
        return {
            "name": self.preset_name,
            "size": self.size.value,
            "wall": self.wall.value,
            "targets": self.target_count,
            "movement": self.movement.value,
            "mode": self.mode.value,
            "health": int(self.health),
        }

    def to_engine(self) -> dict:
        count = self.target_count
        wall = self.wall_m
        radius = self.size.radius_m
        return {
            "name": self.preset_name,
            "mode": self.mode.value,
            "health": int(self.health),
            "targets": [count, count],
            "wall": [wall[0], wall[1]],
            "radius": [radius, radius],
            "h_speed": list(self.movement.h_speed),
            "v_speed": list(self.movement.v_speed),
            "accel": list(self.movement.accel),
            "dir_change": list(self.movement.dir_change),
            "bounce": 1 if self.movement is Movement.BOUNCE else 0,
            "bounce_angle": [30.0, 75.0],
            "bounce_speed": [4.0, 6.0],
            "bounce_camera": 0.25,
            "bounce_gravity": 6.0,
            "bounce_dir_change": 0.0,
        }


def task_from_json(item: dict) -> Task:
    return Task(
        size=Size(item["size"]),
        targets=_targets_from_json(item["targets"]),
        movement=Movement(item["movement"]),
        mode=Mode(item.get("mode", Mode.CLICKING.value)),
        health=int(item.get("health", 1)),
        wall=_wall_from_json(item.get("wall", Wall.MID.value)),
    )


def _keep_size(wall: Wall, movement: Movement, size: Size) -> bool:
    if movement == Movement.DYNAMIC and size == Size.EXTRA_SMALL:
        return False
    if wall == Wall.MID and movement != Movement.STATIC and size == Size.EXTRA_SMALL:
        return False
    return True


def _targets_for_wall(wall: Wall) -> int:
    return 2 if wall == Wall.MID else 4


# Built-in tasks. Mid wall is the default. Close/far append CLOSE or FAR.
# Clicking: one-shot. Switching: tracking copies of dynamic/strafe clicking
# (dynamic health 20, strafe health 10). Tracking: one small dynamic target
# with infinite health.
DEFAULT_TASKS = []

for wall in [Wall.MID, Wall.CLOSE]:
    for movement in [Movement.DYNAMIC, Movement.STRAFING, Movement.STATIC]:
        for size in [Size.NORMAL, Size.SMALL, Size.EXTRA_SMALL]:
            if not _keep_size(wall, movement, size):
                continue
            DEFAULT_TASKS.append(
                Task(size=size, targets=_targets_for_wall(wall), movement=movement, wall=wall)
            )

for wall in [Wall.MID, Wall.CLOSE]:
    for movement in [Movement.DYNAMIC, Movement.STRAFING]:
        for size in [Size.NORMAL, Size.SMALL, Size.EXTRA_SMALL]:
            if not _keep_size(wall, movement, size):
                continue

            health = 20 if movement == Movement.DYNAMIC else 10
            DEFAULT_TASKS.append(
                Task(
                    size=size,
                    targets=_targets_for_wall(wall),
                    movement=movement,
                    mode=Mode.TRACKING,
                    health=health,
                    wall=wall,
                )
            )

for wall in [Wall.MID, Wall.CLOSE]:
    DEFAULT_TASKS.append(
        Task(
            size=Size.SMALL,
            targets=1,
            movement=Movement.DYNAMIC,
            mode=Mode.TRACKING,
            health=0,
            wall=wall,
        )
    )

# KovaaK-style Bounce 180: four balls on cylinders around the player,
# parabolic hops, configurable camera and gravity. Spawn min/max is the
# cylinder radius range (8-10m). Jump angle 30-75°, takeoff 4-6 m/s,
# camera 0.25 m. Gravity defaults to 6 m/s².
DEFAULT_TASKS.append(
    Task(
        size=Size.NORMAL,
        targets=4,
        movement=Movement.BOUNCE,
        name="THE BOUNCE 180",
    )
)


def _assert_names() -> None:
    # Naming examples are independent of DEFAULT_TASKS so editing that list
    # cannot break `python scripts/default-tasks.py --dump` (RESET TASKS).
    mid = Task(size=Size.NORMAL, targets=3, movement=Movement.DYNAMIC)
    close = Task(size=Size.NORMAL, targets=3, movement=Movement.DYNAMIC, wall=Wall.CLOSE)
    far = Task(size=Size.SMALL, targets=6, movement=Movement.STRAFING, wall=Wall.FAR)
    switch = Task(size=Size.NORMAL, targets=2, movement=Movement.DYNAMIC, mode=Mode.TRACKING, health=10)
    track = Task(size=Size.SMALL, targets=1, movement=Movement.DYNAMIC, mode=Mode.TRACKING, health=0, wall=Wall.CLOSE)
    if mid.preset_name != "1W3T DYNAMIC":
        raise AssertionError(mid.preset_name)
    if close.preset_name != "1W3T DYNAMIC CLOSE":
        raise AssertionError(close.preset_name)
    if far.preset_name != "1W6TS STRAFE FAR":
        raise AssertionError(far.preset_name)
    if switch.preset_name != "1W2T DYNAMIC SWITCHING 10":
        raise AssertionError(switch.preset_name)
    if track.preset_name != "1W1TS DYNAMIC TRACKING CLOSE":
        raise AssertionError(track.preset_name)
    bounce = Task(
        size=Size.NORMAL,
        targets=4,
        movement=Movement.BOUNCE,
        name="THE BOUNCE 180",
    )
    if bounce.preset_name != "THE BOUNCE 180":
        raise AssertionError(bounce.preset_name)


def _f(value: Number) -> str:
    text = f"{float(value):.4f}".rstrip("0").rstrip(".")
    if "." not in text:
        text += ".0"
    return text + "f"


def render_inc(task_list: Iterable[Task]) -> str:
    lines = [
        "// Generated by scripts/default-tasks.py. Do not edit by hand.",
        "// Re-run that script after changing default tasks.",
        "",
        "struct DefaultTaskDef {",
        "    const char* name;",
        "    int tracking;",
        "    int health;",
        "    int targets_min;",
        "    int targets_max;",
        "    float wall_min;",
        "    float wall_max;",
        "    float radius_min;",
        "    float radius_max;",
        "    float h_speed_min;",
        "    float h_speed_max;",
        "    float v_speed_min;",
        "    float v_speed_max;",
        "    float accel_min;",
        "    float accel_max;",
        "    float change_min;",
        "    float change_max;",
        "    int bounce;",
        "    float bounce_angle_min;",
        "    float bounce_angle_max;",
        "    float bounce_speed_min;",
        "    float bounce_speed_max;",
        "    float bounce_camera;",
        "    float bounce_gravity;",
        "    float bounce_dir_change;",
        "};",
        "",
        "static const DefaultTaskDef kDefaultTasks[] = {",
    ]
    for item in task_list:
        engine = item.to_engine()
        lines.append(
            "    {"
            f"\"{engine['name']}\", "
            f"{1 if engine['mode'] == Mode.TRACKING.value else 0}, "
            f"{int(engine['health'])}, "
            f"{int(engine['targets'][0])}, {int(engine['targets'][1])}, "
            f"{_f(engine['wall'][0])}, {_f(engine['wall'][1])}, "
            f"{_f(engine['radius'][0])}, {_f(engine['radius'][1])}, "
            f"{_f(engine['h_speed'][0])}, {_f(engine['h_speed'][1])}, "
            f"{_f(engine['v_speed'][0])}, {_f(engine['v_speed'][1])}, "
            f"{_f(engine['accel'][0])}, {_f(engine['accel'][1])}, "
            f"{_f(engine['dir_change'][0])}, {_f(engine['dir_change'][1])}, "
            f"{int(engine['bounce'])}, "
            f"{_f(engine['bounce_angle'][0])}, {_f(engine['bounce_angle'][1])}, "
            f"{_f(engine['bounce_speed'][0])}, {_f(engine['bounce_speed'][1])}, "
            f"{_f(engine['bounce_camera'])}, "
            f"{_f(engine['bounce_gravity'])}, "
            f"{_f(engine['bounce_dir_change'])}"
            "},"
        )
    lines.append("};")
    lines.append("")
    return "\n".join(lines) + "\n"


def _compact_json(task_list: Sequence[Task]) -> str:
    chunks = ['{\n  "tasks": [\n']
    for index, item in enumerate(task_list):
        data = item.to_json()
        comma = "," if index + 1 < len(task_list) else ""
        targets = data["targets"]
        targets_json = json.dumps(targets) if isinstance(targets, list) else str(int(targets))
        chunks.append("    {\n")
        chunks.append(f'      "name": {json.dumps(data["name"])},\n')
        chunks.append(f'      "size": {json.dumps(data["size"])},\n')
        chunks.append(f'      "wall": {json.dumps(data["wall"])},\n')
        chunks.append(f'      "targets": {targets_json},\n')
        chunks.append(f'      "movement": {json.dumps(data["movement"])},\n')
        chunks.append(f'      "mode": {json.dumps(data["mode"])},\n')
        chunks.append(f'      "health": {int(data["health"])}\n')
        chunks.append(f"    }}{comma}\n")
    chunks.append("  ]\n}\n")
    return "".join(chunks)


def load_tasks_json(json_path: Path = JSON_PATH) -> list[Task]:
    payload = json.loads(json_path.read_text(encoding="utf-8"))
    raw_tasks = payload["tasks"] if isinstance(payload, dict) else payload
    return [task_from_json(item) for item in raw_tasks]


def write_outputs(
    task_list: Sequence[Task],
    json_path: Path = JSON_PATH,
    inc_path: Path = INC_PATH,
    write_json: bool = True,
) -> tuple[Path, Path]:
    if not task_list:
        raise ValueError("default task list is empty")
    _assert_names()
    names = [item.preset_name for item in task_list]
    if len(names) != len(set(names)):
        raise ValueError("default task names must be unique")
    too_long = [name for name in names if len(name) > 32]
    if too_long:
        raise ValueError("default task names exceed 32 characters: " + ", ".join(too_long))
    if write_json:
        json_path.parent.mkdir(parents=True, exist_ok=True)
        json_path.write_text(_compact_json(task_list), encoding="utf-8")
    inc_path.write_text(render_inc(task_list), encoding="utf-8")
    return json_path, inc_path


def dump_engine_lines(task_list: Sequence[Task], file=sys.stdout) -> None:
    """Write expanded tasks as `task "NAME" ...` lines for the game to parse."""
    for item in task_list:
        engine = item.to_engine()
        file.write("task ")
        file.write(json.dumps(engine["name"]))
        file.write(
            " {targets[0]} {targets[1]} {wall[0]} {wall[1]} {radius[0]} {radius[1]}"
            " {h_speed[0]} {h_speed[1]} {v_speed[0]} {v_speed[1]}"
            " {accel[0]} {accel[1]} {dir_change[0]} {dir_change[1]}"
            " {tracking} {health} {bounce}"
            " {bounce_angle[0]} {bounce_angle[1]} {bounce_speed[0]} {bounce_speed[1]}"
            " {bounce_camera} {bounce_gravity} {bounce_dir_change}\n".format(
                targets=engine["targets"],
                wall=engine["wall"],
                radius=engine["radius"],
                h_speed=engine["h_speed"],
                v_speed=engine["v_speed"],
                accel=engine["accel"],
                dir_change=engine["dir_change"],
                tracking=1 if engine["mode"] == Mode.TRACKING.value else 0,
                health=int(engine["health"]),
                bounce=int(engine["bounce"]),
                bounce_angle=engine["bounce_angle"],
                bounce_speed=engine["bounce_speed"],
                bounce_camera=engine["bounce_camera"],
                bounce_gravity=engine["bounce_gravity"],
                bounce_dir_change=engine["bounce_dir_change"],
            )
        )


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] in ("-h", "--help"):
        print("Usage: python scripts/default-tasks.py [--from-json | --dump]")
        print("  default     write JSON + C++ from DEFAULT_TASKS in this file")
        print("  --from-json rebuild C++ from data/default-tasks.json")
        print("  --dump      print engine lines to stdout (used by RESET TASKS)")
        return 0
    if args and args[0] == "--from-json":
        json_path, inc_path = write_outputs(load_tasks_json(), write_json=False)
        print(f"read  {json_path.relative_to(ROOT)}", file=sys.stderr)
        print(f"wrote {inc_path.relative_to(ROOT)}", file=sys.stderr)
        return 0
    if args and args[0] == "--dump":
        dump_engine_lines(DEFAULT_TASKS)
        return 0
    json_path, inc_path = write_outputs(DEFAULT_TASKS)
    print(f"wrote {json_path.relative_to(ROOT)}")
    print(f"wrote {inc_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
