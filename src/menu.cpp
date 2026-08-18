#include "menu.hpp"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "render.hpp"
#include "scenario.hpp"
#include "world.hpp"

// ---------------------------------------------------------------------------
// Field descriptors: map a FieldId to the value it edits and its limits.
// ---------------------------------------------------------------------------

struct FieldDesc {
    enum Kind { Name, Search, IntVal, FloatVal };
    Kind kind = FloatVal;
    float* fptr = nullptr;
    int* iptr = nullptr;
    std::string* sptr = nullptr;
    float lo = 0.0f;
    float hi = 0.0f;
    int decimals = 2;
};

static FieldDesc f_float(float* p, float lo, float hi, int decimals) {
    FieldDesc d;
    d.kind = FieldDesc::FloatVal;
    d.fptr = p;
    d.lo = lo;
    d.hi = hi;
    d.decimals = decimals;
    return d;
}

static FieldDesc f_int(int* p, int lo, int hi) {
    FieldDesc d;
    d.kind = FieldDesc::IntVal;
    d.iptr = p;
    d.lo = static_cast<float>(lo);
    d.hi = static_cast<float>(hi);
    return d;
}

static FieldDesc f_name(std::string* p) {
    FieldDesc d;
    d.kind = FieldDesc::Name;
    d.sptr = p;
    return d;
}

static FieldDesc f_search(std::string* p) {
    FieldDesc d;
    d.kind = FieldDesc::Search;
    d.sptr = p;
    return d;
}

static FieldDesc field_desc(Game& g, FieldId id) {
    int capacity = wall_capacity_for_radius(g.wall_settings.radius_max, g.wall_settings.wall_distance_max);
    switch (id) {
        case FieldId::PresetSearch: return f_search(&g.preset_search);
        case FieldId::WallName: return f_name(&g.wall_preset_name);
        case FieldId::WallHealth: return f_int(&g.wall_settings.target_health, 0, WALL_TARGET_HEALTH_MAX);
        case FieldId::WallDistMin: return f_float(&g.wall_settings.wall_distance_min, 2.0f, 30.0f, 2);
        case FieldId::WallDistMax: return f_float(&g.wall_settings.wall_distance_max, 2.0f, 30.0f, 2);
        case FieldId::WallTargetsMin: return f_int(&g.wall_settings.target_count_min, 1, capacity);
        case FieldId::WallRadiusMin: return f_float(&g.wall_settings.radius_min, WALL_TARGET_RADIUS_MIN_M, WALL_TARGET_RADIUS_MAX_M, 2);
        case FieldId::WallRadiusMax: return f_float(&g.wall_settings.radius_max, WALL_TARGET_RADIUS_MIN_M, WALL_TARGET_RADIUS_MAX_M, 2);
        case FieldId::WallHSpeedMin: return f_float(&g.wall_settings.horizontal_speed_min, 0.0f, 8.0f, 2);
        case FieldId::WallHSpeedMax: return f_float(&g.wall_settings.horizontal_speed_max, 0.0f, 8.0f, 2);
        case FieldId::WallVSpeedMin: return f_float(&g.wall_settings.vertical_speed_min, 0.0f, 8.0f, 2);
        case FieldId::WallVSpeedMax: return f_float(&g.wall_settings.vertical_speed_max, 0.0f, 8.0f, 2);
        case FieldId::WallAccelMin: return f_float(&g.wall_settings.acceleration_min, 0.0f, 40.0f, 2);
        case FieldId::WallAccelMax: return f_float(&g.wall_settings.acceleration_max, 0.0f, 40.0f, 2);
        case FieldId::WallDirMin: return f_float(&g.wall_settings.change_min, 0.0f, 12.0f, 2);
        case FieldId::WallDirMax: return f_float(&g.wall_settings.change_max, 0.0f, 12.0f, 2);
        case FieldId::PlaylistSearch: return f_search(&g.playlist_search);
        case FieldId::PlaylistName: return f_name(&g.playlist_name);
        case FieldId::PlaylistAddSearch: return f_search(&g.playlist_add_search);
        case FieldId::GenSens: return f_float(&g.sensitivity, 0.001f, 10.0f, 3);
        case FieldId::GenLength: return f_float(&g.crosshair.length, 4.0f, 24.0f, 0);
        case FieldId::GenGap: return f_float(&g.crosshair.gap, 0.0f, 16.0f, 0);
        case FieldId::GenThick: return f_float(&g.crosshair.thickness, 1.0f, 6.0f, 0);
        case FieldId::GenTargetR: return f_int(&g.target_color.r, 0, 255);
        case FieldId::GenTargetG: return f_int(&g.target_color.g, 0, 255);
        case FieldId::GenTargetB: return f_int(&g.target_color.b, 0, 255);
        case FieldId::GenWallR: return f_int(&g.wall_color.r, 0, 255);
        case FieldId::GenWallG: return f_int(&g.wall_color.g, 0, 255);
        case FieldId::GenWallB: return f_int(&g.wall_color.b, 0, 255);
        default: return FieldDesc{};
    }
}

static std::string format_field(Game& g, FieldId id) {
    FieldDesc d = field_desc(g, id);
    char buffer[32];
    if (d.kind == FieldDesc::Name || d.kind == FieldDesc::Search) {
        return d.sptr ? *d.sptr : std::string();
    }
    if (d.kind == FieldDesc::IntVal) {
        std::snprintf(buffer, sizeof(buffer), "%d", d.iptr ? *d.iptr : 0);
        return buffer;
    }
    std::snprintf(buffer, sizeof(buffer), "%.*f", d.decimals, d.fptr ? *d.fptr : 0.0f);
    return buffer;
}

// Writes the active draft text into its bound value. Numbers that fail to parse
// (empty, ".", etc.) leave the previous value untouched. Clamping is handled by
// the per-frame normalize_settings() call.
static void commit_to_value(Game& g, FieldId id) {
    if (id == FieldId::None) {
        return;
    }
    FieldDesc d = field_desc(g, id);
    if (d.kind == FieldDesc::Name) {
        // sanitize_preset_name keeps the committed name in sync with what SAVE
        // would store, and falls back to "UNTITLED" so the box is never blank.
        if (d.sptr) {
            *d.sptr = sanitize_preset_name(g.edit_draft);
        }
        return;
    }
    if (d.kind == FieldDesc::Search) {
        if (d.sptr) {
            *d.sptr = g.edit_draft;
        }
        return;
    }
    if (g.edit_draft.empty()) {
        return;
    }
    try {
        float value = std::stof(g.edit_draft);
        if (d.kind == FieldDesc::IntVal) {
            if (d.iptr) *d.iptr = static_cast<int>(std::lround(value));
        } else {
            if (d.fptr) *d.fptr = value;
        }
    } catch (...) {
        // Unparseable draft: keep the previous value.
    }
}

// ---------------------------------------------------------------------------
// Focus / edit state machine.
// ---------------------------------------------------------------------------

void menu_focus_field(Game& g, FieldId id) {
    if (g.active_field != FieldId::None && g.active_field != id) {
        commit_to_value(g, g.active_field);
    }
    g.active_field = id;
    g.edit_draft = format_field(g, id);
    FieldDesc focused = field_desc(g, id);
    g.edit_fresh = focused.kind == FieldDesc::IntVal || focused.kind == FieldDesc::FloatVal;
}

void menu_blur_field(Game& g) {
    if (g.active_field != FieldId::None) {
        commit_to_value(g, g.active_field);
    }
    g.active_field = FieldId::None;
    g.edit_draft.clear();
    g.edit_fresh = false;
}

void menu_cancel_edit(Game& g) {
    g.active_field = FieldId::None;
    g.edit_draft.clear();
    g.edit_fresh = false;
}

static const FieldId WALL_ORDER[] = {
    FieldId::PresetSearch, FieldId::WallName, FieldId::WallTargetsMin, FieldId::WallHealth,
    FieldId::WallDistMin, FieldId::WallDistMax,
    FieldId::WallRadiusMin, FieldId::WallRadiusMax,
    FieldId::WallHSpeedMin, FieldId::WallHSpeedMax,
    FieldId::WallVSpeedMin, FieldId::WallVSpeedMax,
    FieldId::WallAccelMin, FieldId::WallAccelMax,
    FieldId::WallDirMin, FieldId::WallDirMax,
};
static const FieldId PLAYLIST_ORDER[] = {
    FieldId::PlaylistSearch, FieldId::PlaylistName, FieldId::PlaylistAddSearch,
};
static const FieldId GEN_ORDER[] = {
    FieldId::GenSens, FieldId::GenLength, FieldId::GenGap, FieldId::GenThick,
    FieldId::GenTargetR, FieldId::GenTargetG, FieldId::GenTargetB,
    FieldId::GenWallR, FieldId::GenWallG, FieldId::GenWallB,
};

static void tab_field_order(const Game& g, const FieldId** order, int* count) {
    switch (g.menu_tab) {
        case MenuTab::Clicking:
            *order = WALL_ORDER;
            *count = static_cast<int>(sizeof(WALL_ORDER) / sizeof(WALL_ORDER[0]));
            break;
        case MenuTab::Playlists:
            *order = PLAYLIST_ORDER;
            *count = static_cast<int>(sizeof(PLAYLIST_ORDER) / sizeof(PLAYLIST_ORDER[0]));
            break;
        default:
            *order = GEN_ORDER;
            *count = static_cast<int>(sizeof(GEN_ORDER) / sizeof(GEN_ORDER[0]));
            break;
    }
}

static FieldId field_step(const Game& g, int dir) {
    const FieldId* order = nullptr;
    int count = 0;
    tab_field_order(g, &order, &count);
    int idx = 0;
    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (order[i] == g.active_field) {
            idx = i;
            found = true;
            break;
        }
    }
    if (!found) {
        return order[0];  // active field not in this tab: start at the first box
    }
    idx = (idx + dir + count) % count;
    return order[idx];
}

void menu_handle_edit(Game& g, const Input& input) {
    if (g.active_field == FieldId::None) {
        return;
    }
    FieldDesc d = field_desc(g, g.active_field);
    size_t max_len = d.kind == FieldDesc::FloatVal ? 6 : 5;
    if (d.kind == FieldDesc::Name || d.kind == FieldDesc::Search) {
        max_len = static_cast<size_t>(PRESET_NAME_MAX);
    }
    for (char c : input.text_input) {
        if (d.kind == FieldDesc::Name) {
            if (is_allowed_preset_char(c) && g.edit_draft.size() < max_len) {
                g.edit_draft.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            continue;
        }
        if (d.kind == FieldDesc::Search) {
            if (is_allowed_preset_char(c) && g.edit_draft.size() < max_len) {
                g.edit_draft.push_back(c);
            }
            continue;
        }
        if (d.kind == FieldDesc::IntVal && c == '.') {
            break;  // a dot ends integer entry rather than joining the digits around it
        }
        // A fresh numeric box is cleared by the first accepted keystroke, so the
        // "has a dot already" test must look at the post-clear (empty) draft.
        bool has_dot = !g.edit_fresh && g.edit_draft.find('.') != std::string::npos;
        bool is_digit = c >= '0' && c <= '9';
        bool is_dot = c == '.' && d.kind == FieldDesc::FloatVal && !has_dot;
        if (!is_digit && !is_dot) {
            continue;  // reject without disturbing the value or fresh state
        }
        if (g.edit_fresh) {
            g.edit_draft.clear();
            g.edit_fresh = false;
        }
        if (g.edit_draft.size() < max_len) {
            g.edit_draft.push_back(c);
        }
    }
    if (input.backspace_pressed) {
        g.edit_fresh = false;
        if (!g.edit_draft.empty()) {
            g.edit_draft.pop_back();
        }
    }
    if (d.kind == FieldDesc::Name) {
        g.edit_draft = filter_preset_name_draft(g.edit_draft);
    }
    if (input.tab_pressed) {
        menu_focus_field(g, field_step(g, input.shift_down ? -1 : 1));
    } else if (input.enter_pressed) {
        menu_blur_field(g);
    }
}

// ---------------------------------------------------------------------------
// Preset actions.
// ---------------------------------------------------------------------------

static std::string ascii_lower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

static bool preset_name_matches(const std::string& name, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    return ascii_lower(name).find(ascii_lower(query)) != std::string::npos;
}

std::vector<int> matching_wall_presets(const Game& game) {
    std::string query = game.preset_search;
    if (game.active_field == FieldId::PresetSearch) {
        query = game.edit_draft;
    }
    std::vector<int> matches;
    matches.reserve(game.wall_presets.size());
    for (int i = 0; i < static_cast<int>(game.wall_presets.size()); ++i) {
        if (preset_name_matches(game.wall_presets[i].name, query)) {
            matches.push_back(i);
        }
    }
    return matches;
}

std::vector<int> matching_playlists(const Game& game) {
    std::string query = game.playlist_search;
    if (game.active_field == FieldId::PlaylistSearch) {
        query = game.edit_draft;
    }
    std::vector<int> matches;
    matches.reserve(game.playlists.size());
    for (int i = 0; i < static_cast<int>(game.playlists.size()); ++i) {
        if (preset_name_matches(game.playlists[i].name, query)) {
            matches.push_back(i);
        }
    }
    return matches;
}

static std::vector<int> matching_playlist_add_tasks(const Game& game) {
    std::string query = game.playlist_add_search;
    if (game.active_field == FieldId::PlaylistAddSearch) {
        query = game.edit_draft;
    }
    std::vector<int> matches;
    matches.reserve(game.wall_presets.size());
    for (int i = 0; i < static_cast<int>(game.wall_presets.size()); ++i) {
        if (preset_name_matches(game.wall_presets[i].name, query)) {
            matches.push_back(i);
        }
    }
    return matches;
}

// Adjusts `scroll` so the selected row is within the visible window.
static void reveal_selected_row(int selected, int count, int& scroll, int visible_rows) {
    if (selected < scroll) {
        scroll = selected;
    } else if (selected > scroll + visible_rows - 1) {
        scroll = selected - (visible_rows - 1);
    }
    scroll = std::max(0, std::min(scroll, std::max(0, count - visible_rows)));
}

void new_wall_preset(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "CLICK PRESET %d", static_cast<int>(game.wall_presets.size()) + 1);
    game.active_field = FieldId::None;
    game.preset_search.clear();
    std::string unique_name = unique_preset_name(game.wall_presets, name, -1);
    game.wall_presets.push_back({unique_name, game.wall_settings});
    game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    game.wall_settings = game.wall_presets.back().settings;
    game.wall_preset_name = game.wall_presets.back().name;
    reveal_selected_row(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()), game.wall_preset_scroll, VISIBLE_PRESET_ROWS);
}

void delete_wall_preset(Game& game) {
    game.active_field = FieldId::None;
    std::string removed;
    if (game.selected_wall_preset >= 0 && game.selected_wall_preset < static_cast<int>(game.wall_presets.size())) {
        removed = game.wall_presets[game.selected_wall_preset].name;
    }
    if (game.wall_presets.size() <= 1) {
        game.wall_presets.clear();
        game.selected_wall_preset = 0;
    } else {
        game.wall_presets.erase(game.wall_presets.begin() + game.selected_wall_preset);
        game.selected_wall_preset = std::max(0, std::min(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()) - 1));
    }
    remove_task_from_playlists(game, removed);
    apply_selected_presets(game);
    reveal_selected_row(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()), game.wall_preset_scroll, VISIBLE_PRESET_ROWS);
}

void new_playlist(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "PLAYLIST %d", static_cast<int>(game.playlists.size()) + 1);
    game.active_field = FieldId::None;
    game.playlist_search.clear();
    std::vector<std::string> copied;
    if (game.selected_playlist >= 0 && game.selected_playlist < static_cast<int>(game.playlists.size())) {
        copied = game.playlists[game.selected_playlist].task_names;
    }
    std::string unique_name = unique_preset_name(game.playlists, name, -1);
    game.playlists.push_back({unique_name, copied});
    game.selected_playlist = static_cast<int>(game.playlists.size()) - 1;
    game.playlist_name = game.playlists.back().name;
    game.selected_playlist_entry = 0;
    game.playlist_entry_scroll = 0;
    reveal_selected_row(game.selected_playlist, static_cast<int>(game.playlists.size()), game.playlist_scroll, VISIBLE_PLAYLIST_ROWS);
}

void delete_playlist(Game& game) {
    game.active_field = FieldId::None;
    if (game.playlists.empty()) {
        return;
    }
    std::string removed = game.playlists[game.selected_playlist].name;
    game.playlists.erase(game.playlists.begin() + game.selected_playlist);
    if (game.playlist_paused && game.playlist_play_name == removed) {
        clear_playlist_session(game);
    }
    if (game.playlists.empty()) {
        game.selected_playlist = 0;
        game.playlist_name.clear();
        game.selected_playlist_entry = 0;
        game.playlist_entry_scroll = 0;
        game.playlist_scroll = 0;
        return;
    }
    game.selected_playlist = std::max(0, std::min(game.selected_playlist, static_cast<int>(game.playlists.size()) - 1));
    apply_selected_playlist(game);
    reveal_selected_row(game.selected_playlist, static_cast<int>(game.playlists.size()), game.playlist_scroll, VISIBLE_PLAYLIST_ROWS);
}

// ---------------------------------------------------------------------------
// Widgets.
// ---------------------------------------------------------------------------

static const float VALUE_SCALE = 1.85f;

static void draw_card(float x, float y, float w, float h) {
    rect(x + 4.0f, y + 4.0f, w, h, 0, 0, 0, 90);            // soft shadow
    rect(x, y, w, h, 83, 88, 98);                            // border
    rect(x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, 30, 35, 43); // fill
}

static void divider(float x, float y, float w) {
    rect(x, y, w, 1.0f, 60, 68, 80);
}

static bool hit(const Input& in, float x, float y, float w, float h) {
    return in.mouse_x >= x && in.mouse_x <= x + w && in.mouse_y >= y && in.mouse_y <= y + h;
}

// Label like "Radius [m]" drawn with a bright name and a dimmer unit tag.
static void field_label(float x, float y, const std::string& label) {
    size_t bracket = label.find('[');
    if (bracket == std::string::npos) {
        text(x, y, label, VALUE_SCALE, 230, 236, 244);
        return;
    }
    std::string name = label.substr(0, bracket);
    std::string unit = label.substr(bracket);
    text(x, y, name, VALUE_SCALE, 230, 236, 244);
    text(x + text_width(name, VALUE_SCALE), y, unit, VALUE_SCALE, 150, 162, 178);
}

static bool secondary_button(const Input& in, float x, float y, float w, float h, const std::string& label, float scale) {
    bool hovered = hit(in, x, y, w, h);
    rect(x, y, w, h, hovered ? 52 : 40, hovered ? 60 : 46, hovered ? 72 : 56);
    rect(x, y, w, 2.0f, 94, 108, 125);
    rect(x, y + h - 2.0f, w, 2.0f, 94, 108, 125);
    rect(x, y, 2.0f, h, 94, 108, 125);
    rect(x + w - 2.0f, y, 2.0f, h, 94, 108, 125);
    float lw = text_width(label, scale);
    text(x + std::max(8.0f, (w - lw) * 0.5f), y + std::max(6.0f, (h - text_height(scale)) * 0.5f), label, scale, 225, 232, 240);
    return hovered && in.left_pressed;
}

static bool primary_button(const Input& in, float x, float y, float w, float h, const std::string& label, float scale, bool enabled = true) {
    bool hovered = enabled && hit(in, x, y, w, h);
    rect(x + 3.0f, y + 3.0f, w, h, 0, 0, 0, 90);  // shadow
    if (!enabled) rect(x, y, w, h, 88, 70, 76);
    else if (hovered) rect(x, y, w, h, 255, 95, 108);
    else rect(x, y, w, h, 255, 70, 85);
    rect(x, y, w, 2.0f, enabled ? 255 : 140, enabled ? 120 : 90, enabled ? 132 : 98);            // top highlight
    float lw = text_width(label, scale);
    text(x + std::max(8.0f, (w - lw) * 0.5f), y + std::max(6.0f, (h - text_height(scale)) * 0.5f), label, scale,
         enabled ? 248 : 180, enabled ? 248 : 180, enabled ? 248 : 184);
    return enabled && hovered && in.left_pressed;
}

static bool toggle_button(const Input& in, float x, float y, float w, float h, const std::string& label, bool selected) {
    bool hovered = hit(in, x, y, w, h);
    if (selected) rect(x, y, w, h, 255, 70, 85);
    else if (hovered) rect(x, y, w, h, 52, 60, 72);
    else rect(x, y, w, h, 40, 46, 56);
    uint8_t br = selected ? 255 : (hovered ? 132 : 94);
    uint8_t bg = selected ? 120 : (hovered ? 148 : 108);
    uint8_t bb = selected ? 132 : (hovered ? 168 : 125);
    rect(x, y, w, 2.0f, br, bg, bb);
    rect(x, y + h - 2.0f, w, 2.0f, br, bg, bb);
    rect(x, y, 2.0f, h, br, bg, bb);
    rect(x + w - 2.0f, y, 2.0f, h, br, bg, bb);
    float scale = VALUE_SCALE;
    float lw = text_width(label, scale);
    text(x + std::max(6.0f, (w - lw) * 0.5f), y + std::max(6.0f, (h - text_height(scale)) * 0.5f), label, scale,
         selected ? 248 : 225, selected ? 248 : 232, selected ? 248 : 240);
    return hovered && in.left_pressed && !selected;
}

static void tab_button(Game& g, const Input& in, float x, float y, const std::string& label, MenuTab tab) {
    const float w = 156.0f, h = 32.0f;
    bool selected = g.menu_tab == tab;
    bool hovered = hit(in, x, y, w, h);
    rect(x, y, w, h,
         selected ? 46 : (hovered ? 40 : 28),
         selected ? 52 : (hovered ? 46 : 33),
         selected ? 62 : (hovered ? 56 : 41));
    float scale = 2.0f;
    float lw = text_width(label, scale);
    text(x + (w - lw) * 0.5f, y + (h - text_height(scale)) * 0.5f, label, scale,
         selected ? 245 : 180, selected ? 248 : 190, selected ? 252 : 204);
    if (selected) {
        rect(x, y + h, w, 3.0f, 255, 70, 85);
    }
    if (hovered && in.left_pressed && !selected) {
        menu_blur_field(g);
        g.menu_tab = tab;
    }
}

// A text input box bound to a field. Click focuses it; the active box shows the
// live draft with a caret, others show the formatted value.
static void value_box(Game& g, const Input& in, FieldId id, float x, float y, float w, float h) {
    bool active = g.active_field == id;
    bool hovered = hit(in, x, y, w, h);
    if (active) rect(x, y, w, h, 43, 51, 61);
    else if (hovered) rect(x, y, w, h, 40, 45, 55);
    else rect(x, y, w, h, 32, 38, 46);
    uint8_t br = active ? 255 : (hovered ? 132 : 88);
    uint8_t bg = active ? 70 : (hovered ? 148 : 103);
    uint8_t bb = active ? 85 : (hovered ? 168 : 121);
    rect(x, y, w, 2.0f, br, bg, bb);
    rect(x, y + h - 2.0f, w, 2.0f, br, bg, bb);
    rect(x, y, 2.0f, h, br, bg, bb);
    rect(x + w - 2.0f, y, 2.0f, h, br, bg, bb);

    FieldDesc d = field_desc(g, id);
    std::string shown = active ? g.edit_draft : format_field(g, id);
    if (active) {
        shown += "_";
    }
    float ty = y + (h - text_height(VALUE_SCALE)) * 0.5f;
    if (d.kind == FieldDesc::Name || d.kind == FieldDesc::Search) {
        if (!active && shown.empty() && d.kind == FieldDesc::Search) {
            const char* hint = "Search tasks";
            if (id == FieldId::PlaylistSearch) {
                hint = "Search playlists";
            }
            text_fit(x + 10.0f, ty, hint, VALUE_SCALE, w - 20.0f, 120, 130, 145);
        } else {
            text_fit(x + 10.0f, ty, shown, VALUE_SCALE, w - 20.0f, 240, 244, 248);
        }
    } else {
        float tw = text_width(shown, VALUE_SCALE);
        float tx = std::max(x + 8.0f, x + w - 12.0f - tw);
        text(tx, ty, shown, VALUE_SCALE, 240, 244, 248);
    }
    if (hovered && in.left_pressed && !active) {
        menu_focus_field(g, id);
    }
}

static void row_single(Game& g, const Input& in, float label_x, float box_x, float row_y, const std::string& label, FieldId id, float box_w, float box_h) {
    field_label(label_x, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, label);
    value_box(g, in, id, box_x, row_y, box_w, box_h);
}

static void row_range(Game& g, const Input& in, float label_x, float min_x, float max_x, float row_y, const std::string& label, FieldId min_id, FieldId max_id, float box_w, float box_h) {
    field_label(label_x, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, label);
    value_box(g, in, min_id, min_x, row_y, box_w, box_h);
    value_box(g, in, max_id, max_x, row_y, box_w, box_h);
}

static void color_swatch(int r, int g, int b, float x, float y, float w, float h) {
    rect(x, y, w, h, 92, 104, 122);
    rect(x + 2.0f, y + 2.0f, w - 4.0f, h - 4.0f,
         static_cast<uint8_t>(r),
         static_cast<uint8_t>(g),
         static_cast<uint8_t>(b));
}

static void color_row(Game& g, const Input& in, float label_x, float value_x, float row_y, const std::string& label, FieldId r_id, FieldId g_id, FieldId b_id, int r, int green, int b) {
    field_label(label_x, row_y + (28.0f - text_height(VALUE_SCALE)) * 0.5f, label);
    value_box(g, in, r_id, value_x, row_y, 64.0f, 28.0f);
    value_box(g, in, g_id, value_x + 88.0f, row_y, 64.0f, 28.0f);
    value_box(g, in, b_id, value_x + 176.0f, row_y, 64.0f, 28.0f);
    color_swatch(r, green, b, value_x + 256.0f, row_y, 72.0f, 28.0f);
}

// ---------------------------------------------------------------------------
// Tab content.
// ---------------------------------------------------------------------------

static const float CARD_Y = 140.0f;
static const float CARD_H = 608.0f;
static const float SIDEBAR_W = 280.0f;
static const float EDITOR_DX = 294.0f;
static const float EDITOR_W = 662.0f;
static const float ROW_H = 26.0f;
static const float ROW_PITCH = 28.0f;
static const float LIST_ROW_H = 28.0f;
static const float LIST_PITCH = 30.0f;
static const float BTN_H = 26.0f;
static const float BTN_SCALE = 1.7f;

static void draw_preset_sidebar(Game& g, const Input& in, float x) {
    draw_card(x, CARD_Y, SIDEBAR_W, CARD_H);

    float row_x = x + 12.0f;
    float row_w = SIDEBAR_W - 24.0f;
    value_box(g, in, FieldId::PresetSearch, row_x, CARD_Y + 10.0f, row_w, ROW_H);

    std::vector<int> matches = matching_wall_presets(g);
    int count = static_cast<int>(matches.size());
    int& scroll = g.wall_preset_scroll;
    int& selected = g.selected_wall_preset;
    int max_scroll = std::max(0, count - VISIBLE_PRESET_ROWS);
    if (in.wheel_y != 0) {
        scroll = std::max(0, std::min(scroll - in.wheel_y, max_scroll));
    }
    scroll = std::max(0, std::min(scroll, max_scroll));

    float list_y = CARD_Y + 42.0f;
    if (count == 0) {
        text(row_x + 4.0f, list_y + 8.0f, "No matching tasks", 1.6f, 120, 130, 145);
    }
    for (int row = 0; row < VISIBLE_PRESET_ROWS; ++row) {
        int match_index = scroll + row;
        if (match_index >= count) {
            break;
        }
        int index = matches[match_index];
        const std::string& name = g.wall_presets[index].name;
        bool is_selected = index == selected;
        float y = list_y + static_cast<float>(row) * LIST_PITCH;
        bool clicked = list_button(in, row_x, y, row_w, LIST_ROW_H, name, is_selected);
        if (is_selected) {
            rect(row_x, y, 4.0f, LIST_ROW_H, 255, 70, 85);
        }
        if (clicked) {
            menu_blur_field(g);
            if (is_selected) {
                start_scenario(g, g.scenarios[0], RunMode::Challenge);
            } else {
                selected = index;
                g.wall_settings = g.wall_presets[index].settings;
                g.wall_preset_name = g.wall_presets[index].name;
            }
        }
    }

    float button_y = CARD_Y + CARD_H - 96.0f;
    if (secondary_button(in, row_x, button_y, 118.0f, BTN_H, "New", BTN_SCALE)) {
        menu_blur_field(g);
        new_wall_preset(g);
    }
    if (secondary_button(in, row_x + 128.0f, button_y, row_w - 128.0f, BTN_H, "Delete", BTN_SCALE)) {
        menu_blur_field(g);
        delete_wall_preset(g);
    }
    if (secondary_button(in, row_x, button_y + 32.0f, row_w, BTN_H, "Save preset", BTN_SCALE)) {
        menu_blur_field(g);
        save_current_wall_preset(g);
        save_settings(g);
    }
    if (secondary_button(in, row_x, button_y + 64.0f, row_w, BTN_H, "Reset tasks", BTN_SCALE)) {
        menu_blur_field(g);
        reset_wall_presets(g);
        save_settings(g);
    }
}

// Shared editor card chrome: title, BEST readout, NAME box, PRACTICE/CHALLENGE.
static void draw_editor_header(Game& g, const Input& in, float x, const std::string& title, FieldId name_id, ScenarioKind kind) {
    draw_card(x, CARD_Y, EDITOR_W, CARD_H);
    float cl = x + 14.0f;
    text(cl, CARD_Y + 10.0f, title, 2.3f, 230, 236, 244);

    int best = best_run_score(g, kind, g.wall_preset_name);
    if (best >= 0) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "Best %d", best);
        std::string best_text = buf;
        text(x + EDITOR_W - 14.0f - text_width(best_text, 1.7f), CARD_Y + 14.0f, best_text, 1.7f, 255, 200, 90);
    }

    float row_y = CARD_Y + 38.0f;
    field_label(cl, row_y + (ROW_H - text_height(VALUE_SCALE)) * 0.5f, "Name");
    float name_box_x = cl + 58.0f;
    float name_box_w = 252.0f;
    value_box(g, in, name_id, name_box_x, row_y, name_box_w, ROW_H);

    float buttons_x = name_box_x + name_box_w + 12.0f;
    float buttons_right = x + EDITOR_W - 14.0f;
    float gap = 8.0f;
    float bw = (buttons_right - buttons_x - gap) * 0.5f;
    if (secondary_button(in, buttons_x, row_y, bw, ROW_H, "Practice", BTN_SCALE)) {
        menu_blur_field(g);
        start_scenario(g, g.scenarios[0], RunMode::Practice);
    }
    if (primary_button(in, buttons_x + bw + gap, row_y, bw, ROW_H, "Challenge", BTN_SCALE)) {
        menu_blur_field(g);
        start_scenario(g, g.scenarios[0], RunMode::Challenge);
    }
    divider(cl, CARD_Y + 72.0f, EDITOR_W - 28.0f);
}

static void draw_column_headers(float min_x, float max_x, float box_w, float y) {
    const float scale = 1.5f;
    std::string mn = "Min";
    std::string mx = "Max";
    text(min_x + (box_w - text_width(mn, scale)) * 0.5f, y, mn, scale, 150, 162, 178);
    text(max_x + (box_w - text_width(mx, scale)) * 0.5f, y, mx, scale, 150, 162, 178);
}

static void draw_footer_hint(float x) {
    divider(x + 14.0f, CARD_Y + CARD_H - 34.0f, EDITOR_W - 28.0f);
    text(x + 14.0f, CARD_Y + CARD_H - 24.0f, "Tab next   Enter commits   Esc cancels", 1.5f, 150, 162, 178);
}

static void draw_tasks_tab(Game& g, const Input& in, float left) {
    draw_preset_sidebar(g, in, left);

    float x = left + EDITOR_DX;
    bool tracking = is_tracking(g.wall_settings.task_mode);
    ScenarioKind kind = tracking ? ScenarioKind::Tracking : ScenarioKind::WallClick;
    draw_editor_header(g, in, x, tracking ? "Wall tracking" : "Wall clicking", FieldId::WallName, kind);

    float cl = x + 14.0f;
    float inner_right = cl + EDITOR_W - 28.0f;
    const float label_w = 148.0f;
    const float col_gap = 10.0f;
    float min_x = cl + label_w;
    float box_w = (inner_right - min_x - col_gap) * 0.5f;
    float max_x = min_x + box_w + col_gap;
    float box_h = ROW_H;
    const float pitch = ROW_PITCH;

    float row_y = CARD_Y + 82.0f;
    field_label(cl, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, "Task");
    if (toggle_button(in, min_x, row_y, box_w, box_h, "Clicking", !tracking)) {
        menu_blur_field(g);
        g.wall_settings.task_mode = TaskMode::Clicking;
    }
    if (toggle_button(in, max_x, row_y, box_w, box_h, "Tracking", tracking)) {
        menu_blur_field(g);
        g.wall_settings.task_mode = TaskMode::Tracking;
    }
    row_y += pitch;

    field_label(cl, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, "Targets");
    value_box(g, in, FieldId::WallTargetsMin, min_x, row_y, box_w, box_h);
    row_y += pitch;

    field_label(cl, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, "Health");
    value_box(g, in, FieldId::WallHealth, min_x, row_y, box_w, box_h);
    text(max_x + 10.0f, row_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, "0 = inf", 1.5f, 150, 162, 178);
    row_y += pitch;

    draw_column_headers(min_x, max_x, box_w, row_y + 4.0f);
    row_y += 18.0f;

    row_range(g, in, cl, min_x, max_x, row_y, "Wall [m]", FieldId::WallDistMin, FieldId::WallDistMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "Radius [m]", FieldId::WallRadiusMin, FieldId::WallRadiusMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "H speed [m/s]", FieldId::WallHSpeedMin, FieldId::WallHSpeedMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "V speed [m/s]", FieldId::WallVSpeedMin, FieldId::WallVSpeedMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "Accel [m/s2]", FieldId::WallAccelMin, FieldId::WallAccelMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "Dir change [s]", FieldId::WallDirMin, FieldId::WallDirMax, box_w, box_h);

    draw_footer_hint(x);
}

static Playlist* selected_playlist_ptr(Game& g) {
    if (g.selected_playlist < 0 || g.selected_playlist >= static_cast<int>(g.playlists.size())) {
        return nullptr;
    }
    return &g.playlists[g.selected_playlist];
}

static void append_playlist_task(Game& g, const std::string& task_name) {
    Playlist* playlist = selected_playlist_ptr(g);
    if (!playlist) {
        return;
    }
    playlist->task_names.push_back(task_name);
    g.selected_playlist_entry = static_cast<int>(playlist->task_names.size()) - 1;
    reveal_selected_row(g.selected_playlist_entry, static_cast<int>(playlist->task_names.size()), g.playlist_entry_scroll, VISIBLE_PLAYLIST_ENTRY_ROWS);
}

static void draw_playlist_sidebar(Game& g, const Input& in, float x) {
    draw_card(x, CARD_Y, SIDEBAR_W, CARD_H);

    float row_x = x + 12.0f;
    float row_w = SIDEBAR_W - 24.0f;
    value_box(g, in, FieldId::PlaylistSearch, row_x, CARD_Y + 10.0f, row_w, ROW_H);

    std::vector<int> matches = matching_playlists(g);
    int count = static_cast<int>(matches.size());
    int& scroll = g.playlist_scroll;
    int& selected = g.selected_playlist;
    int max_scroll = std::max(0, count - VISIBLE_PLAYLIST_ROWS);
    bool over_list = hit(in, row_x, CARD_Y + 42.0f, row_w, static_cast<float>(VISIBLE_PLAYLIST_ROWS) * LIST_PITCH);
    if (over_list && in.wheel_y != 0) {
        scroll = std::max(0, std::min(scroll - in.wheel_y, max_scroll));
    }
    scroll = std::max(0, std::min(scroll, max_scroll));

    float list_y = CARD_Y + 42.0f;
    if (count == 0) {
        text(row_x + 4.0f, list_y + 8.0f, g.playlists.empty() ? "No playlists" : "No matching playlists", 1.6f, 120, 130, 145);
    }
    for (int row = 0; row < VISIBLE_PLAYLIST_ROWS; ++row) {
        int match_index = scroll + row;
        if (match_index >= count) {
            break;
        }
        int index = matches[match_index];
        const std::string& name = g.playlists[index].name;
        bool is_selected = index == selected;
        float y = list_y + static_cast<float>(row) * LIST_PITCH;
        bool clicked = list_button(in, row_x, y, row_w, LIST_ROW_H, name, is_selected);
        if (is_selected) {
            rect(row_x, y, 4.0f, LIST_ROW_H, 255, 70, 85);
        }
        if (clicked) {
            menu_blur_field(g);
            if (is_selected) {
                start_playlist(g);
            } else {
                selected = index;
                apply_selected_playlist(g);
            }
        }
    }

    float button_y = CARD_Y + CARD_H - 96.0f;
    if (secondary_button(in, row_x, button_y, 118.0f, BTN_H, "New", BTN_SCALE)) {
        menu_blur_field(g);
        new_playlist(g);
    }
    if (secondary_button(in, row_x + 128.0f, button_y, row_w - 128.0f, BTN_H, "Delete", BTN_SCALE)) {
        menu_blur_field(g);
        delete_playlist(g);
    }
    if (secondary_button(in, row_x, button_y + 32.0f, row_w, BTN_H, "Save playlist", BTN_SCALE)) {
        menu_blur_field(g);
        if (!g.playlists.empty() || !g.playlist_name.empty()) {
            save_current_playlist(g);
            save_settings(g);
        }
    }
}

static void draw_playlists_tab(Game& g, const Input& in, float left) {
    draw_playlist_sidebar(g, in, left);

    float x = left + EDITOR_DX;
    draw_card(x, CARD_Y, EDITOR_W, CARD_H);
    float cl = x + 14.0f;
    text(cl, CARD_Y + 10.0f, "Playlist", 2.3f, 230, 236, 244);

    float row_y = CARD_Y + 38.0f;
    field_label(cl, row_y + (ROW_H - text_height(VALUE_SCALE)) * 0.5f, "Name");
    float name_box_x = cl + 58.0f;
    float name_box_w = 252.0f;
    value_box(g, in, FieldId::PlaylistName, name_box_x, row_y, name_box_w, ROW_H);

    float buttons_x = name_box_x + name_box_w + 12.0f;
    float buttons_right = x + EDITOR_W - 14.0f;
    float gap = 8.0f;
    float bw = (buttons_right - buttons_x - gap) * 0.5f;
    bool can_play = playlist_has_playable_tasks(g);
    if (primary_button(in, buttons_x, row_y, bw, ROW_H, "Play", BTN_SCALE, can_play)) {
        menu_blur_field(g);
        start_playlist(g, 0);
    }
    bool can_resume = playlist_can_resume(g);
    if (primary_button(in, buttons_x + bw + gap, row_y, bw, ROW_H, "Resume", BTN_SCALE, can_resume)) {
        menu_blur_field(g);
        resume_playlist(g);
    }
    divider(cl, CARD_Y + 72.0f, EDITOR_W - 28.0f);

    if (g.playlists.empty()) {
        text(cl, CARD_Y + 96.0f, "Create a playlist with New", 1.8f, 150, 162, 178);
        draw_footer_hint(x);
        return;
    }

    Playlist* playlist = selected_playlist_ptr(g);
    float col_gap = 12.0f;
    float inner_w = EDITOR_W - 28.0f;
    float col_w = (inner_w - col_gap) * 0.5f;
    float add_x = cl;
    float list_x = cl + col_w + col_gap;
    float cols_y = CARD_Y + 84.0f;
    text(add_x, cols_y, "Add tasks", 1.8f, 230, 236, 244);
    text(list_x, cols_y, "In playlist", 1.8f, 230, 236, 244);

    float search_y = cols_y + 22.0f;
    value_box(g, in, FieldId::PlaylistAddSearch, add_x, search_y, col_w, ROW_H);

    std::vector<int> add_matches = matching_playlist_add_tasks(g);
    int add_count = static_cast<int>(add_matches.size());
    int add_max_scroll = std::max(0, add_count - VISIBLE_PLAYLIST_ADD_ROWS);
    float add_list_y = search_y + ROW_H + 6.0f;
    bool over_add = hit(in, add_x, add_list_y, col_w, static_cast<float>(VISIBLE_PLAYLIST_ADD_ROWS) * LIST_PITCH);
    if (over_add && in.wheel_y != 0) {
        g.playlist_add_scroll = std::max(0, std::min(g.playlist_add_scroll - in.wheel_y, add_max_scroll));
    }
    g.playlist_add_scroll = std::max(0, std::min(g.playlist_add_scroll, add_max_scroll));
    if (add_count == 0) {
        text(add_x + 4.0f, add_list_y + 8.0f, "No matching tasks", 1.6f, 120, 130, 145);
    }
    for (int row = 0; row < VISIBLE_PLAYLIST_ADD_ROWS; ++row) {
        int match_index = g.playlist_add_scroll + row;
        if (match_index >= add_count) {
            break;
        }
        int index = add_matches[match_index];
        const std::string& name = g.wall_presets[index].name;
        float y = add_list_y + static_cast<float>(row) * LIST_PITCH;
        bool clicked = list_button(in, add_x, y, col_w, LIST_ROW_H, name, false);
        if (clicked) {
            menu_blur_field(g);
            append_playlist_task(g, name);
        }
    }

    int entry_count = playlist ? static_cast<int>(playlist->task_names.size()) : 0;
    int entry_max_scroll = std::max(0, entry_count - VISIBLE_PLAYLIST_ENTRY_ROWS);
    float entry_list_y = search_y;
    bool over_entries = hit(in, list_x, entry_list_y, col_w, static_cast<float>(VISIBLE_PLAYLIST_ENTRY_ROWS) * LIST_PITCH);
    if (over_entries && in.wheel_y != 0) {
        g.playlist_entry_scroll = std::max(0, std::min(g.playlist_entry_scroll - in.wheel_y, entry_max_scroll));
    }
    g.playlist_entry_scroll = std::max(0, std::min(g.playlist_entry_scroll, entry_max_scroll));
    if (entry_count == 0) {
        text(list_x + 4.0f, entry_list_y + 8.0f, "Add tasks from the list", 1.6f, 120, 130, 145);
    }
    for (int row = 0; row < VISIBLE_PLAYLIST_ENTRY_ROWS; ++row) {
        int index = g.playlist_entry_scroll + row;
        if (index >= entry_count) {
            break;
        }
        char label[48];
        std::snprintf(label, sizeof(label), "%d. %s", index + 1, playlist->task_names[index].c_str());
        bool is_selected = index == g.selected_playlist_entry;
        float y = entry_list_y + static_cast<float>(row) * LIST_PITCH;
        bool clicked = list_button(in, list_x, y, col_w, LIST_ROW_H, label, is_selected);
        if (is_selected) {
            rect(list_x, y, 4.0f, LIST_ROW_H, 255, 70, 85);
        }
        if (clicked) {
            menu_blur_field(g);
            if (is_selected) {
                start_playlist(g, index);
            } else {
                g.selected_playlist_entry = index;
            }
        }
    }

    float button_y = CARD_Y + CARD_H - 64.0f;
    float btn_w = (col_w - 16.0f) / 3.0f;
    if (secondary_button(in, list_x, button_y, btn_w, BTN_H, "Up", BTN_SCALE)) {
        menu_blur_field(g);
        if (playlist && g.selected_playlist_entry > 0 && g.selected_playlist_entry < entry_count) {
            std::swap(playlist->task_names[g.selected_playlist_entry - 1], playlist->task_names[g.selected_playlist_entry]);
            g.selected_playlist_entry -= 1;
            reveal_selected_row(g.selected_playlist_entry, entry_count, g.playlist_entry_scroll, VISIBLE_PLAYLIST_ENTRY_ROWS);
        }
    }
    if (secondary_button(in, list_x + btn_w + 8.0f, button_y, btn_w, BTN_H, "Down", BTN_SCALE)) {
        menu_blur_field(g);
        if (playlist && g.selected_playlist_entry >= 0 && g.selected_playlist_entry + 1 < entry_count) {
            std::swap(playlist->task_names[g.selected_playlist_entry], playlist->task_names[g.selected_playlist_entry + 1]);
            g.selected_playlist_entry += 1;
            reveal_selected_row(g.selected_playlist_entry, entry_count, g.playlist_entry_scroll, VISIBLE_PLAYLIST_ENTRY_ROWS);
        }
    }
    if (secondary_button(in, list_x + 2.0f * (btn_w + 8.0f), button_y, btn_w, BTN_H, "Remove", BTN_SCALE)) {
        menu_blur_field(g);
        if (playlist && g.selected_playlist_entry >= 0 && g.selected_playlist_entry < entry_count) {
            playlist->task_names.erase(playlist->task_names.begin() + g.selected_playlist_entry);
            int next_count = static_cast<int>(playlist->task_names.size());
            g.selected_playlist_entry = next_count > 0 ? std::min(g.selected_playlist_entry, next_count - 1) : 0;
            reveal_selected_row(g.selected_playlist_entry, next_count, g.playlist_entry_scroll, VISIBLE_PLAYLIST_ENTRY_ROWS);
        }
    }

    draw_footer_hint(x);
}

static void draw_general_tab(Game& g, const Input& in, float left) {
    float w = SIDEBAR_W + (EDITOR_DX - SIDEBAR_W) + EDITOR_W;
    draw_card(left, CARD_Y, w, CARD_H);
    float cl = left + 14.0f;
    text(cl, CARD_Y + 12.0f, "Settings", 2.3f, 230, 236, 244);
    if (primary_button(in, left + w - 220.0f, CARD_Y + 10.0f, 206.0f, 32.0f, "Save settings", 1.85f)) {
        menu_blur_field(g);
        save_settings(g);
    }

    float value_x = cl + 240.0f;
    float box_h = 28.0f;

    float sens_y = CARD_Y + 52.0f;
    field_label(cl, sens_y + (box_h - text_height(VALUE_SCALE)) * 0.5f, "Sensitivity");
    value_box(g, in, FieldId::GenSens, value_x, sens_y, 120.0f, box_h);

    divider(cl, CARD_Y + 92.0f, w - 28.0f);
    text(cl, CARD_Y + 104.0f, "Crosshair", 2.1f, 230, 236, 244);

    float row_y = CARD_Y + 136.0f;
    const float pitch = 34.0f;
    row_single(g, in, cl, value_x, row_y, "Length [px]", FieldId::GenLength, 120.0f, box_h); row_y += pitch;
    row_single(g, in, cl, value_x, row_y, "Gap [px]", FieldId::GenGap, 120.0f, box_h); row_y += pitch;
    row_single(g, in, cl, value_x, row_y, "Thickness [px]", FieldId::GenThick, 120.0f, box_h);

    divider(cl, CARD_Y + 244.0f, w - 28.0f);
    text(cl, CARD_Y + 256.0f, "Colors", 2.1f, 230, 236, 244);

    float color_y = CARD_Y + 292.0f;
    text(value_x + 32.0f - text_width("R", 1.4f) * 0.5f, color_y - 16.0f, "R", 1.4f, 150, 162, 178);
    text(value_x + 120.0f - text_width("G", 1.4f) * 0.5f, color_y - 16.0f, "G", 1.4f, 150, 162, 178);
    text(value_x + 208.0f - text_width("B", 1.4f) * 0.5f, color_y - 16.0f, "B", 1.4f, 150, 162, 178);
    color_row(g, in, cl, value_x, color_y, "Target", FieldId::GenTargetR, FieldId::GenTargetG, FieldId::GenTargetB, g.target_color.r, g.target_color.g, g.target_color.b);
    color_row(g, in, cl, value_x, color_y + 36.0f, "Wall", FieldId::GenWallR, FieldId::GenWallG, FieldId::GenWallB, g.wall_color.r, g.wall_color.g, g.wall_color.b);
}

// ---------------------------------------------------------------------------
// Top-level menu.
// ---------------------------------------------------------------------------

void draw_menu(Game& game, const Input& input, int w, int h) {
    ensure_presets(game);
    menu_handle_edit(game, input);
    normalize_settings(game);

    glClearColor(16.0f / 255.0f, 18.0f / 255.0f, 22.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_2d(w, h);
    float ui_scale = ui_scale_for_height(h);
    float ui_w = static_cast<float>(w) / ui_scale;
    float ui_h = static_cast<float>(h) / ui_scale;
    float menu_scale = std::min(1.0f, std::min((ui_w - 84.0f) / 1040.0f, (ui_h - 42.0f) / 840.0f));
    menu_scale = std::max(0.25f, menu_scale);
    float base_w = ui_w / menu_scale;
    float base_h = ui_h / menu_scale;
    // Vertically center the title-to-card block in the available height.
    float content_mid = (32.0f + (CARD_Y + CARD_H)) * 0.5f;
    float voff = std::max(0.0f, base_h * 0.5f - content_mid);

    Input ui_input = input;
    ui_input.mouse_x = static_cast<int>(std::floor(static_cast<float>(input.mouse_x) / ui_scale / menu_scale));
    ui_input.mouse_y = static_cast<int>(std::floor(static_cast<float>(input.mouse_y) / ui_scale / menu_scale - voff));

    glPushMatrix();
    glScalef(menu_scale, menu_scale, 1.0f);
    glTranslatef(0.0f, voff, 0.0f);

    float left = std::max(42.0f, base_w * 0.5f - 520.0f);
    text(left, 32.0f, "Aim Trainer", 3.6f, 235, 240, 245);
    text(left, 68.0f, "FOV locked to 103  |  Match in-game sensitivity", 1.7f, 150, 162, 178);

    float tabs_y = 96.0f;
    tab_button(game, ui_input, left, tabs_y, "Tasks", MenuTab::Clicking);
    tab_button(game, ui_input, left + 168.0f, tabs_y, "Playlists", MenuTab::Playlists);
    tab_button(game, ui_input, left + 336.0f, tabs_y, "Settings", MenuTab::Settings);

    if (game.menu_tab == MenuTab::Clicking) {
        draw_tasks_tab(game, ui_input, left);
    } else if (game.menu_tab == MenuTab::Playlists) {
        draw_playlists_tab(game, ui_input, left);
    } else {
        draw_general_tab(game, ui_input, left);
    }

    glPopMatrix();
}

// ---------------------------------------------------------------------------
// Challenge results screen.
// ---------------------------------------------------------------------------

void draw_results(const Game& game, int w, int h) {
    glClearColor(16.0f / 255.0f, 18.0f / 255.0f, 22.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_2d(w, h);
    float ui_scale = ui_scale_for_height(h);
    float ui_w = static_cast<float>(w) / ui_scale;
    float ui_h = static_cast<float>(h) / ui_scale;
    float cx = ui_w * 0.5f;

    auto centered = [&](float y, const std::string& s, float scale, uint8_t cr, uint8_t cg, uint8_t cb) {
        text(cx - text_width(s, scale) * 0.5f, y, s, scale, cr, cg, cb);
    };

    if (game.playlist_active && game.playlist_complete) {
        int total = 0;
        for (const RunRecord& session_run : game.playlist_session_runs) {
            total += session_run.score;
        }
        int task_count = static_cast<int>(game.playlist_session_runs.size());
        int shown = std::min(task_count, 8);
        float card_w = 560.0f;
        float card_h = 280.0f + static_cast<float>(shown) * 28.0f;
        float card_x = cx - card_w * 0.5f;
        float card_y = ui_h * 0.5f - card_h * 0.5f;
        draw_card(card_x, card_y, card_w, card_h);

        char buf[96];
        float y = card_y + 30.0f;
        centered(y, "Playlist complete", 3.0f, 255, 70, 85); y += 46.0f;
        centered(y, game.playlist_play_name, 2.0f, 230, 236, 244); y += 40.0f;
        centered(y, "Total score", 2.0f, 150, 162, 178); y += 28.0f;
        std::snprintf(buf, sizeof(buf), "%d", total);
        centered(y, buf, 5.2f, 245, 248, 252); y += 62.0f;
        for (int i = 0; i < shown; ++i) {
            const RunRecord& session_run = game.playlist_session_runs[i];
            std::snprintf(buf, sizeof(buf), "%d. %s  %d", i + 1, session_run.preset_name.c_str(), session_run.score);
            centered(y, buf, 1.7f, 210, 220, 232);
            y += 28.0f;
        }
        if (task_count > shown) {
            std::snprintf(buf, sizeof(buf), "+%d more", task_count - shown);
            centered(y, buf, 1.6f, 150, 162, 178);
        }
        centered(card_y + card_h - 28.0f, "Click or Esc to continue", 1.8f, 150, 162, 178);
        return;
    }

    const RunRecord& run = game.last_run;
    int prev_best = -1;  // best for this scenario/preset before this run
    for (size_t i = 0; i + 1 < game.runs.size(); ++i) {
        const RunRecord& other = game.runs[i];
        if (other.kind == run.kind && other.preset_name == run.preset_name) {
            prev_best = std::max(prev_best, other.score);
        }
    }
    bool new_best = prev_best < 0 || run.score > prev_best;
    int best = std::max(prev_best, run.score);
    const char* scenario_title = run.kind == ScenarioKind::Tracking ? "Wall tracking" : "Wall clicking";

    float card_w = 560.0f;
    float card_h = game.playlist_active ? 460.0f : 430.0f;
    float card_x = cx - card_w * 0.5f;
    float card_y = ui_h * 0.5f - card_h * 0.5f;
    draw_card(card_x, card_y, card_w, card_h);

    char buf[64];
    float y = card_y + 30.0f;
    centered(y, "Challenge complete", 3.0f, 255, 70, 85); y += 50.0f;
    if (game.playlist_active) {
        std::snprintf(buf, sizeof(buf), "Task %d of %d", game.playlist_play_index + 1,
                      std::max(1, static_cast<int>(game.playlist_play_tasks.size())));
        centered(y, buf, 2.0f, 255, 200, 90); y += 30.0f;
    }
    centered(y, scenario_title, 2.0f, 230, 236, 244); y += 30.0f;
    centered(y, run.preset_name, 2.0f, 150, 162, 178); y += 56.0f;

    centered(y, "Score", 2.0f, 150, 162, 178); y += 30.0f;
    std::snprintf(buf, sizeof(buf), "%d", run.score);
    centered(y, buf, 6.0f, 245, 248, 252); y += 78.0f;

    std::snprintf(buf, sizeof(buf), "Accuracy %.1f%%   Shots %d", run.accuracy, run.shots);
    centered(y, buf, 2.0f, 210, 220, 232); y += 34.0f;

    if (new_best) {
        centered(y, "New best", 2.4f, 255, 200, 90);
    } else {
        std::snprintf(buf, sizeof(buf), "Best %d", best);
        centered(y, buf, 2.0f, 150, 162, 178);
    }
    y += 40.0f;

    char datebuf[40] = "";
    std::time_t t = static_cast<std::time_t>(run.timestamp);
    std::tm* local = std::localtime(&t);
    if (local) {
        std::strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M", local);
        centered(y, datebuf, 1.7f, 120, 130, 145);
    }

    const char* footer = game.playlist_active ? "Click to continue" : "Click or Esc to continue";
    centered(card_y + card_h - 28.0f, footer, 1.8f, 150, 162, 178);
}
