#pragma once

#include "types.hpp"

// Main menu entry point (handles editing, layout and drawing for one frame).
void draw_menu(Game& game, const Input& input, int w, int h);

// Text-box editing primitives (also exercised by the self-test).
void menu_focus_field(Game& game, FieldId id);
void menu_handle_edit(Game& game, const Input& input);
void menu_blur_field(Game& game);    // commit the active field and unfocus
void menu_cancel_edit(Game& game);   // discard the active field and unfocus

// Preset actions.
void new_wall_preset(Game& game);
void new_pill_preset(Game& game);
void delete_wall_preset(Game& game);
void delete_pill_preset(Game& game);
