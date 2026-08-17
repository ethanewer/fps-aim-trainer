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

Target spawn_wall_target(Game& game, int skip_index) {
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
    game.scenario.title = is_tracking(game.scenario.kind) ? "WALL TRACKING" : "WALL CLICKING";
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

void update_wall_targets(Game& game, float dt) {
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
        if (input.left_pressed) {
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
    } else if (input.left_down) {
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
        {"WALL CLICKING", ScenarioKind::WallClick, MapKind::WallRoom, 0.0f, 0, 0.0f},
    };
    game.scenario = game.scenarios.front();
}
