# AGENTS.md

Guidance for AI agents (and humans) working in this repository. Read this before making changes.

## Project

**Aim Trainer** — a lightweight native FPS aim-trainer prototype.

- **Language / stack:** C++17, SDL2, fixed-function OpenGL (2.1-era calls only). No external
  engine or UI toolkit — the 3D scene, menu, and HUD are drawn by hand. UI text uses a
  system monospace TrueType font (Consolas on Windows, Menlo on macOS, DejaVu Sans Mono
  on Linux), rasterized with `stb_truetype`.
- **Platforms:** macOS is primary (SDL2 from Homebrew). Linux and Windows are supported by the
  same source; see `README.md` for per-platform build notes.
- **What it does:** wall tasks in a 3D room, each of which can be `CLICKING` or `TRACKING`,
  with `Wall` or `Bounce` motion. Bounce 180 keeps the wall room, sizes left/right/front
  inner faces a ball-radius outside the far spawn cylinder so spheres touch without
  clipping, and runs balls on cylinders around the player. Bounce has
  its own editor: target count, spawn-radius range (also the room size), ball radius,
  initial jump-angle range, initial speed range, camera height, gravity, and a floor
  dir-change probability (default 0). Max jump
  height is a computed readout from max speed, max angle, and gravity. Each ball
  keeps its spawn radius and only bounces off the floor and the visible back wall
  (not a 90° clip at the camera); contact is at the sphere surface with no penetration.
  On a floor bounce, the ball may reverse its horizontal heading with that probability,
  in the same frame as the vertical bounce; it never reverses heading in mid-air.
  Aim uses a fixed-FOV sensitivity model (horizontal FOV locked to 103°, yaw =
  `0.07° per mouse count × in-game sensitivity`). All user-facing distances/sizes/speeds are in
  meters / m·s⁻¹ / m·s⁻², converted to internal units against a 2 m camera-height reference.
- **Run modes** (`RunMode`): `Practice` (endless) and `Challenge` (a `CHALLENGE_DURATION_SEC` timed
  run whose score is hits; tracking auto-fires at `TRACKING_FIRE_HZ`). Accuracy is recorded but is
  not the score. Wall tasks can set target health (`1` = one shot, `N` = N hits, `0` = infinite).
- `start_scenario` takes the mode; `update_playing` runs the timer/auto-fire and calls
  `finalize_challenge` on expiry, which appends a `RunRecord` and switches to `AppMode::Results`.
  A playlist is an ordered list of task names; `start_playlist` runs each as a Challenge,
  shows results between tasks, and a summary after the last. Esc pauses the session;
  `resume_playlist` continues from the unfinished task. Play starts from the first task;
  clicking an already-selected playlist entry starts from that task.
- **Settings & presets** persist to `~/.aim_trainer.cfg` (macOS/Linux) or
  `%APPDATA%\aim_trainer.cfg` (Windows). `load_settings` migrates older file formats;
  the self-test guards those migrations. Playlists are stored in the same file.
- **Challenge run history** persists separately to `~/.aim_trainer_runs.cfg` via
  `save_runs`/`load_runs`; `best_run_score` powers the `Best` readout in the menu and results.

## Source layout

The code is split into small, single-responsibility translation units. Dependency direction flows
downward (upper modules may include lower ones, not vice-versa):

| File | Responsibility |
|------|----------------|
| `src/math.hpp` | `Vec3` + scalar math (header-only, all `inline`). |
| `src/types.hpp` | Shared structs, enums, constants, the `Game` state, and `FieldId` (menu fields). |
| `src/world.{hpp,cpp}` | Meters↔units conversions, room/wall/tracking geometry, camera, RNG, far-plane. |
| `src/config.{hpp,cpp}` | Settings normalization/clamping, presets, and `.cfg` save/load + migration. |
| `src/default_tasks.inc` | Generated default-task table (from `scripts/default-tasks.py`). |
| `scripts/default-tasks.py` | Default-task catalog (edit this to add or change builtins). |
| `scripts/default_tasks.py` | Catalog plumbing: `size`/`wall`/`movement` factories, JSON, C++, `--dump`. |
| `data/default-tasks.json` | Human-readable default-task definitions. |
| `src/scenario.{hpp,cpp}` | Target spawning, movement physics, and scenario simulation. |
| `src/render.{hpp,cpp}` | 2D primitives, 3D world, in-scenario HUD. |
| `src/font.cpp` | System monospace UI font (stb_truetype atlas). |
| `third_party/stb_truetype.h` | Vendored TrueType rasterizer (public domain). |
| `src/menu.{hpp,cpp}` | The text-box settings menu and its editing state machine. |
| `src/selftest.cpp` | The headless `--self-test` suite. |
| `src/main.cpp` | SDL setup, the main loop, and the debug/screenshot CLI modes. |

`normalize_settings()` in `config.cpp` is the single authority for clamping every setting into its
valid range; it runs every menu frame. Prefer writing raw values and letting it clamp, rather than
duplicating limits.

## Build

Requires SDL2 (`brew install sdl2` on macOS). From the repo root:

```sh
make            # build build/aim-trainer (compiles all src/*.cpp, links SDL2 + OpenGL)
make run        # build and run
make clean      # remove build/
make default-tasks # regenerate data/default-tasks.json and src/default_tasks.inc
make app-dev    # build and install "~/Desktop/Aim Trainer Dev.app" (macOS dev bundle)
make app-stable # build and install "~/Desktop/Aim Trainer.app" (macOS stable bundle)
```

The Makefile globs `src/*.cpp` and tracks header deps (`-MMD -MP`), so adding a new `.cpp` needs no
Makefile change. Build flags: `-std=c++17 -O3 -Wall -Wextra`.

### Windows desktop launchers

Windows has two desktop shortcuts with intentionally different behavior:

- `Aim Trainer Dev` launches `scripts/windows-run.ps1`. It checks whether `build/aim-trainer.exe`
  is current, rebuilds when needed, syncs runtime DLLs, and starts the latest dev build.
- `Aim Trainer` launches the pinned stable copy in `dist/Aim Trainer Stable`. It must stay stable
  and must not be updated during normal development.

Only update the stable copy when the user explicitly asks for it. To promote the current verified
build to stable, run:

```powershell
.\scripts\windows-install-shortcut.ps1 -UpdateStable
```

Without `-UpdateStable`, the shortcut installer may refresh shortcuts but must preserve the current
stable executable. After completing work and verifying that it is working and stable, ask the user
whether they want to promote the current dev build to the stable `Aim Trainer` shortcut.

### macOS desktop apps

macOS also has two Desktop apps with intentionally different behavior:

- `Aim Trainer Dev.app` is the normal local-development app. Refresh it with `make app-dev` after
  verified source changes so local manual testing launches the latest build.
- `Aim Trainer.app` is the stable app. It must stay stable and must not be updated during normal
  development unless the user explicitly asks to promote the current verified build.

## Testing & agent dev tools

There is **no external test framework or CI**. Correctness is gated by an in-binary self-test plus
a render-screenshot review. Always run both gates after a change.

### 1. Self-test (logic gate — must pass)

```sh
make && ./build/aim-trainer --self-test     # prints "SELF TEST PASSED" and exits 0 on success
```

Covers settings normalization, preset save/load and legacy-format migration, target spawning &
movement physics, and the menu editing state machine. **Add a test for every behavior you touch.**
Tests live in `src/selftest.cpp` using the `self_test_check(condition, "message")` helper:

```cpp
ok = self_test_check(some_condition, "what this guarantees") && ok;
```

Drive headless logic directly (no window needed): construct a `Game`, call the relevant
`config`/`scenario`/`menu` functions, and assert. For deterministic RNG, seed `game.rng.seed(N)`.
For file I/O tests, set `g_settings_path_override` / `g_runs_path_override` to a temp path and
`std::remove` it after. (Challenge logic is tested by driving `update_playing` in `Challenge` mode
to expiry and asserting the recorded `RunRecord` and the saved/reloaded run history.)

### 2. Lint (warning-clean gate — must stay clean)

There is no separate linter; the lint gate **is** the warning-clean build. Never introduce
`-Wall -Wextra` warnings:

```sh
make clean && make 2>&1 | grep -iE 'warning|error'   # must print nothing
```

`clang-format`/`clang-tidy` are **not** configured. Match the surrounding style by hand: 4-space
indent, braces on the same line, `lower_snake_case` for functions/locals, `PascalCase` for types,
explicit `static_cast`. If you run clang-tidy ad-hoc, treat results as advisory.

### 3. Screenshot review (visual gate for any UI/render change)

The binary can render scenes and menus to BMP without a human at the controls. Use this to review
every menu or rendering change.

```sh
# Menu: --debug-menu <out.bmp> [width height tab state]
#   tab:   0=TASKS  1=PLAYLISTS  2=SETTINGS
#   state: 0=default  1=empty-name editing  2=long-name editing
#          3=long preset list (scrolled)  4=max-range stress  5=focused numeric box
#          6=tracking mode selected  7=task search (strafe)  8=bounce 180 selected
#          Playlists tab: 0=sample playlist  1=empty  2=long name  3=long list
#                         7=add-task search  8=long entry list  9=resume available
#          Settings tab:  0=default  1=center-dot only (length 0, outlines on)
#                         2=inner lines + center dot + outlines
./build/aim-trainer --debug-menu /tmp/menu.bmp 1920 1080 0 0

# Scenario: --debug-shot <scenario-index> <out.bmp> [width height frames]
#   0=clicking  1=tracking  2=bounce 180
./build/aim-trainer --debug-shot 0 /tmp/wall.bmp 1920 1080 8

# Challenge results screen: --debug-results <out.bmp> [width height scenario]
#   scenario: 0=clicking  1=tracking  2=playlist mid  3=playlist complete
./build/aim-trainer --debug-results /tmp/results.bmp 1920 1080 0

# All scenarios into a directory:
./build/aim-trainer --debug-all /tmp/shots 1920 1080
```

**Converting for review:** captures are SDL `RGBA32` BMPs that **`sips` cannot read**. Convert with
Python/PIL, and note the render is **high-DPI (~2× the requested size)** — downscale for viewing:

```sh
python3 - <<'PY'
from PIL import Image
Image.open("/tmp/menu.bmp").convert("RGB").resize((1280, 720)).save("/tmp/menu.png")
PY
```

Then open/Read the PNG. When validating a menu change, render both tabs plus the relevant
state (e.g. `5` to confirm the focused-box look, `6` for tracking selected) and visually confirm
alignment, overflow, and focus highlighting.

## Conventions & gotchas

- **UI font is a system monospace TTF** covering printable ASCII (`32–126`), including
  mixed case, commas, and `=`. Units can be written as `[m]`, `[m/s]`, `[m/s2]`, `[s]`,
  `[px]`. Rasterization lives in `src/font.cpp`; `text_height(scale)` is the em box used
  for vertical centering. If no system font is found, the self-test fails.
- **Menu coordinates:** the menu is authored on a virtual ~1040×840 canvas, uniformly scaled by
  `menu_scale` and vertically centered by `voff`. Mouse input is inverse-transformed by the same
  factors in `draw_menu` — if you change the draw transform, change the mouse transform to match.
  The Tasks sidebar search box is `FieldId::PresetSearch`; it filters presets by case-insensitive
  substring and is first in the Tasks tab order. Playlists tab order is `PlaylistSearch`,
  `PlaylistName`, `PlaylistAddSearch`. Bounce 180 uses a separate tab order after radius:
  jump angle, takeoff speed, camera height, gravity, then dir-change probability. Wall H/V
  speed, accel, and timed dir-change are hidden while Bounce is selected.
- **Editing model:** each editable box is a `FieldId`; the focused field's text lives in
  `game.edit_draft` and is committed to the real value on blur/Enter/Tab/focus-change. Numeric
  fields fresh-replace on the first keystroke; names sanitize on commit. `field_desc()` in
  `menu.cpp` maps a `FieldId` to its value, kind, limits, and decimals — extend it there when
  adding a setting.
- **Wall targets have per-target depth.** Wall clicking distance is a min/max range; each target
  picks its own distance (`Target.distance`) and lives on that depth plane. Distance changes only
  the room depth along the starting view axis: `wall_width_for_distance` and
  `wall_height_for_distance` stay fixed, so increasing distance reduces the angular spawn area.
  The room/far-plane depth is sized to `wall_distance_max`.
- **Settings file format is versioned.** If you change what `save_settings` writes, bump the
  `version` and add a migration branch to `load_settings`, then add a self-test that loads the old
  format. Don't silently break existing `.cfg` files. (Current: `version 18`; v17 files
  without bounce dir-change probability keep 0; v16 files
  without bounce gravity keep 9.81 m/s²; v15 files
  without bounce jump fields keep default angle/speed/camera; v14 Bounce 180
  one-target presets migrate to four balls; v13 Bounce 180
  half-width ranges migrate to 8-10m; v12 files without a
  bounce flag load as wall motion; v12 files without
  outline opacity default to 0.5; v11 three-value
  `crosshair` lines load with outlines and center dot off; v10 files load with
  empty playlists; v9 built-in presets
  migrate to an 8-10m wall range; v8 clicking presets with unused health 0 migrate to one-shot
  health 1; v7 wall presets migrate to clicking with health 1; v4 single wall distance migrates
  to a min==max range. Leftover `pill_preset` lines are ignored.)
  Default tasks live in `scripts/default-tasks.py`. That file also defines size radii,
  wall distances, movement speeds/accel/dir-change, and bounce physics. A task is size,
  wall, targets, movement, mode, and health. Clicking defaults are one-shot.
  Target-switching defaults are tracking copies of the dynamic/strafe clicking
  tasks (health 20 on dynamic, 10 on strafe). Tracking defaults are one small
  dynamic target with infinite health. `BOUNCE 180` is a four-target clicking bounce
  task. Clicking and switching currently use 3 targets on mid and close walls.
  A tracking task with one target and infinite health spawns at the center of the
  spawn rectangle; clicking and switching stay random.
  Mid wall is omitted from the preset name; close and far append `CLOSE` or `FAR`;
  switching names include `SWITCHING` and the health value; tracking names include
  `TRACKING`.
  Generation plumbing (`size()` / `wall()` / `movement()` / `bounce_defaults()` /
  `tasks()`, JSON, C++, `--dump`) lives in `scripts/default_tasks.py`. A catalog
  task can override radius, wall range, speeds, accel, dir-change, and bounce
  physics when the named defaults are not enough.
  Edit `scripts/default-tasks.py` and click **Reset tasks** to re-run that script and
  replace every saved preset. The dumped list becomes the live builtin table for that
  session, so target-count edits (which change preset names) are not overwritten by the
  compiled `default_tasks.inc` table. Run `python scripts/default-tasks.py` (add `--from-json`
  if you edited the JSON) when you want to refresh `src/default_tasks.inc` for the next compile.
- **Don't commit build artifacts.** `build/`, `target/`, `debug-shots/`, and Python `__pycache__/`
  are gitignored; keep them out of commits. Do commit `data/default-tasks.json` and `src/default_tasks.inc` after
  regenerating defaults. The repo tracks source, `Makefile`, `README.md`, docs, and that task table.
- **Git:** branch off `main` before committing; keep commits focused. Run the self-test and a
  warning-clean build before committing.

## Definition of done (checklist for any change)

1. `make clean && make` is warning- and error-free.
2. `./build/aim-trainer --self-test` prints `SELF TEST PASSED`.
3. New/changed behavior has a self-test assertion.
4. UI/render changes are screenshot-reviewed (all affected tabs/states).
5. `README.md` / this file updated if behavior, controls, build, or layout changed.
6. On Windows, ask whether to promote the verified dev build to stable; do not update stable unless
   the user says yes.
7. On macOS, refresh `Aim Trainer Dev.app` after verified source changes; do not update
   `Aim Trainer.app` unless the user explicitly asks for stable promotion.
