#include "render.hpp"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "world.hpp"

static void color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glColor4ub(r, g, b, a);
}

static uint8_t shade_channel(int value, float scale) {
    return static_cast<uint8_t>(std::max(0, std::min(255, static_cast<int>(std::round(static_cast<float>(value) * scale)))));
}

static void room_color(const WallColorSettings& wall_color, float scale) {
    color(
        shade_channel(wall_color.r, scale),
        shade_channel(wall_color.g, scale),
        shade_channel(wall_color.b, scale)
    );
}

static void draw_box(Vec3 c, Vec3 s) {
    float x0 = c.x - s.x * 0.5f, x1 = c.x + s.x * 0.5f;
    float y0 = c.y - s.y * 0.5f, y1 = c.y + s.y * 0.5f;
    float z0 = c.z - s.z * 0.5f, z1 = c.z + s.z * 0.5f;
    glBegin(GL_QUADS);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glVertex3f(x1, y0, z0); glVertex3f(x0, y0, z0); glVertex3f(x0, y1, z0); glVertex3f(x1, y1, z0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glEnd();
}

static void draw_lit_sphere(Vec3 c, float radius) {
    constexpr int stacks = 24;
    constexpr int slices = 36;
    for (int i = 0; i < stacks; ++i) {
        float lat0 = static_cast<float>(M_PI) * (-0.5f + static_cast<float>(i) / stacks);
        float lat1 = static_cast<float>(M_PI) * (-0.5f + static_cast<float>(i + 1) / stacks);
        float z0 = std::sin(lat0), zr0 = std::cos(lat0);
        float z1 = std::sin(lat1), zr1 = std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = static_cast<float>(M_PI) * 2.0f * static_cast<float>(j) / slices;
            float x = std::cos(lng), y = std::sin(lng);
            glNormal3f(x * zr0, z0, y * zr0);
            glVertex3f(c.x + radius * x * zr0, c.y + radius * z0, c.z + radius * y * zr0);
            glNormal3f(x * zr1, z1, y * zr1);
            glVertex3f(c.x + radius * x * zr1, c.y + radius * z1, c.z + radius * y * zr1);
        }
        glEnd();
    }
}

static void perspective(float vertical_fov_deg, float aspect, float near_z, float far_z) {
    float ymax = near_z * std::tan(deg_to_rad(vertical_fov_deg) * 0.5f);
    float xmax = ymax * aspect;
    glFrustum(-xmax, xmax, -ymax, ymax, near_z, far_z);
}

static void look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    float m[16] = {
        s.x, u.x, -f.x, 0.0f,
        s.y, u.y, -f.y, 0.0f,
        s.z, u.z, -f.z, 0.0f,
        -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f,
    };
    glMultMatrixf(m);
}

static void draw_wall_room(const Game& game) {
    constexpr float WALL_VISUAL_SCALE = 1.10f;
    float wall_distance = game.wall_settings.wall_distance_max;  // back wall at the farthest configured distance
    float wall_z = wall_z_from_distance(wall_distance);
    float back_z = wall_back_z_for_distance(wall_distance);
    float width = wall_width_for_distance(wall_distance) * WALL_VISUAL_SCALE;
    float gameplay_height = wall_height_for_distance(wall_distance);
    float height = gameplay_height * WALL_VISUAL_SCALE;
    float center_y = gameplay_height * 0.5f;
    float bottom_y = center_y - height * 0.5f;
    float top_y = center_y + height * 0.5f;
    room_color(game.wall_color, 1.00f);
    draw_box({0.0f, center_y, wall_z}, {width, height, 0.18f});
    room_color(game.wall_color, 0.87f);
    draw_box({0.0f, bottom_y - 0.05f, (wall_z + back_z) * 0.5f}, {width, 0.1f, back_z - wall_z});
    room_color(game.wall_color, 0.83f);
    draw_box({0.0f, top_y + 0.05f, (wall_z + back_z) * 0.5f}, {width, 0.1f, back_z - wall_z});
    room_color(game.wall_color, 0.77f);
    draw_box({-width * 0.5f, center_y, (wall_z + back_z) * 0.5f}, {0.15f, height, back_z - wall_z});
    draw_box({width * 0.5f, center_y, (wall_z + back_z) * 0.5f}, {0.15f, height, back_z - wall_z});
    room_color(game.wall_color, 0.68f);
    draw_box({0.0f, top_y, wall_z + 0.03f}, {width + 0.25f, 0.18f, 0.08f});
    draw_box({0.0f, bottom_y, wall_z + 0.03f}, {width + 0.25f, 0.18f, 0.08f});
    draw_box({-width * 0.5f, center_y, wall_z + 0.03f}, {0.18f, height + 0.25f, 0.08f});
    draw_box({width * 0.5f, center_y, wall_z + 0.03f}, {0.18f, height + 0.25f, 0.08f});
}

static void set_target_material(const TargetColorSettings& target_color) {
    float r = static_cast<float>(target_color.r) / 255.0f;
    float g = static_cast<float>(target_color.g) / 255.0f;
    float b = static_cast<float>(target_color.b) / 255.0f;
    GLfloat ambient[] = {std::max(0.04f, r * 0.48f), std::max(0.04f, g * 0.48f), std::max(0.04f, b * 0.48f), 1.0f};
    GLfloat diffuse[] = {r, g, b, 1.0f};
    GLfloat specular[] = {std::max(0.10f, r * 0.28f), std::max(0.10f, g * 0.28f), std::max(0.10f, b * 0.28f), 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 14.0f);
}

static void draw_target(const Target& target, const TargetColorSettings& target_color) {
    set_target_material(target_color);
    draw_lit_sphere(target.pos, target.radius);
}

static void begin_3d(const Game& game, int w, int h) {
    float aspect = static_cast<float>(w) / static_cast<float>(std::max(1, h));
    float vertical_fov = rad_to_deg(2.0f * std::atan(std::tan(deg_to_rad(HORIZONTAL_FOV_DEG) * 0.5f) / aspect));
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(vertical_fov, aspect, 0.03f, scene_far_plane(game));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Vec3 eye = camera_pos(game);
    look_at(eye, eye + forward_dir(game), {0.0f, 1.0f, 0.0f});
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
}

void begin_2d(int w, int h) {
    glDisable(GL_DEPTH_TEST);
    float scale = std::max(1.0f, static_cast<float>(h) / 1080.0f);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<float>(w) / scale, static_cast<float>(h) / scale, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

float ui_scale_for_height(int h) {
    return std::max(1.0f, static_cast<float>(h) / 1080.0f);
}

void rect(float x, float y, float w, float h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    color(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}

bool list_button(const Input& input, float x, float y, float w, float h, const std::string& label, bool selected) {
    bool hovered = input.mouse_x >= x && input.mouse_x <= x + w && input.mouse_y >= y && input.mouse_y <= y + h;
    if (selected) rect(x, y, w, h, 86, 98, 114);
    else if (hovered) rect(x, y, w, h, 58, 66, 76);
    else rect(x, y, w, h, 31, 36, 42);
    rect(x, y, w, 2, 83, 96, 112);
    rect(x, y + h - 2, w, 2, 83, 96, 112);
    rect(x, y, 2, h, 83, 96, 112);
    rect(x + w - 2, y, 2, h, 83, 96, 112);
    const float label_scale = 1.85f;
    text_fit(x + 14.0f, y + (h - text_height(label_scale)) * 0.5f, label, label_scale, w - 28.0f, 230, 236, 244);
    return hovered && input.left_pressed;
}

void draw_world(const Game& game, int w, int h) {
    glClearColor(
        static_cast<float>(shade_channel(game.wall_color.r, 0.70f)) / 255.0f,
        static_cast<float>(shade_channel(game.wall_color.g, 0.70f)) / 255.0f,
        static_cast<float>(shade_channel(game.wall_color.b, 0.70f)) / 255.0f,
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_3d(game, w, h);
    draw_wall_room(game);
    GLfloat light_ambient[] = {0.82f, 0.82f, 0.84f, 1.0f};
    GLfloat light_diffuse[] = {0.70f, 0.70f, 0.68f, 1.0f};
    GLfloat light_pos[] = {-4.0f, wall_height_for_distance(game.wall_settings.wall_distance_max) + 1.5f, 1.0f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    for (const Target& target : game.targets) {
        draw_target(target, game.target_color);
    }
    glDisable(GL_LIGHTING);
    begin_2d(w, h);
    float ui_scale = ui_scale_for_height(h);
    float ui_w = static_cast<float>(w) / ui_scale;
    float ui_h = static_cast<float>(h) / ui_scale;
    float cx = ui_w * 0.5f, cy = ui_h * 0.5f;
    float len = game.crosshair.length;
    float gap = game.crosshair.gap;
    float thick = game.crosshair.thickness;
    rect(cx - gap - len, cy - thick * 0.5f, len, thick, 245, 248, 252);
    rect(cx + gap, cy - thick * 0.5f, len, thick, 245, 248, 252);
    rect(cx - thick * 0.5f, cy - gap - len, thick, len, 245, 248, 252);
    rect(cx - thick * 0.5f, cy + gap, thick, len, 245, 248, 252);
    char line[160];
    std::snprintf(line, sizeof(line), "FOV 103  Sens %.3f", game.sensitivity);
    std::string sens_line = line;
    std::string stat_line;
    std::string timer_line;
    std::string playlist_line;
    if (game.playlist_active && !game.playlist_play_tasks.empty()) {
        std::snprintf(line, sizeof(line), "Playlist %d/%d  %s",
                      game.playlist_play_index + 1,
                      static_cast<int>(game.playlist_play_tasks.size()),
                      game.wall_preset_name.c_str());
        playlist_line = line;
    }
    if (game.run_mode == RunMode::Challenge) {
        // Score is hits; accuracy is shown but is not the score.
        float accuracy = game.stats.shots == 0 ? 0.0f : static_cast<float>(game.stats.hits) / static_cast<float>(game.stats.shots) * 100.0f;
        std::snprintf(line, sizeof(line), "Score %d  Acc %.1f%%", game.stats.hits, accuracy);
        stat_line = line;
        float remaining = game.challenge_time_left < 0.0f ? 0.0f : game.challenge_time_left;
        std::snprintf(line, sizeof(line), "Challenge  Time %.1f", remaining);
        timer_line = line;
    } else if (!is_tracking(game.scenario.kind)) {
        float accuracy = game.stats.shots == 0 ? 100.0f : static_cast<float>(game.stats.hits) / static_cast<float>(game.stats.shots) * 100.0f;
        std::snprintf(line, sizeof(line), "Hits %d  Shots %d  Acc %.1f%%", game.stats.hits, game.stats.shots, accuracy);
        stat_line = line;
    } else {
        float tracking = game.stats.tracking_fire_time <= 0.0001f ? 0.0f : game.stats.tracking_on_time / game.stats.tracking_fire_time * 100.0f;
        std::snprintf(line, sizeof(line), "Tracking %.1f%%  Hold LMB/Space", tracking);
        stat_line = line;
    }
    bool challenge = !timer_line.empty();
    bool playlist = !playlist_line.empty();
    float hud_w = std::max({
        260.0f,
        text_width(game.scenario.title, 3.0f) + 36.0f,
        text_width(sens_line, 2.4f) + 36.0f,
        text_width(stat_line, 2.4f) + 36.0f,
        text_width(timer_line, 2.4f) + 36.0f,
        text_width(playlist_line, 2.4f) + 36.0f,
    });
    hud_w = std::min(hud_w, ui_w - 48.0f);
    float hud_h = 122.0f;
    if (challenge) {
        hud_h += 30.0f;
    }
    if (playlist) {
        hud_h += 30.0f;
    }
    rect(24, 22, hud_w, hud_h, 0, 0, 0, 150);
    float text_y = 42.0f;
    text_fit(42, text_y, game.scenario.title, 3.0f, hud_w - 36.0f);
    text_y += 36.0f;
    text_fit(42, text_y, sens_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_y += 30.0f;
    text_fit(42, text_y, stat_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_y += 30.0f;
    if (challenge) {
        text_fit(42, text_y, timer_line, 2.4f, hud_w - 36.0f, 255, 200, 90);
        text_y += 30.0f;
    }
    if (playlist) {
        text_fit(42, text_y, playlist_line, 2.4f, hud_w - 36.0f, 255, 200, 90);
    }
    text_fit(42, ui_h - 42, challenge ? "Esc abort" : "Esc menu / Quit", 2.2f, ui_w - 84.0f, 210, 220, 232);
}
