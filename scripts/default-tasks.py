#!/usr/bin/env python3
"""Generate the default wall-task table.

A default task is:

    size        target radius:  normal | small | extra_small
    wall        close | mid | far  (default mid; close/far are appended to the name)
    targets     how many targets are alive at once (fixed count)
    movement    static | strafing | dynamic
    mode        clicking | tracking
    health      1 = one shot, N = N hits, 0 = infinite

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

    @property
    def preset_label(self) -> str:
        return {
            Movement.STATIC: "STATIC",
            Movement.STRAFING: "STRAFE",
            Movement.DYNAMIC: "DYNAMIC",
        }[self]

    @property
    def h_speed(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (1.0, 1.5),
            Movement.DYNAMIC: (1.0, 1.5),
        }[self]

    @property
    def v_speed(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (0.0, 0.0),
            Movement.DYNAMIC: (0.0, 0.75),
        }[self]

    @property
    def accel(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (8.0, 8.0),
            Movement.DYNAMIC: (8.0, 8.0),
        }[self]

    @property
    def dir_change(self) -> tuple[float, float]:
        return {
            Movement.STATIC: (0.0, 0.0),
            Movement.STRAFING: (1.0, 4.0),
            Movement.DYNAMIC: (1.0, 2.0),
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
        parts = [
            f"1W{self.target_count}{self.size.name_suffix}",
            self.movement.preset_label,
        ]
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


# Built-in clicking tasks. Add tracking with mode=Mode.TRACKING.
# Mid wall is the default. Close/far append CLOSE or FAR to the name:
#   Task(size=Size.NORMAL, targets=4, movement=Movement.DYNAMIC, wall=Wall.CLOSE)
DEFAULT_TASKS = [
    Task(size=Size.NORMAL, targets=2, movement=Movement.DYNAMIC),
    Task(size=Size.SMALL, targets=2, movement=Movement.DYNAMIC),
    Task(size=Size.NORMAL, targets=2, movement=Movement.STRAFING),
    Task(size=Size.SMALL, targets=2, movement=Movement.STRAFING),
    Task(size=Size.NORMAL, targets=2, movement=Movement.STATIC),
    Task(size=Size.SMALL, targets=2, movement=Movement.STATIC),
    Task(size=Size.EXTRA_SMALL, targets=2, movement=Movement.STATIC),
    Task(size=Size.NORMAL, targets=4, movement=Movement.DYNAMIC, wall=Wall.CLOSE),
    Task(size=Size.SMALL, targets=4, movement=Movement.DYNAMIC, wall=Wall.CLOSE),
    Task(size=Size.NORMAL, targets=4, movement=Movement.STRAFING, wall=Wall.CLOSE),
    Task(size=Size.SMALL, targets=4, movement=Movement.STRAFING, wall=Wall.CLOSE),
    Task(size=Size.EXTRA_SMALL, targets=4, movement=Movement.STRAFING, wall=Wall.CLOSE),
    Task(size=Size.NORMAL, targets=4, movement=Movement.STATIC, wall=Wall.CLOSE),
    Task(size=Size.SMALL, targets=4, movement=Movement.STATIC, wall=Wall.CLOSE),
    Task(size=Size.EXTRA_SMALL, targets=4, movement=Movement.STATIC, wall=Wall.CLOSE),
]


def _assert_names() -> None:
    # Naming examples are independent of DEFAULT_TASKS so editing that list
    # cannot break `python scripts/default-tasks.py --dump` (RESET TASKS).
    mid = Task(size=Size.NORMAL, targets=3, movement=Movement.DYNAMIC)
    close = Task(size=Size.NORMAL, targets=3, movement=Movement.DYNAMIC, wall=Wall.CLOSE)
    far = Task(size=Size.SMALL, targets=6, movement=Movement.STRAFING, wall=Wall.FAR)
    if mid.preset_name != "1W3T DYNAMIC":
        raise AssertionError(mid.preset_name)
    if close.preset_name != "1W3T DYNAMIC CLOSE":
        raise AssertionError(close.preset_name)
    if far.preset_name != "1W6TS STRAFE FAR":
        raise AssertionError(far.preset_name)


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
            f"{_f(engine['dir_change'][0])}, {_f(engine['dir_change'][1])}"
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
            " {tracking} {health}\n".format(
                targets=engine["targets"],
                wall=engine["wall"],
                radius=engine["radius"],
                h_speed=engine["h_speed"],
                v_speed=engine["v_speed"],
                accel=engine["accel"],
                dir_change=engine["dir_change"],
                tracking=1 if engine["mode"] == Mode.TRACKING.value else 0,
                health=int(engine["health"]),
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
