#include "selftest.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "math.hpp"
#include "menu.hpp"
#include "render.hpp"
#include "scenario.hpp"
#include "types.hpp"
#include "world.hpp"

static bool self_test_check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "SELF TEST FAILED: %s\n", message);
        return false;
    }
    return true;
}

int run_self_test() {
    bool ok = true;
    g_live_default_tasks = false;

    ok = self_test_check(text_width("[", 2.0f) > 1.0f && text_width("]", 2.0f) > 1.0f, "menu font measures unit brackets") && ok;
    ok = self_test_check(text_width("Aim", 2.0f) > 1.0f && text_width("aim", 2.0f) > 1.0f, "menu font measures mixed-case text") && ok;
    ok = self_test_check(std::fabs(text_width("i", 2.0f) - text_width("M", 2.0f)) < 0.5f, "menu font is monospace") && ok;
    {
        std::string long_text(40, 'W');
        const float scale = 2.0f;
        const float max_width = 50.0f;
        ok = self_test_check(text_width(long_text, 1.0f) > max_width, "long label is wider than the fit box at scale 1") && ok;
        float fitted = text_fit_scale(long_text, scale, max_width);
        ok = self_test_check(fitted < 1.0f, "text_fit shrinks below scale 1 when needed") && ok;
        ok = self_test_check(text_width(long_text, fitted) <= max_width + 0.05f, "fitted text width stays within max_width") && ok;
        ok = self_test_check(std::fabs(text_fit_scale("OK", scale, 400.0f) - scale) < 0.0001f, "text_fit keeps the requested scale when the text already fits") && ok;
    }

    Game game;
    ensure_presets(game);
    apply_selected_presets(game);

    auto find_wall_preset = [](const Game& g, const std::string& name) {
        for (int i = 0; i < static_cast<int>(g.wall_presets.size()); ++i) {
            if (g.wall_presets[i].name == name) {
                return i;
            }
        }
        return -1;
    };

    const char* default_wall_order[] = {
        "1W2T DYNAMIC",
        "1W2TS DYNAMIC",
        "1W2T STRAFE",
        "1W2TS STRAFE",
        "1W2T STATIC",
        "1W2TS STATIC",
        "1W2TES STATIC",
        "1W4T DYNAMIC CLOSE",
        "1W4TS DYNAMIC CLOSE",
        "1W4T STRAFE CLOSE",
        "1W4TS STRAFE CLOSE",
        "1W4TES STRAFE CLOSE",
        "1W4T STATIC CLOSE",
        "1W4TS STATIC CLOSE",
        "1W4TES STATIC CLOSE",
        "1W2T DYNAMIC SWITCHING 20",
        "1W2TS DYNAMIC SWITCHING 20",
        "1W2T STRAFE SWITCHING 10",
        "1W2TS STRAFE SWITCHING 10",
        "1W4T DYNAMIC SWITCHING 20 CLOSE",
        "1W4TS DYNAMIC SWITCHING 20 CLOSE",
        "1W4T STRAFE SWITCHING 10 CLOSE",
        "1W4TS STRAFE SWITCHING 10 CLOSE",
        "1W4TES STRAFE SWITCHING 10 CLOSE",
        "1W1TS DYNAMIC TRACKING",
        "1W1TS DYNAMIC TRACKING CLOSE",
    };
    const int default_wall_count = static_cast<int>(sizeof(default_wall_order) / sizeof(default_wall_order[0]));
    ok = self_test_check(static_cast<int>(game.wall_presets.size()) == default_wall_count, "default wall preset list matches generated clicking, switching, and tracking presets") && ok;
    for (int i = 0; i < default_wall_count && i < static_cast<int>(game.wall_presets.size()); ++i) {
        ok = self_test_check(game.wall_presets[i].name == default_wall_order[i], "default wall presets are in sensible order") && ok;
    }
    int dynamic_index = find_wall_preset(game, "1W2T DYNAMIC");
    int dynamic_small_index = find_wall_preset(game, "1W2TS DYNAMIC");
    int strafe_small_index = find_wall_preset(game, "1W2TS STRAFE");
    int static_index = find_wall_preset(game, "1W2T STATIC");
    int static_extra_index = find_wall_preset(game, "1W2TES STATIC");
    int close_index = find_wall_preset(game, "1W4T DYNAMIC CLOSE");
    ok = self_test_check(dynamic_index >= 0 && game.wall_presets[dynamic_index].settings.target_count_min == 2 && std::fabs(game.wall_presets[dynamic_index].settings.radius_min - 0.08f) < 0.0001f && std::fabs(game.wall_presets[dynamic_index].settings.horizontal_speed_max - 1.5f) < 0.0001f && std::fabs(game.wall_presets[dynamic_index].settings.vertical_speed_max - 0.75f) < 0.0001f, "dynamic default uses 1W2T settings") && ok;
    ok = self_test_check(dynamic_small_index >= 0 && std::fabs(game.wall_presets[dynamic_small_index].settings.radius_min - 0.04f) < 0.0001f, "dynamic small default uses small target size") && ok;
    ok = self_test_check(strafe_small_index >= 0 && game.wall_presets[strafe_small_index].settings.target_count_min == 2 && std::fabs(game.wall_presets[strafe_small_index].settings.radius_min - 0.04f) < 0.0001f && std::fabs(game.wall_presets[strafe_small_index].settings.horizontal_speed_min - 1.0f) < 0.0001f && std::fabs(game.wall_presets[strafe_small_index].settings.change_max - 4.0f) < 0.0001f, "strafe small default uses small targets") && ok;
    ok = self_test_check(static_index >= 0 && game.wall_presets[static_index].settings.target_count_min == 2 && game.wall_presets[static_index].settings.horizontal_speed_max == 0.0f, "static 1W2T default is non-moving") && ok;
    ok = self_test_check(static_extra_index >= 0 && game.wall_presets[static_extra_index].settings.target_count_min == 2 && std::fabs(game.wall_presets[static_extra_index].settings.radius_min - 0.02f) < 0.0001f && game.wall_presets[static_extra_index].settings.horizontal_speed_max == 0.0f, "static 1W2TES default is non-moving extra-small") && ok;
    ok = self_test_check(close_index >= 0 && std::fabs(game.wall_presets[close_index].settings.wall_distance_min - 4.0f) < 0.0001f && std::fabs(game.wall_presets[close_index].settings.wall_distance_max - 5.0f) < 0.0001f, "close default uses a 4-5m wall range") && ok;
    int switch_index = find_wall_preset(game, "1W2T DYNAMIC SWITCHING 20");
    int switch_strafe_index = find_wall_preset(game, "1W2T STRAFE SWITCHING 10");
    int track_index = find_wall_preset(game, "1W1TS DYNAMIC TRACKING");
    ok = self_test_check(switch_index >= 0 && game.wall_presets[switch_index].settings.task_mode == TaskMode::Tracking && game.wall_presets[switch_index].settings.target_health == 20 && game.wall_presets[switch_index].settings.target_count_min == 2 && std::fabs(game.wall_presets[switch_index].settings.vertical_speed_max - 0.75f) < 0.0001f, "target-switching default copies dynamic clicking motion with 20 health") && ok;
    ok = self_test_check(switch_strafe_index >= 0 && game.wall_presets[switch_strafe_index].settings.task_mode == TaskMode::Tracking && game.wall_presets[switch_strafe_index].settings.target_health == 10 && std::fabs(game.wall_presets[switch_strafe_index].settings.vertical_speed_max) < 0.0001f, "strafe switching default uses 10 health") && ok;
    ok = self_test_check(track_index >= 0 && game.wall_presets[track_index].settings.task_mode == TaskMode::Tracking && game.wall_presets[track_index].settings.target_health == 0 && game.wall_presets[track_index].settings.target_count_min == 1 && std::fabs(game.wall_presets[track_index].settings.radius_min - 0.04f) < 0.0001f, "tracking default is one small dynamic target with infinite health") && ok;
    for (const WallPreset& preset : game.wall_presets) {
        bool close_wall = preset.name.size() >= 6 && preset.name.compare(preset.name.size() - 6, 6, " CLOSE") == 0;
        float expected_min = close_wall ? 4.0f : 8.0f;
        float expected_max = close_wall ? 5.0f : 10.0f;
        ok = self_test_check(std::fabs(preset.settings.wall_distance_min - expected_min) < 0.0001f && std::fabs(preset.settings.wall_distance_max - expected_max) < 0.0001f, "default wall presets use mid 8-10m or close 4-5m ranges") && ok;
    }

    Game preset_order;
    preset_order.wall_presets = {
        {"CUSTOM", WallClickSettings{}},
        {"1W6T STRAFE", WallClickSettings{}},
        {"1W2T DYNAMIC", WallClickSettings{}},
    };
    preset_order.selected_wall_preset = 2;
    preset_order.wall_presets[1].settings.radius_min = 0.12f;
    preset_order.wall_presets[1].settings.radius_max = 0.12f;
    preset_order.wall_presets[2].settings.radius_min = 0.13f;
    preset_order.wall_presets[2].settings.radius_max = 0.13f;
    ensure_presets(preset_order);
    ok = self_test_check(preset_order.wall_presets[0].name == "1W2T DYNAMIC" && preset_order.wall_presets[1].name == "1W2TS DYNAMIC", "existing built-in wall presets are reordered with defaults") && ok;
    ok = self_test_check(static_cast<int>(preset_order.wall_presets.size()) >= 3 && preset_order.wall_presets[preset_order.wall_presets.size() - 2].name == "CUSTOM" && preset_order.wall_presets.back().name == "1W6T STRAFE", "custom and unknown wall presets remain after built-in defaults") && ok;
    ok = self_test_check(std::fabs(preset_order.wall_presets[find_wall_preset(preset_order, "1W6T STRAFE")].settings.radius_min - 0.12f) < 0.0001f, "reordering preserves existing strafe preset settings") && ok;
    ok = self_test_check(preset_order.wall_presets[preset_order.selected_wall_preset].name == "1W2T DYNAMIC" && preset_order.wall_presets[preset_order.selected_wall_preset].settings.target_count_min == 3 && std::fabs(preset_order.wall_presets[preset_order.selected_wall_preset].settings.radius_min - 0.13f) < 0.0001f, "reordering preserves selected built-in preset settings") && ok;

    // Name field editing through the draft-based text-box model.
    menu_focus_field(game, FieldId::WallName);
    ok = self_test_check(game.active_field == FieldId::WallName, "clicking a name box focuses it") && ok;
    while (!game.edit_draft.empty()) {
        Input input;
        input.backspace_pressed = true;
        menu_handle_edit(game, input);
    }
    ok = self_test_check(game.edit_draft.empty(), "preset name draft can be emptied") && ok;
    Input type_input;
    type_input.text_input = "  u custom!";
    menu_handle_edit(game, type_input);
    ok = self_test_check(game.edit_draft == "U CUSTOM", "typing into empty name field filters, uppercases, and trims") && ok;
    ok = self_test_check(game.active_field == FieldId::WallName, "typing keeps the name field focused") && ok;
    Input commit_name;
    commit_name.enter_pressed = true;
    menu_handle_edit(game, commit_name);
    ok = self_test_check(game.wall_preset_name == "U CUSTOM" && game.active_field == FieldId::None, "enter commits the name draft and unfocuses") && ok;

    game.wall_preset_name.clear();
    save_current_wall_preset(game);
    ok = self_test_check(game.wall_presets[game.selected_wall_preset].name == "UNTITLED", "empty saved preset finalizes to UNTITLED") && ok;

    // Numeric box editing: fresh-replace, backspace-edits, commit + clamp.
    apply_selected_presets(game);
    game.wall_settings.wall_distance_min = 5.71f;
    menu_focus_field(game, FieldId::WallDistMin);
    ok = self_test_check(game.edit_draft == "5.71" && game.edit_fresh, "focusing a numeric box shows its value and arms fresh-replace") && ok;
    Input digit8;
    digit8.text_input = "8";
    menu_handle_edit(game, digit8);
    ok = self_test_check(game.edit_draft == "8" && !game.edit_fresh, "first keystroke replaces a freshly focused numeric value") && ok;
    menu_focus_field(game, FieldId::WallDistMin);
    Input bs;
    bs.backspace_pressed = true;
    menu_handle_edit(game, bs);
    ok = self_test_check(game.edit_draft == "5.7" && !game.edit_fresh, "backspace edits a fresh numeric value instead of wiping it") && ok;
    Input letters;
    letters.text_input = "ab1.2x.5";  // letters and a second dot are rejected
    menu_focus_field(game, FieldId::WallDistMin);
    menu_handle_edit(game, letters);
    ok = self_test_check(game.edit_draft == "1.25", "numeric boxes only accept digits and a single dot") && ok;
    Input commit_num;
    commit_num.enter_pressed = true;
    menu_handle_edit(game, commit_num);
    normalize_settings(game);
    ok = self_test_check(std::fabs(game.wall_settings.wall_distance_min - 2.0f) < 0.0001f, "committed numeric value is clamped into range") && ok;

    // Empty numeric draft keeps the previous value.
    game.wall_settings.wall_distance_min = 7.0f;
    menu_focus_field(game, FieldId::WallDistMin);
    while (!game.edit_draft.empty()) {
        Input b;
        b.backspace_pressed = true;
        menu_handle_edit(game, b);
    }
    menu_blur_field(game);
    ok = self_test_check(std::fabs(game.wall_settings.wall_distance_min - 7.0f) < 0.0001f, "committing an empty numeric draft keeps the previous value") && ok;

    // TAB / SHIFT+TAB navigation walks the field order within the active tab.
    game.menu_tab = MenuTab::Clicking;
    menu_focus_field(game, FieldId::WallName);
    Input tab_fwd;
    tab_fwd.tab_pressed = true;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::WallTargetsMin, "tab advances from name to target count") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::WallHealth, "tab advances from target count to health") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::WallDistMin, "tab advances from health to distance") && ok;
    Input tab_back;
    tab_back.tab_pressed = true;
    tab_back.shift_down = true;
    menu_handle_edit(game, tab_back);
    ok = self_test_check(game.active_field == FieldId::WallHealth, "shift+tab returns to health") && ok;
    menu_handle_edit(game, tab_back);
    ok = self_test_check(game.active_field == FieldId::WallTargetsMin, "shift+tab returns to target count") && ok;
    menu_handle_edit(game, tab_back);
    ok = self_test_check(game.active_field == FieldId::WallName, "shift+tab returns to the name") && ok;
    menu_blur_field(game);

    game.wall_settings.target_count_min = 3;
    game.wall_settings.target_count_max = 8;
    normalize_settings(game);
    ok = self_test_check(game.wall_settings.target_count_min == 3 && game.wall_settings.target_count_max == 3, "target count is a single value") && ok;
    menu_focus_field(game, FieldId::WallDistMax);
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::WallRadiusMin, "tab advances from wall max to radius") && ok;
    menu_blur_field(game);

    game.wall_settings.target_health = 2000;
    normalize_settings(game);
    ok = self_test_check(game.wall_settings.target_health == WALL_TARGET_HEALTH_MAX, "target health clamps to the maximum") && ok;
    game.wall_settings.target_health = -4;
    normalize_settings(game);
    ok = self_test_check(game.wall_settings.target_health == 0, "target health clamps below zero to infinite") && ok;

    game.wall_settings.radius_min = 0.09f;
    game.wall_settings.radius_max = 0.20f;
    menu_focus_field(game, FieldId::WallRadiusMin);
    Input tiny_radius;
    tiny_radius.text_input = "0.01";
    menu_handle_edit(game, tiny_radius);
    menu_blur_field(game);
    normalize_settings(game);
    ok = self_test_check(std::fabs(game.wall_settings.radius_min - 0.01f) < 0.0001f, "wall target radius accepts 0.01m minimum") && ok;
    game.wall_settings.radius_min = 0.001f;
    normalize_settings(game);
    ok = self_test_check(std::fabs(game.wall_settings.radius_min - 0.01f) < 0.0001f, "wall target radius clamps below 0.01m") && ok;

    game.menu_tab = MenuTab::Settings;
    menu_focus_field(game, FieldId::GenSens);
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenOutlineOpacity, "settings tab navigation reaches outline opacity") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenOutlineThick, "settings tab navigation reaches outline thickness") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenDotThick, "settings tab navigation reaches center dot thickness") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenLength, "settings tab navigation reaches inner line length") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenThick, "settings tab navigation reaches inner line thickness") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenGap, "settings tab navigation reaches inner line offset") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenTargetR, "settings tab navigation reaches target color red") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenTargetG, "settings tab navigation reaches target color green") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenTargetB, "settings tab navigation reaches target color blue") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenWallR, "settings tab navigation reaches wall color red") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenWallG, "settings tab navigation reaches wall color green") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenWallB, "settings tab navigation reaches wall color blue") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::GenSens, "settings tab navigation wraps after wall color") && ok;
    menu_blur_field(game);

    game.menu_tab = MenuTab::Playlists;
    menu_focus_field(game, FieldId::PlaylistSearch);
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::PlaylistName, "playlist tab navigation advances from search to name") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::PlaylistAddSearch, "playlist tab navigation advances from name to add search") && ok;
    menu_handle_edit(game, tab_fwd);
    ok = self_test_check(game.active_field == FieldId::PlaylistSearch, "playlist tab navigation wraps after add search") && ok;
    menu_blur_field(game);

    game.selected_wall_preset = 0;
    game.wall_presets[0].settings.radius_min = 0.34f;
    game.wall_presets[0].settings.radius_max = 0.34f;
    game.wall_settings = game.wall_presets[0].settings;
    game.wall_settings.radius_min = 0.22f;
    game.wall_settings.radius_max = 0.28f;
    game.active_field = FieldId::WallName;
    int old_count = static_cast<int>(game.wall_presets.size());
    new_wall_preset(game);
    ok = self_test_check(static_cast<int>(game.wall_presets.size()) == old_count + 1, "new wall preset appends exactly one preset") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[0].settings.radius_min - 0.34f) < 0.0001f, "new wall preset does not overwrite selected preset") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[game.selected_wall_preset].settings.radius_min - 0.22f) < 0.0001f, "new wall preset copies current edited min settings") && ok;
    ok = self_test_check(std::fabs(game.wall_presets[game.selected_wall_preset].settings.radius_max - 0.28f) < 0.0001f, "new wall preset copies current edited max settings") && ok;
    ok = self_test_check(game.active_field == FieldId::None, "new wall preset exits text edit mode") && ok;

    game.wall_presets = {{"SELECTED", WallClickSettings{}}, {"BOTTOM", WallClickSettings{}}};
    game.selected_wall_preset = 0;
    game.wall_presets[0].settings.radius_min = 0.11f;
    game.wall_presets[0].settings.radius_max = 0.12f;
    game.wall_presets[1].settings.radius_min = 0.31f;
    game.wall_presets[1].settings.radius_max = 0.32f;
    game.wall_settings = game.wall_presets[0].settings;
    new_wall_preset(game);
    ok = self_test_check(std::fabs(game.wall_settings.radius_min - 0.11f) < 0.0001f && std::fabs(game.wall_settings.radius_max - 0.12f) < 0.0001f, "new wall preset copies selected settings instead of bottom row") && ok;

    game.wall_presets.push_back({"CLICK PRESET 4", game.wall_settings});
    std::string unique_click = unique_preset_name(game.wall_presets, "CLICK PRESET 4", -1);
    ok = self_test_check(unique_click != "CLICK PRESET 4", "generated wall preset names avoid duplicates") && ok;
    ok = self_test_check(sanitize_preset_name("1W4TES STRAFE SWITCHING 20 CLOSE") == "1W4TES STRAFE SWITCHING 20 CLOSE", "preset name limit keeps switching CLOSE labels") && ok;

    game.wall_presets = {{"ONLY WALL", WallClickSettings{}}};
    game.selected_wall_preset = 0;
    game.active_field = FieldId::WallName;
    delete_wall_preset(game);
    ok = self_test_check(!game.wall_presets.empty() && game.wall_presets[0].name == "1W2T DYNAMIC" && find_wall_preset(game, "1W2TES STATIC") >= 0, "deleting the last wall preset restores default wall presets") && ok;
    ok = self_test_check(game.active_field == FieldId::None, "delete wall preset exits text edit mode") && ok;

    {
        Game reset_test;
        reset_test.wall_presets = {
            {"CUSTOM", WallClickSettings{}},
            {"1W3T DYNAMIC", WallClickSettings{}},
        };
        reset_test.wall_presets[1].settings.radius_min = 0.22f;
        reset_test.wall_presets[1].settings.radius_max = 0.22f;
        reset_test.selected_wall_preset = 1;
        reset_test.wall_preset_scroll = 1;
        reset_test.active_field = FieldId::WallName;
        reset_wall_presets(reset_test);
        ok = self_test_check(static_cast<int>(reset_test.wall_presets.size()) == 26 && find_wall_preset(reset_test, "CUSTOM") < 0, "reset tasks replaces every preset with the compiled defaults") && ok;
        ok = self_test_check(reset_test.selected_wall_preset == 0 && reset_test.wall_preset_scroll == 0 && reset_test.wall_preset_name == "1W2T DYNAMIC", "reset tasks selects the first default task") && ok;
        ok = self_test_check(std::fabs(reset_test.wall_settings.radius_min - 0.08f) < 0.0001f && std::fabs(reset_test.wall_settings.horizontal_speed_max - 1.5f) < 0.0001f, "reset tasks restores compiled default settings") && ok;
        ok = self_test_check(reset_test.active_field == FieldId::None, "reset tasks exits text edit mode") && ok;
    }

    {
        std::ofstream dump("build/self-test-live.dump");
        dump << "task \"LIVE RESET\" 4 4 4 5 0.08 0.08 7.5 7.5 0 0 0 0 0 0 0 7\n";
        dump.close();
        g_default_tasks_dump_override = "build/self-test-live.dump";
        g_live_default_tasks = true;
        Game live;
        live.wall_presets = {{"CUSTOM", WallClickSettings{}}};
        reset_wall_presets(live);
        ensure_presets(live);
        ok = self_test_check(live.wall_presets.size() == 1 && live.wall_preset_name == "LIVE RESET", "reset tasks loads dumped default tasks") && ok;
        ok = self_test_check(find_wall_preset(live, "1W2T DYNAMIC") < 0 && find_wall_preset(live, "1W4T DYNAMIC") < 0, "reset tasks does not re-inject compiled defaults after a live dump") && ok;
        ok = self_test_check(live.wall_settings.target_count_min == 4 && live.wall_settings.target_health == 7, "reset tasks applies dumped target count and health") && ok;
        ok = self_test_check(std::fabs(live.wall_settings.wall_distance_min - 4.0f) < 0.0001f && std::fabs(live.wall_settings.horizontal_speed_min - 7.5f) < 0.0001f, "reset tasks applies dumped wall and speed values") && ok;
        g_default_tasks_dump_override.clear();
        g_live_default_tasks = false;
        std::remove("build/self-test-live.dump");
    }

    {
        std::ofstream script("build/self-test-live-tasks.py");
        script << "print('task \"PY RESET\" 2 2 12 15 0.04 0.04 1 2 0 0 4 4 1 4 1 0')\n";
        script.close();
        g_live_default_tasks = true;
        g_default_tasks_script_override = "build/self-test-live-tasks.py";
        Game py;
        reset_wall_presets(py);
        ensure_presets(py);
        ok = self_test_check(py.wall_presets.size() == 1 && py.wall_preset_name == "PY RESET", "reset tasks re-runs a python default-tasks script") && ok;
        ok = self_test_check(py.wall_settings.task_mode == TaskMode::Tracking && py.wall_settings.target_health == 0, "reset tasks applies python mode and health") && ok;
        ok = self_test_check(std::fabs(py.wall_settings.wall_distance_max - 15.0f) < 0.0001f, "reset tasks applies python wall range") && ok;
        g_default_tasks_script_override.clear();
        g_live_default_tasks = false;
        std::remove("build/self-test-live-tasks.py");
    }

    // Creating a preset must scroll it into the visible list window.
    {
        Game scroll_test;
        for (int i = 0; i < 12; ++i) {
            scroll_test.wall_presets.push_back({"FILLER", WallClickSettings{}});
        }
        scroll_test.wall_preset_scroll = 0;
        new_wall_preset(scroll_test);
        int sel = scroll_test.selected_wall_preset;
        ok = self_test_check(sel >= scroll_test.wall_preset_scroll && sel <= scroll_test.wall_preset_scroll + VISIBLE_PRESET_ROWS - 1, "new preset is scrolled into the visible list window") && ok;
    }

    {
        Game search_test;
        search_test.wall_presets = {
            {"1W2T DYNAMIC", WallClickSettings{}},
            {"1W3T STRAFE", WallClickSettings{}},
            {"1W2T STRAFE FAR", WallClickSettings{}},
            {"1W1TS DYNAMIC TRACKING", WallClickSettings{}},
        };
        search_test.preset_search = "strafe";
        std::vector<int> hits = matching_wall_presets(search_test);
        ok = self_test_check(hits.size() == 2 && hits[0] == 1 && hits[1] == 2, "preset search matches names case-insensitively") && ok;
        search_test.preset_search.clear();
        ok = self_test_check(matching_wall_presets(search_test).size() == 4, "empty preset search shows every task") && ok;
        menu_focus_field(search_test, FieldId::PresetSearch);
        Input typed;
        typed.text_input = "Dyn";
        menu_handle_edit(search_test, typed);
        ok = self_test_check(search_test.edit_draft == "Dyn", "preset search keeps mixed-case draft") && ok;
        hits = matching_wall_presets(search_test);
        ok = self_test_check(hits.size() == 2 && hits[0] == 0 && hits[1] == 3, "live search draft filters the task list") && ok;
        search_test.preset_search = "NOPE";
        new_wall_preset(search_test);
        ok = self_test_check(search_test.preset_search.empty() && matching_wall_presets(search_test).size() == 5, "new task clears search so the copy is visible") && ok;
    }

    g_settings_path_override = "build/self-test-settings.cfg";
    std::remove(g_settings_path_override.c_str());
    game.wall_presets = {{"1W3T DYNAMIC", WallClickSettings{}}, {"1W6T STRAFE", WallClickSettings{}}};
    game.selected_wall_preset = 0;
    apply_selected_presets(game);
    game.wall_preset_name = "TINY PASU";
    game.wall_settings.target_count_min = 8;
    game.wall_settings.target_count_max = 8;
    game.wall_settings.wall_distance_min = 6.25f;
    game.wall_settings.radius_min = 0.18f;
    game.wall_settings.radius_max = 0.22f;
    save_current_wall_preset(game);
    game.sensitivity = 0.777f;
    game.crosshair.length = 14.0f;
    game.crosshair.gap = 6.0f;
    game.crosshair.thickness = 3.0f;
    game.crosshair.outlines = true;
    game.crosshair.outline_opacity = 0.75f;
    game.crosshair.outline_thickness = 2.0f;
    game.crosshair.center_dot = true;
    game.crosshair.center_dot_thickness = 4.0f;
    game.target_color = {32, 210, 244};
    game.wall_color = {44, 55, 66};
    save_settings(game);

    Game loaded;
    load_settings(loaded);
    ok = self_test_check(std::fabs(loaded.sensitivity - 0.777f) < 0.0001f, "saved general sensitivity loads") && ok;
    ok = self_test_check(std::fabs(loaded.crosshair.length - 14.0f) < 0.0001f, "saved crosshair loads") && ok;
    ok = self_test_check(loaded.crosshair.outlines && std::fabs(loaded.crosshair.outline_opacity - 0.75f) < 0.0001f && std::fabs(loaded.crosshair.outline_thickness - 2.0f) < 0.0001f, "saved crosshair outlines load") && ok;
    ok = self_test_check(loaded.crosshair.center_dot && std::fabs(loaded.crosshair.center_dot_thickness - 4.0f) < 0.0001f, "saved crosshair center dot loads") && ok;
    ok = self_test_check(loaded.target_color.r == 32 && loaded.target_color.g == 210 && loaded.target_color.b == 244, "saved target color loads") && ok;
    ok = self_test_check(loaded.wall_color.r == 44 && loaded.wall_color.g == 55 && loaded.wall_color.b == 66, "saved wall color loads") && ok;

    game.crosshair.length = 0.0f;
    game.crosshair.center_dot = true;
    save_settings(game);
    Game zero_lines;
    load_settings(zero_lines);
    ok = self_test_check(std::fabs(zero_lines.crosshair.length) < 0.0001f && zero_lines.crosshair.center_dot, "inner line length 0 saves as a hidden-lines crosshair") && ok;
    ok = self_test_check(!loaded.wall_presets.empty(), "saved wall presets load") && ok;
    ok = self_test_check(loaded.wall_preset_name == "TINY PASU", "selected named wall preset loads into editor") && ok;
    ok = self_test_check(loaded.wall_settings.target_count_min == 8 && loaded.wall_settings.target_count_max == 8, "selected wall preset target count loads") && ok;
    ok = self_test_check(std::fabs(loaded.wall_settings.wall_distance_min - 6.25f) < 0.0001f, "selected wall distance loads") && ok;
    ok = self_test_check(std::fabs(loaded.wall_settings.radius_max - 0.22f) < 0.0001f, "selected wall preset radius range loads") && ok;
    ok = self_test_check(loaded.wall_settings.task_mode == TaskMode::Clicking && loaded.wall_settings.target_health == 1, "saved wall presets default to clicking with one-shot health") && ok;

    game.selected_wall_preset = 1;
    apply_selected_presets(game);
    float selected_radius = game.wall_settings.radius_min;
    game.wall_settings.radius_min = 0.88f;
    game.sensitivity = 0.555f;
    save_settings(game);
    Game general_only_loaded;
    load_settings(general_only_loaded);
    ok = self_test_check(std::fabs(general_only_loaded.sensitivity - 0.555f) < 0.0001f, "general save persists sensitivity") && ok;
    ok = self_test_check(std::fabs(general_only_loaded.wall_settings.radius_min - selected_radius) < 0.0001f, "general save does not silently overwrite selected wall preset") && ok;

    Game color_clamp;
    color_clamp.target_color = {-10, 128, 300};
    color_clamp.wall_color = {999, -20, 64};
    normalize_settings(color_clamp);
    ok = self_test_check(color_clamp.target_color.r == 0 && color_clamp.target_color.g == 128 && color_clamp.target_color.b == 255, "target color channels clamp to RGB byte range") && ok;
    ok = self_test_check(color_clamp.wall_color.r == 255 && color_clamp.wall_color.g == 0 && color_clamp.wall_color.b == 64, "wall color channels clamp to RGB byte range") && ok;

    Game crosshair_clamp;
    crosshair_clamp.crosshair.length = -4.0f;
    crosshair_clamp.crosshair.gap = 99.0f;
    crosshair_clamp.crosshair.thickness = 0.0f;
    crosshair_clamp.crosshair.outline_opacity = 1.5f;
    crosshair_clamp.crosshair.outline_thickness = 0.0f;
    crosshair_clamp.crosshair.center_dot_thickness = 12.0f;
    normalize_settings(crosshair_clamp);
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.length) < 0.0001f, "crosshair length 0 hides the inner lines") && ok;
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.gap - 20.0f) < 0.0001f, "crosshair offset clamps to 20") && ok;
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.thickness - 1.0f) < 0.0001f, "crosshair thickness clamps to at least 1") && ok;
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.outline_opacity - 1.0f) < 0.0001f, "outline opacity clamps to 1") && ok;
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.outline_thickness - 1.0f) < 0.0001f, "outline thickness clamps to at least 1") && ok;
    ok = self_test_check(std::fabs(crosshair_clamp.crosshair.center_dot_thickness - 6.0f) < 0.0001f, "center dot thickness clamps to 6") && ok;

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
    ok = self_test_check(std::fabs(old_loaded.wall_settings.radius_min - units_to_wall_meters(0.31f)) < 0.0001f && std::fabs(old_loaded.wall_settings.radius_max - units_to_wall_meters(0.31f)) < 0.0001f, "old radius migrates to fixed range") && ok;
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
    ok = self_test_check(std::fabs(legacy_loaded.wall_settings.horizontal_speed_min - units_to_wall_meters(4.25f)) < 0.0001f && std::fabs(legacy_loaded.wall_settings.horizontal_speed_max - units_to_wall_meters(4.25f)) < 0.0001f, "legacy horizontal speed key migrates to fixed range") && ok;
    ok = self_test_check(std::fabs(legacy_loaded.wall_settings.change_min - 0.66f) < 0.0001f && std::fabs(legacy_loaded.wall_settings.change_max - 1.86f) < 0.0001f, "legacy direction interval key migrates to randomized range") && ok;

    {
        std::ofstream actual_v2("build/self-test-settings.cfg");
        actual_v2 << "version 2\n";
        actual_v2 << "sensitivity 0.5\n";
        actual_v2 << "crosshair 9 4 2\n";
        actual_v2 << "selected_wall 1\n";
        actual_v2 << "selected_pill 0\n";
        actual_v2 << "wall_preset \"1W6T STRAFE\" 6 0.12 4 0 20 1.6\n";
        actual_v2 << "wall_preset \"1W4T DYNAMIC\" 4 0.24 6 2 20 1.6\n";
        actual_v2 << "pill_preset \"SMOOTH PILL\" 1.24 4 12 0.35 2.4\n";
    }
    Game actual_v2_loaded;
    load_settings(actual_v2_loaded);
    ok = self_test_check(actual_v2_loaded.wall_preset_name == "1W4T DYNAMIC", "actual v2 selected wall preset keeps 1W4T") && ok;
    ok = self_test_check(actual_v2_loaded.wall_settings.target_count_min == 4 && actual_v2_loaded.wall_settings.target_count_max == 4, "actual v2 dynamic wall target count stays 1W4T") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.radius_min) - 0.24f) < 0.001f, "actual v2 wall radius preserves old internal size") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.horizontal_speed_min) - 6.0f) < 0.001f, "actual v2 wall horizontal speed preserves old internal speed") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.vertical_speed_min) - 2.0f) < 0.001f, "actual v2 wall vertical speed preserves old internal speed") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.acceleration_min) - 20.0f) < 0.001f, "actual v2 wall acceleration preserves old internal acceleration") && ok;
    ok = self_test_check(std::fabs(actual_v2_loaded.wall_settings.wall_distance_min - 8.0f) < 0.0001f && std::fabs(actual_v2_loaded.wall_settings.wall_distance_max - 10.0f) < 0.0001f, "actual v2 built-in wall distance migrates to 8-10m") && ok;
    ok = self_test_check(actual_v2_loaded.wall_settings.task_mode == TaskMode::Clicking && actual_v2_loaded.wall_settings.target_health == 1, "actual v2 wall presets migrate to clicking with one-shot health") && ok;
    save_settings(actual_v2_loaded);
    Game actual_v2_roundtrip;
    load_settings(actual_v2_roundtrip);
    ok = self_test_check(actual_v2_roundtrip.wall_preset_name == "1W4T DYNAMIC", "actual v2 save round trip preserves selected wall preset") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_roundtrip.wall_settings.radius_min) - 0.24f) < 0.001f && std::fabs(wall_to_units(actual_v2_roundtrip.wall_settings.horizontal_speed_min) - 6.0f) < 0.001f, "actual v2 save round trip preserves wall behavior") && ok;

    {
        std::ofstream v7("build/self-test-settings.cfg");
        v7 << "version 7\n";
        v7 << "selected_wall 0\n";
        v7 << "wall_preset \"TRACK MIGRATE\" 3 3 6 7.5 0.08 0.08 1 1.5 0 0.75 5 5 0.75 1.5\n";
        v7 << "pill_preset \"SMOOTH PILL\" 1.13 6.8 9.55 3.64 10.91 0.35 2.4\n";
    }
    Game v7_loaded;
    load_settings(v7_loaded);
    ok = self_test_check(v7_loaded.wall_preset_name == "TRACK MIGRATE", "v7 wall preset name loads after pill removal") && ok;
    ok = self_test_check(v7_loaded.wall_settings.task_mode == TaskMode::Clicking && v7_loaded.wall_settings.target_health == 1, "v7 wall presets migrate to clicking with one-shot health") && ok;

    {
        std::ofstream v8("build/self-test-settings.cfg");
        v8 << "version 8\n";
        v8 << "selected_wall 0\n";
        v8 << "wall_preset \"WALL TRACK\" 2 2 6 7.5 0.08 0.08 1 1.5 0 0.75 5 5 0.75 1.5 1 20\n";
    }
    Game v8_loaded;
    load_settings(v8_loaded);
    ok = self_test_check(v8_loaded.wall_preset_name == "WALL TRACK", "v8 tracking wall preset name loads") && ok;
    ok = self_test_check(v8_loaded.wall_settings.task_mode == TaskMode::Tracking && v8_loaded.wall_settings.target_health == 20, "v8 wall preset task mode and health load") && ok;
    save_settings(v8_loaded);
    Game v8_roundtrip;
    load_settings(v8_roundtrip);
    ok = self_test_check(v8_roundtrip.wall_settings.task_mode == TaskMode::Tracking && v8_roundtrip.wall_settings.target_health == 20, "v8 save round trip preserves tracking mode and health") && ok;

    {
        std::ofstream v8_click("build/self-test-settings.cfg");
        v8_click << "version 8\n";
        v8_click << "selected_wall 0\n";
        v8_click << "wall_preset \"OLD CLICK\" 3 3 6 7.5 0.08 0.08 1 1.5 0 0.75 5 5 0.75 1.5 0 0\n";
    }
    Game v8_click_loaded;
    load_settings(v8_click_loaded);
    ok = self_test_check(v8_click_loaded.wall_settings.task_mode == TaskMode::Clicking && v8_click_loaded.wall_settings.target_health == 1, "v8 clicking health 0 migrates to one-shot") && ok;

    {
        std::ofstream v9("build/self-test-settings.cfg");
        v9 << "version 9\n";
        v9 << "selected_wall 0\n";
        v9 << "wall_preset \"CLICK INF\" 3 3 6 7.5 0.08 0.08 1 1.5 0 0.75 5 5 0.75 1.5 0 0\n";
    }
    Game v9_loaded;
    load_settings(v9_loaded);
    ok = self_test_check(v9_loaded.wall_settings.task_mode == TaskMode::Clicking && v9_loaded.wall_settings.target_health == 0, "v9 clicking health 0 stays infinite") && ok;

    {
        std::ofstream v9_builtin("build/self-test-settings.cfg");
        v9_builtin << "version 9\n";
        v9_builtin << "selected_wall 0\n";
        v9_builtin << "wall_preset \"1W6T STRAFE\" 6 6 6 7.5 0.08 0.08 0.75 1.25 0 0 4 4 1 2.5 0 1\n";
        v9_builtin << "wall_preset \"MY CUSTOM\" 2 2 6 7.5 0.08 0.08 0 0 0 0 0 0 0 0 0 1\n";
    }
    Game v9_builtin_loaded;
    load_settings(v9_builtin_loaded);
    int strafe_loaded = find_wall_preset(v9_builtin_loaded, "1W6T STRAFE");
    int custom_loaded = find_wall_preset(v9_builtin_loaded, "MY CUSTOM");
    ok = self_test_check(strafe_loaded >= 0 && std::fabs(v9_builtin_loaded.wall_presets[strafe_loaded].settings.wall_distance_min - 8.0f) < 0.0001f && std::fabs(v9_builtin_loaded.wall_presets[strafe_loaded].settings.wall_distance_max - 10.0f) < 0.0001f, "v9 built-in wall range migrates to 8-10m") && ok;
    ok = self_test_check(custom_loaded >= 0 && std::fabs(v9_builtin_loaded.wall_presets[custom_loaded].settings.wall_distance_min - 6.0f) < 0.0001f && std::fabs(v9_builtin_loaded.wall_presets[custom_loaded].settings.wall_distance_max - 7.5f) < 0.0001f, "v9 custom wall range is left unchanged") && ok;

    {
        std::ofstream v10("build/self-test-settings.cfg");
        v10 << "version 10\n";
        v10 << "selected_wall 0\n";
        v10 << "wall_preset \"1W6T STRAFE\" 6 6 5 6 0.08 0.08 0.75 1.25 0 0 4 4 1 2.5 0 1\n";
    }
    Game v10_loaded;
    load_settings(v10_loaded);
    ok = self_test_check(std::fabs(v10_loaded.wall_settings.wall_distance_min - 5.0f) < 0.0001f && std::fabs(v10_loaded.wall_settings.wall_distance_max - 6.0f) < 0.0001f, "v10 built-in wall range is not rewritten") && ok;
    ok = self_test_check(v10_loaded.playlists.empty(), "v10 settings load with no playlists") && ok;

    {
        std::ofstream v11("build/self-test-settings.cfg");
        v11 << "version 11\n";
        v11 << "sensitivity 0.35\n";
        v11 << "crosshair 9 4 2\n";
        v11 << "selected_wall 0\n";
        v11 << "wall_preset \"1W2T DYNAMIC\" 3 3 8 10 0.08 0.08 1 1.5 0 0.75 8 8 1 2 0 1\n";
    }
    Game v11_loaded;
    load_settings(v11_loaded);
    ok = self_test_check(std::fabs(v11_loaded.crosshair.length - 9.0f) < 0.0001f && std::fabs(v11_loaded.crosshair.gap - 4.0f) < 0.0001f && std::fabs(v11_loaded.crosshair.thickness - 2.0f) < 0.0001f, "v11 three-value crosshair still loads") && ok;
    ok = self_test_check(!v11_loaded.crosshair.outlines && !v11_loaded.crosshair.center_dot, "v11 crosshair migrates with outlines and center dot off") && ok;
    ok = self_test_check(std::fabs(v11_loaded.crosshair.outline_opacity - 0.5f) < 0.0001f && std::fabs(v11_loaded.crosshair.outline_thickness - 1.0f) < 0.0001f && std::fabs(v11_loaded.crosshair.center_dot_thickness - 2.0f) < 0.0001f, "v11 crosshair keeps default outline and dot thickness") && ok;

    {
        std::ofstream v12_no_opacity("build/self-test-settings.cfg");
        v12_no_opacity << "version 12\n";
        v12_no_opacity << "crosshair 8 3 2 1 2 1 4\n";
        v12_no_opacity << "selected_wall 0\n";
        v12_no_opacity << "wall_preset \"1W2T DYNAMIC\" 3 3 8 10 0.08 0.08 1 1.5 0 0.75 8 8 1 2 0 1\n";
    }
    Game v12_no_opacity_loaded;
    load_settings(v12_no_opacity_loaded);
    ok = self_test_check(v12_no_opacity_loaded.crosshair.outlines && v12_no_opacity_loaded.crosshair.center_dot && std::fabs(v12_no_opacity_loaded.crosshair.outline_thickness - 2.0f) < 0.0001f && std::fabs(v12_no_opacity_loaded.crosshair.center_dot_thickness - 4.0f) < 0.0001f, "v12 crosshair without opacity still loads outlines and center dot") && ok;
    ok = self_test_check(std::fabs(v12_no_opacity_loaded.crosshair.outline_opacity - 0.5f) < 0.0001f, "v12 crosshair without opacity defaults to 0.5") && ok;

    {
        Game playlist_save;
        ensure_presets(playlist_save);
        playlist_save.playlists = {
            {"WARMUP", {"1W2T DYNAMIC", "1W2T STRAFE", "MISSING TASK"}},
            {"TRACK SET", {"1W1TS DYNAMIC TRACKING"}},
        };
        playlist_save.selected_playlist = 1;
        save_settings(playlist_save);
        Game playlist_loaded;
        load_settings(playlist_loaded);
        ok = self_test_check(static_cast<int>(playlist_loaded.playlists.size()) == 2, "v11 playlists round-trip") && ok;
        ok = self_test_check(playlist_loaded.playlists[0].name == "WARMUP" && playlist_loaded.playlists[0].task_names.size() == 2 && playlist_loaded.playlists[0].task_names[0] == "1W2T DYNAMIC" && playlist_loaded.playlists[0].task_names[1] == "1W2T STRAFE", "missing playlist tasks are dropped and kept tasks load in order") && ok;
        ok = self_test_check(playlist_loaded.selected_playlist == 1 && playlist_loaded.playlist_name == "TRACK SET", "selected playlist loads into the editor") && ok;
    }

    {
        Game rename_pl;
        ensure_presets(rename_pl);
        rename_pl.playlists = {{"WARMUP", {"1W2T DYNAMIC", "1W2T STRAFE"}}};
        int renamed = find_wall_preset(rename_pl, "1W2T DYNAMIC");
        ok = self_test_check(renamed >= 0, "rename test finds the dynamic preset") && ok;
        rename_pl.selected_wall_preset = renamed;
        apply_selected_presets(rename_pl);
        rename_pl.wall_preset_name = "RENAMED TASK";
        save_current_wall_preset(rename_pl);
        ok = self_test_check(rename_pl.playlists[0].task_names[0] == "RENAMED TASK" && rename_pl.playlists[0].task_names[1] == "1W2T STRAFE", "renaming a task updates playlist entries") && ok;
    }

    {
        Game delete_pl;
        ensure_presets(delete_pl);
        delete_pl.wall_presets.push_back({"MY TASK", WallClickSettings{}});
        delete_pl.playlists = {{"WARMUP", {"MY TASK", "1W2T DYNAMIC"}}};
        delete_pl.selected_wall_preset = static_cast<int>(delete_pl.wall_presets.size()) - 1;
        apply_selected_presets(delete_pl);
        delete_wall_preset(delete_pl);
        ok = self_test_check(delete_pl.playlists[0].task_names.size() == 1 && delete_pl.playlists[0].task_names[0] == "1W2T DYNAMIC", "deleting a task drops matching playlist entries") && ok;
    }

    {
        Game empty_play;
        ensure_presets(empty_play);
        init_scenarios(empty_play);
        ok = self_test_check(!start_playlist(empty_play) && empty_play.mode == AppMode::Menu, "play is a no-op on an empty playlist") && ok;
        empty_play.playlists = {{"EMPTY", {}}};
        empty_play.selected_playlist = 0;
        ok = self_test_check(!start_playlist(empty_play) && empty_play.mode == AppMode::Menu, "play is a no-op when the selected playlist has no tasks") && ok;
    }

    std::remove(g_settings_path_override.c_str());
    g_settings_path_override.clear();

    Game physics;
    physics.wall_settings.radius_min = 0.05f;
    physics.wall_settings.radius_max = 0.05f;
    physics.wall_settings.horizontal_speed_min = 1.0f;
    physics.wall_settings.horizontal_speed_max = 1.0f;
    physics.wall_settings.vertical_speed_min = 0.0f;
    physics.wall_settings.vertical_speed_max = 0.0f;
    physics.wall_settings.acceleration_min = 0.0f;
    physics.wall_settings.acceleration_max = 0.0f;
    normalize_settings(physics);
    physics.targets.push_back({{-0.5f, ROOM_EYE_HEIGHT, wall_z_from_distance(physics.wall_settings.wall_distance_max) + 0.45f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f, wall_to_units(physics.wall_settings.radius_min)});
    physics.targets.push_back({{0.5f, ROOM_EYE_HEIGHT, wall_z_from_distance(physics.wall_settings.wall_distance_max) + 0.45f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 1000.0f, wall_to_units(physics.wall_settings.radius_min)});
    float before_a = physics.targets[0].vel.x;
    float before_b = physics.targets[1].vel.x;
    update_wall_targets(physics, 1.0f / 120.0f);
    ok = self_test_check(std::fabs(physics.targets[0].vel.x - before_a) < 0.0001f && std::fabs(physics.targets[1].vel.x - before_b) < 0.0001f, "wall targets do not interact before visible contact") && ok;

    Game moving_axis;
    moving_axis.wall_settings.horizontal_speed_min = 0.0f;
    moving_axis.wall_settings.horizontal_speed_max = 0.0f;
    moving_axis.wall_settings.vertical_speed_min = 0.0f;
    moving_axis.wall_settings.vertical_speed_max = 2.0f;
    normalize_settings(moving_axis);
    for (int i = 0; i < 20; ++i) {
        Vec3 velocity = wall_desired_velocity(moving_axis);
        ok = self_test_check(std::fabs(velocity.x) < 0.0001f && std::fabs(velocity.y) > 0.0001f, "enabled wall movement axis cannot sample a still velocity") && ok;
    }

    Game collision;
    collision.wall_settings.radius_min = 0.25f;
    collision.wall_settings.radius_max = 0.25f;
    collision.wall_settings.horizontal_speed_min = 4.0f;
    collision.wall_settings.horizontal_speed_max = 4.0f;
    collision.wall_settings.vertical_speed_min = 0.0f;
    collision.wall_settings.vertical_speed_max = 0.0f;
    collision.wall_settings.acceleration_min = 0.0f;
    collision.wall_settings.acceleration_max = 0.0f;
    float collision_radius = wall_to_units(collision.wall_settings.radius_min);
    collision.targets.push_back({{-collision_radius * 0.88f, ROOM_EYE_HEIGHT, wall_z_from_distance(collision.wall_settings.wall_distance_max) + 0.45f}, {2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, 1000.0f, collision_radius});
    collision.targets.push_back({{collision_radius * 0.88f, ROOM_EYE_HEIGHT + 0.05f, wall_z_from_distance(collision.wall_settings.wall_distance_max) + 0.45f}, {-2.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}, 1000.0f, collision_radius});
    float collision_before_a = collision.targets[0].vel.x;
    float collision_before_b = collision.targets[1].vel.x;
    update_wall_targets(collision, 1.0f / 120.0f);
    ok = self_test_check(
        std::fabs(collision.targets[0].vel.x - collision_before_a) < 0.0001f &&
            std::fabs(collision.targets[1].vel.x - collision_before_b) < 0.0001f &&
            std::fabs(collision.targets[0].vel.y) < 0.0001f &&
            std::fabs(collision.targets[1].vel.y) < 0.0001f,
        "overlapping wall targets do not rewrite each other's movement"
    ) && ok;
    for (int i = 0; i < 12; ++i) {
        update_wall_targets(collision, 1.0f / 120.0f);
    }
    ok = self_test_check(
        collision.targets[0].vel.x > 0.0f &&
            collision.targets[1].vel.x < 0.0f &&
            std::fabs(collision.targets[0].vel.y) < 0.0001f &&
            std::fabs(collision.targets[1].vel.y) < 0.0001f,
        "overlapping wall targets do not become stuck together or coupled over time"
    ) && ok;

    Game wall_edge;
    wall_edge.wall_settings.radius_min = 0.10f;
    wall_edge.wall_settings.radius_max = 0.10f;
    wall_edge.wall_settings.horizontal_speed_min = 2.0f;
    wall_edge.wall_settings.horizontal_speed_max = 2.0f;
    wall_edge.wall_settings.vertical_speed_min = 0.0f;
    wall_edge.wall_settings.vertical_speed_max = 0.0f;
    wall_edge.wall_settings.acceleration_min = 0.0f;
    wall_edge.wall_settings.acceleration_max = 0.0f;
    normalize_settings(wall_edge);
    float edge_radius = wall_to_units(wall_edge.wall_settings.radius_min);
    float edge_distance = wall_edge.wall_settings.wall_distance_max;
    float edge_max_x = wall_width_for_distance(edge_distance) * 0.48f - edge_radius;
    wall_edge.targets.push_back({
        {edge_max_x - edge_radius * 0.5f, ROOM_EYE_HEIGHT, wall_z_from_distance(edge_distance) + 0.45f},
        {wall_to_units(2.0f), 0.0f, 0.0f},
        {wall_to_units(2.0f), 0.0f, 0.0f},
        1000.0f,
        edge_radius,
        0.0f,
        edge_distance
    });
    update_wall_targets(wall_edge, 1.0f / 120.0f);
    ok = self_test_check(
        wall_edge.targets[0].pos.x <= edge_max_x + 0.001f &&
            wall_edge.targets[0].vel.x < 0.0f &&
            wall_edge.targets[0].desired_vel.x < 0.0f,
        "wall targets steer inward before edge contact instead of bouncing off the wall"
    ) && ok;

    Game wall_contain;
    wall_contain.wall_settings.radius_min = 0.10f;
    wall_contain.wall_settings.radius_max = 0.10f;
    wall_contain.wall_settings.horizontal_speed_min = 2.0f;
    wall_contain.wall_settings.horizontal_speed_max = 2.0f;
    wall_contain.wall_settings.vertical_speed_min = 0.0f;
    wall_contain.wall_settings.vertical_speed_max = 0.0f;
    wall_contain.wall_settings.acceleration_min = 0.05f;
    wall_contain.wall_settings.acceleration_max = 0.05f;
    normalize_settings(wall_contain);
    float contain_radius = wall_to_units(wall_contain.wall_settings.radius_min);
    float contain_distance = wall_contain.wall_settings.wall_distance_max;
    float contain_max_x = wall_width_for_distance(contain_distance) * 0.48f - contain_radius;
    wall_contain.targets.push_back({
        {contain_max_x + contain_radius * 0.25f, ROOM_EYE_HEIGHT, wall_z_from_distance(contain_distance) + 0.45f},
        {wall_to_units(2.0f), 0.0f, 0.0f},
        {wall_to_units(2.0f), 0.0f, 0.0f},
        1000.0f,
        contain_radius,
        wall_to_units(0.05f),
        contain_distance
    });
    update_wall_targets(wall_contain, 1.0f / 120.0f);
    ok = self_test_check(
        wall_contain.targets[0].pos.x <= contain_max_x + 0.001f &&
            wall_contain.targets[0].vel.x < -0.0001f &&
            wall_contain.targets[0].desired_vel.x < -0.0001f,
        "wall boundary containment immediately restores inward movement"
    ) && ok;
    wall_contain.targets[0] = {
        {contain_max_x + contain_radius * 0.25f, ROOM_EYE_HEIGHT, wall_z_from_distance(contain_distance) + 0.45f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        1000.0f,
        contain_radius,
        wall_to_units(0.05f),
        contain_distance
    };
    update_wall_targets(wall_contain, 1.0f / 120.0f);
    ok = self_test_check(
        wall_contain.targets[0].vel.x < -0.0001f &&
            wall_contain.targets[0].desired_vel.x < -0.0001f,
        "wall boundary containment recovers from a zero desired velocity on enabled movement"
    ) && ok;

    Game wide_guard;
    wide_guard.wall_settings.radius_min = 0.10f;
    wide_guard.wall_settings.radius_max = 0.10f;
    wide_guard.wall_settings.horizontal_speed_min = 8.0f;
    wide_guard.wall_settings.horizontal_speed_max = 8.0f;
    wide_guard.wall_settings.vertical_speed_min = 0.0f;
    wide_guard.wall_settings.vertical_speed_max = 0.0f;
    wide_guard.wall_settings.acceleration_min = 0.05f;
    wide_guard.wall_settings.acceleration_max = 0.05f;
    normalize_settings(wide_guard);
    float wide_radius = wall_to_units(wide_guard.wall_settings.radius_min);
    float wide_distance = wide_guard.wall_settings.wall_distance_max;
    wide_guard.targets.push_back({
        {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(wide_distance) + 0.45f},
        {wall_to_units(8.0f), 0.0f, 0.0f},
        {wall_to_units(8.0f), 0.0f, 0.0f},
        1000.0f,
        wide_radius,
        wall_to_units(0.05f),
        wide_distance
    });
    update_wall_targets(wide_guard, 1.0f / 120.0f);
    ok = self_test_check(wide_guard.targets[0].desired_vel.x > 0.0f, "wall boundary guard does not couple edge steering to the center lane") && ok;

    Game large_vertical;
    large_vertical.wall_settings.radius_min = WALL_TARGET_RADIUS_MAX_M;
    large_vertical.wall_settings.radius_max = WALL_TARGET_RADIUS_MAX_M;
    large_vertical.wall_settings.horizontal_speed_min = 0.0f;
    large_vertical.wall_settings.horizontal_speed_max = 0.0f;
    large_vertical.wall_settings.vertical_speed_min = 2.0f;
    large_vertical.wall_settings.vertical_speed_max = 2.0f;
    large_vertical.wall_settings.acceleration_min = 0.05f;
    large_vertical.wall_settings.acceleration_max = 0.05f;
    normalize_settings(large_vertical);
    float large_radius = wall_to_units(large_vertical.wall_settings.radius_max);
    float large_distance = large_vertical.wall_settings.wall_distance_max;
    float large_min_y = wall_height_for_distance(large_distance) * 0.16f + large_radius;
    float large_max_y = wall_height_for_distance(large_distance) * 0.84f - large_radius;
    large_vertical.targets.push_back({
        {0.0f, (large_min_y + large_max_y) * 0.5f, wall_z_from_distance(large_distance) + 0.45f},
        {0.0f, wall_to_units(2.0f), 0.0f},
        {0.0f, wall_to_units(2.0f), 0.0f},
        1000.0f,
        large_radius,
        wall_to_units(0.05f),
        large_distance
    });
    update_wall_targets(large_vertical, 1.0f / 120.0f);
    ok = self_test_check(large_vertical.targets[0].desired_vel.y > 0.0f, "large vertical wall targets are not edge-steered in the center lane") && ok;

    // Targets on different depth planes must not collide even when they overlap on screen.
    Game cross_plane;
    cross_plane.wall_settings.radius_min = 0.10f;
    cross_plane.wall_settings.radius_max = 0.10f;
    cross_plane.wall_settings.horizontal_speed_min = 1.0f;
    cross_plane.wall_settings.horizontal_speed_max = 1.0f;
    cross_plane.wall_settings.vertical_speed_min = 0.0f;
    cross_plane.wall_settings.vertical_speed_max = 0.0f;
    cross_plane.wall_settings.acceleration_min = 0.0f;
    cross_plane.wall_settings.acceleration_max = 0.0f;
    float cross_radius = wall_to_units(0.10f);
    float near_distance = 4.0f;
    float far_distance = 12.0f;
    // Overlapping in x (within one radius) but on far-apart depth planes, closing on each other.
    cross_plane.targets.push_back({{0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(near_distance) + 0.45f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, 1000.0f, cross_radius, 0.0f, near_distance});
    cross_plane.targets.push_back({{cross_radius, ROOM_EYE_HEIGHT, wall_z_from_distance(far_distance) + 0.45f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, 1000.0f, cross_radius, 0.0f, far_distance});
    float cross_before_a = cross_plane.targets[0].vel.x;
    float cross_before_b = cross_plane.targets[1].vel.x;
    update_wall_targets(cross_plane, 1.0f / 120.0f);
    ok = self_test_check(std::fabs(cross_plane.targets[0].vel.x - cross_before_a) < 0.0001f && std::fabs(cross_plane.targets[1].vel.x - cross_before_b) < 0.0001f, "targets on different depth planes do not collide when overlapping on screen") && ok;

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
        float h = units_to_wall_meters(std::fabs(velocity.x));
        float v = units_to_wall_meters(std::fabs(velocity.y));
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
    float first_radius = units_to_wall_meters(movement_sampling.targets[0].radius);
    float first_accel = units_to_wall_meters(movement_sampling.targets[0].acceleration);
    for (const Target& target : movement_sampling.targets) {
        float radius_m = units_to_wall_meters(target.radius);
        float accel_m = units_to_wall_meters(target.acceleration);
        ok = self_test_check(radius_m >= 0.14f && radius_m <= 0.34f, "sampled wall target radius stays within range") && ok;
        ok = self_test_check(accel_m >= 5.0f && accel_m <= 15.0f, "sampled wall target acceleration stays within range") && ok;
        varied_radius = varied_radius || std::fabs(radius_m - first_radius) > 0.001f;
        varied_accel = varied_accel || std::fabs(accel_m - first_accel) > 0.001f;
    }
    ok = self_test_check(varied_radius, "wall target sizes vary when radius min and max differ") && ok;
    ok = self_test_check(varied_accel, "wall target acceleration varies when acceleration min and max differ") && ok;

    Game accel_reroll;
    accel_reroll.rng.seed(789);
    accel_reroll.wall_settings.radius_min = 0.08f;
    accel_reroll.wall_settings.radius_max = 0.08f;
    accel_reroll.wall_settings.horizontal_speed_min = 1.0f;
    accel_reroll.wall_settings.horizontal_speed_max = 1.0f;
    accel_reroll.wall_settings.vertical_speed_min = 0.0f;
    accel_reroll.wall_settings.vertical_speed_max = 0.0f;
    accel_reroll.wall_settings.acceleration_min = 5.0f;
    accel_reroll.wall_settings.acceleration_max = 15.0f;
    accel_reroll.wall_settings.change_min = 0.01f;
    accel_reroll.wall_settings.change_max = 0.01f;
    normalize_settings(accel_reroll);
    accel_reroll.targets.push_back(spawn_wall_target(accel_reroll));
    float first_live_accel = units_to_wall_meters(accel_reroll.targets[0].acceleration);
    bool varied_live_accel = false;
    for (int i = 0; i < 80; ++i) {
        accel_reroll.targets[0].change_timer = 0.0f;
        update_wall_targets(accel_reroll, 1.0f / 120.0f);
        float accel_m = units_to_wall_meters(accel_reroll.targets[0].acceleration);
        ok = self_test_check(accel_m >= 5.0f && accel_m <= 15.0f, "direction-change acceleration stays within range") && ok;
        varied_live_accel = varied_live_accel || std::fabs(accel_m - first_live_accel) > 0.001f;
    }
    ok = self_test_check(varied_live_accel, "direction change re-samples acceleration when min and max differ") && ok;

    Game fixed_accel;
    fixed_accel.rng.seed(790);
    fixed_accel.wall_settings.radius_min = 0.08f;
    fixed_accel.wall_settings.radius_max = 0.08f;
    fixed_accel.wall_settings.horizontal_speed_min = 1.0f;
    fixed_accel.wall_settings.horizontal_speed_max = 1.5f;
    fixed_accel.wall_settings.vertical_speed_min = 0.0f;
    fixed_accel.wall_settings.vertical_speed_max = 0.75f;
    fixed_accel.wall_settings.acceleration_min = 8.0f;
    fixed_accel.wall_settings.acceleration_max = 8.0f;
    fixed_accel.wall_settings.change_min = 0.01f;
    fixed_accel.wall_settings.change_max = 0.01f;
    normalize_settings(fixed_accel);
    fixed_accel.targets.push_back(spawn_wall_target(fixed_accel));
    for (int i = 0; i < 40; ++i) {
        fixed_accel.targets[0].change_timer = 0.0f;
        update_wall_targets(fixed_accel, 1.0f / 120.0f);
        ok = self_test_check(std::fabs(units_to_wall_meters(fixed_accel.targets[0].acceleration) - 8.0f) < 0.0001f, "fixed acceleration is unchanged across direction changes") && ok;
    }
    for (const WallPreset& preset : game.wall_presets) {
        ok = self_test_check(std::fabs(preset.settings.acceleration_min - preset.settings.acceleration_max) < 0.0001f, "default wall presets keep acceleration non-random") && ok;
    }

    Game distance_test;
    distance_test.wall_settings.wall_distance_max = 4.0f;
    float near_wall_z = wall_z_from_distance(distance_test.wall_settings.wall_distance_max);
    distance_test.wall_settings.wall_distance_max = 8.0f;
    float far_wall_z = wall_z_from_distance(distance_test.wall_settings.wall_distance_max);
    ok = self_test_check(far_wall_z < near_wall_z && std::fabs((near_wall_z - far_wall_z) - wall_to_units(4.0f)) < 0.001f, "wall distance moves wall by configured meters") && ok;
    float near_wall_width = wall_width_for_distance(4.0f);
    float far_wall_width = wall_width_for_distance(12.0f);
    float near_wall_height = wall_height_for_distance(4.0f);
    float far_wall_height = wall_height_for_distance(12.0f);
    ok = self_test_check(std::fabs(near_wall_width - far_wall_width) < 0.001f && std::fabs(near_wall_height - far_wall_height) < 0.001f, "wall distance keeps wall width and height fixed") && ok;
    float near_spawn_angle = std::atan((near_wall_width * 0.44f) / wall_to_units(4.0f));
    float far_spawn_angle = std::atan((far_wall_width * 0.44f) / wall_to_units(12.0f));
    ok = self_test_check(far_spawn_angle < near_spawn_angle, "farther wall distance reduces angular spawn area") && ok;

    Game wall_far_plane_test;
    init_scenarios(wall_far_plane_test);
    wall_far_plane_test.scenario = wall_far_plane_test.scenarios[0];
    wall_far_plane_test.wall_settings.wall_distance_max = 30.0f;
    normalize_settings(wall_far_plane_test);
    float far_width = wall_width_for_distance(wall_far_plane_test.wall_settings.wall_distance_max);
    float far_height = wall_height_for_distance(wall_far_plane_test.wall_settings.wall_distance_max);
    float far_radius = wall_to_units(wall_far_plane_test.wall_settings.radius_max);
    Vec3 wall_eye = camera_pos(wall_far_plane_test);
    float wall_required_far = std::sqrt(
        (far_width * 0.5f + far_radius) * (far_width * 0.5f + far_radius) +
        (std::fabs(far_height - wall_eye.y) + far_radius) * (std::fabs(far_height - wall_eye.y) + far_radius) +
        (std::fabs(wall_z_from_distance(wall_far_plane_test.wall_settings.wall_distance_max) - far_radius - wall_eye.z)) *
            (std::fabs(wall_z_from_distance(wall_far_plane_test.wall_settings.wall_distance_max) - far_radius - wall_eye.z))
    );
    ok = self_test_check(scene_far_plane(wall_far_plane_test) > wall_required_far, "far wall distance stays inside far clipping plane") && ok;

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

    // Wall targets sample a depth in the configured distance range.
    Game wall_depth;
    wall_depth.rng.seed(515);
    wall_depth.wall_settings.wall_distance_min = 4.0f;
    wall_depth.wall_settings.wall_distance_max = 12.0f;
    wall_depth.wall_settings.radius_min = 0.10f;
    wall_depth.wall_settings.radius_max = 0.10f;
    normalize_settings(wall_depth);
    bool varied_distance = false;
    bool depth_in_range = true;
    float first_distance = -1.0f;
    for (int i = 0; i < 24; ++i) {
        Target target = spawn_wall_target(wall_depth);
        if (i == 0) {
            first_distance = target.distance;
        }
        depth_in_range = depth_in_range && target.distance >= 4.0f - 0.001f && target.distance <= 12.0f + 0.001f;
        varied_distance = varied_distance || std::fabs(target.distance - first_distance) > 0.01f;
    }
    ok = self_test_check(depth_in_range, "wall target depth stays within the configured distance range") && ok;
    ok = self_test_check(varied_distance, "wall target depth varies when min and max distance differ") && ok;

    Game wall_flat;
    wall_flat.wall_settings.wall_distance_min = 7.0f;
    wall_flat.wall_settings.wall_distance_max = 7.0f;
    normalize_settings(wall_flat);
    Target flat_target = spawn_wall_target(wall_flat);
    ok = self_test_check(std::fabs(flat_target.distance - 7.0f) < 0.001f, "equal min and max distance spawns all targets on one plane") && ok;

    Game center_spawn;
    center_spawn.rng.seed(616);
    center_spawn.wall_settings.task_mode = TaskMode::Tracking;
    center_spawn.wall_settings.target_health = 0;
    center_spawn.wall_settings.target_count_min = 1;
    center_spawn.wall_settings.target_count_max = 1;
    center_spawn.wall_settings.wall_distance_min = 4.0f;
    center_spawn.wall_settings.wall_distance_max = 12.0f;
    center_spawn.wall_settings.radius_min = 0.10f;
    center_spawn.wall_settings.radius_max = 0.10f;
    normalize_settings(center_spawn);
    float center_radius = wall_to_units(0.10f);
    float center_distance = 8.0f;
    float center_height = wall_height_for_distance(center_distance);
    float expected_x = 0.0f;
    float expected_y = (center_height * 0.16f + center_radius + center_height * 0.84f - center_radius) * 0.5f;
    bool center_stable = true;
    for (int i = 0; i < 16; ++i) {
        Target target = spawn_wall_target(center_spawn);
        center_stable = center_stable && std::fabs(target.distance - center_distance) < 0.001f && std::fabs(target.pos.x - expected_x) < 0.001f && std::fabs(target.pos.y - expected_y) < 0.001f;
    }
    ok = self_test_check(center_stable, "single infinite tracking target spawns at the center of the spawn rectangle") && ok;

    Game switch_spawn = center_spawn;
    switch_spawn.rng.seed(617);
    switch_spawn.wall_settings.target_health = 10;
    switch_spawn.wall_settings.target_count_min = 2;
    switch_spawn.wall_settings.target_count_max = 2;
    normalize_settings(switch_spawn);
    bool switch_varied = false;
    Target switch_first = spawn_wall_target(switch_spawn);
    for (int i = 0; i < 24; ++i) {
        Target target = spawn_wall_target(switch_spawn);
        switch_varied = switch_varied || std::fabs(target.pos.x - switch_first.pos.x) > 0.01f || std::fabs(target.pos.y - switch_first.pos.y) > 0.01f || std::fabs(target.distance - switch_first.distance) > 0.01f;
    }
    ok = self_test_check(switch_varied, "target-switching spawns randomly like clicking") && ok;

    Game click_one = center_spawn;
    click_one.rng.seed(618);
    click_one.wall_settings.task_mode = TaskMode::Clicking;
    click_one.wall_settings.target_health = 0;
    click_one.wall_settings.target_count_min = 1;
    click_one.wall_settings.target_count_max = 1;
    normalize_settings(click_one);
    bool click_one_varied = false;
    Target click_one_first = spawn_wall_target(click_one);
    for (int i = 0; i < 24; ++i) {
        Target target = spawn_wall_target(click_one);
        click_one_varied = click_one_varied || std::fabs(target.pos.x - click_one_first.pos.x) > 0.01f || std::fabs(target.pos.y - click_one_first.pos.y) > 0.01f;
    }
    ok = self_test_check(click_one_varied, "clicking does not use the tracking center spawn") && ok;

    Game hit_sound_test;
    init_scenarios(hit_sound_test);
    hit_sound_test.wall_settings.target_count_min = 1;
    hit_sound_test.wall_settings.target_count_max = 1;
    hit_sound_test.wall_settings.radius_min = 0.4f;
    hit_sound_test.wall_settings.radius_max = 0.4f;
    hit_sound_test.wall_settings.horizontal_speed_min = 0.0f;
    hit_sound_test.wall_settings.horizontal_speed_max = 0.0f;
    hit_sound_test.wall_settings.vertical_speed_min = 0.0f;
    hit_sound_test.wall_settings.vertical_speed_max = 0.0f;
    hit_sound_test.wall_settings.acceleration_min = 0.0f;
    hit_sound_test.wall_settings.acceleration_max = 0.0f;
    normalize_settings(hit_sound_test);
    start_scenario(hit_sound_test, hit_sound_test.scenarios[0], RunMode::Practice);
    float hit_distance = hit_sound_test.wall_settings.wall_distance_max;
    hit_sound_test.targets = {{
        {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(hit_distance) + 0.45f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        1000.0f,
        wall_to_units(hit_sound_test.wall_settings.radius_min),
        0.0f,
        hit_distance,
    }};
    Input hit_click;
    hit_click.left_pressed = true;
    update_playing(hit_sound_test, hit_click, 1.0f / 120.0f);
    ok = self_test_check(hit_sound_test.stats.hits == 1 && hit_sound_test.pending_hit_sounds == 1, "wall hit queues one hit sound event") && ok;

    {
        Game space_click;
        init_scenarios(space_click);
        space_click.wall_settings.task_mode = TaskMode::Clicking;
        space_click.wall_settings.target_count_min = 1;
        space_click.wall_settings.target_count_max = 1;
        space_click.wall_settings.radius_min = 0.4f;
        space_click.wall_settings.radius_max = 0.4f;
        space_click.wall_settings.horizontal_speed_min = 0.0f;
        space_click.wall_settings.horizontal_speed_max = 0.0f;
        space_click.wall_settings.vertical_speed_min = 0.0f;
        space_click.wall_settings.vertical_speed_max = 0.0f;
        normalize_settings(space_click);
        start_scenario(space_click, space_click.scenarios[0], RunMode::Practice);
        float dist = space_click.wall_settings.wall_distance_max;
        space_click.targets = {{
            {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(dist) + 0.45f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            1000.0f,
            wall_to_units(space_click.wall_settings.radius_min),
            0.0f,
            dist,
        }};
        Input space;
        space.space_pressed = true;
        update_playing(space_click, space, 1.0f / 120.0f);
        ok = self_test_check(space_click.stats.shots == 1 && space_click.stats.hits == 1, "space fires a clicking shot") && ok;

        Game space_track;
        init_scenarios(space_track);
        space_track.wall_settings.task_mode = TaskMode::Tracking;
        space_track.wall_settings.target_health = 0;
        space_track.wall_settings.target_count_min = 1;
        space_track.wall_settings.target_count_max = 1;
        normalize_settings(space_track);
        start_scenario(space_track, space_track.scenarios[0], RunMode::Practice);
        Input hold_space;
        hold_space.space_down = true;
        update_playing(space_track, hold_space, 1.0f / TRACKING_FIRE_HZ);
        ok = self_test_check(space_track.stats.tracking_fire_time > 0.0f, "holding space fires in tracking practice") && ok;
        Input both;
        both.left_pressed = true;
        both.space_pressed = true;
        Game both_click = space_click;
        both_click.stats = {};
        update_playing(both_click, both, 1.0f / 120.0f);
        ok = self_test_check(both_click.stats.shots == 1, "space and mouse1 in the same frame still fire one clicking shot") && ok;
    }


    // Tracking health: 0 is infinite; a positive value respawns after that many hits.
    {
        Game health_inf;
        init_scenarios(health_inf);
        health_inf.wall_settings.task_mode = TaskMode::Tracking;
        health_inf.wall_settings.target_health = 0;
        health_inf.wall_settings.target_count_min = 1;
        health_inf.wall_settings.target_count_max = 1;
        health_inf.wall_settings.horizontal_speed_min = 0.0f;
        health_inf.wall_settings.horizontal_speed_max = 0.0f;
        health_inf.wall_settings.vertical_speed_min = 0.0f;
        health_inf.wall_settings.vertical_speed_max = 0.0f;
        normalize_settings(health_inf);
        start_scenario(health_inf, health_inf.scenarios[0], RunMode::Challenge);
        float dist = health_inf.wall_settings.wall_distance_max;
        health_inf.targets = {{
            {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(dist) + 0.45f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            1000.0f,
            wall_to_units(health_inf.wall_settings.radius_min),
            0.0f,
            dist,
            0,
        }};
        Vec3 start_pos = health_inf.targets[0].pos;
        Input none;
        for (int i = 0; i < 60; ++i) {
            update_playing(health_inf, none, 1.0f / TRACKING_FIRE_HZ);
        }
        ok = self_test_check(health_inf.stats.hits >= 50 && health_inf.targets.size() == 1, "infinite tracking health keeps scoring hits") && ok;
        ok = self_test_check(std::fabs(health_inf.targets[0].pos.x - start_pos.x) < 0.0001f && std::fabs(health_inf.targets[0].pos.y - start_pos.y) < 0.0001f, "infinite tracking health does not respawn the target") && ok;

        Game health_finite;
        init_scenarios(health_finite);
        health_finite.wall_settings.task_mode = TaskMode::Tracking;
        health_finite.wall_settings.target_health = 3;
        health_finite.wall_settings.target_count_min = 1;
        health_finite.wall_settings.target_count_max = 1;
        health_finite.wall_settings.horizontal_speed_min = 0.0f;
        health_finite.wall_settings.horizontal_speed_max = 0.0f;
        health_finite.wall_settings.vertical_speed_min = 0.0f;
        health_finite.wall_settings.vertical_speed_max = 0.0f;
        health_finite.wall_settings.radius_min = 0.4f;
        health_finite.wall_settings.radius_max = 0.4f;
        normalize_settings(health_finite);
        start_scenario(health_finite, health_finite.scenarios[0], RunMode::Challenge);
        dist = health_finite.wall_settings.wall_distance_max;
        health_finite.targets = {{
            {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(dist) + 0.45f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            1000.0f,
            wall_to_units(0.4f),
            0.0f,
            dist,
            3,
        }};
        Vec3 first_pos = health_finite.targets[0].pos;
        for (int i = 0; i < 2; ++i) {
            update_playing(health_finite, none, 1.0f / TRACKING_FIRE_HZ);
        }
        ok = self_test_check(health_finite.targets[0].health == 1 && std::fabs(health_finite.targets[0].pos.x - first_pos.x) < 0.0001f, "finite tracking health survives until depleted") && ok;
        update_playing(health_finite, none, 1.0f / TRACKING_FIRE_HZ);
        ok = self_test_check(health_finite.targets.size() == 1 && health_finite.targets[0].health == 3, "finite tracking health respawns at full health") && ok;
    }

    // Clicking health: 1 is one-shot; N requires N clicks; 0 never despawns.
    {
        auto place_click_target = [](Game& g, int health) {
            float dist = g.wall_settings.wall_distance_max;
            g.targets = {{
                {0.0f, ROOM_EYE_HEIGHT, wall_z_from_distance(dist) + 0.45f},
                {0.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 0.0f},
                1000.0f,
                wall_to_units(g.wall_settings.radius_min),
                0.0f,
                dist,
                health,
            }};
        };

        Game click_hp;
        init_scenarios(click_hp);
        click_hp.wall_settings.task_mode = TaskMode::Clicking;
        click_hp.wall_settings.target_health = 3;
        click_hp.wall_settings.target_count_min = 1;
        click_hp.wall_settings.target_count_max = 1;
        click_hp.wall_settings.horizontal_speed_min = 0.0f;
        click_hp.wall_settings.horizontal_speed_max = 0.0f;
        click_hp.wall_settings.vertical_speed_min = 0.0f;
        click_hp.wall_settings.vertical_speed_max = 0.0f;
        click_hp.wall_settings.radius_min = 0.4f;
        click_hp.wall_settings.radius_max = 0.4f;
        normalize_settings(click_hp);
        start_scenario(click_hp, click_hp.scenarios[0], RunMode::Practice);
        place_click_target(click_hp, 3);
        Vec3 click_pos = click_hp.targets[0].pos;
        Input click;
        click.left_pressed = true;
        Input idle;
        update_playing(click_hp, click, 1.0f / 120.0f);
        update_playing(click_hp, idle, 1.0f / 120.0f);
        update_playing(click_hp, click, 1.0f / 120.0f);
        ok = self_test_check(click_hp.stats.hits == 2 && click_hp.targets[0].health == 1, "clicking health survives until depleted") && ok;
        ok = self_test_check(std::fabs(click_hp.targets[0].pos.x - click_pos.x) < 0.0001f && std::fabs(click_hp.targets[0].pos.y - click_pos.y) < 0.0001f, "clicking health does not respawn before the last shot") && ok;
        update_playing(click_hp, idle, 1.0f / 120.0f);
        update_playing(click_hp, click, 1.0f / 120.0f);
        ok = self_test_check(click_hp.targets.size() == 1 && click_hp.targets[0].health == 3, "clicking health respawns at full health") && ok;

        Game click_inf;
        init_scenarios(click_inf);
        click_inf.wall_settings.task_mode = TaskMode::Clicking;
        click_inf.wall_settings.target_health = 0;
        click_inf.wall_settings.target_count_min = 1;
        click_inf.wall_settings.target_count_max = 1;
        click_inf.wall_settings.horizontal_speed_min = 0.0f;
        click_inf.wall_settings.horizontal_speed_max = 0.0f;
        click_inf.wall_settings.vertical_speed_min = 0.0f;
        click_inf.wall_settings.vertical_speed_max = 0.0f;
        click_inf.wall_settings.radius_min = 0.4f;
        click_inf.wall_settings.radius_max = 0.4f;
        normalize_settings(click_inf);
        start_scenario(click_inf, click_inf.scenarios[0], RunMode::Practice);
        place_click_target(click_inf, 0);
        Vec3 inf_pos = click_inf.targets[0].pos;
        for (int i = 0; i < 8; ++i) {
            update_playing(click_inf, click, 1.0f / 120.0f);
            update_playing(click_inf, idle, 1.0f / 120.0f);
        }
        ok = self_test_check(click_inf.stats.hits == 8 && click_inf.targets.size() == 1, "infinite clicking health keeps scoring hits") && ok;
        ok = self_test_check(std::fabs(click_inf.targets[0].pos.x - inf_pos.x) < 0.0001f && std::fabs(click_inf.targets[0].pos.y - inf_pos.y) < 0.0001f, "infinite clicking health does not respawn the target") && ok;
    }

    // Challenge mode + run persistence.
    {
        g_runs_path_override = "build/self-test-runs.cfg";
        std::remove(g_runs_path_override.c_str());

        Game ch;
        ch.rng.seed(99);
        init_scenarios(ch);
        ch.wall_preset_name = "TEST TRACK";
        ch.wall_settings.task_mode = TaskMode::Tracking;
        ch.wall_settings.target_health = 0;
        start_scenario(ch, ch.scenarios[0], RunMode::Challenge);
        ok = self_test_check(ch.run_mode == RunMode::Challenge && std::fabs(ch.challenge_time_left - CHALLENGE_DURATION_SEC) < 0.0001f, "challenge starts with the full time budget") && ok;

        Input none;
        int guard = 0;
        while (ch.mode == AppMode::Playing && guard < 100000) {
            update_playing(ch, none, 1.0f / 120.0f);
            ++guard;
        }
        ok = self_test_check(ch.mode == AppMode::Results, "challenge ends and shows results") && ok;
        // ~20 shots/sec across 30s, allowing for float drift on the final tick.
        // ~20 shots/sec across CHALLENGE_DURATION_SEC (60s -> ~1200), allowing for float drift.
        int expected_shots = static_cast<int>(TRACKING_FIRE_HZ * CHALLENGE_DURATION_SEC);
        ok = self_test_check(ch.stats.shots >= expected_shots - 5 && ch.stats.shots <= expected_shots + 1, "tracking challenge auto-fires at ~20Hz") && ok;
        ok = self_test_check(ch.last_run.score == ch.stats.hits, "challenge score equals hits") && ok;
        float expected_acc = ch.stats.shots > 0 ? static_cast<float>(ch.stats.hits) / static_cast<float>(ch.stats.shots) * 100.0f : 0.0f;
        ok = self_test_check(std::fabs(ch.last_run.accuracy - expected_acc) < 0.001f, "challenge records accuracy separately from the score") && ok;
        ok = self_test_check(ch.last_run.kind == ScenarioKind::Tracking && ch.last_run.preset_name == "TEST TRACK", "run records scenario kind and preset name") && ok;
        ok = self_test_check(static_cast<int>(ch.runs.size()) == 1, "finished challenge is appended to the run history") && ok;

        ok = self_test_check(best_run_score(ch, ScenarioKind::Tracking, "TEST TRACK") == ch.last_run.score, "best_run_score returns the recorded score") && ok;
        ok = self_test_check(best_run_score(ch, ScenarioKind::WallClick, "TEST TRACK") == -1, "best_run_score is -1 for an unplayed scenario") && ok;

        Game reloaded;
        load_runs(reloaded);
        ok = self_test_check(static_cast<int>(reloaded.runs.size()) == 1, "saved runs reload from disk") && ok;
        ok = self_test_check(reloaded.runs[0].kind == ScenarioKind::Tracking && reloaded.runs[0].preset_name == "TEST TRACK" && reloaded.runs[0].score == ch.last_run.score, "reloaded run preserves kind, preset, and score") && ok;

        Game pr;
        pr.rng.seed(7);
        init_scenarios(pr);
        pr.wall_settings.task_mode = TaskMode::Tracking;
        start_scenario(pr, pr.scenarios[0], RunMode::Practice);
        for (int i = 0; i < 240; ++i) {
            update_playing(pr, none, 1.0f / 120.0f);
        }
        ok = self_test_check(pr.mode == AppMode::Playing && pr.stats.shots == 0, "practice tracking neither auto-fires nor times out") && ok;

        std::remove(g_runs_path_override.c_str());
        g_runs_path_override.clear();
    }

    {
        g_runs_path_override = "build/self-test-runs.cfg";
        std::remove(g_runs_path_override.c_str());

        Game pl;
        pl.rng.seed(3);
        ensure_presets(pl);
        init_scenarios(pl);
        int first = find_wall_preset(pl, "1W2T DYNAMIC");
        int second = find_wall_preset(pl, "1W2T STRAFE");
        ok = self_test_check(first >= 0 && second >= 0, "playlist play test finds two default tasks") && ok;
        pl.playlists = {{"WARMUP", {pl.wall_presets[first].name, pl.wall_presets[second].name}}};
        pl.selected_playlist = 0;
        ok = self_test_check(start_playlist(pl), "start_playlist begins the first task") && ok;
        ok = self_test_check(pl.mode == AppMode::Playing && pl.run_mode == RunMode::Challenge && pl.wall_preset_name == "1W2T DYNAMIC", "playlist starts a challenge on the first task") && ok;
        ok = self_test_check(pl.playlist_active && !pl.playlist_complete && pl.playlist_play_index == 0, "playlist session tracks the first task") && ok;

        Input none;
        pl.challenge_time_left = 0.0f;
        update_playing(pl, none, 1.0f / 120.0f);
        ok = self_test_check(pl.mode == AppMode::Results && !pl.playlist_complete, "first playlist task shows mid-playlist results") && ok;
        ok = self_test_check(static_cast<int>(pl.playlist_session_runs.size()) == 1, "first playlist task is recorded in the session") && ok;

        handle_results_continue(pl);
        ok = self_test_check(pl.mode == AppMode::Playing && pl.wall_preset_name == "1W2T STRAFE" && pl.playlist_play_index == 1, "continuing a playlist starts the next task") && ok;

        pl.challenge_time_left = 0.0f;
        update_playing(pl, none, 1.0f / 120.0f);
        ok = self_test_check(pl.mode == AppMode::Results && pl.playlist_complete, "last playlist task shows the playlist summary") && ok;
        ok = self_test_check(static_cast<int>(pl.playlist_session_runs.size()) == 2, "playlist session records every finished task") && ok;

        handle_results_continue(pl);
        ok = self_test_check(pl.mode == AppMode::Menu && !pl.playlist_active && !pl.playlist_paused, "finishing a playlist returns to the menu") && ok;

        Game mid;
        mid.rng.seed(4);
        ensure_presets(mid);
        init_scenarios(mid);
        mid.playlists = {{"WARMUP", {mid.wall_presets[first].name, mid.wall_presets[second].name}}};
        mid.selected_playlist = 0;
        start_playlist(mid);
        mid.challenge_time_left = 0.0f;
        update_playing(mid, none, 1.0f / 120.0f);
        abort_to_menu(mid);
        ok = self_test_check(mid.mode == AppMode::Menu && !mid.playlist_active && mid.playlist_paused, "esc mid-playlist returns to the menu without starting the next task") && ok;
        ok = self_test_check(mid.playlist_play_index == 1 && static_cast<int>(mid.playlist_session_runs.size()) == 1, "esc from mid-playlist results keeps the finished task and advances to the next") && ok;
        ok = self_test_check(playlist_can_resume(mid), "esc mid-playlist leaves a resumable session") && ok;
        ok = self_test_check(resume_playlist(mid), "resume_playlist starts the unfinished task") && ok;
        ok = self_test_check(mid.mode == AppMode::Playing && mid.wall_preset_name == "1W2T STRAFE" && mid.playlist_play_index == 1, "resume continues from the next playlist task") && ok;
        ok = self_test_check(static_cast<int>(mid.playlist_session_runs.size()) == 1 && !mid.playlist_paused, "resume keeps prior session scores") && ok;

        Game from_task;
        from_task.rng.seed(5);
        ensure_presets(from_task);
        init_scenarios(from_task);
        from_task.playlists = {{"WARMUP", {from_task.wall_presets[first].name, from_task.wall_presets[second].name}}};
        from_task.selected_playlist = 0;
        ok = self_test_check(start_playlist(from_task, 1), "start_playlist can begin at a later entry") && ok;
        ok = self_test_check(from_task.mode == AppMode::Playing && from_task.wall_preset_name == "1W2T STRAFE" && from_task.playlist_play_index == 1, "playing from a later entry starts that task") && ok;

        Game abort_task;
        abort_task.rng.seed(6);
        ensure_presets(abort_task);
        init_scenarios(abort_task);
        abort_task.playlists = {{"WARMUP", {abort_task.wall_presets[first].name, abort_task.wall_presets[second].name}}};
        abort_task.selected_playlist = 0;
        start_playlist(abort_task);
        abort_to_menu(abort_task);
        ok = self_test_check(abort_task.playlist_paused && abort_task.playlist_play_index == 0 && abort_task.playlist_session_runs.empty(), "esc during a playlist task pauses on that unfinished task") && ok;
        ok = self_test_check(resume_playlist(abort_task) && abort_task.mode == AppMode::Playing && abort_task.wall_preset_name == "1W2T DYNAMIC", "resume after esc during a task restarts that task") && ok;
        abort_to_menu(abort_task);
        ok = self_test_check(start_playlist(abort_task, 0) && abort_task.playlist_play_index == 0 && abort_task.playlist_session_runs.empty() && !abort_task.playlist_paused, "play starts the playlist from the beginning and clears a pause") && ok;

        Game other;
        other.rng.seed(7);
        ensure_presets(other);
        init_scenarios(other);
        other.playlists = {
            {"WARMUP", {other.wall_presets[first].name, other.wall_presets[second].name}},
            {"OTHER", {other.wall_presets[first].name}},
        };
        other.selected_playlist = 0;
        start_playlist(other);
        abort_to_menu(other);
        other.selected_playlist = 1;
        apply_selected_playlist(other);
        ok = self_test_check(!playlist_can_resume(other), "resume is only offered for the paused playlist") && ok;
        ok = self_test_check(!resume_playlist(other) && other.playlist_paused, "resume is a no-op on a different playlist") && ok;
        start_playlist(other, 0);
        ok = self_test_check(other.mode == AppMode::Playing && !other.playlist_paused && other.playlist_play_name == "OTHER", "play starts a new session and clears the previous pause") && ok;

        std::remove(g_runs_path_override.c_str());
        g_runs_path_override.clear();
    }

    if (ok) {
        std::printf("SELF TEST PASSED\n");
        return 0;
    }
    return 1;
}
