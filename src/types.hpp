#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "math.hpp"

inline constexpr float HORIZONTAL_FOV_DEG = 103.0f;
inline constexpr float YAW_DEG_PER_COUNT = 0.07f;
inline constexpr float ROOM_WALL_Z = -16.0f;
inline constexpr float ROOM_BACK_Z = 8.0f;
inline constexpr float ROOM_WIDTH = 28.0f;
inline constexpr float ROOM_HEIGHT = 15.75f;
inline constexpr float ROOM_EYE_HEIGHT = ROOM_HEIGHT * 0.5f;
inline constexpr float PLANE_EYE_HEIGHT = 2.2f;
inline constexpr float TRACKING_CAPSULE_HEIGHT = 1.35f;
inline constexpr float PLANE_WALL_HEIGHT = 6.8f;
inline constexpr float CAMERA_REFERENCE_HEIGHT_M = 2.0f;
inline constexpr float TRACKING_ROOM_SIDE_SCALE = 2.5f;
inline constexpr float WALL_TARGET_RADIUS_MIN_M = 0.01f;
inline constexpr float WALL_TARGET_RADIUS_MAX_M = 0.45f;
// Challenge mode: count hits within a fixed time budget. Tracking auto-fires at
// a fixed rate so tracking quality becomes a discrete hit count.
inline constexpr float CHALLENGE_DURATION_SEC = 60.0f;
inline constexpr float TRACKING_FIRE_HZ = 20.0f;

enum class AppMode { Menu, Playing, Results };
enum class ScenarioKind { WallClick, PillTracking };
enum class MapKind { WallRoom, Plane360 };
enum class MenuTab { Clicking, Tracking, General };
enum class RunMode { Practice, Challenge };

// Every editable text box in the menu has a stable id. `None` means nothing is
// being edited. The order within each tab is also the TAB-key navigation order.
enum class FieldId {
    None,
    // Clicking tab
    WallName,
    WallDistMin,
    WallDistMax,
    WallTargetsMin,
    WallTargetsMax,
    WallRadiusMin,
    WallRadiusMax,
    WallHSpeedMin,
    WallHSpeedMax,
    WallVSpeedMin,
    WallVSpeedMax,
    WallAccelMin,
    WallAccelMax,
    WallDirMin,
    WallDirMax,
    // Tracking tab
    PillName,
    PillWidth,
    PillDistMin,
    PillDistMax,
    PillSpeed,
    PillAccel,
    PillDirMin,
    PillDirMax,
    // General tab
    GenSens,
    GenLength,
    GenGap,
    GenThick,
    GenTargetR,
    GenTargetG,
    GenTargetB,
    GenWallR,
    GenWallG,
    GenWallB,
};

inline bool is_tracking(ScenarioKind kind) {
    return kind == ScenarioKind::PillTracking;
}

struct WallClickSettings {
    int target_count_min = 3;
    int target_count_max = 3;
    float wall_distance_min = 6.0f;
    float wall_distance_max = 7.5f;
    float radius_min = 0.08f;
    float radius_max = 0.08f;
    float horizontal_speed_min = 1.0f;
    float horizontal_speed_max = 1.5f;
    float vertical_speed_min = 0.0f;
    float vertical_speed_max = 0.75f;
    float acceleration_min = 5.0f;
    float acceleration_max = 5.0f;
    float change_min = 0.75f;
    float change_max = 1.50f;
};

struct PillTrackingSettings {
    float width = 1.13f;
    float distance_min = 6.80f;
    float distance_max = 9.55f;
    float speed = 3.64f;
    float acceleration = 10.91f;
    float change_min = 0.35f;
    float change_max = 2.4f;
};

struct CrosshairSettings {
    float length = 9.0f;
    float gap = 4.0f;
    float thickness = 2.0f;
};

struct TargetColorSettings {
    int r = 255;
    int g = 70;
    int b = 85;
};

struct WallColorSettings {
    int r = 94;
    int g = 101;
    int b = 109;
};

struct WallPreset {
    std::string name;
    WallClickSettings settings;
};

struct PillPreset {
    std::string name;
    PillTrackingSettings settings;
};

struct ScenarioDef {
    const char* title;
    ScenarioKind kind;
    MapKind map;
    float radius;
    int target_count;
    float speed;
};

struct Target {
    Vec3 pos;
    Vec3 vel;
    Vec3 desired_vel;
    float change_timer = 0.0f;
    float radius;
    float acceleration = 0.0f;
    float distance = 0.0f;  // wall targets: depth in meters (sets the plane bounds)
};

struct Stats {
    int shots = 0;
    int hits = 0;
    float tracking_fire_time = 0.0f;
    float tracking_on_time = 0.0f;
    float elapsed = 0.0f;
};

// One completed challenge run, persisted to the local runs file.
struct RunRecord {
    ScenarioKind kind = ScenarioKind::WallClick;
    std::string preset_name;
    int score = 0;          // hits (the score; accuracy is tracked but not scored)
    int shots = 0;
    float accuracy = 0.0f;  // hits / shots * 100
    float duration = 0.0f;  // seconds
    long long timestamp = 0;  // unix epoch seconds when the run finished
};

struct Input {
    int mouse_x = 0;
    int mouse_y = 0;
    int rel_x = 0;
    int rel_y = 0;
    bool left_pressed = false;
    bool left_down = false;
    bool escape_pressed = false;
    bool backspace_pressed = false;
    bool enter_pressed = false;
    bool tab_pressed = false;
    bool shift_down = false;
    bool quit = false;
    int wheel_y = 0;
    std::string text_input;
};

struct Game {
    AppMode mode = AppMode::Menu;
    std::vector<ScenarioDef> scenarios;
    ScenarioDef scenario{};
    std::vector<Target> targets;
    Stats stats;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float sensitivity = 0.35f;
    CrosshairSettings crosshair;
    TargetColorSettings target_color;
    WallColorSettings wall_color;
    WallClickSettings wall_settings;
    PillTrackingSettings pill_settings;
    std::vector<WallPreset> wall_presets;
    std::vector<PillPreset> pill_presets;
    int selected_wall_preset = 0;
    int selected_pill_preset = 0;
    int wall_preset_scroll = 0;
    int pill_preset_scroll = 0;
    std::string wall_preset_name = "1W3T DYNAMIC";
    std::string pill_preset_name = "SMOOTH PILL";
    MenuTab menu_tab = MenuTab::Clicking;
    // Text-box editing state. `active_field` is the focused box (None = idle);
    // `edit_draft` is the raw text being typed; `edit_fresh` is true right after
    // focusing a numeric box so the first keystroke replaces the shown value.
    FieldId active_field = FieldId::None;
    std::string edit_draft;
    bool edit_fresh = false;
    // Challenge mode state and the locally-saved run history.
    RunMode run_mode = RunMode::Practice;
    float challenge_time_left = 0.0f;
    float fire_accumulator = 0.0f;  // drives the tracking auto-fire rate
    std::vector<RunRecord> runs;
    RunRecord last_run;
    bool mouse_grabbed = false;
    int pending_hit_sounds = 0;
    std::mt19937 rng{std::random_device{}()};
};
