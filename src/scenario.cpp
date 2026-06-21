#include "scenario.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "config.hpp"
#include "world.hpp"

Vec3 wall_desired_velocity(Game& game) {
    float h_speed = wall_to_units(rand_wall_range(game, game.wall_settings.horizontal_speed_min, game.wall_settings.horizontal_speed_max));
    float v_speed = wall_to_units(rand_wall_range(game, game.wall_settings.vertical_speed_min, game.wall_settings.vertical_speed_max));
    return {
        random_sign(game) * h_speed,
        random_sign(game) * v_speed,
        0.0f,
    };
}

float wall_change_timer(Game& game) {
    if (game.wall_settings.change_max <= 0.0f) {
        return 1.0e9f;
    }
    return rand_wall_range(game, game.wall_settings.change_min, game.wall_settings.change_max);
}

Target spawn_wall_target(Game& game, int skip_index) {
    float radius = wall_to_units(rand_wall_range(game, game.wall_settings.radius_min, game.wall_settings.radius_max));
    // Each target picks its own depth in the configured range, so it can spawn closer.
    float distance = rand_wall_range(game, game.wall_settings.wall_distance_min, game.wall_settings.wall_distance_max);
    float wall_width = wall_width_for_distance(distance);
    float wall_height = wall_height_for_distance(distance);
    float wall_z = wall_z_from_distance(distance);
    float min_x = -wall_width * 0.44f + radius;
    float max_x = wall_width * 0.44f - radius;
    float min_y = wall_height * 0.16f + radius;
    float max_y = wall_height * 0.84f - radius;
    Vec3 pos{0.0f, ROOM_EYE_HEIGHT, wall_z + 0.45f};
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
    float acceleration = wall_to_units(rand_wall_range(game, game.wall_settings.acceleration_min, game.wall_settings.acceleration_max));
    return {pos, desired, desired, wall_change_timer(game), radius, acceleration, distance};
}

static Vec3 pill_desired_velocity(Game& game) {
    float angle = rand_range(game, 0.0f, static_cast<float>(M_PI) * 2.0f);
    float speed = tracking_to_units(game.pill_settings.speed);
    return {std::cos(angle) * speed, 0.0f, std::sin(angle) * speed};
}

Vec3 pill_desired_velocity_for_position(Game& game, Vec3 pos) {
    float speed = tracking_to_units(game.pill_settings.speed);
    Vec3 radial{pos.x, 0.0f, pos.z};
    float dist = length(radial);
    float min_dist = tracking_to_units(game.pill_settings.distance_min);
    float max_dist = tracking_to_units(game.pill_settings.distance_max);
    if (dist > 0.001f) {
        Vec3 outward = radial / dist;
        float tangent_mix = rand_range(game, -0.45f, 0.45f);
        Vec3 tangent{-outward.z, 0.0f, outward.x};
        float inner_guard = min_dist * 1.05f;
        float outer_guard = max_dist * 0.95f;
        if (inner_guard >= outer_guard) {
            float midpoint = (min_dist + max_dist) * 0.5f;
            if (dist > midpoint + 0.001f) {
                return normalize(outward * -1.0f + tangent * tangent_mix) * speed;
            }
            if (dist < midpoint - 0.001f) {
                return normalize(outward + tangent * tangent_mix) * speed;
            }
            return tangent * random_sign(game) * speed;
        }
        if (dist >= outer_guard) {
            return normalize(outward * -1.0f + tangent * tangent_mix) * speed;
        }
        if (dist <= inner_guard) {
            return normalize(outward + tangent * tangent_mix) * speed;
        }
    }
    return pill_desired_velocity(game);
}

Target spawn_pill_target(Game& game) {
    float radius = tracking_to_units(game.pill_settings.width) * 0.5f;
    float angle = -static_cast<float>(M_PI) * 0.5f + rand_range(game, -0.35f, 0.35f);
    float dist = tracking_to_units(rand_range(game, game.pill_settings.distance_min, game.pill_settings.distance_max));
    Vec3 desired = pill_desired_velocity_for_position(game, {std::cos(angle) * dist, PLANE_EYE_HEIGHT, std::sin(angle) * dist});
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

void start_scenario(Game& game, const ScenarioDef& scenario, RunMode mode) {
    normalize_settings(game);
    game.mode = AppMode::Playing;
    game.active_field = FieldId::None;
    game.scenario = scenario;
    game.run_mode = mode;
    game.challenge_time_left = mode == RunMode::Challenge ? CHALLENGE_DURATION_SEC : 0.0f;
    game.fire_accumulator = 0.0f;
    game.pending_hit_sounds = 0;
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

// Movement bounds for a wall target on its own depth plane.
static void wall_target_bounds(const Target& target, float& min_x, float& max_x, float& min_y, float& max_y) {
    float wall_width = wall_width_for_distance(target.distance);
    float wall_height = wall_height_for_distance(target.distance);
    min_x = -wall_width * 0.48f + target.radius;
    max_x = wall_width * 0.48f - target.radius;
    min_y = wall_height * 0.16f + target.radius;
    max_y = wall_height * 0.84f - target.radius;
}

static void resolve_wall_target_collisions(Game& game) {
    for (int i = 0; i < static_cast<int>(game.targets.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(game.targets.size()); ++j) {
            Target& a = game.targets[i];
            Target& b = game.targets[j];
            float contact_dist = a.radius + b.radius;
            if (std::fabs(a.pos.z - b.pos.z) >= contact_dist) {
                continue;  // on different depth planes — cannot be touching (no spurious 2D collision)
            }
            Vec3 delta = b.pos - a.pos;
            delta.z = 0.0f;
            float dist = length(delta);
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
            float a_min_x, a_max_x, a_min_y, a_max_y;
            float b_min_x, b_max_x, b_min_y, b_max_y;
            wall_target_bounds(a, a_min_x, a_max_x, a_min_y, a_max_y);
            wall_target_bounds(b, b_min_x, b_max_x, b_min_y, b_max_y);
            a.pos.x = clampf(a.pos.x, a_min_x, a_max_x);
            b.pos.x = clampf(b.pos.x, b_min_x, b_max_x);
            a.pos.y = clampf(a.pos.y, a_min_y, a_max_y);
            b.pos.y = clampf(b.pos.y, b_min_y, b_max_y);

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
                target.change_timer = wall_change_timer(game);
            }
            target.vel = approach_velocity(target.vel, target.desired_vel, target.acceleration, step_dt);
            lock_disabled_wall_axes(game, target);
            target.pos = target.pos + target.vel * step_dt;

            float min_x, max_x, min_y, max_y;
            wall_target_bounds(target, min_x, max_x, min_y, max_y);
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
        resolve_wall_target_collisions(game);
    }
}

void update_pill_target(Game& game, float dt) {
    if (game.targets.empty()) {
        return;
    }
    Target& target = game.targets[0];
    target.change_timer -= dt;
    if (target.change_timer <= 0.0f) {
        target.desired_vel = pill_desired_velocity_for_position(game, target.pos);
        target.change_timer = rand_range(game, game.pill_settings.change_min, game.pill_settings.change_max);
    }
    Vec3 radial{target.pos.x, 0.0f, target.pos.z};
    float dist = length(radial);
    float min_dist = tracking_to_units(game.pill_settings.distance_min);
    float max_dist = tracking_to_units(game.pill_settings.distance_max);
    Vec3 previous_radial = radial;
    if (dist > 0.001f && (dist <= min_dist * 1.03f || dist >= max_dist * 0.97f)) {
        target.desired_vel = pill_desired_velocity_for_position(game, target.pos);
    }
    target.vel = approach_velocity(target.vel, target.desired_vel, tracking_to_units(game.pill_settings.acceleration), dt);
    target.pos = target.pos + target.vel * dt;
    radial = {target.pos.x, 0.0f, target.pos.z};
    dist = length(radial);
    if (dist > 0.001f && dist > max_dist) {
        Vec3 outward = radial / dist;
        target.pos.x = outward.x * max_dist;
        target.pos.z = outward.z * max_dist;
        float radial_speed = dot(target.vel, outward);
        if (radial_speed > 0.0f) {
            target.vel = target.vel - outward * radial_speed;
        }
        target.desired_vel = pill_desired_velocity_for_position(game, target.pos);
    } else if (dist > 0.001f && dist < min_dist) {
        float previous_dist = length(previous_radial);
        Vec3 outward = previous_dist > 0.001f ? previous_radial / previous_dist : radial / dist;
        target.pos.x = outward.x * min_dist;
        target.pos.z = outward.z * min_dist;
        float radial_speed = dot(target.vel, outward);
        if (radial_speed < 0.0f) {
            target.vel = target.vel - outward * radial_speed;
        }
        target.desired_vel = pill_desired_velocity_for_position(game, target.pos);
    }
    float limit = tracking_room_half_size(game.pill_settings) - tracking_to_units(1.0f) - target.radius;
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
}

// Records the finished challenge run, saves it, and shows the results screen.
static void finalize_challenge(Game& game) {
    RunRecord run;
    run.kind = game.scenario.kind;
    run.preset_name = game.scenario.kind == ScenarioKind::WallClick ? game.wall_preset_name : game.pill_preset_name;
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

void update_playing(Game& game, const Input& input, float dt) {
    float radians_per_count = deg_to_rad(YAW_DEG_PER_COUNT * game.sensitivity);
    game.yaw += static_cast<float>(input.rel_x) * radians_per_count;
    game.pitch = clampf(game.pitch - static_cast<float>(input.rel_y) * radians_per_count, -1.45f, 1.45f);
    game.stats.elapsed += dt;
    if (game.run_mode == RunMode::Challenge) {
        game.challenge_time_left -= dt;
    }

    if (game.scenario.kind == ScenarioKind::WallClick) {
        update_wall_targets(game, dt);
    } else {
        update_pill_target(game, dt);
    }

    int hit_index = aimed_target(game);
    if (game.scenario.kind == ScenarioKind::WallClick) {
        // Clicking always scores on manual shots (score = hits).
        if (input.left_pressed) {
            game.stats.shots += 1;
            if (hit_index >= 0) {
                game.stats.hits += 1;
                game.pending_hit_sounds += 1;
                game.targets[hit_index] = spawn_wall_target(game, hit_index);
            }
        }
    } else if (game.run_mode == RunMode::Challenge) {
        // Tracking challenge: auto-fire at a fixed rate; each on-target tick is a hit.
        game.fire_accumulator += dt;
        float interval = 1.0f / TRACKING_FIRE_HZ;
        while (game.fire_accumulator >= interval) {
            game.fire_accumulator -= interval;
            game.stats.shots += 1;
            if (hit_index >= 0) {
                game.stats.hits += 1;
                game.pending_hit_sounds += 1;
            }
        }
    } else if (input.left_down) {
        // Tracking practice: score by time-on-target while firing.
        game.stats.tracking_fire_time += dt;
        if (hit_index >= 0) {
            game.stats.tracking_on_time += dt;
        }
    }

    if (game.run_mode == RunMode::Challenge && game.challenge_time_left <= 0.0f) {
        finalize_challenge(game);
    }
}

void init_scenarios(Game& game) {
    game.scenarios = {
        {"WALL CLICKING", ScenarioKind::WallClick, MapKind::WallRoom, 0.0f, 0, 0.0f},
        {"360 PILL TRACKING", ScenarioKind::PillTracking, MapKind::Plane360, 0.0f, 1, 0.0f},
    };
    game.scenario = game.scenarios.front();
}
