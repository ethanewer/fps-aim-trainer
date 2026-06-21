# AGENTS.md

Guidance for AI agents (and humans) working in this repository. Read this before making changes.

## Project

**Aim Trainer** — a lightweight native FPS aim-trainer prototype.

- **Language / stack:** C++17, SDL2, fixed-function OpenGL (2.1-era calls only). No external
  engine or UI toolkit — everything (3D scene, bitmap font, menu, HUD) is drawn by hand.
- **Platforms:** macOS is primary (SDL2 from Homebrew). Linux and Windows are supported by the
  same source; see `README.md` for per-platform build notes.
- **What it does:** two scenarios — `WALL CLICKING` and `360 PILL TRACKING` — rendered in a 3D
  room. Aim uses a fixed-FOV sensitivity model (horizontal FOV locked to 103°, yaw =
  `0.07° per mouse count × in-game sensitivity`). All user-facing distances/sizes/speeds are in
  meters / m·s⁻¹ / m·s⁻², converted to internal units against a 2 m camera-height reference.
- **Run modes** (`RunMode`): `Practice` (endless) and `Challenge` (a 30 s timed run whose score is
  hits; tracking auto-fires at `TRACKING_FIRE_HZ`). Accuracy is recorded but is not the score.
  `start_scenario` takes the mode; `update_playing` runs the timer/auto-fire and calls
  `finalize_challenge` on expiry, which appends a `RunRecord` and switches to `AppMode::Results`.
- **Settings & presets** persist to `~/.aim_trainer.cfg` (macOS/Linux) or
  `%APPDATA%\aim_trainer.cfg` (Windows). `load_settings` migrates older file formats;
  the self-test guards those migrations.
- **Challenge run history** persists separately to `~/.aim_trainer_runs.cfg` via
  `save_runs`/`load_runs`; `best_run_score` powers the `BEST` readout in the menu and results.

## Source layout

The code is split into small, single-responsibility translation units. Dependency direction flows
downward (upper modules may include lower ones, not vice-versa):

| File | Responsibility |
|------|----------------|
| `src/math.hpp` | `Vec3` + scalar math (header-only, all `inline`). |
| `src/types.hpp` | Shared structs, enums, constants, the `Game` state, and `FieldId` (menu fields). |
| `src/world.{hpp,cpp}` | Meters↔units conversions, room/wall/tracking geometry, camera, RNG, far-plane. |
| `src/config.{hpp,cpp}` | Settings normalization/clamping, presets, and `.cfg` save/load + migration. |
| `src/scenario.{hpp,cpp}` | Target spawning, movement physics, and scenario simulation. |
| `src/render.{hpp,cpp}` | Bitmap font + glyphs, 2D primitives, 3D world, in-scenario HUD. |
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
make app        # build and install "~/Desktop/Aim Trainer.app" (macOS bundle, ad-hoc signed)
```

The Makefile globs `src/*.cpp` and tracks header deps (`-MMD -MP`), so adding a new `.cpp` needs no
Makefile change. Build flags: `-std=c++17 -O3 -Wall -Wextra`.

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
#   tab:   0=CLICKING  1=TRACKING  2=GENERAL
#   state: 0=default  1=empty-name editing  2=long-name editing
#          3=long preset list (scrolled)  4=max-range stress  5=focused numeric box
./build/aim-trainer --debug-menu /tmp/menu.bmp 1920 1080 0 0

# Scenario: --debug-shot <scenario-index> <out.bmp> [width height frames]   (0=wall, 1=pill)
./build/aim-trainer --debug-shot 1 /tmp/pill.bmp 1920 1080 8

# Challenge results screen: --debug-results <out.bmp> [width height scenario]  (0=wall, 1=pill)
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

Then open/Read the PNG. When validating a menu change, render all three tabs plus the relevant
state (e.g. `5` to confirm the focused-box look) and visually confirm alignment, overflow, and
focus highlighting.

## Conventions & gotchas

- **Bitmap font is UPPERCASE-ONLY** with a limited charset: `0-9 A-Z` and `. - + : / % _ ( ) [ ]`.
  There is **no lowercase, comma, `>`, `?`, or arrow glyph** (see `glyph()` in `render.cpp`). Any
  string drawn in the menu/HUD must use only these; write units as `[M]`, `[M/S]`, `[M/S2]`,
  `[SEC]`, `[PX]`.
- **Menu coordinates:** the menu is authored on a virtual ~1040×720 canvas, uniformly scaled by
  `menu_scale` and vertically centered by `voff`. Mouse input is inverse-transformed by the same
  factors in `draw_menu` — if you change the draw transform, change the mouse transform to match.
- **Editing model:** each editable box is a `FieldId`; the focused field's text lives in
  `game.edit_draft` and is committed to the real value on blur/Enter/Tab/focus-change. Numeric
  fields fresh-replace on the first keystroke; names sanitize on commit. `field_desc()` in
  `menu.cpp` maps a `FieldId` to its value, kind, limits, and decimals — extend it there when
  adding a setting.
- **Settings file format is versioned.** If you change what `save_settings` writes, bump the
  `version` and add a migration branch to `load_settings`, then add a self-test that loads the old
  format. Don't silently break existing `.cfg` files.
- **Don't commit build artifacts.** `build/`, `target/`, and `debug-shots/` are gitignored; keep
  them out of commits. The repo tracks only source, `Makefile`, `README.md`, and docs.
- **Git:** branch off `main` before committing; keep commits focused. Run the self-test and a
  warning-clean build before committing.

## Definition of done (checklist for any change)

1. `make clean && make` is warning- and error-free.
2. `./build/aim-trainer --self-test` prints `SELF TEST PASSED`.
3. New/changed behavior has a self-test assertion.
4. UI/render changes are screenshot-reviewed (all affected tabs/states).
5. `README.md` / this file updated if behavior, controls, build, or layout changed.
