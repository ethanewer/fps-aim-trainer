#include "render.hpp"

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

constexpr int kFirstChar = 32;
constexpr int kCharCount = 96;
constexpr float kBakeH = 64.0f;
constexpr float kEm = 7.0f;

struct UiFont {
    bool metrics_ready = false;
    bool texture_ready = false;
    GLuint texture = 0;
    int atlas_w = 0;
    int atlas_h = 0;
    float bake_h = kBakeH;
    float ascent = 0.0f;
    stbtt_bakedchar chars[kCharCount]{};
    std::vector<unsigned char> atlas_rgba;
};

UiFont g_font;

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    long size = std::ftell(file);
    if (size <= 0) {
        std::fclose(file);
        return false;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    out.resize(static_cast<size_t>(size));
    size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    return read == out.size();
}

std::vector<std::string> font_candidates() {
    std::vector<std::string> paths;
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    std::string root = windir ? std::string(windir) + "\\Fonts\\" : "C:\\Windows\\Fonts\\";
    paths.push_back(root + "cascadiamono.ttf");
    paths.push_back(root + "CascadiaMono.ttf");
    paths.push_back(root + "consola.ttf");
    paths.push_back(root + "cour.ttf");
#elif defined(__APPLE__)
    paths.push_back("/System/Library/Fonts/Menlo.ttc");
    paths.push_back("/Library/Fonts/Menlo.ttc");
    paths.push_back("/System/Library/Fonts/Supplemental/Courier New.ttf");
    paths.push_back("/System/Library/Fonts/Monaco.ttf");
    paths.push_back("/System/Library/Fonts/Courier.ttc");
#else
    paths.push_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    paths.push_back("/usr/share/fonts/TTF/DejaVuSansMono.ttf");
    paths.push_back("/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf");
    paths.push_back("/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf");
    paths.push_back("/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf");
#endif
    return paths;
}

bool bake_font(const std::vector<unsigned char>& ttf, int offset) {
    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf.data(), offset)) {
        return false;
    }

    auto try_atlas = [&](int width, int height) {
        std::vector<unsigned char> alpha(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
        stbtt_bakedchar chars[kCharCount];
        std::memset(chars, 0, sizeof(chars));
        int rows = stbtt_BakeFontBitmap(
            ttf.data(),
            offset,
            kBakeH,
            alpha.data(),
            width,
            height,
            kFirstChar,
            kCharCount,
            chars
        );
        if (rows <= 0) {
            return false;
        }
        g_font.atlas_w = width;
        g_font.atlas_h = height;
        std::memcpy(g_font.chars, chars, sizeof(chars));
        g_font.atlas_rgba.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 255);
        for (int i = 0; i < width * height; ++i) {
            g_font.atlas_rgba[static_cast<size_t>(i) * 4u + 3u] = alpha[static_cast<size_t>(i)];
        }
        return true;
    };

    if (!try_atlas(512, 512) && !try_atlas(1024, 512) && !try_atlas(1024, 1024)) {
        return false;
    }

    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    g_font.ascent = static_cast<float>(ascent) * stbtt_ScaleForPixelHeight(&info, kBakeH);
    g_font.bake_h = kBakeH;
    g_font.metrics_ready = true;
    return true;
}

bool load_ui_font() {
    if (g_font.metrics_ready) {
        return true;
    }
    for (const std::string& path : font_candidates()) {
        std::vector<unsigned char> ttf;
        if (!read_file(path, ttf)) {
            continue;
        }
        int offset = stbtt_GetFontOffsetForIndex(ttf.data(), 0);
        if (offset < 0) {
            offset = 0;
        }
        if (bake_font(ttf, offset)) {
            return true;
        }
    }
    std::fprintf(stderr, "failed to load a system monospace UI font\n");
    return false;
}

void ensure_font_texture() {
    if (!g_font.metrics_ready || g_font.texture_ready) {
        return;
    }
    if (!SDL_GL_GetCurrentContext()) {
        return;
    }
    glGenTextures(1, &g_font.texture);
    glBindTexture(GL_TEXTURE_2D, g_font.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        g_font.atlas_w,
        g_font.atlas_h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        g_font.atlas_rgba.data()
    );
    glBindTexture(GL_TEXTURE_2D, 0);
    g_font.texture_ready = true;
}

int glyph_index(unsigned char c) {
    if (c < kFirstChar || c >= kFirstChar + kCharCount) {
        c = static_cast<unsigned char>('?');
    }
    return c - kFirstChar;
}

float draw_scale(float scale) {
    return (kEm * scale) / g_font.bake_h;
}

}  // namespace

float text_height(float scale) {
    return kEm * scale;
}

float text_width(const std::string& value, float scale) {
    if (!load_ui_font()) {
        return static_cast<float>(value.size()) * 6.0f * scale;
    }
    float s = draw_scale(scale);
    float width = 0.0f;
    for (char ch : value) {
        unsigned char c = static_cast<unsigned char>(ch);
        width += g_font.chars[glyph_index(c)].xadvance * s;
    }
    return width;
}

void text(float x, float y, const std::string& value, float scale, uint8_t r, uint8_t g, uint8_t b) {
    if (!load_ui_font()) {
        return;
    }
    ensure_font_texture();
    if (!g_font.texture_ready) {
        return;
    }

    float s = draw_scale(scale);
    float cursor = x;
    float baseline = y + g_font.ascent * s;
    float ipw = 1.0f / static_cast<float>(g_font.atlas_w);
    float iph = 1.0f / static_cast<float>(g_font.atlas_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_font.texture);
    glColor4ub(r, g, b, 255);
    glBegin(GL_QUADS);
    for (char ch : value) {
        unsigned char c = static_cast<unsigned char>(ch);
        const stbtt_bakedchar& glyph = g_font.chars[glyph_index(c)];
        float x0 = cursor + glyph.xoff * s;
        float y0 = baseline + glyph.yoff * s;
        float x1 = x0 + static_cast<float>(glyph.x1 - glyph.x0) * s;
        float y1 = y0 + static_cast<float>(glyph.y1 - glyph.y0) * s;
        float s0 = static_cast<float>(glyph.x0) * ipw;
        float t0 = static_cast<float>(glyph.y0) * iph;
        float s1 = static_cast<float>(glyph.x1) * ipw;
        float t1 = static_cast<float>(glyph.y1) * iph;
        glTexCoord2f(s0, t0); glVertex2f(x0, y0);
        glTexCoord2f(s1, t0); glVertex2f(x1, y0);
        glTexCoord2f(s1, t1); glVertex2f(x1, y1);
        glTexCoord2f(s0, t1); glVertex2f(x0, y1);
        cursor += glyph.xadvance * s;
    }
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

float text_fit_scale(const std::string& value, float scale, float max_width) {
    float width = text_width(value, scale);
    if (width > max_width && width > 0.0f) {
        return scale * (max_width / width);
    }
    return scale;
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
    text(x, y, value, text_fit_scale(value, scale, max_width), r, g, b);
}
