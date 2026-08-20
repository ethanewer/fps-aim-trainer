#!/usr/bin/env python3
"""Plumbing for the default wall-task table.

The catalog itself lives in ``default-tasks.py``. This module turns those
``Task`` values into JSON, C++, and the ``--dump`` lines Reset tasks parses.

A task is size, wall, targets, movement, mode, and health. Named sizes, walls,
and movements are defined in ``default-tasks.py`` and fill in radius, distance,
and speeds; any of those (plus bounce physics) can be overridden on a single
task. ``tasks()`` builds a cartesian product so the catalog does not have to
list every combination by hand.
"""

from __future__ import annotations

import inspect
import itertools
import json
import sys
from dataclasses import dataclass, fields
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Iterable, Iterator, Sequence, Union

ROOT = Path(__file__).resolve().parent.parent
JSON_PATH = ROOT / "data" / "default-tasks.json"
INC_PATH = ROOT / "src" / "default_tasks.inc"

Number = Union[int, float]
Range = tuple[Number, Number]
KeepFn = Callable[..., bool]


@dataclass(frozen=True)
class Size:
    id: str
    radius: float
    suffix: str

    @property
    def value(self) -> str:
        return self.id

    @property
    def radius_m(self) -> float:
        return self.radius

    @property
    def name_suffix(self) -> str:
        return self.suffix


@dataclass(frozen=True)
class Wall:
    id: str
    lo: float
    hi: float
    named: bool = True

    @property
    def value(self) -> str:
        return self.id

    @property
    def name(self) -> str:
        return self.id.upper()

    @property
    def distance_m(self) -> tuple[float, float]:
        return (self.lo, self.hi)


@dataclass(frozen=True)
class Movement:
    id: str
    h_speed: tuple[float, float]
    v_speed: tuple[float, float]
    accel: tuple[float, float]
    dir_change: tuple[float, float]
    label: str
    bounce: bool = False

    @property
    def value(self) -> str:
        return self.id

    @property
    def preset_label(self) -> str:
        return self.label


class Mode(Enum):
    CLICKING = "clicking"
    TRACKING = "tracking"


CLICKING = Mode.CLICKING
TRACKING = Mode.TRACKING

_SIZES: dict[str, Size] = {}
_WALLS: dict[str, Wall] = {}
_MOVEMENTS: dict[str, Movement] = {}


@dataclass(frozen=True)
class BounceDefaults:
    angle: tuple[float, float]
    speed: tuple[float, float]
    camera: float
    gravity: float
    dir_change: float


_BOUNCE: BounceDefaults | None = None


def _lookup(table: dict[str, Any], kind: str, name: str) -> Any:
    try:
        return table[name]
    except KeyError as exc:
        raise KeyError(f"unknown {kind} {name!r}; define it in default-tasks.py") from exc


def size(name: str, radius: Number, suffix: str) -> Size:
    item = Size(name.strip().lower(), float(radius), str(suffix))
    _SIZES[item.id] = item
    return item


def wall(name: str, lo: Number, hi: Number, *, named: bool = True) -> Wall:
    item = Wall(name.strip().lower(), float(lo), float(hi), named)
    _WALLS[item.id] = item
    return item


def movement(
    name: str,
    *,
    h: Number | Range,
    v: Number | Range,
    accel: Number | Range,
    dir_change: Number | Range,
    label: str | None = None,
    bounce: bool = False,
) -> Movement:
    ident = name.strip().lower()
    item = Movement(
        ident,
        _pair(h),
        _pair(v),
        _pair(accel),
        _pair(dir_change),
        label if label is not None else ident.upper(),
        bounce,
    )
    _MOVEMENTS[item.id] = item
    return item


def bounce_defaults(
    *,
    angle: Range,
    speed: Range,
    camera: Number,
    gravity: Number,
    dir_change: Number,
) -> BounceDefaults:
    global _BOUNCE
    _BOUNCE = BounceDefaults(
        _pair(angle),
        _pair(speed),
        float(camera),
        float(gravity),
        float(dir_change),
    )
    return _BOUNCE


def _bounce_value(field: str, value: Any, attr: str) -> Any:
    if value is not None:
        return value
    if _BOUNCE is None:
        raise ValueError(f"set {field} on the task or call bounce_defaults() in default-tasks.py")
    return getattr(_BOUNCE, attr)


def _pair(value: Number | Range) -> tuple[float, float]:
    if isinstance(value, (list, tuple)):
        if len(value) != 2:
            raise ValueError(f"range must have two values, got {value!r}")
        return (float(value[0]), float(value[1]))
    number = float(value)
    return (number, number)


def _named_wall(lo: float, hi: float) -> Wall | tuple[float, float]:
    pair = (float(lo), float(hi))
    for item in _WALLS.values():
        if item.distance_m == pair:
            return item
    return pair


def _wall_from_json(value) -> Wall | tuple[float, float]:
    if value is None:
        unnamed = [item for item in _WALLS.values() if not item.named]
        if len(unnamed) == 1:
            return unnamed[0]
        raise ValueError("wall is required")
    if isinstance(value, Wall):
        return value
    if isinstance(value, str):
        return _lookup(_WALLS, "wall", value.strip().lower())
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return _named_wall(value[0], value[1])
    raise ValueError(f"wall must be a named wall or a [min, max] range, got {value!r}")


def _targets_from_json(value) -> int | tuple[int, int]:
    if isinstance(value, (list, tuple)):
        if not value:
            raise ValueError("targets list is empty")
        if len(value) == 1:
            return int(value[0])
        if len(value) != 2:
            raise ValueError(f"targets must be a count or [min, max], got {value!r}")
        lo, hi = int(value[0]), int(value[1])
        return lo if lo == hi else (lo, hi)
    return int(value)


def _optional_range(item: dict, key: str) -> Range | None:
    if key not in item:
        return None
    return _pair(item[key])


def _optional_number(item: dict, key: str) -> float | None:
    if key not in item:
        return None
    return float(item[key])


@dataclass(frozen=True)
class Task:
    size: Size
    targets: Union[int, tuple[int, int]]
    movement: Movement
    mode: Mode = Mode.CLICKING
    health: int = 1
    wall: Union[Wall, tuple[float, float], None] = None
    name: str | None = None
    radius: Union[Number, Range, None] = None
    h_speed: Range | None = None
    v_speed: Range | None = None
    accel: Range | None = None
    dir_change: Range | None = None
    bounce_angle: Range | None = None
    bounce_speed: Range | None = None
    bounce_camera: Number | None = None
    bounce_gravity: Number | None = None
    bounce_dir_change: Number | None = None

    def __post_init__(self) -> None:
        if self.wall is None or isinstance(self.wall, Wall):
            return
        if isinstance(self.wall, (list, tuple)) and len(self.wall) == 2:
            object.__setattr__(self, "wall", _named_wall(self.wall[0], self.wall[1]))
            return
        raise ValueError(f"wall must be a named wall or a [min, max] range, got {self.wall!r}")

    @property
    def target_range(self) -> tuple[int, int]:
        if isinstance(self.targets, (list, tuple)):
            lo, hi = int(self.targets[0]), int(self.targets[1])
        else:
            lo = hi = int(self.targets)
        if lo < 1 or hi < 1:
            raise ValueError(f"targets must be at least 1, got {self.targets!r}")
        if hi < lo:
            raise ValueError(f"targets max must be >= min, got {self.targets!r}")
        return (lo, hi)

    @property
    def target_count(self) -> int:
        return self.target_range[0]

    @property
    def wall_m(self) -> tuple[float, float]:
        resolved = self.wall
        if resolved is None:
            resolved = _wall_from_json(None)
        if isinstance(resolved, Wall):
            return resolved.distance_m
        return (float(resolved[0]), float(resolved[1]))

    @property
    def wall_label(self) -> Wall | None:
        return self.wall if isinstance(self.wall, Wall) else None

    @property
    def preset_name(self) -> str:
        if self.name:
            return self.name
        parts = [
            f"1W{self.target_range[0]}{self.size.name_suffix}",
            self.movement.preset_label,
        ]
        if self.mode is Mode.TRACKING:
            if int(self.health) > 0:
                parts.append("SWITCHING")
                parts.append(str(int(self.health)))
            else:
                parts.append("TRACKING")
        wall = self.wall_label
        if wall is not None and wall.named:
            parts.append(wall.name)
        return " ".join(parts)

    def to_json(self) -> dict:
        lo, hi = self.target_range
        wall = self.wall
        data = {
            "name": self.preset_name,
            "size": self.size.value,
            "wall": wall.value if isinstance(wall, Wall) else [wall[0], wall[1]],
            "targets": lo if lo == hi else [lo, hi],
            "movement": self.movement.value,
            "mode": self.mode.value,
            "health": int(self.health),
        }
        if self.radius is not None:
            data["radius"] = _json_range(self.radius)
        if self.h_speed is not None:
            data["h_speed"] = list(_pair(self.h_speed))
        if self.v_speed is not None:
            data["v_speed"] = list(_pair(self.v_speed))
        if self.accel is not None:
            data["accel"] = list(_pair(self.accel))
        if self.dir_change is not None:
            data["dir_change"] = list(_pair(self.dir_change))
        if self.bounce_angle is not None:
            data["bounce_angle"] = list(_pair(self.bounce_angle))
        if self.bounce_speed is not None:
            data["bounce_speed"] = list(_pair(self.bounce_speed))
        if self.bounce_camera is not None:
            data["bounce_camera"] = float(self.bounce_camera)
        if self.bounce_gravity is not None:
            data["bounce_gravity"] = float(self.bounce_gravity)
        if self.bounce_dir_change is not None:
            data["bounce_dir_change"] = float(self.bounce_dir_change)
        return data

    def to_engine(self) -> dict:
        count = self.target_range
        wall = self.wall_m
        radius = _pair(self.radius if self.radius is not None else self.size.radius_m)
        return {
            "name": self.preset_name,
            "mode": self.mode.value,
            "health": int(self.health),
            "targets": [count[0], count[1]],
            "wall": [wall[0], wall[1]],
            "radius": list(radius),
            "h_speed": list(_pair(self.h_speed if self.h_speed is not None else self.movement.h_speed)),
            "v_speed": list(_pair(self.v_speed if self.v_speed is not None else self.movement.v_speed)),
            "accel": list(_pair(self.accel if self.accel is not None else self.movement.accel)),
            "dir_change": list(
                _pair(self.dir_change if self.dir_change is not None else self.movement.dir_change)
            ),
            "bounce": 1 if self.movement.bounce else 0,
            "bounce_angle": list(_pair(_bounce_value("bounce_angle", self.bounce_angle, "angle"))),
            "bounce_speed": list(_pair(_bounce_value("bounce_speed", self.bounce_speed, "speed"))),
            "bounce_camera": float(_bounce_value("bounce_camera", self.bounce_camera, "camera")),
            "bounce_gravity": float(_bounce_value("bounce_gravity", self.bounce_gravity, "gravity")),
            "bounce_dir_change": float(
                _bounce_value("bounce_dir_change", self.bounce_dir_change, "dir_change")
            ),
        }


_TASK_FIELD_NAMES = {item.name for item in fields(Task)}
_JSON_EXTRA_KEYS = (
    "radius",
    "h_speed",
    "v_speed",
    "accel",
    "dir_change",
    "bounce_angle",
    "bounce_speed",
    "bounce_camera",
    "bounce_gravity",
    "bounce_dir_change",
)
_LOOKUP_AXES = ("movement", "size", "wall", "mode")


def _json_range(value: Number | Range):
    lo, hi = _pair(value)
    return lo if lo == hi else [lo, hi]


def task_from_json(item: dict) -> Task:
    return Task(
        size=_lookup(_SIZES, "size", item["size"]),
        targets=_targets_from_json(item["targets"]),
        movement=_lookup(_MOVEMENTS, "movement", item["movement"]),
        mode=Mode(item.get("mode", Mode.CLICKING.value)),
        health=int(item.get("health", 1)),
        wall=_wall_from_json(item.get("wall")),
        name=item.get("name"),
        radius=item.get("radius"),
        h_speed=_optional_range(item, "h_speed"),
        v_speed=_optional_range(item, "v_speed"),
        accel=_optional_range(item, "accel"),
        dir_change=_optional_range(item, "dir_change"),
        bounce_angle=_optional_range(item, "bounce_angle"),
        bounce_speed=_optional_range(item, "bounce_speed"),
        bounce_camera=_optional_number(item, "bounce_camera"),
        bounce_gravity=_optional_number(item, "bounce_gravity"),
        bounce_dir_change=_optional_number(item, "bounce_dir_change"),
    )


def _call(fn: Callable[..., Any], context: dict[str, Any]) -> Any:
    try:
        signature = inspect.signature(fn)
    except (TypeError, ValueError):
        return fn(**context)
    if any(param.kind == inspect.Parameter.VAR_KEYWORD for param in signature.parameters.values()):
        return fn(**context)
    kwargs = {}
    for name, param in signature.parameters.items():
        if name in context:
            kwargs[name] = context[name]
        elif param.default is inspect.Parameter.empty and param.kind in (
            inspect.Parameter.POSITIONAL_ONLY,
            inspect.Parameter.POSITIONAL_OR_KEYWORD,
            inspect.Parameter.KEYWORD_ONLY,
        ):
            raise TypeError(f"{fn.__name__} needs {name}")
    return fn(**kwargs)


def _is_numeric_range(value: Any) -> bool:
    return (
        isinstance(value, (list, tuple))
        and len(value) == 2
        and all(isinstance(item, (int, float)) and not isinstance(item, bool) for item in value)
    )


def _resolve(value: Any, context: dict[str, Any]) -> Any:
    if callable(value) and not isinstance(value, Enum):
        return _call(value, context)
    if isinstance(value, dict):
        for axis in _LOOKUP_AXES:
            if axis in context and context[axis] in value:
                return value[context[axis]]
        for item in context.values():
            if item in value:
                return value[item]
        raise KeyError(f"no mapping for {context!r} in {value!r}")
    return value


def tasks(*, keep: KeepFn | None = None, skip: KeepFn | None = None, **axes: Any) -> list[Task]:
    """Build tasks from the cartesian product of list arguments.

    Lists are axes. A two-number list or tuple is a min/max range, not an axis.
    Other values are shared across the product: a scalar, a
    ``{axis_value: setting}`` lookup (by movement, size, wall, or mode), or a
    function of those bound fields. ``keep`` / ``skip`` filter combinations.
    """
    axis_names: list[str] = []
    axis_values: list[Sequence[Any]] = []
    derived: dict[str, Any] = {}
    for name, value in axes.items():
        if isinstance(value, list) and not _is_numeric_range(value):
            axis_names.append(name)
            axis_values.append(value)
        else:
            derived[name] = value

    combos: Iterator[tuple[Any, ...]]
    if axis_values:
        combos = itertools.product(*axis_values)
    else:
        combos = iter([()])

    out: list[Task] = []
    for combo in combos:
        bound = dict(zip(axis_names, combo))
        context = dict(bound)
        for name, value in derived.items():
            context[name] = _resolve(value, context)
        if keep is not None and not _call(keep, context):
            continue
        if skip is not None and _call(skip, context):
            continue
        unknown = sorted(name for name in context if name not in _TASK_FIELD_NAMES)
        if unknown:
            raise TypeError("unknown task fields: " + ", ".join(unknown))
        out.append(Task(**context))
    return out


def _assert_names() -> None:
    # Naming examples use local size/wall/movement objects so editing the
    # catalog cannot break `python scripts/default-tasks.py --dump`.
    normal = Size("normal", 0.08, "T")
    small = Size("small", 0.04, "TS")
    mid_wall = Wall("mid", 8.0, 10.0, named=False)
    close_wall = Wall("close", 4.0, 5.0, named=True)
    far_wall = Wall("far", 12.0, 15.0, named=True)
    dynamic = Movement("dynamic", (1.0, 1.5), (0.0, 0.75), (8.0, 8.0), (1.0, 2.0), "DYNAMIC")
    strafe = Movement("strafing", (1.0, 1.5), (0.0, 0.0), (8.0, 8.0), (1.0, 4.0), "STRAFE")
    static = Movement("static", (0.0, 0.0), (0.0, 0.0), (0.0, 0.0), (0.0, 0.0), "STATIC")
    bounce_move = Movement("bounce", (1.0, 1.5), (0.0, 0.75), (0.0, 0.0), (0.0, 0.0), "BOUNCE", True)
    mid = Task(size=normal, targets=3, movement=dynamic, wall=mid_wall)
    close = Task(size=normal, targets=3, movement=dynamic, wall=close_wall)
    far = Task(size=small, targets=6, movement=strafe, wall=far_wall)
    switch = Task(size=normal, targets=2, movement=dynamic, mode=Mode.TRACKING, health=10, wall=mid_wall)
    track = Task(size=small, targets=1, movement=dynamic, mode=Mode.TRACKING, health=0, wall=close_wall)
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
    bounce = Task(size=normal, targets=4, movement=bounce_move, name="THE BOUNCE 180", wall=mid_wall)
    if bounce.preset_name != "THE BOUNCE 180":
        raise AssertionError(bounce.preset_name)
    grid = tasks(
        wall=[mid_wall, close_wall],
        movement=[dynamic],
        size=[normal, small],
        targets=2,
    )
    if [item.preset_name for item in grid] != [
        "1W2T DYNAMIC",
        "1W2TS DYNAMIC",
        "1W2T DYNAMIC CLOSE",
        "1W2TS DYNAMIC CLOSE",
    ]:
        raise AssertionError([item.preset_name for item in grid])
    switched = tasks(
        movement=[dynamic, strafe],
        size=normal,
        targets=1,
        health={dynamic: 20, strafe: 10},
        wall=mid_wall,
    )
    if [item.health for item in switched] != [20, 10]:
        raise AssertionError([item.health for item in switched])
    custom = Task(
        size=normal,
        targets=(3, 5),
        movement=static,
        wall=(7.0, 9.0),
        name="CUSTOM RANGE",
        radius=0.06,
        h_speed=(2.0, 3.0),
        bounce_gravity=9.81,
    )
    engine = custom.to_engine()
    if engine["name"] != "CUSTOM RANGE":
        raise AssertionError(engine["name"])
    if engine["targets"] != [3, 5] or engine["wall"] != [7.0, 9.0]:
        raise AssertionError((engine["targets"], engine["wall"]))
    if engine["radius"] != [0.06, 0.06] or engine["h_speed"] != [2.0, 3.0]:
        raise AssertionError((engine["radius"], engine["h_speed"]))
    if engine["bounce_gravity"] != 9.81:
        raise AssertionError(engine["bounce_gravity"])
    ranged = tasks(
        size=normal,
        targets=4,
        movement=bounce_move,
        name="RANGE LIST",
        bounce_angle=[20, 80],
        wall=[9, 11],
    )
    if len(ranged) != 1:
        raise AssertionError(len(ranged))
    ranged_engine = ranged[0].to_engine()
    if ranged_engine["bounce_angle"] != [20.0, 80.0] or ranged_engine["wall"] != [9.0, 11.0]:
        raise AssertionError((ranged_engine["bounce_angle"], ranged_engine["wall"]))


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


def _json_value(value: Any) -> str:
    if isinstance(value, list):
        return json.dumps(value)
    if isinstance(value, float):
        text = f"{value:.4f}".rstrip("0").rstrip(".")
        return text if text else "0"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    return json.dumps(value)


def _compact_json(task_list: Sequence[Task]) -> str:
    chunks = ['{\n  "tasks": [\n']
    for index, item in enumerate(task_list):
        data = item.to_json()
        comma = "," if index + 1 < len(task_list) else ""
        extras = [(key, data[key]) for key in _JSON_EXTRA_KEYS if key in data]
        health_comma = "," if extras else ""
        chunks.append("    {\n")
        chunks.append(f'      "name": {json.dumps(data["name"])},\n')
        chunks.append(f'      "size": {json.dumps(data["size"])},\n')
        chunks.append(f'      "wall": {json.dumps(data["wall"])},\n')
        targets = data["targets"]
        targets_json = json.dumps(targets) if isinstance(targets, list) else str(int(targets))
        chunks.append(f'      "targets": {targets_json},\n')
        chunks.append(f'      "movement": {json.dumps(data["movement"])},\n')
        chunks.append(f'      "mode": {json.dumps(data["mode"])},\n')
        chunks.append(f'      "health": {int(data["health"])}{health_comma}\n')
        for extra_index, (key, value) in enumerate(extras):
            extra_comma = "," if extra_index + 1 < len(extras) else ""
            chunks.append(f'      {json.dumps(key)}: {_json_value(value)}{extra_comma}\n')
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


def main(task_list: Sequence[Task], argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] in ("-h", "--help"):
        print("Usage: python scripts/default-tasks.py [--from-json | --dump]")
        print("  default     write JSON + C++ from DEFAULT_TASKS in default-tasks.py")
        print("  --from-json rebuild C++ from data/default-tasks.json")
        print("  --dump      print engine lines to stdout (used by RESET TASKS)")
        return 0
    if args and args[0] == "--from-json":
        json_path, inc_path = write_outputs(load_tasks_json(), write_json=False)
        print(f"read  {json_path.relative_to(ROOT)}", file=sys.stderr)
        print(f"wrote {inc_path.relative_to(ROOT)}", file=sys.stderr)
        return 0
    if args and args[0] == "--dump":
        dump_engine_lines(task_list)
        return 0
    json_path, inc_path = write_outputs(task_list)
    print(f"wrote {json_path.relative_to(ROOT)}")
    print(f"wrote {inc_path.relative_to(ROOT)}")
    return 0


__all__ = [
    "CLICKING",
    "INC_PATH",
    "JSON_PATH",
    "ROOT",
    "TRACKING",
    "BounceDefaults",
    "Movement",
    "Mode",
    "Size",
    "Task",
    "Wall",
    "bounce_defaults",
    "dump_engine_lines",
    "load_tasks_json",
    "main",
    "movement",
    "render_inc",
    "size",
    "task_from_json",
    "tasks",
    "wall",
    "write_outputs",
]
