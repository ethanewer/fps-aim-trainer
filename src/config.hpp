#pragma once

#include <string>

#include "types.hpp"

// Path of the on-disk settings file. Override for tests/screenshots.
extern std::string g_settings_path_override;

// Preset name helpers.
bool is_allowed_preset_char(char c);
std::string sanitize_preset_name(const std::string& raw);
std::string filter_preset_name_draft(const std::string& raw);

// Returns `requested` (sanitized) made unique among `presets`, skipping
// `skip_index`. Appends " 2", " 3", ... when needed.
template <typename PresetList>
std::string unique_preset_name(const PresetList& presets, const std::string& requested, int skip_index) {
    std::string base = sanitize_preset_name(requested);
    auto name_taken = [&](const std::string& candidate) {
        for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
            if (i != skip_index && presets[i].name == candidate) {
                return true;
            }
        }
        return false;
    };
    if (!name_taken(base)) {
        return base;
    }

    for (int suffix = 2; suffix < 1000; ++suffix) {
        std::string tail = " " + std::to_string(suffix);
        std::string trimmed = base;
        if (trimmed.size() + tail.size() > 22) {
            trimmed.resize(22 - tail.size());
            while (!trimmed.empty() && trimmed.back() == ' ') {
                trimmed.pop_back();
            }
        }
        std::string candidate = sanitize_preset_name(trimmed + tail);
        if (!name_taken(candidate)) {
            return candidate;
        }
    }
    return base;
}

// Settings normalization (clamps every value into its valid range).
void normalize_settings(Game& game);

// Preset management.
void ensure_presets(Game& game);
void apply_selected_presets(Game& game);
void save_current_wall_preset(Game& game);
void save_current_pill_preset(Game& game);

// Persistence.
std::string settings_path();
void save_settings(const Game& game);
void load_settings(Game& game);
