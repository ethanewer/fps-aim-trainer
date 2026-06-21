#include <SDL2/SDL.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "config.hpp"
#include "menu.hpp"
#include "render.hpp"
#include "scenario.hpp"
#include "selftest.hpp"
#include "types.hpp"

static void set_mouse_grab(Game& game, bool grabbed) {
    if (game.mouse_grabbed == grabbed) {
        return;
    }
    SDL_SetRelativeMouseMode(grabbed ? SDL_TRUE : SDL_FALSE);
    SDL_ShowCursor(grabbed ? SDL_DISABLE : SDL_ENABLE);
    game.mouse_grabbed = grabbed;
}

static bool save_bmp_screenshot(const std::string& path, int w, int h) {
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    std::vector<uint8_t> flipped(pixels.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    int stride = w * 4;
    for (int y = 0; y < h; ++y) {
        std::copy(
            pixels.begin() + static_cast<size_t>(h - 1 - y) * stride,
            pixels.begin() + static_cast<size_t>(h - y) * stride,
            flipped.begin() + static_cast<size_t>(y) * stride
        );
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        flipped.data(),
        w,
        h,
        32,
        stride,
        SDL_PIXELFORMAT_RGBA32
    );
    if (!surface) {
        std::fprintf(stderr, "SDL_CreateRGBSurfaceWithFormatFrom failed: %s\n", SDL_GetError());
        return false;
    }
    int rc = SDL_SaveBMP(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (rc != 0) {
        std::fprintf(stderr, "SDL_SaveBMP failed for %s: %s\n", path.c_str(), SDL_GetError());
        return false;
    }
    return true;
}

static SDL_Window* create_gl_window(const char* title, int width, int height, bool fullscreen) {
    uint32_t flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    return SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
}

static void make_dir_if_needed(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static bool render_debug_shot(int scenario_index, const std::string& path, int width, int height, int frames) {
    SDL_Window* window = create_gl_window("Aim Trainer Debug Shot", width, height, false);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_GL_SetSwapInterval(0);
    glEnable(GL_MULTISAMPLE);

    Game game;
    game.rng.seed(7);
    load_settings(game);
    init_scenarios(game);
    if (scenario_index < 0 || scenario_index >= static_cast<int>(game.scenarios.size())) {
        std::fprintf(stderr, "Invalid scenario index %d\n", scenario_index);
        SDL_GL_DeleteContext(gl);
        SDL_DestroyWindow(window);
        return false;
    }
    start_scenario(game, game.scenarios[scenario_index]);

    for (int frame = 0; frame < std::max(1, frames); ++frame) {
        Input input;
        update_playing(game, input, 1.0f / 60.0f);
        int drawable_w = 0, drawable_h = 0;
        SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
        draw_world(game, drawable_w, drawable_h);
        SDL_GL_SwapWindow(window);
    }

    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    bool ok = save_bmp_screenshot(path, drawable_w, drawable_h);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    return ok;
}

static bool render_debug_menu(const std::string& path, int width, int height, int tab_index = 0, int state_index = 0) {
    SDL_Window* window = create_gl_window("Aim Trainer Debug Menu", width, height, false);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    SDL_GL_SetSwapInterval(0);

    Game game;
    load_settings(game);
    init_scenarios(game);
    if (tab_index == 1) game.menu_tab = MenuTab::Tracking;
    else if (tab_index == 2) game.menu_tab = MenuTab::General;
    else game.menu_tab = MenuTab::Clicking;
    if (state_index == 1) {
        if (game.menu_tab == MenuTab::Tracking) {
            game.pill_preset_name.clear();
            menu_focus_field(game, FieldId::PillName);
        } else {
            game.wall_preset_name.clear();
            menu_focus_field(game, FieldId::WallName);
        }
    } else if (state_index == 2) {
        if (game.menu_tab == MenuTab::Tracking) {
            game.pill_preset_name = "THIS NAME IS EXACTLY M";
            menu_focus_field(game, FieldId::PillName);
        } else {
            game.wall_preset_name = "THIS NAME IS EXACTLY M";
            menu_focus_field(game, FieldId::WallName);
        }
    } else if (state_index == 5) {
        // Focused numeric box mid-edit (validates the active value-box look).
        if (game.menu_tab == MenuTab::Tracking) menu_focus_field(game, FieldId::PillDistMax);
        else menu_focus_field(game, FieldId::WallRadiusMin);
        game.edit_draft = "0.12";
        game.edit_fresh = false;
    } else if (state_index == 3) {
        for (int i = 0; i < 12; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "LONG PRESET NAME %02d", i + 1);
            game.wall_presets.push_back({name, game.wall_settings});
            game.pill_presets.push_back({name, game.pill_settings});
        }
        ensure_presets(game);
        game.wall_preset_scroll = std::max(0, static_cast<int>(game.wall_presets.size()) - 7);
        game.pill_preset_scroll = std::max(0, static_cast<int>(game.pill_presets.size()) - 7);
    } else if (state_index == 4) {
        game.menu_tab = MenuTab::Clicking;
        game.wall_preset_name = "MAX RANGE STRESS TEST";
        game.wall_settings.target_count_min = 18;
        game.wall_settings.target_count_max = 18;
        game.wall_settings.wall_distance = 30.0f;
        game.wall_settings.radius_min = 0.44f;
        game.wall_settings.radius_max = 0.45f;
        game.wall_settings.horizontal_speed_min = 7.90f;
        game.wall_settings.horizontal_speed_max = 8.00f;
        game.wall_settings.vertical_speed_min = 7.90f;
        game.wall_settings.vertical_speed_max = 8.00f;
        game.wall_settings.acceleration_min = 39.5f;
        game.wall_settings.acceleration_max = 40.0f;
        game.wall_settings.change_min = 11.90f;
        game.wall_settings.change_max = 12.00f;
        normalize_settings(game);
    }
    Input input;
    int drawable_w = 0, drawable_h = 0;
    SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
    draw_menu(game, input, drawable_w, drawable_h);
    bool ok = save_bmp_screenshot(path, drawable_w, drawable_h);
    SDL_GL_SwapWindow(window);
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    return ok;
}

static int run_debug_mode(int argc, char** argv) {
    int width = 1280;
    int height = 720;
    int frames = 8;
    if (std::string(argv[1]) == "--debug-shot") {
        if (argc < 4) {
            std::fprintf(stderr, "Usage: %s --debug-shot <scenario-index> <out.bmp> [width height frames]\n", argv[0]);
            return 2;
        }
        if (argc >= 6) {
            width = std::max(320, std::atoi(argv[4]));
            height = std::max(180, std::atoi(argv[5]));
        }
        if (argc >= 7) {
            frames = std::max(1, std::atoi(argv[6]));
        }
        int scenario = std::atoi(argv[2]);
        return render_debug_shot(scenario, argv[3], width, height, frames) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--debug-menu") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: %s --debug-menu <out.bmp> [width height tab state]\n", argv[0]);
            return 2;
        }
        if (argc >= 5) {
            width = std::max(320, std::atoi(argv[3]));
            height = std::max(180, std::atoi(argv[4]));
        }
        int tab = argc >= 6 ? std::atoi(argv[5]) : 0;
        int state = argc >= 7 ? std::atoi(argv[6]) : 0;
        return render_debug_menu(argv[2], width, height, tab, state) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--debug-all") {
        if (argc < 3) {
            std::fprintf(stderr, "Usage: %s --debug-all <output-dir> [width height]\n", argv[0]);
            return 2;
        }
        if (argc >= 5) {
            width = std::max(320, std::atoi(argv[3]));
            height = std::max(180, std::atoi(argv[4]));
        }
        make_dir_if_needed(argv[2]);
        bool ok = true;
        Game scenario_count_game;
        init_scenarios(scenario_count_game);
        for (int scenario = 0; scenario < static_cast<int>(scenario_count_game.scenarios.size()); ++scenario) {
            char path[1024];
            std::snprintf(path, sizeof(path), "%s/scenario-%d.bmp", argv[2], scenario);
            ok = render_debug_shot(scenario, path, width, height, frames) && ok;
        }
        return ok ? 0 : 1;
    }
    return 2;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        int rc = run_self_test();
        SDL_Quit();
        return rc;
    }

    if (argc > 1 && (std::string(argv[1]) == "--debug-shot" || std::string(argv[1]) == "--debug-all" || std::string(argv[1]) == "--debug-menu")) {
        int rc = run_debug_mode(argc, argv);
        SDL_Quit();
        return rc;
    }

    SDL_Window* window = create_gl_window("Aim Trainer", 1280, 720, true);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_MULTISAMPLE);
    SDL_StartTextInput();

    Game game;
    load_settings(game);
    init_scenarios(game);

    uint64_t last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        Input input;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) input.quit = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE && event.key.repeat == 0) input.escape_pressed = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) input.backspace_pressed = true;
            if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) && event.key.repeat == 0) input.enter_pressed = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB && event.key.repeat == 0) input.tab_pressed = true;
            if (event.type == SDL_TEXTINPUT) input.text_input += event.text.text;
            if (event.type == SDL_MOUSEWHEEL) input.wheel_y += event.wheel.y;
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                input.left_pressed = true;
                input.left_down = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) input.left_down = false;
            if (event.type == SDL_MOUSEMOTION) {
                input.mouse_x = event.motion.x;
                input.mouse_y = event.motion.y;
                input.rel_x += event.motion.xrel;
                input.rel_y += event.motion.yrel;
            }
        }
        int mx, my;
        uint32_t buttons = SDL_GetMouseState(&mx, &my);
        input.mouse_x = mx;
        input.mouse_y = my;
        input.left_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        input.shift_down = (SDL_GetModState() & KMOD_SHIFT) != 0;

        if (input.quit) {
            running = false;
        }
        if (input.escape_pressed) {
            if (game.mode == AppMode::Playing) game.mode = AppMode::Menu;
            else if (game.active_field != FieldId::None) menu_cancel_edit(game);
            else running = false;
        }

        uint64_t now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency()));
        last = now;
        dt = std::min(dt, 1.0f / 20.0f);

        int window_w = 0, window_h = 0;
        int drawable_w = 0, drawable_h = 0;
        SDL_GetWindowSize(window, &window_w, &window_h);
        SDL_GL_GetDrawableSize(window, &drawable_w, &drawable_h);
        float scale_x = window_w > 0 ? static_cast<float>(drawable_w) / static_cast<float>(window_w) : 1.0f;
        float scale_y = window_h > 0 ? static_cast<float>(drawable_h) / static_cast<float>(window_h) : 1.0f;
        input.mouse_x = static_cast<int>(std::round(static_cast<float>(input.mouse_x) * scale_x));
        input.mouse_y = static_cast<int>(std::round(static_cast<float>(input.mouse_y) * scale_y));
        if (game.mode == AppMode::Playing) {
            set_mouse_grab(game, true);
            update_playing(game, input, dt);
            draw_world(game, drawable_w, drawable_h);
        } else {
            set_mouse_grab(game, false);
            draw_menu(game, input, drawable_w, drawable_h);
        }

        SDL_GL_SwapWindow(window);
    }

    set_mouse_grab(game, false);
    SDL_StopTextInput();
    SDL_GL_DeleteContext(gl);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
