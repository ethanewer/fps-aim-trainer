# Aim Trainer

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
- `PRACTICE` starts an endless run; `CHALLENGE` starts a timed 30-second run.
  Clicking an already-selected preset in the list also starts a challenge.
- `SAVE PRESET` / `SAVE GENERAL` persist settings.
- Esc on the menu (with nothing being edited) quits.

## Challenge Mode

- A challenge lasts 60 seconds; your score is how many shots you hit.
- Clicking scenarios score on manual clicks. Tracking scenarios auto-fire at 20 Hz, so
  each on-target moment counts as a hit.
- Accuracy (`hits / shots`) is recorded but is not part of the score.
- Every run is saved locally to `~/.aim_trainer_runs.cfg` (time, score, accuracy, shots, ...).
  The results screen shows the run and your best, and the menu shows `BEST` per preset.
- `ESC` aborts a run without recording it.

## Sensitivity Mapping

- Horizontal FOV is locked to 103 degrees.
- The sensitivity value shown in the menu is your in-game sensitivity number.
- Mouse rotation uses `0.07 degrees per mouse count * sensitivity`.

## Build

Requires SDL2.

macOS:

```sh
brew install sdl2
make app
```

The app bundle is written to `~/Desktop/Aim Trainer.app`. Use `make run` to build and run without packaging a bundle.

Linux:

```sh
sudo apt install libsdl2-dev
make
```

Windows:

Install MSYS2, then install the UCRT64 build dependencies:

```powershell
winget install --id MSYS2.MSYS2 -e
C:\msys64\usr\bin\bash.exe -lc "pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-pkgconf make"
```

Build, test, or run from PowerShell:

```powershell
.\scripts\windows-build.ps1
.\scripts\windows-test.ps1
.\scripts\windows-run.ps1
```

`windows-run.ps1` runs `make -q` first and rebuilds only when source files are newer than `build/aim-trainer.exe`.
Install the desktop shortcut with:

```powershell
.\scripts\windows-install-shortcut.ps1
```

The shortcut uses the same launcher, so double-clicking it keeps the playable build current with your latest source changes.

## Modes And Settings

- Wall clicking: wall distance (a min/max range — targets spawn at varying depths, so they can be closer when configured), target count, radius, horizontal speed, vertical speed, acceleration, and direction-change timing. Set both speed values to `0` for static clicking. The room is sized to the maximum wall distance.
- 360 pill tracking: pill width, min/max distance from the player, movement speed, acceleration, and min/max direction-change timing.
- Static wall spawns enforce center spacing of at least `3 * radius`.
- Settings are saved to `~/.aim_trainer.cfg` on macOS/Linux.
- User-facing distance, size, speed, and acceleration settings are in meters, meters/second, and meters/second². The current camera height is treated as a 2 meter reference without moving the camera.

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
