#include "scenario.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "config.hpp"
#include "world.hpp"

static float wall_axis_speed(Game& game, float min_m, float max_m) {
    float speed = wall_to_units(rand_wall_range(game, min_m, max_m));
    if (speed <= 0.0001f && max_m > 0.0f) {
        speed = wall_to_units(max_m);
    }
    return speed;
}

Vec3 wall_desired_velocity(Game& game) {
    float h_speed = wall_axis_speed(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max);
    float v_speed = wall_axis_speed(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max);
    return {
        random_sign(game) * h_speed,
        random_sign(game) * v_speed,
        0.0f,
    };
}

float wall_sample_acceleration(Game& game) {
    return wall_to_units(rand_wall_range(game, game.wall_settings.acceleration_min, game.wall_settings.acceleration_max));
}

float wall_change_timer(Game& game) {
    if (game.wall_settings.change_max <= 0.0f) {
        return 1.0e9f;
    }
    return rand_wall_range(game, game.wall_settings.change_min, game.wall_settings.change_max);
}

static bool uses_center_wall_spawn(const Game& game) {
    return is_tracking(game.wall_settings.task_mode) && game.wall_settings.target_health <= 0 && game.wall_settings.target_count_min == 1;
}

static void bounce_sample_takeoff(Game& game, float r, float& omega, float& launch_y) {
    float speed_m = rand_wall_range(game, game.wall_settings.bounce_speed_min, game.wall_settings.bounce_speed_max);
    float elev = deg_to_rad(rand_wall_range(game, game.wall_settings.bounce_angle_min, game.wall_settings.bounce_angle_max));
    float speed = wall_to_units(speed_m);
    launch_y = speed * std::sin(elev);
    float tangent = speed * std::cos(elev);
    omega = 0.0f;
    if (r > 0.0001f) {
        omega = random_sign(game) * (tangent / r);
    }
}

static void bounce_apply_cylinder(Target& target, float theta) {
    float r = bounce_cylinder_radius(target.distance);
    bounce_place_on_cylinder(target.pos, r, theta);
    float omega = target.desired_vel.x;
    target.vel.x = omega * r * std::cos(theta);
    target.vel.z = omega * r * std::sin(theta);
}

static bool bounce_separated(const Vec3& a, float radius_a, const Vec3& b, float radius_b) {
    return length(a - b) >= wall_spacing_for_radii(radius_a, radius_b);
}

static Target spawn_bounce_target(Game& game, int skip_index) {
    float radius = wall_to_units(rand_wall_range(game, game.wall_settings.radius_min, game.wall_settings.radius_max));
    float min_y = radius;
    bool center_spawn = uses_center_wall_spawn(game);
    float distance = center_spawn
        ? 0.5f * (game.wall_settings.wall_distance_min + game.wall_settings.wall_distance_max)
        : rand_wall_range(game, game.wall_settings.wall_distance_min, game.wall_settings.wall_distance_max);
    float r = bounce_cylinder_radius(distance);
    float limit = bounce_theta_limit(game, r, radius);
    float theta = 0.0f;
    Vec3 pos{0.0f, min_y, 0.0f};
    bounce_place_on_cylinder(pos, r, theta);
    pos.y = min_y;
    bool placed = center_spawn;
    for (int attempt = 0; attempt < 300 && !placed; ++attempt) {
        distance = rand_wall_range(game, game.wall_settings.wall_distance_min, game.wall_settings.wall_distance_max);
        r = bounce_cylinder_radius(distance);
        limit = bounce_theta_limit(game, r, radius);
        theta = rand_range(game, -limit, limit);
        bounce_place_on_cylinder(pos, r, theta);
        pos.y = min_y;
        placed = true;
        for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
            if (i == skip_index) {
                continue;
            }
            if (!bounce_separated(pos, radius, game.targets[i].pos, game.targets[i].radius)) {
                placed = false;
                break;
            }
        }
    }
    if (!placed) {
        int slots = std::max(2, game.wall_settings.target_count_min + 1);
        for (int slot = 1; slot < slots && !placed; ++slot) {
            float t = static_cast<float>(slot) / static_cast<float>(slots);
            distance = game.wall_settings.wall_distance_min +
                (game.wall_settings.wall_distance_max - game.wall_settings.wall_distance_min) * t;
            r = bounce_cylinder_radius(distance);
            limit = bounce_theta_limit(game, r, radius);
            theta = -limit + (2.0f * limit) * t;
            bounce_place_on_cylinder(pos, r, theta);
            pos.y = min_y;
            placed = true;
            for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
                if (i == skip_index) {
                    continue;
                }
                if (!bounce_separated(pos, radius, game.targets[i].pos, game.targets[i].radius)) {
                    placed = false;
                    break;
                }
            }
        }
    }
    if (!placed) {
        distance = 0.5f * (game.wall_settings.wall_distance_min + game.wall_settings.wall_distance_max);
        r = bounce_cylinder_radius(distance);
        theta = 0.0f;
        bounce_place_on_cylinder(pos, r, theta);
        pos.y = min_y;
    }
    float omega = 0.0f;
    float launch_y = 0.0f;
    bounce_sample_takeoff(game, r, omega, launch_y);
    Vec3 vel{omega * r * std::cos(theta), launch_y, omega * r * std::sin(theta)};
    int health = game.wall_settings.target_health;
    return {pos, vel, {omega, launch_y, 0.0f}, 1.0e9f, radius, 0.0f, distance, health};
}

Target spawn_wall_target(Game& game, int skip_index) {
    if (is_bounce(game.wall_settings)) {
        return spawn_bounce_target(game, skip_index);
    }
    float radius = wall_to_units(rand_wall_range(game, game.wall_settings.radius_min, game.wall_settings.radius_max));
    // Single-target infinite tracking starts in the middle of the spawn rectangle.
    // Clicking and target-switching keep a random spawn in that same rectangle.
    bool center_spawn = uses_center_wall_spawn(game);
    float distance = center_spawn
        ? 0.5f * (game.wall_settings.wall_distance_min + game.wall_settings.wall_distance_max)
        : rand_wall_range(game, game.wall_settings.wall_distance_min, game.wall_settings.wall_distance_max);
    float wall_width = wall_width_for_distance(distance);
    float wall_height = wall_height_for_distance(distance);
    float wall_z = wall_z_from_distance(distance);
    float min_x = -wall_width * 0.44f + radius;
    float max_x = wall_width * 0.44f - radius;
    float min_y = wall_height * 0.16f + radius;
    float max_y = wall_height * 0.84f - radius;
    Vec3 pos{0.0f, ROOM_EYE_HEIGHT, wall_z + 0.45f};
    bool placed = false;
    if (center_spawn) {
        pos.x = (min_x + max_x) * 0.5f;
        pos.y = (min_y + max_y) * 0.5f;
        placed = true;
    }
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
        radius = wall_to_units(game.wall_settings.radius_max);
        min_x = -wall_width * 0.44f + radius;
        max_x = wall_width * 0.44f - radius;
        min_y = wall_height * 0.16f + radius;
        max_y = wall_height * 0.84f - radius;
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
    float acceleration = wall_sample_acceleration(game);
    int health = game.wall_settings.target_health;
    return {pos, desired, desired, wall_change_timer(game), radius, acceleration, distance, health};
}

void start_scenario(Game& game, const ScenarioDef& scenario, RunMode mode) {
    normalize_settings(game);
    game.mode = AppMode::Playing;
    game.active_field = FieldId::None;
    game.scenario = scenario;
    game.scenario.kind = is_tracking(game.wall_settings.task_mode) ? ScenarioKind::Tracking : ScenarioKind::WallClick;
    if (is_bounce(game.wall_settings)) {
        game.scenario.title = "The Bounce 180";
    } else {
        game.scenario.title = is_tracking(game.scenario.kind) ? "Wall tracking" : "Wall clicking";
    }
    game.run_mode = mode;
    game.challenge_time_left = mode == RunMode::Challenge ? CHALLENGE_DURATION_SEC : 0.0f;
    game.fire_accumulator = 0.0f;
    game.pending_hit_sounds = 0;
    game.targets.clear();
    game.stats = {};
    game.yaw = 0.0f;
    game.pitch = 0.0f;
    int count = game.wall_settings.target_count_min;
    for (int i = 0; i < count; ++i) {
        game.targets.push_back(spawn_wall_target(game));
    }
}

void clear_playlist_session(Game& game) {
    game.playlist_active = false;
    game.playlist_paused = false;
    game.playlist_complete = false;
    game.playlist_play_index = 0;
    game.playlist_play_id = -1;
    game.playlist_play_name.clear();
    game.playlist_play_tasks.clear();
    game.playlist_session_runs.clear();
}

static bool playlist_task_exists(const Game& game, const std::string& name) {
    for (const WallPreset& preset : game.wall_presets) {
        if (preset.name == name) {
            return true;
        }
    }
    return false;
}

static std::vector<std::string> playable_playlist_tasks(const Game& game, const Playlist& playlist) {
    std::vector<std::string> tasks;
    tasks.reserve(playlist.task_names.size());
    for (const std::string& task_name : playlist.task_names) {
        if (playlist_task_exists(game, task_name)) {
            tasks.push_back(task_name);
        }
    }
    return tasks;
}

static int playlist_playable_index(const Game& game, const Playlist& playlist, int start_entry) {
    int index = 0;
    int count = static_cast<int>(playlist.task_names.size());
    int until = std::max(0, std::min(start_entry, count));
    for (int i = 0; i < until; ++i) {
        if (playlist_task_exists(game, playlist.task_names[i])) {
            index += 1;
        }
    }
    return index;
}

static bool apply_playlist_task(Game& game, int task_index) {
    if (task_index < 0 || task_index >= static_cast<int>(game.playlist_play_tasks.size())) {
        return false;
    }
    const std::string& name = game.playlist_play_tasks[task_index];
    for (int i = 0; i < static_cast<int>(game.wall_presets.size()); ++i) {
        if (game.wall_presets[i].name == name) {
            game.selected_wall_preset = i;
            game.wall_settings = game.wall_presets[i].settings;
            game.wall_preset_name = game.wall_presets[i].name;
            return true;
        }
    }
    return false;
}

static bool start_current_playlist_task(Game& game) {
    while (game.playlist_play_index < static_cast<int>(game.playlist_play_tasks.size()) &&
           !apply_playlist_task(game, game.playlist_play_index)) {
        game.playlist_play_index += 1;
    }
    if (game.playlist_play_index >= static_cast<int>(game.playlist_play_tasks.size())) {
        return false;
    }
    if (game.scenarios.empty()) {
        init_scenarios(game);
    }
    game.playlist_active = true;
    game.playlist_paused = false;
    game.playlist_complete = false;
    start_scenario(game, game.scenarios[0], RunMode::Challenge);
    return true;
}

bool playlist_can_resume(const Game& game) {
    if (!game.playlist_paused || game.playlist_play_tasks.empty()) {
        return false;
    }
    if (game.playlist_play_index < 0 ||
        game.playlist_play_index >= static_cast<int>(game.playlist_play_tasks.size())) {
        return false;
    }
    if (game.selected_playlist < 0 || game.selected_playlist >= static_cast<int>(game.playlists.size())) {
        return false;
    }
    return game.playlists[game.selected_playlist].name == game.playlist_play_name;
}

bool start_playlist(Game& game, int start_entry) {
    ensure_presets(game);
    if (!playlist_has_playable_tasks(game)) {
        return false;
    }
    const Playlist& playlist = game.playlists[game.selected_playlist];
    std::vector<std::string> tasks = playable_playlist_tasks(game, playlist);
    int start_index = playlist_playable_index(game, playlist, start_entry);
    if (tasks.empty() || start_index >= static_cast<int>(tasks.size())) {
        return false;
    }
    game.playlist_play_id = game.selected_playlist;
    game.playlist_play_name = playlist.name;
    game.playlist_play_tasks = tasks;
    game.playlist_play_index = start_index;
    game.playlist_session_runs.clear();
    if (!start_current_playlist_task(game)) {
        clear_playlist_session(game);
        return false;
    }
    return true;
}

bool resume_playlist(Game& game) {
    if (!playlist_can_resume(game)) {
        return false;
    }
    ensure_presets(game);
    if (!start_current_playlist_task(game)) {
        clear_playlist_session(game);
        return false;
    }
    return true;
}

void continue_playlist(Game& game) {
    if (!game.playlist_active || game.playlist_complete) {
        return;
    }
    game.playlist_play_index += 1;
    if (game.playlist_play_index >= static_cast<int>(game.playlist_play_tasks.size())) {
        game.playlist_complete = true;
        game.mode = AppMode::Results;
        return;
    }
    if (!start_current_playlist_task(game)) {
        clear_playlist_session(game);
        game.mode = AppMode::Menu;
    }
}

void handle_results_continue(Game& game) {
    if (game.playlist_active && !game.playlist_complete) {
        continue_playlist(game);
        return;
    }
    clear_playlist_session(game);
    game.mode = AppMode::Menu;
}

void abort_to_menu(Game& game) {
    if (game.playlist_active && !game.playlist_complete) {
        if (game.mode == AppMode::Results) {
            game.playlist_play_index += 1;
            if (game.playlist_play_index >= static_cast<int>(game.playlist_play_tasks.size())) {
                clear_playlist_session(game);
                game.mode = AppMode::Menu;
                return;
            }
        }
        game.playlist_active = false;
        game.playlist_paused = true;
    } else {
        clear_playlist_session(game);
    }
    game.mode = AppMode::Menu;
}

static int aimed_target(const Game& game) {
    Vec3 origin = camera_pos(game);
    Vec3 dir = forward_dir(game);
    int best = -1;
    float best_projected = 1.0e9f;
    for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
        const Target& target = game.targets[i];
        Vec3 to_target = target.pos - origin;
        float projected = dot(to_target, dir);
        if (projected < 0.0f) {
            continue;
        }
        Vec3 closest = origin + dir * projected;
        if (length(closest - target.pos) <= target.radius && projected < best_projected) {
            best = i;
            best_projected = projected;
        }
    }
    return best;
}

static void apply_target_damage(Game& game, int hit_index) {
    if (hit_index < 0) {
        return;
    }
    if (game.wall_settings.target_health <= 0) {
        return;
    }
    game.targets[hit_index].health -= 1;
    if (game.targets[hit_index].health <= 0) {
        game.targets[hit_index] = spawn_wall_target(game, hit_index);
    }
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

// Movement bounds for a wall target on its own depth plane.
static void wall_target_bounds(const Target& target, float& min_x, float& max_x, float& min_y, float& max_y) {
    float wall_width = wall_width_for_distance(target.distance);
    float wall_height = wall_height_for_distance(target.distance);
    min_x = -wall_width * 0.48f + target.radius;
    max_x = wall_width * 0.48f - target.radius;
    min_y = wall_height * 0.16f + target.radius;
    max_y = wall_height * 0.84f - target.radius;
}

static float wall_boundary_guard(float speed, float acceleration, float radius, float span) {
    float frame_room = std::fabs(speed) * (1.0f / 30.0f);
    float max_guard = span * 0.45f;
    if (acceleration > 0.0001f) {
        return std::min(max_guard, std::max(radius * 2.0f, speed * speed / (2.0f * acceleration) + frame_room));
    }
    return std::min(max_guard, std::max(radius * 2.0f, frame_room));
}

static float signed_wall_axis_speed(Game& game, float min_m, float max_m, float sign, float current) {
    float speed = std::fabs(current);
    if (speed <= 0.0001f) {
        speed = wall_axis_speed(game, min_m, max_m);
    }
    return sign * speed;
}

static void steer_wall_target_from_bounds(Game& game, Target& target) {
    float min_x, max_x, min_y, max_y;
    wall_target_bounds(target, min_x, max_x, min_y, max_y);
    float guard_x = wall_boundary_guard(target.vel.x, target.acceleration, target.radius, max_x - min_x);
    float guard_y = wall_boundary_guard(target.vel.y, target.acceleration, target.radius, max_y - min_y);
    if (target.pos.x <= min_x + guard_x && target.desired_vel.x < 0.0f) {
        target.desired_vel.x = signed_wall_axis_speed(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max, 1.0f, target.desired_vel.x);
    } else if (target.pos.x >= max_x - guard_x && target.desired_vel.x > 0.0f) {
        target.desired_vel.x = signed_wall_axis_speed(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max, -1.0f, target.desired_vel.x);
    }
    if (target.pos.y <= min_y + guard_y && target.desired_vel.y < 0.0f) {
        target.desired_vel.y = signed_wall_axis_speed(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max, 1.0f, target.desired_vel.y);
    } else if (target.pos.y >= max_y - guard_y && target.desired_vel.y > 0.0f) {
        target.desired_vel.y = signed_wall_axis_speed(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max, -1.0f, target.desired_vel.y);
    }
    lock_disabled_wall_axes(game, target);
}

static void contain_wall_target(Game& game, Target& target) {
    float min_x, max_x, min_y, max_y;
    wall_target_bounds(target, min_x, max_x, min_y, max_y);
    if (target.pos.x < min_x) {
        target.pos.x = min_x;
        if (target.desired_vel.x <= 0.0001f) {
            target.desired_vel.x = signed_wall_axis_speed(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max, 1.0f, target.desired_vel.x);
        }
        if (target.vel.x < 0.0f) {
            target.vel.x = target.desired_vel.x;
        }
        if (std::fabs(target.vel.x) <= 0.0001f && target.desired_vel.x > 0.0f) {
            target.vel.x = target.desired_vel.x;
        }
    } else if (target.pos.x > max_x) {
        target.pos.x = max_x;
        if (target.desired_vel.x >= -0.0001f) {
            target.desired_vel.x = signed_wall_axis_speed(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max, -1.0f, target.desired_vel.x);
        }
        if (target.vel.x > 0.0f) {
            target.vel.x = target.desired_vel.x;
        }
        if (std::fabs(target.vel.x) <= 0.0001f && target.desired_vel.x < 0.0f) {
            target.vel.x = target.desired_vel.x;
        }
    }
    if (target.pos.y < min_y) {
        target.pos.y = min_y;
        if (target.desired_vel.y <= 0.0001f) {
            target.desired_vel.y = signed_wall_axis_speed(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max, 1.0f, target.desired_vel.y);
        }
        if (target.vel.y < 0.0f) {
            target.vel.y = target.desired_vel.y;
        }
        if (std::fabs(target.vel.y) <= 0.0001f && target.desired_vel.y > 0.0f) {
            target.vel.y = target.desired_vel.y;
        }
    } else if (target.pos.y > max_y) {
        target.pos.y = max_y;
        if (target.desired_vel.y >= -0.0001f) {
            target.desired_vel.y = signed_wall_axis_speed(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max, -1.0f, target.desired_vel.y);
        }
        if (target.vel.y > 0.0f) {
            target.vel.y = target.desired_vel.y;
        }
        if (std::fabs(target.vel.y) <= 0.0001f && target.desired_vel.y < 0.0f) {
            target.vel.y = target.desired_vel.y;
        }
    }
    lock_disabled_wall_axes(game, target);
}

static void bounce_floor_dir_change(Game& game, Target& target) {
    float p = game.wall_settings.bounce_dir_change_p;
    if (p <= 0.0f) {
        return;
    }
    if (p >= 1.0f || rand_range(game, 0.0f, 1.0f) < p) {
        target.desired_vel.x = -target.desired_vel.x;
    }
}

static void contain_bounce_target(Game& game, Target& target) {
    float floor_y = target.radius;
    float ceil_y = ROOM_HEIGHT - target.radius;
    float r = bounce_cylinder_radius(target.distance);
    float limit = bounce_theta_limit(game, r, target.radius);
    float theta = bounce_arc_theta(target.pos);
    if (theta > limit) {
        theta = limit;
        target.desired_vel.x = -std::fabs(target.desired_vel.x);
    } else if (theta < -limit) {
        theta = -limit;
        target.desired_vel.x = std::fabs(target.desired_vel.x);
    }
    if (target.pos.y < floor_y) {
        target.pos.y = floor_y;
        if (target.vel.y <= 0.0f) {
            target.vel.y = std::fabs(target.desired_vel.y);
        }
    } else if (target.pos.y > ceil_y) {
        target.pos.y = ceil_y;
        target.vel.y = -std::fabs(target.vel.y);
    }
    bounce_apply_cylinder(target, theta);
}

static void update_bounce_targets(Game& game, float dt) {
    float gravity = wall_to_units(std::max(game.wall_settings.bounce_gravity_m, 0.1f));
    float speed = wall_to_units(std::max(game.wall_settings.bounce_speed_max, 0.5f));
    float max_speed = speed + gravity;
    int substeps = std::max(1, static_cast<int>(std::ceil((max_speed * dt) / std::max(0.04f, wall_to_units(game.wall_settings.radius_min) * 0.4f))));
    substeps = std::min(substeps, 32);
    float step_dt = dt / static_cast<float>(substeps);

    for (int step = 0; step < substeps; ++step) {
        for (Target& target : game.targets) {
            float floor_y = target.radius;
            float ceil_y = ROOM_HEIGHT - target.radius;
            float r = bounce_cylinder_radius(target.distance);
            float limit = bounce_theta_limit(game, r, target.radius);
            target.vel.y -= gravity * step_dt;
            float y0 = target.pos.y;
            float y1 = y0 + target.vel.y * step_dt;
            if (y1 <= floor_y && target.vel.y <= 0.0f) {
                target.pos.y = floor_y;
                target.vel.y = std::fabs(target.desired_vel.y);
                if (y0 > floor_y) {
                    bounce_floor_dir_change(game, target);
                }
            } else if (y1 >= ceil_y && target.vel.y >= 0.0f) {
                target.pos.y = ceil_y;
                target.vel.y = -std::fabs(target.vel.y);
            } else {
                target.pos.y = y1;
            }
            float theta = bounce_arc_theta(target.pos) + target.desired_vel.x * step_dt;
            if (theta > limit) {
                theta = limit;
                target.desired_vel.x = -std::fabs(target.desired_vel.x);
            } else if (theta < -limit) {
                theta = -limit;
                target.desired_vel.x = std::fabs(target.desired_vel.x);
            }
            bounce_apply_cylinder(target, theta);
            contain_bounce_target(game, target);
        }
    }
}

void update_wall_targets(Game& game, float dt) {
    if (is_bounce(game.wall_settings)) {
        update_bounce_targets(game, dt);
        return;
    }
    float max_speed = std::sqrt(
        wall_to_units(game.wall_settings.horizontal_speed_max) * wall_to_units(game.wall_settings.horizontal_speed_max) +
        wall_to_units(game.wall_settings.vertical_speed_max) * wall_to_units(game.wall_settings.vertical_speed_max)
    );
    int substeps = std::max(1, static_cast<int>(std::ceil((max_speed * dt) / std::max(0.04f, wall_to_units(game.wall_settings.radius_min) * 0.4f))));
    substeps = std::min(substeps, 24);
    float step_dt = dt / static_cast<float>(substeps);

    for (int step = 0; step < substeps; ++step) {
        for (Target& target : game.targets) {
            target.change_timer -= step_dt;
            if (target.change_timer <= 0.0f) {
                target.desired_vel = wall_desired_velocity(game);
                target.acceleration = wall_sample_acceleration(game);
                target.change_timer = wall_change_timer(game);
            }
            steer_wall_target_from_bounds(game, target);
            target.vel = approach_velocity(target.vel, target.desired_vel, target.acceleration, step_dt);
            lock_disabled_wall_axes(game, target);
            target.pos = target.pos + target.vel * step_dt;
            contain_wall_target(game, target);
        }
    }
}

// Records the finished challenge run, saves it, and shows the results screen.
static void finalize_challenge(Game& game) {
    RunRecord run;
    run.kind = game.scenario.kind;
    run.preset_name = game.wall_preset_name;
    run.score = game.stats.hits;
    run.shots = game.stats.shots;
    run.accuracy = game.stats.shots > 0 ? static_cast<float>(game.stats.hits) / static_cast<float>(game.stats.shots) * 100.0f : 0.0f;
    run.duration = CHALLENGE_DURATION_SEC;
    run.timestamp = static_cast<long long>(std::time(nullptr));
    game.runs.push_back(run);
    game.last_run = run;
    save_runs(game);
    if (game.playlist_active) {
        game.playlist_session_runs.push_back(run);
        if (game.playlist_play_index + 1 >= static_cast<int>(game.playlist_play_tasks.size())) {
            game.playlist_complete = true;
        }
    }
    game.mode = AppMode::Results;
}

static void fire_tracking_shots(Game& game, int hit_index, float dt) {
    game.fire_accumulator += dt;
    float interval = 1.0f / TRACKING_FIRE_HZ;
    while (game.fire_accumulator >= interval) {
        game.fire_accumulator -= interval;
        game.stats.shots += 1;
        if (hit_index >= 0) {
            game.stats.hits += 1;
            game.pending_hit_sounds += 1;
            apply_target_damage(game, hit_index);
        }
    }
}

void update_playing(Game& game, const Input& input, float dt) {
    float radians_per_count = deg_to_rad(YAW_DEG_PER_COUNT * game.sensitivity);
    game.yaw += static_cast<float>(input.rel_x) * radians_per_count;
    game.pitch = clampf(game.pitch - static_cast<float>(input.rel_y) * radians_per_count, -1.45f, 1.45f);
    game.stats.elapsed += dt;
    if (game.run_mode == RunMode::Challenge) {
        game.challenge_time_left -= dt;
    }

    update_wall_targets(game, dt);

    int hit_index = aimed_target(game);
    if (!is_tracking(game.scenario.kind)) {
        // Clicking always scores on manual shots (score = hits).
        if (fire_pressed(input)) {
            game.stats.shots += 1;
            if (hit_index >= 0) {
                game.stats.hits += 1;
                game.pending_hit_sounds += 1;
                apply_target_damage(game, hit_index);
            }
        }
    } else if (game.run_mode == RunMode::Challenge) {
        // Tracking challenge: auto-fire at a fixed rate; each on-target tick is a hit.
        fire_tracking_shots(game, hit_index, dt);
    } else if (fire_down(input)) {
        // Tracking practice: score by time-on-target while firing. Health ticks at the
        // same fire rate, but practice does not auto-count shots.
        game.stats.tracking_fire_time += dt;
        if (hit_index >= 0) {
            game.stats.tracking_on_time += dt;
        }
        game.fire_accumulator += dt;
        float interval = 1.0f / TRACKING_FIRE_HZ;
        while (game.fire_accumulator >= interval) {
            game.fire_accumulator -= interval;
            if (hit_index >= 0) {
                apply_target_damage(game, hit_index);
            }
        }
    }

    if (game.run_mode == RunMode::Challenge && game.challenge_time_left <= 0.0f) {
        finalize_challenge(game);
    }
}

void init_scenarios(Game& game) {
    game.scenarios = {
        {"Wall clicking", ScenarioKind::WallClick, MapKind::WallRoom, 0.0f, 0, 0.0f},
    };
    game.scenario = game.scenarios.front();
}
