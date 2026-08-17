#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <random>

float rand_range(Game& game, float low, float high) {
    std::uniform_real_distribution<float> dist(low, high);
    return dist(game.rng);
}

Vec3 camera_pos(const Game& game) {
    (void)game;
    return {0.0f, ROOM_EYE_HEIGHT, ROOM_BACK_Z - 1.5f};
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

float scene_far_plane(const Game& game) {
    float wall_distance = game.wall_settings.wall_distance_max;  // depth/far-plane sized to the farthest target
    float width = wall_width_for_distance(wall_distance);
    float height = wall_height_for_distance(wall_distance);
    float far_z = wall_z_from_distance(wall_distance) - wall_to_units(game.wall_settings.radius_max);
    Vec3 eye = camera_pos(game);
    float max_x = width * 0.5f + wall_to_units(game.wall_settings.radius_max);
    float max_y = std::max(std::fabs(0.0f - eye.y), std::fabs(height - eye.y)) + wall_to_units(game.wall_settings.radius_max);
    float max_z = std::fabs(far_z - eye.z);
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
