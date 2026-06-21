#include <SDL2/SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr float VALORANT_HORIZONTAL_FOV_DEG = 103.0f;
static constexpr float VALORANT_YAW_DEG_PER_COUNT = 0.07f;
static constexpr float ROOM_WALL_Z = -16.0f;
static constexpr float ROOM_BACK_Z = 8.0f;
static constexpr float ROOM_WIDTH = 28.0f;
static constexpr float ROOM_HEIGHT = 15.75f;
static constexpr float ROOM_EYE_HEIGHT = ROOM_HEIGHT * 0.5f;
static constexpr float PLANE_EYE_HEIGHT = 2.2f;
static constexpr float TRACKING_CAPSULE_HEIGHT = 1.35f;
static constexpr float PLANE_HALF_SIZE = 15.0f;
static constexpr float PLANE_WALL_HEIGHT = 6.8f;
static std::string g_settings_path_override;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

static Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }

static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static float length(Vec3 a) { return std::sqrt(dot(a, a)); }
static Vec3 normalize(Vec3 a) {
    float len = length(a);
    return len > 0.00001f ? a / len : Vec3{0.0f, 0.0f, -1.0f};
}
static float clampf(float value, float low, float high) {
    return std::max(low, std::min(value, high));
}
static float deg_to_rad(float deg) { return deg * static_cast<float>(M_PI) / 180.0f; }
static float rad_to_deg(float rad) { return rad * 180.0f / static_cast<float>(M_PI); }

enum class AppMode { Menu, Playing };
enum class ScenarioKind { WallClick, PillTracking };
enum class MapKind { WallRoom, Plane360 };
enum class MenuTab { Clicking, Tracking, General };
enum class EditField { None, WallPresetName, PillPresetName };

static bool is_tracking(ScenarioKind kind) {
    return kind == ScenarioKind::PillTracking;
}

struct WallClickSettings {
    int target_count_min = 5;
    int target_count_max = 5;
    float radius_min = 0.34f;
    float radius_max = 0.34f;
    float horizontal_speed_min = 4.0f;
    float horizontal_speed_max = 4.0f;
    float vertical_speed_min = 0.0f;
    float vertical_speed_max = 2.4f;
    float acceleration_min = 20.0f;
    float acceleration_max = 20.0f;
    float change_min = 0.90f;
    float change_max = 2.50f;
};

struct PillTrackingSettings {
    float width = 1.24f;
    float speed = 4.0f;
    float acceleration = 12.0f;
    float change_min = 0.35f;
    float change_max = 2.4f;
};

struct CrosshairSettings {
    float length = 9.0f;
    float gap = 4.0f;
    float thickness = 2.0f;
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
};

struct Stats {
    int shots = 0;
    int hits = 0;
    float tracking_fire_time = 0.0f;
    float tracking_on_time = 0.0f;
    float elapsed = 0.0f;
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
    float valorant_sens = 0.35f;
    CrosshairSettings crosshair;
    WallClickSettings wall_settings;
    PillTrackingSettings pill_settings;
    std::vector<WallPreset> wall_presets;
    std::vector<PillPreset> pill_presets;
    int selected_wall_preset = 0;
    int selected_pill_preset = 0;
    int wall_preset_scroll = 0;
    int pill_preset_scroll = 0;
    std::string wall_preset_name = "PASU FIVE";
    std::string pill_preset_name = "SMOOTH PILL";
    MenuTab menu_tab = MenuTab::Clicking;
    EditField edit_field = EditField::None;
    bool mouse_grabbed = false;
    std::mt19937 rng{std::random_device{}()};
};

static float rand_range(Game& game, float low, float high) {
    std::uniform_real_distribution<float> dist(low, high);
    return dist(game.rng);
}

static Vec3 camera_pos(const Game& game) {
    if (game.scenario.map == MapKind::Plane360) {
        return {0.0f, PLANE_EYE_HEIGHT, 0.0f};
    }
    return {0.0f, ROOM_EYE_HEIGHT, ROOM_BACK_Z - 1.5f};
}

static Vec3 forward_dir(const Game& game) {
    return normalize({
        std::sin(game.yaw) * std::cos(game.pitch),
        std::sin(game.pitch),
        -std::cos(game.yaw) * std::cos(game.pitch),
    });
}

static float random_sign(Game& game) {
    return rand_range(game, 0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
}

static float rand_wall_range(Game& game, float low, float high) {
    if (high <= low) {
        return low;
    }
    return rand_range(game, low, high);
}

static int rand_wall_int_range(Game& game, int low, int high) {
    if (high <= low) {
        return low;
    }
    std::uniform_int_distribution<int> dist(low, high);
    return dist(game.rng);
}

static Vec3 wall_desired_velocity(Game& game) {
    float h_speed = rand_wall_range(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max);
    float v_speed = rand_wall_range(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max);
    return {
        random_sign(game) * h_speed,
        random_sign(game) * v_speed,
        0.0f,
    };
}

static float wall_change_timer(Game& game) {
    if (game.wall_settings.change_max <= 0.0f) {
        return 1.0e9f;
    }
    return rand_wall_range(game, game.wall_settings.change_min, game.wall_settings.change_max);
}

static int wall_capacity_for_radius(float radius) {
    float min_x = -ROOM_WIDTH * 0.44f + radius;
    float max_x = ROOM_WIDTH * 0.44f - radius;
    float min_y = ROOM_HEIGHT * 0.16f + radius;
    float max_y = ROOM_HEIGHT * 0.84f - radius;
    float spacing = radius * 3.0f;
    int cols = std::max(1, static_cast<int>(std::floor((max_x - min_x) / spacing)) + 1);
    int rows = std::max(1, static_cast<int>(std::floor((max_y - min_y) / spacing)) + 1);
    return std::max(1, std::min(18, cols * rows));
}

static float wall_spacing_for_radii(float a, float b) {
    return a + b + std::min(a, b);
}

static void normalize_float_range(float& low, float& high, float min_allowed, float max_allowed) {
    low = clampf(low, min_allowed, max_allowed);
    high = clampf(high, min_allowed, max_allowed);
    if (high < low) {
        high = low;
    }
}

static void normalize_int_range(int& low, int& high, int min_allowed, int max_allowed) {
    low = std::max(min_allowed, std::min(low, max_allowed));
    high = std::max(min_allowed, std::min(high, max_allowed));
    if (high < low) {
        high = low;
    }
}

static std::string sanitize_preset_name(const std::string& raw) {
    std::string name;
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        char up = static_cast<char>(std::toupper(uc));
        if ((up >= 'A' && up <= 'Z') || (up >= '0' && up <= '9') || up == ' ' || up == '-' || up == '_') {
            if (name.size() < 22) {
                name.push_back(up);
            }
        }
    }
    while (!name.empty() && name.front() == ' ') {
        name.erase(name.begin());
    }
    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    return name.empty() ? "UNTITLED" : name;
}

static bool is_allowed_preset_char(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    char up = static_cast<char>(std::toupper(uc));
    return (up >= 'A' && up <= 'Z') || (up >= '0' && up <= '9') || up == ' ' || up == '-' || up == '_';
}

static std::string filter_preset_name_draft(const std::string& raw) {
    std::string name;
    for (char c : raw) {
        if (is_allowed_preset_char(c) && name.size() < 22) {
            name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    while (!name.empty() && name.front() == ' ') {
        name.erase(name.begin());
    }
    return name;
}

template <typename PresetList>
static std::string unique_preset_name(const PresetList& presets, const std::string& requested, int skip_index) {
    std::string base = sanitize_preset_name(requested);
    auto name_taken = [&](const std::string& candidate) {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (i != skip_index && presets[i].name == candidate) {
                return true;
            }
        }
        return false;
    };
    if (!name_taken(base)) {
        return base;
    }

    for (int suffix = 2; suffix < 1000; ++suffix) {
        std::string tail = " " + std::to_string(suffix);
        std::string trimmed = base;
        if (trimmed.size() + tail.size() > 22) {
            trimmed.resize(22 - tail.size());
            while (!trimmed.empty() && trimmed.back() == ' ') {
                trimmed.pop_back();
            }
        }
        std::string candidate = sanitize_preset_name(trimmed + tail);
        if (!name_taken(candidate)) {
            return candidate;
        }
    }
    return base;
}

static void normalize_wall_settings(Game& game, WallClickSettings& settings) {
    normalize_float_range(settings.radius_min, settings.radius_max, 0.12f, 0.9f);
    int capacity = wall_capacity_for_radius(settings.radius_max);
    normalize_int_range(settings.target_count_min, settings.target_count_max, 1, capacity);
    normalize_float_range(settings.horizontal_speed_min, settings.horizontal_speed_max, 0.0f, 12.0f);
    normalize_float_range(settings.vertical_speed_min, settings.vertical_speed_max, 0.0f, 12.0f);
    normalize_float_range(settings.acceleration_min, settings.acceleration_max, 0.0f, 80.0f);
    normalize_float_range(settings.change_min, settings.change_max, 0.0f, 12.0f);
    (void)game;
}

static void normalize_pill_settings(PillTrackingSettings& settings) {
    settings.width = clampf(settings.width, 0.35f, 2.4f);
    settings.speed = clampf(settings.speed, 0.0f, 14.0f);
    settings.acceleration = clampf(settings.acceleration, 0.0f, 80.0f);
    settings.change_min = clampf(settings.change_min, 0.05f, 8.0f);
    settings.change_max = clampf(settings.change_max, settings.change_min, 12.0f);
}

static void normalize_crosshair(CrosshairSettings& settings) {
    settings.length = clampf(settings.length, 4.0f, 24.0f);
    settings.gap = clampf(settings.gap, 0.0f, 16.0f);
    settings.thickness = clampf(settings.thickness, 1.0f, 6.0f);
}

static void normalize_settings(Game& game) {
    normalize_wall_settings(game, game.wall_settings);
    normalize_pill_settings(game.pill_settings);
    normalize_crosshair(game.crosshair);
    game.valorant_sens = clampf(game.valorant_sens, 0.001f, 10.0f);
    for (WallPreset& preset : game.wall_presets) {
        preset.name = sanitize_preset_name(preset.name);
        if (preset.name == "MIGRATED CLICK") {
            preset.name = "PASU FIVE";
        }
        normalize_wall_settings(game, preset.settings);
    }
    for (PillPreset& preset : game.pill_presets) {
        preset.name = sanitize_preset_name(preset.name);
        if (preset.name == "MIGRATED PILL") {
            preset.name = "SMOOTH PILL";
        }
        normalize_pill_settings(preset.settings);
    }
}

static void ensure_presets(Game& game) {
    if (game.wall_presets.empty()) {
        game.wall_presets.push_back({"PASU FIVE", game.wall_settings});
        WallClickSettings statics = game.wall_settings;
        statics.horizontal_speed_min = 0.0f;
        statics.horizontal_speed_max = 0.0f;
        statics.vertical_speed_min = 0.0f;
        statics.vertical_speed_max = 0.0f;
        statics.acceleration_min = 0.0f;
        statics.acceleration_max = 0.0f;
        statics.change_min = 0.0f;
        statics.change_max = 0.0f;
        game.wall_presets.push_back({"STATIC FIVE", statics});
    }
    if (game.pill_presets.empty()) {
        game.pill_presets.push_back({"SMOOTH PILL", game.pill_settings});
        PillTrackingSettings reactive = game.pill_settings;
        reactive.change_min = 0.20f;
        reactive.change_max = 1.20f;
        reactive.acceleration = 24.0f;
        game.pill_presets.push_back({"REACTIVE PILL", reactive});
    }
    normalize_settings(game);
    game.selected_wall_preset = std::max(0, std::min(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()) - 1));
    game.selected_pill_preset = std::max(0, std::min(game.selected_pill_preset, static_cast<int>(game.pill_presets.size()) - 1));
    game.wall_preset_scroll = std::max(0, std::min(game.wall_preset_scroll, std::max(0, static_cast<int>(game.wall_presets.size()) - 7)));
    game.pill_preset_scroll = std::max(0, std::min(game.pill_preset_scroll, std::max(0, static_cast<int>(game.pill_presets.size()) - 7)));
}

static void apply_selected_presets(Game& game) {
    ensure_presets(game);
    game.wall_settings = game.wall_presets[game.selected_wall_preset].settings;
    game.wall_preset_name = game.wall_presets[game.selected_wall_preset].name;
    game.pill_settings = game.pill_presets[game.selected_pill_preset].settings;
    game.pill_preset_name = game.pill_presets[game.selected_pill_preset].name;
}

static void save_current_wall_preset(Game& game) {
    normalize_settings(game);
    game.wall_preset_name = unique_preset_name(game.wall_presets, game.wall_preset_name, game.selected_wall_preset);
    if (game.selected_wall_preset < 0 || game.selected_wall_preset >= static_cast<int>(game.wall_presets.size())) {
        game.wall_presets.push_back({game.wall_preset_name, game.wall_settings});
        game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    } else {
        game.wall_presets[game.selected_wall_preset] = {game.wall_preset_name, game.wall_settings};
    }
}

static void save_current_pill_preset(Game& game) {
    normalize_settings(game);
    game.pill_preset_name = unique_preset_name(game.pill_presets, game.pill_preset_name, game.selected_pill_preset);
    if (game.selected_pill_preset < 0 || game.selected_pill_preset >= static_cast<int>(game.pill_presets.size())) {
        game.pill_presets.push_back({game.pill_preset_name, game.pill_settings});
        game.selected_pill_preset = static_cast<int>(game.pill_presets.size()) - 1;
    } else {
        game.pill_presets[game.selected_pill_preset] = {game.pill_preset_name, game.pill_settings};
    }
}

static Target spawn_wall_target(Game& game, int skip_index = -1) {
    float radius = rand_wall_range(game, game.wall_settings.radius_min, game.wall_settings.radius_max);
    float min_x = -ROOM_WIDTH * 0.44f + radius;
    float max_x = ROOM_WIDTH * 0.44f - radius;
    float min_y = ROOM_HEIGHT * 0.16f + radius;
    float max_y = ROOM_HEIGHT * 0.84f - radius;
    Vec3 pos{0.0f, ROOM_EYE_HEIGHT, ROOM_WALL_Z + 0.45f};
    bool placed = false;
    for (int attempt = 0; attempt < 300 && !placed; ++attempt) {
        pos.x = rand_range(game, min_x, max_x);
        pos.y = rand_range(game, min_y, max_y);
        placed = true;
        for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
            if (i == skip_index) {
                continue;
            }
            Vec3 delta = pos - game.targets[i].pos;
            if (std::sqrt(delta.x * delta.x + delta.y * delta.y) < wall_spacing_for_radii(radius, game.targets[i].radius)) {
                placed = false;
                break;
            }
        }
    }
    if (!placed) {
        radius = game.wall_settings.radius_max;
        min_x = -ROOM_WIDTH * 0.44f + radius;
        max_x = ROOM_WIDTH * 0.44f - radius;
        min_y = ROOM_HEIGHT * 0.16f + radius;
        max_y = ROOM_HEIGHT * 0.84f - radius;
        float spacing = radius * 3.0f;
        for (float y = min_y; y <= max_y && !placed; y += spacing) {
            for (float x = min_x; x <= max_x && !placed; x += spacing) {
                pos.x = x;
                pos.y = y;
                placed = true;
                for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
                    if (i == skip_index) {
                        continue;
                    }
                    Vec3 delta = pos - game.targets[i].pos;
                    if (std::sqrt(delta.x * delta.x + delta.y * delta.y) < wall_spacing_for_radii(radius, game.targets[i].radius)) {
                        placed = false;
                        break;
                    }
                }
            }
        }
    }
    if (!placed) {
        float best_score = -1.0f;
        for (float y = min_y; y <= max_y; y += std::max(radius, radius * 2.0f)) {
            for (float x = min_x; x <= max_x; x += std::max(radius, radius * 2.0f)) {
                float nearest = 1.0e9f;
                for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
                    if (i == skip_index) {
                        continue;
                    }
                    Vec3 delta = {x - game.targets[i].pos.x, y - game.targets[i].pos.y, 0.0f};
                    nearest = std::min(nearest, length(delta) - wall_spacing_for_radii(radius, game.targets[i].radius));
                }
                if (nearest > best_score) {
                    best_score = nearest;
                    pos.x = x;
                    pos.y = y;
                }
            }
        }
    }
    Vec3 desired = wall_desired_velocity(game);
    float acceleration = rand_wall_range(game, game.wall_settings.acceleration_min, game.wall_settings.acceleration_max);
    return {pos, desired, desired, wall_change_timer(game), radius, acceleration};
}

static Vec3 pill_desired_velocity(Game& game) {
    float angle = rand_range(game, 0.0f, static_cast<float>(M_PI) * 2.0f);
    return {std::cos(angle) * game.pill_settings.speed, 0.0f, std::sin(angle) * game.pill_settings.speed};
}

static Target spawn_pill_target(Game& game) {
    float radius = game.pill_settings.width * 0.5f;
    float angle = -static_cast<float>(M_PI) * 0.5f + rand_range(game, -0.35f, 0.35f);
    float dist = rand_range(game, 7.5f, 10.5f);
    Vec3 desired = pill_desired_velocity(game);
    return {
        {std::cos(angle) * dist, PLANE_EYE_HEIGHT, std::sin(angle) * dist},
        desired,
        desired,
        rand_range(game, game.pill_settings.change_min, game.pill_settings.change_max),
        radius,
    };
}

static float closest_distance_to_segment(Vec3 point, Vec3 a, Vec3 b) {
    Vec3 ab = b - a;
    float denom = dot(ab, ab);
    if (denom <= 0.00001f) {
        return length(point - a);
    }
    float t = clampf(dot(point - a, ab) / denom, 0.0f, 1.0f);
    return length(point - (a + ab * t));
}

static bool ray_hits_capsule(Vec3 origin, Vec3 dir, const Target& target, float* projected_out) {
    Vec3 top = target.pos;
    Vec3 bottom = target.pos - Vec3{0.0f, TRACKING_CAPSULE_HEIGHT, 0.0f};
    float max_projection = std::max(0.0f, dot(top - origin, dir) + target.radius);
    int steps = 96;
    float best_distance = 1.0e9f;
    float best_projection = 0.0f;
    for (int i = 0; i <= steps; ++i) {
        float projected = max_projection * static_cast<float>(i) / static_cast<float>(steps);
        Vec3 point = origin + dir * projected;
        float distance = closest_distance_to_segment(point, bottom, top);
        if (distance < best_distance) {
            best_distance = distance;
            best_projection = projected;
        }
    }
    if (best_distance <= target.radius) {
        *projected_out = best_projection;
        return true;
    }
    return false;
}

static void start_scenario(Game& game, const ScenarioDef& scenario) {
    normalize_settings(game);
    game.mode = AppMode::Playing;
    game.edit_field = EditField::None;
    game.scenario = scenario;
    game.targets.clear();
    game.stats = {};
    game.yaw = 0.0f;
    game.pitch = 0.0f;
    int count = scenario.kind == ScenarioKind::WallClick
        ? rand_wall_int_range(game, game.wall_settings.target_count_min, game.wall_settings.target_count_max)
        : 1;
    for (int i = 0; i < count; ++i) {
        if (scenario.kind == ScenarioKind::WallClick) {
            game.targets.push_back(spawn_wall_target(game));
        } else {
            game.targets.push_back(spawn_pill_target(game));
        }
    }
}

static int aimed_target(const Game& game) {
    Vec3 origin = camera_pos(game);
    Vec3 dir = forward_dir(game);
    int best = -1;
    float best_projected = 1.0e9f;
    for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
        const Target& target = game.targets[i];
        float projected = 0.0f;
        bool hit = false;
        if (is_tracking(game.scenario.kind)) {
            hit = ray_hits_capsule(origin, dir, target, &projected);
        } else {
            Vec3 to_target = target.pos - origin;
            projected = dot(to_target, dir);
            if (projected < 0.0f) {
                continue;
            }
            Vec3 closest = origin + dir * projected;
            hit = length(closest - target.pos) <= target.radius;
        }
        if (hit && projected < best_projected) {
            best = i;
            best_projected = projected;
        }
    }
    return best;
}

static Vec3 approach_velocity(Vec3 current, Vec3 desired, float accel, float dt) {
    if (accel <= 0.0f) {
        return desired;
    }
    Vec3 delta = desired - current;
    float dist = length(delta);
    float step = accel * dt;
    if (dist <= step || dist <= 0.0001f) {
        return desired;
    }
    return current + delta * (step / dist);
}

static void lock_disabled_wall_axes(Game& game, Target& target) {
    if (game.wall_settings.horizontal_speed_max <= 0.0f) {
        target.vel.x = 0.0f;
        target.desired_vel.x = 0.0f;
    }
    if (game.wall_settings.vertical_speed_max <= 0.0f) {
        target.vel.y = 0.0f;
        target.desired_vel.y = 0.0f;
    }
}

static void resolve_wall_target_collisions(Game& game, float min_x, float max_x, float min_y, float max_y) {
    for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(game.targets.size()); ++j) {
            Target& a = game.targets[i];
            Target& b = game.targets[j];
            Vec3 delta = b.pos - a.pos;
            delta.z = 0.0f;
            float dist = length(delta);
            float contact_dist = a.radius + b.radius;
            if (dist <= 0.0001f) {
                delta = {1.0f, 0.0f, 0.0f};
                dist = 1.0f;
            }
            if (dist >= contact_dist) {
                continue;
            }

            Vec3 n = delta / dist;
            float overlap = contact_dist - dist;
            a.pos = a.pos - n * (overlap * 0.5f);
            b.pos = b.pos + n * (overlap * 0.5f);
            a.pos.x = clampf(a.pos.x, min_x, max_x);
            b.pos.x = clampf(b.pos.x, min_x, max_x);
            a.pos.y = clampf(a.pos.y, min_y, max_y);
            b.pos.y = clampf(b.pos.y, min_y, max_y);

            Vec3 rel = b.vel - a.vel;
            float closing_speed = dot(rel, n);
            if (closing_speed < 0.0f) {
                Vec3 impulse = n * closing_speed;
                a.vel = a.vel + impulse;
                b.vel = b.vel - impulse;
                a.desired_vel = a.vel;
                b.desired_vel = b.vel;
            }
            lock_disabled_wall_axes(game, a);
            lock_disabled_wall_axes(game, b);
        }
    }
}

static void update_wall_targets(Game& game, float dt) {
    float max_speed = std::sqrt(
        game.wall_settings.horizontal_speed_max * game.wall_settings.horizontal_speed_max +
        game.wall_settings.vertical_speed_max * game.wall_settings.vertical_speed_max
    );
    int substeps = std::max(1, static_cast<int>(std::ceil((max_speed * dt) / std::max(0.04f, game.wall_settings.radius_min * 0.4f))));
    substeps = std::min(substeps, 24);
    float step_dt = dt / static_cast<float>(substeps);

    for (int step = 0; step < substeps; ++step) {
        for (Target& target : game.targets) {
            target.change_timer -= step_dt;
            if (target.change_timer <= 0.0f) {
                target.desired_vel = wall_desired_velocity(game);
                target.change_timer = wall_change_timer(game);
            }
            target.vel = approach_velocity(target.vel, target.desired_vel, target.acceleration, step_dt);
            lock_disabled_wall_axes(game, target);
            target.pos = target.pos + target.vel * step_dt;

            float min_x = -ROOM_WIDTH * 0.48f + target.radius;
            float max_x = ROOM_WIDTH * 0.48f - target.radius;
            float min_y = ROOM_HEIGHT * 0.16f + target.radius;
            float max_y = ROOM_HEIGHT * 0.84f - target.radius;
            if (target.pos.x < min_x || target.pos.x > max_x) {
                target.pos.x = clampf(target.pos.x, min_x, max_x);
                target.vel.x = -target.vel.x;
                target.desired_vel.x = -target.desired_vel.x;
            }
            if (target.pos.y < min_y || target.pos.y > max_y) {
                target.pos.y = clampf(target.pos.y, min_y, max_y);
                target.vel.y = -target.vel.y;
                target.desired_vel.y = -target.desired_vel.y;
            }
            lock_disabled_wall_axes(game, target);
        }
        resolve_wall_target_collisions(
            game,
            -ROOM_WIDTH * 0.48f + game.wall_settings.radius_max,
            ROOM_WIDTH * 0.48f - game.wall_settings.radius_max,
            ROOM_HEIGHT * 0.16f + game.wall_settings.radius_max,
            ROOM_HEIGHT * 0.84f - game.wall_settings.radius_max
        );
    }
}

static void update_pill_target(Game& game, float dt) {
    if (game.targets.empty()) {
        return;
    }
    Target& target = game.targets[0];
    target.change_timer -= dt;
    if (target.change_timer <= 0.0f) {
        target.desired_vel = pill_desired_velocity(game);
        target.change_timer = rand_range(game, game.pill_settings.change_min, game.pill_settings.change_max);
    }
    target.vel = approach_velocity(target.vel, target.desired_vel, game.pill_settings.acceleration, dt);
    target.pos = target.pos + target.vel * dt;
    float limit = PLANE_HALF_SIZE - 2.5f - target.radius;
    if (std::fabs(target.pos.x) > limit) {
        target.pos.x = clampf(target.pos.x, -limit, limit);
        target.vel.x = -target.vel.x;
        target.desired_vel.x = -target.desired_vel.x;
    }
    if (std::fabs(target.pos.z) > limit) {
        target.pos.z = clampf(target.pos.z, -limit, limit);
        target.vel.z = -target.vel.z;
        target.desired_vel.z = -target.desired_vel.z;
    }
    if (length(target.pos - camera_pos(game)) < 4.0f) {
        target.vel = target.vel * -1.0f;
        target.desired_vel = target.desired_vel * -1.0f;
    }
}

static void update_playing(Game& game, const Input& input, float dt) {
    float radians_per_count = deg_to_rad(VALORANT_YAW_DEG_PER_COUNT * game.valorant_sens);
    game.yaw += static_cast<float>(input.rel_x) * radians_per_count;
    game.pitch = clampf(game.pitch - static_cast<float>(input.rel_y) * radians_per_count, -1.45f, 1.45f);
    game.stats.elapsed += dt;

    if (game.scenario.kind == ScenarioKind::WallClick) {
        update_wall_targets(game, dt);
    } else {
        update_pill_target(game, dt);
    }

    int hit_index = aimed_target(game);
    if (game.scenario.kind == ScenarioKind::WallClick) {
        if (input.left_pressed) {
            game.stats.shots += 1;
            if (hit_index >= 0) {
                game.stats.hits += 1;
                game.targets[hit_index] = spawn_wall_target(game, hit_index);
            }
        }
    } else if (input.left_down) {
        game.stats.tracking_fire_time += dt;
        if (hit_index >= 0) {
            game.stats.tracking_on_time += dt;
        }
    }
}

static void color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glColor4ub(r, g, b, a);
}

static void draw_box(Vec3 c, Vec3 s) {
    float x0 = c.x - s.x * 0.5f, x1 = c.x + s.x * 0.5f;
    float y0 = c.y - s.y * 0.5f, y1 = c.y + s.y * 0.5f;
    float z0 = c.z - s.z * 0.5f, z1 = c.z + s.z * 0.5f;
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glEnd();
}

static void vertex_color(uint8_t r, uint8_t g, uint8_t b) {
    color(r, g, b);
}

static void draw_quad_gradient(Vec3 a, Vec3 b, Vec3 c, Vec3 d, std::array<std::array<uint8_t, 3>, 4> colors) {
    glBegin(GL_QUADS);
    vertex_color(colors[0][0], colors[0][1], colors[0][2]);
    glVertex3f(a.x, a.y, a.z);
    vertex_color(colors[1][0], colors[1][1], colors[1][2]);
    glVertex3f(b.x, b.y, b.z);
    vertex_color(colors[2][0], colors[2][1], colors[2][2]);
    glVertex3f(c.x, c.y, c.z);
    vertex_color(colors[3][0], colors[3][1], colors[3][2]);
    glVertex3f(d.x, d.y, d.z);
    glEnd();
}

static void draw_lit_sphere(Vec3 c, float radius) {
    constexpr int stacks = 24;
    constexpr int slices = 36;
    for (int i = 0; i < stacks; ++i) {
        float lat0 = static_cast<float>(M_PI) * (-0.5f + static_cast<float>(i) / stacks);
        float lat1 = static_cast<float>(M_PI) * (-0.5f + static_cast<float>(i + 1) / stacks);
        float z0 = std::sin(lat0), zr0 = std::cos(lat0);
        float z1 = std::sin(lat1), zr1 = std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = static_cast<float>(M_PI) * 2.0f * static_cast<float>(j) / slices;
            float x = std::cos(lng), y = std::sin(lng);
            glNormal3f(x * zr0, z0, y * zr0);
            glVertex3f(c.x + radius * x * zr0, c.y + radius * z0, c.z + radius * y * zr0);
            glNormal3f(x * zr1, z1, y * zr1);
            glVertex3f(c.x + radius * x * zr1, c.y + radius * z1, c.z + radius * y * zr1);
        }
        glEnd();
    }
}

static void draw_lit_cylinder_y(Vec3 top, float radius, float height) {
    constexpr int slices = 36;
    float y0 = top.y - height;
    float y1 = top.y;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; ++j) {
        float lng = static_cast<float>(M_PI) * 2.0f * static_cast<float>(j) / slices;
        float x = std::cos(lng);
        float z = std::sin(lng);
        glNormal3f(x, 0.0f, z);
        glVertex3f(top.x + radius * x, y0, top.z + radius * z);
        glVertex3f(top.x + radius * x, y1, top.z + radius * z);
    }
    glEnd();
}

static void perspective(float vertical_fov_deg, float aspect, float near_z, float far_z) {
    float ymax = near_z * std::tan(deg_to_rad(vertical_fov_deg) * 0.5f);
    float xmax = ymax * aspect;
    glFrustum(-xmax, xmax, -ymax, ymax, near_z, far_z);
}

static void look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    float m[16] = {
        s.x, u.x, -f.x, 0.0f,
        s.y, u.y, -f.y, 0.0f,
        s.z, u.z, -f.z, 0.0f,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f,
    };
    glMultMatrixf(m);
}

static void draw_wall_room() {
    color(94, 101, 109);
    draw_box({0.0f, ROOM_HEIGHT * 0.5f, ROOM_WALL_Z}, {ROOM_WIDTH, ROOM_HEIGHT, 0.18f});
    color(82, 89, 97);
    draw_box({0.0f, -0.05f, (ROOM_WALL_Z + ROOM_BACK_Z) * 0.5f}, {ROOM_WIDTH, 0.1f, ROOM_BACK_Z - ROOM_WALL_Z});
    color(78, 85, 93);
    draw_box({0.0f, ROOM_HEIGHT + 0.05f, (ROOM_WALL_Z + ROOM_BACK_Z) * 0.5f}, {ROOM_WIDTH, 0.1f, ROOM_BACK_Z - ROOM_WALL_Z});
    color(72, 79, 87);
    draw_box({-ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.5f, (ROOM_WALL_Z + ROOM_BACK_Z) * 0.5f}, {0.15f, ROOM_HEIGHT, ROOM_BACK_Z - ROOM_WALL_Z});
    draw_box({ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.5f, (ROOM_WALL_Z + ROOM_BACK_Z) * 0.5f}, {0.15f, ROOM_HEIGHT, ROOM_BACK_Z - ROOM_WALL_Z});
    color(64, 70, 78);
    draw_box({0.0f, ROOM_HEIGHT * 0.5f, ROOM_WALL_Z + 0.03f}, {ROOM_WIDTH + 0.25f, 0.18f, 0.08f});
    draw_box({0.0f, 0.0f, ROOM_WALL_Z + 0.03f}, {ROOM_WIDTH + 0.25f, 0.18f, 0.08f});
    draw_box({-ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.5f, ROOM_WALL_Z + 0.03f}, {0.18f, ROOM_HEIGHT + 0.25f, 0.08f});
    draw_box({ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.5f, ROOM_WALL_Z + 0.03f}, {0.18f, ROOM_HEIGHT + 0.25f, 0.08f});
}

static void draw_plane360() {
    float h = PLANE_HALF_SIZE;
    float y0 = 0.0f;
    float y1 = PLANE_WALL_HEIGHT;

    draw_quad_gradient(
        {-h, y0, -h}, {h, y0, -h}, {h, y0, h}, {-h, y0, h},
        {{{68, 76, 85}, {74, 82, 91}, {64, 72, 81}, {58, 66, 75}}}
    );
    draw_quad_gradient(
        {-h, y1, h}, {h, y1, h}, {h, y1, -h}, {-h, y1, -h},
        {{{112, 120, 128}, {121, 129, 137}, {108, 116, 124}, {102, 110, 118}}}
    );
    draw_quad_gradient(
        {-h, y0, -h}, {-h, y1, -h}, {h, y1, -h}, {h, y0, -h},
        {{{76, 85, 94}, {86, 95, 104}, {96, 104, 112}, {86, 95, 104}}}
    );
    draw_quad_gradient(
        {h, y0, h}, {h, y1, h}, {-h, y1, h}, {-h, y0, h},
        {{{55, 64, 73}, {72, 81, 90}, {64, 73, 82}, {50, 59, 68}}}
    );
    draw_quad_gradient(
        {-h, y0, h}, {-h, y1, h}, {-h, y1, -h}, {-h, y0, -h},
        {{{49, 58, 67}, {66, 75, 84}, {78, 87, 96}, {58, 67, 76}}}
    );
    draw_quad_gradient(
        {h, y0, -h}, {h, y1, -h}, {h, y1, h}, {h, y0, h},
        {{{79, 88, 97}, {96, 104, 112}, {86, 95, 103}, {67, 76, 85}}}
    );
}

static void set_target_material() {
    GLfloat ambient[] = {0.72f, 0.14f, 0.18f, 1.0f};
    GLfloat diffuse[] = {1.0f, 0.28f, 0.38f, 1.0f};
    GLfloat specular[] = {0.22f, 0.12f, 0.14f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 14.0f);
}

static void draw_target(const Target& target, ScenarioKind kind) {
    set_target_material();
    if (is_tracking(kind)) {
        draw_lit_cylinder_y(target.pos, target.radius, TRACKING_CAPSULE_HEIGHT);
        draw_lit_sphere(target.pos, target.radius);
        draw_lit_sphere(target.pos - Vec3{0.0f, TRACKING_CAPSULE_HEIGHT, 0.0f}, target.radius);
        return;
    }
    draw_lit_sphere(target.pos, target.radius);
}

static void begin_3d(const Game& game, int w, int h) {
    float aspect = static_cast<float>(w) / static_cast<float>(std::max(1, h));
    float vertical_fov = rad_to_deg(2.0f * std::atan(std::tan(deg_to_rad(VALORANT_HORIZONTAL_FOV_DEG) * 0.5f) / aspect));
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(vertical_fov, aspect, 0.03f, 120.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Vec3 eye = camera_pos(game);
    look_at(eye, eye + forward_dir(game), {0.0f, 1.0f, 0.0f});
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
}

static void begin_2d(int w, int h) {
    glDisable(GL_DEPTH_TEST);
    float scale = std::max(1.0f, static_cast<float>(h) / 1080.0f);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<float>(w) / scale, static_cast<float>(h) / scale, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static float ui_scale_for_height(int h) {
    return std::max(1.0f, static_cast<float>(h) / 1080.0f);
}

static void rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    color(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}

static const std::array<const char*, 7>& glyph(char c) {
    static const std::array<const char*, 7> blank = {"00000","00000","00000","00000","00000","00000","00000"};
    static const std::array<const char*, 7> glyphs[43] = {
        {"01110","10001","10011","10101","11001","10001","01110"}, // 0
        {"00100","01100","00100","00100","00100","00100","01110"}, // 1
        {"01110","10001","00001","00010","00100","01000","11111"}, // 2
        {"11110","00001","00001","01110","00001","00001","11110"}, // 3
        {"00010","00110","01010","10010","11111","00010","00010"}, // 4
        {"11111","10000","10000","11110","00001","00001","11110"}, // 5
        {"00110","01000","10000","11110","10001","10001","01110"}, // 6
        {"11111","00001","00010","00100","01000","01000","01000"}, // 7
        {"01110","10001","10001","01110","10001","10001","01110"}, // 8
        {"01110","10001","10001","01111","00001","00010","11100"}, // 9
        {"01110","10001","10001","11111","10001","10001","10001"}, // A
        {"11110","10001","10001","11110","10001","10001","11110"}, // B
        {"01111","10000","10000","10000","10000","10000","01111"}, // C
        {"11110","10001","10001","10001","10001","10001","11110"}, // D
        {"11111","10000","10000","11110","10000","10000","11111"}, // E
        {"11111","10000","10000","11110","10000","10000","10000"}, // F
        {"01111","10000","10000","10011","10001","10001","01111"}, // G
        {"10001","10001","10001","11111","10001","10001","10001"}, // H
        {"01110","00100","00100","00100","00100","00100","01110"}, // I
        {"00111","00010","00010","00010","00010","10010","01100"}, // J
        {"10001","10010","10100","11000","10100","10010","10001"}, // K
        {"10000","10000","10000","10000","10000","10000","11111"}, // L
        {"10001","11011","10101","10101","10001","10001","10001"}, // M
        {"10001","11001","10101","10011","10001","10001","10001"}, // N
        {"01110","10001","10001","10001","10001","10001","01110"}, // O
        {"11110","10001","10001","11110","10000","10000","10000"}, // P
        {"01110","10001","10001","10001","10101","10010","01101"}, // Q
        {"11110","10001","10001","11110","10100","10010","10001"}, // R
        {"01111","10000","10000","01110","00001","00001","11110"}, // S
        {"11111","00100","00100","00100","00100","00100","00100"}, // T
        {"10001","10001","10001","10001","10001","10001","01110"}, // U
        {"10001","10001","10001","10001","10001","01010","00100"}, // V
        {"10001","10001","10001","10101","10101","10101","01010"}, // W
        {"10001","10001","01010","00100","01010","10001","10001"}, // X
        {"10001","10001","01010","00100","00100","00100","00100"}, // Y
        {"11111","00001","00010","00100","01000","10000","11111"}, // Z
        {"00000","00000","00000","11111","00000","00000","00000"}, // -
        {"00000","00100","00100","11111","00100","00100","00000"}, // +
        {"00000","00000","00000","00000","00000","01100","01100"}, // .
        {"00000","01100","01100","00000","01100","01100","00000"}, // :
        {"00001","00010","00010","00100","01000","01000","10000"}, // /
        {"11001","11010","00100","01000","10110","00110","00000"}, // %
        {"00000","00000","00000","00000","00000","00000","11111"}, // _
    };
    if (c >= '0' && c <= '9') return glyphs[c - '0'];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return glyphs[10 + c - 'A'];
    if (c == '-') return glyphs[36];
    if (c == '+') return glyphs[37];
    if (c == '.') return glyphs[38];
    if (c == ':') return glyphs[39];
    if (c == '/') return glyphs[40];
    if (c == '%') return glyphs[41];
    if (c == '_') return glyphs[42];
    return blank;
}

static float text_width(const std::string& value, float scale) {
    float width = 0.0f;
    for (char c : value) {
        width += (c == ' ') ? 4.0f * scale : 6.0f * scale;
    }
    return width;
}

static void text(float x, float y, const std::string& value, float scale, uint8_t r = 235, uint8_t g = 240, uint8_t b = 245) {
    float cursor = x;
    for (char c : value) {
        if (c == ' ') {
            cursor += 4.0f * scale;
            continue;
        }
        const auto& rows = glyph(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row][col] == '1') {
                    rect(cursor + col * scale, y + row * scale, scale * 0.82f, scale * 0.82f, r, g, b);
                }
            }
        }
        cursor += 6.0f * scale;
    }
}

static void text_fit(
    float x,
    float y,
    const std::string& value,
    float scale,
    float max_width,
    uint8_t r = 235,
    uint8_t g = 240,
    uint8_t b = 245
) {
    float base_width = text_width(value, 1.0f);
    float fitted_scale = scale;
    if (base_width > 0.0f && text_width(value, scale) > max_width) {
        fitted_scale = std::max(1.0f, max_width / base_width);
    }
    text(x, y, value, fitted_scale, r, g, b);
}

static bool button(const Input& input, float x, float y, float w, float h, const std::string& label, float text_scale = 3.0f) {
    bool hovered = input.mouse_x >= x && input.mouse_x <= x + w && input.mouse_y >= y && input.mouse_y <= y + h;
    if (hovered) rect(x, y, w, h, 255, 70, 85);
    else rect(x, y, w, h, 42, 48, 56);
    rect(x, y, w, 2, 94, 108, 125);
    rect(x, y + h - 2, w, 2, 94, 108, 125);
    rect(x, y, 2, h, 94, 108, 125);
    rect(x + w - 2, y, 2, h, 94, 108, 125);
    float base_width = text_width(label, 1.0f);
    float label_scale = text_scale;
    if (base_width > 0.0f && text_width(label, label_scale) > w - 24.0f) {
        label_scale = std::max(1.0f, (w - 24.0f) / base_width);
    }
    float label_width = text_width(label, label_scale);
    text(
        x + std::max(8.0f, (w - label_width) * 0.5f),
        y + std::max(6.0f, (h - 7.0f * label_scale) * 0.5f),
        label,
        label_scale
    );
    return hovered && input.left_pressed;
}

static bool small_button(const Input& input, float x, float y, const std::string& label) {
    return button(input, x, y, 46.0f, 34.0f, label);
}

static void setting_row_float(
    const Input& input,
    float x,
    float y,
    const std::string& label,
    float& value,
    float step,
    float low,
    float high
) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s %.2f", label.c_str(), value);
    text_fit(x, y + 8.0f, buffer, 2.15f, 300.0f, 225, 232, 240);
    if (small_button(input, x + 330.0f, y, "-")) {
        value = clampf(value - step, low, high);
    }
    if (small_button(input, x + 382.0f, y, "+")) {
        value = clampf(value + step, low, high);
    }
}

static void setting_row_float_range(
    const Input& input,
    float x,
    float y,
    const std::string& label,
    float& low_value,
    float& high_value,
    float step,
    float low_limit,
    float high_limit
) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s %.2f-%.2f", label.c_str(), low_value, high_value);
    text_fit(x, y + 8.0f, buffer, 2.0f, 292.0f, 225, 232, 240);
    if (button(input, x + 300.0f, y, 36.0f, 32.0f, "-", 2.5f)) {
        low_value = clampf(low_value - step, low_limit, high_limit);
    }
    if (button(input, x + 340.0f, y, 36.0f, 32.0f, "+", 2.5f)) {
        low_value = clampf(low_value + step, low_limit, high_limit);
    }
    if (button(input, x + 390.0f, y, 36.0f, 32.0f, "-", 2.5f)) {
        high_value = clampf(high_value - step, low_limit, high_limit);
    }
    if (button(input, x + 430.0f, y, 36.0f, 32.0f, "+", 2.5f)) {
        high_value = clampf(high_value + step, low_limit, high_limit);
    }
    if (high_value < low_value) {
        high_value = low_value;
    }
}

static void setting_row_int_range(
    const Input& input,
    float x,
    float y,
    const std::string& label,
    int& low_value,
    int& high_value,
    int step,
    int low_limit,
    int high_limit
) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s %d-%d", label.c_str(), low_value, high_value);
    text_fit(x, y + 8.0f, buffer, 2.0f, 292.0f, 225, 232, 240);
    if (button(input, x + 300.0f, y, 36.0f, 32.0f, "-", 2.5f)) {
        low_value = std::max(low_limit, low_value - step);
    }
    if (button(input, x + 340.0f, y, 36.0f, 32.0f, "+", 2.5f)) {
        low_value = std::min(high_limit, low_value + step);
    }
    if (button(input, x + 390.0f, y, 36.0f, 32.0f, "-", 2.5f)) {
        high_value = std::max(low_limit, high_value - step);
    }
    if (button(input, x + 430.0f, y, 36.0f, 32.0f, "+", 2.5f)) {
        high_value = std::min(high_limit, high_value + step);
    }
    if (high_value < low_value) {
        high_value = low_value;
    }
}

static std::string settings_path() {
    if (!g_settings_path_override.empty()) {
        return g_settings_path_override;
    }
#ifdef _WIN32
    const char* base = std::getenv("APPDATA");
    if (base) {
        return std::string(base) + "\\valorant_aim_trainer.cfg";
    }
    return "valorant_aim_trainer.cfg";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.valorant_aim_trainer.cfg";
    }
    return ".valorant_aim_trainer.cfg";
#endif
}

static void save_settings(const Game& game) {
    Game normalized = game;
    ensure_presets(normalized);
    normalize_settings(normalized);
    std::ofstream out(settings_path());
    if (!out) {
        return;
    }
    out << "version 3\n";
    out << "valorant_sens " << normalized.valorant_sens << "\n";
    out << "crosshair " << normalized.crosshair.length << " " << normalized.crosshair.gap << " " << normalized.crosshair.thickness << "\n";
    out << "selected_wall " << normalized.selected_wall_preset << "\n";
    out << "selected_pill " << normalized.selected_pill_preset << "\n";
    for (const WallPreset& preset : normalized.wall_presets) {
        out << "wall_preset " << std::quoted(preset.name) << " "
            << preset.settings.target_count_min << " "
            << preset.settings.target_count_max << " "
            << preset.settings.radius_min << " "
            << preset.settings.radius_max << " "
            << preset.settings.horizontal_speed_min << " "
            << preset.settings.horizontal_speed_max << " "
            << preset.settings.vertical_speed_min << " "
            << preset.settings.vertical_speed_max << " "
            << preset.settings.acceleration_min << " "
            << preset.settings.acceleration_max << " "
            << preset.settings.change_min << " "
            << preset.settings.change_max << "\n";
    }
    for (const PillPreset& preset : normalized.pill_presets) {
        out << "pill_preset " << std::quoted(preset.name) << " "
            << preset.settings.width << " "
            << preset.settings.speed << " "
            << preset.settings.acceleration << " "
            << preset.settings.change_min << " "
            << preset.settings.change_max << "\n";
    }
}

static void load_settings(Game& game) {
    std::ifstream in(settings_path());
    if (!in) {
        ensure_presets(game);
        apply_selected_presets(game);
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "version") {
            continue;
        }
        if (key == "valorant_sens") {
            row >> game.valorant_sens;
        } else if (key == "crosshair") {
            row >> game.crosshair.length >> game.crosshair.gap >> game.crosshair.thickness;
        } else if (key == "selected_wall") {
            row >> game.selected_wall_preset;
        } else if (key == "selected_pill") {
            row >> game.selected_pill_preset;
        } else if (key == "wall_preset") {
            WallPreset preset;
            row >> std::quoted(preset.name);
            std::vector<float> values;
            float value = 0.0f;
            while (row >> value) {
                values.push_back(value);
            }
            if (values.size() >= 12) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
                preset.settings.radius_min = values[2];
                preset.settings.radius_max = values[3];
                preset.settings.horizontal_speed_min = values[4];
                preset.settings.horizontal_speed_max = values[5];
                preset.settings.vertical_speed_min = values[6];
                preset.settings.vertical_speed_max = values[7];
                preset.settings.acceleration_min = values[8];
                preset.settings.acceleration_max = values[9];
                preset.settings.change_min = values[10];
                preset.settings.change_max = values[11];
            } else if (values.size() >= 6) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = preset.settings.target_count_min;
                preset.settings.radius_min = values[1];
                preset.settings.radius_max = values[1];
                preset.settings.horizontal_speed_min = values[2];
                preset.settings.horizontal_speed_max = values[2];
                preset.settings.vertical_speed_min = values[3];
                preset.settings.vertical_speed_max = values[3];
                preset.settings.acceleration_min = values[4];
                preset.settings.acceleration_max = values[4];
                preset.settings.change_min = values[5] <= 0.0f ? 0.0f : values[5] * 0.55f;
                preset.settings.change_max = values[5] <= 0.0f ? 0.0f : values[5] * 1.55f;
            }
            if (!preset.name.empty()) {
                game.wall_presets.push_back(preset);
            }
        } else if (key == "pill_preset") {
            PillPreset preset;
            row >> std::quoted(preset.name)
                >> preset.settings.width
                >> preset.settings.speed
                >> preset.settings.acceleration
                >> preset.settings.change_min
                >> preset.settings.change_max;
            if (!preset.name.empty()) {
                game.pill_presets.push_back(preset);
            }
        } else {
            float value = 0.0f;
            std::istringstream legacy(line);
            legacy >> key >> value;
            if (key == "wall_count") {
                game.wall_settings.target_count_min = static_cast<int>(std::round(value));
                game.wall_settings.target_count_max = game.wall_settings.target_count_min;
            }
            else if (key == "wall_radius") game.wall_settings.radius_min = game.wall_settings.radius_max = value;
            else if (key == "wall_hspeed") game.wall_settings.horizontal_speed_min = game.wall_settings.horizontal_speed_max = value;
            else if (key == "wall_vspeed") game.wall_settings.vertical_speed_min = game.wall_settings.vertical_speed_max = value;
            else if (key == "wall_accel") game.wall_settings.acceleration_min = game.wall_settings.acceleration_max = value;
            else if (key == "wall_change") {
                game.wall_settings.change_min = value <= 0.0f ? 0.0f : value * 0.55f;
                game.wall_settings.change_max = value <= 0.0f ? 0.0f : value * 1.55f;
            }
            else if (key == "pill_width") game.pill_settings.width = value;
            else if (key == "pill_speed") game.pill_settings.speed = value;
            else if (key == "pill_accel") game.pill_settings.acceleration = value;
            else if (key == "pill_change_min") game.pill_settings.change_min = value;
            else if (key == "pill_change_max") game.pill_settings.change_max = value;
        }
    }
    if (game.wall_presets.empty()) {
        game.wall_presets.push_back({"PASU FIVE", game.wall_settings});
    }
    if (game.pill_presets.empty()) {
        game.pill_presets.push_back({"SMOOTH PILL", game.pill_settings});
    }
    apply_selected_presets(game);
}

static void draw_world(const Game& game, int w, int h) {
    if (game.scenario.map == MapKind::Plane360) {
        glClearColor(76.0f / 255.0f, 83.0f / 255.0f, 92.0f / 255.0f, 1.0f);
    } else {
        glClearColor(66.0f / 255.0f, 72.0f / 255.0f, 80.0f / 255.0f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_3d(game, w, h);
    if (game.scenario.map == MapKind::WallRoom) draw_wall_room();
    else draw_plane360();
    GLfloat light_ambient[] = {0.82f, 0.82f, 0.84f, 1.0f};
    GLfloat light_diffuse[] = {0.70f, 0.70f, 0.68f, 1.0f};
    GLfloat light_pos[] = {-4.0f, game.scenario.map == MapKind::WallRoom ? ROOM_HEIGHT + 1.5f : PLANE_WALL_HEIGHT + 3.0f, 1.0f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    for (const Target& target : game.targets) {
        draw_target(target, game.scenario.kind);
    }
    glDisable(GL_LIGHTING);
    begin_2d(w, h);
    float ui_scale = ui_scale_for_height(h);
    float ui_w = static_cast<float>(w) / ui_scale;
    float ui_h = static_cast<float>(h) / ui_scale;
    float cx = ui_w * 0.5f, cy = ui_h * 0.5f;
    float len = game.crosshair.length;
    float gap = game.crosshair.gap;
    float thick = game.crosshair.thickness;
    rect(cx - gap - len, cy - thick * 0.5f, len, thick, 245, 248, 252);
    rect(cx + gap, cy - thick * 0.5f, len, thick, 245, 248, 252);
    rect(cx - thick * 0.5f, cy - gap - len, thick, len, 245, 248, 252);
    rect(cx - thick * 0.5f, cy + gap, thick, len, 245, 248, 252);
    char line[160];
    std::snprintf(line, sizeof(line), "FOV 103  SENS %.3f", game.valorant_sens);
    std::string sens_line = line;
    std::string stat_line;
    if (game.scenario.kind == ScenarioKind::WallClick) {
        float accuracy = game.stats.shots == 0 ? 100.0f : static_cast<float>(game.stats.hits) / static_cast<float>(game.stats.shots) * 100.0f;
        std::snprintf(line, sizeof(line), "HITS %d  SHOTS %d  ACC %.1f%%", game.stats.hits, game.stats.shots, accuracy);
        stat_line = line;
    } else {
        float tracking = game.stats.tracking_fire_time <= 0.0001f ? 0.0f : game.stats.tracking_on_time / game.stats.tracking_fire_time * 100.0f;
        std::snprintf(line, sizeof(line), "TRACKING %.1f%%  HOLD LMB", tracking);
        stat_line = line;
    }
    float hud_w = std::max({
        260.0f,
        text_width(game.scenario.title, 3.0f) + 36.0f,
        text_width(sens_line, 2.4f) + 36.0f,
        text_width(stat_line, 2.4f) + 36.0f,
    });
    hud_w = std::min(hud_w, ui_w - 48.0f);
    rect(24, 22, hud_w, 122, 0, 0, 0, 150);
    text_fit(42, 42, game.scenario.title, 3.0f, hud_w - 36.0f);
    text_fit(42, 78, sens_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_fit(42, 108, stat_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_fit(42, ui_h - 42, "ESC MENU / QUIT", 2.2f, ui_w - 84.0f, 210, 220, 232);
}

static bool list_button(const Input& input, float x, float y, float w, float h, const std::string& label, bool selected) {
    bool hovered = input.mouse_x >= x && input.mouse_x <= x + w && input.mouse_y >= y && input.mouse_y <= y + h;
    if (selected) rect(x, y, w, h, 86, 98, 114);
    else if (hovered) rect(x, y, w, h, 58, 66, 76);
    else rect(x, y, w, h, 31, 36, 42);
    rect(x, y, w, 2, 83, 96, 112);
    rect(x, y + h - 2, w, 2, 83, 96, 112);
    rect(x, y, 2, h, 83, 96, 112);
    rect(x + w - 2, y, 2, h, 83, 96, 112);
    text_fit(x + 14.0f, y + 10.0f, label, 2.1f, w - 28.0f, 230, 236, 244);
    return hovered && input.left_pressed;
}

static bool text_field(const Input& input, float x, float y, float w, float h, const std::string& value, bool active) {
    bool hovered = input.mouse_x >= x && input.mouse_x <= x + w && input.mouse_y >= y && input.mouse_y <= y + h;
    rect(x, y, w, h, active ? 43 : 32, active ? 51 : 38, active ? 61 : 46);
    uint8_t border = active ? 255 : hovered ? 132 : 88;
    rect(x, y, w, 2, border, active ? 70 : 103, active ? 85 : 121);
    rect(x, y + h - 2, w, 2, border, active ? 70 : 103, active ? 85 : 121);
    rect(x, y, 2, h, border, active ? 70 : 103, active ? 85 : 121);
    rect(x + w - 2, y, 2, h, border, active ? 70 : 103, active ? 85 : 121);
    std::string shown = value + (active ? "_" : "");
    text_fit(x + 14.0f, y + 12.0f, shown, 2.35f, w - 28.0f);
    return hovered && input.left_pressed;
}

static void apply_name_edit(Game& game, const Input& input) {
    std::string* value = nullptr;
    if (game.edit_field == EditField::WallPresetName) {
        value = &game.wall_preset_name;
    } else if (game.edit_field == EditField::PillPresetName) {
        value = &game.pill_preset_name;
    }
    if (!value) {
        return;
    }
    if (input.backspace_pressed && !value->empty()) {
        value->pop_back();
    }
    for (char c : input.text_input) {
        if (is_allowed_preset_char(c) && value->size() < 22) {
            value->push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    *value = filter_preset_name_draft(*value);
    if (input.enter_pressed) {
        game.edit_field = EditField::None;
    }
}

static void new_wall_preset(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "CLICK PRESET %d", static_cast<int>(game.wall_presets.size()) + 1);
    game.edit_field = EditField::None;
    std::string unique_name = unique_preset_name(game.wall_presets, name, -1);
    game.wall_presets.push_back({unique_name, game.wall_settings});
    game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    game.wall_settings = game.wall_presets.back().settings;
    game.wall_preset_name = game.wall_presets.back().name;
}

static void new_pill_preset(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "TRACK PRESET %d", static_cast<int>(game.pill_presets.size()) + 1);
    game.edit_field = EditField::None;
    std::string unique_name = unique_preset_name(game.pill_presets, name, -1);
    game.pill_presets.push_back({unique_name, game.pill_settings});
    game.selected_pill_preset = static_cast<int>(game.pill_presets.size()) - 1;
    game.pill_settings = game.pill_presets.back().settings;
    game.pill_preset_name = game.pill_presets.back().name;
}

static void delete_wall_preset(Game& game) {
    game.edit_field = EditField::None;
    if (game.wall_presets.size() <= 1) {
        game.wall_presets[0] = {"PASU FIVE", WallClickSettings{}};
        game.selected_wall_preset = 0;
    } else {
        game.wall_presets.erase(game.wall_presets.begin() + game.selected_wall_preset);
        game.selected_wall_preset = std::max(0, std::min(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()) - 1));
    }
    apply_selected_presets(game);
}

static void delete_pill_preset(Game& game) {
    game.edit_field = EditField::None;
    if (game.pill_presets.size() <= 1) {
        game.pill_presets[0] = {"SMOOTH PILL", PillTrackingSettings{}};
        game.selected_pill_preset = 0;
    } else {
        game.pill_presets.erase(game.pill_presets.begin() + game.selected_pill_preset);
        game.selected_pill_preset = std::max(0, std::min(game.selected_pill_preset, static_cast<int>(game.pill_presets.size()) - 1));
    }
    apply_selected_presets(game);
}

static void draw_tab_button(Game& game, const Input& input, float x, float y, const std::string& label, MenuTab tab) {
    bool selected = game.menu_tab == tab;
    bool clicked = button(input, x, y, 190.0f, 42.0f, label, 2.35f);
    if (selected) {
        rect(x, y + 40.0f, 190.0f, 3.0f, 255, 70, 85);
    }
    if (clicked) {
        game.menu_tab = tab;
        game.edit_field = EditField::None;
    }
}

static void draw_wall_tab(Game& game, const Input& input, float left, float top, float base_h) {
    if (input.wheel_y != 0) {
        game.wall_preset_scroll = std::max(0, std::min(game.wall_preset_scroll - input.wheel_y, std::max(0, static_cast<int>(game.wall_presets.size()) - 7)));
    }
    text(left, top, "CLICKING PRESETS", 2.8f);
    float list_y = top + 38.0f;
    for (int row = 0; row < 7; ++row) {
        int index = game.wall_preset_scroll + row;
        if (index >= static_cast<int>(game.wall_presets.size())) {
            break;
        }
        if (list_button(input, left, list_y + row * 42.0f, 270.0f, 34.0f, game.wall_presets[index].name, index == game.selected_wall_preset)) {
            game.selected_wall_preset = index;
            game.wall_settings = game.wall_presets[index].settings;
            game.wall_preset_name = game.wall_presets[index].name;
            game.edit_field = EditField::None;
        }
    }
    if (button(input, left, list_y + 310.0f, 64.0f, 34.0f, "NEW", 1.85f)) new_wall_preset(game);
    if (button(input, left + 74.0f, list_y + 310.0f, 72.0f, 34.0f, "DELETE", 1.7f)) delete_wall_preset(game);
    if (button(input, left + 156.0f, list_y + 310.0f, 114.0f, 34.0f, "SAVE PRESET", 1.55f)) {
        save_current_wall_preset(game);
        game.edit_field = EditField::None;
        save_settings(game);
    }

    float x = left + 330.0f;
    text(x, top, "WALL CLICKING", 3.2f);
    text_fit(x, top + 38.0f, "NAME", 2.0f, 260.0f, 180, 190, 204);
    if (text_field(input, x + 90.0f, top + 28.0f, 360.0f, 40.0f, game.wall_preset_name, game.edit_field == EditField::WallPresetName)) {
        game.edit_field = EditField::WallPresetName;
    }
    if (button(input, x + 470.0f, top + 28.0f, 180.0f, 40.0f, "START", 2.35f)) {
        start_scenario(game, game.scenarios[0]);
    }
    float row = top + 92.0f;
    text_fit(x + 304.0f, row - 22.0f, "MIN", 1.45f, 72.0f, 150, 162, 178);
    text_fit(x + 394.0f, row - 22.0f, "MAX", 1.45f, 72.0f, 150, 162, 178);
    setting_row_int_range(input, x, row, "TARGETS", game.wall_settings.target_count_min, game.wall_settings.target_count_max, 1, 1, wall_capacity_for_radius(game.wall_settings.radius_max));
    row += 36.0f;
    setting_row_float_range(input, x, row, "RADIUS", game.wall_settings.radius_min, game.wall_settings.radius_max, 0.02f, 0.12f, 0.9f);
    row += 36.0f;
    setting_row_float_range(input, x, row, "H SPEED", game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max, 0.25f, 0.0f, 12.0f);
    row += 36.0f;
    setting_row_float_range(input, x, row, "V SPEED", game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max, 0.25f, 0.0f, 12.0f);
    row += 36.0f;
    setting_row_float_range(input, x, row, "ACCEL", game.wall_settings.acceleration_min, game.wall_settings.acceleration_max, 1.0f, 0.0f, 80.0f);
    row += 36.0f;
    setting_row_float_range(input, x, row, "DIR SEC", game.wall_settings.change_min, game.wall_settings.change_max, 0.10f, 0.0f, 12.0f);
    text_fit(x, base_h - 42.0f, "MIN-MAX RANGES ARE SAMPLED UNIFORMLY PER TARGET", 2.0f, 650.0f, 180, 190, 204);
}

static void draw_pill_tab(Game& game, const Input& input, float left, float top, float base_h) {
    if (input.wheel_y != 0) {
        game.pill_preset_scroll = std::max(0, std::min(game.pill_preset_scroll - input.wheel_y, std::max(0, static_cast<int>(game.pill_presets.size()) - 7)));
    }
    text(left, top, "TRACKING PRESETS", 2.8f);
    float list_y = top + 38.0f;
    for (int row = 0; row < 7; ++row) {
        int index = game.pill_preset_scroll + row;
        if (index >= static_cast<int>(game.pill_presets.size())) {
            break;
        }
        if (list_button(input, left, list_y + row * 42.0f, 270.0f, 34.0f, game.pill_presets[index].name, index == game.selected_pill_preset)) {
            game.selected_pill_preset = index;
            game.pill_settings = game.pill_presets[index].settings;
            game.pill_preset_name = game.pill_presets[index].name;
            game.edit_field = EditField::None;
        }
    }
    if (button(input, left, list_y + 310.0f, 64.0f, 34.0f, "NEW", 1.85f)) new_pill_preset(game);
    if (button(input, left + 74.0f, list_y + 310.0f, 72.0f, 34.0f, "DELETE", 1.7f)) delete_pill_preset(game);
    if (button(input, left + 156.0f, list_y + 310.0f, 114.0f, 34.0f, "SAVE PRESET", 1.55f)) {
        save_current_pill_preset(game);
        game.edit_field = EditField::None;
        save_settings(game);
    }

    float x = left + 330.0f;
    text(x, top, "360 PILL TRACKING", 3.2f);
    text_fit(x, top + 38.0f, "NAME", 2.0f, 260.0f, 180, 190, 204);
    if (text_field(input, x + 90.0f, top + 28.0f, 360.0f, 40.0f, game.pill_preset_name, game.edit_field == EditField::PillPresetName)) {
        game.edit_field = EditField::PillPresetName;
    }
    if (button(input, x + 470.0f, top + 28.0f, 180.0f, 40.0f, "START", 2.35f)) {
        start_scenario(game, game.scenarios[1]);
    }
    float row = top + 92.0f;
    setting_row_float(input, x, row, "PILL WIDTH", game.pill_settings.width, 0.05f, 0.35f, 2.4f);
    row += 42.0f;
    setting_row_float(input, x, row, "SPEED", game.pill_settings.speed, 0.25f, 0.0f, 14.0f);
    row += 42.0f;
    setting_row_float(input, x, row, "ACCEL", game.pill_settings.acceleration, 1.0f, 0.0f, 80.0f);
    row += 42.0f;
    setting_row_float(input, x, row, "MIN SEC", game.pill_settings.change_min, 0.05f, 0.05f, 8.0f);
    row += 42.0f;
    setting_row_float(input, x, row, "MAX SEC", game.pill_settings.change_max, 0.10f, game.pill_settings.change_min, 12.0f);
    text_fit(x, base_h - 42.0f, "DIRECTION TIMING IS RANDOM BETWEEN MIN AND MAX", 2.0f, 650.0f, 180, 190, 204);
}

static void draw_general_tab(Game& game, const Input& input, float left, float top, float base_h) {
    text(left, top, "GENERAL", 3.2f);
    float row = top + 58.0f;
    char sens[64];
    std::snprintf(sens, sizeof(sens), "VALORANT SENS %.3f", game.valorant_sens);
    text(left, row + 8.0f, sens, 2.45f);
    if (button(input, left + 360.0f, row, 88.0f, 38.0f, "-0.01", 2.0f)) game.valorant_sens = std::max(0.001f, game.valorant_sens - 0.010f);
    if (button(input, left + 456.0f, row, 88.0f, 38.0f, "+0.01", 2.0f)) game.valorant_sens = std::min(10.0f, game.valorant_sens + 0.010f);
    if (button(input, left + 552.0f, row, 98.0f, 38.0f, "-0.001", 2.0f)) game.valorant_sens = std::max(0.001f, game.valorant_sens - 0.001f);
    if (button(input, left + 658.0f, row, 98.0f, 38.0f, "+0.001", 2.0f)) game.valorant_sens = std::min(10.0f, game.valorant_sens + 0.001f);
    row += 74.0f;
    text(left, row, "CROSSHAIR", 2.8f);
    row += 42.0f;
    setting_row_float(input, left, row, "LENGTH", game.crosshair.length, 1.0f, 4.0f, 24.0f);
    row += 42.0f;
    setting_row_float(input, left, row, "GAP", game.crosshair.gap, 1.0f, 0.0f, 16.0f);
    row += 42.0f;
    setting_row_float(input, left, row, "THICK", game.crosshair.thickness, 1.0f, 1.0f, 6.0f);
    if (button(input, left, row + 74.0f, 250.0f, 46.0f, "SAVE GENERAL", 2.35f)) {
        save_settings(game);
    }
    text_fit(left, base_h - 42.0f, "GENERAL SAVES SENSITIVITY AND CROSSHAIR ONLY", 2.0f, 760.0f, 180, 190, 204);
}

static void draw_menu(Game& game, const Input& input, int w, int h) {
    ensure_presets(game);
    apply_name_edit(game, input);
    normalize_settings(game);

    glClearColor(16.0f / 255.0f, 18.0f / 255.0f, 22.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_2d(w, h);
    float ui_scale = ui_scale_for_height(h);
    float ui_w = static_cast<float>(w) / ui_scale;
    float ui_h = static_cast<float>(h) / ui_scale;
    float menu_scale = std::min(1.0f, std::min((ui_w - 84.0f) / 1040.0f, (ui_h - 42.0f) / 720.0f));
    menu_scale = std::max(0.25f, menu_scale);
    Input ui_input = input;
    ui_input.mouse_x = static_cast<int>(static_cast<float>(ui_input.mouse_x) / ui_scale);
    ui_input.mouse_y = static_cast<int>(static_cast<float>(ui_input.mouse_y) / ui_scale);
    ui_input.mouse_x = static_cast<int>(static_cast<float>(ui_input.mouse_x) / menu_scale);
    ui_input.mouse_y = static_cast<int>(static_cast<float>(ui_input.mouse_y) / menu_scale);
    float base_w = ui_w / menu_scale;
    float base_h = ui_h / menu_scale;

    glPushMatrix();
    glScalef(menu_scale, menu_scale, 1.0f);

    float left = std::max(42.0f, base_w * 0.5f - 520.0f);
    float top = 58.0f;
    text(left, top, "VALORANT AIM TRAINER", 4.7f);
    text(left, top + 46.0f, "FOV LOCKED TO 103 HORIZONTAL", 2.25f, 190, 199, 211);
    text(left, top + 72.0f, "SENSITIVITY USES VALORANT UNITS", 2.25f, 190, 199, 211);

    float tabs_y = top + 112.0f;
    draw_tab_button(game, ui_input, left, tabs_y, "CLICKING", MenuTab::Clicking);
    draw_tab_button(game, ui_input, left + 210.0f, tabs_y, "TRACKING", MenuTab::Tracking);
    draw_tab_button(game, ui_input, left + 420.0f, tabs_y, "GENERAL", MenuTab::General);

    float content_top = tabs_y + 74.0f;
    if (game.menu_tab == MenuTab::Clicking) {
        draw_wall_tab(game, ui_input, left, content_top, base_h);
    } else if (game.menu_tab == MenuTab::Tracking) {
        draw_pill_tab(game, ui_input, left, content_top, base_h);
    } else {
        draw_general_tab(game, ui_input, left, content_top, base_h);
    }

    glPopMatrix();
}

static void set_mouse_grab(Game& game, bool grabbed) {
    if (game.mouse_grabbed == grabbed) {
        return;
    }
    SDL_SetRelativeMouseMode(grabbed ? SDL_TRUE : SDL_FALSE);
    SDL_ShowCursor(grabbed ? SDL_DISABLE : SDL_ENABLE);
    game.mouse_grabbed = grabbed;
}

static void init_scenarios(Game& game) {
    game.scenarios = {
        {"WALL CLICKING", ScenarioKind::WallClick, MapKind::WallRoom, 0.0f, 0, 0.0f},
        {"360 PILL TRACKING", ScenarioKind::PillTracking, MapKind::Plane360, 0.0f, 1, 0.0f},
    };
    game.scenario = game.scenarios.front();
}

static bool self_test_check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "SELF TEST FAILED: %s\n", message);
        return false;
    }
    return true;
}

static int run_self_test() {
    bool ok = true;

    Game game;
    ensure_presets(game);
    apply_selected_presets(game);
    game.edit_field = EditField::WallPresetName;
    while (!game.wall_preset_name.empty()) {
        Input input;
        input.backspace_pressed = true;
        apply_name_edit(game, input);
    }
    ok = self_test_check(game.wall_preset_name.empty(), "preset name draft can be emptied") && ok;
    Input type_input;
    type_input.text_input = "  u custom!";
    apply_name_edit(game, type_input);
    ok = self_test_check(game.wall_preset_name == "U CUSTOM", "typing into empty preset field does not prepend UNTITLED") && ok;

    game.wall_preset_name.clear();
    save_current_wall_preset(game);
    ok = self_test_check(game.wall_presets[game.selected_wall_preset].name == "UNTITLED", "empty saved preset finalizes to UNTITLED") && ok;
    ok = self_test_check(game.edit_field == EditField::WallPresetName, "saving helper does not unexpectedly alter edit field") && ok;

    game.selected_wall_preset = 0;
    game.wall_presets[0].settings.radius_min = 0.34f;
    game.wall_presets[0].settings.radius_max = 0.34f;
    game.wall_settings = game.wall_presets[0].settings;
    game.wall_settings.radius_min = 0.22f;
    game.wall_settings.radius_max = 0.28f;
    game.edit_field = EditField::WallPresetName;
    int old_count = static_cast<int>(game.wall_presets.size());
    new_wall_preset(game);
    ok = self_test_check(static_cast<int>(game.wall_presets.size()) == old_count + 1, "new wall preset appends exactly one preset") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[0].settings.radius_min - 0.34f) < 0.0001f, "new wall preset does not overwrite selected preset") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[game.selected_wall_preset].settings.radius_min - 0.22f) < 0.0001f, "new wall preset copies current edited min settings") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[game.selected_wall_preset].settings.radius_max - 0.28f) < 0.0001f, "new wall preset copies current edited max settings") && ok;
    ok = self_test_check(game.edit_field == EditField::None, "new wall preset exits text edit mode") && ok;

    game.wall_presets.push_back({"CLICK PRESET 4", game.wall_settings});
    std::string unique_click = unique_preset_name(game.wall_presets, "CLICK PRESET 4", -1);
    ok = self_test_check(unique_click != "CLICK PRESET 4", "generated wall preset names avoid duplicates") && ok;

    game.wall_presets = {{"ONLY WALL", WallClickSettings{}}};
    game.selected_wall_preset = 0;
    game.edit_field = EditField::WallPresetName;
    delete_wall_preset(game);
    ok = self_test_check(game.wall_presets.size() == 1 && game.wall_presets[0].name == "PASU FIVE", "deleting the last wall preset restores default") && ok;
    ok = self_test_check(game.edit_field == EditField::None, "delete wall preset exits text edit mode") && ok;

    game.pill_preset_name.clear();
    game.edit_field = EditField::PillPresetName;
    Input pill_input;
    pill_input.text_input = "fast pill";
    apply_name_edit(game, pill_input);
    ok = self_test_check(game.pill_preset_name == "FAST PILL", "pill preset name edit filters and uppercases") && ok;
    int old_pill_count = static_cast<int>(game.pill_presets.size());
    new_pill_preset(game);
    ok = self_test_check(static_cast<int>(game.pill_presets.size()) == old_pill_count + 1, "new pill preset appends exactly one preset") && ok;
    ok = self_test_check(game.edit_field == EditField::None, "new pill preset exits text edit mode") && ok;

    g_settings_path_override = "build/self-test-settings.cfg";
    std::remove(g_settings_path_override.c_str());
    game.wall_presets = {{"PASU FIVE", WallClickSettings{}}, {"STATIC FIVE", WallClickSettings{}}};
    game.pill_presets = {{"SMOOTH PILL", PillTrackingSettings{}}, {"REACTIVE PILL", PillTrackingSettings{}}};
    game.selected_wall_preset = 0;
    game.selected_pill_preset = 0;
    apply_selected_presets(game);
    game.wall_preset_name = "TINY PASU";
    game.wall_settings.target_count_min = 8;
    game.wall_settings.target_count_max = 8;
    game.wall_settings.radius_min = 0.18f;
    game.wall_settings.radius_max = 0.22f;
    save_current_wall_preset(game);
    game.valorant_sens = 0.777f;
    game.crosshair = {14.0f, 6.0f, 3.0f};
    save_settings(game);

    Game loaded;
    load_settings(loaded);
    ok = self_test_check(std::fabs(loaded.valorant_sens - 0.777f) < 0.0001f, "saved general sensitivity loads") && ok;
    ok = self_test_check(std::fabs(loaded.crosshair.length - 14.0f) < 0.0001f, "saved crosshair loads") && ok;
    ok = self_test_check(!loaded.wall_presets.empty(), "saved wall presets load") && ok;
    ok = self_test_check(loaded.wall_preset_name == "TINY PASU", "selected named wall preset loads into editor") && ok;
    ok = self_test_check(loaded.wall_settings.target_count_min == 8 && loaded.wall_settings.target_count_max == 8, "selected wall preset target count range loads") && ok;
    ok = self_test_check(std::fabs(loaded.wall_settings.radius_max - 0.22f) < 0.0001f, "selected wall preset radius range loads") && ok;

    game.selected_wall_preset = 1;
    apply_selected_presets(game);
    float selected_radius = game.wall_settings.radius_min;
    game.wall_settings.radius_min = 0.88f;
    game.valorant_sens = 0.555f;
    save_settings(game);
    Game general_only_loaded;
    load_settings(general_only_loaded);
    ok = self_test_check(std::fabs(general_only_loaded.valorant_sens - 0.555f) < 0.0001f, "general save persists sensitivity") && ok;
    ok = self_test_check(std::fabs(general_only_loaded.wall_settings.radius_min - selected_radius) < 0.0001f, "general save does not silently overwrite selected wall preset") && ok;

    {
        std::ofstream old("build/self-test-settings.cfg");
        old << "version 2\n";
        old << "selected_wall 0\n";
        old << "wall_preset \"OLD PASU\" 7 0.31 5.5 1.75 22 1.6\n";
        old << "pill_preset \"OLD PILL\" 1.24 4 12 0.35 2.4\n";
    }
    Game old_loaded;
    load_settings(old_loaded);
    ok = self_test_check(old_loaded.wall_preset_name == "OLD PASU", "old wall preset format loads") && ok;
    ok = self_test_check(old_loaded.wall_settings.target_count_min == 7 && old_loaded.wall_settings.target_count_max == 7, "old wall count migrates to fixed range") && ok;
    ok = self_test_check(std::fabs(old_loaded.wall_settings.radius_min - 0.31f) < 0.0001f && std::fabs(old_loaded.wall_settings.radius_max - 0.31f) < 0.0001f, "old radius migrates to fixed range") && ok;
    ok = self_test_check(std::fabs(old_loaded.wall_settings.change_min - 0.88f) < 0.0001f && std::fabs(old_loaded.wall_settings.change_max - 2.48f) < 0.0001f, "old direction interval migrates to randomized range") && ok;

    {
        std::ofstream legacy("build/self-test-settings.cfg");
        legacy << "wall_count 6\n";
        legacy << "wall_radius 0.27\n";
        legacy << "wall_hspeed 4.25\n";
        legacy << "wall_vspeed 2.25\n";
        legacy << "wall_accel 18\n";
        legacy << "wall_change 1.2\n";
    }
    Game legacy_loaded;
    load_settings(legacy_loaded);
    ok = self_test_check(legacy_loaded.wall_settings.target_count_min == 6 && legacy_loaded.wall_settings.target_count_max == 6, "legacy wall count key migrates to fixed range") && ok;
    ok = self_test_check(std::fabs(legacy_loaded.wall_settings.horizontal_speed_min - 4.25f) < 0.0001f && std::fabs(legacy_loaded.wall_settings.horizontal_speed_max - 4.25f) < 0.0001f, "legacy horizontal speed key migrates to fixed range") && ok;
    ok = self_test_check(std::fabs(legacy_loaded.wall_settings.change_min - 0.66f) < 0.0001f && std::fabs(legacy_loaded.wall_settings.change_max - 1.86f) < 0.0001f, "legacy direction interval key migrates to randomized range") && ok;
    std::remove(g_settings_path_override.c_str());
    g_settings_path_override.clear();

    Game physics;
    physics.wall_settings.radius_min = 0.25f;
    physics.wall_settings.radius_max = 0.25f;
    physics.wall_settings.horizontal_speed_min = 1.0f;
    physics.wall_settings.horizontal_speed_max = 1.0f;
    physics.wall_settings.vertical_speed_min = 0.0f;
    physics.wall_settings.vertical_speed_max = 0.0f;
    physics.wall_settings.acceleration_min = 0.0f;
    physics.wall_settings.acceleration_max = 0.0f;
    normalize_settings(physics);
    physics.targets.push_back({{-0.5f, ROOM_EYE_HEIGHT, ROOM_WALL_Z + 0.45f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f, physics.wall_settings.radius_min});
    physics.targets.push_back({{0.5f, ROOM_EYE_HEIGHT, ROOM_WALL_Z + 0.45f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 1000.0f, physics.wall_settings.radius_min});
    float before_a = physics.targets[0].vel.x;
    float before_b = physics.targets[1].vel.x;
    update_wall_targets(physics, 1.0f / 120.0f);
    ok = self_test_check(std::fabs(physics.targets[0].vel.x - before_a) < 0.0001f && std::fabs(physics.targets[1].vel.x - before_b) < 0.0001f, "wall targets do not interact before visible contact") && ok;

    Game collision;
    collision.wall_settings.radius_min = 0.25f;
    collision.wall_settings.radius_max = 0.25f;
    collision.wall_settings.horizontal_speed_min = 4.0f;
    collision.wall_settings.horizontal_speed_max = 4.0f;
    collision.wall_settings.vertical_speed_min = 0.0f;
    collision.wall_settings.vertical_speed_max = 0.0f;
    collision.wall_settings.acceleration_min = 0.0f;
    collision.wall_settings.acceleration_max = 0.0f;
    collision.targets.push_back({{-0.22f, ROOM_EYE_HEIGHT, ROOM_WALL_Z + 0.45f}, {2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 1000.0f, collision.wall_settings.radius_min});
    collision.targets.push_back({{0.22f, ROOM_EYE_HEIGHT + 0.05f, ROOM_WALL_Z + 0.45f}, {-2.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}, 1000.0f, collision.wall_settings.radius_min});
    update_wall_targets(collision, 1.0f / 120.0f);
    ok = self_test_check(std::fabs(collision.targets[0].vel.y) < 0.0001f && std::fabs(collision.targets[1].vel.y) < 0.0001f, "horizontal-only wall collisions do not create vertical movement") && ok;

    Game spawn_test;
    init_scenarios(spawn_test);
    spawn_test.wall_settings.radius_min = 0.16f;
    spawn_test.wall_settings.radius_max = 0.20f;
    spawn_test.wall_settings.target_count_min = 10;
    spawn_test.wall_settings.target_count_max = 10;
    spawn_test.wall_settings.horizontal_speed_min = 0.0f;
    spawn_test.wall_settings.horizontal_speed_max = 0.0f;
    spawn_test.wall_settings.vertical_speed_min = 0.0f;
    spawn_test.wall_settings.vertical_speed_max = 0.0f;
    start_scenario(spawn_test, spawn_test.scenarios[0]);
    for (int i = 0; i < static_cast<int>(spawn_test.targets.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(spawn_test.targets.size()); ++j) {
            Vec3 delta = spawn_test.targets[j].pos - spawn_test.targets[i].pos;
            float min_spacing = wall_spacing_for_radii(spawn_test.targets[i].radius, spawn_test.targets[j].radius) - 0.001f;
            ok = self_test_check(std::sqrt(delta.x * delta.x + delta.y * delta.y) >= min_spacing, "static wall spawns keep at least one radius of visible gap") && ok;
        }
    }

    Game timer_test;
    timer_test.rng.seed(123);
    timer_test.wall_settings.change_min = 0.4f;
    timer_test.wall_settings.change_max = 1.8f;
    bool saw_varied_timer = false;
    float first_timer = wall_change_timer(timer_test);
    for (int i = 0; i < 30; ++i) {
        float timer = wall_change_timer(timer_test);
        ok = self_test_check(timer >= 0.4f && timer <= 1.8f, "wall direction timers stay within randomized bounds") && ok;
        saw_varied_timer = saw_varied_timer || std::fabs(timer - first_timer) > 0.0001f;
    }
    ok = self_test_check(saw_varied_timer, "wall direction timers are randomized, not uniform") && ok;

    Game movement_sampling;
    movement_sampling.rng.seed(456);
    movement_sampling.wall_settings.radius_min = 0.14f;
    movement_sampling.wall_settings.radius_max = 0.34f;
    movement_sampling.wall_settings.horizontal_speed_min = 2.0f;
    movement_sampling.wall_settings.horizontal_speed_max = 6.0f;
    movement_sampling.wall_settings.vertical_speed_min = 0.0f;
    movement_sampling.wall_settings.vertical_speed_max = 3.0f;
    movement_sampling.wall_settings.acceleration_min = 5.0f;
    movement_sampling.wall_settings.acceleration_max = 15.0f;
    movement_sampling.wall_settings.change_min = 0.5f;
    movement_sampling.wall_settings.change_max = 1.5f;
    normalize_settings(movement_sampling);
    bool varied_h_speed = false;
    bool varied_v_speed = false;
    bool saw_shallow_vertical = false;
    bool saw_steep_vertical = false;
    float first_h = -1.0f;
    float first_v = -1.0f;
    for (int i = 0; i < 80; ++i) {
        Vec3 velocity = wall_desired_velocity(movement_sampling);
        float h = std::fabs(velocity.x);
        float v = std::fabs(velocity.y);
        if (i == 0) {
            first_h = h;
            first_v = v;
        }
        ok = self_test_check(h >= 2.0f && h <= 6.0f, "sampled horizontal wall speed stays within range") && ok;
        ok = self_test_check(v >= 0.0f && v <= 3.0f, "sampled vertical wall speed stays within range") && ok;
        varied_h_speed = varied_h_speed || std::fabs(h - first_h) > 0.001f;
        varied_v_speed = varied_v_speed || std::fabs(v - first_v) > 0.001f;
        saw_shallow_vertical = saw_shallow_vertical || v < 0.75f;
        saw_steep_vertical = saw_steep_vertical || v > 2.25f;
    }
    ok = self_test_check(varied_h_speed, "horizontal wall speed samples vary when min and max differ") && ok;
    ok = self_test_check(varied_v_speed, "vertical wall speed samples vary when min and max differ") && ok;
    ok = self_test_check(saw_shallow_vertical && saw_steep_vertical, "vertical wall sampling creates multiple movement angles") && ok;

    movement_sampling.targets.clear();
    for (int i = 0; i < 8; ++i) {
        movement_sampling.targets.push_back(spawn_wall_target(movement_sampling));
    }
    bool varied_radius = false;
    bool varied_accel = false;
    float first_radius = movement_sampling.targets[0].radius;
    float first_accel = movement_sampling.targets[0].acceleration;
    for (const Target& target : movement_sampling.targets) {
        ok = self_test_check(target.radius >= 0.14f && target.radius <= 0.34f, "sampled wall target radius stays within range") && ok;
        ok = self_test_check(target.acceleration >= 5.0f && target.acceleration <= 15.0f, "sampled wall target acceleration stays within range") && ok;
        varied_radius = varied_radius || std::fabs(target.radius - first_radius) > 0.001f;
        varied_accel = varied_accel || std::fabs(target.acceleration - first_accel) > 0.001f;
    }
    ok = self_test_check(varied_radius, "wall target sizes vary when radius min and max differ") && ok;
    ok = self_test_check(varied_accel, "wall target acceleration varies when acceleration min and max differ") && ok;

    Game count_sampling;
    count_sampling.rng.seed(789);
    bool saw_count_3 = false;
    bool saw_count_4 = false;
    bool saw_count_5 = false;
    for (int i = 0; i < 120; ++i) {
        int count = rand_wall_int_range(count_sampling, 3, 5);
        ok = self_test_check(count >= 3 && count <= 5, "sampled wall target count stays within range") && ok;
        saw_count_3 = saw_count_3 || count == 3;
        saw_count_4 = saw_count_4 || count == 4;
        saw_count_5 = saw_count_5 || count == 5;
    }
    ok = self_test_check(saw_count_3 && saw_count_4 && saw_count_5, "wall target count samples all integer values in range") && ok;

    if (ok) {
        std::printf("SELF TEST PASSED\n");
        return 0;
    }
    return 1;
}

static bool save_bmp_screenshot(const std::string& path, int w, int h) {
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    std::vector<uint8_t> flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    int stride = w * 4;
    for (int y = 0; y < h; ++y) {
        std::copy(
            pixels.begin() + static_cast<size_t>(h - 1 - y) * stride,
            pixels.begin() + static_cast<size_t>(h - y) * stride,
            flipped.begin() + static_cast<size_t>(y) * stride
        );
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        flipped.data(),
        w,
        h,
        32,
        stride,
        SDL_PIXELFORMAT_RGBA32
    );
    if (!surface) {
        std::fprintf(stderr, "SDL_CreateRGBSurfaceWithFormatFrom failed: %s\n", SDL_GetError());
        return false;
    }
    int rc = SDL_SaveBMP(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (rc != 0) {
        std::fprintf(stderr, "SDL_SaveBMP failed for %s: %s\n", path.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

static SDL_Window* create_gl_window(const char* title, int width, int height, bool fullscreen) {
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    return SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
}

static void make_dir_if_needed(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static bool render_debug_shot(int scenario_index, const std::string& path, int width, int height, int frames) {
    SDL_Window* window = create_gl_window("Aim Trainer Debug Shot", width, height, false);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_GL_SetSwapInterval(0);
    glEnable(GL_MULTISAMPLE);

    Game game;
    game.rng.seed(7);
    load_settings(game);
    init_scenarios(game);
    if (scenario_index < 0 || scenario_index >= static_cast<int>(game.scenarios.size())) {
        std::fprintf(stderr, "Invalid scenario index %d\n", scenario_index);
        SDL_GL_DeleteContext(gl);
        SDL_DestroyWindow(window);
        return false;
    }
    start_scenario(game, game.scenarios[scenario_index]);

    for (int frame = 0; frame < std::max(1, frames); ++frame) {
        Input input;
        update_playing(game, input, 1.0f / 60.0f);
        int drawable_w = 0, drawable_h = 0;
        SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
        draw_world(game, drawable_w, drawable_h);
        SDL_GL_SwapWindow(window);
    }

    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    bool ok = save_bmp_screenshot(path, drawable_w, drawable_h);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    return ok;
}

static bool render_debug_menu(const std::string& path, int width, int height, int tab_index = 0, int state_index = 0) {
    SDL_Window* window = create_gl_window("Aim Trainer Debug Menu", width, height, false);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_GL_SetSwapInterval(0);

    Game game;
    load_settings(game);
    init_scenarios(game);
    if (tab_index == 1) game.menu_tab = MenuTab::Tracking;
    else if (tab_index == 2) game.menu_tab = MenuTab::General;
    else game.menu_tab = MenuTab::Clicking;
    if (state_index == 1) {
        if (game.menu_tab == MenuTab::Tracking) {
            game.pill_preset_name.clear();
            game.edit_field = EditField::PillPresetName;
        } else {
            game.wall_preset_name.clear();
            game.edit_field = EditField::WallPresetName;
        }
    } else if (state_index == 2) {
        if (game.menu_tab == MenuTab::Tracking) {
            game.pill_preset_name = "THIS NAME IS EXACTLY MAX";
            game.edit_field = EditField::PillPresetName;
        } else {
            game.wall_preset_name = "THIS NAME IS EXACTLY MAX";
            game.edit_field = EditField::WallPresetName;
        }
    } else if (state_index == 3) {
        for (int i = 0; i < 12; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "LONG PRESET NAME %02d", i + 1);
            game.wall_presets.push_back({name, game.wall_settings});
            game.pill_presets.push_back({name, game.pill_settings});
        }
        ensure_presets(game);
        game.wall_preset_scroll = std::max(0, static_cast<int>(game.wall_presets.size()) - 7);
        game.pill_preset_scroll = std::max(0, static_cast<int>(game.pill_presets.size()) - 7);
    } else if (state_index == 4) {
        game.menu_tab = MenuTab::Clicking;
        game.wall_preset_name = "MAX RANGE STRESS TEST";
        game.wall_settings.target_count_min = 18;
        game.wall_settings.target_count_max = 18;
        game.wall_settings.radius_min = 0.88f;
        game.wall_settings.radius_max = 0.90f;
        game.wall_settings.horizontal_speed_min = 11.75f;
        game.wall_settings.horizontal_speed_max = 12.00f;
        game.wall_settings.vertical_speed_min = 11.75f;
        game.wall_settings.vertical_speed_max = 12.00f;
        game.wall_settings.acceleration_min = 79.0f;
        game.wall_settings.acceleration_max = 80.0f;
        game.wall_settings.change_min = 11.90f;
        game.wall_settings.change_max = 12.00f;
        normalize_settings(game);
    }
    Input input;
    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    draw_menu(game, input, drawable_w, drawable_h);
    bool ok = save_bmp_screenshot(path, drawable_w, drawable_h);
    SDL_GL_SwapWindow(window);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    return ok;
}

static int run_debug_mode(int argc, char** argv) {
    int width = 1280;
    int height = 720;
    int frames = 8;
    if (std::string(argv[1]) == "--debug-shot") {
        if (argc < 4) {
            std::fprintf(stderr, "Usage: %s --debug-shot <scenario-index> <out.bmp> [width height frames]\n", argv[0]);
            return 2;
        }
        if (argc >= 6) {
            width = std::max(320, std::atoi(argv[4]));
            height = std::max(180, std::atoi(argv[5]));
        }
        if (argc >= 7) {
            frames = std::max(1, std::atoi(argv[6]));
        }
        int scenario = std::atoi(argv[2]);
        return render_debug_shot(scenario, argv[3], width, height, frames) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--debug-menu") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: %s --debug-menu <out.bmp> [width height tab state]\n", argv[0]);
            return 2;
        }
        if (argc >= 5) {
            width = std::max(320, std::atoi(argv[3]));
            height = std::max(180, std::atoi(argv[4]));
        }
        int tab = argc >= 6 ? std::atoi(argv[5]) : 0;
        int state = argc >= 7 ? std::atoi(argv[6]) : 0;
        return render_debug_menu(argv[2], width, height, tab, state) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--debug-all") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: %s --debug-all <output-dir> [width height]\n", argv[0]);
            return 2;
        }
        if (argc >= 5) {
            width = std::max(320, std::atoi(argv[3]));
            height = std::max(180, std::atoi(argv[4]));
        }
        make_dir_if_needed(argv[2]);
        bool ok = true;
        Game scenario_count_game;
        init_scenarios(scenario_count_game);
        for (int scenario = 0; scenario < static_cast<int>(scenario_count_game.scenarios.size()); ++scenario) {
            char path[1024];
            std::snprintf(path, sizeof(path), "%s/scenario-%d.bmp", argv[2], scenario);
            ok = render_debug_shot(scenario, path, width, height, frames) && ok;
        }
        return ok ? 0 : 1;
    }
    return 2;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        int rc = run_self_test();
        SDL_Quit();
        return rc;
    }

    if (argc > 1 && (std::string(argv[1]) == "--debug-shot" || std::string(argv[1]) == "--debug-all" || std::string(argv[1]) == "--debug-menu")) {
        int rc = run_debug_mode(argc, argv);
        SDL_Quit();
        return rc;
    }

    SDL_Window* window = create_gl_window("Valorant Aim Trainer", 1280, 720, true);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_MULTISAMPLE);
    SDL_StartTextInput();

    Game game;
    load_settings(game);
    init_scenarios(game);

    uint64_t last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        Input input;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) input.quit = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0) input.escape_pressed = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) input.backspace_pressed = true;
            if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) && event.key.repeat == 0) input.enter_pressed = true;
            if (event.type == SDL_TEXTINPUT) input.text_input += event.text.text;
            if (event.type == SDL_MOUSEWHEEL) input.wheel_y += event.wheel.y;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                input.left_pressed = true;
                input.left_down = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) input.left_down = false;
            if (event.type == SDL_MOUSEMOTION) {
                input.mouse_x = event.motion.x;
                input.mouse_y = event.motion.y;
                input.rel_x += event.motion.xrel;
                input.rel_y += event.motion.yrel;
            }
        }
        int mx, my;
        uint32_t buttons = SDL_GetMouseState(&mx, &my);
        input.mouse_x = mx;
        input.mouse_y = my;
        input.left_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

        if (input.quit) {
            running = false;
        }
        if (input.escape_pressed) {
            if (game.mode == AppMode::Playing) game.mode = AppMode::Menu;
            else if (game.edit_field != EditField::None) game.edit_field = EditField::None;
            else running = false;
        }

        uint64_t now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency()));
        last = now;
        dt = std::min(dt, 1.0f / 20.0f);

        int window_w = 0, window_h = 0;
        int drawable_w = 0, drawable_h = 0;
        SDL_GetWindowSize(window, &window_w, &window_h);
        SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
        float scale_x = window_w > 0 ? static_cast<float>(drawable_w) / static_cast<float>(window_w) : 1.0f;
        float scale_y = window_h > 0 ? static_cast<float>(drawable_h) / static_cast<float>(window_h) : 1.0f;
        input.mouse_x = static_cast<int>(std::round(static_cast<float>(input.mouse_x) * scale_x));
        input.mouse_y = static_cast<int>(std::round(static_cast<float>(input.mouse_y) * scale_y));
        if (game.mode == AppMode::Playing) {
            set_mouse_grab(game, true);
            update_playing(game, input, dt);
            draw_world(game, drawable_w, drawable_h);
        } else {
            set_mouse_grab(game, false);
            draw_menu(game, input, drawable_w, drawable_h);
        }

        SDL_GL_SwapWindow(window);
    }

    set_mouse_grab(game, false);
    SDL_StopTextInput();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
