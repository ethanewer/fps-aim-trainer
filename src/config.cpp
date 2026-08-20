#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include "world.hpp"

#include "default_tasks.inc"

std::string g_settings_path_override;
std::string g_runs_path_override;
std::string g_default_tasks_dump_override;
std::string g_default_tasks_script_override;
bool g_live_default_tasks = true;

static void normalize_float_range(float& low, float& high, float min_allowed, float max_allowed) {
    low = clampf(low, min_allowed, max_allowed);
    high = clampf(high, min_allowed, max_allowed);
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
            if (name.size() < static_cast<size_t>(PRESET_NAME_MAX)) {
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
        if (is_allowed_preset_char(c) && name.size() < static_cast<size_t>(PRESET_NAME_MAX)) {
            name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    while (!name.empty() && name.front() == ' ') {
        name.erase(name.begin());
    }
    return name;
}

static void normalize_wall_settings(Game& game, WallClickSettings& settings) {
    normalize_float_range(settings.wall_distance_min, settings.wall_distance_max, 2.0f, 30.0f);
    normalize_float_range(settings.radius_min, settings.radius_max, WALL_TARGET_RADIUS_MIN_M, WALL_TARGET_RADIUS_MAX_M);
    int capacity = is_bounce(settings)
        ? bounce_capacity_for_radius(settings.radius_max, settings.wall_distance_max)
        : wall_capacity_for_radius(settings.radius_max, settings.wall_distance_max);
    settings.target_count_min = std::max(1, std::min(settings.target_count_min, capacity));
    settings.target_count_max = settings.target_count_min;
    normalize_float_range(settings.horizontal_speed_min, settings.horizontal_speed_max, 0.0f, 8.0f);
    normalize_float_range(settings.vertical_speed_min, settings.vertical_speed_max, 0.0f, 8.0f);
    normalize_float_range(settings.acceleration_min, settings.acceleration_max, 0.0f, 40.0f);
    normalize_float_range(settings.change_min, settings.change_max, 0.0f, 12.0f);
    normalize_float_range(settings.bounce_angle_min, settings.bounce_angle_max, 0.0f, 85.0f);
    normalize_float_range(settings.bounce_speed_min, settings.bounce_speed_max, 0.0f, 20.0f);
    settings.bounce_camera_height_m = clampf(settings.bounce_camera_height_m, BOUNCE_CAMERA_HEIGHT_MIN_M, 3.2f);
    settings.bounce_gravity_m = clampf(settings.bounce_gravity_m, 0.1f, 40.0f);
    settings.bounce_dir_change_p = clampf(settings.bounce_dir_change_p, 0.0f, 1.0f);
    settings.target_health = std::max(0, std::min(settings.target_health, WALL_TARGET_HEALTH_MAX));
    if (settings.task_mode != TaskMode::Tracking) {
        settings.task_mode = TaskMode::Clicking;
    }
    (void)game;
}

static void normalize_crosshair(CrosshairSettings& settings) {
    settings.length = clampf(settings.length, 0.0f, 24.0f);
    settings.gap = clampf(settings.gap, 0.0f, 20.0f);
    settings.thickness = clampf(settings.thickness, 1.0f, 10.0f);
    settings.outline_opacity = clampf(settings.outline_opacity, 0.0f, 1.0f);
    settings.outline_thickness = clampf(settings.outline_thickness, 1.0f, 6.0f);
    settings.center_dot_thickness = clampf(settings.center_dot_thickness, 1.0f, 6.0f);
}

static void normalize_target_color(TargetColorSettings& settings) {
    settings.r = std::max(0, std::min(settings.r, 255));
    settings.g = std::max(0, std::min(settings.g, 255));
    settings.b = std::max(0, std::min(settings.b, 255));
}

static void normalize_wall_color(WallColorSettings& settings) {
    settings.r = std::max(0, std::min(settings.r, 255));
    settings.g = std::max(0, std::min(settings.g, 255));
    settings.b = std::max(0, std::min(settings.b, 255));
}

static WallClickSettings settings_from_def(const DefaultTaskDef& def) {
    WallClickSettings settings;
    settings.task_mode = def.tracking ? TaskMode::Tracking : TaskMode::Clicking;
    settings.target_health = def.health;
    settings.target_count_min = def.targets_min;
    settings.target_count_max = def.targets_max;
    settings.wall_distance_min = def.wall_min;
    settings.wall_distance_max = def.wall_max;
    settings.radius_min = def.radius_min;
    settings.radius_max = def.radius_max;
    settings.horizontal_speed_min = def.h_speed_min;
    settings.horizontal_speed_max = def.h_speed_max;
    settings.vertical_speed_min = def.v_speed_min;
    settings.vertical_speed_max = def.v_speed_max;
    settings.acceleration_min = def.accel_min;
    settings.acceleration_max = def.accel_max;
    settings.change_min = def.change_min;
    settings.change_max = def.change_max;
    settings.bounce = def.bounce != 0;
    settings.bounce_angle_min = def.bounce_angle_min;
    settings.bounce_angle_max = def.bounce_angle_max;
    settings.bounce_speed_min = def.bounce_speed_min;
    settings.bounce_speed_max = def.bounce_speed_max;
    settings.bounce_camera_height_m = def.bounce_camera;
    settings.bounce_gravity_m = def.bounce_gravity;
    settings.bounce_dir_change_p = def.bounce_dir_change;
    return settings;
}

static std::vector<WallPreset> compiled_default_wall_presets() {
    std::vector<WallPreset> presets;
    presets.reserve(sizeof(kDefaultTasks) / sizeof(kDefaultTasks[0]));
    for (const DefaultTaskDef& def : kDefaultTasks) {
        presets.push_back({def.name, settings_from_def(def)});
    }
    return presets;
}

static std::vector<WallPreset> g_runtime_defaults;
static bool g_runtime_defaults_ready = false;

static std::vector<WallPreset> load_reset_presets();

static std::vector<WallPreset> default_wall_presets() {
    if (!g_live_default_tasks) {
        return compiled_default_wall_presets();
    }
    if (!g_runtime_defaults_ready) {
        g_runtime_defaults = load_reset_presets();
        g_runtime_defaults_ready = true;
    }
    return g_runtime_defaults.empty() ? compiled_default_wall_presets() : g_runtime_defaults;
}

static int wall_preset_index(const std::vector<WallPreset>& presets, const std::string& name) {
    for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
        if (presets[i].name == name) {
            return i;
        }
    }
    return -1;
}

static bool name_in_list(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

void remember_deleted_default_task(Game& game, const std::string& name) {
    if (name.empty() || name_in_list(game.deleted_default_tasks, name)) {
        return;
    }
    if (wall_preset_index(default_wall_presets(), name) < 0) {
        return;
    }
    game.deleted_default_tasks.push_back(name);
}

static void ensure_wall_presets(Game& game) {
    std::vector<WallPreset> defaults = default_wall_presets();
    if (game.wall_presets.empty()) {
        game.deleted_default_tasks.clear();
        game.wall_presets = defaults;
        return;
    }

    std::string selected_name;
    if (game.selected_wall_preset >= 0 && game.selected_wall_preset < static_cast<int>(game.wall_presets.size())) {
        selected_name = game.wall_presets[game.selected_wall_preset].name;
    }

    std::vector<WallPreset> ordered;
    std::vector<bool> used(game.wall_presets.size(), false);
    for (const WallPreset& preset : defaults) {
        int existing = wall_preset_index(game.wall_presets, preset.name);
        if (existing >= 0) {
            ordered.push_back(game.wall_presets[existing]);
            used[existing] = true;
        } else if (!name_in_list(game.deleted_default_tasks, preset.name)) {
            ordered.push_back(preset);
        }
    }
    for (int i = 0; i < static_cast<int>(game.wall_presets.size()); ++i) {
        if (!used[i]) {
            ordered.push_back(game.wall_presets[i]);
        }
    }
    game.wall_presets = ordered;
    if (!selected_name.empty()) {
        int selected = wall_preset_index(game.wall_presets, selected_name);
        if (selected >= 0) {
            game.selected_wall_preset = selected;
        }
    }
}

static bool wall_preset_exists(const std::vector<WallPreset>& presets, const std::string& name) {
    return wall_preset_index(presets, name) >= 0;
}

static void normalize_playlists(Game& game) {
    for (Playlist& playlist : game.playlists) {
        playlist.name = sanitize_preset_name(playlist.name);
        std::vector<std::string> kept;
        kept.reserve(playlist.task_names.size());
        for (const std::string& task_name : playlist.task_names) {
            if (wall_preset_exists(game.wall_presets, task_name)) {
                kept.push_back(task_name);
            }
        }
        playlist.task_names = kept;
    }
}

void normalize_settings(Game& game) {
    normalize_wall_settings(game, game.wall_settings);
    normalize_crosshair(game.crosshair);
    normalize_target_color(game.target_color);
    normalize_wall_color(game.wall_color);
    game.sensitivity = clampf(game.sensitivity, 0.001f, 10.0f);
    for (WallPreset& preset : game.wall_presets) {
        preset.name = sanitize_preset_name(preset.name);
        if (preset.name == "MIGRATED CLICK") {
            preset.name = kDefaultTasks[0].name;
        }
        normalize_wall_settings(game, preset.settings);
    }
    normalize_playlists(game);
}

static void clamp_playlist_indices(Game& game) {
    int playlist_count = static_cast<int>(game.playlists.size());
    if (playlist_count <= 0) {
        game.selected_playlist = 0;
        game.playlist_scroll = 0;
        game.selected_playlist_entry = 0;
        game.playlist_entry_scroll = 0;
        return;
    }
    game.selected_playlist = std::max(0, std::min(game.selected_playlist, playlist_count - 1));
    game.playlist_scroll = std::max(0, std::min(game.playlist_scroll, std::max(0, playlist_count - VISIBLE_PLAYLIST_ROWS)));
    int entry_count = static_cast<int>(game.playlists[game.selected_playlist].task_names.size());
    if (entry_count <= 0) {
        game.selected_playlist_entry = 0;
        game.playlist_entry_scroll = 0;
    } else {
        game.selected_playlist_entry = std::max(0, std::min(game.selected_playlist_entry, entry_count - 1));
        game.playlist_entry_scroll = std::max(0, std::min(game.playlist_entry_scroll, std::max(0, entry_count - VISIBLE_PLAYLIST_ENTRY_ROWS)));
    }
    game.playlist_add_scroll = std::max(0, game.playlist_add_scroll);
}

void ensure_presets(Game& game) {
    ensure_wall_presets(game);
    normalize_settings(game);
    game.selected_wall_preset = std::max(0, std::min(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()) - 1));
    game.wall_preset_scroll = std::max(0, std::min(game.wall_preset_scroll, std::max(0, static_cast<int>(game.wall_presets.size()) - VISIBLE_PRESET_ROWS)));
    clamp_playlist_indices(game);
}

void apply_selected_presets(Game& game) {
    ensure_presets(game);
    game.wall_settings = game.wall_presets[game.selected_wall_preset].settings;
    game.wall_preset_name = game.wall_presets[game.selected_wall_preset].name;
}

static bool file_exists(const std::string& path) {
    std::ifstream in(path);
    return static_cast<bool>(in);
}

static std::string path_parent(const std::string& path) {
    std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return path.substr(0, 1);
    }
    return path.substr(0, slash);
}

static std::string path_join(const std::string& dir, const std::string& name) {
    if (dir.empty()) {
        return name;
    }
    char last = dir.back();
    if (last == '/' || last == '\\') {
        return dir + name;
    }
    return dir + "/" + name;
}

static std::string executable_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    return path_parent(buf);
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) {
        return ".";
    }
    return path_parent(buf);
#else
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return ".";
    }
    buf[n] = 0;
    return path_parent(buf);
#endif
}

static std::string current_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetCurrentDirectoryA(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    return buf;
#else
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == nullptr) {
        return ".";
    }
    return buf;
#endif
}

static std::string find_default_tasks_script() {
    if (!g_default_tasks_script_override.empty()) {
        return g_default_tasks_script_override;
    }
    std::string dirs[] = {
        executable_dir(),
        path_parent(executable_dir()),
        current_dir(),
        path_parent(current_dir()),
    };
    for (const std::string& dir : dirs) {
        std::string script = path_join(path_join(dir, "scripts"), "default-tasks.py");
        if (file_exists(script)) {
            return script;
        }
    }
    return {};
}

#ifdef _WIN32
static std::string run_process(const std::vector<std::string>& args) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE read_out = nullptr;
    HANDLE write_out = nullptr;
    if (!CreatePipe(&read_out, &write_out, &sa, 0)) {
        return {};
    }
    SetHandleInformation(read_out, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::string cmdline;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            cmdline += ' ';
        }
        cmdline += '"';
        for (char c : args[i]) {
            if (c == '"') {
                cmdline += '\\';
            }
            cmdline += c;
        }
        cmdline += '"';
    }
    std::vector<char> cmd(cmdline.begin(), cmdline.end());
    cmd.push_back('\0');

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = write_out;
    si.hStdError = nul != INVALID_HANDLE_VALUE ? nul : write_out;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(write_out);
    if (nul != INVALID_HANDLE_VALUE) {
        CloseHandle(nul);
    }
    std::string out;
    if (ok) {
        char buf[4096];
        DWORD n = 0;
        while (ReadFile(read_out, buf, sizeof(buf), &n, nullptr) && n > 0) {
            out.append(buf, buf + n);
        }
        WaitForSingleObject(pi.hProcess, 20000);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (code != 0) {
            out.clear();
        }
    }
    CloseHandle(read_out);
    return out;
}
#else
static std::string run_process(const std::vector<std::string>& args) {
    std::string cmd;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i != 0) {
            cmd += ' ';
        }
        cmd += '\'';
        for (char c : args[i]) {
            if (c == '\'') {
                cmd += "'\\''";
            } else {
                cmd += c;
            }
        }
        cmd += '\'';
    }
    cmd += " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return {};
    }
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        out += buf;
    }
    int status = pclose(pipe);
    if (status != 0) {
        return {};
    }
    return out;
}
#endif

static std::string run_python_dump(const std::string& script) {
    std::vector<std::vector<std::string>> commands = {
        {"python", script, "--dump"},
        {"python3", script, "--dump"},
        {"py", "-3", script, "--dump"},
    };
    for (const std::vector<std::string>& args : commands) {
        std::string out = run_process(args);
        if (!out.empty()) {
            return out;
        }
    }
    return {};
}

static std::vector<WallPreset> parse_default_task_dump(const std::string& text) {
    std::vector<WallPreset> presets;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream row(line);
        std::string key;
        row >> key;
        if (key != "task") {
            continue;
        }
        WallPreset preset;
        if (!(row >> std::quoted(preset.name)) || preset.name.empty()) {
            continue;
        }
        std::vector<float> values;
        float value = 0.0f;
        while (row >> value) {
            values.push_back(value);
        }
        if (values.size() < 16) {
            continue;
        }
        preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
        preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
        preset.settings.wall_distance_min = values[2];
        preset.settings.wall_distance_max = values[3];
        preset.settings.radius_min = values[4];
        preset.settings.radius_max = values[5];
        preset.settings.horizontal_speed_min = values[6];
        preset.settings.horizontal_speed_max = values[7];
        preset.settings.vertical_speed_min = values[8];
        preset.settings.vertical_speed_max = values[9];
        preset.settings.acceleration_min = values[10];
        preset.settings.acceleration_max = values[11];
        preset.settings.change_min = values[12];
        preset.settings.change_max = values[13];
        preset.settings.task_mode = static_cast<int>(std::round(values[14])) == 1 ? TaskMode::Tracking : TaskMode::Clicking;
        preset.settings.target_health = static_cast<int>(std::round(values[15]));
        if (values.size() >= 17) {
            preset.settings.bounce = static_cast<int>(std::round(values[16])) == 1;
        }
        if (values.size() >= 22) {
            preset.settings.bounce_angle_min = values[17];
            preset.settings.bounce_angle_max = values[18];
            preset.settings.bounce_speed_min = values[19];
            preset.settings.bounce_speed_max = values[20];
            preset.settings.bounce_camera_height_m = values[21];
        }
        if (values.size() >= 23) {
            preset.settings.bounce_gravity_m = values[22];
        } else if (values.size() >= 17) {
            preset.settings.bounce_gravity_m = BOUNCE_GRAVITY_LEGACY_M;
        }
        if (values.size() >= 24) {
            preset.settings.bounce_dir_change_p = values[23];
        }
        presets.push_back(preset);
    }
    return presets;
}

static std::vector<WallPreset> load_reset_presets() {
    if (!g_default_tasks_dump_override.empty()) {
        std::ifstream in(g_default_tasks_dump_override);
        std::ostringstream text;
        text << in.rdbuf();
        std::vector<WallPreset> parsed = parse_default_task_dump(text.str());
        if (!parsed.empty()) {
            return parsed;
        }
    }
    if (g_live_default_tasks) {
        std::string script = find_default_tasks_script();
        if (!script.empty()) {
            std::vector<WallPreset> parsed = parse_default_task_dump(run_python_dump(script));
            if (!parsed.empty()) {
                return parsed;
            }
        }
    }
    return compiled_default_wall_presets();
}

void reset_wall_presets(Game& game) {
    game.active_field = FieldId::None;
    game.edit_draft.clear();
    game.edit_fresh = false;
    std::vector<WallPreset> loaded = load_reset_presets();
    if (loaded.empty()) {
        loaded = compiled_default_wall_presets();
    }
    g_runtime_defaults = loaded;
    g_runtime_defaults_ready = true;
    game.deleted_default_tasks.clear();
    game.wall_presets = loaded;
    game.selected_wall_preset = 0;
    game.wall_preset_scroll = 0;
    game.preset_search.clear();
    normalize_settings(game);
    if (game.wall_presets.empty()) {
        game.wall_presets = compiled_default_wall_presets();
    }
    game.selected_wall_preset = 0;
    game.wall_settings = game.wall_presets.front().settings;
    game.wall_preset_name = game.wall_presets.front().name;
}

void rename_task_in_playlists(Game& game, const std::string& old_name, const std::string& new_name) {
    if (old_name.empty() || old_name == new_name) {
        return;
    }
    for (Playlist& playlist : game.playlists) {
        for (std::string& task_name : playlist.task_names) {
            if (task_name == old_name) {
                task_name = new_name;
            }
        }
    }
}

void remove_task_from_playlists(Game& game, const std::string& name) {
    if (name.empty()) {
        return;
    }
    for (Playlist& playlist : game.playlists) {
        playlist.task_names.erase(
            std::remove(playlist.task_names.begin(), playlist.task_names.end(), name),
            playlist.task_names.end());
    }
    clamp_playlist_indices(game);
}

bool playlist_has_playable_tasks(const Game& game) {
    if (game.selected_playlist < 0 || game.selected_playlist >= static_cast<int>(game.playlists.size())) {
        return false;
    }
    for (const std::string& task_name : game.playlists[game.selected_playlist].task_names) {
        if (wall_preset_exists(game.wall_presets, task_name)) {
            return true;
        }
    }
    return false;
}

void apply_selected_playlist(Game& game) {
    ensure_presets(game);
    if (game.playlists.empty()) {
        game.playlist_name.clear();
        game.selected_playlist_entry = 0;
        game.playlist_entry_scroll = 0;
        return;
    }
    game.playlist_name = game.playlists[game.selected_playlist].name;
    int entry_count = static_cast<int>(game.playlists[game.selected_playlist].task_names.size());
    game.selected_playlist_entry = entry_count > 0 ? std::min(game.selected_playlist_entry, entry_count - 1) : 0;
}

void save_current_playlist(Game& game) {
    normalize_settings(game);
    std::string old_name;
    if (game.selected_playlist >= 0 && game.selected_playlist < static_cast<int>(game.playlists.size())) {
        old_name = game.playlists[game.selected_playlist].name;
    }
    game.playlist_name = unique_preset_name(game.playlists, game.playlist_name, game.selected_playlist);
    if (game.selected_playlist < 0 || game.selected_playlist >= static_cast<int>(game.playlists.size())) {
        game.playlists.push_back({game.playlist_name, {}});
        game.selected_playlist = static_cast<int>(game.playlists.size()) - 1;
    } else {
        game.playlists[game.selected_playlist].name = game.playlist_name;
    }
    if (game.playlist_paused && game.playlist_play_name == old_name) {
        game.playlist_play_name = game.playlist_name;
    }
}

void save_current_wall_preset(Game& game) {
    normalize_settings(game);
    std::string old_name;
    if (game.selected_wall_preset >= 0 && game.selected_wall_preset < static_cast<int>(game.wall_presets.size())) {
        old_name = game.wall_presets[game.selected_wall_preset].name;
    }
    game.wall_preset_name = unique_preset_name(game.wall_presets, game.wall_preset_name, game.selected_wall_preset);
    if (game.selected_wall_preset < 0 || game.selected_wall_preset >= static_cast<int>(game.wall_presets.size())) {
        game.wall_presets.push_back({game.wall_preset_name, game.wall_settings});
        game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    } else {
        game.wall_presets[game.selected_wall_preset] = {game.wall_preset_name, game.wall_settings};
    }
    rename_task_in_playlists(game, old_name, game.wall_preset_name);
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
    if (normalized.wall_presets.empty()) {
        ensure_presets(normalized);
    }
    normalize_settings(normalized);
    std::ofstream out(settings_path());
    if (!out) {
        return;
    }
    out << "version 19\n";
    out << "sensitivity " << normalized.sensitivity << "\n";
    out << "crosshair " << normalized.crosshair.length << " " << normalized.crosshair.gap << " "
        << normalized.crosshair.thickness << " "
        << (normalized.crosshair.outlines ? 1 : 0) << " " << normalized.crosshair.outline_opacity << " "
        << normalized.crosshair.outline_thickness << " "
        << (normalized.crosshair.center_dot ? 1 : 0) << " " << normalized.crosshair.center_dot_thickness << "\n";
    out << "target_color " << normalized.target_color.r << " " << normalized.target_color.g << " " << normalized.target_color.b << "\n";
    out << "wall_color " << normalized.wall_color.r << " " << normalized.wall_color.g << " " << normalized.wall_color.b << "\n";
    out << "selected_wall " << normalized.selected_wall_preset << "\n";
    out << "selected_playlist " << normalized.selected_playlist << "\n";
    for (const WallPreset& preset : normalized.wall_presets) {
        out << "wall_preset " << std::quoted(preset.name) << " "
            << preset.settings.target_count_min << " "
            << preset.settings.target_count_max << " "
            << preset.settings.wall_distance_min << " "
            << preset.settings.wall_distance_max << " "
            << preset.settings.radius_min << " "
            << preset.settings.radius_max << " "
            << preset.settings.horizontal_speed_min << " "
            << preset.settings.horizontal_speed_max << " "
            << preset.settings.vertical_speed_min << " "
            << preset.settings.vertical_speed_max << " "
            << preset.settings.acceleration_min << " "
            << preset.settings.acceleration_max << " "
            << preset.settings.change_min << " "
            << preset.settings.change_max << " "
            << (preset.settings.task_mode == TaskMode::Tracking ? 1 : 0) << " "
            << preset.settings.target_health << " "
            << (preset.settings.bounce ? 1 : 0) << " "
            << preset.settings.bounce_angle_min << " "
            << preset.settings.bounce_angle_max << " "
            << preset.settings.bounce_speed_min << " "
            << preset.settings.bounce_speed_max << " "
            << preset.settings.bounce_camera_height_m << " "
            << preset.settings.bounce_gravity_m << " "
            << preset.settings.bounce_dir_change_p << "\n";
    }
    for (const std::string& name : normalized.deleted_default_tasks) {
        out << "deleted_default " << std::quoted(name) << "\n";
    }
    for (const Playlist& playlist : normalized.playlists) {
        out << "playlist " << std::quoted(playlist.name);
        for (const std::string& task_name : playlist.task_names) {
            out << " " << std::quoted(task_name);
        }
        out << "\n";
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
            std::vector<float> extra;
            float value = 0.0f;
            while (row >> value) {
                extra.push_back(value);
            }
            if (extra.size() >= 5) {
                // outlines, opacity, thickness, center_dot, dot_thickness
                game.crosshair.outlines = extra[0] != 0.0f;
                game.crosshair.outline_opacity = extra[1];
                game.crosshair.outline_thickness = extra[2];
                game.crosshair.center_dot = extra[3] != 0.0f;
                game.crosshair.center_dot_thickness = extra[4];
            } else if (extra.size() >= 4) {
                // v12 without opacity: outlines, thickness, center_dot, dot_thickness
                game.crosshair.outlines = extra[0] != 0.0f;
                game.crosshair.outline_thickness = extra[1];
                game.crosshair.center_dot = extra[2] != 0.0f;
                game.crosshair.center_dot_thickness = extra[3];
            }
        } else if (key == "target_color") {
            row >> game.target_color.r >> game.target_color.g >> game.target_color.b;
        } else if (key == "wall_color") {
            row >> game.wall_color.r >> game.wall_color.g >> game.wall_color.b;
        } else if (key == "selected_wall") {
            row >> game.selected_wall_preset;
        } else if (key == "selected_playlist") {
            row >> game.selected_playlist;
        } else if (key == "deleted_default") {
            std::string name;
            if (row >> std::quoted(name) && !name.empty() && !name_in_list(game.deleted_default_tasks, name)) {
                game.deleted_default_tasks.push_back(name);
            }
        } else if (key == "playlist") {
            Playlist playlist;
            if (row >> std::quoted(playlist.name)) {
                std::string task_name;
                while (row >> std::quoted(task_name)) {
                    if (!task_name.empty()) {
                        playlist.task_names.push_back(task_name);
                    }
                }
                if (!playlist.name.empty()) {
                    game.playlists.push_back(playlist);
                }
            }
        } else if (key == "selected_pill") {
            continue;  // pill tracking was removed; ignore leftover keys
        } else if (key == "wall_preset") {
            WallPreset preset;
            row >> std::quoted(preset.name);
            std::vector<float> values;
            float value = 0.0f;
            while (row >> value) {
                values.push_back(value);
            }
            if (values.size() >= 14) {
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
                preset.settings.wall_distance_min = values[2];
                preset.settings.wall_distance_max = values[3];
                preset.settings.radius_min = values[4];
                preset.settings.radius_max = values[5];
                preset.settings.horizontal_speed_min = values[6];
                preset.settings.horizontal_speed_max = values[7];
                preset.settings.vertical_speed_min = values[8];
                preset.settings.vertical_speed_max = values[9];
                preset.settings.acceleration_min = values[10];
                preset.settings.acceleration_max = values[11];
                preset.settings.change_min = values[12];
                preset.settings.change_max = values[13];
                if (values.size() >= 16) {
                    preset.settings.task_mode = static_cast<int>(std::round(values[14])) == 1 ? TaskMode::Tracking : TaskMode::Clicking;
                    preset.settings.target_health = static_cast<int>(std::round(values[15]));
                }
                if (values.size() >= 17) {
                    preset.settings.bounce = static_cast<int>(std::round(values[16])) == 1;
                }
                if (values.size() >= 22) {
                    preset.settings.bounce_angle_min = values[17];
                    preset.settings.bounce_angle_max = values[18];
                    preset.settings.bounce_speed_min = values[19];
                    preset.settings.bounce_speed_max = values[20];
                    preset.settings.bounce_camera_height_m = values[21];
                }
                if (values.size() >= 23) {
                    preset.settings.bounce_gravity_m = values[22];
                } else if (values.size() >= 17) {
                    preset.settings.bounce_gravity_m = BOUNCE_GRAVITY_LEGACY_M;
                }
                if (values.size() >= 24) {
                    preset.settings.bounce_dir_change_p = values[23];
                }
            } else if (values.size() >= 13) {
                // v4: a single wall distance -> migrate to a min==max range.
                preset.settings.target_count_min = static_cast<int>(std::round(values[0]));
                preset.settings.target_count_max = static_cast<int>(std::round(values[1]));
                preset.settings.wall_distance_min = values[2];
                preset.settings.wall_distance_max = values[2];
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
                preset.settings.wall_distance_min = units_to_wall_meters(wall_camera_z() - ROOM_WALL_Z);
                preset.settings.wall_distance_max = preset.settings.wall_distance_min;
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
                preset.settings.wall_distance_min = units_to_wall_meters(wall_camera_z() - ROOM_WALL_Z);
                preset.settings.wall_distance_max = preset.settings.wall_distance_min;
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
            continue;  // pill tracking was removed; ignore leftover presets
        } else {
            float value = 0.0f;
            std::istringstream legacy(line);
            legacy >> key >> value;
            if (key == "wall_count") {
                game.wall_settings.target_count_min = static_cast<int>(std::round(value));
                game.wall_settings.target_count_max = game.wall_settings.target_count_min;
            }
            else if (key == "wall_distance") game.wall_settings.wall_distance_min = game.wall_settings.wall_distance_max = value;
            else if (key == "wall_radius") game.wall_settings.radius_min = game.wall_settings.radius_max = units_to_wall_meters(value);
            else if (key == "wall_hspeed") game.wall_settings.horizontal_speed_min = game.wall_settings.horizontal_speed_max = units_to_wall_meters(value);
            else if (key == "wall_vspeed") game.wall_settings.vertical_speed_min = game.wall_settings.vertical_speed_max = units_to_wall_meters(value);
            else if (key == "wall_accel") game.wall_settings.acceleration_min = game.wall_settings.acceleration_max = units_to_wall_meters(value);
            else if (key == "wall_change") {
                game.wall_settings.change_min = value <= 0.0f ? 0.0f : value * 0.55f;
                game.wall_settings.change_max = value <= 0.0f ? 0.0f : value * 1.55f;
            }
        }
    }
    if (game.wall_presets.empty()) {
        game.wall_presets.push_back({kDefaultTasks[0].name, game.wall_settings});
    }
    // v8 stored unused health 0 on clicking presets. Health 0 now means infinite
    // for both modes, so one-shot clicking must migrate to 1.
    if (settings_version < 9) {
        auto migrate_clicking_health = [](WallClickSettings& settings) {
            if (settings.task_mode != TaskMode::Tracking && settings.target_health == 0) {
                settings.target_health = 1;
            }
        };
        migrate_clicking_health(game.wall_settings);
        for (WallPreset& preset : game.wall_presets) {
            migrate_clicking_health(preset.settings);
        }
    }
    // v10: built-in tasks use an 8-10m wall range.
    if (settings_version < 10) {
        auto migrate_builtin_wall = [](const std::string& name, WallClickSettings& settings) {
            for (const DefaultTaskDef& def : kDefaultTasks) {
                if (name == def.name) {
                    settings.wall_distance_min = def.wall_min;
                    settings.wall_distance_max = def.wall_max;
                    return;
                }
            }
            static const char* kLegacyMid[] = {
                "1W4T DYNAMIC",
                "1W4TS DYNAMIC",
                "1W4T STRAFE",
                "1W4TS STRAFE",
                "1W4TES STRAFE",
                "1W4T STATIC",
                "1W4TES STATIC",
                "1W3T DYNAMIC",
                "1W3TS DYNAMIC",
                "1W6T STRAFE",
                "1W6TS STRAFE",
                "1W6TES STRAFE",
                "1W2T STATIC",
                "1W4TS STATIC",
                "1W8TES STATIC",
                "1W16T STATIC",
            };
            for (const char* legacy : kLegacyMid) {
                if (name == legacy) {
                    settings.wall_distance_min = 8.0f;
                    settings.wall_distance_max = 10.0f;
                    return;
                }
            }
        };
        migrate_builtin_wall(game.wall_preset_name, game.wall_settings);
        for (WallPreset& preset : game.wall_presets) {
            migrate_builtin_wall(preset.name, preset.settings);
        }
    }
    // v13 Bounce 180 used the old room half-width (~3.56m). Move it to the same
    // 8-10m range as 1W2T DYNAMIC so the ball stays at a similar distance.
    if (settings_version < 14) {
        auto migrate_bounce_range = [](const std::string& name, WallClickSettings& settings) {
            if (name != "THE BOUNCE 180") {
                return;
            }
            float old_even = bounce_equal_wall_distance_m();
            if (std::fabs(settings.wall_distance_min - old_even) < 0.05f &&
                std::fabs(settings.wall_distance_max - old_even) < 0.05f) {
                settings.wall_distance_min = 8.0f;
                settings.wall_distance_max = 10.0f;
            }
        };
        migrate_bounce_range(game.wall_preset_name, game.wall_settings);
        for (WallPreset& preset : game.wall_presets) {
            migrate_bounce_range(preset.name, preset.settings);
        }
    }
    if (settings_version < 15) {
        auto migrate_bounce_count = [](const std::string& name, WallClickSettings& settings) {
            if (name != "THE BOUNCE 180" || !settings.bounce) {
                return;
            }
            if (settings.target_count_min == 1 && settings.target_count_max == 1) {
                settings.target_count_min = BOUNCE_DEFAULT_TARGETS;
                settings.target_count_max = BOUNCE_DEFAULT_TARGETS;
            }
        };
        migrate_bounce_count(game.wall_preset_name, game.wall_settings);
        for (WallPreset& preset : game.wall_presets) {
            migrate_bounce_count(preset.name, preset.settings);
        }
    }
    apply_selected_presets(game);
    apply_selected_playlist(game);
}

std::string runs_path() {
    if (!g_runs_path_override.empty()) {
        return g_runs_path_override;
    }
#ifdef _WIN32
    const char* base = std::getenv("APPDATA");
    if (base) {
        return std::string(base) + "\\aim_trainer_runs.cfg";
    }
    return "aim_trainer_runs.cfg";
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.aim_trainer_runs.cfg";
    }
    return ".aim_trainer_runs.cfg";
#endif
}

void save_runs(const Game& game) {
    std::ofstream out(runs_path());
    if (!out) {
        return;
    }
    out << "version 1\n";
    for (const RunRecord& run : game.runs) {
        out << "run " << static_cast<int>(run.kind) << " "
            << run.score << " "
            << run.shots << " "
            << run.accuracy << " "
            << run.duration << " "
            << run.timestamp << " "
            << std::quoted(run.preset_name) << "\n";
    }
}

void load_runs(Game& game) {
    game.runs.clear();
    std::ifstream in(runs_path());
    if (!in) {
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
        if (key != "run") {
            continue;
        }
        int kind = 0;
        RunRecord run;
        if (row >> kind >> run.score >> run.shots >> run.accuracy >> run.duration >> run.timestamp >> std::quoted(run.preset_name)) {
            run.kind = kind == static_cast<int>(ScenarioKind::Tracking) ? ScenarioKind::Tracking : ScenarioKind::WallClick;
            game.runs.push_back(run);
        }
    }
}

int best_run_score(const Game& game, ScenarioKind kind, const std::string& preset_name) {
    int best = -1;
    for (const RunRecord& run : game.runs) {
        if (run.kind == kind && run.preset_name == preset_name) {
            best = std::max(best, run.score);
        }
    }
    return best;
}
