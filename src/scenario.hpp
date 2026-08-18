#pragma once

#include "math.hpp"
#include "types.hpp"

// Target motion sampling.
Vec3 wall_desired_velocity(Game& game);
float wall_sample_acceleration(Game& game);
float wall_change_timer(Game& game);

// Target spawning.
Target spawn_wall_target(Game& game, int skip_index = -1);

// Scenario lifecycle and simulation.
void start_scenario(Game& game, const ScenarioDef& scenario, RunMode mode = RunMode::Practice);
void update_wall_targets(Game& game, float dt);
void update_playing(Game& game, const Input& input, float dt);
void init_scenarios(Game& game);

// Playlist play session. `start_playlist` is a no-op (returns false) when the
// selected playlist has no tasks that still exist. `start_entry` is an index
// into the editor task list; Play uses 0, and clicking an already-selected
// entry starts from that task. Esc pauses; `resume_playlist` continues.
void clear_playlist_session(Game& game);
bool playlist_can_resume(const Game& game);
bool start_playlist(Game& game, int start_entry = 0);
bool resume_playlist(Game& game);
void continue_playlist(Game& game);
void handle_results_continue(Game& game);
void abort_to_menu(Game& game);
