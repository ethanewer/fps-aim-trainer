#pragma once

#include "math.hpp"
#include "types.hpp"

// Random helpers (seeded by the game RNG).
float rand_range(Game& game, float low, float high);
float random_sign(Game& game);
float rand_wall_range(Game& game, float low, float high);
int rand_wall_int_range(Game& game, int low, int high);

// Camera.
Vec3 camera_pos(const Game& game);
Vec3 forward_dir(const Game& game);

// Meters <-> internal units.
float wall_units_per_meter();
float wall_to_units(float meters);
float units_to_wall_meters(float units);

// Wall room geometry.
float wall_camera_z();
float wall_z_from_distance(float meters);
float wall_width_for_distance(float meters);
float wall_height_for_distance(float meters);
float wall_back_z_for_distance(float meters);
int wall_capacity_for_radius(float radius_m, float wall_distance_m);
float wall_spacing_for_radii(float a, float b);

// Far clipping distance needed to contain the active scenario's geometry.
float scene_far_plane(const Game& game);
