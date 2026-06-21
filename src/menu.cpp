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

#include "config.hpp"
#include "render.hpp"
#include "scenario.hpp"
#include "world.hpp"

// ---------------------------------------------------------------------------
// Field descriptors: map a FieldId to the value it edits and its limits.
// ---------------------------------------------------------------------------

struct FieldDesc {
    enum Kind { Name, IntVal, FloatVal };
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

static FieldDesc field_desc(Game& g, FieldId id) {
    int capacity = wall_capacity_for_radius(g.wall_settings.radius_max, g.wall_settings.wall_distance_max);
    switch (id) {
        case FieldId::WallName: return f_name(&g.wall_preset_name);
        case FieldId::WallDistMin: return f_float(&g.wall_settings.wall_distance_min, 2.0f, 30.0f, 2);
        case FieldId::WallDistMax: return f_float(&g.wall_settings.wall_distance_max, 2.0f, 30.0f, 2);
        case FieldId::WallTargetsMin: return f_int(&g.wall_settings.target_count_min, 1, capacity);
        case FieldId::WallTargetsMax: return f_int(&g.wall_settings.target_count_max, 1, capacity);
        case FieldId::WallRadiusMin: return f_float(&g.wall_settings.radius_min, 0.03f, 0.45f, 2);
        case FieldId::WallRadiusMax: return f_float(&g.wall_settings.radius_max, 0.03f, 0.45f, 2);
        case FieldId::WallHSpeedMin: return f_float(&g.wall_settings.horizontal_speed_min, 0.0f, 8.0f, 2);
        case FieldId::WallHSpeedMax: return f_float(&g.wall_settings.horizontal_speed_max, 0.0f, 8.0f, 2);
        case FieldId::WallVSpeedMin: return f_float(&g.wall_settings.vertical_speed_min, 0.0f, 8.0f, 2);
        case FieldId::WallVSpeedMax: return f_float(&g.wall_settings.vertical_speed_max, 0.0f, 8.0f, 2);
        case FieldId::WallAccelMin: return f_float(&g.wall_settings.acceleration_min, 0.0f, 40.0f, 2);
        case FieldId::WallAccelMax: return f_float(&g.wall_settings.acceleration_max, 0.0f, 40.0f, 2);
        case FieldId::WallDirMin: return f_float(&g.wall_settings.change_min, 0.0f, 12.0f, 2);
        case FieldId::WallDirMax: return f_float(&g.wall_settings.change_max, 0.0f, 12.0f, 2);
        case FieldId::PillName: return f_name(&g.pill_preset_name);
        case FieldId::PillWidth: return f_float(&g.pill_settings.width, 0.20f, 2.5f, 2);
        case FieldId::PillDistMin: return f_float(&g.pill_settings.distance_min, 1.5f, 30.0f, 2);
        case FieldId::PillDistMax: return f_float(&g.pill_settings.distance_max, 1.5f, 30.0f, 2);
        case FieldId::PillSpeed: return f_float(&g.pill_settings.speed, 0.0f, pill_speed_max_meters(), 2);
        case FieldId::PillAccel: return f_float(&g.pill_settings.acceleration, 0.0f, pill_acceleration_max_meters(), 2);
        case FieldId::PillDirMin: return f_float(&g.pill_settings.change_min, 0.05f, 8.0f, 2);
        case FieldId::PillDirMax: return f_float(&g.pill_settings.change_max, 0.05f, 12.0f, 2);
        case FieldId::GenSens: return f_float(&g.sensitivity, 0.001f, 10.0f, 3);
        case FieldId::GenLength: return f_float(&g.crosshair.length, 4.0f, 24.0f, 0);
        case FieldId::GenGap: return f_float(&g.crosshair.gap, 0.0f, 16.0f, 0);
        case FieldId::GenThick: return f_float(&g.crosshair.thickness, 1.0f, 6.0f, 0);
        default: return FieldDesc{};
    }
}

static std::string format_field(Game& g, FieldId id) {
    FieldDesc d = field_desc(g, id);
    char buffer[32];
    if (d.kind == FieldDesc::Name) {
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
    g.edit_fresh = field_desc(g, id).kind != FieldDesc::Name;  // numbers retype, names edit in place
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
    FieldId::WallName, FieldId::WallDistMin, FieldId::WallDistMax,
    FieldId::WallTargetsMin, FieldId::WallTargetsMax,
    FieldId::WallRadiusMin, FieldId::WallRadiusMax,
    FieldId::WallHSpeedMin, FieldId::WallHSpeedMax,
    FieldId::WallVSpeedMin, FieldId::WallVSpeedMax,
    FieldId::WallAccelMin, FieldId::WallAccelMax,
    FieldId::WallDirMin, FieldId::WallDirMax,
};
static const FieldId PILL_ORDER[] = {
    FieldId::PillName, FieldId::PillWidth,
    FieldId::PillDistMin, FieldId::PillDistMax,
    FieldId::PillSpeed, FieldId::PillAccel,
    FieldId::PillDirMin, FieldId::PillDirMax,
};
static const FieldId GEN_ORDER[] = {
    FieldId::GenSens, FieldId::GenLength, FieldId::GenGap, FieldId::GenThick,
};

static void tab_field_order(MenuTab tab, const FieldId** order, int* count) {
    switch (tab) {
        case MenuTab::Clicking: *order = WALL_ORDER; *count = static_cast<int>(sizeof(WALL_ORDER) / sizeof(WALL_ORDER[0])); break;
        case MenuTab::Tracking: *order = PILL_ORDER; *count = static_cast<int>(sizeof(PILL_ORDER) / sizeof(PILL_ORDER[0])); break;
        default: *order = GEN_ORDER; *count = static_cast<int>(sizeof(GEN_ORDER) / sizeof(GEN_ORDER[0])); break;
    }
}

static FieldId field_step(const Game& g, int dir) {
    const FieldId* order = nullptr;
    int count = 0;
    tab_field_order(g.menu_tab, &order, &count);
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
    for (char c : input.text_input) {
        if (d.kind == FieldDesc::Name) {
            if (is_allowed_preset_char(c) && g.edit_draft.size() < 22) {
                g.edit_draft.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
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

static const int VISIBLE_PRESET_ROWS = 7;

// Adjusts `scroll` so the selected row is within the visible window.
static void reveal_selected_preset(int selected, int count, int& scroll) {
    if (selected < scroll) {
        scroll = selected;
    } else if (selected > scroll + VISIBLE_PRESET_ROWS - 1) {
        scroll = selected - (VISIBLE_PRESET_ROWS - 1);
    }
    scroll = std::max(0, std::min(scroll, std::max(0, count - VISIBLE_PRESET_ROWS)));
}

void new_wall_preset(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "CLICK PRESET %d", static_cast<int>(game.wall_presets.size()) + 1);
    game.active_field = FieldId::None;
    std::string unique_name = unique_preset_name(game.wall_presets, name, -1);
    game.wall_presets.push_back({unique_name, game.wall_settings});
    game.selected_wall_preset = static_cast<int>(game.wall_presets.size()) - 1;
    game.wall_settings = game.wall_presets.back().settings;
    game.wall_preset_name = game.wall_presets.back().name;
    reveal_selected_preset(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()), game.wall_preset_scroll);
}

void new_pill_preset(Game& game) {
    char name[32];
    std::snprintf(name, sizeof(name), "TRACK PRESET %d", static_cast<int>(game.pill_presets.size()) + 1);
    game.active_field = FieldId::None;
    std::string unique_name = unique_preset_name(game.pill_presets, name, -1);
    game.pill_presets.push_back({unique_name, game.pill_settings});
    game.selected_pill_preset = static_cast<int>(game.pill_presets.size()) - 1;
    game.pill_settings = game.pill_presets.back().settings;
    game.pill_preset_name = game.pill_presets.back().name;
    reveal_selected_preset(game.selected_pill_preset, static_cast<int>(game.pill_presets.size()), game.pill_preset_scroll);
}

void delete_wall_preset(Game& game) {
    game.active_field = FieldId::None;
    if (game.wall_presets.size() <= 1) {
        game.wall_presets[0] = {"PASU FIVE", WallClickSettings{}};
        game.selected_wall_preset = 0;
    } else {
        game.wall_presets.erase(game.wall_presets.begin() + game.selected_wall_preset);
        game.selected_wall_preset = std::max(0, std::min(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()) - 1));
    }
    apply_selected_presets(game);
    reveal_selected_preset(game.selected_wall_preset, static_cast<int>(game.wall_presets.size()), game.wall_preset_scroll);
}

void delete_pill_preset(Game& game) {
    game.active_field = FieldId::None;
    if (game.pill_presets.size() <= 1) {
        game.pill_presets[0] = {"SMOOTH PILL", PillTrackingSettings{}};
        game.selected_pill_preset = 0;
    } else {
        game.pill_presets.erase(game.pill_presets.begin() + game.selected_pill_preset);
        game.selected_pill_preset = std::max(0, std::min(game.selected_pill_preset, static_cast<int>(game.pill_presets.size()) - 1));
    }
    apply_selected_presets(game);
    reveal_selected_preset(game.selected_pill_preset, static_cast<int>(game.pill_presets.size()), game.pill_preset_scroll);
}

// ---------------------------------------------------------------------------
// Widgets.
// ---------------------------------------------------------------------------

static const float VALUE_SCALE = 2.0f;
static const float GLYPH_H = 7.0f * VALUE_SCALE;  // height of value/label text

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

// Label like "RADIUS [M]" drawn with a bright name and a dimmer unit tag.
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
    text(x + std::max(8.0f, (w - lw) * 0.5f), y + std::max(6.0f, (h - 7.0f * scale) * 0.5f), label, scale, 225, 232, 240);
    return hovered && in.left_pressed;
}

static bool primary_button(const Input& in, float x, float y, float w, float h, const std::string& label, float scale) {
    bool hovered = hit(in, x, y, w, h);
    rect(x + 3.0f, y + 3.0f, w, h, 0, 0, 0, 90);  // shadow
    if (hovered) rect(x, y, w, h, 255, 95, 108);
    else rect(x, y, w, h, 255, 70, 85);
    rect(x, y, w, 2.0f, 255, 120, 132);            // top highlight
    float lw = text_width(label, scale);
    text(x + std::max(8.0f, (w - lw) * 0.5f), y + std::max(6.0f, (h - 7.0f * scale) * 0.5f), label, scale, 248, 248, 248);
    return hovered && in.left_pressed;
}

static void tab_button(Game& g, const Input& in, float x, float y, const std::string& label, MenuTab tab) {
    const float w = 190.0f, h = 42.0f;
    bool selected = g.menu_tab == tab;
    bool hovered = hit(in, x, y, w, h);
    rect(x, y, w, h,
         selected ? 46 : (hovered ? 40 : 28),
         selected ? 52 : (hovered ? 46 : 33),
         selected ? 62 : (hovered ? 56 : 41));
    float scale = 2.35f;
    float lw = text_width(label, scale);
    text(x + (w - lw) * 0.5f, y + (h - 7.0f * scale) * 0.5f, label, scale,
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
    float ty = y + (h - GLYPH_H) * 0.5f;
    if (d.kind == FieldDesc::Name) {
        text_fit(x + 12.0f, ty, shown, VALUE_SCALE, w - 22.0f, 240, 244, 248);
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
    field_label(label_x, row_y + (box_h - GLYPH_H) * 0.5f, label);
    value_box(g, in, id, box_x, row_y, box_w, box_h);
}

static void row_range(Game& g, const Input& in, float label_x, float min_x, float max_x, float row_y, const std::string& label, FieldId min_id, FieldId max_id, float box_w, float box_h) {
    field_label(label_x, row_y + (box_h - GLYPH_H) * 0.5f, label);
    value_box(g, in, min_id, min_x, row_y, box_w, box_h);
    value_box(g, in, max_id, max_x, row_y, box_w, box_h);
}

// ---------------------------------------------------------------------------
// Tab content.
// ---------------------------------------------------------------------------

static const float CARD_Y = 234.0f;
static const float CARD_H = 446.0f;
static const float SIDEBAR_W = 290.0f;
static const float EDITOR_DX = 306.0f;
static const float EDITOR_W = 662.0f;

static void draw_preset_sidebar(Game& g, const Input& in, float x, bool tracking) {
    draw_card(x, CARD_Y, SIDEBAR_W, CARD_H);
    text(x + 16.0f, CARD_Y + 14.0f, "PRESETS", 2.0f, 150, 162, 178);

    int count = tracking ? static_cast<int>(g.pill_presets.size()) : static_cast<int>(g.wall_presets.size());
    int& scroll = tracking ? g.pill_preset_scroll : g.wall_preset_scroll;
    int& selected = tracking ? g.selected_pill_preset : g.selected_wall_preset;
    int max_scroll = std::max(0, count - VISIBLE_PRESET_ROWS);
    if (in.wheel_y != 0) {
        scroll = std::max(0, std::min(scroll - in.wheel_y, max_scroll));
    }
    scroll = std::max(0, std::min(scroll, max_scroll));

    float row_x = x + 16.0f;
    float row_w = SIDEBAR_W - 32.0f;
    float list_y = CARD_Y + 48.0f;
    for (int row = 0; row < VISIBLE_PRESET_ROWS; ++row) {
        int index = scroll + row;
        if (index >= count) {
            break;
        }
        const std::string& name = tracking ? g.pill_presets[index].name : g.wall_presets[index].name;
        bool is_selected = index == selected;
        bool clicked = list_button(in, row_x, list_y + row * 40.0f, row_w, 34.0f, name, is_selected);
        if (is_selected) {
            rect(row_x, list_y + row * 40.0f, 4.0f, 34.0f, 255, 70, 85);  // accent bar
        }
        if (clicked) {
            menu_blur_field(g);
            if (is_selected) {
                // Clicking the already-selected scenario starts a challenge run.
                start_scenario(g, g.scenarios[tracking ? 1 : 0], RunMode::Challenge);
            } else {
                selected = index;
                if (tracking) {
                    g.pill_settings = g.pill_presets[index].settings;
                    g.pill_preset_name = g.pill_presets[index].name;
                } else {
                    g.wall_settings = g.wall_presets[index].settings;
                    g.wall_preset_name = g.wall_presets[index].name;
                }
            }
        }
    }

    float button_y = CARD_Y + CARD_H - 90.0f;
    if (secondary_button(in, row_x, button_y, 120.0f, 34.0f, "NEW", 2.0f)) {
        menu_blur_field(g);
        if (tracking) new_pill_preset(g);
        else new_wall_preset(g);
    }
    if (secondary_button(in, row_x + 132.0f, button_y, row_w - 132.0f, 34.0f, "DELETE", 2.0f)) {
        menu_blur_field(g);
        if (tracking) delete_pill_preset(g);
        else delete_wall_preset(g);
    }
    if (secondary_button(in, row_x, button_y + 42.0f, row_w, 34.0f, "SAVE PRESET", 2.0f)) {
        menu_blur_field(g);
        if (tracking) save_current_pill_preset(g);
        else save_current_wall_preset(g);
        save_settings(g);
    }
}

// Shared editor card chrome: title, BEST readout, NAME box, PRACTICE/CHALLENGE.
static void draw_editor_header(Game& g, const Input& in, float x, const std::string& title, FieldId name_id, ScenarioKind kind, int scenario_index) {
    draw_card(x, CARD_Y, EDITOR_W, CARD_H);
    float cl = x + 16.0f;
    text(cl, CARD_Y + 12.0f, title, 2.8f, 230, 236, 244);

    // Best challenge score for the selected preset, right-aligned on the title row.
    const std::string& preset = kind == ScenarioKind::WallClick ? g.wall_preset_name : g.pill_preset_name;
    int best = best_run_score(g, kind, preset);
    if (best >= 0) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "BEST %d", best);
        std::string best_text = buf;
        text(x + EDITOR_W - 16.0f - text_width(best_text, 2.0f), CARD_Y + 16.0f, best_text, 2.0f, 255, 200, 90);
    }

    float row_y = CARD_Y + 46.0f;
    field_label(cl, row_y + 11.0f, "NAME");
    float name_box_x = cl + 78.0f;
    float name_box_w = 248.0f;
    value_box(g, in, name_id, name_box_x, row_y, name_box_w, 36.0f);

    float buttons_x = name_box_x + name_box_w + 16.0f;
    float buttons_right = x + EDITOR_W - 16.0f;
    float gap = 10.0f;
    float bw = (buttons_right - buttons_x - gap) * 0.5f;
    if (secondary_button(in, buttons_x, row_y, bw, 36.0f, "PRACTICE", 2.0f)) {
        menu_blur_field(g);
        start_scenario(g, g.scenarios[scenario_index], RunMode::Practice);
    }
    if (primary_button(in, buttons_x + bw + gap, row_y, bw, 36.0f, "CHALLENGE", 2.0f)) {
        menu_blur_field(g);
        start_scenario(g, g.scenarios[scenario_index], RunMode::Challenge);
    }
    divider(cl, CARD_Y + 94.0f, EDITOR_W - 32.0f);
}

static void draw_column_headers(float min_x, float max_x, float box_w, bool show_max) {
    float y = CARD_Y + 104.0f;
    // Right-align headers to the same edge as the right-aligned box values.
    std::string mn = "MIN";
    text(min_x + box_w - 12.0f - text_width(mn, 1.5f), y, mn, 1.5f, 150, 162, 178);
    if (show_max) {
        std::string mx = "MAX";
        text(max_x + box_w - 12.0f - text_width(mx, 1.5f), y, mx, 1.5f, 150, 162, 178);
    }
}

static void draw_footer_hint(float x) {
    divider(x + 16.0f, CARD_Y + CARD_H - 42.0f, EDITOR_W - 32.0f);
    text(x + 16.0f, CARD_Y + CARD_H - 30.0f, "TAB NEXT BOX   ENTER COMMITS   ESC CANCELS", 1.7f, 150, 162, 178);
}

static void draw_clicking_tab(Game& g, const Input& in, float left) {
    draw_preset_sidebar(g, in, left, false);

    float x = left + EDITOR_DX;
    draw_editor_header(g, in, x, "WALL CLICKING", FieldId::WallName, ScenarioKind::WallClick, 0);

    float cl = x + 16.0f;
    float min_x = cl + 272.0f;
    float max_x = cl + 408.0f;
    float box_w = 116.0f;
    float box_h = 30.0f;
    draw_column_headers(min_x, max_x, box_w, true);

    float row_y = CARD_Y + 126.0f;
    const float pitch = 38.0f;
    row_range(g, in, cl, min_x, max_x, row_y, "WALL [M]", FieldId::WallDistMin, FieldId::WallDistMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "TARGETS", FieldId::WallTargetsMin, FieldId::WallTargetsMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "RADIUS [M]", FieldId::WallRadiusMin, FieldId::WallRadiusMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "H SPEED [M/S]", FieldId::WallHSpeedMin, FieldId::WallHSpeedMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "V SPEED [M/S]", FieldId::WallVSpeedMin, FieldId::WallVSpeedMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "ACCEL [M/S2]", FieldId::WallAccelMin, FieldId::WallAccelMax, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "DIR CHANGE [SEC]", FieldId::WallDirMin, FieldId::WallDirMax, box_w, box_h);

    draw_footer_hint(x);
}

static void draw_tracking_tab(Game& g, const Input& in, float left) {
    draw_preset_sidebar(g, in, left, true);

    float x = left + EDITOR_DX;
    draw_editor_header(g, in, x, "PILL TRACKING", FieldId::PillName, ScenarioKind::PillTracking, 1);

    float cl = x + 16.0f;
    float min_x = cl + 272.0f;
    float max_x = cl + 408.0f;
    float box_w = 116.0f;
    float box_h = 30.0f;
    draw_column_headers(min_x, max_x, box_w, true);

    float row_y = CARD_Y + 126.0f;
    const float pitch = 38.0f;
    row_single(g, in, cl, min_x, row_y, "PILL WIDTH [M]", FieldId::PillWidth, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "DIST [M]", FieldId::PillDistMin, FieldId::PillDistMax, box_w, box_h); row_y += pitch;
    row_single(g, in, cl, min_x, row_y, "SPEED [M/S]", FieldId::PillSpeed, box_w, box_h); row_y += pitch;
    row_single(g, in, cl, min_x, row_y, "ACCEL [M/S2]", FieldId::PillAccel, box_w, box_h); row_y += pitch;
    row_range(g, in, cl, min_x, max_x, row_y, "DIR CHANGE [SEC]", FieldId::PillDirMin, FieldId::PillDirMax, box_w, box_h);

    draw_footer_hint(x);
}

static void draw_general_tab(Game& g, const Input& in, float left) {
    float w = SIDEBAR_W + (EDITOR_DX - SIDEBAR_W) + EDITOR_W;  // span both columns
    draw_card(left, CARD_Y, w, CARD_H);
    float cl = left + 16.0f;
    text(cl, CARD_Y + 14.0f, "GENERAL", 2.8f, 230, 236, 244);

    float value_x = cl + 280.0f;
    float box_h = 34.0f;

    float sens_y = CARD_Y + 64.0f;
    field_label(cl, sens_y + (box_h - GLYPH_H) * 0.5f, "SENSITIVITY");
    value_box(g, in, FieldId::GenSens, value_x, sens_y, 130.0f, box_h);

    divider(cl, CARD_Y + 112.0f, w - 32.0f);
    text(cl, CARD_Y + 128.0f, "CROSSHAIR", 2.6f, 230, 236, 244);

    float row_y = CARD_Y + 176.0f;
    const float pitch = 52.0f;
    row_single(g, in, cl, value_x, row_y, "LENGTH [PX]", FieldId::GenLength, 130.0f, box_h); row_y += pitch;
    row_single(g, in, cl, value_x, row_y, "GAP [PX]", FieldId::GenGap, 130.0f, box_h); row_y += pitch;
    row_single(g, in, cl, value_x, row_y, "THICKNESS [PX]", FieldId::GenThick, 130.0f, box_h);

    divider(cl, CARD_Y + 344.0f, w - 32.0f);
    if (primary_button(in, cl, CARD_Y + 360.0f, 260.0f, 44.0f, "SAVE GENERAL", 2.35f)) {
        menu_blur_field(g);
        save_settings(g);
    }
    text(cl + 280.0f, CARD_Y + 372.0f, "STORES SENSITIVITY AND CROSSHAIR", 1.7f, 150, 162, 178);
    text(cl, CARD_Y + CARD_H - 30.0f, "TAB NEXT BOX   ENTER COMMITS   ESC CANCELS", 1.7f, 150, 162, 178);
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
    float menu_scale = std::min(1.0f, std::min((ui_w - 84.0f) / 1040.0f, (ui_h - 42.0f) / 720.0f));
    menu_scale = std::max(0.25f, menu_scale);
    float base_w = ui_w / menu_scale;
    float base_h = ui_h / menu_scale;
    // Vertically center the title-to-card block in the available height.
    float content_mid = (58.0f + (CARD_Y + CARD_H)) * 0.5f;
    float voff = std::max(0.0f, base_h * 0.5f - content_mid);

    Input ui_input = input;
    ui_input.mouse_x = static_cast<int>(std::floor(static_cast<float>(input.mouse_x) / ui_scale / menu_scale));
    ui_input.mouse_y = static_cast<int>(std::floor(static_cast<float>(input.mouse_y) / ui_scale / menu_scale - voff));

    glPushMatrix();
    glScalef(menu_scale, menu_scale, 1.0f);
    glTranslatef(0.0f, voff, 0.0f);

    float left = std::max(42.0f, base_w * 0.5f - 520.0f);
    text(left, 58.0f, "AIM TRAINER", 4.7f, 235, 240, 245);
    text(left, 110.0f, "FOV LOCKED TO 103 HORIZONTAL", 2.0f, 150, 162, 178);
    text(left, 134.0f, "MATCH YOUR IN-GAME SENSITIVITY", 2.0f, 150, 162, 178);

    float tabs_y = 170.0f;
    tab_button(game, ui_input, left, tabs_y, "CLICKING", MenuTab::Clicking);
    tab_button(game, ui_input, left + 210.0f, tabs_y, "TRACKING", MenuTab::Tracking);
    tab_button(game, ui_input, left + 420.0f, tabs_y, "GENERAL", MenuTab::General);

    if (game.menu_tab == MenuTab::Clicking) {
        draw_clicking_tab(game, ui_input, left);
    } else if (game.menu_tab == MenuTab::Tracking) {
        draw_tracking_tab(game, ui_input, left);
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
    const char* scenario_title = run.kind == ScenarioKind::WallClick ? "WALL CLICKING" : "360 PILL TRACKING";

    float card_w = 560.0f;
    float card_h = 430.0f;
    float card_x = cx - card_w * 0.5f;
    float card_y = ui_h * 0.5f - card_h * 0.5f;
    draw_card(card_x, card_y, card_w, card_h);

    auto centered = [&](float y, const std::string& s, float scale, uint8_t cr, uint8_t cg, uint8_t cb) {
        text(cx - text_width(s, scale) * 0.5f, y, s, scale, cr, cg, cb);
    };

    char buf[64];
    float y = card_y + 30.0f;
    centered(y, "CHALLENGE COMPLETE", 3.0f, 255, 70, 85); y += 50.0f;
    centered(y, scenario_title, 2.0f, 230, 236, 244); y += 30.0f;
    centered(y, run.preset_name, 2.0f, 150, 162, 178); y += 56.0f;

    centered(y, "SCORE", 2.0f, 150, 162, 178); y += 30.0f;
    std::snprintf(buf, sizeof(buf), "%d", run.score);
    centered(y, buf, 6.0f, 245, 248, 252); y += 78.0f;

    std::snprintf(buf, sizeof(buf), "ACCURACY %.1f%%   SHOTS %d", run.accuracy, run.shots);
    centered(y, buf, 2.0f, 210, 220, 232); y += 34.0f;

    if (new_best) {
        centered(y, "NEW BEST", 2.4f, 255, 200, 90);
    } else {
        std::snprintf(buf, sizeof(buf), "BEST %d", best);
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

    centered(card_y + card_h - 28.0f, "CLICK OR ESC TO CONTINUE", 1.8f, 150, 162, 178);
}
