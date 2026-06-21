# Valorant Aim Trainer

Lightweight native FPS aim trainer prototype.

## Controls

- Click `START WALL CLICK` or `START PILL TRACK`.
- Move the mouse to aim. The scenario captures relative mouse input.
- Left click shoots in clicking scenarios.
- Hold left click to score in tracking scenarios.
- Esc returns to the menu. Esc on the menu quits.

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

- Wall clicking: target count, radius, horizontal speed, vertical speed, acceleration, and direction-change timing. Set both speed values to `0` for static clicking.
- 360 pill tracking: pill width, movement speed, acceleration, and min/max direction-change timing.
- Static wall spawns enforce center spacing of at least `3 * radius`.
- Settings are saved to `~/.valorant_aim_trainer.cfg` on macOS/Linux.

Linux:

```sh
sudo apt install libsdl2-dev
make
```

Windows:

Build `src/main.cpp` with SDL2 and OpenGL linked. The source uses only SDL2, OpenGL 2.1-era calls, and the C++17 standard library.

## Debug Screenshots

Render screenshots without manually playing:

```sh
build/aim-trainer --debug-all debug-shots 1920 1080
build/aim-trainer --debug-menu debug-shots/menu.bmp 1920 1080
build/aim-trainer --debug-shot 1 debug-shots/tracking-360.bmp 1920 1080 8
```

The screenshot runner uses the same render path as the app and saves high-DPI BMP captures.
