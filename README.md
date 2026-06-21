# Valorant Aim Trainer

Lightweight native FPS aim trainer prototype.

## Controls

In a scenario:

- Move the mouse to aim. The scenario captures relative mouse input.
- Left click shoots in clicking scenarios.
- Hold left click to score in tracking scenarios.
- Esc returns to the menu.

In the menu:

- Pick a tab (`CLICKING`, `TRACKING`, `GENERAL`) and a preset from the list.
- Every setting is a text box. Click a box (or press `TAB`) to focus it, then type the value.
  The first keystroke replaces the shown number; backspace edits it.
- `TAB` / `SHIFT+TAB` move between boxes, `ENTER` commits, `ESC` cancels editing.
- `START` launches the scenario; `SAVE PRESET` / `SAVE GENERAL` persist settings.
- Esc on the menu (with nothing being edited) quits.

## Valorant Mapping

- Horizontal FOV is locked to 103 degrees.
- The sensitivity value shown in the menu is the Valorant in-game sensitivity number.
- Mouse rotation uses `0.07 degrees per mouse count * Valorant sensitivity`.

## Build

macOS:

```sh
make app
```

The app bundle is written to `~/Desktop/Valorant Aim Trainer.app`.

## Modes And Settings

- Wall clicking: wall distance, target count, radius, horizontal speed, vertical speed, acceleration, and direction-change timing. Set both speed values to `0` for static clicking.
- 360 pill tracking: pill width, min/max distance from the player, movement speed, acceleration, and min/max direction-change timing.
- Static wall spawns enforce center spacing of at least `3 * radius`.
- Settings are saved to `~/.valorant_aim_trainer.cfg` on macOS/Linux.
- User-facing distance, size, speed, and acceleration settings are in meters, meters/second, and meters/second². The current camera height is treated as a 2 meter reference without moving the camera.

Linux:

```sh
sudo apt install libsdl2-dev
make
```

Windows:

Build all `src/*.cpp` files with SDL2 and OpenGL linked. The source uses only SDL2, OpenGL 2.1-era calls, and the C++17 standard library.

## Source Layout

The code is split into focused translation units:

- `math.hpp` — `Vec3` and scalar math helpers.
- `types.hpp` — shared structs, enums, constants, and the `Game` state.
- `world.{hpp,cpp}` — unit conversions, room geometry, camera, and RNG helpers.
- `config.{hpp,cpp}` — settings normalization, presets, and save/load.
- `scenario.{hpp,cpp}` — target spawning, movement, and scenario simulation.
- `render.{hpp,cpp}` — bitmap font, 2D primitives, 3D world, and the in-scenario HUD.
- `menu.{hpp,cpp}` — the text-box settings menu and its editing state machine.
- `selftest.cpp` — the headless `--self-test` suite.
- `main.cpp` — SDL setup, the main loop, and the debug/screenshot modes.

## Debug Screenshots

Render screenshots without manually playing:

```sh
build/aim-trainer --debug-all debug-shots 1920 1080
build/aim-trainer --debug-menu debug-shots/menu.bmp 1920 1080
build/aim-trainer --debug-shot 1 debug-shots/tracking-360.bmp 1920 1080 8
```

The screenshot runner uses the same render path as the app and saves high-DPI BMP captures.
