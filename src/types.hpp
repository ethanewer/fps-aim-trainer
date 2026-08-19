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
inline constexpr float BOUNCE_GRAVITY_M = 6.0f;
inline constexpr float BOUNCE_GRAVITY_LEGACY_M = 9.81f;
inline constexpr float BOUNCE_ANGLE_MIN_DEG = 30.0f;
inline constexpr float BOUNCE_ANGLE_MAX_DEG = 75.0f;
inline constexpr float BOUNCE_SPEED_MIN_M = 4.0f;
inline constexpr float BOUNCE_SPEED_MAX_M = 6.0f;
inline constexpr float BOUNCE_CAMERA_HEIGHT_MIN_M = 0.25f;
inline constexpr float BOUNCE_CAMERA_HEIGHT_M = 0.25f;
inline constexpr float BOUNCE_DIR_CHANGE_P = 0.0f;
inline constexpr int BOUNCE_DEFAULT_TARGETS = 4;
inline constexpr float CAMERA_REFERENCE_HEIGHT_M = 2.0f;
inline constexpr float WALL_TARGET_RADIUS_MIN_M = 0.01f;
inline constexpr float WALL_TARGET_RADIUS_MAX_M = 0.45f;
inline constexpr int WALL_TARGET_HEALTH_MAX = 999;
inline constexpr int PRESET_NAME_MAX = 32;
inline constexpr int VISIBLE_PRESET_ROWS = 14;
inline constexpr int VISIBLE_PLAYLIST_ROWS = 14;
inline constexpr int VISIBLE_PLAYLIST_ADD_ROWS = 14;
inline constexpr int VISIBLE_PLAYLIST_ENTRY_ROWS = 14;
// Challenge mode: count hits within a fixed time budget. Tracking auto-fires at
// a fixed rate so tracking quality becomes a discrete hit count.
inline constexpr float CHALLENGE_DURATION_SEC = 60.0f;
inline constexpr float TRACKING_FIRE_HZ = 20.0f;

enum class AppMode { Menu, Playing, Results };
enum class ScenarioKind { WallClick, Tracking };
enum class TaskMode { Clicking, Tracking };
enum class MapKind { WallRoom };
enum class MenuTab { Clicking, Playlists, Settings };
enum class RunMode { Practice, Challenge };

// Every editable text box in the menu has a stable id. `None` means nothing is
// being edited. The order within each tab is also the TAB-key navigation order.
enum class FieldId {
    None,
    // Tasks tab
    PresetSearch,
    WallName,
    WallTargetsMin,
    WallHealth,
    WallDistMin,
    WallDistMax,
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
    BounceAngleMin,
    BounceAngleMax,
    BounceSpeedMin,
    BounceSpeedMax,
    BounceCamera,
    BounceGravity,
    BounceDirChange,
    // Playlists tab
    PlaylistSearch,
    PlaylistName,
    PlaylistAddSearch,
    // Settings tab
    GenSens,
    GenOutlineOpacity,
    GenOutlineThick,
    GenDotThick,
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
    return kind == ScenarioKind::Tracking;
}

inline bool is_tracking(TaskMode mode) {
    return mode == TaskMode::Tracking;
}

struct WallClickSettings {
    TaskMode task_mode = TaskMode::Clicking;
    bool bounce = false;  // cylindrical Bounce 180 in the wall room
    int target_health = 1;  // 0 = infinite; 1 = one shot; N = N hits to kill
    int target_count_min = 3;
    int target_count_max = 3;
    float wall_distance_min = 8.0f;
    float wall_distance_max = 10.0f;
    float radius_min = 0.08f;
    float radius_max = 0.08f;
    float horizontal_speed_min = 1.0f;
    float horizontal_speed_max = 1.5f;
    float vertical_speed_min = 0.0f;
    float vertical_speed_max = 0.75f;
    float acceleration_min = 8.0f;
    float acceleration_max = 8.0f;
    float change_min = 1.0f;
    float change_max = 2.0f;
    float bounce_angle_min = BOUNCE_ANGLE_MIN_DEG;
    float bounce_angle_max = BOUNCE_ANGLE_MAX_DEG;
    float bounce_speed_min = BOUNCE_SPEED_MIN_M;
    float bounce_speed_max = BOUNCE_SPEED_MAX_M;
    float bounce_camera_height_m = BOUNCE_CAMERA_HEIGHT_M;
    float bounce_gravity_m = BOUNCE_GRAVITY_M;
    float bounce_dir_change_p = BOUNCE_DIR_CHANGE_P;
};

inline bool is_bounce(const WallClickSettings& settings) {
    return settings.bounce;
}

struct CrosshairSettings {
    float length = 9.0f;
    float gap = 4.0f;
    float thickness = 2.0f;
    bool outlines = false;
    float outline_opacity = 0.5f;
    float outline_thickness = 1.0f;
    bool center_dot = false;
    float center_dot_thickness = 2.0f;
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

struct Playlist {
    std::string name;
    std::vector<std::string> task_names;  // references WallPreset::name; duplicates allowed
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
    float distance = 0.0f;  // wall: depth in meters; bounce: fixed cylinder radius in meters
    int health = 0;         // remaining hits until respawn; 0 with settings health 0 is infinite
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
    bool space_pressed = false;
    bool space_down = false;
    bool escape_pressed = false;
    bool backspace_pressed = false;
    bool enter_pressed = false;
    bool tab_pressed = false;
    bool shift_down = false;
    bool quit = false;
    int wheel_y = 0;
    std::string text_input;
};

inline bool fire_pressed(const Input& input) {
    return input.left_pressed || input.space_pressed;
}

inline bool fire_down(const Input& input) {
    return input.left_down || input.space_down;
}

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
    std::vector<WallPreset> wall_presets;
    int selected_wall_preset = 0;
    int wall_preset_scroll = 0;
    std::string wall_preset_name = "1W2T DYNAMIC";
    std::string preset_search;
    std::vector<Playlist> playlists;
    int selected_playlist = 0;
    int playlist_scroll = 0;
    std::string playlist_name;
    std::string playlist_search;
    int selected_playlist_entry = 0;
    int playlist_entry_scroll = 0;
    std::string playlist_add_search;
    int playlist_add_scroll = 0;
    bool playlist_active = false;
    bool playlist_paused = false;
    bool playlist_complete = false;
    int playlist_play_index = 0;
    int playlist_play_id = -1;
    std::string playlist_play_name;
    std::vector<std::string> playlist_play_tasks;
    std::vector<RunRecord> playlist_session_runs;
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
