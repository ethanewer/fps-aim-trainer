# Aim Trainer

Lightweight native FPS aim trainer prototype.

## Controls

In a scenario:

- Move the mouse to aim. The scenario captures relative mouse input.
- Left click or Space shoots in clicking scenarios.
- Hold left click or Space to score in tracking scenarios.
- Hits play a short, crisp confirmation sound.
- Esc returns to the menu.

In the menu:

- Pick a tab (`TASKS`, `PLAYLISTS`, `SETTINGS`) and a preset or playlist from the list.
- Every setting is a text box. Click a box (or press `TAB`) to focus it, then type the value.
  The first keystroke replaces the shown number; backspace edits it.
- `TAB` / `SHIFT+TAB` move between boxes, `ENTER` commits, `ESC` cancels editing.
- The Tasks sidebar has a search box that filters presets by name as you type.
- Each task can be clicking or tracking. Both modes have a Health setting (`1` = one shot, `N` = N hits, `0` = infinite).
- **Reset tasks** replaces every saved task with the list in `scripts/default-tasks.py`.
- **Practice** starts an endless run; **Challenge** starts a timed 60-second run.
  Clicking an already-selected preset in the list also starts a challenge.
- **New** copies the current selected/editor values in that tab.
- **Save preset** / **Save playlist** / **Save settings** persist settings. Settings include
  sensitivity, a Valorant-style crosshair (inner lines, optional center dot, optional black
  outlines), target color, and wall color. Inner line length `0` hides the four arms.
- Playlists are ordered lists of existing tasks. Add tasks from the catalog, reorder with
  **Up** / **Down**, and **Play** runs each one as a 60-second challenge from the start,
  with results between tasks and a summary at the end. Clicking an already-selected playlist
  also starts it. Clicking an already-selected task in the playlist starts from that task.
  Esc during a playlist returns to the menu without losing progress; **Resume** continues
  from the unfinished task and keeps scores from tasks already finished in that session.
- Esc on the menu (with nothing being edited) quits.

## Challenge Mode

- A challenge lasts 60 seconds; your score is how many shots you hit.
- Clicking scenarios score on manual clicks. Tracking scenarios auto-fire at 20 Hz, so
  each on-target moment counts as a hit.
- Accuracy (`hits / shots`) is recorded but is not part of the score.
- Every run is saved locally to `~/.aim_trainer_runs.cfg` (time, score, accuracy, shots, ...).
  The results screen shows the run and your best, and the menu shows **Best** per preset.
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
make app-dev
```

The dev app bundle is written to `~/Desktop/Aim Trainer Dev.app`. It is the normal local-development app and should be refreshed after source changes. The stable app bundle is `~/Desktop/Aim Trainer.app`; update it only when intentionally promoting a verified build:

```sh
make app-stable
```

`make app` is an alias for `make app-stable`. Use `make run` to build and run without packaging a bundle.

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

This installs/refreshes `Aim Trainer Dev`, which uses the same launcher, so double-clicking it keeps the playable build current with your latest source changes. The stable `Aim Trainer` shortcut points at a pinned copy under `dist\Aim Trainer Stable` and is not updated unless you explicitly promote the current build:

```powershell
.\scripts\windows-install-shortcut.ps1 -UpdateStable
```

## Modes And Settings

- Wall tasks: wall distance (a min/max range — targets spawn at varying depths, so they can be closer when configured), a fixed target count, radius, horizontal speed, vertical speed, acceleration, and direction-change timing. Each direction change re-samples speed and acceleration from their min/max ranges. Set both speed values to `0` for static targets. Distance only changes room depth along the starting view axis; wall width/height stay fixed, so farther targets occupy a smaller angular spawn area. A tracking task with one target and infinite health spawns that target at the center of the spawn rectangle; clicking and target-switching keep random spawns.
- Each wall task can be clicking or tracking. Tracking auto-fires at 20 Hz in challenge mode (score = hits). Target health is how many hits destroy a target in either mode (`1` = one shot, `N` = N hits, `0` = infinite / never despawns). Clicking with health `0` is click-tracking: every click on the target scores, but the target stays.
- Default presets come from `scripts/default-tasks.py`. A default task is size, wall (`close` / `mid` / `far`), a fixed target count, movement (`static` / `strafing` / `dynamic`), mode (`clicking` / `tracking`), and health. Clicking defaults are one-shot. Target-switching defaults are tracking copies of the dynamic/strafe clicking tasks (health `20` on dynamic, `10` on strafe). Tracking defaults are one small dynamic target with infinite health. Mid wall is omitted from the name; close and far append `CLOSE` or `FAR`; switching names include `SWITCHING` and the health value; tracking names include `TRACKING`. Edit that Python file and click **Reset tasks** to reload it immediately (no rebuild). `python scripts/default-tasks.py` still regenerates the committed JSON/C++ table for the next compile.
- Static wall spawns enforce center spacing of at least `3 * radius`.
- Settings are saved to `~/.aim_trainer.cfg` on macOS/Linux.
- The Settings tab crosshair is Valorant-style: optional black outlines (opacity and thickness),
  an optional square center dot, and inner lines whose length, thickness, and offset (gap) can
  be edited. Set inner line length to `0` to hide the four arms and keep only the center dot.
- User-facing distance, size, speed, and acceleration settings are in meters, meters/second, and meters/second². The current camera height is treated as a 2 meter reference without moving the camera.

## Source Layout

The code is split into focused translation units:

- `math.hpp` — `Vec3` and scalar math helpers.
- `types.hpp` — shared structs, enums, constants, and the `Game` state.
- `world.{hpp,cpp}` — unit conversions, room geometry, camera, and RNG helpers.
- `config.{hpp,cpp}` — settings normalization, presets, and save/load.
- `default_tasks.inc` — generated built-in task table (`python scripts/default-tasks.py`).
- `data/default-tasks.json` — human-readable default task definitions.
- `scenario.{hpp,cpp}` — target spawning, movement, and scenario simulation.
- `render.{hpp,cpp}` — 2D primitives, 3D world, and the in-scenario HUD.
- `font.cpp` — system monospace UI text (Consolas / Menlo / DejaVu).
- `menu.{hpp,cpp}` — the text-box settings menu and its editing state machine.
- `selftest.cpp` — the headless `--self-test` suite.
- `main.cpp` — SDL setup, the main loop, and the debug/screenshot modes.

## Debug Screenshots

Render screenshots without manually playing:

```sh
build/aim-trainer --debug-all debug-shots 1920 1080
build/aim-trainer --debug-menu debug-shots/menu.bmp 1920 1080
build/aim-trainer --debug-shot 1 debug-shots/tracking.bmp 1920 1080 8
```

The screenshot runner uses the same render path as the app and saves high-DPI BMP captures.
