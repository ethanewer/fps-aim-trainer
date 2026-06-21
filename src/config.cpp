#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "world.hpp"

std::string g_settings_path_override;

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

std::string sanitize_preset_name(const std::string& raw) {
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

bool is_allowed_preset_char(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    char up = static_cast<char>(std::toupper(uc));
    return (up >= 'A' && up <= 'Z') || (up >= '0' && up <= '9') || up == ' ' || up == '-' || up == '_';
}

std::string filter_preset_name_draft(const std::string& raw) {
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

static void normalize_wall_settings(Game& game, WallClickSettings& settings) {
    settings.wall_distance = clampf(settings.wall_distance, 2.0f, 30.0f);
    normalize_float_range(settings.radius_min, settings.radius_max, 0.03f, 0.45f);
    int capacity = wall_capacity_for_radius(settings.radius_max, settings.wall_distance);
    normalize_int_range(settings.target_count_min, settings.target_count_max, 1, capacity);
    normalize_float_range(settings.horizontal_speed_min, settings.horizontal_speed_max, 0.0f, 8.0f);
    normalize_float_range(settings.vertical_speed_min, settings.vertical_speed_max, 0.0f, 8.0f);
    normalize_float_range(settings.acceleration_min, settings.acceleration_max, 0.0f, 40.0f);
    normalize_float_range(settings.change_min, settings.change_max, 0.0f, 12.0f);
    (void)game;
}

static void normalize_pill_settings(PillTrackingSettings& settings) {
    settings.width = clampf(settings.width, 0.20f, 2.50f);
    normalize_float_range(settings.distance_min, settings.distance_max, 1.5f, 30.0f);
    settings.speed = clampf(settings.speed, 0.0f, pill_speed_max_meters());
    settings.acceleration = clampf(settings.acceleration, 0.0f, pill_acceleration_max_meters());
    settings.change_min = clampf(settings.change_min, 0.05f, 8.0f);
    settings.change_max = clampf(settings.change_max, settings.change_min, 12.0f);
}

static void normalize_crosshair(CrosshairSettings& settings) {
    settings.length = clampf(settings.length, 4.0f, 24.0f);
    settings.gap = clampf(settings.gap, 0.0f, 16.0f);
    settings.thickness = clampf(settings.thickness, 1.0f, 6.0f);
}

void normalize_settings(Game& game) {
    normalize_wall_settings(game, game.wall_settings);
    normalize_pill_settings(game.pill_settings);
    normalize_crosshair(game.crosshair);
    game.sensitivity = clampf(game.sensitivity, 0.001f, 10.0f);
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

void ensure_presets(Game& game) {
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

void apply_selected_presets(Game& game) {
    ensure_presets(game);
    game.wall_settings = game.wall_presets[game.selected_wall_preset].settings;
    game.wall_preset_name = game.wall_presets[game.selected_wall_preset].name;
    game.pill_settings = game.pill_presets[game.selected_pill_preset].settings;
    game.pill_preset_name = game.pill_presets[game.selected_pill_preset].name;
}

void save_current_wall_preset(Game& game) {
    normalize_settings(game);
    game.wall_preset_name = unique_preset_name(game.wall_presets, game.wall_preset_name, game.selected_wall_preset);
    if (game.selected_wall_preset < 0 || game.selected_wall_preset >= static_cast<int>(game.wall_presets.size())) {
        game.wall_presets.push_back({game.wall_preset_name, game.wall_settings});
        game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    } else {
        game.wall_presets[game.selected_wall_preset] = {game.wall_preset_name, game.wall_settings};
    }
}

void save_current_pill_preset(Game& game) {
    normalize_settings(game);
    game.pill_preset_name = unique_preset_name(game.pill_presets, game.pill_preset_name, game.selected_pill_preset);
    if (game.selected_pill_preset < 0 || game.selected_pill_preset >= static_cast<int>(game.pill_presets.size())) {
        game.pill_presets.push_back({game.pill_preset_name, game.pill_settings});
        game.selected_pill_preset = static_cast<int>(game.pill_presets.size()) - 1;
    } else {
        game.pill_presets[game.selected_pill_preset] = {game.pill_preset_name, game.pill_settings};
    }
}

std::string settings_path() {
    if (!g_settings_path_override.empty()) {
        return g_settings_path_override;
    }
#ifdef _WIN32
    const char* base = std::getenv("APPDATA");
    if (base) {
        return std::string(base) + "\\aim_trainer.cfg";
    }
    return "aim_trainer.cfg";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.aim_trainer.cfg";
    }
    return ".aim_trainer.cfg";
#endif
}

void save_settings(const Game& game) {
    Game normalized = game;
    ensure_presets(normalized);
    normalize_settings(normalized);
    std::ofstream out(settings_path());
    if (!out) {
        return;
    }
    out << "version 4\n";
    out << "sensitivity " << normalized.sensitivity << "\n";
    out << "crosshair " << normalized.crosshair.length << " " << normalized.crosshair.gap << " " << normalized.crosshair.thickness << "\n";
    out << "selected_wall " << normalized.selected_wall_preset << "\n";
    out << "selected_pill " << normalized.selected_pill_preset << "\n";
    for (const WallPreset& preset : normalized.wall_presets) {
        out << "wall_preset " << std::quoted(preset.name) << " "
            << preset.settings.target_count_min << " "
            << preset.settings.target_count_max << " "
            << preset.settings.wall_distance << " "
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
            << preset.settings.distance_min << " "
            << preset.settings.distance_max << " "
            << preset.settings.speed << " "
            << preset.settings.acceleration << " "
            << preset.settings.change_min << " "
            << preset.settings.change_max << "\n";
    }
}

void load_settings(Game& game) {
    std::ifstream in(settings_path());
    if (!in) {
        ensure_presets(game);
        apply_selected_presets(game);
        return;
    }
    int settings_version = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key == "version") {
            row >> settings_version;
            continue;
        }
        if (key == "sensitivity") {
            row >> game.sensitivity;
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
            if (values.size() >= 13) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
                preset.settings.wall_distance = values[2];
                preset.settings.radius_min = values[3];
                preset.settings.radius_max = values[4];
                preset.settings.horizontal_speed_min = values[5];
                preset.settings.horizontal_speed_max = values[6];
                preset.settings.vertical_speed_min = values[7];
                preset.settings.vertical_speed_max = values[8];
                preset.settings.acceleration_min = values[9];
                preset.settings.acceleration_max = values[10];
                preset.settings.change_min = values[11];
                preset.settings.change_max = values[12];
            } else if (values.size() >= 12) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
                preset.settings.wall_distance = units_to_wall_meters(wall_camera_z() - ROOM_WALL_Z);
                preset.settings.radius_min = units_to_wall_meters(values[2]);
                preset.settings.radius_max = units_to_wall_meters(values[3]);
                preset.settings.horizontal_speed_min = units_to_wall_meters(values[4]);
                preset.settings.horizontal_speed_max = units_to_wall_meters(values[5]);
                preset.settings.vertical_speed_min = units_to_wall_meters(values[6]);
                preset.settings.vertical_speed_max = units_to_wall_meters(values[7]);
                preset.settings.acceleration_min = units_to_wall_meters(values[8]);
                preset.settings.acceleration_max = units_to_wall_meters(values[9]);
                preset.settings.change_min = values[10];
                preset.settings.change_max = values[11];
            } else if (values.size() >= 6) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = preset.settings.target_count_min;
                preset.settings.wall_distance = units_to_wall_meters(wall_camera_z() - ROOM_WALL_Z);
                preset.settings.radius_min = units_to_wall_meters(values[1]);
                preset.settings.radius_max = units_to_wall_meters(values[1]);
                preset.settings.horizontal_speed_min = units_to_wall_meters(values[2]);
                preset.settings.horizontal_speed_max = units_to_wall_meters(values[2]);
                preset.settings.vertical_speed_min = units_to_wall_meters(values[3]);
                preset.settings.vertical_speed_max = units_to_wall_meters(values[3]);
                preset.settings.acceleration_min = units_to_wall_meters(values[4]);
                preset.settings.acceleration_max = units_to_wall_meters(values[4]);
                preset.settings.change_min = values[5] <= 0.0f ? 0.0f : values[5] * 0.55f;
                preset.settings.change_max = values[5] <= 0.0f ? 0.0f : values[5] * 1.55f;
            }
            if (!preset.name.empty()) {
                game.wall_presets.push_back(preset);
            }
        } else if (key == "pill_preset") {
            PillPreset preset;
            row >> std::quoted(preset.name);
            std::vector<float> values;
            float value = 0.0f;
            while (row >> value) {
                values.push_back(value);
            }
            if (values.size() >= 7) {
                preset.settings.width = values[0];
                preset.settings.distance_min = values[1];
                preset.settings.distance_max = values[2];
                preset.settings.speed = values[3];
                preset.settings.acceleration = values[4];
                preset.settings.change_min = values[5];
                preset.settings.change_max = values[6];
            } else if (values.size() >= 5) {
                preset.settings.width = units_to_tracking_meters(values[0]);
                preset.settings.distance_min = units_to_tracking_meters(7.5f);
                preset.settings.distance_max = units_to_tracking_meters(10.5f);
                preset.settings.speed = units_to_tracking_meters(values[1]);
                preset.settings.acceleration = units_to_tracking_meters(values[2]);
                preset.settings.change_min = values[3];
                preset.settings.change_max = values[4];
            }
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
            else if (key == "wall_distance") game.wall_settings.wall_distance = value;
            else if (key == "wall_radius") game.wall_settings.radius_min = game.wall_settings.radius_max = units_to_wall_meters(value);
            else if (key == "wall_hspeed") game.wall_settings.horizontal_speed_min = game.wall_settings.horizontal_speed_max = units_to_wall_meters(value);
            else if (key == "wall_vspeed") game.wall_settings.vertical_speed_min = game.wall_settings.vertical_speed_max = units_to_wall_meters(value);
            else if (key == "wall_accel") game.wall_settings.acceleration_min = game.wall_settings.acceleration_max = units_to_wall_meters(value);
            else if (key == "wall_change") {
                game.wall_settings.change_min = value <= 0.0f ? 0.0f : value * 0.55f;
                game.wall_settings.change_max = value <= 0.0f ? 0.0f : value * 1.55f;
            }
            else if (key == "pill_width") game.pill_settings.width = units_to_tracking_meters(value);
            else if (key == "pill_dist_min") game.pill_settings.distance_min = value;
            else if (key == "pill_dist_max") game.pill_settings.distance_max = value;
            else if (key == "pill_speed") game.pill_settings.speed = units_to_tracking_meters(value);
            else if (key == "pill_accel") game.pill_settings.acceleration = units_to_tracking_meters(value);
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
