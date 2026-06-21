#include "selftest.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>

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

    ok = self_test_check(std::string(glyph('[')[0]) != "00000" && std::string(glyph(']')[0]) != "00000", "menu font draws unit brackets") && ok;

    Game game;
    ensure_presets(game);
    apply_selected_presets(game);

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
    ok = self_test_check(game.active_field == FieldId::WallDistMin, "tab advances to the next field") && ok;
    Input tab_back;
    tab_back.tab_pressed = true;
    tab_back.shift_down = true;
    menu_handle_edit(game, tab_back);
    ok = self_test_check(game.active_field == FieldId::WallName, "shift+tab moves to the previous field") && ok;
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

    game.wall_presets.push_back({"CLICK PRESET 4", game.wall_settings});
    std::string unique_click = unique_preset_name(game.wall_presets, "CLICK PRESET 4", -1);
    ok = self_test_check(unique_click != "CLICK PRESET 4", "generated wall preset names avoid duplicates") && ok;

    game.wall_presets = {{"ONLY WALL", WallClickSettings{}}};
    game.selected_wall_preset = 0;
    game.active_field = FieldId::WallName;
    delete_wall_preset(game);
    ok = self_test_check(game.wall_presets.size() == 1 && game.wall_presets[0].name == "PASU FIVE", "deleting the last wall preset restores default") && ok;
    ok = self_test_check(game.active_field == FieldId::None, "delete wall preset exits text edit mode") && ok;

    game.pill_preset_name.clear();
    menu_focus_field(game, FieldId::PillName);
    Input pill_input;
    pill_input.text_input = "fast pill";
    menu_handle_edit(game, pill_input);
    ok = self_test_check(game.edit_draft == "FAST PILL", "pill preset name edit filters and uppercases") && ok;
    int old_pill_count = static_cast<int>(game.pill_presets.size());
    new_pill_preset(game);
    ok = self_test_check(static_cast<int>(game.pill_presets.size()) == old_pill_count + 1, "new pill preset appends exactly one preset") && ok;
    ok = self_test_check(game.active_field == FieldId::None, "new pill preset exits text edit mode") && ok;

    // Creating a preset must scroll it into the 7-row visible window.
    {
        Game scroll_test;
        for (int i = 0; i < 12; ++i) {
            scroll_test.wall_presets.push_back({"FILLER", WallClickSettings{}});
        }
        scroll_test.wall_preset_scroll = 0;
        new_wall_preset(scroll_test);
        int sel = scroll_test.selected_wall_preset;
        ok = self_test_check(sel >= scroll_test.wall_preset_scroll && sel <= scroll_test.wall_preset_scroll + 6, "new preset is scrolled into the visible list window") && ok;
    }

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
    game.wall_settings.wall_distance_min = 6.25f;
    game.wall_settings.radius_min = 0.18f;
    game.wall_settings.radius_max = 0.22f;
    save_current_wall_preset(game);
    game.sensitivity = 0.777f;
    game.crosshair = {14.0f, 6.0f, 3.0f};
    save_settings(game);

    Game loaded;
    load_settings(loaded);
    ok = self_test_check(std::fabs(loaded.sensitivity - 0.777f) < 0.0001f, "saved general sensitivity loads") && ok;
    ok = self_test_check(std::fabs(loaded.crosshair.length - 14.0f) < 0.0001f, "saved crosshair loads") && ok;
    ok = self_test_check(!loaded.wall_presets.empty(), "saved wall presets load") && ok;
    ok = self_test_check(loaded.wall_preset_name == "TINY PASU", "selected named wall preset loads into editor") && ok;
    ok = self_test_check(loaded.wall_settings.target_count_min == 8 && loaded.wall_settings.target_count_max == 8, "selected wall preset target count range loads") && ok;
    ok = self_test_check(std::fabs(loaded.wall_settings.wall_distance_min - 6.25f) < 0.0001f, "selected wall distance loads") && ok;
    ok = self_test_check(std::fabs(loaded.wall_settings.radius_max - 0.22f) < 0.0001f, "selected wall preset radius range loads") && ok;

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
    ok = self_test_check(actual_v2_loaded.wall_preset_name == "1W4T DYNAMIC", "actual v2 selected wall preset remains selected") && ok;
    ok = self_test_check(actual_v2_loaded.wall_settings.target_count_min == 4 && actual_v2_loaded.wall_settings.target_count_max == 4, "actual v2 wall target count is preserved") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.radius_min) - 0.24f) < 0.001f, "actual v2 wall radius preserves old internal size") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.horizontal_speed_min) - 6.0f) < 0.001f, "actual v2 wall horizontal speed preserves old internal speed") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.vertical_speed_min) - 2.0f) < 0.001f, "actual v2 wall vertical speed preserves old internal speed") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_loaded.wall_settings.acceleration_min) - 20.0f) < 0.001f, "actual v2 wall acceleration preserves old internal acceleration") && ok;
    ok = self_test_check(std::fabs(wall_z_from_distance(actual_v2_loaded.wall_settings.wall_distance_max) - ROOM_WALL_Z) < 0.001f, "actual v2 wall distance defaults to old wall plane") && ok;
    ok = self_test_check(actual_v2_loaded.pill_preset_name == "SMOOTH PILL", "actual v2 selected pill preset remains selected") && ok;
    ok = self_test_check(std::fabs(tracking_to_units(actual_v2_loaded.pill_settings.width) - 1.24f) < 0.001f, "actual v2 pill width preserves old internal size") && ok;
    ok = self_test_check(std::fabs(tracking_to_units(actual_v2_loaded.pill_settings.speed) - 4.0f) < 0.001f, "actual v2 pill speed preserves old internal speed") && ok;
    ok = self_test_check(std::fabs(tracking_to_units(actual_v2_loaded.pill_settings.acceleration) - 12.0f) < 0.001f, "actual v2 pill acceleration preserves old internal acceleration") && ok;
    ok = self_test_check(std::fabs(tracking_to_units(actual_v2_loaded.pill_settings.distance_min) - 7.5f) < 0.001f && std::fabs(tracking_to_units(actual_v2_loaded.pill_settings.distance_max) - 10.5f) < 0.001f, "actual v2 pill distance defaults preserve old spawn band") && ok;
    save_settings(actual_v2_loaded);
    Game actual_v2_roundtrip;
    load_settings(actual_v2_roundtrip);
    ok = self_test_check(actual_v2_roundtrip.wall_preset_name == "1W4T DYNAMIC" && actual_v2_roundtrip.pill_preset_name == "SMOOTH PILL", "actual v2 save round trip preserves selected presets") && ok;
    ok = self_test_check(std::fabs(wall_to_units(actual_v2_roundtrip.wall_settings.radius_min) - 0.24f) < 0.001f && std::fabs(wall_to_units(actual_v2_roundtrip.wall_settings.horizontal_speed_min) - 6.0f) < 0.001f, "actual v2 save round trip preserves wall behavior") && ok;
    ok = self_test_check(std::fabs(tracking_to_units(actual_v2_roundtrip.pill_settings.width) - 1.24f) < 0.001f && std::fabs(tracking_to_units(actual_v2_roundtrip.pill_settings.speed) - 4.0f) < 0.001f, "actual v2 save round trip preserves pill behavior") && ok;

    {
        std::ofstream high_accel("build/self-test-settings.cfg");
        high_accel << "version 2\n";
        high_accel << "pill_preset \"HIGH ACCEL\" 1.24 4 80 0.35 2.4\n";
    }
    Game high_accel_loaded;
    load_settings(high_accel_loaded);
    ok = self_test_check(std::fabs(tracking_to_units(high_accel_loaded.pill_settings.acceleration) - 80.0f) < 0.001f, "old high pill acceleration is preserved on load") && ok;
    save_settings(high_accel_loaded);
    Game high_accel_roundtrip;
    load_settings(high_accel_roundtrip);
    ok = self_test_check(std::fabs(tracking_to_units(high_accel_roundtrip.pill_settings.acceleration) - 80.0f) < 0.001f, "old high pill acceleration is preserved after save round trip") && ok;

    {
        std::ofstream high_speed("build/self-test-settings.cfg");
        high_speed << "version 2\n";
        high_speed << "pill_preset \"HIGH SPEED\" 1.24 14 12 0.35 2.4\n";
    }
    Game high_speed_loaded;
    load_settings(high_speed_loaded);
    ok = self_test_check(std::fabs(tracking_to_units(high_speed_loaded.pill_settings.speed) - 14.0f) < 0.001f, "old high pill speed is preserved on load") && ok;
    save_settings(high_speed_loaded);
    Game high_speed_roundtrip;
    load_settings(high_speed_roundtrip);
    ok = self_test_check(std::fabs(tracking_to_units(high_speed_roundtrip.pill_settings.speed) - 14.0f) < 0.001f, "old high pill speed is preserved after save round trip") && ok;
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
    update_wall_targets(collision, 1.0f / 120.0f);
    ok = self_test_check(std::fabs(collision.targets[0].vel.y) < 0.0001f && std::fabs(collision.targets[1].vel.y) < 0.0001f, "horizontal-only wall collisions do not create vertical movement") && ok;

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

    Game distance_test;
    distance_test.wall_settings.wall_distance_max = 4.0f;
    float near_wall_z = wall_z_from_distance(distance_test.wall_settings.wall_distance_max);
    distance_test.wall_settings.wall_distance_max = 8.0f;
    float far_wall_z = wall_z_from_distance(distance_test.wall_settings.wall_distance_max);
    ok = self_test_check(far_wall_z < near_wall_z && std::fabs((near_wall_z - far_wall_z) - wall_to_units(4.0f)) < 0.001f, "wall distance moves wall by configured meters") && ok;

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

    Game pill_distance_test;
    pill_distance_test.rng.seed(321);
    init_scenarios(pill_distance_test);
    pill_distance_test.scenario = pill_distance_test.scenarios[1];
    pill_distance_test.pill_settings.distance_min = 4.0f;
    pill_distance_test.pill_settings.distance_max = 8.0f;
    normalize_settings(pill_distance_test);
    float pill_room_limit = tracking_room_half_size(pill_distance_test.pill_settings) - tracking_to_units(1.0f) - tracking_to_units(pill_distance_test.pill_settings.width) * 0.5f;
    ok = self_test_check(pill_room_limit >= tracking_to_units(pill_distance_test.pill_settings.distance_max) - 0.001f, "tracking room boundary allows configured max pill distance") && ok;
    ok = self_test_check(tracking_room_half_size(pill_distance_test.pill_settings) * 2.0f >= tracking_to_units(8.0f) * TRACKING_ROOM_SIDE_SCALE - 0.001f, "tracking room side length scales from max pill distance") && ok;
    Game close_room_test;
    close_room_test.pill_settings.distance_min = 1.5f;
    close_room_test.pill_settings.distance_max = 4.0f;
    normalize_settings(close_room_test);
    float close_room_limit = tracking_room_half_size(close_room_test.pill_settings) - tracking_to_units(1.0f) - tracking_to_units(close_room_test.pill_settings.width) * 0.5f;
    ok = self_test_check(close_room_limit >= tracking_to_units(close_room_test.pill_settings.distance_max) - 0.001f, "close tracking room boundary allows configured max pill distance") && ok;
    ok = self_test_check(scene_far_plane(pill_distance_test) >= 120.0f, "tracking far plane remains at least the default range") && ok;
    bool saw_near_pill_spawn = false;
    bool saw_far_pill_spawn = false;
    for (int i = 0; i < 80; ++i) {
        Target target = spawn_pill_target(pill_distance_test);
        float dist_m = units_to_tracking_meters(std::sqrt(target.pos.x * target.pos.x + target.pos.z * target.pos.z));
        ok = self_test_check(dist_m >= 4.0f && dist_m <= 8.0f, "pill spawn distance stays within configured range") && ok;
        saw_near_pill_spawn = saw_near_pill_spawn || dist_m < 5.0f;
        saw_far_pill_spawn = saw_far_pill_spawn || dist_m > 7.0f;
    }
    ok = self_test_check(saw_near_pill_spawn && saw_far_pill_spawn, "pill spawn samples across configured distance range") && ok;
    Vec3 outside_pos{tracking_to_units(8.2f), PLANE_EYE_HEIGHT, 0.0f};
    Vec3 outside_vel = pill_desired_velocity_for_position(pill_distance_test, outside_pos);
    ok = self_test_check(dot(outside_vel, {outside_pos.x, 0.0f, outside_pos.z}) < 0.0f, "pill direction near max distance points inward") && ok;
    Vec3 inside_pos{tracking_to_units(3.9f), PLANE_EYE_HEIGHT, 0.0f};
    Vec3 inside_vel = pill_desired_velocity_for_position(pill_distance_test, inside_pos);
    ok = self_test_check(dot(inside_vel, {inside_pos.x, 0.0f, inside_pos.z}) > 0.0f, "pill direction near min distance points outward") && ok;

    Game narrow_band_test;
    narrow_band_test.rng.seed(44);
    narrow_band_test.pill_settings.distance_min = 4.0f;
    narrow_band_test.pill_settings.distance_max = 4.0f;
    normalize_settings(narrow_band_test);
    Vec3 narrow_outer_pos{tracking_to_units(4.05f), PLANE_EYE_HEIGHT, 0.0f};
    Vec3 narrow_outer_vel = pill_desired_velocity_for_position(narrow_band_test, narrow_outer_pos);
    ok = self_test_check(dot(narrow_outer_vel, {narrow_outer_pos.x, 0.0f, narrow_outer_pos.z}) < 0.0f, "narrow pill band steers inward outside fixed radius") && ok;
    Vec3 narrow_inner_pos{tracking_to_units(3.95f), PLANE_EYE_HEIGHT, 0.0f};
    Vec3 narrow_inner_vel = pill_desired_velocity_for_position(narrow_band_test, narrow_inner_pos);
    ok = self_test_check(dot(narrow_inner_vel, {narrow_inner_pos.x, 0.0f, narrow_inner_pos.z}) > 0.0f, "narrow pill band steers outward inside fixed radius") && ok;
    Vec3 narrow_exact_pos{tracking_to_units(4.0f), PLANE_EYE_HEIGHT, 0.0f};
    Vec3 narrow_exact_vel = pill_desired_velocity_for_position(narrow_band_test, narrow_exact_pos);
    ok = self_test_check(std::fabs(dot(narrow_exact_vel, {narrow_exact_pos.x, 0.0f, narrow_exact_pos.z})) < 0.001f, "fixed-radius pill band chooses tangential movement at boundary") && ok;

    Game close_distance_test;
    close_distance_test.pill_settings.distance_min = 1.5f;
    close_distance_test.pill_settings.distance_max = 4.0f;
    close_distance_test.pill_settings.speed = 0.5f;
    close_distance_test.pill_settings.acceleration = 20.0f;
    normalize_settings(close_distance_test);
    close_distance_test.targets.push_back({
        {tracking_to_units(1.6f), PLANE_EYE_HEIGHT, 0.0f},
        {-tracking_to_units(0.4f), 0.0f, 0.0f},
        {-tracking_to_units(0.4f), 0.0f, 0.0f},
        10.0f,
        tracking_to_units(close_distance_test.pill_settings.width) * 0.5f
    });
    update_pill_target(close_distance_test, 1.0f / 120.0f);
    ok = self_test_check(close_distance_test.targets[0].vel.x < 0.0f, "pill close distance uses configured min without hidden camera bounce") && ok;

    Game radial_max_test;
    radial_max_test.pill_settings.distance_min = 2.0f;
    radial_max_test.pill_settings.distance_max = 4.0f;
    radial_max_test.pill_settings.speed = 4.0f;
    radial_max_test.pill_settings.acceleration = 0.05f;
    normalize_settings(radial_max_test);
    radial_max_test.targets.push_back({
        {tracking_to_units(3.98f), PLANE_EYE_HEIGHT, 0.0f},
        {tracking_to_units(2.0f), 0.0f, 0.0f},
        {tracking_to_units(2.0f), 0.0f, 0.0f},
        10.0f,
        tracking_to_units(radial_max_test.pill_settings.width) * 0.5f
    });
    update_pill_target(radial_max_test, 1.0f);
    float radial_max_dist = units_to_tracking_meters(std::sqrt(radial_max_test.targets[0].pos.x * radial_max_test.targets[0].pos.x + radial_max_test.targets[0].pos.z * radial_max_test.targets[0].pos.z));
    ok = self_test_check(radial_max_dist <= 4.001f && radial_max_test.targets[0].vel.x <= 0.001f, "pill cannot drift past configured max distance under low acceleration") && ok;

    Game radial_min_test;
    radial_min_test.pill_settings.distance_min = 2.0f;
    radial_min_test.pill_settings.distance_max = 4.0f;
    radial_min_test.pill_settings.speed = 4.0f;
    radial_min_test.pill_settings.acceleration = 0.05f;
    normalize_settings(radial_min_test);
    radial_min_test.targets.push_back({
        {tracking_to_units(2.02f), PLANE_EYE_HEIGHT, 0.0f},
        {-tracking_to_units(2.0f), 0.0f, 0.0f},
        {-tracking_to_units(2.0f), 0.0f, 0.0f},
        10.0f,
        tracking_to_units(radial_min_test.pill_settings.width) * 0.5f
    });
    update_pill_target(radial_min_test, 1.0f);
    float radial_min_dist = units_to_tracking_meters(std::sqrt(radial_min_test.targets[0].pos.x * radial_min_test.targets[0].pos.x + radial_min_test.targets[0].pos.z * radial_min_test.targets[0].pos.z));
    ok = self_test_check(radial_min_dist >= 1.999f && radial_min_test.targets[0].vel.x >= -0.001f, "pill cannot drift inside configured min distance under low acceleration") && ok;

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

    // Challenge mode + run persistence.
    {
        g_runs_path_override = "build/self-test-runs.cfg";
        std::remove(g_runs_path_override.c_str());

        Game ch;
        ch.rng.seed(99);
        init_scenarios(ch);
        ch.pill_preset_name = "TEST PILL";
        start_scenario(ch, ch.scenarios[1], RunMode::Challenge);
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
        ok = self_test_check(ch.last_run.kind == ScenarioKind::PillTracking && ch.last_run.preset_name == "TEST PILL", "run records scenario kind and preset name") && ok;
        ok = self_test_check(static_cast<int>(ch.runs.size()) == 1, "finished challenge is appended to the run history") && ok;

        ok = self_test_check(best_run_score(ch, ScenarioKind::PillTracking, "TEST PILL") == ch.last_run.score, "best_run_score returns the recorded score") && ok;
        ok = self_test_check(best_run_score(ch, ScenarioKind::WallClick, "TEST PILL") == -1, "best_run_score is -1 for an unplayed scenario") && ok;

        Game reloaded;
        load_runs(reloaded);
        ok = self_test_check(static_cast<int>(reloaded.runs.size()) == 1, "saved runs reload from disk") && ok;
        ok = self_test_check(reloaded.runs[0].kind == ScenarioKind::PillTracking && reloaded.runs[0].preset_name == "TEST PILL" && reloaded.runs[0].score == ch.last_run.score, "reloaded run preserves kind, preset, and score") && ok;

        Game pr;
        pr.rng.seed(7);
        init_scenarios(pr);
        start_scenario(pr, pr.scenarios[1], RunMode::Practice);
        for (int i = 0; i < 240; ++i) {
            update_playing(pr, none, 1.0f / 120.0f);
        }
        ok = self_test_check(pr.mode == AppMode::Playing && pr.stats.shots == 0, "practice tracking neither auto-fires nor times out") && ok;

        std::remove(g_runs_path_override.c_str());
        g_runs_path_override.clear();
    }

    if (ok) {
        std::printf("SELF TEST PASSED\n");
        return 0;
    }
    return 1;
}
