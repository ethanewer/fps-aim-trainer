#pragma once

#include "math.hpp"
#include "types.hpp"

// Target motion sampling.
Vec3 wall_desired_velocity(Game& game);
float wall_change_timer(Game& game);
Vec3 pill_desired_velocity_for_position(Game& game, Vec3 pos);

// Target spawning.
Target spawn_wall_target(Game& game, int skip_index = -1);
Target spawn_pill_target(Game& game);

// Scenario lifecycle and simulation.
void start_scenario(Game& game, const ScenarioDef& scenario, RunMode mode = RunMode::Practice);
void update_wall_targets(Game& game, float dt);
void update_pill_target(Game& game, float dt);
void update_playing(Game& game, const Input& input, float dt);
void init_scenarios(Game& game);
