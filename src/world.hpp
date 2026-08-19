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

// Bounce 180: wall room with a lower camera on the back wall. Left, right, and
// front inner faces sit a ball-radius outside the far spawn cylinder so spheres
// can touch the walls without intersecting them. Each ball travels on a cylinder
// of fixed radius sampled from wall_distance_min/max and only bounces off the
// floor and the visible back wall (behind the camera), not a 90° clip at the
// camera plane.
float bounce_camera_y(const Game& game);
float bounce_equal_wall_distance_m();
float bounce_half_extent(const Game& game);
float bounce_front_z(const Game& game);
float bounce_max_jump_height_m(const WallClickSettings& settings);
float bounce_cylinder_radius(float distance_m);
float bounce_arc_theta(const Vec3& pos);
float bounce_back_z(const Game& game);
float bounce_theta_limit(float radius_units, float ball_radius, float back_z);
float bounce_theta_limit(const Game& game, float radius_units, float ball_radius);
void bounce_place_on_cylinder(Vec3& pos, float radius_units, float theta);
float room_play_width(const Game& game);
int bounce_capacity_for_radius(float radius_m, float wall_distance_m);
void bounce_target_bounds(const Game& game, float radius, float& min_x, float& max_x, float& min_y, float& max_y, float& min_z, float& max_z);

// Far clipping distance needed to contain the active scenario's geometry.
float scene_far_plane(const Game& game);
