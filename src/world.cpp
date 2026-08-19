#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <random>

float rand_range(Game& game, float low, float high) {
    std::uniform_real_distribution<float> dist(low, high);
    return dist(game.rng);
}

Vec3 camera_pos(const Game& game) {
    float y = is_bounce(game.wall_settings) ? bounce_camera_y(game) : ROOM_EYE_HEIGHT;
    return {0.0f, y, wall_camera_z()};
}

Vec3 forward_dir(const Game& game) {
    return normalize({
        std::sin(game.yaw) * std::cos(game.pitch),
        std::sin(game.pitch),
        -std::cos(game.yaw) * std::cos(game.pitch),
    });
}

float random_sign(Game& game) {
    return rand_range(game, 0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
}

float rand_wall_range(Game& game, float low, float high) {
    if (high <= low) {
        return low;
    }
    return rand_range(game, low, high);
}

int rand_wall_int_range(Game& game, int low, int high) {
    if (high <= low) {
        return low;
    }
    std::uniform_int_distribution<int> dist(low, high);
    return dist(game.rng);
}

float wall_units_per_meter() {
    return ROOM_EYE_HEIGHT / CAMERA_REFERENCE_HEIGHT_M;
}

float wall_to_units(float meters) {
    return meters * wall_units_per_meter();
}

float units_to_wall_meters(float units) {
    return units / wall_units_per_meter();
}

float wall_camera_z() {
    return ROOM_BACK_Z - 1.5f;
}

float wall_z_from_distance(float meters) {
    return wall_camera_z() - wall_to_units(meters);
}

float wall_width_for_distance(float meters) {
    (void)meters;
    return ROOM_WIDTH;
}

float wall_height_for_distance(float meters) {
    (void)meters;
    return ROOM_HEIGHT;
}

float wall_back_z_for_distance(float meters) {
    return wall_camera_z() + std::max(wall_to_units(0.4f), wall_to_units(meters) * 0.18f);
}

float bounce_camera_y(const Game& game) {
    float y = wall_to_units(game.wall_settings.bounce_camera_height_m);
    float min_y = wall_to_units(BOUNCE_CAMERA_HEIGHT_MIN_M);
    float max_y = ROOM_HEIGHT - wall_to_units(0.4f);
    return clampf(y, min_y, max_y);
}

float bounce_equal_wall_distance_m() {
    return units_to_wall_meters(ROOM_WIDTH * 0.5f);
}

float bounce_half_extent(const Game& game) {
    return wall_to_units(game.wall_settings.wall_distance_max) + wall_to_units(game.wall_settings.radius_max);
}

float bounce_front_z(const Game& game) {
    return wall_camera_z() - bounce_half_extent(game);
}

float bounce_max_jump_height_m(const WallClickSettings& settings) {
    float gravity = std::max(settings.bounce_gravity_m, 0.01f);
    float vy = settings.bounce_speed_max * std::sin(deg_to_rad(settings.bounce_angle_max));
    return (vy * vy) / (2.0f * gravity);
}

float bounce_cylinder_radius(float distance_m) {
    return std::max(wall_to_units(distance_m), wall_to_units(0.5f));
}

float bounce_arc_theta(const Vec3& pos) {
    return std::atan2(pos.x, -(pos.z - wall_camera_z()));
}

void bounce_place_on_cylinder(Vec3& pos, float radius_units, float theta) {
    pos.x = radius_units * std::sin(theta);
    pos.z = wall_camera_z() - radius_units * std::cos(theta);
}

float bounce_back_z(const Game& game) {
    return wall_back_z_for_distance(game.wall_settings.wall_distance_max);
}

float bounce_theta_limit(float radius_units, float ball_radius, float back_z) {
    float cam_z = wall_camera_z();
    float z_hit = back_z - ball_radius;
    float c = (cam_z - z_hit) / std::max(radius_units, 0.001f);
    return std::acos(clampf(c, -1.0f, 1.0f));
}

float bounce_theta_limit(const Game& game, float radius_units, float ball_radius) {
    return bounce_theta_limit(radius_units, ball_radius, bounce_back_z(game));
}

float room_play_width(const Game& game) {
    if (is_bounce(game.wall_settings)) {
        return bounce_half_extent(game) * 2.0f;
    }
    return wall_width_for_distance(game.wall_settings.wall_distance_max);
}

void bounce_target_bounds(const Game& game, float radius, float& min_x, float& max_x, float& min_y, float& max_y, float& min_z, float& max_z) {
    float half = bounce_half_extent(game);
    float cam_z = wall_camera_z();
    min_x = -half + radius;
    max_x = half - radius;
    min_y = radius;
    max_y = ROOM_HEIGHT - radius;
    min_z = cam_z - half + radius;
    max_z = bounce_back_z(game) - radius;
    if (max_x < min_x) {
        float mid = (min_x + max_x) * 0.5f;
        min_x = mid;
        max_x = mid;
    }
    if (max_y < min_y) {
        float mid = (min_y + max_y) * 0.5f;
        min_y = mid;
        max_y = mid;
    }
    if (max_z < min_z) {
        float mid = (min_z + max_z) * 0.5f;
        min_z = mid;
        max_z = mid;
    }
}

int bounce_capacity_for_radius(float radius_m, float wall_distance_m) {
    float radius = wall_to_units(radius_m);
    float arc = bounce_cylinder_radius(wall_distance_m) * static_cast<float>(M_PI);
    float spacing = std::max(radius * 3.0f, 0.01f);
    int along_arc = std::max(1, static_cast<int>(std::floor(arc / spacing)) + 1);
    return std::max(1, std::min(18, along_arc));
}

float scene_far_plane(const Game& game) {
    float wall_distance = game.wall_settings.wall_distance_max;  // depth/far-plane sized to the farthest target
    float width = room_play_width(game);
    float height = wall_height_for_distance(wall_distance);
    float far_z = is_bounce(game.wall_settings)
        ? bounce_front_z(game) - wall_to_units(game.wall_settings.radius_max)
        : wall_z_from_distance(wall_distance) - wall_to_units(game.wall_settings.radius_max);
    Vec3 eye = camera_pos(game);
    float max_x = width * 0.5f + wall_to_units(game.wall_settings.radius_max);
    float max_y = std::max(std::fabs(0.0f - eye.y), std::fabs(height - eye.y)) + wall_to_units(game.wall_settings.radius_max);
    float max_z = std::fabs(far_z - eye.z);
    if (is_bounce(game.wall_settings)) {
        max_z = std::max(max_z, std::fabs(bounce_back_z(game) - eye.z));
    }
    return std::max(120.0f, std::sqrt(max_x * max_x + max_y * max_y + max_z * max_z) + 8.0f);
}

int wall_capacity_for_radius(float radius_m, float wall_distance_m) {
    float radius = wall_to_units(radius_m);
    float width = wall_width_for_distance(wall_distance_m);
    float height = wall_height_for_distance(wall_distance_m);
    float min_x = -width * 0.44f + radius;
    float max_x = width * 0.44f - radius;
    float min_y = height * 0.16f + radius;
    float max_y = height * 0.84f - radius;
    float spacing = radius * 3.0f;
    int cols = std::max(1, static_cast<int>(std::floor((max_x - min_x) / spacing)) + 1);
    int rows = std::max(1, static_cast<int>(std::floor((max_y - min_y) / spacing)) + 1);
    return std::max(1, std::min(18, cols * rows));
}

float wall_spacing_for_radii(float a, float b) {
    return a + b + std::min(a, b);
}
