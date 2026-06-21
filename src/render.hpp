#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "types.hpp"

// Coordinate / scale helpers.
float ui_scale_for_height(int h);
void begin_2d(int w, int h);

// 2D primitives and bitmap text.
void rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
const std::array<const char*, 7>& glyph(char c);
float text_width(const std::string& value, float scale);
void text(float x, float y, const std::string& value, float scale, uint8_t r = 235, uint8_t g = 240, uint8_t b = 245);
void text_fit(float x, float y, const std::string& value, float scale, float max_width, uint8_t r = 235, uint8_t g = 240, uint8_t b = 245);

// Reusable widgets.
bool list_button(const Input& input, float x, float y, float w, float h, const std::string& label, bool selected);

// In-scenario world + HUD.
void draw_world(const Game& game, int w, int h);
