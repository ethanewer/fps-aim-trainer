#include "render.hpp"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

#include "world.hpp"

static void color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    glColor4ub(r, g, b, a);
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

static void vertex_color(uint8_t r, uint8_t g, uint8_t b) {
    color(r, g, b);
}

static void draw_quad_gradient(Vec3 a, Vec3 b, Vec3 c, Vec3 d, std::array<std::array<uint8_t, 3>, 4> colors) {
    glBegin(GL_QUADS);
    vertex_color(colors[0][0], colors[0][1], colors[0][2]);
    glVertex3f(a.x, a.y, a.z);
    vertex_color(colors[1][0], colors[1][1], colors[1][2]);
    glVertex3f(b.x, b.y, b.z);
    vertex_color(colors[2][0], colors[2][1], colors[2][2]);
    glVertex3f(c.x, c.y, c.z);
    vertex_color(colors[3][0], colors[3][1], colors[3][2]);
    glVertex3f(d.x, d.y, d.z);
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

static void draw_lit_cylinder_y(Vec3 top, float radius, float height) {
    constexpr int slices = 36;
    float y0 = top.y - height;
    float y1 = top.y;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; ++j) {
        float lng = static_cast<float>(M_PI) * 2.0f * static_cast<float>(j) / slices;
        float x = std::cos(lng);
        float z = std::sin(lng);
        glNormal3f(x, 0.0f, z);
        glVertex3f(top.x + radius * x, y0, top.z + radius * z);
        glVertex3f(top.x + radius * x, y1, top.z + radius * z);
    }
    glEnd();
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
    float wall_distance = game.wall_settings.wall_distance;
    float wall_z = wall_z_from_distance(wall_distance);
    float back_z = wall_back_z_for_distance(wall_distance);
    float width = wall_width_for_distance(wall_distance);
    float height = wall_height_for_distance(wall_distance);
    color(94, 101, 109);
    draw_box({0.0f, height * 0.5f, wall_z}, {width, height, 0.18f});
    color(82, 89, 97);
    draw_box({0.0f, -0.05f, (wall_z + back_z) * 0.5f}, {width, 0.1f, back_z - wall_z});
    color(78, 85, 93);
    draw_box({0.0f, height + 0.05f, (wall_z + back_z) * 0.5f}, {width, 0.1f, back_z - wall_z});
    color(72, 79, 87);
    draw_box({-width * 0.5f, height * 0.5f, (wall_z + back_z) * 0.5f}, {0.15f, height, back_z - wall_z});
    draw_box({width * 0.5f, height * 0.5f, (wall_z + back_z) * 0.5f}, {0.15f, height, back_z - wall_z});
    color(64, 70, 78);
    draw_box({0.0f, height * 0.5f, wall_z + 0.03f}, {width + 0.25f, 0.18f, 0.08f});
    draw_box({0.0f, 0.0f, wall_z + 0.03f}, {width + 0.25f, 0.18f, 0.08f});
    draw_box({-width * 0.5f, height * 0.5f, wall_z + 0.03f}, {0.18f, height + 0.25f, 0.08f});
    draw_box({width * 0.5f, height * 0.5f, wall_z + 0.03f}, {0.18f, height + 0.25f, 0.08f});
}

static void draw_plane360(const Game& game) {
    float h = tracking_room_half_size(game.pill_settings);
    float y0 = 0.0f;
    float y1 = tracking_room_height(game.pill_settings);

    draw_quad_gradient(
        {-h, y0, -h}, {h, y0, -h}, {h, y0, h}, {-h, y0, h},
        {{{68, 76, 85}, {74, 82, 91}, {64, 72, 81}, {58, 66, 75}}}
    );
    draw_quad_gradient(
        {-h, y1, h}, {h, y1, h}, {h, y1, -h}, {-h, y1, -h},
        {{{112, 120, 128}, {121, 129, 137}, {108, 116, 124}, {102, 110, 118}}}
    );
    draw_quad_gradient(
        {-h, y0, -h}, {-h, y1, -h}, {h, y1, -h}, {h, y0, -h},
        {{{76, 85, 94}, {86, 95, 104}, {96, 104, 112}, {86, 95, 104}}}
    );
    draw_quad_gradient(
        {h, y0, h}, {h, y1, h}, {-h, y1, h}, {-h, y0, h},
        {{{55, 64, 73}, {72, 81, 90}, {64, 73, 82}, {50, 59, 68}}}
    );
    draw_quad_gradient(
        {-h, y0, h}, {-h, y1, h}, {-h, y1, -h}, {-h, y0, -h},
        {{{49, 58, 67}, {66, 75, 84}, {78, 87, 96}, {58, 67, 76}}}
    );
    draw_quad_gradient(
        {h, y0, -h}, {h, y1, -h}, {h, y1, h}, {h, y0, h},
        {{{79, 88, 97}, {96, 104, 112}, {86, 95, 103}, {67, 76, 85}}}
    );
}

static void set_target_material() {
    GLfloat ambient[] = {0.72f, 0.14f, 0.18f, 1.0f};
    GLfloat diffuse[] = {1.0f, 0.28f, 0.38f, 1.0f};
    GLfloat specular[] = {0.22f, 0.12f, 0.14f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 14.0f);
}

static void draw_target(const Target& target, ScenarioKind kind) {
    set_target_material();
    if (is_tracking(kind)) {
        draw_lit_cylinder_y(target.pos, target.radius, TRACKING_CAPSULE_HEIGHT);
        draw_lit_sphere(target.pos, target.radius);
        draw_lit_sphere(target.pos - Vec3{0.0f, TRACKING_CAPSULE_HEIGHT, 0.0f}, target.radius);
        return;
    }
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    color(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
    glDisable(GL_BLEND);
}

const std::array<const char*, 7>& glyph(char c) {
    static const std::array<const char*, 7> blank = {"00000","00000","00000","00000","00000","00000","00000"};
    static const std::array<const char*, 7> glyphs[47] = {
        {"01110","10001","10011","10101","11001","10001","01110"}, // 0
        {"00100","01100","00100","00100","00100","00100","01110"}, // 1
        {"01110","10001","00001","00010","00100","01000","11111"}, // 2
        {"11110","00001","00001","01110","00001","00001","11110"}, // 3
        {"00010","00110","01010","10010","11111","00010","00010"}, // 4
        {"11111","10000","10000","11110","00001","00001","11110"}, // 5
        {"00110","01000","10000","11110","10001","10001","01110"}, // 6
        {"11111","00001","00010","00100","01000","01000","01000"}, // 7
        {"01110","10001","10001","01110","10001","10001","01110"}, // 8
        {"01110","10001","10001","01111","00001","00010","11100"}, // 9
        {"01110","10001","10001","11111","10001","10001","10001"}, // A
        {"11110","10001","10001","11110","10001","10001","11110"}, // B
        {"01111","10000","10000","10000","10000","10000","01111"}, // C
        {"11110","10001","10001","10001","10001","10001","11110"}, // D
        {"11111","10000","10000","11110","10000","10000","11111"}, // E
        {"11111","10000","10000","11110","10000","10000","10000"}, // F
        {"01111","10000","10000","10011","10001","10001","01111"}, // G
        {"10001","10001","10001","11111","10001","10001","10001"}, // H
        {"01110","00100","00100","00100","00100","00100","01110"}, // I
        {"00111","00010","00010","00010","00010","10010","01100"}, // J
        {"10001","10010","10100","11000","10100","10010","10001"}, // K
        {"10000","10000","10000","10000","10000","10000","11111"}, // L
        {"10001","11011","10101","10101","10001","10001","10001"}, // M
        {"10001","11001","10101","10011","10001","10001","10001"}, // N
        {"01110","10001","10001","10001","10001","10001","01110"}, // O
        {"11110","10001","10001","11110","10000","10000","10000"}, // P
        {"01110","10001","10001","10001","10101","10010","01101"}, // Q
        {"11110","10001","10001","11110","10100","10010","10001"}, // R
        {"01111","10000","10000","01110","00001","00001","11110"}, // S
        {"11111","00100","00100","00100","00100","00100","00100"}, // T
        {"10001","10001","10001","10001","10001","10001","01110"}, // U
        {"10001","10001","10001","10001","10001","01010","00100"}, // V
        {"10001","10001","10001","10101","10101","10101","01010"}, // W
        {"10001","10001","01010","00100","01010","10001","10001"}, // X
        {"10001","10001","01010","00100","00100","00100","00100"}, // Y
        {"11111","00001","00010","00100","01000","10000","11111"}, // Z
        {"00000","00000","00000","11111","00000","00000","00000"}, // -
        {"00000","00100","00100","11111","00100","00100","00000"}, // +
        {"00000","00000","00000","00000","00000","01100","01100"}, // .
        {"00000","01100","01100","00000","01100","01100","00000"}, // :
        {"00001","00010","00010","00100","01000","01000","10000"}, // /
        {"11001","11010","00100","01000","10110","00110","00000"}, // %
        {"00000","00000","00000","00000","00000","00000","11111"}, // _
        {"00110","01000","10000","10000","10000","01000","00110"}, // (
        {"01100","00010","00001","00001","00001","00010","01100"}, // )
        {"11100","10000","10000","10000","10000","10000","11100"}, // [
        {"00111","00001","00001","00001","00001","00001","00111"}, // ]
    };
    if (c >= '0' && c <= '9') return glyphs[c - '0'];
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return glyphs[10 + c - 'A'];
    if (c == '-') return glyphs[36];
    if (c == '+') return glyphs[37];
    if (c == '.') return glyphs[38];
    if (c == ':') return glyphs[39];
    if (c == '/') return glyphs[40];
    if (c == '%') return glyphs[41];
    if (c == '_') return glyphs[42];
    if (c == '(') return glyphs[43];
    if (c == ')') return glyphs[44];
    if (c == '[') return glyphs[45];
    if (c == ']') return glyphs[46];
    return blank;
}

float text_width(const std::string& value, float scale) {
    float width = 0.0f;
    for (char c : value) {
        width += (c == ' ') ? 4.0f * scale : 6.0f * scale;
    }
    return width;
}

void text(float x, float y, const std::string& value, float scale, uint8_t r, uint8_t g, uint8_t b) {
    float cursor = x;
    for (char c : value) {
        if (c == ' ') {
            cursor += 4.0f * scale;
            continue;
        }
        const auto& rows = glyph(c);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if (rows[row][col] == '1') {
                    rect(cursor + col * scale, y + row * scale, scale * 0.82f, scale * 0.82f, r, g, b);
                }
            }
        }
        cursor += 6.0f * scale;
    }
}

void text_fit(
    float x,
    float y,
    const std::string& value,
    float scale,
    float max_width,
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    float base_width = text_width(value, 1.0f);
    float fitted_scale = scale;
    if (base_width > 0.0f && text_width(value, scale) > max_width) {
        fitted_scale = std::max(1.0f, max_width / base_width);
    }
    text(x, y, value, fitted_scale, r, g, b);
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
    text_fit(x + 14.0f, y + 10.0f, label, 2.1f, w - 28.0f, 230, 236, 244);
    return hovered && input.left_pressed;
}

void draw_world(const Game& game, int w, int h) {
    if (game.scenario.map == MapKind::Plane360) {
        glClearColor(76.0f / 255.0f, 83.0f / 255.0f, 92.0f / 255.0f, 1.0f);
    } else {
        glClearColor(66.0f / 255.0f, 72.0f / 255.0f, 80.0f / 255.0f, 1.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    begin_3d(game, w, h);
    if (game.scenario.map == MapKind::WallRoom) draw_wall_room(game);
    else draw_plane360(game);
    GLfloat light_ambient[] = {0.82f, 0.82f, 0.84f, 1.0f};
    GLfloat light_diffuse[] = {0.70f, 0.70f, 0.68f, 1.0f};
    GLfloat light_pos[] = {-4.0f, game.scenario.map == MapKind::WallRoom ? wall_height_for_distance(game.wall_settings.wall_distance) + 1.5f : tracking_room_height(game.pill_settings) + 3.0f, 1.0f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    for (const Target& target : game.targets) {
        draw_target(target, game.scenario.kind);
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
    std::snprintf(line, sizeof(line), "FOV 103  SENS %.3f", game.sensitivity);
    std::string sens_line = line;
    std::string stat_line;
    if (game.scenario.kind == ScenarioKind::WallClick) {
        float accuracy = game.stats.shots == 0 ? 100.0f : static_cast<float>(game.stats.hits) / static_cast<float>(game.stats.shots) * 100.0f;
        std::snprintf(line, sizeof(line), "HITS %d  SHOTS %d  ACC %.1f%%", game.stats.hits, game.stats.shots, accuracy);
        stat_line = line;
    } else {
        float tracking = game.stats.tracking_fire_time <= 0.0001f ? 0.0f : game.stats.tracking_on_time / game.stats.tracking_fire_time * 100.0f;
        std::snprintf(line, sizeof(line), "TRACKING %.1f%%  HOLD LMB", tracking);
        stat_line = line;
    }
    float hud_w = std::max({
        260.0f,
        text_width(game.scenario.title, 3.0f) + 36.0f,
        text_width(sens_line, 2.4f) + 36.0f,
        text_width(stat_line, 2.4f) + 36.0f,
    });
    hud_w = std::min(hud_w, ui_w - 48.0f);
    rect(24, 22, hud_w, 122, 0, 0, 0, 150);
    text_fit(42, 42, game.scenario.title, 3.0f, hud_w - 36.0f);
    text_fit(42, 78, sens_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_fit(42, 108, stat_line, 2.4f, hud_w - 36.0f, 210, 220, 232);
    text_fit(42, ui_h - 42, "ESC MENU / QUIT", 2.2f, ui_w - 84.0f, 210, 220, 232);
}
